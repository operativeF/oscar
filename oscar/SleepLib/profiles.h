/* SleepLib Profiles Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef PROFILES_H
#define PROFILES_H

#include <QString>
#include <QCryptographicHash>
#include <QThread>

#include "version.h"
#include "progressdialog.h"
#include "machine.h"
#include "machine_loader.h"
#include "preferences.h"
#include "common.h"

class Machine;

enum Gender { GenderNotSpecified, Male, Female };
enum MaskType { Mask_Unknown, Mask_NasalPillows, Mask_Hybrid, Mask_StandardNasal, Mask_FullFace };

class DoctorInfo;
class UserInfo;
class UserSettings;
class OxiSettings;
class CPAPSettings;
class AppearanceSettings;
class SessionSettings;


/*!
  \class Profile
  \author Mark Watkins
  \date 28/04/11
  \brief The User profile system, containing all information for a user, and an index into all Machine data
 */
class Profile : public Preferences
{
  public:
    //! \brief Constructor.. Does not open profile
    Profile(QString path);

    virtual ~Profile();

    //! \brief Parse machines.xml
    bool OpenMachines();
    bool StoreMachines();

    qint64 diskSpaceSummaries();
    qint64 diskSpaceEvents();
    qint64 diskSpaceBackups();
    qint64 diskSpace();

    //! \brief Returns hostname that locked profile, or empty string if unlocked
    QString checkLock();

    //! \brief Removes a lockfile
    bool removeLock();

    void addLock();

    //! \brief Save Profile object (This is an extension to Preference::Save(..))
    virtual bool Save(QString filename = "");

    //! \brief Add machine to this profiles machlist
    void AddMachine(Machine *m);

    //! \brief Remove machine from this profiles machlist
    void DelMachine(Machine *m);

    //! \brief Loads all machine (summary) data belonging to this profile
    void LoadMachineData(ProgressDialog *progress);

    //! \brief Unloads all machine (summary) data for this profile to free up memory;
    void UnloadMachineData();

    //! \brief Barf because data format has changed. This does a purge of CPAP data for machine *m
    void DataFormatError(Machine *m);

    QString path() { return p_path; }

        /*! \brief Import Machine Data
        \param path containing import location
     */
    int Import(QString path);

    //! \brief Removes a given day from the date, destroying the daylist date record if empty
    bool unlinkDay(Day * day);

//    bool trashMachine(Machine * mach);

    //! \brief Add Day record to Profile Day list
    Day *addDay(QDate date);

    //! \brief Get Day record if data available for date and machine type, else return nullptr
    Day *GetDay(QDate date, MachineType type = MT_UNKNOWN);

    //! \brief Same as GetDay but does not open the summaries
    Day *FindDay(QDate date, MachineType type = MT_UNKNOWN);

    //! \brief Get Day record if data available for date and machine type,
    //         and has enabled session data, else return nullptr
    Day *GetGoodDay(QDate date, MachineType type);

    //! \breif Same as GetGoodDay but does not open the summaries
    Day *FindGoodDay(QDate date, MachineType type);

    //! \brief Returns a list of all machines of type t
    QList<Machine *> GetMachines(MachineType t = MT_UNKNOWN);

    //! \brief Returns the machine of type t used on date, nullptr if none..
    Machine *GetMachine(MachineType t, QDate date);

    //! \brief return the first machine of type t
    Machine *GetMachine(MachineType t);

    //! \brief Returns true if this profile stores this variable identified by key
    bool contains(QString key) { return p_preferences.contains(key); }


    //! \brief Get all days records of machine type between start and end dates
    QList<Day *> getDays(MachineType mt, QDate start, QDate end);

    //! \brief Returns a count of all days (with data) of machine type, between start and end dates
    int countDays(MachineType mt = MT_UNKNOWN, QDate start = QDate(), QDate end = QDate());

    //! \brief Returns a count of all compliant days of machine type between start and end dates
    int countCompliantDays(MachineType mt, QDate start, QDate end);

    //! \brief Returns a count of all event entries for code, matching machine type between start an end dates
    EventDataType calcCount(ChannelID code, MachineType mt = MT_CPAP, QDate start = QDate(),
                            QDate end = QDate());

    //! \brief Returns a sum of all event data for Channel code, matching machine type between start an end dates
    double calcSum(ChannelID code, MachineType mt = MT_CPAP, QDate start = QDate(),
                   QDate end = QDate());

    //! \brief Returns a sum of all session durations for machine type, between start and end dates
    EventDataType calcHours(MachineType mt = MT_CPAP, QDate start = QDate(), QDate end = QDate());

    //! \brief Calculates Channel Average (Sums and counts all events, returning the sum divided by the count.)
    EventDataType calcAvg(ChannelID code, MachineType mt = MT_CPAP, QDate start = QDate(),
                          QDate end = QDate());

    //! \brief Calculates Channel Weighted Average between start and end dates
    EventDataType calcWavg(ChannelID code, MachineType mt = MT_CPAP, QDate start = QDate(),
                           QDate end = QDate());

