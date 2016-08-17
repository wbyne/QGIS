/***************************************************************************
    qgsgraduatedsymbolrendererwidget.cpp
    ---------------------
    begin                : November 2009
    copyright            : (C) 2009 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgsgraduatedsymbolrendererwidget.h"
#include "qgspanelwidget.h"

#include "qgssymbol.h"
#include "qgssymbollayerutils.h"
#include "qgsvectorcolorramp.h"
#include "qgsstyle.h"

#include "qgsvectorlayer.h"

#include "qgssymbolselectordialog.h"
#include "qgsexpressionbuilderdialog.h"

#include "qgsludialog.h"

#include "qgsproject.h"
#include "qgsmapcanvas.h"

#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QPen>
#include <QPainter>

// ------------------------------ Model ------------------------------------

///@cond PRIVATE

QgsGraduatedSymbolRendererModel::QgsGraduatedSymbolRendererModel( QObject * parent ) : QAbstractItemModel( parent )
    , mRenderer( nullptr )
    , mMimeFormat( "application/x-qgsgraduatedsymbolrendererv2model" )
{
}

void QgsGraduatedSymbolRendererModel::setRenderer( QgsGraduatedSymbolRenderer* renderer )
{
  if ( mRenderer )
  {
    beginRemoveRows( QModelIndex(), 0, mRenderer->ranges().size() - 1 );
    mRenderer = nullptr;
    endRemoveRows();
  }
  if ( renderer )
  {
    beginInsertRows( QModelIndex(), 0, renderer->ranges().size() - 1 );
    mRenderer = renderer;
    endInsertRows();
  }
}

void QgsGraduatedSymbolRendererModel::addClass( QgsSymbol* symbol )
{
  if ( !mRenderer ) return;
  int idx = mRenderer->ranges().size();
  beginInsertRows( QModelIndex(), idx, idx );
  mRenderer->addClass( symbol );
  endInsertRows();
}

void QgsGraduatedSymbolRendererModel::addClass( const QgsRendererRange& range )
{
  if ( !mRenderer )
  {
    return;
  }
  int idx = mRenderer->ranges().size();
  beginInsertRows( QModelIndex(), idx, idx );
  mRenderer->addClass( range );
  endInsertRows();
}

QgsRendererRange QgsGraduatedSymbolRendererModel::rendererRange( const QModelIndex &index )
{
  if ( !index.isValid() || !mRenderer || mRenderer->ranges().size() <= index.row() )
  {
    return QgsRendererRange();
  }

  return mRenderer->ranges().value( index.row() );
}

Qt::ItemFlags QgsGraduatedSymbolRendererModel::flags( const QModelIndex & index ) const
{
  if ( !index.isValid() )
  {
    return Qt::ItemIsDropEnabled;
  }

  Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsUserCheckable;

  if ( index.column() == 2 )
  {
    flags |= Qt::ItemIsEditable;
  }

  return flags;
}

Qt::DropActions QgsGraduatedSymbolRendererModel::supportedDropActions() const
{
  return Qt::MoveAction;
}

QVariant QgsGraduatedSymbolRendererModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() || !mRenderer ) return QVariant();

  const QgsRendererRange range = mRenderer->ranges().value( index.row() );

  if ( role == Qt::CheckStateRole && index.column() == 0 )
  {
    return range.renderState() ? Qt::Checked : Qt::Unchecked;
  }
  else if ( role == Qt::DisplayRole || role == Qt::ToolTipRole )
  {
    switch ( index.column() )
    {
      case 1:
      {
        int decimalPlaces = mRenderer->labelFormat().precision() + 2;
        if ( decimalPlaces < 0 ) decimalPlaces = 0;
        return QString::number( range.lowerValue(), 'f', decimalPlaces ) + " - " + QString::number( range.upperValue(), 'f', decimalPlaces );
      }
      case 2:
        return range.label();
      default:
        return QVariant();
    }
  }
  else if ( role == Qt::DecorationRole && index.column() == 0 && range.symbol() )
  {
    return QgsSymbolLayerUtils::symbolPreviewIcon( range.symbol(), QSize( 16, 16 ) );
  }
  else if ( role == Qt::TextAlignmentRole )
  {
    return ( index.column() == 0 ) ? Qt::AlignHCenter : Qt::AlignLeft;
  }
  else if ( role == Qt::EditRole )
  {
    switch ( index.column() )
    {
        // case 1: return rangeStr;
      case 2:
        return range.label();
      default:
        return QVariant();
    }
  }

  return QVariant();
}

bool QgsGraduatedSymbolRendererModel::setData( const QModelIndex & index, const QVariant & value, int role )
{
  if ( !index.isValid() )
    return false;

  if ( index.column() == 0 && role == Qt::CheckStateRole )
  {
    mRenderer->updateRangeRenderState( index.row(), value == Qt::Checked );
    emit dataChanged( index, index );
    return true;
  }

  if ( role != Qt::EditRole )
    return false;

  switch ( index.column() )
  {
    case 1: // range
      return false; // range is edited in popup dialog
    case 2: // label
      mRenderer->updateRangeLabel( index.row(), value.toString() );
      break;
    default:
      return false;
  }

  emit dataChanged( index, index );
  return true;
}

QVariant QgsGraduatedSymbolRendererModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
  if ( orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < 3 )
  {
    QStringList lst;
    lst << tr( "Symbol" ) << tr( "Values" ) << tr( "Legend" );
    return lst.value( section );
  }
  return QVariant();
}

int QgsGraduatedSymbolRendererModel::rowCount( const QModelIndex &parent ) const
{
  if ( parent.isValid() || !mRenderer )
  {
    return 0;
  }
  return mRenderer->ranges().size();
}

int QgsGraduatedSymbolRendererModel::columnCount( const QModelIndex & index ) const
{
  Q_UNUSED( index );
  return 3;
}

QModelIndex QgsGraduatedSymbolRendererModel::index( int row, int column, const QModelIndex &parent ) const
{
  if ( hasIndex( row, column, parent ) )
  {
    return createIndex( row, column );
  }
  return QModelIndex();
}

QModelIndex QgsGraduatedSymbolRendererModel::parent( const QModelIndex &index ) const
{
  Q_UNUSED( index );
  return QModelIndex();
}

QStringList QgsGraduatedSymbolRendererModel::mimeTypes() const
{
  QStringList types;
  types << mMimeFormat;
  return types;
}

QMimeData *QgsGraduatedSymbolRendererModel::mimeData( const QModelIndexList &indexes ) const
{
  QMimeData *mimeData = new QMimeData();
  QByteArray encodedData;

  QDataStream stream( &encodedData, QIODevice::WriteOnly );

  // Create list of rows
  Q_FOREACH ( const QModelIndex &index, indexes )
  {
    if ( !index.isValid() || index.column() != 0 )
      continue;

    stream << index.row();
  }
  mimeData->setData( mMimeFormat, encodedData );
  return mimeData;
}

bool QgsGraduatedSymbolRendererModel::dropMimeData( const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent )
{
  Q_UNUSED( row );
  Q_UNUSED( column );
  if ( action != Qt::MoveAction ) return true;

  if ( !data->hasFormat( mMimeFormat ) ) return false;

  QByteArray encodedData = data->data( mMimeFormat );
  QDataStream stream( &encodedData, QIODevice::ReadOnly );

  QVector<int> rows;
  while ( !stream.atEnd() )
  {
    int r;
    stream >> r;
    rows.append( r );
  }

  int to = parent.row();
  // to is -1 if dragged outside items, i.e. below any item,
  // then move to the last position
  if ( to == -1 ) to = mRenderer->ranges().size(); // out of rang ok, will be decreased
  for ( int i = rows.size() - 1; i >= 0; i-- )
  {
    QgsDebugMsg( QString( "move %1 to %2" ).arg( rows[i] ).arg( to ) );
    int t = to;
    // moveCategory first removes and then inserts
    if ( rows[i] < t ) t--;
    mRenderer->moveClass( rows[i], t );
    // current moved under another, shift its index up
    for ( int j = 0; j < i; j++ )
    {
      if ( to < rows[j] && rows[i] > rows[j] ) rows[j] += 1;
    }
    // removed under 'to' so the target shifted down
    if ( rows[i] < to ) to--;
  }
  emit dataChanged( createIndex( 0, 0 ), createIndex( mRenderer->ranges().size(), 0 ) );
  emit rowsMoved();
  return false;
}

void QgsGraduatedSymbolRendererModel::deleteRows( QList<int> rows )
{
  for ( int i = rows.size() - 1; i >= 0; i-- )
  {
    beginRemoveRows( QModelIndex(), rows[i], rows[i] );
    mRenderer->deleteClass( rows[i] );
    endRemoveRows();
  }
}

void QgsGraduatedSymbolRendererModel::removeAllRows()
{
  beginRemoveRows( QModelIndex(), 0, mRenderer->ranges().size() - 1 );
  mRenderer->deleteAllClasses();
  endRemoveRows();
}

void QgsGraduatedSymbolRendererModel::sort( int column, Qt::SortOrder order )
{
  if ( column == 0 )
  {
    return;
  }
  if ( column == 1 )
  {
    mRenderer->sortByValue( order );
  }
  else if ( column == 2 )
  {
    mRenderer->sortByLabel( order );
  }
  emit rowsMoved();
  emit dataChanged( createIndex( 0, 0 ), createIndex( mRenderer->ranges().size(), 0 ) );
  QgsDebugMsg( "Done" );
}

void QgsGraduatedSymbolRendererModel::updateSymbology( bool resetModel )
{
  if ( resetModel )
  {
    reset();
  }
  else
  {
    emit dataChanged( createIndex( 0, 0 ), createIndex( mRenderer->ranges().size(), 0 ) );
  }
}

void QgsGraduatedSymbolRendererModel::updateLabels()
{
  emit dataChanged( createIndex( 0, 2 ), createIndex( mRenderer->ranges().size(), 2 ) );
}

// ------------------------------ View style --------------------------------
QgsGraduatedSymbolRendererViewStyle::QgsGraduatedSymbolRendererViewStyle( QStyle* style )
    : QProxyStyle( style )
{}

void QgsGraduatedSymbolRendererViewStyle::drawPrimitive( PrimitiveElement element, const QStyleOption * option, QPainter * painter, const QWidget * widget ) const
{
  if ( element == QStyle::PE_IndicatorItemViewItemDrop && !option->rect.isNull() )
  {
    QStyleOption opt( *option );
    opt.rect.setLeft( 0 );
    // draw always as line above, because we move item to that index
    opt.rect.setHeight( 0 );
    if ( widget ) opt.rect.setRight( widget->width() );
    QProxyStyle::drawPrimitive( element, &opt, painter, widget );
    return;
  }
  QProxyStyle::drawPrimitive( element, option, painter, widget );
}

///@endcond

// ------------------------------ Widget ------------------------------------

QgsRendererWidget* QgsGraduatedSymbolRendererWidget::create( QgsVectorLayer* layer, QgsStyle* style, QgsFeatureRenderer* renderer )
{
  return new QgsGraduatedSymbolRendererWidget( layer, style, renderer );
}

QgsExpressionContext QgsGraduatedSymbolRendererWidget::createExpressionContext() const
{
  QgsExpressionContext expContext;
  expContext << QgsExpressionContextUtils::globalScope()
  << QgsExpressionContextUtils::projectScope()
  << QgsExpressionContextUtils::atlasScope( nullptr );

  if ( mapCanvas() )
  {
    expContext << QgsExpressionContextUtils::mapSettingsScope( mapCanvas()->mapSettings() )
    << new QgsExpressionContextScope( mapCanvas()->expressionContextScope() );
  }
  else
  {
    expContext << QgsExpressionContextUtils::mapSettingsScope( QgsMapSettings() );
  }

  if ( vectorLayer() )
    expContext << QgsExpressionContextUtils::layerScope( vectorLayer() );

  return expContext;
}

QgsGraduatedSymbolRendererWidget::QgsGraduatedSymbolRendererWidget( QgsVectorLayer* layer, QgsStyle* style, QgsFeatureRenderer* renderer )
    : QgsRendererWidget( layer, style )
    , mRenderer( nullptr )
    , mModel( nullptr )
{


  // try to recognize the previous renderer
  // (null renderer means "no previous renderer")
  if ( renderer )
  {
    mRenderer = QgsGraduatedSymbolRenderer::convertFromRenderer( renderer );
  }
  if ( !mRenderer )
  {
    mRenderer = new QgsGraduatedSymbolRenderer( "", QgsRangeList() );
  }

  // setup user interface
  setupUi( this );

  mModel = new QgsGraduatedSymbolRendererModel( this );

  mExpressionWidget->setFilters( QgsFieldProxyModel::Numeric | QgsFieldProxyModel::Date );
  mExpressionWidget->setLayer( mLayer );

  mSizeUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels );

  cboGraduatedColorRamp->populate( mStyle );

  spinPrecision->setMinimum( QgsRendererRangeLabelFormat::MinPrecision );
  spinPrecision->setMaximum( QgsRendererRangeLabelFormat::MaxPrecision );

  // set project default color ramp
  QString defaultColorRamp = QgsProject::instance()->readEntry( "DefaultStyles", "/ColorRamp", "" );
  if ( defaultColorRamp != "" )
  {
    int index = cboGraduatedColorRamp->findText( defaultColorRamp, Qt::MatchCaseSensitive );
    if ( index >= 0 )
      cboGraduatedColorRamp->setCurrentIndex( index );
  }


  viewGraduated->setStyle( new QgsGraduatedSymbolRendererViewStyle( viewGraduated->style() ) );

  mGraduatedSymbol = QgsSymbol::defaultSymbol( mLayer->geometryType() );

  methodComboBox->blockSignals( true );
  methodComboBox->addItem( "Color" );
  if ( mGraduatedSymbol->type() == QgsSymbol::Marker )
  {
    methodComboBox->addItem( "Size" );
    minSizeSpinBox->setValue( 1 );
    maxSizeSpinBox->setValue( 8 );
  }
  else if ( mGraduatedSymbol->type() == QgsSymbol::Line )
  {
    methodComboBox->addItem( "Size" );
    minSizeSpinBox->setValue( .1 );
    maxSizeSpinBox->setValue( 2 );
  }
  methodComboBox->blockSignals( false );

  connect( mExpressionWidget, SIGNAL( fieldChanged( QString ) ), this, SLOT( graduatedColumnChanged( QString ) ) );
  connect( viewGraduated, SIGNAL( doubleClicked( const QModelIndex & ) ), this, SLOT( rangesDoubleClicked( const QModelIndex & ) ) );
  connect( viewGraduated, SIGNAL( clicked( const QModelIndex & ) ), this, SLOT( rangesClicked( const QModelIndex & ) ) );
  connect( viewGraduated, SIGNAL( customContextMenuRequested( const QPoint& ) ),  this, SLOT( contextMenuViewCategories( const QPoint& ) ) );

  connect( btnGraduatedClassify, SIGNAL( clicked() ), this, SLOT( classifyGraduated() ) );
  connect( btnChangeGraduatedSymbol, SIGNAL( clicked() ), this, SLOT( changeGraduatedSymbol() ) );
  connect( btnGraduatedDelete, SIGNAL( clicked() ), this, SLOT( deleteClasses() ) );
  connect( btnDeleteAllClasses, SIGNAL( clicked() ), this, SLOT( deleteAllClasses() ) );
  connect( btnGraduatedAdd, SIGNAL( clicked() ), this, SLOT( addClass() ) );
  connect( cbxLinkBoundaries, SIGNAL( toggled( bool ) ), this, SLOT( toggleBoundariesLink( bool ) ) );

  connect( mSizeUnitWidget, SIGNAL( changed() ), this, SLOT( on_mSizeUnitWidget_changed() ) );

  connectUpdateHandlers();

  // initialize from previously set renderer
  updateUiFromRenderer();

  // menus for data-defined rotation/size
  QMenu* advMenu = new QMenu;

  advMenu->addAction( tr( "Symbol levels..." ), this, SLOT( showSymbolLevels() ) );

  btnAdvanced->setMenu( advMenu );

  mHistogramWidget->setLayer( mLayer );
  mHistogramWidget->setRenderer( mRenderer );
  connect( mHistogramWidget, SIGNAL( rangesModified( bool ) ), this, SLOT( refreshRanges( bool ) ) );
  connect( mExpressionWidget, SIGNAL( fieldChanged( QString ) ), mHistogramWidget, SLOT( setSourceFieldExp( QString ) ) );

  mExpressionWidget->registerExpressionContextGenerator( this );
}

void QgsGraduatedSymbolRendererWidget::on_mSizeUnitWidget_changed()
{
  if ( !mGraduatedSymbol ) return;
  mGraduatedSymbol->setOutputUnit( mSizeUnitWidget->unit() );
  mGraduatedSymbol->setMapUnitScale( mSizeUnitWidget->getMapUnitScale() );
  updateGraduatedSymbolIcon();
  mRenderer->updateSymbols( mGraduatedSymbol );
  refreshSymbolView();
}

QgsGraduatedSymbolRendererWidget::~QgsGraduatedSymbolRendererWidget()
{
  delete mRenderer;
  delete mModel;
  delete mGraduatedSymbol;
}

QgsFeatureRenderer* QgsGraduatedSymbolRendererWidget::renderer()
{
  return mRenderer;
}

// Connect/disconnect event handlers which trigger updating renderer

void QgsGraduatedSymbolRendererWidget::connectUpdateHandlers()
{
  connect( spinGraduatedClasses, SIGNAL( valueChanged( int ) ), this, SLOT( classifyGraduated() ) );
  connect( cboGraduatedMode, SIGNAL( currentIndexChanged( int ) ), this, SLOT( classifyGraduated() ) );
  connect( cboGraduatedColorRamp, SIGNAL( currentIndexChanged( int ) ), this, SLOT( reapplyColorRamp() ) );
  connect( cboGraduatedColorRamp, SIGNAL( sourceRampEdited() ), this, SLOT( reapplyColorRamp() ) );
  connect( mButtonEditRamp, SIGNAL( clicked() ), cboGraduatedColorRamp, SLOT( editSourceRamp() ) );
  connect( cbxInvertedColorRamp, SIGNAL( toggled( bool ) ), this, SLOT( reapplyColorRamp() ) );
  connect( spinPrecision, SIGNAL( valueChanged( int ) ), this, SLOT( labelFormatChanged() ) );
  connect( cbxTrimTrailingZeroes, SIGNAL( toggled( bool ) ), this, SLOT( labelFormatChanged() ) );
  connect( txtLegendFormat, SIGNAL( textChanged( QString ) ), this, SLOT( labelFormatChanged() ) );
  connect( minSizeSpinBox, SIGNAL( valueChanged( double ) ), this, SLOT( reapplySizes() ) );
  connect( maxSizeSpinBox, SIGNAL( valueChanged( double ) ), this, SLOT( reapplySizes() ) );

  connect( mModel, SIGNAL( rowsMoved() ), this, SLOT( rowsMoved() ) );
  connect( mModel, SIGNAL( dataChanged( QModelIndex, QModelIndex ) ), this, SLOT( modelDataChanged() ) );
}

// Connect/disconnect event handlers which trigger updating renderer

void QgsGraduatedSymbolRendererWidget::disconnectUpdateHandlers()
{
  disconnect( spinGraduatedClasses, SIGNAL( valueChanged( int ) ), this, SLOT( classifyGraduated() ) );
  disconnect( cboGraduatedMode, SIGNAL( currentIndexChanged( int ) ), this, SLOT( classifyGraduated() ) );
  disconnect( cboGraduatedColorRamp, SIGNAL( currentIndexChanged( int ) ), this, SLOT( reapplyColorRamp() ) );
  disconnect( cboGraduatedColorRamp, SIGNAL( sourceRampEdited() ), this, SLOT( reapplyColorRamp() ) );
  disconnect( mButtonEditRamp, SIGNAL( clicked() ), cboGraduatedColorRamp, SLOT( editSourceRamp() ) );
  disconnect( cbxInvertedColorRamp, SIGNAL( toggled( bool ) ), this, SLOT( reapplyColorRamp() ) );
  disconnect( spinPrecision, SIGNAL( valueChanged( int ) ), this, SLOT( labelFormatChanged() ) );
  disconnect( cbxTrimTrailingZeroes, SIGNAL( toggled( bool ) ), this, SLOT( labelFormatChanged() ) );
  disconnect( txtLegendFormat, SIGNAL( textChanged( QString ) ), this, SLOT( labelFormatChanged() ) );
  disconnect( minSizeSpinBox, SIGNAL( valueChanged( double ) ), this, SLOT( reapplySizes() ) );
  disconnect( maxSizeSpinBox, SIGNAL( valueChanged( double ) ), this, SLOT( reapplySizes() ) );

  disconnect( mModel, SIGNAL( rowsMoved() ), this, SLOT( rowsMoved() ) );
  disconnect( mModel, SIGNAL( dataChanged( QModelIndex, QModelIndex ) ), this, SLOT( modelDataChanged() ) );
}

void QgsGraduatedSymbolRendererWidget::updateUiFromRenderer( bool updateCount )
{
  disconnectUpdateHandlers();

  updateGraduatedSymbolIcon();

  // update UI from the graduated renderer (update combo boxes, view)
  if ( mRenderer->mode() < cboGraduatedMode->count() )
    cboGraduatedMode->setCurrentIndex( mRenderer->mode() );

  // Only update class count if different - otherwise typing value gets very messy
  int nclasses = mRenderer->ranges().count();
  if ( nclasses && updateCount )
    spinGraduatedClasses->setValue( mRenderer->ranges().count() );

  // set column
  QString attrName = mRenderer->classAttribute();
  mExpressionWidget->setField( attrName );
  mHistogramWidget->setSourceFieldExp( attrName );

  // set source symbol
  if ( mRenderer->sourceSymbol() )
  {
    delete mGraduatedSymbol;
    mGraduatedSymbol = mRenderer->sourceSymbol()->clone();
    updateGraduatedSymbolIcon();
  }

  mModel->setRenderer( mRenderer );
  viewGraduated->setModel( mModel );

  if ( mGraduatedSymbol )
  {
    mSizeUnitWidget->blockSignals( true );
    mSizeUnitWidget->setUnit( mGraduatedSymbol->outputUnit() );
    mSizeUnitWidget->setMapUnitScale( mGraduatedSymbol->mapUnitScale() );
    mSizeUnitWidget->blockSignals( false );
  }

  // set source color ramp
  methodComboBox->blockSignals( true );
  if ( mRenderer->graduatedMethod() == QgsGraduatedSymbolRenderer::GraduatedColor )
  {
    methodComboBox->setCurrentIndex( 0 );
    if ( mRenderer->sourceColorRamp() )
      cboGraduatedColorRamp->setSourceColorRamp( mRenderer->sourceColorRamp() );
    cbxInvertedColorRamp->setChecked( mRenderer->invertedColorRamp() );
  }
  else
  {
    methodComboBox->setCurrentIndex( 1 );
    if ( !mRenderer->ranges().isEmpty() ) // avoid overiding default size with zeros
    {
      minSizeSpinBox->setValue( mRenderer->minSymbolSize() );
      maxSizeSpinBox->setValue( mRenderer->maxSymbolSize() );
    }
  }
  mMethodStackedWidget->setCurrentIndex( methodComboBox->currentIndex() );
  methodComboBox->blockSignals( false );

  QgsRendererRangeLabelFormat labelFormat = mRenderer->labelFormat();
  txtLegendFormat->setText( labelFormat.format() );
  spinPrecision->setValue( labelFormat.precision() );
  cbxTrimTrailingZeroes->setChecked( labelFormat.trimTrailingZeroes() );

  viewGraduated->resizeColumnToContents( 0 );
  viewGraduated->resizeColumnToContents( 1 );
  viewGraduated->resizeColumnToContents( 2 );

  mHistogramWidget->refresh();

  connectUpdateHandlers();
  emit widgetChanged();
}

void QgsGraduatedSymbolRendererWidget::graduatedColumnChanged( const QString& field )
{
  mRenderer->setClassAttribute( field );
}

void QgsGraduatedSymbolRendererWidget::on_methodComboBox_currentIndexChanged( int idx )
{
  mMethodStackedWidget->setCurrentIndex( idx );
  if ( idx == 0 )
  {
    mRenderer->setGraduatedMethod( QgsGraduatedSymbolRenderer::GraduatedColor );
    QgsVectorColorRamp* ramp = cboGraduatedColorRamp->currentColorRamp();

    if ( !ramp )
    {
      if ( cboGraduatedColorRamp->count() == 0 )
        QMessageBox::critical( this, tr( "Error" ), tr( "There are no available color ramps. You can add them in Style Manager." ) );
      else
        QMessageBox::critical( this, tr( "Error" ), tr( "The selected color ramp is not available." ) );
      return;
    }
    mRenderer->setSourceColorRamp( ramp );
    reapplyColorRamp();
  }
  else
  {
    mRenderer->setGraduatedMethod( QgsGraduatedSymbolRenderer::GraduatedSize );
    reapplySizes();
  }
}

void QgsGraduatedSymbolRendererWidget::refreshRanges( bool reset )
{
  if ( !mModel )
    return;

  mModel->updateSymbology( reset );
  emit widgetChanged();
}

void QgsGraduatedSymbolRendererWidget::cleanUpSymbolSelector( QgsPanelWidget *container )
{
  if ( container )
  {
    QgsSymbolSelectorWidget* dlg = qobject_cast<QgsSymbolSelectorWidget*>( container );
    delete dlg->symbol();
  }
}

void QgsGraduatedSymbolRendererWidget::updateSymbolsFromWidget()
{
  QgsSymbolSelectorWidget* dlg = qobject_cast<QgsSymbolSelectorWidget*>( sender() );
  delete mGraduatedSymbol;
  mGraduatedSymbol = dlg->symbol()->clone();

  mSizeUnitWidget->blockSignals( true );
  mSizeUnitWidget->setUnit( mGraduatedSymbol->outputUnit() );
  mSizeUnitWidget->setMapUnitScale( mGraduatedSymbol->mapUnitScale() );
  mSizeUnitWidget->blockSignals( false );

  QItemSelectionModel* m = viewGraduated->selectionModel();
  QModelIndexList selectedIndexes = m->selectedRows( 1 );
  if ( m && !selectedIndexes.isEmpty() )
  {
    Q_FOREACH ( const QModelIndex& idx, selectedIndexes )
    {
      if ( idx.isValid() )
      {
        int rangeIdx = idx.row();
        QgsSymbol* newRangeSymbol = mGraduatedSymbol->clone();
        if ( selectedIndexes.count() > 1 )
        {
          //if updating multiple ranges, retain the existing range colors
          newRangeSymbol->setColor( mRenderer->ranges().at( rangeIdx ).symbol()->color() );
        }
        mRenderer->updateRangeSymbol( rangeIdx, newRangeSymbol );
      }
    }
  }
  else
  {
    updateGraduatedSymbolIcon();
    mRenderer->updateSymbols( mGraduatedSymbol );
  }

  refreshSymbolView();
  emit widgetChanged();
}


void QgsGraduatedSymbolRendererWidget::classifyGraduated()
{
  QString attrName = mExpressionWidget->currentField();

  int nclasses = spinGraduatedClasses->value();

  QSharedPointer<QgsVectorColorRamp> ramp( cboGraduatedColorRamp->currentColorRamp() );
  if ( !ramp )
  {
    if ( cboGraduatedColorRamp->count() == 0 )
      QMessageBox::critical( this, tr( "Error" ), tr( "There are no available color ramps. You can add them in Style Manager." ) );
    else
      QMessageBox::critical( this, tr( "Error" ), tr( "The selected color ramp is not available." ) );
    return;
  }

  QgsGraduatedSymbolRenderer::Mode mode;
  if ( cboGraduatedMode->currentIndex() == 0 )
    mode = QgsGraduatedSymbolRenderer::EqualInterval;
  else if ( cboGraduatedMode->currentIndex() == 2 )
    mode = QgsGraduatedSymbolRenderer::Jenks;
  else if ( cboGraduatedMode->currentIndex() == 3 )
    mode = QgsGraduatedSymbolRenderer::StdDev;
  else if ( cboGraduatedMode->currentIndex() == 4 )
    mode = QgsGraduatedSymbolRenderer::Pretty;
  else // default should be quantile for now
    mode = QgsGraduatedSymbolRenderer::Quantile;

  // Jenks is n^2 complexity, warn for big dataset (more than 50k records)
  // and give the user the chance to cancel
  if ( QgsGraduatedSymbolRenderer::Jenks == mode && mLayer->featureCount() > 50000 )
  {
    if ( QMessageBox::Cancel == QMessageBox::question( this, tr( "Warning" ), tr( "Natural break classification (Jenks) is O(n2) complexity, your classification may take a long time.\nPress cancel to abort breaks calculation or OK to continue." ), QMessageBox::Cancel, QMessageBox::Ok ) )
      return;
  }

  // create and set new renderer

  mRenderer->setClassAttribute( attrName );
  mRenderer->setMode( mode );

  if ( methodComboBox->currentIndex() == 0 )
  {
    QgsVectorColorRamp* ramp = cboGraduatedColorRamp->currentColorRamp();

    if ( !ramp )
    {
      if ( cboGraduatedColorRamp->count() == 0 )
        QMessageBox::critical( this, tr( "Error" ), tr( "There are no available color ramps. You can add them in Style Manager." ) );
      else
        QMessageBox::critical( this, tr( "Error" ), tr( "The selected color ramp is not available." ) );
      return;
    }
    mRenderer->setSourceColorRamp( ramp );
  }
  else
  {
    mRenderer->setSourceColorRamp( nullptr );
  }

  QApplication::setOverrideCursor( Qt::WaitCursor );
  mRenderer->updateClasses( mLayer, mode, nclasses );

  if ( methodComboBox->currentIndex() == 1 )
    mRenderer->setSymbolSizes( minSizeSpinBox->value(), maxSizeSpinBox->value() );

  mRenderer->calculateLabelPrecision();
  QApplication::restoreOverrideCursor();
  // PrettyBreaks and StdDev calculation don't generate exact
  // number of classes - leave user interface unchanged for these
  updateUiFromRenderer( false );
}

void QgsGraduatedSymbolRendererWidget::reapplyColorRamp()
{
  QgsVectorColorRamp* ramp = cboGraduatedColorRamp->currentColorRamp();
  if ( !ramp )
    return;

  mRenderer->updateColorRamp( ramp, cbxInvertedColorRamp->isChecked() );
  mRenderer->updateSymbols( mGraduatedSymbol );
  refreshSymbolView();
}

void QgsGraduatedSymbolRendererWidget::reapplySizes()
{
  mRenderer->setSymbolSizes( minSizeSpinBox->value(), maxSizeSpinBox->value() );
  mRenderer->updateSymbols( mGraduatedSymbol );
  refreshSymbolView();
}

void QgsGraduatedSymbolRendererWidget::changeGraduatedSymbol()
{
  QgsSymbol* newSymbol = mGraduatedSymbol->clone();
  QgsSymbolSelectorWidget* dlg = new QgsSymbolSelectorWidget( newSymbol, mStyle, mLayer, nullptr );
  dlg->setMapCanvas( mMapCanvas );

  connect( dlg, SIGNAL( widgetChanged() ), this, SLOT( updateSymbolsFromWidget() ) );
  connect( dlg, SIGNAL( accepted( QgsPanelWidget* ) ), this, SLOT( cleanUpSymbolSelector( QgsPanelWidget* ) ) );
  openPanel( dlg );
}

void QgsGraduatedSymbolRendererWidget::updateGraduatedSymbolIcon()
{
  if ( !mGraduatedSymbol )
    return;

  QIcon icon = QgsSymbolLayerUtils::symbolPreviewIcon( mGraduatedSymbol, btnChangeGraduatedSymbol->iconSize() );
  btnChangeGraduatedSymbol->setIcon( icon );
}

#if 0
int QgsRendererPropertiesDialog::currentRangeRow()
{
  QModelIndex idx = viewGraduated->selectionModel()->currentIndex();
  if ( !idx.isValid() )
    return -1;
  return idx.row();
}
#endif

QList<int> QgsGraduatedSymbolRendererWidget::selectedClasses()
{
  QList<int> rows;
  QModelIndexList selectedRows = viewGraduated->selectionModel()->selectedRows();

  Q_FOREACH ( const QModelIndex& r, selectedRows )
  {
    if ( r.isValid() )
    {
      rows.append( r.row() );
    }
  }
  return rows;
}

QgsRangeList QgsGraduatedSymbolRendererWidget::selectedRanges()
{
  QgsRangeList selectedRanges;
  QModelIndexList selectedRows = viewGraduated->selectionModel()->selectedRows();
  QModelIndexList::const_iterator sIt = selectedRows.constBegin();

  for ( ; sIt != selectedRows.constEnd(); ++sIt )
  {
    selectedRanges.append( mModel->rendererRange( *sIt ) );
  }
  return selectedRanges;
}

void QgsGraduatedSymbolRendererWidget::rangesDoubleClicked( const QModelIndex & idx )
{
  if ( idx.isValid() && idx.column() == 0 )
    changeRangeSymbol( idx.row() );
  if ( idx.isValid() && idx.column() == 1 )
    changeRange( idx.row() );
}

void QgsGraduatedSymbolRendererWidget::rangesClicked( const QModelIndex & idx )
{
  if ( !idx.isValid() )
    mRowSelected = -1;
  else
    mRowSelected = idx.row();
}

void QgsGraduatedSymbolRendererWidget::changeSelectedSymbols()
{
}

void QgsGraduatedSymbolRendererWidget::changeRangeSymbol( int rangeIdx )
{
  QgsSymbol* newSymbol = mRenderer->ranges()[rangeIdx].symbol()->clone();
  QgsSymbolSelectorWidget* dlg = new QgsSymbolSelectorWidget( newSymbol, mStyle, mLayer, nullptr );
  dlg->setDockMode( this->dockMode() );
  dlg->setMapCanvas( mMapCanvas );

  connect( dlg, SIGNAL( widgetChanged() ), this, SLOT( updateSymbolsFromWidget() ) );
  connect( dlg, SIGNAL( accepted( QgsPanelWidget* ) ), this, SLOT( cleanUpSymbolSelector( QgsPanelWidget* ) ) );
  openPanel( dlg );
}

void QgsGraduatedSymbolRendererWidget::changeRange( int rangeIdx )
{
  QgsLUDialog dialog( this );

  const QgsRendererRange& range = mRenderer->ranges()[rangeIdx];
  // Add arbitrary 2 to number of decimal places to retain a bit extra.
  // Ensures users can see if legend is not completely honest!
  int decimalPlaces = mRenderer->labelFormat().precision() + 2;
  if ( decimalPlaces < 0 ) decimalPlaces = 0;
  dialog.setLowerValue( QString::number( range.lowerValue(), 'f', decimalPlaces ) );
  dialog.setUpperValue( QString::number( range.upperValue(), 'f', decimalPlaces ) );

  if ( dialog.exec() == QDialog::Accepted )
  {
    double lowerValue = dialog.lowerValue().toDouble();
    double upperValue = dialog.upperValue().toDouble();
    mRenderer->updateRangeUpperValue( rangeIdx, upperValue );
    mRenderer->updateRangeLowerValue( rangeIdx, lowerValue );

    //If the boundaries have to stay linked, we update the ranges above and below, as well as their label if needed
    if ( cbxLinkBoundaries->isChecked() )
    {
      if ( rangeIdx > 0 )
      {
        mRenderer->updateRangeUpperValue( rangeIdx - 1, lowerValue );
      }

      if ( rangeIdx < mRenderer->ranges().size() - 1 )
      {
        mRenderer->updateRangeLowerValue( rangeIdx + 1, upperValue );
      }
    }
  }
  mHistogramWidget->refresh();
  emit widgetChanged();
}

void QgsGraduatedSymbolRendererWidget::addClass()
{
  mModel->addClass( mGraduatedSymbol );
  mHistogramWidget->refresh();
}

void QgsGraduatedSymbolRendererWidget::deleteClasses()
{
  QList<int> classIndexes = selectedClasses();
  mModel->deleteRows( classIndexes );
  mHistogramWidget->refresh();
}

void QgsGraduatedSymbolRendererWidget::deleteAllClasses()
{
  mModel->removeAllRows();
  mHistogramWidget->refresh();
}

bool QgsGraduatedSymbolRendererWidget::rowsOrdered()
{
  const QgsRangeList &ranges = mRenderer->ranges();
  bool ordered = true;
  for ( int i = 1;i < ranges.size();++i )
  {
    if ( ranges[i] < ranges[i-1] )
    {
      ordered = false;
      break;
    }
  }
  return ordered;
}

void QgsGraduatedSymbolRendererWidget::toggleBoundariesLink( bool linked )
{
  //If the checkbox controlling the link between boundaries was unchecked and we check it, we have to link the boundaries
  //This is done by updating all lower ranges to the upper value of the range above
  if ( linked )
  {
    if ( ! rowsOrdered() )
    {
      int result = QMessageBox::warning(
                     this,
                     tr( "Linked range warning" ),
                     tr( "Rows will be reordered before linking boundaries. Continue?" ),
                     QMessageBox::Ok | QMessageBox::Cancel );
      if ( result != QMessageBox::Ok )
      {
        cbxLinkBoundaries->setChecked( false );
        return;
      }
      mRenderer->sortByValue();
    }

    // Ok to proceed
    for ( int i = 1;i < mRenderer->ranges().size();++i )
    {
      mRenderer->updateRangeLowerValue( i, mRenderer->ranges()[i-1].upperValue() );
    }
    refreshSymbolView();
  }
}

void QgsGraduatedSymbolRendererWidget::changeCurrentValue( QStandardItem * item )
{
  if ( item->column() == 2 )
  {
    QString label = item->text();
    int idx = item->row();
    mRenderer->updateRangeLabel( idx, label );
  }
}

void QgsGraduatedSymbolRendererWidget::sizeScaleFieldChanged( const QString& fldName )
{
  mRenderer->setSizeScaleField( fldName );
}

void QgsGraduatedSymbolRendererWidget::scaleMethodChanged( QgsSymbol::ScaleMethod scaleMethod )
{
  mRenderer->setScaleMethod( scaleMethod );
}

void QgsGraduatedSymbolRendererWidget::labelFormatChanged()
{
  QgsRendererRangeLabelFormat labelFormat = QgsRendererRangeLabelFormat(
        txtLegendFormat->text(),
        spinPrecision->value(),
        cbxTrimTrailingZeroes->isChecked() );
  mRenderer->setLabelFormat( labelFormat, true );
  mModel->updateLabels();
}


QList<QgsSymbol*> QgsGraduatedSymbolRendererWidget::selectedSymbols()
{
  QList<QgsSymbol*> selectedSymbols;

  QItemSelectionModel* m = viewGraduated->selectionModel();
  QModelIndexList selectedIndexes = m->selectedRows( 1 );
  if ( m && !selectedIndexes.isEmpty() )
  {
    const QgsRangeList& ranges = mRenderer->ranges();
    QModelIndexList::const_iterator indexIt = selectedIndexes.constBegin();
    for ( ; indexIt != selectedIndexes.constEnd(); ++indexIt )
    {
      QStringList list = m->model()->data( *indexIt ).toString().split( ' ' );
      if ( list.size() < 3 )
      {
        continue;
      }

      double lowerBound = list.at( 0 ).toDouble();
      double upperBound = list.at( 2 ).toDouble();
      QgsSymbol* s = findSymbolForRange( lowerBound, upperBound, ranges );
      if ( s )
      {
        selectedSymbols.append( s );
      }
    }
  }
  return selectedSymbols;
}

QgsSymbol* QgsGraduatedSymbolRendererWidget::findSymbolForRange( double lowerBound, double upperBound, const QgsRangeList& ranges ) const
{
  int decimalPlaces = mRenderer->labelFormat().precision() + 2;
  if ( decimalPlaces < 0 )
    decimalPlaces = 0;
  double precision = 1.0 / qPow( 10, decimalPlaces );

  for ( QgsRangeList::const_iterator it = ranges.begin(); it != ranges.end(); ++it )
  {
    if ( qgsDoubleNear( lowerBound, it->lowerValue(), precision ) && qgsDoubleNear( upperBound, it->upperValue(), precision ) )
    {
      return it->symbol();
    }
  }
  return nullptr;
}

void QgsGraduatedSymbolRendererWidget::refreshSymbolView()
{
  if ( mModel )
  {
    mModel->updateSymbology();
  }
  mHistogramWidget->refresh();
  emit widgetChanged();
}

void QgsGraduatedSymbolRendererWidget::showSymbolLevels()
{
  showSymbolLevelsDialog( mRenderer );
}

void QgsGraduatedSymbolRendererWidget::rowsMoved()
{
  viewGraduated->selectionModel()->clear();
  if ( ! rowsOrdered() )
  {
    cbxLinkBoundaries->setChecked( false );
  }
  emit widgetChanged();
}

void QgsGraduatedSymbolRendererWidget::modelDataChanged()
{
  emit widgetChanged();
}

void QgsGraduatedSymbolRendererWidget::keyPressEvent( QKeyEvent* event )
{
  if ( !event )
  {
    return;
  }

  if ( event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier )
  {
    mCopyBuffer.clear();
    mCopyBuffer = selectedRanges();
  }
  else if ( event->key() == Qt::Key_V && event->modifiers() == Qt::ControlModifier )
  {
    QgsRangeList::const_iterator rIt = mCopyBuffer.constBegin();
    for ( ; rIt != mCopyBuffer.constEnd(); ++rIt )
    {
      mModel->addClass( *rIt );
    }
    emit widgetChanged();
  }
}
