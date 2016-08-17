/***************************************************************************
                         qgspointv2.cpp
                         --------------
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


#include "qgspointv2.h"
#include "qgsapplication.h"
#include "qgscoordinatetransform.h"
#include "qgsgeometryutils.h"
#include "qgsmaptopixel.h"
#include "qgswkbptr.h"
#include <QPainter>

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

QgsPointV2::QgsPointV2( double x, double y )
    : QgsAbstractGeometry()
    , mX( x )
    , mY( y )
    , mZ( 0.0 )
    , mM( 0.0 )
{
  mWkbType = QgsWkbTypes::Point;
}

QgsPointV2::QgsPointV2( const QgsPoint& p )
    : QgsAbstractGeometry()
    , mX( p.x() )
    , mY( p.y() )
    , mZ( 0.0 )
    , mM( 0.0 )
{
  mWkbType = QgsWkbTypes::Point;
}

QgsPointV2::QgsPointV2( QPointF p )
    : QgsAbstractGeometry()
    , mX( p.x() )
    , mY( p.y() )
    , mZ( 0.0 )
    , mM( 0.0 )
{
  mWkbType = QgsWkbTypes::Point;
}

QgsPointV2::QgsPointV2( QgsWkbTypes::Type type, double x, double y, double z, double m )
    : mX( x )
    , mY( y )
    , mZ( z )
    , mM( m )
{
  //protect against non-point WKB types
  Q_ASSERT( QgsWkbTypes::flatType( type ) == QgsWkbTypes::Point );
  mWkbType = type;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

bool QgsPointV2::operator==( const QgsPointV2& pt ) const
{
  return ( pt.wkbType() == wkbType() &&
           qgsDoubleNear( pt.x(), mX, 1E-8 ) &&
           qgsDoubleNear( pt.y(), mY, 1E-8 ) &&
           qgsDoubleNear( pt.z(), mZ, 1E-8 ) &&
           qgsDoubleNear( pt.m(), mM, 1E-8 ) );
}

bool QgsPointV2::operator!=( const QgsPointV2& pt ) const
{
  return !operator==( pt );
}

QgsPointV2 *QgsPointV2::clone() const
{
  return new QgsPointV2( *this );
}

bool QgsPointV2::fromWkb( QgsConstWkbPtr wkbPtr )
{
  QgsWkbTypes::Type type = wkbPtr.readHeader();
  if ( QgsWkbTypes::flatType( type ) != QgsWkbTypes::Point )
  {
    clear();
    return false;
  }
  mWkbType = type;

  wkbPtr >> mX;
  wkbPtr >> mY;
  if ( is3D() )
    wkbPtr >> mZ;
  if ( isMeasure() )
    wkbPtr >> mM;

  clearCache();

  return true;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

bool QgsPointV2::fromWkt( const QString& wkt )
{
  clear();

  QPair<QgsWkbTypes::Type, QString> parts = QgsGeometryUtils::wktReadBlock( wkt );

  if ( QgsWkbTypes::flatType( parts.first ) != QgsWkbTypes::Point )
    return false;
  mWkbType = parts.first;

  QStringList coordinates = parts.second.split( ' ', QString::SkipEmptyParts );
  if ( coordinates.size() < 2 )
  {
    clear();
    return false;
  }
  else if ( coordinates.size() == 3 && !is3D() && !isMeasure() )
  {
    // 3 dimensional coordinates, but not specifically marked as such. We allow this
    // anyway and upgrade geometry to have Z dimension
    mWkbType = QgsWkbTypes::addZ( mWkbType );
  }
  else if ( coordinates.size() >= 4 && ( !is3D() || !isMeasure() ) )
  {
    // 4 (or more) dimensional coordinates, but not specifically marked as such. We allow this
    // anyway and upgrade geometry to have Z&M dimensions
    mWkbType = QgsWkbTypes::addZ( mWkbType );
    mWkbType = QgsWkbTypes::addM( mWkbType );
  }

  int idx = 0;
  mX = coordinates[idx++].toDouble();
  mY = coordinates[idx++].toDouble();
  if ( is3D() && coordinates.length() > 2 )
    mZ = coordinates[idx++].toDouble();
  if ( isMeasure() && coordinates.length() > 2 + is3D() )
    mM = coordinates[idx++].toDouble();

  return true;
}

int QgsPointV2::wkbSize() const
{
  int size = sizeof( char ) + sizeof( quint32 );
  size += ( 2 + is3D() + isMeasure() ) * sizeof( double );
  return size;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

unsigned char* QgsPointV2::asWkb( int& binarySize ) const
{
  binarySize = wkbSize();
  unsigned char* geomPtr = new unsigned char[binarySize];
  QgsWkbPtr wkb( geomPtr, binarySize );
  wkb << static_cast<char>( QgsApplication::endian() );
  wkb << static_cast<quint32>( wkbType() );
  wkb << mX << mY;
  if ( is3D() )
  {
    wkb << mZ;
  }
  if ( isMeasure() )
  {
    wkb << mM;
  }
  return geomPtr;
}

QString QgsPointV2::asWkt( int precision ) const
{
  QString wkt = wktTypeStr() + " (";
  wkt += qgsDoubleToString( mX, precision ) + ' ' + qgsDoubleToString( mY, precision );
  if ( is3D() )
    wkt += ' ' + qgsDoubleToString( mZ, precision );
  if ( isMeasure() )
    wkt += ' ' + qgsDoubleToString( mM, precision );
  wkt += ')';
  return wkt;
}

QDomElement QgsPointV2::asGML2( QDomDocument& doc, int precision, const QString& ns ) const
{
  QDomElement elemPoint = doc.createElementNS( ns, "Point" );
  QDomElement elemCoordinates = doc.createElementNS( ns, "coordinates" );
  QString strCoordinates = qgsDoubleToString( mX, precision ) + ',' + qgsDoubleToString( mY, precision );
  elemCoordinates.appendChild( doc.createTextNode( strCoordinates ) );
  elemPoint.appendChild( elemCoordinates );
  return elemPoint;
}

QDomElement QgsPointV2::asGML3( QDomDocument& doc, int precision, const QString& ns ) const
{
  QDomElement elemPoint = doc.createElementNS( ns, "Point" );
  QDomElement elemPosList = doc.createElementNS( ns, "pos" );
  elemPosList.setAttribute( "srsDimension", is3D() ? 3 : 2 );
  QString strCoordinates = qgsDoubleToString( mX, precision ) + ' ' + qgsDoubleToString( mY, precision );
  if ( is3D() )
    strCoordinates += ' ' + qgsDoubleToString( mZ, precision );

  elemPosList.appendChild( doc.createTextNode( strCoordinates ) );
  elemPoint.appendChild( elemPosList );
  return elemPoint;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

QString QgsPointV2::asJSON( int precision ) const
{
  return "{\"type\": \"Point\", \"coordinates\": ["
         + qgsDoubleToString( mX, precision ) + ", " + qgsDoubleToString( mY, precision )
         + "]}";
}

void QgsPointV2::draw( QPainter& p ) const
{
  p.drawRect( mX - 2, mY - 2, 4, 4 );
}

void QgsPointV2::clear()
{
  mX = mY = mZ = mM = 0.;
  clearCache();
}

void QgsPointV2::transform( const QgsCoordinateTransform& ct, QgsCoordinateTransform::TransformDirection d, bool transformZ )
{
  clearCache();
  if ( transformZ )
  {
    ct.transformInPlace( mX, mY, mZ, d );
  }
  else
  {
    double z = 0.0;
    ct.transformInPlace( mX, mY, z, d );
  }
}

QgsCoordinateSequence QgsPointV2::coordinateSequence() const
{
  QgsCoordinateSequence cs;

  cs.append( QgsRingSequence() );
  cs.back().append( QgsPointSequence() << QgsPointV2( *this ) );

  return cs;
}

QgsAbstractGeometry* QgsPointV2::boundary() const
{
  return nullptr;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

bool QgsPointV2::moveVertex( QgsVertexId position, const QgsPointV2& newPos )
{
  Q_UNUSED( position );
  clearCache();
  mX = newPos.mX;
  mY = newPos.mY;
  if ( is3D() && newPos.is3D() )
  {
    mZ = newPos.mZ;
  }
  if ( isMeasure() && newPos.isMeasure() )
  {
    mM = newPos.mM;
  }
  return true;
}

double QgsPointV2::closestSegment( const QgsPointV2& pt, QgsPointV2& segmentPt,  QgsVertexId& vertexAfter, bool* leftOf, double epsilon ) const
{
  Q_UNUSED( pt );
  Q_UNUSED( segmentPt );
  Q_UNUSED( vertexAfter );
  Q_UNUSED( leftOf );
  Q_UNUSED( epsilon );
  return -1;  // no segments - return error
}

bool QgsPointV2::nextVertex( QgsVertexId& id, QgsPointV2& vertex ) const
{
  if ( id.vertex < 0 )
  {
    id.vertex = 0;
    if ( id.part < 0 )
    {
      id.part = 0;
    }
    if ( id.ring < 0 )
    {
      id.ring = 0;
    }
    vertex = *this;
    return true;
  }
  else
  {
    return false;
  }
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests.
 * See details in QEP #17
 ****************************************************************************/

