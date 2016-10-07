/***************************************************************************
    qgssubstitutionlistwidget.cpp
    -----------------------------
    begin                : August 2016
    copyright            : (C) 2016 Nyall Dawson
    email                : nyall dot dawson at gmail dot com


 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgssubstitutionlistwidget.h"
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

QgsSubstitutionListWidget::QgsSubstitutionListWidget( QWidget* parent )
    : QgsPanelWidget( parent )
{
  setupUi( this );
  connect( mTableSubstitutions, SIGNAL( cellChanged( int, int ) ), this, SLOT( tableChanged() ) );
}

void QgsSubstitutionListWidget::setSubstitutions( const QgsStringReplacementCollection& substitutions )
{
  mTableSubstitutions->blockSignals( true );
  mTableSubstitutions->clearContents();
  Q_FOREACH ( const QgsStringReplacement& replacement, substitutions.replacements() )
  {
    addSubstitution( replacement );
  }
  mTableSubstitutions->blockSignals( false );
}

QgsStringReplacementCollection QgsSubstitutionListWidget::substitutions() const
{
  QList< QgsStringReplacement > result;
  for ( int i = 0; i < mTableSubstitutions->rowCount(); ++i )
  {
    if ( !mTableSubstitutions->item( i, 0 ) )
      continue;

    if ( mTableSubstitutions->item( i, 0 )->text().isEmpty() )
      continue;

    QCheckBox* chkCaseSensitive = qobject_cast<QCheckBox*>( mTableSubstitutions->cellWidget( i, 2 ) );
    QCheckBox* chkWholeWord = qobject_cast<QCheckBox*>( mTableSubstitutions->cellWidget( i, 3 ) );

    QgsStringReplacement replacement( mTableSubstitutions->item( i, 0 )->text(),
                                      mTableSubstitutions->item( i, 1 )->text(),
                                      chkCaseSensitive->isChecked(),
                                      chkWholeWord->isChecked() );
    result << replacement;
  }
  return QgsStringReplacementCollection( result );
}

void QgsSubstitutionListWidget::on_mButtonAdd_clicked()
{
  addSubstitution( QgsStringReplacement( QString(), QString(), false, true ) );
  mTableSubstitutions->setFocus();
  mTableSubstitutions->setCurrentCell( mTableSubstitutions->rowCount() - 1, 0 );
}

void QgsSubstitutionListWidget::on_mButtonRemove_clicked()
{
  int currentRow = mTableSubstitutions->currentRow();
  mTableSubstitutions->removeRow( currentRow );
  tableChanged();
}

void QgsSubstitutionListWidget::tableChanged()
{
  emit substitutionsChanged( substitutions() );
}

void QgsSubstitutionListWidget::on_mButtonExport_clicked()
{
  QString fileName = QFileDialog::getSaveFileName( this, tr( "Save substitutions" ), QDir::homePath(),
                     tr( "XML files (*.xml *.XML)" ) );
  if ( fileName.isEmpty() )
  {
    return;
  }

  // ensure the user never ommited the extension from the file name
  if ( !fileName.endsWith( ".xml", Qt::CaseInsensitive ) )
  {
    fileName += ".xml";
  }

  QDomDocument doc;
  QDomElement root = doc.createElement( "substitutions" );
  root.setAttribute( "version", "1.0" );
  QgsStringReplacementCollection collection = substitutions();
  collection.writeXml( root, doc );
  doc.appendChild( root );

  QFile file( fileName );
  if ( !file.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) )
  {
    QMessageBox::warning( nullptr, tr( "Export substitutions" ),
                          tr( "Cannot write file %1:\n%2." ).arg( fileName, file.errorString() ),
                          QMessageBox::Ok,
                          QMessageBox::Ok );
    return;
  }

  QTextStream out( &file );
  doc.save( out, 4 );
}

void QgsSubstitutionListWidget::on_mButtonImport_clicked()
{
  QString fileName = QFileDialog::getOpenFileName( this, tr( "Load substitutions" ), QDir::homePath(),
                     tr( "XML files (*.xml *.XML)" ) );
  if ( fileName.isEmpty() )
  {
    return;
  }

  QFile file( fileName );
  if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
  {
    QMessageBox::warning( nullptr, tr( "Import substitutions" ),
                          tr( "Cannot read file %1:\n%2." ).arg( fileName, file.errorString() ),
                          QMessageBox::Ok,
                          QMessageBox::Ok );
    return;
  }

  QDomDocument doc;
  QString errorStr;
  int errorLine;
  int errorColumn;

  if ( !doc.setContent( &file, true, &errorStr, &errorLine, &errorColumn ) )
  {
    QMessageBox::warning( nullptr, tr( "Import substitutions" ),
                          tr( "Parse error at line %1, column %2:\n%3" )
                          .arg( errorLine )
                          .arg( errorColumn )
                          .arg( errorStr ),
                          QMessageBox::Ok,
                          QMessageBox::Ok );
    return;
  }

  QDomElement root = doc.documentElement();
  if ( root.tagName() != "substitutions" )
  {
    QMessageBox::warning( nullptr, tr( "Import substitutions" ),
                          tr( "The selected file in not an substitutions list." ),
                          QMessageBox::Ok,
                          QMessageBox::Ok );
    return;
  }

  QgsStringReplacementCollection collection;
  collection.readXml( root );
  setSubstitutions( collection );
  tableChanged();
}

void QgsSubstitutionListWidget::addSubstitution( const QgsStringReplacement& substitution )
{
  int row = mTableSubstitutions->rowCount();
  mTableSubstitutions->insertRow( row );

  Qt::ItemFlags itemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable
                            | Qt::ItemIsEditable;

  QTableWidgetItem* matchItem = new QTableWidgetItem( substitution.match() );
  matchItem->setFlags( itemFlags );
  mTableSubstitutions->setItem( row, 0, matchItem );
  QTableWidgetItem* replaceItem = new QTableWidgetItem( substitution.replacement() );
  replaceItem->setFlags( itemFlags );
  mTableSubstitutions->setItem( row, 1, replaceItem );

  QCheckBox* caseSensitiveChk = new QCheckBox( this );
  caseSensitiveChk->setChecked( substitution.caseSensitive() );
  mTableSubstitutions->setCellWidget( row, 2, caseSensitiveChk );
  connect( caseSensitiveChk, SIGNAL( toggled( bool ) ), this, SLOT( tableChanged() ) );

  QCheckBox* wholeWordChk = new QCheckBox( this );
  wholeWordChk->setChecked( substitution.wholeWordOnly() );
  mTableSubstitutions->setCellWidget( row, 3, wholeWordChk );
  connect( wholeWordChk, SIGNAL( toggled( bool ) ), this, SLOT( tableChanged() ) );
}


//
// QgsSubstitutionListDialog
//


QgsSubstitutionListDialog::QgsSubstitutionListDialog( QWidget* parent )
    : QDialog( parent )
    , mWidget( nullptr )
{
  setWindowTitle( tr( "Substitutions" ) );
  QVBoxLayout* vLayout = new QVBoxLayout();
  mWidget = new QgsSubstitutionListWidget();
  vLayout->addWidget( mWidget );
  QDialogButtonBox* bbox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal );
  connect( bbox, SIGNAL( accepted() ), this, SLOT( accept() ) );
  connect( bbox, SIGNAL( rejected() ), this, SLOT( reject() ) );
  vLayout->addWidget( bbox );
  setLayout( vLayout );
}

void QgsSubstitutionListDialog::setSubstitutions( const QgsStringReplacementCollection& substitutions )
{
  mWidget->setSubstitutions( substitutions );
}

QgsStringReplacementCollection QgsSubstitutionListDialog::substitutions() const
{
  return mWidget->substitutions();
}
