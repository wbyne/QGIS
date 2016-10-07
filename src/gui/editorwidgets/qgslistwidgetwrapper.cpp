/***************************************************************************
    qgslistwidgetwrapper.cpp
     --------------------------------------
    Date                 : 09.2016
    Copyright            : (C) 2016 Patrick Valsecchi
    Email                : patrick.valsecchi@camptocamp.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgslistwidgetwrapper.h"
#include "qgslistwidget.h"
#include "qgsattributeform.h"

QgsListWidgetWrapper::QgsListWidgetWrapper( QgsVectorLayer* vl, int fieldIdx, QWidget* editor, QWidget* parent ):
    QgsEditorWidgetWrapper( vl, fieldIdx, editor, parent ), mWidget( nullptr )
{
}

void QgsListWidgetWrapper::showIndeterminateState()
{
  mWidget->setList( QVariantList() );
}

QWidget* QgsListWidgetWrapper::createWidget( QWidget* parent )
{
  if ( isInTable( parent ) )
  {
    // if to be put in a table, draw a border and set a decent size
    QFrame* ret = new QFrame( parent );
    ret->setFrameShape( QFrame::StyledPanel );
    QHBoxLayout* layout = new QHBoxLayout( ret );
    layout->addWidget( new QgsListWidget( field().subType(), ret ) );
    ret->setMinimumSize( QSize( 320, 110 ) );
    return ret;
  }
  else
  {
    return new QgsListWidget( field().subType(), parent );
  }
}

void QgsListWidgetWrapper::initWidget( QWidget* editor )
{
  mWidget = qobject_cast<QgsListWidget*>( editor );
  if ( !mWidget )
  {
    mWidget = editor->findChild<QgsListWidget*>();
  }

  connect( mWidget, SIGNAL( valueChanged() ), this, SLOT( onValueChanged() ) );
}

bool QgsListWidgetWrapper::valid() const
{
  return mWidget ? mWidget->valid() : true;
}

void QgsListWidgetWrapper::setValue( const QVariant& value )
{
  mWidget->setList( value.toList() );
}

QVariant QgsListWidgetWrapper::value() const
{
  QVariant::Type type = field().type();
  if ( !mWidget ) return QVariant( type );
  if ( type == QVariant::StringList )
  {
    QStringList result;
    const QVariantList list = mWidget->list();
    for ( QVariantList::const_iterator it = list.constBegin(); it != list.constEnd(); ++it )
      result.append( it->toString() );
    return result;
  }
  else
    return QVariant( mWidget->list() );
}

void QgsListWidgetWrapper::onValueChanged()
{
  emit valueChanged( value() );
}

void QgsListWidgetWrapper::updateConstraintWidgetStatus( bool /*constraintValid*/ )
{
  // Nothing
}