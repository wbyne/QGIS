/***************************************************************************
    qgsrenderer.h
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

#ifndef QGSRENDERERV2_H
#define QGSRENDERERV2_H

#include "qgis.h"
#include "qgsrectangle.h"
#include "qgsrendercontext.h"
#include "qgssymbol.h"
#include "qgsfield.h"
#include "qgsfeaturerequest.h"

#include <QList>
#include <QString>
#include <QVariant>
#include <QPair>
#include <QPixmap>
#include <QDomDocument>
#include <QDomElement>

class QgsFeature;
class QgsVectorLayer;
class QgsPaintEffect;

typedef QMap<QString, QString> QgsStringMap;

typedef QList<QgsSymbol*> QgsSymbolList;
typedef QMap<QString, QgsSymbol* > QgsSymbolMap;

typedef QList< QPair<QString, QPixmap> > QgsLegendSymbologyList;
typedef QList< QPair<QString, QgsSymbol*> > QgsLegendSymbolList;

#include "qgslegendsymbolitem.h"


#define RENDERER_TAG_NAME   "renderer-v2"

////////
// symbol levels

/** \ingroup core
 * \class QgsSymbolLevelItem
 */
class CORE_EXPORT QgsSymbolLevelItem
{
  public:
    QgsSymbolLevelItem( QgsSymbol* symbol, int layer )
        : mSymbol( symbol )
        , mLayer( layer )
    {}
    QgsSymbol* symbol() { return mSymbol; }
    int layer() { return mLayer; }
  protected:
    QgsSymbol* mSymbol;
    int mLayer;
};

// every level has list of items: symbol + symbol layer num
typedef QList< QgsSymbolLevelItem > QgsSymbolLevel;

// this is a list of levels
typedef QList< QgsSymbolLevel > QgsSymbolLevelOrder;


//////////////
// renderers

/** \ingroup core
 * \class QgsFeatureRenderer
 */
class CORE_EXPORT QgsFeatureRenderer
{
  public:
    // renderer takes ownership of its symbols!

    //! return a new renderer - used by default in vector layers
    static QgsFeatureRenderer* defaultRenderer( QgsWkbTypes::GeometryType geomType );

    QString type() const { return mType; }

    /** To be overridden
     *
     * Must be called between startRender() and stopRender() calls.
     * @param feature feature
     * @return returns pointer to symbol or 0 if symbol was not found
     * @deprecated use symbolForFeature( QgsFeature& feature, QgsRenderContext& context ) instead
     */
    Q_DECL_DEPRECATED virtual QgsSymbol* symbolForFeature( QgsFeature& feature );

    /** To be overridden
     *
     * Must be called between startRender() and stopRender() calls.
     * @param feature feature
     * @param context render context
     * @return returns pointer to symbol or 0 if symbol was not found
     * @note added in QGIS 2.12
     * @note available in Python bindings as symbolForFeature2
     */
    // TODO - QGIS 3.0 make pure virtual when above method is removed
    // TODO - QGIS 3.0 change PyName to symbolForFeature when deprecated method is removed
    virtual QgsSymbol* symbolForFeature( QgsFeature& feature, QgsRenderContext& context );

    /**
     * Return symbol for feature. The difference compared to symbolForFeature() is that it returns original
     * symbol which can be used as an identifier for renderer's rule - the former may return a temporary replacement
     * of a symbol for use in rendering.
     * @note added in 2.6
     * @deprecated use originalSymbolForFeature( QgsFeature& feature, QgsRenderContext& context ) instead
     */
    Q_DECL_DEPRECATED virtual QgsSymbol* originalSymbolForFeature( QgsFeature& feature );

    /**
     * Return symbol for feature. The difference compared to symbolForFeature() is that it returns original
     * symbol which can be used as an identifier for renderer's rule - the former may return a temporary replacement
     * of a symbol for use in rendering.
     * @note added in 2.12
     * @note available in Python bindings as originalSymbolForFeature2
     */
    //TODO - QGIS 3.0 change PyName to originalSymbolForFeature when deprecated method is removed
    virtual QgsSymbol* originalSymbolForFeature( QgsFeature& feature, QgsRenderContext& context );

