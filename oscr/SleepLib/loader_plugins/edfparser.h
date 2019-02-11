/* EDF Parser Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef EDFPARSER_H
#define EDFPARSER_H

#include <QString>
#include <QVector>
#include <QHash>
#include <QList>
#include <QMutex>

#include "SleepLib/common.h"

const QString STR_ext_EDF = "edf";
const QString STR_ext_gz = ".gz";

/*! \struct EDFHeader
    \brief  Represents the EDF+ header structure, used as a place holder while processing the text data.
    \note More information on the EDF+ file format can be obtained from http://edfplus.info
*/
struct EDFHeader {
    char version[8];
    char patientident[80];
    char recordingident[80];
    char datetime[16];
    char num_header_bytes[8];
    char reserved[44];
    char num_data_records[8];
    char dur_data_records[8];
    char num_signals[4];
}
#ifndef _MSC_VER
__attribute__((packed))
#endif
;

const int EDFHeaderSize = sizeof(EDFHeader);

/*! \struct EDFSignal
    \brief Contains information about a single EDF+ Signal
    \note More information on the EDF+ file format can be obtained from http://edfplus.info
    */
struct EDFSignal {
  public:
    //! \brief Name of this Signal
    QString label;

    //! \brief Tranducer Type (source of the data, usually blank)
    QString transducer_type;

    //! \brief The units of measurements represented by this signal
    QString physical_dimension;

    //! \brief The minimum limits of the ungained data
    EventDataType physical_minimum;

    //! \brief The maximum limits of the ungained data
    EventDataType physical_maximum;

    //! \brief The minimum limits of the data with gain and offset applied
    EventDataType digital_minimum;

    //! \brief The maximum limits of the data with gain and offset applied
    EventDataType digital_maximum;

    //! \brief Raw integer data is multiplied by this value
    EventDataType gain;

    //! \brief This value is added to the raw data
    EventDataType offset;

    //! \brief Any prefiltering methods used (usually blank)
    QString prefiltering;

    //! \brief Number of records
    long nr;

    //! \brief Reserved (usually blank)
    QString reserved;

    //! \brief Pointer to the signals sample data
    qint16 *data;

    //! \brief a non-EDF extra used internally to count the signal data
    int pos;
};


/*! \class EDFParser
    \author Mark Watkins <mark@jedimark.net>
    \brief Parse an EDF+ data file into a list of EDFSignal's
    \note More information on the EDF+ file format can be obtained from http://edfplus.info
    */
class EDFParser
{
  public:
    //! \brief Constructs an EDFParser object, opening the filename if one supplied
    EDFParser(QString filename = "");

    ~EDFParser();

    //! \brief Open the EDF+ file, and read it's header
    bool Open(const QString & name);

    //! \brief Read n bytes of 8 bit data from the EDF+ data stream
    QString Read(unsigned n);

    //! \brief Read 16 bit word of data from the EDF+ data stream
    qint16 Read16();

    //! \brief Vector containing the list of EDFSignals contained in this edf file
    QVector<EDFSignal> edfsignals;

    //! \brief An by-name indexed into the EDFSignal data
    QStringList signal_labels;

    //! \brief ResMed likes to use the SAME signal name
    QHash<QString, QList<EDFSignal *> > signalList;

    EDFSignal *lookupLabel(const QString & name, int index=0);

    //! \brief Returns the number of signals contained in this EDF file
    long GetNumSignals() { return num_signals; }

    //! \brief Returns the number of data records contained per signal.
    long GetNumDataRecords() { return num_data_records; }

    //! \brief Returns the duration represented by this EDF file (in milliseconds)
    qint64 GetDuration() { return dur_data_record; }

    //! \brief Returns the patientid field from the EDF header
    QString GetPatient() { return patientident; }

    //! \brief Parse the EDF+ file into the list of EDFSignals.. Must be call Open(..) first.
    bool Parse();
    char *buffer;

    //! \brief  The EDF+ files header structure, used as a place holder while processing the text data.
    EDFHeader *header;
    QByteArray data;

    QString filename;
    long filesize;
    long datasize;
    long pos;

    long version;
    long num_header_bytes;
    long num_data_records;
    qint64 dur_data_record;
    long num_signals;

    QString patientident;
    QString recordingident;
    QString serialnumber;
    QDateTime startdate_orig;
    qint64 startdate;
    qint64 enddate;
    QString reserved44;
//    static QMutex EDFMutex;
    bool eof;
};


#endif // EDFPARSER_H
