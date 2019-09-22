/* SleepLib CMS50X Loader Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the cms50_data_version in cms50_loader.h when making changes to this loader
// that change loader behaviour or modify channels.
//********************************************************************************************

#include <QApplication>
#include <QDir>
#include <QString>
#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <QList>
#include <QMessageBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

#include "cms50_loader.h"
#include "SleepLib/machine.h"
#include "SleepLib/session.h"

CMS50Loader::CMS50Loader()
{
    m_type = MT_OXIMETER;
    m_abort = false;
    m_streaming = false;
    m_importing = false;
    imp_callbacks = 0;

    m_vendorID = 0x10c4;
    m_productID = 0xea60;
    cms50dplus = false;

    oxirec = nullptr;

    startTimer.setParent(this);
    resetTimer.setParent(this);

}

CMS50Loader::~CMS50Loader()
{
}

bool CMS50Loader::Detect(const QString &path)
{
    if (p_profile->oxi->oximeterType() == 1) {
        return true;
    }

    Q_UNUSED(path);
    return false;
}

int CMS50Loader::Open(const QString & path)
{
    // Only one active Oximeter module at a time, set in preferences

    m_itemCnt = 0;
    m_itemTotal = 0;

    m_abort = false;
    m_importing = false;

    started_import = false;
    started_reading = false;
    finished_import = false;
    setStatus(NEUTRAL);

    imp_callbacks = 0;
    cb_reset = 0;

    m_time.start();

    if (oxirec) {
        trashRecords();
    }

    // Cheating using path for two serial oximetry modes

    if (path.compare("import") == 0) {
        for (int i=0; i<5; ++i) {
            resetDevice();
            serial.flush();
//            QThread::msleep(50);
            QApplication::processEvents();
        }
        serial.clear();

        setStatus(IMPORTING);

        startTimer.stop();
        startImportTimeout();
        return 1;
    } else if (path.compare("live") == 0) {
        for (int i=0; i<5; ++i) {
            resetDevice();
            serial.flush();
            QApplication::processEvents();
        }
        serial.clear();

        m_startTime = QDateTime::currentDateTime();

        oxirec = new QVector<OxiRecord>;
        oxisessions[m_startTime] = oxirec;

        setStatus(LIVE);
        return 1;
    }
    QString ext = path.section(".", -1);	// find the last '.'
    if ((ext.compare("spo2", Qt::CaseInsensitive)==0) || (ext.compare("spo", Qt::CaseInsensitive)==0) || (ext.compare("spor", Qt::CaseInsensitive)==0)) {
        // try to read and process SpoR file..
        return readSpoRFile(path) ? 1 : 0;
    }

    return 0;
}


void CMS50Loader::processBytes(QByteArray bytes)
{
    // Sync to start of message type we are interested in
    quint8 c;
    quint8 msgcode = 0x80;

    int idx=0;
    int bytesread = bytes.size();
    while ((idx < bytesread) && (((c=(quint8)bytes.at(idx)) & msgcode)!=msgcode)) {
        if (buffer.length()>0) {
            // If buffer is the start of a valid but short frame, add to it..
            buffer.append(c);
        }// otherwise dump these bytes, as they are out of sequence.
        ++idx;
    }

    // Copy the rest to the buffer.
    buffer.append(bytes.mid(idx));

    int available = buffer.length();

    switch (status()) {
    case IMPORTING:
        idx = doImportMode();
        break;
    case LIVE:
        idx = doLiveMode();
        break;
    default:
        ;
//        qDebug() << "Device mode not supported by" << loaderName();
    }

    if (idx >= available) {
        buffer.clear();
    } else if (idx > 0) {
        // Trim any processed bytes from the buffer.
        buffer = buffer.mid(idx);
    }

    if (buffer.length() > 0) {
        // If what's left doesn't start with a marker bit, dump it
        if (((unsigned char)buffer.at(0) & 0x80) != 0x80) {
            buffer.clear();
        }
    }

}

int CMS50Loader::doImportMode()
{
    int available = buffer.size();

    int hour,minute;
    int idx = 0;
    while (idx < available) {
        unsigned char c=(unsigned char)buffer.at(idx);
        if (!started_import) {
            // There are three [0xf2 0xXX 0xXX] trio's at start of recording
            // Followed by [0xfX 0xXX 0xXX] trios containing spo2 and pulse till the end of recording

            // Scan for first header trio starting byte.
            if (c != 0xf2) {
                idx++;
                continue;
            }

            // sometimes a f2 starting trio can be corrupted by live data
            // peek ahead and see where the f2 headers are
            int f2cnt = 0;
            int f2idx[3] = {-1};

            for (int i=0; i < 30; ++i) {

                if ((idx + i) >= available) {
                    qDebug() << "Not enough bytes to read CMS50 headers";
                    break;
                }

                c = (unsigned char)buffer.at(idx+i);

                if (c == 0xf2) {
                    f2idx[f2cnt++] = idx+i;
                    if (f2cnt >= 3)
                        break;  // got all 3 headers

                    // Skip the following two bytes
                    i += 2;
                }
            }

            if (f2cnt<3) {
                qDebug() << "Did not get all header Trio's";
            }

            f2cnt--;

            // CHECK: Check there might be length data after the last header trio..


            received_bytes=0;
            bool badheader = false;

            // Look for the best of three headers trios
            int bestf2 = 0;

            if ((f2cnt >= 1) && ((f2idx[1] - f2idx[0]) == 3)) {
                bestf2 = f2idx[0];
            } else if ((f2cnt >= 2) && ((f2idx[2] - f2idx[1]) == 3)) {
                bestf2 = f2idx[1];
            } else {
                bestf2 = f2idx[f2cnt];

                // ouch.. check if f0 starts afterwards
                if (((unsigned char)buffer.at(bestf2+3) & 0xf0) != 0xf0) {
                    // crap.. bad time
                    badheader = true;
                }
            }

            if (!badheader) {
                hour = (unsigned char)buffer.at(bestf2 + 1) & 0x7f;
                minute = (unsigned char)buffer.at(bestf2 + 2) & 0x7f;
            } else {
                hour = 0;
                minute = 0;
            }

            // Either a CMS50D+, has a bad header, or it's really midnight, set a flag anyway for later to help choose the right sync time
            cms50dplus = (hour == 0) && (minute == 0);

            MachineInfo info = newInfo();
            info.model = cms50dplus ? QObject::tr("CMS50D+") : QObject::tr("CMS50E/F");
            info.serial = QString();
            Machine * mach = p_profile->CreateMachine(info);

            Q_UNUSED(mach);

            qDebug() << QString("Receiving Oximeter transmission %1:%2").arg(hour).arg(minute);
            // set importing to true or whatever..

            finished_import = false;
            started_import = true;
            started_reading = false;

            m_importing = true;

            m_itemCnt=0;
            m_itemTotal=5000;

            killTimers();
            qDebug() << "Getting ready for import";

            oxirec = new QVector<OxiRecord>;
            oxirec->reserve(30000);

            QDate oda=QDate::currentDate();
            QTime oti=QTime(hour,minute); // Only CMS50E/F's have a realtime clock. CMS50D+ will set this to midnight


            // If the oximeter record time is more than the current time, then assume it was from the day before
            // Or should I use split time preference instead??? Foggy Confusements..
            if (oti > QTime::currentTime()) {
                oda = oda.addDays(-1);
            }

            m_startTime = QDateTime(oda,oti);

            oxisessions[m_startTime] = oxirec;
            qDebug() << "Session start (according to CMS50)" << m_startTime << hex << buffer.at(idx + 1) << buffer.at(idx + 2) << ":" << dec << hour << minute ;

            cb_reset = 1;

            // CMS50D+ needs an end timer because it just stops dead after uploading
            resetTimer.singleShot(2000,this,SLOT(resetImportTimeout()));

            QStringList data;

            int len = f2idx[f2cnt]+3;
            for (int i=idx; i < len; ++i) {
                data.append(QString::number((unsigned char)buffer.at(i),16));
            }
            qDebug() << "CMS50 Record Header bytes:" << data.join(",");

            idx = len;

            // peek ahead to see if there really is data length bytes..
            data.clear();
            for (int i=0; i < 12; ++i) {
                if ((idx+i) > available) break;
                data.push_back(QString::number((unsigned char)buffer.at(idx+i), 16));
            }
            qDebug() << "bytes directly following header trio's:" << data.join(",");

        } else { // have started import
            if ((c & 0xf0) == 0xf0) { // Data trio
                started_reading=true; // Sometimes errornous crap is sent after data rec header

                // Recording import
                if ((idx + 2) >= available) {
                    return idx;
                }

                quint8 pulse=(unsigned char)((buffer.at(idx + 1) & 0x7f) | ((c & 1) << 7));
                quint8 spo2=(unsigned char)buffer.at(idx + 2) & 0xff;

                oxirec->append(OxiRecord(pulse,spo2));
                received_bytes+=3;

                // TODO: Store the data to the session

                emit updateProgress(0, 0);

                idx += 3;
            } else if (!started_reading) { // have not got a valid trio yet, skip...
                idx += 1;
            } else {
                // scan ahead for another 0xf0 in case it's corrupted..
                bool resync = false;

                for (int i=idx; i < available; ++i) {
                    c=(unsigned char)buffer.at(i);
                    if ((c & 0xf0) == 0xf0) {
                        idx = i;
                        resync = true;
                        break;
                    }
                }

                if (!resync) {
                    // Data transfer has completed
                    finished_import = true;
                    killTimers();
                    m_importing = false;
                    m_status = NEUTRAL;
                    emit importComplete(this);
                    resetTimer.singleShot(2000, this, SLOT(shutdownPorts()));
                    return available;
                }
            }
        }
    }

    if (!started_import) {
        imp_callbacks = 0;
    } else {
        imp_callbacks++;
    }

    return idx;
}

int CMS50Loader::doLiveMode()
{
    if (oxirec == nullptr) {
        qWarning() << "CMS50Loader::doLiveMode() called when null oxirec object";
        return 0;
    }

    int available = buffer.size();
    int idx = 0;

    QByteArray plethy;
    while (idx < available-5) {
        if (((unsigned char)buffer.at(idx) & 0x80) != 0x80) {
            idx++;
            continue;
        }
        int pwave=(unsigned char)buffer.at(idx + 1);
        int pbeat=(unsigned char)buffer.at(idx + 2);
        int pulse=((unsigned char)buffer.at(idx + 3) & 0x7f) | ((pbeat & 0x40) << 1);
        int spo2=(unsigned char)buffer.at(idx + 4) & 0x7f;

        oxirec->append(OxiRecord(pulse, spo2));
        plethy.append(pwave);

        idx += 5;
    }
    emit updatePlethy(plethy);

    return idx;
}

void CMS50Loader::resetDevice() // Switch CMS50D+ device to live streaming mode
{
    //qDebug() << "Sending reset code to CMS50 device";
    //m_port->flush();

    static unsigned char b1[3]={0xf6,0xf6,0xf6};

    if (serial.write((char *)b1,3) == -1) {
        qDebug() << "Couldn't write data reset bytes to CMS50";
    }

    QApplication::processEvents();
}

void CMS50Loader::requestData() // Switch CMS50D+ device to record transmission mode
{
    static unsigned char b1[2]={0xf5,0xf5};

    //qDebug() << "Sending request code to CMS50 device";
    if (serial.write((char *)b1,2) == -1) {
        qDebug() << "Couldn't write data request bytes to CMS50";
    }
    QApplication::processEvents();
}

void CMS50Loader::killTimers()
{
    startTimer.stop();
    resetTimer.stop();
}

void CMS50Loader::startImportTimeout()
{
    if (!m_streaming)
        return;

    if (started_import) {
        return;
    }
    if (finished_import != false) {
        qWarning() << "CMS50Loader::startImportTimeout() called when finished_import != false";
        return;
    }

    //qDebug() << "Starting oximeter import timeout";

    // Wait until events really are jammed on the CMS50D+ before re-requesting data.

    const int delay = 500;

    if (m_abort) {
        m_streaming = false;
        closeDevice();
        return;
    }

    if (imp_callbacks == 0) { // Frozen, but still hasn't started?
        m_itemCnt = m_time.elapsed();
        if (m_itemCnt > START_TIMEOUT) { // Give up after START_TIMEOUT
            closeDevice();
            abort();
            QMessageBox::warning(nullptr, STR_MessageBox_Error, "<h2>"+tr("Could not get data transmission from oximeter.")+"<br/><br/>"+tr("Please ensure you select 'upload' from the oximeter devices menu.")+"</h2>");
            return;
        } else {
            // Note: Newer CMS50 devices transmit from user input, but there is no way of differentiating between models
            requestData();
        }
        emit updateProgress(m_itemCnt, START_TIMEOUT);

        // Schedule another callback to make sure it's started
        startTimer.singleShot(delay, this, SLOT(startImportTimeout()));
    }
}

void CMS50Loader::resetImportTimeout()
{
    if (finished_import) {
        return;
    }

    if (imp_callbacks != cb_reset) {
        // Still receiving data.. reset timer
        qDebug() << "Still receiving data in resetImportTimeout()" << imp_callbacks << cb_reset;
        if (resetTimer.isActive())
            resetTimer.stop();

        if (!finished_import) resetTimer.singleShot(2000, this, SLOT(resetImportTimeout()));
    } else {
        qDebug() << "Oximeter device stopped transmitting.. Transfer complete";
        // We were importing, but now are done
        if (!finished_import && (started_import && started_reading)) {
            qDebug() << "Switching CMS50 back to live mode and finalizing import";
            // Turn back on live streaming so the end of capture can be dealt with
            resetTimer.stop();

            resetDevice(); // Send Reset to CMS50D+
            serial.flush();
            QThread::msleep(200);
            resetDevice(); // Send Reset to CMS50D+
            serial.flush();
            serial.clear();
            //started_import = false;
           // finished_import = true;
            //m_streaming=false;

            //closeDevice();
            //emit transferComplete();
            //doImportComplete();
            return;
        }
        qDebug() << "Should CMS50 resetImportTimeout reach here?";
        // else what???
    }
    cb_reset = imp_callbacks;
}

void CMS50Loader::shutdownPorts()
{
    closeDevice();
}




bool CMS50Loader::readSpoRFile(QString path)
{
    QFile file(path);
    if (!file.exists()) {
    	qWarning() << "Can't find the oximeter file: " << path;
        QMessageBox::warning(nullptr, STR_MessageBox_Error, "<h2>"+tr("Could not find the oximeter file:")+"<br/><br/>"+path+"</h2>");
        return false;
    }

    if (!file.open(QFile::ReadOnly)) {
    	qWarning() << "Can't open the oximeter file: " << path;
        QMessageBox::warning(nullptr, STR_MessageBox_Error, "<h2>"+tr("Could not open the oximeter file:")+"<br/><br/>"+path+"</h2>");
        return false;
    }

    bool spo2header = false;
    QString ext = path.section('.', -1);
    qDebug() << "Oximeter file extention is " << ext;
    if (ext.compare("spo2",Qt::CaseInsensitive) == 0) {
        spo2header = true;
        qDebug() << "Oximeter file looks like an SpO2 type" ;
    }

    QByteArray data;

    qint64 filesize = file.size();
    data = file.readAll();
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    quint16 pos;
    in >> pos;

    in.skipRawData(pos - 2);

    //long size = data.size();
    int bytes_per_record = 2;

    if (!spo2header) {
        // next is 0x0002
        // followed by 16bit duration in seconds

        // Read date and time (it's a 16bit charset)

        char dchr[20];
        int j = 0;
        for (int i = 0; i < 18 * 2; i += 2) {
            dchr[j++] = data.at(8 + i);
        }

        dchr[j] = 0;
        if (dchr[0]) {
            QString dstr(dchr);
            m_startTime = QDateTime::fromString(dstr, "MM/dd/yy HH:mm:ss");
            if (m_startTime.date().year() < 2000) {
                m_startTime = m_startTime.addYears(100);
            }
        } else {
            m_startTime = QDateTime(QDate::currentDate(), QTime(0,0,0));
            cms50dplus = true;
        }
    } else { // it is an spo2header

        quint32 samples = 0; // number of samples

        quint32 year, month, day;
        quint32 hour, minute, second;

        if (data.at(pos) != 1) {
            qWarning() << "oximeter file" << path << "might be odd format";
        }

        // Unknown cruft header...
        in.skipRawData(200);

        in >> year >> month >> day;
        in >> hour >> minute >> second;
        
        if ( year == 0 ) {    // typically from a CMS50D+
            m_startTime = QDateTime(QDate::currentDate(), QTime(hour, minute, second));
            cms50dplus = true;
        }
        else
            m_startTime = QDateTime(QDate(year, month, day), QTime(hour, minute, second));

        // ignoring it for now
        pos += 0x1c + 200;

        in >> samples;

        int remainder = filesize - pos;

        bytes_per_record = remainder / samples;
        qDebug() << samples << "samples of" << bytes_per_record << "bytes each";

        // CMS50I .spo2 data have 4 bytes: a 16bit, followed by spo2 then pulse

    }

    oxirec = new QVector<OxiRecord>;
    oxisessions[m_startTime] = oxirec;

    unsigned char o2, pr;
    quint16 un;

    // Read all Pulse and SPO2 data
    do {
        if (bytes_per_record > 2) {
            in >> un;
        }
        in >> o2;
        in >> pr;

        if ((o2 == 0x7f) && (pr == 0xff)) {
            o2 = pr = 0;
            un = 0;
        }

        if (spo2header) {
            oxirec->append(OxiRecord(pr, o2));
        } else {
            oxirec->append(OxiRecord(o2, pr));
        }
    } while (!in.atEnd());


//    for (int i = pos; i < size - 2;) {
//        o2 = (unsigned char)(data.at(i + 1));
//        pr = (unsigned char)(data.at(i + 0));
//        oxirec->append(OxiRecord(pr, o2));
//        i += 2;
//    }

    // processing gets done later
    return true;
}

void CMS50Loader::process()
{
    // Just clean up any extra crap before oximeterimport parses the oxirecords..
    return;
//    if (!oxirec)
//        return;
//    int size=oxirec->size();
//    if (size<10)
//        return;

}



static bool cms50_initialized = false;

void CMS50Loader::Register()
{
    if (cms50_initialized) { return; }

    qDebug() << "Registering CMS50Loader";
    RegisterLoader(new CMS50Loader());
    cms50_initialized = true;
}

