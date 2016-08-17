/***************************************************************************
    memoryprovider.h - provider with storage in memory
    ------------------
    begin                : June 2008
    copyright            : (C) 2008 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsvectordataprovider.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsfield.h"

typedef QMap<QgsFeatureId, QgsFeature> QgsFeatureMap;

class QgsSpatialIndex;

class QgsMemoryFeatureIterator;

class QgsMemoryProvider : public QgsVectorDataProvider
{
    Q_OBJECT

  public:
    explicit QgsMemoryProvider( const QString& uri = QString() );

    virtual ~QgsMemoryProvider();

    /* Implementation of functions from QgsVectorDataProvider */

    virtual QgsAbstractFeatureSource* featureSource() const override;

    /**
     * Returns the permanent storage type for this layer as a friendly name.
     */

    virtual QString dataSourceUri( bool expandAuthConfig = true ) const override;

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

    /**
      * Adds a list of features
      * @return true in case of success and false in case of failure
      */
    virtual bool addFeatures( QgsFeatureList & flist ) override;

    /**
      * Deletes a feature
      * @param id list containing feature ids to delete
      * @return true in case of success and false in case of failure
      */
    virtual bool deleteFeatures( const QgsFeatureIds & id ) override;


    /**
     * Adds new attributes
     * @param attributes map with attribute name as key and type as value
     * @return true in case of success and false in case of failure
     */
    virtual bool addAttributes( const QList<QgsField> &attributes ) override;

    virtual bool renameAttributes( const QgsFieldNameMap& renamedAttributes ) override;

    /**
     * Deletes existing attributes
     * @param attributes a set containing names of attributes
     * @return true in case of success and false in case of failure
     */
    virtual bool deleteAttributes( const QgsAttributeIds& attributes ) override;

    /**
     * Changes attribute values of existing features.
     * @param attr_map a map containing changed attributes
     * @return true in case of success and false in case of failure
     */
    virtual bool changeAttributeValues( const QgsChangedAttributesMap & attr_map ) override;

    /**
     * Changes geometries of existing features
     * @param geometry_map   A std::map containing the feature IDs to change the geometries of.
     *                       the second map parameter being the new geometries themselves
     * @return               true in case of success and false in case of failure
     */
    virtual bool changeGeometryValues( const QgsGeometryMap & geometry_map ) override;

    QString subsetString() const override;

    /** Mutator for sql where clause used to limit dataset size */
    bool setSubsetString( const QString& theSQL, bool updateFeatureCount = true ) override;

    virtual bool supportsSubsetString() const override { return true; }

    /**
     * Creates a spatial index
     * @return true in case of success
     */
    virtual bool createSpatialIndex() override;

    /** Returns a bitmask containing the supported capabilities
    Note, some capabilities may change depending on whether
    a spatial filter is active on this provider, so it may
    be prudent to check this value per intended operation.
     */
    virtual QgsVectorDataProvider::Capabilities capabilities() const override;

    /* Implementation of functions from QgsDataProvider */

    /**
     * return a provider name
     */
    QString name() const override;

    /**
     * return description
     */
    QString description() const override;

    virtual QgsRectangle extent() const override;
    bool isValid() const override;

    virtual QgsCoordinateReferenceSystem crs() const override;

  protected:

    // called when added / removed features or geometries has been changed
    void updateExtent();

  private:
    // Coordinate reference system
    QgsCoordinateReferenceSystem mCrs;

    // fields
    QgsFields mFields;
    QgsWkbTypes::Type mWkbType;
    QgsRectangle mExtent;

    // features
    QgsFeatureMap mFeatures;
    QgsFeatureId mNextFeatureId;

    // indexing
    QgsSpatialIndex* mSpatialIndex;

    QString mSubsetString;

    friend class QgsMemoryFeatureSource;
};