    /**
     * Return legend keys matching a specified feature.
     * @note added in 2.14
     */
    virtual QSet< QString > legendKeysForFeature( QgsFeature& feature, QgsRenderContext& context );

    /**
     * Needs to be called when a new render cycle is started
     *
     * @param context  Additional information passed to the renderer about the job which will be rendered
     * @param fields   The fields available for rendering
     * @return         Information passed back from the renderer that can e.g. be used to reduce the amount of requested features
     */
    virtual void startRender( QgsRenderContext& context, const QgsFields& fields ) = 0;

    //! @deprecated since 2.4 - not using QgsVectorLayer directly anymore
    Q_DECL_DEPRECATED virtual void startRender( QgsRenderContext& context, const QgsVectorLayer* vlayer );

    /**
     * Needs to be called when a render cycle has finished to clean up.
     */
    virtual void stopRender( QgsRenderContext& context ) = 0;

    /**
     * If a renderer does not require all the features this method may be overridden
     * and return an expression used as where clause.
     * This will be called once after {@link startRender()} and before the first call
     * to {@link renderFeature()}.
     * By default this returns a null string and all features will be requested.
     * You do not need to specify the extent in here, this is taken care of separately and
     * will be combined with a filter returned from this method.
     *
     * @return An expression used as where clause
     */
    virtual QString filter( const QgsFields& fields = QgsFields() ) { Q_UNUSED( fields ); return QString::null; }

    /**
     * Return a list of attributes required by this renderer. Attributes not listed in here may
     * not have been requested from the provider at rendering time.
     *
     * @return A set of attributes
     */
    // TODO QGIS3: Change QList to QSet
    virtual QList<QString> usedAttributes() = 0;

    /**
     * Returns true if this renderer requires the geometry to apply the filter.
     */
    virtual bool filterNeedsGeometry() const;

    virtual ~QgsFeatureRenderer();

    /**
     * Create a deep copy of this renderer. Should be implemented by all subclasses
     * and generate a proper subclass.
     *
     * @return A copy of this renderer
     */
    virtual QgsFeatureRenderer* clone() const = 0;

    /**
     * Render a feature using this renderer in the given context.
     * Must be called between startRender() and stopRender() calls.
     * Default implementation renders a symbol as determined by symbolForFeature() call.
     * Returns true if the feature has been returned (this is used for example
     * to determine whether the feature may be labelled).
     *
     * If layer is not -1, the renderer should draw only a particula layer from symbols
     * (in order to support symbol level rendering).
     */
    virtual bool renderFeature( QgsFeature& feature, QgsRenderContext& context, int layer = -1, bool selected = false, bool drawVertexMarker = false );

    //! Returns debug information about this renderer
    virtual QString dump() const;

    /**
     * Used to specify details about a renderer.
     * Is returned from the capabilities() method.
     */
    enum Capability
    {
      SymbolLevels          = 1,      //!< rendering with symbol levels (i.e. implements symbols(), symbolForFeature())
      RotationField         = 1 << 1, //!< rotate symbols by attribute value
      MoreSymbolsPerFeature = 1 << 2, //!< may use more than one symbol to render a feature: symbolsForFeature() will return them
      Filter                = 1 << 3, //!< features may be filtered, i.e. some features may not be rendered (categorized, rule based ...)
      ScaleDependent        = 1 << 4  //!< depends on scale if feature will be rendered (rule based )
    };

    Q_DECLARE_FLAGS( Capabilities, Capability )

    /**
     * Returns details about internals of this renderer.
     *
     * E.g. if you only want to deal with visible features:
     *
     * ~~~{.py}
     * if not renderer.capabilities().testFlag(QgsFeatureRenderer.Filter) or renderer.willRenderFeature(feature, context):
     *     deal_with_my_feature()
     * else:
     *     skip_the_curren_feature()
     * ~~~
     */
    virtual Capabilities capabilities() { return 0; }

    /** For symbol levels
     * @deprecated use symbols( QgsRenderContext& context ) instead
     */
    Q_DECL_DEPRECATED virtual QgsSymbolList symbols();

    /** Returns list of symbols used by the renderer.
     * @param context render context
     * @note added in QGIS 2.12
     * @note available in Python bindings as symbols2
     */
    //TODO - QGIS 3.0 change PyName to symbols when deprecated method is removed
    virtual QgsSymbolList symbols( QgsRenderContext& context );

