/* SleepLib (DeVilbiss) Intellipap Loader Implementation
 *
 * Notes: Intellipap DV54 requires the SmartLink attachment to access this data.
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDir>

#include "intellipap_loader.h"

ChannelID INTP_SmartFlexMode, INTP_SmartFlexLevel;

Intellipap::Intellipap(Profile *profile, MachineID id)
    : CPAP(profile, id)
{
}

Intellipap::~Intellipap()
{
}

IntellipapLoader::IntellipapLoader()
{
    const QString INTELLIPAP_ICON = ":/icons/intellipap.png";
    const QString DV6_ICON = ":/icons/dv64.png";

    QString s = newInfo().series;
    m_pixmap_paths[s] = INTELLIPAP_ICON;
    m_pixmaps[s] = QPixmap(INTELLIPAP_ICON);
    m_pixmap_paths["DV6"] = DV6_ICON;
    m_pixmaps["DV6"] = QPixmap(DV6_ICON);

    m_buffer = nullptr;
    m_type = MT_CPAP;
}

IntellipapLoader::~IntellipapLoader()
{
}

const QString SET_BIN = "SET.BIN";
const QString SET1 = "SET1";
const QString DV6 = "DV6";
const QString SL = "SL";

const QString DV6_DIR = "/" + DV6;
const QString SL_DIR = "/" + SL;

bool IntellipapLoader::Detect(const QString & givenpath)
{
    QString path = givenpath;
    if (path.endsWith(SL_DIR)) {
        path.chop(3);
    }
    if (path.endsWith(DV6_DIR)) {
        path.chop(4);
    }

    QDir dir(path);

    if (!dir.exists()) {
        return false;
    }

    // Intellipap DV54 has a folder called SL in the root directory, DV64 has DV6
    if (dir.cd(SL)) {
        // Test for presence of settings file
        return dir.exists(SET1) ? true : false;
    }

    if (dir.cd(DV6)) { // DV64
        return dir.exists(SET_BIN) ? true : false;
    }
    return false;
}

enum INTPAP_Type { INTPAP_Unknown, INTPAP_DV5, INTPAP_DV6 };


int IntellipapLoader::OpenDV5(const QString & path)
{
    QString newpath = path + SL_DIR;
    QString filename;


    //////////////////////////
    // Parse the Settings File
    //////////////////////////
    filename = newpath + "/" + SET1;
    QFile f(filename);

    if (!f.exists()) {
        return -1;
    }

    f.open(QFile::ReadOnly);
    QTextStream tstream(&f);

    const QString INT_PROP_Serial = "Serial";
    const QString INT_PROP_Model = "Model";
    const QString INT_PROP_Mode = "Mode";
    const QString INT_PROP_MaxPressure = "Max Pressure";
    const QString INT_PROP_MinPressure = "Min Pressure";
    const QString INT_PROP_IPAP = "IPAP";
    const QString INT_PROP_EPAP = "EPAP";
    const QString INT_PROP_PS = "PS";
    const QString INT_PROP_RampPressure = "Ramp Pressure";
    const QString INT_PROP_RampTime = "Ramp Time";

    const QString INT_PROP_HourMeter = "Usage Hours";
    const QString INT_PROP_ComplianceMeter = "Compliance Hours";
    const QString INT_PROP_ErrorCode = "Error";
    const QString INT_PROP_LastErrorCode = "Long Error";
    const QString INT_PROP_LowUseThreshold = "Low Usage";
    const QString INT_PROP_SmartFlex = "SmartFlex";
    const QString INT_PROP_SmartFlexMode = "SmartFlexMode";


    QHash<QString, QString> lookup;
        lookup["Sn"] = INT_PROP_Serial;
        lookup["Mn"] = INT_PROP_Model;
        lookup["Mo"] = INT_PROP_Mode;     // 0 cpap, 1 auto
        //lookup["Pn"]="??";
        lookup["Pu"] = INT_PROP_MaxPressure;
        lookup["Pl"] = INT_PROP_MinPressure;
        lookup["Pi"] = INT_PROP_IPAP;
        lookup["Pe"] = INT_PROP_EPAP;  // == WF on Auto models
        lookup["Ps"] = INT_PROP_PS;             // == WF on Auto models, Pressure support
        //lookup["Ds"]="??";
        //lookup["Pc"]="??";
        lookup["Pd"] = INT_PROP_RampPressure;
        lookup["Dt"] = INT_PROP_RampTime;
        //lookup["Ld"]="??";
        //lookup["Lh"]="??";
        //lookup["FC"]="??";
        //lookup["FE"]="??";
        //lookup["FL"]="??";
        lookup["A%"]="ApneaThreshold";
        lookup["Ad"]="ApneaDuration";
        lookup["H%"]="HypopneaThreshold";
        lookup["Hd"]="HypopneaDuration";
        //lookup["Pi"]="??";
        //lookup["Pe"]="??";
        lookup["Ri"]="SmartFlexIRnd";  // Inhale Rounding (0-5)
        lookup["Re"]="SmartFlexERnd"; // Inhale Rounding (0-5)
        //lookup["Bu"]="??"; // WF
        //lookup["Ie"]="??"; // 20
        //lookup["Se"]="??"; // 05    //Inspiratory trigger?
        //lookup["Si"]="??"; // 05    // Expiratory Trigger?
        //lookup["Mi"]="??"; // 0
        lookup["Uh"]="HoursMeter"; // 0000.0
        lookup["Up"]="ComplianceMeter"; // 0000.00
        //lookup["Er"]="ErrorCode";, // E00
        //lookup["El"]="LongErrorCode"; // E00 00/00/0000
        //lookup["Hp"]="??";, // 1
        //lookup["Hs"]="??";, // 02
        //lookup["Lu"]="LowUseThreshold"; // defaults to 0 (4 hours)
        lookup["Sf"] = INT_PROP_SmartFlex;
        lookup["Sm"] = INT_PROP_SmartFlexMode;
        lookup["Ks=s"]="Ks_s";
        lookup["Ks=i"]="ks_i";

    QHash<QString, QString> set1;
    QHash<QString, QString>::iterator hi;

    Machine *mach = nullptr;

    MachineInfo info = newInfo();


    bool ok;

    //EventDataType min_pressure = 0, max_pressure = 0, set_ipap = 0, set_ps = 0,
    EventDataType ramp_pressure = 0, set_epap = 0, ramp_time = 0;

    int papmode = 0, smartflex = 0, smartflexmode = 0;
    while (1) {
        QString line = tstream.readLine();

        if ((line.length() <= 2) ||
                (line.isNull())) { break; }

        QString key = line.section("\t", 0, 0).trimmed();
        hi = lookup.find(key);

        if (hi != lookup.end()) {
            key = hi.value();
        }

        QString value = line.section("\t", 1).trimmed();

        if (key == INT_PROP_Mode) {
            papmode = value.toInt(&ok);
        } else if (key == INT_PROP_Serial) {
            info.serial = value;
        } else if (key == INT_PROP_Model) {
            info.model = value;
        } else if (key == INT_PROP_MinPressure) {
            //min_pressure = value.toFloat() / 10.0;
        } else if (key == INT_PROP_MaxPressure) {
            //max_pressure = value.toFloat() / 10.0;
        } else if (key == INT_PROP_IPAP) {
            //set_ipap = value.toFloat() / 10.0;
        } else if (key == INT_PROP_EPAP) {
            set_epap = value.toFloat() / 10.0;
        } else if (key == INT_PROP_PS) {
            //set_ps = value.toFloat() / 10.0;
        } else if (key == INT_PROP_RampPressure) {
            ramp_pressure = value.toFloat() / 10.0;
        } else if (key == INT_PROP_RampTime) {
            ramp_time = value.toFloat() / 10.0;
        } else if (key == INT_PROP_SmartFlex) {
            smartflex = value.toInt();
        } else if (key == INT_PROP_SmartFlexMode) {
            smartflexmode = value.toInt();
        } else {
            set1[key] = value;
        }
        qDebug() << key << "=" << value;
    }

    CPAPMode mode = MODE_UNKNOWN;

    switch (papmode) {
    case 0:
        mode = MODE_CPAP;
        break;
    case 1:
        mode = (set_epap > 0) ? MODE_BILEVEL_FIXED : MODE_APAP;
        break;
    default:
        qDebug() << "New machine mode";
    }

    if (!info.serial.isEmpty()) {
        mach = p_profile->CreateMachine(info);
    }

    if (!mach) {
        qDebug() << "Couldn't get Intellipap machine record";
        return -1;
    }

    QString backupPath = mach->getBackupPath();
    QString copypath = path;

    if (QDir::cleanPath(path).compare(QDir::cleanPath(backupPath)) != 0) {
        copyPath(path, backupPath);
    }


    // Refresh properties data..
    for (QHash<QString, QString>::iterator i = set1.begin(); i != set1.end(); i++) {
        mach->properties[i.key()] = i.value();
    }

    f.close();

    ///////////////////////////////////////////////
    // Parse the Session Index (U File)
    ///////////////////////////////////////////////
    unsigned char buf[27];
    filename = newpath + "/U";
    f.setFileName(filename);

    if (!f.exists()) { return -1; }

    QVector<quint32> SessionStart;
    QVector<quint32> SessionEnd;
    QHash<SessionID, Session *> Sessions;

    quint32 ts1, ts2;//, length;
    //unsigned char cs;
    f.open(QFile::ReadOnly);
    int cnt = 0;
    QDateTime epoch(QDate(2002, 1, 1), QTime(0, 0, 0), Qt::UTC); // Intellipap Epoch
    int ep = epoch.toTime_t();

    do {
        cnt = f.read((char *)buf, 9);
        // big endian
        ts1 = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
        ts2 = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
        // buf[8] == ??? What is this byte? A Bit Field? A checksum?
        ts1 += ep;
        ts2 += ep;
        SessionStart.append(ts1);
        SessionEnd.append(ts2);
    } while (cnt > 0);

    qDebug() << "U file logs" << SessionStart.size() << "sessions.";
    f.close();

    ///////////////////////////////////////////////
    // Parse the Session Data (L File)
    ///////////////////////////////////////////////
    filename = newpath + "/L";
    f.setFileName(filename);

    if (!f.exists()) { return -1; }

    f.open(QFile::ReadOnly);
    long size = f.size();
    int recs = size / 26;
    m_buffer = new unsigned char [size];

    if (size != f.read((char *)m_buffer, size)) {
        qDebug()  << "Couldn't read 'L' data" << filename;
        return -1;
    }

    Session *sess;
    SessionID sid;
    QHash<SessionID,qint64> rampstart;
    QHash<SessionID,qint64> rampend;

    for (int i = 0; i < SessionStart.size(); i++) {
        sid = SessionStart[i];

        if (mach->SessionExists(sid)) {
            // knock out the already imported sessions..
            SessionStart[i] = 0;
            SessionEnd[i] = 0;
        } else if (!Sessions.contains(sid)) {
            sess = Sessions[sid] = new Session(mach, sid);

            sess->really_set_first(qint64(sid) * 1000L);
         //   sess->really_set_last(qint64(SessionEnd[i]) * 1000L);

            rampstart[sid] = 0;
            rampend[sid] = 0;
            sess->SetChanged(true);
            if (mode >= MODE_BILEVEL_FIXED) {
                sess->AddEventList(CPAP_IPAP, EVL_Event);
                sess->AddEventList(CPAP_EPAP, EVL_Event);
                sess->AddEventList(CPAP_PS, EVL_Event);
            } else {
                sess->AddEventList(CPAP_Pressure, EVL_Event);
            }

            sess->AddEventList(INTELLIPAP_Unknown1, EVL_Event);
            sess->AddEventList(INTELLIPAP_Unknown2, EVL_Event);

            sess->AddEventList(CPAP_LeakTotal, EVL_Event);
            sess->AddEventList(CPAP_MaxLeak, EVL_Event);
            sess->AddEventList(CPAP_TidalVolume, EVL_Event);
            sess->AddEventList(CPAP_MinuteVent, EVL_Event);
            sess->AddEventList(CPAP_RespRate, EVL_Event);
            sess->AddEventList(CPAP_Snore, EVL_Event);

            sess->AddEventList(CPAP_Obstructive, EVL_Event);
            sess->AddEventList(CPAP_VSnore, EVL_Event);
            sess->AddEventList(CPAP_Hypopnea, EVL_Event);
            sess->AddEventList(CPAP_NRI, EVL_Event);
            sess->AddEventList(CPAP_LeakFlag, EVL_Event);
            sess->AddEventList(CPAP_ExP, EVL_Event);


        } else {
            // If there is a double up, null out the earlier session
            // otherwise there will be a crash on shutdown.
            for (int z = 0; z < SessionStart.size(); z++) {
                if (SessionStart[z] == (quint32)sid) {
                    SessionStart[z] = 0;
                    SessionEnd[z] = 0;
                    break;
                }
            }

            QDateTime d = QDateTime::fromTime_t(sid);
            qDebug() << sid << "has double ups" << d;
            /*Session *sess=Sessions[sid];
            Sessions.erase(Sessions.find(sid));
            delete sess;
            SessionStart[i]=0;
            SessionEnd[i]=0; */
        }
    }

    long pos = 0;
    int rampval = 0;
    sid = 0;
    //SessionID lastsid = 0;

    //int last_minp=0, last_maxp=0, last_ps=0, last_pres = 0;

    for (int i = 0; i < recs; i++) {
        // convert timestamp to real epoch
        ts1 = ((m_buffer[pos] << 24) | (m_buffer[pos + 1] << 16) | (m_buffer[pos + 2] << 8) | m_buffer[pos + 3]) + ep;

        for (int j = 0; j < SessionStart.size(); j++) {
            sid = SessionStart[j];

            if (!sid) { continue; }

            if ((ts1 >= (quint32)sid) && (ts1 <= SessionEnd[j])) {
                Session *sess = Sessions[sid];

                qint64 time = quint64(ts1) * 1000L;
                sess->really_set_last(time);
                sess->settings[CPAP_Mode] = mode;

                int minp = m_buffer[pos + 0x13];
                int maxp = m_buffer[pos + 0x14];
                int ps = m_buffer[pos + 0x15];
                int pres = m_buffer[pos + 0xd];

                if (mode >= MODE_BILEVEL_FIXED) {
                    rampval = maxp;
                } else {
                    rampval = minp;
                }

                qint64 rs = rampstart[sid];

                if (pres < rampval) {
                    if (!rs) {
                        // ramp started


//                        int rv = pres-rampval;
//                        double ramp =

                        rampstart[sid] = time;
                    }
                    rampend[sid] = time;
                } else {
                    if (rs > 0) {
                        if (!sess->eventlist.contains(CPAP_Ramp)) {
                            sess->AddEventList(CPAP_Ramp, EVL_Event);
                        }
                        int duration = (time - rs) / 1000L;
                        sess->eventlist[CPAP_Ramp][0]->AddEvent(time, duration);

                        rampstart.remove(sid);
                        rampend.remove(sid);
                    }
                }


                // Do this after ramp, because ramp calcs might need to insert interpolated pressure samples
                if (mode >= MODE_BILEVEL_FIXED) {

                    sess->settings[CPAP_EPAP] = float(minp) / 10.0;
                    sess->settings[CPAP_IPAP] = float(maxp) / 10.0;

                    sess->settings[CPAP_PS] = float(ps) / 10.0;


                    sess->eventlist[CPAP_IPAP][0]->AddEvent(time, float(pres) / 10.0);
                    sess->eventlist[CPAP_EPAP][0]->AddEvent(time, float(pres-ps) / 10.0);
//                   rampval = maxp;

                } else {
                    sess->eventlist[CPAP_Pressure][0]->AddEvent(time, float(pres) / 10.0); // current pressure
//                    rampval = minp;

                    if (mode == MODE_APAP) {
                        sess->settings[CPAP_PressureMin] = float(minp) / 10.0;
                        sess->settings[CPAP_PressureMax] = float(maxp) / 10.0;
                    } else if (mode == MODE_CPAP) {
                        sess->settings[CPAP_Pressure] = float(maxp) / 10.0;
                    }
                }


                sess->eventlist[CPAP_LeakTotal][0]->AddEvent(time, m_buffer[pos + 0x7]); // "Average Leak"
                sess->eventlist[CPAP_MaxLeak][0]->AddEvent(time, m_buffer[pos + 0x6]); // "Max Leak"

                int rr = m_buffer[pos + 0xa];
                sess->eventlist[CPAP_RespRate][0]->AddEvent(time, rr); // Respiratory Rate
               // sess->eventlist[INTELLIPAP_Unknown1][0]->AddEvent(time, m_buffer[pos + 0xf]); //
                sess->eventlist[INTELLIPAP_Unknown1][0]->AddEvent(time, m_buffer[pos + 0xc]);

                sess->eventlist[CPAP_Snore][0]->AddEvent(time, m_buffer[pos + 0x4]); //4/5??

                if (m_buffer[pos+0x4] > 0) {
                    sess->eventlist[CPAP_VSnore][0]->AddEvent(time, m_buffer[pos + 0x5]);
                }

                // 0x0f == Leak Event
                // 0x04 == Snore?
                if (m_buffer[pos + 0xf] > 0) { // Leak Event
                    sess->eventlist[CPAP_LeakFlag][0]->AddEvent(time, m_buffer[pos + 0xf]);
                }

                if (m_buffer[pos + 0x5] > 4) { // This matches Exhale Puff.. not sure why 4
                    //MW: Are the lower 2 bits something else?

                    sess->eventlist[CPAP_ExP][0]->AddEvent(time, m_buffer[pos + 0x5]);
                }

                if (m_buffer[pos + 0x10] > 0) {
                    sess->eventlist[CPAP_Obstructive][0]->AddEvent(time, m_buffer[pos + 0x10]);
                }

                if (m_buffer[pos + 0x11] > 0) {
                    sess->eventlist[CPAP_Hypopnea][0]->AddEvent(time, m_buffer[pos + 0x11]);
                }

                if (m_buffer[pos + 0x12] > 0) { // NRI // is this == to RERA?? CA??
                    sess->eventlist[CPAP_NRI][0]->AddEvent(time, m_buffer[pos + 0x12]);
                }

                quint16 tv = (m_buffer[pos + 0x8] << 8) | m_buffer[pos + 0x9]; // correct

                sess->eventlist[CPAP_TidalVolume][0]->AddEvent(time, tv);

                EventDataType mv = tv * rr; // MinuteVent=TidalVolume * Respiratory Rate
                sess->eventlist[CPAP_MinuteVent][0]->AddEvent(time, mv / 1000.0);
                break;
            } else {
            }
            //lastsid = sid;
        }

        pos += 26;
    }

    // Close any open ramps and store the event.
    QHash<SessionID,qint64>::iterator rit;
    QHash<SessionID,qint64>::iterator rit_end = rampstart.end();

    for (rit = rampstart.begin(); rit != rit_end; ++rit) {
        qint64 rs = rit.value();
        SessionID sid = rit.key();
        if (rs > 0) {
            qint64 re = rampend[rit.key()];

            Session *sess = Sessions[sid];
            if (!sess->eventlist.contains(CPAP_Ramp)) {
                sess->AddEventList(CPAP_Ramp, EVL_Event);
            }

            int duration = (re - rs) / 1000L;
            sess->eventlist[CPAP_Ramp][0]->AddEvent(re, duration);
            rit.value() = 0;

        }
    }

    for (int i = 0; i < SessionStart.size(); i++) {
        SessionID sid = SessionStart[i];

        if (sid) {
            sess = Sessions[sid];

            if (!sess) continue;

//            quint64 first = qint64(sid) * 1000L;
            //quint64 last = qint64(SessionEnd[i]) * 1000L;

            if (sess->last() > 0) {
             //   sess->really_set_last(last);


                sess->settings[INTP_SmartFlexLevel] = smartflex;

                if (smartflexmode == 0) {
                    sess->settings[INTP_SmartFlexMode] = PM_FullTime;
                } else {
                    sess->settings[INTP_SmartFlexMode] = PM_RampOnly;
                }

                sess->settings[CPAP_RampPressure] = ramp_pressure;
                sess->settings[CPAP_RampTime] = ramp_time;

                sess->UpdateSummaries();

                addSession(sess);
            } else {
                delete sess;
            }

        }
    }

    finishAddingSessions();
    mach->Save();


    delete [] m_buffer;

    f.close();

    int c = Sessions.size();
    return c;
}

