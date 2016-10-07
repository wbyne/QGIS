/***************************************************************************
                         qgscomposermap.h
                             -------------------
    begin                : January 2005
    copyright            : (C) 2005 by Radim Blazek
    email                : blazek@itc.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSCOMPOSERMAP_H
#define QGSCOMPOSERMAP_H

//#include "ui_qgscomposermapbase.h"
#include "qgscomposeritem.h"
#include "qgsrectangle.h"
#include <QFont>
#include <QGraphicsRectItem>

class QgsComposition;
class QgsComposerMapOverviewStack;
class QgsComposerMapOverview;
class QgsComposerMapGridStack;
class QgsComposerMapGrid;
class QgsMapToPixel;
class QDomNode;
class QDomDocument;
class QGraphicsView;
class QPainter;
class QgsFillSymbol;
class QgsLineSymbol;
class QgsVectorLayer;
class QgsAnnotation;

/** \ingroup core
 *  \class QgsComposerMap
 *  \brief Object representing map window.
 */

class CORE_EXPORT QgsComposerMap : public QgsComposerItem
{
    Q_OBJECT

  public:
    /** Constructor. */
    QgsComposerMap( QgsComposition *composition, int x, int y, int width, int height );
    /** Constructor. Settings are read from project. */
    QgsComposerMap( QgsComposition *composition );
    virtual ~QgsComposerMap();

    /** Return correct graphics item type. */
    virtual int type() const override { return ComposerMap; }

    /** \brief Preview style  */
    enum PreviewMode
    {
      Cache = 0,   // Use raster cache
      Render,      // Render the map
      Rectangle    // Display only rectangle
    };

    /** Scaling modes used for the serial rendering (atlas)
     */
    enum AtlasScalingMode
    {
      Fixed,      /*!< The current scale of the map is used for each feature of the atlas */
      Predefined, /*!< A scale is chosen from the predefined scales. The smallest scale from
                    the list of scales where the atlas feature is fully visible is chosen.
                    @see QgsAtlasComposition::setPredefinedScales.
                    @note This mode is only valid for polygon or line atlas coverage layers
                */
      Auto        /*!< The extent is adjusted so that each feature is fully visible.
                    A margin is applied around the center @see setAtlasMargin
                    @note This mode is only valid for polygon or line atlas coverage layers*/
    };

    /** \brief Draw to paint device
     *  @param painter painter
     *  @param extent map extent
     *  @param size size in scene coordinates
     *  @param dpi scene dpi
     *  @param forceWidthScale force wysiwyg line widths / marker sizes
     */
    void draw( QPainter *painter, const QgsRectangle& extent, QSizeF size, double dpi, double* forceWidthScale = nullptr );

    /** \brief Reimplementation of QCanvasItem::paint - draw on canvas */
    void paint( QPainter* painter, const QStyleOptionGraphicsItem* itemStyle, QWidget* pWidget ) override;

    /** \brief Create cache image */
    void cache();

    /** Return map settings that would be used for drawing of the map
     *  @note added in 2.6 */
    QgsMapSettings mapSettings( const QgsRectangle& extent, QSizeF size, int dpi ) const;

    /** \brief Get identification number*/
    int id() const {return mId;}

    /** True if a draw is already in progress*/
    bool isDrawing() const {return mDrawing;}

    /** Resizes an item in x- and y direction (canvas coordinates)*/
    void resize( double dx, double dy );

    /** Move content of map
     * @param dx move in x-direction (item and canvas coordinates)
     * @param dy move in y-direction (item and canvas coordinates)
     */
    void moveContent( double dx, double dy ) override;

    /** Zoom content of item. Does nothing per default (but implemented in composer map)
     * @param factor zoom factor, where > 1 results in a zoom in and < 1 results in a zoom out
     * @param point item point for zoom center
     * @param mode zoom mode
     * @note added in QGIS 2.5
     */
    virtual void zoomContent( const double factor, const QPointF point, const ZoomMode mode = QgsComposerItem::Zoom ) override;

    /** Sets new scene rectangle bounds and recalculates hight and extent*/
    void setSceneRect( const QRectF& rectangle ) override;

    /** \brief Scale */
    double scale() const;

    /** Sets new scale and changes only mExtent*/
    void setNewScale( double scaleDenominator, bool forceUpdate = true );

    /** Sets new extent for the map. This method may change the width or height of the map
     * item to ensure that the extent exactly matches the specified extent, with no
     * overlap or margin. This method implicitly alters the map scale.
     * @param extent new extent for the map
     * @see zoomToExtent
     */
    void setNewExtent( const QgsRectangle& extent );

