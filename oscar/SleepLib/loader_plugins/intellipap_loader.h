/* Intellipap Loader Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef INTELLIPAP_LOADER_H
#define INTELLIPAP_LOADER_H

#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"


//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation.
//
const int intellipap_data_version = 3;
//
//********************************************************************************************

/*! \class Intellipap
    \brief Intellipap customized machine object
    */
class Intellipap: public CPAP
{
  public:
    Intellipap(Profile *, MachineID id = 0);
    virtual ~Intellipap();
};


const int intellipap_load_buffer_size = 1024 * 1024;

extern ChannelID INTP_SmartFlexMode;
extern ChannelID INTP_SmartFlexLevel;

const QString intellipap_class_name = STR_MACH_Intellipap;

/*! \class IntellipapLoader
    \brief Loader for DeVilbiss Intellipap Auto data
    This is only relatively recent addition and still needs more work
    */
class IntellipapLoader : public CPAPLoader
{
  public:
    IntellipapLoader();
    virtual ~IntellipapLoader();

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Scans path for Intellipap data signature, and Loads any new data
    virtual int Open(const QString & path);
    //! \brief Scans path for Intellipap DV5 data signature, and Loads any new data
    virtual int OpenDV5(const QString & path);
    //! \brief Scans path for Intellipap DV6 data signature, and Loads any new data
    virtual int OpenDV6(const QString & path);

    //! \brief Returns SleepLib database version of this IntelliPap loader
    virtual int Version() { return intellipap_data_version; }

    //! \brief Returns the machine class name of this IntelliPap, "Intellipap"
    virtual const QString &loaderName() { return intellipap_class_name; }

    //! \brief Creates a machine object, indexed by serial number
 //   Machine *CreateMachine(QString serial);

    //! \brief Registers this MachineLoader with the master list, so Intellipap data can load
    static void Register();

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, intellipap_class_name, QObject::tr("DeVilbiss"), QString(), QString(), QString(), QObject::tr("Intellipap"), QDateTime::currentDateTime(), intellipap_data_version);
    }
    virtual void initChannels();


    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Now for some CPAPLoader overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual QString presRelLabel() { return QObject::tr("SmartFlex Settings"); } // might not need this one

    virtual ChannelID presReliefMode() { return INTP_SmartFlexMode; }
    virtual ChannelID presRelLevel() { return INTP_SmartFlexLevel; }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

  protected:
    QString last;

    unsigned char *m_buffer;
};


#endif // INTELLIPAP_LOADER_H
