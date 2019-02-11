/* SleepLib Profiles Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QString>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QDebug>
#include <QProcess>
#include <QByteArray>
#include <QHostInfo>
#include <QApplication>
#include <QSettings>
#include <algorithm>
#include <cmath>

#include "preferences.h"
#include "profiles.h"
#include "machine.h"
#include "machine_common.h"

#include "machine_loader.h"

#include "mainwindow.h"
#include "translation.h"

extern MainWindow *mainwin;
Preferences *p_pref;
Preferences *p_layout;
Profile *p_profile;

Profile::Profile(QString path)
  : is_first_day(true),
     m_opened(false),
     m_machopened(false)
{
    p_name = STR_GEN_Profile;

    if (path.isEmpty()) {
        p_path = GetAppRoot();
    } else {
        p_path = path;
    }

    (*this)[STR_GEN_DataFolder] = p_path;
    path = path.replace("\\", "/");

    if (!p_path.endsWith("/")) {
        p_path += "/";
    }

    p_filename = p_path + p_name + STR_ext_XML;
    m_machlist.clear();

    Open(p_filename);

    Set(STR_GEN_DataFolder, QString("{home}/Profiles/{UserName}"));

    doctor = new DoctorInfo(this);
    user = new UserInfo(this);
    cpap = new CPAPSettings(this);
    oxi = new OxiSettings(this);
    appearance = new AppearanceSettings(this);
    session = new SessionSettings(this);
    general = new UserSettings(this);

    OpenMachines();
    m_opened=true;
}

Profile::~Profile()
{
    removeLock();

    delete user;
    delete doctor;
    delete cpap;
    delete oxi;
    delete appearance;
    delete session;
    delete general;

    // delete machine objects...
    for (auto & mach : m_machlist) {
        delete mach;
    }

    for (auto & day : daylist) {
        delete day;
    }

}

bool Profile::Save(QString filename)
{
    if (m_opened) {
        return Preferences::Save(filename) && StoreMachines();
    } else return false;
}

bool Profile::removeLock()
{
    QString filename=p_path+"/lockfile";
    QFile file(filename);
    return file.remove();
}

QString Profile::checkLock()
{

    QString filename=p_path+"/lockfile";
    QFile file(filename);

    if (!file.exists())
        return QString();

    file.open(QFile::ReadOnly);
    QString lockhost = file.readLine(1024).trimmed();
    return lockhost;
}

const QString STR_PROP_Brand = "brand";
const QString STR_PROP_Model = "model";
const QString STR_PROP_Series = "series";
const QString STR_PROP_ModelNumber = "modelnumber";
const QString STR_PROP_SubModel = "submodel";
const QString STR_PROP_Serial = "serial";
const QString STR_PROP_DataVersion = "dataversion";
const QString STR_PROP_LastImported = "lastimported";

void Profile::addLock()
{
    QFile lockfile(p_path+"lockfile");
    lockfile.open(QFile::WriteOnly);
    QByteArray ba;
    ba.append(QHostInfo::localHostName());
    lockfile.write(ba);
    lockfile.close();
}

bool Profile::OpenMachines()
{
    if (m_machopened)
        return true;

    QString filename = p_path+"machines.xml";
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Could not open" << filename.toLocal8Bit().data();
        return false;
    }
    QDomDocument doc("machines.xml");

    if (!doc.setContent(&file)) {
        qWarning() << "Invalid XML Content in" << filename.toLocal8Bit().data();
        return false;
    }
    file.close();
    QDomElement root = doc.firstChild().toElement();

    if (root.tagName().toLower() != "machines") {
        //qDebug() << "No Machines Tag in Profiles.xml";
        return false;
    }

    QDomElement elem = root.firstChildElement();

    while (!elem.isNull()) {
        QString pKey = elem.tagName();

        if (pKey.toLower() != "machine") {
            qWarning() << "Profile::OpenMachines() pKey!=\"machine\"";
            elem = elem.nextSiblingElement();
            continue;
        }

        int m_id;
        bool ok;
        m_id = elem.attribute("id", "").toInt(&ok);
        int mt;
        mt = elem.attribute("type", "").toInt(&ok);
        MachineType m_type = (MachineType)mt;

        QString m_class = elem.attribute("class", "");

        MachineInfo info;

        info.type = m_type;
        info.loadername = m_class;

        QHash<QString, QString> prop;

        QDomElement e = elem.firstChildElement();

        for (; !e.isNull(); e = e.nextSiblingElement()) {
            QString pKey = e.tagName();
            QString key = pKey.toLower();
            if (key == STR_PROP_Brand) {
                info.brand = e.text();
            } else if (key == STR_PROP_Model) {
                info.model = e.text();
            } else if (key == STR_PROP_ModelNumber) {
                info.modelnumber = e.text();
            } else if (key == STR_PROP_Serial) {
                info.serial = e.text();
            } else if (key == STR_PROP_Series) {
                info.series = e.text();
            } else if (key == STR_PROP_DataVersion) {
                info.version = e.text().toInt();
            } else if (key == STR_PROP_LastImported) {
                info.lastimported = QDateTime::fromString(e.text(), Qt::ISODate);
            } else if (key == "properties") {
                QDomElement pe = e.firstChildElement();
                for (; !pe.isNull(); pe = pe.nextSiblingElement()) {
                    prop[pe.tagName()] = pe.text();
                }
            } else {
                // skip any old rubbish
                if ((key == "backuppath") || (key == "path") || (key == "submodel")) continue;

                prop[pKey] = e.text();
            }
        }

        Machine *m = nullptr;

        // Create Machine needs a profile passed to it..

        m = CreateMachine(info, m_id);

        if (m) m->properties = prop;

        elem = elem.nextSiblingElement();
    }

    m_machopened = true;
    return true;
}

bool Profile::StoreMachines()
{
    QDomDocument doc("Machines");

    QDomElement mach = doc.createElement("machines");

    for (int i=0; i<m_machlist.size(); ++i) {
        Machine *m = m_machlist[i];

        QDomElement me = doc.createElement("machine");
        me.setAttribute("id", (int)m->id());
        me.setAttribute("type", (int)m->type());
        me.setAttribute("class", m->loaderName());

        QDomElement pe = doc.createElement("properties");
        me.appendChild(pe);

        for (QHash<QString, QString>::iterator j = m->properties.begin(); j != m->properties.end(); j++) {
            QDomElement pp = doc.createElement(j.key());
            pp.appendChild(doc.createTextNode(j.value()));
            pe.appendChild(pp);
        }

        QDomElement mp = doc.createElement(STR_PROP_Brand);
        mp.appendChild(doc.createTextNode(m->brand()));
        me.appendChild(mp);

        mp = doc.createElement(STR_PROP_Model);
        mp.appendChild(doc.createTextNode(m->model()));
        me.appendChild(mp);

        mp = doc.createElement(STR_PROP_ModelNumber);
        mp.appendChild(doc.createTextNode(m->modelnumber()));
        me.appendChild(mp);

        mp = doc.createElement(STR_PROP_Serial);
        mp.appendChild(doc.createTextNode(m->serial()));
        me.appendChild(mp);

        mp = doc.createElement(STR_PROP_Series);
        mp.appendChild(doc.createTextNode(m->series()));
        me.appendChild(mp);

        mp = doc.createElement(STR_PROP_DataVersion);
        mp.appendChild(doc.createTextNode(QString::number(m->version())));
        me.appendChild(mp);

        mp = doc.createElement(STR_PROP_LastImported);
        mp.appendChild(doc.createTextNode(m->lastImported().toString(Qt::ISODate)));
        me.appendChild(mp);

        mach.appendChild(me);
    }

    doc.appendChild(mach);

    QString filename = p_path+"machines.xml";
    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        return false;
    }
    file.write(doc.toByteArray());
    return true;
}

qint64 Profile::diskSpaceSummaries()
{
    qint64 size = 0;
    for (auto & mach : m_machlist) {
        size += mach->diskSpaceSummaries();
    }
    return size;
}
qint64 Profile::diskSpaceEvents()
{
    qint64 size = 0;
    for (auto & mach : m_machlist) {
        size += mach->diskSpaceEvents();
    }
    return size;
}
qint64 Profile::diskSpaceBackups()
{
    qint64 size = 0;
    for (auto & mach : m_machlist) {
        size += mach->diskSpaceBackups();
    }
    return size;
}
qint64 Profile::diskSpace()
{
    return (diskSpaceSummaries()+diskSpaceEvents()+diskSpaceBackups());
}



#if defined(Q_OS_WIN)
class Environment
{
public:
    Environment();

    QStringList path();
    QString searchInDirectory(const QStringList & execs, QString directory);
    QString searchInPath(const QString &executable, const QStringList & additionalDirs = QStringList());

    QProcessEnvironment env;
};
Environment::Environment()
{
    env = QProcessEnvironment::systemEnvironment();
}

QStringList Environment::path()
{
    return env.value(QLatin1String("PATH"), "").split(';');
}

QString Environment::searchInDirectory(const QStringList & execs, QString directory)
{
    const QChar slash = QLatin1Char('/');

    if (directory.isEmpty())
        return QString();

    if (!directory.endsWith(slash))
        directory += slash;

    for (auto & exec : execs) {
        QFileInfo fi(directory + exec);
        if (fi.exists() && fi.isFile() && fi.isExecutable())
            return fi.absoluteFilePath();
    }
    return QString();
}

QString Environment::searchInPath(const QString &executable, const QStringList & additionalDirs)
{
    if (executable.isEmpty()) return QString();

    QString exec = QDir::cleanPath(executable);
    QFileInfo fi(exec);

    QStringList execs(exec);

    if (fi.suffix().isEmpty()) {
        QStringList extensions = env.value(QLatin1String("PATHEXT")).split(QLatin1Char(';'));

        foreach (const QString &ext, extensions) {
            QString tmp = executable + ext.toLower();
            if (fi.isAbsolute()) {
                if (QFile::exists(tmp))
                    return tmp;
            } else {
                execs << tmp;
            }
        }
    }

    if (fi.isAbsolute())
        return exec;

    QSet<QString> alreadyChecked;
    foreach (const QString &dir, additionalDirs) {
        if (alreadyChecked.contains(dir))
            continue;
        alreadyChecked.insert(dir);
        QString tmp = searchInDirectory(execs, dir);
        if (!tmp.isEmpty())
            return tmp;
    }

    if (executable.indexOf(QLatin1Char('/')) != -1)
        return QString();

    for (auto & p : path()) {
        if (alreadyChecked.contains(p))
            continue;
        alreadyChecked.insert(p);
        QString tmp = searchInDirectory(execs, QDir::fromNativeSeparators(p));
        if (!tmp.isEmpty())
            return tmp;
    }
    return QString();
}
#endif

// Borrowed from QtCreator (http://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt)
void showInGraphicalShell(const QString & pathIn)
{

    // Mac, Windows support folder or file.
#if defined(Q_OS_WIN)
    QWidget * parent = NULL;
    Environment env;
    const QString explorer = env.searchInPath(QLatin1String("explorer.exe"));
    if (explorer.isEmpty()) {
        QMessageBox::warning(parent,
                             QObject::tr("Launching Windows Explorer failed"),
                             QObject::tr("Could not find explorer.exe in path to launch Windows Explorer."));
        return;
    }
    QString param;
    //if (!QFileInfo(pathIn).isDir())
        param = QLatin1String("/select,");
    param += QDir::toNativeSeparators(pathIn);
    QProcess::startDetached(explorer, QStringList(param));
#elif defined(Q_OS_MAC)
   // Q_UNUSED(parent)
    QStringList scriptArgs;
    scriptArgs << QLatin1String("-e")
               << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
                                     .arg(pathIn);
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    scriptArgs.clear();
    scriptArgs << QLatin1String("-e")
               << QLatin1String("tell application \"Finder\" to activate");
    QProcess::execute("/usr/bin/osascript", scriptArgs);
#else
    Q_UNUSED(pathIn);
    // we cannot select a file here, because no file browser really supports it...
    /*
    const QFileInfo fileInfo(pathIn);
    const QString folder = fileInfo.absoluteFilePath();
    const QString app = Utils::UnixUtils::fileBrowser(Core::ICore::instance()->settings());
    QProcess browserProc;
    const QString browserArgs = Utils::UnixUtils::substituteFileBrowserParameters(app, folder);
    if (debug)
        qDebug() <<  browserArgs;
    bool success = browserProc.startDetached(browserArgs);
    const QString error = QString::fromLocal8Bit(browserProc.readAllStandardError());
    success = success && error.isEmpty();
    if (!success) {
        QMessageBox::warning(NULL,STR_MessageBox_Error, "Could not find the file browser for your system, you will have to find your profile directory yourself."+"\n\n"+error, QMessageBox::Ok);
//        showGraphicalShellError(parent, app, error);
    }*/
