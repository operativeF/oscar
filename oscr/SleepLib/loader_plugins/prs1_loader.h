/* SleepLib PRS1 Loader Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef PRS1LOADER_H
#define PRS1LOADER_H
//#include <map>
//using namespace std;
#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"


//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation
// BEFORE making a release
const int prs1_data_version = 15;
//
//********************************************************************************************

/*! \class PRS1
    \brief PRS1 customized machine object (via CPAP)
    */
class PRS1: public CPAP
{
  public:
    PRS1(Profile *, MachineID id = 0);
    virtual ~PRS1();
};


const int max_load_buffer_size = 1024 * 1024;
const QString prs1_class_name = STR_MACH_PRS1;

/*! \struct PRS1Waveform
    \brief Used in PRS1 Waveform Parsing */
struct PRS1Waveform {
    PRS1Waveform(quint16 i, quint8 f) {
        interleave = i;
        sample_format = f;
    }
    quint16 interleave;
    quint8 sample_format;
};


/*! \class PRS1DataChunk
 *  \brief Representing a chunk of event/summary/waveform data after the header is parsed. */
class PRS1DataChunk
{
    friend class PRS1DataGroup;
public:
    PRS1DataChunk() {
        timestamp = 0;
        ext = 255;
        sessionid = 0;
        htype = 0;
        family = 0;
        familyVersion = 0;
        duration = 0;

    }
    ~PRS1DataChunk() {
    }
    inline int size() const { return m_data.size(); }

    QByteArray m_data;
    QByteArray m_headerblock;

    SessionID sessionid;

    quint8 fileVersion;
    quint8 ext;
    quint8 htype;
    quint8 family;
    quint8 familyVersion;
    quint32 timestamp;

    quint16 duration;

    QList<PRS1Waveform> waveformInfo;
    QMap<unsigned char, short> hblock;
};

class PRS1Loader;

/*! \class PRS1Import
 *  \brief Contains the functions to parse a single session... multithreaded */
class PRS1Import:public ImportTask
{
public:
    PRS1Import(PRS1Loader * l, SessionID s, Machine * m): loader(l), sessionid(s), mach(m) {
        summary = nullptr;
        compliance = nullptr;
        event = nullptr;
        session = nullptr;
    }
    virtual ~PRS1Import() {
        delete compliance;
        delete summary;
        delete event;
        for (int i=0;i < waveforms.size(); ++i) { delete waveforms.at(i); }
    }

    //! \brief PRS1Import thread starts execution here.
    virtual void run();

    PRS1DataChunk * compliance;
    PRS1DataChunk * summary;
    PRS1DataChunk * event;
    QList<PRS1DataChunk *> waveforms;
    QList<PRS1DataChunk *> oximetry;

    QMap<unsigned char, QByteArray> mainblock;
    QMap<unsigned char, QByteArray> hbdata;


    QString wavefile;
    QString oxifile;

    //! \brief As it says on the tin.. Parses .001 files for bricks.
    bool ParseCompliance();

    //! \brief Figures out which Summary Parser to call, based on machine family/version and calls it.
    bool ParseSummary();

    //! \brief Figures out which Event Parser to call, based on machine family/version and calls it.
    bool ParseEvents();

    //! \brief Takes the parsed list of Flow/MaskPressure waveform chunks and adds them to the database
    bool ParseWaveforms();

    //! \brief Takes the parsed list of oximeter waveform chunks and adds them to the database.
    bool ParseOximetery();


    //! \brief Summary parser for 50 series Family 0 CPAP/APAP models
    bool ParseSummaryF0();
    //! \brief Summary parser for 60 series Family 0 CPAP/APAP models
    bool ParseSummaryF0V4();
    //! \brief Summary parser for 1060 series AVAPS models
    bool ParseSummaryF3();
    //! \brief Summary parser for 50 series Family 5-0 BiPAP/AutoSV models
    bool ParseSummaryF5V0();
    //! \brief Summary parser for 60 series Family 5-1 BiPAP/AutoSV models
    bool ParseSummaryF5V1();
    //! \brief Summary parser for 60 series Family 5-2 BiPAP/AutoSV models
    bool ParseSummaryF5V2();
    //! \brief Summary parser for 60 series Family 5-2 BiPAP/AutoSV models
    bool ParseSummaryF5V3();

    //! \brief Summary parser for DreamStation series CPAP/APAP models
    bool ParseSummaryF0V6();

