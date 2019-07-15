/* SleepLib MachineLoader Base Class Header
 *
 * Copyright (c) 2019 The OSCAR Team
 * Copyright (c) 2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef MACHINE_LOADER_H
#define MACHINE_LOADER_H

#include <QMutex>
#include <QRunnable>
#include <QPixmap>


#include "profiles.h"
#include "machine.h"
#ifdef _MSC_VER
#include "QtZlib/zlib.h"
#else
#include "zlib.h"
#endif

#ifdef UNITTEST_MODE
#define private public
#define protected public
#endif

class MachineLoader;    // forward

enum DeviceStatus { NEUTRAL, IMPORTING, LIVE, DETECTING };

const QString genericPixmapPath = ":/icons/mask.png";


/*! \class MachineLoader
    \brief Base class to derive a new Machine importer from
    */
class MachineLoader: public QObject
{
    Q_OBJECT
    friend class ImportThread;
    friend class Machine;
  public:
    MachineLoader();
    virtual ~MachineLoader();

    //! \brief Detect if the given path contains a valid folder structure
    virtual bool Detect(const QString & path) = 0;

    //! \brief Look up and return machine model information stored at path
    virtual MachineInfo PeekInfo(const QString & path) { Q_UNUSED(path); return MachineInfo(); }

    //! \brief Override this to scan path and detect new machine data
    virtual int Open(const QString & path) = 0;

    //! \brief Override to returns the Version number of this MachineLoader
    virtual int Version() = 0;

    // !\\brief Used internally by loaders, override to return base MachineInfo record
    virtual MachineInfo newInfo() { return MachineInfo(); }

    //! \brief Override to returns the class name of this MachineLoader
    virtual const QString & loaderName() = 0;

    virtual void process() {}

    virtual void initChannels() {}

    void unsupported(Machine * m);

    void addSession(Session * sess);

    inline MachineType type() { return m_type; }
    inline DeviceStatus status() { return m_status; }
    inline void setStatus(DeviceStatus status) { m_status = status; }

    QPixmap & getPixmap(QString series);
    QString getPixmapPath(QString series);

    void queTask(ImportTask * task);
    //! \brief Process Task list using all available threads.
    void runTasks(bool threaded = false);

    inline int countTasks() { return m_MLtasklist.size(); }

    inline bool isAborted() { return m_abort; }
    inline void abort() { m_abort = true; }

    QMutex sessionMutex;
    QMutex saveMutex;
public slots:
    void abortImport() { abort(); }

signals:
    void updateProgress(int cnt, int total);
    void setProgressMax(int max);
    void setProgressValue(int val);
    void updateMessage(QString);
    void machineUnsupported(Machine *);

protected:
    void finishAddingSessions();

    static QPixmap * genericCPAPPixmap;

    int m_currentMLtask;
    int m_totalMLtasks;

    bool m_abort;

    DeviceStatus m_status;
    MachineType m_type;
    QString m_class;

    QMap<SessionID, Session *> new_sessions;

    QHash<QString, QPixmap> m_pixmaps;
    QHash<QString, QString> m_pixmap_paths;

  private:
    QList<ImportTask *> m_MLtasklist;
};

class CPAPLoader:public MachineLoader
{
    Q_OBJECT
public:
    CPAPLoader() : MachineLoader() {}
    virtual ~CPAPLoader() {}

    virtual QList<ChannelID> eventFlags(Day * day);

    virtual QString PresReliefLabel() { return QString(""); }
    virtual ChannelID PresReliefMode() { return NoChannel; }
    virtual ChannelID PresReliefLevel() { return NoChannel; }
    virtual ChannelID HumidifierConnected() { return NoChannel; }
    virtual ChannelID HumidifierLevel() { return CPAP_HumidSetting; }
    virtual ChannelID CPAPModeChannel() { return CPAP_Mode; }
    virtual void initChannels() {}

};

class ImportPath
{
public:
    ImportPath() {
        path = QString();
        loader = nullptr;
    }
//  ImportPath(const ImportPath & copy) {
//      loader = copy.loader;
//      path = copy.path;
//  }
    ImportPath(QString path, MachineLoader * loader) :
        path(path), loader(loader) {}

    QString path;
    MachineLoader * loader;
};


// Put in machine loader class as static??
void RegisterLoader(MachineLoader *loader);
QList<MachineLoader *> GetLoaders(MachineType mt = MT_UNKNOWN);
MachineLoader * lookupLoader(Machine * m);
MachineLoader * lookupLoader(QString loaderName);

void DestroyLoaders();

// Why here? Where are these called?
bool compressFile(QString inpath, QString outpath = "");
bool uncompressFile(QString infile, QString outfile);


#endif //MACHINE_LOADER_H
