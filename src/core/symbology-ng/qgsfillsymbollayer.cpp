/***************************************************************************
 qgsfillsymbollayer.cpp
 ---------------------
 begin                : November 2009
 copyright            : (C) 2009 by Martin Dobias
 email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsfillsymbollayer.h"
#include "qgslinesymbollayer.h"
#include "qgsmarkersymbollayer.h"
#include "qgssymbollayerutils.h"
#include "qgsdxfexport.h"
#include "qgsexpression.h"
#include "qgsgeometry.h"
#include "qgsgeometrycollection.h"
#include "qgsrendercontext.h"
#include "qgsproject.h"
#include "qgssvgcache.h"
#include "qgslogger.h"
#include "qgscolorramp.h"
#include "qgsunittypes.h"

#include <QPainter>
#include <QFile>
#include <QSvgRenderer>
#include <QDomDocument>
#include <QDomElement>

QgsSimpleFillSymbolLayer::QgsSimpleFillSymbolLayer( const QColor& color, Qt::BrushStyle style, const QColor& borderColor, Qt::PenStyle borderStyle, double borderWidth,
    Qt::PenJoinStyle penJoinStyle )
    : mBrushStyle( style )
    , mBorderColor( borderColor )
    , mBorderStyle( borderStyle )
    , mBorderWidth( borderWidth )
    , mBorderWidthUnit( QgsUnitTypes::RenderMillimeters )
    , mPenJoinStyle( penJoinStyle )
    , mOffsetUnit( QgsUnitTypes::RenderMillimeters )
{
  mColor = color;
}

void QgsSimpleFillSymbolLayer::setOutputUnit( QgsUnitTypes::RenderUnit unit )
{
  mBorderWidthUnit = unit;
  mOffsetUnit = unit;
}

QgsUnitTypes::RenderUnit QgsSimpleFillSymbolLayer::outputUnit() const
{
  QgsUnitTypes::RenderUnit unit = mBorderWidthUnit;
  if ( mOffsetUnit != unit )
  {
    return QgsUnitTypes::RenderUnknownUnit;
  }
  return unit;
}

void QgsSimpleFillSymbolLayer::setMapUnitScale( const QgsMapUnitScale &scale )
{
  mBorderWidthMapUnitScale = scale;
  mOffsetMapUnitScale = scale;
}

QgsMapUnitScale QgsSimpleFillSymbolLayer::mapUnitScale() const
{
  if ( mBorderWidthMapUnitScale == mOffsetMapUnitScale )
  {
    return mBorderWidthMapUnitScale;
  }
  return QgsMapUnitScale();
}

void QgsSimpleFillSymbolLayer::applyDataDefinedSymbology( QgsSymbolRenderContext& context, QBrush& brush, QPen& pen, QPen& selPen )
{
  if ( !hasDataDefinedProperties() )
    return; // shortcut

  bool ok;

  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mColor ) );
    QString color = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR, context, QVariant(), &ok ).toString();
    if ( ok )
      brush.setColor( QgsSymbolLayerUtils::decodeColor( color ) );
  }
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_FILL_STYLE ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeBrushStyle( mBrushStyle ) );
    QString style = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_FILL_STYLE, context, QVariant(), &ok ).toString();
    if ( ok )
      brush.setStyle( QgsSymbolLayerUtils::decodeBrushStyle( style ) );
  }
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR_BORDER ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mBorderColor ) );
    QString color = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR_BORDER, context, QVariant(), &ok ).toString();
    if ( ok )
      pen.setColor( QgsSymbolLayerUtils::decodeColor( color ) );
  }
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH_BORDER ) )
  {
    context.setOriginalValueVariable( mBorderWidth );
    double width = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH_BORDER, context, mBorderWidth ).toDouble();
    width = QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), width, mBorderWidthUnit, mBorderWidthMapUnitScale );
    pen.setWidthF( width );
    selPen.setWidthF( width );
  }
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_BORDER_STYLE ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodePenStyle( mBorderStyle ) );
    QString style = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_BORDER_STYLE, context, QVariant(), &ok ).toString();
    if ( ok )
    {
      pen.setStyle( QgsSymbolLayerUtils::decodePenStyle( style ) );
      selPen.setStyle( QgsSymbolLayerUtils::decodePenStyle( style ) );
    }
  }
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_JOIN_STYLE ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodePenJoinStyle( mPenJoinStyle ) );
    QString style = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_JOIN_STYLE, context, QVariant(), &ok ).toString();
    if ( ok )
    {
      pen.setJoinStyle( QgsSymbolLayerUtils::decodePenJoinStyle( style ) );
      selPen.setJoinStyle( QgsSymbolLayerUtils::decodePenJoinStyle( style ) );
    }
  }
}


QgsSymbolLayer* QgsSimpleFillSymbolLayer::create( const QgsStringMap& props )
{
  QColor color = DEFAULT_SIMPLEFILL_COLOR;
  Qt::BrushStyle style = DEFAULT_SIMPLEFILL_STYLE;
  QColor borderColor = DEFAULT_SIMPLEFILL_BORDERCOLOR;
  Qt::PenStyle borderStyle = DEFAULT_SIMPLEFILL_BORDERSTYLE;
  double borderWidth = DEFAULT_SIMPLEFILL_BORDERWIDTH;
  Qt::PenJoinStyle penJoinStyle = DEFAULT_SIMPLEFILL_JOINSTYLE;
  QPointF offset;

  if ( props.contains( "color" ) )
    color = QgsSymbolLayerUtils::decodeColor( props["color"] );
  if ( props.contains( "style" ) )
    style = QgsSymbolLayerUtils::decodeBrushStyle( props["style"] );
  if ( props.contains( "color_border" ) )
  {
    //pre 2.5 projects used "color_border"
    borderColor = QgsSymbolLayerUtils::decodeColor( props["color_border"] );
  }
  else if ( props.contains( "outline_color" ) )
  {
    borderColor = QgsSymbolLayerUtils::decodeColor( props["outline_color"] );
  }
  else if ( props.contains( "line_color" ) )
  {
    borderColor = QgsSymbolLayerUtils::decodeColor( props["line_color"] );
  }

  if ( props.contains( "style_border" ) )
  {
    //pre 2.5 projects used "style_border"
    borderStyle = QgsSymbolLayerUtils::decodePenStyle( props["style_border"] );
  }
  else if ( props.contains( "outline_style" ) )
  {
    borderStyle = QgsSymbolLayerUtils::decodePenStyle( props["outline_style"] );
  }
  else if ( props.contains( "line_style" ) )
  {
    borderStyle = QgsSymbolLayerUtils::decodePenStyle( props["line_style"] );
  }
  if ( props.contains( "width_border" ) )
  {
    //pre 2.5 projects used "width_border"
    borderWidth = props["width_border"].toDouble();
  }
  else if ( props.contains( "outline_width" ) )
  {
    borderWidth = props["outline_width"].toDouble();
  }
  else if ( props.contains( "line_width" ) )
  {
    borderWidth = props["line_width"].toDouble();
  }
  if ( props.contains( "offset" ) )
    offset = QgsSymbolLayerUtils::decodePoint( props["offset"] );
  if ( props.contains( "joinstyle" ) )
    penJoinStyle = QgsSymbolLayerUtils::decodePenJoinStyle( props["joinstyle"] );

  QgsSimpleFillSymbolLayer* sl = new QgsSimpleFillSymbolLayer( color, style, borderColor, borderStyle, borderWidth, penJoinStyle );
  sl->setOffset( offset );
  if ( props.contains( "border_width_unit" ) )
  {
    sl->setBorderWidthUnit( QgsUnitTypes::decodeRenderUnit( props["border_width_unit"] ) );
  }
  else if ( props.contains( "outline_width_unit" ) )
  {
    sl->setBorderWidthUnit( QgsUnitTypes::decodeRenderUnit( props["outline_width_unit"] ) );
  }
  else if ( props.contains( "line_width_unit" ) )
  {
    sl->setBorderWidthUnit( QgsUnitTypes::decodeRenderUnit( props["line_width_unit"] ) );
  }
  if ( props.contains( "offset_unit" ) )
    sl->setOffsetUnit( QgsUnitTypes::decodeRenderUnit( props["offset_unit"] ) );

  if ( props.contains( "border_width_map_unit_scale" ) )
    sl->setBorderWidthMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( props["border_width_map_unit_scale"] ) );
  if ( props.contains( "offset_map_unit_scale" ) )
    sl->setOffsetMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( props["offset_map_unit_scale"] ) );

  sl->restoreDataDefinedProperties( props );

  return sl;
}


QString QgsSimpleFillSymbolLayer::layerType() const
{
  return "SimpleFill";
}

void QgsSimpleFillSymbolLayer::startRender( QgsSymbolRenderContext& context )
{
  QColor fillColor = mColor;
  fillColor.setAlphaF( context.alpha() * mColor.alphaF() );
  mBrush = QBrush( fillColor, mBrushStyle );

  // scale brush content for printout
  double rasterScaleFactor = context.renderContext().rasterScaleFactor();
  if ( rasterScaleFactor != 1.0 )
  {
    mBrush.setMatrix( QMatrix().scale( 1.0 / rasterScaleFactor, 1.0 / rasterScaleFactor ) );
  }

  QColor selColor = context.renderContext().selectionColor();
  QColor selPenColor = selColor == mColor ? selColor : mBorderColor;
  if ( ! selectionIsOpaque ) selColor.setAlphaF( context.alpha() );
  mSelBrush = QBrush( selColor );
  // N.B. unless a "selection line color" is implemented in addition to the "selection color" option
  // this would mean symbols with "no fill" look the same whether or not they are selected
  if ( selectFillStyle )
    mSelBrush.setStyle( mBrushStyle );

  QColor borderColor = mBorderColor;
  borderColor.setAlphaF( context.alpha() * mBorderColor.alphaF() );
  mPen = QPen( borderColor );
  mSelPen = QPen( selPenColor );
  mPen.setStyle( mBorderStyle );
  mPen.setWidthF( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mBorderWidth, mBorderWidthUnit, mBorderWidthMapUnitScale ) );
  mPen.setJoinStyle( mPenJoinStyle );
  prepareExpressions( context );
}

void QgsSimpleFillSymbolLayer::stopRender( QgsSymbolRenderContext& context )
{
  Q_UNUSED( context );
}

void QgsSimpleFillSymbolLayer::renderPolygon( const QPolygonF& points, QList<QPolygonF>* rings, QgsSymbolRenderContext& context )
{
  QPainter* p = context.renderContext().painter();
  if ( !p )
  {
    return;
  }

  applyDataDefinedSymbology( context, mBrush, mPen, mSelPen );

  p->setBrush( context.selected() ? mSelBrush : mBrush );
  p->setPen( context.selected() ? mSelPen : mPen );

  QPointF offset;
  if ( !mOffset.isNull() )
  {
    offset.setX( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.x(), mOffsetUnit, mOffsetMapUnitScale ) );
    offset.setY( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.y(), mOffsetUnit, mOffsetMapUnitScale ) );
    p->translate( offset );
  }

  _renderPolygon( p, points, rings, context );

  if ( !mOffset.isNull() )
  {
    p->translate( -offset );
  }
}

QgsStringMap QgsSimpleFillSymbolLayer::properties() const
{
  QgsStringMap map;
  map["color"] = QgsSymbolLayerUtils::encodeColor( mColor );
  map["style"] = QgsSymbolLayerUtils::encodeBrushStyle( mBrushStyle );
  map["outline_color"] = QgsSymbolLayerUtils::encodeColor( mBorderColor );
  map["outline_style"] = QgsSymbolLayerUtils::encodePenStyle( mBorderStyle );
  map["outline_width"] = QString::number( mBorderWidth );
  map["outline_width_unit"] = QgsUnitTypes::encodeUnit( mBorderWidthUnit );
  map["border_width_map_unit_scale"] = QgsSymbolLayerUtils::encodeMapUnitScale( mBorderWidthMapUnitScale );
  map["joinstyle"] = QgsSymbolLayerUtils::encodePenJoinStyle( mPenJoinStyle );
  map["offset"] = QgsSymbolLayerUtils::encodePoint( mOffset );
  map["offset_unit"] = QgsUnitTypes::encodeUnit( mOffsetUnit );
  map["offset_map_unit_scale"] = QgsSymbolLayerUtils::encodeMapUnitScale( mOffsetMapUnitScale );
  saveDataDefinedProperties( map );
  return map;
}

QgsSimpleFillSymbolLayer* QgsSimpleFillSymbolLayer::clone() const
{
  QgsSimpleFillSymbolLayer* sl = new QgsSimpleFillSymbolLayer( mColor, mBrushStyle, mBorderColor, mBorderStyle, mBorderWidth, mPenJoinStyle );
  sl->setOffset( mOffset );
  sl->setOffsetUnit( mOffsetUnit );
  sl->setOffsetMapUnitScale( mOffsetMapUnitScale );
  sl->setBorderWidthUnit( mBorderWidthUnit );
  sl->setBorderWidthMapUnitScale( mBorderWidthMapUnitScale );
  copyDataDefinedProperties( sl );
  copyPaintEffect( sl );
  return sl;
}

void QgsSimpleFillSymbolLayer::toSld( QDomDocument &doc, QDomElement &element, const QgsStringMap& props ) const
{
  if ( mBrushStyle == Qt::NoBrush && mBorderStyle == Qt::NoPen )
    return;

  QDomElement symbolizerElem = doc.createElement( "se:PolygonSymbolizer" );
  if ( !props.value( "uom", "" ).isEmpty() )
    symbolizerElem.setAttribute( "uom", props.value( "uom", "" ) );
  element.appendChild( symbolizerElem );

  // <Geometry>
  QgsSymbolLayerUtils::createGeometryElement( doc, symbolizerElem, props.value( "geom", "" ) );

  if ( mBrushStyle != Qt::NoBrush )
  {
    // <Fill>
    QDomElement fillElem = doc.createElement( "se:Fill" );
    symbolizerElem.appendChild( fillElem );
    QgsSymbolLayerUtils::fillToSld( doc, fillElem, mBrushStyle, mColor );
  }

  if ( mBorderStyle != Qt::NoPen )
  {
    // <Stroke>
    QDomElement strokeElem = doc.createElement( "se:Stroke" );
    symbolizerElem.appendChild( strokeElem );
    double borderWidth = QgsSymbolLayerUtils::rescaleUom( mBorderWidth, mBorderWidthUnit, props );
    QgsSymbolLayerUtils::lineToSld( doc, strokeElem, mBorderStyle, borderWidth, borderWidth, &mPenJoinStyle );
  }

  // <se:Displacement>
  QPointF offset = QgsSymbolLayerUtils::rescaleUom( mOffset, mOffsetUnit, props );
  QgsSymbolLayerUtils::createDisplacementElement( doc, symbolizerElem, offset );
}

QString QgsSimpleFillSymbolLayer::ogrFeatureStyle( double mmScaleFactor, double mapUnitScaleFactor ) const
{
  //brush
  QString symbolStyle;
  symbolStyle.append( QgsSymbolLayerUtils::ogrFeatureStyleBrush( mColor ) );
  symbolStyle.append( ';' );
  //pen
  symbolStyle.append( QgsSymbolLayerUtils::ogrFeatureStylePen( mBorderWidth, mmScaleFactor, mapUnitScaleFactor, mBorderColor, mPenJoinStyle ) );
  return symbolStyle;
}

QgsSymbolLayer* QgsSimpleFillSymbolLayer::createFromSld( QDomElement &element )
{
  QgsDebugMsg( "Entered." );

  QColor color, borderColor;
  Qt::BrushStyle fillStyle;
  Qt::PenStyle borderStyle;
  double borderWidth;

  QDomElement fillElem = element.firstChildElement( "Fill" );
  QgsSymbolLayerUtils::fillFromSld( fillElem, fillStyle, color );

  QDomElement strokeElem = element.firstChildElement( "Stroke" );
  QgsSymbolLayerUtils::lineFromSld( strokeElem, borderStyle, borderColor, borderWidth );

  QPointF offset;
  QgsSymbolLayerUtils::displacementFromSldElement( element, offset );

  QgsSimpleFillSymbolLayer* sl = new QgsSimpleFillSymbolLayer( color, fillStyle, borderColor, borderStyle, borderWidth );
  sl->setOffset( offset );
  return sl;
}

double QgsSimpleFillSymbolLayer::estimateMaxBleed() const
{
  double penBleed = mBorderStyle == Qt::NoPen ? 0 : ( mBorderWidth / 2.0 );
  double offsetBleed = mOffset.x() > mOffset.y() ? mOffset.x() : mOffset.y();
  return penBleed + offsetBleed;
}

double QgsSimpleFillSymbolLayer::dxfWidth( const QgsDxfExport& e, QgsSymbolRenderContext &context ) const
{
  double width = mBorderWidth;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH_BORDER ) )
  {
    context.setOriginalValueVariable( mBorderWidth );
    width = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH_BORDER, context, mBorderWidth ).toDouble();
  }
  return width * e.mapUnitScaleFactor( e.symbologyScaleDenominator(), mBorderWidthUnit, e.mapUnits() );
}

QColor QgsSimpleFillSymbolLayer::dxfColor( QgsSymbolRenderContext &context ) const
{
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_BORDER_COLOR ) )
  {
    bool ok;
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mBorderColor ) );
    QString color = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_BORDER_COLOR, context, QVariant(), &ok ).toString();
    if ( ok )
      return QgsSymbolLayerUtils::decodeColor( color );
  }
  return mBorderColor;
}

double QgsSimpleFillSymbolLayer::dxfAngle( QgsSymbolRenderContext &context ) const
{
  double angle = mAngle;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE ) )
  {
    context.setOriginalValueVariable( mAngle );
    angle = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE, context, mAngle ).toDouble();
  }
  return angle;
}

Qt::PenStyle QgsSimpleFillSymbolLayer::dxfPenStyle() const
{
  return mBorderStyle;
}

QColor QgsSimpleFillSymbolLayer::dxfBrushColor( QgsSymbolRenderContext& context ) const
{
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR ) )
  {
    bool ok;
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mColor ) );
    QString color = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR, context, QVariant(), &ok ).toString();
    if ( ok )
      return QgsSymbolLayerUtils::decodeColor( color );
  }
  return mColor;
}

Qt::BrushStyle QgsSimpleFillSymbolLayer::dxfBrushStyle() const
{
  return mBrushStyle;
}

//QgsGradientFillSymbolLayer

QgsGradientFillSymbolLayer::QgsGradientFillSymbolLayer( const QColor& color, const QColor& color2,
    GradientColorType colorType, GradientType gradientType,
    GradientCoordinateMode coordinateMode, GradientSpread spread )
    : mGradientColorType( colorType )
    , mGradientRamp( nullptr )
    , mGradientType( gradientType )
    , mCoordinateMode( coordinateMode )
    , mGradientSpread( spread )
    , mReferencePoint1( QPointF( 0.5, 0 ) )
    , mReferencePoint1IsCentroid( false )
    , mReferencePoint2( QPointF( 0.5, 1 ) )
    , mReferencePoint2IsCentroid( false )
    , mOffsetUnit( QgsUnitTypes::RenderMillimeters )
{
  mColor = color;
  mColor2 = color2;
}

QgsGradientFillSymbolLayer::~QgsGradientFillSymbolLayer()
{
  delete mGradientRamp;
}

QgsSymbolLayer* QgsGradientFillSymbolLayer::create( const QgsStringMap& props )
{
  //default to a two-color, linear gradient with feature mode and pad spreading
  GradientType type = QgsGradientFillSymbolLayer::Linear;
  GradientColorType colorType = QgsGradientFillSymbolLayer::SimpleTwoColor;
  GradientCoordinateMode coordinateMode = QgsGradientFillSymbolLayer::Feature;
  GradientSpread gradientSpread = QgsGradientFillSymbolLayer::Pad;
  //default to gradient from the default fill color to white
  QColor color = DEFAULT_SIMPLEFILL_COLOR, color2 = Qt::white;
  QPointF referencePoint1 = QPointF( 0.5, 0 );
  bool refPoint1IsCentroid = false;
  QPointF referencePoint2 = QPointF( 0.5, 1 );
  bool refPoint2IsCentroid = false;
  double angle = 0;
  QPointF offset;

  //update gradient properties from props
  if ( props.contains( "type" ) )
    type = static_cast< GradientType >( props["type"].toInt() );
  if ( props.contains( "coordinate_mode" ) )
    coordinateMode = static_cast< GradientCoordinateMode >( props["coordinate_mode"].toInt() );
  if ( props.contains( "spread" ) )
    gradientSpread = static_cast< GradientSpread >( props["spread"].toInt() );
  if ( props.contains( "color_type" ) )
    colorType = static_cast< GradientColorType >( props["color_type"].toInt() );
  if ( props.contains( "gradient_color" ) )
  {
    //pre 2.5 projects used "gradient_color"
    color = QgsSymbolLayerUtils::decodeColor( props["gradient_color"] );
  }
  else if ( props.contains( "color" ) )
  {
    color = QgsSymbolLayerUtils::decodeColor( props["color"] );
  }
  if ( props.contains( "gradient_color2" ) )
  {
    color2 = QgsSymbolLayerUtils::decodeColor( props["gradient_color2"] );
  }

  if ( props.contains( "reference_point1" ) )
    referencePoint1 = QgsSymbolLayerUtils::decodePoint( props["reference_point1"] );
  if ( props.contains( "reference_point1_iscentroid" ) )
    refPoint1IsCentroid = props["reference_point1_iscentroid"].toInt();
  if ( props.contains( "reference_point2" ) )
    referencePoint2 = QgsSymbolLayerUtils::decodePoint( props["reference_point2"] );
  if ( props.contains( "reference_point2_iscentroid" ) )
    refPoint2IsCentroid = props["reference_point2_iscentroid"].toInt();
  if ( props.contains( "angle" ) )
    angle = props["angle"].toDouble();

  if ( props.contains( "offset" ) )
    offset = QgsSymbolLayerUtils::decodePoint( props["offset"] );

  //attempt to create color ramp from props
  QgsColorRamp* gradientRamp = QgsGradientColorRamp::create( props );

  //create a new gradient fill layer with desired properties
  QgsGradientFillSymbolLayer* sl = new QgsGradientFillSymbolLayer( color, color2, colorType, type, coordinateMode, gradientSpread );
  sl->setOffset( offset );
  if ( props.contains( "offset_unit" ) )
    sl->setOffsetUnit( QgsUnitTypes::decodeRenderUnit( props["offset_unit"] ) );
  if ( props.contains( "offset_map_unit_scale" ) )
    sl->setOffsetMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( props["offset_map_unit_scale"] ) );
  sl->setReferencePoint1( referencePoint1 );
  sl->setReferencePoint1IsCentroid( refPoint1IsCentroid );
  sl->setReferencePoint2( referencePoint2 );
  sl->setReferencePoint2IsCentroid( refPoint2IsCentroid );
  sl->setAngle( angle );
  if ( gradientRamp )
    sl->setColorRamp( gradientRamp );

  sl->restoreDataDefinedProperties( props );

  return sl;
}

void QgsGradientFillSymbolLayer::setColorRamp( QgsColorRamp* ramp )
{
  delete mGradientRamp;
  mGradientRamp = ramp;
}

QString QgsGradientFillSymbolLayer::layerType() const
{
  return "GradientFill";
}

void QgsGradientFillSymbolLayer::applyDataDefinedSymbology( QgsSymbolRenderContext& context, const QPolygonF& points )
{
  if ( !hasDataDefinedProperties() && !mReferencePoint1IsCentroid && !mReferencePoint2IsCentroid )
  {
    //shortcut
    applyGradient( context, mBrush, mColor, mColor2,  mGradientColorType, mGradientRamp, mGradientType, mCoordinateMode,
                   mGradientSpread, mReferencePoint1, mReferencePoint2, mAngle );
    return;
  }

  bool ok;

  //first gradient color
  QColor color = mColor;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mColor ) );
    QString colorString = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR, context, QVariant(), &ok ).toString();
    if ( ok )
      color = QgsSymbolLayerUtils::decodeColor( colorString );
  }

  //second gradient color
  QColor color2 = mColor2;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR2 ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mColor2 ) );
    QString colorString = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR2, context, QVariant(), &ok ).toString();
    if ( ok )
      color2 = QgsSymbolLayerUtils::decodeColor( colorString );
  }

  //gradient rotation angle
  double angle = mAngle;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE ) )
  {
    context.setOriginalValueVariable( mAngle );
    angle = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE, context, mAngle ).toDouble();
  }

  //gradient type
  QgsGradientFillSymbolLayer::GradientType gradientType = mGradientType;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_GRADIENT_TYPE ) )
  {
    QString currentType = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_GRADIENT_TYPE, context, QVariant(), &ok ).toString();
    if ( ok )
    {
      if ( currentType == QObject::tr( "linear" ) )
      {
        gradientType = QgsGradientFillSymbolLayer::Linear;
      }
      else if ( currentType == QObject::tr( "radial" ) )
      {
        gradientType = QgsGradientFillSymbolLayer::Radial;
      }
      else if ( currentType == QObject::tr( "conical" ) )
      {
        gradientType = QgsGradientFillSymbolLayer::Conical;
      }
    }
  }

  //coordinate mode
  GradientCoordinateMode coordinateMode = mCoordinateMode;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COORDINATE_MODE ) )
  {
    QString currentCoordMode =  evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COORDINATE_MODE, context, QVariant(), &ok ).toString();
    if ( ok )
    {
      if ( currentCoordMode == QObject::tr( "feature" ) )
      {
        coordinateMode = QgsGradientFillSymbolLayer::Feature;
      }
      else if ( currentCoordMode == QObject::tr( "viewport" ) )
      {
        coordinateMode = QgsGradientFillSymbolLayer::Viewport;
      }
    }
  }

  //gradient spread
  GradientSpread spread = mGradientSpread;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_SPREAD ) )
  {
    QString currentSpread = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_SPREAD, context, QVariant(), &ok ).toString();
    if ( ok )
    {
      if ( currentSpread == QObject::tr( "pad" ) )
      {
        spread = QgsGradientFillSymbolLayer::Pad;
      }
      else if ( currentSpread == QObject::tr( "repeat" ) )
      {
        spread = QgsGradientFillSymbolLayer::Repeat;
      }
      else if ( currentSpread == QObject::tr( "reflect" ) )
      {
        spread = QgsGradientFillSymbolLayer::Reflect;
      }
    }
  }

  //reference point 1 x & y
  double refPoint1X = mReferencePoint1.x();
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE1_X ) )
  {
    context.setOriginalValueVariable( refPoint1X );
    refPoint1X = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE1_X, context, refPoint1X ).toDouble();
  }
  double refPoint1Y = mReferencePoint1.y();
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE1_Y ) )
  {
    context.setOriginalValueVariable( refPoint1Y );
    refPoint1Y = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE1_Y, context, refPoint1Y ).toDouble();
  }
  bool refPoint1IsCentroid = mReferencePoint1IsCentroid;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE1_ISCENTROID ) )
  {
    context.setOriginalValueVariable( refPoint1IsCentroid );
    refPoint1IsCentroid = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE1_ISCENTROID, context, refPoint1IsCentroid ).toBool();
  }

  //reference point 2 x & y
  double refPoint2X = mReferencePoint2.x();
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE2_X ) )
  {
    context.setOriginalValueVariable( refPoint2X );
    refPoint2X = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE2_X, context, refPoint2X ).toDouble();
  }
  double refPoint2Y = mReferencePoint2.y();
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE2_Y ) )
  {
    context.setOriginalValueVariable( refPoint2Y );
    refPoint2Y = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE2_Y, context, refPoint2Y ).toDouble();
  }
  bool refPoint2IsCentroid = mReferencePoint2IsCentroid;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE2_ISCENTROID ) )
  {
    context.setOriginalValueVariable( refPoint2IsCentroid );
    refPoint2IsCentroid = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_REFERENCE2_ISCENTROID, context, refPoint2IsCentroid ).toBool();
  }

  if ( refPoint1IsCentroid || refPoint2IsCentroid )
  {
    //either the gradient is starting or ending at a centroid, so calculate it
    QPointF centroid = QgsSymbolLayerUtils::polygonCentroid( points );
    //centroid coordinates need to be scaled to a range [0, 1] relative to polygon bounds
    QRectF bbox = points.boundingRect();
    double centroidX = ( centroid.x() - bbox.left() ) / bbox.width();
    double centroidY = ( centroid.y() - bbox.top() ) / bbox.height();

    if ( refPoint1IsCentroid )
    {
      refPoint1X = centroidX;
      refPoint1Y = centroidY;
    }
    if ( refPoint2IsCentroid )
    {
      refPoint2X = centroidX;
      refPoint2Y = centroidY;
    }
  }

  //update gradient with data defined values
  applyGradient( context, mBrush, color, color2,  mGradientColorType, mGradientRamp, gradientType, coordinateMode,
                 spread, QPointF( refPoint1X, refPoint1Y ), QPointF( refPoint2X, refPoint2Y ), angle );
}

QPointF QgsGradientFillSymbolLayer::rotateReferencePoint( QPointF refPoint, double angle )
{
  //rotate a reference point by a specified angle around the point (0.5, 0.5)

  //create a line from the centrepoint of a rectangle bounded by (0, 0) and (1, 1) to the reference point
  QLineF refLine = QLineF( QPointF( 0.5, 0.5 ), refPoint );
  //rotate this line by the current rotation angle
  refLine.setAngle( refLine.angle() + angle );
  //get new end point of line
  QPointF rotatedReferencePoint = refLine.p2();
  //make sure coords of new end point is within [0, 1]
  if ( rotatedReferencePoint.x() > 1 )
    rotatedReferencePoint.setX( 1 );
  if ( rotatedReferencePoint.x() < 0 )
    rotatedReferencePoint.setX( 0 );
  if ( rotatedReferencePoint.y() > 1 )
    rotatedReferencePoint.setY( 1 );
  if ( rotatedReferencePoint.y() < 0 )
    rotatedReferencePoint.setY( 0 );

  return rotatedReferencePoint;
}

void QgsGradientFillSymbolLayer::applyGradient( const QgsSymbolRenderContext &context, QBrush &brush,
    const QColor &color, const QColor &color2, GradientColorType gradientColorType,
    QgsColorRamp *gradientRamp, GradientType gradientType,
    GradientCoordinateMode coordinateMode, GradientSpread gradientSpread,
    QPointF referencePoint1, QPointF referencePoint2, const double angle )
{
  //update alpha of gradient colors
  QColor fillColor = color;
  fillColor.setAlphaF( context.alpha() * fillColor.alphaF() );
  QColor fillColor2 = color2;
  fillColor2.setAlphaF( context.alpha() * fillColor2.alphaF() );

  //rotate reference points
  QPointF rotatedReferencePoint1 = !qgsDoubleNear( angle, 0.0 ) ? rotateReferencePoint( referencePoint1, angle ) : referencePoint1;
  QPointF rotatedReferencePoint2 = !qgsDoubleNear( angle, 0.0 ) ? rotateReferencePoint( referencePoint2, angle ) : referencePoint2;

  //create a QGradient with the desired properties
  QGradient gradient;
  switch ( gradientType )
  {
    case QgsGradientFillSymbolLayer::Linear:
      gradient = QLinearGradient( rotatedReferencePoint1, rotatedReferencePoint2 );
      break;
    case QgsGradientFillSymbolLayer::Radial:
      gradient = QRadialGradient( rotatedReferencePoint1, QLineF( rotatedReferencePoint1, rotatedReferencePoint2 ).length() );
      break;
    case QgsGradientFillSymbolLayer::Conical:
      gradient = QConicalGradient( rotatedReferencePoint1, QLineF( rotatedReferencePoint1, rotatedReferencePoint2 ).angle() );
      break;
  }
  switch ( coordinateMode )
  {
    case QgsGradientFillSymbolLayer::Feature:
      gradient.setCoordinateMode( QGradient::ObjectBoundingMode );
      break;
    case QgsGradientFillSymbolLayer::Viewport:
      gradient.setCoordinateMode( QGradient::StretchToDeviceMode );
      break;
  }
  switch ( gradientSpread )
  {
    case QgsGradientFillSymbolLayer::Pad:
      gradient.setSpread( QGradient::PadSpread );
      break;
    case QgsGradientFillSymbolLayer::Reflect:
      gradient.setSpread( QGradient::ReflectSpread );
      break;
    case QgsGradientFillSymbolLayer::Repeat:
      gradient.setSpread( QGradient::RepeatSpread );
      break;
  }

  //add stops to gradient
  if ( gradientColorType == QgsGradientFillSymbolLayer::ColorRamp && gradientRamp && gradientRamp->type() == "gradient" )
  {
    //color ramp gradient
    QgsGradientColorRamp* gradRamp = static_cast<QgsGradientColorRamp*>( gradientRamp );
    gradRamp->addStopsToGradient( &gradient, context.alpha() );
  }
  else
  {
    //two color gradient
    gradient.setColorAt( 0.0, fillColor );
    gradient.setColorAt( 1.0, fillColor2 );
  }

  //update QBrush use gradient
  brush = QBrush( gradient );
}

void QgsGradientFillSymbolLayer::startRender( QgsSymbolRenderContext& context )
{
  QColor selColor = context.renderContext().selectionColor();
  if ( ! selectionIsOpaque ) selColor.setAlphaF( context.alpha() );
  mSelBrush = QBrush( selColor );

  //update mBrush to use a gradient fill with specified properties
  prepareExpressions( context );
}

void QgsGradientFillSymbolLayer::stopRender( QgsSymbolRenderContext& context )
{
  Q_UNUSED( context );
}

void QgsGradientFillSymbolLayer::renderPolygon( const QPolygonF& points, QList<QPolygonF>* rings, QgsSymbolRenderContext& context )
{
  QPainter* p = context.renderContext().painter();
  if ( !p )
  {
    return;
  }

  applyDataDefinedSymbology( context, points );

  p->setBrush( context.selected() ? mSelBrush : mBrush );
  p->setPen( Qt::NoPen );

  QPointF offset;
  if ( !mOffset.isNull() )
  {
    offset.setX( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.x(), mOffsetUnit, mOffsetMapUnitScale ) );
    offset.setY( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.y(), mOffsetUnit, mOffsetMapUnitScale ) );
    p->translate( offset );
  }

  _renderPolygon( p, points, rings, context );

  if ( !mOffset.isNull() )
  {
    p->translate( -offset );
  }
}

QgsStringMap QgsGradientFillSymbolLayer::properties() const
{
  QgsStringMap map;
  map["color"] = QgsSymbolLayerUtils::encodeColor( mColor );
  map["gradient_color2"] = QgsSymbolLayerUtils::encodeColor( mColor2 );
  map["color_type"] = QString::number( mGradientColorType );
  map["type"] = QString::number( mGradientType );
  map["coordinate_mode"] = QString::number( mCoordinateMode );
  map["spread"] = QString::number( mGradientSpread );
  map["reference_point1"] = QgsSymbolLayerUtils::encodePoint( mReferencePoint1 );
  map["reference_point1_iscentroid"] = QString::number( mReferencePoint1IsCentroid );
  map["reference_point2"] = QgsSymbolLayerUtils::encodePoint( mReferencePoint2 );
  map["reference_point2_iscentroid"] = QString::number( mReferencePoint2IsCentroid );
  map["angle"] = QString::number( mAngle );
  map["offset"] = QgsSymbolLayerUtils::encodePoint( mOffset );
  map["offset_unit"] = QgsUnitTypes::encodeUnit( mOffsetUnit );
  map["offset_map_unit_scale"] = QgsSymbolLayerUtils::encodeMapUnitScale( mOffsetMapUnitScale );
  saveDataDefinedProperties( map );
  if ( mGradientRamp )
  {
    map.unite( mGradientRamp->properties() );
  }
  return map;
}

QgsGradientFillSymbolLayer* QgsGradientFillSymbolLayer::clone() const
{
  QgsGradientFillSymbolLayer* sl = new QgsGradientFillSymbolLayer( mColor, mColor2, mGradientColorType, mGradientType, mCoordinateMode, mGradientSpread );
  if ( mGradientRamp )
    sl->setColorRamp( mGradientRamp->clone() );
  sl->setReferencePoint1( mReferencePoint1 );
  sl->setReferencePoint1IsCentroid( mReferencePoint1IsCentroid );
  sl->setReferencePoint2( mReferencePoint2 );
  sl->setReferencePoint2IsCentroid( mReferencePoint2IsCentroid );
  sl->setAngle( mAngle );
  sl->setOffset( mOffset );
  sl->setOffsetUnit( mOffsetUnit );
  sl->setOffsetMapUnitScale( mOffsetMapUnitScale );
  copyDataDefinedProperties( sl );
  copyPaintEffect( sl );
  return sl;
}

double QgsGradientFillSymbolLayer::estimateMaxBleed() const
{
  double offsetBleed = mOffset.x() > mOffset.y() ? mOffset.x() : mOffset.y();
  return offsetBleed;
}

void QgsGradientFillSymbolLayer::setOutputUnit( QgsUnitTypes::RenderUnit unit )
{
  mOffsetUnit = unit;
}

QgsUnitTypes::RenderUnit QgsGradientFillSymbolLayer::outputUnit() const
{
  return mOffsetUnit;
}

void QgsGradientFillSymbolLayer::setMapUnitScale( const QgsMapUnitScale &scale )
{
  mOffsetMapUnitScale = scale;
}

QgsMapUnitScale QgsGradientFillSymbolLayer::mapUnitScale() const
{
  return mOffsetMapUnitScale;
}

//QgsShapeburstFillSymbolLayer

QgsShapeburstFillSymbolLayer::QgsShapeburstFillSymbolLayer( const QColor& color, const QColor& color2, ShapeburstColorType colorType,
    int blurRadius, bool useWholeShape, double maxDistance )
    : mBlurRadius( blurRadius )
    , mUseWholeShape( useWholeShape )
    , mMaxDistance( maxDistance )
    , mDistanceUnit( QgsUnitTypes::RenderMillimeters )
    , mColorType( colorType )
    , mColor2( color2 )
    , mGradientRamp( nullptr )
    , mTwoColorGradientRamp( nullptr )
    , mIgnoreRings( false )
    , mOffsetUnit( QgsUnitTypes::RenderMillimeters )
{
  mColor = color;
}

QgsShapeburstFillSymbolLayer::~QgsShapeburstFillSymbolLayer()
{
  delete mGradientRamp;
}

QgsSymbolLayer* QgsShapeburstFillSymbolLayer::create( const QgsStringMap& props )
{
  //default to a two-color gradient
  ShapeburstColorType colorType = QgsShapeburstFillSymbolLayer::SimpleTwoColor;
  QColor color = DEFAULT_SIMPLEFILL_COLOR, color2 = Qt::white;
  int blurRadius = 0;
  bool useWholeShape = true;
  double maxDistance = 5;
  QPointF offset;

  //update fill properties from props
  if ( props.contains( "color_type" ) )
  {
    colorType = static_cast< ShapeburstColorType >( props["color_type"].toInt() );
  }
  if ( props.contains( "shapeburst_color" ) )
  {
    //pre 2.5 projects used "shapeburst_color"
    color = QgsSymbolLayerUtils::decodeColor( props["shapeburst_color"] );
  }
  else if ( props.contains( "color" ) )
  {
    color = QgsSymbolLayerUtils::decodeColor( props["color"] );
  }

  if ( props.contains( "shapeburst_color2" ) )
  {
    //pre 2.5 projects used "shapeburst_color2"
    color2 = QgsSymbolLayerUtils::decodeColor( props["shapeburst_color2"] );
  }
  else if ( props.contains( "gradient_color2" ) )
  {
    color2 = QgsSymbolLayerUtils::decodeColor( props["gradient_color2"] );
  }
  if ( props.contains( "blur_radius" ) )
  {
    blurRadius = props["blur_radius"].toInt();
  }
  if ( props.contains( "use_whole_shape" ) )
  {
    useWholeShape = props["use_whole_shape"].toInt();
  }
  if ( props.contains( "max_distance" ) )
  {
    maxDistance = props["max_distance"].toDouble();
  }
  if ( props.contains( "offset" ) )
  {
    offset = QgsSymbolLayerUtils::decodePoint( props["offset"] );
  }

  //attempt to create color ramp from props
  QgsColorRamp* gradientRamp = QgsGradientColorRamp::create( props );

  //create a new shapeburst fill layer with desired properties
  QgsShapeburstFillSymbolLayer* sl = new QgsShapeburstFillSymbolLayer( color, color2, colorType, blurRadius, useWholeShape, maxDistance );
  sl->setOffset( offset );
  if ( props.contains( "offset_unit" ) )
  {
    sl->setOffsetUnit( QgsUnitTypes::decodeRenderUnit( props["offset_unit"] ) );
  }
  if ( props.contains( "distance_unit" ) )
  {
    sl->setDistanceUnit( QgsUnitTypes::decodeRenderUnit( props["distance_unit"] ) );
  }
  if ( props.contains( "offset_map_unit_scale" ) )
  {
    sl->setOffsetMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( props["offset_map_unit_scale"] ) );
  }
  if ( props.contains( "distance_map_unit_scale" ) )
  {
    sl->setDistanceMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( props["distance_map_unit_scale"] ) );
  }
  if ( props.contains( "ignore_rings" ) )
  {
    sl->setIgnoreRings( props["ignore_rings"].toInt() );
  }
  if ( gradientRamp )
  {
    sl->setColorRamp( gradientRamp );
  }

  sl->restoreDataDefinedProperties( props );

  return sl;
}

QString QgsShapeburstFillSymbolLayer::layerType() const
{
  return "ShapeburstFill";
}

void QgsShapeburstFillSymbolLayer::setColorRamp( QgsColorRamp* ramp )
{
  delete mGradientRamp;
  mGradientRamp = ramp;
}

void QgsShapeburstFillSymbolLayer::applyDataDefinedSymbology( QgsSymbolRenderContext& context, QColor& color, QColor& color2, int& blurRadius, bool& useWholeShape,
    double& maxDistance, bool& ignoreRings )
{
  bool ok;

  //first gradient color
  color = mColor;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mColor ) );
    QString colorString = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR, context, QVariant(), &ok ).toString();
    if ( ok )
      color = QgsSymbolLayerUtils::decodeColor( colorString );
  }

  //second gradient color
  color2 = mColor2;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR2 ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mColor2 ) );
    QString colorString = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR2, context, QVariant(), &ok ).toString();
    if ( ok )
      color2 = QgsSymbolLayerUtils::decodeColor( colorString );
  }

  //blur radius
  blurRadius = mBlurRadius;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_BLUR_RADIUS ) )
  {
    context.setOriginalValueVariable( mBlurRadius );
    blurRadius = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_BLUR_RADIUS, context, mBlurRadius ).toInt();
  }

  //use whole shape
  useWholeShape = mUseWholeShape;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_USE_WHOLE_SHAPE ) )
  {
    context.setOriginalValueVariable( mUseWholeShape );
    useWholeShape = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_USE_WHOLE_SHAPE, context, mUseWholeShape ).toBool();
  }

  //max distance
  maxDistance = mMaxDistance;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_MAX_DISTANCE ) )
  {
    context.setOriginalValueVariable( mMaxDistance );
    maxDistance = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_MAX_DISTANCE, context, mMaxDistance ).toDouble();
  }

  //ignore rings
  ignoreRings = mIgnoreRings;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_IGNORE_RINGS ) )
  {
    context.setOriginalValueVariable( mIgnoreRings );
    ignoreRings = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_IGNORE_RINGS, context, mIgnoreRings ).toBool();
  }

}

void QgsShapeburstFillSymbolLayer::startRender( QgsSymbolRenderContext& context )
{
  //TODO - check this
  QColor selColor = context.renderContext().selectionColor();
  if ( ! selectionIsOpaque ) selColor.setAlphaF( context.alpha() );
  mSelBrush = QBrush( selColor );

  prepareExpressions( context );
}

void QgsShapeburstFillSymbolLayer::stopRender( QgsSymbolRenderContext& context )
{
  Q_UNUSED( context );
}

void QgsShapeburstFillSymbolLayer::renderPolygon( const QPolygonF& points, QList<QPolygonF>* rings, QgsSymbolRenderContext& context )
{
  QPainter* p = context.renderContext().painter();
  if ( !p )
  {
    return;
  }

  if ( context.selected() )
  {
    //feature is selected, draw using selection style
    p->setBrush( mSelBrush );
    QPointF offset;
    if ( !mOffset.isNull() )
    {
      offset.setX( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.x(), mOffsetUnit, mOffsetMapUnitScale ) );
      offset.setY( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.y(), mOffsetUnit, mOffsetMapUnitScale ) );
      p->translate( offset );
    }
    _renderPolygon( p, points, rings, context );
    if ( !mOffset.isNull() )
    {
      p->translate( -offset );
    }
    return;
  }

  QColor color1, color2;
  int blurRadius;
  bool useWholeShape;
  double maxDistance;
  bool ignoreRings;
  //calculate data defined symbology
  applyDataDefinedSymbology( context, color1, color2, blurRadius, useWholeShape, maxDistance, ignoreRings );

  //calculate max distance for shapeburst fill to extend from polygon boundary, in pixels
  int outputPixelMaxDist = 0;
  if ( !useWholeShape && !qgsDoubleNear( maxDistance, 0.0 ) )
  {
    //convert max distance to pixels
    const QgsRenderContext& ctx = context.renderContext();
    outputPixelMaxDist = maxDistance * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx, mDistanceUnit, mDistanceMapUnitScale );
  }

  //if we are using the two color mode, create a gradient ramp
  if ( mColorType == QgsShapeburstFillSymbolLayer::SimpleTwoColor )
  {
    mTwoColorGradientRamp = new QgsGradientColorRamp( color1, color2 );
  }

  //no border for shapeburst fills
  p->setPen( QPen( Qt::NoPen ) );

  //calculate margin size in pixels so that QImage of polygon has sufficient space to draw the full blur effect
  int sideBuffer = 4 + ( blurRadius + 2 ) * 4;
  //create a QImage to draw shapeburst in
  double imWidth = points.boundingRect().width() + ( sideBuffer * 2 );
  double imHeight = points.boundingRect().height() + ( sideBuffer * 2 );
  QImage * fillImage = new QImage( imWidth * context.renderContext().rasterScaleFactor(),
                                   imHeight * context.renderContext().rasterScaleFactor(), QImage::Format_ARGB32_Premultiplied );
  //Fill this image with black. Initially the distance transform is drawn in greyscale, where black pixels have zero distance from the
  //polygon boundary. Since we don't care about pixels which fall outside the polygon, we start with a black image and then draw over it the
  //polygon in white. The distance transform function then fills in the correct distance values for the white pixels.
  fillImage->fill( Qt::black );

  //also create an image to store the alpha channel
  QImage * alphaImage = new QImage( fillImage->width(), fillImage->height(), QImage::Format_ARGB32_Premultiplied );
  //initially fill the alpha channel image with a transparent color
  alphaImage->fill( Qt::transparent );

  //now, draw the polygon in the alpha channel image
  QPainter imgPainter;
  imgPainter.begin( alphaImage );
  imgPainter.setRenderHint( QPainter::Antialiasing, true );
  imgPainter.setBrush( QBrush( Qt::white ) );
  imgPainter.setPen( QPen( Qt::black ) );
  imgPainter.translate( -points.boundingRect().left() + sideBuffer, - points.boundingRect().top() + sideBuffer );
  imgPainter.scale( context.renderContext().rasterScaleFactor(), context.renderContext().rasterScaleFactor() );
  _renderPolygon( &imgPainter, points, rings, context );
  imgPainter.end();

  //now that we have a render of the polygon in white, draw this onto the shapeburst fill image too
  //(this avoids calling _renderPolygon twice, since that can be slow)
  imgPainter.begin( fillImage );
  if ( !ignoreRings )
  {
    imgPainter.drawImage( 0, 0, *alphaImage );
  }
  else
  {
    //using ignore rings mode, so the alpha image can't be used
    //directly as the alpha channel contains polygon rings and we need
    //to draw now without any rings
    imgPainter.setBrush( QBrush( Qt::white ) );
    imgPainter.setPen( QPen( Qt::black ) );
    imgPainter.translate( -points.boundingRect().left() + sideBuffer, - points.boundingRect().top() + sideBuffer );
    imgPainter.scale( context.renderContext().rasterScaleFactor(), context.renderContext().rasterScaleFactor() );
    _renderPolygon( &imgPainter, points, nullptr, context );
  }
  imgPainter.end();

  //apply distance transform to image, uses the current color ramp to calculate final pixel colors
  double * dtArray = distanceTransform( fillImage );

  //copy distance transform values back to QImage, shading by appropriate color ramp
  dtArrayToQImage( dtArray, fillImage, mColorType == QgsShapeburstFillSymbolLayer::SimpleTwoColor ? mTwoColorGradientRamp : mGradientRamp,
                   context.alpha(), useWholeShape, outputPixelMaxDist );

  //clean up some variables
  delete [] dtArray;
  if ( mColorType == QgsShapeburstFillSymbolLayer::SimpleTwoColor )
  {
    delete mTwoColorGradientRamp;
  }

  //apply blur if desired
  if ( blurRadius > 0 )
  {
    QgsSymbolLayerUtils::blurImageInPlace( *fillImage, QRect( 0, 0, fillImage->width(), fillImage->height() ), blurRadius, false );
  }

  //apply alpha channel to distance transform image, so that areas outside the polygon are transparent
  imgPainter.begin( fillImage );
  imgPainter.setCompositionMode( QPainter::CompositionMode_DestinationIn );
  imgPainter.drawImage( 0, 0, *alphaImage );
  imgPainter.end();
  //we're finished with the alpha channel image now
  delete alphaImage;

  //draw shapeburst image in correct place in the destination painter

  p->save();
  QPointF offset;
  if ( !mOffset.isNull() )
  {
    offset.setX( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.x(), mOffsetUnit, mOffsetMapUnitScale ) );
    offset.setY( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.y(), mOffsetUnit, mOffsetMapUnitScale ) );
    p->translate( offset );
  }

  p->scale( 1 / context.renderContext().rasterScaleFactor(), 1 / context.renderContext().rasterScaleFactor() );
  p->drawImage( points.boundingRect().left() - sideBuffer, points.boundingRect().top() - sideBuffer, *fillImage );

  delete fillImage;

  if ( !mOffset.isNull() )
  {
    p->translate( -offset );
  }
  p->restore();

}

//fast distance transform code, adapted from http://cs.brown.edu/~pff/dt/

/* distance transform of a 1d function using squared distance */
void QgsShapeburstFillSymbolLayer::distanceTransform1d( double *f, int n, int *v, double *z, double *d )
{
  int k = 0;
  v[0] = 0;
  z[0] = -INF;
  z[1] = + INF;
  for ( int q = 1; q <= n - 1; q++ )
  {
    double s  = (( f[q] + q * q ) - ( f[v[k]] + ( v[k] * v[k] ) ) ) / ( 2 * q - 2 * v[k] );
    while ( s <= z[k] )
    {
      k--;
      s  = (( f[q] + q * q ) - ( f[v[k]] + ( v[k] * v[k] ) ) ) / ( 2 * q - 2 * v[k] );
    }
    k++;
    v[k] = q;
    z[k] = s;
    z[k+1] = + INF;
  }

  k = 0;
  for ( int q = 0; q <= n - 1; q++ )
  {
    while ( z[k+1] < q )
      k++;
    d[q] = ( q - v[k] ) * ( q - v[k] ) + f[v[k]];
  }
}

