/* SleepLib Machine Class Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QString>
#include <QObject>
#include <QThreadPool>
#include <QFile>
#include <QDataStream>
#include <QDomDocument>
#include <QDomElement>


#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include "mainwindow.h"

#include "progressdialog.h"

#include <time.h>

#include "machine.h"
#include "profiles.h"
#include <algorithm>
#include "SleepLib/schema.h"
#include "SleepLib/day.h"
#include "mainwindow.h"

extern MainWindow * mainwin;

//////////////////////////////////////////////////////////////////////////////////////////
// Machine Base-Class implmementation
//////////////////////////////////////////////////////////////////////////////////////////
Machine::Machine(Profile *_profile, MachineID id) : profile(_profile)
{
    day.clear();
    highest_sessionid = 0;
    m_unsupported = false;

    if (!id) {
        srand(time(nullptr));
        MachineID temp;

        bool found;
        // Keep trying until we get a unique machineID for this profile
        do {
            temp = rand();

            found = false;
            for (int i=0;i<profile->m_machlist.size(); ++i) {
                if (profile->m_machlist.at(i)->id() == temp) found = true;
            }
        } while (found);

        m_id = temp;

    } else { m_id = id; }
    m_loader = nullptr;

   // qDebug() << "Create Machine: " << hex << m_id; //%lx",m_id);
    m_type = MT_UNKNOWN;
    firstsession = true;
}
Machine::~Machine()
{
    saveSessionInfo();
    qDebug() << "Destroy Machine" << info.loadername << hex << m_id;
}
Session *Machine::SessionExists(SessionID session)
{
    if (sessionlist.find(session) != sessionlist.end()) {
        return sessionlist[session];
    } else {
        return nullptr;
    }
}

const quint16 sessinfo_version = 2;

bool Machine::saveSessionInfo()
{
    if (info.type == MT_JOURNAL) return false;
    if (sessionlist.size() == 0) return false;

    qDebug() << "Saving" << info.brand << "session info" << info.loadername;
    QString filename = getDataPath() + "Sessions.info";
    QFile file(filename);
    if (!file.open(QFile::WriteOnly)) {
        qDebug() << "Couldn't open" << filename << "for writing";
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setVersion(QDataStream::Qt_5_0);

    out << magic;
    out << filetype_sessenabled;
    out << sessinfo_version;

    QHash<SessionID, Session *>::iterator s;

    out << (int)sessionlist.size();
    for (s = sessionlist.begin(); s != sessionlist.end(); ++s) {
        Session * sess = s.value();
        out << (quint32) sess->session();
        out << (bool)(sess->enabled());

        //out << sess->m_availableChannels;
    }
    qDebug() << "Done Saving" << info.brand << "session info";

    return true;
}

bool Machine::loadSessionInfo()
{
    // SessionInfo basically just contains a list of all sessions and their enabled status,
    // so the enabling/reenabling doesn't require a summary rewrite every time.

    if (info.type == MT_JOURNAL)
        return true;

    QHash<SessionID, Session *>::iterator s;
    QFile file(getDataPath() + "Sessions.info");

    if (!file.open(QFile::ReadOnly)) {
        // No session.info file present, so let's create one...

        // But first check for legacy SESSION_ENABLED field in session settings
        for (s = sessionlist.begin(); s!= sessionlist.end(); ++s) {
            Session * sess = s.value();
            QHash<ChannelID, QVariant>::iterator it = sess->settings.find(SESSION_ENABLED);

            bool b = true;
            if (it != sess->settings.end()) {
                b = it.value().toBool();
            }
            sess->setEnabled(b);        // Extract from session settings and save..
        }

        // Now write the file
        saveSessionInfo();
        return true;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_5_0);

    quint32 mag32;
    in >> mag32;

    quint16 ft16, version;
    in >> ft16;
    in >> version;

    // Legacy crud
    if (version == 1) {
        // was available channels
        QHash<ChannelID, bool> crap;
        in >> crap;
    }


    // Read in size record, followed by size * [SessionID, bool] pairs containing the enable status.
    int size;
    in >> size;

    quint32 sid;
    bool b;

    for (int i=0; i< size; ++i) {
        in >> sid;
        in >> b;

        s = sessionlist.find(sid);

        if (s != sessionlist.end()) {
            Session * sess = s.value();

            sess->setEnabled(b);
        }
    }
    return true;
}

// Find date this session belongs in
QDate Machine::pickDate(qint64 first)
{
    QTime split_time = profile->session->daySplitTime();
    int combine_sessions = profile->session->combineCloseSessions();

    QDateTime d2 = QDateTime::fromTime_t(first / 1000);

    QDate date = d2.date();
    QTime time = d2.time();

    int closest_session = 0;

    if (time < split_time) {
        date = date.addDays(-1);
    } else if (combine_sessions > 0) {
        QMap<QDate, Day *>::iterator dit = day.find(date.addDays(-1)); // Check Day Before

        if (dit != day.end()) {
            QDateTime lt = QDateTime::fromTime_t(dit.value()->last() / 1000L);
            closest_session = lt.secsTo(d2) / 60;

            if (closest_session < combine_sessions) {
                date = date.addDays(-1);
            }
        }
    }

    return date;
}

bool Machine::AddSession(Session *s)
{
    if (s == nullptr) {
        qCritical() << "AddSession() called with a null object";
        return false;
    }
    if (profile == nullptr) {
        qCritical() << "AddSession() called without a valid profile";
        return false;
    }

    if (profile->session->ignoreOlderSessions()) {
        qint64 ignorebefore = profile->session->ignoreOlderSessionsDate().toMSecsSinceEpoch();
        if (s->last() < ignorebefore) {
            skipped_sessions++;
            return false;
        }
    }


    updateChannels(s);

    if (s->session() > highest_sessionid) {
        highest_sessionid = s->session();
    }

    QTime split_time;
    int combine_sessions;
    bool locksessions = profile->session->lockSummarySessions();

    if (locksessions) {
        split_time = s->summaryOnly() ? QTime(12,0,0) : profile->session->daySplitTime();
        combine_sessions = s->summaryOnly() ? 0 : profile->session->combineCloseSessions();
    } else {
        split_time = profile->session->daySplitTime();
        combine_sessions = profile->session->combineCloseSessions();
    }

    int ignore_sessions = profile->session->ignoreShortSessions();

    int session_length = s->last() - s->first();
    session_length /= 60000;

    sessionlist[s->session()] = s; // To make sure it get's saved later even if it's not wanted.

    //int drift=profile->cpap->clockDrift();

    QDateTime d2 = QDateTime::fromTime_t(s->first() / 1000);

    QDate date = d2.date();
    QTime time = d2.time();

    QMap<QDate, Day *>::iterator dit, nextday;

    bool combine_next_day = false;
    int closest_session = 0;


    // Multithreaded import screws this up. :(

    if (time < split_time) {
        date = date.addDays(-1);
    } else if (combine_sessions > 0) {
        dit = day.find(date.addDays(-1)); // Check Day Before

        if (dit != day.end()) {
            QDateTime lt = QDateTime::fromTime_t(dit.value()->last() / 1000);
            closest_session = lt.secsTo(d2) / 60;

            if (closest_session < combine_sessions) {
                date = date.addDays(-1);
            } else {
                if ((split_time < time) && (split_time.secsTo(time) < 2)) {
                    if (s->machine()->loaderName() == STR_MACH_ResMed) {
                        date = date.addDays(-1);
                    }
                }
            }
        } else {
            nextday = day.find(date.addDays(1)); // Check Day Afterwards

            if (nextday != day.end()) {
                QDateTime lt = QDateTime::fromTime_t(nextday.value()->first() / 1000);
                closest_session = d2.secsTo(lt) / 60;

                if (closest_session < combine_sessions) {
                    // add todays here. pull all tomorrows records to this date.
                    combine_next_day = true;
                }
            }
        }
    }

    if (session_length < ignore_sessions) {
        // keep the session to save importing it again, but don't add it to the day record this time
        return true;
    }

    if (!firstsession) {
        if (firstday > date) { firstday = date; }

        if (lastday < date) { lastday = date; }
    } else {
        firstday = lastday = date;
        firstsession = false;
    }


    Day *dd = nullptr;
    dit = day.find(date);

    if (dit == day.end()) {
        dit = day.insert(date, profile->addDay(date));
    }
    dd = dit.value();

    dd->addSession(s);

    if (combine_next_day) {
        for (QList<Session *>::iterator i = nextday.value()->begin(); i != nextday.value()->end(); i++) {
            // i may need to do something here
            if (locksessions && (*i)->summaryOnly()) continue; // can't move summary only sessions..
            unlinkSession(*i);
            // Add it back

            sessionlist[(*i)->session()] = *i;

            dd->addSession(*i);
        }

//        QMap<QDate, QList<Day *> >::iterator nd = profile->daylist.find(date.addDays(1));
//        if (nd != profile->daylist.end()) {
//            profile->unlinkDay(nd.key(), nd.value());
//        }

//        QList<Day *>::iterator iend = nd.value().end();
//        for (QList<Day *>::iterator i = nd.value()->begin(); i != iend; ++i) {
//            if (*i == nextday.value()) {
//                nd.value().erase(i);
//            }
//        }

//        day.erase(nextday);
    }

    return true;
}

bool Machine::unlinkDay(Day * d)
{
    return day.remove(day.key(d)) > 0;
}

QString Machine::getPixmapPath()
{
    if (!loader()) return "";
    return loader()->getPixmapPath(info.series);
}

QPixmap & Machine::getPixmap()
{
    static QPixmap pm;
    if (!loader()) return pm;
    return loader()->getPixmap(info.series);
}

bool Machine::unlinkSession(Session * sess)
{
    MachineType mt = sess->type();

    // Remove the object from the machine object's session list
    bool b=sessionlist.remove(sess->session());

    QList<QDate> dates;

    QList<Day *> days;
    QMap<QDate, Day *>::iterator it;

    Day * d;

    // Doing this in case of accidental double linkages
    for (it = day.begin(); it != day.end(); ++it) {
        d = it.value();
        if (it.value()->sessions.contains(sess)) {
            days.push_back(d);
            dates.push_back(it.key());
        }
    }

    for (int i=0; i < days.size(); ++i) {
        d = days.at(i);
        if (d->sessions.removeAll(sess)) {
            b=true;
            if (!d->searchMachine(mt)) {
                d->machines.remove(mt);
                day.remove(dates[i]);
            }

            if (d->size() == 0) {
                profile->unlinkDay(d);
            }
        }
    }

    return b;
}

// This functions purpose is murder and mayhem... It deletes all of a machines data.
bool Machine::Purge(int secret)
{
    // Boring api key to stop this function getting called by accident :)
    if (secret != 3478216) { return false; }

    QString path = getDataPath();

    QDir dir(path);

    if (!dir.exists()) { // It doesn't exist anyway.
        return true;
    }

    if (!dir.isReadable()) {
        return false;
    }

    qDebug() << "Purging" << info.loadername << info.serial << dir.absoluteFilePath(path);

    // Remove any imported file list
    QFile impfile(getDataPath()+"/imported_files.csv");
    impfile.remove();

    QFile rxcache(profile->Get("{" + STR_GEN_DataFolder + "}/RXChanges.cache" ));
    rxcache.remove();

    QFile sumfile(getDataPath()+"/Summaries.xml.gz");
    sumfile.remove();

    QFile sessinfofile(getDataPath()+"/Sessions.info");
    sessinfofile.remove();

    // Create a copy of the list so the hash can be manipulated
    QList<Session *> sessions = sessionlist.values();

    // Clean up any loaded sessions from memory first..
    //bool success = true;
    for (int i=0; i < sessions.size(); ++i) {
        Session * sess = sessions[i];
        if (!sess->Destroy()) {
            qDebug() << "Could not destroy "+ info.loadername +" ("+info.serial+") session" << sess->session();
      //      success = false;
        } else {
//            sessionlist.remove(sess->session());
        }

        delete sess;
    }

    // Remove EVERYTHING under Events folder..
    QString eventspath = getEventsPath();
    QDir evdir(eventspath);
    evdir.removeRecursively();

    QString summariespath = getSummariesPath();
    QDir sumdir(summariespath);
    sumdir.removeRecursively();


    // Clean up any straggling files (like from short sessions not being loaded...)
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);

    const QFileInfoList & list = dir.entryInfoList();
    int could_not_kill = 0;

    for (const auto & fi : list) {
        QString fullpath = fi.canonicalFilePath();

        QString ext_s = fullpath.section('.', -1);
        bool ok;
        ext_s.toInt(&ok, 10);

        if (ok) {
            qDebug() << "Deleting " << fullpath;
            if (!dir.remove(fullpath)) {
                qDebug() << "Could not purge file" << fullpath;
                //success=false;
                could_not_kill++;
            }
        } else {
            qDebug() << "Didn't bother deleting cruft file" << fullpath;
            // cruft file..
        }
    }

    if (could_not_kill > 0) {
        qWarning() << "Could not purge path" << could_not_kill << "files in " << path;
        return false;
    }

    return true;
}

void Machine::setLoaderName(QString value)
{
    info.loadername = value;
    m_loader = GetLoader(value);
}

void Machine::setInfo(MachineInfo inf)
{
    info = inf;
    m_loader = GetLoader(inf.loadername);
}


//const quint32 channel_version=1;

const QString Machine::getDataPath()
{    
    m_dataPath = PREF.Get("{home}/Profiles/")+profile->user->userName()+"/"+info.loadername + "_" + (info.serial.isEmpty() ? hexid() : info.serial) + "/";

    //if (m_dataPath.isEmpty()) {
 //       m_dataPath = profile->Get("{" + STR_GEN_DataFolder + "}/" + info.loadername + "_" + (info.serial.isEmpty() ? hexid() : info.serial)) + "/";
   // }
    return m_dataPath;
}
const QString Machine::getSummariesPath()
{
    return getDataPath() + "Summaries/";
}
const QString Machine::getEventsPath()
{
    return getDataPath() + "Events/";
}
const QString Machine::getBackupPath()
{
    return getDataPath() + "Backup/";
}

// dirSize lazily pinched from https://stackoverflow.com/questions/47854288/can-not-get-directory-size-in-qt-c, thank's "Mike"
qint64 dirSize(QString dirPath) {
    qint64 size = 0;

    QDir dir(dirPath);
    QDir::Filters fileFilters = QDir::Files | QDir::System | QDir::Hidden;
    for(QString filePath : dir.entryList(fileFilters)) {
        QFileInfo fi(dir, filePath);
        size += fi.size();
    }

    QDir::Filters dirFilters = QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden;
    for(QString childDirPath : dir.entryList(dirFilters))
        size += dirSize(dirPath + QDir::separator() + childDirPath);

    return size;
}

qint64 Machine::diskSpaceSummaries()
{
    return dirSize(getSummariesPath());
}
qint64 Machine::diskSpaceEvents()
{
    return dirSize(getEventsPath());
}
qint64 Machine::diskSpaceBackups()
{
    return dirSize(getBackupPath());
}

bool Machine::Load(ProgressDialog *progress)
{
    QString path = getDataPath();

    QDir dir(path);
    qDebug() << "Loading" << info.loadername.toLocal8Bit().data() << "record:" << path.toLocal8Bit().data();

    if (!dir.exists() || !dir.isReadable()) {
        return false;
    }

    QPixmap image = getPixmap().scaled(64,64);
    progress->setPixmap(image);
    progress->setMessage(QObject::tr("Loading %1 data for %2...").arg(info.brand).arg(profile->user->userName()));

    if (loader()) {
        mainwin->connect(loader(), SIGNAL(updateMessage(QString)), progress, SLOT(setMessage(QString)));
        mainwin->connect(loader(), SIGNAL(setProgressMax(int)),   progress, SLOT(setProgressMax(int)));
        mainwin->connect(loader(), SIGNAL(setProgressValue(int)), progress, SLOT(setProgressValue(int)));
    }

    if (!LoadSummary(progress)) {
        // No XML index file, so assume upgrading, or it simply just got screwed up or deleted...
        progress->setMessage(QObject::tr("Scanning Files"));
        progress->setProgressValue(0);
        QApplication::processEvents();



        QTime time;
        time.start();
        dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

        ///////////////////////////////////////////////////////////////////////
        // First move any old files to correct locations
        ///////////////////////////////////////////////////////////////////////
        QString summarypath = getSummariesPath();
        QString eventpath = getEventsPath();

        if (!dir.exists(summarypath)) dir.mkpath(summarypath);

        QStringList filters;
        filters << "*.000";
        dir.setNameFilters(filters);
        QStringList filelist = dir.entryList();
        int size = filelist.size();


        // Legacy crap.. Summary and Event stuff used to be in one big pile in the machine folder root
        for (auto & filename : filelist) {
            QFile::rename(path+filename, summarypath+filename);
        }

        // Copy old Event files to folder
        filters.clear();
        filters << "*.001";
        dir.setNameFilters(filters);
        filelist = dir.entryList();
        size = filelist.size();
        progress->setMessage(QObject::tr("Migrating Summary File Location"));
        progress->setProgressMax(size);
        QApplication::processEvents();
        if (size > 0) {
            if (!dir.exists(eventpath)) dir.mkpath(eventpath);
            for (int i=0; i< size; i++) {
                if ((i % 20) == 0) { // This is slow.. :-/
                    progress->setProgressValue(i);

                    QApplication::processEvents();
                }

                QString filename = filelist.at(i);
                QFile::rename(path+filename, eventpath+filename);
            }
        }

        ///////////////////////////////////////////////////////////////////////
        // Now read summary files from correct location and load them
        ///////////////////////////////////////////////////////////////////////
        dir.setPath(summarypath);
        filters.clear();
        filters << "*.000";
        dir.setNameFilters(filters);
        filelist = dir.entryList();
        size = filelist.size();

        progress->setMessage("Reading summary files");
        progress->setProgressValue(0);
        QApplication::processEvents();

        QString sesstr;
        SessionID sessid;
        bool ok;

        for (int i=0; i < size; i++) {

            if ((i % 20) == 0) { // This is slow.. :-/
                progress->setProgressValue(i);
                QApplication::processEvents();
            }

            QString filename = filelist.at(i);
            sesstr = filename.section(".", 0, -2);
            sessid = sesstr.toLong(&ok, 16);

            if (!ok) { continue; }

            Session *sess = new Session(this, sessid);

            // Forced to load it, because know nothing about this session..
            if (sess->LoadSummary()) {
                AddSession(sess);
            } else {
                qWarning() << "Error loading summary file" << filename;
                delete sess;
            }
        }

        SaveSummaryCache();
        qDebug() << "Loaded" << info.model.toLocal8Bit().data() << "data in" << time.elapsed() << "ms";
        progress->setProgressValue(size);
    }
    progress->setMessage("Loading Session Info");
    QApplication::processEvents();

    loadSessionInfo();

    if (loader()) {
        mainwin->disconnect(loader(), SIGNAL(updateMessage(QString)), progress, SLOT(setMessage(QString)));
        mainwin->disconnect(loader(), SIGNAL(setProgressMax(int)),   progress, SLOT(setProgressMax(int)));
        mainwin->disconnect(loader(), SIGNAL(setProgressValue(int)), progress, SLOT(setProgressValue(int)));
    }

    return true;
}

bool Machine::SaveSession(Session *sess)
{
    QString path = getDataPath();

    if (sess->IsChanged()) { sess->Store(path); }

    return true;
}

/*void Machine::queSaveList(Session * sess)
{
    if (!m_save_threads_running) {
        // Threads aren't being used.. so run the actual immediately...

         int i = (float(m_donetasks) / float(m_totaltasks) * 100.0);
        //qprogress->setValue(i);
        //QApplication::processEvents();

        sess->UpdateSummaries();
        sess->Store(getDataPath());

        if (!AppSetting->cacheSessions()) {
            sess->TrashEvents();
        }

    } else {
        listMutex.lock();
        m_savelist.append(sess);
        listMutex.unlock();
    }
}*/

