/***************************************************************************
                              qgspointdisplacementrenderer.cpp
                              --------------------------------
  begin                : January 26, 2010
  copyright            : (C) 2010 by Marco Hugentobler
  email                : marco at hugis dot net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSPOINTDISPLACEMENTRENDERER_H
#define QGSPOINTDISPLACEMENTRENDERER_H

#include "qgspointdistancerenderer.h"

/** \class QgsPointDisplacementRenderer
 * \ingroup core
 * A renderer that automatically displaces points with the same geographic location.
*/
class CORE_EXPORT QgsPointDisplacementRenderer: public QgsPointDistanceRenderer
{
  public:

    /** Placement methods for dispersing points
     */
    enum Placement
    {
      Ring, /*!< Place points in a single ring around group*/
      ConcentricRings /*!< Place points in concentric rings around group*/
    };

    /** Constructor for QgsPointDisplacementRenderer.
     * @param labelAttributeName optional attribute name for labeling points
     */
    QgsPointDisplacementRenderer( const QString& labelAttributeName = QString() );

    QgsPointDisplacementRenderer* clone() const override;
    virtual void startRender( QgsRenderContext& context, const QgsFields& fields ) override;
    void stopRender( QgsRenderContext& context ) override;
    QDomElement save( QDomDocument& doc ) override;
    virtual QSet<QString> usedAttributes() const override;

    //! Create a renderer from XML element
    static QgsFeatureRenderer* create( QDomElement& symbologyElem );

    /** Sets the line width for the displacement group circle.
     * @param width line width in mm
     * @see circleWidth()
     * @see setCircleColor()
     */
    void setCircleWidth( double width ) { mCircleWidth = width; }

    /** Returns the line width for the displacement group circle in mm.
     * @see setCircleWidth()
     * @see circleColor()
     */
    double circleWidth() const { return mCircleWidth; }

    /** Sets the color used for drawing the displacement group circle.
     * @param color circle color
     * @see circleColor()
     * @see setCircleWidth()
     */
    void setCircleColor( const QColor& color ) { mCircleColor = color; }

    /** Returns the color used for drawing the displacement group circle.
     * @see setCircleColor()
     * @see circleWidth()
     */
    QColor circleColor() const { return mCircleColor; }

    /** Sets a factor for increasing the ring size of displacement groups.
     * @param distance addition factor
     * @see circleRadiusAddition()
     */
    void setCircleRadiusAddition( double distance ) { mCircleRadiusAddition = distance; }

    /** Returns the factor for increasing the ring size of displacement groups.
     * @see setCircleRadiusAddition()
     */
    double circleRadiusAddition() const { return mCircleRadiusAddition; }

    /** Returns the placement method used for dispersing the points.
     * @see setPlacement()
     * @note added in QGIS 2.12
     */
    Placement placement() const { return mPlacement; }

    /** Sets the placement method used for dispersing the points.
     * @param placement placement method
     * @see placement()
     * @note added in QGIS 2.12
     */
    void setPlacement( Placement placement ) { mPlacement = placement; }

    /** Returns the symbol for the center of a displacement group (but not ownership of the symbol).
     * @see setCenterSymbol()
    */
    QgsMarkerSymbol* centerSymbol();

    /** Sets the center symbol for a displacement group.
     * @param symbol new center symbol. Ownership is transferred to the renderer.
     * @see centerSymbol()
    */
    void setCenterSymbol( QgsMarkerSymbol* symbol );

    /** Creates a QgsPointDisplacementRenderer from an existing renderer.
     * @note added in 2.5
     * @returns a new renderer if the conversion was possible, otherwise nullptr.
     */
    static QgsPointDisplacementRenderer* convertFromRenderer( const QgsFeatureRenderer *renderer );

  private:

    //! Center symbol for a displacement group
    QScopedPointer< QgsMarkerSymbol > mCenterSymbol;

    //! Displacement placement mode
    Placement mPlacement;

    //! Line width for the circle
    double mCircleWidth;
    //! Color to draw the circle
    QColor mCircleColor;
    //! Addition to the default circle radius
    double mCircleRadiusAddition;

    virtual void drawGroup( QPointF centerPoint, QgsRenderContext& context, const QgsPointDistanceRenderer::ClusteredGroup& group ) override;

    //helper functions
    void calculateSymbolAndLabelPositions( QgsSymbolRenderContext &symbolContext, QPointF centerPoint, int nPosition, double symbolDiagonal, QList<QPointF>& symbolPositions, QList<QPointF>& labelShifts , double &circleRadius ) const;
    void drawCircle( double radiusPainterUnits, QgsSymbolRenderContext& context, QPointF centerPoint, int nSymbols );
    void drawSymbols( const ClusteredGroup& group, QgsRenderContext& context, const QList<QPointF>& symbolPositions );
};

#endif // QGSPOINTDISPLACEMENTRENDERER_H
