/* SleepLib CMS50X Loader Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef CMS50LOADER_H
#define CMS50LOADER_H

#include "SleepLib/serialoximeter.h"

const QString cms50_class_name = "CMS50";
const int cms50_data_version = 4;


/*! \class CMS50Loader
    \brief Importer for CMS50 Oximeter
    */
class CMS50Loader : public SerialOximeter
{
Q_OBJECT
  public:


    CMS50Loader();
    virtual ~CMS50Loader();

    virtual bool Detect(const QString &path);
    virtual int Open(const QString & path);

    static void Register();

    virtual int Version() { return cms50_data_version; }
    virtual const QString &loaderName() { return cms50_class_name; }

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_OXIMETER, 0, cms50_class_name, QObject::tr("Contec"), QObject::tr("CMS50"), QString(), QString(), QObject::tr("CMS50"), QDateTime::currentDateTime(), cms50_data_version);
    }


  //  Machine *CreateMachine();

    virtual void process();

    virtual bool isStartTimeValid() { return !cms50dplus; }

protected slots:
//    virtual void dataAvailable();
    virtual void resetImportTimeout();
    virtual void startImportTimeout();
    virtual void shutdownPorts();

protected:

    bool readSpoRFile(QString path);
    virtual void processBytes(QByteArray bytes);

    int doImportMode();
    int doLiveMode();

    virtual void killTimers();

    // Switch CMS50D+ device to live streaming mode
    virtual void resetDevice();

    // Switch CMS50D+ device to record transmission mode
    void requestData();

  private:
    EventList *PULSE;
    EventList *SPO2;

    QTime m_time;

    QByteArray buffer;

    bool started_import;
    bool finished_import;
    bool started_reading;
    bool cms50dplus;

    int cb_reset,imp_callbacks;

    int received_bytes;

    int m_itemCnt;
    int m_itemTotal;


};


#endif // CMS50LOADER_H