Session *Machine::popSaveList()
{
    Session *sess = nullptr;
    listMutex.lock();

    if (!m_savelist.isEmpty()) {
        sess = m_savelist.at(0);
        m_savelist.pop_front();
        m_donetasks++;
    }

    listMutex.unlock();
    return sess;
}

/*// Call any time queing starts
void Machine::StartSaveThreads()
{
    m_savelist.clear();
    if (!AppSetting->multithreading()) return;

    QString path = getDataPath();

    int threads = QThread::idealThreadCount();
    savelistSem = new QSemaphore(threads);
    savelistSem->acquire(threads);

    m_save_threads_running = true;
    m_donetasks=0;
    m_totaltasks=0;

    for (int i = 0; i < threads; i++) {
        SaveThread * thr = new SaveThread(this, path);
        QObject::connect(thr, SIGNAL(UpdateProgress(int)), qprogress, SLOT(setValue(int)));
        thread.push_back(thr);
        thread[i]->start();
    }

}

// Call when all queing is completed
void Machine::FinishSaveThreads()
{
    if (!m_save_threads_running)
        return;

    m_save_threads_running = false;

    // Wait for all tasks to finish
    while (!savelistSem->tryAcquire(thread.size(), 250)) {
        if (qprogress) {
            QApplication::processEvents();
        }
    }

    for (int i = 0; i < thread.size(); ++i) {
        while (thread[i]->isRunning()) {
            SaveThread::msleep(250);
            QApplication::processEvents();
        }
        QObject::disconnect(thread[i], SIGNAL(UpdateProgress(int)), qprogress, SLOT(setValue(int)));

        delete thread[i];
    }

    delete savelistSem;
} */