    //! \brief Calculates the minimum value for channel code, between start and end dates
    EventDataType calcMin(ChannelID code, MachineType mt = MT_CPAP, QDate start = QDate(),
                          QDate end = QDate());

    //! \brief Calculates the maximum value for channel code, between start and end dates
    EventDataType calcMax(ChannelID code, MachineType mt = MT_CPAP, QDate start = QDate(),
                          QDate end = QDate());

    //! \brief Calculates a percentile value percent for channel code, between start and end dates
    EventDataType calcPercentile(ChannelID code, EventDataType percent, MachineType mt = MT_CPAP,
                                 QDate start = QDate(), QDate end = QDate());

    //! \brief Tests if Channel code is available in all day sets
    bool hasChannel(ChannelID code);


    //! \brief Looks up if any machines report channel is available
    bool channelAvailable(ChannelID code);


    //! \brief Calculates the minimum session settings value for channel code, between start and end dates
    EventDataType calcSettingsMin(ChannelID code, MachineType mt = MT_CPAP, QDate start = QDate(),
                                  QDate end = QDate());

    //! \brief Calculates the maximum session settings value for channel code, between start and end dates
    EventDataType calcSettingsMax(ChannelID code, MachineType mt = MT_CPAP, QDate start = QDate(),
                                  QDate end = QDate());

    //! \brief Calculates the time channel code spends above threshold value for machine type, between start and end dates
    EventDataType calcAboveThreshold(ChannelID code, EventDataType threshold, MachineType mt = MT_CPAP,
                                     QDate start = QDate(), QDate end = QDate());

    //! \brief Calculates the time channel code spends below threshold value for machine type, between start and end dates
    EventDataType calcBelowThreshold(ChannelID code, EventDataType threshold, MachineType mt = MT_CPAP,
                                     QDate start = QDate(), QDate end = QDate());


    Day * findSessionDay(Session * session);

    //! \brief Looks for the first date containing a day record matching machinetype
    QDate FirstDay(MachineType mt = MT_UNKNOWN);

    //! \brief Looks for the last date containing a day record matching machinetype
    QDate LastDay(MachineType mt = MT_UNKNOWN);

    //! \brief Looks for the first date containing a day record with enabled sessions matching machinetype
    QDate FirstGoodDay(MachineType mt = MT_UNKNOWN);

    //! \brief Looks for the last date containing a day record with enabled sessions matching machinetype
    QDate LastGoodDay(MachineType mt = MT_UNKNOWN);

    //! \brief Returns this profiles data folder
    QString dataFolder() { return (*this).Get("{DataFolder}"); }

    //! \brief Return if this profile has been opened or not
    bool isOpen() { return m_opened; }

    //! \brief QMap of day records (iterates in order).
    QMap<QDate, Day *> daylist;

    void removeMachine(Machine *);
    Machine * lookupMachine(QString serial, QString loadername);
    Machine * CreateMachine(MachineInfo info, MachineID id = 0);

    void loadChannels();
    void saveChannels();


    bool is_first_day;

    UserInfo *user;
    CPAPSettings *cpap;
    OxiSettings *oxi;
    DoctorInfo *doctor;
    AppearanceSettings *appearance;
    UserSettings *general;
    SessionSettings *session;
    QList<Machine *> m_machlist;

  protected:
    QDate m_first;
    QDate m_last;

    bool m_opened;
    bool m_machopened;

    QHash<QString, QHash<QString, Machine *> > MachineList;

};

class MachineLoader;
extern MachineLoader *GetLoader(QString name);

extern Preferences *p_pref;
extern Profile *p_profile;

// these are bad and must change
#define PREF (*p_pref)


//! \brief Returns a count of all files & directories in a supplied folder
int dirCount(QString path);


namespace Profiles {

extern QMap<QString, Profile *> profiles;
void Scan(); // Initialize and load Profile
void Done(); // Save all Profile objects and clear list
int CleanupProfile(Profile *prof);

Profile *Create(QString name);
Profile *Get(QString name);
Profile *Get();

}

// DoctorInfo Strings
const QString STR_DI_Name = "DoctorName";
const QString STR_DI_Phone = "DoctorPhone";
const QString STR_DI_Email = "DoctorEmail";
const QString STR_DI_Practice = "DoctorPractice";
const QString STR_DI_Address = "DoctorAddress";
const QString STR_DI_PatientID = "DoctorPatientID";

// UserInfo Strings
const QString STR_UI_DOB = "DOB";
const QString STR_UI_FirstName = "FirstName";
const QString STR_UI_LastName = "LastName";
const QString STR_UI_UserName = "UserName";
const QString STR_UI_Password = "Password";
const QString STR_UI_Address = "Address";
const QString STR_UI_Phone = "Phone";
const QString STR_UI_EmailAddress = "EmailAddress";
const QString STR_UI_Country = "Country";
const QString STR_UI_Height = "Height";
const QString STR_UI_Gender = "Gender";
const QString STR_UI_TimeZone = "TimeZone";
const QString STR_UI_DST = "DST";