bool QgsPointV2::addZValue( double zValue )
{
  if ( QgsWkbTypes::hasZ( mWkbType ) )
    return false;

  mWkbType = QgsWkbTypes::addZ( mWkbType );
  mZ = zValue;
  clearCache();
  return true;
}

bool QgsPointV2::addMValue( double mValue )
{
  if ( QgsWkbTypes::hasM( mWkbType ) )
    return false;

  mWkbType = QgsWkbTypes::addM( mWkbType );
  mM = mValue;
  clearCache();
  return true;
}

void QgsPointV2::transform( const QTransform& t )
{
  clearCache();
  qreal x, y;
  t.map( mX, mY, &x, &y );
  mX = x;
  mY = y;
}


bool QgsPointV2::dropZValue()
{
  if ( !is3D() )
    return false;

  mWkbType = QgsWkbTypes::dropZ( mWkbType );
  mZ = 0.0;
  clearCache();
  return true;
}

bool QgsPointV2::dropMValue()
{
  if ( !isMeasure() )
    return false;

  mWkbType = QgsWkbTypes::dropM( mWkbType );
  mM = 0.0;
  clearCache();
  return true;
}

bool QgsPointV2::convertTo( QgsWkbTypes::Type type )
{
  if ( type == mWkbType )
    return true;

  clearCache();

  switch ( type )
  {
    case QgsWkbTypes::Point:
      mZ = 0.0;
      mM = 0.0;
      mWkbType = type;
      return true;
    case QgsWkbTypes::PointZ:
    case QgsWkbTypes::Point25D:
      mM = 0.0;
      mWkbType = type;
      return true;
    case QgsWkbTypes::PointM:
      mZ = 0.0;
      mWkbType = type;
      return true;
    case QgsWkbTypes::PointZM:
      mWkbType = type;
      return true;
    default:
      break;
  }

  return false;
}


QPointF QgsPointV2::toQPointF() const
{
  return QPointF( mX, mY );
}
