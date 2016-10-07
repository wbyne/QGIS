/***************************************************************************
    qgsrenderer.cpp
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

#include "qgsrenderer.h"
#include "qgssymbol.h"
#include "qgssymbollayerutils.h"
#include "qgsrulebasedrenderer.h"
#include "qgsdatadefined.h"

#include "qgssinglesymbolrenderer.h" // for default renderer

#include "qgsrendererregistry.h"

#include "qgsrendercontext.h"
#include "qgsclipper.h"
#include "qgsgeometry.h"
#include "qgsgeometrycollection.h"
#include "qgsfeature.h"
#include "qgslogger.h"
#include "qgsvectorlayer.h"
#include "qgspainteffect.h"
#include "qgseffectstack.h"
#include "qgspainteffectregistry.h"
#include "qgswkbptr.h"
#include "qgspointv2.h"

#include <QDomElement>
#include <QDomDocument>
#include <QPolygonF>

QPointF QgsFeatureRenderer::_getPoint( QgsRenderContext& context, const QgsPointV2& point )
{
  return QgsSymbol::_getPoint( context, point );
}

void QgsFeatureRenderer::copyRendererData( QgsFeatureRenderer* destRenderer ) const
{
  if ( !destRenderer || !mPaintEffect )
    return;

  destRenderer->setPaintEffect( mPaintEffect->clone() );
  destRenderer->mOrderBy = mOrderBy;
  destRenderer->mOrderByEnabled = mOrderByEnabled;
}

QgsFeatureRenderer::QgsFeatureRenderer( const QString& type )
    : mType( type )
    , mUsingSymbolLevels( false )
    , mCurrentVertexMarkerType( QgsVectorLayer::Cross )
    , mCurrentVertexMarkerSize( 3 )
    , mPaintEffect( nullptr )
    , mForceRaster( false )
    , mOrderByEnabled( false )
{
  mPaintEffect = QgsPaintEffectRegistry::defaultStack();
  mPaintEffect->setEnabled( false );
}

QgsFeatureRenderer::~QgsFeatureRenderer()
{
  delete mPaintEffect;
}

QgsFeatureRenderer* QgsFeatureRenderer::defaultRenderer( QgsWkbTypes::GeometryType geomType )
{
  return new QgsSingleSymbolRenderer( QgsSymbol::defaultSymbol( geomType ) );
}

QgsSymbol* QgsFeatureRenderer::originalSymbolForFeature( QgsFeature& feature, QgsRenderContext& context )
{
  return symbolForFeature( feature, context );
}

QSet< QString > QgsFeatureRenderer::legendKeysForFeature( QgsFeature& feature, QgsRenderContext& context )
{
  Q_UNUSED( feature );
  Q_UNUSED( context );
  return QSet< QString >();
}

bool QgsFeatureRenderer::filterNeedsGeometry() const
{
  return false;
}

bool QgsFeatureRenderer::renderFeature( QgsFeature& feature, QgsRenderContext& context, int layer, bool selected, bool drawVertexMarker )
{
  QgsSymbol* symbol = symbolForFeature( feature, context );
  if ( !symbol )
    return false;

  renderFeatureWithSymbol( feature, symbol, context, layer, selected, drawVertexMarker );
  return true;
}

void QgsFeatureRenderer::renderFeatureWithSymbol( QgsFeature& feature, QgsSymbol* symbol, QgsRenderContext& context, int layer, bool selected, bool drawVertexMarker )
{
  symbol->renderFeature( feature, context, layer, selected, drawVertexMarker, mCurrentVertexMarkerType, mCurrentVertexMarkerSize );
}

QString QgsFeatureRenderer::dump() const
{
  return "UNKNOWN RENDERER\n";
}

QgsFeatureRenderer* QgsFeatureRenderer::load( QDomElement& element )
{
  // <renderer-v2 type=""> ... </renderer-v2>

  if ( element.isNull() )
    return nullptr;

  // load renderer
  QString rendererType = element.attribute( "type" );

  QgsRendererAbstractMetadata* m = QgsRendererRegistry::instance()->rendererMetadata( rendererType );
  if ( !m )
    return nullptr;

  QgsFeatureRenderer* r = m->createRenderer( element );
  if ( r )
  {
    r->setUsingSymbolLevels( element.attribute( "symbollevels", "0" ).toInt() );
    r->setForceRasterRender( element.attribute( "forceraster", "0" ).toInt() );

    //restore layer effect
    QDomElement effectElem = element.firstChildElement( "effect" );
    if ( !effectElem.isNull() )
    {
      r->setPaintEffect( QgsPaintEffectRegistry::instance()->createEffect( effectElem ) );
    }

    // restore order by
    QDomElement orderByElem = element.firstChildElement( "orderby" );
    r->mOrderBy.load( orderByElem );
    r->setOrderByEnabled( element.attribute( "enableorderby", "0" ).toInt() );
  }
  return r;
}

QDomElement QgsFeatureRenderer::save( QDomDocument& doc )
{
  // create empty renderer element
  QDomElement rendererElem = doc.createElement( RENDERER_TAG_NAME );
  rendererElem.setAttribute( "forceraster", ( mForceRaster ? "1" : "0" ) );

  if ( mPaintEffect && !QgsPaintEffectRegistry::isDefaultStack( mPaintEffect ) )
    mPaintEffect->saveProperties( doc, rendererElem );

  if ( !mOrderBy.isEmpty() )
  {
    QDomElement orderBy = doc.createElement( "orderby" );
    mOrderBy.save( orderBy );
    rendererElem.appendChild( orderBy );
  }
  rendererElem.setAttribute( "enableorderby", ( mOrderByEnabled ? "1" : "0" ) );
  return rendererElem;
}

QgsFeatureRenderer* QgsFeatureRenderer::loadSld( const QDomNode &node, QgsWkbTypes::GeometryType geomType, QString &errorMessage )
{
  QDomElement element = node.toElement();
  if ( element.isNull() )
    return nullptr;

  // get the UserStyle element
  QDomElement userStyleElem = element.firstChildElement( "UserStyle" );
  if ( userStyleElem.isNull() )
  {
    // UserStyle element not found, nothing will be rendered
    errorMessage = "Info: UserStyle element not found.";
    return nullptr;
  }

  // get the FeatureTypeStyle element
  QDomElement featTypeStyleElem = userStyleElem.firstChildElement( "FeatureTypeStyle" );
  if ( featTypeStyleElem.isNull() )
  {
    errorMessage = "Info: FeatureTypeStyle element not found.";
    return nullptr;
  }

  // use the RuleRenderer when more rules are present or the rule
  // has filters or min/max scale denominators set,
  // otherwise use the SingleSymbol renderer
  bool needRuleRenderer = false;
  int ruleCount = 0;

  QDomElement ruleElem = featTypeStyleElem.firstChildElement( "Rule" );
  while ( !ruleElem.isNull() )
  {
    ruleCount++;

    // more rules present, use the RuleRenderer
    if ( ruleCount > 1 )
    {
      QgsDebugMsg( "more Rule elements found: need a RuleRenderer" );
      needRuleRenderer = true;
      break;
    }

    QDomElement ruleChildElem = ruleElem.firstChildElement();
    while ( !ruleChildElem.isNull() )
    {
      // rule has filter or min/max scale denominator, use the RuleRenderer
      if ( ruleChildElem.localName() == "Filter" ||
           ruleChildElem.localName() == "MinScaleDenominator" ||
           ruleChildElem.localName() == "MaxScaleDenominator" )
      {
        QgsDebugMsg( "Filter or Min/MaxScaleDenominator element found: need a RuleRenderer" );
        needRuleRenderer = true;
        break;
      }

      ruleChildElem = ruleChildElem.nextSiblingElement();
    }

    if ( needRuleRenderer )
    {
      break;
    }

    ruleElem = ruleElem.nextSiblingElement( "Rule" );
  }

  QString rendererType;
  if ( needRuleRenderer )
  {
    rendererType = "RuleRenderer";
  }
  else
  {
    rendererType = "singleSymbol";
  }
  QgsDebugMsg( QString( "Instantiating a '%1' renderer..." ).arg( rendererType ) );

  // create the renderer and return it
  QgsRendererAbstractMetadata* m = QgsRendererRegistry::instance()->rendererMetadata( rendererType );
  if ( !m )
  {
    errorMessage = QString( "Error: Unable to get metadata for '%1' renderer." ).arg( rendererType );
    return nullptr;
  }

  QgsFeatureRenderer* r = m->createRendererFromSld( featTypeStyleElem, geomType );
  return r;
}

QDomElement QgsFeatureRenderer::writeSld( QDomDocument& doc, const QString& styleName, QgsStringMap props ) const
{
  QDomElement userStyleElem = doc.createElement( "UserStyle" );

  QDomElement nameElem = doc.createElement( "se:Name" );
  nameElem.appendChild( doc.createTextNode( styleName ) );
  userStyleElem.appendChild( nameElem );

  QDomElement featureTypeStyleElem = doc.createElement( "se:FeatureTypeStyle" );
  toSld( doc, featureTypeStyleElem, props );
  userStyleElem.appendChild( featureTypeStyleElem );

  return userStyleElem;
}

QgsLegendSymbologyList QgsFeatureRenderer::legendSymbologyItems( QSize iconSize )
{
  Q_UNUSED( iconSize );
  // empty list by default
  return QgsLegendSymbologyList();
}

bool QgsFeatureRenderer::legendSymbolItemsCheckable() const
{
  return false;
}

bool QgsFeatureRenderer::legendSymbolItemChecked( const QString& key )
{
  Q_UNUSED( key );
  return false;
}

void QgsFeatureRenderer::checkLegendSymbolItem( const QString& key, bool state )
{
  Q_UNUSED( key );
  Q_UNUSED( state );
}

void QgsFeatureRenderer::setLegendSymbolItem( const QString& key, QgsSymbol* symbol )
{
  Q_UNUSED( key );
  delete symbol;
}

QgsLegendSymbolList QgsFeatureRenderer::legendSymbolItems( double scaleDenominator, const QString& rule )
{
  Q_UNUSED( scaleDenominator );
  Q_UNUSED( rule );
  return QgsLegendSymbolList();
}

QgsLegendSymbolListV2 QgsFeatureRenderer::legendSymbolItemsV2() const
{
  QgsLegendSymbolList lst = const_cast<QgsFeatureRenderer*>( this )->legendSymbolItems();
  QgsLegendSymbolListV2 lst2;
  int i = 0;
  for ( QgsLegendSymbolList::const_iterator it = lst.begin(); it != lst.end(); ++it, ++i )
  {
    lst2 << QgsLegendSymbolItem( it->second, it->first, QString::number( i ), legendSymbolItemsCheckable() );
  }
  return lst2;
}

void QgsFeatureRenderer::setVertexMarkerAppearance( int type, int size )
{
  mCurrentVertexMarkerType = type;
  mCurrentVertexMarkerSize = size;
}

bool QgsFeatureRenderer::willRenderFeature( QgsFeature &feat, QgsRenderContext &context )
{
  return nullptr != symbolForFeature( feat, context );
}

void QgsFeatureRenderer::renderVertexMarker( QPointF pt, QgsRenderContext& context )
{
  QgsVectorLayer::drawVertexMarker( pt.x(), pt.y(), *context.painter(),
                                    static_cast< QgsVectorLayer::VertexMarkerType >( mCurrentVertexMarkerType ),
                                    mCurrentVertexMarkerSize );
}

void QgsFeatureRenderer::renderVertexMarkerPolyline( QPolygonF& pts, QgsRenderContext& context )
{
  Q_FOREACH ( QPointF pt, pts )
    renderVertexMarker( pt, context );
}

void QgsFeatureRenderer::renderVertexMarkerPolygon( QPolygonF& pts, QList<QPolygonF>* rings, QgsRenderContext& context )
{
  Q_FOREACH ( QPointF pt, pts )
    renderVertexMarker( pt, context );

  if ( rings )
  {
    Q_FOREACH ( const QPolygonF& ring, *rings )
    {
      Q_FOREACH ( QPointF pt, ring )
        renderVertexMarker( pt, context );
    }
  }
}

QgsSymbolList QgsFeatureRenderer::symbolsForFeature( QgsFeature &feat, QgsRenderContext &context )
{
  QgsSymbolList lst;
  QgsSymbol* s = symbolForFeature( feat, context );
  if ( s ) lst.append( s );
  return lst;
}

QgsSymbolList QgsFeatureRenderer::originalSymbolsForFeature( QgsFeature &feat, QgsRenderContext &context )
{
  QgsSymbolList lst;
  QgsSymbol* s = originalSymbolForFeature( feat, context );
  if ( s ) lst.append( s );
  return lst;
}

QgsPaintEffect *QgsFeatureRenderer::paintEffect() const
{
  return mPaintEffect;
}

void QgsFeatureRenderer::setPaintEffect( QgsPaintEffect *effect )
{
  delete mPaintEffect;
  mPaintEffect = effect;
}

QgsFeatureRequest::OrderBy QgsFeatureRenderer::orderBy() const
{
  return mOrderBy;
}

void QgsFeatureRenderer::setOrderBy( const QgsFeatureRequest::OrderBy& orderBy )
{
  mOrderBy = orderBy;
}

bool QgsFeatureRenderer::orderByEnabled() const
{
  return mOrderByEnabled;
}

void QgsFeatureRenderer::setOrderByEnabled( bool enabled )
{
  mOrderByEnabled = enabled;
}

void QgsFeatureRenderer::convertSymbolSizeScale( QgsSymbol * symbol, QgsSymbol::ScaleMethod method, const QString & field )
{
  if ( symbol->type() == QgsSymbol::Marker )
  {
    QgsMarkerSymbol * s = static_cast<QgsMarkerSymbol *>( symbol );
    if ( QgsSymbol::ScaleArea == QgsSymbol::ScaleMethod( method ) )
    {
      const QgsDataDefined dd( "coalesce(sqrt(" + QString::number( s->size() ) + " * (" + field + ")),0)" );
      s->setDataDefinedSize( dd );
    }
    else
    {
      const QgsDataDefined dd( "coalesce(" + QString::number( s->size() ) + " * (" + field + "),0)" );
      s->setDataDefinedSize( dd );
    }
    s->setScaleMethod( QgsSymbol::ScaleDiameter );
  }
  else if ( symbol->type() == QgsSymbol::Line )
  {
    QgsLineSymbol * s = static_cast<QgsLineSymbol *>( symbol );
    const QgsDataDefined dd( "coalesce(" + QString::number( s->width() ) + " * (" + field + "),0)" );
    s->setDataDefinedWidth( dd );
  }
}

void QgsFeatureRenderer::convertSymbolRotation( QgsSymbol * symbol, const QString & field )
{
  if ( symbol->type() == QgsSymbol::Marker )
  {
    QgsMarkerSymbol * s = static_cast<QgsMarkerSymbol *>( symbol );
    const QgsDataDefined dd(( s->angle()
                              ? QString::number( s->angle() ) + " + "
                              : QString() ) + field );
    s->setDataDefinedAngle( dd );
  }
}