/* distance transform of 2d function using squared distance */
void QgsShapeburstFillSymbolLayer::distanceTransform2d( double * im, int width, int height )
{
  int maxDimension = qMax( width, height );
  double *f = new double[ maxDimension ];
  int *v = new int[ maxDimension ];
  double *z = new double[ maxDimension + 1 ];
  double *d = new double[ maxDimension ];

  // transform along columns
  for ( int x = 0; x < width; x++ )
  {
    for ( int y = 0; y < height; y++ )
    {
      f[y] = im[ x + y * width ];
    }
    distanceTransform1d( f, height, v, z, d );
    for ( int y = 0; y < height; y++ )
    {
      im[ x + y * width ] = d[y];
    }
  }

  // transform along rows
  for ( int y = 0; y < height; y++ )
  {
    for ( int x = 0; x < width; x++ )
    {
      f[x] = im[  x + y*width ];
    }
    distanceTransform1d( f, width, v, z, d );
    for ( int x = 0; x < width; x++ )
    {
      im[  x + y*width ] = d[x];
    }
  }

  delete [] d;
  delete [] f;
  delete [] v;
  delete [] z;
}

/* distance transform of a binary QImage */
double * QgsShapeburstFillSymbolLayer::distanceTransform( QImage *im )
{
  int width = im->width();
  int height = im->height();

  double * dtArray = new double[width * height];

  //load qImage to array
  QRgb tmpRgb;
  int idx = 0;
  for ( int heightIndex = 0; heightIndex < height; ++heightIndex )
  {
    const QRgb* scanLine = reinterpret_cast< const QRgb* >( im->constScanLine( heightIndex ) );
    for ( int widthIndex = 0; widthIndex < width; ++widthIndex )
    {
      tmpRgb = scanLine[widthIndex];
      if ( qRed( tmpRgb ) == 0 )
      {
        //black pixel, so zero distance
        dtArray[ idx ] = 0;
      }
      else
      {
        //white pixel, so initially set distance as infinite
        dtArray[ idx ] = INF;
      }
      idx++;
    }
  }

  //calculate squared distance transform
  distanceTransform2d( dtArray, width, height );

  return dtArray;
}

