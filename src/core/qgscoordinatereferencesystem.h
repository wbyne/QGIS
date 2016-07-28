/***************************************************************************
                             qgscoordinatereferencesystem.h

                             -------------------
    begin                : 2007
    copyright            : (C) 2007 by Gary E. Sherman
    email                : sherman@mrcc.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSCOORDINATEREFERENCESYSTEM_H
#define QGSCOORDINATEREFERENCESYSTEM_H

//Standard includes
#include <ostream>

//qt includes
#include <QString>
#include <QMap>
#include <QHash>
#include <QReadWriteLock>

class QDomNode;
class QDomDocument;
class QgsCoordinateReferenceSystemPrivate;

// forward declaration for sqlite3
typedef struct sqlite3 sqlite3;

//qgis includes
#include "qgis.h"
#include "qgsunittypes.h"

#ifdef DEBUG
typedef struct OGRSpatialReferenceHS *OGRSpatialReferenceH;
#else
typedef void *OGRSpatialReferenceH;
#endif

class QgsCoordinateReferenceSystem;
typedef void ( *CUSTOM_CRS_VALIDATION )( QgsCoordinateReferenceSystem& );

/** \ingroup core
 * This class represents a coordinate reference system (CRS).
 *
 * Coordinate reference system object defines a specific map projection, as well as transformations
 * between different coordinate reference systems. There are various ways how a CRS can be defined:
 * using well-known text (WKT), PROJ.4 string or combination of authority and code (e.g. EPSG:4326).
 * QGIS comes with its internal database of coordinate reference systems (stored in SQLite) that
 * allows lookups of CRS and seamless conversions between the various definitions.
 *
 * Most commonly one comes across two types of coordinate systems:
 *
 * 1. **Geographic coordinate systems** - based on a geodetic datum, normally with coordinates being
 *    latitude/longitude in degrees. The most common one is World Geodetic System 84 (WGS84).
 * 2. **Projected coordinate systems** - based on a geodetic datum with coordinates projected to a plane,
 *    typically using meters or feet as units. Common projected coordinate systems are Universal
 *    Transverse Mercator or Albers Equal Area.
 *
 * Internally QGIS uses proj4 library for all the math behind coordinate transformations, so in case
 * of any troubles with projections it is best to examine the PROJ.4 representation within the object,
 * as that is the representation that will be ultimately used.
 *
 * Methods that allow inspection of CRS instances include isValid(), authid(), description(),
 * toWkt(), toProj4(), mapUnits() and others.
 * Creation of CRS instances is further described in \ref crs_construct_and_copy section below.
 * Transformations between coordinate reference systems are done using QgsCoordinateTransform class.
 *
 * For example, the following code will create and inspect "British national grid" CRS:
 *
 * ~~~{.py}
 * crs = QgsCoordinateReferenceSystem("EPSG:27700")
 * if crs.isValid():
 *     print("CRS Description: {}".format(crs.description()))
 *     print("CRS PROJ.4 text: {}".format(crs.toProj4()))
 * else:
 *     print("Invalid CRS!")
 * ~~~
 *
 * This will produce the following output:
 *
 * ~~~
 * CRS Description: OSGB 1936 / British National Grid
 * CRS PROJ.4 text: +proj=tmerc +lat_0=49 +lon_0=-2 +k=0.9996012717 +x_0=400000 +y_0=-100000 [output trimmed]
 * ~~~
 *
 * CRS Definition Formats
 * ======================
 *
 * This section gives an overview of various supported CRS definition formats:
 *
 * 1. **Authority and Code.** Also referred to as OGC WMS format within QGIS as they have been widely
 *    used in OGC standards. These are encoded as `<auth>:<code>`, for example `EPSG:4326` refers
 *    to WGS84 system. EPSG is the most commonly used authority that covers a wide range
 *    of coordinate systems around the world.
 *
 *    An extended variant of this format is OGC URN. Syntax of URN for CRS definition is
 *    `urn:ogc:def:crs:<auth>:[<version>]:<code>`. This class can also parse URNs (versions
 *    are currently ignored). For example, WGS84 may be encoded as `urn:ogc:def:crs:OGC:1.3:CRS84`.
 *
 *    QGIS adds support for "USER" authority that refers to IDs used internally in QGIS. This variant
 *    is best avoided or used with caution as the IDs are not permanent and they refer to different CRS
 *    on different machines or user profiles.
 *
 *    See authid() and createFromOgcWmsCrs() methods.
 *
 * 2. **PROJ.4 string.** This is a string consisting of a series of key/value pairs in the following
 *    format: `+param1=value1 +param2=value2 [...]`. This is the format natively used by the
 *    underlying proj4 library. For example, the definition of WGS84 looks like this:
 *
 *        +proj=longlat +datum=WGS84 +no_defs
 *
 *    See toProj4() and createFromProj4() methods.
 *
 * 3. **Well-known text (WKT).** Defined by Open Geospatial Consortium (OGC), this is another common
 *    format to define CRS. For WGS84 the OGC WKT definition is the following:
 *
 *        GEOGCS["WGS 84",
 *               DATUM["WGS_1984",
 *                 SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],
 *                 AUTHORITY["EPSG","6326"]],
 *               PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],
 *               UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],
 *               AUTHORITY["EPSG","4326"]]
 *
 *    See toWkt() and createFromWkt() methods.
 *
 * CRS Database and Custom CRS
 * ===========================
 *
 * The database of CRS shipped with QGIS is stored in a SQLite database (see QgsApplication::srsDbFilePath())
 * and it is based on the data files maintained by GDAL project (a variety of .csv and .wkt files).
 *
 * Sometimes it happens that users need to use a CRS definition that is not well known
 * or that has been only created with a specific purpose (and thus its definition is not
 * available in our database of CRS). Whenever a new CRS definition is seen, it will
 * be added to the local databse (in user's home directory, see QgsApplication::qgisUserDbFilePath()).
 * QGIS also features a GUI for management of local custom CRS definitions.
 *
 * There are therefore two databases: one for shipped CRS definitions and one for custom CRS definitions.
 * Custom CRS have internal IDs (accessible with srsid()) greater or equal to \ref USER_CRS_START_ID.
 * The local CRS databases should never be accessed directly with SQLite functions, instead
 * you should use QgsCoordinateReferenceSystem API for CRS lookups and for managements of custom CRS.
 *
 * Validation
 * ==========
 *
 * In some cases (most prominently when loading a map layer), QGIS will try to ensure
 * that the given map layer CRS is valid using validate() call. If not, a custom
 * validation function will be called - such function may for example show a GUI
 * for manual CRS selection. The validation function is configured using setCustomCrsValidation().
 * If validation fails or no validation function is set, the default CRS is assigned
 * (WGS84). QGIS application registers its validation function that will act according
 * to user's settings (either show CRS selector dialog or use project/custom CRS).
 *
 * Object Construction and Copying  {#crs_construct_and_copy}
 * ===============================
 *
 * The easiest way of creating CRS instances is to use QgsCoordinateReferenceSystem(const QString&)
 * constructor that automatically recognizes definition format from the given string.
 *
 * Creation of CRS object involves some queries in a local SQLite database, which may
 * be potentially expensive. Consequently, CRS creation methods use an internal cache to avoid
 * unnecessary database lookups. If the CRS database is modified, then it is necessary to call
 * invalidateCache() to ensure that outdated records are not being returned from the cache.
 *
 * Since QGIS 2.16 QgsCoordinateReferenceSystem objects are implicitly shared.
 *
 * Caveats
 * =======
 *
 * There are two different flavours of WKT: one is defined by OGC, the other is the standard
 * used by ESRI. They look very similar, but they are not the same. QGIS is able to consume
 * both flavours.
 *
 * \see QgsCoordinateTransform
 */
