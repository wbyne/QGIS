/***************************************************************************
    qgshighlight.cpp - widget to highlight features on the map
     --------------------------------------
    Date                 : 02-03-2011
    Copyright            : (C) 2011 by Juergen E. Fischer, norBIT GmbH
    Email                : jef at norbit dot de
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QImage>

#include "qgsmarkersymbollayer.h"
#include "qgslinesymbollayer.h"

#include "qgscoordinatetransform.h"
#include "qgsfillsymbollayer.h"
#include "qgsgeometry.h"
#include "qgshighlight.h"
#include "qgsmapcanvas.h"
#include "qgsmaplayer.h"
#include "qgsrendercontext.h"
#include "qgssymbollayer.h"
#include "qgssymbol.h"
#include "qgsvectorlayer.h"
#include "qgsrenderer.h"

/* Few notes about highlighting (RB):
 - The highlight fill must always be partially transparent because above highlighted layer
   may be another layer which must remain partially visible.
 - Because single highlight color does not work well with layers using similar layer color
   there were considered various possibilities but no optimal solution was found.
   What does not work:
   - lighter/darker color: it would work more or less for fully opaque highlight, but
     overlaying transparent lighter color over original has small visual efect.
   - complemetary color: mixing transparent (128) complement color with original color
     results in grey for all colors
   - contrast line style/ fill pattern: impression is not highligh but just different style
   - line buffer with contrast (or 2 contrast) color: the same as with patterns, no highlight impression
   - fill with highlight or contrast color but opaque and using pattern
     (e.g. Qt::Dense7Pattern): again no highlight impression
*/
/*!
  \class QgsHighlight
  \brief The QgsHighlight class provides a transparent overlay widget
  for highlighting features on the map.
*/
QgsHighlight::QgsHighlight( QgsMapCanvas* mapCanvas, const QgsGeometry& geom, QgsMapLayer *layer )
    : QgsMapCanvasItem( mapCanvas )
    , mLayer( layer )
    , mBuffer( 0 )
    , mMinWidth( 0 )
{
  mGeometry = !geom.isEmpty() ? new QgsGeometry( geom ) : nullptr;
  init();
}

QgsHighlight::QgsHighlight( QgsMapCanvas* mapCanvas, const QgsGeometry& geom, QgsVectorLayer *layer )
    : QgsMapCanvasItem( mapCanvas )
    , mLayer( static_cast<QgsMapLayer *>( layer ) )
    , mBuffer( 0 )
    , mMinWidth( 0 )
{
  mGeometry = !geom.isEmpty() ? new QgsGeometry( geom ) : nullptr;
  init();
}

QgsHighlight::QgsHighlight( QgsMapCanvas* mapCanvas, const QgsFeature& feature, QgsVectorLayer *layer )
    : QgsMapCanvasItem( mapCanvas )
    , mGeometry( nullptr )
    , mLayer( static_cast<QgsMapLayer *>( layer ) )
    , mFeature( feature )
    , mBuffer( 0 )
    , mMinWidth( 0 )
{
  init();
}

void QgsHighlight::init()
{
  if ( mMapCanvas->mapSettings().hasCrsTransformEnabled() )
  {
    QgsCoordinateTransform ct = mMapCanvas->mapSettings().layerTransform( mLayer );
    if ( ct.isValid() )
    {
      if ( mGeometry )
      {
        mGeometry->transform( ct );
      }
      else if ( mFeature.hasGeometry() )
      {
        QgsGeometry g = mFeature.geometry();
        g.transform( ct );
        mFeature.setGeometry( g );
      }
    }
  }
  updateRect();
  update();
  setColor( QColor( Qt::lightGray ) );
}

QgsHighlight::~QgsHighlight()
{
  delete mGeometry;
}

/*!
  Set the outline and fill color.
  */
void QgsHighlight::setColor( const QColor & color )
{
  mPen.setColor( color );
  QColor fillColor( color.red(), color.green(), color.blue(), 63 );
  mBrush.setColor( fillColor );
  mBrush.setStyle( Qt::SolidPattern );
}

void QgsHighlight::setFillColor( const QColor & fillColor )
{
  mBrush.setColor( fillColor );
  mBrush.setStyle( Qt::SolidPattern );
}

QgsFeatureRenderer * QgsHighlight::getRenderer( QgsRenderContext & context, const QColor & color, const QColor & fillColor )
{
  QgsFeatureRenderer *renderer = nullptr;
  QgsVectorLayer *layer = qobject_cast<QgsVectorLayer*>( mLayer );
  if ( layer && layer->renderer() )
  {
    renderer = layer->renderer()->clone();
  }
  if ( renderer )
  {
    Q_FOREACH ( QgsSymbol* symbol, renderer->symbols( context ) )
    {
      if ( !symbol ) continue;
      setSymbol( symbol, context, color, fillColor );
    }
  }
  return renderer;
}

