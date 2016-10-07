/***************************************************************************
         qgspointdistancerenderer.cpp
         ----------------------------
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

#include "qgspointdistancerenderer.h"
#include "qgsgeometry.h"
#include "qgssymbollayerutils.h"
#include "qgsspatialindex.h"
#include "qgsmultipoint.h"
#include "qgslogger.h"

#include <QDomElement>
#include <QPainter>

#include <cmath>

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

QgsPointDistanceRenderer::QgsPointDistanceRenderer( const QString& rendererName, const QString& labelAttributeName )
    : QgsFeatureRenderer( rendererName )
    , mLabelAttributeName( labelAttributeName )
    , mLabelIndex( -1 )
    , mTolerance( 3 )
    , mToleranceUnit( QgsUnitTypes::RenderMillimeters )
    , mDrawLabels( true )
    , mMaxLabelScaleDenominator( -1 )
    , mSpatialIndex( nullptr )
{
  mRenderer.reset( QgsFeatureRenderer::defaultRenderer( QgsWkbTypes::PointGeometry ) );
}

void QgsPointDistanceRenderer::toSld( QDomDocument& doc, QDomElement &element, QgsStringMap props ) const
{
  mRenderer->toSld( doc, element, props );
}


bool QgsPointDistanceRenderer::renderFeature( QgsFeature& feature, QgsRenderContext& context, int layer, bool selected, bool drawVertexMarker )
{
  Q_UNUSED( drawVertexMarker );
  Q_UNUSED( context );
  Q_UNUSED( layer );

  //check if there is already a point at that position
  if ( !feature.hasGeometry() )
    return false;

  QgsMarkerSymbol* symbol = firstSymbolForFeature( feature, context );

  //if the feature has no symbol (eg, no matching rule in a rule-based renderer), skip it
  if ( !symbol )
    return false;

  //point position in screen coords
  QgsGeometry geom = feature.geometry();
  QgsWkbTypes::Type geomType = geom.wkbType();
  if ( QgsWkbTypes::flatType( geomType ) != QgsWkbTypes::Point )
  {
    //can only render point type
    return false;
  }

  QString label;
  if ( mDrawLabels )
  {
    label = getLabel( feature );
  }

  QgsCoordinateTransform xform = context.coordinateTransform();
  QgsFeature transformedFeature = feature;
  if ( xform.isValid() )
  {
    geom.transform( xform );
    transformedFeature.setGeometry( geom );
  }

  double searchDistance = mTolerance * QgsSymbolLayerUtils::mapUnitScaleFactor( context, mToleranceUnit, mToleranceMapUnitScale );
  QgsPoint point = transformedFeature.geometry().asPoint();
  QList<QgsFeatureId> intersectList = mSpatialIndex->intersects( searchRect( point, searchDistance ) );
  if ( intersectList.empty() )
  {
    mSpatialIndex->insertFeature( transformedFeature );
    // create new group
    ClusteredGroup newGroup;
    newGroup << GroupedFeature( transformedFeature, symbol, selected, label );
    mClusteredGroups.push_back( newGroup );
    // add to group index
    mGroupIndex.insert( transformedFeature.id(), mClusteredGroups.count() - 1 );
    mGroupLocations.insert( transformedFeature.id(), point );
  }
  else
  {
    // find group with closest location to this point (may be more than one within search tolerance)
    QgsFeatureId minDistFeatureId = intersectList.at( 0 );
    double minDist = mGroupLocations.value( minDistFeatureId ).distance( point );
    for ( int i = 1; i < intersectList.count(); ++i )
    {
      QgsFeatureId candidateId = intersectList.at( i );
      double newDist = mGroupLocations.value( candidateId ).distance( point );
      if ( newDist < minDist )
      {
        minDist = newDist;
        minDistFeatureId = candidateId;
      }
    }

    int groupIdx = mGroupIndex[ minDistFeatureId ];
    ClusteredGroup& group = mClusteredGroups[groupIdx];

    // calculate new centroid of group
    QgsPoint oldCenter = mGroupLocations.value( minDistFeatureId );
    mGroupLocations[ minDistFeatureId ] = QgsPoint(( oldCenter.x() * group.size() + point.x() ) / ( group.size() + 1.0 ),
                                          ( oldCenter.y() * group.size() + point.y() ) / ( group.size() + 1.0 ) );

    // add to a group
    group << GroupedFeature( transformedFeature, symbol, selected, label );
    // add to group index
    mGroupIndex.insert( transformedFeature.id(), groupIdx );
  }

  return true;
}

void QgsPointDistanceRenderer::drawGroup( const ClusteredGroup& group, QgsRenderContext& context )
{
  //calculate centroid of all points, this will be center of group
  QgsMultiPointV2* groupMultiPoint = new QgsMultiPointV2();
  Q_FOREACH ( const GroupedFeature& f, group )
  {
    groupMultiPoint->addGeometry( f.feature.geometry().geometry()->clone() );
  }
  QgsGeometry groupGeom( groupMultiPoint );
  QgsGeometry centroid = groupGeom.centroid();
  QPointF pt = centroid.asQPointF();
  context.mapToPixel().transformInPlace( pt.rx(), pt.ry() );

  context.expressionContext().appendScope( createGroupScope( group ) );
  drawGroup( pt, context, group );
  delete context.expressionContext().popScope();
}

void QgsPointDistanceRenderer::setEmbeddedRenderer( QgsFeatureRenderer* r )
{
  mRenderer.reset( r );
}

const QgsFeatureRenderer*QgsPointDistanceRenderer::embeddedRenderer() const
{
  return mRenderer.data();
}

void QgsPointDistanceRenderer::setLegendSymbolItem( const QString& key, QgsSymbol* symbol )
{
  if ( !mRenderer )
    return;

  mRenderer->setLegendSymbolItem( key, symbol );
}

bool QgsPointDistanceRenderer::legendSymbolItemsCheckable() const
{
  if ( !mRenderer )
    return false;

  return mRenderer->legendSymbolItemsCheckable();
}

bool QgsPointDistanceRenderer::legendSymbolItemChecked( const QString& key )
{
  if ( !mRenderer )
    return false;

  return mRenderer->legendSymbolItemChecked( key );
}

void QgsPointDistanceRenderer::checkLegendSymbolItem( const QString& key, bool state )
{
  if ( !mRenderer )
    return;

  return mRenderer->checkLegendSymbolItem( key, state );
}

QString QgsPointDistanceRenderer::filter( const QgsFields& fields )
{
  if ( !mRenderer )
    return QgsFeatureRenderer::filter( fields );
  else
    return mRenderer->filter( fields );
}

QSet<QString> QgsPointDistanceRenderer::usedAttributes() const
{
  QSet<QString> attributeList;
  if ( !mLabelAttributeName.isEmpty() )
  {
    attributeList.insert( mLabelAttributeName );
  }
  if ( mRenderer )
  {
    attributeList += mRenderer->usedAttributes();
  }
  return attributeList;
}

QgsFeatureRenderer::Capabilities QgsPointDistanceRenderer::capabilities()
{
  if ( !mRenderer )
  {
    return 0;
  }
  return mRenderer->capabilities();
}

QgsSymbolList QgsPointDistanceRenderer::symbols( QgsRenderContext& context )
{
  if ( !mRenderer )
  {
    return QgsSymbolList();
  }
  return mRenderer->symbols( context );
}

QgsSymbol* QgsPointDistanceRenderer::symbolForFeature( QgsFeature& feature, QgsRenderContext& context )
{
  if ( !mRenderer )
  {
    return nullptr;
  }
  return mRenderer->symbolForFeature( feature, context );
}

QgsSymbol* QgsPointDistanceRenderer::originalSymbolForFeature( QgsFeature& feat, QgsRenderContext& context )
{
  if ( !mRenderer )
    return nullptr;
  return mRenderer->originalSymbolForFeature( feat, context );
}

QgsSymbolList QgsPointDistanceRenderer::symbolsForFeature( QgsFeature& feature, QgsRenderContext& context )
{
  if ( !mRenderer )
  {
    return QgsSymbolList();
  }
  return mRenderer->symbolsForFeature( feature, context );
}

QgsSymbolList QgsPointDistanceRenderer::originalSymbolsForFeature( QgsFeature& feat, QgsRenderContext& context )
{
  if ( !mRenderer )
    return QgsSymbolList();
  return mRenderer->originalSymbolsForFeature( feat, context );
}

bool QgsPointDistanceRenderer::willRenderFeature( QgsFeature& feat, QgsRenderContext& context )
{
  if ( !mRenderer )
  {
    return false;
  }
  return mRenderer->willRenderFeature( feat, context );
}


void QgsPointDistanceRenderer::startRender( QgsRenderContext& context, const QgsFields& fields )
{
  mRenderer->startRender( context, fields );

  mClusteredGroups.clear();
  mGroupIndex.clear();
  mGroupLocations.clear();
  mSpatialIndex = new QgsSpatialIndex;

  if ( mLabelAttributeName.isEmpty() )
  {
    mLabelIndex = -1;
  }
  else
  {
    mLabelIndex = fields.lookupField( mLabelAttributeName );
  }

  if ( mMaxLabelScaleDenominator > 0 && context.rendererScale() > mMaxLabelScaleDenominator )
  {
    mDrawLabels = false;
  }
  else
  {
    mDrawLabels = true;
  }
}

void QgsPointDistanceRenderer::stopRender( QgsRenderContext& context )
{
  //printInfoDisplacementGroups(); //just for debugging

  Q_FOREACH ( const ClusteredGroup& group, mClusteredGroups )
  {
    drawGroup( group, context );
  }

  mClusteredGroups.clear();
  mGroupIndex.clear();
  mGroupLocations.clear();
  delete mSpatialIndex;
  mSpatialIndex = nullptr;

  mRenderer->stopRender( context );
}

QgsLegendSymbologyList QgsPointDistanceRenderer::legendSymbologyItems( QSize iconSize )
{
  if ( mRenderer )
  {
    return mRenderer->legendSymbologyItems( iconSize );
  }
  return QgsLegendSymbologyList();
}

QgsLegendSymbolList QgsPointDistanceRenderer::legendSymbolItems( double scaleDenominator, const QString& rule )
{
  if ( mRenderer )
  {
    return mRenderer->legendSymbolItems( scaleDenominator, rule );
  }
  return QgsLegendSymbolList();
}


QgsRectangle QgsPointDistanceRenderer::searchRect( const QgsPoint& p, double distance ) const
{
  return QgsRectangle( p.x() - distance, p.y() - distance, p.x() + distance, p.y() + distance );
}

void QgsPointDistanceRenderer::printGroupInfo() const
{
#ifdef QGISDEBUG
  int nGroups = mClusteredGroups.size();
  QgsDebugMsg( "number of displacement groups:" + QString::number( nGroups ) );
  for ( int i = 0; i < nGroups; ++i )
  {
    QgsDebugMsg( "***************displacement group " + QString::number( i ) );
    Q_FOREACH ( const GroupedFeature& feature, mClusteredGroups.at( i ) )
    {
      QgsDebugMsg( FID_TO_STRING( feature.feature.id() ) );
    }
  }
#endif
}

QString QgsPointDistanceRenderer::getLabel( const QgsFeature& feature ) const
{
  QString attribute;
  QgsAttributes attrs = feature.attributes();
  if ( mLabelIndex >= 0 && mLabelIndex < attrs.count() )
  {
    attribute = attrs.at( mLabelIndex ).toString();
  }
  return attribute;
}

void QgsPointDistanceRenderer::drawLabels( QPointF centerPoint, QgsSymbolRenderContext& context, const QList<QPointF>& labelShifts, const ClusteredGroup& group )
{
  QPainter* p = context.renderContext().painter();
  if ( !p )
  {
    return;
  }

  QPen labelPen( mLabelColor );
  p->setPen( labelPen );

  //scale font (for printing)
  QFont pixelSizeFont = mLabelFont;
  pixelSizeFont.setPixelSize( context.outputLineWidth( mLabelFont.pointSizeF() * 0.3527 ) );
  QFont scaledFont = pixelSizeFont;
  scaledFont.setPixelSize( pixelSizeFont.pixelSize() * context.renderContext().rasterScaleFactor() );
  p->setFont( scaledFont );

  QFontMetricsF fontMetrics( pixelSizeFont );
  QPointF currentLabelShift; //considers the signs to determine the label position

  QList<QPointF>::const_iterator labelPosIt = labelShifts.constBegin();
  ClusteredGroup::const_iterator groupIt = group.constBegin();

  for ( ; labelPosIt != labelShifts.constEnd() && groupIt != group.constEnd(); ++labelPosIt, ++groupIt )
  {
    currentLabelShift = *labelPosIt;
    if ( currentLabelShift.x() < 0 )
    {
      currentLabelShift.setX( currentLabelShift.x() - fontMetrics.width( groupIt->label ) );
    }
    if ( currentLabelShift.y() > 0 )
    {
      currentLabelShift.setY( currentLabelShift.y() + fontMetrics.ascent() );
    }

    QPointF drawingPoint( centerPoint + currentLabelShift );
    p->save();
    p->translate( drawingPoint.x(), drawingPoint.y() );
    p->scale( 1.0 / context.renderContext().rasterScaleFactor(), 1.0 / context.renderContext().rasterScaleFactor() );
    p->drawText( QPointF( 0, 0 ), groupIt->label );
    p->restore();
  }
}

QgsExpressionContextScope* QgsPointDistanceRenderer::createGroupScope( const ClusteredGroup& group ) const
{
  QgsExpressionContextScope* clusterScope = new QgsExpressionContextScope();
  if ( group.size() > 1 )
  {
    //scan through symbols to check color, eg if all clustered symbols are same color
    QColor groupColor;
    ClusteredGroup::const_iterator groupIt = group.constBegin();
    for ( ; groupIt != group.constEnd(); ++groupIt )
    {
      if ( !groupIt->symbol )
        continue;

      if ( !groupColor.isValid() )
      {
        groupColor = groupIt->symbol->color();
      }
      else
      {
        if ( groupColor != groupIt->symbol->color() )
        {
          groupColor = QColor();
          break;
        }
      }
    }

    if ( groupColor.isValid() )
    {
      clusterScope->setVariable( QgsExpressionContext::EXPR_CLUSTER_COLOR, QgsSymbolLayerUtils::encodeColor( groupColor ) );
    }
    else
    {
      //mixed colors
      clusterScope->setVariable( QgsExpressionContext::EXPR_CLUSTER_COLOR, "" );
    }

    clusterScope->setVariable( QgsExpressionContext::EXPR_CLUSTER_SIZE, group.size() );
  }
  return clusterScope;
}

QgsMarkerSymbol* QgsPointDistanceRenderer::firstSymbolForFeature( QgsFeature& feature, QgsRenderContext &context )
{
  if ( !mRenderer )
  {
    return nullptr;
  }

  QgsSymbolList symbolList = mRenderer->symbolsForFeature( feature, context );
  if ( symbolList.isEmpty() )
  {
    return nullptr;
  }

  return dynamic_cast< QgsMarkerSymbol* >( symbolList.at( 0 ) );
}
