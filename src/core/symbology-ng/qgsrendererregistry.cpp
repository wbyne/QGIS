/***************************************************************************
    qgsrendererregistry.cpp
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
#include "qgsrendererregistry.h"

// default renderers
#include "qgssinglesymbolrenderer.h"
#include "qgscategorizedsymbolrenderer.h"
#include "qgsgraduatedsymbolrenderer.h"
#include "qgsrulebasedrenderer.h"
#include "qgspointdisplacementrenderer.h"
#include "qgsinvertedpolygonrenderer.h"
#include "qgsheatmaprenderer.h"
#include "qgs25drenderer.h"
#include "qgsnullsymbolrenderer.h"
#include "qgsvectorlayer.h"

QgsRendererRegistry::QgsRendererRegistry()
{
  // add default renderers
  addRenderer( new QgsRendererMetadata( "nullSymbol",
                                        QObject::tr( "No symbols" ),
                                        QgsNullSymbolRenderer::create ) );

  addRenderer( new QgsRendererMetadata( "singleSymbol",
                                        QObject::tr( "Single symbol" ),
                                        QgsSingleSymbolRenderer::create,
                                        QgsSingleSymbolRenderer::createFromSld ) );

  addRenderer( new QgsRendererMetadata( "categorizedSymbol",
                                        QObject::tr( "Categorized" ),
                                        QgsCategorizedSymbolRenderer::create ) );

  addRenderer( new QgsRendererMetadata( "graduatedSymbol",
                                        QObject::tr( "Graduated" ),
                                        QgsGraduatedSymbolRenderer::create ) );

  addRenderer( new QgsRendererMetadata( "RuleRenderer",
                                        QObject::tr( "Rule-based" ),
                                        QgsRuleBasedRenderer::create,
                                        QgsRuleBasedRenderer::createFromSld ) );

  addRenderer( new QgsRendererMetadata( "pointDisplacement",
                                        QObject::tr( "Point displacement" ),
                                        QgsPointDisplacementRenderer::create,
                                        QIcon(),
                                        nullptr,
                                        QgsRendererAbstractMetadata::PointLayer ) );

  addRenderer( new QgsRendererMetadata( "invertedPolygonRenderer",
                                        QObject::tr( "Inverted polygons" ),
                                        QgsInvertedPolygonRenderer::create,
                                        QIcon(),
                                        nullptr,
                                        QgsRendererAbstractMetadata::PolygonLayer ) );

  addRenderer( new QgsRendererMetadata( "heatmapRenderer",
                                        QObject::tr( "Heatmap" ),
                                        QgsHeatmapRenderer::create,
                                        QIcon(),
                                        nullptr,
                                        QgsRendererAbstractMetadata::PointLayer ) );


  addRenderer( new QgsRendererMetadata( "25dRenderer",
                                        QObject::tr( "2.5 D" ),
                                        Qgs25DRenderer::create,
                                        QIcon(),
                                        nullptr,
                                        QgsRendererAbstractMetadata::PolygonLayer ) );
}

QgsRendererRegistry::~QgsRendererRegistry()
{
  qDeleteAll( mRenderers );
}

QgsRendererRegistry* QgsRendererRegistry::instance()
{
  static QgsRendererRegistry mInstance;
  return &mInstance;
}


bool QgsRendererRegistry::addRenderer( QgsRendererAbstractMetadata* metadata )
{
  if ( !metadata || mRenderers.contains( metadata->name() ) )
    return false;

  mRenderers[metadata->name()] = metadata;
  mRenderersOrder << metadata->name();
  return true;
}

bool QgsRendererRegistry::removeRenderer( const QString& rendererName )
{
  if ( !mRenderers.contains( rendererName ) )
    return false;

  delete mRenderers[rendererName];
  mRenderers.remove( rendererName );
  mRenderersOrder.removeAll( rendererName );
  return true;
}

QgsRendererAbstractMetadata* QgsRendererRegistry::rendererMetadata( const QString& rendererName )
{
  return mRenderers.value( rendererName );
}

QgsRendererMetadata::~QgsRendererMetadata() {}

QStringList QgsRendererRegistry::renderersList( QgsRendererAbstractMetadata::LayerTypes layerTypes ) const
{
  QStringList renderers;
  Q_FOREACH ( const QString& renderer, mRenderersOrder )
  {
    if ( mRenderers.value( renderer )->compatibleLayerTypes() & layerTypes )
      renderers << renderer;
  }
  return renderers;
}

QStringList QgsRendererRegistry::renderersList( const QgsVectorLayer* layer ) const
{
  QgsRendererAbstractMetadata::LayerType layerType = QgsRendererAbstractMetadata::All;

  switch ( layer->geometryType() )
  {
    case QgsWkbTypes::PointGeometry:
      layerType = QgsRendererAbstractMetadata::PointLayer;
      break;

    case QgsWkbTypes::LineGeometry:
      layerType = QgsRendererAbstractMetadata::LineLayer;
      break;

    case QgsWkbTypes::PolygonGeometry:
      layerType = QgsRendererAbstractMetadata::PolygonLayer;
      break;

    case QgsWkbTypes::UnknownGeometry:
    case QgsWkbTypes::NullGeometry:
      break;
  }

  return renderersList( layerType );
}