void SaveThread::run()
{
    bool running = true;
    while (running) {
        Session *sess = machine->popSaveList();
        if (sess) {
            if (machine->m_donetasks % 20 == 0) {
                int i = (float(machine->m_donetasks) / float(machine->m_totaltasks) * 100.0);
                emit UpdateProgress(i);
            }
            sess->UpdateSummaries();
            //machine->saveMutex.lock();
            sess->Store(path);
            //machine->saveMutex.unlock();

            sess->TrashEvents();
        } else {
            if (!machine->m_save_threads_running) {
                break; // done
            } else {
                yieldCurrentThread(); // go do something else for a while
            }
        }
    }

    machine->savelistSem->release(1);
}



class SaveTask:public ImportTask
{
public:
    SaveTask(Session * s, Machine * m): sess(s), mach(m) {}
    virtual ~SaveTask() {}
    virtual void run();

protected:
    Session * sess;
    Machine * mach;
};

void SaveTask::run()
{
    sess->UpdateSummaries();
    mach->saveMutex.lock();
    sess->Store(mach->getDataPath());
    mach->saveMutex.unlock();
    sess->TrashEvents();
}

void Machine::queTask(ImportTask * task)
{
    if (AppSetting->multithreading()) {
        m_tasklist.push_back(task);
        return;
    }

    // Not multithreading, run it right now...
    task->run();
    return;
}

