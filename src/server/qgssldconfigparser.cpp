/***************************************************************************
                              qgssldconfigparser.cpp
                              ----------------------
  begin                : March 28, 2014
  copyright            : (C) 2014 by Marco Hugentobler
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

#include "qgssldconfigparser.h"
#include "qgsapplication.h"
#include "qgsconfigparserutils.h"
#include "qgslogger.h"
#include "qgsmapserviceexception.h"
#include "qgsrasterlayer.h"
#include "qgsrenderer.h"
#include "qgssinglesymbolrenderer.h"
#include "qgssymbol.h"
#include "qgsvectorlayer.h"
#include "qgsvectordataprovider.h"
#include <sqlite3.h>

//layer builders
#include "qgshostedrdsbuilder.h"
#include "qgshostedvdsbuilder.h"
#include "qgsremotedatasourcebuilder.h"
#include "qgsremoteowsbuilder.h"
#include "qgssentdatasourcebuilder.h"

//for contours
#include "gdal_alg.h"
#include "ogr_srs_api.h"
#include "ogr_api.h"

//for raster interpolation
#include "qgsinterpolationlayerbuilder.h"
#include "qgsgridfilewriter.h"
#include "qgsidwinterpolator.h"
#include "qgstininterpolator.h"

#include <QTemporaryFile>

#if defined(GDAL_VERSION_NUM) && GDAL_VERSION_NUM >= 1800
#define TO8(x) (x).toUtf8().constData()
#else
#define TO8(x) (x).toLocal8Bit().constData()
#endif

QgsSLDConfigParser::QgsSLDConfigParser( QDomDocument* doc, const QMap<QString, QString>& parameters )
    : QgsWmsConfigParser()
    , mXMLDoc( doc )
    , mParameterMap( parameters )
    , mSLDNamespace( "http://www.opengis.net/sld" )
    , mOutputUnits( QgsMapRenderer::Pixels )
    , mFallbackParser( nullptr )
{

  //set output units
  if ( mXMLDoc )
  {
    //first search attribute "units" in <StyledLayerDescriptor> element
    QDomElement sldElement = mXMLDoc->documentElement();
    if ( !sldElement.isNull() )
    {
      QString unitString = sldElement.attribute( "units" );
      if ( !unitString.isEmpty() )
      {
        if ( unitString == "mm" )
        {
          mOutputUnits = QgsMapRenderer::Millimeters;
        }
        else if ( unitString == "pixel" )
        {
          mOutputUnits = QgsMapRenderer::Pixels;
        }
      }
    }
  }
}

QgsSLDConfigParser::~QgsSLDConfigParser()
{
  delete mXMLDoc;
}

void QgsSLDConfigParser::layersAndStylesCapabilities( QDomElement& parentElement, QDomDocument& doc, const QString& version, bool fullProjectSettings ) const
{
  Q_UNUSED( version );
  Q_UNUSED( fullProjectSettings );

  //iterate over all <UserLayer> nodes
  if ( mXMLDoc )
  {
    QDomNode sldNode = mXMLDoc->documentElement();
    if ( !sldNode.isNull() )
    {
      //create wgs84 to reproject the layer bounding boxes
      //QgsCoordinateReferenceSystem wgs84;
      //wgs84.createFromEpsg(4326);

      QDomNodeList layerNodeList = sldNode.toElement().elementsByTagName( "UserLayer" );
      for ( int i = 0; i < layerNodeList.size(); ++i )
      {
        QDomElement layerElement = doc.createElement( "Layer" );
        layerElement.setAttribute( "queryable", "1" ); //support GetFeatureInfo for all layers
        parentElement.appendChild( layerElement );

        //add name
        QDomNodeList nameList = layerNodeList.item( i ).toElement().elementsByTagName( "Name" );
        if ( !nameList.isEmpty() )
        {
          //layer name
          QDomElement layerNameElement = doc.createElement( "Name" );
          QDomText layerNameText = doc.createTextNode( nameList.item( 0 ).toElement().text() );
          layerNameElement.appendChild( layerNameText );
          layerElement.appendChild( layerNameElement );
        }

        //add title
        QDomNodeList titleList = layerNodeList.item( i ).toElement().elementsByTagName( "Title" );
        if ( !titleList.isEmpty() )
        {
          QDomElement layerTitleElement = doc.createElement( "Title" );
          QDomText layerTitleText = doc.createTextNode( titleList.item( 0 ).toElement().text() );
          layerTitleElement.appendChild( layerTitleText );
          layerElement.appendChild( layerTitleElement );
        }
        //add abstract
        QDomNodeList abstractList = layerNodeList.item( i ).toElement().elementsByTagName( "Abstract" );
        if ( !abstractList.isEmpty() )
        {
          QDomElement layerAbstractElement = doc.createElement( "Abstract" );
          QDomText layerAbstractText = doc.createTextNode( abstractList.item( 0 ).toElement().text() );
          layerAbstractElement.appendChild( layerAbstractText );
          layerElement.appendChild( layerAbstractElement );
        }


        //get QgsMapLayer object to add Ex_GeographicalBoundingBox, Bounding Box
        QList<QgsMapLayer*> layerList = mapLayerFromStyle( nameList.item( 0 ).toElement().text(), "" );
        if ( layerList.size() < 1 )//error while generating the layer
        {
          QgsDebugMsg( "Error, no maplayer in layer list" );
          continue;
        }

        //get only the first layer since we don't want to have the other ones in the capabilities document
        QgsMapLayer* theMapLayer = layerList.at( 0 );
        if ( !theMapLayer )//error while generating the layer
        {
          QgsDebugMsg( "Error, QgsMapLayer object is 0" );
          continue;
        }

        //append geographic bbox and the CRS elements
        QStringList crsNumbers = QgsConfigParserUtils::createCrsListForLayer( theMapLayer );
        QStringList crsRestriction; //no crs restrictions in SLD parser
        QgsConfigParserUtils::appendCrsElementsToLayer( layerElement, doc, crsNumbers, crsRestriction );
        QgsConfigParserUtils::appendLayerBoundingBoxes( layerElement, doc, theMapLayer->extent(), theMapLayer->crs(), crsNumbers, crsRestriction );

        //iterate over all <UserStyle> nodes within a user layer
        QDomNodeList userStyleList = layerNodeList.item( i ).toElement().elementsByTagName( "UserStyle" );
        for ( int j = 0; j < userStyleList.size(); ++j )
        {
          QDomElement styleElement = doc.createElement( "Style" );
          layerElement.appendChild( styleElement );
          //Name
          QDomNodeList nameList = userStyleList.item( j ).toElement().elementsByTagName( "Name" );
          if ( !nameList.isEmpty() )
          {
            QDomElement styleNameElement = doc.createElement( "Name" );
            QDomText styleNameText = doc.createTextNode( nameList.item( 0 ).toElement().text() );
            styleNameElement.appendChild( styleNameText );
            styleElement.appendChild( styleNameElement );

            QDomElement styleTitleElement = doc.createElement( "Title" );
            QDomText styleTitleText = doc.createTextNode( nameList.item( 0 ).toElement().text() );
            styleTitleElement.appendChild( styleTitleText );
            styleElement.appendChild( styleTitleElement );
          }
          //Title
          QDomNodeList titleList = userStyleList.item( j ).toElement().elementsByTagName( "Title" );
          if ( !titleList.isEmpty() )
          {
            QDomElement styleTitleElement = doc.createElement( "Title" );
            QDomText styleTitleText = doc.createTextNode( titleList.item( 0 ).toElement().text() );
            styleTitleElement.appendChild( styleTitleText );
            styleElement.appendChild( styleTitleElement );
          }
          //Abstract
          QDomNodeList abstractList = userStyleList.item( j ).toElement().elementsByTagName( "Abstract" );
          if ( !abstractList.isEmpty() )
          {
            QDomElement styleAbstractElement = doc.createElement( "Abstract" );
            QDomText styleAbstractText = doc.createTextNode( abstractList.item( 0 ).toElement().text() );
            styleAbstractElement.appendChild( styleAbstractText );
            styleElement.appendChild( styleAbstractElement );
          }
        }
      }
    }
  }
}

QList<QgsMapLayer*> QgsSLDConfigParser::mapLayerFromStyle( const QString& lName, const QString& styleName, bool useCache ) const
{
  QList<QgsMapLayer*> fallbackLayerList;
  QList<QgsMapLayer*> resultList;

  //first check if this layer is a named layer with a user style
  QList<QDomElement> namedLayerElemList = findNamedLayerElements( lName );
  for ( int i = 0; i < namedLayerElemList.size(); ++i )
  {
    QDomElement userStyleElement = findUserStyleElement( namedLayerElemList[i], styleName );
    if ( !userStyleElement.isNull() )
    {
      fallbackLayerList = mFallbackParser->mapLayerFromStyle( lName, "", false );
      if ( !fallbackLayerList.isEmpty() )
      {
        QgsVectorLayer* v = dynamic_cast<QgsVectorLayer*>( fallbackLayerList.at( 0 ) );
        if ( v )
        {
          QgsFeatureRenderer* r = rendererFromUserStyle( userStyleElement, v );
          v->setRenderer( r );
          labelSettingsFromUserStyle( userStyleElement, v );

          resultList.push_back( v );
          return resultList;
        }
        else
        {
          QgsRasterLayer* r = dynamic_cast<QgsRasterLayer*>( fallbackLayerList.at( 0 ) ); //a raster layer?
          if ( r )
          {
            rasterSymbologyFromUserStyle( userStyleElement, r );

            //Using a contour symbolizer, there may be a raster and a vector layer
            QgsVectorLayer* v = contourLayerFromRaster( userStyleElement, r );
            if ( v )
            {
              resultList.push_back( v ); //contour layer must be added before raster
              mLayersToRemove.push_back( v ); //don't cache contour layers at the moment
            }
            resultList.push_back( r );
            return resultList;
          }
        }
      }
    }

    // try with a predefined named style
    QDomElement namedStyleElement = findNamedStyleElement( namedLayerElemList[i], styleName );
    if ( !namedStyleElement.isNull() )
    {
      fallbackLayerList = mFallbackParser->mapLayerFromStyle( lName, styleName, false );
      if ( !fallbackLayerList.isEmpty() )
      {
        resultList << fallbackLayerList;
        return resultList;
      }
    }
  }

  QDomElement userLayerElement = findUserLayerElement( lName );

  if ( userLayerElement.isNull() )
  {
    //maybe named layer and named style is defined in the fallback SLD?
    if ( mFallbackParser )
    {
      resultList = mFallbackParser->mapLayerFromStyle( lName, styleName, useCache );
    }

    return resultList;
  }

  QDomElement userStyleElement = findUserStyleElement( userLayerElement, styleName );

  QgsMapLayer* theMapLayer = mapLayerFromUserLayer( userLayerElement, lName, useCache );
  if ( !theMapLayer )
  {
    return resultList;
  }

  QgsFeatureRenderer* theRenderer = nullptr;

  QgsRasterLayer* theRasterLayer = dynamic_cast<QgsRasterLayer*>( theMapLayer );
  if ( theRasterLayer )
  {
    QgsDebugMsg( "Layer is a rasterLayer" );
    if ( !userStyleElement.isNull() )
    {
      QgsDebugMsg( "Trying to add raster symbology" );
      rasterSymbologyFromUserStyle( userStyleElement, theRasterLayer );
      //todo: possibility to have vector layer or raster layer
      QgsDebugMsg( "Trying to find contour symbolizer" );
      QgsVectorLayer* v = contourLayerFromRaster( userStyleElement, theRasterLayer );
      if ( v )
      {
        QgsDebugMsg( "Returning vector layer" );
        resultList.push_back( v );
        mLayersToRemove.push_back( v );
      }
    }
    resultList.push_back( theMapLayer );

    return resultList;
  }

  QgsVectorLayer* theVectorLayer = dynamic_cast<QgsVectorLayer*>( theMapLayer );
  if ( userStyleElement.isNull() )//apply a default style
  {
    QgsSymbol* symbol = QgsSymbol::defaultSymbol( theVectorLayer->geometryType() );
    theRenderer = new QgsSingleSymbolRenderer( symbol );
  }
  else
  {
    QgsDebugMsg( "Trying to get a renderer from the user style" );
    theRenderer = rendererFromUserStyle( userStyleElement, theVectorLayer );
    //apply labels if <TextSymbolizer> tag is present
    labelSettingsFromUserStyle( userStyleElement, theVectorLayer );
  }

  if ( !theRenderer )
  {
    QgsDebugMsg( "Error, could not create a renderer" );
    delete theVectorLayer;
    return resultList;
  }
  theVectorLayer->setRenderer( theRenderer );
  QgsDebugMsg( "Returning the vectorlayer" );
  resultList.push_back( theVectorLayer );
  return resultList;
}

int QgsSLDConfigParser::layersAndStyles( QStringList& layers, QStringList& styles ) const
{
  QgsDebugMsg( "Entering." );
  layers.clear();
  styles.clear();

  if ( mXMLDoc )
  {
    QDomElement sldElem = mXMLDoc->documentElement();
    if ( !sldElem.isNull() )
    {
      //go through all the children and search for <NamedLayers> and <UserLayers>
      QDomNodeList layerNodes = sldElem.childNodes();
      for ( int i = 0; i < layerNodes.size(); ++i )
      {
        QDomElement currentLayerElement = layerNodes.item( i ).toElement();
        if ( currentLayerElement.localName() == "NamedLayer" )
        {
          QgsDebugMsg( "Found a NamedLayer" );
          //layer name
          QDomNodeList nameList = currentLayerElement.elementsByTagName/*NS*/( /*mSLDNamespace,*/ "Name" );
          if ( nameList.length() < 1 )
          {
            continue; //a layer name is mandatory
          }
          QString layerName = nameList.item( 0 ).toElement().text();

          //find the Named Styles and the corresponding names
          QDomNodeList namedStyleList = currentLayerElement.elementsByTagName/*NS*/( /*mSLDNamespace,*/ "NamedStyle" );
          for ( int j = 0; j < namedStyleList.size(); ++j )
          {
            QDomNodeList styleNameList = namedStyleList.item( j ).toElement().elementsByTagName/*NS*/( /*mSLDNamespace,*/ "Name" );
            if ( styleNameList.size() < 1 )
            {
              continue; //a layer name is mandatory
            }
            QString styleName = styleNameList.item( 0 ).toElement().text();
            QgsDebugMsg( "styleName is: " + styleName );
            layers.push_back( layerName );
            styles.push_back( styleName );
          }

          //named layers can also have User Styles
          QDomNodeList userStyleList = currentLayerElement.elementsByTagName/*NS*/( /*mSLDNamespace,*/ "UserStyle" );
          for ( int j = 0; j < userStyleList.size(); ++j )
          {
            QDomNodeList styleNameList = userStyleList.item( j ).toElement().elementsByTagName/*NS*/( /*mSLDNamespace,*/ "Name" );
            if ( styleNameList.size() < 1 )
            {
              continue; //a layer name is mandatory
            }
            QString styleName = styleNameList.item( 0 ).toElement().text();
            QgsDebugMsg( "styleName is: " + styleName );
            layers.push_back( layerName );
            styles.push_back( styleName );
          }
        }
        else if ( currentLayerElement.localName() == "UserLayer" )
        {
          QgsDebugMsg( "Found a UserLayer" );
          //layer name
          QDomNodeList nameList = currentLayerElement.elementsByTagName/*NS*/( /*mSLDNamespace,*/ "Name" );
          if ( nameList.length() < 1 )
          {
            QgsDebugMsg( "Namelist size is <1" );
            continue; //a layer name is mandatory
          }
          QString layerName = nameList.item( 0 ).toElement().text();
          QgsDebugMsg( "layerName is: " + layerName );
          //find the User Styles and the corresponding names
          QDomNodeList userStyleList = currentLayerElement.elementsByTagName/*NS*/( /*mSLDNamespace,*/ "UserStyle" );
          for ( int j = 0; j < userStyleList.size(); ++j )
          {
            QDomNodeList styleNameList = userStyleList.item( j ).toElement().elementsByTagName/*NS*/( /*mSLDNamespace,*/ "Name" );
            if ( styleNameList.size() < 1 )
            {
              QgsDebugMsg( "Namelist size is <1" );
              continue;
            }

            QString styleName = styleNameList.item( 0 ).toElement().text();
            QgsDebugMsg( "styleName is: " + styleName );
            layers.push_back( layerName );
            styles.push_back( styleName );
          }
        }
      }
    }
  }
  else
  {
    return 1;
  }
  return 0;
}