void QgsShapeburstFillSymbolLayer::dtArrayToQImage( double * array, QImage *im, QgsColorRamp* ramp, double layerAlpha, bool useWholeShape, int maxPixelDistance )
{
  int width = im->width();
  int height = im->height();

  //find maximum distance value
  double maxDistanceValue;

  if ( useWholeShape )
  {
    //no max distance specified in symbol properties, so calculate from maximum value in distance transform results
    double dtMaxValue = array[0];
    for ( int i = 1; i < ( width * height ); ++i )
    {
      if ( array[i] > dtMaxValue )
      {
        dtMaxValue = array[i];
      }
    }

    //values in distance transform are squared
    maxDistanceValue = sqrt( dtMaxValue );
  }
  else
  {
    //use max distance set in symbol properties
    maxDistanceValue = maxPixelDistance;
  }

  //update the pixels in the provided QImage
  int idx = 0;
  double squaredVal = 0;
  double pixVal = 0;
  QColor pixColor;
  bool layerHasAlpha = layerAlpha < 1.0;

  for ( int heightIndex = 0; heightIndex < height; ++heightIndex )
  {
    QRgb* scanLine = reinterpret_cast< QRgb* >( im->scanLine( heightIndex ) );
    for ( int widthIndex = 0; widthIndex < width; ++widthIndex )
    {
      //result of distance transform
      squaredVal = array[idx];

      //scale result to fit in the range [0, 1]
      if ( maxDistanceValue > 0 )
      {
        pixVal = squaredVal > 0 ? qMin(( sqrt( squaredVal ) / maxDistanceValue ), 1.0 ) : 0;
      }
      else
      {
        pixVal = 1.0;
      }

      //convert value to color from ramp
      pixColor = ramp->color( pixVal );

      int pixAlpha = pixColor.alpha();
      if (( layerHasAlpha ) || ( pixAlpha != 255 ) )
      {
        //apply layer's transparency to alpha value
        double alpha = pixAlpha * layerAlpha;
        //premultiply ramp color since we are storing this in a ARGB32_Premultiplied QImage
        QgsSymbolLayerUtils::premultiplyColor( pixColor, alpha );
      }

      scanLine[widthIndex] = pixColor.rgba();
      idx++;
    }
  }
}