class CORE_EXPORT QgsCoordinateReferenceSystem
{
  public:

    //! Enumeration of types of IDs accepted in createFromId() method
    enum CrsType
    {
      InternalCrsId,  //!< Internal ID used by QGIS in the local SQLite database
      PostgisCrsId,   //!< SRID used in PostGIS
      EpsgCrsId       //!< EPSG code
    };

    /** Constructs an invalid CRS object */
    QgsCoordinateReferenceSystem();

    ~QgsCoordinateReferenceSystem();

    /*!
     * Constructs a CRS object from a string definition using createFromString()
     *
     * It supports the following formats:
     * - "EPSG:<code>" - handled with createFromOgcWms()
     * - "POSTGIS:<srid>" - handled with createFromSrid()
     * - "INTERNAL:<srsid>" - handled with createFromSrsId()
     * - "PROJ4:<proj4>" - handled with createFromProj4()
     * - "WKT:<wkt>" - handled with createFromWkt()
     *
     * If no prefix is specified, WKT definition is assumed.
     * @param theDefinition A String containing a coordinate reference system definition.
     * @see createFromString()
     */
    // TODO QGIS 3: remove "POSTGIS" and "INTERNAL", allow PROJ4 without the prefix
    explicit QgsCoordinateReferenceSystem( const QString& theDefinition );

