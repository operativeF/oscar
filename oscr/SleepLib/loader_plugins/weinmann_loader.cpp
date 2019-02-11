/* SleepLib Weinmann SOMNOsoft/Balance Loader Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDir>
#include <QFile>
#include <QProgressBar>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>


#include "weinmann_loader.h"

Weinmann::Weinmann(Profile *profile, MachineID id)
    : CPAP(profile, id)
{
}

Weinmann::~Weinmann()
{
}

WeinmannLoader::WeinmannLoader()
{
    m_buffer = nullptr;
    m_type = MT_CPAP;
}

WeinmannLoader::~WeinmannLoader()
{
}

bool WeinmannLoader::Detect(const QString & givenpath)
{
    QDir dir(givenpath);

    if (!dir.exists()) {
        return false;
    }

    // Check for the settings file inside the .. folder
    if (!dir.exists("WM_DATA.TDF")) {
        return false;
    }

    return true;
}

int WeinmannLoader::ParseIndex(QFile & wmdata)
{
    QByteArray xml;
    do {
        xml += wmdata.readLine(250);
    } while (!wmdata.atEnd());

    QDomDocument index_xml("weinmann");

    index_xml.setContent(xml);
    QDomElement docElem = index_xml.documentElement();

    QDomNode n = docElem.firstChild();

    index.clear();
    while (!n.isNull()) {
        QDomElement e = n.toElement();
        if (!e.isNull()) {
            bool ok;
            int val = e.attribute("val").toInt(&ok);
            if (ok) {
                index[e.attribute("name")] = val;
                qDebug() << e.attribute("name") << "=" << hex << val;
            }
        }
        n = n.nextSibling();
    }
    return index.size();
}

const QString DayComplianceCount = "DayComplianceCount";
const QString CompOffset = "DayComplianceOffset";
const QString FlowOffset = "TID_Flow_Offset";
const QString StatusOffset = "TID_Status_Offset";
const QString PresOffset = "TID_P_Offset";
const QString AMVOffset = "TID_AMV_Offset";
const QString EventsOffset = "TID_Events_Offset";


void HighPass(char * data, int samples, float cutoff, float dt)
{
    float *Y = new float [samples];
    for (int i=0; i < samples; ++i) Y[i] = 0.0f;

    Y[0] = ((unsigned char *)data)[0] ;

    float RC = 1.0 / (cutoff * 2 * 3.1415926);
    float alpha = RC / (RC + dt);

    for (int i=1; i < samples; ++i) {
        float x = ((unsigned char *)data)[i] ;
        float x1 = ((unsigned char *)data)[i-1] ;
        Y[i] = alpha * (Y[i-1] + x - x1);
    }

    for (int i=0; i< samples; ++i) {
        data[i] = Y[i];
    }
    delete [] Y;
}

int WeinmannLoader::Open(const QString & dirpath)
{
    QString path(dirpath);
    path = path.replace("\\", "/");

    QFile wmdata(path + "/WM_DATA.TDF");
    if (!wmdata.open(QFile::ReadOnly)) {
        return -1;
    }

    int res = ParseIndex(wmdata);
    if (res < 0) return -1;


    MachineInfo info = newInfo();
    info.serial = "141819";
    Machine * mach = p_profile->CreateMachine(info);


    int WeekComplianceOffset = index["WeekComplianceOffset"];
    int WCD_Pin_Offset = index["WCD_Pin_Offset"];
//    int WCD_Pex_Offset = index["WCD_Pex_Offset"];
//    int WCD_Snore_Offset = index["WCD_Snore_Offset"];
//    int WCD_Lf_Offset = index["WCD_Lf_Offset"];
//    int WCD_Events_Offset = index["WCD_Events_Offset"];
//    int WCD_IO_Offset = index["WCD_IO_Offset"];
    int comp_start = index[CompOffset];

    int wccount = index["WeekComplianceCount"];

    int size = WCD_Pin_Offset - WeekComplianceOffset;

    quint8 * weekco = new quint8 [size];
    memset(weekco, 0, size);
    wmdata.seek(WeekComplianceOffset);
    wmdata.read((char *)weekco, size);

    unsigned char *p = weekco;
    for (int c=0; c < wccount; ++c) {
        int year = QString().sprintf("%02i%02i", p[0], p[1]).toInt();
        int month = p[2];
        int day = p[3];
        int hour = p[5];
        int minute = p[6];
        int second = p[7];
        QDateTime date = QDateTime(QDate(year,month,day), QTime(hour,minute,second));
        quint32 ts = date.toTime_t();
        if (!mach->SessionExists(ts)) {
            qDebug() << date;
        }

        // stores used length of data at 0x46, in 16bit integers, for IPAP, EPAP, snore, leak,
        // stores total length of data block at 0x66, in 16 bit integers for IPAP, EPAP, snore, leak


        p+=0x84;
    }

    delete [] weekco;


    //////////////////////////////////////////////////////////////////////
    // Read Day Compliance Information....
    //////////////////////////////////////////////////////////////////////

    int comp_end = index[FlowOffset];
    int comp_size = comp_end - comp_start;

    quint8 * comp = new quint8 [comp_size];
    memset((char *)comp, 0, comp_size);

    wmdata.seek(comp_start);
    wmdata.read((char *)comp, comp_size);

    p = comp;

    QDateTime dt_epoch(QDate(2000,1,1), QTime(0,0,0));
    //int epoch = dt_epoch.toTime_t();
    //epoch = 0;


    float flow_sample_duration = 1000.0 / 5;
    float pressure_sample_duration = 1000.0 / 2;
    //float amv_sample_duration = 200 * 10;

    //int c = index[DayComplianceCount];
    for (int i=0; i < 5; i++) {
        int year = QString().sprintf("%02i%02i", p[0], p[1]).toInt();
        int month = p[2];
        int day = p[3];
        int hour = p[5];
        int minute = p[6];
        int second = p[7];
        QDateTime date = QDateTime(QDate(year,month,day), QTime(hour,minute,second));
        quint32 ts = date.toTime_t();

        if (mach->SessionExists(ts)) continue;

        Session * sess = new Session(mach, ts);
        sess->SetChanged(true);


        // Flow Waveform
        quint32 fs = p[8] | p[9] << 8 | p[10] << 16 | p[11] << 24;
        quint32 fl = p[0x44] | p[0x45] << 8 | p[0x46] << 16 | p[0x47] << 24;

        // Status
        quint32 ss = p[12] | p[13] << 8 | p[14] << 16 | p[15] << 24;
        quint32 sl = p[0x48] | p[0x49] << 8 | p[0x4a] << 16 | p[0x4b] << 24;

        // Pressure
        quint32 ps = p[16] | p[17] << 8 | p[18] << 16 | p[19] << 24;
        quint32 pl = p[0x4c] | p[0x4d] << 8 | p[0x4e] << 16 | p[0x4f] << 24;

        // AMV
        quint32 ms = p[20] | p[21] << 8 | p[22] << 16 | p[23] << 24;
        quint32 ml = p[0x50] | p[0x51] << 8 | p[0x52] << 16 | p[0x53] << 24;

        // Events
        quint32 es = p[24] | p[25] << 8 | p[26] << 16 | p[27] << 24;
        quint32 er = p[0x54] | p[0x55] << 8 | p[0x56] << 16 | p[0x57] << 24;  // number of records


        compinfo.append(CompInfo(sess, date, fs, fl, ss, sl, ps, pl, ms, ml, es, er));

        int dur = fl / 5;

        sess->really_set_first(qint64(ts) * 1000L);
        sess->really_set_last(qint64(ts+dur) * 1000L);
        sessions[ts] = sess;

//        qDebug() << date << ts << dur << QString().sprintf("%02i:%02i:%02i", dur / 3600, dur/60 % 60, dur % 60);

        p += 0xd6;
    }

    delete [] comp;

    //////////////////////////////////////////////////////////////////////
    // Read Flow Waveform....
    //////////////////////////////////////////////////////////////////////

    int flowstart = index[FlowOffset];
    int flowend = index[StatusOffset];

    wmdata.seek(flowstart);

    int flowsize = flowend - flowstart;
    char * data = new char [flowsize];
    memset((char *)data, 0, flowsize);
    wmdata.read((char *)data, flowsize);

    float dt = 1.0 / (1000.0 / flow_sample_duration); // samples per second

    // Centre Waveform using High Pass Filter
    HighPass(data, flowsize, 0.1f, dt);

    //////////////////////////////////////////////////////////////////////
    // Read Status....
    //////////////////////////////////////////////////////////////////////

    int st_start = index[StatusOffset];
    int st_end = index[PresOffset];
    int st_size = st_end - st_start;

    char * st = new char [st_size];
    memset(st, 0, st_size);

    wmdata.seek(st_start);
    wmdata.read(st, st_size);


    //////////////////////////////////////////////////////////////////////
    // Read Mask Pressure....
    //////////////////////////////////////////////////////////////////////

    int pr_start = index[PresOffset];
    int pr_end = index[AMVOffset];
    int pr_size = pr_end - pr_start;

    char * pres = new char [pr_size];
    memset(pres, 0, pr_size);

    wmdata.seek(pr_start);
    wmdata.read(pres, pr_size);

    //////////////////////////////////////////////////////////////////////
    // Read AMV....
    //////////////////////////////////////////////////////////////////////

    int mv_start = index[AMVOffset];
    int mv_end = index[EventsOffset];
    int mv_size = mv_end - mv_start;

    char * mv = new char [mv_size];
    memset(mv, 0, mv_size);

    wmdata.seek(mv_start);
    wmdata.read(mv, mv_size);

    //////////////////////////////////////////////////////////////////////
    // Read Events....
    //////////////////////////////////////////////////////////////////////

    int ev_start = index[EventsOffset];
    int ev_end = wmdata.size();
    int ev_size = ev_end - ev_start;

    quint8 * ev = new quint8 [ev_size];
    memset((char *) ev, 0, ev_size);

    wmdata.seek(ev_start);
    wmdata.read((char *) ev, ev_size);

    //////////////////////////////////////////////////////////////////////
    // Process sessions
    //////////////////////////////////////////////////////////////////////

    for (int i=0; i< compinfo.size(); ++i) {
        const CompInfo & ci = compinfo.at(i);
        Session * sess = ci.session;

        qint64 ti = sess->first();

        EventList * flow = sess->AddEventList(CPAP_FlowRate, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, flow_sample_duration);
        flow->AddWaveform(ti, &data[ci.flow_start], ci.flow_size, (ci.flow_size/(1000.0/flow_sample_duration)) * 1000.0);

        EventList * PR = sess->AddEventList(CPAP_MaskPressure, EVL_Waveform, 0.1f, 0.0, 0.0, 0.0, pressure_sample_duration);
        PR->AddWaveform(ti, (unsigned char *)&pres[ci.pres_start], ci.pres_size, (ci.pres_size/(1000.0/pressure_sample_duration)) * 1000.0);

        // Weinmann's MV graph is pretty dodgy... commenting this out and using my flow calced ones instead (the code below is mapped to snore for comparison purposes)
        //EventList * MV = sess->AddEventList(CPAP_Snore, EVL_Waveform, 1.0f, 0.0, 0.0, 0.0, amv_sample_duration);
        //MV->AddWaveform(ti, (unsigned char *)&mv[ci.amv_start], ci.amv_size, (ci.amv_size/(1000/amv_sample_duration)) * 1000L);

//        EventList * L = sess->AddEventList(CPAP_Leak, EVL_Event);
//        EventList * S = sess->AddEventList(CPAP_Snore, EVL_Event);
        EventList * OA = sess->AddEventList(CPAP_Obstructive, EVL_Event);
        EventList * A = sess->AddEventList(CPAP_Apnea, EVL_Event);
        EventList * H = sess->AddEventList(CPAP_Hypopnea, EVL_Event);
        EventList * FL = sess->AddEventList(CPAP_FlowLimit, EVL_Event);
//        EventList * VS = sess->AddEventList(CPAP_VSnore, EVL_Event);
        quint64 tt = ti;
        quint64 step = sess->length() / ci.event_recs;
        unsigned char *p = &ev[ci.event_start];
        for (quint32 j=0; j < ci.event_recs; ++j) {
            QDate evdate = ci.time.date();
            QTime evtime(p[1], p[2], p[3]);
            if (evtime < ci.time.time()) {
                evdate = evdate.addDays(1);
            }
            quint64 ts = QDateTime(evdate, evtime).toMSecsSinceEpoch();

            // I think p[0] is amount of flow restriction..

            unsigned char evcode = p[0];
            EventStoreType data = p[4] | p[5] << 8;

            if (evcode == '@') {
                OA->AddEvent(ts,data/10.0);
            } else if (evcode =='A') {
                A->AddEvent(ts,data/10.0);
            } else if (evcode == 'F') {
                FL->AddEvent(ts,data/10.0);
            } else if (evcode == '*') {
                H->AddEvent(ts,data/10.0);
            }
/*            switch (evcode) {
            case 0x03:
                break;
            case 0x04:
                break;
            case 0x08:
                break;
            case 0x09:
                break;
            case 0x0a:
                break;
            case 0x0b:
                break;
            case 0x0c:
                break;
            case 0x10:
                break;
            case 0x11:
                break;
            case 0x12:
                break;
            case 0x13:
                S->AddEvent(ts, data);
                break;
            case 0x22:
              //  VS->AddEvent(ts, data/10.0);

                break;
            case 0x28:
                VS->AddEvent(ts, data/10.0);

                break;
            case 'F':
                FL->AddEvent(ts, data/10.0);
                break;
            case '@':
                OA->AddEvent(ts, data/10.0);
                break;
            case '\'':
                //A->AddEvent(ts, data/10.0);
                break;
            case 'a':
                A->AddEvent(ts, data/10.0);
                break;
            case 'A':
               // A->AddEvent(ts, data/10.0);
                break;
            case '*':
                H->AddEvent(ts, data/10.0);
                break;
            case 'd':
                break;
            case 0x91:
                break;
            case 0x96:
                break;
            case 0x84:
                break;
            default:
                qDebug() << (int)evcode << endl;
            }*/

          //  S->AddEvent(ts, p[5]);



            // p[5] == 0 corresponds to peak events
            // p[5] == 1 corresponds to hypopnea/bstructive events

            //if (p[5] == 2) OA->AddEvent(ts, p[4]);


            // This is ugggggly...


            tt += step;
            p += 6;
        }

        sess->UpdateSummaries();
        mach->AddSession(sess);
    }
    delete [] data;
    delete [] st;
    delete [] pres;
    delete [] mv;
    delete [] ev;

    mach->Save();

    return 1;

