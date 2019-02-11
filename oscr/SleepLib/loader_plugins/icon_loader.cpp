/* SleepLib Fisher & Paykel Icon Loader Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDir>
#include <QMessageBox>
#include <QDataStream>
#include <QTextStream>
#include <cmath>

#include "icon_loader.h"

const QString FPHCARE = "FPHCARE";

FPIcon::FPIcon(Profile *profile, MachineID id)
    : CPAP(profile, id)
{
}

FPIcon::~FPIcon()
{
}

FPIconLoader::FPIconLoader()
{
    m_buffer = nullptr;
    m_type = MT_CPAP;
}

FPIconLoader::~FPIconLoader()
{
}

bool FPIconLoader::Detect(const QString & givenpath)
{
    QString path = givenpath;
    
    path = path.replace("\\", "/");

    if (path.endsWith("/")) {
        path.chop(1);
    }

    if (path.endsWith("/" + FPHCARE)) {
        path = path.section("/",0,-2);
    }
    
    QDir dir(path);

    if (!dir.exists()) {
        return false;
    }
    

    // F&P Icon have a folder called FPHCARE in the root directory
    if (!dir.exists(FPHCARE)) {
        return false;
    }

    // CHECKME: I can't access F&P ICON data right now
    if (!dir.exists("FPHCARE/ICON")) {
        return false;
    }

    return true;
}


int FPIconLoader::Open(const QString & path)
{
    QString tmp = path;

    tmp = tmp.replace("\\", "/");
    if (tmp.endsWith("/")) {
        tmp.chop(1);
    }

    QString newpath;

    if (tmp.endsWith("/" + FPHCARE)) {
        newpath = tmp;
    } else {
        newpath = tmp + "/" + FPHCARE;
    }

    newpath += "/ICON/";

    QString filename;

    QDir dir(newpath);

    if ((!dir.exists() || !dir.isReadable())) {
        return -1;
    }

    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    QStringList SerialNumbers;

    bool ok;

    for (int i = 0; i < flist.size(); i++) {
        QFileInfo fi = flist.at(i);
        QString filename = fi.fileName();

        filename.toInt(&ok);

        if (ok) {
            SerialNumbers.push_back(filename);
        }
    }

    Machine *m;

    QString npath;

    int c = 0;
    for (int i = 0; i < SerialNumbers.size(); i++) {
        MachineInfo info = newInfo();
        info.serial = SerialNumbers[i];
        m = p_profile->CreateMachine(info);

        npath = newpath + "/" + info.serial;

        try {
            if (m) {
                c+=OpenMachine(m, npath);
            }
        } catch (OneTypePerDay& e) {
            Q_UNUSED(e)
            p_profile->DelMachine(m);
            MachList.erase(MachList.find(info.serial));
            QMessageBox::warning(nullptr, tr("Import Error"),
                                 tr("This Machine Record cannot be imported in this profile.")+"\n\n"+tr("The Day records overlap with already existing content."),
                                 QMessageBox::Ok);
            delete m;
        }
    }

    return c;
}

struct FPWaveChunk {
    FPWaveChunk() {
        st = 0;
        duration = 0;
        flow = nullptr;
        pressure = nullptr;
        leak = nullptr;
        file = 0;
    }
    FPWaveChunk(qint64 start, qint64 dur, int f) { st = start; duration = dur; file = f, flow = nullptr; leak = nullptr; pressure = nullptr; }
    FPWaveChunk(const FPWaveChunk &copy) {
        st = copy.st;
        duration = copy.duration;
        flow = copy.flow;
        leak = copy.leak;
        pressure = copy.pressure;
        file = copy.file;
    }
    qint64 st;
    qint64 duration;
    int file;
    EventList *flow;
    EventList *leak;
    EventList *pressure;
};

bool operator<(const FPWaveChunk &a, const FPWaveChunk &b)
{
    return (a.st < b.st);
}

int FPIconLoader::OpenMachine(Machine *mach, const QString & path)
{
    qDebug() << "Opening FPIcon " << path;
    QDir dir(path);

    if (!dir.exists() || (!dir.isReadable())) {
        return -1;
    }

    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    QString filename, fpath;

    emit setProgressValue(0);

    QStringList summary, log, flw, det;
    Sessions.clear();

    for (int i = 0; i < flist.size(); i++) {
        QFileInfo fi = flist.at(i);
        filename = fi.fileName();
        fpath = path + "/" + filename;

        if (filename.left(3).toUpper() == "SUM") {
            summary.push_back(fpath);
            OpenSummary(mach, fpath);
        } else if (filename.left(3).toUpper() == "DET") {
            det.push_back(fpath);
        } else if (filename.left(3).toUpper() == "FLW") {
            flw.push_back(fpath);
        } else if (filename.left(3).toUpper() == "LOG") {
            log.push_back(fpath);
        }
    }

    for (int i = 0; i < det.size(); i++) {
        OpenDetail(mach, det[i]);
    }

    for (int i = 0; i < flw.size(); i++) {
        OpenFLW(mach, flw[i]);
    }

    SessionID sid;//,st;
    float hours, mins;

    qDebug() << "Last 20 Sessions";

    int cnt = 0;
    QDateTime dt;
    QString a;

    if (Sessions.size() > 0) {

        QMap<SessionID, Session *>::iterator it = Sessions.end();
        it--;

        dt = QDateTime::fromTime_t(qint64(it.value()->first()) / 1000L);
        QDate date = dt.date().addDays(-7);
        it++;

        do {
            it--;
            Session *sess = it.value();
            sid = sess->session();
            hours = sess->hours();
            mins = hours * 60;
            dt = QDateTime::fromTime_t(sid);

            if (sess->channelDataExists(CPAP_FlowRate)) { a = "(flow)"; }
            else { a = ""; }

            qDebug() << cnt << ":" << dt << "session" << sid << "," << mins << "minutes" << a;

            if (dt.date() < date) { break; }

            ++cnt;

        } while (it != Sessions.begin());

    }
        //    qDebug() << "Unmatched Sessions";
        //    QList<FPWaveChunk> chunks;
        //    for (QMap<int,QDate>::iterator dit=FLWDate.begin();dit!=FLWDate.end();dit++) {
        //        int k=dit.key();
        //        //QDate date=dit.value();
        ////        QList<Session *> values = SessDate.values(date);
        //        for (int j=0;j<FLWTS[k].size();j++) {

        //            FPWaveChunk chunk(FLWTS[k].at(j),FLWDuration[k].at(j),k);
        //            chunk.flow=FLWMapFlow[k].at(j);
        //            chunk.leak=FLWMapLeak[k].at(j);
        //            chunk.pressure=FLWMapPres[k].at(j);

        //            chunks.push_back(chunk);

        //            zz=FLWTS[k].at(j)/1000;
        //            dur=double(FLWDuration[k].at(j))/60000.0;
        //            bool b,c=false;
        //            if (Sessions.contains(zz)) b=true; else b=false;
        //            if (b) {
        //                if (Sessions[zz]->channelDataExists(CPAP_FlowRate)) c=true;
        //            }
        //            qDebug() << k << "-" <<j << ":" << zz << qRound(dur) << "minutes" << (b ? "*" : "") << (c ? QDateTime::fromTime_t(zz).toString() : "");
        //        }
        //    }
        //    std::sort(chunks.begin(), chunks.end());
        //    bool b,c;
        //    for (int i=0;i<chunks.size();i++) {
        //        const FPWaveChunk & chunk=chunks.at(i);
        //        zz=chunk.st/1000;
        //        dur=double(chunk.duration)/60000.0;
        //        if (Sessions.contains(zz)) b=true; else b=false;
        //        if (b) {
        //            if (Sessions[zz]->channelDataExists(CPAP_FlowRate)) c=true;
        //        }
        //        qDebug() << chunk.file << ":" << i << zz << dur << "minutes" << (b ? "*" : "") << (c ? QDateTime::fromTime_t(zz).toString() : "");
        //    }

    int c = Sessions.size();
    finishAddingSessions();
    mach->Save();


    return c;
}

// !\brief Convert F&P 32bit date format to 32bit UNIX Timestamp
quint32 convertDate(quint32 timestamp)
{
    quint16 day, month,hour=0, minute=0, second=0;
    quint16 year;


    day = timestamp & 0x1f;
    month = (timestamp  >> 5) & 0x0f;
    year = 2000 + ((timestamp  >> 9) & 0x3f);
    timestamp >>= 15;
    second = timestamp & 0x3f;
    minute = (timestamp >> 6) & 0x3f;
    hour = (timestamp >> 12);

    QDateTime dt = QDateTime(QDate(year, month, day), QTime(hour, minute, second),Qt::UTC);

//    Q NO!!! _ASSERT(dt.isValid());
//    if ((year == 2013) && (month == 9) && (day == 18)) {
//        // this is for testing.. set a breakpoint on here and
//        int i=5;
//    }


    // From Rudd's data set compared to times reported from his F&P software's report (just the time bits left over)
    // 90514 = 00:06:18 WET 23:06:18 UTC 09:06:18 AEST
    // 94360 = 01:02:24 WET
    // 91596 = 00:23:12 WET
    // 19790 = 23:23:50 WET

    return dt.addSecs(-54).toTime_t();
}

quint32 convertFLWDate(quint32 timestamp) // Bit format: hhhhhmmmmmmssssssYYYYYYMMMMDDDDD
{
    quint16 day, month, hour, minute, second;
    quint16 year;

    day = timestamp & 0x1f;
    month = (timestamp  >> 5) & 0x0f;
    year = 2000 + ((timestamp  >> 9) & 0x3f);
    timestamp >>= 15;
    second = timestamp & 0x3f;
    minute = (timestamp >> 6) & 0x3f;
    hour = (timestamp >> 12);

    QDateTime dt = QDateTime(QDate(year, month, day), QTime(hour, minute, second), Qt::UTC);

    if(!dt.isValid()){
        dt = QDateTime(QDate(2015,1,1), QTime(0,0,1));
    }
//    Q NO!!! _ASSERT(dt.isValid());
//    if ((year == 2013) && (month == 9) && (day == 18)) {
//        int i=5;
//    }
    // 87922 23:23:50 WET
    return dt.addSecs(-54).toTime_t();
}

//QDateTime FPIconLoader::readFPDateTime(quint8 *data)
//{
//    quint32 ts = (data[3] << 24) | (data[2] << 16) | ((data[1] << 8) | data[0]); // ^ 0xc00000;
//    // 0x20a41b18

//    quint8 day = ts & 0x1f; // 0X18      24
//    ts >>= 5;               // 10520D8
//    quint8 month = ts & 0x0f; // 0X08      8
//    ts >>= 4;               // 10520D
//    quint8 year = ts & 0x3f; // 0X0D      13
//    ts >>= 6;               // 4148
//    quint8 second = ts & 0x3f; // 0X08      8
//    ts >>= 6;               // 20A
//    quint8 minute = ts & 0x3f; // 0A        10
//    ts >>= 6;               // 10
//    quint8 hour = ts & 0x1f; // 10        16
//    QDate date = QDate(2000 + year, month, day);
//    QTime time = QTime(hour, minute, second);
//    QDateTime datetime = QDateTime(date, time, Qt::UTC);
//    return datetime;
//}

/*
 *in >> a1;
in >> a2;
t1=a2 << 8 | a1;

if (t1==0xfafe)
    break;

day=t1 & 0x1f;
month=(t1 >> 5) & 0x0f;
year=2000+((t1 >> 9) & 0x3f);

in >> a1;
in >> a2;

ts=((a2 << 8) | a1) << 1;
ts|=(t1 >> 15) & 1;

second=(ts & 0x3f);
minute=(ts >> 6) & 0x3f;
hour=(ts >> 12) & 0x1f; */



