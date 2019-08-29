/* SleepLib PRS1 Loader Header
 *
 * Copyright (c) 2019 The OSCAR Team
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

#ifdef UNITTEST_MODE
#define private public
#define protected public
#endif

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation
// BEFORE making a release
const int prs1_data_version = 16;
//
//********************************************************************************************
#if 0  // Apparently unused
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
#endif
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
        fileVersion = 0;
        blockSize = 0;
        htype = 0;
        family = 0;
        familyVersion = 0;
        ext = 255;
        sessionid = 0;
        timestamp = 0;
        
        duration = 0;

        m_filepos = -1;
        m_index = -1;
    }
    PRS1DataChunk(class QFile & f);
    ~PRS1DataChunk();
    inline int size() const { return m_data.size(); }

    QByteArray m_header;
    QByteArray m_data;
    QByteArray m_headerblock;
    QList<class PRS1ParsedEvent*> m_parsedData;

    QString m_path;
    qint64 m_filepos;  // file offset
    int m_index;  // nth chunk in file
    inline void SetIndex(int index) { m_index = index; }

    // Common fields
    quint8 fileVersion;
    quint16 blockSize;
    quint8 htype;
    quint8 family;
    quint8 familyVersion;
    quint8 ext;
    SessionID sessionid;
    quint32 timestamp;

    // Waveform-specific fields
    quint16 interval_count;
    quint8 interval_seconds;
    int duration;
    QList<PRS1Waveform> waveformInfo;
    
    // V3 normal/non-waveform fields
    QMap<unsigned char, short> hblock;

    QMap<unsigned char, QByteArray> mainblock;
    QMap<unsigned char, QByteArray> hbdata;
    
    // Trailing common fields
    quint8 storedChecksum;  // header checksum stored in file, last byte of m_header
    quint8 calcChecksum;    // header checksum as calculated when parsing
    quint32 storedCrc;      // header + data CRC stored in file, last 2-4 bytes of chunk
    quint32 calcCrc;        // header + data CRC as calculated when parsing

    //! \brief Parse and return the next chunk from a PRS1 file
    static PRS1DataChunk* ParseNext(class QFile & f);

    //! \brief Read and parse the next chunk header from a PRS1 file
    bool ReadHeader(class QFile & f);

    //! \brief Read the chunk's data from a PRS1 file and calculate its CRC, must be called after ReadHeader
    bool ReadData(class QFile & f);
    
    //! \brief Figures out which Compliance Parser to call, based on machine family/version and calls it.
    bool ParseCompliance(void);
    
    //! \brief Parse a single data chunk from a .000 file containing compliance data for a P25x brick
    bool ParseComplianceF0V23(void);
    
    //! \brief Parse a single data chunk from a .000 file containing compliance data for a DreamStation 200X brick
    bool ParseComplianceF0V6(void);
    
    //! \brief Figures out which Summary Parser to call, based on machine family/version and calls it.
    bool ParseSummary();

    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 0 CPAP/APAP family version 2 or 3 machine
    bool ParseSummaryF0V23(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 0 CPAP/APAP family version 4 machine
    bool ParseSummaryF0V4(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 0 CPAP/APAP family version 6 machine
    bool ParseSummaryF0V6(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 3 ventilator (family version 3) machine
    bool ParseSummaryF3V3(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 3 ventilator (family version 6) machine
    bool ParseSummaryF3V6(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 5 ASV family version 0-2 machine
    bool ParseSummaryF5V012(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 5 ASV family version 3 machine
    bool ParseSummaryF5V3(void);

    //! \brief Parse a flex setting byte from a .000 or .001 containing compliance/summary data
    void ParseFlexSetting(quint8 flex, int prs1mode);
    
    //! \brief Parse an humidifier setting byte from a .000 or .001 containing compliance/summary data for fileversion 2 machines: F0V234, F5V012, and maybe others
    void ParseHumidifierSettingV2(int humid, bool supportsHeatedTubing=true);

    //! \brief Parse humidifier setting bytes from a .000 or .001 containing compliance/summary data for fileversion 3 machines
    void ParseHumidifierSettingV3(unsigned char byte1, unsigned char byte2, bool add_setting=false);

    //! \brief Figures out which Event Parser to call, based on machine family/version and calls it.
    bool ParseEvents(CPAPMode mode);

    //! \brief Parse a single data chunk from a .002 file containing event data for a family 0 CPAP/APAP machine
    bool ParseEventsF0V234(CPAPMode mode);
    
    //! \brief Parse a single data chunk from a .002 file containing event data for a DreamStation family 0 CPAP/APAP machine
    bool ParseEventsF0V6(CPAPMode mode);

    //! \brief Parse a single data chunk from a .002 file containing event data for a family 3 ventilator family version 3 machine
    bool ParseEventsF3V3(void);
    
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 3 ventilator family version 6 machine
    bool ParseEventsF3V6(void);
    
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV family version 0-2 machine
    bool ParseEventsF5V012(void);

    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV family version 3 machine
    bool ParseEventsF5V3(void);

protected:
    //! \brief Add a parsed event to the chunk
    void AddEvent(class PRS1ParsedEvent* event);

    //! \brief Read and parse the non-waveform header data from a V2 PRS1 file
    bool ReadNormalHeaderV2(class QFile & f);

    //! \brief Read and parse the non-waveform header data from a V3 PRS1 file
    bool ReadNormalHeaderV3(class QFile & f);

    //! \brief Read and parse the waveform-specific header data from a PRS1 file
    bool ReadWaveformHeader(class QFile & f);

    //! \brief Extract the stored CRC from the end of the data of a PRS1 chunk
    bool ExtractStoredCrc(int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF0V6(const unsigned char* data, int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF5V3(const unsigned char* data, int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF3V6(const unsigned char* data, int size);
};


#if UNITTEST_MODE
QString _PRS1ParsedEventName(PRS1ParsedEvent* e);
QMap<QString,QString> _PRS1ParsedEventContents(PRS1ParsedEvent* e);
#endif


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


    QString wavefile;
    QString oxifile;

    //! \brief Imports .000 files for bricks.
    bool ImportCompliance();

    //! \brief Imports the .001 summary file.
    bool ImportSummary();

    //! \brief Figures out which Event Parser to call, based on machine family/version and calls it.
    bool ParseEvents();

    //! \brief Coalesce contiguous .005 or .006 waveform chunks from the file into larger chunks for import.
    QList<PRS1DataChunk *> CoalesceWaveformChunks(QList<PRS1DataChunk *> & allchunks);

    //! \brief Takes the parsed list of Flow/MaskPressure waveform chunks and adds them to the database
    bool ParseWaveforms();

    //! \brief Takes the parsed list of oximeter waveform chunks and adds them to the database.
    bool ParseOximetry();


    //! \brief Parse a single data chunk from a .002 file containing event data for a standard system one machine
    bool ParseF0Events();
    //! \brief Parse a single data chunk from a .002 file containing event data for a standard system one machine (family version 6)
    bool ParseEventsF0V6();
    //! \brief Parse a single data chunk from a .002 file containing event data for a AVAPS 1060P machine
    bool ParseF3Events();
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 3 ventilator machine (family version 6)
    bool ParseEventsF3V6();
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV machine (which has a different format)
    bool ParseF5Events();
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV family version 3 machine (which has a different format again)
    bool ParseEventsF5V3();


protected:
    Session * session;
    PRS1Loader * loader;
    SessionID sessionid;
    Machine * mach;

    int summary_duration;

    //! \brief Translate the PRS1-specific machine mode to the importable vendor-neutral enum.
    CPAPMode importMode(int mode);
    //! \brief Parse all the chunks in a single machine session
    bool ParseSession(void);
    //! \brief Save parsed session data to the database
    void SaveSessionToDatabase(void);
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

    //! \brief Peek into PROP.TXT or properties.txt at given path, and return it as a normalized key/value hash
    bool PeekProperties(const QString & filename, QHash<QString,QString> & props);
    
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

    //! \brief Finds the P0,P1,... session paths and property pathname and returns the base (10 or 16) of the session filenames
    int FindSessionDirsAndProperties(const QString & path, QStringList & paths, QString & propertyfile);

    //! \brief Reads the model number from the property file, evaluates its capabilities, and returns a machine instance
    Machine* CreateMachineFromProperties(QString propertyfile);

    //! \brief Scans the given directories for session data and create an import task for each logical session.
    void ScanFiles(const QStringList & paths, int sessionid_base, Machine * m);
    
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


//********************************************************************************************

class PRS1ModelInfo
{
protected:
    QHash<int, QHash<int, QStringList>> m_testedModels;
    QHash<QString,const char*> m_modelNames;
    QSet<QString> m_bricks;
    
public:
    PRS1ModelInfo();
    bool IsSupported(const QHash<QString,QString> & properties) const;
    bool IsSupported(int family, int familyVersion) const;
    bool IsTested(const QHash<QString,QString> & properties) const;
    bool IsTested(const QString & modelNumber, int family, int familyVersion) const;
    bool IsBrick(const QString & model) const;
    const char* Name(const QString & model) const;
};


#endif // PRS1LOADER_H
