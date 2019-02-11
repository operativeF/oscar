/* SleepLib Weinmann SOMNOsoft/Balance Loader Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef WEINMANN_LOADER_H
#define WEINMANN_LOADER_H

#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"


//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation.
//
const int weinmann_data_version = 3;
//
//********************************************************************************************

/*! \class Weinmann
    \brief Weinmann customized machine object
    */
class Weinmann: public CPAP
{
  public:
    Weinmann(Profile *, MachineID id = 0);
    virtual ~Weinmann();
};


struct CompInfo
{
    CompInfo() {
        session = nullptr;
        flow_start = 0; flow_size = 0;
        stat_start = 0; stat_size = 0;
        pres_start = 0; pres_size = 0;
        amv_start = 0; amv_size =0;
        event_start = 0; event_recs = 0;
    }
    CompInfo(const CompInfo & copy) {
        session = copy.session;
        time = copy.time;
        flow_start = copy.flow_start;
        flow_size= copy.flow_size;
        stat_start = copy.flow_start;
        stat_size= copy.flow_size;
        pres_start = copy.pres_start;
        pres_size = copy.pres_size;
        amv_start = copy.amv_start;
        amv_size = copy.amv_size;
        event_start = copy.event_start;
        event_recs = copy.event_recs;
    }
    CompInfo(Session * sess, QDateTime dt, quint32 fs, quint32 fl, quint32 ss, quint32 sl,quint32 ps, quint32 pl, quint32 ms, quint32 ml, quint32 es, quint32 er):
        session(sess), time(dt),
        flow_start(fs), flow_size(fl),
        stat_start(ss), stat_size(sl),
        pres_start(ps), pres_size(pl),
        amv_start(ms), amv_size(ml),
        event_start(es), event_recs(er) {}
    Session * session;
    QDateTime time;
    quint32 flow_start;
    quint32 flow_size;
    quint32 stat_start;
    quint32 stat_size;
    quint32 pres_start;
    quint32 pres_size;
    quint32 amv_start;
    quint32 amv_size;
    quint32 event_start;
    quint32 event_recs;
};

const QString weinmann_class_name = STR_MACH_Weinmann;

/*! \class WeinmannLoader
    \brief Loader for Weinmann CPAP data
    This is only relatively recent addition and still needs more work
    */
class WeinmannLoader : public CPAPLoader
{
  public:
    WeinmannLoader();
    virtual ~WeinmannLoader();

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Scans path for Weinmann data signature, and Loads any new data
    virtual int Open(const QString & path);

    //! \brief Returns SleepLib database version of this Weinmann loader
    virtual int Version() { return weinmann_data_version; }

    //! \brief Returns the machine loader name of this class
    virtual const QString &loaderName() { return weinmann_class_name; }

    int ParseIndex(QFile & wmdata);


    //! \brief Creates a machine object, indexed by serial number
 //   Machine *CreateMachine(QString serial);

    //! \brief Registers this MachineLoader with the master list, so Weinmann data can load
    static void Register();

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, weinmann_class_name, QObject::tr("Weinmann"), QObject::tr("SOMNOsoft2"), QString(), QString(), QObject::tr(""), QDateTime::currentDateTime(), weinmann_data_version);
    }
    virtual void initChannels();


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Now for some CPAPLoader overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual QString presRelType() { return QObject::tr("Unknown"); } // might not need this one

    virtual ChannelID presRelSet() { return NoChannel; }
    virtual ChannelID presRelLevel() { return NoChannel; }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

  protected:
    QHash<QString, int> index;
    QList<CompInfo> compinfo;
    QMap<SessionID, Session *> sessions;

    QString last;

    unsigned char *m_buffer;
};


#endif // WEINMANN_LOADER_H