// FLW Header Structure
// 0x0000-0x01fe
// newline (0x0d) seperated list of machine information strings.
// magic?         0201
// version        1.5.0
// serial number  12 digits
// Machine Series "ICON"
// Machine Model  "Auto"
// Remainder of header is 0 filled...
// 0x01ff 8 bit additive sum checksum byte of previous header bytes

// 0x0200-0x0203 32bit timestamp in
bool FPIconLoader::OpenFLW(Machine *mach, const QString & filename)
{
    Q_UNUSED(mach);

    quint32 ts;
    double ti;

    EventList *flow = nullptr, * pressure = nullptr;

    qDebug() << filename;
    QFile file(filename);

    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Couldn't open" << filename;
        return false;
    }

    QByteArray header = file.read(0x200);
    if (header.size() != 0x200) {
        qDebug() << "Short file" << filename;
        return false;
    }

    unsigned char hsum = 0x0;
    for (int i = 0; i < 0x1ff; i++) { hsum ^= header[i]; }
    if (hsum != header[0x1ff]) {
        qDebug() << "Header checksum mismatch" << filename;
    }

    QTextStream htxt(&header);
    QString h1, version, fname, serial, model, type;
    htxt >> h1;
    htxt >> version;
    htxt >> fname;
    htxt >> serial;
    htxt >> model;
    htxt >> type;

    if (mach->model().isEmpty()) {
        mach->setModel(model+" "+type);
    }

    QByteArray buf = file.read(4);
    unsigned char * data = (unsigned char *)buf.data();

    quint32 t2 = data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;

    // this line is probably superflous crud.
    if (t2 == 0xffffffff) return false;

    QByteArray block = file.readAll();
    file.close();
    data = (unsigned char *)block.data();

    // Abort if crapy
    if (!(data[103]==0xff && data[104]==0xff))
        return false;

    ts = convertFLWDate(t2);

    if (ts > QDateTime(QDate(2015,1,1), QTime(0,0,0)).toTime_t()) {
        return false;
    }


    ti = qint64(ts) * 1000L;

    QMap<SessionID, Session *>::iterator sit = Sessions.find(ts);

    Session *sess;

    bool newsess = false;

    if (sit != Sessions.end()) {
        sess = sit.value();
 //       qDebug() << filenum << ":" << date << sess->session() << ":" << sess->hours() * 60.0;
    } else {
        // Create a session
        qint64 k = -1;
        Session *s1 = nullptr;
        sess = nullptr;

        sit = Sessions.end();
        if (Sessions.begin() != sit) {
            do {
                sit--;
                s1 = sit.value();
                qint64 z = qAbs(qint64(sit.key()) - qint64(ts));
                if (z < 3600) {
                    if ((k < 0) || (k > z)) {
                        k = z;
                        sess = s1;
                    }
                }
            } while (sit != Sessions.begin());
        }

        if (sess) {
            sess->set_first(ti);
            sess->setFirst(CPAP_FlowRate, ti);
            sess->setFirst(CPAP_MaskPressure, ti);
        } else {
            sess = new Session(mach, ts);
            sess->set_first(ti);
            sess->setFirst(CPAP_FlowRate, ti);
            sess->setFirst(CPAP_MaskPressure, ti);
            newsess = true;
//            qDebug() << filenum << ":" << date << "couldn't find matching session for" << ts;
        }
    }
    const int samples_per_block = 50;
    const double rate = 1000.0 / double(samples_per_block);


    // F&P Overwrites this file, not appends to it.
    flow = new EventList(EVL_Waveform, 1.0F, 0, 0, 0, rate);
    pressure = new EventList(EVL_Event, 0.01F, 0, 0, 0, rate * double(samples_per_block));

    flow->setFirst(ti);
    pressure->setFirst(ti);

    quint16 endMarker;
    qint8 offset;         // offset from center for this block
    quint16 pres;       // mask pressure

    qint16 tmp;

    qint16 samples[samples_per_block];

    EventDataType val;

    unsigned char *p = data;
    int datasize = block.size();
    unsigned char *end = data+datasize;
    do {
        endMarker = *((quint16 *)p);
        if (endMarker == 0xffff) {
            p += 2;
            continue;
        }
        if (endMarker == 0x7fff) {
            break;
        }
        offset = ((qint8*)p)[102];
        for (int i=0; i< samples_per_block; ++i) {
            tmp = ((char *)p)[1] << 8 | p[0];
            p += 2;
            // Assuming Litres per hour, converting to litres per minute and applying offset?
            // As in should be 60.0?
            val = (EventDataType(tmp) / 100.0) - offset;
            samples[i] = val;
        }
        flow->AddWaveform(ti, samples, samples_per_block, rate);

        pres = *((quint16 *)p);
        pressure->AddEvent(ti, pres);

        ti += samples_per_block * rate;

        p+=3; // (offset too)
    } while (p < end);

    if (endMarker != 0x7fff) {
        qDebug() << fname << "waveform does not end with the corrent marker" << hex << endMarker;
    }

    if (sess) {
        sess->setLast(CPAP_FlowRate, ti);
        sess->setLast(CPAP_MaskPressure, ti);
        sess->eventlist[CPAP_FlowRate].push_back(flow);
        sess->eventlist[CPAP_MaskPressure].push_back(pressure);
    }

    if (newsess) {
        addSession(sess);
    }

    if (p_profile->session->backupCardData()) {
        QString backup = mach->getBackupPath()+"FPHCARE/ICON/"+serial.right(serial.size()-4)+"/";
        QDir dir;
        QString newname = QString("FLW%1.FPH").arg(ts);
        dir.mkpath(backup);
        dir.cd(backup);
        if (!dir.exists(newname)) {
            file.copy(backup+newname);
        }
    }


    return true;
}