QDomDocument QgsSLDConfigParser::getStyle( const QString& styleName, const QString& layerName ) const
{
  QDomElement userLayerElement = findUserLayerElement( layerName );

  if ( userLayerElement.isNull() )
  {
    throw QgsMapServiceException( "LayerNotDefined", "Operation request is for a Layer not offered by the server." );
  }

  QDomElement userStyleElement = findUserStyleElement( userLayerElement, styleName );

  if ( userStyleElement.isNull() )
  {
    throw QgsMapServiceException( "StyleNotDefined", "Operation request references a Style not offered by the server." );
  }

  QDomDocument styleDoc;
  styleDoc.appendChild( styleDoc.importNode( userStyleElement, true ) );
  return styleDoc;
}

QDomDocument QgsSLDConfigParser::getStyles( QStringList& layerList ) const
{
  QDomDocument styleDoc;
  for ( int i = 0; i < layerList.size(); i++ )
  {
    QString layerName;
    layerName = layerList.at( i );
    QDomElement userLayerElement = findUserLayerElement( layerName );
    if ( userLayerElement.isNull() )
    {
      throw QgsMapServiceException( "LayerNotDefined", "Operation request is for a Layer not offered by the server." );
    }
    QDomNodeList userStyleList = userLayerElement.elementsByTagName( "UserStyle" );
    for ( int j = 0; j < userStyleList.size(); j++ )
    {
      QDomElement userStyleElement = userStyleList.item( i ).toElement();
      styleDoc.appendChild( styleDoc.importNode( userStyleElement, true ) );
    }
  }
  return styleDoc;
}

