# -*- coding: utf-8 -*-
"""QGIS Unit tests for the attribute table model.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Matthias Kuhn'
__date__ = '27/05/2015'
__copyright__ = 'Copyright 2015, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

from qgis.gui import (
    QgsAttributeTableModel,
    QgsEditorWidgetRegistry
)
from qgis.core import (
    QgsFeature,
    QgsGeometry,
    QgsPoint,
    QgsVectorLayer,
    QgsVectorLayerCache
)

from qgis.testing import (start_app,
                          unittest
                          )

start_app()


class TestQgsAttributeTableModel(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        QgsEditorWidgetRegistry.initEditors()

    def setUp(self):
        self.layer = self.createLayer()
        self.cache = QgsVectorLayerCache(self.layer, 100)
        self.am = QgsAttributeTableModel(self.cache)
        self.am.loadLayer()
        self.am.loadAttributes()

    def tearDown(self):
        del self.am
        del self.cache
        del self.layer

    def createLayer(self):
        layer = QgsVectorLayer("Point?field=fldtxt:string&field=fldint:integer",
                               "addfeat", "memory")
        pr = layer.dataProvider()
        features = list()
        for i in range(10):
            f = QgsFeature()
            f.setAttributes(["test", i])
            f.setGeometry(QgsGeometry.fromPoint(QgsPoint(100 * i, 2 ^ i)))
            features.append(f)

        self.assertTrue(pr.addFeatures(features))
        return layer

    def testLoad(self):
        self.assertEquals(self.am.rowCount(), 10)
        self.assertEquals(self.am.columnCount(), 2)

    def testRemove(self):
        self.layer.startEditing()
        self.layer.deleteFeature(5)
        self.assertEquals(self.am.rowCount(), 9)
        self.layer.selectByIds([1, 3, 6, 7])
        self.layer.deleteSelectedFeatures()
        self.assertEquals(self.am.rowCount(), 5)

    def testAdd(self):
        self.layer.startEditing()

        f = QgsFeature()
        f.setAttributes(["test", 8])
        f.setGeometry(QgsGeometry.fromPoint(QgsPoint(100, 200)))
        self.layer.addFeature(f)

        self.assertEquals(self.am.rowCount(), 11)

    def testRemoveColumns(self):
        self.assertTrue(self.layer.startEditing())

        self.assertTrue(self.layer.deleteAttribute(1))

        self.assertEquals(self.am.columnCount(), 1)

if __name__ == '__main__':
    unittest.main()