////////////////////////////////////////////////////////////////////////////////////////////
// Open Summary file
////////////////////////////////////////////////////////////////////////////////////////////
bool FPIconLoader::OpenSummary(Machine *mach, const QString & filename)
{
    qDebug() << filename;
    QByteArray header;
    QFile file(filename);

    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Couldn't open" << filename;
        return false;
    }

    header = file.read(0x200);

    if (header.size() != 0x200) {
        qDebug() << "Short file" << filename;
        return false;
    }

    unsigned char hsum = 0xff;

    for (int i = 0; i < 0x1ff; i++) {
        hsum += header[i];
    }

    if (hsum != header[0x1ff]) {
        qDebug() << "Header checksum mismatch" << filename;
    }

    QTextStream htxt(&header);
    QString h1, version, fname, serial, model, type;
    htxt >> h1;
    htxt >> version;
    htxt >> fname;
    htxt >> serial;
    htxt >> model;
    htxt >> type;

    mach->setModel(model + " " + type);

    QByteArray data;
    data = file.readAll();

    QDataStream in(data);
    in.setVersion(QDataStream::Qt_4_8);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 ts;
    //QByteArray line;
    unsigned char a1, a2, a3, a4, a5, p1, p2,  p3, p4, p5, j1, j2, j3 , j4, j5, j6, j7, x1, x2;

    quint16 d1, d2, d3;

    int usage; //,runtime;

    QDate date;

    do {
        in >> ts;
        if (ts == 0xffffffff)
            break;
        if ((ts & 0xfafe) == 0xfafe)
            break;

        ts = convertDate(ts);

        // the following two quite often match in value
        in >> a1;  // 0x04 Run Time
        in >> a2;  // 0x05 Usage Time
        //runtime = a1 * 360; // durations are in tenth of an hour intervals
        usage = a2 * 360;

        in >> a3;  // 0x06  // Ramps???
        in >> a4;  // 0x07  // a pressure value?
        in >> a5;  // 0x08  // ?? varies.. always less than 90% leak..

        in >> d1;  // 0x09
        in >> d2;  // 0x0b
        in >> d3;  // 0x0d   // 90% Leak value..

        in >> p1;  // 0x0f
        in >> p2;  // 0x10

        in >> j1;  // 0x11
        in >> j2;  // 0x12  // Apnea Events
        in >> j3;  // 0x13  // Hypopnea events
        in >> j4;  // 0x14  // Flow Limitation events
        in >> j5;  // 0x15
        in >> j6;  // 0x16
        in >> j7;  // 0x17

        in >> p3;  // 0x18
        in >> p4;  // 0x19
        in >> p5;  // 0x1a

        in >> x1;  // 0x1b
        in >> x2;  // 0x1c    // humidifier setting

        if (!mach->SessionExists(ts)) {
            Session *sess = new Session(mach, ts);
            sess->really_set_first(qint64(ts) * 1000L);
            sess->really_set_last(qint64(ts + usage) * 1000L);
            sess->SetChanged(true);

            sess->setCount(CPAP_Obstructive, j2);
            sess->setCph(CPAP_Obstructive, j2 / (float(usage)/3600.00));

            sess->setCount(CPAP_Hypopnea, j3);
            sess->setCph(CPAP_Hypopnea, j3 / (float(usage)/3600.00));

            sess->setCount(CPAP_FlowLimit, j4);
            sess->setCph(CPAP_FlowLimit, j4 / (float(usage)/3600.00));

            SessDate.insert(date, sess);

            //            sess->setCount(CPAP_Obstructive,j1);
            //            sess->setCount(CPAP_Hypopnea,j2);
            //            sess->setCount(CPAP_ClearAirway,j3);
            //            sess->setCount(CPAP_Apnea,j4);
            //sess->setCount(CPAP_,j5);
            if (p1 != p2) {
                sess->settings[CPAP_Mode] = (int)MODE_APAP;
                sess->settings[CPAP_PressureMin] = p3 / 10.0;
                sess->settings[CPAP_PressureMax] = p4 / 10.0;
            } else {
                sess->settings[CPAP_Mode] = (int)MODE_CPAP;
                sess->settings[CPAP_Pressure] = p1 / 10.0;
            }

            sess->settings[CPAP_HumidSetting] = x2;
            //sess->settings[CPAP_PresReliefType]=PR_SENSAWAKE;
            Sessions[ts] = sess;

            addSession(sess);
        }
    } while (!in.atEnd());

    if (p_profile->session->backupCardData()) {
        QString backup = mach->getBackupPath()+"FPHCARE/ICON/"+serial.right(serial.size()-4)+"/";
        QDir dir;
        QString newname = QString("SUM%1.FPH").arg(QDate::currentDate().year(),4,10,QChar('0'));
        dir.mkpath(backup);
        dir.cd(backup);
        if (!dir.exists(newname)) {
            file.copy(backup+newname);
        }
    }

    return true;
}