QgsStringMap QgsShapeburstFillSymbolLayer::properties() const
{
  QgsStringMap map;
  map["color"] = QgsSymbolLayerUtils::encodeColor( mColor );
  map["gradient_color2"] = QgsSymbolLayerUtils::encodeColor( mColor2 );
  map["color_type"] = QString::number( mColorType );
  map["blur_radius"] = QString::number( mBlurRadius );
  map["use_whole_shape"] = QString::number( mUseWholeShape );
  map["max_distance"] = QString::number( mMaxDistance );
  map["distance_unit"] = QgsUnitTypes::encodeUnit( mDistanceUnit );
  map["distance_map_unit_scale"] = QgsSymbolLayerUtils::encodeMapUnitScale( mDistanceMapUnitScale );
  map["ignore_rings"] = QString::number( mIgnoreRings );
  map["offset"] = QgsSymbolLayerUtils::encodePoint( mOffset );
  map["offset_unit"] = QgsUnitTypes::encodeUnit( mOffsetUnit );
  map["offset_map_unit_scale"] = QgsSymbolLayerUtils::encodeMapUnitScale( mOffsetMapUnitScale );

  saveDataDefinedProperties( map );

  if ( mGradientRamp )
  {
    map.unite( mGradientRamp->properties() );
  }

  return map;
}

QgsShapeburstFillSymbolLayer* QgsShapeburstFillSymbolLayer::clone() const
{
  QgsShapeburstFillSymbolLayer* sl = new QgsShapeburstFillSymbolLayer( mColor, mColor2, mColorType, mBlurRadius, mUseWholeShape, mMaxDistance );
  if ( mGradientRamp )
  {
    sl->setColorRamp( mGradientRamp->clone() );
  }
  sl->setDistanceUnit( mDistanceUnit );
  sl->setDistanceMapUnitScale( mDistanceMapUnitScale );
  sl->setIgnoreRings( mIgnoreRings );
  sl->setOffset( mOffset );
  sl->setOffsetUnit( mOffsetUnit );
  sl->setOffsetMapUnitScale( mOffsetMapUnitScale );
  copyDataDefinedProperties( sl );
  copyPaintEffect( sl );
  return sl;
}

double QgsShapeburstFillSymbolLayer::estimateMaxBleed() const
{
  double offsetBleed = qMax( mOffset.x(), mOffset.y() );
  return offsetBleed;
}

void QgsShapeburstFillSymbolLayer::setOutputUnit( QgsUnitTypes::RenderUnit unit )
{
  mDistanceUnit = unit;
  mOffsetUnit = unit;
}

QgsUnitTypes::RenderUnit QgsShapeburstFillSymbolLayer::outputUnit() const
{
  if ( mDistanceUnit == mOffsetUnit )
  {
    return mDistanceUnit;
  }
  return QgsUnitTypes::RenderUnknownUnit;
}

void QgsShapeburstFillSymbolLayer::setMapUnitScale( const QgsMapUnitScale &scale )
{
  mDistanceMapUnitScale = scale;
  mOffsetMapUnitScale = scale;
}

QgsMapUnitScale QgsShapeburstFillSymbolLayer::mapUnitScale() const
{
  if ( mDistanceMapUnitScale == mOffsetMapUnitScale )
  {
    return mDistanceMapUnitScale;
  }
  return QgsMapUnitScale();
}


//QgsImageFillSymbolLayer

QgsImageFillSymbolLayer::QgsImageFillSymbolLayer()
    : mNextAngle( 0.0 )
    , mOutlineWidth( 0.0 )
    , mOutlineWidthUnit( QgsUnitTypes::RenderMillimeters )
    , mOutline( nullptr )
{
  setSubSymbol( new QgsLineSymbol() );
}

QgsImageFillSymbolLayer::~QgsImageFillSymbolLayer()
{
}

void QgsImageFillSymbolLayer::renderPolygon( const QPolygonF& points, QList<QPolygonF>* rings, QgsSymbolRenderContext& context )
{
  QPainter* p = context.renderContext().painter();
  if ( !p )
  {
    return;
  }

  mNextAngle = mAngle;
  applyDataDefinedSettings( context );

  p->setPen( QPen( Qt::NoPen ) );

  QTransform bkTransform = mBrush.transform();
  if ( context.renderContext().testFlag( QgsRenderContext::RenderMapTile ) )
  {
    //transform brush to upper left corner of geometry bbox
    QPointF leftCorner = points.boundingRect().topLeft();
    QTransform t = mBrush.transform();
    t.translate( leftCorner.x(), leftCorner.y() );
    mBrush.setTransform( t );
  }

  if ( context.selected() )
  {
    QColor selColor = context.renderContext().selectionColor();
    // Alister - this doesn't seem to work here
    //if ( ! selectionIsOpaque )
    //  selColor.setAlphaF( context.alpha() );
    p->setBrush( QBrush( selColor ) );
    _renderPolygon( p, points, rings, context );
  }

  if ( !qgsDoubleNear( mNextAngle, 0.0 ) )
  {
    QTransform t = mBrush.transform();
    t.rotate( mNextAngle );
    mBrush.setTransform( t );
  }
  p->setBrush( mBrush );
  _renderPolygon( p, points, rings, context );
  if ( mOutline )
  {
    mOutline->renderPolyline( points, context.feature(), context.renderContext(), -1, selectFillBorder && context.selected() );
    if ( rings )
    {
      QList<QPolygonF>::const_iterator ringIt = rings->constBegin();
      for ( ; ringIt != rings->constEnd(); ++ringIt )
      {
        mOutline->renderPolyline( *ringIt, context.feature(), context.renderContext(), -1, selectFillBorder && context.selected() );
      }
    }
  }

  mBrush.setTransform( bkTransform );
}

bool QgsImageFillSymbolLayer::setSubSymbol( QgsSymbol* symbol )
{
  if ( !symbol ) //unset current outline
  {
    delete mOutline;
    mOutline = nullptr;
    return true;
  }

  if ( symbol->type() != QgsSymbol::Line )
  {
    delete symbol;
    return false;
  }

  QgsLineSymbol* lineSymbol = dynamic_cast<QgsLineSymbol*>( symbol );
  if ( lineSymbol )
  {
    delete mOutline;
    mOutline = lineSymbol;
    return true;
  }

  delete symbol;
  return false;
}

void QgsImageFillSymbolLayer::setOutputUnit( QgsUnitTypes::RenderUnit unit )
{
  mOutlineWidthUnit = unit;
}

QgsUnitTypes::RenderUnit QgsImageFillSymbolLayer::outputUnit() const
{
  return mOutlineWidthUnit;
}

void QgsImageFillSymbolLayer::setMapUnitScale( const QgsMapUnitScale& scale )
{
  mOutlineWidthMapUnitScale = scale;
}

QgsMapUnitScale QgsImageFillSymbolLayer::mapUnitScale() const
{
  return mOutlineWidthMapUnitScale;
}

double QgsImageFillSymbolLayer::estimateMaxBleed() const
{
  if ( mOutline && mOutline->symbolLayer( 0 ) )
  {
    double subLayerBleed = mOutline->symbolLayer( 0 )->estimateMaxBleed();
    return subLayerBleed;
  }
  return 0;
}

double QgsImageFillSymbolLayer::dxfWidth( const QgsDxfExport& e, QgsSymbolRenderContext &context ) const
{
  double width = mOutlineWidth;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH ) )
  {
    context.setOriginalValueVariable( mOutlineWidth );
    width = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH, context, mOutlineWidth ).toDouble();
  }
  return width * e.mapUnitScaleFactor( e.symbologyScaleDenominator(), mOutlineWidthUnit, e.mapUnits() );
}

QColor QgsImageFillSymbolLayer::dxfColor( QgsSymbolRenderContext &context ) const
{
  Q_UNUSED( context );
  if ( !mOutline )
  {
    return QColor( Qt::black );
  }
  return mOutline->color();
}

Qt::PenStyle QgsImageFillSymbolLayer::dxfPenStyle() const
{
  return Qt::SolidLine;
#if 0
  if ( !mOutline )
  {
    return Qt::SolidLine;
  }
  else
  {
    return mOutline->dxfPenStyle();
  }
#endif //0
}

QSet<QString> QgsImageFillSymbolLayer::usedAttributes() const
{
  QSet<QString> attr = QgsFillSymbolLayer::usedAttributes();
  if ( mOutline )
    attr.unite( mOutline->usedAttributes() );
  return attr;
}


//QgsSVGFillSymbolLayer

QgsSVGFillSymbolLayer::QgsSVGFillSymbolLayer( const QString& svgFilePath, double width, double angle )
    : QgsImageFillSymbolLayer()
    , mPatternWidth( width )
    , mPatternWidthUnit( QgsUnitTypes::RenderMillimeters )
    , mSvgOutlineWidthUnit( QgsUnitTypes::RenderMillimeters )
{
  setSvgFilePath( svgFilePath );
  mOutlineWidth = 0.3;
  mAngle = angle;
  mColor = QColor( 255, 255, 255 );
  mSvgOutlineColor = QColor( 0, 0, 0 );
  mSvgOutlineWidth = 0.2;
  setDefaultSvgParams();
  mSvgPattern = nullptr;
}

QgsSVGFillSymbolLayer::QgsSVGFillSymbolLayer( const QByteArray& svgData, double width, double angle )
    : QgsImageFillSymbolLayer()
    , mPatternWidth( width )
    , mPatternWidthUnit( QgsUnitTypes::RenderMillimeters )
    , mSvgData( svgData )
    , mSvgOutlineWidthUnit( QgsUnitTypes::RenderMillimeters )
{
  storeViewBox();
  mOutlineWidth = 0.3;
  mAngle = angle;
  mColor = QColor( 255, 255, 255 );
  mSvgOutlineColor = QColor( 0, 0, 0 );
  mSvgOutlineWidth = 0.2;
  setSubSymbol( new QgsLineSymbol() );
  setDefaultSvgParams();
  mSvgPattern = nullptr;
}

QgsSVGFillSymbolLayer::~QgsSVGFillSymbolLayer()
{
  delete mSvgPattern;
}

void QgsSVGFillSymbolLayer::setOutputUnit( QgsUnitTypes::RenderUnit unit )
{
  QgsImageFillSymbolLayer::setOutputUnit( unit );
  mPatternWidthUnit = unit;
  mSvgOutlineWidthUnit = unit;
  mOutlineWidthUnit = unit;
  mOutline->setOutputUnit( unit );
}

QgsUnitTypes::RenderUnit QgsSVGFillSymbolLayer::outputUnit() const
{
  QgsUnitTypes::RenderUnit unit = QgsImageFillSymbolLayer::outputUnit();
  if ( mPatternWidthUnit != unit || mSvgOutlineWidthUnit != unit || mOutlineWidthUnit != unit )
  {
    return QgsUnitTypes::RenderUnknownUnit;
  }
  return unit;
}

void QgsSVGFillSymbolLayer::setMapUnitScale( const QgsMapUnitScale &scale )
{
  QgsImageFillSymbolLayer::setMapUnitScale( scale );
  mPatternWidthMapUnitScale = scale;
  mSvgOutlineWidthMapUnitScale = scale;
  mOutlineWidthMapUnitScale = scale;
}

QgsMapUnitScale QgsSVGFillSymbolLayer::mapUnitScale() const
{
  if ( QgsImageFillSymbolLayer::mapUnitScale() == mPatternWidthMapUnitScale &&
       mPatternWidthMapUnitScale == mSvgOutlineWidthMapUnitScale &&
       mSvgOutlineWidthMapUnitScale == mOutlineWidthMapUnitScale )
  {
    return mPatternWidthMapUnitScale;
  }
  return QgsMapUnitScale();
}

void QgsSVGFillSymbolLayer::setSvgFilePath( const QString& svgPath )
{
  mSvgData = QgsSvgCache::instance()->getImageData( svgPath );
  storeViewBox();

  mSvgFilePath = svgPath;
  setDefaultSvgParams();
}

QgsSymbolLayer* QgsSVGFillSymbolLayer::create( const QgsStringMap& properties )
{
  QByteArray data;
  double width = 20;
  QString svgFilePath;
  double angle = 0.0;

  if ( properties.contains( "width" ) )
  {
    width = properties["width"].toDouble();
  }
  if ( properties.contains( "svgFile" ) )
  {
    QString svgName = properties["svgFile"];
    QString savePath = QgsSymbolLayerUtils::symbolNameToPath( svgName );
    svgFilePath = ( savePath.isEmpty() ? svgName : savePath );
  }
  if ( properties.contains( "angle" ) )
  {
    angle = properties["angle"].toDouble();
  }

  QgsSVGFillSymbolLayer* symbolLayer = nullptr;
  if ( !svgFilePath.isEmpty() )
  {
    symbolLayer = new QgsSVGFillSymbolLayer( svgFilePath, width, angle );
  }
  else
  {
    if ( properties.contains( "data" ) )
    {
      data = QByteArray::fromHex( properties["data"].toLocal8Bit() );
    }
    symbolLayer = new QgsSVGFillSymbolLayer( data, width, angle );
  }

  //svg parameters
  if ( properties.contains( "svgFillColor" ) )
  {
    //pre 2.5 projects used "svgFillColor"
    symbolLayer->setSvgFillColor( QgsSymbolLayerUtils::decodeColor( properties["svgFillColor"] ) );
  }
  else if ( properties.contains( "color" ) )
  {
    symbolLayer->setSvgFillColor( QgsSymbolLayerUtils::decodeColor( properties["color"] ) );
  }
  if ( properties.contains( "svgOutlineColor" ) )
  {
    //pre 2.5 projects used "svgOutlineColor"
    symbolLayer->setSvgOutlineColor( QgsSymbolLayerUtils::decodeColor( properties["svgOutlineColor"] ) );
  }
  else if ( properties.contains( "outline_color" ) )
  {
    symbolLayer->setSvgOutlineColor( QgsSymbolLayerUtils::decodeColor( properties["outline_color"] ) );
  }
  else if ( properties.contains( "line_color" ) )
  {
    symbolLayer->setSvgOutlineColor( QgsSymbolLayerUtils::decodeColor( properties["line_color"] ) );
  }
  if ( properties.contains( "svgOutlineWidth" ) )
  {
    //pre 2.5 projects used "svgOutlineWidth"
    symbolLayer->setSvgOutlineWidth( properties["svgOutlineWidth"].toDouble() );
  }
  else if ( properties.contains( "outline_width" ) )
  {
    symbolLayer->setSvgOutlineWidth( properties["outline_width"].toDouble() );
  }
  else if ( properties.contains( "line_width" ) )
  {
    symbolLayer->setSvgOutlineWidth( properties["line_width"].toDouble() );
  }

  //units
  if ( properties.contains( "pattern_width_unit" ) )
  {
    symbolLayer->setPatternWidthUnit( QgsUnitTypes::decodeRenderUnit( properties["pattern_width_unit"] ) );
  }
  if ( properties.contains( "pattern_width_map_unit_scale" ) )
  {
    symbolLayer->setPatternWidthMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["pattern_width_map_unit_scale"] ) );
  }
  if ( properties.contains( "svg_outline_width_unit" ) )
  {
    symbolLayer->setSvgOutlineWidthUnit( QgsUnitTypes::decodeRenderUnit( properties["svg_outline_width_unit"] ) );
  }
  if ( properties.contains( "svg_outline_width_map_unit_scale" ) )
  {
    symbolLayer->setSvgOutlineWidthMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["svg_outline_width_map_unit_scale"] ) );
  }
  if ( properties.contains( "outline_width_unit" ) )
  {
    symbolLayer->setOutlineWidthUnit( QgsUnitTypes::decodeRenderUnit( properties["outline_width_unit"] ) );
  }
  if ( properties.contains( "outline_width_map_unit_scale" ) )
  {
    symbolLayer->setOutlineWidthMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["outline_width_map_unit_scale"] ) );
  }

  symbolLayer->restoreDataDefinedProperties( properties );

  return symbolLayer;
}

QString QgsSVGFillSymbolLayer::layerType() const
{
  return "SVGFill";
}

void QgsSVGFillSymbolLayer::applyPattern( QBrush& brush, const QString& svgFilePath, double patternWidth, QgsUnitTypes::RenderUnit patternWidthUnit,
    const QColor& svgFillColor, const QColor& svgOutlineColor, double svgOutlineWidth,
    QgsUnitTypes::RenderUnit svgOutlineWidthUnit, const QgsSymbolRenderContext& context,
    const QgsMapUnitScale& patternWidthMapUnitScale, const QgsMapUnitScale& svgOutlineWidthMapUnitScale )
{
  if ( mSvgViewBox.isNull() )
  {
    return;
  }

  delete mSvgPattern;
  mSvgPattern = nullptr;
  double size = patternWidth * QgsSymbolLayerUtils::pixelSizeScaleFactor( context.renderContext(), patternWidthUnit, patternWidthMapUnitScale );

  if ( static_cast< int >( size ) < 1.0 || 10000.0 < size )
  {
    mSvgPattern = new QImage();
    brush.setTextureImage( *mSvgPattern );
  }
  else
  {
    bool fitsInCache = true;
    double outlineWidth = QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), svgOutlineWidth, svgOutlineWidthUnit, svgOutlineWidthMapUnitScale );
    const QImage& patternImage = QgsSvgCache::instance()->svgAsImage( svgFilePath, size, svgFillColor, svgOutlineColor, outlineWidth,
                                 context.renderContext().scaleFactor(), context.renderContext().rasterScaleFactor(), fitsInCache );
    if ( !fitsInCache )
    {
      const QPicture& patternPict = QgsSvgCache::instance()->svgAsPicture( svgFilePath, size, svgFillColor, svgOutlineColor, outlineWidth,
                                    context.renderContext().scaleFactor(), 1.0 );
      double hwRatio = 1.0;
      if ( patternPict.width() > 0 )
      {
        hwRatio = static_cast< double >( patternPict.height() ) / static_cast< double >( patternPict.width() );
      }
      mSvgPattern = new QImage( static_cast< int >( size ), static_cast< int >( size * hwRatio ), QImage::Format_ARGB32_Premultiplied );
      mSvgPattern->fill( 0 ); // transparent background

      QPainter p( mSvgPattern );
      p.drawPicture( QPointF( size / 2, size * hwRatio / 2 ), patternPict );
    }

    QTransform brushTransform;
    brushTransform.scale( 1.0 / context.renderContext().rasterScaleFactor(), 1.0 / context.renderContext().rasterScaleFactor() );
    if ( !qgsDoubleNear( context.alpha(), 1.0 ) )
    {
      QImage transparentImage = fitsInCache ? patternImage.copy() : mSvgPattern->copy();
      QgsSymbolLayerUtils::multiplyImageOpacity( &transparentImage, context.alpha() );
      brush.setTextureImage( transparentImage );
    }
    else
    {
      brush.setTextureImage( fitsInCache ? patternImage : *mSvgPattern );
    }
    brush.setTransform( brushTransform );
  }
}

void QgsSVGFillSymbolLayer::startRender( QgsSymbolRenderContext& context )
{

  applyPattern( mBrush, mSvgFilePath, mPatternWidth, mPatternWidthUnit, mColor, mSvgOutlineColor, mSvgOutlineWidth, mSvgOutlineWidthUnit, context, mPatternWidthMapUnitScale, mSvgOutlineWidthMapUnitScale );

  if ( mOutline )
  {
    mOutline->startRender( context.renderContext(), context.fields() );
  }

  prepareExpressions( context );
}

