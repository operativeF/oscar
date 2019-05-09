/* SleepLib CMS50X Loader Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef CMS50F37LOADER_H
#define CMS50F37LOADER_H

#include "SleepLib/serialoximeter.h"

const QString cms50f37_class_name = "CMS50F37";
const int cms50f37_data_version = 0;


/*! \class CMS5037Loader
    \brief Bulk Importer for newer CMS50 oximeters
    */
class CMS50F37Loader : public SerialOximeter
{
Q_OBJECT
  public:


    CMS50F37Loader();
    virtual ~CMS50F37Loader();

    virtual bool Detect(const QString &path);
    virtual int Open(const QString & path);
    virtual bool openDevice();


    static void Register();

    virtual int Version() { return cms50f37_data_version; }
    virtual const QString &loaderName() { return cms50f37_class_name; }

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_OXIMETER, 0, cms50f37_class_name, QObject::tr("Contec"), QObject::tr("CMS50F3.7"), QString(), QString(), QObject::tr("CMS50F"), QDateTime::currentDateTime(), cms50f37_data_version);
    }


  //  Machine *CreateMachine();

    virtual void process();

    virtual bool isStartTimeValid() { return !cms50dplus; }

    virtual QString getUser();
    virtual QString getModel();
    virtual QString getVendor();
    virtual QString getDeviceString();

    virtual QDateTime getDateTime(int session);
    virtual int getDuration(int session);
    virtual int getSessionCount();
    virtual int getUserCount();
    virtual int getOximeterInfo();
    virtual void eraseSession(int user, int session);

    virtual void syncClock();
    virtual QString getDeviceID();
    virtual void setDeviceID(const QString &);


    virtual void setDuration(int d) { duration=d; }

    virtual bool commandDriven() { return true; }


    virtual void getSessionData(int session);


    // Switch device to record transmission mode
    void requestData();


protected slots:
//    virtual void dataAvailable();
    virtual void resetImportTimeout();
    virtual void startImportTimeout();
    virtual void shutdownPorts();



    void nextCommand();


protected:

    bool readSpoRFile(const QString & path);
    virtual void processBytes(QByteArray bytes);

//    int doLiveMode();

    virtual void killTimers();

    void sendCommand(quint8 c);
    void sendCommand(quint8 c, quint8 c2);


    // Switch device to live streaming mode
    virtual void resetDevice();



  private:

    int sequence;

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

    QDate imp_date;
    QTime imp_time;

    QString userName;

    unsigned char current_command;

    volatile int session_count;
    volatile int duration;
    volatile int userCount;
    volatile int userIdx;
    volatile int protocolVersion;
    int device_info;
    QString model;
    QString vendor;
    QString devid;

    int duration_divisor;
    int selected_session;

    int timectr;

    int modelsegments;

};


#endif // CMS50F37LOADER_H