    bool usingSymbolLevels() const { return mUsingSymbolLevels; }
    void setUsingSymbolLevels( bool usingSymbolLevels ) { mUsingSymbolLevels = usingSymbolLevels; }

    //! create a renderer from XML element
    static QgsFeatureRenderer* load( QDomElement& symbologyElem );

    //! store renderer info to XML element
    virtual QDomElement save( QDomDocument& doc );

    //! create the SLD UserStyle element following the SLD v1.1 specs
    //! @deprecated since 2.8 - use the other override with styleName
    Q_DECL_DEPRECATED virtual QDomElement writeSld( QDomDocument& doc, const QgsVectorLayer &layer ) const;
    //! create the SLD UserStyle element following the SLD v1.1 specs with the given name
    //! @note added in 2.8
    virtual QDomElement writeSld( QDomDocument& doc, const QString& styleName ) const;

    /** Create a new renderer according to the information contained in
     * the UserStyle element of a SLD style document
     * @param node the node in the SLD document whose the UserStyle element
     * is a child
     * @param geomType the geometry type of the features, used to convert
     * Symbolizer elements
     * @param errorMessage it will contain the error message if something
     * went wrong
     * @return the renderer
     */
    static QgsFeatureRenderer* loadSld( const QDomNode &node, QgsWkbTypes::GeometryType geomType, QString &errorMessage );

    //! used from subclasses to create SLD Rule elements following SLD v1.1 specs
    virtual void toSld( QDomDocument& doc, QDomElement &element ) const
    { element.appendChild( doc.createComment( QString( "FeatureRendererV2 %1 not implemented yet" ).arg( type() ) ) ); }

    //! return a list of symbology items for the legend
    virtual QgsLegendSymbologyList legendSymbologyItems( QSize iconSize );

    //! items of symbology items in legend should be checkable
    //! @note added in 2.5
    virtual bool legendSymbolItemsCheckable() const;

    //! items of symbology items in legend is checked
    //! @note added in 2.5
    virtual bool legendSymbolItemChecked( const QString& key );

    //! item in symbology was checked
    //! @note added in 2.5
    virtual void checkLegendSymbolItem( const QString& key, bool state = true );

    /** Sets the symbol to be used for a legend symbol item.
     * @param key rule key for legend symbol
     * @param symbol new symbol for legend item. Ownership is transferred to renderer.
     * @note added in QGIS 2.14
     */
    virtual void setLegendSymbolItem( const QString& key, QgsSymbol* symbol );

    //! return a list of item text / symbol
    //! @note not available in python bindings
    virtual QgsLegendSymbolList legendSymbolItems( double scaleDenominator = -1, const QString& rule = "" );

    //! Return a list of symbology items for the legend. Better choice than legendSymbolItems().
    //! Default fallback implementation just uses legendSymbolItems() implementation
    //! @note added in 2.6
    virtual QgsLegendSymbolListV2 legendSymbolItemsV2() const;

    //! If supported by the renderer, return classification attribute for the use in legend
    //! @note added in 2.6
    virtual QString legendClassificationAttribute() const { return QString(); }

    //! set type and size of editing vertex markers for subsequent rendering
    void setVertexMarkerAppearance( int type, int size );

    //! return rotation field name (or empty string if not set or not supported by renderer)
    //! @deprecated use the symbol's methods instead
    Q_DECL_DEPRECATED virtual QString rotationField() const { return QString(); }

    //! sets rotation field of renderer (if supported by the renderer)
    //! @deprecated use the symbol's methods instead
    Q_DECL_DEPRECATED virtual void setRotationField( const QString& fieldName ) { Q_UNUSED( fieldName ); }

    /** Returns whether the renderer will render a feature or not.
     * Must be called between startRender() and stopRender() calls.
     * Default implementation uses symbolForFeature().
     * @deprecated use willRenderFeature( QgsFeature& feat, QgsRenderContext& context ) instead
     */
    Q_DECL_DEPRECATED virtual bool willRenderFeature( QgsFeature& feat );

