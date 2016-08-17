/***************************************************************************
    qgsmaptooladvanceddigitizing.cpp  - map tool with event in map coordinates
    ----------------------
    begin                : October 2014
    copyright            : (C) Denis Rouzaud
    email                : denis.rouzaud@gmail.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmapmouseevent.h"
#include "qgsmaptooladvanceddigitizing.h"
#include "qgsmapcanvas.h"
#include "qgsadvanceddigitizingdockwidget.h"

QgsMapToolAdvancedDigitizing::QgsMapToolAdvancedDigitizing( QgsMapCanvas* canvas, QgsAdvancedDigitizingDockWidget* cadDockWidget )
    : QgsMapToolEdit( canvas )
    , mCaptureMode( CapturePoint )
    , mSnapOnPress( false )
    , mSnapOnRelease( false )
    , mSnapOnMove( false )
    , mSnapOnDoubleClick( false )
    , mCadDockWidget( cadDockWidget )
{
}

QgsMapToolAdvancedDigitizing::~QgsMapToolAdvancedDigitizing()
{
}

void QgsMapToolAdvancedDigitizing::canvasPressEvent( QgsMapMouseEvent* e )
{
  snap( e );
  if ( !mCadDockWidget->canvasPressEvent( e ) )
    cadCanvasPressEvent( e );
}

void QgsMapToolAdvancedDigitizing::canvasReleaseEvent( QgsMapMouseEvent* e )
{
  snap( e );

  QgsAdvancedDigitizingDockWidget::AdvancedDigitizingMode dockMode;
  switch ( mCaptureMode )
  {
    case CaptureLine:
    case CapturePolygon:
      dockMode = QgsAdvancedDigitizingDockWidget::ManyPoints;
      break;
    case CaptureSegment:
      dockMode = QgsAdvancedDigitizingDockWidget::TwoPoints;
      break;
    default:
      dockMode = QgsAdvancedDigitizingDockWidget::SinglePoint;
      break;
  }

  if ( !mCadDockWidget->canvasReleaseEvent( e, dockMode ) )
    cadCanvasReleaseEvent( e );
}

void QgsMapToolAdvancedDigitizing::canvasMoveEvent( QgsMapMouseEvent* e )
{
  snap( e );
  if ( !mCadDockWidget->canvasMoveEvent( e ) )
    cadCanvasMoveEvent( e );
}

void QgsMapToolAdvancedDigitizing::activate()
{
  QgsMapToolEdit::activate();
  connect( mCadDockWidget, SIGNAL( pointChanged( QgsPoint ) ), this, SLOT( cadPointChanged( QgsPoint ) ) );
  mCadDockWidget->enable();
}

void QgsMapToolAdvancedDigitizing::deactivate()
{
  QgsMapToolEdit::deactivate();
  disconnect( mCadDockWidget, SIGNAL( pointChanged( QgsPoint ) ), this, SLOT( cadPointChanged( QgsPoint ) ) );
  mCadDockWidget->disable();
}

void QgsMapToolAdvancedDigitizing::cadPointChanged( const QgsPoint& point )
{
  Q_UNUSED( point );
  QMouseEvent* ev = new QMouseEvent( QEvent::MouseMove, mCanvas->mouseLastXY(), Qt::NoButton, Qt::NoButton, Qt::NoModifier );
  qApp->postEvent( mCanvas->viewport(), ev );  // event queue will delete the event when processed
}

void QgsMapToolAdvancedDigitizing::snap( QgsMapMouseEvent* e )
{
  if ( !mCadDockWidget->cadEnabled() )
    e->snapPoint( QgsMapMouseEvent::SnapProjectConfig );
}
