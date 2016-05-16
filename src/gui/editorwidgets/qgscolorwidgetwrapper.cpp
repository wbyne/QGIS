/***************************************************************************
    qgscolorwidgetwrapper.cpp
     --------------------------------------
    Date                 : 5.1.2014
    Copyright            : (C) 2014 Matthias Kuhn
    Email                : matthias at opengis dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgscolorwidgetwrapper.h"
#include <QLayout>

QgsColorWidgetWrapper::QgsColorWidgetWrapper( QgsVectorLayer* vl, int fieldIdx, QWidget* editor, QWidget* parent )
    : QgsEditorWidgetWrapper( vl, fieldIdx, editor, parent )
    , mColorButton( nullptr )
{
}


QVariant QgsColorWidgetWrapper::value() const
{
  QVariant v;

  if ( mColorButton )
    v = mColorButton->color();

  return v;
}

void QgsColorWidgetWrapper::showIndeterminateState()
{
  if ( mColorButton )
  {
    whileBlocking( mColorButton )->setColor( QColor() );
  }
}

QWidget* QgsColorWidgetWrapper::createWidget( QWidget* parent )
{
  QWidget* container = new QWidget( parent );
  QHBoxLayout* layout = new QHBoxLayout();
  container->setLayout( layout );
  layout->setMargin( 0 );
  layout->setContentsMargins( 0, 0, 0, 0 );
  QgsColorButtonV2* button = new QgsColorButtonV2();
  button->setContext( QString( "editor" ) );
  layout->addWidget( button );
  layout->addStretch();
  container->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum );
  return container;
}

void QgsColorWidgetWrapper::initWidget( QWidget* editor )
{
  mColorButton = qobject_cast<QgsColorButtonV2*>( editor );
  if ( !mColorButton )
  {
    mColorButton = editor->findChild<QgsColorButtonV2*>();
  }

  connect( mColorButton, SIGNAL( colorChanged( QColor ) ), this, SLOT( valueChanged() ) );
}

bool QgsColorWidgetWrapper::valid() const
{
  return mColorButton;
}

void QgsColorWidgetWrapper::setValue( const QVariant& value )
{
  if ( mColorButton )
    mColorButton->setColor( QColor( value.toString() ) );
}