    /** Returns whether the renderer will render a feature or not.
     * Must be called between startRender() and stopRender() calls.
     * Default implementation uses symbolForFeature().
     * @note added in QGIS 2.12
     * @note available in Python bindings as willRenderFeature2
     */
    //TODO - QGIS 3.0 change PyName to willRenderFeature when deprecated method is removed
    virtual bool willRenderFeature( QgsFeature& feat, QgsRenderContext& context );

    /** Returns list of symbols used for rendering the feature.
     * For renderers that do not support MoreSymbolsPerFeature it is more efficient
     * to use symbolForFeature()
     * @deprecated use symbolsForFeature( QgsFeature& feat, QgsRenderContext& context ) instead
     */
    Q_DECL_DEPRECATED virtual QgsSymbolList symbolsForFeature( QgsFeature& feat );

    /** Returns list of symbols used for rendering the feature.
     * For renderers that do not support MoreSymbolsPerFeature it is more efficient
     * to use symbolForFeature()
     * @note added in QGIS 2.12
     * @note available in Python bindings as symbolsForFeature2
     */
    //TODO - QGIS 3.0 change PyName to symbolsForFeature when deprecated method is removed
    virtual QgsSymbolList symbolsForFeature( QgsFeature& feat, QgsRenderContext& context );

    /** Equivalent of originalSymbolsForFeature() call
     * extended to support renderers that may use more symbols per feature - similar to symbolsForFeature()
     * @note added in 2.6
     * @deprecated use originalSymbolsForFeature( QgsFeature& feat, QgsRenderContext& context ) instead
     */
    Q_DECL_DEPRECATED virtual QgsSymbolList originalSymbolsForFeature( QgsFeature& feat );

    /** Equivalent of originalSymbolsForFeature() call
     * extended to support renderers that may use more symbols per feature - similar to symbolsForFeature()
     * @note added in 2.12
     * @note available in Python bindings as originalSymbolsForFeature2
     */
    //TODO - QGIS 3.0 change PyName to symbolsForFeature when deprecated method is removed
    virtual QgsSymbolList originalSymbolsForFeature( QgsFeature& feat, QgsRenderContext& context );

    /** Allows for a renderer to modify the extent of a feature request prior to rendering
     * @param extent reference to request's filter extent. Modify extent to change the
     * extent of feature request
     * @param context render context
     * @note added in QGIS 2.7
     */
    virtual void modifyRequestExtent( QgsRectangle& extent, QgsRenderContext& context ) { Q_UNUSED( extent ); Q_UNUSED( context ); }

    /** Returns the current paint effect for the renderer.
     * @returns paint effect
     * @note added in QGIS 2.9
     * @see setPaintEffect
     */
    QgsPaintEffect* paintEffect() const;

    /** Sets the current paint effect for the renderer.
     * @param effect paint effect. Ownership is transferred to the renderer.
     * @note added in QGIS 2.9
     * @see paintEffect
     */
    void setPaintEffect( QgsPaintEffect* effect );

    /** Returns whether the renderer must render as a raster.
     * @note added in QGIS 2.12
     * @see setForceRasterRender
     */
    bool forceRasterRender() const { return mForceRaster; }

    /** Sets whether the renderer should be rendered to a raster destination.
     * @param forceRaster set to true if renderer must be drawn on a raster surface.
     * This may be desirable for highly detailed layers where rendering as a vector
     * would result in a large, complex vector output.
     * @see forceRasterRender
     * @note added in QGIS 2.12
     */
    void setForceRasterRender( bool forceRaster ) { mForceRaster = forceRaster; }

    /**
     * Get the order in which features shall be processed by this renderer.
     * @note added in QGIS 2.14
     * @note this property has no effect if orderByEnabled() is false
     * @see orderByEnabled()
     */
    QgsFeatureRequest::OrderBy orderBy() const;

    /**
     * Define the order in which features shall be processed by this renderer.
     * @note this property has no effect if orderByEnabled() is false
     * @note added in QGIS 2.14
     * @see setOrderByEnabled()
     */
    void setOrderBy( const QgsFeatureRequest::OrderBy& orderBy );

    /**
     * Returns whether custom ordering will be applied before features are processed by this renderer.
     * @note added in QGIS 2.14
     * @see orderBy()
     * @see setOrderByEnabled()
     */
    bool orderByEnabled() const;

