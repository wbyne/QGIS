/***************************************************************************
    qgsrulebasedrendererwidget.h - Settings widget for rule-based renderer
    ---------------------
    begin                : May 2010
    copyright            : (C) 2010 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSRULEBASEDRENDERERV2WIDGET_H
#define QGSRULEBASEDRENDERERV2WIDGET_H

#include "qgsrendererwidget.h"

#include "qgsrulebasedrenderer.h"
class QMenu;
class QgsSymbolSelectorWidget;

///////

#include <QAbstractItemModel>

/* Features count fro rule */
struct QgsRuleBasedRendererCount
{
  int count; // number of features
  int duplicateCount; // number of features present also in other rule(s)
  // map of feature counts in other rules
  QMap<QgsRuleBasedRenderer::Rule*, int> duplicateCountMap;
};

/** \ingroup gui
Tree model for the rules:

(invalid)  == root node
 +--- top level rule
 +--- top level rule
*/
class GUI_EXPORT QgsRuleBasedRendererModel : public QAbstractItemModel
{
    Q_OBJECT

  public:
    QgsRuleBasedRendererModel( QgsRuleBasedRenderer* r );

    virtual Qt::ItemFlags flags( const QModelIndex &index ) const override;
    virtual QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const override;
    virtual QVariant headerData( int section, Qt::Orientation orientation,
                                 int role = Qt::DisplayRole ) const override;
    virtual int rowCount( const QModelIndex &parent = QModelIndex() ) const override;
    virtual int columnCount( const QModelIndex & = QModelIndex() ) const override;
    //! provide model index for parent's child item
    virtual QModelIndex index( int row, int column, const QModelIndex &parent = QModelIndex() ) const override;
    //! provide parent model index
    virtual QModelIndex parent( const QModelIndex &index ) const override;

    // editing support
    virtual bool setData( const QModelIndex & index, const QVariant & value, int role = Qt::EditRole ) override;

    // drag'n'drop support
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData( const QModelIndexList &indexes ) const override;
    bool dropMimeData( const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent ) override;

    bool removeRows( int row, int count, const QModelIndex & parent = QModelIndex() ) override;

    // new methods

    QgsRuleBasedRenderer::Rule* ruleForIndex( const QModelIndex& index ) const;

    void insertRule( const QModelIndex& parent, int before, QgsRuleBasedRenderer::Rule* newrule );
    void updateRule( const QModelIndex& parent, int row );
    // update rule and all its descendants
    void updateRule( const QModelIndex& index );
    void removeRule( const QModelIndex& index );

    void willAddRules( const QModelIndex& parent, int count ); // call beginInsertRows
    void finishedAddingRules(); // call endInsertRows

    //! @note not available in python bindungs
    void setFeatureCounts( const QMap<QgsRuleBasedRenderer::Rule*, QgsRuleBasedRendererCount>& theCountMap );
    void clearFeatureCounts();

  protected:
    QgsRuleBasedRenderer* mR;
    QMap<QgsRuleBasedRenderer::Rule*, QgsRuleBasedRendererCount> mFeatureCountMap;
};


///////

#include "ui_qgsrulebasedrendererv2widget.h"

/** \ingroup gui
 * \class QgsRuleBasedRendererWidget
 */
class GUI_EXPORT QgsRuleBasedRendererWidget : public QgsRendererWidget, private Ui::QgsRuleBasedRendererWidget
{
    Q_OBJECT

  public:

    static QgsRendererWidget* create( QgsVectorLayer* layer, QgsStyle* style, QgsFeatureRenderer* renderer );

    QgsRuleBasedRendererWidget( QgsVectorLayer* layer, QgsStyle* style, QgsFeatureRenderer* renderer );
    ~QgsRuleBasedRendererWidget();

    virtual QgsFeatureRenderer* renderer() override;

  public slots:

    void addRule();
    void editRule();
    void editRule( const QModelIndex& index );
    void removeRule();
    void countFeatures();
    void clearFeatureCounts() { mModel->clearFeatureCounts(); }

    void refineRuleScales();
    void refineRuleCategories();
    void refineRuleRanges();

