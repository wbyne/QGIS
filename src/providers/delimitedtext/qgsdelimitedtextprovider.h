/***************************************************************************
      qgsdelimitedtextprovider.h  -  Data provider for delimited text
                             -------------------
    begin                : 2004-02-27
    copyright            : (C) 2004 by Gary E.Sherman
    email                : sherman at mrcc.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSDELIMITEDTEXTPROVIDER_H
#define QGSDELIMITEDTEXTPROVIDER_H

#include "qgsvectordataprovider.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsdelimitedtextfile.h"
#include "qgsfield.h"

#include <QStringList>

class QgsFeature;
class QgsField;
class QgsGeometry;
class QgsPoint;
class QFile;
class QTextStream;

class QgsDelimitedTextFeatureIterator;
class QgsExpression;
class QgsSpatialIndex;

/**
 * \class QgsDelimitedTextProvider
 * \brief Data provider for delimited text files.
 *
 * The provider needs to know both the path to the text file and
 * the delimiter to use. Since the means to add a layer is fairly
 * rigid, we must provide this information encoded in a form that
 * the provider can decipher and use.
 *
 * The uri must defines the file path and the parameters used to
 * interpret the contents of the file.
 *
 * Example uri = "/home/foo/delim.txt?delimiter=|"*
 *
 * For detailed information on the uri format see the QGSVectorLayer
 * documentation.  Note that the interpretation of the URI is split
 * between QgsDelimitedTextFile and QgsDelimitedTextProvider.
 *
 */
class QgsDelimitedTextProvider : public QgsVectorDataProvider
{
    Q_OBJECT

  public:

    /**
     * Regular expression defining possible prefixes to WKT string,
     * (EWKT srid, Informix SRID)
     */
    static QRegExp WktPrefixRegexp;
    static QRegExp CrdDmsRegexp;

    enum GeomRepresentationType
    {
      GeomNone,
      GeomAsXy,
      GeomAsWkt
    };

    explicit QgsDelimitedTextProvider( const QString& uri = QString() );

    virtual ~QgsDelimitedTextProvider();

    /* Implementation of functions from QgsVectorDataProvider */

    virtual QgsAbstractFeatureSource* featureSource() const override;

    /**
     * Returns the permanent storage type for this layer as a friendly name.
     */
    virtual QString storageType() const override;

    virtual QgsFeatureIterator getFeatures( const QgsFeatureRequest& request ) const override;

    /**
     * Get feature type.
     * @return int representing the feature type
     */
    virtual QgsWkbTypes::Type wkbType() const override;

    /**
     * Number of features in the layer
     * @return long containing number of features
     */
    virtual long featureCount() const override;

    virtual QgsFields fields() const override;

    /** Returns a bitmask containing the supported capabilities
     * Note, some capabilities may change depending on whether
     * a spatial filter is active on this provider, so it may
     * be prudent to check this value per intended operation.
     */
    virtual QgsVectorDataProvider::Capabilities capabilities() const override;

    /** Creates a spatial index on the data
     * @return indexCreated  Returns true if a spatial index is created
     */
    virtual bool createSpatialIndex() override;

    /* Implementation of functions from QgsDataProvider */

    /** Return a provider name
     *
     *  Essentially just returns the provider key.  Should be used to build file
     *  dialogs so that providers can be shown with their supported types. Thus
     *  if more than one provider supports a given format, the user is able to
     *  select a specific provider to open that file.
     *
     *  @note
     *
     *  Instead of being pure virtual, might be better to generalize this
     *  behavior and presume that none of the sub-classes are going to do
     *  anything strange with regards to their name or description?
     */
    QString name() const override;

    /** Return description
     *
     *  Return a terse string describing what the provider is.
     *
     *  @note
     *
     *  Instead of being pure virtual, might be better to generalize this
     *  behavior and presume that none of the sub-classes are going to do
     *  anything strange with regards to their name or description?
     */
    QString description() const override;

    virtual QgsRectangle extent() const override;
    bool isValid() const override;

    virtual QgsCoordinateReferenceSystem crs() const override;
    /**
     * Set the subset string used to create a subset of features in
     * the layer.
     */
    virtual bool setSubsetString( const QString& subset, bool updateFeatureCount = true ) override;

