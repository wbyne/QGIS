/***************************************************************************
    qgssymbolwidgetcontext.cpp
    --------------------------
    begin                : September 2016
    copyright            : (C) 2016 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgssymbolwidgetcontext.h"

QgsSymbolWidgetContext::QgsSymbolWidgetContext()
    : mMapCanvas( nullptr )
{}

QgsSymbolWidgetContext::QgsSymbolWidgetContext( const QgsSymbolWidgetContext& other )
    : mMapCanvas( other.mMapCanvas )
    , mAdditionalScopes( other.mAdditionalScopes )
{
  if ( other.mExpressionContext )
  {
    mExpressionContext.reset( new QgsExpressionContext( *other.mExpressionContext ) );
  }
}

QgsSymbolWidgetContext& QgsSymbolWidgetContext::operator=( const QgsSymbolWidgetContext & other )
{
  mMapCanvas = other.mMapCanvas;
  mAdditionalScopes = other.mAdditionalScopes;
  if ( other.mExpressionContext )
  {
    mExpressionContext.reset( new QgsExpressionContext( *other.mExpressionContext ) );
  }
  else
  {
    mExpressionContext.reset();
  }
  return *this;
}

void QgsSymbolWidgetContext::setMapCanvas( QgsMapCanvas* canvas )
{
  mMapCanvas = canvas;
}

QgsMapCanvas* QgsSymbolWidgetContext::mapCanvas() const
{
  return mMapCanvas;
}

void QgsSymbolWidgetContext::setExpressionContext( QgsExpressionContext* context )
{
  if ( context )
    mExpressionContext.reset( new QgsExpressionContext( *context ) );
  else
    mExpressionContext.reset();
}

QgsExpressionContext* QgsSymbolWidgetContext::expressionContext() const
{
  return mExpressionContext.data();
}

void QgsSymbolWidgetContext::setAdditionalExpressionContextScopes( const QList<QgsExpressionContextScope>& scopes )
{
  mAdditionalScopes = scopes;
}

QList<QgsExpressionContextScope> QgsSymbolWidgetContext::additionalExpressionContextScopes() const
{
  return mAdditionalScopes;
}
