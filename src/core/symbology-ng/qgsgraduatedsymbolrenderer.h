/***************************************************************************
    qgsgraduatedsymbolrenderer.h
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
#ifndef QGSGRADUATEDSYMBOLRENDERERV2_H
#define QGSGRADUATEDSYMBOLRENDERERV2_H

#include "qgssymbol.h"
#include "qgsrenderer.h"
#include "qgsexpression.h"
#include <QScopedPointer>
#include <QRegExp>

/** \ingroup core
 * \class QgsRendererRange
 */
class CORE_EXPORT QgsRendererRange
{
  public:
    QgsRendererRange();
    QgsRendererRange( double lowerValue, double upperValue, QgsSymbol* symbol, const QString& label, bool render = true );
    QgsRendererRange( const QgsRendererRange& range );

    ~QgsRendererRange() {}

    // default dtor is ok
    QgsRendererRange& operator=( QgsRendererRange range );

    bool operator<( const QgsRendererRange &other ) const;

    double lowerValue() const;
    double upperValue() const;

    QgsSymbol* symbol() const;
    QString label() const;

    void setSymbol( QgsSymbol* s );
    void setLabel( const QString& label );
    void setLowerValue( double lowerValue );
    void setUpperValue( double upperValue );

    // @note added in 2.5
    bool renderState() const;
    void setRenderState( bool render );

    // debugging
    QString dump() const;

    /** Creates a DOM element representing the range in SLD format.
     * @param doc DOM document
     * @param element destination DOM element
     * @param props graduated renderer properties
     * @param firstRange set to true if the range is the first range, where the lower value uses a <= test
     * rather than a < test.
     */
    void toSld( QDomDocument& doc, QDomElement &element, QgsStringMap props, bool firstRange = false ) const;

  protected:
    double mLowerValue, mUpperValue;
    QScopedPointer<QgsSymbol> mSymbol;
    QString mLabel;
    bool mRender;

    // for cpy+swap idiom
    void swap( QgsRendererRange & other );
};

typedef QList<QgsRendererRange> QgsRangeList;


/** \ingroup core
 * \class QgsRendererRangeLabelFormat
 * \note added in QGIS 2.6
 */
class CORE_EXPORT QgsRendererRangeLabelFormat
{
  public:
    QgsRendererRangeLabelFormat();
    QgsRendererRangeLabelFormat( const QString& format, int precision = 4, bool trimTrailingZeroes = false );

    bool operator==( const QgsRendererRangeLabelFormat & other ) const;
    bool operator!=( const QgsRendererRangeLabelFormat & other ) const;

    QString format() const { return mFormat; }
    void setFormat( const QString& format ) { mFormat = format; }

    int precision() const { return mPrecision; }
    void setPrecision( int precision );

    bool trimTrailingZeroes() const { return mTrimTrailingZeroes; }
    void setTrimTrailingZeroes( bool trimTrailingZeroes ) { mTrimTrailingZeroes = trimTrailingZeroes; }

    //! @note labelForLowerUpper in python bindings
    QString labelForRange( double lower, double upper ) const;
    QString labelForRange( const QgsRendererRange &range ) const;
    QString formatNumber( double value ) const;

    void setFromDomElement( QDomElement &element );
    void saveToDomElement( QDomElement &element );

    static int MaxPrecision;
    static int MinPrecision;

  protected:
    QString mFormat;
    int mPrecision;
    bool mTrimTrailingZeroes;
    // values used to manage number formatting - precision and trailing zeroes
    double mNumberScale;
    QString mNumberSuffix;
    QRegExp mReTrailingZeroes;
    QRegExp mReNegativeZero;
};

class QgsVectorLayer;
class QgsColorRamp;

/** \ingroup core
 * \class QgsGraduatedSymbolRenderer
 */
class CORE_EXPORT QgsGraduatedSymbolRenderer : public QgsFeatureRenderer
{
  public:

    QgsGraduatedSymbolRenderer( const QString& attrName = QString(), const QgsRangeList& ranges = QgsRangeList() );

    virtual ~QgsGraduatedSymbolRenderer();

    virtual QgsSymbol* symbolForFeature( QgsFeature& feature, QgsRenderContext &context ) override;
    virtual QgsSymbol* originalSymbolForFeature( QgsFeature& feature, QgsRenderContext &context ) override;
    virtual void startRender( QgsRenderContext& context, const QgsFields& fields ) override;
    virtual void stopRender( QgsRenderContext& context ) override;
    virtual QSet<QString> usedAttributes() const override;
    virtual QString dump() const override;
    virtual QgsGraduatedSymbolRenderer* clone() const override;
    virtual void toSld( QDomDocument& doc, QDomElement &element, QgsStringMap props = QgsStringMap() ) const override;
    virtual Capabilities capabilities() override { return SymbolLevels | Filter; }
    virtual QgsSymbolList symbols( QgsRenderContext &context ) override;