    virtual bool supportsSubsetString() const override { return true; }

    virtual QString subsetString() const override
    {
      return mSubsetString;
    }
    /* new functions */

    /**
     * Check to see if the point is withn the selection
     * rectangle
     * @param x X value of point
     * @param y Y value of point
     * @return True if point is within the rectangle
     */
    bool boundsCheck( double x, double y );


    /**
     * Check to see if a geometry overlaps the selection
     * rectangle
     * @param geom geometry to test against bounds
     * @param y Y value of point
     * @return True if point is within the rectangle
     */
    bool boundsCheck( QgsGeometry *geom );

    /**
     * Try to read field types from CSVT (or equialent xxxT) file.
     * @param filename The name of the file from which to read the field types
     * @param message  Pointer to a string to receive a status message
     * @return A list of field type strings, empty if not found or not valid
     */
    QStringList readCsvtFieldTypes( const QString& filename, QString *message = nullptr );

  private slots:

    void onFileUpdated();

  private:

    void scanFile( bool buildIndexes );

    //some of these methods const, as they need to be called from const methods such as extent()
    void rescanFile() const;
    void resetCachedSubset() const;
    void resetIndexes() const;
    void clearInvalidLines() const;
    void recordInvalidLine( const QString& message );
    void reportErrors( const QStringList& messages = QStringList(), bool showDialog = false ) const;
    static bool recordIsEmpty( QStringList &record );
    void setUriParameter( const QString& parameter, const QString& value );


    static QgsGeometry geomFromWkt( QString &sWkt, bool wktHasPrefixRegexp );
    static bool pointFromXY( QString &sX, QString &sY, QgsPoint &point, const QString& decimalPoint, bool xyDms );
    static double dmsStringToDouble( const QString &sX, bool *xOk );

    // mLayerValid defines whether the layer has been loaded as a valid layer
    bool mLayerValid;
    // mValid defines whether the layer is currently valid (may differ from
    // mLayerValid if the file has been rewritten)
    mutable bool mValid;

    //! Text file
    QgsDelimitedTextFile *mFile;

    // Fields
    GeomRepresentationType mGeomRep;
    mutable QList<int> attributeColumns;
    QgsFields attributeFields;

    int mFieldCount;  // Note: this includes field count for wkt field
    QString mWktFieldName;
    QString mXFieldName;
    QString mYFieldName;

    mutable int mXFieldIndex;
    mutable int mYFieldIndex;
    mutable int mWktFieldIndex;

    // mWktPrefix regexp is used to clean up
    // prefixes sometimes used for WKT (postgis EWKT, informix SRID)
    bool mWktHasPrefix;

    //! Layer extent
    mutable QgsRectangle mExtent;

    int mGeomType;

    mutable long mNumberFeatures;
    int mSkipLines;
    QString mDecimalPoint;
    bool mXyDms;

    QString mSubsetString;
    mutable QString mCachedSubsetString;
    QgsExpression *mSubsetExpression;
    bool mBuildSubsetIndex;
    mutable QList<quintptr> mSubsetIndex;
    mutable bool mUseSubsetIndex;
    mutable bool mCachedUseSubsetIndex;

    //! Storage for any lines in the file that couldn't be loaded
    int mMaxInvalidLines;
    mutable int mNExtraInvalidLines;
    mutable QStringList mInvalidLines;
    //! Only want to show the invalid lines once to the user
    bool mShowInvalidLines;

    //! Record file updates, flags rescan required
    mutable bool mRescanRequired;

    // Coordinate reference sytem
    QgsCoordinateReferenceSystem mCrs;

    QgsWkbTypes::Type mWkbType;
    QgsWkbTypes::GeometryType mGeometryType;

    // Spatial index
    bool mBuildSpatialIndex;
    mutable bool mUseSpatialIndex;
    mutable bool mCachedUseSpatialIndex;
    mutable QgsSpatialIndex *mSpatialIndex;

    friend class QgsDelimitedTextFeatureIterator;
    friend class QgsDelimitedTextFeatureSource;
};

#endif