void QgsHighlight::setSymbol( QgsSymbol* symbol, const QgsRenderContext & context,   const QColor & color, const QColor & fillColor )
{
  if ( !symbol ) return;


  for ( int i = symbol->symbolLayerCount() - 1; i >= 0;  i-- )
  {
    QgsSymbolLayer* symbolLayer = symbol->symbolLayer( i );
    if ( !symbolLayer ) continue;

    if ( symbolLayer->subSymbol() )
    {
      setSymbol( symbolLayer->subSymbol(), context, color, fillColor );
    }
    else
    {
      symbolLayer->setColor( color ); // line symbology layers
      symbolLayer->setOutlineColor( color ); // marker and fill symbology layers
      symbolLayer->setFillColor( fillColor ); // marker and fill symbology layers

      // Data defined widths overwrite what we set here (widths do not work with data defined)
      QgsSimpleMarkerSymbolLayer * simpleMarker = dynamic_cast<QgsSimpleMarkerSymbolLayer*>( symbolLayer );
      if ( simpleMarker )
      {
        simpleMarker->setOutlineWidth( getSymbolWidth( context, simpleMarker->outlineWidth(), simpleMarker->outlineWidthUnit() ) );
      }
      QgsSimpleLineSymbolLayer * simpleLine = dynamic_cast<QgsSimpleLineSymbolLayer*>( symbolLayer );
      if ( simpleLine )
      {
        simpleLine->setWidth( getSymbolWidth( context, simpleLine->width(), simpleLine->widthUnit() ) );
      }
      QgsSimpleFillSymbolLayer * simpleFill = dynamic_cast<QgsSimpleFillSymbolLayer*>( symbolLayer );
      if ( simpleFill )
      {
        simpleFill->setBorderWidth( getSymbolWidth( context, simpleFill->borderWidth(), simpleFill->outputUnit() ) );
      }
      symbolLayer->removeDataDefinedProperty( "color" );
      symbolLayer->removeDataDefinedProperty( "color_border" );
    }
  }
}

double QgsHighlight::getSymbolWidth( const QgsRenderContext & context, double width, QgsUnitTypes::RenderUnit unit )
{
  // if necessary scale mm to map units
  double scale = 1.;
  if ( unit == QgsUnitTypes::RenderMapUnits )
  {
    scale = QgsSymbolLayerUtils::lineWidthScaleFactor( context, QgsUnitTypes::RenderMillimeters ) / QgsSymbolLayerUtils::lineWidthScaleFactor( context, QgsUnitTypes::RenderMapUnits );
  }
  width =  qMax( width + 2 * mBuffer * scale, mMinWidth * scale );
  return width;
}

/*!
  Set the outline width.
  */
void QgsHighlight::setWidth( int width )
{
  mPen.setWidth( width );
}

void QgsHighlight::paintPoint( QPainter *p, const QgsPoint& point )
{
  QPolygonF r( 5 );

  double d = mMapCanvas->extent().width() * 0.005;
  r[0] = toCanvasCoordinates( point + QgsVector( -d, -d ) ) - pos();
  r[1] = toCanvasCoordinates( point + QgsVector( d, -d ) ) - pos();
  r[2] = toCanvasCoordinates( point + QgsVector( d, d ) ) - pos();
  r[3] = toCanvasCoordinates( point + QgsVector( -d, d ) ) - pos();
  r[4] = r[0];

  p->drawPolygon( r );
}

void QgsHighlight::paintLine( QPainter *p, QgsPolyline line )
{
  QPolygonF polygon( line.size() );

  for ( int i = 0; i < line.size(); i++ )
  {
    polygon[i] = toCanvasCoordinates( line[i] ) - pos();
  }

  p->drawPolyline( polygon );
}

void QgsHighlight::paintPolygon( QPainter *p, QgsPolygon polygon )
{
  // OddEven fill rule by default
  QPainterPath path;

  p->setPen( mPen );
  p->setBrush( mBrush );

  for ( int i = 0; i < polygon.size(); i++ )
  {
    if ( polygon[i].empty() ) continue;

    QPolygonF ring;
    ring.reserve( polygon[i].size() + 1 );

    for ( int j = 0; j < polygon[i].size(); j++ )
    {
      //adding point only if it is more than a pixel appart from the previous one
      const QPointF cur = toCanvasCoordinates( polygon[i][j] ) - pos();
      if ( 0 == j || std::abs( ring.back().x() - cur.x() ) > 1 || std::abs( ring.back().y() - cur.y() ) > 1 )
      {
        ring.push_back( cur );
      }
    }

    ring.push_back( ring[ 0 ] );

    path.addPolygon( ring );
  }

  p->drawPath( path );
}

void QgsHighlight::updatePosition()
{
  // nothing to do here...
}

/*!
  Draw the shape in response to an update event.
  */