    /**
     * Sets whether custom ordering should be applied before features are processed by this renderer.
     * @param enabled set to true to enable custom feature ordering
     * @note added in QGIS 2.14
     * @see setOrderBy()
     * @see orderByEnabled()
     */
    void setOrderByEnabled( bool enabled );

    /** Sets an embedded renderer (subrenderer) for this feature renderer. The base class implementation
     * does nothing with subrenderers, but individual derived classes can use these to modify their behaviour.
     * @param subRenderer the embedded renderer. Ownership will be transferred.
     * @see embeddedRenderer()
     * @note added in QGIS 2.16
     */
    virtual void setEmbeddedRenderer( QgsFeatureRenderer* subRenderer ) { delete subRenderer; }

    /** Returns the current embedded renderer (subrenderer) for this feature renderer. The base class
     * implementation does not use subrenderers and will always return null.
     * @see setEmbeddedRenderer()
     * @note added in QGIS 2.16
     */
    virtual const QgsFeatureRenderer* embeddedRenderer() const { return nullptr; }

  protected:
    QgsFeatureRenderer( const QString& type );

    void renderFeatureWithSymbol( QgsFeature& feature,
                                  QgsSymbol* symbol,
                                  QgsRenderContext& context,
                                  int layer,
                                  bool selected,
                                  bool drawVertexMarker );

    //! render editing vertex marker at specified point
    void renderVertexMarker( QPointF pt, QgsRenderContext& context );
    //! render editing vertex marker for a polyline
    void renderVertexMarkerPolyline( QPolygonF& pts, QgsRenderContext& context );
    //! render editing vertex marker for a polygon
    void renderVertexMarkerPolygon( QPolygonF& pts, QList<QPolygonF>* rings, QgsRenderContext& context );

    /**
     * Creates a point in screen coordinates from a wkb string in map
     * coordinates
     */
    static QgsConstWkbPtr _getPoint( QPointF& pt, QgsRenderContext& context, QgsConstWkbPtr& wkb );

    /**
     * Creates a line string in screen coordinates from a wkb string in map
     * coordinates
     */
    static QgsConstWkbPtr _getLineString( QPolygonF& pts, QgsRenderContext& context, QgsConstWkbPtr& wkb, bool clipToExtent = true );

    /**
     * Creates a polygon in screen coordinates from a wkb string in map
     * coordinates
     */
    static QgsConstWkbPtr _getPolygon( QPolygonF& pts, QList<QPolygonF>& holes, QgsRenderContext& context, QgsConstWkbPtr& wkb, bool clipToExtent = true );

    void setScaleMethodToSymbol( QgsSymbol* symbol, int scaleMethod );

    /**
     * Clones generic renderer data to another renderer.
     * Currently clones
     *  * Order By
     *  * Paint Effect
     *
     * @param destRenderer destination renderer for copied effect
     */
    void copyRendererData( QgsFeatureRenderer *destRenderer ) const;

    /** Copies paint effect of this renderer to another renderer
     * @param destRenderer destination renderer for copied effect
     * @deprecated use copyRendererData instead
     */
    Q_DECL_DEPRECATED void copyPaintEffect( QgsFeatureRenderer *destRenderer ) const;

    QString mType;

    bool mUsingSymbolLevels;

    /** The current type of editing marker */
    int mCurrentVertexMarkerType;
    /** The current size of editing marker */
    int mCurrentVertexMarkerSize;

    QgsPaintEffect* mPaintEffect;

    bool mForceRaster;

    /** @note this function is used to convert old sizeScale expresssions to symbol
     * level DataDefined size
     */
    static void convertSymbolSizeScale( QgsSymbol * symbol, QgsSymbol::ScaleMethod method, const QString & field );
    /** @note this function is used to convert old rotations expresssions to symbol
     * level DataDefined angle
     */
    static void convertSymbolRotation( QgsSymbol * symbol, const QString & field );

    QgsFeatureRequest::OrderBy mOrderBy;

    bool mOrderByEnabled;

  private:
    Q_DISABLE_COPY( QgsFeatureRenderer )
};

Q_DECLARE_OPERATORS_FOR_FLAGS( QgsFeatureRenderer::Capabilities )

// for some reason SIP compilation fails if these lines are not included:
class QgsRendererWidget;
class QgsPaintEffectWidget;

#endif // QGSRENDERERV2_H