#endif
}

int dirCount(QString path)
{
    QDir dir(path);

    QStringList list = dir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    return list.size();
}

void Profile::DataFormatError(Machine *m)
{
    QString msg;

    msg = "<font size=+1>"+QObject::tr("SleepyHead (%1) needs to upgrade its database for %2 %3 %4").
            arg(VersionString).
            arg(m->brand()).arg(m->model()).arg(m->serial())
            + "</font><br/><br/>";

    bool backups = false;
    if (p_profile->session->backupCardData()) {
        QString bpath = m->getBackupPath();
        int cnt = dirCount(bpath);
        if (cnt > 0) backups = true;
    }

    if (backups) {
        msg = msg + QObject::tr("<b>SleepyHead maintains a backup of your devices data card that it uses for this purpose.</b>")+ "<br/><br/>";
        msg = msg + QObject::tr("<i>Your old machine data should be regenerated provided this backup feature has not been disabled in preferences during a previous data import.</i>") + "<br/><br/>";
        backups = true;
    } else {
        msg = msg + "<font size=+1>"+STR_MessageBox_Warning+":</font> "+QObject::tr("SleepyHead does not yet have any automatic card backups stored for this device.") + "<br/><br/>";
        msg = msg + QObject::tr("This means you will need to import this machine data again afterwards from your own backups or data card.") + "<br/><br/>";
    }

    msg += "<font size=+1>"+QObject::tr("Important:")+"</font> "+QObject::tr("Once you upgrade, you <font size=+1>can not</font> use this profile with the previous version anymore.")+"<br/><br/>"+
            QObject::tr("If you are concerned, click No to exit, and backup your profile manually, before starting SleepyHead again.")+ "<br/><br/>";
    msg = msg + "<font size=+1>"+QObject::tr("Are you ready to upgrade, so you can run the new version of SleepyHead?")+"</font>";


    QMessageBox * question = new QMessageBox(QMessageBox::Warning, QObject::tr("Machine Database Changes"), msg, QMessageBox::Yes | QMessageBox::No);
    question->setDefaultButton(QMessageBox::Yes);

    QFont font("Sans Serif", 11, QFont::Normal);

    question->setFont(font);

    if (question->exec() == QMessageBox::Yes) {
        if (!m->Purge(3478216)) {
            // Purge failed.. probably a permissions error.. let the user deal with it.
            QMessageBox::critical(nullptr, STR_MessageBox_Error,
                                  QObject::tr("Sorry, the purge operation failed, which means this version of SleepyHead can't start.")+"\n\n"+
                                  QObject::tr("The machine data folder needs to be removed manually.")+"\n\n"+
                                  QObject::tr("This folder currently resides at the following location:")+"\n\n"+
                                  QDir::toNativeSeparators(Get(p_preferences[STR_GEN_DataFolder].toString())), QMessageBox::Ok);
            QApplication::exit(-1);
        }
        // Note: I deliberately haven't added a Profile help for this
        if (backups) {
            mainwin->importCPAP(ImportPath(m->getBackupPath(), lookupLoader(m)), QObject::tr("Rebuilding from %1 Backup").arg(m->brand()));
        } else {
            if (!p_profile->session->backupCardData()) {
                // Automatic backups not available for Intellipap users yet, so don't taunt them..
                if (m->loaderName() != STR_MACH_Intellipap) {
                    if (QMessageBox::question(nullptr, STR_MessageBox_Question, QObject::tr("Would you like to switch on automatic backups, so next time a new version of SleepyHead needs to do so, it can rebuild from these?"),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)) {
                        p_profile->session->setBackupCardData(true);
                    }
                }
            }
            QMessageBox::information(nullptr, STR_MessageBox_Information,
                                     QObject::tr("SleepyHead will now start the import wizard so you can reinstall your %1 data.").arg(m->brand())
                                     ,QMessageBox::Ok, QMessageBox::Ok);
            mainwin->startImportDialog();
        }
        p_profile->Save();
        delete question;

    } else {
        delete question;
        QMessageBox::information(nullptr, STR_MessageBox_Information,
            QObject::tr("SleepyHead will now exit, then (attempt to) launch your computers file manager so you can manually back your profile up:")+"\n\n"+
            QDir::toNativeSeparators(Get(p_preferences[STR_GEN_DataFolder].toString()))+"\n\n"+
            QObject::tr("Use your file manager to make a copy of your profile directory, then afterwards, restart Sleepyhead and complete the upgrade process.")
                , QMessageBox::Ok, QMessageBox::Ok);

        showInGraphicalShell(Get(p_preferences[STR_GEN_DataFolder].toString()));
        QApplication::exit(-1);
    }


    return;

}
void Profile::UnloadMachineData()
{
    if (!m_machopened) {
        qCritical() << "Profile::UnloadMachineData() called with m_machopened==false";
        return;
    }

    for (auto & day : daylist) {
        delete day;
    }
    daylist.clear();

    for (auto & mach : m_machlist) {
        mach->sessionlist.clear();
        mach->day.clear();
    }
    removeLock();
}
void Profile::LoadMachineData(ProgressDialog *progress)
{
    addLock();

    for (auto & mach : m_machlist) {
        MachineLoader *loader = lookupLoader(mach);

        if (loader) {
            if (mach->version() < loader->Version()) {
                DataFormatError(mach);
            } else {
                try {
                    mach->Load(progress);
                } catch (OldDBVersion& e) {
                    Q_UNUSED(e)
                    DataFormatError(mach);
                }
            }
        } else {
            mach->Load(progress);
        }
    }
    progress->setMessage("Loading Channel Information");
    loadChannels();
}