void QgsSVGFillSymbolLayer::stopRender( QgsSymbolRenderContext& context )
{
  if ( mOutline )
  {
    mOutline->stopRender( context.renderContext() );
  }
}

QgsStringMap QgsSVGFillSymbolLayer::properties() const
{
  QgsStringMap map;
  if ( !mSvgFilePath.isEmpty() )
  {
    map.insert( "svgFile", QgsSymbolLayerUtils::symbolPathToName( mSvgFilePath ) );
  }
  else
  {
    map.insert( "data", QString( mSvgData.toHex() ) );
  }

  map.insert( "width", QString::number( mPatternWidth ) );
  map.insert( "angle", QString::number( mAngle ) );

  //svg parameters
  map.insert( "color", QgsSymbolLayerUtils::encodeColor( mColor ) );
  map.insert( "outline_color", QgsSymbolLayerUtils::encodeColor( mSvgOutlineColor ) );
  map.insert( "outline_width", QString::number( mSvgOutlineWidth ) );

  //units
  map.insert( "pattern_width_unit", QgsUnitTypes::encodeUnit( mPatternWidthUnit ) );
  map.insert( "pattern_width_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mPatternWidthMapUnitScale ) );
  map.insert( "svg_outline_width_unit", QgsUnitTypes::encodeUnit( mSvgOutlineWidthUnit ) );
  map.insert( "svg_outline_width_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mSvgOutlineWidthMapUnitScale ) );
  map.insert( "outline_width_unit", QgsUnitTypes::encodeUnit( mOutlineWidthUnit ) );
  map.insert( "outline_width_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mOutlineWidthMapUnitScale ) );

  saveDataDefinedProperties( map );
  return map;
}

QgsSVGFillSymbolLayer* QgsSVGFillSymbolLayer::clone() const
{
  QgsSVGFillSymbolLayer* clonedLayer = nullptr;
  if ( !mSvgFilePath.isEmpty() )
  {
    clonedLayer = new QgsSVGFillSymbolLayer( mSvgFilePath, mPatternWidth, mAngle );
    clonedLayer->setSvgFillColor( mColor );
    clonedLayer->setSvgOutlineColor( mSvgOutlineColor );
    clonedLayer->setSvgOutlineWidth( mSvgOutlineWidth );
  }
  else
  {
    clonedLayer = new QgsSVGFillSymbolLayer( mSvgData, mPatternWidth, mAngle );
  }

  clonedLayer->setPatternWidthUnit( mPatternWidthUnit );
  clonedLayer->setPatternWidthMapUnitScale( mPatternWidthMapUnitScale );
  clonedLayer->setSvgOutlineWidthUnit( mSvgOutlineWidthUnit );
  clonedLayer->setSvgOutlineWidthMapUnitScale( mSvgOutlineWidthMapUnitScale );
  clonedLayer->setOutlineWidthUnit( mOutlineWidthUnit );
  clonedLayer->setOutlineWidthMapUnitScale( mOutlineWidthMapUnitScale );

  if ( mOutline )
  {
    clonedLayer->setSubSymbol( mOutline->clone() );
  }
  copyDataDefinedProperties( clonedLayer );
  copyPaintEffect( clonedLayer );
  return clonedLayer;
}

void QgsSVGFillSymbolLayer::toSld( QDomDocument &doc, QDomElement &element, const QgsStringMap& props ) const
{
  QDomElement symbolizerElem = doc.createElement( "se:PolygonSymbolizer" );
  if ( !props.value( "uom", "" ).isEmpty() )
    symbolizerElem.setAttribute( "uom", props.value( "uom", "" ) );
  element.appendChild( symbolizerElem );

  QgsSymbolLayerUtils::createGeometryElement( doc, symbolizerElem, props.value( "geom", "" ) );

  QDomElement fillElem = doc.createElement( "se:Fill" );
  symbolizerElem.appendChild( fillElem );

  QDomElement graphicFillElem = doc.createElement( "se:GraphicFill" );
  fillElem.appendChild( graphicFillElem );

  QDomElement graphicElem = doc.createElement( "se:Graphic" );
  graphicFillElem.appendChild( graphicElem );

  if ( !mSvgFilePath.isEmpty() )
  {
    double partternWidth = QgsSymbolLayerUtils::rescaleUom( mPatternWidth, mPatternWidthUnit, props );
    QgsSymbolLayerUtils::externalGraphicToSld( doc, graphicElem, mSvgFilePath, "image/svg+xml", mColor, partternWidth );
  }
  else
  {
    // TODO: create svg from data
    // <se:InlineContent>
    symbolizerElem.appendChild( doc.createComment( "SVG from data not implemented yet" ) );
  }

  if ( mSvgOutlineColor.isValid() || mSvgOutlineWidth >= 0 )
  {
    double svgOutlineWidth = QgsSymbolLayerUtils::rescaleUom( mSvgOutlineWidth, mSvgOutlineWidthUnit, props );
    QgsSymbolLayerUtils::lineToSld( doc, graphicElem, Qt::SolidLine, mSvgOutlineColor, svgOutlineWidth );
  }

  // <Rotation>
  QString angleFunc;
  bool ok;
  double angle = props.value( "angle", "0" ).toDouble( &ok );
  if ( !ok )
  {
    angleFunc = QString( "%1 + %2" ).arg( props.value( "angle", "0" ) ).arg( mAngle );
  }
  else if ( !qgsDoubleNear( angle + mAngle, 0.0 ) )
  {
    angleFunc = QString::number( angle + mAngle );
  }
  QgsSymbolLayerUtils::createRotationElement( doc, graphicElem, angleFunc );

  if ( mOutline )
  {
    // the outline sub symbol should be stored within the Stroke element,
    // but it will be stored in a separated LineSymbolizer because it could
    // have more than one layer
    mOutline->toSld( doc, element, props );
  }
}

QgsSymbolLayer* QgsSVGFillSymbolLayer::createFromSld( QDomElement &element )
{
  QgsDebugMsg( "Entered." );

  QString path, mimeType;
  QColor fillColor, borderColor;
  Qt::PenStyle penStyle;
  double size, borderWidth;

  QDomElement fillElem = element.firstChildElement( "Fill" );
  if ( fillElem.isNull() )
    return nullptr;

  QDomElement graphicFillElem = fillElem.firstChildElement( "GraphicFill" );
  if ( graphicFillElem.isNull() )
    return nullptr;

  QDomElement graphicElem = graphicFillElem.firstChildElement( "Graphic" );
  if ( graphicElem.isNull() )
    return nullptr;

  if ( !QgsSymbolLayerUtils::externalGraphicFromSld( graphicElem, path, mimeType, fillColor, size ) )
    return nullptr;

  if ( mimeType != "image/svg+xml" )
    return nullptr;

  QgsSymbolLayerUtils::lineFromSld( graphicElem, penStyle, borderColor, borderWidth );

  double angle = 0.0;
  QString angleFunc;
  if ( QgsSymbolLayerUtils::rotationFromSldElement( graphicElem, angleFunc ) )
  {
    bool ok;
    double d = angleFunc.toDouble( &ok );
    if ( ok )
      angle = d;
  }

  QgsSVGFillSymbolLayer* sl = new QgsSVGFillSymbolLayer( path, size, angle );
  sl->setSvgFillColor( fillColor );
  sl->setSvgOutlineColor( borderColor );
  sl->setSvgOutlineWidth( borderWidth );

  // try to get the outline
  QDomElement strokeElem = element.firstChildElement( "Stroke" );
  if ( !strokeElem.isNull() )
  {
    QgsSymbolLayer *l = QgsSymbolLayerUtils::createLineLayerFromSld( strokeElem );
    if ( l )
    {
      QgsSymbolLayerList layers;
      layers.append( l );
      sl->setSubSymbol( new QgsLineSymbol( layers ) );
    }
  }

  return sl;
}

void QgsSVGFillSymbolLayer::applyDataDefinedSettings( QgsSymbolRenderContext &context )
{
  if ( !hasDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH ) && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_FILE )
       && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_FILL_COLOR ) && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_OUTLINE_COLOR )
       && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_OUTLINE_WIDTH ) && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE ) )
  {
    return; //no data defined settings
  }

  bool ok;

  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE ) )
  {
    context.setOriginalValueVariable( mAngle );
    double nextAngle = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE, context, QVariant(), &ok ).toDouble();
    if ( ok )
      mNextAngle = nextAngle;
  }

  double width = mPatternWidth;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH ) )
  {
    context.setOriginalValueVariable( mPatternWidth );
    width = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH, context, mPatternWidth ).toDouble();
  }
  QString svgFile = mSvgFilePath;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_FILE ) )
  {
    context.setOriginalValueVariable( mSvgFilePath );
    svgFile = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_FILE, context, mSvgFilePath ).toString();
  }
  QColor svgFillColor = mColor;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_FILL_COLOR ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mColor ) );
    QString colorString = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_FILL_COLOR, context, QVariant(), &ok ).toString();
    if ( ok )
      svgFillColor = QgsSymbolLayerUtils::decodeColor( colorString );
  }
  QColor svgOutlineColor = mSvgOutlineColor;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_OUTLINE_COLOR ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mSvgOutlineColor ) );
    QString colorString = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_OUTLINE_COLOR, context, QVariant(), &ok ).toString();
    if ( ok )
      svgOutlineColor = QgsSymbolLayerUtils::decodeColor( colorString );
  }
  double outlineWidth = mSvgOutlineWidth;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_OUTLINE_WIDTH ) )
  {
    context.setOriginalValueVariable( mSvgOutlineWidth );
    outlineWidth = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_SVG_OUTLINE_WIDTH, context, mSvgOutlineWidth ).toDouble();
  }
  applyPattern( mBrush, svgFile, width, mPatternWidthUnit, svgFillColor, svgOutlineColor, outlineWidth,
                mSvgOutlineWidthUnit, context, mPatternWidthMapUnitScale, mSvgOutlineWidthMapUnitScale );

}

void QgsSVGFillSymbolLayer::storeViewBox()
{
  if ( !mSvgData.isEmpty() )
  {
    QSvgRenderer r( mSvgData );
    if ( r.isValid() )
    {
      mSvgViewBox = r.viewBoxF();
      return;
    }
  }

  mSvgViewBox = QRectF();
  return;
}

void QgsSVGFillSymbolLayer::setDefaultSvgParams()
{
  if ( mSvgFilePath.isEmpty() )
  {
    return;
  }

  bool hasFillParam, hasFillOpacityParam, hasOutlineParam, hasOutlineWidthParam, hasOutlineOpacityParam;
  bool hasDefaultFillColor, hasDefaultFillOpacity, hasDefaultOutlineColor, hasDefaultOutlineWidth, hasDefaultOutlineOpacity;
  QColor defaultFillColor, defaultOutlineColor;
  double defaultOutlineWidth, defaultFillOpacity, defaultOutlineOpacity;
  QgsSvgCache::instance()->containsParams( mSvgFilePath, hasFillParam, hasDefaultFillColor, defaultFillColor,
      hasFillOpacityParam, hasDefaultFillOpacity, defaultFillOpacity,
      hasOutlineParam, hasDefaultOutlineColor, defaultOutlineColor,
      hasOutlineWidthParam, hasDefaultOutlineWidth, defaultOutlineWidth,
      hasOutlineOpacityParam, hasDefaultOutlineOpacity, defaultOutlineOpacity );

  double newFillOpacity = hasFillOpacityParam ? mColor.alphaF() : 1.0;
  double newOutlineOpacity = hasOutlineOpacityParam ? mSvgOutlineColor.alphaF() : 1.0;

  if ( hasDefaultFillColor )
  {
    mColor = defaultFillColor;
    mColor.setAlphaF( newFillOpacity );
  }
  if ( hasDefaultFillOpacity )
  {
    mColor.setAlphaF( defaultFillOpacity );
  }
  if ( hasDefaultOutlineColor )
  {
    mSvgOutlineColor = defaultOutlineColor;
    mSvgOutlineColor.setAlphaF( newOutlineOpacity );
  }
  if ( hasDefaultOutlineOpacity )
  {
    mSvgOutlineColor.setAlphaF( defaultOutlineOpacity );
  }
  if ( hasDefaultOutlineWidth )
  {
    mSvgOutlineWidth = defaultOutlineWidth;
  }
}


QgsLinePatternFillSymbolLayer::QgsLinePatternFillSymbolLayer()
    : QgsImageFillSymbolLayer()
    , mDistance( 5.0 )
    , mDistanceUnit( QgsUnitTypes::RenderMillimeters )
    , mLineWidth( 0 )
    , mLineWidthUnit( QgsUnitTypes::RenderMillimeters )
    , mLineAngle( 45.0 )
    , mOffset( 0.0 )
    , mOffsetUnit( QgsUnitTypes::RenderMillimeters )
    , mFillLineSymbol( nullptr )
{
  setSubSymbol( new QgsLineSymbol() );
  QgsImageFillSymbolLayer::setSubSymbol( nullptr ); //no outline
}

void QgsLinePatternFillSymbolLayer::setLineWidth( double w )
{
  mFillLineSymbol->setWidth( w );
  mLineWidth = w;
}

void QgsLinePatternFillSymbolLayer::setColor( const QColor& c )
{
  mFillLineSymbol->setColor( c );
  mColor = c;
}

QColor QgsLinePatternFillSymbolLayer::color() const
{
  return mFillLineSymbol ? mFillLineSymbol->color() : mColor;
}

QgsLinePatternFillSymbolLayer::~QgsLinePatternFillSymbolLayer()
{
  delete mFillLineSymbol;
}

bool QgsLinePatternFillSymbolLayer::setSubSymbol( QgsSymbol* symbol )
{
  if ( !symbol )
  {
    return false;
  }

  if ( symbol->type() == QgsSymbol::Line )
  {
    QgsLineSymbol* lineSymbol = dynamic_cast<QgsLineSymbol*>( symbol );
    if ( lineSymbol )
    {
      delete mFillLineSymbol;
      mFillLineSymbol = lineSymbol;

      return true;
    }
  }
  delete symbol;
  return false;
}

QgsSymbol* QgsLinePatternFillSymbolLayer::subSymbol()
{
  return mFillLineSymbol;
}

QSet<QString> QgsLinePatternFillSymbolLayer::usedAttributes() const
{
  QSet<QString> attr = QgsImageFillSymbolLayer::usedAttributes();
  if ( mFillLineSymbol )
    attr.unite( mFillLineSymbol->usedAttributes() );
  return attr;
}

double QgsLinePatternFillSymbolLayer::estimateMaxBleed() const
{
  return 0;
}

void QgsLinePatternFillSymbolLayer::setOutputUnit( QgsUnitTypes::RenderUnit unit )
{
  QgsImageFillSymbolLayer::setOutputUnit( unit );
  mDistanceUnit = unit;
  mLineWidthUnit = unit;
  mOffsetUnit = unit;
}

QgsUnitTypes::RenderUnit QgsLinePatternFillSymbolLayer::outputUnit() const
{
  QgsUnitTypes::RenderUnit unit = QgsImageFillSymbolLayer::outputUnit();
  if ( mDistanceUnit != unit || mLineWidthUnit != unit || mOffsetUnit != unit )
  {
    return QgsUnitTypes::RenderUnknownUnit;
  }
  return unit;
}

void QgsLinePatternFillSymbolLayer::setMapUnitScale( const QgsMapUnitScale &scale )
{
  QgsImageFillSymbolLayer::setMapUnitScale( scale );
  mDistanceMapUnitScale = scale;
  mLineWidthMapUnitScale = scale;
  mOffsetMapUnitScale = scale;
}

QgsMapUnitScale QgsLinePatternFillSymbolLayer::mapUnitScale() const
{
  if ( QgsImageFillSymbolLayer::mapUnitScale() == mDistanceMapUnitScale &&
       mDistanceMapUnitScale == mLineWidthMapUnitScale &&
       mLineWidthMapUnitScale == mOffsetMapUnitScale )
  {
    return mDistanceMapUnitScale;
  }
  return QgsMapUnitScale();
}

QgsSymbolLayer* QgsLinePatternFillSymbolLayer::create( const QgsStringMap& properties )
{
  QgsLinePatternFillSymbolLayer* patternLayer = new QgsLinePatternFillSymbolLayer();

  //default values
  double lineAngle = 45;
  double distance = 5;
  double lineWidth = 0.5;
  QColor color( Qt::black );
  double offset = 0.0;

  if ( properties.contains( "lineangle" ) )
  {
    //pre 2.5 projects used "lineangle"
    lineAngle = properties["lineangle"].toDouble();
  }
  else if ( properties.contains( "angle" ) )
  {
    lineAngle = properties["angle"].toDouble();
  }
  patternLayer->setLineAngle( lineAngle );

  if ( properties.contains( "distance" ) )
  {
    distance = properties["distance"].toDouble();
  }
  patternLayer->setDistance( distance );

  if ( properties.contains( "linewidth" ) )
  {
    //pre 2.5 projects used "linewidth"
    lineWidth = properties["linewidth"].toDouble();
  }
  else if ( properties.contains( "outline_width" ) )
  {
    lineWidth = properties["outline_width"].toDouble();
  }
  else if ( properties.contains( "line_width" ) )
  {
    lineWidth = properties["line_width"].toDouble();
  }
  patternLayer->setLineWidth( lineWidth );

  if ( properties.contains( "color" ) )
  {
    color = QgsSymbolLayerUtils::decodeColor( properties["color"] );
  }
  else if ( properties.contains( "outline_color" ) )
  {
    color = QgsSymbolLayerUtils::decodeColor( properties["outline_color"] );
  }
  else if ( properties.contains( "line_color" ) )
  {
    color = QgsSymbolLayerUtils::decodeColor( properties["line_color"] );
  }
  patternLayer->setColor( color );

  if ( properties.contains( "offset" ) )
  {
    offset = properties["offset"].toDouble();
  }
  patternLayer->setOffset( offset );


  if ( properties.contains( "distance_unit" ) )
  {
    patternLayer->setDistanceUnit( QgsUnitTypes::decodeRenderUnit( properties["distance_unit"] ) );
  }
  if ( properties.contains( "distance_map_unit_scale" ) )
  {
    patternLayer->setDistanceMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["distance_map_unit_scale"] ) );
  }
  if ( properties.contains( "line_width_unit" ) )
  {
    patternLayer->setLineWidthUnit( QgsUnitTypes::decodeRenderUnit( properties["line_width_unit"] ) );
  }
  else if ( properties.contains( "outline_width_unit" ) )
  {
    patternLayer->setLineWidthUnit( QgsUnitTypes::decodeRenderUnit( properties["outline_width_unit"] ) );
  }
  if ( properties.contains( "line_width_map_unit_scale" ) )
  {
    patternLayer->setLineWidthMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["line_width_map_unit_scale"] ) );
  }
  if ( properties.contains( "offset_unit" ) )
  {
    patternLayer->setOffsetUnit( QgsUnitTypes::decodeRenderUnit( properties["offset_unit"] ) );
  }
  if ( properties.contains( "offset_map_unit_scale" ) )
  {
    patternLayer->setOffsetMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["offset_map_unit_scale"] ) );
  }
  if ( properties.contains( "outline_width_unit" ) )
  {
    patternLayer->setOutlineWidthUnit( QgsUnitTypes::decodeRenderUnit( properties["outline_width_unit"] ) );
  }
  if ( properties.contains( "outline_width_map_unit_scale" ) )
  {
    patternLayer->setOutlineWidthMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["outline_width_map_unit_scale"] ) );
  }

  patternLayer->restoreDataDefinedProperties( properties );

  return patternLayer;
}

