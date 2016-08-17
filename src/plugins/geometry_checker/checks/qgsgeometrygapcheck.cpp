/***************************************************************************
    qgsgeometrygapcheck.cpp
    ---------------------
    begin                : September 2015
    copyright            : (C) 2014 by Sandro Mani / Sourcepole AG
    email                : smani at sourcepole dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsgeometryengine.h"
#include "qgsgeometrygapcheck.h"
#include "qgsgeometrycollection.h"
#include "../utils/qgsfeaturepool.h"


void QgsGeometryGapCheck::collectErrors( QList<QgsGeometryCheckError*>& errors, QStringList &messages, QAtomicInt* progressCounter , const QgsFeatureIds &ids ) const
{
  Q_ASSERT( mFeaturePool->getLayer()->geometryType() == QgsWkbTypes::PolygonGeometry );
  if ( progressCounter ) progressCounter->fetchAndAddRelaxed( 1 );

  // Collect geometries, build spatial index
  QList<QgsAbstractGeometry*> geomList;
  const QgsFeatureIds& featureIds = ids.isEmpty() ? mFeaturePool->getFeatureIds() : ids;
  Q_FOREACH ( QgsFeatureId id, featureIds )
  {
    QgsFeature feature;
    if ( mFeaturePool->get( id, feature ) )
    {
      geomList.append( feature.geometry().geometry()->clone() );
    }
  }

  if ( geomList.isEmpty() )
  {
    return;
  }

  QgsGeometryEngine* geomEngine = QgsGeometryCheckerUtils::createGeomEngine( nullptr, QgsGeometryCheckPrecision::tolerance() );

  // Create union of geometry
  QString errMsg;
  QgsAbstractGeometry* unionGeom = geomEngine->combine( geomList, &errMsg );
  qDeleteAll( geomList );
  delete geomEngine;
  if ( !unionGeom )
  {
    messages.append( tr( "Gap check: %1" ).arg( errMsg ) );
    return;
  }

  // Get envelope of union
  geomEngine = QgsGeometryCheckerUtils::createGeomEngine( unionGeom, QgsGeometryCheckPrecision::tolerance() );
  QgsAbstractGeometry* envelope = geomEngine->envelope( &errMsg );
  delete geomEngine;
  if ( !envelope )
  {
    messages.append( tr( "Gap check: %1" ).arg( errMsg ) );
    delete unionGeom;
    return;
  }

  // Buffer envelope
  geomEngine = QgsGeometryCheckerUtils::createGeomEngine( envelope, QgsGeometryCheckPrecision::tolerance() );
  QgsAbstractGeometry* bufEnvelope = geomEngine->buffer( 2, 0, GEOSBUF_CAP_SQUARE, GEOSBUF_JOIN_MITRE, 4. );
  delete geomEngine;
  delete envelope;
  envelope = bufEnvelope;

  // Compute difference between envelope and union to obtain gap polygons
  geomEngine = QgsGeometryCheckerUtils::createGeomEngine( envelope, QgsGeometryCheckPrecision::tolerance() );
  QgsAbstractGeometry* diffGeom = geomEngine->difference( *unionGeom, &errMsg );
  delete geomEngine;
  if ( !diffGeom )
  {
    messages.append( tr( "Gap check: %1" ).arg( errMsg ) );
    delete unionGeom;
    delete diffGeom;
    return;
  }

  // For each gap polygon which does not lie on the boundary, get neighboring polygons and add error
  for ( int iPart = 0, nParts = diffGeom->partCount(); iPart < nParts; ++iPart )
  {
    QgsAbstractGeometry* geom = QgsGeometryCheckerUtils::getGeomPart( diffGeom, iPart );
    // Skip the gap between features and boundingbox
    if ( geom->boundingBox() == envelope->boundingBox() )
    {
      continue;
    }

    // Skip gaps above threshold
    if ( geom->area() > mThreshold || geom->area() < QgsGeometryCheckPrecision::reducedTolerance() )
    {
      continue;
    }

    // Get neighboring polygons
    QgsFeatureIds neighboringIds;
    QgsRectangle gapAreaBBox = geom->boundingBox();
    QgsFeatureIds intersectIds = mFeaturePool->getIntersects( geom->boundingBox() );

    Q_FOREACH ( QgsFeatureId id, intersectIds )
    {
      QgsFeature feature;
      if ( !mFeaturePool->get( id, feature ) )
      {
        continue;
      }
      QgsGeometry featureGeom = feature.geometry();
      QgsAbstractGeometry* geom2 = featureGeom.geometry();
      if ( QgsGeometryCheckerUtils::sharedEdgeLength( geom, geom2, QgsGeometryCheckPrecision::reducedTolerance() ) > 0 )
      {
        neighboringIds.insert( feature.id() );
        gapAreaBBox.unionRect( geom2->boundingBox() );
      }
    }
    if ( neighboringIds.isEmpty() )
    {
      delete geom;
      continue;
    }

    // Add error
    errors.append( new QgsGeometryGapCheckError( this, geom->clone(), neighboringIds, geom->area(), gapAreaBBox ) );
  }

  delete unionGeom;
  delete envelope;
  delete diffGeom;
}

void QgsGeometryGapCheck::fixError( QgsGeometryCheckError* error, int method, int /*mergeAttributeIndex*/, Changes &changes ) const
{
  if ( method == NoChange )
  {
    error->setFixed( method );
  }
  else if ( method == MergeLongestEdge )
  {
    QString errMsg;
    if ( mergeWithNeighbor( static_cast<QgsGeometryGapCheckError*>( error ), changes, errMsg ) )
    {
      error->setFixed( method );
    }
    else
    {
      error->setFixFailed( tr( "Failed to merge with neighbor: %1" ).arg( errMsg ) );
    }
  }
  else
  {
    error->setFixFailed( tr( "Unknown method" ) );
  }
}

