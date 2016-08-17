/***************************************************************************
    qgsrelation.cpp
     --------------------------------------
    Date                 : 29.4.2013
    Copyright            : (C) 2013 Matthias Kuhn
    Email                : matthias at opengis dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsrelation.h"

#include "qgsapplication.h"
#include "qgsfeatureiterator.h"
#include "qgslogger.h"
#include "qgsmaplayerregistry.h"
#include "qgsvectorlayer.h"

QgsRelation::QgsRelation()
    : mReferencingLayer( nullptr )
    , mReferencedLayer( nullptr )
    , mValid( false )
{
}

QgsRelation QgsRelation::createFromXml( const QDomNode &node )
{
  QDomElement elem = node.toElement();

  if ( elem.tagName() != "relation" )
  {
    QgsLogger::warning( QApplication::translate( "QgsRelation", "Cannot create relation. Unexpected tag '%1'" ).arg( elem.tagName() ) );
  }

  QgsRelation relation;

  QString referencingLayerId = elem.attribute( "referencingLayer" );
  QString referencedLayerId = elem.attribute( "referencedLayer" );
  QString id = elem.attribute( "id" );
  QString name = elem.attribute( "name" );

  const QMap<QString, QgsMapLayer*>& mapLayers = QgsMapLayerRegistry::instance()->mapLayers();

  QgsMapLayer* referencingLayer = mapLayers[referencingLayerId];
  QgsMapLayer* referencedLayer = mapLayers[referencedLayerId];

  if ( !referencingLayer )
  {
    QgsLogger::warning( QApplication::translate( "QgsRelation", "Relation defined for layer '%1' which does not exist." ).arg( referencingLayerId ) );
  }
  else if ( QgsMapLayer::VectorLayer  != referencingLayer->type() )
  {
    QgsLogger::warning( QApplication::translate( "QgsRelation", "Relation defined for layer '%1' which is not of type VectorLayer." ).arg( referencingLayerId ) );
  }

  if ( !referencedLayer )
  {
    QgsLogger::warning( QApplication::translate( "QgsRelation", "Relation defined for layer '%1' which does not exist." ).arg( referencedLayerId ) );
  }
  else if ( QgsMapLayer::VectorLayer  != referencedLayer->type() )
  {
    QgsLogger::warning( QApplication::translate( "QgsRelation", "Relation defined for layer '%1' which is not of type VectorLayer." ).arg( referencedLayerId ) );
  }

  relation.mReferencingLayerId = referencingLayerId;
  relation.mReferencingLayer = qobject_cast<QgsVectorLayer*>( referencingLayer );
  relation.mReferencedLayerId = referencedLayerId;
  relation.mReferencedLayer = qobject_cast<QgsVectorLayer*>( referencedLayer );
  relation.mRelationId = id;
  relation.mRelationName = name;

  QDomNodeList references = elem.elementsByTagName( "fieldRef" );
  for ( int i = 0; i < references.size(); ++i )
  {
    QDomElement refEl = references.at( i ).toElement();

    QString referencingField = refEl.attribute( "referencingField" );
    QString referencedField = refEl.attribute( "referencedField" );

    relation.addFieldPair( referencingField, referencedField );
  }

  relation.updateRelationStatus();

  return relation;
}

void QgsRelation::writeXml( QDomNode &node, QDomDocument &doc ) const
{
  QDomElement elem = doc.createElement( "relation" );
  elem.setAttribute( "id", mRelationId );
  elem.setAttribute( "name", mRelationName );
  elem.setAttribute( "referencingLayer", mReferencingLayerId );
  elem.setAttribute( "referencedLayer", mReferencedLayerId );

  Q_FOREACH ( const FieldPair& fields, mFieldPairs )
  {
    QDomElement referenceElem = doc.createElement( "fieldRef" );
    referenceElem.setAttribute( "referencingField", fields.first );
    referenceElem.setAttribute( "referencedField", fields.second );
    elem.appendChild( referenceElem );
  }

  node.appendChild( elem );
}

void QgsRelation::setRelationId( const QString& id )
{
  mRelationId = id;

  updateRelationStatus();
}

void QgsRelation::setRelationName( const QString& name )
{
  mRelationName = name;
}

void QgsRelation::setReferencingLayer( const QString& id )
{
  mReferencingLayerId = id;

  updateRelationStatus();
}

void QgsRelation::setReferencedLayer( const QString& id )
{
  mReferencedLayerId = id;

  updateRelationStatus();
}

void QgsRelation::addFieldPair( const QString& referencingField, const QString& referencedField )
{
  mFieldPairs << FieldPair( referencingField, referencedField );
  updateRelationStatus();
}

void QgsRelation::addFieldPair( const FieldPair& fieldPair )
{
  mFieldPairs << fieldPair;
  updateRelationStatus();
}

QgsFeatureIterator QgsRelation::getRelatedFeatures( const QgsFeature& feature ) const
{
  return referencingLayer()->getFeatures( getRelatedFeaturesRequest( feature ) );
}

QgsFeatureRequest QgsRelation::getRelatedFeaturesRequest( const QgsFeature& feature ) const
{
  QString filter = getRelatedFeaturesFilter( feature );
  QgsDebugMsg( QString( "Filter conditions: '%1'" ).arg( filter ) );

  QgsFeatureRequest myRequest;
  myRequest.setFilterExpression( filter );
  return myRequest;
}

QString QgsRelation::getRelatedFeaturesFilter( const QgsFeature& feature ) const
{
  QStringList conditions;

  Q_FOREACH ( const QgsRelation::FieldPair& fieldPair, mFieldPairs )
  {
    int referencingIdx = referencingLayer()->fields().indexFromName( fieldPair.referencingField() );
    QgsField referencingField = referencingLayer()->fields().at( referencingIdx );

    QVariant val( feature.attribute( fieldPair.referencedField() ) );

    if ( val.isNull() )
    {
      conditions << QString( "\"%1\" IS NULL" ).arg( fieldPair.referencingField() );
    }
    else if ( referencingField.type() == QVariant::String )
    {
      // Use quotes
      conditions << QString( "\"%1\" = '%2'" ).arg( fieldPair.referencingField(), val.toString() );
    }
    else
    {
      // No quotes
      conditions << QString( "\"%1\" = %2" ).arg( fieldPair.referencingField(), val.toString() );
    }
  }

  return conditions.join( " AND " );
}

QgsFeatureRequest QgsRelation::getReferencedFeatureRequest( const QgsAttributes& attributes ) const
{
  QStringList conditions;

  Q_FOREACH ( const QgsRelation::FieldPair& fieldPair, mFieldPairs )
  {
    int referencedIdx = referencedLayer()->fields().indexFromName( fieldPair.referencedField() );
    int referencingIdx = referencingLayer()->fields().indexFromName( fieldPair.referencingField() );

    QgsField referencedField = referencedLayer()->fields().at( referencedIdx );

    if ( referencedField.type() == QVariant::String )
    {
      // Use quotes
      conditions << QString( "\"%1\" = '%2'" ).arg( fieldPair.referencedField(), attributes.at( referencingIdx ).toString() );
    }
    else
    {
      // No quotes
      conditions << QString( "\"%1\" = %2" ).arg( fieldPair.referencedField(), attributes.at( referencingIdx ).toString() );
    }
  }

  QgsFeatureRequest myRequest;

  QgsDebugMsg( QString( "Filter conditions: '%1'" ).arg( conditions.join( " AND " ) ) );

  myRequest.setFilterExpression( conditions.join( " AND " ) );

  return myRequest;
}

QgsFeatureRequest QgsRelation::getReferencedFeatureRequest( const QgsFeature& feature ) const
{
  return getReferencedFeatureRequest( feature.attributes() );
}

QgsFeature QgsRelation::getReferencedFeature( const QgsFeature& feature ) const
{
  QgsFeatureRequest request = getReferencedFeatureRequest( feature );

  QgsFeature f;
  mReferencedLayer->getFeatures( request ).nextFeature( f );
  return f;
}

QString QgsRelation::name() const
{
  return mRelationName;
}

QString QgsRelation::id() const
{
  return mRelationId;
}

QString QgsRelation::referencingLayerId() const
{
  return mReferencingLayerId;
}

QgsVectorLayer* QgsRelation::referencingLayer() const
{
  return mReferencingLayer;
}

QString QgsRelation::referencedLayerId() const
{
  return mReferencedLayerId;
}

QgsVectorLayer* QgsRelation::referencedLayer() const
{
  return mReferencedLayer;
}

QList<QgsRelation::FieldPair> QgsRelation::fieldPairs() const
{
  return mFieldPairs;
}

QgsAttributeList QgsRelation::referencedFields() const
{
  QgsAttributeList attrs;

  Q_FOREACH ( const FieldPair& pair, mFieldPairs )
  {
    attrs << mReferencedLayer->fieldNameIndex( pair.second );
  }
  return attrs;
}

QgsAttributeList QgsRelation::referencingFields() const
{
  QgsAttributeList attrs;

  Q_FOREACH ( const FieldPair& pair, mFieldPairs )
  {
    attrs << mReferencingLayer->fieldNameIndex( pair.first );
  }
  return attrs;

}

bool QgsRelation::isValid() const
{
  return mValid;
}

void QgsRelation::updateRelationStatus()
{
  const QMap<QString, QgsMapLayer*>& mapLayers = QgsMapLayerRegistry::instance()->mapLayers();

  mReferencingLayer = qobject_cast<QgsVectorLayer*>( mapLayers[mReferencingLayerId] );
  mReferencedLayer = qobject_cast<QgsVectorLayer*>( mapLayers[mReferencedLayerId] );

  mValid = true;

  if ( mRelationId.isEmpty() )
  {
    QgsDebugMsg( "Invalid relation: no ID" );
    mValid = false;
      }
  else
  {
    if ( !mReferencedLayer )
    {
      QgsDebugMsg( QString("Invalid relation: referenced layer does not exist. ID: %1").arg(mReferencedLayerId) );
      mValid = false;
    }
    else if ( !mReferencingLayer )
    {
      QgsDebugMsg( QString("Invalid relation: referencing layer does not exist. ID: %2").arg(mReferencingLayerId) );
      mValid = false;
    }
    else
    {
      if ( mFieldPairs.count() < 1 )
      {
        QgsDebugMsg( "Invalid relation: no pair of field is specified." );
        mValid = false;
      }

      Q_FOREACH ( const FieldPair& fieldPair, mFieldPairs )
      {
        if ( -1 == mReferencingLayer->fieldNameIndex( fieldPair.first ))
        {
          QgsDebugMsg( QString("Invalid relation: field %1 does not exist in referencing layer %2").arg(fieldPair.first, mReferencingLayer->name()) );
          mValid = false;
          break;
        }
        else if ( -1 == mReferencedLayer->fieldNameIndex( fieldPair.second ) )
        {
          QgsDebugMsg( QString("Invalid relation: field %1 does not exist in referencedg layer %2").arg(fieldPair.second, mReferencedLayer->name()) );
          mValid = false;
          break;
        }
      }
    }

  }
}
