/* SleepLib ChoiceMMed MD300W1 Oximeter Loader Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef MD300W1LOADER_H
#define MD300W1LOADER_H

#include "SleepLib/serialoximeter.h"

const QString md300w1_class_name = "MD300W1";
const int md300w1_data_version = 1;


/*! \class MD300W1Loader
    \brief Importer for ChoiceMMed MD300W1 data format..
    */
class MD300W1Loader : public SerialOximeter
{
Q_OBJECT
  public:
    MD300W1Loader();
    virtual ~MD300W1Loader();

    virtual bool Detect(const QString &path);
    virtual int Open(const QString & path);

    static void Register();

    virtual int Version() { return md300w1_data_version; }
    virtual const QString &loaderName() { return md300w1_class_name; }

    // Machine *CreateMachine();

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_OXIMETER, 0, md300w1_class_name, QObject::tr("ChoiceMMed"), QString(), QString(), QString(), QObject::tr("MD300"), QDateTime::currentDateTime(), md300w1_data_version);
    }


    virtual void process();

    virtual bool isStartTimeValid() { return true; }

protected slots:
    virtual void resetImportTimeout();
    virtual void startImportTimeout();

protected:

    bool readDATFile(const QString & path);
    virtual void processBytes(QByteArray bytes);

    int doImportMode();
    int doLiveMode();

    virtual void killTimers();

    // Switch MD300W1 device to live streaming mode
    virtual void resetDevice();

    // Switch MD300W1 device to record transmission mode
    void requestData();

  private:
    EventList *PULSE;
    EventList *SPO2;

    QTime m_time;

    QByteArray buffer;

    bool started_import;
    bool finished_import;
    bool started_reading;
    QDateTime oxitime;

    int cb_reset,imp_callbacks;

    int received_bytes;

    int m_itemCnt;
    int m_itemTotal;


};


#endif // MD300W1LOADER_H
