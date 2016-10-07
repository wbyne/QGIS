/***************************************************************************
    testqgskeyvaluewidget.cpp
     --------------------------------------
    Date                 : 08 09 2016
    Copyright            : (C) 2016 Patrick Valsecchi
    Email                : patrick dot valsecchi at camptocamp dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include <QtTest/QtTest>

#include <editorwidgets/qgskeyvaluewidgetfactory.h>
#include <qgskeyvaluewidget.h>
#include <editorwidgets/core/qgseditorwidgetwrapper.h>
#include <qgsapplication.h>

class TestQgsKeyValueWidget : public QObject
{
    Q_OBJECT
  public:

  private slots:
    void initTestCase() // will be called before the first testfunction is executed.
    {
      QgsApplication::init();
      QgsApplication::initQgis();
    }

    void cleanupTestCase() // will be called after the last testfunction was executed.
    {
      QgsApplication::exitQgis();
    }

    void testUpdate()
    {
      const QgsKeyValueWidgetFactory factory( "testKeyValue" );
      QgsEditorWidgetWrapper* wrapper = factory.create( nullptr, 0, nullptr, nullptr );
      QVERIFY( wrapper );
      QSignalSpy spy( wrapper, SIGNAL( valueChanged( const QVariant& ) ) );

      QgsKeyValueWidget* widget = qobject_cast< QgsKeyValueWidget* >( wrapper->widget() );
      QVERIFY( widget );

      QVariantMap initial;
      initial["1"] = "one";
      initial["2"] = "two";
      wrapper->setValue( initial );

      const QVariant value = wrapper->value();
      QCOMPARE( int( value.type() ), int( QVariant::Map ) );
      QCOMPARE( value.toMap(), initial );
      QCOMPARE( spy.count(), 0 );

      QAbstractItemModel* model = widget->tableView->model();
      model->setData( model->index( 0, 1 ), "hello" );
      QCOMPARE( spy.count(), 1 );

      QVariantMap expected = initial;
      expected["1"] = "hello";
      QVariant eventValue = spy.at( 0 ).at( 0 ).value<QVariant>();
      QCOMPARE( int( eventValue.type() ), int( QVariant::Map ) );
      QCOMPARE( eventValue.toMap(), expected );
      QCOMPARE( wrapper->value().toMap(), expected );
      QCOMPARE( spy.count(), 1 );
    }
};

QTEST_MAIN( TestQgsKeyValueWidget )
#include "testqgskeyvaluewidget.moc"
