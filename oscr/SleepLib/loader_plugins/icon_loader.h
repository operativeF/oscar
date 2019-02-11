/* SleepLib Fisher & Paykel Icon Loader Implementation
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef ICON_LOADER_H
#define ICON_LOADER_H

#include <QMultiMap>
#include "SleepLib/machine.h"
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"


//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation.
//
const int fpicon_data_version = 3;
//
//********************************************************************************************

/*! \class FPIcon
    \brief F&P Icon customized machine object
    */
class FPIcon: public CPAP
{
  public:
    FPIcon(Profile *, MachineID id = 0);
    virtual ~FPIcon();
};


const int fpicon_load_buffer_size = 1024 * 1024;


const QString fpicon_class_name = STR_MACH_FPIcon;

/*! \class FPIconLoader
    \brief Loader for Fisher & Paykel Icon data
    This is only relatively recent addition and still needs more work
    */

class FPIconLoader : public CPAPLoader
{
  Q_OBJECT
  public:
    FPIconLoader();
    virtual ~FPIconLoader();

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Scans path for F&P Icon data signature, and Loads any new data
    virtual int Open(const QString & path);

    int OpenMachine(Machine *mach, const QString &path);

    bool OpenSummary(Machine *mach, const QString & path);
    bool OpenDetail(Machine *mach, const QString & path);
    bool OpenFLW(Machine *mach, const QString & filename);

    //! \brief Returns SleepLib database version of this F&P Icon loader
    virtual int Version() { return fpicon_data_version; }

    //! \brief Returns the machine class name of this CPAP machine, "FPIcon"
    virtual const QString & loaderName() { return fpicon_class_name; }

    // ! \brief Creates a machine object, indexed by serial number
    //Machine *CreateMachine(QString serial);

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, fpicon_class_name, QObject::tr("Fisher & Paykel"), QString(), QString(), QString(), QObject::tr("ICON"), QDateTime::currentDateTime(), fpicon_data_version);
    }


    //! \brief Registers this MachineLoader with the master list, so F&P Icon data can load
    static void Register();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Now for some CPAPLoader overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual QString presRelType() { return QObject::tr(""); } // might not need this one

    virtual ChannelID presRelSet() { return NoChannel; }
    virtual ChannelID presRelLevel() { return NoChannel; }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////


  protected:
    QDateTime readFPDateTime(quint8 *data);

    QString last;
    QHash<QString, Machine *> MachList;
    QMap<SessionID, Session *> Sessions;
    QMultiMap<QDate, Session *> SessDate;
    //QMap<int,QList<EventList *> > FLWMapFlow;
    //QMap<int,QList<EventList *> > FLWMapLeak;
    //QMap<int,QList<EventList *> > FLWMapPres;
    //QMap<int,QList<qint64> > FLWDuration;
    //QMap<int,QList<qint64> > FLWTS;
    //QMap<int,QDate> FLWDate;

    unsigned char *m_buffer;
};

#endif // ICON_LOADER_H
