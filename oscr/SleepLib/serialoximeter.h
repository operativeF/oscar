/* SleepLib MachineLoader Base Class Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SERIALOXIMETER_H
#define SERIALOXIMETER_H

#include <QTimer>
#include <QtSerialPort/QSerialPort>

#include "SleepLib/machine_loader.h"

const int START_TIMEOUT = 30000;


struct OxiRecord
{
    quint8 pulse;
    quint8 spo2;
    quint16 perf;

    OxiRecord():pulse(0), spo2(0),perf(0) {}
    OxiRecord(quint8 p, quint8 s): pulse(p), spo2(s) {}
    OxiRecord(quint8 p, quint8 s, quint16 pi): pulse(p), spo2(s), perf(pi) {} // with perfusion index
    OxiRecord(const OxiRecord & copy) { pulse = copy.pulse; spo2 = copy.spo2; perf = copy.perf; }
};

class SerialOximeter : public MachineLoader
{
Q_OBJECT
public:
    SerialOximeter() : MachineLoader() {
        m_importing = m_streaming = false;
        m_productID = m_vendorID = 0;
        have_perfindex = false;
    }
    virtual ~SerialOximeter() {}

    virtual bool Detect(const QString &path)=0;
    virtual int Open(const QString & path)=0;

    static void Register() {}

    virtual int Version()=0;
    virtual const QString &loaderName()=0;

    virtual QDateTime getDateTime(int session) { Q_UNUSED(session); return QDateTime(); }
    virtual int getDuration(int session) { Q_UNUSED(session); return 0; }
    virtual int getSessionCount() { return 0; }
    virtual QString getUser() { return QString(); }
    virtual QString getModel() { return QString(); }
    virtual QString getVendor()  { return QString(); }
    virtual QString getDeviceString()  { return QString(); }

    virtual void getSessionData(int session) { Q_UNUSED(session); }
    virtual void syncClock() {}

    virtual QString getDeviceID() { return QString(); }
    virtual void setDeviceID(const QString &) {}

    virtual void eraseSession(int /*user*/, int /*session*/) {}



    virtual bool commandDriven() { return false; }

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_OXIMETER, 0, "", QString(), QString(), QString(), QString(), "Generic", QDateTime::currentDateTime(), 0);
    }

    // Serial Stuff
    virtual bool scanDevice(QString keyword="",quint16 vendor_id=0, quint16 product_id=0);
    virtual bool openDevice();
    virtual void closeDevice();

    inline bool isStreaming() { return m_streaming; }
    inline bool isImporting() { return m_importing; }

    bool havePerfIndex() { return have_perfindex; }

    virtual void process() {}

    //virtual Machine *CreateMachine()=0;

    // available sessions
    QMap<QDateTime, QVector<OxiRecord> *> oxisessions;

    // current session
    QVector<OxiRecord> * oxirec;

    QDateTime startTime() { return m_startTime; }
    void setStartTime(QDateTime datetime) { m_startTime = datetime; }
    virtual bool isStartTimeValid() { return true; }
    virtual void setDuration(int) { }

    virtual qint64 importResolution() { return 1000; }
    virtual qint64 liveResolution() { return 20; }

    void trashRecords();
    virtual void resetDevice() {}


signals:
    void noDeviceFound();
    void deviceDetected();
    void updatePlethy(QByteArray plethy);
    void importComplete(SerialOximeter *);

protected slots:
    virtual void dataAvailable();
    virtual void resetImportTimeout() {}
    virtual void startImportTimeout() {}

    virtual void stopRecording();
    virtual void shutdownPorts() {}
//    virtual void abortTask();

protected:
    virtual void processBytes(QByteArray buffer) { Q_UNUSED(buffer) }

    virtual void killTimers() {}
    virtual void requestData() {}

    QString port;
    QSerialPort serial;

    QTimer startTimer;
    QTimer resetTimer;

    QDateTime m_startTime;

    quint16 m_productID;
    quint16 m_vendorID;

    bool m_streaming;
    bool m_importing;
    bool have_perfindex;

};

#endif // SERIALOXIMETER_H
