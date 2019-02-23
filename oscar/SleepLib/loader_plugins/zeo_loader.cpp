/* SleepLib ZEO Loader Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the zeo_data_version in zel_loader.h when making changes to this loader
// that change loader behaviour or modify channels.
//********************************************************************************************

#include <QDir>
#include <QTextStream>
#include "zeo_loader.h"
#include "SleepLib/machine.h"

ZEOLoader::ZEOLoader()
{
    m_type = MT_SLEEPSTAGE;
}

ZEOLoader::~ZEOLoader()
{
}

int ZEOLoader::Open(const QString & dirpath)
{
    QString newpath;

    QString dirtag = "zeo";

    // Could Scan the ZEO folder for a list of CSVs

    QString path(dirpath);
    path = path.replace("\\", "/");

    if (path.toLower().endsWith("/" + dirtag)) {
        return 0;
        //newpath=path;
    } else {
        newpath = path + "/" + dirtag.toUpper();
    }

    //QString filename;

    // ZEO folder structure detection stuff here.

    return 0; // number of machines affected
}

/*15233: "Sleep Date"
15234: "ZQ"
15236: "Total Z"
15237: "Time to Z"
15237: "Time in Wake"
15238: "Time in REM"
15238: "Time in Light"
15241: "Time in Deep"
15242: "Awakenings"
15245: "Start of Night"
15246: "End of Night"
15246: "Rise Time"
15247: "Alarm Reason"
15247: "Snooze Time"
15254: "Wake Tone"
15259: "Wake Window"
15259: "Alarm Type"
15260: "First Alarm Ring"
15261: "Last Alarm Ring"
15261: "First Snooze Time"
15265: "Last Snooze Time"
15266: "Set Alarm Time"
15266: "Morning Feel"
15267: "Sleep Graph"
15267: "Detailed Sleep Graph"
15268: "Firmware Version"  */