void Machine::runTasks()
{
    if (m_tasklist.isEmpty())
        return;

    QThreadPool * threadpool = QThreadPool::globalInstance();
    //int m_totaltasks=m_tasklist.size();
    //int m_currenttask=0;
 //   if (loader()) emit loader()->setProgressMax(m_totaltasks);
    while (!m_tasklist.isEmpty()) {
        if (threadpool->tryStart(m_tasklist.at(0))) {
            m_tasklist.pop_front();
/*            if (loader()) {
                emit loader()->setProgressValue(++m_currenttask);
                QApplication::processEvents();
            }*/
        }
    }
    QThreadPool::globalInstance()->waitForDone(-1);

}

bool Machine::hasModifiedSessions()
{
    QHash<SessionID, Session *>::iterator s;

    for (s = sessionlist.begin(); s != sessionlist.end(); s++) {
        if (s.value()->IsChanged()) {
            return true;
        }
    }
    return false;
}

const QString summaryFileName = "Summaries.xml";
const int summaryxml_version=1;

class LoadTask:public ImportTask
{
public:
    LoadTask(Session * s, Machine * m): sess(s), mach(m) {}
    virtual ~LoadTask() {}
    virtual void run();

protected:
    Session * sess;
    Machine * mach;
};

void LoadTask::run()
{
    sess->LoadSummary();
}

