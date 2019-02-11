/* SleepLib RemStar M-Series Loader Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDir>
#include <QProgressBar>

#include "mseries_loader.h"


MSeries::MSeries(Profile *profile, MachineID id)
    : CPAP(profile, id)
{
}

MSeries::~MSeries()
{
}

MSeriesLoader::MSeriesLoader()
{
    m_type = MT_CPAP;

    epoch = QDateTime(QDate(2000, 1, 1), QTime(0, 0, 0), Qt::UTC).toTime_t();
    epoch -= QDateTime(QDate(1970, 1, 1), QTime(0, 0, 0), Qt::UTC).toTime_t();
}

MSeriesLoader::~MSeriesLoader()
{
}

//struct MSeriesHeader {
//    quint8   b1;            //0x52
//    quint32  a32;           //0x00000049
//    quint16  u16[8];
//    quint8   b2;            //0x02
//    char     setname[16];
//    char     firstname[25];
//    char     lastname[25];
//    char     serial[50];
//    quint16  b3;            //0x00
//    quint16  b4;            //0x66
//    quint16  b5;            //0xff

//} __attribute__((packed));
/*

blockLayoutOffsets   {
            cardInformationBlock = 0,
                brandID = 0,
                cardType = 2,
                cardVersion = 3
                startUIDB = 4
                endUIDB = 6,
                startCPB = 8,
                endCPB = 10,
                startCDCB = 12,
                endCDCB = 14,
                startCDB = 0x10,
                endCDB = 0x12,
                checksum = 20,

            userIDBlock = 0x15
                personalID = 1,
                patientFName = 0x11,
                patientLName = 0x2a,
                serialNumber = 0x43,
                modelNumber = 0x4d,
                textData = 0x57
                checksum = 0x77,

            cardPrescriptionBlock = 0x8d,


            cardDataControlBlock = 0xa3,
                validFlagOne = 3,
                headPtrOne = 4,
                tailPtrOne = 6,
                cdbChecksumOne = 8,
                validFlagTwo = 9
                headPtrTwo = 10,
                tailPtrTwo = 12,
                cdbChecksumTwo = 14,

            cardDataBlock = 0xb2,
                basicCompliance = 1,
                fosq = 2,
                Invalid = 0xff,
                sleepProfile = 8,
                sleepProfile2 = 10,
                sleepProfile3 = 14,
                sleepTrend = 9,
                sleepTrend2 = 11,
                sleepTrend3 = 15,
                smartAutoCPAPProfile = 3,
                smartAutoCPAPTrend = 4,
                ventCompliance2 = 13,
                ventilatorCompliance = 7,
                ventilatorProfile = 6,
                ventProfile2 = 12
                startChar = 0xfe,
                stopChar = 0x7f
                    */


