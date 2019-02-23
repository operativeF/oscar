/* SleepLib Machine Loader Class Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QThreadPool>

#include "machine_loader.h"

bool genpixmapinit = false;
QPixmap * MachineLoader::genericCPAPPixmap;

// This crap moves to Profile
QList<MachineLoader *> m_loaders;

QList<MachineLoader *> GetLoaders(MachineType mt)
{
    QList<MachineLoader *> list;
    for (int i=0; i < m_loaders.size(); ++i) {
        if (mt == MT_UNKNOWN) {
            list.push_back(m_loaders.at(i));
        } else {
            MachineType mtype = m_loaders.at(i)->type();
            if (mtype == mt) {
                list.push_back(m_loaders.at(i));
            }
        }
    }
    return list;
}

MachineLoader * lookupLoader(Machine * m)
{
    for (int i=0; i < m_loaders.size(); ++i) {
        MachineLoader * loader = m_loaders.at(i);
        if (loader->loaderName() == m->loaderName())
            return loader;
    }
    return nullptr;
}

MachineLoader * lookupLoader(QString loaderName)
{
    for (int i=0; i < m_loaders.size(); ++i) {
        MachineLoader * loader = m_loaders.at(i);
        if (loader->loaderName() == loaderName)
            return loader;
    }
    return nullptr;
}

void RegisterLoader(MachineLoader *loader)
{
    loader->initChannels();
    m_loaders.push_back(loader);
}
void DestroyLoaders()
{
    for (auto & loader : m_loaders) {
        delete(loader);
    }

    m_loaders.clear();
}

MachineLoader::MachineLoader() :QObject(nullptr)
{
    if (!genpixmapinit) {
        genericCPAPPixmap = new QPixmap(genericPixmapPath);
        genpixmapinit = true;
    }
    m_abort = false;
    m_type = MT_UNKNOWN;
    m_status = NEUTRAL;
}

MachineLoader::~MachineLoader()
{
}

void MachineLoader::finishAddingSessions()
{
    // Using a map specifically so they are inserted in order.
    for (auto it=new_sessions.begin(), end=new_sessions.end(); it != end; ++it) {
        Session * sess = it.value();
        Machine * mach = sess->machine();
        mach->AddSession(sess);
    }
    new_sessions.clear();
}

bool uncompressFile(QString infile, QString outfile)
{
    if (!infile.endsWith(".gz",Qt::CaseInsensitive)) {
        qDebug() << "uncompressFile()" << outfile << "missing .gz extension???";
        return false;
    }

    if (QFile::exists(outfile)) {
        qDebug() << "uncompressFile()" << outfile << "already exists";
        return false;
    }

    // Get file length from inside gzip file
    QFile fi(infile);

    if (!fi.open(QFile::ReadOnly) || !fi.seek(fi.size() - 4)) {
        return false;
    }

    unsigned char ch[4];
    fi.read((char *)ch, 4);
    quint32 datasize = ch[0] | (ch [1] << 8) | (ch[2] << 16) | (ch[3] << 24);

    // Open gzip file for reading
    gzFile f = gzopen(infile.toLatin1(), "rb");
    if (!f) {
        return false;
    }


    // Decompressed header and data block
    char * buffer = new char [datasize];
    gzread(f, buffer, datasize);
    gzclose(f);

    QFile out(outfile);
    if (out.open(QFile::WriteOnly)) {
        out.write(buffer, datasize);
        out.close();
    }

    delete [] buffer;
    return true;

}

bool compressFile(QString infile, QString outfile)
{
    if (outfile.isEmpty()) {
        outfile = infile + ".gz";
    } else if (!outfile.endsWith(".gz")) {
        outfile += ".gz";
    }
    if (QFile::exists(outfile)) {
        qDebug() << "compressFile()" << outfile << "already exists";
        return false;
    }

    QFile f(infile);

    if (!f.exists(infile)) {
        qDebug() << "compressFile()" << infile << "does not exist";
        return false;
    }

    qint64 size = f.size();

    if (!f.open(QFile::ReadOnly)) {
        qDebug() << "compressFile() Couldn't open" << infile;
        return false;
    }

    char *buf = new char [size];

    if (!f.read(buf, size)) {
        delete [] buf;
        qDebug() << "compressFile() Couldn't read all of" << infile;
        return false;
    }

    f.close();

    gzFile gz = gzopen(outfile.toLatin1(), "wb");

    //gzbuffer(gz,65536*2);
    if (!gz) {
        qDebug() << "compressFile() Couldn't open" << outfile << "for writing";
        delete [] buf;
        return false;
    }

    gzwrite(gz, buf, size);
    gzclose(gz);
    delete [] buf;
    return true;
}

void MachineLoader::queTask(ImportTask * task)
{
    m_tasklist.push_back(task);
}

void MachineLoader::runTasks(bool threaded)
{

    m_totaltasks=m_tasklist.size();
    if (m_totaltasks == 0) return;
    emit setProgressMax(m_totaltasks);
    m_currenttask=0;

    threaded=AppSetting->multithreading();

    if (!threaded) {
        while (!m_tasklist.isEmpty() && !m_abort) {
            ImportTask * task = m_tasklist.takeFirst();
            task->run();

            // update progress bar
            m_currenttask++;
            emit setProgressValue(++m_currenttask);
            QApplication::processEvents();

            delete task;
        }
    } else {
        ImportTask * task = m_tasklist[0];

        QThreadPool * threadpool = QThreadPool::globalInstance();

        while (!m_abort) {

            if (threadpool->tryStart(task)) {
                m_tasklist.pop_front();

                if (!m_tasklist.isEmpty()) {
                    // next task to be run
                    task = m_tasklist[0];

                    // update progress bar
                    emit setProgressValue(++m_currenttask);
                    QApplication::processEvents();
                } else {
                    // job list finished
                    break;
                }
            }
            //QThread::sleep(100);
        }
        QThreadPool::globalInstance()->waitForDone(-1);
    }
    if (m_abort) {
        // delete remaining tasks and clear task list
        for (auto & task : m_tasklist) {
            delete task;
        }
        m_tasklist.clear();
    }
}


QList<ChannelID> CPAPLoader::eventFlags(Day * day)
{
    Machine * mach = day->machine(MT_CPAP);

    QList<ChannelID> list;

    if (mach->loader() != this) {
        qDebug() << "Trying to ask" << loaderName() << "for" << mach->loaderName() << "data";
        return list;
    }

    list.push_back(CPAP_ClearAirway);
    list.push_back(CPAP_Obstructive);
    list.push_back(CPAP_Hypopnea);
    list.push_back(CPAP_Apnea);

    return list;
}

/*const QString machine_profile_name="MachineList.xml";

void MachineLoader::LoadMachineList()
{
}

void MachineLoader::StoreMachineList()
{
}
void MachineLoader::LoadSummary(Machine *m, QString &filename)
{
    QFile f(filename);
    if (!f.exists())
        return;
    f.open(QIODevice::ReadOnly);
    if (!f.isReadable())
        return;


}
void MachineLoader::LoadSummaries(Machine *m)
{
    QString path=(*profile)["ProfileDirectory"]+"/"+m_classname+"/"+mach->hexid();
    QDir dir(path);
    if (!dir.exists() || !dir.isReadable())
        return false;

    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);

    QString fullpath,ext_s,sesstr;
    int ext;
    SessionID sessid;
    bool ok;
    QMap<SessionID, int> sessions;
    QFileInfoList list=dir.entryInfoList();
    for (int i=0;i<list.size();i++) {
        QFileInfo fi=list.at(i);
        fullpath=fi.canonicalFilePath();
        ext_s=fi.fileName().section(".",-1);
        ext=ext_s.toInt(&ok,10);
        if (!ok) continue;
        sesstr=fi.fileName().section(".",0,-2);
        sessid=sesstr.toLong(&ok,16);
        if (!ok) continue;

    }
}

void MachineLoader::LoadAllSummaries()
{
    for (int i=0;i<m_machlist.size();i++)
        LoadSummaries(m_machlist[i]);
}
void MachineLoader::LoadAllEvents()
{
    for (int i=0;i<m_machlist.size();i++)
        LoadEvents(m_machlist[i]);
}
void MachineLoader::LoadAllWaveforms()
{
    for (int i=0;i<m_machlist.size();i++)
        LoadWaveforms(m_machlist[i]);
}
void MachineLoader::LoadAll()
{
    LoadAllSummaries();
    LoadAllEvents();
    LoadAllWaveforms();
}

void MachineLoader::Save(Machine *m)
{
}
void MachineLoader::SaveAll()
{
    for (int i=0;i<m_machlist.size();i++)
        Save(m_machlist[i]);
}
*/