void Profile::removeMachine(Machine * mach)
{
    if (m_machlist.removeAll(mach)) {

        QHash<QString, QHash<QString, Machine *> >::iterator mlit = MachineList.find(mach->loaderName());

        if (mlit != MachineList.end()) {
            QHash<QString, Machine *>::iterator mit = mlit.value().find(mach->serial());
            if (mit != mlit.value().end()) {
                mlit.value().erase(mit);
            }
        }
    }

}

Machine * Profile::lookupMachine(QString serial, QString loadername)
{
    auto mlit = MachineList.find(loadername);
    if (mlit != MachineList.end()) {
        auto mit = mlit.value().find(serial);
        if (mit != mlit.value().end()) {
            return mit.value();
        }
    }
    return nullptr;
}


Machine * Profile::CreateMachine(MachineInfo info, MachineID id)
{
    Machine *m = nullptr;

    auto mlit = MachineList.find(info.loadername);

    if (mlit != MachineList.end()) {
        auto mit = mlit.value().find(info.serial);
        if (mit != mlit.value().end()) {
            mit.value()->setInfo(info); // update info
            return mit.value();
        }
    }

    // Before we create, find any lost folder to get the old ID
    if ((id == 0) && ((info.type == MT_OXIMETER) || (info.type == MT_JOURNAL) || (info.type == MT_POSITION)|| (info.type == MT_SLEEPSTAGE))) {
        QString dataPath = Get("{" + STR_GEN_DataFolder + "}/");
        QDir dir(dataPath);
        QStringList namefilter(QString(info.loadername+"_*"));
        QStringList files = dir.entryList(namefilter, QDir::Dirs);
        if (files.size() > 0) {
            QString idstr = files[0].section("_",-1);
            bool ok;
            id = idstr.toInt(&ok, 16);
        }
    }

    switch (info.type) {
    case MT_CPAP:
        m = new CPAP(this, id);
        break;
    case MT_SLEEPSTAGE:
        m = new SleepStage(this, id);
        break;
    case MT_OXIMETER:
        m = new Oximeter(this, id);
        break;
    case MT_POSITION:
        m = new PositionSensor(this, id);
        break;
    case MT_JOURNAL:
        m = new Machine(this, id);
        m->setType(MT_JOURNAL);
        break;
    default:
        m = new Machine(this, id);

        break;
    }

    m->setInfo(info);

  //  qDebug() << "Reading" << info.loadername << "Machine Record" << (info.serial.isEmpty() ? m->hexid() : info.serial);

    MachineList[info.loadername][info.serial] = m;
    AddMachine(m);

    return m;
}



