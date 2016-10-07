/***************************************************************************
     testqgsmaptopixel.cpp
     --------------------------------------
    Date                 : Tue  9 Dec 2014
    Copyright            : (C) 2014 by Sandro Santilli
    Email                : strk@keybit.net
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <QtTest/QtTest>
#include <QObject>
#include <QString>
//header for class being tested
#include <qgsrectangle.h>
#include <qgsmaptopixel.h>
#include <qgspoint.h>
#include "qgslogger.h"
#include "qgstestutils.h"

class TestQgsMapToPixel: public QObject
{
    Q_OBJECT
  private slots:
    void rotation();
    void getters();
    void fromScale();
};

void TestQgsMapToPixel::rotation()
{
  QgsMapToPixel m2p( 1, 5, 5, 10, 10, 90 );

  QgsPoint p( 5, 5 ); // in geographical units
  QgsPoint d = m2p.transform( p ); // to device pixels
  QCOMPARE( d.x(), 5.0 ); // center doesn't move
  QCOMPARE( d.y(), 5.0 );

  QgsPoint b = m2p.toMapCoordinatesF( d.x(), d.y() ); // transform back
  QCOMPARE( p, b );

  m2p.transform( &p ); // in place transform
  QCOMPARE( p, d );

  m2p.setParameters( 0.1, 5, 5, 10, 10, -90 );
  p = m2p.toMapCoordinates( 5, 5 );
  QCOMPARE( p.x(), 5.0 ); // center doesn't move
  QCOMPARE( p.y(), 5.0 );
  d = m2p.transform( p );
  QCOMPARE( d, QgsPoint( 5, 5 ) );

  p = m2p.toMapCoordinates( 10, 0 );
  QCOMPARE( p.x(), 5.5 ); // corner scales and rotates
  QCOMPARE( p.y(), 4.5 );
  d = m2p.transform( p );
  QCOMPARE( d, QgsPoint( 10, 0 ) );

  m2p.setParameters( 0.1, 5, 5, 10, 10, 360 );
  p = m2p.toMapCoordinates( 10, 0 );
  QCOMPARE( p.x(), 5.5 ); // corner scales
  QCOMPARE( p.y(), 5.5 );
  d = m2p.transform( p );
  QCOMPARE( d, QgsPoint( 10, 0 ) );

  m2p.setParameters( 0.1, 5, 5, 10, 10, 0 );
  p = m2p.toMapCoordinates( 10, 0 );
  QCOMPARE( p.x(), 5.5 ); // corner scales
  QCOMPARE( p.y(), 5.5 );
  d = m2p.transform( p );
  QCOMPARE( d, QgsPoint( 10, 0 ) );

}

void TestQgsMapToPixel::getters()
{
  QgsMapToPixel m2p( 1, 5, 6, 10, 100, 90 );
  QCOMPARE( m2p.xCenter(), 5.0 );
  QCOMPARE( m2p.yCenter(), 6.0 );
  QCOMPARE( m2p.mapRotation(), 90.0 );
  QCOMPARE( m2p.mapHeight(), 100 );
  QCOMPARE( m2p.mapWidth(), 10 );
  QCOMPARE( m2p.mapUnitsPerPixel(), 1.0 );

  m2p.setParameters( 2, 10, 12, 20, 200, 180 );
  QCOMPARE( m2p.xCenter(), 10.0 );
  QCOMPARE( m2p.yCenter(), 12.0 );
  QCOMPARE( m2p.mapRotation(), 180.0 );
  QCOMPARE( m2p.mapHeight(), 200 );
  QCOMPARE( m2p.mapWidth(), 20 );
  QCOMPARE( m2p.mapUnitsPerPixel(), 2.0 );
}

void TestQgsMapToPixel::fromScale()
{
  QgsMapToPixel m2p = QgsMapToPixel::fromScale( 0.001, QgsUnitTypes::DistanceMeters, 96.0 );
  QGSCOMPARENEAR( m2p.mapUnitsPerPixel(), 0.264583, 0.000001 );
  m2p = QgsMapToPixel::fromScale( 0.0001, QgsUnitTypes::DistanceMeters, 96.0 );
  QGSCOMPARENEAR( m2p.mapUnitsPerPixel(), 2.645833, 0.000001 );
  m2p = QgsMapToPixel::fromScale( 0.001, QgsUnitTypes::DistanceMeters, 72.0 );
  QGSCOMPARENEAR( m2p.mapUnitsPerPixel(), 0.352778, 0.000001 );
  m2p = QgsMapToPixel::fromScale( 0.001, QgsUnitTypes::DistanceKilometers, 96.0 );
  QGSCOMPARENEAR( m2p.mapUnitsPerPixel(), 0.000265, 0.000001 );
}

QTEST_MAIN( TestQgsMapToPixel )
#include "testqgsmaptopixel.moc"




