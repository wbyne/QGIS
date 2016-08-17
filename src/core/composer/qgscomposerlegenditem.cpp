/***************************************************************************
                         qgscomposerlegenditem.cpp  -  description
                         -------------------------
    begin                : May 2010
    copyright            : (C) 2010 by Marco Hugentobler
    email                : marco dot hugentobler at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgscomposerlegendstyle.h"
#include "qgscomposerlegenditem.h"
#include "qgscomposerlegend.h"
#include "qgsmaplayerregistry.h"
#include "qgsrasterlayer.h"
#include "qgsrenderer.h"
#include "qgssymbol.h"
#include "qgssymbollayerutils.h"
#include "qgsvectorlayer.h"
#include "qgsapplication.h"
#include <QDomDocument>
#include <QDomElement>

QgsComposerLegendItem::QgsComposerLegendItem( QgsComposerLegendStyle::Style s ): QStandardItem()
    , mStyle( s )
{
}

QgsComposerLegendItem::QgsComposerLegendItem( const QString& text, QgsComposerLegendStyle::Style s ): QStandardItem( text )
    , mStyle( s )
{
}

QgsComposerLegendItem::QgsComposerLegendItem( const QIcon& icon, const QString& text, QgsComposerLegendStyle::Style s ): QStandardItem( icon, text )
    , mStyle( s )
{
}

QgsComposerLegendItem::~QgsComposerLegendItem()
{
}

void QgsComposerLegendItem::writeXmlChildren( QDomElement& elem, QDomDocument& doc ) const
{
  int numRows = rowCount();
  QgsComposerLegendItem* currentItem = nullptr;
  for ( int i = 0; i < numRows; ++i )
  {
    currentItem = dynamic_cast<QgsComposerLegendItem*>( child( i, 0 ) );
    if ( currentItem )
    {
      currentItem->writeXml( elem, doc );
    }
  }
}


////////////////QgsComposerSymbolItem


QgsComposerSymbolItem::QgsComposerSymbolItem(): QgsComposerLegendItem( QgsComposerLegendStyle::Symbol ), mSymbol( nullptr )
{
}

QgsComposerSymbolItem::QgsComposerSymbolItem( const QString& text ): QgsComposerLegendItem( text, QgsComposerLegendStyle::Symbol ), mSymbol( nullptr )
{
}

QgsComposerSymbolItem::QgsComposerSymbolItem( const QIcon& icon, const QString& text ): QgsComposerLegendItem( icon, text, QgsComposerLegendStyle::Symbol ), mSymbol( nullptr )
{
}

QgsComposerSymbolItem::~QgsComposerSymbolItem()
{
  delete mSymbol;
}

QStandardItem* QgsComposerSymbolItem::clone() const
{
  QgsComposerSymbolItem* cloneItem = new QgsComposerSymbolItem();
  *cloneItem = *this;
  if ( mSymbol )
  {
    cloneItem->setSymbol( mSymbol->clone() );
  }
  return cloneItem;
}

void QgsComposerSymbolItem::writeXml( QDomElement& elem, QDomDocument& doc ) const
{
  QDomElement vectorClassElem = doc.createElement( "VectorClassificationItemNg" );
  if ( mSymbol )
  {
    QgsSymbolMap saveSymbolMap;
    saveSymbolMap.insert( "classificationSymbol", mSymbol );
    QDomElement symbolsElem = QgsSymbolLayerUtils::saveSymbols( saveSymbolMap, "symbols", doc );
    vectorClassElem.appendChild( symbolsElem );
  }
  vectorClassElem.setAttribute( "text", text() );
  vectorClassElem.setAttribute( "userText", userText() );
  elem.appendChild( vectorClassElem );
}

void QgsComposerSymbolItem::readXml( const QDomElement& itemElem, bool xServerAvailable )
{
  if ( itemElem.isNull() )
  {
    return;
  }

  setText( itemElem.attribute( "text", "" ) );
  setUserText( itemElem.attribute( "userText", "" ) );
  QDomElement symbolsElem = itemElem.firstChildElement( "symbols" );
  if ( !symbolsElem.isNull() )
  {
    QgsSymbolMap loadSymbolMap = QgsSymbolLayerUtils::loadSymbols( symbolsElem );
    //we assume there is only one symbol in the map...
    QgsSymbolMap::iterator mapIt = loadSymbolMap.begin();
    if ( mapIt != loadSymbolMap.end() )
    {
      QgsSymbol* symbolNg = mapIt.value();
      if ( symbolNg )
      {
        setSymbol( symbolNg );
        if ( xServerAvailable )
        {
          setIcon( QgsSymbolLayerUtils::symbolPreviewIcon( symbolNg, QSize( 30, 30 ) ) );
        }
      }
    }
  }
}

void QgsComposerSymbolItem::setSymbol( QgsSymbol* s )
{
  delete mSymbol;
  mSymbol = s;
}

////////////////////QgsComposerRasterSymbolItem

QgsComposerRasterSymbolItem::QgsComposerRasterSymbolItem(): QgsComposerLegendItem( QgsComposerLegendStyle::Symbol )
{
}

QgsComposerRasterSymbolItem::QgsComposerRasterSymbolItem( const QString& text ): QgsComposerLegendItem( text, QgsComposerLegendStyle::Symbol )
{
}

QgsComposerRasterSymbolItem::QgsComposerRasterSymbolItem( const QIcon& icon, const QString& text ): QgsComposerLegendItem( icon, text, QgsComposerLegendStyle::Symbol )
{
}

QgsComposerRasterSymbolItem::~QgsComposerRasterSymbolItem()
{
}

QStandardItem* QgsComposerRasterSymbolItem::clone() const
{
  QgsComposerRasterSymbolItem* cloneItem  = new QgsComposerRasterSymbolItem();
  *cloneItem = *this;
  cloneItem->setLayerId( mLayerID );
  return cloneItem;
}

void QgsComposerRasterSymbolItem::writeXml( QDomElement& elem, QDomDocument& doc ) const
{
  QDomElement rasterClassElem = doc.createElement( "RasterClassificationItem" );
  rasterClassElem.setAttribute( "layerId", mLayerID );
  rasterClassElem.setAttribute( "text", text() );
  rasterClassElem.setAttribute( "userText", userText() );
  rasterClassElem.setAttribute( "color", mColor.name() );
  elem.appendChild( rasterClassElem );
}

void QgsComposerRasterSymbolItem::readXml( const QDomElement& itemElem, bool xServerAvailable )
{
  if ( itemElem.isNull() )
  {
    return;
  }
  setText( itemElem.attribute( "text", "" ) );
  setUserText( itemElem.attribute( "userText", "" ) );
  setLayerId( itemElem.attribute( "layerId", "" ) );
  setColor( QColor( itemElem.attribute( "color" ) ) );

  if ( xServerAvailable )
  {
    QPixmap itemPixmap( 20, 20 );
    itemPixmap.fill( mColor );
    setIcon( QIcon( itemPixmap ) );
  }
}

////////////////////QgsComposerLayerItem

QgsComposerLayerItem::QgsComposerLayerItem(): QgsComposerLegendItem( QgsComposerLegendStyle::Subgroup )
    , mShowFeatureCount( false )
{
}

QgsComposerLayerItem::QgsComposerLayerItem( const QString& text ): QgsComposerLegendItem( text, QgsComposerLegendStyle::Subgroup )
    , mShowFeatureCount( false )
{
}

QgsComposerLayerItem::~QgsComposerLayerItem()
{
}

QStandardItem* QgsComposerLayerItem::clone() const
{
  QgsComposerLayerItem* cloneItem  = new QgsComposerLayerItem();
  *cloneItem = *this;
  cloneItem->setLayerId( mLayerID );
  return cloneItem;
}

void QgsComposerLayerItem::writeXml( QDomElement& elem, QDomDocument& doc ) const
{
  QDomElement layerItemElem = doc.createElement( "LayerItem" );
  layerItemElem.setAttribute( "layerId", mLayerID );
  layerItemElem.setAttribute( "text", text() );
  layerItemElem.setAttribute( "userText", userText() );
  layerItemElem.setAttribute( "showFeatureCount", showFeatureCount() );
  layerItemElem.setAttribute( "style", QgsComposerLegendStyle::styleName( mStyle ) );
  writeXmlChildren( layerItemElem, doc );
  elem.appendChild( layerItemElem );
}

void QgsComposerLayerItem::readXml( const QDomElement& itemElem, bool xServerAvailable )
{
  if ( itemElem.isNull() )
  {
    return;
  }
  setText( itemElem.attribute( "text", "" ) );
  setUserText( itemElem.attribute( "userText", "" ) );
  setLayerId( itemElem.attribute( "layerId", "" ) );
  setShowFeatureCount( itemElem.attribute( "showFeatureCount", "" ) == "1" ? true : false );
  setStyle( QgsComposerLegendStyle::styleFromName( itemElem.attribute( "style", "subgroup" ) ) );

  //now call readXml for all the child items
  QDomNodeList childList = itemElem.childNodes();
  QDomNode currentNode;
  QDomElement currentElem;
  QgsComposerLegendItem* currentChildItem = nullptr;

  int nChildItems = childList.count();
  for ( int i = 0; i < nChildItems; ++i )
  {
    currentNode = childList.at( i );
    if ( !currentNode.isElement() )
    {
      continue;
    }

    currentElem = currentNode.toElement();
    QString elemTag = currentElem.tagName();
    if ( elemTag == "VectorClassificationItem" )
    {
      continue; // legacy - unsupported
    }
    else if ( elemTag == "VectorClassificationItemNg" )
    {
      currentChildItem = new QgsComposerSymbolItem();
    }
    else if ( elemTag == "RasterClassificationItem" )
    {
      currentChildItem = new QgsComposerRasterSymbolItem();
    }
    else
    {
      continue; //unsupported child type
    }
    currentChildItem->readXml( currentElem, xServerAvailable );
    appendRow( currentChildItem );
  }
}

void QgsComposerLayerItem::setDefaultStyle( double scaleDenominator, const QString& rule )
{
  // set default style according to number of symbols
  QgsVectorLayer* vLayer = qobject_cast<QgsVectorLayer*>( QgsMapLayerRegistry::instance()->mapLayer( layerId() ) );
  if ( vLayer )
  {
    QgsFeatureRenderer* renderer = vLayer->renderer();
    if ( renderer )
    {
      QPair<QString, QgsSymbol*> symbolItem = renderer->legendSymbolItems( scaleDenominator, rule ).value( 0 );
      if ( renderer->legendSymbolItems( scaleDenominator, rule ).size() > 1 || !symbolItem.first.isEmpty() )
      {
        setStyle( QgsComposerLegendStyle::Subgroup );
      }
      else
      {
        // Hide title by default for single symbol
        setStyle( QgsComposerLegendStyle::Hidden );
      }
    }
  }
}

////////////////////QgsComposerGroupItem

QgsComposerGroupItem::QgsComposerGroupItem(): QgsComposerLegendItem( QgsComposerLegendStyle::Group )
{
}

QgsComposerGroupItem::QgsComposerGroupItem( const QString& text ): QgsComposerLegendItem( text, QgsComposerLegendStyle::Group )
{
}

QgsComposerGroupItem::~QgsComposerGroupItem()
{
}

QStandardItem* QgsComposerGroupItem::clone() const
{
  QgsComposerGroupItem* cloneItem = new QgsComposerGroupItem();
  *cloneItem = *this;
  return cloneItem;
}

void QgsComposerGroupItem::writeXml( QDomElement& elem, QDomDocument& doc ) const
{
  QDomElement layerGroupElem = doc.createElement( "GroupItem" );
  // text is always user text, but for forward compatibility for now write both
  layerGroupElem.setAttribute( "text", text() );
  layerGroupElem.setAttribute( "userText", userText() );
  layerGroupElem.setAttribute( "style", QgsComposerLegendStyle::styleName( mStyle ) );
  writeXmlChildren( layerGroupElem, doc );
  elem.appendChild( layerGroupElem );
}

void QgsComposerGroupItem::readXml( const QDomElement& itemElem, bool xServerAvailable )
{
  if ( itemElem.isNull() )
  {
    return;
  }
  // text is always user text but for backward compatibility we read also text
  QString userText = itemElem.attribute( "userText", "" );
  if ( userText.isEmpty() )
  {
    userText = itemElem.attribute( "text", "" );
  }
  setText( userText );
  setUserText( userText );

  setStyle( QgsComposerLegendStyle::styleFromName( itemElem.attribute( "style", "group" ) ) );

  //now call readXml for all the child items
  QDomNodeList childList = itemElem.childNodes();
  QDomNode currentNode;
  QDomElement currentElem;
  QgsComposerLegendItem* currentChildItem = nullptr;

  int nChildItems = childList.count();
  for ( int i = 0; i < nChildItems; ++i )
  {
    currentNode = childList.at( i );
    if ( !currentNode.isElement() )
    {
      continue;
    }

    currentElem = currentNode.toElement();
    QString elemTag = currentElem.tagName();

    if ( elemTag == "GroupItem" )
    {
      currentChildItem = new QgsComposerGroupItem();
    }
    else if ( elemTag == "LayerItem" )
    {
      currentChildItem = new QgsComposerLayerItem();
    }
    else
    {
      continue; //unsupported child item type
    }
    currentChildItem->readXml( currentElem, xServerAvailable );

    QList<QStandardItem *> itemsList;
    itemsList << currentChildItem << new QgsComposerStyleItem( currentChildItem );
    appendRow( itemsList );
  }
}

QgsComposerStyleItem::QgsComposerStyleItem(): QStandardItem()
{
}

QgsComposerStyleItem::QgsComposerStyleItem( QgsComposerLegendItem *item ): QStandardItem()
{
  setData( QgsComposerLegendStyle::styleLabel( item->style() ), Qt::DisplayRole );
}

QgsComposerStyleItem::~QgsComposerStyleItem()
{
}