QDomDocument QgsSLDConfigParser::describeLayer( QStringList& layerList, const QString& hrefString ) const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->describeLayer( layerList, hrefString );
  }
  return QDomDocument();
}

QgsMapRenderer::OutputUnits QgsSLDConfigParser::outputUnits() const
{
  return mOutputUnits;
}

QStringList QgsSLDConfigParser::identifyDisabledLayers() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->identifyDisabledLayers();
  }
  return QStringList();
}

bool QgsSLDConfigParser::featureInfoWithWktGeometry() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->featureInfoWithWktGeometry();
  }
  return false;
}

bool QgsSLDConfigParser::segmentizeFeatureInfoWktGeometry() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->segmentizeFeatureInfoWktGeometry();
  }
  return false;
}


QHash<QString, QString> QgsSLDConfigParser::featureInfoLayerAliasMap() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->featureInfoLayerAliasMap();
  }
  return QHash<QString, QString>();
}

QString QgsSLDConfigParser::featureInfoDocumentElement( const QString& defaultValue ) const
{
  return defaultValue;
}

QString QgsSLDConfigParser::featureInfoDocumentElementNS() const
{
  return QString();
}

QString QgsSLDConfigParser::featureInfoSchema() const
{
  return QString();
}

bool QgsSLDConfigParser::featureInfoFormatSIA2045() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->featureInfoFormatSIA2045();
  }
  return false;
}

