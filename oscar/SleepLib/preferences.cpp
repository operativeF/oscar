/* SleepLib Preferences Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QString>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QVariant>
#include <QDateTime>
#include <QDir>
#include <QDesktopServices>
#include <QDebug>
#include <QSettings>

#ifdef Q_OS_WIN
#include "windows.h"
#include "lmcons.h"
#endif

#include "common.h"
#include "preferences.h"

const QString &getUserName()
{
    static QString userName;
    userName = getenv("USER");

    if (userName.isEmpty()) {
        userName = QObject::tr("Windows User");

#if defined (Q_OS_WIN)
#if defined(UNICODE)

        if (QSysInfo::WindowsVersion >= QSysInfo::WV_NT) {
            TCHAR winUserName[UNLEN + 1]; // UNLEN is defined in LMCONS.H
            DWORD winUserNameSize = sizeof(winUserName);
            GetUserNameW(winUserName, &winUserNameSize);
            userName = QString::fromStdWString(winUserName);
        } else
#endif
        {
            char winUserName[UNLEN + 1]; // UNLEN is defined in LMCONS.H
            DWORD winUserNameSize = sizeof(winUserName);
            GetUserNameA(winUserName, &winUserNameSize);
            userName = QString::fromLocal8Bit(winUserName);
        }

#endif
    }

    return userName;
}


QString GetAppRoot()
{
    QSettings settings;

    QString HomeAppRoot = settings.value("Settings/AppRoot").toString();

    if (HomeAppRoot.isEmpty()) {
        HomeAppRoot = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/"+getDefaultAppRoot();
    }

    return HomeAppRoot;
}


Preferences::Preferences()
{
    p_name = "Preferences";
    p_path = GetAppRoot();
}

Preferences::Preferences(QString name, QString filename)
{
    if (name.endsWith(STR_ext_XML)) {
        p_name = name.section(".", 0, 0);
    } else {
        p_name = name;
    }

    if (filename.isEmpty()) {
        p_filename = GetAppRoot() + "/" + p_name + STR_ext_XML;
    } else {
        if (!filename.contains("/")) {
            p_filename = GetAppRoot() + "/";
        } else { p_filename = ""; }

        p_filename += filename;

        if (!p_filename.endsWith(STR_ext_XML)) { p_filename += STR_ext_XML; }
    }
}

Preferences::~Preferences()
{
    //Save(); // Don't..Save calls a virtual function.
}

/*int Preferences::GetCode(QString s)
{
    int prefcode=0;
    for (QHash<int,QString>::iterator i=p_codes.begin(); i!=p_codes.end(); i++) {
        if (i.value()==s) return i.key();
        prefcode++;
    }
    p_codes[prefcode]=s;
    return prefcode;
}*/

const QString Preferences::Get(QString name)
{
    QString temp;
    QChar obr = QChar('{');
    QChar cbr = QChar('}');
    QString t, a, ref; // How I miss Regular Expressions here..

    if (p_preferences.find(name) != p_preferences.end()) {
        temp = "";
        t = p_preferences[name].toString();

        if (p_preferences[name].type() != QVariant::String) {
            return t;
        }
    } else {
        t = name; // parse the string..
    }

    while (t.contains(obr)) {
        temp += t.section(obr, 0, 0);
        a = t.section(obr, 1);

        if (a.startsWith("{")) {
            temp += obr;
            t = a.section(obr, 1);
            continue;
        }

        ref = a.section(cbr, 0, 0);

        if (ref.toLower() == "home") {
            temp += GetAppRoot();
        } else if (ref.toLower() == "user") {
            temp += getUserName();
        } else if (ref.toLower() == "sep") { // redundant in QT
            temp += "/";
        } else {
            temp += Get(ref);
        }

        t = a.section(cbr, 1);
    }

    temp += t;
    temp.replace("}}", "}"); // Make things look a bit better when escaping braces.

    return temp;
}


