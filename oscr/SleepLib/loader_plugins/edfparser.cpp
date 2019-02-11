/* EDF Parser Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMutexLocker>
#ifdef _MSC_VER
#include <QtZlib/zlib.h>
#else
#include <zlib.h>
#endif

#include "edfparser.h"

EDFParser::EDFParser(QString name)
{
    buffer = nullptr;
    if (!name.isEmpty())
        Open(name);
}
EDFParser::~EDFParser()
{
    for (auto & s : edfsignals) {
        if (s.data) { delete [] s.data; }
    }
}
// Read a 16 bits integer
qint16 EDFParser::Read16()
{
    if ((pos + 2) > datasize) {
        eof = true;
        return 0;
    }

#ifdef Q_LITTLE_ENDIAN
    // Intel, etc...
    qint16 res = *(qint16 *)&buffer[pos];
#else
    // ARM, PPC, etc..
    qint16 res = quint8(buffer[pos]) | (qint8(buffer[pos+1]) << 8);
#endif

    pos += 2;
    return res;
}

QString EDFParser::Read(unsigned n)
{
    if ((pos + long(n)) > datasize) {
        eof = true;
        return QString();
    }

    QByteArray buf(&buffer[pos], n);
    pos+=n;

    return buf.trimmed();
}
bool EDFParser::Parse()
{
    bool ok;

    if (header == nullptr) {
        qWarning() << "EDFParser::Parse() called without valid EDF data" << filename;
        return false;
    }

    eof = false;
    version = QString::fromLatin1(header->version, 8).toLong(&ok);
    if (!ok) {
        return false;
    }

    //patientident=QString::fromLatin1(header.patientident,80);
    recordingident = QString::fromLatin1(header->recordingident, 80); // Serial number is in here..
    int snp = recordingident.indexOf("SRN=");
    serialnumber.clear();

    for (int i = snp + 4; i < recordingident.length(); i++) {
        if (recordingident[i] == ' ') {
            break;
        }

        serialnumber += recordingident[i];
    }

    startdate_orig = QDateTime::fromString(QString::fromLatin1(header->datetime, 16), "dd.MM.yyHH.mm.ss");

    QDate d2 = startdate_orig.date();

    if (d2.year() < 2000) {
        d2.setDate(d2.year() + 100, d2.month(), d2.day());
        startdate_orig.setDate(d2);
    }

    if (!startdate_orig.isValid()) {
        qDebug() << "Invalid date time retreieved parsing EDF File " << filename;
        return false;
    }

    startdate = qint64(startdate_orig.toTime_t()) * 1000L;
    //startdate-=timezoneOffset();

    //qDebug() << startDate.toString("yyyy-MM-dd HH:mm:ss");

    num_header_bytes = QString::fromLatin1(header->num_header_bytes, 8).toLong(&ok);

    if (!ok) {
        return false;
    }

    //reserved44=QString::fromLatin1(header.reserved,44);
    num_data_records = QString::fromLatin1(header->num_data_records, 8).toLong(&ok);

    if (!ok) {
        return false;
    }

    dur_data_record = (QString::fromLatin1(header->dur_data_records, 8).toDouble(&ok) * 1000.0L);

    if (!ok) {
        return false;
    }

    num_signals = QString::fromLatin1(header->num_signals, 4).toLong(&ok);

    if (!ok) {
        return false;
    }

    enddate = startdate + dur_data_record * qint64(num_data_records);
    // if (dur_data_record==0)
    //   return false;

    // this could be loaded quicker by transducer_type[signal] etc..

    // Initialize fixed-size signal list.
    edfsignals.resize(num_signals);

    for (auto & sig : edfsignals) {
        sig.data = nullptr;
        sig.label = Read(16);

        signal_labels.push_back(sig.label);
        signalList[sig.label].push_back(&sig);
        if (eof) return false;
    }

    for (auto & sig : edfsignals) { sig.transducer_type = Read(80); }
    for (auto & sig : edfsignals) { sig.physical_dimension = Read(8); }
    for (auto & sig : edfsignals) { sig.physical_minimum = Read(8).toDouble(&ok); }
    for (auto & sig : edfsignals) { sig.physical_maximum = Read(8).toDouble(&ok); }
    for (auto & sig : edfsignals) { sig.digital_minimum = Read(8).toDouble(&ok); }
    for (auto & sig : edfsignals) { sig.digital_maximum = Read(8).toDouble(&ok);
        sig.gain = (sig.physical_maximum - sig.physical_minimum) / (sig.digital_maximum - sig.digital_minimum);
        sig.offset = 0;
    }
    for (auto & sig : edfsignals) { sig.prefiltering = Read(80); }
    for (auto & sig : edfsignals) { sig.nr = Read(8).toLong(&ok); }
    for (auto & sig : edfsignals) { sig.reserved = Read(32); }

    // could do it earlier, but it won't crash from > EOF Reads
    if (eof) return false;

    // Now check the file isn't truncated before allocating all the memory
    long allocsize = 0;
    for (auto & sig : edfsignals) {
        if (num_data_records > 0) {
            allocsize += sig.nr * num_data_records * 2;
        }
    }
    if (allocsize > (datasize - pos)) {
        // Space required more than the remainder left to read,
        // so abort and let the user clean up the corrupted file themselves
        qWarning() << "EDFParser::Parse():" << filename << " is truncated!";
        return false;
    }

    // allocate the buffers
    for (auto & sig : edfsignals) {
        long recs = sig.nr * num_data_records;

        if (num_data_records <= 0) {
            sig.data = nullptr;
            continue;
        }

        sig.data = new qint16 [recs];
        sig.pos = 0;
    }


    for (int x = 0; x < num_data_records; x++) {
        for (auto & sig : edfsignals) {
#ifdef Q_LITTLE_ENDIAN
            // Intel x86, etc..
            memcpy((char *)&sig.data[sig.pos], (char *)&buffer[pos], sig.nr * 2);
            sig.pos += sig.nr;
            pos += sig.nr * 2;
#else
            // Big endian safe
            for (int j=0;j<sig.nr;j++) {
                qint16 t=Read16();
                sig.data[sig.pos++]=t;
            }
#endif
        }
    }

    return true;
}

QByteArray gUncompress(const QByteArray &data);

bool EDFParser::Open(const QString & name)
{
    if (buffer != nullptr) {
        qWarning() << "EDFParser::Open() called with file already open" << name;
        return false;
    }

    QFile fi(name);
    if (!fi.open(QFile::ReadOnly)) {
        goto badfile;
    }

    if (name.endsWith(STR_ext_gz)) {
        filename = name; //name.mid(0, -3); // DoubleCheck: why am I cropping the extension? this is used for debugging

        // Open and decempress file
        data = gUncompress(fi.readAll());
    } else {
        // Open and read uncompressed file
        filename = name;
        data = fi.readAll();
    }
    fi.close();

    filesize = data.size();

    if (filesize > EDFHeaderSize) {
        header = (EDFHeader *)data.constData();
        buffer = (char *)data.constData() + EDFHeaderSize;
        datasize = filesize - EDFHeaderSize;
    } else goto badfile;

    pos = 0;
    return true;

badfile:
    filesize = 0;
    datasize = 0;
    buffer = nullptr;
    header = nullptr;
    data.clear();
    qDebug() << "EDFParser::Open() Couldn't open file" << name;
    return false;
}

EDFSignal *EDFParser::lookupLabel(const QString & name, int index)
{
    auto it = signalList.find(name);
    if (it == signalList.end()) return nullptr;

    if (index >= it.value().size()) return nullptr;

    return it.value()[index];
}

//QMutex EDFParser::EDFMutex;