void QgsSLDConfigParser::drawOverlays( QPainter* p, int dpi, int width, int height ) const
{
  if ( mFallbackParser )
  {
    mFallbackParser->drawOverlays( p, dpi, width, height );
  }
}

void QgsSLDConfigParser::loadLabelSettings( QgsLabelingEngineInterface * lbl ) const
{
  if ( mFallbackParser )
  {
    mFallbackParser->loadLabelSettings( lbl );
  }
}

QString QgsSLDConfigParser::serviceUrl() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->serviceUrl();
  }
  return QString();
}

QStringList QgsSLDConfigParser::wfsLayerNames() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->wfsLayerNames();
  }
  return QStringList();
}

void QgsSLDConfigParser::owsGeneralAndResourceList( QDomElement& parentElement, QDomDocument& doc, const QString& strHref ) const
{
  if ( mFallbackParser )
  {
    mFallbackParser->owsGeneralAndResourceList( parentElement, doc, strHref );
  }
}

double QgsSLDConfigParser::legendBoxSpace() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendBoxSpace();
  }
  return 0;
}

double QgsSLDConfigParser::legendLayerSpace() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendLayerSpace();
  }
  return 0;
}

double QgsSLDConfigParser::legendLayerTitleSpace() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendLayerTitleSpace();
  }
  return 0;
}

double QgsSLDConfigParser::legendSymbolSpace() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendSymbolSpace();
  }
  return 0;
}

double QgsSLDConfigParser::legendIconLabelSpace() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendIconLabelSpace();
  }
  return 0;
}

double QgsSLDConfigParser::legendSymbolWidth() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendSymbolWidth();
  }
  return 0;
}

double QgsSLDConfigParser::legendSymbolHeight() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendSymbolHeight();
  }
  return 0;
}

const QFont& QgsSLDConfigParser::legendLayerFont() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendLayerFont();
  }
  return mLegendLayerFont;
}

const QFont& QgsSLDConfigParser::legendItemFont() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->legendItemFont();
  }
  return mLegendItemFont;
}

double QgsSLDConfigParser::maxWidth() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->maxWidth();
  }
  return -1;
}

double QgsSLDConfigParser::maxHeight() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->maxHeight();
  }
  return -1;
}

double QgsSLDConfigParser::imageQuality() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->imageQuality();
  }
  return -1;
}

int QgsSLDConfigParser::wmsPrecision() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->wmsPrecision();
  }
  return -1;
}

bool QgsSLDConfigParser::wmsInspireActivated() const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->wmsInspireActivated();
  }
  return false;
}

QgsComposition* QgsSLDConfigParser::createPrintComposition( const QString& composerTemplate, QgsMapRenderer* mapRenderer, const QMap< QString, QString >& parameterMap, QStringList& highlightLayers ) const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->createPrintComposition( composerTemplate, mapRenderer, parameterMap, highlightLayers );
  }
  return nullptr;
}