struct DV6_S_Record
{
    Session * sess;
    unsigned char u1;            //00 (position)
    unsigned int start_time;     //01
    unsigned int stop_time;      //05
    unsigned int atpressure_time;//09
    EventDataType hours;         //13
    EventDataType meh;           //14
    EventDataType pressureAvg;   //15
    EventDataType pressureMax;   //16
    EventDataType pressure50;    //17 50th percentile
    EventDataType pressure90;    //18 90th percentile
    EventDataType pressure95;    //19 95th percentile
    EventDataType pressureStdDev;//20 std deviation
    EventDataType u2;            //21
    EventDataType leakAvg;       //22
    EventDataType leakMax;       //23
    EventDataType leak50;        //24 50th percentile
    EventDataType leak90;        //25 90th percentile
    EventDataType leak95;        //26 95th percentile
    EventDataType leakStdDev;    //27 std deviation
    EventDataType tidalVolume;   //28 & 0x29
    EventDataType avgBreathRate; //30
    EventDataType u3;
    EventDataType u4;            //32 snores / hypopnea per minute
    EventDataType timeInExPuf;   //33 Time in Expiratory Puff
    EventDataType timeInFL;      //34 Time in Flow Limitation
    EventDataType timeInPB;      //35 Time in Periodic Breathing
    EventDataType maskFit;       //36 mask fit (or rather, not fit) percentage
    EventDataType indexOA;       //37 Obstructive
    EventDataType indexCA;       //38 Central index
    EventDataType indexHyp;      //39 Hypopnea Index
    EventDataType r0;            //40 Reserved?
    EventDataType r1;            //41 Reserved?
                                 //42-48 unknown
    EventDataType pressureSetMin;   //49
    EventDataType pressureSetMax;   //50

