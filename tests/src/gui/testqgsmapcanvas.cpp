/***************************************************************************
    testqgsmapcanvas.cpp
    ---------------------
    begin                : December 2013
    copyright            : (C) 2013 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QtTest/QtTest>

#include <qgsapplication.h>
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsmaplayerregistry.h>
#include <qgsrenderchecker.h>
#include <qgsvectordataprovider.h>
#include <qgsmaptoolpan.h>
#include "qgstestutils.h"

namespace QTest
{
  template<>
  char* toString( const QgsRectangle& r )
  {
    QByteArray ba = r.toString().toLocal8Bit();
    return qstrdup( ba.data() );
  }
}

class QgsMapToolTest : public QgsMapTool
{
  public:
    QgsMapToolTest( QgsMapCanvas* canvas ) : QgsMapTool( canvas ) {}

};

class TestQgsMapCanvas : public QObject
{
    Q_OBJECT
  public:
    TestQgsMapCanvas()
        : mCanvas( nullptr )
    {}

  private slots:
    void initTestCase(); // will be called before the first testfunction is executed.
    void cleanupTestCase(); // will be called after the last testfunction was executed.

    void testPanByKeyboard();
    void testMagnification();
    void testMagnificationExtent();
    void testMagnificationScale();
    void testZoomByWheel();
    void testShiftZoom();

  private:
    QgsMapCanvas* mCanvas;
};



void TestQgsMapCanvas::initTestCase()
{
  QgsApplication::init(); // init paths for CRS lookup
  QgsApplication::initQgis();

  mCanvas = new QgsMapCanvas();
}

void TestQgsMapCanvas::cleanupTestCase()
{
}


void TestQgsMapCanvas::testPanByKeyboard()
{
  // The keys to simulate
  QList<Qt::Key> keys = QList<Qt::Key>() << Qt::Key_Left << Qt::Key_Down << Qt::Key_Right << Qt::Key_Up;

  // The canvas rotations to test
  QList<double> rotations = QList<double>() << 0.0 << 30.0;

  QgsRectangle initialExtent( 100, 100, 110, 110 );

  Q_FOREACH ( double rotation, rotations )
  {
    // Set rotation and initial extent
    mCanvas->setRotation( rotation );
    mCanvas->setExtent( initialExtent );

    // Save actual extent, simulate panning by keyboard and verify the extent is unchanged
    QgsRectangle originalExtent = mCanvas->extent();
    Q_FOREACH ( Qt::Key key, keys )
    {
      QgsRectangle tempExtent = mCanvas->extent();
      QKeyEvent keyEvent( QEvent::KeyPress, key, Qt::NoModifier );
      QApplication::sendEvent( mCanvas, &keyEvent );
      QVERIFY( mCanvas->extent() != tempExtent );
    }
    QVERIFY( mCanvas->extent() == originalExtent );
  }
}

void TestQgsMapCanvas::testMagnification()
{
  // test directory
  QString testDataDir = QString( TEST_DATA_DIR ) + '/';
  QString controlImageDir = testDataDir + "control_images/expected_map_magnification/";

  // prepare spy and unit testing stuff
  QgsRenderChecker checker;
  checker.setControlPathPrefix( "mapcanvas" );
  checker.setColorTolerance( 5 );

  QSignalSpy spy( mCanvas, SIGNAL( mapCanvasRefreshed() ) );

  QEventLoop loop;
  QObject::connect( mCanvas, SIGNAL( mapCanvasRefreshed() ), &loop, SLOT( quit() ) );

  QTimer timer;
  QObject::connect( &timer, SIGNAL( timeout() ), &loop, SLOT( quit() ) );

  QTemporaryFile tmpFile;
  tmpFile.setAutoRemove( false );
  tmpFile.open(); // fileName is not available until open
  QString tmpName = tmpFile.fileName();
  tmpFile.close();

  // build vector layer
  QString myPointsFileName = testDataDir + "points.shp";
  QFileInfo myPointFileInfo( myPointsFileName );
  QgsVectorLayer *layer = new QgsVectorLayer( myPointFileInfo.filePath(),
      myPointFileInfo.completeBaseName(), "ogr" );

  // prepare map canvas
  QList<QgsMapCanvasLayer> layers;
  layers.append( layer );
  mCanvas->setLayerSet( layers );
  QgsMapLayerRegistry::instance()->addMapLayers( QList<QgsMapLayer *>() << layer );

  mCanvas->setExtent( layer->extent() );

  // refresh and wait for rendering
  mCanvas->refresh();
  timer.start( 3000 );
  loop.exec();
  QCOMPARE( spy.count(), 1 );
  spy.clear();

  // control image with magnification factor 1.0
  mCanvas->saveAsImage( tmpName );

  checker.setControlName( "expected_map_magnification" );
  checker.setRenderedImage( tmpName );
  checker.setSizeTolerance( 10, 10 );
  QCOMPARE( checker.compareImages( "map_magnification", 100 ), true );

  // set magnification factor (auto refresh)
  mCanvas->setMagnificationFactor( 6.5 );
  QCOMPARE( mCanvas->magnificationFactor(), 6.5 );

  // wait for rendering
  timer.start( 3000 );
  loop.exec();
  QCOMPARE( spy.count(), 1 );
  spy.clear();

  // control image with magnification factor 6.5
  mCanvas->saveAsImage( tmpName );

  checker.setRenderedImage( tmpName );
  checker.setControlName( "expected_map_magnification_6_5" );
  controlImageDir = testDataDir + "control_images/";
  checker.setSizeTolerance( 10, 10 );
  QCOMPARE( checker.compareImages( "map_magnification_6_5", 100 ), true );

  // set magnification factor (auto refresh)
  mCanvas->setMagnificationFactor( 1.0 );
  QCOMPARE( mCanvas->magnificationFactor(), 1.0 );

  // wait for rendering
  timer.start( 3000 );
  loop.exec();
  QCOMPARE( spy.count(), 1 );
  spy.clear();

  // control image with magnification factor 1.0
  mCanvas->saveAsImage( tmpName );

  checker.setControlName( "expected_map_magnification" );
  checker.setRenderedImage( tmpName );
  checker.setSizeTolerance( 10, 10 );
  QCOMPARE( checker.compareImages( "map_magnification", 100 ), true );
}

void compareExtent( const QgsRectangle &initialExtent,
                    const QgsRectangle &extent )
{
  QGSCOMPARENEAR( initialExtent.xMinimum(), extent.xMinimum(), 0.00000000001 );
  QGSCOMPARENEAR( initialExtent.xMaximum(), extent.xMaximum(), 0.00000000001 );
  QGSCOMPARENEAR( initialExtent.yMinimum(), extent.yMinimum(), 0.00000000001 );
  QGSCOMPARENEAR( initialExtent.yMaximum(), extent.yMaximum(), 0.00000000001 );
}

void TestQgsMapCanvas::testMagnificationExtent()
{
  // build vector layer
  QString testDataDir = QString( TEST_DATA_DIR ) + '/';
  QString myPointsFileName = testDataDir + "points.shp";
  QFileInfo myPointFileInfo( myPointsFileName );
  QgsVectorLayer *layer = new QgsVectorLayer( myPointFileInfo.filePath(),
      myPointFileInfo.completeBaseName(), "ogr" );

  // prepare map canvas
  QList<QgsMapCanvasLayer> layers;
  layers.append( layer );
  mCanvas->setLayerSet( layers );
  QgsMapLayerRegistry::instance()->addMapLayers( QList<QgsMapLayer *>() << layer );

  // zoomToFullExtent
  mCanvas->zoomToFullExtent();
  QgsRectangle initialExtent = mCanvas->extent();

  mCanvas->setMagnificationFactor( 4.0 );
  mCanvas->setMagnificationFactor( 1.0 );

  compareExtent( mCanvas->extent(), initialExtent );

  // setExtent with layer extent
  mCanvas->setExtent( layer->extent() );
  initialExtent = mCanvas->extent();

  mCanvas->setMagnificationFactor( 4.0 );
  mCanvas->setMagnificationFactor( 1.0 );

  compareExtent( mCanvas->extent(), initialExtent );

  // zoomToSelected
  QgsFeature f1( layer->dataProvider()->fields(), 1 );
  QgsFeature f2( layer->dataProvider()->fields(), 2 );
  QgsFeatureIds ids;
  ids << f1.id() << f2.id();
  layer->selectByIds( ids );

  mCanvas->zoomToSelected( layer );
  initialExtent = mCanvas->extent();

  mCanvas->setMagnificationFactor( 4.0 );
  mCanvas->setMagnificationFactor( 1.0 );

  compareExtent( mCanvas->extent(), initialExtent );

  // zoomToFeatureIds
  mCanvas->zoomToFeatureIds( layer, ids );
  initialExtent = mCanvas->extent();

  mCanvas->setMagnificationFactor( 4.0 );
  mCanvas->setMagnificationFactor( 1.0 );

  compareExtent( mCanvas->extent(), initialExtent );

  // zoomIn / zoomOut
  initialExtent = mCanvas->extent();
  mCanvas->zoomIn();

  mCanvas->setMagnificationFactor( 4.0 );
  mCanvas->zoomIn();
  mCanvas->zoomOut();
  mCanvas->setMagnificationFactor( 1.0 );

  mCanvas->zoomOut();

  compareExtent( mCanvas->extent(), initialExtent );

  // zoomScale
  initialExtent = mCanvas->extent();
  double scale = mCanvas->scale();
  mCanvas->zoomScale( 6.052017*10e7 );

  mCanvas->setMagnificationFactor( 4.0 );
  mCanvas->setMagnificationFactor( 1.0 );

  mCanvas->zoomScale( scale );
  compareExtent( mCanvas->extent(), initialExtent );
}

void TestQgsMapCanvas::testMagnificationScale()
{
  mCanvas->setMagnificationFactor( 1.0 );
  double initialScale = mCanvas->scale();

  mCanvas->setMagnificationFactor( 4.0 );
  QCOMPARE( initialScale, mCanvas->scale() );

  mCanvas->setMagnificationFactor( 7.5 );
  QCOMPARE( initialScale, mCanvas->scale() );

  mCanvas->setMagnificationFactor( 1.0 );
  QCOMPARE( initialScale, mCanvas->scale() );
}

void TestQgsMapCanvas::testZoomByWheel()
{
  mCanvas->setExtent( QgsRectangle( 0, 0, 10, 10 ) );
  QgsRectangle initialExtent = mCanvas->extent();
  double originalWidth = initialExtent.width();
  double originalHeight = initialExtent.height();

  mCanvas->setWheelFactor( 2 );

  //test zoom out
  QWheelEvent e( QPoint( 0, 0 ), -1, Qt::NoButton, Qt::NoModifier );
  mCanvas->wheelEvent( &e );
  QGSCOMPARENEAR( mCanvas->extent().width(), originalWidth * 2.0, 0.1 );
  QGSCOMPARENEAR( mCanvas->extent().height(), originalHeight * 2.0, 0.1 );

  //test zoom in
  e = QWheelEvent( QPoint( 0, 0 ), 1, Qt::NoButton, Qt::NoModifier );
  mCanvas->wheelEvent( &e );
  QGSCOMPARENEAR( mCanvas->extent().width(), originalWidth, 0.1 );
  QGSCOMPARENEAR( mCanvas->extent().height(), originalHeight, 0.1 );

  // test zoom out with ctrl
  e = QWheelEvent( QPoint( 0, 0 ), -1, Qt::NoButton, Qt::ControlModifier );
  mCanvas->wheelEvent( &e );
  QGSCOMPARENEAR( mCanvas->extent().width(), 1.05 * originalWidth, 0.1 );
  QGSCOMPARENEAR( mCanvas->extent().height(), 1.05 * originalHeight, 0.1 );

  //test zoom in with ctrl
  e = QWheelEvent( QPoint( 0, 0 ), 1, Qt::NoButton, Qt::ControlModifier );
  mCanvas->wheelEvent( &e );
  QGSCOMPARENEAR( mCanvas->extent().width(), originalWidth, 0.1 );
  QGSCOMPARENEAR( mCanvas->extent().height(), originalHeight, 0.1 );
}

void TestQgsMapCanvas::testShiftZoom()
{
  mCanvas->setExtent( QgsRectangle( 0, 0, 10, 10 ) );
  QgsRectangle initialExtent = mCanvas->extent();
  double originalWidth = initialExtent.width();
  double originalHeight = initialExtent.height();

  QPoint startPos = QPoint( mCanvas->width() / 4, mCanvas->height() / 4 );
  QPoint endPos = QPoint( mCanvas->width() * 3 / 4.0, mCanvas->height() * 3 / 4.0 );

  QgsMapToolPan panTool( mCanvas );

  // start by testing a tool with shift-zoom enabled
  mCanvas->setMapTool( &panTool );

  QMouseEvent e( QMouseEvent::MouseButtonPress, startPos,
                 Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mousePressEvent( &e );
  e = QMouseEvent( QMouseEvent::MouseMove, endPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mouseMoveEvent( &e );
  e = QMouseEvent( QMouseEvent::MouseButtonRelease, endPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mouseReleaseEvent( &e );

  QGSCOMPARENEAR( mCanvas->extent().width(), originalWidth / 2.0, 0.2 );
  QGSCOMPARENEAR( mCanvas->extent().height(), originalHeight / 2.0, 0.2 );

  //reset
  mCanvas->setExtent( QgsRectangle( 0, 0, 10, 10 ) );

  //test that a shift-click (no movement) will not zoom
  e = QMouseEvent( QMouseEvent::MouseButtonPress, startPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mousePressEvent( &e );
  e = QMouseEvent( QMouseEvent::MouseMove, startPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mouseMoveEvent( &e );
  e = QMouseEvent( QMouseEvent::MouseButtonRelease, startPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mouseReleaseEvent( &e );

  QGSCOMPARENEAR( mCanvas->extent().width(), originalWidth, 0.0001 );
  QGSCOMPARENEAR( mCanvas->extent().height(), originalHeight, 0.0001 );

  //reset
  mCanvas->setExtent( QgsRectangle( 0, 0, 10, 10 ) );

  //test with map tool which does not have shift-zoom enabled
  QgsMapToolTest mapTool( mCanvas );
  mCanvas->setMapTool( &mapTool );

  e = QMouseEvent( QMouseEvent::MouseButtonPress, startPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mousePressEvent( &e );
  e = QMouseEvent( QMouseEvent::MouseMove, endPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mouseMoveEvent( &e );
  e = QMouseEvent( QMouseEvent::MouseButtonRelease, endPos,
                   Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier );
  mCanvas->mouseReleaseEvent( &e );

  QGSCOMPARENEAR( mCanvas->extent().width(), originalWidth, 0.00001 );
  QGSCOMPARENEAR( mCanvas->extent().height(), originalHeight, 0.00001 );
}

QTEST_MAIN( TestQgsMapCanvas )
#include "testqgsmapcanvas.moc"