void Profile::AddMachine(Machine *m)
{
    if (!m) {
        qWarning() << "Empty Machine in Profile::AddMachine()";
        return;
    }
    m_machlist.append(m);
}

void Profile::DelMachine(Machine *m)
{
    if (!m) {
        qWarning() << "Empty Machine in Profile::AddMachine()";
        return;
    }

    removeMachine(m);
}

Day *Profile::addDay(QDate date)
{
    auto dit = daylist.find(date);
    if (dit == daylist.end()) {
        dit = daylist.insert(date, new Day());
    }
    Day * day = dit.value();
    day->setDate(date);

    if (is_first_day) {
        m_first = m_last = date;
        is_first_day = false;
    }

    if (m_first > date) {
        m_first = date;
    }

    if (m_last < date) {
        m_last = date;
    }
    return day;
}

// Get Day record if data available for date and machine type,
// and has enabled session data, else return nullptr
Day *Profile::GetGoodDay(QDate date, MachineType type)
{
    Day *day = GetDay(date, type);
    if (!day)
        return nullptr;

    // For a machine match, find at least one enabled Session.

    for (auto & sess : day->sessions) {
        if (((type == MT_UNKNOWN) || (sess->type() == type)) && sess->enabled()) {
            day->OpenSummary();
            return day;
        }
    }

    // No enabled Sessions were found.
    return nullptr;
}

Day *Profile::FindGoodDay(QDate date, MachineType type)
{
    Day *day = FindDay(date, type);
    if (!day)
        return nullptr;

    // For a machine match, find at least one enabled Session.
    for (auto & sess : day->sessions) {
        if (((type == MT_UNKNOWN) || (sess->type() == type)) && sess->enabled()) {
            return day;
        }
    }

    // No enabled Sessions were found.
    return nullptr;
}


Day *Profile::GetDay(QDate date, MachineType type)
{
    auto di = daylist.find(date);
    if (di == daylist.end()) return nullptr;

    Day * day = di.value();

    if (type == MT_UNKNOWN) {
        day->OpenSummary();
        return day; // just want the day record
    }

    if (day->machines.contains(type)) {
        day->OpenSummary();
        return day;
    }

    return nullptr;
}

Day *Profile::FindDay(QDate date, MachineType type)
{
    auto di = daylist.find(date);
    if (di == daylist.end()) return nullptr;

    Day * day = di.value();

    if (type == MT_UNKNOWN) {
        return day; // just want the day record
    }

    if (day->machines.contains(type)) {
        return day;
    }

    return nullptr;
}