bool Machine::LoadSummary(ProgressDialog * progress)
{
    QTime time;
    time.start();
    QString filename = getDataPath() + summaryFileName + ".gz";

    QDomDocument doc;
    QFile file(filename);
    qDebug() << "Loading" << filename.toLocal8Bit().data();
    progress->setMessage(QObject::tr("Loading Summaries.xml.gz"));
    QApplication::processEvents();

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open" << filename;
        return false;
    }

    QByteArray data = file.readAll();
    QByteArray uncompressed = gUncompress(data);

    QString errorMsg;
    int errorLine;

    if (!doc.setContent(uncompressed, false, &errorMsg, &errorLine)) {
        qWarning() << "Invalid XML Content in" << filename;
        qWarning() << "Error line" << errorLine << ":" << errorMsg;
        return false;
    }

    file.close();

    QDomElement root = doc.documentElement();

    if (root.tagName().compare("sessions", Qt::CaseInsensitive) != 0) {
        qDebug() << "Summaries cache messed up, recreating...";
        return false;
    }
    bool ok;
    int version = root.attribute("version", "").toInt(&ok);
    if (!ok || (version != summaryxml_version)) {
        qDebug() << "Summaries cache outdated, recreating...";
        return false;
    }
    QDomNode node;

    bool s_ok;

    QDomNodeList sessionlist = root.childNodes();

    int size = sessionlist.size();

    QMap<qint64, Session *>  sess_order;

    progress->setProgressMax(size);
    for (int s=0; s < size; ++s) {
        if ((s % 20) == 0) {
            progress->setProgressValue(s);
            QApplication::processEvents();
        }
        node = sessionlist.at(s);
        QDomElement e = node.toElement();
        SessionID sessid = e.attribute("id", "0").toLong(&s_ok);
        qint64 first =  e.attribute("first", "0").toLongLong();
        qint64 last =  e.attribute("last", "0").toLongLong();
        bool enabled = e.attribute("enabled", "1").toInt() == 1;
        bool events = e.attribute("events", "1").toInt() == 1;
        if (s_ok) {
            Session * sess = new Session(this, sessid);
            sess->really_set_first(first);
            sess->really_set_last(last);
            sess->setEnabled(enabled);
            sess->setSummaryOnly(!events);

            if (e.hasChildNodes()) {
                QList<ChannelID> available_channels;
                QList<ChannelID> available_settings;

                QDomElement chans = e.firstChildElement("channels");
                if (chans.isElement()) {
                    QDomNode node = chans.firstChild();
                    QString txt = node.nodeValue();
                    QStringList channels = txt.split(",");
                    for (int i=0; i<channels.size(); ++i) {
                        bool ok;
                        ChannelID code = channels.at(i).toInt(&ok, 16);
                        available_channels.append(code);
                    }
                }
                sess->m_availableChannels = available_channels;

                QDomElement sete = e.firstChildElement("settings");
                if (sete.isElement()) {
                    QString sets = sete.firstChild().nodeValue();
                    QStringList settings = sets.split(",");
                    for (int i=0; i<settings.size(); ++i) {
                        bool ok;
                        ChannelID code = settings.at(i).toInt(&ok, 16);
                        available_settings.append(code);
                    }
                }
                sess->m_availableSettings = available_settings;
            }


            sess_order[first] = sess;
        }
    }
    QMap<qint64, Session *>::iterator it_end = sess_order.end();
    QMap<qint64, Session *>::iterator it;
    int cnt = 0;
    bool loadSummaries = profile->session->preloadSummaries();

    //progress->setMessage(QObject::tr("Queueing Open Tasks"));
    //QApplication::processEvents();

  //  progress->setMaximum(sess_order.size());
    for (it = sess_order.begin(); it != it_end; ++it, ++cnt) {
        //
/*        if ((cnt % 100) == 0) {
            progress->setValue(cnt);
            //QApplication::processEvents();
        } */
        Session * sess = it.value();
        if (!AddSession(sess)) {
            delete sess;
        } else {
            if (loadSummaries) {
                if (loader()) {
                    loader()->queTask(new LoadTask(sess,this));
                } else {
                    // no progress bar
                    queTask(new LoadTask(sess,this));
                }
            }
        }
    }
    progress->setMessage(QObject::tr("Loading Summary Data"));
    QApplication::processEvents();

    if (loader()) {
        loader()->runTasks();
    } else {
        runTasks();
    }
    progress->setProgressValue(sess_order.size());
    QApplication::processEvents();

    qDebug() << "Loaded" << info.series.toLocal8Bit().data() << info.model.toLocal8Bit().data() << "data in" << time.elapsed() << "ms";

    return true;
}

