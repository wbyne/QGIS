/***************************************************************************
               qgsfield.h - Describes a field in a layer or table
                     --------------------------------------
               Date                 : 01-Jan-2004
               Copyright            : (C) 2004 by Gary E.Sherman
               email                : sherman at mrcc.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSFIELD_H
#define QGSFIELD_H

#include <QString>
#include <QVariant>
#include <QVector>
#include <QSharedDataPointer>

typedef QList<int> QgsAttributeList;

class QgsExpression;
class QgsFieldPrivate;
class QgsFieldsPrivate;

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests in testqgsfield.cpp.
 * See details in QEP #17
 ****************************************************************************/

#include "qgseditorwidgetsetup.h"

/** \class QgsField
  * \ingroup core
  * Encapsulate a field in an attribute table or data source.
  * QgsField stores metadata about an attribute field, including name, type
  * length, and if applicable, precision.
  * \note QgsField objects are implicitly shared.
 */

class CORE_EXPORT QgsField
{
    Q_GADGET

    Q_PROPERTY( bool isNumeric READ isNumeric )
    Q_PROPERTY( int length READ length WRITE setLength )
    Q_PROPERTY( int precision READ precision WRITE setPrecision )
    Q_PROPERTY( QString comment READ comment WRITE setComment )
    Q_PROPERTY( QString name READ name WRITE setName )
    Q_PROPERTY( QString alias READ alias WRITE setAlias )
    Q_PROPERTY( QString defaultValueExpression READ defaultValueExpression WRITE setDefaultValueExpression )

  public:
    /** Constructor. Constructs a new QgsField object.
     * @param name Field name
     * @param type Field variant type, currently supported: String / Int / Double
     * @param typeName Field type (eg. char, varchar, text, int, serial, double).
     * Field types are usually unique to the source and are stored exactly
     * as returned from the data store.
     * @param len Field length
     * @param prec Field precision. Usually decimal places but may also be
     * used in conjunction with other fields types (eg. variable character fields)
     * @param comment Comment for the field
     * @param subType If the field is a collection, its element's type. When
     *                all the elements don't need to have the same type, leave
     *                this to QVariant::Invalid.
     */
    QgsField( const QString& name = QString(),
              QVariant::Type type = QVariant::Invalid,
              const QString& typeName = QString(),
              int len = 0,
              int prec = 0,
              const QString& comment = QString(),
              QVariant::Type subType = QVariant::Invalid );

    /** Copy constructor
     */
    QgsField( const QgsField& other );

    /** Assignment operator
     */
    QgsField& operator =( const QgsField &other );

    //! Destructor
    virtual ~QgsField();

    bool operator==( const QgsField& other ) const;
    bool operator!=( const QgsField& other ) const;

    /** Returns the name of the field.
     * @see setName()
     * @see displayName()
     */
    QString name() const;

    /** Returns the name to use when displaying this field. This will be the
     * field alias if set, otherwise the field name.
     * @see name()
     * @see alias()
     * @note added in QGIS 3.0
     */
    QString displayName() const;

    //! Gets variant type of the field as it will be retrieved from data source
    QVariant::Type type() const;

    /**
     * If the field is a collection, gets its element's type.
     * When all the elements don't need to have the same type, this returns
     * QVariant::Invalid.
     * @note added in QGIS 3.0
     */
    QVariant::Type subType() const;

    /**
     * Gets the field type. Field types vary depending on the data source. Examples
     * are char, int, double, blob, geometry, etc. The type is stored exactly as
     * the data store reports it, with no attempt to standardize the value.
     * @return QString containing the field type
     */
    QString typeName() const;

    /**
     * Gets the length of the field.
     * @return int containing the length of the field
     */
    int length() const;

    /**
     * Gets the precision of the field. Not all field types have a related precision.
     * @return int containing the precision or zero if not applicable to the field type.
     */
    int precision() const;

    /**
     * Returns the field comment
     */
    QString comment() const;

    /**
     * Returns if this field is numeric. Any integer or floating point type
     * will return true for this.
     *
     * @note added in QGIS 2.18
     */
    bool isNumeric() const;

    /**
     * Set the field name.
     * @param name Name of the field
     */
    void setName( const QString& name );

    /**
     * Set variant type.
     */
    void setType( QVariant::Type type );

    /**
     * If the field is a collection, set its element's type.
     * When all the elements don't need to have the same type, set this to
     * QVariant::Invalid.
     * @note added in QGIS 3.0
     */
    void setSubType( QVariant::Type subType );

    /**
     * Set the field type.
     * @param typeName Field type
     */
    void setTypeName( const QString& typeName );

    /**
     * Set the field length.
     * @param len Length of the field
     */
    void setLength( int len );

    /**
     * Set the field precision.
     * @param precision Precision of the field
     */
    void setPrecision( int precision );

    /**
     * Set the field comment
     */
    void setComment( const QString& comment );

    /** Returns the expression used when calculating the default value for the field.
     * @returns expression evaluated when calculating default values for field, or an
     * empty string if no default is set
     * @note added in QGIS 3.0
     * @see setDefaultValueExpression()
     */
    QString defaultValueExpression() const;

    /** Sets an expression to use when calculating the default value for the field.
     * @param expression expression to evaluate when calculating default values for field. Pass
     * an empty expression to clear the default.
     * @note added in QGIS 3.0
     * @see defaultValueExpression()
     */
    void setDefaultValueExpression( const QString& expression );

    /** Returns the alias for the field (the friendly displayed name of the field ),
     * or an empty string if there is no alias.
     * @see setAlias()
     * @note added in QGIS 3.0
     */
    QString alias() const;

    /** Sets the alias for the field (the friendly displayed name of the field ).
     * @param alias field alias, or empty string to remove an existing alias
     * @see alias()
     * @note added in QGIS 3.0
     */
    void setAlias( const QString& alias );

    /** Formats string for display*/
    QString displayString( const QVariant& v ) const;

    /**
     * Converts the provided variant to a compatible format
     *
     * @param v  The value to convert
     *
     * @return   True if the conversion was successful
     */
    bool convertCompatible( QVariant& v ) const;

    //! Allows direct construction of QVariants from fields.
    operator QVariant() const
    {
      return QVariant::fromValue( *this );
    }

    /**
     * Set the editor widget setup for the field.
     *
     * @param v  The value to set
     */
    void setEditorWidgetSetup( const QgsEditorWidgetSetup& v );

    /**
     * Get the editor widget setup for the field.
     *
     * @return the value
     */
    const QgsEditorWidgetSetup& editorWidgetSetup() const;

  private:

    QSharedDataPointer<QgsFieldPrivate> d;


}; // class QgsField

Q_DECLARE_METATYPE( QgsField )

/** Writes the field to stream out. QGIS version compatibility is not guaranteed. */
CORE_EXPORT QDataStream& operator<<( QDataStream& out, const QgsField& field );
/** Reads a field from stream in into field. QGIS version compatibility is not guaranteed. */
CORE_EXPORT QDataStream& operator>>( QDataStream& in, QgsField& field );


#endif