    /** Zooms the map so that the specified extent is fully visible within the map item.
     * This method will not change the width or height of the map, and may result in
     * an overlap or margin from the specified extent. This method implicitly alters the
     * map scale.
     * @param extent new extent for the map
     * @see setNewExtent
     * @note added in QGIS 2.5
     */
    void zoomToExtent( const QgsRectangle& extent );

    /** Sets new Extent for the current atlas preview and changes width, height (and implicitely also scale).
      Atlas preview extents are only temporary, and are regenerated whenever the atlas feature changes
     */
    void setNewAtlasFeatureExtent( const QgsRectangle& extent );

    /** Returns a pointer to the current map extent, which is either the original user specified
     * extent or the temporary atlas-driven feature extent depending on the current atlas state
     * of the composition. Both a const and non-const version are included.
     * @returns pointer to current map extent
     * @see visibleExtentPolygon
     */
    const QgsRectangle* currentMapExtent() const;

    //! @note not available in python bindings
    QgsRectangle* currentMapExtent();

    PreviewMode previewMode() const {return mPreviewMode;}
    void setPreviewMode( PreviewMode m );

    /** Getter for flag that determines if the stored layer set should be used or the current layer set of the qgis mapcanvas */
    bool keepLayerSet() const {return mKeepLayerSet;}
    /** Setter for flag that determines if the stored layer set should be used or the current layer set of the qgis mapcanvas */
    void setKeepLayerSet( bool enabled ) {mKeepLayerSet = enabled;}

    /** Getter for stored layer set that is used if mKeepLayerSet is true */
    QStringList layerSet() const {return mLayerSet;}
    /** Setter for stored layer set that is used if mKeepLayerSet is true */
    void setLayerSet( const QStringList& layerSet ) {mLayerSet = layerSet;}
    /** Stores the current layer set of the qgis mapcanvas in mLayerSet*/
    void storeCurrentLayerSet();

    /** Getter for flag that determines if current styles of layers should be overridden by previously stored styles. @note added in 2.8 */
    bool keepLayerStyles() const { return mKeepLayerStyles; }
    /** Setter for flag that determines if current styles of layers should be overridden by previously stored styles. @note added in 2.8 */
    void setKeepLayerStyles( bool enabled ) { mKeepLayerStyles = enabled; }

    /** Getter for stored overrides of styles for layers. @note added in 2.8 */
    QMap<QString, QString> layerStyleOverrides() const { return mLayerStyleOverrides; }
    /** Setter for stored overrides of styles for layers. @note added in 2.8 */
    void setLayerStyleOverrides( const QMap<QString, QString>& overrides );
    /** Stores the current layer styles into style overrides. @note added in 2.8 */
    void storeCurrentLayerStyles();

    /** Whether the map should follow a map theme. If true, the layers and layer styles
     * will be used from given preset name (configured with setFollowVisibilityPresetName() method).
     * This means when preset's settings are changed, the new settings are automatically
     * picked up next time when rendering, without having to explicitly update them.
     * At most one of the flags keepLayerSet() and followVisibilityPreset() should be enabled
     * at any time since they are alternative approaches - if both are enabled,
     * following map theme has higher priority. If neither is enabled (or if preset name is not set),
     * map will use the same configuration as the map canvas uses.
     * @note added in 2.16 */
    bool followVisibilityPreset() const { return mFollowVisibilityPreset; }
    /** Sets whether the map should follow a map theme. See followVisibilityPreset() for more details.
     * @note added in 2.16 */
    void setFollowVisibilityPreset( bool follow ) { mFollowVisibilityPreset = follow; }
    /** Preset name that decides which layers and layer styles are used for map rendering. It is only
     * used when followVisibilityPreset() returns true.
     * @note added in 2.16 */
    QString followVisibilityPresetName() const { return mFollowVisibilityPresetName; }
    /** Sets preset name for map rendering. See followVisibilityPresetName() for more details.
     * @note added in 2.16 */
    void setFollowVisibilityPresetName( const QString& name ) { mFollowVisibilityPresetName = name; }

    // Set cache outdated
    void setCacheUpdated( bool u = false );

    QgsRectangle extent() const {return mExtent;}

    /** Sets offset values to shift image (useful for live updates when moving item content)*/
    void setOffset( double xOffset, double yOffset );