QgsComposition* QgsSLDConfigParser::initComposition( const QString& composerTemplate, QgsMapRenderer* mapRenderer, QList< QgsComposerMap*>& mapList, QList< QgsComposerLegend* >& legendList, QList< QgsComposerLabel* >& labelList, QList<const QgsComposerHtml *>& htmlFrameList ) const
{
  if ( mFallbackParser )
  {
    return mFallbackParser->initComposition( composerTemplate, mapRenderer, mapList, legendList, labelList, htmlFrameList );
  }
  return nullptr;
}

void QgsSLDConfigParser::printCapabilities( QDomElement& parentElement, QDomDocument& doc ) const
{
  if ( mFallbackParser )
  {
    mFallbackParser->printCapabilities( parentElement, doc );
  }
}

void QgsSLDConfigParser::inspireCapabilities( QDomElement& parentElement, QDomDocument& doc ) const
{
  if ( mFallbackParser )
  {
    mFallbackParser->inspireCapabilities( parentElement, doc );
  }
}

void QgsSLDConfigParser::setScaleDenominator( double )
{
  //soon...
}

void QgsSLDConfigParser::addExternalGMLData( const QString &, QDomDocument * )
{
  //soon...
}

QList< QPair< QString, QgsLayerCoordinateTransform > > QgsSLDConfigParser::layerCoordinateTransforms() const
{
  return QList< QPair< QString, QgsLayerCoordinateTransform > >();
}

int QgsSLDConfigParser::nLayers() const
{
  if ( mXMLDoc )
  {
    QDomNode sldNode = mXMLDoc->documentElement();
    if ( !sldNode.isNull() )
    {
      QDomNodeList layerNodeList = sldNode.toElement().elementsByTagName( "UserLayer" );
      return layerNodeList.size();
    }
  }
  return 0;
}

void QgsSLDConfigParser::serviceCapabilities( QDomElement& parentElement, QDomDocument& doc ) const
{
  if ( mFallbackParser )
  {
    mFallbackParser->serviceCapabilities( parentElement, doc );
  }
  else
  {
    QgsConfigParserUtils::fallbackServiceCapabilities( parentElement, doc );
  }
}

QList<QDomElement> QgsSLDConfigParser::findNamedLayerElements( const QString& layerName ) const
{
  QList<QDomElement> resultList;
  if ( mXMLDoc )
  {
    QDomElement sldElement = mXMLDoc->documentElement();
    if ( !sldElement.isNull() )
    {
      QDomNodeList NamedLayerList = sldElement.elementsByTagName( "NamedLayer" );
      for ( int i = 0; i < NamedLayerList.size(); ++i )
      {
        QDomNodeList nameList = NamedLayerList.item( i ).toElement().elementsByTagName( "Name" );
        if ( !nameList.isEmpty() )
        {
          if ( nameList.item( 0 ).toElement().text() == layerName )
          {
            resultList.push_back( NamedLayerList.item( i ).toElement() );
          }
        }
      }
    }
  }
  return resultList;
}

QDomElement QgsSLDConfigParser::findUserStyleElement( const QDomElement& userLayerElement, const QString& styleName ) const
{
  QDomElement defaultResult;
  if ( !userLayerElement.isNull() )
  {
    QDomNodeList userStyleList = userLayerElement.elementsByTagName( "UserStyle" );
    for ( int i = 0; i < userStyleList.size(); ++i )
    {
      QDomNodeList nameList = userStyleList.item( i ).toElement().elementsByTagName( "Name" );
      if ( !nameList.isEmpty() )
      {
        if ( nameList.item( 0 ).toElement().text() == styleName )
        {
          return userStyleList.item( i ).toElement();
        }
      }
    }
  }
  return defaultResult;
}

QDomElement QgsSLDConfigParser::findNamedStyleElement( const QDomElement& layerElement, const QString& styleName ) const
{
  QDomElement defaultResult;
  if ( !layerElement.isNull() )
  {
    QDomNodeList styleList = layerElement.elementsByTagName( "NamedStyle" );
    for ( int i = 0; i < styleList.size(); ++i )
    {
      QDomNodeList nameList = styleList.item( i ).toElement().elementsByTagName( "Name" );
      if ( !nameList.isEmpty() )
      {
        if ( nameList.item( 0 ).toElement().text() == styleName )
        {
          return styleList.item( i ).toElement();
        }
      }
    }
  }
  return defaultResult;
}

QgsFeatureRenderer* QgsSLDConfigParser::rendererFromUserStyle( const QDomElement& userStyleElement, QgsVectorLayer* vec ) const
{
  if ( !vec || userStyleElement.isNull() )
  {
    return nullptr;
  }

  QgsDebugMsg( "Entering" );

  QString errorMessage;
  QgsFeatureRenderer* renderer = QgsFeatureRenderer::loadSld( userStyleElement.parentNode(), vec->geometryType(), errorMessage );
  if ( !renderer )
  {
    throw QgsMapServiceException( "SLD error", errorMessage );
  }
  return renderer;
}