// OxiSettings Strings
const QString STR_OS_EnableOximetry = "EnableOximetry";
const QString STR_OS_DefaultDevice = "DefaultOxiDevice";
const QString STR_OS_SyncOximeterClock = "SyncOximeterClock";
const QString STR_OS_OximeterType = "OximeterType";
const QString STR_OS_OxiDiscardThreshold = "OxiDiscardThreshold";
const QString STR_OS_SPO2DropDuration = "SPO2DropDuration";
const QString STR_OS_SPO2DropPercentage = "SPO2DropPercentage";
const QString STR_OS_PulseChangeDuration = "PulseChangeDuration";
const QString STR_OS_PulseChangeBPM = "PulseChangeBPM";
const QString STR_OS_SkipOxiIntroScreen = "SkipOxiIntroScreen";


// CPAPSettings Strings
const QString STR_CS_ComplianceHours = "ComplianceHours";
const QString STR_CS_ShowCompliance = "ShowCompliance";
const QString STR_CS_ShowLeaksMode = "ShowLeaksMode";
const QString STR_CS_MaskStartDate = "MaskStartDate";
const QString STR_CS_MaskDescription = "MaskDescription";
const QString STR_CS_MaskType = "MaskType";
const QString STR_CS_PrescribedMode = "CPAPPrescribedMode";
const QString STR_CS_PrescribedMinPressure = "CPAPPrescribedMinPressure";
const QString STR_CS_PrescribedMaxPressure = "CPAPPrescribedMaxPressure";
const QString STR_CS_UntreatedAHI = "UntreatedAHI";
const QString STR_CS_Notes = "CPAPNotes";
const QString STR_CS_DateDiagnosed = "DateDiagnosed";
const QString STR_CS_UserEventFlagging = "UserEventFlagging";
const QString STR_CS_AutoImport = "AutoImport";
const QString STR_CS_BrickWarning = "BrickWarning";

const QString STR_CS_UserFlowRestriction = "UserFlowRestriction";
const QString STR_CS_UserEventDuration = "UserEventDuration";
const QString STR_CS_UserFlowRestriction2 = "UserFlowRestriction2";
const QString STR_CS_UserEventDuration2 = "UserEventDuration2";
const QString STR_CS_UserEventDuplicates = "UserEventDuplicates";
const QString STR_CS_ResyncFromUserFlagging = "ResyncFromUserFlagging";

const QString STR_CS_AHIWindow = "AHIWindow";
const QString STR_CS_AHIReset = "AHIReset";
const QString STR_CS_ClockDrift = "ClockDrift";
const QString STR_CS_LeakRedline = "LeakRedline";
const QString STR_CS_ShowLeakRedline = "ShowLeakRedline";

const QString STR_CS_CalculateUnintentionalLeaks = "CalculateUnintentionalLeaks";
const QString STR_CS_4cmH2OLeaks = "Custom4cmH2OLeaks";
const QString STR_CS_20cmH2OLeaks = "Custom20cmH2OLeaks";

// ImportSettings Strings
const QString STR_IS_DaySplitTime = "DaySplitTime";
const QString STR_IS_PreloadSummaries = "PreloadSummaries";
const QString STR_IS_CombineCloseSessions = "CombineCloserSessions";
const QString STR_IS_IgnoreShorterSessions = "IgnoreShorterSessions";
const QString STR_IS_BackupCardData = "BackupCardData";
const QString STR_IS_CompressBackupData = "CompressBackupData";
const QString STR_IS_CompressSessionData = "CompressSessionData";
const QString STR_IS_IgnoreOlderSessions = "IgnoreOlderSessions";
const QString STR_IS_IgnoreOlderSessionsDate = "IgnoreOlderSessionsDate";
const QString STR_IS_LockSummarySessions = "LockSummarySessions";


// UserSettings Strings
const QString STR_US_UnitSystem = "UnitSystem";
const QString STR_US_EventWindowSize = "EventWindowSize";
const QString STR_US_SkipEmptyDays = "SkipEmptyDays";
const QString STR_US_RebuildCache = "RebuildCache";
const QString STR_US_LinkGroups = "LinkGroups";
const QString STR_US_CalculateRDI = "CalculateRDI";
const QString STR_US_PrefCalcMiddle = "PrefCalcMiddle";
const QString STR_US_PrefCalcPercentile = "PrefCalcPercentile";
const QString STR_US_PrefCalcMax = "PrefCalcMax";
const QString STR_US_ShowUnknownFlags = "ShowUnknownFlags";
const QString STR_US_StatReportMode = "StatReportMode";
const QString STR_US_LastOverviewRange = "LastOverviewRange";