QString QgsLinePatternFillSymbolLayer::layerType() const
{
  return "LinePatternFill";
}

void QgsLinePatternFillSymbolLayer::applyPattern( const QgsSymbolRenderContext& context, QBrush& brush, double lineAngle, double distance,
    double lineWidth, const QColor& color )
{
  Q_UNUSED( lineWidth );
  Q_UNUSED( color );

  mBrush.setTextureImage( QImage() ); // set empty in case we have to return

  if ( !mFillLineSymbol )
  {
    return;
  }
  // We have to make a copy because marker intervals will have to be adjusted
  QgsLineSymbol* fillLineSymbol = mFillLineSymbol->clone();
  if ( !fillLineSymbol )
  {
    return;
  }

  const QgsRenderContext& ctx = context.renderContext();
  //double outlinePixelWidth = lineWidth * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx,  mLineWidthUnit, mLineWidthMapUnitScale );
  double outputPixelDist = distance * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx, mDistanceUnit, mDistanceMapUnitScale );
  double outputPixelOffset = mOffset * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx,  mOffsetUnit, mOffsetMapUnitScale );

  // To get all patterns into image, we have to consider symbols size (estimateMaxBleed()).
  // For marker lines we have to get markers interval.
  double outputPixelBleed = 0;
  double outputPixelInterval = 0; // maximum interval
  for ( int i = 0; i < fillLineSymbol->symbolLayerCount(); i++ )
  {
    QgsSymbolLayer *layer = fillLineSymbol->symbolLayer( i );
    double layerBleed = layer->estimateMaxBleed();
    // TODO: to get real bleed we have to scale it using context and units,
    // unfortunately estimateMaxBleed() ignore units completely, e.g.
    // QgsMarkerLineSymbolLayer::estimateMaxBleed() is mixing marker size and
    // offset regardless units. This has to be fixed especially
    // in estimateMaxBleed(), context probably has to be used.
    // For now, we only support millimeters
    double outputPixelLayerBleed = layerBleed * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx, QgsUnitTypes::RenderMillimeters );
    outputPixelBleed = qMax( outputPixelBleed, outputPixelLayerBleed );

    QgsMarkerLineSymbolLayer *markerLineLayer = dynamic_cast<QgsMarkerLineSymbolLayer *>( layer );
    if ( markerLineLayer )
    {
      double outputPixelLayerInterval = markerLineLayer->interval() * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx, markerLineLayer->intervalUnit(), markerLineLayer->intervalMapUnitScale() );

      // There may be multiple marker lines with different intervals.
      // In theory we should find the least common multiple, but that could be too
      // big (multiplication of intervals in the worst case).
      // Because patterns without small common interval would look strange, we
      // believe that the longest interval should usually be sufficient.
      outputPixelInterval = qMax( outputPixelInterval, outputPixelLayerInterval );
    }
  }

  if ( outputPixelInterval > 0 )
  {
    // We have to adjust marker intervals to integer pixel size to get
    // repeatable pattern.
    double intervalScale = qRound( outputPixelInterval ) / outputPixelInterval;
    outputPixelInterval = qRound( outputPixelInterval );

    for ( int i = 0; i < fillLineSymbol->symbolLayerCount(); i++ )
    {
      QgsSymbolLayer *layer = fillLineSymbol->symbolLayer( i );

      QgsMarkerLineSymbolLayer *markerLineLayer = dynamic_cast<QgsMarkerLineSymbolLayer *>( layer );
      if ( markerLineLayer )
      {
        markerLineLayer->setInterval( intervalScale * markerLineLayer->interval() );
      }
    }
  }

  //create image
  int height, width;
  if ( qgsDoubleNear( lineAngle, 0 ) || qgsDoubleNear( lineAngle, 360 ) || qgsDoubleNear( lineAngle, 180 ) )
  {
    height = outputPixelDist;
    width = outputPixelInterval > 0 ? outputPixelInterval : height;
  }
  else if ( qgsDoubleNear( lineAngle, 90 ) || qgsDoubleNear( lineAngle, 270 ) )
  {
    width = outputPixelDist;
    height = outputPixelInterval > 0 ? outputPixelInterval : width;
  }
  else
  {
    height = outputPixelDist / cos( lineAngle * M_PI / 180 ); //keep perpendicular distance between lines constant
    width = outputPixelDist / sin( lineAngle * M_PI / 180 );

    // recalculate real angle and distance after rounding to pixels
    lineAngle = 180 * atan2( static_cast< double >( height ), static_cast< double >( width ) ) / M_PI;
    if ( lineAngle < 0 )
    {
      lineAngle += 360.;
    }

    height = qAbs( height );
    width = qAbs( width );

    outputPixelDist = height * cos( lineAngle * M_PI / 180 );

    // Round offset to correspond to one pixel height, otherwise lines may
    // be shifted on tile border if offset falls close to pixel center
    int offsetHeight = qRound( qAbs( outputPixelOffset / cos( lineAngle * M_PI / 180 ) ) );
    outputPixelOffset = offsetHeight * cos( lineAngle * M_PI / 180 );
  }

  //depending on the angle, we might need to render into a larger image and use a subset of it
  double dx = 0;
  double dy = 0;

  // Add buffer based on bleed but keep precisely the height/width ratio (angle)
  // thus we add integer multiplications of width and height covering the bleed
  int bufferMulti = qMax( qCeil( outputPixelBleed / width ), qCeil( outputPixelBleed / width ) );

  // Always buffer at least once so that center of line marker in upper right corner
  // does not fall outside due to representation error
  bufferMulti = qMax( bufferMulti, 1 );

  int xBuffer = width * bufferMulti;
  int yBuffer = height * bufferMulti;
  int innerWidth = width;
  int innerHeight = height;
  width += 2 * xBuffer;
  height += 2 * yBuffer;

  if ( width > 10000 || height > 10000 ) //protect symbol layer from eating too much memory
  {
    return;
  }

  QImage patternImage( width, height, QImage::Format_ARGB32 );
  patternImage.fill( 0 );

  QPointF p1, p2, p3, p4, p5, p6;
  if ( qgsDoubleNear( lineAngle, 0.0 ) || qgsDoubleNear( lineAngle, 360.0 ) || qgsDoubleNear( lineAngle, 180.0 ) )
  {
    p1 = QPointF( 0, yBuffer );
    p2 = QPointF( width, yBuffer );
    p3 = QPointF( 0, yBuffer + innerHeight );
    p4 = QPointF( width, yBuffer + innerHeight );
  }
  else if ( qgsDoubleNear( lineAngle, 90.0 ) || qgsDoubleNear( lineAngle, 270.0 ) )
  {
    p1 = QPointF( xBuffer, height );
    p2 = QPointF( xBuffer, 0 );
    p3 = QPointF( xBuffer + innerWidth, height );
    p4 = QPointF( xBuffer + innerWidth, 0 );
  }
  else if ( lineAngle > 0 && lineAngle < 90 )
  {
    dx = outputPixelDist * cos(( 90 - lineAngle ) * M_PI / 180.0 );
    dy = outputPixelDist * sin(( 90 - lineAngle ) * M_PI / 180.0 );
    p1 = QPointF( 0, height );
    p2 = QPointF( width, 0 );
    p3 = QPointF( -dx, height - dy );
    p4 = QPointF( width - dx, -dy );
    p5 = QPointF( dx, height + dy );
    p6 = QPointF( width + dx, dy );
  }
  else if ( lineAngle > 180 && lineAngle < 270 )
  {
    dx = outputPixelDist * cos(( 90 - lineAngle ) * M_PI / 180.0 );
    dy = outputPixelDist * sin(( 90 - lineAngle ) * M_PI / 180.0 );
    p1 = QPointF( width, 0 );
    p2 = QPointF( 0, height );
    p3 = QPointF( width - dx, -dy );
    p4 = QPointF( -dx, height - dy );
    p5 = QPointF( width + dx, dy );
    p6 = QPointF( dx, height + dy );
  }
  else if ( lineAngle > 90 && lineAngle < 180 )
  {
    dy = outputPixelDist * cos(( 180 - lineAngle ) * M_PI / 180 );
    dx = outputPixelDist * sin(( 180 - lineAngle ) * M_PI / 180 );
    p1 = QPointF( 0, 0 );
    p2 = QPointF( width, height );
    p5 = QPointF( dx, -dy );
    p6 = QPointF( width + dx, height - dy );
    p3 = QPointF( -dx, dy );
    p4 = QPointF( width - dx, height + dy );
  }
  else if ( lineAngle > 270 && lineAngle < 360 )
  {
    dy = outputPixelDist * cos(( 180 - lineAngle ) * M_PI / 180 );
    dx = outputPixelDist * sin(( 180 - lineAngle ) * M_PI / 180 );
    p1 = QPointF( width, height );
    p2 = QPointF( 0, 0 );
    p5 = QPointF( width + dx, height - dy );
    p6 = QPointF( dx, -dy );
    p3 = QPointF( width - dx, height + dy );
    p4 = QPointF( -dx, dy );
  }

  if ( !qgsDoubleNear( mOffset, 0.0 ) ) //shift everything
  {
    QPointF tempPt;
    tempPt = QgsSymbolLayerUtils::pointOnLineWithDistance( p1, p3, outputPixelDist + outputPixelOffset );
    p3 = QPointF( tempPt.x(), tempPt.y() );
    tempPt = QgsSymbolLayerUtils::pointOnLineWithDistance( p2, p4, outputPixelDist + outputPixelOffset );
    p4 = QPointF( tempPt.x(), tempPt.y() );
    tempPt = QgsSymbolLayerUtils::pointOnLineWithDistance( p1, p5, outputPixelDist - outputPixelOffset );
    p5 = QPointF( tempPt.x(), tempPt.y() );
    tempPt = QgsSymbolLayerUtils::pointOnLineWithDistance( p2, p6, outputPixelDist - outputPixelOffset );
    p6 = QPointF( tempPt.x(), tempPt.y() );

    //update p1, p2 last
    tempPt = QgsSymbolLayerUtils::pointOnLineWithDistance( p1, p3, outputPixelOffset );
    p1 = QPointF( tempPt.x(), tempPt.y() );
    tempPt = QgsSymbolLayerUtils::pointOnLineWithDistance( p2, p4, outputPixelOffset );
    p2 = QPointF( tempPt.x(), tempPt.y() );
  }

  QPainter p( &patternImage );

#if 0
  // DEBUG: Draw rectangle
  p.setRenderHint( QPainter::Antialiasing, false ); // get true rect
  QPen pen( QColor( Qt::black ) );
  pen.setWidthF( 0.1 );
  pen.setCapStyle( Qt::FlatCap );
  p.setPen( pen );

  // To see this rectangle, comment buffer cut below.
  // Subtract 1 because not antialiased are rendered to the right/down by 1 pixel
  QPolygon polygon = QPolygon() << QPoint( 0, 0 ) << QPoint( width - 1, 0 ) << QPoint( width - 1, height - 1 ) << QPoint( 0, height - 1 ) << QPoint( 0, 0 );
  p.drawPolygon( polygon );

  polygon = QPolygon() << QPoint( xBuffer, yBuffer ) << QPoint( width - xBuffer - 1, yBuffer ) << QPoint( width - xBuffer - 1, height - yBuffer - 1 ) << QPoint( xBuffer, height - yBuffer - 1 ) << QPoint( xBuffer, yBuffer );
  p.drawPolygon( polygon );
#endif

  // Use antialiasing because without antialiasing lines are rendered to the
  // right and below the mathematically defined points (not symmetrical)
  // and such tiles become useless for are filling
  p.setRenderHint( QPainter::Antialiasing, true );

  // line rendering needs context for drawing on patternImage
  QgsRenderContext lineRenderContext;
  lineRenderContext.setPainter( &p );
  lineRenderContext.setRasterScaleFactor( 1.0 );
  lineRenderContext.setScaleFactor( context.renderContext().scaleFactor() * context.renderContext().rasterScaleFactor() );
  QgsMapToPixel mtp( context.renderContext().mapToPixel().mapUnitsPerPixel() / context.renderContext().rasterScaleFactor() );
  lineRenderContext.setMapToPixel( mtp );
  lineRenderContext.setForceVectorOutput( false );
  lineRenderContext.setExpressionContext( context.renderContext().expressionContext() );

  fillLineSymbol->startRender( lineRenderContext, context.fields() );

  QVector<QPolygonF> polygons;
  polygons.append( QPolygonF() << p1 << p2 );
  polygons.append( QPolygonF() << p3 << p4 );
  if ( !qgsDoubleNear( lineAngle, 0 ) && !qgsDoubleNear( lineAngle, 360 ) && !qgsDoubleNear( lineAngle, 90 ) && !qgsDoubleNear( lineAngle, 180 ) && !qgsDoubleNear( lineAngle, 270 ) )
  {
    polygons.append( QPolygonF() << p5 << p6 );
  }

  Q_FOREACH ( const QPolygonF& polygon, polygons )
  {
    fillLineSymbol->renderPolyline( polygon, context.feature(), lineRenderContext, -1, context.selected() );
  }

  fillLineSymbol->stopRender( lineRenderContext );
  p.end();

  // Cut off the buffer
  patternImage = patternImage.copy( xBuffer, yBuffer, patternImage.width() - 2 * xBuffer, patternImage.height() - 2 * yBuffer );

  //set image to mBrush
  if ( !qgsDoubleNear( context.alpha(), 1.0 ) )
  {
    QImage transparentImage = patternImage.copy();
    QgsSymbolLayerUtils::multiplyImageOpacity( &transparentImage, context.alpha() );
    brush.setTextureImage( transparentImage );
  }
  else
  {
    brush.setTextureImage( patternImage );
  }

  QTransform brushTransform;
  brushTransform.scale( 1.0 / context.renderContext().rasterScaleFactor(), 1.0 / context.renderContext().rasterScaleFactor() );
  brush.setTransform( brushTransform );

  delete fillLineSymbol;
}

void QgsLinePatternFillSymbolLayer::startRender( QgsSymbolRenderContext& context )
{
  applyPattern( context, mBrush, mLineAngle, mDistance, mLineWidth, mColor );

  if ( mFillLineSymbol )
  {
    mFillLineSymbol->startRender( context.renderContext(), context.fields() );
  }

  prepareExpressions( context );
}

void QgsLinePatternFillSymbolLayer::stopRender( QgsSymbolRenderContext & )
{
}

QgsStringMap QgsLinePatternFillSymbolLayer::properties() const
{
  QgsStringMap map;
  map.insert( "angle", QString::number( mLineAngle ) );
  map.insert( "distance", QString::number( mDistance ) );
  map.insert( "line_width", QString::number( mLineWidth ) );
  map.insert( "color", QgsSymbolLayerUtils::encodeColor( mColor ) );
  map.insert( "offset", QString::number( mOffset ) );
  map.insert( "distance_unit", QgsUnitTypes::encodeUnit( mDistanceUnit ) );
  map.insert( "line_width_unit", QgsUnitTypes::encodeUnit( mLineWidthUnit ) );
  map.insert( "offset_unit", QgsUnitTypes::encodeUnit( mOffsetUnit ) );
  map.insert( "distance_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mDistanceMapUnitScale ) );
  map.insert( "line_width_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mLineWidthMapUnitScale ) );
  map.insert( "offset_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mOffsetMapUnitScale ) );
  map.insert( "outline_width_unit", QgsUnitTypes::encodeUnit( mOutlineWidthUnit ) );
  map.insert( "outline_width_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mOutlineWidthMapUnitScale ) );
  saveDataDefinedProperties( map );
  return map;
}

QgsLinePatternFillSymbolLayer* QgsLinePatternFillSymbolLayer::clone() const
{
  QgsLinePatternFillSymbolLayer* clonedLayer = static_cast<QgsLinePatternFillSymbolLayer*>( QgsLinePatternFillSymbolLayer::create( properties() ) );
  if ( mFillLineSymbol )
  {
    clonedLayer->setSubSymbol( mFillLineSymbol->clone() );
  }
  copyPaintEffect( clonedLayer );
  return clonedLayer;
}

void QgsLinePatternFillSymbolLayer::toSld( QDomDocument &doc, QDomElement &element, const QgsStringMap& props ) const
{
  QDomElement symbolizerElem = doc.createElement( "se:PolygonSymbolizer" );
  if ( !props.value( "uom", "" ).isEmpty() )
    symbolizerElem.setAttribute( "uom", props.value( "uom", "" ) );
  element.appendChild( symbolizerElem );

  // <Geometry>
  QgsSymbolLayerUtils::createGeometryElement( doc, symbolizerElem, props.value( "geom", "" ) );

  QDomElement fillElem = doc.createElement( "se:Fill" );
  symbolizerElem.appendChild( fillElem );

  QDomElement graphicFillElem = doc.createElement( "se:GraphicFill" );
  fillElem.appendChild( graphicFillElem );

  QDomElement graphicElem = doc.createElement( "se:Graphic" );
  graphicFillElem.appendChild( graphicElem );

  //line properties must be inside the graphic definition
  QColor lineColor = mFillLineSymbol ? mFillLineSymbol->color() : QColor();
  double lineWidth = mFillLineSymbol ? mFillLineSymbol->width() : 0.0;
  lineWidth = QgsSymbolLayerUtils::rescaleUom( lineWidth, mLineWidthUnit,  props );
  double distance = QgsSymbolLayerUtils::rescaleUom( mDistance, mDistanceUnit,  props );
  QgsSymbolLayerUtils::wellKnownMarkerToSld( doc, graphicElem, "horline", QColor(), lineColor, Qt::SolidLine, lineWidth, distance );

  // <Rotation>
  QString angleFunc;
  bool ok;
  double angle = props.value( "angle", "0" ).toDouble( &ok );
  if ( !ok )
  {
    angleFunc = QString( "%1 + %2" ).arg( props.value( "angle", "0" ) ).arg( mLineAngle );
  }
  else if ( !qgsDoubleNear( angle + mLineAngle, 0.0 ) )
  {
    angleFunc = QString::number( angle + mLineAngle );
  }
  QgsSymbolLayerUtils::createRotationElement( doc, graphicElem, angleFunc );

  // <se:Displacement>
  QPointF lineOffset( sin( mLineAngle ) * mOffset, cos( mLineAngle ) * mOffset );
  lineOffset = QgsSymbolLayerUtils::rescaleUom( lineOffset, mOffsetUnit, props );
  QgsSymbolLayerUtils::createDisplacementElement( doc, graphicElem, lineOffset );
}

QString QgsLinePatternFillSymbolLayer::ogrFeatureStyleWidth( double widthScaleFactor ) const
{
  QString featureStyle;
  featureStyle.append( "Brush(" );
  featureStyle.append( QString( "fc:%1" ).arg( mColor.name() ) );
  featureStyle.append( QString( ",bc:%1" ).arg( "#00000000" ) ); //transparent background
  featureStyle.append( ",id:\"ogr-brush-2\"" );
  featureStyle.append( QString( ",a:%1" ).arg( mLineAngle ) );
  featureStyle.append( QString( ",s:%1" ).arg( mLineWidth * widthScaleFactor ) );
  featureStyle.append( ",dx:0mm" );
  featureStyle.append( QString( ",dy:%1mm" ).arg( mDistance * widthScaleFactor ) );
  featureStyle.append( ')' );
  return featureStyle;
}

void QgsLinePatternFillSymbolLayer::applyDataDefinedSettings( QgsSymbolRenderContext &context )
{
  if ( !hasDataDefinedProperty( QgsSymbolLayer::EXPR_LINEANGLE ) && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE )
       && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_LINEWIDTH ) && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR )
       && ( !mFillLineSymbol || !mFillLineSymbol->hasDataDefinedProperties() ) )
  {
    return; //no data defined settings
  }

  bool ok;
  double lineAngle = mLineAngle;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_LINEANGLE ) )
  {
    context.setOriginalValueVariable( mLineAngle );
    lineAngle = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_LINEANGLE, context, mLineAngle ).toDouble();
  }
  double distance = mDistance;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE ) )
  {
    context.setOriginalValueVariable( mDistance );
    distance = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE, context, mDistance ).toDouble();
  }
  double lineWidth = mLineWidth;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_LINEWIDTH ) )
  {
    context.setOriginalValueVariable( mLineWidth );
    lineWidth = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_LINEWIDTH, context, mLineWidth ).toDouble();
  }
  QColor color = mColor;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR ) )
  {
    context.setOriginalValueVariable( QgsSymbolLayerUtils::encodeColor( mColor ) );
    QString colorString = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_COLOR, context, QVariant(), &ok ).toString();
    if ( ok )
      color = QgsSymbolLayerUtils::decodeColor( colorString );
  }
  applyPattern( context, mBrush, lineAngle, distance, lineWidth, color );
}

