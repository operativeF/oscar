/* SleepLib Preferences Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QString>
#include <QVariant>
#include <QHash>
#include <QDomElement>
#include <QDomDocument>
#include <map>

const QString STR_ext_XML = ".xml";

extern QString GetAppRoot(); //returns app root path plus trailing path separator.

inline QString PrefMacro(QString s)
{
    return "{" + s + "}";
}

//! \brief Returns a QString containing the Username, according to the Operating System
const QString &getUserName();


/*! \class Preferences
    \author Mark Watkins <jedimark_at_users.sourceforge.net>
    \brief Holds a group of preference variables
    */
class Preferences
{
  public:
    //! \brief Constructs a Preferences object 'name', and remembers sets the filename
    Preferences(QString name, QString filename = "");
    Preferences();
    virtual ~Preferences();

    //! \brief Returns a QString containing preference 'name', processing any {} macros
    const QString Get(QString name);

    //! \brief Returns the QVariant value of the selected preference.. Note, preference must exist, and will not expand {} macros
    QVariant &operator[](QString name) {
        return p_preferences[name];
    }

    //! \brief Sets the Preference 'name' to QVariant 'value'
    void Set(QString name, QVariant value) {
        p_preferences[name] = value;
    }

    //! \brief Returns true if preference 'name' exists
    bool contains(QString name) {
        return (p_preferences.contains(name));
    }

    //! \brief Create a preference and set the default if it doesn't exists
    QVariant & init(QString name, QVariant value) {
        auto it = p_preferences.find(name);
        if (it == p_preferences.end()) {
            return p_preferences[name] = value;
        }
        return it.value();
    }

    //! \brief Returns true if preference 'name' exists, and contains a boolean true value
    bool ExistsAndTrue(QString name) {
        QHash<QString, QVariant>::iterator i = p_preferences.find(name);

        if (i == p_preferences.end()) { return false; }

        return i.value().toBool();
    }

    //! \brief Removes preference 'name' from this Preferences group
    void Erase(QString name) {
        QHash<QString, QVariant>::iterator i = p_preferences.find(name);

        if (i != p_preferences.end()) {
            p_preferences.erase(i);
        }
    }

    //! \brief Opens, processes the XML for this Preferences group, loading all preferences stored therein.
    //! \note If filename is empty, it will use the one specified in the constructor
    //! \returns true if succesful
    bool Open(QString filename = "");

    //! \brief Saves all preferences to XML file.
    //! \note If filename is empty, it will use the one specified in the constructor
    //! \returns true if succesful
    bool Save(QString filename = "");

    //! \note Sets a comment string whici will be stored in the XML
    void SetComment(const QString &str) {
        p_comment = str;
    }

    //! \brief Finds a given preference.
    //! \returns a QHash<QString,QString>::iterator pointing to the preference named 'key', or an empty end() iterator
    inline QHash<QString, QVariant>::iterator find(QString key) { return p_preferences.find(key); }

    //! \brief Returns an empty iterator pointing to the end of the preferences list
    inline QHash<QString, QVariant>::iterator end() { return p_preferences.end(); }

    //! \brief Returns an iterator pointing to the first item in the preferences list
    inline QHash<QString, QVariant>::iterator begin() { return p_preferences.begin(); }

    //int  GetCode(QString name); // For registering/looking up new preference code.

    //! \brief Stores all the variants indexed by a QString name for this Preferences object
    QHash<QString, QVariant> p_preferences;

    void setPath(const QString &path) { p_path = path; }
    void setFilename(const QString &filename) { p_filename = filename; }

    const QString name() { return p_name; }

  protected:
    //QHash<int,QString> p_codes;
    QString p_comment;
    QString p_name;
    QString p_filename;
    QString p_path;
};

//! \brief Main Preferences Object used throughout the application
extern Preferences PREF;

// Parent class for subclasses that manipulate the profile.
class PrefSettings
{
  public:
    PrefSettings(Preferences *pref)
      : m_pref(pref)
    { }

    inline void setPref(QString name, QVariant value) {
        (*m_pref)[name] = value;
    }

    inline QVariant & initPref(QString name, QVariant value) {
        return m_pref->init(name, value);
    }

    inline QVariant & getPref(QString name) const {
        return (*m_pref)[name];
    }

    void setPrefObject(Preferences *pref) {
        m_pref = pref;
    }

  public:
    Preferences *m_pref;
};
#include "appsettings.h"

#endif // PREFERENCES_H

