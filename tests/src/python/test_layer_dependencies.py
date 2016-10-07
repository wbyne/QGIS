# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsSnappingUtils (complement to C++-based tests)

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Hugo Mercier'
__date__ = '12/07/2016'
__copyright__ = 'Copyright 2016, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis  # NOQA
import os

from qgis.core import (QgsMapLayerRegistry,
                       QgsVectorLayer,
                       QgsMapSettings,
                       QgsSnappingUtils,
                       QgsPointLocator,
                       QgsTolerance,
                       QgsRectangle,
                       QgsPoint,
                       QgsFeature,
                       QgsGeometry,
                       QgsProject,
                       QgsLayerDefinition,
                       QgsMapLayerDependency
                       )

from qgis.testing import start_app, unittest
from utilities import unitTestDataPath

from qgis.PyQt.QtCore import QSize, QPoint

import tempfile

from qgis.utils import spatialite_connect

# Convenience instances in case you may need them
start_app()


class TestLayerDependencies(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        """Run before all tests"""

        # create a temp spatialite db with a trigger
        fo = tempfile.NamedTemporaryFile()
        fn = fo.name
        fo.close()
        cls.fn = fn
        con = spatialite_connect(fn)
        cur = con.cursor()
        cur.execute("SELECT InitSpatialMetadata(1)")
        cur.execute("create table node(id integer primary key autoincrement);")
        cur.execute("select AddGeometryColumn('node', 'geom', 4326, 'POINT');")
        cur.execute("create table section(id integer primary key autoincrement, node1 integer, node2 integer);")
        cur.execute("select AddGeometryColumn('section', 'geom', 4326, 'LINESTRING');")
        cur.execute("create trigger add_nodes after insert on section begin insert into node (geom) values (st_startpoint(NEW.geom)); insert into node (geom) values (st_endpoint(NEW.geom)); end;")
        cur.execute("insert into node (geom) values (geomfromtext('point(0 0)', 4326));")
        cur.execute("insert into node (geom) values (geomfromtext('point(1 0)', 4326));")
        cur.execute("create table node2(id integer primary key autoincrement);")
        cur.execute("select AddGeometryColumn('node2', 'geom', 4326, 'POINT');")
        cur.execute("create trigger add_nodes2 after insert on node begin insert into node2 (geom) values (st_translate(NEW.geom, 0.2, 0, 0)); end;")
        con.commit()
        con.close()

        cls.pointsLayer = QgsVectorLayer("dbname='%s' table=\"node\" (geom) sql=" % fn, "points", "spatialite")
        assert (cls.pointsLayer.isValid())
        cls.linesLayer = QgsVectorLayer("dbname='%s' table=\"section\" (geom) sql=" % fn, "lines", "spatialite")
        assert (cls.linesLayer.isValid())
        cls.pointsLayer2 = QgsVectorLayer("dbname='%s' table=\"node2\" (geom) sql=" % fn, "_points2", "spatialite")
        assert (cls.pointsLayer2.isValid())
        QgsMapLayerRegistry.instance().addMapLayers([cls.pointsLayer, cls.linesLayer, cls.pointsLayer2])

        # save the project file
        fo = tempfile.NamedTemporaryFile()
        fn = fo.name
        fo.close()
        cls.projectFile = fn
        QgsProject.instance().setFileName(cls.projectFile)
        QgsProject.instance().write()

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""
        pass

    def setUp(self):
        """Run before each test."""
        pass

    def tearDown(self):
        """Run after each test."""
        pass

    def test_resetSnappingIndex(self):
        self.pointsLayer.setDependencies([])
        self.linesLayer.setDependencies([])
        self.pointsLayer2.setDependencies([])

        ms = QgsMapSettings()
        ms.setOutputSize(QSize(100, 100))
        ms.setExtent(QgsRectangle(0, 0, 1, 1))
        self.assertTrue(ms.hasValidSettings())

        u = QgsSnappingUtils()
        u.setMapSettings(ms)
        u.setSnapToMapMode(QgsSnappingUtils.SnapAdvanced)
        layers = [QgsSnappingUtils.LayerConfig(self.pointsLayer, QgsPointLocator.Vertex, 20, QgsTolerance.Pixels)]
        u.setLayers(layers)

        m = u.snapToMap(QPoint(95, 100))
        self.assertTrue(m.isValid())
        self.assertTrue(m.hasVertex())
        self.assertEqual(m.point(), QgsPoint(1, 0))

        f = QgsFeature(self.linesLayer.fields())
        f.setFeatureId(1)
        geom = QgsGeometry.fromWkt("LINESTRING(0 0,1 1)")
        f.setGeometry(geom)
        self.linesLayer.startEditing()
        self.linesLayer.addFeatures([f])
        self.linesLayer.commitChanges()

        l1 = len([f for f in self.pointsLayer.getFeatures()])
        self.assertEqual(l1, 4)
        m = u.snapToMap(QPoint(95, 0))
        # snapping not updated
        self.pointsLayer.setDependencies([])
        self.assertEqual(m.isValid(), False)

        # set layer dependencies
        self.pointsLayer.setDependencies([QgsMapLayerDependency(self.linesLayer.id())])
        # add another line
        f = QgsFeature(self.linesLayer.fields())
        f.setFeatureId(2)
        geom = QgsGeometry.fromWkt("LINESTRING(0 0,0.5 0.5)")
        f.setGeometry(geom)
        self.linesLayer.startEditing()
        self.linesLayer.addFeatures([f])
        self.linesLayer.commitChanges()
        # check the snapped point is ok
        m = u.snapToMap(QPoint(45, 50))
        self.assertTrue(m.isValid())
        self.assertTrue(m.hasVertex())
        self.assertEqual(m.point(), QgsPoint(0.5, 0.5))
        self.pointsLayer.setDependencies([])

        # test chained layer dependencies A -> B -> C
        layers = [QgsSnappingUtils.LayerConfig(self.pointsLayer, QgsPointLocator.Vertex, 20, QgsTolerance.Pixels),
                  QgsSnappingUtils.LayerConfig(self.pointsLayer2, QgsPointLocator.Vertex, 20, QgsTolerance.Pixels)
                  ]
        u.setLayers(layers)
        self.pointsLayer.setDependencies([QgsMapLayerDependency(self.linesLayer.id())])
        self.pointsLayer2.setDependencies([QgsMapLayerDependency(self.pointsLayer.id())])
        # add another line
        f = QgsFeature(self.linesLayer.fields())
        f.setFeatureId(3)
        geom = QgsGeometry.fromWkt("LINESTRING(0 0.2,0.5 0.8)")
        f.setGeometry(geom)
        self.linesLayer.startEditing()
        self.linesLayer.addFeatures([f])
        self.linesLayer.commitChanges()
        # check the second snapped point is ok
        m = u.snapToMap(QPoint(75, 100 - 80))
        self.assertTrue(m.isValid())
        self.assertTrue(m.hasVertex())
        self.assertEqual(m.point(), QgsPoint(0.7, 0.8))
        self.pointsLayer.setDependencies([])
        self.pointsLayer2.setDependencies([])

    def test_cycleDetection(self):
        self.assertTrue(self.pointsLayer.setDependencies([QgsMapLayerDependency(self.linesLayer.id())]))
        self.assertFalse(self.linesLayer.setDependencies([QgsMapLayerDependency(self.pointsLayer.id())]))
        self.pointsLayer.setDependencies([])
        self.linesLayer.setDependencies([])

    def test_layerDefinitionRewriteId(self):
        tmpfile = os.path.join(tempfile.tempdir, "test.qlr")

        ltr = QgsProject.instance().layerTreeRoot()

        self.pointsLayer.setDependencies([QgsMapLayerDependency(self.linesLayer.id())])

        QgsLayerDefinition.exportLayerDefinition(tmpfile, [ltr])

        grp = ltr.addGroup("imported")
        QgsLayerDefinition.loadLayerDefinition(tmpfile, grp)

        newPointsLayer = None
        newLinesLayer = None
        for l in grp.findLayers():
            if l.layerId().startswith('points'):
                newPointsLayer = l.layer()
            elif l.layerId().startswith('lines'):
                newLinesLayer = l.layer()
        self.assertFalse(newPointsLayer is None)
        self.assertFalse(newLinesLayer is None)
        self.assertTrue(newLinesLayer.id() in [dep.layerId() for dep in newPointsLayer.dependencies()])

        self.pointsLayer.setDependencies([])

    def test_signalConnection(self):
        # remove all layers
        QgsMapLayerRegistry.instance().removeAllMapLayers()
        # set dependencies and add back layers
        self.pointsLayer = QgsVectorLayer("dbname='%s' table=\"node\" (geom) sql=" % self.fn, "points", "spatialite")
        assert (self.pointsLayer.isValid())
        self.linesLayer = QgsVectorLayer("dbname='%s' table=\"section\" (geom) sql=" % self.fn, "lines", "spatialite")
        assert (self.linesLayer.isValid())
        self.pointsLayer2 = QgsVectorLayer("dbname='%s' table=\"node2\" (geom) sql=" % self.fn, "_points2", "spatialite")
        assert (self.pointsLayer2.isValid())
        self.pointsLayer.setDependencies([QgsMapLayerDependency(self.linesLayer.id())])
        self.pointsLayer2.setDependencies([QgsMapLayerDependency(self.pointsLayer.id())])
        # this should update connections between layers
        QgsMapLayerRegistry.instance().addMapLayers([self.pointsLayer])
        QgsMapLayerRegistry.instance().addMapLayers([self.linesLayer])
        QgsMapLayerRegistry.instance().addMapLayers([self.pointsLayer2])

        ms = QgsMapSettings()
        ms.setOutputSize(QSize(100, 100))
        ms.setExtent(QgsRectangle(0, 0, 1, 1))
        self.assertTrue(ms.hasValidSettings())

        u = QgsSnappingUtils()
        u.setMapSettings(ms)
        u.setSnapToMapMode(QgsSnappingUtils.SnapAdvanced)
        layers = [QgsSnappingUtils.LayerConfig(self.pointsLayer, QgsPointLocator.Vertex, 20, QgsTolerance.Pixels),
                  QgsSnappingUtils.LayerConfig(self.pointsLayer2, QgsPointLocator.Vertex, 20, QgsTolerance.Pixels)
                  ]
        u.setLayers(layers)
        # add another line
        f = QgsFeature(self.linesLayer.fields())
        f.setFeatureId(4)
        geom = QgsGeometry.fromWkt("LINESTRING(0.5 0.2,0.6 0)")
        f.setGeometry(geom)
        self.linesLayer.startEditing()
        self.linesLayer.addFeatures([f])
        self.linesLayer.commitChanges()
        # check the second snapped point is ok
        m = u.snapToMap(QPoint(75, 100 - 0))
        self.assertTrue(m.isValid())
        self.assertTrue(m.hasVertex())
        self.assertEqual(m.point(), QgsPoint(0.8, 0.0))

        self.pointsLayer.setDependencies([])
        self.pointsLayer2.setDependencies([])


if __name__ == '__main__':
    unittest.main()
