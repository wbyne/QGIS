/***************************************************************************
               QgsCoordinateTransform.cpp  - Coordinate Transforms
                             -------------------
    begin                : Dec 2004
    copyright            : (C) 2004 Tim Sutton
    email                : tim at linfiniti.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgscoordinatetransform.h"
#include "qgscoordinatetransform_p.h"
#include "qgsapplication.h"
#include "qgsmessagelog.h"
#include "qgslogger.h"
#include "qgspoint.h"
#include "qgsrectangle.h"
#include "qgscsexception.h"

//qt includes
#include <QDomNode>
#include <QDomElement>
#include <QApplication>
#include <QPolygonF>
#include <QStringList>
#include <QVector>

extern "C"
{
#include <proj_api.h>
}
#include <sqlite3.h>

// if defined shows all information about transform to stdout
// #define COORDINATE_TRANSFORM_VERBOSE

QgsCoordinateTransform::QgsCoordinateTransform()
{
  d = new QgsCoordinateTransformPrivate();
}

QgsCoordinateTransform::QgsCoordinateTransform( const QgsCoordinateReferenceSystem& source, const QgsCoordinateReferenceSystem& destination )
{
  d = new QgsCoordinateTransformPrivate( source, destination );
}

QgsCoordinateTransform::QgsCoordinateTransform( const QgsCoordinateTransform& o )
{
  d = o.d;
}

QgsCoordinateTransform& QgsCoordinateTransform::operator=( const QgsCoordinateTransform & o )
{
  d = o.d;
  return *this;
}

QgsCoordinateTransform::~QgsCoordinateTransform()
{

}

void QgsCoordinateTransform::setSourceCrs( const QgsCoordinateReferenceSystem& crs )
{
  d.detach();
  d->mSourceCRS = crs;
  d->initialise();
}
void QgsCoordinateTransform::setDestinationCrs( const QgsCoordinateReferenceSystem& crs )
{
  d.detach();
  d->mDestCRS = crs;
  d->initialise();
}

QgsCoordinateReferenceSystem QgsCoordinateTransform::sourceCrs() const
{
  return d->mSourceCRS;
}

QgsCoordinateReferenceSystem QgsCoordinateTransform::destinationCrs() const
{
  return d->mDestCRS;
}

QgsPoint QgsCoordinateTransform::transform( const QgsPoint &thePoint, TransformDirection direction ) const
{
  if ( !d->mIsValid || d->mShortCircuit )
    return thePoint;

  // transform x
  double x = thePoint.x();
  double y = thePoint.y();
  double z = 0.0;
  try
  {
    transformCoords( 1, &x, &y, &z, direction );
  }
  catch ( const QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }

  return QgsPoint( x, y );
}


QgsPoint QgsCoordinateTransform::transform( const double theX, const double theY = 0.0, TransformDirection direction ) const
{
  try
  {
    return transform( QgsPoint( theX, theY ), direction );
  }
  catch ( const QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }
}

QgsRectangle QgsCoordinateTransform::transform( const QgsRectangle &theRect, TransformDirection direction ) const
{
  if ( !d->mIsValid || d->mShortCircuit )
    return theRect;
  // transform x
  double x1 = theRect.xMinimum();
  double y1 = theRect.yMinimum();
  double x2 = theRect.xMaximum();
  double y2 = theRect.yMaximum();

  // Number of points to reproject------+
  //                                    |
  //                                    V
  try
  {
    double z = 0.0;
    transformCoords( 1, &x1, &y1, &z, direction );
    transformCoords( 1, &x2, &y2, &z, direction );
  }
  catch ( const QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }

#ifdef COORDINATE_TRANSFORM_VERBOSE
  QgsDebugMsg( "Rect projection..." );
  QgsDebugMsg( QString( "Xmin : %1 --> %2" ).arg( theRect.xMinimum() ).arg( x1 ) );
  QgsDebugMsg( QString( "Ymin : %1 --> %2" ).arg( theRect.yMinimum() ).arg( y1 ) );
  QgsDebugMsg( QString( "Xmax : %1 --> %2" ).arg( theRect.xMaximum() ).arg( x2 ) );
  QgsDebugMsg( QString( "Ymax : %1 --> %2" ).arg( theRect.yMaximum() ).arg( y2 ) );
#endif
  return QgsRectangle( x1, y1, x2, y2 );
}

void QgsCoordinateTransform::transformInPlace( double& x, double& y, double& z,
    TransformDirection direction ) const
{
  if ( !d->mIsValid || d->mShortCircuit )
    return;
#ifdef QGISDEBUG
// QgsDebugMsg(QString("Using transform in place %1 %2").arg(__FILE__).arg(__LINE__));
#endif
  // transform x
  try
  {
    transformCoords( 1, &x, &y, &z, direction );
  }
  catch ( const QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }
}

void QgsCoordinateTransform::transformInPlace( float& x, float& y, double& z,
    TransformDirection direction ) const
{
  double xd = static_cast< double >( x ), yd = static_cast< double >( y );
  transformInPlace( xd, yd, z, direction );
  x = xd;
  y = yd;
}

void QgsCoordinateTransform::transformInPlace( float& x, float& y, float& z,
    TransformDirection direction ) const
{
  if ( !d->mIsValid || d->mShortCircuit )
    return;
#ifdef QGISDEBUG
  // QgsDebugMsg(QString("Using transform in place %1 %2").arg(__FILE__).arg(__LINE__));
#endif
  // transform x
  try
  {
    double xd = x;
    double yd = y;
    double zd = z;
    transformCoords( 1, &xd, &yd, &zd, direction );
    x = xd;
    y = yd;
    z = zd;
  }
  catch ( QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }
}

void QgsCoordinateTransform::transformPolygon( QPolygonF& poly, TransformDirection direction ) const
{
  if ( !d->mIsValid || d->mShortCircuit )
  {
    return;
  }

  //create x, y arrays
  int nVertices = poly.size();

  QVector<double> x( nVertices );
  QVector<double> y( nVertices );
  QVector<double> z( nVertices );

  for ( int i = 0; i < nVertices; ++i )
  {
    const QPointF& pt = poly.at( i );
    x[i] = pt.x();
    y[i] = pt.y();
    z[i] = 0;
  }

  try
  {
    transformCoords( nVertices, x.data(), y.data(), z.data(), direction );
  }
  catch ( const QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }

  for ( int i = 0; i < nVertices; ++i )
  {
    QPointF& pt = poly[i];
    pt.rx() = x[i];
    pt.ry() = y[i];
  }
}

void QgsCoordinateTransform::transformInPlace(
  QVector<double>& x, QVector<double>& y, QVector<double>& z,
  TransformDirection direction ) const
{

  if ( !d->mIsValid || d->mShortCircuit )
    return;

  Q_ASSERT( x.size() == y.size() );

  // Apparently, if one has a std::vector, it is valid to use the
  // address of the first element in the vector as a pointer to an
  // array of the vectors data, and hence easily interface with code
  // that wants C-style arrays.

  try
  {
    transformCoords( x.size(), &x[0], &y[0], &z[0], direction );
  }
  catch ( const QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }
}


void QgsCoordinateTransform::transformInPlace(
  QVector<float>& x, QVector<float>& y, QVector<float>& z,
  TransformDirection direction ) const
{
  if ( !d->mIsValid || d->mShortCircuit )
    return;

  Q_ASSERT( x.size() == y.size() );

  // Apparently, if one has a std::vector, it is valid to use the
  // address of the first element in the vector as a pointer to an
  // array of the vectors data, and hence easily interface with code
  // that wants C-style arrays.

  try
  {
    //copy everything to double vectors since proj needs double
    int vectorSize = x.size();
    QVector<double> xd( x.size() );
    QVector<double> yd( y.size() );
    QVector<double> zd( z.size() );
    for ( int i = 0; i < vectorSize; ++i )
    {
      xd[i] = x[i];
      yd[i] = y[i];
      zd[i] = z[i];
    }
    transformCoords( x.size(), &xd[0], &yd[0], &zd[0], direction );

    //copy back
    for ( int i = 0; i < vectorSize; ++i )
    {
      x[i] = xd[i];
      y[i] = yd[i];
      z[i] = zd[i];
    }
  }
  catch ( QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }
}

QgsRectangle QgsCoordinateTransform::transformBoundingBox( const QgsRectangle &rect, TransformDirection direction, const bool handle180Crossover ) const
{
  // Calculate the bounding box of a QgsRectangle in the source CRS
  // when projected to the destination CRS (or the inverse).
  // This is done by looking at a number of points spread evenly
  // across the rectangle

  if ( !d->mIsValid || d->mShortCircuit )
    return rect;

  if ( rect.isEmpty() )
  {
    QgsPoint p = transform( rect.xMinimum(), rect.yMinimum(), direction );
    return QgsRectangle( p, p );
  }

  // 64 points (<=2.12) is not enough, see #13665, for EPSG:4326 -> EPSG:3574 (say that it is a hard one),
  // are decent result from about 500 points and more. This method is called quite often, but
  // even with 1000 points it takes < 1ms
  // TODO: how to effectively and precisely reproject bounding box?
  const int nPoints = 1000;
  double d = sqrt(( rect.width() * rect.height() ) / pow( sqrt( static_cast< double >( nPoints ) ) - 1, 2.0 ) );
  int nXPoints = static_cast< int >( ceil( rect.width() / d ) ) + 1;
  int nYPoints = static_cast< int >( ceil( rect.height() / d ) ) + 1;

  QgsRectangle bb_rect;
  bb_rect.setMinimal();

  // We're interfacing with C-style vectors in the
  // end, so let's do C-style vectors here too.

  QVector<double> x( nXPoints * nYPoints );
  QVector<double> y( nXPoints * nYPoints );
  QVector<double> z( nXPoints * nYPoints );

  QgsDebugMsgLevel( "Entering transformBoundingBox...", 4 );

  // Populate the vectors

  double dx = rect.width()  / static_cast< double >( nXPoints - 1 );
  double dy = rect.height() / static_cast< double >( nYPoints - 1 );

  double pointY = rect.yMinimum();

  for ( int i = 0; i < nYPoints ; i++ )
  {

    // Start at right edge
    double pointX = rect.xMinimum();

    for ( int j = 0; j < nXPoints; j++ )
    {
      x[( i*nXPoints ) + j] = pointX;
      y[( i*nXPoints ) + j] = pointY;
      // and the height...
      z[( i*nXPoints ) + j] = 0.0;
      // QgsDebugMsg(QString("BBox coord: (%1, %2)").arg(x[(i*numP) + j]).arg(y[(i*numP) + j]));
      pointX += dx;
    }
    pointY += dy;
  }

  // Do transformation. Any exception generated must
  // be handled in above layers.
  try
  {
    transformCoords( nXPoints * nYPoints, x.data(), y.data(), z.data(), direction );
  }
  catch ( const QgsCsException & )
  {
    // rethrow the exception
    QgsDebugMsg( "rethrowing exception" );
    throw;
  }

  // Calculate the bounding box and use that for the extent

  for ( int i = 0; i < nXPoints * nYPoints; i++ )
  {
    if ( !qIsFinite( x[i] ) || !qIsFinite( y[i] ) )
    {
      continue;
    }

    if ( handle180Crossover )
    {
      //if crossing the date line, temporarily add 360 degrees to -ve longitudes
      bb_rect.combineExtentWith( x[i] >= 0.0 ? x[i] : x[i] + 360.0, y[i] );
    }
    else
    {
      bb_rect.combineExtentWith( x[i], y[i] );
    }
  }

  if ( handle180Crossover )
  {
    //subtract temporary addition of 360 degrees from longitudes
    if ( bb_rect.xMinimum() > 180.0 )
      bb_rect.setXMinimum( bb_rect.xMinimum() - 360.0 );
    if ( bb_rect.xMaximum() > 180.0 )
      bb_rect.setXMaximum( bb_rect.xMaximum() - 360.0 );
  }

  QgsDebugMsgLevel( "Projected extent: " + bb_rect.toString(), 4 );

  if ( bb_rect.isEmpty() )
  {
    QgsDebugMsgLevel( "Original extent: " + rect.toString(), 4 );
  }

  return bb_rect;
}

void QgsCoordinateTransform::transformCoords( int numPoints, double *x, double *y, double *z, TransformDirection direction ) const
{
  if ( !d->mIsValid || d->mShortCircuit )
    return;
  // Refuse to transform the points if the srs's are invalid
  if ( !d->mSourceCRS.isValid() )
  {
    QgsMessageLog::logMessage( QObject::tr( "The source spatial reference system (CRS) is not valid. "
                                            "The coordinates can not be reprojected. The CRS is: %1" )
                               .arg( d->mSourceCRS.toProj4() ), QObject::tr( "CRS" ) );
    return;
  }
  if ( !d->mDestCRS.isValid() )
  {
    QgsMessageLog::logMessage( QObject::tr( "The destination spatial reference system (CRS) is not valid. "
                                            "The coordinates can not be reprojected. The CRS is: %1" ).arg( d->mDestCRS.toProj4() ), QObject::tr( "CRS" ) );
    return;
  }

#ifdef COORDINATE_TRANSFORM_VERBOSE
  double xorg = *x;
  double yorg = *y;
  QgsDebugMsg( QString( "[[[[[[ Number of points to transform: %1 ]]]]]]" ).arg( numPoints ) );
#endif

  // use proj4 to do the transform

  // if the source/destination projection is lat/long, convert the points to radians
  // prior to transforming
  if (( pj_is_latlong( d->mDestinationProjection ) && ( direction == ReverseTransform ) )
      || ( pj_is_latlong( d->mSourceProjection ) && ( direction == ForwardTransform ) ) )
  {
    for ( int i = 0; i < numPoints; ++i )
    {
      x[i] *= DEG_TO_RAD;
      y[i] *= DEG_TO_RAD;
    }

  }
  int projResult;
  if ( direction == ReverseTransform )
  {
    projResult = pj_transform( d->mDestinationProjection, d->mSourceProjection, numPoints, 0, x, y, z );
  }
  else
  {
    Q_ASSERT( d->mSourceProjection );
    Q_ASSERT( d->mDestinationProjection );
    projResult = pj_transform( d->mSourceProjection, d->mDestinationProjection, numPoints, 0, x, y, z );
  }

  if ( projResult != 0 )
  {
    //something bad happened....
    QString points;

    for ( int i = 0; i < numPoints; ++i )
    {
      if ( direction == ForwardTransform )
      {
        points += QString( "(%1, %2)\n" ).arg( x[i], 0, 'f' ).arg( y[i], 0, 'f' );
      }
      else
      {
        points += QString( "(%1, %2)\n" ).arg( x[i] * RAD_TO_DEG, 0, 'f' ).arg( y[i] * RAD_TO_DEG, 0, 'f' );
      }
    }

    QString dir = ( direction == ForwardTransform ) ? QObject::tr( "forward transform" ) : QObject::tr( "inverse transform" );

    char *srcdef = pj_get_def( d->mSourceProjection, 0 );
    char *dstdef = pj_get_def( d->mDestinationProjection, 0 );

    QString msg = QObject::tr( "%1 of\n"
                               "%2"
                               "PROJ.4: %3 +to %4\n"
                               "Error: %5" )
                  .arg( dir,
                        points,
                        srcdef, dstdef,
                        QString::fromUtf8( pj_strerrno( projResult ) ) );

    pj_dalloc( srcdef );
    pj_dalloc( dstdef );

    QgsDebugMsg( "Projection failed emitting invalid transform signal: " + msg );
    QgsDebugMsg( "throwing exception" );

    throw QgsCsException( msg );
  }

  // if the result is lat/long, convert the results from radians back
  // to degrees
  if (( pj_is_latlong( d->mDestinationProjection ) && ( direction == ForwardTransform ) )
      || ( pj_is_latlong( d->mSourceProjection ) && ( direction == ReverseTransform ) ) )
  {
    for ( int i = 0; i < numPoints; ++i )
    {
      x[i] *= RAD_TO_DEG;
      y[i] *= RAD_TO_DEG;
    }
  }
#ifdef COORDINATE_TRANSFORM_VERBOSE
  QgsDebugMsg( QString( "[[[[[[ Projected %1, %2 to %3, %4 ]]]]]]" )
               .arg( xorg, 0, 'g', 15 ).arg( yorg, 0, 'g', 15 )
               .arg( *x, 0, 'g', 15 ).arg( *y, 0, 'g', 15 ) );
#endif
}

bool QgsCoordinateTransform::isValid() const
{
  return d->mIsValid;
}

bool QgsCoordinateTransform::isShortCircuited() const
{
  return !d->mIsValid || d->mShortCircuit;
}

bool QgsCoordinateTransform::readXml( const QDomNode & theNode )
{
  d.detach();

  QgsDebugMsg( "Reading Coordinate Transform from xml ------------------------!" );

  QDomNode mySrcNode = theNode.namedItem( "sourcesrs" );
  d->mSourceCRS.readXml( mySrcNode );

  QDomNode myDestNode = theNode.namedItem( "destinationsrs" );
  d->mDestCRS.readXml( myDestNode );

  d->mSourceDatumTransform = theNode.toElement().attribute( "sourceDatumTransform", "-1" ).toInt();
  d->mDestinationDatumTransform = theNode.toElement().attribute( "destinationDatumTransform", "-1" ).toInt();

  return d->initialise();
}

bool QgsCoordinateTransform::writeXml( QDomNode & theNode, QDomDocument & theDoc ) const
{
  QDomElement myNodeElement = theNode.toElement();
  QDomElement myTransformElement = theDoc.createElement( "coordinatetransform" );
  myTransformElement.setAttribute( "sourceDatumTransform", QString::number( d->mSourceDatumTransform ) );
  myTransformElement.setAttribute( "destinationDatumTransform", QString::number( d->mDestinationDatumTransform ) );

  QDomElement mySourceElement = theDoc.createElement( "sourcesrs" );
  d->mSourceCRS.writeXml( mySourceElement, theDoc );
  myTransformElement.appendChild( mySourceElement );

  QDomElement myDestElement = theDoc.createElement( "destinationsrs" );
  d->mDestCRS.writeXml( myDestElement, theDoc );
  myTransformElement.appendChild( myDestElement );

  myNodeElement.appendChild( myTransformElement );

  return true;
}

const char *finder( const char *name )
{
  QString proj;
#ifdef Q_OS_WIN
  proj = QApplication::applicationDirPath()
         + "/share/proj/" + QString( name );
#else
  Q_UNUSED( name );
#endif
  return proj.toUtf8();
}



QList< QList< int > > QgsCoordinateTransform::datumTransformations( const QgsCoordinateReferenceSystem& srcCRS, const QgsCoordinateReferenceSystem& destCRS )
{
  QList< QList< int > > transformations;

  QString srcGeoId = srcCRS.geographicCrsAuthId();
  QString destGeoId = destCRS.geographicCrsAuthId();

  if ( srcGeoId.isEmpty() || destGeoId.isEmpty() )
  {
    return transformations;
  }

  QStringList srcSplit = srcGeoId.split( ':' );
  QStringList destSplit = destGeoId.split( ':' );

  if ( srcSplit.size() < 2 || destSplit.size() < 2 )
  {
    return transformations;
  }

  int srcAuthCode = srcSplit.at( 1 ).toInt();
  int destAuthCode = destSplit.at( 1 ).toInt();

  if ( srcAuthCode == destAuthCode )
  {
    return transformations; //crs have the same datum
  }

  QList<int> directTransforms;
  searchDatumTransform( QString( "SELECT coord_op_code FROM tbl_datum_transform WHERE source_crs_code=%1 AND target_crs_code=%2 ORDER BY deprecated ASC,preferred DESC" ).arg( srcAuthCode ).arg( destAuthCode ),
                        directTransforms );
  QList<int> reverseDirectTransforms;
  searchDatumTransform( QString( "SELECT coord_op_code FROM tbl_datum_transform WHERE source_crs_code = %1 AND target_crs_code=%2 ORDER BY deprecated ASC,preferred DESC" ).arg( destAuthCode ).arg( srcAuthCode ),
                        reverseDirectTransforms );
  QList<int> srcToWgs84;
  searchDatumTransform( QString( "SELECT coord_op_code FROM tbl_datum_transform WHERE (source_crs_code=%1 AND target_crs_code=%2) OR (source_crs_code=%2 AND target_crs_code=%1) ORDER BY deprecated ASC,preferred DESC" ).arg( srcAuthCode ).arg( 4326 ),
                        srcToWgs84 );
  QList<int> destToWgs84;
  searchDatumTransform( QString( "SELECT coord_op_code FROM tbl_datum_transform WHERE (source_crs_code=%1 AND target_crs_code=%2) OR (source_crs_code=%2 AND target_crs_code=%1) ORDER BY deprecated ASC,preferred DESC" ).arg( destAuthCode ).arg( 4326 ),
                        destToWgs84 );

  //add direct datum transformations
  QList<int>::const_iterator directIt = directTransforms.constBegin();
  for ( ; directIt != directTransforms.constEnd(); ++directIt )
  {
    transformations.push_back( QList<int>() << *directIt << -1 );
  }

  //add direct datum transformations
  directIt = reverseDirectTransforms.constBegin();
  for ( ; directIt != reverseDirectTransforms.constEnd(); ++directIt )
  {
    transformations.push_back( QList<int>() << -1 << *directIt );
  }

  QList<int>::const_iterator srcWgsIt = srcToWgs84.constBegin();
  for ( ; srcWgsIt != srcToWgs84.constEnd(); ++srcWgsIt )
  {
    QList<int>::const_iterator dstWgsIt = destToWgs84.constBegin();
    for ( ; dstWgsIt != destToWgs84.constEnd(); ++dstWgsIt )
    {
      transformations.push_back( QList<int>() << *srcWgsIt << *dstWgsIt );
    }
  }

  return transformations;
}

void QgsCoordinateTransform::searchDatumTransform( const QString& sql, QList< int >& transforms )
{
  sqlite3* db;
  int openResult = sqlite3_open_v2( QgsApplication::srsDbFilePath().toUtf8().constData(), &db, SQLITE_OPEN_READONLY, 0 );
  if ( openResult != SQLITE_OK )
  {
    sqlite3_close( db );
    return;
  }

  sqlite3_stmt* stmt;
  int prepareRes = sqlite3_prepare( db, sql.toLatin1(), sql.size(), &stmt, nullptr );
  if ( prepareRes != SQLITE_OK )
  {
    sqlite3_finalize( stmt );
    sqlite3_close( db );
    return;
  }

  QString cOpCode;
  while ( sqlite3_step( stmt ) == SQLITE_ROW )
  {
    cOpCode = reinterpret_cast< const char * >( sqlite3_column_text( stmt, 0 ) );
    transforms.push_back( cOpCode.toInt() );
  }
  sqlite3_finalize( stmt );
  sqlite3_close( db );
}

QString QgsCoordinateTransform::datumTransformString( int datumTransform )
{
  return QgsCoordinateTransformPrivate::datumTransformString( datumTransform );
}

bool QgsCoordinateTransform::datumTransformCrsInfo( int datumTransform, int& epsgNr, QString& srcProjection, QString& dstProjection, QString &remarks, QString &scope, bool &preferred, bool &deprecated )
{
  sqlite3* db;
  int openResult = sqlite3_open_v2( QgsApplication::srsDbFilePath().toUtf8().constData(), &db, SQLITE_OPEN_READONLY, 0 );
  if ( openResult != SQLITE_OK )
  {
    sqlite3_close( db );
    return false;
  }

  sqlite3_stmt* stmt;
  QString sql = QString( "SELECT epsg_nr,source_crs_code,target_crs_code,remarks,scope,preferred,deprecated FROM tbl_datum_transform WHERE coord_op_code=%1" ).arg( datumTransform );
  int prepareRes = sqlite3_prepare( db, sql.toLatin1(), sql.size(), &stmt, nullptr );
  if ( prepareRes != SQLITE_OK )
  {
    sqlite3_finalize( stmt );
    sqlite3_close( db );
    return false;
  }

  int srcCrsId, destCrsId;
  if ( sqlite3_step( stmt ) != SQLITE_ROW )
  {
    sqlite3_finalize( stmt );
    sqlite3_close( db );
    return false;
  }

  epsgNr = sqlite3_column_int( stmt, 0 );
  srcCrsId = sqlite3_column_int( stmt, 1 );
  destCrsId = sqlite3_column_int( stmt, 2 );
  remarks = QString::fromUtf8( reinterpret_cast< const char * >( sqlite3_column_text( stmt, 3 ) ) );
  scope = QString::fromUtf8( reinterpret_cast< const char * >( sqlite3_column_text( stmt, 4 ) ) );
  preferred = sqlite3_column_int( stmt, 5 ) != 0;
  deprecated = sqlite3_column_int( stmt, 6 ) != 0;

  QgsCoordinateReferenceSystem srcCrs = QgsCoordinateReferenceSystem::fromOgcWmsCrs( QString( "EPSG:%1" ).arg( srcCrsId ) );
  srcProjection = srcCrs.description();
  QgsCoordinateReferenceSystem destCrs = QgsCoordinateReferenceSystem::fromOgcWmsCrs( QString( "EPSG:%1" ).arg( destCrsId ) );
  dstProjection = destCrs.description();

  sqlite3_finalize( stmt );
  sqlite3_close( db );
  return true;
}

int QgsCoordinateTransform::sourceDatumTransform() const
{
  return d->mSourceDatumTransform;
}

void QgsCoordinateTransform::setSourceDatumTransform( int dt )
{
  d.detach();
  d->mSourceDatumTransform = dt;
}

int QgsCoordinateTransform::destinationDatumTransform() const
{
  return d->mDestinationDatumTransform;
}

void QgsCoordinateTransform::setDestinationDatumTransform( int dt )
{
  d.detach();
  d->mDestinationDatumTransform = dt;
}

void QgsCoordinateTransform::initialise()
{
  d.detach();
  d->initialise();
}