bool Machine::SaveSummaryCache()
{
    qDebug() << "Saving" << info.brand << info.model <<  "Summaries";
    QString filename = getDataPath() + summaryFileName;

    QDomDocument doc("SleepyHeadSessionIndex");

    QDomElement root = doc.createElement("sessions");
    root.setAttribute("version", summaryxml_version);
    root.setAttribute("profile", profile->user->userName());
    root.setAttribute("count", sessionlist.size());
    root.setAttribute("loader", info.loadername);
    root.setAttribute("serial", info.serial);

    doc.appendChild(root);

    if (!QDir().exists(getSummariesPath()))
        QDir().mkpath(getSummariesPath());

    QHash<SessionID, Session *>::iterator s;
    QHash<SessionID, Session *>::iterator sess_end = sessionlist.end();

    for (s = sessionlist.begin(); s != sess_end; ++s) {
        QDomElement el = doc.createElement("session");
        Session * sess = s.value();
        el.setAttribute("id", (quint32)sess->session());
        el.setAttribute("first", sess->realFirst());
        el.setAttribute("last", sess->realLast());
        el.setAttribute("enabled", sess->enabled() ? "1" : "0");
        el.setAttribute("events", sess->summaryOnly() ? "0" : "1");

        QHash<ChannelID, QVector<EventList *> >::iterator ev;
        QHash<ChannelID, QVector<EventList *> >::iterator ev_end = sess->eventlist.end();
        QStringList chanlist;
        for (ev = sess->eventlist.begin(); ev != ev_end; ++ev) {
            chanlist.append(QString::number(ev.key(), 16));
        }
        if (chanlist.size() == 0) {
            for (int i=0; i<sess->m_availableChannels.size(); i++) {
                ChannelID code = sess->m_availableChannels.at(i);
                chanlist.append(QString::number(code, 16));
            }
        }

        QDomElement chans = doc.createElement("channels");
        chans.appendChild(doc.createTextNode(chanlist.join(",")));
        el.appendChild(chans);

        chanlist.clear();
        QHash<ChannelID, QVariant>::iterator si;
        QHash<ChannelID, QVariant>::iterator set_end = sess->settings.end();
        for (si = sess->settings.begin(); si != set_end; ++si) {
            chanlist.append(QString::number(si.key(), 16));
        }
        QDomElement settings = doc.createElement("settings");
        settings.appendChild(doc.createTextNode(chanlist.join(",")));
        el.appendChild(settings);

        root.appendChild(el);
        if (sess->IsChanged())
            sess->StoreSummary();
    }

    QString xmltext;
    QTextStream ts(&xmltext);
    doc.save(ts, 1);

    QByteArray data = gCompress(xmltext.toUtf8());

    QFile file(filename + ".gz");

    file.open(QFile::WriteOnly);
    file.write(data);

    return true;
}

