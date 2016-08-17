# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsVectorLayerEditBuffer.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Nyall Dawson'
__date__ = '15/07/2016'
__copyright__ = 'Copyright 2016, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis  # NOQA

import os

from qgis.PyQt.QtCore import QVariant
from qgis.PyQt.QtGui import QPainter

from qgis.core import (Qgis,
                       QgsWkbTypes,
                       QgsVectorLayer,
                       QgsRectangle,
                       QgsFeature,
                       QgsFeatureRequest,
                       QgsGeometry,
                       QgsPoint,
                       QgsField,
                       QgsFields,
                       QgsMapLayerRegistry,
                       QgsVectorJoinInfo,
                       QgsSymbol,
                       QgsSingleSymbolRenderer,
                       QgsCoordinateReferenceSystem,
                       QgsProject,
                       QgsUnitTypes,
                       QgsAggregateCalculator)
from qgis.testing import start_app, unittest
from utilities import unitTestDataPath
start_app()


def createEmptyLayer():
    layer = QgsVectorLayer("Point?field=fldtxt:string&field=fldint:integer",
                           "addfeat", "memory")
    assert layer.isValid()
    return layer


def createLayerWithOnePoint():
    layer = QgsVectorLayer("Point?field=fldtxt:string&field=fldint:integer",
                           "addfeat", "memory")
    pr = layer.dataProvider()
    f = QgsFeature()
    f.setAttributes(["test", 123])
    f.setGeometry(QgsGeometry.fromPoint(QgsPoint(100, 200)))
    assert pr.addFeatures([f])
    assert layer.pendingFeatureCount() == 1
    return layer