bool QgsSLDConfigParser::rasterSymbologyFromUserStyle( const QDomElement& userStyleElement, QgsRasterLayer* r ) const
{
  return false;
#if 0 //needs to be fixed
  QgsDebugMsg( "Entering" );
  if ( !r )
  {
    return false;
  }

  //search raster symbolizer
  QDomNodeList rasterSymbolizerList = userStyleElement.elementsByTagName( "RasterSymbolizer" );
  if ( rasterSymbolizerList.size() < 1 )
  {
    return false;
  }

  QDomElement rasterSymbolizerElem = rasterSymbolizerList.at( 0 ).toElement();

  //search colormap and entries
  QDomNodeList colorMapList = rasterSymbolizerElem.elementsByTagName( "ColorMap" );
  if ( colorMapList.size() < 1 )
  {
    return false;
  }

  QDomElement colorMapElem = colorMapList.at( 0 ).toElement();

  //search for color map entries
  r->setColorShadingAlgorithm( QgsRasterLayer::ColorRampShader );
  QgsColorRampShader* myRasterShaderFunction = ( QgsColorRampShader* )r->rasterShader()->rasterShaderFunction();
  QList<QgsColorRampShader::ColorRampItem> colorRampItems;

  QDomNodeList colorMapEntryList = colorMapElem.elementsByTagName( "ColorMapEntry" );
  QDomElement currentColorMapEntryElem;
  bool conversion;
  int red, green, blue;

  for ( int i = 0; i < colorMapEntryList.size(); ++i )
  {
    currentColorMapEntryElem = colorMapEntryList.at( i ).toElement();
    QgsColorRampShader::ColorRampItem myNewColorRampItem;
    QString color = currentColorMapEntryElem.attribute( "color" );
    if ( color.length() != 7 ) //color string must be in the form #ffffff
    {
      continue;
    }
    red = color.mid( 1, 2 ).toInt( &conversion, 16 );
    if ( !conversion )
    {
      red = 0;
    }
    green = color.mid( 3, 2 ).toInt( &conversion, 16 );
    if ( !conversion )
    {
      green = 0;
    }
    blue = color.mid( 5, 2 ).toInt( &conversion, 16 );
    if ( !conversion )
    {
      blue = 0;
    }
    myNewColorRampItem.color = QColor( red, green, blue );
    QString value = currentColorMapEntryElem.attribute( "quantity" );
    myNewColorRampItem.value = value.toDouble();
    QgsDebugMsg( "Adding colormap entry" );
    colorRampItems.push_back( myNewColorRampItem );
  }

  myRasterShaderFunction->setColorRampItemList( colorRampItems );

  //linear interpolation or discrete classification
  QString interpolation = colorMapElem.attribute( "interpolation" );
  if ( interpolation == "linear" )
  {
    myRasterShaderFunction->setColorRampType( QgsColorRampShader::INTERPOLATED );
  }
  else if ( interpolation == "discrete" )
  {
    myRasterShaderFunction->setColorRampType( QgsColorRampShader::DISCRETE );
  }

  //QgsDebugMsg("Setting drawing style");
  r->setDrawingStyle( QgsRasterLayer::SingleBandPseudoColor );

  //set pseudo color mode
  return true;
#else
  Q_UNUSED( userStyleElement );
  Q_UNUSED( r );
#endif //0
}

bool QgsSLDConfigParser::labelSettingsFromUserStyle( const QDomElement& userStyleElement, QgsVectorLayer* vec ) const
{
  // TODO create rule based labeling from user's SLD
  // (there is QgsVectorLayer::readSldLabeling() that does something very similar)
  Q_UNUSED( userStyleElement );
  Q_UNUSED( vec );
  return false;
}