    //! \brief Parse a single data chunk from a .002 file containing event data for a standard system one machine
    bool ParseF0Events();
    //! \brief Parse a single data chunk from a .002 file containing event data for a AVAPS 1060P machine
    bool ParseF3Events();
    //! \brief Parse a single data chunk from a .002 file containing event data for a AVAPS 1060P machine file version 3
    bool ParseF3EventsV3();
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV machine (which has a different format)
    bool ParseF5Events();
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV file version 3 machine (which has a different format again)
    bool ParseF5EventsFV3();


protected:
    Session * session;
    PRS1Loader * loader;
    SessionID sessionid;
    Machine * mach;

    int summary_duration;
};

/*! \class PRS1Loader
    \brief Philips Respironics System One Loader Module
    */
class PRS1Loader : public CPAPLoader
{
    Q_OBJECT
  public:
    PRS1Loader();
    virtual ~PRS1Loader();

    //! \brief Examine path and return it back if it contains what looks to be a valid PRS1 SD card structure
    QString checkDir(const QString & path);

    //! \brief Peek into PROP.TXT or properties.txt at given path, and use it to fill MachineInfo structure
    bool PeekProperties(MachineInfo & info, const QString & path, Machine * mach = nullptr);

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Wrapper for PeekProperties that creates the MachineInfo structure.
    virtual MachineInfo PeekInfo(const QString & path);

    //! \brief Scans directory path for valid PRS1 signature
    virtual int Open(const QString & path);

    //! \brief Returns the database version of this loader
    virtual int Version() { return prs1_data_version; }

    //! \brief Return the loaderName, in this case "PRS1"
    virtual const QString &loaderName() { return prs1_class_name; }

    //! \brief Parse a PRS1 summary/event/waveform file and break into invidivual session or waveform chunks
    QList<PRS1DataChunk *> ParseFile(const QString & path);

    //! \brief Register this Module to the list of Loaders, so it knows to search for PRS1 data.
    static void Register();

    //! \brief Generate a generic MachineInfo structure, with basic PRS1 info to be expanded upon.
    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, prs1_class_name, QObject::tr("Philips Respironics"), QString(), QString(), QString(), QObject::tr("System One"), QDateTime::currentDateTime(), prs1_data_version);
    }


    virtual QString PresReliefLabel() { return QObject::tr(""); }
    //! \brief Returns the PRS1 specific code for Pressure Relief Mode
    virtual ChannelID PresReliefMode() { return PRS1_FlexMode; }
    //! \brief Returns the PRS1 specific code for Pressure Relief Setting
    virtual ChannelID PresReliefLevel() { return PRS1_FlexLevel; }

    //! \brief Returns the PRS1 specific code for Humidifier Connected
    virtual ChannelID HumidifierConnected() { return PRS1_HumidStatus; }
    //! \brief Returns the PRS1 specific code for Humidifier Level
    virtual ChannelID HumidifierLevel() { return PRS1_HumidLevel; }

    //! \brief Called at application init, to set up any custom PRS1 Channels
    void initChannels();


    QHash<SessionID, PRS1Import*> sesstasks;
    QMap<unsigned char, QStringList> unknownCodes;

  protected:
    QString last;
    QHash<QString, Machine *> PRS1List;

    //! \brief Opens the SD folder structure for this machine, scans for data files and imports any new sessions
    int OpenMachine(const QString & path);

//    //! \brief Parses "properties.txt" file containing machine information
//    bool ParseProperties(Machine *m, QString filename);

    //! \brief Parse a .005 waveform file, extracting Flow Rate waveform (and Mask Pressure data if available)
    bool OpenWaveforms(SessionID sid, const QString & filename);

    //! \brief Parse a data chunk from the .000 (brick) and .001 (summary) files.
    bool ParseSummary(Machine *mach, qint32 sequence, quint32 timestamp, unsigned char *data,
                      quint16 size, int family, int familyVersion);



    //! \brief Open a PRS1 data file, and break into data chunks, delivering them to the correct parser.
    bool OpenFile(Machine *mach, const QString & filename);

    QHash<SessionID, Session *> extra_session;

    //! \brief PRS1 Data files can store multiple sessions, so store them in this list for later processing.
    QHash<SessionID, Session *> new_sessions;

    qint32 summary_duration;
};

#endif // PRS1LOADER_H