    /** True if composer map renders a WMS layer*/
    bool containsWmsLayer() const;

    /** True if composer map contains layers with blend modes or flattened layers for vectors */
    bool containsAdvancedEffects() const;

    /** Stores state in Dom node
     * @param elem is Dom element corresponding to 'Composer' tag
     * @param doc Dom document
     */
    bool writeXml( QDomElement& elem, QDomDocument & doc ) const override;

    /** Sets state from Dom document
     * @param itemElem is Dom node corresponding to 'ComposerMap' tag
     * @param doc is Dom document
     */
    bool readXml( const QDomElement& itemElem, const QDomDocument& doc ) override;

    /** Returns the map item's grid stack, which is used to control how grids
     * are drawn over the map's contents.
     * @returns pointer to grid stack
     * @see grid()
     * @note introduced in QGIS 2.5
     */
    QgsComposerMapGridStack* grids() { return mGridStack; }

    /** Returns the map item's first grid. This is a convenience function.
     * @returns pointer to first grid for map item
     * @see grids()
     * @note introduced in QGIS 2.5
     */
    QgsComposerMapGrid* grid();

    /** Returns the map item's overview stack, which is used to control how overviews
     * are drawn over the map's contents.
     * @returns pointer to overview stack
     * @see overview()
     * @note introduced in QGIS 2.5
     */
    QgsComposerMapOverviewStack* overviews() { return mOverviewStack; }

    /** Returns the map item's first overview. This is a convenience function.
     * @returns pointer to first overview for map item
     * @see overviews()
     * @note introduced in QGIS 2.5
     */
    QgsComposerMapOverview* overview();

    /** In case of annotations, the bounding rectangle can be larger than the map item rectangle */
    QRectF boundingRect() const override;

    /* reimplement setFrameOutlineWidth, so that updateBoundingRect() is called after setting the frame width */
    virtual void setFrameOutlineWidth( const double outlineWidth ) override;

    /** Sets rotation for the map - this does not affect the composer item shape, only the
     * way the map is drawn within the item
     * @note this function was added in version 2.1
     */
    void setMapRotation( double r );

    /** Returns the rotation used for drawing the map within the composer item
     * @returns rotation for map
     * @param valueType controls whether the returned value is the user specified rotation,
     * or the current evaluated rotation (which may be affected by data driven rotation
     * settings).
     */
    double mapRotation( QgsComposerObject::PropertyValueType valueType = QgsComposerObject::EvaluatedValue ) const;

    void updateItem() override;

    /** Sets canvas pointer (necessary to query and draw map canvas items)*/
    void setMapCanvas( QGraphicsView* canvas ) { mMapCanvas = canvas; }

    void setDrawCanvasItems( bool b ) { mDrawCanvasItems = b; }
    bool drawCanvasItems() const { return mDrawCanvasItems; }

    /** Returns the conversion factor map units -> mm*/
    double mapUnitsToMM() const;

    /** Sets mId to a number not yet used in the composition. mId is kept if it is not in use.
        Usually, this function is called before adding the composer map to the composition*/
    void assignFreeId();

    /** Returns whether the map extent is set to follow the current atlas feature.
     * @returns true if map will follow the current atlas feature.
     * @see setAtlasDriven
     * @see atlasScalingMode
     */
    bool atlasDriven() const { return mAtlasDriven; }

    /** Sets whether the map extent will follow the current atlas feature.
     * @param enabled set to true if the map extents should be set by the current atlas feature.
     * @see atlasDriven
     * @see setAtlasScalingMode
     */
    void setAtlasDriven( bool enabled );

    /** Returns the current atlas scaling mode. This controls how the map's extents
     * are calculated for the current atlas feature when an atlas composition
     * is enabled.
     * @returns the current scaling mode
     * @note this parameter is only used if atlasDriven() is true
     * @see setAtlasScalingMode
     * @see atlasDriven
     */
    AtlasScalingMode atlasScalingMode() const { return mAtlasScalingMode; }

    /** Sets the current atlas scaling mode. This controls how the map's extents
     * are calculated for the current atlas feature when an atlas composition
     * is enabled.
     * @param mode atlas scaling mode to set
     * @note this parameter is only used if atlasDriven() is true
     * @see atlasScalingMode
     * @see atlasDriven
     */
    void setAtlasScalingMode( AtlasScalingMode mode ) { mAtlasScalingMode = mode; }