class TestQgsVectorLayerEditBuffer(unittest.TestCase):

    def testAddFeatures(self):
        # test adding features to an edit buffer
        layer = createEmptyLayer()
        self.assertTrue(layer.startEditing())

        self.assertEqual(layer.editBuffer().addedFeatures(), {})
        self.assertFalse(layer.editBuffer().isFeatureAdded(1))
        self.assertFalse(layer.editBuffer().isFeatureAdded(3))

        # add two features
        f1 = QgsFeature(layer.fields(), 1)
        f1.setGeometry(QgsGeometry.fromPoint(QgsPoint(1, 2)))
        f1.setAttributes(["test", 123])
        self.assertTrue(layer.addFeature(f1))

        f2 = QgsFeature(layer.fields(), 2)
        f2.setGeometry(QgsGeometry.fromPoint(QgsPoint(2, 4)))
        f2.setAttributes(["test2", 246])

        self.assertTrue(layer.addFeature(f2))

        # test contents of buffer
        added = layer.editBuffer().addedFeatures()
        new_feature_ids = list(added.keys())
        self.assertEqual(added[new_feature_ids[0]]['fldtxt'], 'test2')
        self.assertEqual(added[new_feature_ids[0]]['fldint'], 246)
        self.assertEqual(added[new_feature_ids[1]]['fldtxt'], 'test')
        self.assertEqual(added[new_feature_ids[1]]['fldint'], 123)

        self.assertTrue(layer.editBuffer().isFeatureAdded(new_feature_ids[0]))
        self.assertTrue(layer.editBuffer().isFeatureAdded(new_feature_ids[1]))

    def testAddMultipleFeatures(self):
        # test adding multiple features to an edit buffer
        layer = createEmptyLayer()
        self.assertTrue(layer.startEditing())

        self.assertEqual(layer.editBuffer().addedFeatures(), {})
        self.assertFalse(layer.editBuffer().isFeatureAdded(1))
        self.assertFalse(layer.editBuffer().isFeatureAdded(3))

        # add two features
        f1 = QgsFeature(layer.fields(), 1)
        f1.setGeometry(QgsGeometry.fromPoint(QgsPoint(1, 2)))
        f1.setAttributes(["test", 123])
        f2 = QgsFeature(layer.fields(), 2)
        f2.setGeometry(QgsGeometry.fromPoint(QgsPoint(2, 4)))
        f2.setAttributes(["test2", 246])

        self.assertTrue(layer.addFeatures([f1, f2]))

        # test contents of buffer
        added = layer.editBuffer().addedFeatures()
        new_feature_ids = list(added.keys())
        self.assertEqual(added[new_feature_ids[0]]['fldtxt'], 'test2')
        self.assertEqual(added[new_feature_ids[0]]['fldint'], 246)
        self.assertEqual(added[new_feature_ids[1]]['fldtxt'], 'test')
        self.assertEqual(added[new_feature_ids[1]]['fldint'], 123)

        self.assertTrue(layer.editBuffer().isFeatureAdded(new_feature_ids[0]))
        self.assertTrue(layer.editBuffer().isFeatureAdded(new_feature_ids[1]))

    def testDeleteFeatures(self):
        # test deleting features from an edit buffer

        # make a layer with two features
        layer = createEmptyLayer()
        self.assertTrue(layer.startEditing())

        # add two features
        f1 = QgsFeature(layer.fields(), 1)
        f1.setGeometry(QgsGeometry.fromPoint(QgsPoint(1, 2)))
        f1.setAttributes(["test", 123])
        self.assertTrue(layer.addFeature(f1))

        f2 = QgsFeature(layer.fields(), 2)
        f2.setGeometry(QgsGeometry.fromPoint(QgsPoint(2, 4)))
        f2.setAttributes(["test2", 246])
        self.assertTrue(layer.addFeature(f2))

        layer.commitChanges()
        layer.startEditing()

        self.assertEqual(layer.editBuffer().deletedFeatureIds(), [])
        self.assertFalse(layer.editBuffer().isFeatureDeleted(1))
        self.assertFalse(layer.editBuffer().isFeatureDeleted(2))

        # delete a feature
        layer.deleteFeature(1)

        # test contents of buffer
        self.assertEqual(layer.editBuffer().deletedFeatureIds(), [1])
        self.assertTrue(layer.editBuffer().isFeatureDeleted(1))
        self.assertFalse(layer.editBuffer().isFeatureDeleted(2))

        # delete a feature
        layer.deleteFeature(2)

        # test contents of buffer
        self.assertEqual(set(layer.editBuffer().deletedFeatureIds()), set([1, 2]))
        self.assertTrue(layer.editBuffer().isFeatureDeleted(1))
        self.assertTrue(layer.editBuffer().isFeatureDeleted(2))

    def testDeleteMultipleFeatures(self):
        # test deleting multiple features from an edit buffer

        # make a layer with two features
        layer = createEmptyLayer()
        self.assertTrue(layer.startEditing())

        # add two features
        f1 = QgsFeature(layer.fields(), 1)
        f1.setGeometry(QgsGeometry.fromPoint(QgsPoint(1, 2)))
        f1.setAttributes(["test", 123])
        self.assertTrue(layer.addFeature(f1))

        f2 = QgsFeature(layer.fields(), 2)
        f2.setGeometry(QgsGeometry.fromPoint(QgsPoint(2, 4)))
        f2.setAttributes(["test2", 246])
        self.assertTrue(layer.addFeature(f2))

        layer.commitChanges()
        layer.startEditing()

        self.assertEqual(layer.editBuffer().deletedFeatureIds(), [])
        self.assertFalse(layer.editBuffer().isFeatureDeleted(1))
        self.assertFalse(layer.editBuffer().isFeatureDeleted(2))

        # delete features
        layer.deleteFeatures([1, 2])

        # test contents of buffer
        self.assertEqual(set(layer.editBuffer().deletedFeatureIds()), set([1, 2]))
        self.assertTrue(layer.editBuffer().isFeatureDeleted(1))
        self.assertTrue(layer.editBuffer().isFeatureDeleted(2))

    def testChangeAttributeValues(self):
        # test changing attributes values from an edit buffer

        # make a layer with two features
        layer = createEmptyLayer()
        self.assertTrue(layer.startEditing())

        # add two features
        f1 = QgsFeature(layer.fields(), 1)
        f1.setGeometry(QgsGeometry.fromPoint(QgsPoint(1, 2)))
        f1.setAttributes(["test", 123])
        self.assertTrue(layer.addFeature(f1))

        f2 = QgsFeature(layer.fields(), 2)
        f2.setGeometry(QgsGeometry.fromPoint(QgsPoint(2, 4)))
        f2.setAttributes(["test2", 246])
        self.assertTrue(layer.addFeature(f2))

        layer.commitChanges()
        layer.startEditing()

        self.assertEqual(layer.editBuffer().changedAttributeValues(), {})
        self.assertFalse(layer.editBuffer().isFeatureAttributesChanged(1))
        self.assertFalse(layer.editBuffer().isFeatureAttributesChanged(2))

        # change attribute values
        layer.changeAttributeValue(1, 0, 'a')

        # test contents of buffer
        self.assertEqual(list(layer.editBuffer().changedAttributeValues().keys()), [1])
        self.assertEqual(layer.editBuffer().changedAttributeValues()[1], {0: 'a'})
        self.assertTrue(layer.editBuffer().isFeatureAttributesChanged(1))
        self.assertFalse(layer.editBuffer().isFeatureAttributesChanged(2))

        layer.changeAttributeValue(2, 1, 5)

        # test contents of buffer
        self.assertEqual(set(layer.editBuffer().changedAttributeValues().keys()), set([1, 2]))
        self.assertEqual(layer.editBuffer().changedAttributeValues()[1], {0: 'a'})
        self.assertEqual(layer.editBuffer().changedAttributeValues()[2], {1: 5})
        self.assertTrue(layer.editBuffer().isFeatureAttributesChanged(1))
        self.assertTrue(layer.editBuffer().isFeatureAttributesChanged(2))

    def testChangeGeometry(self):
        # test changing geometries values from an edit buffer

        # make a layer with two features
        layer = createEmptyLayer()
        self.assertTrue(layer.startEditing())

        # add two features
        f1 = QgsFeature(layer.fields(), 1)
        f1.setGeometry(QgsGeometry.fromPoint(QgsPoint(1, 2)))
        f1.setAttributes(["test", 123])
        self.assertTrue(layer.addFeature(f1))

        f2 = QgsFeature(layer.fields(), 2)
        f2.setGeometry(QgsGeometry.fromPoint(QgsPoint(2, 4)))
        f2.setAttributes(["test2", 246])
        self.assertTrue(layer.addFeature(f2))

        layer.commitChanges()
        layer.startEditing()

        self.assertEqual(layer.editBuffer().changedGeometries(), {})
        self.assertFalse(layer.editBuffer().isFeatureGeometryChanged(1))
        self.assertFalse(layer.editBuffer().isFeatureGeometryChanged(2))

        # change attribute values
        layer.changeGeometry(1, QgsGeometry.fromPoint(QgsPoint(10, 20)))

        # test contents of buffer
        self.assertEqual(list(layer.editBuffer().changedGeometries().keys()), [1])
        self.assertEqual(layer.editBuffer().changedGeometries()[1].geometry().x(), 10)
        self.assertTrue(layer.editBuffer().isFeatureGeometryChanged(1))
        self.assertFalse(layer.editBuffer().isFeatureGeometryChanged(2))

        layer.changeGeometry(2, QgsGeometry.fromPoint(QgsPoint(20, 40)))

        # test contents of buffer
        self.assertEqual(set(layer.editBuffer().changedGeometries().keys()), set([1, 2]))
        self.assertEqual(layer.editBuffer().changedGeometries()[1].geometry().x(), 10)
        self.assertEqual(layer.editBuffer().changedGeometries()[2].geometry().x(), 20)
        self.assertTrue(layer.editBuffer().isFeatureGeometryChanged(1))
        self.assertTrue(layer.editBuffer().isFeatureGeometryChanged(2))

    def testDeleteAttribute(self):
        # test deleting attributes from an edit buffer

        layer = createEmptyLayer()
        layer.startEditing()

        self.assertEqual(layer.editBuffer().deletedAttributeIds(), [])
        self.assertFalse(layer.editBuffer().isAttributeDeleted(0))
        self.assertFalse(layer.editBuffer().isAttributeDeleted(1))

        # delete attribute
        layer.deleteAttribute(0)

        # test contents of buffer
        self.assertEqual(layer.editBuffer().deletedAttributeIds(), [0])
        self.assertTrue(layer.editBuffer().isAttributeDeleted(0))
        self.assertFalse(layer.editBuffer().isAttributeDeleted(1))

        # delete remaining attribute (now at position 0)
        layer.deleteAttribute(0)

        # test contents of buffer
        self.assertEqual(layer.editBuffer().deletedAttributeIds(), [0, 1])
        self.assertTrue(layer.editBuffer().isAttributeDeleted(0))
        self.assertTrue(layer.editBuffer().isAttributeDeleted(1))

    def testAddAttribute(self):
        # test adding attributes to an edit buffer

        layer = createEmptyLayer()
        layer.startEditing()

        self.assertEqual(layer.editBuffer().addedAttributes(), [])

        # add attribute
        layer.addAttribute(QgsField('new1', QVariant.String))

        # test contents of buffer
        self.assertEqual(layer.editBuffer().addedAttributes()[0].name(), 'new1')

        # add another attribute
        layer.addAttribute(QgsField('new2', QVariant.String))

        # test contents of buffer
        self.assertEqual(layer.editBuffer().addedAttributes()[0].name(), 'new1')
        self.assertEqual(layer.editBuffer().addedAttributes()[1].name(), 'new2')

if __name__ == '__main__':
    unittest.main()