    /** Constructor a CRS object using a postgis SRID, an EPSG code or an internal QGIS CRS ID.
     * @note We encourage you to use EPSG code, WKT or Proj4 to describe CRS's in your code
     * wherever possible. Internal QGIS CRS IDs are not guaranteed to be permanent / involatile.
     * @param theId The ID valid for the chosen CRS ID type
     * @param theType One of the types described in CrsType
     */
    // TODO QGIS 3: remove theType and always use EPSG code
    QgsCoordinateReferenceSystem( const long theId, CrsType theType = PostgisCrsId );

    //! Copy constructor
    QgsCoordinateReferenceSystem( const QgsCoordinateReferenceSystem& srs );

    //! Assignment operator
    QgsCoordinateReferenceSystem& operator=( const QgsCoordinateReferenceSystem& srs );

    // static creators

    /** Creates a CRS from a given OGC WMS-format Coordinate Reference System string.
     * @param ogcCrs OGR compliant CRS definition, eg "EPSG:4326"
     * @returns matching CRS, or an invalid CRS if string could not be matched
     * @note added in QGIS 3.0
     * @see createFromOgcWmsCrs()
    */
    static QgsCoordinateReferenceSystem fromOgcWmsCrs( const QString& ogcCrs );

    /** Creates a CRS from a given EPSG ID.
     * @param epsg epsg CRS ID
     * @returns matching CRS, or an invalid CRS if string could not be matched
     * @note added in QGIS 3.0
    */
    static QgsCoordinateReferenceSystem fromEpsgId( long epsg );

    /** Creates a CRS from a proj4 style formatted string.
     * @param proj4 proj4 format string
     * @returns matching CRS, or an invalid CRS if string could not be matched
     * @note added in QGIS 3.0
     * @see createFromProj4()
    */
    static QgsCoordinateReferenceSystem fromProj4( const QString& proj4 );

    /** Creates a CRS from a WKT spatial ref sys definition string.
     * @param wkt WKT for the desired spatial reference system.
     * @returns matching CRS, or an invalid CRS if string could not be matched
     * @note added in QGIS 3.0
     * @see createFromWkt()
    */
    static QgsCoordinateReferenceSystem fromWkt( const QString& wkt );

    /** Creates a CRS from a specified QGIS SRS ID.
     * @param srsId internal QGIS SRS ID
     * @returns matching CRS, or an invalid CRS if ID could not be found
     * @note added in QGIS 3.0
     * @see createFromSrsId()
    */
    static QgsCoordinateReferenceSystem fromSrsId( long srsId );

    // Misc helper functions -----------------------

    /**
     * Sets this CRS by lookup of the given ID in the CRS database.
     * @note We encourage you to use EPSG code, WKT or Proj4 to describe CRS's in your code
     * wherever possible. Internal QGIS CRS IDs are not guaranteed to be permanent / involatile.
     * @return True on success else false
     */
    // TODO QGIS 3: remove theType and always use EPSG code, rename to createFromEpsg
    bool createFromId( const long theId, CrsType theType = PostgisCrsId );

    /**
     * Sets this CRS to the given OGC WMS-format Coordinate Reference Systems.
     *
     * Accepts both "<auth>:<code>" format and OGC URN "urn:ogc:def:crs:<auth>:[<version>]:<code>".
     * It also recognizes "QGIS", "USER", "CUSTOM" authorities, which all have the same meaning
     * and refer to QGIS internal CRS IDs.
     * @note this method uses an internal cache. Call invalidateCache() to clear the cache.
     * @return True on success else false
     * @see fromOgcWmsCrs()
     */
    // TODO QGIS 3: remove "QGIS" and "CUSTOM", only support "USER" (also returned by authid())
    bool createFromOgcWmsCrs( const QString& theCrs );