QgsSymbolLayer* QgsLinePatternFillSymbolLayer::createFromSld( QDomElement &element )
{
  QgsDebugMsg( "Entered." );

  QString name;
  QColor fillColor, lineColor;
  double size, lineWidth;
  Qt::PenStyle lineStyle;

  QDomElement fillElem = element.firstChildElement( "Fill" );
  if ( fillElem.isNull() )
    return nullptr;

  QDomElement graphicFillElem = fillElem.firstChildElement( "GraphicFill" );
  if ( graphicFillElem.isNull() )
    return nullptr;

  QDomElement graphicElem = graphicFillElem.firstChildElement( "Graphic" );
  if ( graphicElem.isNull() )
    return nullptr;

  if ( !QgsSymbolLayerUtils::wellKnownMarkerFromSld( graphicElem, name, fillColor, lineColor, lineStyle, lineWidth, size ) )
    return nullptr;

  if ( name != "horline" )
    return nullptr;

  double angle = 0.0;
  QString angleFunc;
  if ( QgsSymbolLayerUtils::rotationFromSldElement( graphicElem, angleFunc ) )
  {
    bool ok;
    double d = angleFunc.toDouble( &ok );
    if ( ok )
      angle = d;
  }

  double offset = 0.0;
  QPointF vectOffset;
  if ( QgsSymbolLayerUtils::displacementFromSldElement( graphicElem, vectOffset ) )
  {
    offset = sqrt( pow( vectOffset.x(), 2 ) + pow( vectOffset.y(), 2 ) );
  }

  QgsLinePatternFillSymbolLayer* sl = new QgsLinePatternFillSymbolLayer();
  sl->setColor( lineColor );
  sl->setLineWidth( lineWidth );
  sl->setLineAngle( angle );
  sl->setOffset( offset );
  sl->setDistance( size );

  // try to get the outline
  QDomElement strokeElem = element.firstChildElement( "Stroke" );
  if ( !strokeElem.isNull() )
  {
    QgsSymbolLayer *l = QgsSymbolLayerUtils::createLineLayerFromSld( strokeElem );
    if ( l )
    {
      QgsSymbolLayerList layers;
      layers.append( l );
      sl->setSubSymbol( new QgsLineSymbol( layers ) );
    }
  }

  return sl;
}


////////////////////////

QgsPointPatternFillSymbolLayer::QgsPointPatternFillSymbolLayer()
    : QgsImageFillSymbolLayer()
    , mMarkerSymbol( nullptr )
    , mDistanceX( 15 )
    , mDistanceXUnit( QgsUnitTypes::RenderMillimeters )
    , mDistanceY( 15 )
    , mDistanceYUnit( QgsUnitTypes::RenderMillimeters )
    , mDisplacementX( 0 )
    , mDisplacementXUnit( QgsUnitTypes::RenderMillimeters )
    , mDisplacementY( 0 )
    , mDisplacementYUnit( QgsUnitTypes::RenderMillimeters )
{
  mDistanceX = 15;
  mDistanceY = 15;
  mDisplacementX = 0;
  mDisplacementY = 0;
  setSubSymbol( new QgsMarkerSymbol() );
  QgsImageFillSymbolLayer::setSubSymbol( nullptr ); //no outline
}

QgsPointPatternFillSymbolLayer::~QgsPointPatternFillSymbolLayer()
{
  delete mMarkerSymbol;
}

void QgsPointPatternFillSymbolLayer::setOutputUnit( QgsUnitTypes::RenderUnit unit )
{
  QgsImageFillSymbolLayer::setOutputUnit( unit );
  mDistanceXUnit = unit;
  mDistanceYUnit = unit;
  mDisplacementXUnit = unit;
  mDisplacementYUnit = unit;
  if ( mMarkerSymbol )
  {
    mMarkerSymbol->setOutputUnit( unit );
  }

}

QgsUnitTypes::RenderUnit QgsPointPatternFillSymbolLayer::outputUnit() const
{
  QgsUnitTypes::RenderUnit unit = QgsImageFillSymbolLayer::outputUnit();
  if ( mDistanceXUnit != unit || mDistanceYUnit != unit || mDisplacementXUnit != unit || mDisplacementYUnit != unit )
  {
    return QgsUnitTypes::RenderUnknownUnit;
  }
  return unit;
}

void QgsPointPatternFillSymbolLayer::setMapUnitScale( const QgsMapUnitScale &scale )
{
  QgsImageFillSymbolLayer::setMapUnitScale( scale );
  mDistanceXMapUnitScale = scale;
  mDistanceYMapUnitScale = scale;
  mDisplacementXMapUnitScale = scale;
  mDisplacementYMapUnitScale = scale;
}

QgsMapUnitScale QgsPointPatternFillSymbolLayer::mapUnitScale() const
{
  if ( QgsImageFillSymbolLayer::mapUnitScale() == mDistanceXMapUnitScale &&
       mDistanceXMapUnitScale == mDistanceYMapUnitScale &&
       mDistanceYMapUnitScale == mDisplacementXMapUnitScale &&
       mDisplacementXMapUnitScale == mDisplacementYMapUnitScale )
  {
    return mDistanceXMapUnitScale;
  }
  return QgsMapUnitScale();
}

QgsSymbolLayer* QgsPointPatternFillSymbolLayer::create( const QgsStringMap& properties )
{
  QgsPointPatternFillSymbolLayer* layer = new QgsPointPatternFillSymbolLayer();
  if ( properties.contains( "distance_x" ) )
  {
    layer->setDistanceX( properties["distance_x"].toDouble() );
  }
  if ( properties.contains( "distance_y" ) )
  {
    layer->setDistanceY( properties["distance_y"].toDouble() );
  }
  if ( properties.contains( "displacement_x" ) )
  {
    layer->setDisplacementX( properties["displacement_x"].toDouble() );
  }
  if ( properties.contains( "displacement_y" ) )
  {
    layer->setDisplacementY( properties["displacement_y"].toDouble() );
  }

  if ( properties.contains( "distance_x_unit" ) )
  {
    layer->setDistanceXUnit( QgsUnitTypes::decodeRenderUnit( properties["distance_x_unit"] ) );
  }
  if ( properties.contains( "distance_x_map_unit_scale" ) )
  {
    layer->setDistanceXMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["distance_x_map_unit_scale"] ) );
  }
  if ( properties.contains( "distance_y_unit" ) )
  {
    layer->setDistanceYUnit( QgsUnitTypes::decodeRenderUnit( properties["distance_y_unit"] ) );
  }
  if ( properties.contains( "distance_y_map_unit_scale" ) )
  {
    layer->setDistanceYMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["distance_y_map_unit_scale"] ) );
  }
  if ( properties.contains( "displacement_x_unit" ) )
  {
    layer->setDisplacementXUnit( QgsUnitTypes::decodeRenderUnit( properties["displacement_x_unit"] ) );
  }
  if ( properties.contains( "displacement_x_map_unit_scale" ) )
  {
    layer->setDisplacementXMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["displacement_x_map_unit_scale"] ) );
  }
  if ( properties.contains( "displacement_y_unit" ) )
  {
    layer->setDisplacementYUnit( QgsUnitTypes::decodeRenderUnit( properties["displacement_y_unit"] ) );
  }
  if ( properties.contains( "displacement_y_map_unit_scale" ) )
  {
    layer->setDisplacementYMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["displacement_y_map_unit_scale"] ) );
  }
  if ( properties.contains( "outline_width_unit" ) )
  {
    layer->setOutlineWidthUnit( QgsUnitTypes::decodeRenderUnit( properties["outline_width_unit"] ) );
  }
  if ( properties.contains( "outline_width_map_unit_scale" ) )
  {
    layer->setOutlineWidthMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["outline_width_map_unit_scale"] ) );
  }

  layer->restoreDataDefinedProperties( properties );

  return layer;
}

QString QgsPointPatternFillSymbolLayer::layerType() const
{
  return "PointPatternFill";
}

void QgsPointPatternFillSymbolLayer::applyPattern( const QgsSymbolRenderContext& context, QBrush& brush, double distanceX, double distanceY,
    double displacementX, double displacementY )
{
  //render 3 rows and columns in one go to easily incorporate displacement
  const QgsRenderContext& ctx = context.renderContext();
  double width = distanceX * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx, mDistanceXUnit, mDistanceXMapUnitScale ) * 2.0;
  double height = distanceY * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx, mDistanceYUnit, mDisplacementYMapUnitScale ) * 2.0;

  if ( width > 10000 || height > 10000 ) //protect symbol layer from eating too much memory
  {
    QImage img;
    brush.setTextureImage( img );
    return;
  }

  QImage patternImage( width, height, QImage::Format_ARGB32 );
  patternImage.fill( 0 );

  if ( mMarkerSymbol )
  {
    QPainter p( &patternImage );

    //marker rendering needs context for drawing on patternImage
    QgsRenderContext pointRenderContext;
    pointRenderContext.setRendererScale( context.renderContext().rendererScale() );
    pointRenderContext.setPainter( &p );
    pointRenderContext.setRasterScaleFactor( 1.0 );
    pointRenderContext.setScaleFactor( context.renderContext().scaleFactor() * context.renderContext().rasterScaleFactor() );
    QgsMapToPixel mtp( context.renderContext().mapToPixel().mapUnitsPerPixel() / context.renderContext().rasterScaleFactor() );
    pointRenderContext.setMapToPixel( mtp );
    pointRenderContext.setForceVectorOutput( false );
    pointRenderContext.setExpressionContext( context.renderContext().expressionContext() );

    mMarkerSymbol->startRender( pointRenderContext, context.fields() );

    //render corner points
    mMarkerSymbol->renderPoint( QPointF( 0, 0 ), context.feature(), pointRenderContext );
    mMarkerSymbol->renderPoint( QPointF( width, 0 ), context.feature(), pointRenderContext );
    mMarkerSymbol->renderPoint( QPointF( 0, height ), context.feature(), pointRenderContext );
    mMarkerSymbol->renderPoint( QPointF( width, height ), context.feature(), pointRenderContext );

    //render displaced points
    double displacementPixelX = displacementX * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx, mDisplacementXUnit, mDisplacementXMapUnitScale );
    double displacementPixelY = displacementY * QgsSymbolLayerUtils::pixelSizeScaleFactor( ctx, mDisplacementYUnit, mDisplacementYMapUnitScale );
    mMarkerSymbol->renderPoint( QPointF( width / 2.0, -displacementPixelY ), context.feature(), pointRenderContext );
    mMarkerSymbol->renderPoint( QPointF( displacementPixelX, height / 2.0 ), context.feature(), pointRenderContext );
    mMarkerSymbol->renderPoint( QPointF( width / 2.0 + displacementPixelX, height / 2.0 - displacementPixelY ), context.feature(), pointRenderContext );
    mMarkerSymbol->renderPoint( QPointF( width + displacementPixelX, height / 2.0 ), context.feature(), pointRenderContext );
    mMarkerSymbol->renderPoint( QPointF( width / 2.0, height - displacementPixelY ), context.feature(), pointRenderContext );

    mMarkerSymbol->stopRender( pointRenderContext );
  }

  if ( !qgsDoubleNear( context.alpha(), 1.0 ) )
  {
    QImage transparentImage = patternImage.copy();
    QgsSymbolLayerUtils::multiplyImageOpacity( &transparentImage, context.alpha() );
    brush.setTextureImage( transparentImage );
  }
  else
  {
    brush.setTextureImage( patternImage );
  }
  QTransform brushTransform;
  brushTransform.scale( 1.0 / context.renderContext().rasterScaleFactor(), 1.0 / context.renderContext().rasterScaleFactor() );
  brush.setTransform( brushTransform );
}

void QgsPointPatternFillSymbolLayer::startRender( QgsSymbolRenderContext& context )
{
  applyPattern( context, mBrush, mDistanceX, mDistanceY, mDisplacementX, mDisplacementY );

  if ( mOutline )
  {
    mOutline->startRender( context.renderContext(), context.fields() );
  }
  prepareExpressions( context );
}

void QgsPointPatternFillSymbolLayer::stopRender( QgsSymbolRenderContext& context )
{
  if ( mOutline )
  {
    mOutline->stopRender( context.renderContext() );
  }
}

QgsStringMap QgsPointPatternFillSymbolLayer::properties() const
{
  QgsStringMap map;
  map.insert( "distance_x", QString::number( mDistanceX ) );
  map.insert( "distance_y", QString::number( mDistanceY ) );
  map.insert( "displacement_x", QString::number( mDisplacementX ) );
  map.insert( "displacement_y", QString::number( mDisplacementY ) );
  map.insert( "distance_x_unit", QgsUnitTypes::encodeUnit( mDistanceXUnit ) );
  map.insert( "distance_y_unit", QgsUnitTypes::encodeUnit( mDistanceYUnit ) );
  map.insert( "displacement_x_unit", QgsUnitTypes::encodeUnit( mDisplacementXUnit ) );
  map.insert( "displacement_y_unit", QgsUnitTypes::encodeUnit( mDisplacementYUnit ) );
  map.insert( "distance_x_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mDistanceXMapUnitScale ) );
  map.insert( "distance_y_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mDistanceYMapUnitScale ) );
  map.insert( "displacement_x_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mDisplacementXMapUnitScale ) );
  map.insert( "displacement_y_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mDisplacementYMapUnitScale ) );
  map.insert( "outline_width_unit", QgsUnitTypes::encodeUnit( mOutlineWidthUnit ) );
  map.insert( "outline_width_map_unit_scale", QgsSymbolLayerUtils::encodeMapUnitScale( mOutlineWidthMapUnitScale ) );
  saveDataDefinedProperties( map );
  return map;
}

QgsPointPatternFillSymbolLayer* QgsPointPatternFillSymbolLayer::clone() const
{
  QgsPointPatternFillSymbolLayer* clonedLayer = static_cast<QgsPointPatternFillSymbolLayer*>( QgsPointPatternFillSymbolLayer::create( properties() ) );
  if ( mMarkerSymbol )
  {
    clonedLayer->setSubSymbol( mMarkerSymbol->clone() );
  }
  copyPaintEffect( clonedLayer );
  return clonedLayer;
}

void QgsPointPatternFillSymbolLayer::toSld( QDomDocument &doc, QDomElement &element, const QgsStringMap& props ) const
{
  for ( int i = 0; i < mMarkerSymbol->symbolLayerCount(); i++ )
  {
    QDomElement symbolizerElem = doc.createElement( "se:PolygonSymbolizer" );
    if ( !props.value( "uom", "" ).isEmpty() )
      symbolizerElem.setAttribute( "uom", props.value( "uom", "" ) );
    element.appendChild( symbolizerElem );

    // <Geometry>
    QgsSymbolLayerUtils::createGeometryElement( doc, symbolizerElem, props.value( "geom", "" ) );

    QDomElement fillElem = doc.createElement( "se:Fill" );
    symbolizerElem.appendChild( fillElem );

    QDomElement graphicFillElem = doc.createElement( "se:GraphicFill" );
    fillElem.appendChild( graphicFillElem );

    // store distanceX, distanceY, displacementX, displacementY in a <VendorOption>
    double dx  = QgsSymbolLayerUtils::rescaleUom( mDistanceX, mDistanceXUnit, props );
    double dy  = QgsSymbolLayerUtils::rescaleUom( mDistanceY, mDistanceYUnit, props );
    QString dist = QgsSymbolLayerUtils::encodePoint( QPointF( dx, dy ) );
    QDomElement distanceElem = QgsSymbolLayerUtils::createVendorOptionElement( doc, "distance", dist );
    symbolizerElem.appendChild( distanceElem );

    QgsSymbolLayer *layer = mMarkerSymbol->symbolLayer( i );
    QgsMarkerSymbolLayer *markerLayer = static_cast<QgsMarkerSymbolLayer *>( layer );
    if ( !markerLayer )
    {
      QString errorMsg = QString( "MarkerSymbolLayerV2 expected, %1 found. Skip it." ).arg( layer->layerType() );
      graphicFillElem.appendChild( doc.createComment( errorMsg ) );
    }
    else
    {
      markerLayer->writeSldMarker( doc, graphicFillElem, props );
    }
  }
}

QgsSymbolLayer* QgsPointPatternFillSymbolLayer::createFromSld( QDomElement &element )
{
  Q_UNUSED( element );
  return nullptr;
}

bool QgsPointPatternFillSymbolLayer::setSubSymbol( QgsSymbol* symbol )
{
  if ( !symbol )
  {
    return false;
  }

  if ( symbol->type() == QgsSymbol::Marker )
  {
    QgsMarkerSymbol* markerSymbol = static_cast<QgsMarkerSymbol*>( symbol );
    delete mMarkerSymbol;
    mMarkerSymbol = markerSymbol;
  }
  return true;
}

void QgsPointPatternFillSymbolLayer::applyDataDefinedSettings( QgsSymbolRenderContext &context )
{
  if ( !hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE_X ) && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE_Y )
       && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISPLACEMENT_X ) && !hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISPLACEMENT_Y )
       && ( !mMarkerSymbol || !mMarkerSymbol->hasDataDefinedProperties() ) )
  {
    return;
  }

  double distanceX = mDistanceX;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE_X ) )
  {
    context.setOriginalValueVariable( mDistanceX );
    distanceX = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE_X, context, mDistanceX ).toDouble();
  }
  double distanceY = mDistanceY;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE_Y ) )
  {
    context.setOriginalValueVariable( mDistanceY );
    distanceY = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_DISTANCE_Y, context, mDistanceY ).toDouble();
  }
  double displacementX = mDisplacementX;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISPLACEMENT_X ) )
  {
    context.setOriginalValueVariable( mDisplacementX );
    displacementX = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_DISPLACEMENT_X, context, mDisplacementX ).toDouble();
  }
  double displacementY = mDisplacementY;
  if ( hasDataDefinedProperty( QgsSymbolLayer::EXPR_DISPLACEMENT_Y ) )
  {
    context.setOriginalValueVariable( mDisplacementY );
    displacementY = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_DISPLACEMENT_Y, context, mDisplacementY ).toDouble();
  }
  applyPattern( context, mBrush, distanceX, distanceY, displacementX, displacementY );
}

