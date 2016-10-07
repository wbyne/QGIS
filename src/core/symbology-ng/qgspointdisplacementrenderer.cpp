/***************************************************************************
         qgspointdisplacementrenderer.cpp
         --------------------------------
  begin                : January 26, 2010
  copyright            : (C) 2010 by Marco Hugentobler
  email                : marco at hugis dot net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgspointdisplacementrenderer.h"
#include "qgssymbollayerutils.h"
#include "qgsfontutils.h"
#include "qgspainteffectregistry.h"
#include "qgspainteffect.h"
#include "qgspointclusterrenderer.h"

#include <QPainter>
#include <cmath>

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

QgsPointDisplacementRenderer::QgsPointDisplacementRenderer( const QString& labelAttributeName )
    : QgsPointDistanceRenderer( "pointDisplacement", labelAttributeName )
    , mPlacement( Ring )
    , mCircleWidth( 0.4 )
    , mCircleColor( QColor( 125, 125, 125 ) )
    , mCircleRadiusAddition( 0 )
{
  mCenterSymbol.reset( new QgsMarkerSymbol() );
}

QgsPointDisplacementRenderer* QgsPointDisplacementRenderer::clone() const
{
  QgsPointDisplacementRenderer* r = new QgsPointDisplacementRenderer( mLabelAttributeName );
  if ( mRenderer )
    r->setEmbeddedRenderer( mRenderer->clone() );
  r->setCircleWidth( mCircleWidth );
  r->setCircleColor( mCircleColor );
  r->setLabelFont( mLabelFont );
  r->setLabelColor( mLabelColor );
  r->setPlacement( mPlacement );
  r->setCircleRadiusAddition( mCircleRadiusAddition );
  r->setMaxLabelScaleDenominator( mMaxLabelScaleDenominator );
  r->setTolerance( mTolerance );
  r->setToleranceUnit( mToleranceUnit );
  r->setToleranceMapUnitScale( mToleranceMapUnitScale );
  if ( mCenterSymbol )
  {
    r->setCenterSymbol( mCenterSymbol->clone() );
  }
  copyRendererData( r );
  return r;
}

void QgsPointDisplacementRenderer::drawGroup( QPointF centerPoint, QgsRenderContext& context, const ClusteredGroup& group )
{

  //calculate max diagonal size from all symbols in group
  double diagonal = 0;

  Q_FOREACH ( const GroupedFeature& feature, group )
  {
    if ( QgsMarkerSymbol* symbol = feature.symbol )
    {
      diagonal = qMax( diagonal, QgsSymbolLayerUtils::convertToPainterUnits( context,
                       M_SQRT2 * symbol->size(),
                       symbol->sizeUnit(), symbol->sizeMapUnitScale() ) );
    }
  }

  QgsSymbolRenderContext symbolContext( context, QgsUnitTypes::RenderMillimeters, 1.0, false );

  QList<QPointF> symbolPositions;
  QList<QPointF> labelPositions;
  double circleRadius = -1.0;
  calculateSymbolAndLabelPositions( symbolContext, centerPoint, group.size(), diagonal, symbolPositions, labelPositions, circleRadius );

  //draw circle
  if ( circleRadius > 0 )
    drawCircle( circleRadius, symbolContext, centerPoint, group.size() );

  if ( group.size() > 1 )
  {
    //draw mid point
    QgsFeature firstFeature = group.at( 0 ).feature;
    if ( mCenterSymbol )
    {
      mCenterSymbol->renderPoint( centerPoint, &firstFeature, context, -1, false );
    }
    else
    {
      context.painter()->drawRect( QRectF( centerPoint.x() - symbolContext.outputLineWidth( 1 ), centerPoint.y() - symbolContext.outputLineWidth( 1 ), symbolContext.outputLineWidth( 2 ), symbolContext.outputLineWidth( 2 ) ) );
    }
  }

  //draw symbols on the circle
  drawSymbols( group, context, symbolPositions );
  //and also the labels
  if ( mLabelIndex >= 0 )
  {
    drawLabels( centerPoint, symbolContext, labelPositions, group );
  }
}


void QgsPointDisplacementRenderer::startRender( QgsRenderContext& context, const QgsFields& fields )
{
  if ( mCenterSymbol )
  {
    mCenterSymbol->startRender( context, fields );
  }

  QgsPointDistanceRenderer::startRender( context, fields );
}

void QgsPointDisplacementRenderer::stopRender( QgsRenderContext& context )
{
  QgsPointDistanceRenderer::stopRender( context );
  if ( mCenterSymbol )
  {
    mCenterSymbol->stopRender( context );
  }
}

QgsFeatureRenderer* QgsPointDisplacementRenderer::create( QDomElement& symbologyElem )
{
  QgsPointDisplacementRenderer* r = new QgsPointDisplacementRenderer();
  r->setLabelAttributeName( symbologyElem.attribute( "labelAttributeName" ) );
  QFont labelFont;
  if ( !QgsFontUtils::setFromXmlChildNode( labelFont, symbologyElem, "labelFontProperties" ) )
  {
    labelFont.fromString( symbologyElem.attribute( "labelFont", "" ) );
  }
  r->setLabelFont( labelFont );
  r->setPlacement( static_cast< Placement >( symbologyElem.attribute( "placement", "0" ).toInt() ) );
  r->setCircleWidth( symbologyElem.attribute( "circleWidth", "0.4" ).toDouble() );
  r->setCircleColor( QgsSymbolLayerUtils::decodeColor( symbologyElem.attribute( "circleColor", "" ) ) );
  r->setLabelColor( QgsSymbolLayerUtils::decodeColor( symbologyElem.attribute( "labelColor", "" ) ) );
  r->setCircleRadiusAddition( symbologyElem.attribute( "circleRadiusAddition", "0.0" ).toDouble() );
  r->setMaxLabelScaleDenominator( symbologyElem.attribute( "maxLabelScaleDenominator", "-1" ).toDouble() );
  r->setTolerance( symbologyElem.attribute( "tolerance", "0.00001" ).toDouble() );
  r->setToleranceUnit( QgsUnitTypes::decodeRenderUnit( symbologyElem.attribute( "toleranceUnit", "MapUnit" ) ) );
  r->setToleranceMapUnitScale( QgsSymbolLayerUtils::decodeMapUnitScale( symbologyElem.attribute( "toleranceUnitScale" ) ) );

  //look for an embedded renderer <renderer-v2>
  QDomElement embeddedRendererElem = symbologyElem.firstChildElement( "renderer-v2" );
  if ( !embeddedRendererElem.isNull() )
  {
    r->setEmbeddedRenderer( QgsFeatureRenderer::load( embeddedRendererElem ) );
  }

  //center symbol
  QDomElement centerSymbolElem = symbologyElem.firstChildElement( "symbol" );
  if ( !centerSymbolElem.isNull() )
  {
    r->setCenterSymbol( QgsSymbolLayerUtils::loadSymbol<QgsMarkerSymbol>( centerSymbolElem ) );
  }
  return r;
}

QgsMarkerSymbol* QgsPointDisplacementRenderer::centerSymbol()
{
  return mCenterSymbol.data();
}

QDomElement QgsPointDisplacementRenderer::save( QDomDocument& doc )
{
  QDomElement rendererElement = doc.createElement( RENDERER_TAG_NAME );
  rendererElement.setAttribute( "forceraster", ( mForceRaster ? "1" : "0" ) );
  rendererElement.setAttribute( "type", "pointDisplacement" );
  rendererElement.setAttribute( "labelAttributeName", mLabelAttributeName );
  rendererElement.appendChild( QgsFontUtils::toXmlElement( mLabelFont, doc, "labelFontProperties" ) );
  rendererElement.setAttribute( "circleWidth", QString::number( mCircleWidth ) );
  rendererElement.setAttribute( "circleColor", QgsSymbolLayerUtils::encodeColor( mCircleColor ) );
  rendererElement.setAttribute( "labelColor", QgsSymbolLayerUtils::encodeColor( mLabelColor ) );
  rendererElement.setAttribute( "circleRadiusAddition", QString::number( mCircleRadiusAddition ) );
  rendererElement.setAttribute( "placement", static_cast< int >( mPlacement ) );
  rendererElement.setAttribute( "maxLabelScaleDenominator", QString::number( mMaxLabelScaleDenominator ) );
  rendererElement.setAttribute( "tolerance", QString::number( mTolerance ) );
  rendererElement.setAttribute( "toleranceUnit", QgsUnitTypes::encodeUnit( mToleranceUnit ) );
  rendererElement.setAttribute( "toleranceUnitScale", QgsSymbolLayerUtils::encodeMapUnitScale( mToleranceMapUnitScale ) );

  if ( mRenderer )
  {
    QDomElement embeddedRendererElem = mRenderer->save( doc );
    rendererElement.appendChild( embeddedRendererElem );
  }
  if ( mCenterSymbol )
  {
    QDomElement centerSymbolElem = QgsSymbolLayerUtils::saveSymbol( "centerSymbol", mCenterSymbol.data(), doc );
    rendererElement.appendChild( centerSymbolElem );
  }

  if ( mPaintEffect && !QgsPaintEffectRegistry::isDefaultStack( mPaintEffect ) )
    mPaintEffect->saveProperties( doc, rendererElement );

  if ( !mOrderBy.isEmpty() )
  {
    QDomElement orderBy = doc.createElement( "orderby" );
    mOrderBy.save( orderBy );
    rendererElement.appendChild( orderBy );
  }
  rendererElement.setAttribute( "enableorderby", ( mOrderByEnabled ? "1" : "0" ) );

  return rendererElement;
}

QSet<QString> QgsPointDisplacementRenderer::usedAttributes() const
{
  QSet<QString> attr = QgsPointDistanceRenderer::usedAttributes();
  if ( mCenterSymbol )
    attr.unite( mCenterSymbol->usedAttributes() );
  return attr;
}

void QgsPointDisplacementRenderer::setCenterSymbol( QgsMarkerSymbol* symbol )
{
  mCenterSymbol.reset( symbol );
}

void QgsPointDisplacementRenderer::calculateSymbolAndLabelPositions( QgsSymbolRenderContext& symbolContext, QPointF centerPoint, int nPosition,
    double symbolDiagonal, QList<QPointF>& symbolPositions, QList<QPointF>& labelShifts, double& circleRadius ) const
{
  symbolPositions.clear();
  labelShifts.clear();

  if ( nPosition < 1 )
  {
    return;
  }
  else if ( nPosition == 1 ) //If there is only one feature, draw it exactly at the center position
  {
    symbolPositions.append( centerPoint );
    labelShifts.append( QPointF( symbolDiagonal / 2.0, -symbolDiagonal / 2.0 ) );
    return;
  }

  double circleAdditionPainterUnits = symbolContext.outputLineWidth( mCircleRadiusAddition );

  switch ( mPlacement )
  {
    case Ring:
    {
      double minDiameterToFitSymbols = nPosition * symbolDiagonal / ( 2.0 * M_PI );
      double radius = qMax( symbolDiagonal / 2, minDiameterToFitSymbols ) + circleAdditionPainterUnits;

      double fullPerimeter = 2 * M_PI;
      double angleStep = fullPerimeter / nPosition;
      for ( double currentAngle = 0.0; currentAngle < fullPerimeter; currentAngle += angleStep )
      {
        double sinusCurrentAngle = sin( currentAngle );
        double cosinusCurrentAngle = cos( currentAngle );
        QPointF positionShift( radius * sinusCurrentAngle, radius * cosinusCurrentAngle );
        QPointF labelShift(( radius + symbolDiagonal / 2 ) * sinusCurrentAngle, ( radius + symbolDiagonal / 2 ) * cosinusCurrentAngle );
        symbolPositions.append( centerPoint + positionShift );
        labelShifts.append( labelShift );
      }

      circleRadius = radius;
      break;
    }
    case ConcentricRings:
    {
      double centerDiagonal = QgsSymbolLayerUtils::convertToPainterUnits( symbolContext.renderContext(),
                              M_SQRT2 * mCenterSymbol->size(),
                              mCenterSymbol->sizeUnit(), mCenterSymbol->sizeMapUnitScale() );

      int pointsRemaining = nPosition;
      int ringNumber = 1;
      double firstRingRadius = centerDiagonal / 2.0 + symbolDiagonal / 2.0;
      while ( pointsRemaining > 0 )
      {
        double radiusCurrentRing = qMax( firstRingRadius + ( ringNumber - 1 ) * symbolDiagonal + ringNumber * circleAdditionPainterUnits, 0.0 );
        int maxPointsCurrentRing = qMax( floor( 2 * M_PI * radiusCurrentRing / symbolDiagonal ), 1.0 );
        int actualPointsCurrentRing = qMin( maxPointsCurrentRing, pointsRemaining );

        double angleStep = 2 * M_PI / actualPointsCurrentRing;
        double currentAngle = 0.0;
        for ( int i = 0; i < actualPointsCurrentRing; ++i )
        {
          double sinusCurrentAngle = sin( currentAngle );
          double cosinusCurrentAngle = cos( currentAngle );
          QPointF positionShift( radiusCurrentRing * sinusCurrentAngle, radiusCurrentRing * cosinusCurrentAngle );
          QPointF labelShift(( radiusCurrentRing + symbolDiagonal / 2 ) * sinusCurrentAngle, ( radiusCurrentRing + symbolDiagonal / 2 ) * cosinusCurrentAngle );
          symbolPositions.append( centerPoint + positionShift );
          labelShifts.append( labelShift );
          currentAngle += angleStep;
        }

        pointsRemaining -= actualPointsCurrentRing;
        ringNumber++;
        circleRadius = radiusCurrentRing;
      }
      break;
    }
  }
}

void QgsPointDisplacementRenderer::drawCircle( double radiusPainterUnits, QgsSymbolRenderContext& context, QPointF centerPoint, int nSymbols )
{
  QPainter* p = context.renderContext().painter();
  if ( nSymbols < 2 || !p ) //draw circle only if multiple features
  {
    return;
  }

  //draw Circle
  QPen circlePen( mCircleColor );
  circlePen.setWidthF( context.outputLineWidth( mCircleWidth ) );
  p->setPen( circlePen );
  p->drawArc( QRectF( centerPoint.x() - radiusPainterUnits, centerPoint.y() - radiusPainterUnits, 2 * radiusPainterUnits, 2 * radiusPainterUnits ), 0, 5760 );
}

void QgsPointDisplacementRenderer::drawSymbols( const ClusteredGroup& group, QgsRenderContext& context, const QList<QPointF>& symbolPositions )
{
  QList<QPointF>::const_iterator symbolPosIt = symbolPositions.constBegin();
  ClusteredGroup::const_iterator groupIt = group.constBegin();
  for ( ; symbolPosIt != symbolPositions.constEnd() && groupIt != group.constEnd();
        ++symbolPosIt, ++groupIt )
  {
    context.expressionContext().setFeature( groupIt->feature );
    groupIt->symbol->startRender( context );
    groupIt->symbol->renderPoint( *symbolPosIt, &( groupIt->feature ), context, -1, groupIt->isSelected );
    groupIt->symbol->stopRender( context );
  }
}

QgsPointDisplacementRenderer* QgsPointDisplacementRenderer::convertFromRenderer( const QgsFeatureRenderer* renderer )
{
  if ( renderer->type() == "pointDisplacement" )
  {
    return dynamic_cast<QgsPointDisplacementRenderer*>( renderer->clone() );
  }
  else if ( renderer->type() == "singleSymbol" ||
            renderer->type() == "categorizedSymbol" ||
            renderer->type() == "graduatedSymbol" ||
            renderer->type() == "RuleRenderer" )
  {
    QgsPointDisplacementRenderer* pointRenderer = new QgsPointDisplacementRenderer();
    pointRenderer->setEmbeddedRenderer( renderer->clone() );
    return pointRenderer;
  }
  else if ( renderer->type() == "pointCluster" )
  {
    QgsPointDisplacementRenderer* pointRenderer = new QgsPointDisplacementRenderer();
    const QgsPointClusterRenderer* clusterRenderer = static_cast< const QgsPointClusterRenderer* >( renderer );
    if ( clusterRenderer->embeddedRenderer() )
      pointRenderer->setEmbeddedRenderer( clusterRenderer->embeddedRenderer()->clone() );
    pointRenderer->setTolerance( clusterRenderer->tolerance() );
    pointRenderer->setToleranceUnit( clusterRenderer->toleranceUnit() );
    pointRenderer->setToleranceMapUnitScale( clusterRenderer->toleranceMapUnitScale() );
    if ( const_cast< QgsPointClusterRenderer* >( clusterRenderer )->clusterSymbol() )
      pointRenderer->setCenterSymbol( const_cast< QgsPointClusterRenderer* >( clusterRenderer )->clusterSymbol()->clone() );
    return pointRenderer;
  }
  else
  {
    return nullptr;
  }
}