    /** Sets this CRS by lookup of the given PostGIS SRID in the CRS database.
     * @param theSrid The postgis SRID for the desired spatial reference system.
     * @return True on success else false
     */
    // TODO QGIS 3: remove unless really necessary - let's use EPSG codes instead
    bool createFromSrid( const long theSrid );

    /** Sets this CRS using a WKT definition.
     *
     * If EPSG code of the WKT definition can be determined, it is extracted
     * and createFromOgcWmsCrs() is used to initialize the object.
     * Otherwise the WKT will be converted to a proj4 string and createFromProj4()
     * set up the object.
     * @note Some members may be left blank if no match can be found in CRS database.
     * @note this method uses an internal cache. Call invalidateCache() to clear the cache.
     * @param theWkt The WKT for the desired spatial reference system.
     * @return True on success else false
     * @see fromWkt()
     */
    bool createFromWkt( const QString &theWkt );

    /** Sets this CRS by lookup of internal QGIS CRS ID in the CRS database.
     *
     * If the srsid is < USER_CRS_START_ID, system CRS database is used, otherwise
     * user's local CRS database from home directory is used.
     * @note this method uses an internal cache. Call invalidateCache() to clear the cache.
     * @param theSrsId The internal QGIS CRS ID for the desired spatial reference system.
     * @return True on success else false
     * @see fromSrsId()
     */
    bool createFromSrsId( const long theSrsId );

    /** Sets this CRS by passing it a PROJ.4 style formatted string.
     *
     * The string will be parsed and the projection and ellipsoid
     * members set and the remainder of the proj4 string will be stored
     * in the parameters member. The reason for this is so that we
     * can easily present the user with 'natural language' representation
     * of the projection and ellipsoid by looking them up in the srs.db sqlite
     * database.
     *
     * We try to match the proj4 string to internal QGIS CRS ID using the following logic:
     *
     * - perform a whole text search on proj4 string (if not null)
     * - if not match is found, split proj4 into individual parameters and try to find
     *   a match where the parameters are in a different order
     * - if none of the above match, use findMatchingProj()
     *
     * @note Some members may be left blank if no match can be found in CRS database.
     * @note this method uses an internal cache. Call invalidateCache() to clear the cache.
     * @param theProjString A proj4 format string
     * @return True on success else false
     * @see fromProj4()
     */
    bool createFromProj4( const QString &theProjString );

    /** Set up this CRS from a string definition.
     *
     * It supports the following formats:
     * - "EPSG:<code>" - handled with createFromOgcWms()
     * - "POSTGIS:<srid>" - handled with createFromSrid()
     * - "INTERNAL:<srsid>" - handled with createFromSrsId()
     * - "PROJ4:<proj4>" - handled with createFromProj4()
     * - "WKT:<wkt>" - handled with createFromWkt()
     *
     * If no prefix is specified, WKT definition is assumed.
     * @param theDefinition A String containing a coordinate reference system definition.
     * @return True on success else false
     */
    bool createFromString( const QString &theDefinition );

    /** Set up this CRS from various text formats.
     *
     * Valid formats: WKT string, "EPSG:n", "EPSGA:n", "AUTO:proj_id,unit_id,lon0,lat0",
     * "urn:ogc:def:crs:EPSG::n", PROJ.4 string, filename (with WKT, XML or PROJ.4 string),
     * well known name (such as NAD27, NAD83, WGS84 or WGS72),
     * ESRI::[WKT string] (directly or in a file), "IGNF:xxx"
     *
     * For more details on supported formats see OGRSpatialReference::SetFromUserInput()
     * ( http://www.gdal.org/ogr/classOGRSpatialReference.html#aec3c6a49533fe457ddc763d699ff8796 )
     * @note this function generates a WKT string using OSRSetFromUserInput() and
     * passes it to createFromWkt() function.
     * @param theDefinition A String containing a coordinate reference system definition.
     * @return True on success else false
     */
    // TODO QGIS3: rename to createFromStringOGR so it is clear it's similar to createFromString, just different backend
    bool createFromUserInput( const QString &theDefinition );