int MSeriesLoader::Open(const QString & path)
{
    // Until a smartcard reader is written, this is not an auto-scanner.. it just opens a block file..

    QFile file(path);

    if (!file.exists()) { return 0; }

    if (file.size() != 32768) { // Check filesize matches smartcard?
        return 0;
    }

    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Couldn't open M-Series file:" << path;
        return 0;
    }

    QByteArray block = file.readAll();


    // Thanks to Phil Gillam for the pointers on this one..

    const unsigned char *cardinfo = (unsigned char *)block.data();
    quint16 magic = cardinfo[0] << 8 | cardinfo[1];

    if (magic != 0x5249) { // "RI" Respironics Magic number
        return 0;
    }

    //quint8 cardtype=cardinfo[2];
    //quint8 cardver=cardinfo[3];

    quint16 user_offset = (cardinfo[4] << 8) | cardinfo[5];
    //quint16 rx_offset=(cardinfo[8] << 8) | cardinfo[9];
    quint16 control_offset = (cardinfo[12] << 8) | cardinfo[13];
    //quint16 data_offset=(cardinfo[16] << 8) | cardinfo[17];


    const char *userinfo = block.data() + user_offset;
    QString setname = QString(userinfo + 0x1);
    QString firstname = QString(userinfo + 0x11);
    QString lastname = QString(userinfo + 0x2a);
    QString serial = QString(userinfo + 0x43);
    serial.truncate(10);
    QString model = QString(userinfo + 0x4d);
    QString textdata = QString(userinfo + 0x57);
    quint8 userinfochk = *(userinfo + 0x77);
    quint8 tmp = 0;

    for (int i = 0; i < 0x77; i++) {
        tmp += userinfo[i];
    }

    if (tmp != userinfochk) {
        qDebug() << "MSeries UserInfo block checksum failure" << path;
    }

    //const unsigned char * rxblock=(unsigned char *)block.data()+rx_offset;

    unsigned char *controlblock = (unsigned char *)block.data() + control_offset;
    quint16 count = controlblock[0] << 8 | controlblock[1]; // number of control blocks

    if (controlblock[1] != controlblock[2]) {
        qDebug() << "Control block count does not match." << path;
    }

    QList<quint16> head, tail;
    controlblock += 3;
    quint16 datastarts=0, dataends=0, h16, t16;//, tmp16,

    if (controlblock[0]) {
        datastarts = controlblock[1] | (controlblock[2] << 8);
        dataends = controlblock[3] | (controlblock[4] << 8);
    }

    controlblock += 6;

    if (controlblock[0]) {
        if ((controlblock[1] | (controlblock[2] << 8)) != datastarts) {
            qDebug() << "Non matching card size start identifier" << path;
        }

        if ((controlblock[3] | (controlblock[4] << 8)) != dataends) {
            qDebug() << "Non matching card size end identifier" << path;
        }
    }

    controlblock += 6;
    count -= 2;

    //tmp16 = controlblock[0] | controlblock[1] << 8;

    controlblock += 2;

    for (int i = 0; i < count / 2; i++) {
        if (controlblock[0]) {
            h16 = controlblock[1] | (controlblock[2] << 8);
            t16 = controlblock[3] | (controlblock[4] << 8);
            head.push_back(h16);
            tail.push_back(t16);
        }

        controlblock += 6;

        if (controlblock[0]) {
            if ((controlblock[1] | (controlblock[2] << 8)) != h16) {
                qDebug() << "Non matching control block head value" << path;
            }

            if ((controlblock[3] | (controlblock[4] << 8)) != t16) {
                qDebug() << "Non matching control block tail value" << path;
            }
        }

        controlblock += 6;
    }

    unsigned char *cb = controlblock;
    quint16 u1, u2, u3, u4, d1;
    quint32 ts, st; //, lt;
    QDateTime dt;
    QDate date;
    QTime time;

    for (int chk = 0; chk < 7; chk++) {
        ts = cb[0] << 24 | cb[1] << 16 | cb[2] << 8 | cb[3];
        //ts-=epoch;
        dt = QDateTime::fromTime_t(ts);
        date = dt.date();
        time = dt.time();
        qDebug() << "New Sparse Chunk" << chk << dt << hex << ts;

        cb += 4;
        quint8 sum = 0;

        for (int i = 0; i < 0x268; i++) { sum += cb[i]; }

        if (cb[0x268] == sum) {
            qDebug() << "Checksum bad for block" << chk << path;
        }

        cb += 0x26a;
    }

    unsigned char *endcard = (unsigned char *)block.data() + dataends;
    bool done = false;
    //qint64 ti;
    int cnt = 0;

    do {
        ts = cb[0] << 24 | cb[1] << 16 | cb[2] << 8 | cb[3];
        //lt =
        st = ts;
        //ti = qint64(ts) * 1000L;
        dt = QDateTime::fromTime_t(ts);
        date = dt.date();
        time = dt.time();
        qDebug() << "Details New Data Chunk" << cnt << dt << hex << ts;

        cb += 4;

        do {
            if (cb[0] == 0xfe) { // not sure what this means
                cb++;
            }

            u1 = cb[0] << 8 | cb[1]; // expecting 0xCXXX

            if (u1 == 0xffff) { // adjust timestamp code
                cb += 2;
                u1 = cb[0];
                cb++;

                if (cb[0] == 0xfe) {
                    u1 = cb[0] << 8 | cb[1]; // fe 0a, followed by timestamp
                    cb += 2;
                    break; // start on the next timestamp
                }
            } else {
                if ((cb[0] & 0xc0) == 0xc0) {
                    cb += 2;
                    u1 &= 0x0fff; // time delta??
                    //lt = ts;
                    ts = st + (u1 * 60);
                    //ti = qint64(ts) * 1000L;

                    d1 = cb[0] << 8 | cb[1];
                    u2 = cb[2] << 8 | cb[3];
                    u3 = cb[4] << 8 | cb[5];
                    u4 = cb[6] << 8 | cb[7];

                    if ((d1 != 0xf302) || (u2 != 0xf097) || (u3 != 0xf2ff) || (u4 != 0xf281)) {
                        qDebug() << "Lost details sync reading M-Series file" << path;
                        return false;
                    }

                    cb += 8;
                } else { cb++; }

                dt = QDateTime::fromTime_t(ts);
                qDebug() << "Details Data Chunk" << cnt++ << dt;

                do {
                    d1 = cb[0] << 8 | cb[1];
                    cb += 2;

                    if (d1 == 0x7f0a) { // end of entire block
                        done = true;
                        break;
                    }

                    if ((d1 & 0xb000) == 0xb000) {
                        qDebug() << "Duration" << (d1 & 0x7ff);
                        break; // end of section
                    }

                    // process binary data..
                    // 64 c0
                } while (cb < endcard);
            }
        } while (cb < endcard && !done);
    } while (cb < endcard && !done);

   // done = false;
    //bool first=true;
    //quint8 exch;
    cnt = 0;

    do {
        u1 = cb[0] << 8 | cb[1];

        if (u1 != 0xfe0b) {
           // done = true;
            break;
        }

        cb += 2;
        st = ts = cb[0] << 24 | cb[1] << 16 | cb[2] << 8 | cb[3];
        dt = QDateTime::fromTime_t(ts);
        date = dt.date();
        time = dt.time();
        //qDebug() << "Summary Data Chunk" << cnt << dt << hex << ts;
        cb += 4;

        while (cb < endcard) {
            if (((cb[0] & 0xc0) != 0xc0) || ((cb[0] & 0xf0) == 0xf0)) {
                // what is this for??
                //exch = cb[0];
                cb++;
            }

            u1 = (cb[0] << 8 | cb[1]) & 0x7ff; // time delta

            u2 = (cb[2] << 8 | cb[3]) & 0x7ff; // 0xBX XX??
            ts = st + u1 * 60;
            dt = QDateTime::fromTime_t(ts);
            //qDebug() << "Summary Sub Chunk" << dt << u1 << u2 << hex << ts;
            cb += 4;

            if (cb[0] == 0xff) { break; }
        }

        cb++; // ff;

        //        05905: "22 48 00 00 04 01 01 5C 9E 30 00 F0 00 01 73 00 00 00 F2  Sat Jul 9 2011 10:44:25"
        //        05905: "20 58 00 00 00 00 00 32 69 88 00 70 00 01 73 00 00 00 AF  Sun Jul 10 2011 05:09:21"
        //        05906: "22 00 00 00 0B 00 01 4E 79 F8 02 70 00 01 73 00 00 00 56  Sun Jul 10 2011 10:27:05"
        //        05907: "21 4C 00 00 11 00 01 5C 95 F8 01 F0 00 01 73 00 00 00 54  Mon Jul 11 2011 10:59:42"
        //        05908: "20 A8 00 00 02 00 01 4E 7D 88 00 F0 00 01 73 00 00 00 90  Tue Jul 12 2011 03:44:38"
        //        05909: "21 94 00 00 34 01 01 6A 96 D8 01 70 00 01 73 00 00 00 FC  Tue Jul 12 2011 10:30:49"
        //        05910: "21 84 00 00 19 01 01 6A A2 30 00 F0 00 01 73 00 00 00 3E  Wed Jul 13 2011 10:30:14"
        //        05911: "22 38 00 00 3F 01 01 86 B2 A0 00 F1 00 01 73 00 00 00 F4  Thu Jul 14 2011 10:01:50"
        //        05912: "21 68 00 00 36 01 01 5C 91 F8 02 70 00 01 73 00 00 00 BF  Fri Jul 15 2011 10:46:33"
        //        05913: "22 6C 0E 00 A1 01 01 78 AB 10 00 F0 00 01 73 00 00 00 9A  Sat Jul 16 2011 10:44:56"


        // 0x04 Vibratory Snore
        cnt++;
        QString a;

        for (int i = 0; i < 0x13; i++) {
            a += QString().sprintf("%02X ", cb[i]);
        }

        a += " " + date.toString() + " " + time.toString();
        qDebug() << a;
        cb += 0x13;
    } while (cb < endcard && !done);

    //graph data
    //starts with timestamp.. or time delta if high bit is set.

    //    validFlagOne = 3,
    //    headPtrOne = 4,
    //    tailPtrOne = 6,
    //    cdbChecksumOne = 8,
    //    validFlagTwo = 9
    //    headPtrTwo = 10,
    //    tailPtrTwo = 12,
    //    cdbChecksumTwo = 14,

    //    const char * datablock=block.data()+data_offset;
    //    quint8 basicCompliance=datablock[1];
    //    quint8 fosq=datablock[2];
    //    quint8 smartAutoCPAPProfile=datablock[3];
    //    quint8 smartAutoCPAPTrend=datablock[4];
    //    quint8 ventProfile=datablock[6];
    //    quint8 ventCompliance1=datablock[7];
    //    quint8 sleepProfile1=datablock[8];
    //    quint8 sleepTrend1=datablock[9];
    //    quint8 sleepProfile2=datablock[10];
    //    quint8 sleepTrend2=datablock[11];
    //    quint8 ventProfile2=datablock[12];
    //    quint8 ventCompliance2=datablock[13];
    //    quint8 sleepProfile3=datablock[14];
    //    quint8 sleepTrend3=datablock[15];


    // 0xa6: 01 00 b2 7f ff 31
    // 0xac: 01 00 b2 7f ff 31

    // 0xb2: ??? block... ?
    // 0xb2: 00 00

    // 0xb4: 01 36 a3 36 a2 b2    // the last bytes of all these are 8 bit additive checksums.
    // 0xba: 01 36 a3 36 a2 b2
    // 0xc0: 01 00 26 00 07 2e
    // 0xc6: 01 00 26 00 07 2e
    // 0xcc: 01 52 5a 58 e6 eb
    // 0xd2: 01 52 5a 58 e6 eb

    // repeat 8 times
    // 0xd8: 4e 1a 4a fe
    // 268 bytes
    // 1 byte checksum
    // starting at 0xD8, with timestamp?
    // 8 blocks of 0x26e in size

    // idx 0x159 =

    //    basicCompliance = 1,
    //    fosq = 2,
    //    sleepProfile = 8,
    //    sleepProfile2 = 10,
    //    sleepProfile3 = 14,
    //    sleepTrend = 9,
    //    sleepTrend2 = 11,
    //    sleepTrend3 = 15,
    //    smartAutoCPAPProfile = 3,
    //    smartAutoCPAPTrend = 4,
    //    ventCompliance2 = 13,
    //    ventilatorCompliance = 7,
    //    ventilatorProfile = 6,
    //    ventProfile2 = 12

    //    Invalid = 0xff,
    //    startChar = 0xfe,
    //    stopChar = 0x7f


    //Machine *mach=CreateMachine(serial,profile);


    // 0xcount till next block (between f3 02... blocks)
    // 0xc0 00 // varies
    // 0xf3 02 f0 97 f2 ff f2 81

    return 1;
}


bool mseries_initialized = false;
void MSeriesLoader::Register()
{
    if (mseries_initialized) { return; }

    qDebug() << "Registering RemStar M-Series Loader";
    RegisterLoader(new MSeriesLoader());
    //InitModelMap();
    mseries_initialized = true;
}