    /** Returns the margin size (percentage) used when the map is in atlas mode.
     * @param valueType controls whether the returned value is the user specified atlas margin,
     * or the current evaluated atlas margin (which may be affected by data driven atlas margin
     * settings).
     * @returns margin size in percentage to leave around the atlas feature's extent
     * @note this is only used if atlasScalingMode() is Auto.
     * @see atlasScalingMode
     * @see setAtlasMargin
     */
    double atlasMargin( const QgsComposerObject::PropertyValueType valueType = QgsComposerObject::EvaluatedValue );

    /** Sets the margin size (percentage) used when the map is in atlas mode.
     * @param margin size in percentage to leave around the atlas feature's extent
     * @note this is only used if atlasScalingMode() is Auto.
     * @see atlasScalingMode
     * @see atlasMargin
     */
    void setAtlasMargin( double margin ) { mAtlasMargin = margin; }

    /** Sets whether updates to the composer map are enabled. */
    void setUpdatesEnabled( bool enabled ) { mUpdatesEnabled = enabled; }

    /** Returns whether updates to the composer map are enabled. */
    bool updatesEnabled() const { return mUpdatesEnabled; }

    /** Get the number of layers that this item requires for exporting as layers
     * @returns 0 if this item is to be placed on the same layer as the previous item,
     * 1 if it should be placed on its own layer, and >1 if it requires multiple export layers
     * @note this method was added in version 2.4
     */
    int numberExportLayers() const override;

    /** Returns a polygon representing the current visible map extent, considering map extents and rotation.
     * If the map rotation is 0, the result is the same as currentMapExtent
     * @returns polygon with the four corner points representing the visible map extent. The points are
     * clockwise, starting at the top-left point
     * @see currentMapExtent
     */
    QPolygonF visibleExtentPolygon() const;

    //overridden to show "Map 1" type names
    virtual QString displayName() const override;

    /** Returns extent that considers rotation and shift with mOffsetX / mOffsetY*/
    QPolygonF transformedMapPolygon() const;

    /** Transforms map coordinates to item coordinates (considering rotation and move offset)*/
    QPointF mapToItemCoords( QPointF mapCoords ) const;

    /** Calculates the extent to request and the yShift of the top-left point in case of rotation.
     * @note added in 2.6 */
    void requestedExtent( QgsRectangle& extent ) const;

    virtual QgsExpressionContext createExpressionContext() const override;

  signals:
    void extentChanged();

    /** Is emitted on rotation change to notify north arrow pictures*/
    void mapRotationChanged( double newRotation );

    /** Is emitted when the map has been prepared for atlas rendering, just before actual rendering*/
    void preparedForAtlas();

    /** Emitted when layer style overrides are changed... a means to let
     * associated legend items know they should update
     * @note added in 2.10
     */
    void layerStyleOverridesChanged();

  public slots:

    /** Forces an update of the cached map image*/
    void updateCachedImage();

    /** Updates the cached map image if the map is set to Render mode
     * @see updateCachedImage
     */
    void renderModeUpdateCachedImage();

    /** Updates the bounding rect of this item. Call this function before doing any changes related to annotation out of the map rectangle */
    void updateBoundingRect();

    virtual void refreshDataDefinedProperty( const QgsComposerObject::DataDefinedProperty property = QgsComposerObject::AllProperties, const QgsExpressionContext* context = nullptr ) override;

  protected slots:

    /** Called when layers are added or removed from the layer registry. Updates the maps
     * layer set and redraws the map if required.
     * @note added in QGIS 2.9
     */
    void layersChanged();

  private:

    /** Unique identifier*/
    int mId;

    QgsComposerMapGridStack* mGridStack;

    QgsComposerMapOverviewStack* mOverviewStack;

    // Map region in map units realy used for rendering
    // It can be the same as mUserExtent, but it can be bigger in on dimension if mCalculate==Scale,
    // so that full rectangle in paper is used.
    QgsRectangle mExtent;

    // Current temporary map region in map units. This is overwritten when atlas feature changes. It's also
    // used when the user changes the map extent and an atlas preview is enabled. This allows the user
    // to manually tweak each atlas preview page without affecting the actual original map extent.
    QgsRectangle mAtlasFeatureExtent;

    // Cache used in composer preview
    QImage mCacheImage;

    // Is cache up to date
    bool mCacheUpdated;

    /** \brief Preview style  */
    PreviewMode mPreviewMode;

    /** \brief Number of layers when cache was created  */
    int mNumCachedLayers;

    /** \brief set to true if in state of drawing. Concurrent requests to draw method are returned if set to true */
    bool mDrawing;