/*

    // Center the waveform
    HighPass(data, flowsize, 0.6, dt);

    EventList * flow = sess->AddEventList(CPAP_FlowRate, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, sample_duration);
    flow->AddWaveform(tt, (char *)data, flowsize, (flowsize/(1000/sample_duration)) * 1000L);


    qint64 ti = tt;
    for (int i=0; i < pr_size; ++i) {
        EventStoreType c = ((unsigned char *)pres)[i];
        PR->AddEvent(ti, c);
        ti += sample_duration * 2.5; //46296296296296;
    }
    // Their calcs is uglier than mine!
    EventList * MV = sess->AddEventList(CPAP_Snore, EVL_Event, 1.0);


    ti = tt;
    for (int i=0; i < mv_size; ++i) {
        EventStoreType c = ((unsigned char *)mv)[i];
        MV->AddEvent(ti, c);
        ti += sample_duration * 9;
    }

    // Their calcs is uglier than mine!
    EventList * ST = sess->AddEventList(CPAP_Leak, EVL_Event, 1.0);
    int st_start = index[StatusOffset];
    int st_end = index[PresOffset];
    int st_size = st_end - st_start;

    char st[st_size];
    memset(st, 0, st_size);

    wmdata.seek(st_start);
    wmdata.read(st, st_size);

    ti = tt;
    for (int i=0; i < st_size; ++i) {
        EventStoreType c = ((unsigned char *)st)[i];
 //       if (c & 0x80) {
            ST->AddEvent(ti, c & 0x10);
   //     }
        ti += sample_duration*4;  // *9
    }


//    EventList * LEAK = sess->AddEventList(CPAP_Leak, EVL_Event);
//    EventList * SNORE = sess->AddEventList(CPAP_Snore, EVL_Event);

//    int ev_start = index[EventsOffset];
//    int ev_end = wmdata.size();
//    int ev_size = ev_end - ev_start;
//    int recs = ev_size / 0x12;

//    unsigned char ev[ev_size];
//    memset((char *) ev, 0, ev_size);

//    wmdata.seek(ev_start);
//    wmdata.read((char *) ev, ev_size);

    sess->really_set_last(flow->last());

//    int pos = 0;
//    ti = tt;

//    // 6 byte repeating structure.. No Leaks :(
//    do {
//        //EventStoreType c = ((unsigned char*)ev)[pos+0]; // TV?
//        //c = ((unsigned char*)ev)[pos+6]; // MV?

//        EventStoreType c = ((EventStoreType*)ev)[pos+0];
//        LEAK->AddEvent(ti, c);
//        SNORE->AddEvent(ti, ((unsigned char*)ev)[pos+2]);
//        pos += 0x6;
//        ti += 30000;
//        if (ti > sess->last())
//            break;
//    } while (pos < (ev_size - 0x12));



    m->AddSession(sess);
    sess->UpdateSummaries();

    return 1;*/
}

void WeinmannLoader::initChannels()
{
    //using namespace schema;
    //Channel * chan = nullptr;
//    channel.add(GRP_CPAP, chan = new Channel(INTP_SmartFlex = 0x1165, SETTING,   SESSION,
//        "INTPSmartFlex", QObject::tr("SmartFlex"),
//        QObject::tr("Weinmann pressure relief setting."),
//        QObject::tr("SmartFlex"),
//        "", DEFAULT, Qt::green));


//    chan->addOption(1, STR_TR_None);
}

bool weinmann_initialized = false;
void WeinmannLoader::Register()
{
    if (weinmann_initialized) { return; }

    qDebug() << "Registering WeinmannLoader";
    RegisterLoader(new WeinmannLoader());
    //InitModelMap();
    weinmann_initialized = true;
}