bool Machine::Save()
{
    //int size;
    int cnt = 0;

    QString path = getDataPath();
    QDir dir(path);

    if (!dir.exists()) {
        dir.mkdir(path);
    }

    QHash<SessionID, Session *>::iterator s;

    m_savelist.clear();

    // store any event summaries..
    for (s = sessionlist.begin(); s != sessionlist.end(); s++) {
        cnt++;

        if ((*s)->IsChanged()) {
            queTask(new SaveTask(*s, this));
        }
    }

    runTasks();

    return true;
}

void Machine::updateChannels(Session * sess)
{
    int size = sess->m_availableChannels.size();
    for (int i=0; i < size; ++i) {
        ChannelID code = sess->m_availableChannels.at(i);
        m_availableChannels[code] = true;
    }

    size = sess->m_availableSettings.size();
    for (int i=0; i < size; ++i) {
        ChannelID code = sess->m_availableSettings.at(i);
        m_availableSettings[code] = true;
    }
}

QList<ChannelID> Machine::availableChannels(quint32 chantype)
{
    QList<ChannelID> list;

    QHash<ChannelID, bool>::iterator end = m_availableChannels.end();
    QHash<ChannelID, bool>::iterator it;
    for (it = m_availableChannels.begin(); it != end; ++it) {
        ChannelID code = it.key();
        const schema::Channel & chan = schema::channel[code];
        if (chan.type() & chantype) {
            list.push_back(code);
        }
    }
    return list;
}