    QString classAttribute() const { return mAttrName; }
    void setClassAttribute( const QString& attr ) { mAttrName = attr; }

    const QgsRangeList& ranges() const { return mRanges; }

    bool updateRangeSymbol( int rangeIndex, QgsSymbol* symbol );
    bool updateRangeLabel( int rangeIndex, const QString& label );
    bool updateRangeUpperValue( int rangeIndex, double value );
    bool updateRangeLowerValue( int rangeIndex, double value );
    //! @note added in 2.5
    bool updateRangeRenderState( int rangeIndex, bool render );

    void addClass( QgsSymbol* symbol );
    //! @note available in python bindings as addClassRange
    void addClass( const QgsRendererRange& range );
    //! @note available in python bindings as addClassLowerUpper
    void addClass( double lower, double upper );

    /** Add a breakpoint by splitting existing classes so that the specified
     * value becomes a break between two classes.
     * @param breakValue position to insert break
     * @param updateSymbols set to true to reapply ramp colors to the new
     * symbol ranges
     * @note added in QGIS 2.9
     */
    void addBreak( double breakValue, bool updateSymbols = true );

    void deleteClass( int idx );
    void deleteAllClasses();

    //! Moves the category at index position from to index position to.
    void moveClass( int from, int to );

    /** Tests whether classes assigned to the renderer have ranges which overlap.
     * @returns true if ranges overlap
     * @note added in QGIS 2.10
     */
    bool rangesOverlap() const;

    /** Tests whether classes assigned to the renderer have gaps between the ranges.
     * @returns true if ranges have gaps
     * @note added in QGIS 2.10
     */
    bool rangesHaveGaps() const;

    void sortByValue( Qt::SortOrder order = Qt::AscendingOrder );
    void sortByLabel( Qt::SortOrder order = Qt::AscendingOrder );

    enum Mode
    {
      EqualInterval,
      Quantile,
      Jenks,
      StdDev,
      Pretty,
      Custom
    };

    Mode mode() const { return mMode; }
    void setMode( Mode mode ) { mMode = mode; }
    //! Recalculate classes for a layer
    //! @param vlayer  The layer being rendered (from which data values are calculated)
    //! @param mode    The calculation mode
    //! @param nclasses The number of classes to calculate (approximate for some modes)
    //! @note Added in 2.6
    void updateClasses( QgsVectorLayer *vlayer, Mode mode, int nclasses );

    //! Return the label format used to generate default classification labels
    //! @note Added in 2.6
    const QgsRendererRangeLabelFormat &labelFormat() const { return mLabelFormat; }
    //! Set the label format used to generate default classification labels
    //! @param labelFormat The string appended to classification labels
    //! @param updateRanges If true then ranges ending with the old unit string are updated to the new.
    //! @note Added in 2.6
    void setLabelFormat( const QgsRendererRangeLabelFormat &labelFormat, bool updateRanges = false );

    //! Reset the label decimal places to a numberbased on the minimum class interval
    //! @param updateRanges if true then ranges currently using the default label will be updated
    //! @note Added in 2.6
    void calculateLabelPrecision( bool updateRanges = true );

    /** Creates a new graduated renderer.
     * @param vlayer vector layer
     * @param attrName attribute to classify
     * @param classes number of classes
     * @param mode classification mode
     * @param symbol base symbol
     * @param ramp color ramp for classes
     * @param inverted set to true to invert color ramp
     * @param legendFormat
     * @returns new QgsGraduatedSymbolRenderer object
     */
    static QgsGraduatedSymbolRenderer* createRenderer(
      QgsVectorLayer* vlayer,
      const QString& attrName,
      int classes,
      Mode mode,
      QgsSymbol* symbol,
      QgsColorRamp* ramp,
      bool inverted = false,
      const QgsRendererRangeLabelFormat& legendFormat = QgsRendererRangeLabelFormat()
    );

    //! create renderer from XML element
    static QgsFeatureRenderer* create( QDomElement& element );