    /** Make sure that ESRI WKT import is done properly.
     * This is required for proper shapefile CRS import when using gdal>= 1.9.
     * @note This function is called by createFromUserInput() and QgsOgrProvider::crs(), there is usually
     * no need to call it from elsewhere.
     * @note This function sets CPL config option GDAL_FIX_ESRI_WKT to a proper value,
     * unless it has been set by the user through the commandline or an environment variable.
     * For more details refer to OGRSpatialReference::morphFromESRI() .
     */
    static void setupESRIWktFix();

    /** Returns whether this CRS is correctly initialized and usable */
    bool isValid() const;

    /** Perform some validation on this CRS. If the CRS doesn't validate the
     * default behaviour settings for layers with unknown CRS will be
     * consulted and acted on accordingly. By hell or high water this
     * method will do its best to make sure that this CRS is valid - even
     * if that involves resorting to a hard coded default of geocs:wgs84.
     *
     * @note It is not usually necessary to use this function, unless you
     * are trying to force this CRS to be valid.
     * @see setCustomCrsValidation(), customCrsValidation()
     */
    void validate();

    /** Walks the CRS databases (both system and user database) trying to match
     *  stored PROJ.4 string to a database entry in order to fill in further
     *  pieces of information about CRS.
     *  @note The ellipsoid and projection acronyms must be set as well as the proj4string!
     *  @return long the SrsId of the matched CRS, zero if no match was found
     */
    // TODO QGIS 3: seems completely obsolete now (only compares proj4 - already done in createFromProj4)
    long findMatchingProj();

    /** Overloaded == operator used to compare to CRS's.
     *
     *  Internally it will use authid() for comparison.
     */
    bool operator==( const QgsCoordinateReferenceSystem &theSrs ) const;
    /** Overloaded != operator used to compare to CRS's.
     *
     *  Returns opposite bool value to operator ==
     */
    bool operator!=( const QgsCoordinateReferenceSystem &theSrs ) const;

    /** Restores state from the given DOM node.
     * @param theNode The node from which state will be restored
     * @return bool True on success, False on failure
     */
    bool readXml( const QDomNode & theNode );
    /** Stores state to the given Dom node in the given document.
     * @param theNode The node in which state will be restored
     * @param theDoc The document in which state will be stored
     * @return bool True on success, False on failure
     */
    bool writeXml( QDomNode & theNode, QDomDocument & theDoc ) const;


    /** Sets custom function to force valid CRS
     *  QGIS uses implementation in QgisGui::customSrsValidation
     * @note not available in python bindings
     */
    static void setCustomCrsValidation( CUSTOM_CRS_VALIDATION f );

    /** Gets custom function
     * @note not available in python bindings
     */
    static CUSTOM_CRS_VALIDATION customCrsValidation();

    // Accessors -----------------------------------

    /** Returns the internal CRS ID, if available.
     *  @return the internal sqlite3 srs.db primary key for this CRS
     */
    long srsid() const;

    /** Returns PostGIS SRID for the CRS.
     * @return the PostGIS spatial_ref_sys identifier for this CRS (defaults to 0)
     */
    // TODO QGIS 3: remove unless really necessary - let's use EPSG codes instead
    long postgisSrid() const;

    /** Returns the authority identifier for the CRS.
     *
     * The identifier includes both the authority (eg EPSG) and the CRS number (eg 4326).
     * This is the best method to use when showing a very short CRS identifier to a user,
     * eg "EPSG:4326".
     *
     * If CRS object is a custom CRS (not found in database), the method will return
     * internal QGIS CRS ID with "QGIS" authority, for example "QGIS:100005"
     * @returns the authority identifier for this CRS
     * @see description()
     */
    QString authid() const;

    /** Returns the descriptive name of the CRS, eg "WGS 84" or "GDA 94 / Vicgrid94". In most
     * cases this is the best method to use when showing a friendly identifier for the CRS to a
     * user.
     * @returns descriptive name of the CRS
     * @note an empty string will be returned if the description is not available for the CRS
     * @see authid()
     */
    QString description() const;