bool Preferences::Open(QString filename)
{
    if (!filename.isEmpty()) {
        p_filename = filename;
    }

    QDomDocument doc(p_name);
    QFile file(p_filename);
    qDebug() << "Reading " << p_filename.toLocal8Bit().data();

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open" << p_filename.toLocal8Bit().data();
        return false;
    }

    QString errorMsg;
    int errorLine;
    int errorColumn;
    if (!doc.setContent(&file,false, &errorMsg, &errorLine, &errorColumn)) {
        qWarning() << "Invalid XML Content in" << p_filename.toLocal8Bit().data();
        qWarning() << "Error:" << errorMsg << "in line" << errorLine << ":" << errorColumn;
        return false;
    }

    file.close();


    QDomElement root = doc.documentElement();

    if (root.tagName() != STR_AppName) {
        return false;
    }

    root = root.firstChildElement();

    if (root.tagName() != p_name) {
        return false;
    }

    bool ok;
    p_preferences.clear();
    QDomNode n = root.firstChild();

    while (!n.isNull()) {
        QDomElement e = n.toElement();

        if (!e.isNull()) {
            QString name = e.tagName();
            QString type = e.attribute("type").toLower();
            QString value = e.text();

            if (type == "double") {
                double d;
                d = value.toDouble(&ok);

                if (ok) {
                    p_preferences[name] = d;
                } else {
                    qDebug() << "XML Error:" << name << "=" << value << "??";
                }
            } else if (type == "qlonglong") {
                qint64 d;
                d = value.toLongLong(&ok);

                if (ok) {
                    p_preferences[name] = d;
                } else {
                    qDebug() << "XML Error:" << name << "=" << value << "??";
                }
            } else if (type == "int") {
                int d;
                d = value.toInt(&ok);

                if (ok) {
                    p_preferences[name] = d;
                } else {
                    qDebug() << "XML Error:" << name << "=" << value << "??";
                }
            } else if (type == "bool") {
                QString v = value.toLower();

                if ((v == "true") || (v == "on") || (v == "yes")) {
                    p_preferences[name] = true;
                } else if ((v == "false") || (v == "off") || (v == "no")) {
                    p_preferences[name] = false;
                } else {
                    int d;
                    d = value.toInt(&ok);

                    if (ok) {
                        p_preferences[name] = d != 0;
                    } else {
                        qDebug() << "XML Error:" << name << "=" << value << "??";
                    }
                }
            } else if (type == "qdatetime") {
                QDateTime d;
                d = QDateTime::fromString(value, "yyyy-MM-dd HH:mm:ss");

                if (d.isValid()) {
                    p_preferences[name] = d;
                } else {
                    qWarning() << "XML Error: Invalid DateTime record" << name << value;
                }

            } else if (type == "qtime") {
                QTime d;
                d = QTime::fromString(value, "hh:mm:ss");

                if (d.isValid()) {
                    p_preferences[name] = d;
                } else {
                    qWarning() << "XML Error: Invalid Time record" << name << value;
                }

            } else  {
                p_preferences[name] = value;
            }

        }

        n = n.nextSibling();
    }

    root = root.nextSiblingElement();

    //////////////////////////////////////////////////////////////////////////////////////
    // This is a dirty hack to clean up a legacy issue
    // The old Profile system used to have machines in Profile.xml
    // We need to clean up this mistake up here, because C++ polymorphism won't otherwise
    // let us open properly in constructor
    //////////////////////////////////////////////////////////////////////////////////////
    if ((p_name == "Profile") && (root.tagName().toLower() == "machines")) {

        // Save this sucker
        QDomDocument doc("Machines");

        doc.appendChild(root);

        QFile file(p_path+"/machines.xml");

        // Don't do anything if machines.xml already exists.. the user ran the old version!
        if (!file.exists()) {
            file.open(QFile::WriteOnly);
            file.write(doc.toByteArray());
            file.close();
        }
    }
    return true;
}

bool Preferences::Save(QString filename)
{
    if (!filename.isEmpty()) {
        p_filename = filename;
    }

    QDomDocument doc(p_name);

    QDomElement droot = doc.createElement(STR_AppName);
    doc.appendChild(droot);

    QDomElement root = doc.createElement(p_name);
    droot.appendChild(root);

    for (QHash<QString, QVariant>::iterator i = p_preferences.begin(); i != p_preferences.end(); i++) {
        QVariant::Type type = i.value().type();

        if (type == QVariant::Invalid) { continue; }

        QDomElement cn = doc.createElement(i.key());
        cn.setAttribute("type", i.value().typeName());

        if (type == QVariant::DateTime) {
            cn.appendChild(doc.createTextNode(i.value().toDateTime().toString("yyyy-MM-dd HH:mm:ss")));
        } else if (type == QVariant::Time) {
            cn.appendChild(doc.createTextNode(i.value().toTime().toString("hh:mm:ss")));
        } else {
            cn.appendChild(doc.createTextNode(i.value().toString()));
        }

        root.appendChild(cn);
    }

    QFile file(p_filename);

    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QTextStream ts(&file);
    ts << doc.toString();
    file.close();

    return true;
}


AppWideSetting *AppSetting = nullptr;