void QgsHighlight::paint( QPainter* p )
{
  if ( mGeometry )
  {
    p->setPen( mPen );
    p->setBrush( mBrush );

    switch ( mGeometry->wkbType() )
    {
      case QgsWkbTypes::Point:
      case QgsWkbTypes::Point25D:
      {
        paintPoint( p, mGeometry->asPoint() );
      }
      break;

      case QgsWkbTypes::MultiPoint:
      case QgsWkbTypes::MultiPoint25D:
      {
        QgsMultiPoint m = mGeometry->asMultiPoint();
        for ( int i = 0; i < m.size(); i++ )
        {
          paintPoint( p, m[i] );
        }
      }
      break;

      case QgsWkbTypes::LineString:
      case QgsWkbTypes::LineString25D:
      {
        paintLine( p, mGeometry->asPolyline() );
      }
      break;

      case QgsWkbTypes::MultiLineString:
      case QgsWkbTypes::MultiLineString25D:
      {
        QgsMultiPolyline m = mGeometry->asMultiPolyline();

        for ( int i = 0; i < m.size(); i++ )
        {
          paintLine( p, m[i] );
        }
      }
      break;

      case QgsWkbTypes::Polygon:
      case QgsWkbTypes::Polygon25D:
      {
        paintPolygon( p, mGeometry->asPolygon() );
      }
      break;

      case QgsWkbTypes::MultiPolygon:
      case QgsWkbTypes::MultiPolygon25D:
      {
        QgsMultiPolygon m = mGeometry->asMultiPolygon();
        for ( int i = 0; i < m.size(); i++ )
        {
          paintPolygon( p, m[i] );
        }
      }
      break;

      case QgsWkbTypes::Unknown:
      default:
        return;
    }
  }
  else if ( mFeature.hasGeometry() )
  {
    QgsVectorLayer *layer = qobject_cast<QgsVectorLayer*>( mLayer );
    if ( !layer )
      return;
    QgsMapSettings mapSettings = mMapCanvas->mapSettings();
    QgsRenderContext context = QgsRenderContext::fromMapSettings( mapSettings );

    // Because lower level outlines must be covered by upper level fill color
    // we render first with temporary opaque color, which is then replaced
    // by final transparent fill color.
    QColor tmpColor( 255, 0, 0, 255 );
    QColor tmpFillColor( 0, 255, 0, 255 );

    QgsFeatureRenderer *renderer = getRenderer( context, tmpColor, tmpFillColor );
    if ( layer && renderer )
    {

      QSize imageSize( mMapCanvas->mapSettings().outputSize() );
      QImage image = QImage( imageSize.width(), imageSize.height(), QImage::Format_ARGB32 );
      image.fill( 0 );
      QPainter *imagePainter = new QPainter( &image );
      imagePainter->setRenderHint( QPainter::Antialiasing, true );

      context.setPainter( imagePainter );

      renderer->startRender( context, layer->fields() );
      renderer->renderFeature( mFeature, context );
      renderer->stopRender( context );

      imagePainter->end();

      QColor color( mPen.color() );  // true output color
      // coefficient to subtract alpha using green (temporary fill)
      double k = ( 255. - mBrush.color().alpha() ) / 255.;
      for ( int r = 0; r < image.height(); r++ )
      {
        for ( int c = 0; c < image.width(); c++ )
        {
          QRgb rgba = image.pixel( c, r );
          int alpha = qAlpha( rgba );
          if ( alpha > 0 )
          {
            int green = qGreen( rgba );
            color.setAlpha( qBound<int>( 0, alpha - ( green * k ), 255 ) );

            image.setPixel( c, r, color.rgba() );
          }
        }
      }

      p->drawImage( 0, 0, image );

      delete imagePainter;
      delete renderer;
    }
  }
}

void QgsHighlight::updateRect()
{
  if ( mGeometry )
  {
    QgsRectangle r = mGeometry->boundingBox();

    if ( r.isEmpty() )
    {
      double d = mMapCanvas->extent().width() * 0.005;
      r.setXMinimum( r.xMinimum() - d );
      r.setYMinimum( r.yMinimum() - d );
      r.setXMaximum( r.xMaximum() + d );
      r.setYMaximum( r.yMaximum() + d );
    }

    setRect( r );
    setVisible( mGeometry );
  }
  else if ( mFeature.hasGeometry() )
  {
    // We are currently using full map canvas extent for two reasons:
    // 1) currently there is no method in QgsFeatureRenderer to get rendered feature
    //    bounding box
    // 2) using different extent would result in shifted fill patterns

    // This is an hack to pass QgsMapCanvasItem::setRect what it
    // expects (encoding of position and size of the item)
    const QgsMapToPixel& m2p = mMapCanvas->mapSettings().mapToPixel();
    QgsPoint topLeft = m2p.toMapPoint( 0, 0 );
    double res = m2p.mapUnitsPerPixel();
    QSizeF imageSize = mMapCanvas->mapSettings().outputSize();
    QgsRectangle rect( topLeft.x(), topLeft.y(), topLeft.x() + imageSize.width()*res, topLeft.y() - imageSize.height()*res );
    setRect( rect );

    setVisible( true );
  }
  else
  {
    setRect( QgsRectangle() );
  }
}