    /** Returns the projection acronym for the projection used by the CRS.
     * @returns the official proj4 acronym for the projection family
     * @note an empty string will be returned if the projectionAcronym is not available for the CRS
     * @see ellipsoidAcronym()
     */
    QString projectionAcronym() const;

    /** Returns the ellipsoid acronym for the ellipsoid used by the CRS.
     * @returns the official proj4 acronym for the ellipoid
     * @note an empty string will be returned if the ellipsoidAcronym is not available for the CRS
     * @see projectionAcronym()
     */
    QString ellipsoidAcronym() const;

    /** Returns a WKT representation of this CRS.
     * @return string containing WKT of the CRS
     * @see toProj4()
     */
    QString toWkt() const;

    /** Returns a Proj4 string representation of this CRS.
     *
     * If proj and ellps keys are found in the parameters,
     * they will be stripped out and the projection and ellipsoid acronyms will be
     * overridden with these.
     * @return Proj4 format string that defines this CRS.
     * @note an empty string will be returned if the CRS could not be represented by a Proj4 string
     * @see toWkt()
     */
    QString toProj4() const;

    /** Returns whether the CRS is a geographic CRS (using lat/lon coordinates)
     * @returns true if CRS is geographic, or false if it is a projected CRS
     */
    bool isGeographic() const;

    /** Returns whether axis is inverted (eg. for WMS 1.3) for the CRS.
     * @returns true if CRS axis is inverted
     */
    bool hasAxisInverted() const;

    /** Returns the units for the projection used by the CRS.
     */
    QgsUnitTypes::DistanceUnit mapUnits() const;

    // Mutators -----------------------------------
    /** Set user hint for validation
     */
    void setValidationHint( const QString& html );

    /** Get user hint for validation
     */
    QString validationHint();

    /** Update proj.4 parameters in our database from proj.4
     * @returns number of updated CRS on success and
     *   negative number of failed updates in case of errors.
     * @note This is used internally and should not be necessary to call in client code
     */
    static int syncDb();


    /** Save the proj4-string as a custom CRS
     * @returns bool true if success else false
     */
    bool saveAsUserCrs( const QString& name );

    /** Returns auth id of related geographic CRS*/
    QString geographicCrsAuthId() const;

    /** Returns a list of recently used projections
     * @returns list of srsid for recently used projections
     * @note added in QGIS 2.7
     */
    static QStringList recentProjections();

    /** Clears the internal cache used to initialise QgsCoordinateReferenceSystem objects.
     * This should be called whenever the srs database has been modified in order to ensure
     * that outdated CRS objects are not created.
     * @note added in QGIS 3.0
     */
    static void invalidateCache();

    // Mutators -----------------------------------
    // We don't want to expose these to the public api since they wont create
    // a fully valid crs. Programmers should use the createFrom* methods rather
  private:
    /** A static helper function to find out the proj4 string for a srsid
     * @param theSrsId The srsid used for the lookup
     * @return QString The proj4 string
     */
    static QString proj4FromSrsId( const int theSrsId );

    /** Set the QGIS  SrsId
     *  @param theSrsId The internal sqlite3 srs.db primary key for this CRS
     */
    void setInternalId( long theSrsId );
    /** Set the postgis srid
     *  @param theSrid The postgis spatial_ref_sys key for this CRS
     */
    void setSrid( long theSrid );
    /** Set the Description
     * @param theDescription A textual description of the CRS.
     */
    void setDescription( const QString& theDescription );

    /** Set the Proj Proj4String.
     * @param theProj4String Proj4 format specifies
     * (excluding proj and ellips) that define this CRS.
     * @note some content of the PROJ4 string may be stripped off by this
     * method due to the parsing of the string by OSRNewSpatialReference .
     * For example input:
     * +proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs
     * Gets stored in the CRS as:
     * +proj=longlat +datum=WGS84 +no_defs
     */
    void setProj4String( const QString& theProj4String );

    /** Set this Geographic? flag
     * @param theGeoFlag Whether this is a geographic or projected coordinate system
     */
    void setGeographicFlag( bool theGeoFlag );

    /** Set the EpsgCrsId identifier for this CRS
     * @param theEpsg the ESPG identifier for this CRS (defaults to 0)
     */
    void setEpsg( long theEpsg );