    void setRenderingOrder();

    void currentRuleChanged( const QModelIndex& current = QModelIndex(), const QModelIndex& previous = QModelIndex() );
    void selectedRulesChanged();

    void saveSectionWidth( int section, int oldSize, int newSize );
    void restoreSectionWidths();

  protected:
    void refineRule( int type );
    //TODO QGIS 3.0 - remove index parameter
    void refineRuleCategoriesGui( const QModelIndexList& index );
    //TODO QGIS 3.0 - remove index parameter
    void refineRuleRangesGui( const QModelIndexList& index );
    void refineRuleScalesGui( const QModelIndexList& index );

    QgsRuleBasedRenderer::Rule* currentRule();

    QList<QgsSymbol*> selectedSymbols() override;
    QgsRuleBasedRenderer::RuleList selectedRules();
    void refreshSymbolView() override;
    void keyPressEvent( QKeyEvent* event ) override;

    QgsRuleBasedRenderer* mRenderer;
    QgsRuleBasedRendererModel* mModel;

    QMenu* mRefineMenu;
    QAction* mDeleteAction;

    QgsRuleBasedRenderer::RuleList mCopyBuffer;

  protected slots:
    void copy() override;
    void paste() override;

  private slots:
    void refineRuleCategoriesAccepted( QgsPanelWidget* panel );
    void refineRuleRangesAccepted( QgsPanelWidget* panel );
    void ruleWidgetPanelAccepted( QgsPanelWidget* panel );
    void liveUpdateRuleFromPanel();
};

///////

#include <QDialog>

#include "ui_qgsrendererrulepropsdialogbase.h"

/** \ingroup gui
 * \class QgsRendererRulePropsWidget
 */
class GUI_EXPORT QgsRendererRulePropsWidget : public QgsPanelWidget, private Ui::QgsRendererRulePropsWidget
{
    Q_OBJECT

  public:
    /**
       * Widget to edit the details of a rule based renderer rule.
       * @param rule The rule to edit.
       * @param layer The layer used to pull layer related information.
       * @param style The active QGIS style.
       * @param parent The parent widget.
       * @param mapCanvas The map canvas object.
       */
    QgsRendererRulePropsWidget( QgsRuleBasedRenderer::Rule* rule, QgsVectorLayer* layer, QgsStyle* style, QWidget* parent = nullptr, QgsMapCanvas* mapCanvas = nullptr );
    ~QgsRendererRulePropsWidget();

    /**
     * Return the current set rule.
     * @return The current rule.
     */
    QgsRuleBasedRenderer::Rule* rule() { return mRule; }

  public slots:

    /**
     * Test the filter that is set in the widget
     */
    void testFilter();

    /**
     * Open the expression builder widget to check if the
     */
    void buildExpression();

    /**
     * Apply any changes from the widget to the set rule.
     */
    void apply();
    /**
     * Set the widget in dock mode.
     * @param dockMode True for dock mode.
     */
    virtual void setDockMode( bool dockMode );

  protected:
    QgsRuleBasedRenderer::Rule* mRule; // borrowed
    QgsVectorLayer* mLayer;

    QgsSymbolSelectorWidget* mSymbolSelector;
    QgsSymbol* mSymbol; // a clone of original symbol

    QgsMapCanvas* mMapCanvas;
};

/** \ingroup gui
 * \class QgsRendererRulePropsDialog
 */
class GUI_EXPORT QgsRendererRulePropsDialog : public QDialog
{
    Q_OBJECT

  public:
    QgsRendererRulePropsDialog( QgsRuleBasedRenderer::Rule* rule, QgsVectorLayer* layer, QgsStyle* style, QWidget* parent = nullptr, QgsMapCanvas* mapCanvas = nullptr );
    ~QgsRendererRulePropsDialog();

    QgsRuleBasedRenderer::Rule* rule() { return mPropsWidget->rule(); }

  public slots:
    void testFilter();
    void buildExpression();
    void accept() override;

  private:
    QgsRendererRulePropsWidget* mPropsWidget;
    QDialogButtonBox* buttonBox;
};


#endif // QGSRULEBASEDRENDERERV2WIDGET_H