QgsVectorLayer* QgsSLDConfigParser::contourLayerFromRaster( const QDomElement& userStyleElem, QgsRasterLayer* rasterLayer ) const
{
  QgsDebugMsg( "Entering." );

  if ( !rasterLayer )
  {
    return nullptr;
  }

  //get <ContourSymbolizer> element
  QDomNodeList contourNodeList = userStyleElem.elementsByTagName( "ContourSymbolizer" );
  if ( contourNodeList.size() < 1 )
  {
    return nullptr;
  }

  QDomElement contourSymbolizerElem = contourNodeList.item( 0 ).toElement();
  if ( contourSymbolizerElem.isNull() )
  {
    return nullptr;
  }

  double equidistance, minValue, maxValue, offset;
  QString propertyName;

  equidistance = contourSymbolizerElem.attribute( "equidistance" ).toDouble();
  minValue = contourSymbolizerElem.attribute( "minValue" ).toDouble();
  maxValue = contourSymbolizerElem.attribute( "maxValue" ).toDouble();
  offset = contourSymbolizerElem.attribute( "offset" ).toDouble();
  propertyName = contourSymbolizerElem.attribute( "propertyName" );

  if ( equidistance <= 0.0 )
  {
    return nullptr;
  }

  QTemporaryFile* tmpFile1 = new QTemporaryFile();
  tmpFile1->open();
  mFilesToRemove.push_back( tmpFile1 );
  QString tmpBaseName = tmpFile1->fileName();
  QString tmpFileName = tmpBaseName + ".shp";

  //hack: use gdal_contour first to write into a temporary file
  /* todo: use GDALContourGenerate( hBand, dfInterval, dfOffset,
                                nFixedLevelCount, adfFixedLevels,
                                bNoDataSet, dfNoData,
                                hLayer, 0, nElevField,
                                GDALTermProgress, nullptr );*/


  //do the stuff that is also done in the main method of gdal_contour...
  /* -------------------------------------------------------------------- */
  /*      Open source raster file.                                        */
  /* -------------------------------------------------------------------- */
  GDALRasterBandH hBand;
  GDALDatasetH hSrcDS;

  int numberOfLevels = 0;
  double currentLevel = 0.0;

  if ( maxValue > minValue )
  {
    //find first level
    currentLevel = ( int )(( minValue - offset ) / equidistance + 0.5 ) * equidistance + offset;
    while ( currentLevel <= maxValue )
    {
      ++numberOfLevels;
      currentLevel += equidistance;
    }
  }

  double *adfFixedLevels = new double[numberOfLevels];
  int    nFixedLevelCount = numberOfLevels;
  currentLevel = ( int )(( minValue - offset ) / equidistance + 0.5 ) * equidistance + offset;
  for ( int i = 0; i < numberOfLevels; ++i )
  {
    adfFixedLevels[i] = currentLevel;
    currentLevel += equidistance;
  }
  int nBandIn = 1;
  double dfInterval = equidistance, dfNoData = 0.0, dfOffset = offset;

  int /* b3D = FALSE, */ bNoDataSet = FALSE, bIgnoreNoData = FALSE;

  hSrcDS = GDALOpen( TO8( rasterLayer->source() ), GA_ReadOnly );
  if ( !hSrcDS )
  {
    delete [] adfFixedLevels;
    throw QgsMapServiceException( "LayerNotDefined", "Operation request is for a file not available on the server." );
  }

  hBand = GDALGetRasterBand( hSrcDS, nBandIn );
  if ( !hBand )
  {
    CPLError( CE_Failure, CPLE_AppDefined,
              "Band %d does not exist on dataset.",
              nBandIn );
  }

  if ( !bNoDataSet && !bIgnoreNoData )
    dfNoData = GDALGetRasterNoDataValue( hBand, &bNoDataSet );

  /* -------------------------------------------------------------------- */
  /*      Try to get a coordinate system from the raster.                 */
  /* -------------------------------------------------------------------- */
  OGRSpatialReferenceH hSRS = nullptr;

  const char *pszWKT = GDALGetProjectionRef( hBand );

  if ( pszWKT && strlen( pszWKT ) != 0 )
    hSRS = OSRNewSpatialReference( pszWKT );

  /* -------------------------------------------------------------------- */
  /*      Create the outputfile.                                          */
  /* -------------------------------------------------------------------- */
  OGRDataSourceH hDS;
  OGRSFDriverH hDriver = OGRGetDriverByName( "ESRI Shapefile" );
  OGRFieldDefnH hFld;
  OGRLayerH hLayer;
  int nElevField = -1;

  if ( !hDriver )
  {
    //fprintf( FCGI_stderr, "Unable to find format driver named 'ESRI Shapefile'.\n" );
    delete [] adfFixedLevels;
    throw QgsMapServiceException( "LayerNotDefined", "Operation request is for a file not available on the server." );
  }

  hDS = OGR_Dr_CreateDataSource( hDriver, TO8( tmpFileName ), nullptr );
  if ( !hDS )
  {
    delete [] adfFixedLevels;
    throw QgsMapServiceException( "LayerNotDefined", "Operation request cannot create data source." );
  }

  hLayer = OGR_DS_CreateLayer( hDS, "contour", hSRS,
                               /* b3D ? wkbLineString25D : */ wkbLineString,
                               nullptr );
  if ( !hLayer )
  {
    delete [] adfFixedLevels;
    throw QgsMapServiceException( "LayerNotDefined", "Operation request could not create contour file." );
  }

  hFld = OGR_Fld_Create( "ID", OFTInteger );
  OGR_Fld_SetWidth( hFld, 8 );
  OGR_L_CreateField( hLayer, hFld, FALSE );
  OGR_Fld_Destroy( hFld );

  if ( !propertyName.isEmpty() )
  {
    hFld = OGR_Fld_Create( TO8( propertyName ), OFTReal );
    OGR_Fld_SetWidth( hFld, 12 );
    OGR_Fld_SetPrecision( hFld, 3 );
    OGR_L_CreateField( hLayer, hFld, FALSE );
    OGR_Fld_Destroy( hFld );
    nElevField = 1;
  }

  /* -------------------------------------------------------------------- */
  /*      Invoke.                                                         */
  /* -------------------------------------------------------------------- */
  GDALContourGenerate( hBand, dfInterval, dfOffset,
                       nFixedLevelCount, adfFixedLevels,
                       bNoDataSet, dfNoData,
                       hLayer, 0, nElevField,
                       GDALTermProgress, nullptr );

  delete [] adfFixedLevels;

  OGR_DS_Destroy( hDS );
  GDALClose( hSrcDS );

  //todo: store those three files elsewhere...
  //mark shp, dbf and shx to delete after the request
  mFilePathsToRemove.push_back( tmpBaseName + ".shp" );
  mFilePathsToRemove.push_back( tmpBaseName + ".dbf" );
  mFilePathsToRemove.push_back( tmpBaseName + ".shx" );

  QgsVectorLayer* contourLayer = new QgsVectorLayer( tmpFileName, "layer", "ogr" );

  //create renderer
  QgsFeatureRenderer* theRenderer = rendererFromUserStyle( userStyleElem, contourLayer );
  contourLayer->setRenderer( theRenderer );

  //add labelling if requested
  labelSettingsFromUserStyle( userStyleElem, contourLayer );

  QgsDebugMsg( "Returning the contour layer" );
  return contourLayer;
}

QDomElement QgsSLDConfigParser::findUserLayerElement( const QString& layerName ) const
{
  QDomElement defaultResult;
  if ( mXMLDoc )
  {
    QDomElement sldElement = mXMLDoc->documentElement();
    if ( !sldElement.isNull() )
    {
      QDomNodeList UserLayerList = sldElement.elementsByTagName( "UserLayer" );
      for ( int i = 0; i < UserLayerList.size(); ++i )
      {
        QDomNodeList nameList = UserLayerList.item( i ).toElement().elementsByTagName( "Name" );
        if ( !nameList.isEmpty() )
        {
          if ( nameList.item( 0 ).toElement().text() == layerName )
          {
            return UserLayerList.item( i ).toElement();
          }
        }
      }
    }
  }
  return defaultResult;
}

