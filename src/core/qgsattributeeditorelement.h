/***************************************************************************
  qgsattributeeditorelement.h - QgsAttributeEditorElement

 ---------------------
 begin                : 18.8.2016
 copyright            : (C) 2016 by Matthias Kuhn
 email                : matthias@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSATTRIBUTEEDITORELEMENT_H
#define QGSATTRIBUTEEDITORELEMENT_H

#include "qgsrelation.h"
#include "qgsoptionalexpression.h"

class QgsRelationManager;

/** \ingroup core
 * This is an abstract base class for any elements of a drag and drop form.
 *
 * This can either be a container which will be represented on the screen
 * as a tab widget or ca collapsible group box. Or it can be a field which will
 * then be represented based on the QgsEditorWidget type and configuration.
 * Or it can be a relation and embed the form of several children of another
 * layer.
 */

class CORE_EXPORT QgsAttributeEditorElement
{
  public:
    enum AttributeEditorType
    {
      AeTypeContainer, //!< A container
      AeTypeField,     //!< A field
      AeTypeRelation,  //!< A relation
      AeTypeInvalid    //!< Invalid
    };

    /**
     * Constructor
     *
     * @param type The type of the new element. Should never
     * @param name
     * @param parent
     */
    QgsAttributeEditorElement( AttributeEditorType type, const QString& name, QgsAttributeEditorElement* parent = nullptr )
        : mType( type )
        , mName( name )
        , mParent( parent )
        , mShowLabel( true )
    {}

    //! Destructor
    virtual ~QgsAttributeEditorElement() {}

    /**
     * Return the name of this element
     *
     * @return The name for this element
     */
    QString name() const { return mName; }

    /**
     * The type of this element
     *
     * @return The type
     */
    AttributeEditorType type() const { return mType; }

    /**
     * Get the parent of this element.
     *
     * @note Added in QGIS 3.0
     */
    QgsAttributeEditorElement* parent() const { return mParent; }

    /**
     * Get the XML Dom element to save this element.
     *
     * @param doc The QDomDocument which is used to create new XML elements
     *
     * @return A DOM element to serialize this element
     */
    QDomElement toDomElement( QDomDocument& doc ) const;

    /**
     * Returns a clone of this element. To be implemented by subclasses.
     *
     * @note Added in QGIS 3.0
     */
    virtual QgsAttributeEditorElement* clone( QgsAttributeEditorElement* parent ) const = 0;

    /**
     * Controls if this element should be labeled with a title (field, relation or groupname).
     *
     * @note Added in QGIS 2.18
     */
    bool showLabel() const;

    /**
     * Controls if this element should be labeled with a title (field, relation or groupname).
     *
     * @note Added in QGIS 2.18
     */
    void setShowLabel( bool showLabel );

  protected:
    AttributeEditorType mType;
    QString mName;
    QgsAttributeEditorElement* mParent;
    bool mShowLabel;

  private:
    /**
     * Should be implemented by subclasses to save type specific configuration.
     *
     * @note Added in QGIS 2.18
     */
    virtual void saveConfiguration( QDomElement& elem ) const = 0;

    /**
     * All subclasses need to overwrite this method and return a type specific identifier.
     * Needs to be XML key compatible.
     *
     * @note Added in QGIS 2.18
     */
    virtual QString typeIdentifier() const = 0;
};


/** \ingroup core
 * This is a container for attribute editors, used to group them visually in the
 * attribute form if it is set to the drag and drop designer.
 */
class CORE_EXPORT QgsAttributeEditorContainer : public QgsAttributeEditorElement
{
  public:
    /**
     * Creates a new attribute editor container
     *
     * @param name   The name to show as title
     * @param parent The parent. May be another container.
     */
    QgsAttributeEditorContainer( const QString& name, QgsAttributeEditorElement* parent )
        : QgsAttributeEditorElement( AeTypeContainer, name, parent )
        , mIsGroupBox( true )
        , mColumnCount( 1 )
    {}

    //! Destructor
    virtual ~QgsAttributeEditorContainer();

    /**
     * Add a child element to this container. This may be another container, a field or a relation.
     *
     * @param element The element to add as child
     */
    virtual void addChildElement( QgsAttributeEditorElement* element );

    /**
     * Determines if this container is rendered as collapsible group box or tab in a tabwidget
     *
     * @param isGroupBox If true, this will be a group box
     */
    virtual void setIsGroupBox( bool isGroupBox ) { mIsGroupBox = isGroupBox; }

    /**
     * Returns if this container is going to be rendered as a group box
     *
     * @return True if it will be a group box, false if it will be a tab
     */
    virtual bool isGroupBox() const { return mIsGroupBox; }

    /**
     * Get a list of the children elements of this container
     *
     * @return A list of elements
     */
    QList<QgsAttributeEditorElement*> children() const { return mChildren; }

    /**
     * Traverses the element tree to find any element of the specified type
     *
     * @param type The type which should be searched
     *
     * @return A list of elements of the type which has been searched for
     */
    virtual QList<QgsAttributeEditorElement*> findElements( AttributeEditorType type ) const;