int Profile::Import(QString path)
{
    int c = 0;
    qDebug() << "Importing " << path;
    path = path.replace("\\", "/");

    if (path.endsWith("/")) {
        path.chop(1);
    }

    QList<MachineLoader *>loaders = GetLoaders(MT_CPAP);

    for(auto & loader : loaders) {
        if (c += loader->Open(path)) {
            break;
        }
    }

    return c;
}

MachineLoader *GetLoader(QString name)
{
    QList<MachineLoader *> loaders = GetLoaders();

    for (auto & loader : loaders) {
        if (loader->loaderName() == name) {
            return loader;
        }
    }

    return nullptr;
}


// Returns a QVector containing all machine objects regisered of type t
QList<Machine *> Profile::GetMachines(MachineType t)
{
    QList<Machine *> vec;

    for (auto & mach : m_machlist) {
        if (!mach) {
            qWarning() << "Profile::GetMachines() m == nullptr";
            continue;
        }

        MachineType mt = mach->type();

        if ((t == MT_UNKNOWN) || (mt == t)) {
            vec.push_back(mach);
        }
    }

    return vec;
}

Machine *Profile::GetMachine(MachineType t)
{
    QList<Machine *>vec = GetMachines(t);

    if (vec.size() == 0) {
        return nullptr;
    }

    return vec[0];
}

//bool Profile::trashMachine(Machine * mach)
//{
//    QMap<QDate, QList<Day *> >::iterator it_end = daylist.end();
//    QMap<QDate, QList<Day *> >::iterator it;

//    QList<QDate> datelist;
//    QList<Day *> days;

//    for (it = daylist.begin(); it != it_end; ++it) {
//        for (int i = 0; i< it.value().size(); ++i) {
//            Day * day = it.value().at(i);
//            if (day->machine() == mach) {
//                days.push_back(day);
//                datelist.push_back(it.key());
//            }
//        }
//    }

//    for (int i=0; i < datelist.size(); ++i) {
//        Day * day = days.at(i);
//        it = daylist.find(datelist.at(i));
//        if (it != daylist.end()) {
//            it.value().removeAll(day);
//            if (it.value().size() == 0) {
//                daylist.erase(it);
//            }
//        }
//        mach->unlinkDay(days.at(i));
//    }

//}

bool Profile::unlinkDay(Day * day)
{
    // Find the key...
    for (auto it = daylist.begin(), it_end = daylist.end(); it != it_end; ++it) {
        if (it.value() == day) {
            daylist.erase(it);
            return true;
        }
    }
    return false;
}


//Profile *profile=nullptr;
QString SHA1(QString pass)
{
    return pass;
}

namespace Profiles {

QMap<QString, Profile *> profiles;

void Done()
{
    PREF.Save();

    profiles.clear();
    delete p_pref;
    delete AppSetting;
    DestroyLoaders();
}

Profile *Get(QString name)
{
    auto it = profiles.find(name);
    if (it != profiles.end()) {
        return it.value();
    }

    return nullptr;
}

Profile *Create(QString name)
{
    QString path = PREF.Get("{home}/Profiles/") + name;
    QDir dir(path);

    if (!dir.exists(path)) {
        dir.mkpath(path);
    }

    //path+="/"+name;
    p_profile = new Profile(path);
    profiles[name] = p_profile;
    p_profile->user->setUserName(name);
    //p_profile->Set("Realname",realname);
    //if (!password.isEmpty()) p_profile.user->setPassword(password);
    p_profile->Set(STR_GEN_DataFolder, QString("{home}/Profiles/{") + QString(STR_UI_UserName) + QString("}"));

    Machine *m = new Machine(p_profile, 0);
    m->setType(MT_JOURNAL);
    MachineInfo info(MT_JOURNAL, 0, STR_MACH_Journal, "SleepyHead", STR_MACH_Journal, QString(), m->hexid(), QString(), QDateTime::currentDateTime(), 0);

    m->setInfo(info);
    p_profile->AddMachine(m);

    p_profile->Save();

    return p_profile;
}

Profile *Get()
{
    // username lookup
    //getUserName()
    return profiles[getUserName()];;
}

void saveProfileList()
{
    QString filename = PREF.Get("{home}/profiles.xml");

    QDomDocument doc("profiles");

    QDomElement root = doc.createElement("profiles");
    doc.appendChild(root);

    root.appendChild(doc.createComment("This file is created during Profile Scan for cloud access convenience, it's not used by Desktop version of SleepyHead."));

    for (auto it = profiles.begin(); it != profiles.end(); ++it) {
        QDomElement elem = doc.createElement("profile");
        elem.setAttribute("name", it.key());
        // Not technically nessesary..
        elem.setAttribute("path", QString("{home}/Profiles/%1/Profile.xml").arg(it.key()));
        root.appendChild(elem);
    }

    QFile file(filename);
    file.open(QFile::WriteOnly);

    file.write(doc.toByteArray());

    file.close();
}

int CleanupProfile(Profile *prof)
{
    // Migrate old per Profile settings that should have been put in program main preferences.
    QStringList migrateList;
    migrateList << STR_IS_Multithreading << STR_US_ShowPerformance << STR_US_ShowDebug
                << STR_US_ScrollDampening << STR_AS_CalendarVisible << STR_IS_CacheSessions
                << STR_AS_LineCursorMode << STR_AS_RightSidebarVisible << STR_AS_DailyPanelWidth
                << STR_US_ShowPerformance << STR_AS_GraphHeight << STR_AS_GraphSnapshots
                << STR_AS_AntiAliasing << STR_AS_LineThickness << STR_AS_UsePixmapCaching
                << STR_AS_SquareWave << STR_AS_RightPanelWidth << STR_US_TooltipTimeout
                << STR_AS_Animations << STR_AS_AllowYAxisScaling << STR_AS_GraphTooltips
                << STR_CS_UserEventPieChart << STR_AS_OverlayType << STR_AS_OverviewLinechartMode;

    int cnt = 0;
    for (auto & prf :migrateList) {
        if (prof->contains(prf)) {
            qDebug() << "Migrating profile preference" << prf;
            PREF[prf] = (*prof)[prf];
            prof->Erase(prf);
            cnt++;
        }
    }
    if (cnt > 0) {
        qDebug() << "Migrated" << cnt << "preferences for profile" << (*prof)[STR_UI_UserName];
        prof->Save();
    }
    return cnt;
}

/**
 * @brief Scan Profile directory loading user profiles
 */
void Scan()
{
    QString path = PREF.Get("{home}/Profiles");
    QDir dir(path);

    if (!dir.exists(path)) {
        return;
    }

    if (!dir.isReadable()) {
        qWarning() << "Can't open " << path;
        return;
    }

    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    //dir.setSorting(QDir::Name);

    QFileInfoList list = dir.entryInfoList();

    int cleanup = 0;

    // Iterate through subdirectories and load profiles..
    for (auto & fi : list) {
        QString npath = fi.canonicalFilePath();
        Profile *prof = new Profile(npath);
        //prof->Open();

        profiles[fi.fileName()] = prof;

        // Migrate any old settings
        cleanup += CleanupProfile(prof);
    }

    if (cleanup > 0) {
        qDebug() << "Saving preferences after migration";
        PREF.Save();
    }
    // Update profiles.xml for mobile version
    saveProfileList();
}


} // namespace Profiles