class DoctorInfo : public PrefSettings
{
  public:
    DoctorInfo(Profile *profile)
      : PrefSettings(profile)
    {
        initPref(STR_DI_Name, QString());
        initPref(STR_DI_Phone, QString());
        initPref(STR_DI_Email, QString());
        initPref(STR_DI_Practice, QString());
        initPref(STR_DI_Address, QString());
        initPref(STR_DI_PatientID, QString());
    }

    const QString name() const { return getPref(STR_DI_Name).toString(); }
    const QString phone() const { return getPref(STR_DI_Phone).toString(); }
    const QString email() const { return getPref(STR_DI_Email).toString(); }
    const QString practiceName() const { return getPref(STR_DI_Practice).toString(); }
    const QString address() const { return getPref(STR_DI_Address).toString(); }
    const QString patientID() const { return getPref(STR_DI_PatientID).toString(); }

    void setName(QString name) { setPref(STR_DI_Name, name); }
    void setPhone(QString phone) { setPref(STR_DI_Phone, phone); }
    void setEmail(QString phone) { setPref(STR_DI_Email, phone); }
    void setPracticeName(QString practice) { setPref(STR_DI_Practice, practice); }
    void setAddress(QString address) { setPref(STR_DI_Address, address); }
    void setPatientID(QString pid) { setPref(STR_DI_PatientID, pid); }
};


/*! \class UserInfo
    \brief Profile Options relating to the User Information
    */
class UserInfo : public PrefSettings
{
  public:
    UserInfo(Profile *profile)
      : PrefSettings(profile)
    {
        initPref(STR_UI_DOB, QDate(1970, 1, 1));
        initPref(STR_UI_FirstName, QString());
        initPref(STR_UI_LastName, QString());
        initPref(STR_UI_UserName, QString());
        initPref(STR_UI_Password, QString());
        initPref(STR_UI_Address, QString());
        initPref(STR_UI_Phone, QString());
        initPref(STR_UI_EmailAddress, QString());
        initPref(STR_UI_Country, QString());
        initPref(STR_UI_Height, 0.0);
        initPref(STR_UI_Gender, (int)GenderNotSpecified);
        initPref(STR_UI_TimeZone, QString());
        initPref(STR_UI_DST, false);
    }

    QDate DOB() const { return getPref(STR_UI_DOB).toDate(); }
    const QString firstName() const { return getPref(STR_UI_FirstName).toString(); }
    const QString lastName() const { return getPref(STR_UI_LastName).toString(); }
    const QString userName() const { return getPref(STR_UI_UserName).toString(); }
    const QString address() const { return getPref(STR_UI_Address).toString(); }
    const QString phone() const { return getPref(STR_UI_Phone).toString(); }
    const QString email() const { return getPref(STR_UI_EmailAddress).toString(); }
    double height() const { return getPref(STR_UI_Height).toDouble(); }
    const QString country() const { return getPref(STR_UI_Country).toString(); }
    Gender gender() const { return (Gender)getPref(STR_UI_Gender).toInt(); }
    const QString timeZone() const { return getPref(STR_UI_TimeZone).toString(); }
    bool daylightSaving() const { return getPref(STR_UI_DST).toBool(); }

    void setDOB(QDate date) { setPref(STR_UI_DOB, date); }
    void setFirstName(QString name) { setPref(STR_UI_FirstName, name); }
    void setLastName(QString name) { setPref(STR_UI_LastName, name); }
    void setUserName(QString username) { setPref(STR_UI_UserName, username); }
    void setAddress(QString address) { setPref(STR_UI_Address, address); }
    void setPhone(QString phone) { setPref(STR_UI_Phone, phone); }
    void setEmail(QString email) { setPref(STR_UI_EmailAddress, email); }
    void setHeight(double height) { setPref(STR_UI_Height, height); }
    void setCountry(QString country) { setPref(STR_UI_Country, country); }
    void setGender(Gender g) { setPref(STR_UI_Gender, (int)g); }
    void setTimeZone(QString tz) { setPref(STR_UI_TimeZone, tz); }
    void setDaylightSaving(bool ds) { setPref(STR_UI_DST, ds); }

    bool hasPassword() { return !getPref(STR_UI_Password).toString().isEmpty(); }

    bool checkPassword(QString password) {
        QByteArray ba = password.toUtf8();
        QString hashedPass = QString(QCryptographicHash::hash(ba, QCryptographicHash::Sha1).toHex());
        return getPref(STR_UI_Password).toString() == hashedPass;
    }

    void setPassword(QString password) {
        QByteArray ba = password.toUtf8();
        QString hashedPass = QString(QCryptographicHash::hash(ba, QCryptographicHash::Sha1).toHex());
        setPref(STR_UI_Password, hashedPass);
    }
};

/*! \class OxiSettings
    \brief Profile Options relating to the Oximetry settings
    */