    /** Offset in x direction for showing map cache image*/
    double mXOffset;
    /** Offset in y direction for showing map cache image*/
    double mYOffset;

    /** Map rotation*/
    double mMapRotation;
    /** Temporary evaluated map rotation. Data defined rotation may mean this value
     * differs from mMapRotation*/
    double mEvaluatedMapRotation;

    /** Flag if layers to be displayed should be read from qgis canvas (true) or from stored list in mLayerSet (false)*/
    bool mKeepLayerSet;

    /** Stored layer list (used if layer live-link mKeepLayerSet is disabled)*/
    QStringList mLayerSet;

    bool mKeepLayerStyles;
    /** Stored style names (value) to be used with particular layer IDs (key) instead of default style */
    QMap<QString, QString> mLayerStyleOverrides;

    /** Whether layers and styles should be used from a preset (preset name is stored
     * in mVisibilityPresetName and may be overridden by data-defined expression).
     * This flag has higher priority than mKeepLayerSet. */
    bool mFollowVisibilityPreset;
    /** Map theme name to be used for map's layers and styles in case mFollowVisibilityPreset
     *  is true. May be overridden by data-defined expression. */
    QString mFollowVisibilityPresetName;

    /** Whether updates to the map are enabled */
    bool mUpdatesEnabled;

    /** Establishes signal/slot connection for update in case of layer change*/
    void connectUpdateSlot();

    /** Removes layer ids from mLayerSet that are no longer present in the qgis main map*/
    void syncLayerSet();

    /** Returns first map grid or creates an empty one if none*/
    const QgsComposerMapGrid* constFirstMapGrid() const;

    /** Returns first map overview or creates an empty one if none*/
    const QgsComposerMapOverview* constFirstMapOverview() const;

    /** Current bounding rectangle. This is used to check if notification to the graphics scene is necessary*/
    QRectF mCurrentRectangle;
    QGraphicsView* mMapCanvas;
    /** True if annotation items, rubber band, etc. from the main canvas should be displayed*/
    bool mDrawCanvasItems;

    /** Adjusts an extent rectangle to match the provided item width and height, so that extent
     * center of extent remains the same */
    void adjustExtentToItemShape( double itemWidth, double itemHeight, QgsRectangle& extent ) const;

    /** True if map is being controlled by an atlas*/
    bool mAtlasDriven;
    /** Current atlas scaling mode*/
    AtlasScalingMode mAtlasScalingMode;
    /** Margin size for atlas driven extents (percentage of feature size) - when in auto scaling mode*/
    double mAtlasMargin;

    void init();

    /** Resets the item tooltip to reflect current map id*/
    void updateToolTip();

    /** Returns a list of the layers to render for this map item*/
    QStringList layersToRender( const QgsExpressionContext* context = nullptr ) const;

    /** Returns current layer style overrides for this map item*/
    QMap<QString, QString> layerStyleOverridesToRender( const QgsExpressionContext& context ) const;

    /** Returns extent that considers mOffsetX / mOffsetY (during content move)*/
    QgsRectangle transformedExtent() const;

    /** MapPolygon variant using a given extent */
    void mapPolygon( const QgsRectangle& extent, QPolygonF& poly ) const;

    /** Scales a composer map shift (in MM) and rotates it by mRotation
        @param xShift in: shift in x direction (in item units), out: xShift in map units
        @param yShift in: shift in y direction (in item units), out: yShift in map units*/
    void transformShift( double& xShift, double& yShift ) const;

    void drawCanvasItems( QPainter* painter, const QStyleOptionGraphicsItem* itemStyle );
    void drawCanvasItem( const QgsAnnotation* item, QPainter* painter, const QStyleOptionGraphicsItem* itemStyle );
    QPointF composerMapPosForItem( const QgsAnnotation* item ) const;

    enum PartType
    {
      Background,
      Layer,
      Grid,
      OverviewMapExtent,
      Frame,
      SelectionBoxes
    };

    /** Test if a part of the copmosermap needs to be drawn, considering mCurrentExportLayer*/
    bool shouldDrawPart( PartType part ) const;

    /** Refresh the map's extents, considering data defined extent, scale and rotation
     * @param context expression context for evaluating data defined map parameters
     * @note this method was added in version 2.5
     */
    void refreshMapExtents( const QgsExpressionContext* context = nullptr );

    friend class QgsComposerMapOverview; //to access mXOffset, mYOffset
    friend class TestQgsComposerMap;
};

#endif

