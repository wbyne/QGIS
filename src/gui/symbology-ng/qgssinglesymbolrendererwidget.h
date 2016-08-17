/***************************************************************************
    qgssinglesymbolrendererwidget.h
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
#ifndef QGSSINGLESYMBOLRENDERERV2WIDGET_H
#define QGSSINGLESYMBOLRENDERERV2WIDGET_H

#include "qgsrendererwidget.h"

class QgsSingleSymbolRenderer;
class QgsSymbolSelectorWidget;

class QMenu;

/** \ingroup gui
 * \class QgsSingleSymbolRendererWidget
 */
class GUI_EXPORT QgsSingleSymbolRendererWidget : public QgsRendererWidget
{
    Q_OBJECT

  public:
    static QgsRendererWidget* create( QgsVectorLayer* layer, QgsStyle* style, QgsFeatureRenderer* renderer );

    QgsSingleSymbolRendererWidget( QgsVectorLayer* layer, QgsStyle* style, QgsFeatureRenderer* renderer );
    ~QgsSingleSymbolRendererWidget();

    virtual QgsFeatureRenderer* renderer() override;

    virtual void setMapCanvas( QgsMapCanvas* canvas ) override;

    /**
     * Set the widget in dock mode which tells the widget to emit panel
     * widgets and not open dialogs
     * @param dockMode True to enable dock mode.
     */
    virtual void setDockMode( bool dockMode ) override;

  public slots:
    void changeSingleSymbol();

    void sizeScaleFieldChanged( const QString& fldName );
    void scaleMethodChanged( QgsSymbol::ScaleMethod scaleMethod );

    void showSymbolLevels();

  protected:

    QgsSingleSymbolRenderer* mRenderer;
    QgsSymbolSelectorWidget* mSelector;
    QgsSymbol* mSingleSymbol;
};


#endif // QGSSINGLESYMBOLRENDERERV2WIDGET_H