class OxiSettings : public PrefSettings
{
  public:
    //! \brief Create OxiSettings object given Profile *p, and initialize the defaults
    OxiSettings(Profile *profile)
      : PrefSettings(profile)
    {
        initPref(STR_OS_EnableOximetry, false);
        initPref(STR_OS_DefaultDevice, QString());
        initPref(STR_OS_SyncOximeterClock, true);
        initPref(STR_OS_OximeterType, 0);
        initPref(STR_OS_OxiDiscardThreshold, 0.0);
        initPref(STR_OS_SPO2DropDuration, 8.0);
        initPref(STR_OS_SPO2DropPercentage, 3.0);
        initPref(STR_OS_PulseChangeDuration, 8.0);
        initPref(STR_OS_PulseChangeBPM, 5.0);
        initPref(STR_OS_SkipOxiIntroScreen, false);
    }

    bool oximetryEnabled() const { return getPref(STR_OS_EnableOximetry).toBool(); }
    QString defaultDevice() const { return getPref(STR_OS_DefaultDevice).toString(); }
    bool syncOximeterClock() const { return getPref(STR_OS_SyncOximeterClock).toBool(); }
    int oximeterType() const { return getPref(STR_OS_OximeterType).toInt(); }
    double oxiDiscardThreshold() const { return getPref(STR_OS_OxiDiscardThreshold).toDouble(); }
    double spO2DropDuration() const { return getPref(STR_OS_SPO2DropDuration).toDouble(); }
    double spO2DropPercentage() const { return getPref(STR_OS_SPO2DropPercentage).toDouble(); }
    double pulseChangeDuration() const { return getPref(STR_OS_PulseChangeDuration).toDouble(); }
    double pulseChangeBPM() const { return getPref(STR_OS_PulseChangeBPM).toDouble(); }
    bool skipOxiIntroScreen() const { return getPref(STR_OS_SkipOxiIntroScreen).toBool(); }


    void setOximetryEnabled(bool enabled) { setPref(STR_OS_EnableOximetry, enabled); }
    void setDefaultDevice(QString name) { setPref(STR_OS_DefaultDevice, name); }
    void setSyncOximeterClock(bool synced) { setPref(STR_OS_SyncOximeterClock, synced); }
    void setOximeterType(int oxitype) { setPref(STR_OS_OximeterType, oxitype); }
    void setOxiDiscardThreshold(double thresh) { setPref(STR_OS_OxiDiscardThreshold, thresh); }
    void setSpO2DropDuration(double duration) { setPref(STR_OS_SPO2DropDuration, duration); }
    void setPulseChangeBPM(double bpm) { setPref(STR_OS_PulseChangeBPM, bpm); }
    void setSkipOxiIntroScreen(bool skip) { setPref(STR_OS_SkipOxiIntroScreen, skip); }
    void setSpO2DropPercentage(double percentage) {
        setPref(STR_OS_SPO2DropPercentage, percentage);
    }
    void setPulseChangeDuration(double duration) {
        setPref(STR_OS_PulseChangeDuration, duration);
    }
};

/*! \class CPAPSettings
    \brief Profile Options relating to the CPAP settings
    */
class CPAPSettings : public PrefSettings
{
  public:
    CPAPSettings(Profile *profile)
      : PrefSettings(profile)
    {
        m_complianceHours = initPref(STR_CS_ComplianceHours, 4.0f).toFloat();
        initPref(STR_CS_ShowCompliance, true);
        initPref(STR_CS_ShowLeaksMode, 0);
        // TODO: jedimark: Check if this date is initiliazed yet
        initPref(STR_CS_MaskStartDate, QDate());
        initPref(STR_CS_MaskDescription, QString());
        initPref(STR_CS_MaskType, Mask_Unknown);
        initPref(STR_CS_PrescribedMode, MODE_UNKNOWN);
        initPref(STR_CS_PrescribedMinPressure, 0.0f);
        initPref(STR_CS_PrescribedMaxPressure, 0.0f);
        initPref(STR_CS_UntreatedAHI, 0.0f);
        initPref(STR_CS_Notes, QString());
        initPref(STR_CS_DateDiagnosed, QDate());
        m_userEventRestriction1 = initPref(STR_CS_UserFlowRestriction, 20.0f).toFloat();
        m_userEventDuration1 = initPref(STR_CS_UserEventDuration, 8.0f).toFloat();
        m_userEventRestriction2 = initPref(STR_CS_UserFlowRestriction2, 50.0f).toFloat();
        m_userEventDuration2 = initPref(STR_CS_UserEventDuration2, 8.0f).toFloat();
        m_userEventDuplicates = initPref(STR_CS_UserEventDuplicates, false).toBool();
        m_userEventFlagging = initPref(STR_CS_UserEventFlagging, false).toBool();
        m_ahiWindow = initPref(STR_CS_AHIWindow, 60.0).toFloat();
        m_ahiReset = initPref(STR_CS_AHIReset, false).toBool();
        m_leakRedLine = initPref(STR_CS_LeakRedline, 24.0f).toFloat();
        m_showLeakRedline = initPref(STR_CS_ShowLeakRedline, true).toBool();
        m_resyncFromUserFlagging = initPref(STR_CS_ResyncFromUserFlagging, false).toBool();
        initPref(STR_CS_AutoImport, false);
        initPref(STR_CS_BrickWarning, true);

        m_calcUnintentionalLeaks = initPref(STR_CS_CalculateUnintentionalLeaks, true).toBool();
        m_4cmH2OLeaks = initPref(STR_CS_4cmH2OLeaks, 20.167).toDouble();
        m_20cmH2OLeaks = initPref(STR_CS_20cmH2OLeaks, 48.333).toDouble();
        m_clock_drift = initPref(STR_CS_ClockDrift, (int)0).toInt();
    }