double QgsPointPatternFillSymbolLayer::estimateMaxBleed() const
{
  return 0;
}

QSet<QString> QgsPointPatternFillSymbolLayer::usedAttributes() const
{
  QSet<QString> attributes = QgsImageFillSymbolLayer::usedAttributes();

  if ( mMarkerSymbol )
    attributes.unite( mMarkerSymbol->usedAttributes() );

  return attributes;
}

void QgsPointPatternFillSymbolLayer::setColor( const QColor& c )
{
  mColor = c;
  if ( mMarkerSymbol )
    mMarkerSymbol->setColor( c );
}

QColor QgsPointPatternFillSymbolLayer::color() const
{
  return mMarkerSymbol ? mMarkerSymbol->color() : mColor;
}

//////////////


QgsCentroidFillSymbolLayer::QgsCentroidFillSymbolLayer()
    : mMarker( nullptr )
    , mPointOnSurface( false )
    , mPointOnAllParts( true )
    , mCurrentFeatureId( -1 )
    , mBiggestPartIndex( -1 )
{
  setSubSymbol( new QgsMarkerSymbol() );
}

QgsCentroidFillSymbolLayer::~QgsCentroidFillSymbolLayer()
{
  delete mMarker;
}

QgsSymbolLayer* QgsCentroidFillSymbolLayer::create( const QgsStringMap& properties )
{
  QgsCentroidFillSymbolLayer* sl = new QgsCentroidFillSymbolLayer();

  if ( properties.contains( "point_on_surface" ) )
    sl->setPointOnSurface( properties["point_on_surface"].toInt() != 0 );
  if ( properties.contains( "point_on_all_parts" ) )
    sl->setPointOnAllParts( properties["point_on_all_parts"].toInt() != 0 );

  return sl;
}

QString QgsCentroidFillSymbolLayer::layerType() const
{
  return "CentroidFill";
}

void QgsCentroidFillSymbolLayer::setColor( const QColor& color )
{
  mMarker->setColor( color );
  mColor = color;
}

QColor QgsCentroidFillSymbolLayer::color() const
{
  return mMarker ? mMarker->color() : mColor;
}

void QgsCentroidFillSymbolLayer::startRender( QgsSymbolRenderContext& context )
{
  mMarker->setAlpha( context.alpha() );
  mMarker->startRender( context.renderContext(), context.fields() );

  mCurrentFeatureId = -1;
  mBiggestPartIndex = 0;
}

void QgsCentroidFillSymbolLayer::stopRender( QgsSymbolRenderContext& context )
{
  mMarker->stopRender( context.renderContext() );
}

void QgsCentroidFillSymbolLayer::renderPolygon( const QPolygonF& points, QList<QPolygonF>* rings, QgsSymbolRenderContext& context )
{
  Q_UNUSED( rings );

  if ( !mPointOnAllParts )
  {
    const QgsFeature* feature = context.feature();
    if ( feature )
    {
      if ( feature->id() != mCurrentFeatureId )
      {
        mCurrentFeatureId = feature->id();
        mBiggestPartIndex = 1;

        if ( context.geometryPartCount() > 1 )
        {
          QgsGeometry geom = feature->geometry();
          const QgsGeometryCollection* geomCollection = static_cast<const QgsGeometryCollection*>( geom.geometry() );

          double area = 0;
          double areaBiggest = 0;
          for ( int i = 0; i < context.geometryPartCount(); ++i )
          {
            area = geomCollection->geometryN( i )->area();
            if ( area > areaBiggest )
            {
              areaBiggest = area;
              mBiggestPartIndex = i + 1;
            }
          }
        }
      }
    }
  }

  QgsDebugMsg( QString( "num: %1, count: %2" ).arg( context.geometryPartNum() ).arg( context.geometryPartCount() ) );

  if ( mPointOnAllParts || ( context.geometryPartNum() == mBiggestPartIndex ) )
  {
    QPointF centroid = mPointOnSurface ? QgsSymbolLayerUtils::polygonPointOnSurface( points ) : QgsSymbolLayerUtils::polygonCentroid( points );
    mMarker->renderPoint( centroid, context.feature(), context.renderContext(), -1, context.selected() );
  }
}

QgsStringMap QgsCentroidFillSymbolLayer::properties() const
{
  QgsStringMap map;
  map["point_on_surface"] = QString::number( mPointOnSurface );
  map["point_on_all_parts"] = QString::number( mPointOnAllParts );
  return map;
}

QgsCentroidFillSymbolLayer* QgsCentroidFillSymbolLayer::clone() const
{
  QgsCentroidFillSymbolLayer* x = new QgsCentroidFillSymbolLayer();
  x->mAngle = mAngle;
  x->mColor = mColor;
  x->setSubSymbol( mMarker->clone() );
  x->setPointOnSurface( mPointOnSurface );
  x->setPointOnAllParts( mPointOnAllParts );
  copyPaintEffect( x );
  return x;
}

void QgsCentroidFillSymbolLayer::toSld( QDomDocument &doc, QDomElement &element, const QgsStringMap& props ) const
{
  // SLD 1.0 specs says: "if a line, polygon, or raster geometry is
  // used with PointSymbolizer, then the semantic is to use the centroid
  // of the geometry, or any similar representative point.
  mMarker->toSld( doc, element, props );
}

QgsSymbolLayer* QgsCentroidFillSymbolLayer::createFromSld( QDomElement &element )
{
  QgsDebugMsg( "Entered." );

  QgsSymbolLayer *l = QgsSymbolLayerUtils::createMarkerLayerFromSld( element );
  if ( !l )
    return nullptr;

  QgsSymbolLayerList layers;
  layers.append( l );
  QgsMarkerSymbol *marker = new QgsMarkerSymbol( layers );

  QgsCentroidFillSymbolLayer* sl = new QgsCentroidFillSymbolLayer();
  sl->setSubSymbol( marker );
  return sl;
}


QgsSymbol* QgsCentroidFillSymbolLayer::subSymbol()
{
  return mMarker;
}

bool QgsCentroidFillSymbolLayer::setSubSymbol( QgsSymbol* symbol )
{
  if ( !symbol || symbol->type() != QgsSymbol::Marker )
  {
    delete symbol;
    return false;
  }

  delete mMarker;
  mMarker = static_cast<QgsMarkerSymbol*>( symbol );
  mColor = mMarker->color();
  return true;
}

QSet<QString> QgsCentroidFillSymbolLayer::usedAttributes() const
{
  QSet<QString> attributes = QgsFillSymbolLayer::usedAttributes();

  if ( mMarker )
    attributes.unite( mMarker->usedAttributes() );

  return attributes;
}

void QgsCentroidFillSymbolLayer::setOutputUnit( QgsUnitTypes::RenderUnit unit )
{
  if ( mMarker )
  {
    mMarker->setOutputUnit( unit );
  }
}

QgsUnitTypes::RenderUnit QgsCentroidFillSymbolLayer::outputUnit() const
{
  if ( mMarker )
  {
    return mMarker->outputUnit();
  }
  return QgsUnitTypes::RenderUnknownUnit; //mOutputUnit;
}

void QgsCentroidFillSymbolLayer::setMapUnitScale( const QgsMapUnitScale &scale )
{
  if ( mMarker )
  {
    mMarker->setMapUnitScale( scale );
  }
}

QgsMapUnitScale QgsCentroidFillSymbolLayer::mapUnitScale() const
{
  if ( mMarker )
  {
    return mMarker->mapUnitScale();
  }
  return QgsMapUnitScale();
}




QgsRasterFillSymbolLayer::QgsRasterFillSymbolLayer( const QString &imageFilePath )
    : QgsImageFillSymbolLayer()
    , mImageFilePath( imageFilePath )
    , mCoordinateMode( QgsRasterFillSymbolLayer::Feature )
    , mAlpha( 1.0 )
    , mOffsetUnit( QgsUnitTypes::RenderMillimeters )
    , mWidth( 0.0 )
    , mWidthUnit( QgsUnitTypes::RenderPixels )
{
  QgsImageFillSymbolLayer::setSubSymbol( nullptr ); //disable sub symbol
}

QgsRasterFillSymbolLayer::~QgsRasterFillSymbolLayer()
{

}

QgsSymbolLayer *QgsRasterFillSymbolLayer::create( const QgsStringMap &properties )
{
  FillCoordinateMode mode = QgsRasterFillSymbolLayer::Feature;
  double alpha = 1.0;
  QPointF offset;
  double angle = 0.0;
  double width = 0.0;

  QString imagePath;
  if ( properties.contains( "imageFile" ) )
  {
    imagePath = properties["imageFile"];
  }
  if ( properties.contains( "coordinate_mode" ) )
  {
    mode = static_cast< FillCoordinateMode >( properties["coordinate_mode"].toInt() );
  }
  if ( properties.contains( "alpha" ) )
  {
    alpha = properties["alpha"].toDouble();
  }
  if ( properties.contains( "offset" ) )
  {
    offset = QgsSymbolLayerUtils::decodePoint( properties["offset"] );
  }
  if ( properties.contains( "angle" ) )
  {
    angle = properties["angle"].toDouble();
  }
  if ( properties.contains( "width" ) )
  {
    width = properties["width"].toDouble();
  }
  QgsRasterFillSymbolLayer* symbolLayer = new QgsRasterFillSymbolLayer( imagePath );
  symbolLayer->setCoordinateMode( mode );
  symbolLayer->setAlpha( alpha );
  symbolLayer->setOffset( offset );
  symbolLayer->setAngle( angle );
  symbolLayer->setWidth( width );
  if ( properties.contains( "offset_unit" ) )
  {
    symbolLayer->setOffsetUnit( QgsUnitTypes::decodeRenderUnit( properties["offset_unit"] ) );
  }
  if ( properties.contains( "offset_map_unit_scale" ) )
  {
    symbolLayer->setOffsetMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["offset_map_unit_scale"] ) );
  }
  if ( properties.contains( "width_unit" ) )
  {
    symbolLayer->setWidthUnit( QgsUnitTypes::decodeRenderUnit( properties["width_unit"] ) );
  }
  if ( properties.contains( "width_map_unit_scale" ) )
  {
    symbolLayer->setWidthMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( properties["width_map_unit_scale"] ) );
  }

  symbolLayer->restoreDataDefinedProperties( properties );

  return symbolLayer;
}

bool QgsRasterFillSymbolLayer::setSubSymbol( QgsSymbol *symbol )
{
  Q_UNUSED( symbol );
  return true;
}

QString QgsRasterFillSymbolLayer::layerType() const
{
  return "RasterFill";
}

void QgsRasterFillSymbolLayer::renderPolygon( const QPolygonF &points, QList<QPolygonF> *rings, QgsSymbolRenderContext &context )
{
  QPainter* p = context.renderContext().painter();
  if ( !p )
  {
    return;
  }

  QPointF offset;
  if ( !mOffset.isNull() )
  {
    offset.setX( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.x(), mOffsetUnit, mOffsetMapUnitScale ) );
    offset.setY( QgsSymbolLayerUtils::convertToPainterUnits( context.renderContext(), mOffset.y(), mOffsetUnit, mOffsetMapUnitScale ) );
    p->translate( offset );
  }
  if ( mCoordinateMode == Feature )
  {
    QRectF boundingRect = points.boundingRect();
    mBrush.setTransform( mBrush.transform().translate( boundingRect.left() - mBrush.transform().dx(),
                         boundingRect.top() - mBrush.transform().dy() ) );
  }

  QgsImageFillSymbolLayer::renderPolygon( points, rings, context );
  if ( !mOffset.isNull() )
  {
    p->translate( -offset );
  }
}

void QgsRasterFillSymbolLayer::startRender( QgsSymbolRenderContext &context )
{
  prepareExpressions( context );
  applyPattern( mBrush, mImageFilePath, mWidth, mAlpha, context );
}

void QgsRasterFillSymbolLayer::stopRender( QgsSymbolRenderContext &context )
{
  Q_UNUSED( context );
}

QgsStringMap QgsRasterFillSymbolLayer::properties() const
{
  QgsStringMap map;
  map["imageFile"] = mImageFilePath;
  map["coordinate_mode"] = QString::number( mCoordinateMode );
  map["alpha"] = QString::number( mAlpha );
  map["offset"] = QgsSymbolLayerUtils::encodePoint( mOffset );
  map["offset_unit"] = QgsUnitTypes::encodeUnit( mOffsetUnit );
  map["offset_map_unit_scale"] = QgsSymbolLayerUtils::encodeMapUnitScale( mOffsetMapUnitScale );
  map["angle"] = QString::number( mAngle );
  map["width"] = QString::number( mWidth );
  map["width_unit"] = QgsUnitTypes::encodeUnit( mWidthUnit );
  map["width_map_unit_scale"] = QgsSymbolLayerUtils::encodeMapUnitScale( mWidthMapUnitScale );

  saveDataDefinedProperties( map );
  return map;
}

QgsRasterFillSymbolLayer* QgsRasterFillSymbolLayer::clone() const
{
  QgsRasterFillSymbolLayer* sl = new QgsRasterFillSymbolLayer( mImageFilePath );
  sl->setCoordinateMode( mCoordinateMode );
  sl->setAlpha( mAlpha );
  sl->setOffset( mOffset );
  sl->setOffsetUnit( mOffsetUnit );
  sl->setOffsetMapUnitScale( mOffsetMapUnitScale );
  sl->setAngle( mAngle );
  sl->setWidth( mWidth );
  sl->setWidthUnit( mWidthUnit );
  sl->setWidthMapUnitScale( mWidthMapUnitScale );
  copyDataDefinedProperties( sl );
  copyPaintEffect( sl );
  return sl;
}

double QgsRasterFillSymbolLayer::estimateMaxBleed() const
{
  return mOffset.x() > mOffset.y() ? mOffset.x() : mOffset.y();
}

void QgsRasterFillSymbolLayer::setImageFilePath( const QString &imagePath )
{
  mImageFilePath = imagePath;
}

void QgsRasterFillSymbolLayer::setCoordinateMode( const QgsRasterFillSymbolLayer::FillCoordinateMode mode )
{
  mCoordinateMode = mode;
}

void QgsRasterFillSymbolLayer::setAlpha( const double alpha )
{
  mAlpha = alpha;
}

void QgsRasterFillSymbolLayer::applyDataDefinedSettings( QgsSymbolRenderContext &context )
{
  if ( !hasDataDefinedProperties() )
    return; // shortcut

  bool hasWidthExpression = hasDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH );
  bool hasFileExpression = hasDataDefinedProperty( QgsSymbolLayer::EXPR_FILE );
  bool hasAlphaExpression = hasDataDefinedProperty( QgsSymbolLayer::EXPR_ALPHA );
  bool hasAngleExpression = hasDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE );

  if ( !hasWidthExpression && !hasAngleExpression && !hasAlphaExpression && !hasFileExpression )
  {
    return; //no data defined settings
  }

  bool ok;
  if ( hasAngleExpression )
  {
    context.setOriginalValueVariable( mAngle );
    double nextAngle = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_ANGLE, context, QVariant(), &ok ).toDouble();
    if ( ok )
      mNextAngle = nextAngle;
  }

  if ( !hasWidthExpression && !hasAlphaExpression && !hasFileExpression )
  {
    return; //nothing further to do
  }

  double width = mWidth;
  if ( hasWidthExpression )
  {
    context.setOriginalValueVariable( mWidth );
    width = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_WIDTH, context, mWidth ).toDouble();
  }
  double alpha = mAlpha;
  if ( hasAlphaExpression )
  {
    context.setOriginalValueVariable( mAlpha );
    alpha = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_ALPHA, context, mAlpha ).toDouble();
  }
  QString file = mImageFilePath;
  if ( hasFileExpression )
  {
    context.setOriginalValueVariable( mImageFilePath );
    file = evaluateDataDefinedProperty( QgsSymbolLayer::EXPR_FILE, context, mImageFilePath ).toString();
  }
  applyPattern( mBrush, file, width, alpha, context );
}

void QgsRasterFillSymbolLayer::applyPattern( QBrush &brush, const QString &imageFilePath, const double width, const double alpha, const QgsSymbolRenderContext &context )
{
  QImage image( imageFilePath );
  if ( image.isNull() )
  {
    return;
  }
  if ( !image.hasAlphaChannel() )
  {
    image = image.convertToFormat( QImage::Format_ARGB32 );
  }

  double pixelWidth;
  if ( width > 0 )
  {
    pixelWidth = width * QgsSymbolLayerUtils::pixelSizeScaleFactor( context.renderContext(), mWidthUnit, mWidthMapUnitScale );
  }
  else
  {
    pixelWidth = image.width();
  }

  //reduce alpha of image
  if ( alpha < 1.0 )
  {
    QPainter p;
    p.begin( &image );
    p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
    QColor alphaColor( 0, 0, 0 );
    alphaColor.setAlphaF( alpha );
    p.fillRect( image.rect(), alphaColor );
    p.end();
  }

  //resize image if required
  if ( !qgsDoubleNear( pixelWidth, image.width() ) )
  {
    image = image.scaledToWidth( pixelWidth, Qt::SmoothTransformation );
  }

  brush.setTextureImage( image );
}