    virtual QDomElement save( QDomDocument& doc ) override;
    virtual QgsLegendSymbologyList legendSymbologyItems( QSize iconSize ) override;
    virtual QgsLegendSymbolList legendSymbolItems( double scaleDenominator = -1, const QString& rule = QString() ) override;
    QgsLegendSymbolListV2 legendSymbolItemsV2() const override;
    virtual QSet< QString > legendKeysForFeature( QgsFeature& feature, QgsRenderContext& context ) override;

    /** Returns the renderer's source symbol, which is the base symbol used for the each classes' symbol before applying
     * the classes' color.
     * @see setSourceSymbol()
     * @see sourceColorRamp()
     */
    QgsSymbol* sourceSymbol();

    /** Sets the source symbol for the renderer, which is the base symbol used for the each classes' symbol before applying
     * the classes' color.
     * @param sym source symbol, ownership is transferred to the renderer
     * @see sourceSymbol()
     * @see setSourceColorRamp()
     */
    void setSourceSymbol( QgsSymbol* sym );

    /** Returns the source color ramp, from which each classes' color is derived.
     * @see setSourceColorRamp()
     * @see sourceSymbol()
     */
    QgsColorRamp* sourceColorRamp();

    /** Sets the source color ramp.
     * @param ramp color ramp. Ownership is transferred to the renderer
     */
    void setSourceColorRamp( QgsColorRamp* ramp );

    //! @note added in 2.1
    bool invertedColorRamp() { return mInvertedColorRamp; }
    void setInvertedColorRamp( bool inverted ) { mInvertedColorRamp = inverted; }

    /** Update the color ramp used. Also updates all symbols colors.
     * Doesn't alter current breaks.
     * @param ramp color ramp. Ownership is transferred to the renderer
     * @param inverted set to true to invert ramp colors
     */
    void updateColorRamp( QgsColorRamp* ramp = nullptr, bool inverted = false );

    /** Update all the symbols but leave breaks and colors. This method also sets the source
     * symbol for the renderer.
     * @param sym source symbol to use for classes. Ownership is not transferred.
     * @see setSourceSymbol()
     */
    void updateSymbols( QgsSymbol* sym );

    //! set varying symbol size for classes
    //! @note the classes must already be set so that symbols exist
    //! @note added in 2.10
    void setSymbolSizes( double minSize, double maxSize );

    //! return the min symbol size when graduated by size
    //! @note added in 2.10
    double minSymbolSize() const;

    //! return the max symbol size when graduated by size
    //! @note added in 2.10
    double maxSymbolSize() const;

    enum GraduatedMethod {GraduatedColor = 0, GraduatedSize = 1 };

    //! return the method used for graduation (either size or color)
    //! @note added in 2.10
    GraduatedMethod graduatedMethod() const { return mGraduatedMethod; }

    //! set the method used for graduation (either size or color)
    //! @note added in 2.10
    void setGraduatedMethod( GraduatedMethod method ) { mGraduatedMethod = method; }

    virtual bool legendSymbolItemsCheckable() const override;
    virtual bool legendSymbolItemChecked( const QString& key ) override;
    virtual void checkLegendSymbolItem( const QString& key, bool state = true ) override;
    virtual void setLegendSymbolItem( const QString& key, QgsSymbol* symbol ) override;
    virtual QString legendClassificationAttribute() const override { return classAttribute(); }

    //! creates a QgsGraduatedSymbolRenderer from an existing renderer.
    //! @note added in 2.6
    //! @returns a new renderer if the conversion was possible, otherwise 0.
    static QgsGraduatedSymbolRenderer* convertFromRenderer( const QgsFeatureRenderer *renderer );

  protected:
    QString mAttrName;
    QgsRangeList mRanges;
    Mode mMode;
    QScopedPointer<QgsSymbol> mSourceSymbol;
    QScopedPointer<QgsColorRamp> mSourceColorRamp;
    bool mInvertedColorRamp;
    QgsRendererRangeLabelFormat mLabelFormat;

    QScopedPointer<QgsExpression> mExpression;
    GraduatedMethod mGraduatedMethod;
    //! attribute index (derived from attribute name in startRender)
    int mAttrNum;
    bool mCounting;

    QgsSymbol* symbolForValue( double value );

    /** Returns the matching legend key for a value.
     */
    QString legendKeyForValue( double value ) const;

    //! @note not available in Python bindings
    static const char * graduatedMethodStr( GraduatedMethod method );

  private:

    /** Returns calculated value used for classifying a feature.
     */
    QVariant valueForFeature( QgsFeature& feature, QgsRenderContext &context ) const;

};

#endif // QGSGRADUATEDSYMBOLRENDERERV2_H