    //Getters
    double complianceHours() const { return m_complianceHours; }
    bool showComplianceInfo() const { return getPref(STR_CS_ShowCompliance).toBool(); }
    int leakMode() const { return getPref(STR_CS_ShowLeaksMode).toInt(); }
    QDate maskStartDate() const { return getPref(STR_CS_MaskStartDate).toDate(); }
    QString maskDescription() const { return getPref(STR_CS_MaskDescription).toString(); }
    MaskType maskType() const { return (MaskType)getPref(STR_CS_MaskType).toInt(); }
    CPAPMode mode() const { return CPAPMode(getPref(STR_CS_PrescribedMode).toInt()); }
    EventDataType minPressure() const { return getPref(STR_CS_PrescribedMinPressure).toFloat(); }
    EventDataType maxPressure() const { return getPref(STR_CS_PrescribedMaxPressure).toFloat(); }
    EventDataType untreatedAHI() const { return getPref(STR_CS_UntreatedAHI).toFloat(); }
    const QString notes() const { return getPref(STR_CS_Notes).toString(); }
    QDate dateDiagnosed() const { return getPref(STR_CS_DateDiagnosed).toDate(); }
    inline EventDataType userEventRestriction() const { return m_userEventRestriction1; }
    inline EventDataType userEventDuration() const { return m_userEventDuration1; }
    inline EventDataType userEventRestriction2() const { return m_userEventRestriction2; }
    inline EventDataType userEventDuration2() const { return m_userEventDuration2; }
    inline bool userEventDuplicates() const { return m_userEventDuplicates; }
    inline EventDataType AHIWindow() const { return m_ahiWindow; }
    inline bool AHIReset() const { return m_ahiReset; }
    inline bool userEventFlagging() const { return m_userEventFlagging; }
    inline int clockDrift() const { return m_clock_drift; }
    inline EventDataType leakRedline() const { return m_leakRedLine; }
    inline bool showLeakRedline() const { return m_showLeakRedline; }
    inline bool resyncFromUserFlagging() const { return m_resyncFromUserFlagging; }
    bool autoImport() const { return getPref(STR_CS_AutoImport).toBool(); }
    bool brickWarning() const { return getPref(STR_CS_BrickWarning).toBool(); }

    inline bool calculateUnintentionalLeaks() const { return m_calcUnintentionalLeaks; }
    inline double custom4cmH2OLeaks() const { return m_4cmH2OLeaks; }
    inline double custom20cmH2OLeaks() const { return m_20cmH2OLeaks; }

    //Setters
    void setMode(CPAPMode mode) { setPref(STR_CS_PrescribedMode, (int)mode); }
    void setMinPressure(EventDataType pressure) { setPref(STR_CS_PrescribedMinPressure, pressure); }
    void setMaxPressure(EventDataType pressure) { setPref(STR_CS_PrescribedMaxPressure, pressure); }
    void setUntreatedAHI(EventDataType ahi) { setPref(STR_CS_UntreatedAHI, ahi); }
    void setNotes(QString notes) { setPref(STR_CS_Notes, notes); }
    void setDateDiagnosed(QDate date) { setPref(STR_CS_DateDiagnosed, date); }
    void setComplianceHours(EventDataType hours) { setPref(STR_CS_ComplianceHours, m_complianceHours=hours); }
    void setShowComplianceInfo(bool b) { setPref(STR_CS_ShowCompliance, b); }
    void setLeakMode(int leakmode) { setPref(STR_CS_ShowLeaksMode, (int)leakmode); }
    void setMaskStartDate(QDate date) { setPref(STR_CS_MaskStartDate, date); }
    void setMaskType(MaskType masktype) { setPref(STR_CS_MaskType, (int)masktype); }
    void setUserEventRestriction(EventDataType flow) { setPref(STR_CS_UserFlowRestriction, m_userEventRestriction1=flow); }
    void setUserEventDuration(EventDataType duration) { setPref(STR_CS_UserEventDuration, m_userEventDuration1=duration); }
    void setUserEventRestriction2(EventDataType flow) { setPref(STR_CS_UserFlowRestriction2, m_userEventRestriction2=flow); }
    void setUserEventDuration2(EventDataType duration) { setPref(STR_CS_UserEventDuration2, m_userEventDuration2=duration); }
    void setAHIWindow(EventDataType window) { setPref(STR_CS_AHIWindow, m_ahiWindow=window); }
    void setAHIReset(bool b) { setPref(STR_CS_AHIReset, m_ahiReset=b); }
    void setUserEventFlagging(bool flagging) { setPref(STR_CS_UserEventFlagging, m_userEventFlagging=flagging); }
    void setUserEventDuplicates(bool dup) { setPref(STR_CS_UserEventDuplicates, m_userEventDuplicates=dup); }
    void setMaskDescription(QString description) { setPref(STR_CS_MaskDescription, description); }
    void setClockDrift(int seconds) { setPref(STR_CS_ClockDrift, m_clock_drift = seconds); }
    void setLeakRedline(EventDataType value) { setPref(STR_CS_LeakRedline, m_leakRedLine=value); }
    void setShowLeakRedline(bool b) { setPref(STR_CS_ShowLeakRedline, m_showLeakRedline=b); }
    void setResyncFromUserFlagging(bool b) { setPref(STR_CS_ResyncFromUserFlagging, m_resyncFromUserFlagging=b); }
    void setAutoImport(bool b) { setPref(STR_CS_AutoImport, b); }
    void setBrickWarning(bool b) { setPref(STR_CS_BrickWarning, b); }

