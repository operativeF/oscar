/* UpdateParser Header (Autoupdater component)
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef UPDATEPARSER_H
#define UPDATEPARSER_H

#include <QXmlDefaultHandler>
#include <QXmlStreamReader>
#include <QMetaType>
#include <QDate>

enum UpdateStatus { UPDATE_TESTING = 0, UPDATE_BETA, UPDATE_STABLE, UPDATE_CRITICAL };

/*! \struct Update
    \brief Holds platform specific information about an individual updates
  */
class Update
{
  public:
    Update();
    Update(const Update &copy);
    Update(QString _type, QString _version, QString _platform, QDate _date);
    QString type;
    QString version;
    QString platform;
    UpdateStatus status;
    QDate date;
    QString filename;
    QString url;
    QString hash;
    qint64 size;
    QString notes;
    QString unzipped_path;
};

/*! \struct Release
    \brief Holds information about an individual major release
  */
struct Release {
    Release() {}
    Release(const Release &copy) {
        version = copy.version;
        codename = copy.version;
        notes = copy.notes;
        info_url = copy.info_url;
        status = copy.status;
        updates = copy.updates;
    }

    Release(QString ver, QString code, UpdateStatus stat) { version = ver; codename = code; status = stat; }
    QString version;
    QString codename;
    UpdateStatus status;
    QString info_url;
    QHash<QString, QString> notes; // by platform
    QHash<QString, QList<Update> > updates;
};

Q_DECLARE_METATYPE(Update *)


/*! \class UpdateParser
    \brief SAX XML parser for update.xml
  */
class UpdateParser: public QXmlDefaultHandler
{
  public:
    bool startDocument();
    bool endElement(const QString &namespaceURI, const QString &localName, const QString &name);
    bool characters(const QString &ch);
    bool startElement(const QString &namespaceURI, const QString &localName, const QString &name,
                      const QXmlAttributes &atts);
    bool endDocument();
    QString latest() { return latest_version; }

    QHash<QString, Release> releases;
  private:
    Update *update;
    Release *release;
    QString version, platform;
    QString release_date;
    QString latest_version;
    bool inRelease;
    bool inUpdate;
    bool inNotes;
    bool inUpdateNotes;
};

class PackageUpdate {
public:
    PackageUpdate() {}
    PackageUpdate(const PackageUpdate & copy) {
        // Seriously, why do I still have to do this crud by hand
        // Where is the shortcut to save time here in the latest C++ extensions?
        name = copy.name;
        displayName = copy.displayName;
        description = copy.description;
        versionString = copy.versionString;
        releaseDate = copy.releaseDate;
        defaultInstall = copy.defaultInstall;
        installScript = copy.installScript;
        dependencies = copy.dependencies;
        script = copy.script;
        forcedInstall = copy.forcedInstall;
        downloadArchives = copy.downloadArchives;
        license = copy.license;
        sha1 = copy.sha1;
        compressedSize = copy.compressedSize;
        uncompressedSize = copy.uncompressedSize;
        os = copy.os;
    }

    QString name;
    QString displayName;
    QString description;
    QString versionString;
    QDate releaseDate;
    bool defaultInstall;
    QString installScript;
    QStringList dependencies;
    QString script;
    bool forcedInstall;
    QStringList downloadArchives;
    QHash<QString, QString> license;
    QString sha1;
    unsigned int compressedSize;
    unsigned int uncompressedSize;
    QString os;
};


/*! \class UpdatesParser
    \brief New SAX XML parser for QT Installer Frameworks Updates.xml
  */
class UpdatesParser
{
  public:
    UpdatesParser();
    bool read(QIODevice *device);
    QString errorString() const;
    QHash<QString, PackageUpdate> packages;

  private:
    void readUpdates();
    void readPackageUpdate();

    QXmlStreamReader xml;
    PackageUpdate package;
    QString currentTag;
};

#endif // UPDATEPARSER_H
