/***************************************************************************
  qgsattributeeditorelement.cpp - QgsAttributeEditorElement

 ---------------------
 begin                : 18.8.2016
 copyright            : (C) 2016 by Matthias Kuhn
 email                : matthias@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgsattributeeditorelement.h"
#include "qgsrelationmanager.h"

void QgsAttributeEditorContainer::addChildElement( QgsAttributeEditorElement *widget )
{
  mChildren.append( widget );
}

void QgsAttributeEditorContainer::setName( const QString& name )
{
  mName = name;
}

QgsOptionalExpression QgsAttributeEditorContainer::visibilityExpression() const
{
  return mVisibilityExpression;
}

void QgsAttributeEditorContainer::setVisibilityExpression( const QgsOptionalExpression& visibilityExpression )
{
  if ( visibilityExpression == mVisibilityExpression )
    return;

  mVisibilityExpression = visibilityExpression;
}

QList<QgsAttributeEditorElement*> QgsAttributeEditorContainer::findElements( QgsAttributeEditorElement::AttributeEditorType type ) const
{
  QList<QgsAttributeEditorElement*> results;

  Q_FOREACH ( QgsAttributeEditorElement* elem, mChildren )
  {
    if ( elem->type() == type )
    {
      results.append( elem );
    }

    if ( elem->type() == AeTypeContainer )
    {
      QgsAttributeEditorContainer* cont = dynamic_cast<QgsAttributeEditorContainer*>( elem );
      if ( cont )
        results += cont->findElements( type );
    }
  }

  return results;
}

void QgsAttributeEditorContainer::clear()
{
  qDeleteAll( mChildren );
  mChildren.clear();
}

QgsAttributeEditorElement* QgsAttributeEditorField::clone( QgsAttributeEditorElement* parent ) const
{
  QgsAttributeEditorField* element = new QgsAttributeEditorField( name(), mIdx, parent );

  return element;
}

bool QgsAttributeEditorRelation::init( QgsRelationManager* relationManager )
{
  mRelation = relationManager->relation( mRelationId );
  return mRelation.isValid();
}

QgsAttributeEditorElement* QgsAttributeEditorRelation::clone( QgsAttributeEditorElement* parent ) const
{
  QgsAttributeEditorRelation* element = new QgsAttributeEditorRelation( name(), mRelationId, parent );
  element->mRelation = mRelation;
  element->mShowLinkButton = mShowLinkButton;
  element->mShowUnlinkButton = mShowUnlinkButton;

  return element;
}
void QgsAttributeEditorField::saveConfiguration( QDomElement &elem ) const
{
  elem.setAttribute( "index", mIdx );
}

QString QgsAttributeEditorField::typeIdentifier() const
{
  return "attributeEditorField";
}

QDomElement QgsAttributeEditorElement::toDomElement( QDomDocument& doc ) const
{
  QDomElement elem = doc.createElement( typeIdentifier() );
  elem.setAttribute( "name", mName );
  elem.setAttribute( "showLabel", mShowLabel );

  saveConfiguration( elem );
  return elem;
}

bool QgsAttributeEditorElement::showLabel() const
{
  return mShowLabel;
}

void QgsAttributeEditorElement::setShowLabel( bool showLabel )
{
  mShowLabel = showLabel;
}

void QgsAttributeEditorRelation::saveConfiguration( QDomElement& elem ) const
{
  elem.setAttribute( "relation", mRelation.id() );
  elem.setAttribute( "showLinkButton", mShowLinkButton );
  elem.setAttribute( "showUnlinkButton", mShowUnlinkButton );
}

QString QgsAttributeEditorRelation::typeIdentifier() const
{
  return "attributeEditorRelation";
}

bool QgsAttributeEditorRelation::showLinkButton() const
{
  return mShowLinkButton;
}

void QgsAttributeEditorRelation::setShowLinkButton( bool showLinkButton )
{
  mShowLinkButton = showLinkButton;
}

bool QgsAttributeEditorRelation::showUnlinkButton() const
{
  return mShowUnlinkButton;
}

void QgsAttributeEditorRelation::setShowUnlinkButton( bool showUnlinkButton )
{
  mShowUnlinkButton = showUnlinkButton;
}