    /**
     * Clear all children from this container.
     */
    void clear();

    /**
     * Change the name of this container
     */
    void setName( const QString& name );

    /**
     * Get the number of columns in this group
     */
    int columnCount() const;

    /**
     * Set the number of columns in this group
     */
    void setColumnCount( int columnCount );

    /**
     * Creates a deep copy of this element. To be implemented by subclasses.
     *
     * @note Added in QGIS 3.0
     */
    virtual QgsAttributeEditorElement* clone( QgsAttributeEditorElement* parent ) const override;

    /**
     * The visibility expression is used in the attribute form to
     * show or hide this container based on an expression incorporating
     * the field value controlled by editor widgets.
     *
     * @note Added in QGIS 3.0
     */
    QgsOptionalExpression visibilityExpression() const;

    /**
     * The visibility expression is used in the attribute form to
     * show or hide this container based on an expression incorporating
     * the field value controlled by editor widgets.
     *
     * @note Added in QGIS 3.0
     */
    void setVisibilityExpression( const QgsOptionalExpression& visibilityExpression );

  private:
    void saveConfiguration( QDomElement& elem ) const override;
    QString typeIdentifier() const override;

    bool mIsGroupBox;
    QList<QgsAttributeEditorElement*> mChildren;
    int mColumnCount;
    QgsOptionalExpression mVisibilityExpression;
};

/** \ingroup core
 * This element will load a field's widget onto the form.
 */
class CORE_EXPORT QgsAttributeEditorField : public QgsAttributeEditorElement
{
  public:
    /**
     * Creates a new attribute editor element which represents a field
     *
     * @param name   The name of the element
     * @param idx    The index of the field which should be embedded
     * @param parent The parent of this widget (used as container)
     */
    QgsAttributeEditorField( const QString& name, int idx, QgsAttributeEditorElement *parent )
        : QgsAttributeEditorElement( AeTypeField, name, parent )
        , mIdx( idx )
    {}

    //! Destructor
    virtual ~QgsAttributeEditorField() {}

    /**
     * Return the index of the field
     * @return
     */
    int idx() const { return mIdx; }

    virtual QgsAttributeEditorElement* clone( QgsAttributeEditorElement* parent ) const override;

  private:
    void saveConfiguration( QDomElement& elem ) const override;
    QString typeIdentifier() const override;
    int mIdx;
};

/** \ingroup core
 * This element will load a relation editor onto the form.
 */
class CORE_EXPORT QgsAttributeEditorRelation : public QgsAttributeEditorElement
{
  public:
    /**
     * Creates a new element which embeds a relation.
     *
     * @param name         The name of this element
     * @param relationId   The id of the relation to embed
     * @param parent       The parent (used as container)
     */
    QgsAttributeEditorRelation( const QString& name, const QString &relationId, QgsAttributeEditorElement* parent )
        : QgsAttributeEditorElement( AeTypeRelation, name, parent )
        , mRelationId( relationId )
        , mShowLinkButton( true )
        , mShowUnlinkButton( true )
    {}

    /**
     * Creates a new element which embeds a relation.
     *
     * @param name         The name of this element
     * @param relation     The relation to embed
     * @param parent       The parent (used as container)
     */
    QgsAttributeEditorRelation( const QString& name, const QgsRelation& relation, QgsAttributeEditorElement* parent )
        : QgsAttributeEditorElement( AeTypeRelation, name, parent )
        , mRelationId( relation.id() )
        , mRelation( relation )
        , mShowLinkButton( true )
        , mShowUnlinkButton( true )
    {}

    //! Destructor
    virtual ~QgsAttributeEditorRelation() {}

    /**
     * Get the id of the relation which shall be embedded
     *
     * @return the id
     */
    const QgsRelation& relation() const { return mRelation; }

    /**
     * Initializes the relation from the id
     *
     * @param relManager The relation manager to use for the initialization
     * @return true if the relation was found in the relationmanager
     */
    bool init( QgsRelationManager* relManager );

    virtual QgsAttributeEditorElement* clone( QgsAttributeEditorElement* parent ) const override;

    /**
     * Determines if the "link feature" button should be shown
     *
     * @note Added in QGIS 2.18
     */
    bool showLinkButton() const;
    /**
     * Determines if the "link feature" button should be shown
     *
     * @note Added in QGIS 2.18
     */
    void setShowLinkButton( bool showLinkButton );

    /**
     * Determines if the "unlink feature" button should be shown
     *
     * @note Added in QGIS 2.18
     */
    bool showUnlinkButton() const;
    /**
     * Determines if the "unlink feature" button should be shown
     *
     * @note Added in QGIS 2.18
     */
    void setShowUnlinkButton( bool showUnlinkButton );


  private:
    void saveConfiguration( QDomElement& elem ) const override;
    QString typeIdentifier() const override;
    QString mRelationId;
    QgsRelation mRelation;
    bool mShowLinkButton;
    bool mShowUnlinkButton;
};


#endif // QGSATTRIBUTEEDITORELEMENT_H