    bool hasMaskPressure;
};

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#else
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif


PACK(struct SET_BIN_REC {
    char unknown_00;                        // assuming file version
    char serial[11];                        // null terminated
    unsigned short cap_flags;               // capability flags
    unsigned short cpap_pressure;
    unsigned short max_pressure;
    unsigned short min_pressure;
    unsigned char alg_apnea_threshhold;     // always locked at 00
    unsigned char alg_apnea_duration;
    unsigned char alg_hypop_threshold;
    unsigned char alg_hypop_duration;
    unsigned short ramp_pressure;
    unsigned short ramp_duration;
    unsigned char unknown_01[3];
    unsigned char smartflex;
    unsigned char unknown_02;
    unsigned char inspFlowRounding;
    unsigned char expFlowRounding;
    unsigned char complianceHours;
    unsigned char unknown_03[9];
    unsigned char humidifier_setting;  // 0-5
    unsigned char unused[83];
    unsigned char checksum;
}); // http://digitalvampire.org/blog/index.php/2006/07/31/why-you-shouldnt-use-__attribute__packed/

int IntellipapLoader::OpenDV6(const QString & path)
{
    QString newpath = path + DV6_DIR;

    // Prime the machine database's info field with stuff relevant to this machine
    MachineInfo info = newInfo();
    info.series = "DV6";
    info.serial = "Unknown";

    int vmin=0, vmaj=0;
    EventDataType max_pressure=0, min_pressure=0; //, starting_pressure;

    QByteArray str, dataBA;
    unsigned char *data = NULL;
    /////////////////////////////////////////////////////////////////////////////////
    // Parse SET.BIN settings file
    /////////////////////////////////////////////////////////////////////////////////
    QFile f(newpath+"/"+SET_BIN);
    if (f.open(QIODevice::ReadOnly)) {
        // Read and parse entire SET.BIN file
        dataBA = f.readAll();
        f.close();

        SET_BIN_REC *setbin = (SET_BIN_REC *)dataBA.data();
        info.serial = QString(setbin->serial);
        max_pressure = setbin->max_pressure;
        min_pressure = setbin->min_pressure;

    } else { // if f.open settings file
        // Settings file open failed, return
        return -1;
    }

    ////////////////////////////////////////////////////////////////////////////////////////
    // Parser VER.BIN for model number
    ////////////////////////////////////////////////////////////////////////////////////////
    f.setFileName(newpath+"/VER.BIN");
    if (f.open(QIODevice::ReadOnly)) {
        dataBA = f.readAll();
        f.close();

        int cnt = 0;
        data = (unsigned char *)dataBA.data();
        for (int i=0; i< dataBA.size(); ++i) { // deliberately going one further to catch end condition
            if ((dataBA.at(i) == 0) || (i >= dataBA.size()-1)) { // if null terminated or last byte

                switch(cnt) {
                case 1: // serial
                    break;
                case 2: // model
                    info.model = str;
                    break;
                case 7: // ??? V025RN20170
                    break;
                case 9: // ??? V014BL20150630
                    break;
                case 11: // ??? 01 09
                    break;
                case 12: // ??? 0C 0C
                    break;
                case 14: // ??? BA 0C
                    break;
                default:
                    break;

                }

                // Clear and start a new data record
                str.clear();
                cnt++;
            } else {
                // Add the character to the current string
                str.append(dataBA[i]);
            }
        }

    } else { // if (f.open(...)
        // VER.BIN open failed
        return -1;
    }


    ////////////////////////////////////////////////////////////////////////////////////////
    // Creates Machine database record if it doesn't exist already
    ////////////////////////////////////////////////////////////////////////////////////////
    Machine *mach = p_profile->CreateMachine(info);
    if (mach == nullptr) {
        return -1;
    }
    qDebug() << "Opening DV6 (" << info.serial << ")" << "v" << vmaj << "." << vmin << "Min:" << min_pressure << "Max:" << max_pressure;


    ////////////////////////////////////////////////////////////////////////////////////////
    // Open and parse session list and create a list of sessions to import
    ////////////////////////////////////////////////////////////////////////////////////////


    const int DV6_L_RecLength = 45;
    const int DV6_E_RecLength = 25;
    const int DV6_S_RecLength = 55;
    unsigned int ts1,ts2, lastts1;

    QMap<quint32, DV6_S_Record> summaryList;  // QHash is faster, but QMap keeps order

    QDateTime epoch(QDate(2002, 1, 1), QTime(0, 0, 0), Qt::UTC); // Intellipap Epoch
    int ep = epoch.toTime_t();


    f.setFileName(newpath+"/S.BIN");
    if (f.open(QIODevice::ReadOnly)) {
        dataBA = f.readAll();
        f.close();

        data = (unsigned char *)dataBA.data();

        int records = dataBA.size() / DV6_S_RecLength;

        //data[0x11]; // Start of data block
        //data[0x12]; // Record count
        // First record is block header
        for (int r=1; r<records-1; r++) {
            data += DV6_S_RecLength; // just so happen the headers the same length, though we probably should parse it to get it's version
            DV6_S_Record R;

            ts1 = ((data[4] << 24) | (data[3] << 16) | (data[2] << 8) | data[1])+ep; // session start time
            ts2 = ((data[8] << 24) | (data[7] << 16) | (data[6] << 8) | data[5])+ep; // session end

            if (!mach->sessionlist.contains(ts1)) { // Check if already imported
                qDebug() << "Detected new Session" << ts1;
                R.sess = new Session(mach, ts1);
                R.sess->SetChanged(true);

                R.sess->really_set_first(qint64(ts1) * 1000L);
                R.sess->really_set_last(qint64(ts2) * 1000L);

                R.start_time = ts1;
                R.stop_time = ts2;

                R.atpressure_time = ((data[12] << 24) | (data[11] << 16) | (data[10] << 8) | data[9])+ep;
                R.hours = float(data[13]) / 10.0F;
                R.pressureSetMin = float(data[49]) / 10.0F;
                R.pressureSetMax = float(data[50]) / 10.0F;

                // The following stuff is not necessary to decode, but can be used to verify we are on the right track
                //data[14]... unknown
                R.pressureAvg = float(data[15]) / 10.0F;
                R.pressureMax = float(data[16]) / 10.0F;
                R.pressure50 = float(data[17]) / 10.0F;
                R.pressure90 = float(data[18]) / 10.0F;
                R.pressure95 = float(data[19]) / 10.0F;
                R.pressureStdDev = float(data[20]) / 10.0F;
                //data[21]... unknown
                R.leakAvg = float(data[22]) / 10.0F;
                R.leakMax = float(data[23]) / 10.0F;
                R.leak50= float(data[24]) / 10.0F;
                R.leak90 = float(data[25]) / 10.0F;
                R.leak95 = float(data[26]) / 10.0F;
                R.leakStdDev = float(data[27]) / 10.0F;

                R.tidalVolume = float(data[28] | data[29] << 8);
                R.avgBreathRate = float(data[30] | data[31] << 8);

                if (data[49] != data[50]) {
                    R.sess->settings[CPAP_PressureMin] = R.pressureSetMin;
                    R.sess->settings[CPAP_PressureMax] = R.pressureSetMax;
                    R.sess->settings[CPAP_Mode] = MODE_APAP;
                } else {
                    R.sess->settings[CPAP_Mode] = MODE_CPAP;
                    R.sess->settings[CPAP_Pressure] = R.pressureSetMin;
                }
                R.hasMaskPressure = false;
                summaryList[ts1] = R;
            }
        }

    } else { // if (f.open(...)
        // S.BIN open failed
        return -1;
    }


    QMap<quint32, DV6_S_Record>::iterator SR;
    const int DV6_R_RecLength = 117;
    const int DV6_R_HeaderSize = 55;
    f.setFileName(newpath+"/R.BIN");
    int numRrecs = (f.size()-DV6_R_HeaderSize) / DV6_R_RecLength;
    Session *sess = NULL;
    if (f.open(QIODevice::ReadOnly)) {
        // Let's not parse R all at once, it's huge
        dataBA = f.read(DV6_R_HeaderSize);
        if (dataBA.size() < DV6_R_HeaderSize) {
            // bit mean aborting on corrupt R file... but oh well
            return -1;
        }

        sess = NULL;
        EventList * flow = NULL;
        EventList * pressure = NULL;
//        EventList * leak = NULL;
        EventList * OA  = NULL;
        EventList * HY  = NULL;
        EventList * NOA = NULL;
        EventList * EXP = NULL;
        EventList * FL  = NULL;
        EventList * PB  = NULL;
        EventList * VS  = NULL;
        EventList * LL  = NULL;
        EventList * RE  = NULL;
        bool inOA = false, inH = false, inCA = false, inExP = false, inVS = false, inFL = false, inPB = false, inRE = false, inLL = false;
        qint64 OAstart = 0, OAend = 0;
        qint64 Hstart = 0, Hend = 0;
        qint64 CAstart = 0, CAend = 0;
        qint64 ExPstart = 0, ExPend = 0;
        qint64 VSstart = 0, VSend = 0;
        qint64 FLstart = 0, FLend = 0;
        qint64 PBstart = 0, PBend = 0;
        qint64 REstart =0, REend = 0;
        qint64 LLstart =0, LLend = 0;
        lastts1 = 0;

        SR = summaryList.begin();
        for (int r=0; r<numRrecs; data += DV6_R_RecLength, ++r) {
            dataBA=f.read(DV6_R_RecLength);
            data = (unsigned char *)dataBA.data();
            if (dataBA.size() < DV6_R_RecLength) {
                break;
            }

            DV6_S_Record *R = &SR.value();

            ts1 = ((data[4] << 24) | (data[3] << 16) | (data[2] << 8) | data[1]) + ep;

            if (flow && ((ts1 - lastts1) > 2)) {
                sess->set_last(flow->last());

                sess = nullptr;
                flow = nullptr;
                pressure = nullptr;
            }
            lastts1=ts1;

            while (ts1 > R->stop_time) {
                if (flow && sess) {
                    // update min and max
                    // then add to machine
                    sess = nullptr;
                    flow = nullptr;
                    pressure = nullptr;
                }
                SR++;
                if (SR == summaryList.end()) break;
                R = &SR.value();
            }
            if (SR == summaryList.end())
                break;

            if (ts1 >= R->start_time) {
                if (!flow && R->sess) {
                    flow = R->sess->AddEventList(CPAP_FlowRate, EVL_Waveform, 1.0f/60.0f, 0.0f, 0.0f, 0.0f, double(2000) / double(50));
                    pressure = R->sess->AddEventList(CPAP_Pressure, EVL_Waveform, 0.1f, 0.0f, 0.0f, 0.0f, double(2000) / double(2));
                    R->hasMaskPressure = true;
                    //leak = R->sess->AddEventList(CPAP_Leak, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, double(2000) / double(1));
                    OA = R->sess->AddEventList(CPAP_Obstructive, EVL_Event);
                    NOA = R->sess->AddEventList(CPAP_NRI, EVL_Event);
                    RE = R->sess->AddEventList(CPAP_RERA, EVL_Event);
                    VS = R->sess->AddEventList(CPAP_VSnore, EVL_Event);
                    HY = R->sess->AddEventList(CPAP_Hypopnea, EVL_Event);
                    EXP = R->sess->AddEventList(CPAP_ExP, EVL_Event);
                    FL = R->sess->AddEventList(CPAP_FlowLimit, EVL_Event);
                    PB = R->sess->AddEventList(CPAP_PB, EVL_Event);
                    LL = R->sess->AddEventList(CPAP_LargeLeak, EVL_Event);
                }
                if (flow) {
                    sess = R->sess;
                    // starting at position 5 is 100 bytes, 16bit LE 25hz samples
                    qint16 *wavedata = (qint16 *)(&data[5]);

                    qint64 ti = qint64(ts1) * 1000;

                    unsigned char d[2];
                    d[0] = data[105];
                    d[1] = data[106];
                    flow->AddWaveform(ti+40000,wavedata,50,2000);
                    pressure->AddWaveform(ti+40000, d, 2, 2000);
                    // Fields data[107] && data[108] are bitfields default is 0x90, occasionally 0x98

                    d[0] = data[107];
                    d[1] = data[108];

                    //leak->AddWaveform(ti+40000, d, 2, 2000);


                    // Needs to track state to pull events out cleanly..

                    //////////////////////////////////////////////////////////////////
                    // High Leak
                    //////////////////////////////////////////////////////////////////

                    if (data[110] & 3) {  // LL state 1st second
                        if (!inLL) {
                            LLstart = ti;
                            inLL = true;
                        }
                        LLend = ti+1000L;
                    } else {
                        if (inLL) {
                            inLL = false;
                            LL->AddEvent(LLstart,(LLend-LLstart) / 1000L);
                            LLstart = 0;
                        }
                    }
                    if (data[114] & 3) {
                        if (!inLL) {
                            LLstart = ti+1000L;
                            inLL = true;
                        }
                        LLend = ti+2000L;

                    } else {
                        if (inLL) {
                            inLL = false;
                            LL->AddEvent(LLstart,(LLend-LLstart) / 1000L);
                            LLstart = 0;
                        }
                    }


                    //////////////////////////////////////////////////////////////////
                    // Obstructive Apnea
                    //////////////////////////////////////////////////////////////////

                    if (data[110] & 12) {  // OA state 1st second
                        if (!inOA) {
                            OAstart = ti;
                            inOA = true;
                        }
                        OAend = ti+1000L;
                    } else {
                        if (inOA) {
                            inOA = false;
                            OA->AddEvent(OAstart,(OAend-OAstart) / 1000L);
                            OAstart = 0;
                        }
                    }
                    if (data[114] & 12) {
                        if (!inOA) {
                            OAstart = ti+1000L;
                            inOA = true;
                        }
                        OAend = ti+2000L;

                    } else {
                        if (inOA) {
                            inOA = false;
                            OA->AddEvent(OAstart,(OAend-OAstart) / 1000L);
                            OAstart = 0;
                        }
                    }


                    //////////////////////////////////////////////////////////////////
                    // Hypopnea
                    //////////////////////////////////////////////////////////////////

                    if (data[110] & 192) {
                        if (!inH) {
                            Hstart = ti;
                            inH = true;
                        }
                        Hend = ti + 1000L;
                    } else {
                        if (inH) {
                            inH = false;
                            HY->AddEvent(Hstart,(Hend-Hstart) / 1000L);
                            Hstart = 0;
                        }
                    }

                    if (data[114] & 192) {
                        if (!inH) {
                            Hstart = ti+1000L;
                            inH = true;
                        }
                        Hend = ti + 2000L;
                    } else {
                        if (inH) {
                            inH = false;
                            HY->AddEvent(Hstart,(Hend-Hstart) / 1000L);
                            Hstart = 0;
                        }
                    }

                    //////////////////////////////////////////////////////////////////
                    // Non Responding Apnea Event (Are these CA's???)
                    //////////////////////////////////////////////////////////////////
                    if (data[110] & 48) {  // OA state 1st second
                        if (!inCA) {
                            CAstart = ti;
                            inCA = true;
                        }
                        CAend = ti+1000L;
                    } else {
                        if (inCA) {
                            inCA = false;
                            NOA->AddEvent(CAstart,(CAend-CAstart) / 1000L);
                            CAstart = 0;
                        }
                    }
                    if (data[114] & 48) {
                        if (!inCA) {
                            CAstart = ti+1000L;
                            inCA = true;
                        }
                        CAend = ti+2000L;

                    } else {
                        if (inCA) {
                            inCA = false;
                            NOA->AddEvent(CAstart,(CAend-CAstart) / 1000L);
                            CAstart = 0;
                        }
                    }

                    //////////////////////////////////////////////////////////////////
                    // VSnore Event
                    //////////////////////////////////////////////////////////////////
                    if (data[109] & 3) {  // OA state 1st second
                        if (!inVS) {
                            VSstart = ti;
                            inVS = true;
                        }
                        VSend = ti+1000L;
                    } else {
                        if (inVS) {
                            inVS = false;
                            VS->AddEvent(VSstart,(VSend-VSstart) / 1000L);
                            VSstart = 0;
                        }
                    }
                    if (data[113] & 3) {
                        if (!inVS) {
                            VSstart = ti+1000L;
                            inVS = true;
                        }
                        VSend = ti+2000L;

                    } else {
                        if (inVS) {
                            inVS = false;
                            VS->AddEvent(VSstart,(VSend-VSstart) / 1000L);
                            VSstart = 0;
                        }
                    }

                    //////////////////////////////////////////////////////////////////
                    // Expiratory puff Event
                    //////////////////////////////////////////////////////////////////
                    if (data[109] & 12) {  // OA state 1st second
                        if (!inExP) {
                            ExPstart = ti;
                            inExP = true;
                        }
                        ExPend = ti+1000L;
                    } else {
                        if (inExP) {
                            inExP = false;
                            EXP->AddEvent(ExPstart,(ExPend-ExPstart) / 1000L);
                            ExPstart = 0;
                        }
                    }
                    if (data[113] & 12) {
                        if (!inExP) {
                            ExPstart = ti+1000L;
                            inExP = true;
                        }
                        ExPend = ti+2000L;

                    } else {
                        if (inExP) {
                            inExP = false;
                            EXP->AddEvent(ExPstart,(ExPend-ExPstart) / 1000L);
                            ExPstart = 0;
                        }
                    }

                    //////////////////////////////////////////////////////////////////
                    // Flow Limitation Event
                    //////////////////////////////////////////////////////////////////
                    if (data[109] & 48) {  // OA state 1st second
                        if (!inFL) {
                            FLstart = ti;
                            inFL = true;
                        }
                        FLend = ti+1000L;
                    } else {
                        if (inFL) {
                            inFL = false;
                            FL->AddEvent(FLstart,(FLend-FLstart) / 1000L);
                            FLstart = 0;
                        }
                    }
                    if (data[113] & 48) {
                        if (!inFL) {
                            FLstart = ti+1000L;
                            inFL = true;
                        }
                        FLend = ti+2000L;

                    } else {
                        if (inFL) {
                            inFL = false;
                            FL->AddEvent(FLstart,(FLend-FLstart) / 1000L);
                            FLstart = 0;
                        }
                    }

                    //////////////////////////////////////////////////////////////////
                    // Periodic Breathing Event
                    //////////////////////////////////////////////////////////////////
                    if (data[109] & 192) {  // OA state 1st second
                        if (!inPB) {
                            PBstart = ti;
                            inPB = true;
                        }
                        PBend = ti+1000L;
                    } else {
                        if (inPB) {
                            inPB = false;
                            PB->AddEvent(PBstart,(PBend-PBstart) / 1000L);
                            PBstart = 0;
                        }
                    }
                    if (data[113] & 192) {
                        if (!inPB) {
                            PBstart = ti+1000L;
                            inPB = true;
                        }
                        PBend = ti+2000L;

                    } else {
                        if (inPB) {
                            inPB = false;
                            PB->AddEvent(PBstart,(PBend-PBstart) / 1000L);
                            PBstart = 0;
                        }
                    }

                    //////////////////////////////////////////////////////////////////
                    // Respiratory Effort Related Arousal Event
                    //////////////////////////////////////////////////////////////////
                    if (data[111] & 48) {  // OA state 1st second
                        if (!inRE) {
                            REstart = ti;
                            inRE = true;
                        }
                        REend = ti+1000L;
                    } else {
                        if (inRE) {
                            inRE = false;
                            RE->AddEvent(REstart,(REend-REstart) / 1000L);
                            REstart = 0;
                        }
                    }
                    if (data[115] & 48) {
                        if (!inRE) {
                            REstart = ti+1000L;
                            inRE = true;
                        }
                        REend = ti+2000L;

                    } else {
                        if (inRE) {
                            inRE = false;
                            RE->AddEvent(REstart,(REend-REstart) / 1000L);
                            REstart = 0;
                        }
                    }
                }
            }


        }
        if (flow && sess) {
            // Close event states if they are still open, and write event.
            if (inH) HY->AddEvent(Hstart,(Hend-Hstart) / 1000L);
            if (inOA) OA->AddEvent(OAstart,(OAend-OAstart) / 1000L);
            if (inCA) NOA->AddEvent(CAstart,(CAend-CAstart) / 1000L);
            if (inLL) LL->AddEvent(LLstart,(LLend-LLstart) / 1000L);
            if (inVS) HY->AddEvent(VSstart,(VSend-VSstart) / 1000L);
            if (inExP) EXP->AddEvent(ExPstart,(ExPend-ExPstart) / 1000L);
            if (inFL) FL->AddEvent(FLstart,(FLend-FLstart) / 1000L);
            if (inPB) PB->AddEvent(PBstart,(PBend-PBstart) / 1000L);
            if (inPB) RE->AddEvent(REstart,(REend-REstart) / 1000L);

            // update min and max
            // then add to machine
            EventDataType min = flow->Min();
            EventDataType max = flow->Max();
            sess->setMin(CPAP_FlowRate, min);
            sess->setMax(CPAP_FlowRate, max);

            sess->setPhysMax(CPAP_FlowRate, min); // not sure :/
            sess->setPhysMin(CPAP_FlowRate, max);
            sess->really_set_last(flow->last());

            sess = NULL;
            flow = NULL;
        }

        f.close();
        data = (unsigned char *)dataBA.data();
    } else { // if (f.open(...)
        // L.BIN open failed
        return -1;
    }


    /////////////////////////////////////////////////////////////////////////////////////////////
    /// Parse L.BIN and extract per-minute data.
    /////////////////////////////////////////////////////////////////////////////////////////////

    EventList *leak = NULL;
    EventList *maxleak = NULL;
    EventList * RR  = NULL;
    EventList * Pressure  = NULL;
    EventList * TV  = NULL;
    EventList * MV = NULL;


    sess = NULL;
    const int DV6_L_HeaderSize = 55;
    // Need to parse L.bin minute table to get graphs
    f.setFileName(newpath+"/L.BIN");
    if (f.open(QIODevice::ReadOnly)) {
        dataBA = f.readAll();
        if (dataBA.size() <= DV6_L_HeaderSize) {
            return -1;
        }

        f.close();
        data = (unsigned char *)dataBA.data()+DV6_L_HeaderSize;
        int numLrecs = (dataBA.size()-DV6_L_HeaderSize) / DV6_L_RecLength;

        SR = summaryList.begin();

        lastts1 = 0;


        if (SR != summaryList.end()) for (int r=0; r<numLrecs; data += DV6_L_RecLength, ++r) {
            ts1 = ((data[4] << 24) | (data[3] << 16) | (data[2] << 8) | data[1])+ep; // session start time
            if (ts1 < lastts1) {
                qDebug() << "Corruption/Out of sequence data found in L.bin, aborting";
                break;
            }
            DV6_S_Record *R = &SR.value();
            if (leak && ((ts1 - lastts1) > 60)) {
                sess->set_last(maxleak->last());

                sess = nullptr;
                leak = nullptr;
                maxleak = nullptr;
                MV = TV = RR = nullptr;
                Pressure = nullptr;
            }
            lastts1=ts1;
            while (ts1 > R->stop_time) {
                if (leak && sess) {
                    // Close the open session and update the min and max
                    sess->set_last(maxleak->last());

                    sess = nullptr;
                    leak = nullptr;
                    maxleak = nullptr;
                    MV = TV = RR = nullptr;
                    Pressure = nullptr;
                }
                SR++;
                if (SR == summaryList.end()) break;
                R = &SR.value();
            }
            if (SR == summaryList.end())
                break;

            if (ts1 >= R->start_time) {
                if (!leak && R->sess) {
                    qDebug() << "Adding Leak data for session" << R->sess->session() << "starting at" << ts1;
                    leak = R->sess->AddEventList(CPAP_Leak, EVL_Event); // , 1.0, 0.0, 0.0, 0.0, double(60000) / double(1));
                    maxleak = R->sess->AddEventList(CPAP_MaxLeak, EVL_Event);// , 1.0, 0.0, 0.0, 0.0, double(60000) / double(1));
                    RR = R->sess->AddEventList(CPAP_RespRate, EVL_Event);
                    MV = R->sess->AddEventList(CPAP_MinuteVent, EVL_Event);
                    TV = R->sess->AddEventList(CPAP_TidalVolume, EVL_Event);

                    if (!R->hasMaskPressure) {
                        // Don't use this pressure if we have higher resolution available
                        Pressure = R->sess->AddEventList(CPAP_Pressure, EVL_Event);
                    }
                }
                if (leak) {
                    sess = R->sess;

                    qint64 ti = qint64(ts1) * 1000L;

                    maxleak->AddEvent(ti, data[5]);
                    leak->AddEvent(ti, data[6]);
                    RR->AddEvent(ti, data[9]);

                    if (Pressure) Pressure->AddEvent(ti, data[11] / 10.0f);

                    unsigned tv = data[7] | data[8] << 8;
                    MV->AddEvent(ti, data[10] );
                    TV->AddEvent(ti, tv);

                    if (!sess->channelExists(CPAP_FlowRate)) {
                        // No flow rate, so lets grab this data...
                    }
                }

            }
        } // for
        if (sess && leak) {
            sess->set_last(maxleak->last());
        }

    } else { // if (f.open(...)
        // L.BIN open failed
        return -1;
    }


    // Now sessionList is populated with summary data, lets parse the Events list in E.BIN


    EventList * OA = nullptr;
    EventList * CA = nullptr;
    EventList * H = nullptr;
    EventList * RE = nullptr;
    f.setFileName(newpath+"/E.BIN");
    const int DV6_E_HeaderSize = 55;

    if (f.open(QIODevice::ReadOnly)) {
        dataBA = f.readAll();
        if (dataBA.size() == 0) {
            return -1;
        }
        f.close();
        data = (unsigned char *)dataBA.data()+DV6_E_HeaderSize;
        int numErecs = (dataBA.size()-DV6_E_HeaderSize) / DV6_E_RecLength;

        SR = summaryList.begin();

        for (int r=0; r<numErecs; ++r, data += DV6_E_RecLength) {
            ts1 = ((data[4] << 24) | (data[3] << 16) | (data[2] << 8) | data[1])+ep; // start time
            ts2 = ((data[8] << 24) | (data[7] << 16) | (data[6] << 8) | data[5])+ep; // end

            if (SR == summaryList.end())
                break;

            DV6_S_Record *R = &SR.value();
            while (ts1 > R->stop_time) {
                if (OA && sess) {
                    // Close the open session and update the min and max
                    sess->set_last(OA->last());
                    sess->set_last(CA->last());
                    sess->set_last(H->last());
                    sess->set_last(RE->last());

                    sess = nullptr;
                    H = CA = RE = OA = nullptr;
                }
                SR++;
                if (SR == summaryList.end()) break;
                R = &SR.value();
            }
            if (SR == summaryList.end())
                break;

            if (ts1 >= R->start_time) {
                if (!OA && R->sess) {
                    qDebug() << "Adding Event data for session" << R->sess->session() << "starting at" << ts1;
                    OA = R->sess->AddEventList(CPAP_Obstructive, EVL_Event);
                    H = R->sess->AddEventList(CPAP_Hypopnea, EVL_Event);
                    RE = R->sess->AddEventList(CPAP_RERA, EVL_Event);
                    CA = R->sess->AddEventList(CPAP_ClearAirway, EVL_Event);
                }
                if (OA) {
                    sess = R->sess;

                    qint64 ti = qint64(ts1) * 1000L;
                    int code = data[13];
                    switch (code) {
                    case 1:
                        CA->AddEvent(ti, data[17]);
                        break;
                    case 2:
                        OA->AddEvent(ti, data[17]);
                        break;
                    case 4:
                        H->AddEvent(ti, data[17]);
                        break;
                    case 5:
                        RE->AddEvent(ti, data[17]);
                        break;
                    default:
                        break;
                    }
                }


            } // for
        }
        if (sess && OA) {
            sess->set_last(OA->last());
            sess->set_last(CA->last());
            sess->set_last(H->last());
            sess->set_last(RE->last());

        }
    } else { // if (f.open(...)
        // E.BIN open failed
        return -1;
    }

    QMap<quint32, DV6_S_Record>::iterator it;

    for (it=summaryList.begin(); it!= summaryList.end(); ++it) {
        Session * sess = it.value().sess;

        mach->AddSession(sess);

        // Update indexes, process waveform and perform flagging
        sess->UpdateSummaries();

        // Save is not threadsafe
        sess->Store(mach->getDataPath());

        // Unload them from memory
        sess->TrashEvents();

    }


    return summaryList.size();
}