    /** Set the authority identifier for this CRS
     * @param theID the authority identifier for this CRS (defaults to 0)
     */
    void setAuthId( const QString& theID );
    /** Set the projection acronym
     * @param theProjectionAcronym the acronym (must be a valid proj4 projection acronym)
     */
    void setProjectionAcronym( const QString& theProjectionAcronym );
    /** Set the ellipsoid acronym
     * @param theEllipsoidAcronym the acronym (must be a valid proj4 ellipsoid acronym)
     */
    void setEllipsoidAcronym( const QString& theEllipsoidAcronym );

    /** Print the description if debugging
     */
    void debugPrint();

    /** A string based associative array used for passing records around */
    typedef QMap<QString, QString> RecordMap;
    /** Get a record from the srs.db or qgis.db backends, given an sql statment.
     * @note only handles queries that return a single record.
     * @note it will first try the system srs.db then the users qgis.db!
     * @param theSql The sql query to execute
     * @return An associative array of field name <-> value pairs
     */
    RecordMap getRecord( const QString& theSql );

    //! Open SQLite db and show message if cannot be opened
    //! @return the same code as sqlite3_open
    static int openDb( const QString& path, sqlite3 **db, bool readonly = true );

    //! Work out the projection units and set the appropriate local variable
    void setMapUnits();

    //! Helper for getting number of user CRS already in db
    long getRecordCount();

    //! Helper for sql-safe value quoting
    static QString quotedValue( QString value );

    //! Initialize the CRS object by looking up CRS database in path given in db argument,
    //! using first CRS entry where expression = 'value'
    bool loadFromDb( const QString& db, const QString& expression, const QString& value );

    static bool loadIds( QHash<int, QString> &wkts );
    static bool loadWkts( QHash<int, QString> &wkts, const char *filename );
    //! Update datum shift definitions from GDAL data. Used by syncDb()
    static bool syncDatumTransform( const QString& dbPath );

    QExplicitlySharedDataPointer<QgsCoordinateReferenceSystemPrivate> d;

    //! Function for CRS validation. May be null.
    static CUSTOM_CRS_VALIDATION mCustomSrsValidation;


    // cache

    static QReadWriteLock mSrIdCacheLock;
    static QHash< long, QgsCoordinateReferenceSystem > mSrIdCache;
    static QReadWriteLock mOgcLock;
    static QHash< QString, QgsCoordinateReferenceSystem > mOgcCache;
    static QReadWriteLock mProj4CacheLock;
    static QHash< QString, QgsCoordinateReferenceSystem > mProj4Cache;
    static QReadWriteLock mCRSWktLock;
    static QHash< QString, QgsCoordinateReferenceSystem > mWktCache;
    static QReadWriteLock mCRSSrsIdLock;
    static QHash< long, QgsCoordinateReferenceSystem > mSrsIdCache;
    static QReadWriteLock mCrsStringLock;
    static QHash< QString, QgsCoordinateReferenceSystem > mStringCache;

    friend class TestQgsCoordinateReferenceSystem;
};


//! Output stream operator
inline std::ostream& operator << ( std::ostream& os, const QgsCoordinateReferenceSystem &r )
{
  QString mySummary( "\n\tSpatial Reference System:" );
  mySummary += "\n\t\tDescription : ";
  if ( !r.description().isNull() )
  {
    mySummary += r.description();
  }
  else
  {
    mySummary += "Undefined";
  }
  mySummary += "\n\t\tProjection  : ";
  if ( !r.projectionAcronym().isNull() )
  {
    mySummary += r.projectionAcronym();
  }
  else
  {
    mySummary += "Undefined";
  }

  mySummary += "\n\t\tEllipsoid   : ";
  if ( !r.ellipsoidAcronym().isNull() )
  {
    mySummary += r.ellipsoidAcronym();
  }
  else
  {
    mySummary += "Undefined";
  }

  mySummary += "\n\t\tProj4String  : ";
  if ( !r.toProj4().isNull() )
  {
    mySummary += r.toProj4();
  }
  else
  {
    mySummary += "Undefined";
  }
  // Using streams we need to use local 8 Bit
  return os << mySummary.toLocal8Bit().data() << std::endl;
}

#endif // QGSCOORDINATEREFERENCESYSTEM_H
