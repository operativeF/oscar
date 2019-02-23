/* SleepLib ChoiceMMed MD300W1 Oximeter Loader Implementation
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the md300w1_data_version in md300w1_loader.h when making changes to this
// loader that change loader behaviour or modify channels.
//********************************************************************************************

#include <QProgressBar>
#include <QApplication>
#include <QDir>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <QList>
#include <QMessageBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

using namespace std;

#include "md300w1_loader.h"
#include "SleepLib/machine.h"
#include "SleepLib/session.h"

MD300W1Loader::MD300W1Loader()
{
    m_type = MT_OXIMETER;
    m_abort = false;
    m_streaming = false;
    m_importing = false;
    imp_callbacks = 0;

    // have no idea.. assuming it's another CP2102 USB UART, which won't help detection :/
    m_vendorID = 0;
    m_productID = 0;

    startTimer.setParent(this);
    resetTimer.setParent(this);

}

MD300W1Loader::~MD300W1Loader()
{
}

bool MD300W1Loader::Detect(const QString &path)
{
    Q_UNUSED(path);
    return false;
}

int MD300W1Loader::Open(const QString & path)
{
    // Only one active Oximeter module at a time, set in preferences

	qDebug() << "MD300W1 Loader opening " << path;

    m_itemCnt = 0;
    m_itemTotal = 0;

    m_abort = false;
    m_importing = false;

    started_import = false;
    started_reading = false;
    finished_import = false;

    imp_callbacks = 0;
    cb_reset = 0;

    m_time.start();

    // Cheating using path for two serial oximetry modes

    if (path.compare("import") == 0) {
        setStatus(IMPORTING);

        startTimer.stop();
        startImportTimeout();
        return 1;
    } else if (path.compare("live") == 0) {
        m_startTime = oxitime = QDateTime::currentDateTime();
        setStatus(LIVE);
        return 1;
    }
    QString ext = path.section(".", -1);	// find the last '.'
    if (ext.compare("dat", Qt::CaseInsensitive)==0) {
        // try to read and process SpoR file..
        return readDATFile(path) ? 1 : 0;
    }

    return 0;
}


void MD300W1Loader::processBytes(QByteArray bytes)
{
    Q_UNUSED(bytes);
    return;
}

int MD300W1Loader::doImportMode()
{
    return 0;
}

int MD300W1Loader::doLiveMode()
{
    return 0;
}

// Switch MD300W1 device to live streaming mode
void MD300W1Loader::resetDevice()
{
}

// Switch MD300W1 device to record transmission mode
void MD300W1Loader::requestData()
{
}

void MD300W1Loader::killTimers()
{
    startTimer.stop();
    resetTimer.stop();
}

void MD300W1Loader::startImportTimeout()
{
    return;
}

void MD300W1Loader::resetImportTimeout()
{
    return;
}


// MedView .dat file (ChoiceMMed MD300B, MD300KI, MD300I, MD300W1, MD300C318, MD2000A)
// Format:
// Bytes  0  (1  2)
//        id    n
// n*11   0  1  2  3  4  5  6  7  8  9  10
//        0  0  id yr mm dd hh mm ss o2 pulse
// report title etc.

bool MD300W1Loader::readDATFile(const QString & path)
{
    QFile file(path);
    
    qDebug() << "MD300W Loader attempting to read " << path;
    
    if (!file.exists()) {
        qDebug() << "File does not exist: " << path;
        QMessageBox::warning(nullptr, STR_MessageBox_Error, "<h2>"+tr("Could not find the oximeter file:")+"<br/><br/>"+path+"</h2>");
        return false;
    }

    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Can't open file R/O: " << path;
        QMessageBox::warning(nullptr, STR_MessageBox_Error, "<h2>"+tr("Could not open the oximeter file:")+"<br/><br/>"+path+"</h2>");
        return false;
    }

    QByteArray data;

    data = file.readAll();
    long size = data.size();

    // Number of records
    int n = ((unsigned char)data.at(2) << 8) | (unsigned char)data.at(1);

    // CHECKME:
    if (size < (n*11)+3) {
        qDebug() << "Short MD300W1 .dat file" << path;
        return false;
    }


    unsigned char o2, pr;

    qint32 lasttime=0, ts=0;
    int gap;
    for (int pos = 0; pos < n; ++pos) {
        int i = 3 + (pos * 11);
        QString datestr = QString().sprintf("%02d/%02d/%02d %02d:%02d:%02d",(unsigned char)data.at(i+4),(unsigned char)data.at(i+5),(unsigned char)data.at(i+3),(unsigned char)data.at(i+6),(unsigned char)data.at(i+7),(unsigned char)data.at(i+8));
        QDateTime datetime = QDateTime::fromString(datestr,"MM/dd/yy HH:mm:ss");
        if (datetime.date().year() < 2000) datetime = datetime.addYears(100);
        ts = datetime.toTime_t();
        gap = ts - lasttime;
        if (gap > 1) {			// always true for first record, b/c time started on 1 Jan 1970
            if (gap < 360) {
                // Less than 5 minutes? Merge session
                gap--;
                // fill with zeroes
                for (int j = 0; j < gap; j++) {
                    oxirec->append(OxiRecord(0,0));
                }
            } else {
                // Create a new session, always for first record
                qDebug() << "Create session for " << datetime.toString("yyyy.MM.dd HH:mm:ss");
                oxirec = new QVector<OxiRecord>;
                oxisessions[datetime] = oxirec;
                m_startTime = datetime; // works for single session files... 
            }
        }

        pr=(unsigned char)(data.at(i+10));
        o2=(unsigned char)(data.at(i+9));

        oxirec->append(OxiRecord(pr, o2));

        lasttime = ts;
    }

    // processing gets done later
    return true;
}

void MD300W1Loader::process()
{
}


static bool MD300W1_initialized = false;

void MD300W1Loader::Register()
{
    if (MD300W1_initialized) { return; }

    qDebug() << "Registering MD300W1Loader";
    RegisterLoader(new MD300W1Loader());
    MD300W1_initialized = true;
}