int IntellipapLoader::Open(const QString & dirpath)
{
    // Check for SL directory
    // Check for DV5MFirm.bin?
    QString path(dirpath);
    path = path.replace("\\", "/");

    if (path.endsWith(SL_DIR)) {
        path.chop(3);
    } else if (path.endsWith(DV6_DIR)) {
        path.chop(4);
    }

    QDir dir;

    int r = -1;
    // Sometimes there can be an SL folder because SmartLink dumps an old DV5 firmware in it, so check it first
    if (dir.exists(path + SL_DIR))
        r = OpenDV5(path);

    if ((r<0) && dir.exists(path + DV6_DIR))
        r = OpenDV6(path);

    return r;
}

void IntellipapLoader::initChannels()
{
    using namespace schema;
    Channel * chan = nullptr;
    channel.add(GRP_CPAP, chan = new Channel(INTP_SmartFlexMode = 0x1165, SETTING,  MT_CPAP,  SESSION,
        "INTPSmartFlexMode", QObject::tr("SmartFlex Mode"),
        QObject::tr("Intellipap pressure relief mode."),
        QObject::tr("SmartFlex Mode"),
        "", DEFAULT, Qt::green));


    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, QObject::tr("Ramp Only"));
    chan->addOption(2, QObject::tr("Full Time"));

    channel.add(GRP_CPAP, chan = new Channel(INTP_SmartFlexLevel = 0x1169, SETTING,  MT_CPAP,  SESSION,
        "INTPSmartFlexLevel", QObject::tr("SmartFlex Level"),
        QObject::tr("Intellipap pressure relief level."),
        QObject::tr("SmartFlex Level"),
        "", DEFAULT, Qt::green));
}

bool intellipap_initialized = false;
void IntellipapLoader::Register()
{
    if (intellipap_initialized) { return; }

    qDebug() << "Registering IntellipapLoader";
    RegisterLoader(new IntellipapLoader());
    //InitModelMap();
    intellipap_initialized = true;

}