int ZEOLoader::OpenFile(const QString & filename)
{
    QFile file(filename);

    if (filename.toLower().endsWith(".csv")) {
        if (!file.open(QFile::ReadOnly)) {
            qDebug() << "Couldn't open zeo file" << filename;
            return 0;
        }
    } else {// if (filename.toLower().endsWith(".dat")) {

        return 0;
        // not supported.
    }

    QTextStream text(&file);
    QString headerdata = text.readLine();
    QStringList header = headerdata.split(",");
    QString line;
    QStringList linecomp;
    QDateTime start_of_night, end_of_night, rise_time;
    SessionID sid;

    //const qint64 WindowSize=30000;
    qint64 st, tt;
    int stage;

    int ZQ, TimeToZ, TimeInWake, TimeInREM, TimeInLight, TimeInDeep, Awakenings;

    //int AlarmReason, SnoozeTime, WakeTone, WakeWindow, AlarmType, TotalZ;
    int MorningFeel;
    QString FirmwareVersion, MyZeoVersion;

    QDateTime FirstAlarmRing, LastAlarmRing, FirstSnoozeTime, LastSnoozeTime, SetAlarmTime;

    QStringList SG, DSG;

    MachineInfo info = newInfo();
    Machine *mach = p_profile->CreateMachine(info);


    int idxZQ = header.indexOf("ZQ");
    //int idxTotalZ = header.indexOf("Total Z");
    int idxAwakenings = header.indexOf("Awakenings");
    int idxSG = header.indexOf("Sleep Graph");
    int idxDSG = header.indexOf("Detailed Sleep Graph");
    int idxTimeInWake = header.indexOf("Time in Wake");
    int idxTimeToZ = header.indexOf("Time to Z");
    int idxTimeInREM = header.indexOf("Time in REM");
    int idxTimeInLight = header.indexOf("Time in Light");
    int idxTimeInDeep = header.indexOf("Time in Deep");
    int idxStartOfNight = header.indexOf("Start of Night");
    int idxEndOfNight = header.indexOf("End of Night");
    int idxRiseTime = header.indexOf("Rise Time");
//    int idxAlarmReason = header.indexOf("Alarm Reason");
//    int idxSnoozeTime = header.indexOf("Snooze Time");
//    int idxWakeTone = header.indexOf("Wake Tone");
//    int idxWakeWindow = header.indexOf("Wake Window");
//    int idxAlarmType = header.indexOf("Alarm Type");
    int idxFirstAlaramRing = header.indexOf("First Alarm Ring");
    int idxLastAlaramRing = header.indexOf("Last Alarm Ring");
    int idxFirstSnoozeTime = header.indexOf("First Snooze Time");
    int idxLastSnoozeTime = header.indexOf("Last Snooze Time");
    int idxSetAlarmTime = header.indexOf("Set Alarm Time");
    int idxMorningFeel = header.indexOf("Morning Feel");
    int idxFirmwareVersion = header.indexOf("Firmware Version");
    int idxMyZEOVersion = header.indexOf("My ZEO Version");

    bool ok;
    bool dodgy;

    do {
        line = text.readLine();
        dodgy = false;

        if (line.isEmpty()) { continue; }

        linecomp = line.split(",");
        ZQ = linecomp[idxZQ].toInt(&ok);

        if (!ok) { dodgy = true; }

//        TotalZ = linecomp[idxTotalZ].toInt(&ok);

//        if (!ok) { dodgy = true; }

        TimeToZ = linecomp[idxTimeToZ].toInt(&ok);

        if (!ok) { dodgy = true; }

        TimeInWake = linecomp[idxTimeInWake].toInt(&ok);

        if (!ok) { dodgy = true; }

        TimeInREM = linecomp[idxTimeInREM].toInt(&ok);

        if (!ok) { dodgy = true; }

        TimeInLight = linecomp[idxTimeInLight].toInt(&ok);

        if (!ok) { dodgy = true; }

        TimeInDeep = linecomp[idxTimeInDeep].toInt(&ok);

        if (!ok) { dodgy = true; }

        Awakenings = linecomp[idxAwakenings].toInt(&ok);

        if (!ok) { dodgy = true; }

        start_of_night = QDateTime::fromString(linecomp[idxStartOfNight], "MM/dd/yyyy HH:mm");

        if (!start_of_night.isValid()) { dodgy = true; }

        end_of_night = QDateTime::fromString(linecomp[idxEndOfNight], "MM/dd/yyyy HH:mm");

        if (!end_of_night.isValid()) { dodgy = true; }

        rise_time = QDateTime::fromString(linecomp[idxRiseTime], "MM/dd/yyyy HH:mm");

        if (!rise_time.isValid()) { dodgy = true; }

//        AlarmReason = linecomp[idxAlarmReason].toInt(&ok);

//        if (!ok) { dodgy = true; }

//        SnoozeTime = linecomp[idxSnoozeTime].toInt(&ok);

//        if (!ok) { dodgy = true; }

//        WakeTone = linecomp[idxWakeTone].toInt(&ok);

//        if (!ok) { dodgy = true; }

//        WakeWindow = linecomp[idxWakeWindow].toInt(&ok);

//        if (!ok) { dodgy = true; }

//        AlarmType = linecomp[idxAlarmType].toInt(&ok);

//        if (!ok) { dodgy = true; }

        if (!linecomp[idxFirstAlaramRing].isEmpty()) {
            FirstAlarmRing = QDateTime::fromString(linecomp[idxFirstAlaramRing], "MM/dd/yyyy HH:mm");

            if (!FirstAlarmRing.isValid()) { dodgy = true; }
        }

        if (!linecomp[idxLastAlaramRing].isEmpty()) {
            LastAlarmRing = QDateTime::fromString(linecomp[idxLastAlaramRing], "MM/dd/yyyy HH:mm");

            if (!LastAlarmRing.isValid()) { dodgy = true; }
        }

        if (!linecomp[idxFirstSnoozeTime].isEmpty()) {
            FirstSnoozeTime = QDateTime::fromString(linecomp[idxFirstSnoozeTime], "MM/dd/yyyy HH:mm");

            if (!FirstSnoozeTime.isValid()) { dodgy = true; }
        }

        if (!linecomp[idxLastSnoozeTime].isEmpty()) {
            LastSnoozeTime = QDateTime::fromString(linecomp[idxLastSnoozeTime], "MM/dd/yyyy HH:mm");

            if (!LastSnoozeTime.isValid()) { dodgy = true; }
        }

        if (!linecomp[idxSetAlarmTime].isEmpty()) {
            SetAlarmTime = QDateTime::fromString(linecomp[idxSetAlarmTime], "MM/dd/yyyy HH:mm");

            if (!SetAlarmTime.isValid()) { dodgy = true; }
        }

        MorningFeel = linecomp[idxMorningFeel].toInt(&ok);

        if (!ok) { MorningFeel = 0; }

        FirmwareVersion = linecomp[idxFirmwareVersion];

        if (idxMyZEOVersion >= 0) { MyZeoVersion = linecomp[idxMyZEOVersion]; }

        if (dodgy) {
            continue;
        }

        SG = linecomp[idxSG].split(" ");
        DSG = linecomp[idxDSG].split(" ");

        const int WindowSize = 30000;
        sid = start_of_night.toTime_t();

        if (DSG.size() == 0) {
            continue;
        }

        if (mach->SessionExists(sid)) {
            continue;
        }

        Session *sess = new Session(mach, sid);

        sess->settings[ZEO_Awakenings] = Awakenings;
        sess->settings[ZEO_MorningFeel] = MorningFeel;
        sess->settings[ZEO_TimeToZ] = TimeToZ;
        sess->settings[ZEO_ZQ] = ZQ;
        sess->settings[ZEO_TimeInWake] = TimeInWake;
        sess->settings[ZEO_TimeInREM] = TimeInREM;
        sess->settings[ZEO_TimeInLight] = TimeInLight;
        sess->settings[ZEO_TimeInDeep] = TimeInDeep;

        st = qint64(start_of_night.toTime_t()) * 1000L;
        sess->really_set_first(st);
        tt = st;
        EventList *sleepstage = sess->AddEventList(ZEO_SleepStage, EVL_Event, 1, 0, 0, 4);

        for (int i = 0; i < DSG.size(); i++) {
            stage = DSG[i].toInt(&ok);

            if (ok) {
                sleepstage->AddEvent(tt, stage);
            }

            tt += WindowSize;
        }

        sess->really_set_last(tt);
        int size = DSG.size();
        sess->SetChanged(true);
        mach->AddSession(sess);


        qDebug() << linecomp[0] << start_of_night << end_of_night << rise_time << size <<
                 "30 second chunks";

    } while (!line.isNull());

    mach->Save();
    return true;
}

static bool zeo_initialized = false;

void ZEOLoader::Register()
{
    if (zeo_initialized) { return; }

    qDebug("Registering ZEOLoader");
    RegisterLoader(new ZEOLoader());
    //InitModelMap();
    zeo_initialized = true;
}