bool QgsGeometryGapCheck::mergeWithNeighbor( QgsGeometryGapCheckError* err, Changes &changes, QString& errMsg ) const
{
  double maxVal = 0.;
  QgsFeature mergeFeature;
  int mergePartIdx = -1;

  QgsAbstractGeometry* errGeometry = QgsGeometryCheckerUtils::getGeomPart( err->geometry(), 0 );

  // Search for touching neighboring geometries
  Q_FOREACH ( QgsFeatureId testId, err->neighbors() )
  {
    QgsFeature testFeature;
    if ( !mFeaturePool->get( testId, testFeature ) )
    {
      continue;
    }
    QgsGeometry featureGeom = testFeature.geometry();
    QgsAbstractGeometry* testGeom = featureGeom.geometry();
    for ( int iPart = 0, nParts = testGeom->partCount(); iPart < nParts; ++iPart )
    {
      double len = QgsGeometryCheckerUtils::sharedEdgeLength( errGeometry, QgsGeometryCheckerUtils::getGeomPart( testGeom, iPart ), QgsGeometryCheckPrecision::reducedTolerance() );
      if ( len > maxVal )
      {
        maxVal = len;
        mergeFeature = testFeature;
        mergePartIdx = iPart;
      }
    }
  }

  if ( maxVal == 0. )
  {
    return false;
  }

  // Merge geometries
  QgsGeometry mergeFeatureGeom = mergeFeature.geometry();
  QgsAbstractGeometry* mergeGeom = mergeFeatureGeom.geometry();
  QgsGeometryEngine* geomEngine = QgsGeometryCheckerUtils::createGeomEngine( errGeometry, QgsGeometryCheckPrecision::tolerance() );
  QgsAbstractGeometry* combinedGeom = geomEngine->combine( *QgsGeometryCheckerUtils::getGeomPart( mergeGeom, mergePartIdx ), &errMsg );
  delete geomEngine;
  if ( !combinedGeom || combinedGeom->isEmpty() )
  {
    delete combinedGeom;
    return false;
  }

  // Add merged polygon to destination geometry
  replaceFeatureGeometryPart( mergeFeature, mergePartIdx, combinedGeom, changes );

  return true;
}


const QStringList& QgsGeometryGapCheck::getResolutionMethods() const
{
  static QStringList methods = QStringList() << tr( "Add gap area to neighboring polygon with longest shared edge" ) << tr( "No action" );
  return methods;
}
