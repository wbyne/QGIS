/***************************************************************************
    qgsvectorcolorbrewercolorrampdialog.h
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

#ifndef QGSVECTORCOLORBREWERCOLORRAMPV2DIALOG_H
#define QGSVECTORCOLORBREWERCOLORRAMPV2DIALOG_H

#include <QDialog>

#include "ui_qgsvectorcolorbrewercolorrampv2dialogbase.h"

class QgsVectorColorBrewerColorRamp;

/** \ingroup gui
 * \class QgsVectorColorBrewerColorRampDialog
 */
class GUI_EXPORT QgsVectorColorBrewerColorRampDialog : public QDialog, private Ui::QgsVectorColorBrewerColorRampDialogBase
{
    Q_OBJECT

  public:
    QgsVectorColorBrewerColorRampDialog( QgsVectorColorBrewerColorRamp* ramp, QWidget* parent = nullptr );

  public slots:
    void setSchemeName();
    void setColors();

    void populateVariants();

  protected:

    void updatePreview();

    QgsVectorColorBrewerColorRamp* mRamp;
};

#endif