// Returns a list of all days records matching machine type between start and end date
QList<Day *> Profile::getDays(MachineType mt, QDate start, QDate end)
{
    QList<Day *> list;

    if (!start.isValid()) {
        return list;
    }

    if (!end.isValid()) {
        return list;
    }

    QDate date = start;

    if (date.isNull()) {
        return list;
    }

    QMap<QDate, Day *>::iterator it;

    do {
        it = daylist.find(date);
        if (it != daylist.end()) {
            Day *day = it.value();
            if (mt != MT_UNKNOWN) {
                if (day->hasEnabledSessions(mt)) {
                    list.push_back(day);
                }
            } else {
                if (day->hasEnabledSessions()) {
                    list.push_back(day);
                }
            }
        }
        date = date.addDays(1);
    } while (date <= end);

    return list;
}

int Profile::countDays(MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        return 0;
    }

    if (!end.isValid()) {
        return 0;
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    int days = 0;

    do {
        Day *day = FindGoodDay(date, mt);

        if (day) {
            days++;
        }

        date = date.addDays(1);
    } while (date <= end);

    return days;

}

int Profile::countCompliantDays(MachineType mt, QDate start, QDate end)
{
    EventDataType compliance = cpap->complianceHours();

    if (!start.isValid()) {
        return 0;
    }

    if (!end.isValid()) {
        return 0;
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    int days = 0;

    do {
        Day *day = FindGoodDay(date, mt);

        if (day) {
            if (day->hours(mt) > compliance) { days++; }
        }

        date = date.addDays(1);
    } while (date <= end);

    return days;
}


EventDataType Profile::calcCount(ChannelID code, MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    double val = 0;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            val += day->count(code);
        }

        date = date.addDays(1);
    } while (date <= end);

    return val;
}

double Profile::calcSum(ChannelID code, MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    double val = 0;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            val += day->sum(code);
        }

        date = date.addDays(1);
    } while (date <= end);

    return val;
}

EventDataType Profile::calcHours(MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    double val = 0;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            val += day->hours();
        }

        date = date.addDays(1);
    } while (date <= end);

    return val;
}

EventDataType Profile::calcAboveThreshold(ChannelID code, EventDataType threshold, MachineType mt,
                                 QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    EventDataType val = 0;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            val += day->timeAboveThreshold(code, threshold);
        }

        date = date.addDays(1);
    } while (date <= end);

    return val;
}

EventDataType Profile::calcBelowThreshold(ChannelID code, EventDataType threshold, MachineType mt,
                                 QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    EventDataType val = 0;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            val += day->timeBelowThreshold(code, threshold);
        }

        date = date.addDays(1);
    } while (date <= end);

    return val;
}

Day * Profile::findSessionDay(Session * session)
{
    for (auto it=p_profile->daylist.begin(),it_end = p_profile->daylist.end(); it != it_end; ++it) {
        Day *day = it.value();
        for (auto & sess : day->sessions) {
            if (sess == session) {
                return day;
            }
        }
    }
    return nullptr;
}


EventDataType Profile::calcAvg(ChannelID code, MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    double val = 0;
    int cnt = 0;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            if (!day->summaryOnly() || day->hasData(code, ST_AVG)) {
                val += day->sum(code);
                cnt += day->count(code);
            }
        }

        date = date.addDays(1);
    } while (date <= end);

    if (!cnt) {
        return 0;
    }

    return val / float(cnt);
}

EventDataType Profile::calcWavg(ChannelID code, MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    double val = 0, tmp, tmph, hours = 0;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            if (!day->summaryOnly() || day->hasData(code, ST_WAVG)) {
                tmph = day->hours();
                tmp = day->wavg(code);
                val += tmp * tmph;
                hours += tmph;
            }
        }

        date = date.addDays(1);
    } while (date <= end);

    if (!hours) {
        return 0;
    }

    val = val / hours;
    return val;
}