bool FPIconLoader::OpenDetail(Machine *mach, const QString & filename)
{
    Q_UNUSED(mach);

    qDebug() << filename;
    QByteArray header;
    QFile file(filename);

    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Couldn't open" << filename;
        return false;
    }

    header = file.read(0x200);

    if (header.size() != 0x200) {
        qDebug() << "Short file" << filename;
        return false;
    }

    // Calculate and test checksum
    unsigned char hsum = 0;
    for (int i = 0; i < 0x1ff; i++) {
        hsum += header[i];
    }

    if (hsum != header[0x1ff]) {
        qDebug() << "Header checksum mismatch" << filename;
    }

    QTextStream htxt(&header);
    QString h1, version, fname, serial, model, type;
    htxt >> h1;
    htxt >> version;
    htxt >> fname;
    htxt >> serial;
    htxt >> model;
    htxt >> type;

    QByteArray index = file.read(0x800);
    if (index.size()!=0x800) {
        // faulty file..
        return false;
    }
    QDataStream in(index);
    quint32 ts;

    in.setVersion(QDataStream::Qt_4_6);
    in.setByteOrder(QDataStream::LittleEndian);

    QVector<quint32> times;
    QVector<quint16> start;
    QVector<quint8> records;

    quint16 strt;
    quint8 recs;

    int totalrecs = 0;

    do {
        in >> ts;
        if (ts == 0xffffffff) break;
        if ((ts & 0xfafe) == 0xfafe) break;

        ts = convertDate(ts);

        in >> strt;
        in >> recs;
        totalrecs += recs;

        if (Sessions.contains(ts)) {
            times.push_back(ts);
            start.push_back(strt);
            records.push_back(recs);
        }
    } while (!in.atEnd());

    QByteArray databytes = file.readAll();

    in.setVersion(QDataStream::Qt_4_6);
    in.setByteOrder(QDataStream::BigEndian);

    // 5 byte repeating patterns

    quint8 *data = (quint8 *)databytes.data();

    qint64 ti;
    quint8 pressure, leak, a1, a2, a3, a4;