    void setCalculateUnintentionalLeaks(bool b) { setPref(STR_CS_CalculateUnintentionalLeaks, m_calcUnintentionalLeaks=b); }
    void setCustom4cmH2OLeaks(double val) { setPref(STR_CS_4cmH2OLeaks, m_4cmH2OLeaks=val); }
    void setCustom20cmH2OLeaks(double val) { setPref(STR_CS_20cmH2OLeaks, m_20cmH2OLeaks=val); }

  public:
    int m_clock_drift;
    double m_4cmH2OLeaks, m_20cmH2OLeaks;
    bool m_userEventFlagging, m_userEventDuplicates, m_calcUnintentionalLeaks, m_resyncFromUserFlagging, m_ahiReset;
    bool m_showLeakRedline;

    EventDataType m_leakRedLine, m_complianceHours, m_ahiWindow;

    EventDataType m_userEventRestriction1, m_userEventRestriction2, m_userEventDuration1, m_userEventDuration2;

};

/*! \class ImportSettings
    \brief Profile Options relating to the Import process
    */
class SessionSettings : public PrefSettings
{
  public:
    SessionSettings(Profile *profile)
      : PrefSettings(profile)
    {
        m_daySplitTime = initPref(STR_IS_DaySplitTime, QTime(12, 0, 0)).toTime();
        m_preloadSummaries = initPref(STR_IS_PreloadSummaries, false).toBool();
        m_combineCloseSessions = initPref(STR_IS_CombineCloseSessions, 240.0).toDouble();
        m_ignoreShortSessions = initPref(STR_IS_IgnoreShorterSessions, 5.0).toDouble();
        m_backupCardData = initPref(STR_IS_BackupCardData, true).toBool();
        m_compressBackupData = initPref(STR_IS_CompressBackupData, false).toBool();
        m_compressSessionData = initPref(STR_IS_CompressSessionData, false).toBool();
        m_ignoreOlderSessions = initPref(STR_IS_IgnoreOlderSessions, false).toBool();
        m_ignoreOlderSessionsDate=initPref(STR_IS_IgnoreOlderSessionsDate, QDateTime(QDate::currentDate().addYears(-1), daySplitTime()) ).toDateTime();
        m_lockSummarySessions = initPref(STR_IS_LockSummarySessions, true).toBool();
    }

    inline QTime daySplitTime() const { return m_daySplitTime; }
    inline bool preloadSummaries() const { return m_preloadSummaries; }
    inline double combineCloseSessions() const { return m_combineCloseSessions; }
    inline double ignoreShortSessions() const { return m_ignoreShortSessions; }
    inline bool compressSessionData() const { return m_compressSessionData; }
    inline bool compressBackupData() const { return m_compressBackupData; }
    inline bool backupCardData() const { return m_backupCardData; }
    inline bool ignoreOlderSessions() const { return m_ignoreOlderSessions; }
    inline QDateTime ignoreOlderSessionsDate() const { return m_ignoreOlderSessionsDate; }
    inline bool lockSummarySessions() const { return m_lockSummarySessions; }

