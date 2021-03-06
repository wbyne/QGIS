/***************************************************************************
    qgsheatmaprendererwidget.h
    --------------------------
    begin                : November 2014
    copyright            : (C) 2014 Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSHEATMAPRENDERERWIDGET_H
#define QGSHEATMAPRENDERERWIDGET_H

#include "ui_qgsheatmaprendererwidgetbase.h"
#include "qgsrendererwidget.h"

class QMenu;
class QgsHeatmapRenderer;

/** \ingroup gui
 * \class QgsHeatmapRendererWidget
 */
class GUI_EXPORT QgsHeatmapRendererWidget : public QgsRendererWidget, private Ui::QgsHeatmapRendererWidgetBase, private QgsExpressionContextGenerator
{
    Q_OBJECT

  public:
    /** Static creation method
     * @param layer the layer where this renderer is applied
     * @param style
     * @param renderer the mask renderer (will not take ownership)
     */
    static QgsRendererWidget* create( QgsVectorLayer* layer, QgsStyle* style, QgsFeatureRenderer* renderer );

    /** Constructor
     * @param layer the layer where this renderer is applied
     * @param style
     * @param renderer the mask renderer (will not take ownership)
     */
    QgsHeatmapRendererWidget( QgsVectorLayer* layer, QgsStyle* style, QgsFeatureRenderer* renderer );

    /** @returns the current feature renderer */
    virtual QgsFeatureRenderer* renderer() override;

    virtual void setContext( const QgsSymbolWidgetContext& context ) override;

  private:
    QgsHeatmapRenderer* mRenderer;

    QgsExpressionContext createExpressionContext() const override;

  private slots:

    void applyColorRamp();
    void on_mRadiusUnitWidget_changed();
    void on_mRadiusSpinBox_valueChanged( double d );
    void on_mMaxSpinBox_valueChanged( double d );
    void on_mQualitySlider_valueChanged( int v );
    void on_mInvertCheckBox_toggled( bool v );
    void weightExpressionChanged( const QString& expression );

};


#endif // QGSHEATMAPRENDERERWIDGET_H
