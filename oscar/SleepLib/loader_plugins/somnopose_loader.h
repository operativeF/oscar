/* SleepLib Somnopose Loader Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SOMNOPOSELOADER_H
#define SOMNOPOSELOADER_H

#include "SleepLib/machine_loader.h"

const QString somnopose_class_name = "Somnopose";
const int somnopose_data_version = 1;


/*! \class SomnoposeLoader
    \brief Unfinished stub for loading Somnopose Positional CSV data
*/
class SomnoposeLoader : public MachineLoader
{
  public:
    SomnoposeLoader();
    virtual ~SomnoposeLoader();

    virtual bool Detect(const QString & path) { Q_UNUSED(path); return false; }  // bypass autoscanner

    virtual int Open(const QString & path);
    virtual int OpenFile(const QString & filename);
    static void Register();

    virtual int Version() { return somnopose_data_version; }
    virtual const QString &loaderName() { return somnopose_class_name; }

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_POSITION, 0, somnopose_class_name, QObject::tr("Somnopose"), QString(), QString(), QString(), QObject::tr("Somnopose Software"), QDateTime::currentDateTime(), somnopose_data_version);
    }


  //Machine *CreateMachine();

  protected:
  private:
};

#endif // SOMNOPOSELOADER_H