QgsMapLayer* QgsSLDConfigParser::mapLayerFromUserLayer( const QDomElement& userLayerElem, const QString& layerName, bool allowCaching ) const
{
  QgsDebugMsg( "Entering." );
  QgsMSLayerBuilder* layerBuilder = nullptr;
  QDomElement builderRootElement;

  //hosted vector data?
  QDomNode hostedVDSNode = userLayerElem.namedItem( "HostedVDS" );
  if ( !hostedVDSNode.isNull() )
  {
    builderRootElement = hostedVDSNode.toElement();
    layerBuilder = new QgsHostedVDSBuilder();
  }

  //hosted raster data?
  QDomNode hostedRDSNode = userLayerElem.namedItem( "HostedRDS" );
  if ( !layerBuilder && !hostedRDSNode.isNull() )
  {
    builderRootElement = hostedRDSNode.toElement();
    layerBuilder = new QgsHostedRDSBuilder();
  }

  //remote OWS (WMS, WFS, WCS)?
  QDomNode remoteOWSNode = userLayerElem.namedItem( "RemoteOWS" );
  if ( !layerBuilder && !remoteOWSNode.isNull() )
  {
    builderRootElement = remoteOWSNode.toElement();
    layerBuilder = new QgsRemoteOWSBuilder( mParameterMap );
  }

  //remote vector/raster datasource
  QDomNode remoteRDSNode = userLayerElem.namedItem( "RemoteRDS" );
  if ( !layerBuilder && !remoteRDSNode.isNull() )
  {
    builderRootElement = remoteRDSNode.toElement();
    layerBuilder = new QgsRemoteDataSourceBuilder();
    QgsDebugMsg( "Detected remote raster datasource" );
  }

  QDomNode remoteVDSNode = userLayerElem.namedItem( "RemoteVDS" );
  if ( !layerBuilder && !remoteVDSNode.isNull() )
  {
    builderRootElement = remoteVDSNode.toElement();
    layerBuilder = new QgsRemoteDataSourceBuilder();
    QgsDebugMsg( "Detected remote vector datasource" );
  }

  //sent vector/raster datasource
  QDomNode sentVDSNode = userLayerElem.namedItem( "SentVDS" );
  if ( !layerBuilder && !sentVDSNode.isNull() )
  {
    builderRootElement = sentVDSNode.toElement();
    layerBuilder = new QgsSentDataSourceBuilder();
  }

  QDomNode sentRDSNode = userLayerElem.namedItem( "SentRDS" );
  if ( !layerBuilder && !sentRDSNode.isNull() )
  {
    builderRootElement = sentRDSNode.toElement();
    layerBuilder = new QgsSentDataSourceBuilder();
  }

  if ( !layerBuilder )
  {
    return nullptr;
  }

  QgsMapLayer* theMapLayer = layerBuilder->createMapLayer( builderRootElement, layerName, mFilesToRemove, mLayersToRemove, allowCaching );
  if ( theMapLayer )
  {
    setCrsForLayer( builderRootElement, theMapLayer ); //consider attributes "epsg" and "proj"
  }

  //maybe the datasource is defined in the fallback SLD?
  if ( !theMapLayer && mFallbackParser )
  {
    QList<QgsMapLayer*> fallbackList = mFallbackParser->mapLayerFromStyle( layerName, "", allowCaching );
    if ( !fallbackList.isEmpty() )
    {
      QgsMapLayer* fallbackLayer = fallbackList.at( 0 ); //todo: prevent crash if layer list is empty
      if ( fallbackLayer )
      {
        theMapLayer = dynamic_cast<QgsVectorLayer*>( fallbackLayer );
      }
    }
  }

#if 0 //todo: fixme
  //GML from outside the SLD?
  if ( !theMapLayer )
  {
    QMap<QString, QDomDocument*>::const_iterator gmlIt = mExternalGMLDatasets.find( layerName );

    if ( gmlIt != mExternalGMLDatasets.end() )
    {
      QgsDebugMsg( "Trying to get maplayer from external GML" );
      theMapLayer = vectorLayerFromGML( gmlIt.value()->documentElement() );
    }
  }
#endif //0

  //raster layer from interpolation

  QDomNode rasterInterpolationNode = userLayerElem.namedItem( "RasterInterpolation" );
  if ( !rasterInterpolationNode.isNull() )
  {
    QgsVectorLayer* vectorCast = dynamic_cast<QgsVectorLayer*>( theMapLayer );
    if ( vectorCast )
    {
      builderRootElement = rasterInterpolationNode.toElement();
      layerBuilder = new QgsInterpolationLayerBuilder( vectorCast );
      theMapLayer = layerBuilder->createMapLayer( builderRootElement, layerName, mFilesToRemove, mLayersToRemove, allowCaching );
    }
  }

  return theMapLayer;
}

void QgsSLDConfigParser::setCrsForLayer( const QDomElement& layerElem, QgsMapLayer* ml ) const
{
  //create CRS if specified as attribute ("epsg" or "proj")
  QString epsg = layerElem.attribute( "epsg", "" );
  if ( !epsg.isEmpty() )
  {
    bool conversionOk;
    int epsgnr = epsg.toInt( &conversionOk );
    if ( conversionOk )
    {
      //set spatial ref sys
      QgsCoordinateReferenceSystem srs = QgsCoordinateReferenceSystem::fromOgcWmsCrs( QString( "EPSG:%1" ).arg( epsgnr ) );
      ml->setCrs( srs );
    }
  }
  else
  {
    QString projString = layerElem.attribute( "proj", "" );
    if ( !projString.isEmpty() )
    {
      QgsCoordinateReferenceSystem srs = QgsCoordinateReferenceSystem::fromProj4( projString );
      //TODO: createFromProj4 used to save to the user database any new CRS
      // this behavior was changed in order to separate creation and saving.
      // Not sure if it necessary to save it here, should be checked by someone
      // familiar with the code (should also give a more descriptive name to the generated CRS)
      if ( srs.srsid() == 0 )
      {
        QString myName = QString( " * %1 (%2)" )
                         .arg( QObject::tr( "Generated CRS", "A CRS automatically generated from layer info get this prefix for description" ),
                               srs.toProj4() );
        srs.saveAsUserCrs( myName );
      }

      ml->setCrs( srs );
    }
  }
}






