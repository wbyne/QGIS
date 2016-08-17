/***************************************************************************
                         qgslinestring.cpp
                         -------------------
    begin                : September 2014
    copyright            : (C) 2014 by Marco Hugentobler
    email                : marco at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgslinestring.h"
#include "qgsapplication.h"
#include "qgscompoundcurve.h"
#include "qgscoordinatetransform.h"
#include "qgsgeometryutils.h"
#include "qgsmaptopixel.h"
#include "qgswkbptr.h"

#include <QPainter>
#include <limits>
#include <QDomDocument>
#include <QtCore/qmath.h>


/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

QgsLineString::QgsLineString(): QgsCurve()
{
  mWkbType = QgsWkbTypes::LineString;
}

QgsLineString::~QgsLineString()
{}

bool QgsLineString::operator==( const QgsCurve& other ) const
{
  const QgsLineString* otherLine = dynamic_cast< const QgsLineString* >( &other );
  if ( !otherLine )
    return false;

  if ( mWkbType != otherLine->mWkbType )
    return false;

  if ( mX.count() != otherLine->mX.count() )
    return false;

  for ( int i = 0; i < mX.count(); ++i )
  {
    if ( !qgsDoubleNear( mX.at( i ), otherLine->mX.at( i ) )
         || !qgsDoubleNear( mY.at( i ), otherLine->mY.at( i ) ) )
      return false;

    if ( is3D() && !qgsDoubleNear( mZ.at( i ), otherLine->mZ.at( i ) ) )
      return false;

    if ( isMeasure() && !qgsDoubleNear( mM.at( i ), otherLine->mM.at( i ) ) )
      return false;
  }

  return true;
}

bool QgsLineString::operator!=( const QgsCurve& other ) const
{
  return !operator==( other );
}

QgsLineString *QgsLineString::clone() const
{
  return new QgsLineString( *this );
}

void QgsLineString::clear()
{
  mX.clear();
  mY.clear();
  mZ.clear();
  mM.clear();
  mWkbType = QgsWkbTypes::LineString;
  clearCache();
}

bool QgsLineString::fromWkb( QgsConstWkbPtr wkbPtr )
{
  if ( !wkbPtr )
  {
    return false;
  }

  QgsWkbTypes::Type type = wkbPtr.readHeader();
  if ( QgsWkbTypes::flatType( type ) != QgsWkbTypes::LineString )
  {
    return false;
  }
  mWkbType = type;
  importVerticesFromWkb( wkbPtr );
  return true;
}

void QgsLineString::fromWkbPoints( QgsWkbTypes::Type type, const QgsConstWkbPtr& wkb )
{
  mWkbType = type;
  importVerticesFromWkb( wkb );
}

QgsRectangle QgsLineString::calculateBoundingBox() const
{
  double xmin = std::numeric_limits<double>::max();
  double ymin = std::numeric_limits<double>::max();
  double xmax = -std::numeric_limits<double>::max();
  double ymax = -std::numeric_limits<double>::max();

  Q_FOREACH ( double x, mX )
  {
    if ( x < xmin )
      xmin = x;
    if ( x > xmax )
      xmax = x;
  }
  Q_FOREACH ( double y, mY )
  {
    if ( y < ymin )
      ymin = y;
    if ( y > ymax )
      ymax = y;
  }
  return QgsRectangle( xmin, ymin, xmax, ymax );
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

bool QgsLineString::fromWkt( const QString& wkt )
{
  clear();

  QPair<QgsWkbTypes::Type, QString> parts = QgsGeometryUtils::wktReadBlock( wkt );

  if ( QgsWkbTypes::flatType( parts.first ) != QgsWkbTypes::LineString )
    return false;
  mWkbType = parts.first;

  setPoints( QgsGeometryUtils::pointsFromWKT( parts.second, is3D(), isMeasure() ) );
  return true;
}

int QgsLineString::wkbSize() const
{
  int size = sizeof( char ) + sizeof( quint32 ) + sizeof( quint32 );
  size += numPoints() * ( 2 + is3D() + isMeasure() ) * sizeof( double );
  return size;
}

unsigned char* QgsLineString::asWkb( int& binarySize ) const
{
  binarySize = wkbSize();
  unsigned char* geomPtr = new unsigned char[binarySize];
  QgsWkbPtr wkb( geomPtr, binarySize );
  wkb << static_cast<char>( QgsApplication::endian() );
  wkb << static_cast<quint32>( wkbType() );
  QgsPointSequence pts;
  points( pts );
  QgsGeometryUtils::pointsToWKB( wkb, pts, is3D(), isMeasure() );
  return geomPtr;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

QString QgsLineString::asWkt( int precision ) const
{
  QString wkt = wktTypeStr() + ' ';
  QgsPointSequence pts;
  points( pts );
  wkt += QgsGeometryUtils::pointsToWKT( pts, precision, is3D(), isMeasure() );
  return wkt;
}

QDomElement QgsLineString::asGML2( QDomDocument& doc, int precision, const QString& ns ) const
{
  QgsPointSequence pts;
  points( pts );

  QDomElement elemLineString = doc.createElementNS( ns, "LineString" );
  elemLineString.appendChild( QgsGeometryUtils::pointsToGML2( pts, doc, precision, ns ) );

  return elemLineString;
}

QDomElement QgsLineString::asGML3( QDomDocument& doc, int precision, const QString& ns ) const
{
  QgsPointSequence pts;
  points( pts );

  QDomElement elemCurve = doc.createElementNS( ns, "Curve" );
  QDomElement elemSegments = doc.createElementNS( ns, "segments" );
  QDomElement elemArcString = doc.createElementNS( ns, "LineStringSegment" );
  elemArcString.appendChild( QgsGeometryUtils::pointsToGML3( pts, doc, precision, ns, is3D() ) );
  elemSegments.appendChild( elemArcString );
  elemCurve.appendChild( elemSegments );

  return elemCurve;
}

QString QgsLineString::asJSON( int precision ) const
{
  QgsPointSequence pts;
  points( pts );

  return "{\"type\": \"LineString\", \"coordinates\": " + QgsGeometryUtils::pointsToJSON( pts, precision ) + '}';
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

double QgsLineString::length() const
{
  double length = 0;
  int size = mX.size();
  double dx, dy;
  for ( int i = 1; i < size; ++i )
  {
    dx = mX.at( i ) - mX.at( i - 1 );
    dy = mY.at( i ) - mY.at( i - 1 );
    length += sqrt( dx * dx + dy * dy );
  }
  return length;
}

QgsPointV2 QgsLineString::startPoint() const
{
  if ( numPoints() < 1 )
  {
    return QgsPointV2();
  }
  return pointN( 0 );
}

QgsPointV2 QgsLineString::endPoint() const
{
  if ( numPoints() < 1 )
  {
    return QgsPointV2();
  }
  return pointN( numPoints() - 1 );
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

QgsLineString* QgsLineString::curveToLine( double tolerance, SegmentationToleranceType toleranceType ) const
{
  Q_UNUSED( tolerance );
  Q_UNUSED( toleranceType );
  return static_cast<QgsLineString*>( clone() );
}

int QgsLineString::numPoints() const
{
  return mX.size();
}

QgsPointV2 QgsLineString::pointN( int i ) const
{
  if ( i < 0 || i >= mX.size() )
  {
    return QgsPointV2();
  }

  double x = mX.at( i );
  double y = mY.at( i );
  double z = 0;
  double m = 0;

  bool hasZ = is3D();
  if ( hasZ )
  {
    z = mZ.at( i );
  }
  bool hasM = isMeasure();
  if ( hasM )
  {
    m = mM.at( i );
  }

  QgsWkbTypes::Type t = QgsWkbTypes::Point;
  if ( mWkbType == QgsWkbTypes::LineString25D )
  {
    t = QgsWkbTypes::Point25D;
  }
  else if ( hasZ && hasM )
  {
    t = QgsWkbTypes::PointZM;
  }
  else if ( hasZ )
  {
    t = QgsWkbTypes::PointZ;
  }
  else if ( hasM )
  {
    t = QgsWkbTypes::PointM;
  }
  return QgsPointV2( t, x, y, z, m );
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

double QgsLineString::xAt( int index ) const
{
  if ( index >= 0 && index < mX.size() )
    return mX.at( index );
  else
    return 0.0;
}

double QgsLineString::yAt( int index ) const
{
  if ( index >= 0 && index < mY.size() )
    return mY.at( index );
  else
    return 0.0;
}

double QgsLineString::zAt( int index ) const
{
  if ( index >= 0 && index < mZ.size() )
    return mZ.at( index );
  else
    return 0.0;
}

double QgsLineString::mAt( int index ) const
{
  if ( index >= 0 && index < mM.size() )
    return mM.at( index );
  else
    return 0.0;
}

void QgsLineString::setXAt( int index, double x )
{
  if ( index >= 0 && index < mX.size() )
    mX[ index ] = x;
  clearCache();
}

void QgsLineString::setYAt( int index, double y )
{
  if ( index >= 0 && index < mY.size() )
    mY[ index ] = y;
  clearCache();
}

void QgsLineString::setZAt( int index, double z )
{
  if ( index >= 0 && index < mZ.size() )
    mZ[ index ] = z;
}

void QgsLineString::setMAt( int index, double m )
{
  if ( index >= 0 && index < mM.size() )
    mM[ index ] = m;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

void QgsLineString::points( QgsPointSequence &pts ) const
{
  pts.clear();
  int nPoints = numPoints();
  for ( int i = 0; i < nPoints; ++i )
  {
    pts.push_back( pointN( i ) );
  }
}

void QgsLineString::setPoints( const QgsPointSequence &points )
{
  clearCache(); //set bounding box invalid

  if ( points.isEmpty() )
  {
    clear();
    return;
  }

  //get wkb type from first point
  const QgsPointV2& firstPt = points.at( 0 );
  bool hasZ = firstPt.is3D();
  bool hasM = firstPt.isMeasure();

  setZMTypeFromSubGeometry( &firstPt, QgsWkbTypes::LineString );

  mX.resize( points.size() );
  mY.resize( points.size() );
  if ( hasZ )
  {
    mZ.resize( points.size() );
  }
  else
  {
    mZ.clear();
  }
  if ( hasM )
  {
    mM.resize( points.size() );
  }
  else
  {
    mM.clear();
  }

  for ( int i = 0; i < points.size(); ++i )
  {
    mX[i] = points.at( i ).x();
    mY[i] = points.at( i ).y();
    if ( hasZ )
    {
      mZ[i] = points.at( i ).z();
    }
    if ( hasM )
    {
      mM[i] = points.at( i ).m();
    }
  }
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

void QgsLineString::append( const QgsLineString* line )
{
  if ( !line )
  {
    return;
  }

  if ( numPoints() < 1 )
  {
    setZMTypeFromSubGeometry( line, QgsWkbTypes::LineString );
  }

  // do not store duplicit points
  if ( numPoints() > 0 &&
       line->numPoints() > 0 &&
       endPoint() == line->startPoint() )
  {
    mX.pop_back();
    mY.pop_back();

    if ( is3D() )
    {
      mZ.pop_back();
    }
    if ( isMeasure() )
    {
      mM.pop_back();
    }
  }

  mX += line->mX;
  mY += line->mY;

  if ( is3D() )
  {
    if ( line->is3D() )
    {
      mZ += line->mZ;
    }
    else
    {
      // if append line does not have z coordinates, fill with 0 to match number of points in final line
      mZ.insert( mZ.count(), mX.size() - mZ.size(), 0 );
    }
  }

  if ( isMeasure() )
  {
    if ( line->isMeasure() )
    {
      mM += line->mM;
    }
    else
    {
      // if append line does not have m values, fill with 0 to match number of points in final line
      mM.insert( mM.count(), mX.size() - mM.size(), 0 );
    }
  }

  clearCache(); //set bounding box invalid
}

QgsLineString* QgsLineString::reversed() const
{
  QgsLineString* copy = clone();
  std::reverse( copy->mX.begin(), copy->mX.end() );
  std::reverse( copy->mY.begin(), copy->mY.end() );
  if ( copy->is3D() )
  {
    std::reverse( copy->mZ.begin(), copy->mZ.end() );
  }
  if ( copy->isMeasure() )
  {
    std::reverse( copy->mM.begin(), copy->mM.end() );
  }
  return copy;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

void QgsLineString::draw( QPainter& p ) const
{
  p.drawPolyline( asQPolygonF() );
}

void QgsLineString::addToPainterPath( QPainterPath& path ) const
{
  int nPoints = numPoints();
  if ( nPoints < 1 )
  {
    return;
  }

  if ( path.isEmpty() || path.currentPosition() != QPointF( mX.at( 0 ), mY.at( 0 ) ) )
  {
    path.moveTo( mX.at( 0 ), mY.at( 0 ) );
  }

  for ( int i = 1; i < nPoints; ++i )
  {
    path.lineTo( mX.at( i ), mY.at( i ) );
  }
}

void QgsLineString::drawAsPolygon( QPainter& p ) const
{
  p.drawPolygon( asQPolygonF() );
}

QPolygonF QgsLineString::asQPolygonF() const
{
  QPolygonF points;
  for ( int i = 0; i < mX.count(); ++i )
  {
    points << QPointF( mX.at( i ), mY.at( i ) );
  }
  return points;
}

QgsAbstractGeometry* QgsLineString::toCurveType() const
{
  QgsCompoundCurve* compoundCurve = new QgsCompoundCurve();
  compoundCurve->addCurve( clone() );
  return compoundCurve;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

void QgsLineString::transform( const QgsCoordinateTransform& ct, QgsCoordinateTransform::TransformDirection d, bool transformZ )
{
  double* zArray = mZ.data();

  bool hasZ = is3D();
  int nPoints = numPoints();
  bool useDummyZ = !hasZ || !transformZ;
  if ( useDummyZ )
  {
    zArray = new double[nPoints];
    for ( int i = 0; i < nPoints; ++i )
    {
      zArray[i] = 0;
    }
  }
  ct.transformCoords( nPoints, mX.data(), mY.data(), zArray, d );
  if ( useDummyZ )
  {
    delete[] zArray;
  }
  clearCache();
}

void QgsLineString::transform( const QTransform& t )
{
  int nPoints = numPoints();
  for ( int i = 0; i < nPoints; ++i )
  {
    qreal x, y;
    t.map( mX.at( i ), mY.at( i ), &x, &y );
    mX[i] = x;
    mY[i] = y;
  }
  clearCache();
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

bool QgsLineString::insertVertex( QgsVertexId position, const QgsPointV2& vertex )
{
  if ( position.vertex < 0 || position.vertex > mX.size() )
  {
    return false;
  }

  if ( mWkbType == QgsWkbTypes::Unknown || mX.isEmpty() )
  {
    setZMTypeFromSubGeometry( &vertex, QgsWkbTypes::LineString );
  }

  mX.insert( position.vertex, vertex.x() );
  mY.insert( position.vertex, vertex.y() );
  if ( is3D() )
  {
    mZ.insert( position.vertex, vertex.z() );
  }
  if ( isMeasure() )
  {
    mM.insert( position.vertex, vertex.m() );
  }
  clearCache(); //set bounding box invalid
  return true;
}

bool QgsLineString::moveVertex( QgsVertexId position, const QgsPointV2& newPos )
{
  if ( position.vertex < 0 || position.vertex >= mX.size() )
  {
    return false;
  }
  mX[position.vertex] = newPos.x();
  mY[position.vertex] = newPos.y();
  if ( is3D() && newPos.is3D() )
  {
    mZ[position.vertex] = newPos.z();
  }
  if ( isMeasure() && newPos.isMeasure() )
  {
    mM[position.vertex] = newPos.m();
  }
  clearCache(); //set bounding box invalid
  return true;
}

bool QgsLineString::deleteVertex( QgsVertexId position )
{
  if ( position.vertex >= mX.size() || position.vertex < 0 )
  {
    return false;
  }

  mX.remove( position.vertex );
  mY.remove( position.vertex );
  if ( is3D() )
  {
    mZ.remove( position.vertex );
  }
  if ( isMeasure() )
  {
    mM.remove( position.vertex );
  }

  if ( numPoints() == 1 )
  {
    clear();
  }

  clearCache(); //set bounding box invalid
  return true;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

void QgsLineString::addVertex( const QgsPointV2& pt )
{
  if ( mWkbType == QgsWkbTypes::Unknown || mX.isEmpty() )
  {
    setZMTypeFromSubGeometry( &pt, QgsWkbTypes::LineString );
  }

  mX.append( pt.x() );
  mY.append( pt.y() );
  if ( is3D() )
  {
    mZ.append( pt.z() );
  }
  if ( isMeasure() )
  {
    mM.append( pt.m() );
  }
  clearCache(); //set bounding box invalid
}

double QgsLineString::closestSegment( const QgsPointV2& pt, QgsPointV2& segmentPt,  QgsVertexId& vertexAfter, bool* leftOf, double epsilon ) const
{
  double sqrDist = std::numeric_limits<double>::max();
  double testDist = 0;
  double segmentPtX, segmentPtY;

  int size = mX.size();
  if ( size == 0 )
  {
    vertexAfter = QgsVertexId( 0, 0, 0 );
    return sqrDist;
  }
  else if ( size == 1 )
  {
    segmentPt = pointN( 0 );
    vertexAfter = QgsVertexId( 0, 0, 1 );
    return QgsGeometryUtils::sqrDistance2D( pt, segmentPt );
  }
  for ( int i = 1; i < size; ++i )
  {
    double prevX = mX.at( i - 1 );
    double prevY = mY.at( i - 1 );
    double currentX = mX.at( i );
    double currentY = mY.at( i );
    testDist = QgsGeometryUtils::sqrDistToLine( pt.x(), pt.y(), prevX, prevY, currentX, currentY, segmentPtX, segmentPtY, epsilon );
    if ( testDist < sqrDist )
    {
      sqrDist = testDist;
      segmentPt.setX( segmentPtX );
      segmentPt.setY( segmentPtY );
      if ( leftOf )
      {
        *leftOf = ( QgsGeometryUtils::leftOfLine( pt.x(), pt.y(), prevX, prevY, currentX, currentY ) < 0 );
      }
      vertexAfter.part = 0;
      vertexAfter.ring = 0;
      vertexAfter.vertex = i;
    }
  }
  return sqrDist;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

bool QgsLineString::pointAt( int node, QgsPointV2& point, QgsVertexId::VertexType& type ) const
{
  if ( node < 0 || node >= numPoints() )
  {
    return false;
  }
  point = pointN( node );
  type = QgsVertexId::SegmentVertex;
  return true;
}

QgsPointV2 QgsLineString::centroid() const
{
  if ( mX.isEmpty() )
    return QgsPointV2();

  int numPoints = mX.count();
  if ( numPoints == 1 )
    return QgsPointV2( mX.at( 0 ), mY.at( 0 ) );

  double totalLineLength = 0.0;
  double prevX = mX.at( 0 );
  double prevY = mY.at( 0 );
  double sumX = 0.0;
  double sumY = 0.0;

  for ( int i = 1; i < numPoints ; ++i )
  {
    double currentX = mX.at( i );
    double currentY = mY.at( i );
    double segmentLength = sqrt( qPow( currentX - prevX, 2.0 ) +
                                 qPow( currentY - prevY, 2.0 ) );
    if ( qgsDoubleNear( segmentLength, 0.0 ) )
      continue;

    totalLineLength += segmentLength;
    sumX += segmentLength * 0.5 * ( currentX + prevX );
    sumY += segmentLength * 0.5 * ( currentY + prevY );
    prevX = currentX;
    prevY = currentY;
  }

  if ( qgsDoubleNear( totalLineLength, 0.0 ) )
    return QgsPointV2( mX.at( 0 ), mY.at( 0 ) );
  else
    return QgsPointV2( sumX / totalLineLength, sumY / totalLineLength );

}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

void QgsLineString::sumUpArea( double& sum ) const
{
  int maxIndex = numPoints() - 1;
  if ( maxIndex == 1 )
    return; //no area, just a single line

  for ( int i = 0; i < maxIndex; ++i )
  {
    sum += 0.5 * ( mX.at( i ) * mY.at( i + 1 ) - mY.at( i ) * mX.at( i + 1 ) );
  }
}

void QgsLineString::importVerticesFromWkb( const QgsConstWkbPtr& wkb )
{
  bool hasZ = is3D();
  bool hasM = isMeasure();
  int nVertices = 0;
  wkb >> nVertices;
  mX.resize( nVertices );
  mY.resize( nVertices );
  hasZ ? mZ.resize( nVertices ) : mZ.clear();
  hasM ? mM.resize( nVertices ) : mM.clear();
  for ( int i = 0; i < nVertices; ++i )
  {
    wkb >> mX[i];
    wkb >> mY[i];
    if ( hasZ )
    {
      wkb >> mZ[i];
    }
    if ( hasM )
    {
      wkb >> mM[i];
    }
  }
  clearCache(); //set bounding box invalid
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

void QgsLineString::close()
{
  if ( numPoints() < 1 || isClosed() )
  {
    return;
  }
  addVertex( startPoint() );
}

double QgsLineString::vertexAngle( QgsVertexId vertex ) const
{
  if ( mX.count() < 2 )
  {
    //undefined
    return 0.0;
  }

  if ( vertex.vertex == 0 || vertex.vertex >= ( numPoints() - 1 ) )
  {
    if ( isClosed() )
    {
      double previousX = mX.at( numPoints() - 2 );
      double previousY = mY.at( numPoints() - 2 );
      double currentX = mX.at( 0 );
      double currentY = mY.at( 0 );
      double afterX = mX.at( 1 );
      double afterY = mY.at( 1 );
      return QgsGeometryUtils::averageAngle( previousX, previousY, currentX, currentY, afterX, afterY );
    }
    else if ( vertex.vertex == 0 )
    {
      return QgsGeometryUtils::lineAngle( mX.at( 0 ), mY.at( 0 ), mX.at( 1 ), mY.at( 1 ) );
    }
    else
    {
      int a = numPoints() - 2;
      int b = numPoints() - 1;
      return QgsGeometryUtils::lineAngle( mX.at( a ), mY.at( a ), mX.at( b ), mY.at( b ) );
    }
  }
  else
  {
    double previousX = mX.at( vertex.vertex - 1 );
    double previousY = mY.at( vertex.vertex - 1 );
    double currentX = mX.at( vertex.vertex );
    double currentY = mY.at( vertex.vertex );
    double afterX = mX.at( vertex.vertex + 1 );
    double afterY = mY.at( vertex.vertex + 1 );
    return QgsGeometryUtils::averageAngle( previousX, previousY, currentX, currentY, afterX, afterY );
  }
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

bool QgsLineString::addZValue( double zValue )
{
  if ( QgsWkbTypes::hasZ( mWkbType ) )
    return false;

  clearCache();
  if ( mWkbType == QgsWkbTypes::Unknown )
  {
    mWkbType = QgsWkbTypes::LineStringZ;
    return true;
  }

  mWkbType = QgsWkbTypes::addZ( mWkbType );

  mZ.clear();
  int nPoints = numPoints();
  mZ.reserve( nPoints );
  for ( int i = 0; i < nPoints; ++i )
  {
    mZ << zValue;
  }
  return true;
}

bool QgsLineString::addMValue( double mValue )
{
  if ( QgsWkbTypes::hasM( mWkbType ) )
    return false;

  clearCache();
  if ( mWkbType == QgsWkbTypes::Unknown )
  {
    mWkbType = QgsWkbTypes::LineStringM;
    return true;
  }

  if ( mWkbType == QgsWkbTypes::LineString25D )
  {
    mWkbType = QgsWkbTypes::LineStringZM;
  }
  else
  {
    mWkbType = QgsWkbTypes::addM( mWkbType );
  }

  mM.clear();
  int nPoints = numPoints();
  mM.reserve( nPoints );
  for ( int i = 0; i < nPoints; ++i )
  {
    mM << mValue;
  }
  return true;
}

bool QgsLineString::dropZValue()
{
  if ( !is3D() )
    return false;

  clearCache();
  mWkbType = QgsWkbTypes::dropZ( mWkbType );
  mZ.clear();
  return true;
}

bool QgsLineString::dropMValue()
{
  if ( !isMeasure() )
    return false;

  clearCache();
  mWkbType = QgsWkbTypes::dropM( mWkbType );
  mM.clear();
  return true;
}

bool QgsLineString::convertTo( QgsWkbTypes::Type type )
{
  if ( type == mWkbType )
    return true;

  clearCache();
  if ( type == QgsWkbTypes::LineString25D )
  {
    //special handling required for conversion to LineString25D
    dropMValue();
    addZValue();
    mWkbType = QgsWkbTypes::LineString25D;
    return true;
  }
  else
  {
    return QgsCurve::convertTo( type );
  }
}