//    quint8 sa1, sa2;  // The two sense awake bits per 2 minutes
    SessionID sessid;
    Session *sess;
    int idx;

    for (int r = 0; r < start.size(); r++) {
        sessid = times[r];
        sess = Sessions[sessid];
        ti = qint64(sessid) * 1000L;
        sess->really_set_first(ti);

        EventList *LK = sess->AddEventList(CPAP_LeakTotal, EVL_Event, 1);
        EventList *PR = sess->AddEventList(CPAP_Pressure, EVL_Event, 0.1F);
        EventList *OA = sess->AddEventList(CPAP_Obstructive, EVL_Event);
        EventList *H = sess->AddEventList(CPAP_Hypopnea, EVL_Event);
        EventList *FL = sess->AddEventList(CPAP_FlowLimit, EVL_Event);
        EventList *SA = sess->AddEventList(CPAP_SensAwake, EVL_Event);

        unsigned stidx = start[r];
        int rec = records[r];

        idx = stidx * 15;

        quint8 bitmask;
        for (int i = 0; i < rec; ++i) {
            for (int j = 0; j < 3; ++j) {
                pressure = data[idx];
                PR->AddEvent(ti+120000, pressure);

                leak = data[idx + 1];
                LK->AddEvent(ti+120000, leak);

                a1 = data[idx + 2];   // [0..5] Obstructive flag, [6..7] Unknown
                a2 = data[idx + 3];   // [0..5] Hypopnea,         [6..7] Unknown
                a3 = data[idx + 4];   // [0..5] Flow Limitation,  [6..7] SensAwake bitflags, 1 per minute

                // Sure there isn't 6 SenseAwake bits?
                //  a4 = (a1 >> 6) << 4 | ((a2 >> 6) << 2) | (a3 >> 6);

                // this does the same thing as behaviour
                a4 = (a3 >> 7) << 3 | ((a3 >> 6) & 1);

                bitmask = 1;
                for (int k = 0; k < 6; k++) {  // There are 6 flag sets per 2 minutes
                    if (a1 & bitmask) { OA->AddEvent(ti, 1); }
                    if (a2 & bitmask) { H->AddEvent(ti, 1); }
                    if (a3 & bitmask) { FL->AddEvent(ti, 1); }
                    if (a4 & bitmask) { SA->AddEvent(ti, 1); }

                    bitmask <<= 1;
                    ti += 20000L;  // Increment 20 seconds
                }

                idx += 5;
            }
        }

        //  sess->really_set_last(ti-360000L);
        //        sess->SetChanged(true);
        //       addSession(sess,profile);
    }
    if (p_profile->session->backupCardData()) {
        unsigned char *data = (unsigned char *)index.data();
        ts = data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;
        ts = convertDate(ts);

        QString backup = mach->getBackupPath()+"FPHCARE/ICON/"+serial.right(serial.size()-4)+"/";
        QDir dir;
        QString newname = QString("DET%1.FPH").arg(ts);

        dir.mkpath(backup);
        dir.cd(backup);
        if (!dir.exists(newname)) {
            file.copy(backup+newname);
        }
    }

    return 1;
}

bool fpicon_initialized = false;
void FPIconLoader::Register()
{
    if (fpicon_initialized) { return; }

    qDebug() << "Registering F&P Icon Loader";
    RegisterLoader(new FPIconLoader());
    //InitModelMap();
    fpicon_initialized = true;
}