EventDataType Profile::calcMin(ChannelID code, MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    bool first = true;

    double min = 0, tmp;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            if (!day->summaryOnly() || day->hasData(code, ST_MIN)) {
                tmp = day->Min(code);

                if (first || (min > tmp)) {
                    min = tmp;
                    first = false;
                }
            }

        }

        date = date.addDays(1);
    } while (date <= end);

    if (first) {
        min = 0;
    }

    return min;
}
EventDataType Profile::calcMax(ChannelID code, MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    bool first = true;
    double max = 0, tmp;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            if (!day->summaryOnly() || day->hasData(code, ST_MAX)) {
                tmp = day->Max(code);

                if (first || (max < tmp)) {
                    max = tmp;
                    first = false;
                }
            }
        }

        date = date.addDays(1);
    } while (date <= end);

    if (first) {
        max = 0;
    }

    return max;
}
EventDataType Profile::calcSettingsMin(ChannelID code, MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    bool first = true;
    double min = 0, tmp;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            tmp = day->settings_min(code);

            if (first || (min > tmp)) {
                min = tmp;
                first = false;
            }
        }

        date = date.addDays(1);
    } while (date <= end);

    if (first) {
        min = 0;
    }

    return min;
}

EventDataType Profile::calcSettingsMax(ChannelID code, MachineType mt, QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    bool first = true;
    double max = 0, tmp;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            tmp = day->settings_max(code);

            if (first || (max < tmp)) {
                max = tmp;
                first = false;
            }
        }

        date = date.addDays(1);
    } while (date <= end);

    if (first) {
        max = 0;
    }

    return max;
}

struct CountSummary {
    CountSummary(EventStoreType v) : val(v), count(0), time(0) {}
    EventStoreType val;
    EventStoreType count;
    quint32 time;
};

EventDataType Profile::calcPercentile(ChannelID code, EventDataType percent, MachineType mt,
                                      QDate start, QDate end)
{
    if (!start.isValid()) {
        start = LastGoodDay(mt);
    }

    if (!end.isValid()) {
        end = LastGoodDay(mt);
    }

    QDate date = start;

    if (date.isNull()) {
        return 0;
    }

    QMap<EventDataType, qint64> wmap;
    QMap<EventDataType, qint64>::iterator wmi;

    QHash<ChannelID, QHash<EventStoreType, EventStoreType> >::iterator vsi;
    QHash<ChannelID, QHash<EventStoreType, quint32> >::iterator tsi;
    EventDataType gain;
    //bool setgain=false;
    EventDataType value;
    int weight;

    qint64 SN = 0;
    bool timeweight;
    bool summaryOnly = false;

    do {
        Day *day = GetGoodDay(date, mt);

        if (day) {
            if (day->summaryOnly()) {
                summaryOnly = true;
                break;
            }

            // why was this nested like this???
            //for (int i = 0; i < day->size(); i++) {
                for (auto & sess : day->sessions) {
                    if (!sess->enabled()) {
                        continue;
                    }

                    gain = sess->m_gain[code];

                    if (!gain) { gain = 1; }

                    vsi = sess->m_valuesummary.find(code);

                    if (vsi == sess->m_valuesummary.end()) { continue; }

                    tsi = sess->m_timesummary.find(code);
                    timeweight = (tsi != sess->m_timesummary.end());

                    QHash<EventStoreType, EventStoreType> &vsum = vsi.value();
                    QHash<EventStoreType, quint32> &tsum = tsi.value();

                    if (timeweight) {
                        for (auto k=tsum.begin(), tsumend=tsum.end(); k != tsumend; k++) {
                            weight = k.value();
                            value = EventDataType(k.key()) * gain;

                            SN += weight;
                            wmi = wmap.find(value);

                            if (wmi == wmap.end()) {
                                wmap[value] = weight;
                            } else {
                                wmi.value() += weight;
                            }
                        }
                    } else {
                        for (auto k=vsum.begin(), vsumend=vsum.end(); k!=vsumend; k++) {
                            weight = k.value();
                            value = EventDataType(k.key()) * gain;

                            SN += weight;
                            wmi = wmap.find(value);

                            if (wmi == wmap.end()) {
                                wmap[value] = weight;
                            } else {
                                wmi.value() += weight;
                            }
                        }
                    }
                }
           // }
        }

        date = date.addDays(1);
    } while (date <= end);


    if (summaryOnly) {
        // abort percentile calculation, there is not enough data
        return 0;
    }

    QVector<ValueCount> valcnt;

    // Build sorted list of value/counts
    for (wmi = wmap.begin(); wmi != wmap.end(); wmi++) {
        ValueCount vc;
        vc.value = wmi.key();
        vc.count = wmi.value();
        vc.p = 0;
        valcnt.push_back(vc);
    }

    // sort by weight, then value
    std::sort(valcnt.begin(), valcnt.end());

    //double SN=100.0/double(N); // 100% / overall sum
    double p = 100.0 * percent;

    double nth = double(SN) * percent; // index of the position in the unweighted set would be
    double nthi = floor(nth);

    qint64 sum1 = 0, sum2 = 0;
    qint64 w1, w2 = 0;
    double v1 = 0, v2 = 0;

    int N = valcnt.size();
    int k = 0;

    for (k = 0; k < N; k++) {
        v1 = valcnt[k].value;
        w1 = valcnt[k].count;
        sum1 += w1;

        if (sum1 > nthi) {
            return v1;
        }

        if (sum1 == nthi) {
            break; // boundary condition
        }
    }

    if (k >= N) {
        return v1;
    }

    v2 = valcnt[k + 1].value;
    w2 = valcnt[k + 1].count;
    sum2 = sum1 + w2;
    // value lies between v1 and v2

    double px = 100.0 / double(SN); // Percentile represented by one full value

    // calculate percentile ranks
    double p1 = px * (double(sum1) - (double(w1) / 2.0));
    double p2 = px * (double(sum2) - (double(w2) / 2.0));

    // calculate linear interpolation
    double v = v1 + ((p - p1) / (p2 - p1)) * (v2 - v1);

    //  p1.....p.............p2
    //  37     55            70

    return v;
}

// Lookup first day record of the specified machine type, or return the first day overall if MT_UNKNOWN
QDate Profile::FirstDay(MachineType mt)
{
    if ((mt == MT_UNKNOWN) || (!m_last.isValid()) || (!m_first.isValid())) {
        return m_first;
    }

    QDate d = m_first;

    do {
        if (FindDay(d, mt) != nullptr) {
            return d;
        }

        d = d.addDays(1);
    } while (d <= m_last);

    return m_last;
}

