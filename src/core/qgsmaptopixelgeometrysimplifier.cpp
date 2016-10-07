/***************************************************************************
    qgsmaptopixelgeometrysimplifier.cpp
    ---------------------
    begin                : December 2013
    copyright            : (C) 2013 by Alvaro Huarte
    email                : http://wiki.osgeo.org/wiki/Alvaro_Huarte

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <limits>

#include "qgsmaptopixelgeometrysimplifier.h"
#include "qgsapplication.h"
#include "qgslogger.h"
#include "qgsrectangle.h"
#include "qgswkbptr.h"
#include "qgsgeometry.h"
#include "qgslinestring.h"
#include "qgspolygon.h"
#include "qgsgeometrycollection.h"

QgsMapToPixelSimplifier::QgsMapToPixelSimplifier( int simplifyFlags, double tolerance, SimplifyAlgorithm simplifyAlgorithm )
    : mSimplifyFlags( simplifyFlags )
    , mSimplifyAlgorithm( simplifyAlgorithm )
    , mTolerance( tolerance )
{
}

QgsMapToPixelSimplifier::~QgsMapToPixelSimplifier()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Helper simplification methods

//! Returns the squared 2D-distance of the vector defined by the two points specified
float QgsMapToPixelSimplifier::calculateLengthSquared2D( double x1, double y1, double x2, double y2 )
{
  float vx = static_cast< float >( x2 - x1 );
  float vy = static_cast< float >( y2 - y1 );

  return ( vx * vx ) + ( vy * vy );
}

//! Returns whether the points belong to the same grid
bool QgsMapToPixelSimplifier::equalSnapToGrid( double x1, double y1, double x2, double y2, double gridOriginX, double gridOriginY, float gridInverseSizeXY )
{
  int grid_x1 = qRound(( x1 - gridOriginX ) * gridInverseSizeXY );
  int grid_x2 = qRound(( x2 - gridOriginX ) * gridInverseSizeXY );
  if ( grid_x1 != grid_x2 ) return false;

  int grid_y1 = qRound(( y1 - gridOriginY ) * gridInverseSizeXY );
  int grid_y2 = qRound(( y2 - gridOriginY ) * gridInverseSizeXY );
  if ( grid_y1 != grid_y2 ) return false;

  return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Helper simplification methods for Visvalingam method

// It uses a refactored code of the liblwgeom implementation:
// https://github.com/postgis/postgis/blob/svn-trunk/liblwgeom/effectivearea.h
// https://github.com/postgis/postgis/blob/svn-trunk/liblwgeom/effectivearea.c

#include "simplify/effectivearea.h"

//////////////////////////////////////////////////////////////////////////////////////////////

//! Generalize the WKB-geometry using the BBOX of the original geometry
static QgsGeometry generalizeWkbGeometryByBoundingBox(
  QgsWkbTypes::Type wkbType,
  const QgsAbstractGeometry& geometry,
  const QgsRectangle &envelope )
{
  unsigned int geometryType = QgsWkbTypes::singleType( QgsWkbTypes::flatType( wkbType ) );

  // If the geometry is already minimal skip the generalization
  int minimumSize = geometryType == QgsWkbTypes::LineString ? 2 : 5;

  if ( geometry.nCoordinates() <= minimumSize )
  {
    return QgsGeometry( geometry.clone() );
  }

  const double x1 = envelope.xMinimum();
  const double y1 = envelope.yMinimum();
  const double x2 = envelope.xMaximum();
  const double y2 = envelope.yMaximum();

  // Write the generalized geometry
  if ( geometryType == QgsWkbTypes::LineString )
  {
    QgsLineString* lineString = new QgsLineString();
    lineString->addVertex( QgsPointV2( x1, y1 ) );
    lineString->addVertex( QgsPointV2( x2, y2 ) );
    return QgsGeometry( lineString );
  }
  else
  {
    return QgsGeometry::fromRect( envelope );
  }
}

template<class T>
static T* createEmptySameTypeGeom( const T& geom )
{
  T* output( dynamic_cast<T*>( geom.clone() ) );
  output->clear();
  return output;
}

//! Simplify the WKB-geometry using the specified tolerance
QgsGeometry QgsMapToPixelSimplifier::simplifyGeometry(
  int simplifyFlags,
  SimplifyAlgorithm simplifyAlgorithm,
  QgsWkbTypes::Type wkbType,
  const QgsAbstractGeometry& geometry,
  const QgsRectangle &envelope, double map2pixelTol,
  bool isaLinearRing )
{
  bool isGeneralizable = true;

  // Can replace the geometry by its BBOX ?
  if (( simplifyFlags & QgsMapToPixelSimplifier::SimplifyEnvelope ) &&
      isGeneralizableByMapBoundingBox( envelope, map2pixelTol ) )
  {
    return generalizeWkbGeometryByBoundingBox( wkbType, geometry, envelope );
  }

  if ( !( simplifyFlags & QgsMapToPixelSimplifier::SimplifyGeometry ) )
    isGeneralizable = false;

  const QgsWkbTypes::Type flatType = QgsWkbTypes::flatType( wkbType );

  // Write the geometry
  if ( flatType == QgsWkbTypes::LineString || flatType == QgsWkbTypes::CircularString )
  {
    const QgsCurve& srcCurve = dynamic_cast<const QgsCurve&>( geometry );
    QScopedPointer<QgsCurve> output( createEmptySameTypeGeom( srcCurve ) );
    double x = 0.0, y = 0.0, lastX = 0.0, lastY = 0.0;
    QgsRectangle r;
    r.setMinimal();

    const int numPoints = srcCurve.numPoints();

    if ( numPoints <= ( isaLinearRing ? 4 : 2 ) )
      isGeneralizable = false;

    bool isLongSegment;
    bool hasLongSegments = false; //-> To avoid replace the simplified geometry by its BBOX when there are 'long' segments.

    // Check whether the LinearRing is really closed.
    if ( isaLinearRing )
    {
      isaLinearRing = qgsDoubleNear( srcCurve.xAt( 0 ), srcCurve.xAt( numPoints - 1 ) ) &&
                      qgsDoubleNear( srcCurve.yAt( 0 ), srcCurve.yAt( numPoints - 1 ) );
    }

    // Process each vertex...
    switch ( simplifyAlgorithm )
    {
      case SnapToGrid:
      {
        double gridOriginX = envelope.xMinimum();
        double gridOriginY = envelope.yMinimum();

        // Use a factor for the maximum displacement distance for simplification, similar as GeoServer does
        float gridInverseSizeXY = map2pixelTol != 0 ? ( float )( 1.0f / ( 0.8 * map2pixelTol ) ) : 0.0f;

        for ( int i = 0; i < numPoints; ++i )
        {
          x = srcCurve.xAt( i );
          y = srcCurve.yAt( i );

          if ( i == 0 ||
               !isGeneralizable ||
               !equalSnapToGrid( x, y, lastX, lastY, gridOriginX, gridOriginY, gridInverseSizeXY ) ||
               ( !isaLinearRing && ( i == 1 || i >= numPoints - 2 ) ) )
          {
            output->insertVertex( QgsVertexId( 0, 0, output->numPoints() ), QgsPointV2( x, y ) );
            lastX = x;
            lastY = y;
          }

          r.combineExtentWith( x, y );
        }
        break;
      }

      case Visvalingam:
      {
        map2pixelTol *= map2pixelTol; //-> Use mappixelTol for 'Area' calculations.

        EFFECTIVE_AREAS ea( srcCurve );

        int set_area = 0;
        ptarray_calc_areas( &ea, isaLinearRing ? 4 : 2, set_area, map2pixelTol );

        for ( int i = 0; i < numPoints; ++i )
        {
          if ( ea.res_arealist[ i ] > map2pixelTol )
          {
            output->insertVertex( QgsVertexId( 0, 0, output->numPoints() ), ea.inpts.at( i ) );
          }
        }
        break;
      }

      case Distance:
      {
        map2pixelTol *= map2pixelTol; //-> Use mappixelTol for 'LengthSquare' calculations.

        for ( int i = 0; i < numPoints; ++i )
        {
          x = srcCurve.xAt( i );
          y = srcCurve.yAt( i );

          isLongSegment = false;

          if ( i == 0 ||
               !isGeneralizable ||
               ( isLongSegment = ( calculateLengthSquared2D( x, y, lastX, lastY ) > map2pixelTol ) ) ||
               ( !isaLinearRing && ( i == 1 || i >= numPoints - 2 ) ) )
          {
            output->insertVertex( QgsVertexId( 0, 0, output->numPoints() ), QgsPointV2( x, y ) );
            lastX = x;
            lastY = y;

            hasLongSegments |= isLongSegment;
          }

          r.combineExtentWith( x, y );
        }
      }
    }

    if ( output->numPoints() < ( isaLinearRing ? 4 : 2 ) )
    {
      // we simplified the geometry too much!
      if ( !hasLongSegments )
      {
        // approximate the geometry's shape by its bounding box
        // (rect for linear ring / one segment for line string)
        return generalizeWkbGeometryByBoundingBox( wkbType, geometry, r );
      }
      else
      {
        // Bad luck! The simplified geometry is invalid and approximation by bounding box
        // would create artifacts due to long segments.
        // We will return the original geometry
        return QgsGeometry( geometry.clone() );
      }
    }

    if ( isaLinearRing )
    {
      // make sure we keep the linear ring closed
      if ( !qgsDoubleNear( lastX, output->xAt( 0 ) ) || !qgsDoubleNear( lastY, output->yAt( 0 ) ) )
      {
        output->insertVertex( QgsVertexId( 0, 0, output->numPoints() ), QgsPointV2( output->xAt( 0 ), output->yAt( 0 ) ) );
      }
    }

    return QgsGeometry( output.take() );
  }
  else if ( flatType == QgsWkbTypes::Polygon )
  {
    const QgsPolygonV2& srcPolygon = dynamic_cast<const QgsPolygonV2&>( geometry );
    QScopedPointer<QgsPolygonV2> polygon( new QgsPolygonV2() );
    polygon->setExteriorRing( dynamic_cast<QgsCurve*>( simplifyGeometry( simplifyFlags, simplifyAlgorithm, srcPolygon.exteriorRing()->wkbType(), *srcPolygon.exteriorRing(), envelope, map2pixelTol, true ).geometry()->clone() ) );
    for ( int i = 0; i < srcPolygon.numInteriorRings(); ++i )
    {
      const QgsCurve* sub = srcPolygon.interiorRing( i );
      polygon->addInteriorRing( dynamic_cast<QgsCurve*>( simplifyGeometry( simplifyFlags, simplifyAlgorithm, sub->wkbType(), *sub, envelope, map2pixelTol, true ).geometry()->clone() ) );
    }
    return QgsGeometry( polygon.take() );
  }
  else if ( QgsWkbTypes::isMultiType( flatType ) )
  {
    const QgsGeometryCollection& srcCollection = dynamic_cast<const QgsGeometryCollection&>( geometry );
    QScopedPointer<QgsGeometryCollection> collection( createEmptySameTypeGeom( srcCollection ) );
    const int numGeoms = srcCollection.numGeometries();
    for ( int i = 0; i < numGeoms; ++i )
    {
      const QgsAbstractGeometry* sub = srcCollection.geometryN( i );
      collection->addGeometry( simplifyGeometry( simplifyFlags, simplifyAlgorithm, sub->wkbType(), *sub, envelope, map2pixelTol, false ).geometry()->clone() );
    }
    return QgsGeometry( collection.take() );
  }
  return QgsGeometry( geometry.clone() );
}

//////////////////////////////////////////////////////////////////////////////////////////////

//! Returns whether the envelope can be replaced by its BBOX when is applied the specified map2pixel context
bool QgsMapToPixelSimplifier::isGeneralizableByMapBoundingBox( const QgsRectangle& envelope, double map2pixelTol )
{
  // Can replace the geometry by its BBOX ?
  return envelope.width() < map2pixelTol && envelope.height() < map2pixelTol;
}

//! Returns a simplified version the specified geometry (Removing duplicated points) when is applied the specified map2pixel context
QgsGeometry QgsMapToPixelSimplifier::simplify( const QgsGeometry& geometry ) const
{
  if ( geometry.isEmpty() )
  {
    return QgsGeometry();
  }
  if ( mSimplifyFlags == QgsMapToPixelSimplifier::NoFlags )
  {
    return geometry;
  }

  // Check whether the geometry can be simplified using the map2pixel context
  const QgsWkbTypes::Type singleType = QgsWkbTypes::singleType( geometry.wkbType() );
  const QgsWkbTypes::Type flatType = QgsWkbTypes::flatType( singleType );
  if ( flatType == QgsWkbTypes::Point )
  {
    return geometry;
  }

  const bool isaLinearRing = flatType == QgsWkbTypes::Polygon;
  const int numPoints = geometry.geometry()->nCoordinates();

  if ( numPoints <= ( isaLinearRing ? 6 : 3 ) )
  {
    // No simplify simple geometries
    return geometry;
  }

  const QgsRectangle envelope = geometry.boundingBox();
  if ( qMax( envelope.width(), envelope.height() ) / numPoints > mTolerance * 2.0 )
  {
    //points are in average too far appart to lead to any significant simplification
    return geometry;
  }

  return simplifyGeometry( mSimplifyFlags, mSimplifyAlgorithm, geometry.wkbType(), *geometry.geometry(), envelope, mTolerance, false );
}