//////////////////////////////////////////////////////////////////////////////////////////
// CPAP implmementation
//////////////////////////////////////////////////////////////////////////////////////////
CPAP::CPAP(Profile * profile, MachineID id): Machine(profile, id)
{
    m_type = MT_CPAP;
}

CPAP::~CPAP()
{
}

//////////////////////////////////////////////////////////////////////////////////////////
// Oximeter Class implmementation
//////////////////////////////////////////////////////////////////////////////////////////
Oximeter::Oximeter(Profile * profile, MachineID id): Machine(profile, id)
{
    m_type = MT_OXIMETER;
}

Oximeter::~Oximeter()
{
}

//////////////////////////////////////////////////////////////////////////////////////////
// SleepStage Class implmementation
//////////////////////////////////////////////////////////////////////////////////////////
SleepStage::SleepStage(Profile * profile, MachineID id): Machine(profile, id)
{
    m_type = MT_SLEEPSTAGE;
}
SleepStage::~SleepStage()
{
}

//////////////////////////////////////////////////////////////////////////////////////////
// PositionSensor Class implmementation
//////////////////////////////////////////////////////////////////////////////////////////
PositionSensor::PositionSensor(Profile * profile, MachineID id): Machine(profile, id)
{
    m_type = MT_POSITION;
}
PositionSensor::~PositionSensor()
{
}