    void setDaySplitTime(QTime time) { setPref(STR_IS_DaySplitTime, m_daySplitTime=time); }
    void setPreloadSummaries(bool b) { setPref(STR_IS_PreloadSummaries, m_preloadSummaries=b); }
    void setCombineCloseSessions(double val) { setPref(STR_IS_CombineCloseSessions, m_combineCloseSessions=val); }
    void setIgnoreShortSessions(double val) { setPref(STR_IS_IgnoreShorterSessions, m_ignoreShortSessions=val); }
    void setBackupCardData(bool b) { setPref(STR_IS_BackupCardData, m_backupCardData=b); }
    void setCompressBackupData(bool b) { setPref(STR_IS_CompressBackupData, m_compressBackupData=b); }
    void setCompressSessionData(bool b) { setPref(STR_IS_CompressSessionData, m_compressSessionData=b); }
    void setIgnoreOlderSessions(bool b) { setPref(STR_IS_IgnoreOlderSessions, m_ignoreOlderSessions=b); }
    void setIgnoreOlderSessionsDate(QDate date) { setPref(STR_IS_IgnoreOlderSessionsDate, m_ignoreOlderSessionsDate=QDateTime(date, daySplitTime())); }
    void setLockSummarySessions(bool b) { setPref(STR_IS_LockSummarySessions, m_lockSummarySessions=b); }


    QTime m_daySplitTime;
    QDateTime m_ignoreOlderSessionsDate;
    bool m_preloadSummaries, m_backupCardData, m_compressBackupData, m_compressSessionData, m_ignoreOlderSessions, m_lockSummarySessions;
    double m_combineCloseSessions, m_ignoreShortSessions;
};

/*! \class AppearanceSettings
    \brief Profile Options relating to Visual Appearance
    */
class AppearanceSettings : public PrefSettings
{
  public:
    //! \brief Create AppearanceSettings object given Profile *p, and initialize the defaults
    AppearanceSettings(Profile *profile)
      : PrefSettings(profile)
    {

    }
};

/*! \class UserSettings
    \brief Profile Options relating to General User Settings
    */
class UserSettings : public PrefSettings
{
  public:
    UserSettings(Profile *profile)
      : PrefSettings(profile)
    {
        initPref(STR_US_UnitSystem, US_Metric);
        initPref(STR_US_EventWindowSize, 4.0);
        m_skipEmptyDays = initPref(STR_US_SkipEmptyDays, true).toBool();
        initPref(STR_US_RebuildCache, false); // FIXME: jedimark: can't remember...
        m_calculateRDI = initPref(STR_US_CalculateRDI, false).toBool();
        m_prefCalcMiddle = initPref(STR_US_PrefCalcMiddle, (int)0).toInt();
        m_prefCalcPercentile = initPref(STR_US_PrefCalcPercentile, (double)95.0).toDouble();
        m_prefCalcMax = initPref(STR_US_PrefCalcMax, (int)0).toInt();
        initPref(STR_US_StatReportMode, 0);
        m_showUnownFlags = initPref(STR_US_ShowUnknownFlags, false).toBool();
        initPref(STR_US_LastOverviewRange, 4);
    }

    UnitSystem unitSystem() const { return (UnitSystem)getPref(STR_US_UnitSystem).toInt(); }
    double eventWindowSize() const { return getPref(STR_US_EventWindowSize).toDouble(); }
    inline bool skipEmptyDays() const { return m_skipEmptyDays; }
    bool rebuildCache() const { return getPref(STR_US_RebuildCache).toBool(); }
    inline bool calculateRDI() const { return m_calculateRDI; }
    inline int prefCalcMiddle() const { return m_prefCalcMiddle;  }
    inline double prefCalcPercentile() const { return m_prefCalcPercentile; }
    inline int prefCalcMax() const { return m_prefCalcMax; }
    int statReportMode() const { return getPref(STR_US_StatReportMode).toInt(); }
    inline bool showUnknownFlags() const { return m_showUnownFlags; }
    int lastOverviewRange() const { return getPref(STR_US_LastOverviewRange).toInt(); }

    void setUnitSystem(UnitSystem us) { setPref(STR_US_UnitSystem, (int)us); }
    void setEventWindowSize(double size) { setPref(STR_US_EventWindowSize, size); }
    void setSkipEmptyDays(bool b) { setPref(STR_US_SkipEmptyDays, m_skipEmptyDays=b); }
    void setRebuildCache(bool rebuild) { setPref(STR_US_RebuildCache, rebuild); }
    void setCalculateRDI(bool rdi) { setPref(STR_US_CalculateRDI, m_calculateRDI=rdi); }
    void setPrefCalcMiddle(int i) { setPref(STR_US_PrefCalcMiddle, m_prefCalcMiddle=i); }
    void setPrefCalcPercentile(double p) { setPref(STR_US_PrefCalcPercentile, m_prefCalcPercentile=p); }
    void setPrefCalcMax(int i) { setPref(STR_US_PrefCalcMax, m_prefCalcMax=i); }
    void setStatReportMode(int i) { setPref(STR_US_StatReportMode, i); }
    void setShowUnknownFlags(bool b) { setPref(STR_US_ShowUnknownFlags, m_showUnownFlags=b); }
    void setLastOverviewRange(int i) { setPref(STR_US_LastOverviewRange, i); }

    bool m_calculateRDI, m_showUnownFlags,  m_skipEmptyDays;
    int m_prefCalcMiddle, m_prefCalcMax;
    double m_prefCalcPercentile;
};


#endif // PROFILES_H