// Lookup last day record of the specified machine type, or return the first day overall if MT_UNKNOWN
QDate Profile::LastDay(MachineType mt)
{
    if ((mt == MT_UNKNOWN) || (!m_last.isValid()) || (!m_first.isValid())) {
        return m_last;
    }

    QDate d = m_last;

    do {
        if (FindDay(d, mt) != nullptr) {
            return d;
        }

        d = d.addDays(-1);
    } while (d >= m_first);

    return m_first;
}

QDate Profile::FirstGoodDay(MachineType mt)
{
    if (mt == MT_UNKNOWN) {
        return FirstDay();
    }

    QDate d = FirstDay(mt);
    QDate l = LastDay(mt);

    // No data will return invalid date records
    if (!d.isValid() || !l.isValid()) {
        return QDate();
    }

    do {
        if (FindGoodDay(d, mt) != nullptr) {
            return d;
        }

        d = d.addDays(1);
    } while (d <= l);

    return l; //m_last;
}
QDate Profile::LastGoodDay(MachineType mt)
{
    if (mt == MT_UNKNOWN) {
        return FirstDay();
    }

    QDate d = LastDay(mt);
    QDate f = FirstDay(mt);

    if (!(d.isValid() && f.isValid())) {
        return QDate();
    }

    do {
        if (FindGoodDay(d, mt) != nullptr) {
            return d;
        }

        d = d.addDays(-1);
    } while (d >= f);

    return f;
}

bool Profile::channelAvailable(ChannelID code)
{
    for (auto & mach : m_machlist) {
        if (mach->hasChannel(code))
            return true;
    }
    return false;
}

bool Profile::hasChannel(ChannelID code)
{
    QDate d = LastDay();
    QDate f = FirstDay();

    if (!(d.isValid() && f.isValid())) {
        return false;
    }

    QMap<QDate, Day *>::iterator dit;

    bool found = false;

    do {
        dit = daylist.find(d);

        if (dit != daylist.end()) {
            Day *day = dit.value();


            if (day->channelHasData(code)) {
                found = true;
                break;
            }
        }

        d = d.addDays(-1);
    } while (d >= f);

    return found;
}

const quint16 chandata_version = 1;
void Profile::saveChannels()
{
    // First save the XML version for Mobile versions
    schema::channel.Save(Get("{DataFolder}/") + "channels.xml");

    QString filename = Get("{DataFolder}/") + "channels.dat";
    QFile f(filename);
    qDebug() << "Saving Channel States";
    f.open(QFile::WriteOnly);
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_4_6);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)magic;
    out << (quint16)chandata_version;

    QSettings settings;
    (*p_profile)[STR_PREF_Language] = settings.value(LangSetting, "").toString();

    quint16 size = schema::channel.channels.size();
    out << size;

    for (auto it = schema::channel.channels.begin(),it_end = schema::channel.channels.end(); it != it_end; ++it) {
        schema::Channel * chan = it.value();
        out << it.key();
        out << chan->code();
        out << chan->enabled();
        out << chan->defaultColor();
        out << chan->fullname();
        out << chan->label();
        out << chan->description();
        out << chan->lowerThreshold();
        out << chan->lowerThresholdColor();
        out << chan->upperThreshold();
        out << chan->upperThresholdColor();
        out << chan->showInOverview();
    }

    f.close();

}

void Profile::loadChannels()
{
    bool changing_language = false;

    QString filename = Get("{DataFolder}/") + "channels.dat";
    QFile f(filename);
    if (!f.open(QFile::ReadOnly)) {
        return;
    }
    qDebug() << "Loading channel.dat States";

    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_4_6);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 mag;
    in >> mag;

    if (magic != mag)  {
        qDebug() << "LoadChannels: Faulty data";
        return;
    }
    quint16 version;
    in >> version;

    QSettings settings;
    QString language = Get(STR_PREF_Language);
    if (settings.value(LangSetting, "").toString() != language) {
        qDebug() << "Language change detected, resetting default channel names";
        changing_language = true;
    }

    quint16 size;
    in >> size;

    QString name;
    ChannelID code;
    bool enabled;
    QColor color;
    EventDataType lowerThreshold;
    QColor lowerThresholdColor;
    EventDataType upperThreshold;
    QColor upperThresholdColor;

    QString fullname;
    QString label;
    QString description;
    bool showOverview = false;

    for (int i=0; i < size; i++) {
        in >> code;
        schema::Channel * chan = &schema::channel[code];
        in >> name;
        if (chan->code() != name) {
            qDebug() << "Looking up channel" << name << "by name, as it's ChannedID must have changed";
            chan = &schema::channel[name];
        }
        in >> enabled;
        in >> color;
        in >> fullname;
        in >> label;
        in >> description;
        in >> lowerThreshold;
        in >> lowerThresholdColor;
        in >> upperThreshold;
        in >> upperThresholdColor;
        if (version >= 1) {
            in >> showOverview;
        }

        if (chan->isNull()) {
            qDebug() << "loadChannels has no idea about channel" << name;
            if (in.atEnd()) return;
            continue;
        }
        chan->setEnabled(enabled);
        chan->setDefaultColor(color);

        // Don't import channel descriptions if event renaming is turned off. (helps pick up new translations)
        if (changing_language) {
            // Nothing
        } else {
            chan->setFullname(fullname);
            chan->setLabel(label);
            chan->setDescription(description);
        }

        chan->setLowerThreshold(lowerThreshold);
        chan->setLowerThresholdColor(lowerThresholdColor);
        chan->setUpperThreshold(upperThreshold);
        chan->setUpperThresholdColor(upperThresholdColor);

        chan->setShowInOverview(showOverview);
        if (in.atEnd()) return;
    }

    f.close();
}
