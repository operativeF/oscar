/* SleepLib Common Functions
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDateTime>
#include <QDir>
#include <QThread>
#ifndef BROKEN_OPENGL_BUILD
#include <QGLWidget>
#endif
#include <QOpenGLFunctions>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>

#include "SleepLib/common.h"

#ifdef _MSC_VER
#include <QtZlib/zlib.h>
#else
#include <unistd.h>
#include <zlib.h>
#endif

#include "git_info.h"
#include "version.h"
#include "profiles.h"

// Used by internal settings


const QString & gitRevision()
{
    return GIT_REVISION;
}
const QString & gitBranch()
{
    return GIT_BRANCH;
}

const QString getDeveloperName()
{
    return STR_DeveloperName;
}

const QString getAppName()
{
    QString name = STR_AppName;
    if ((GIT_BRANCH != "master") || (!((ReleaseStatus.compare("r", Qt::CaseInsensitive)==0) || (ReleaseStatus.compare("rc", Qt::CaseInsensitive)==0) || (ReleaseStatus.compare("beta", Qt::CaseInsensitive)==0)))) {
        name += "-"+GIT_BRANCH;
    }
    return name;
}

const QString getDefaultAppRoot()
{
    QString approot = STR_AppRoot;
    if ((GIT_BRANCH != "master") || (!((ReleaseStatus.compare("r", Qt::CaseInsensitive)==0) || (ReleaseStatus.compare("rc", Qt::CaseInsensitive)==0) || (ReleaseStatus.compare("beta", Qt::CaseInsensitive)==0)))) {
        approot += "-"+GIT_BRANCH;
    }
    return approot;
}

bool gfxEgnineIsSupported(GFXEngine e)
{
#if defined(Q_OS_WIN32)
    Q_UNUSED(e)
    return true;
#else
    switch(e) {
    case GFX_OpenGL:
    case GFX_Software:
       return true;
    case GFX_ANGLE:
    default:
       return false;
    }
#endif
}
GFXEngine currentGFXEngine()
{
    QSettings settings;

    return (GFXEngine)qMin(settings.value(GFXEngineSetting, GFX_OpenGL).toUInt(), (unsigned int)MaxGFXEngine);
}
void setCurrentGFXEngine(GFXEngine e)
{
    QSettings settings;

    settings.setValue(GFXEngineSetting, qMin((unsigned int)e, (unsigned int)MaxGFXEngine));
}

QString getOpenGLVersionString()
{
    static QString glversion;
   if (glversion.isEmpty()) {

#ifdef BROKEN_OPENGL_BUILD
        glversion="LegacyGFX";
        qDebug() << "This LegacyGFX build has been created without the need for OpenGL";
#else

        QGLWidget w;
        w.makeCurrent();

        QOpenGLFunctions f;
        f.initializeOpenGLFunctions();
        glversion = QString(QLatin1String(reinterpret_cast<const char*>(f.glGetString(GL_VERSION))));
        qDebug() << "OpenGL Version:" << glversion;
#endif
   }
   return glversion;
}

float getOpenGLVersion()
{
#ifdef BROKEN_OPENGL_BUILD
    return 0;
#else
    QString glversion = getOpenGLVersionString();
    glversion = glversion.section(" ",0,0);
    bool ok;
    float v = glversion.toFloat(&ok);

    if (!ok) {
        QString tmp = glversion.section(".",0,1);
        v = tmp.toFloat(&ok);
        if (!ok) {
            // just look at major, we are only interested in whether we have OpenGL 2.0 anyway
            tmp = glversion.section(".",0,0);
            v = tmp.toFloat(&ok);
        }
    }
    return v;
#endif
}

QString getGraphicsEngine()
{
    QString gfxEngine = QString();
#ifdef BROKEN_OPENGL_BUILD
    gfxEngine = CSTR_GFX_BrokenGL;
#else
    QString glversion = getOpenGLVersionString();
    if (glversion.contains(CSTR_GFX_ANGLE)) {
        gfxEngine = CSTR_GFX_ANGLE;
    } else {
        gfxEngine = CSTR_GFX_OpenGL;
    }
#endif
    return gfxEngine;
}
QString getBranchVersion()
{
    QString version = STR_TR_AppVersion;
    version += " [";
    if (GIT_BRANCH != "master") {
        version += GIT_BRANCH+"-";
    }
    version += GIT_REVISION +" ";
    version += getGraphicsEngine()+"]";

    return version;
}

QString appResourcePath()
{
#ifdef Q_OS_MAC
    QString path = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../Resources");
#else
    // not sure where it goes on Linux yet
    QString path = QCoreApplication::applicationDirPath();
#endif
    return path;
}

Qt::DayOfWeek firstDayOfWeekFromLocale()
{
    return QLocale::system().firstDayOfWeek();
}

int idealThreads() { return QThread::idealThreadCount(); }

qint64 timezoneOffset()
{
    static bool ok = false;
    static qint64 _TZ_offset = 0;

    if (ok) { return _TZ_offset; }

    QDateTime d1 = QDateTime::currentDateTime();
    QDateTime d2 = d1;
    d1.setTimeSpec(Qt::UTC);
    _TZ_offset = d2.secsTo(d1);
    _TZ_offset *= 1000L;
    return _TZ_offset;
}

QString weightString(float kg, UnitSystem us)
{
    if (us == US_Undefined) {
        us = p_profile->general->unitSystem();
    }

    if (us == US_Metric) {
        return QString("%1kg").arg(kg, 0, 'f', 2);
    } else if (us == US_Archiac) {
        int oz = (kg * 1000.0) / (float)ounce_convert;
        int lb = oz / 16.0;
        oz = oz % 16;
        return QString("%1lb %2oz").arg(lb, 0, 10).arg(oz);
    }

    return ("Bad UnitSystem");
}

bool operator <(const ValueCount &a, const ValueCount &b)
{
    return a.value < b.value;
}

bool removeDir(const QString &path)
{
    bool result = true;
    QDir dir(path);

    if (dir.exists(path)) {
        Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  |
                  QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
            if (info.isDir()) { // Recurse to remove this child directory
                result = removeDir(info.absoluteFilePath());
            } else { // File
                result = QFile::remove(info.absoluteFilePath());
            }

            if (!result) {
                return result;
            }
        }
        result = dir.rmdir(path);
    }

    return result;
}

void copyPath(QString src, QString dst)
{
    QDir dir(src);
    if (!dir.exists())
        return;

    // Recursively handle directories
    foreach (QString d, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString dst_path = dst + QDir::separator() + d;
        dir.mkpath(dst_path);
        copyPath(src + QDir::separator() + d, dst_path);
    }

    // Files
    foreach (QString f, dir.entryList(QDir::Files)) {
        QString srcFile = src + QDir::separator() + f;
        QString destFile = dst + QDir::separator() + f;

        if (!QFile::exists(destFile)) {
            QFile::copy(srcFile, destFile);
        }
    }
}

QString GFXEngineNames[MaxGFXEngine+1]; // Set by initializeStrings()

QString STR_UNIT_M;
QString STR_UNIT_CM;
QString STR_UNIT_INCH;
QString STR_UNIT_FOOT;
QString STR_UNIT_POUND;
QString STR_UNIT_OUNCE;
QString STR_UNIT_KG;
QString STR_UNIT_CMH2O;
QString STR_UNIT_Hours;
QString STR_UNIT_Minutes;
QString STR_UNIT_Seconds;
QString STR_UNIT_h;
QString STR_UNIT_m;
QString STR_UNIT_s;
QString STR_UNIT_ms;
QString STR_UNIT_BPM;       // Beats per Minute
QString STR_UNIT_LPM;       // Litres per Minute
QString STR_UNIT_ml;        // MilliLitres
QString STR_UNIT_Litres;
QString STR_UNIT_Hz;
QString STR_UNIT_EventsPerHour;
QString STR_UNIT_BreathsPerMinute;
QString STR_UNIT_Percentage;
QString STR_UNIT_Unknown;
QString STR_UNIT_Ratio;
QString STR_UNIT_Severity;
QString STR_UNIT_Degrees;

QString STR_MessageBox_Question;
QString STR_MessageBox_Error;
QString STR_MessageBox_Warning;
QString STR_MessageBox_Information;
QString STR_MessageBox_Busy;
QString STR_MessageBox_PleaseNote;
QString STR_MessageBox_Yes;
QString STR_MessageBox_No;
QString STR_MessageBox_Cancel;
QString STR_MessageBox_Destroy;
QString STR_MessageBox_Save;

QString STR_Empty_NoData;
QString STR_Empty_Brick;
QString STR_Empty_NoGraphs;
QString STR_Empty_SummaryOnly;
QString STR_Empty_NoSessions;


QString STR_TR_BMI;         // Short form of Body Mass Index
QString STR_TR_Weight;
QString STR_TR_Zombie;
QString STR_TR_PulseRate;   // Pulse / Heart rate
QString STR_TR_SpO2;
QString STR_TR_Plethy;      // Plethysomogram
QString STR_TR_Pressure;

QString STR_TR_Daily;
QString STR_TR_Profile;
QString STR_TR_Overview;
QString STR_TR_Oximetry;

QString STR_TR_Oximeter;
QString STR_TR_EventFlags;

QString STR_TR_Inclination;
QString STR_TR_Orientation;


// Machine type names.
QString STR_TR_CPAP;    // Constant Positive Airway Pressure
QString STR_TR_BIPAP;   // Bi-Level Positive Airway Pressure
QString STR_TR_BiLevel; // Another name for BiPAP
QString STR_TR_EPAP;    // Expiratory Positive Airway Pressure
QString STR_TR_EPAPLo;  // Expiratory Positive Airway Pressure, Low
QString STR_TR_EPAPHi;  // Expiratory Positive Airway Pressure, High
QString STR_TR_IPAP;    // Inspiratory Positive Airway Pressure
QString STR_TR_IPAPLo;  // Inspiratory Positive Airway Pressure, Low
QString STR_TR_IPAPHi;  // Inspiratory Positive Airway Pressure, High
QString STR_TR_APAP;    // Automatic Positive Airway Pressure
QString STR_TR_ASV;     // Assisted Servo Ventilator
QString STR_TR_AVAPS;   // Average Volume Assured Pressure Support
QString STR_TR_STASV;

QString STR_TR_Humidifier;

QString STR_TR_H;       // Short form of Hypopnea
QString STR_TR_OA;      // Short form of Obstructive Apnea
QString STR_TR_UA;      // Short form of Unspecified Apnea
QString STR_TR_CA;      // Short form of Clear Airway Apnea
QString STR_TR_FL;      // Short form of Flow Limitation
QString STR_TR_SA;      // Short form of SensAwake
QString STR_TR_LE;      // Short form of Leak Event
QString STR_TR_EP;      // Short form of Expiratory Puff
QString STR_TR_VS;      // Short form of Vibratory Snore
QString STR_TR_VS2;     // Short form of Secondary Vibratory Snore (Some Philips Respironics Machines have two sources)
QString STR_TR_RERA;    // Acronym for Respiratory Effort Related Arousal
QString STR_TR_PP;      // Short form for Pressure Pulse
QString STR_TR_P;       // Short form for Pressure Event
QString STR_TR_RE;      // Short form of Respiratory Effort Related Arousal
QString STR_TR_NR;      // Short form of Non Responding event? (forgot sorry)
QString STR_TR_NRI;     // Sorry I Forgot.. it's a flag on Intellipap machines
QString STR_TR_O2;      // SpO2 Desaturation
QString STR_TR_PC;      // Short form for Pulse Change
QString STR_TR_UF1;     // Short form for User Flag 1
QString STR_TR_UF2;     // Short form for User Flag 2
QString STR_TR_UF3;     // Short form for User Flag 3



QString STR_TR_PS;     // Short form of Pressure Support
QString STR_TR_AHI;    // Short form of Apnea Hypopnea Index
QString STR_TR_RDI;    // Short form of Respiratory Distress Index
QString STR_TR_AI;     // Short form of Apnea Index
QString STR_TR_HI;     // Short form of Hypopnea Index
QString STR_TR_UAI;    // Short form of Uncatagorized Apnea Index
QString STR_TR_CAI;    // Short form of Clear Airway Index
QString STR_TR_FLI;    // Short form of Flow Limitation Index
//QString STR_TR_SAI;    // Short form of SensAwake Index
QString STR_TR_REI;    // Short form of RERA Index
QString STR_TR_EPI;    // Short form of Expiratory Puff Index
QString STR_TR_CSR;    // Short form of Cheyne Stokes Respiration
QString STR_TR_PB;     // Short form of Periodic Breathing


// Graph Titles
QString STR_TR_IE;              // Inspiratory Expiratory Ratio
QString STR_TR_InspTime;        // Inspiratory Time
QString STR_TR_ExpTime;         // Expiratory Time
QString STR_TR_RespEvent;       // Respiratory Event
QString STR_TR_FlowLimitation;
QString STR_TR_FlowLimit;
//QString STR_TR_FlowLimitation;
QString STR_TR_SensAwake;
QString STR_TR_PatTrigBreath;   // Patient Triggered Breath
QString STR_TR_TgtMinVent;      // Target Minute Ventilation
QString STR_TR_TargetVent;      // Target Ventilation
QString STR_TR_MinuteVent;      // Minute Ventilation
QString STR_TR_TidalVolume;
QString STR_TR_RespRate;        // Respiratory Rate
QString STR_TR_Snore;
QString STR_TR_Leak;
QString STR_TR_Leaks;
QString STR_TR_LargeLeak;
QString STR_TR_LL;
QString STR_TR_TotalLeaks;
QString STR_TR_UnintentionalLeaks;
QString STR_TR_MaskPressure;
QString STR_TR_FlowRate;
QString STR_TR_SleepStage;
QString STR_TR_Usage;
QString STR_TR_Sessions;
QString STR_TR_PrRelief; // Pressure Relief

QString STR_TR_Bookmarks;
QString STR_TR_SleepyHead;
QString STR_TR_AppVersion;

QString STR_TR_Default;

QString STR_TR_Mode;
QString STR_TR_Model;
QString STR_TR_Brand;
QString STR_TR_Serial;
QString STR_TR_Series;
QString STR_TR_Machine;
QString STR_TR_Channel;
QString STR_TR_Settings;

QString STR_TR_Name;
QString STR_TR_DOB;    // Date of Birth
QString STR_TR_Phone;
QString STR_TR_Address;
QString STR_TR_Email;
QString STR_TR_PatientID;
QString STR_TR_Date;

QString STR_TR_BedTime;
QString STR_TR_WakeUp;
QString STR_TR_MaskTime;
QString STR_TR_Unknown;
QString STR_TR_None;
QString STR_TR_Ready;

QString STR_TR_First;
QString STR_TR_Last;
QString STR_TR_Start;
QString STR_TR_End;
QString STR_TR_On;
QString STR_TR_Off;
QString STR_TR_Yes;
QString STR_TR_No;

QString STR_TR_Min;    // Minimum
QString STR_TR_Max;    // Maximum
QString STR_TR_Med;    // Median

QString STR_TR_Average;
QString STR_TR_Median;
QString STR_TR_Avg;    // Short form of Average
QString STR_TR_WAvg;   // Short form of Weighted Average

void initializeStrings()
{
    GFXEngineNames[GFX_Software] = QObject::tr("Software Engine");
    GFXEngineNames[GFX_ANGLE] = QObject::tr("ANGLE / OpenGLES");
    GFXEngineNames[GFX_OpenGL] = QObject::tr("Desktop OpenGL");

    STR_UNIT_M = QObject::tr(" m");
    STR_UNIT_CM = QObject::tr(" cm");
    STR_UNIT_INCH = QObject::tr("\"");
    STR_UNIT_FOOT = QObject::tr("ft");
    STR_UNIT_POUND = QObject::tr("lb");
    STR_UNIT_OUNCE = QObject::tr("oz");
    STR_UNIT_KG = QObject::tr("Kg");
    STR_UNIT_CMH2O = QObject::tr("cmH2O");
    STR_UNIT_Hours = QObject::tr("Hours");
    STR_UNIT_Minutes = QObject::tr("Minutes");
    STR_UNIT_Seconds = QObject::tr("Seconds");
    STR_UNIT_h = QObject::tr("h"); // hours shortform
    STR_UNIT_m = QObject::tr("m"); // minutes shortform
    STR_UNIT_s = QObject::tr("s"); // seconds shortform
    STR_UNIT_ms = QObject::tr("ms"); // milliseconds
    STR_UNIT_EventsPerHour = QObject::tr("Events/hr"); // Events per hour
    STR_UNIT_Percentage = QObject::tr("%");
    STR_UNIT_Hz = QObject::tr("Hz");          // Hertz
    STR_UNIT_BPM = QObject::tr("bpm");        // Beats per Minute
    STR_UNIT_LPM = QObject::tr("L/min");      // Litres per Minute
    STR_UNIT_Litres = QObject::tr("Litres");
    STR_UNIT_ml = QObject::tr("ml");        // millilitres
    STR_UNIT_BreathsPerMinute = QObject::tr("Breaths/min"); // Breaths per minute
    STR_UNIT_Unknown = QObject::tr("?");
    STR_UNIT_Ratio = QObject::tr("ratio");
    STR_UNIT_Severity = QObject::tr("Severity (0-1)");
    STR_UNIT_Degrees = QObject::tr("Degrees");

    STR_MessageBox_Question = QObject::tr("Question");
    STR_MessageBox_Error = QObject::tr("Error");
    STR_MessageBox_Warning = QObject::tr("Warning");
    STR_MessageBox_Information = QObject::tr("Information");
    STR_MessageBox_Busy = QObject::tr("Busy");
    STR_MessageBox_PleaseNote = QObject::tr("Please Note");

    STR_Empty_NoData = QObject::tr("No Data Available");
    STR_Empty_Brick = QObject::tr("Compliance Only :(");
    STR_Empty_NoGraphs = QObject::tr("Graphs Switched Off");
    STR_Empty_SummaryOnly = QObject::tr("Summary Only :(");
    STR_Empty_NoSessions = QObject::tr("Sessions Switched Off");


    // Dialog box options
    STR_MessageBox_Yes = QObject::tr("&Yes");
    STR_MessageBox_No = QObject::tr("&No");
    STR_MessageBox_Cancel = QObject::tr("&Cancel");
    STR_MessageBox_Destroy = QObject::tr("&Destroy");;
    STR_MessageBox_Save = QObject::tr("&Save");

    STR_TR_BMI = QObject::tr("BMI");              // Short form of Body Mass Index
    STR_TR_Weight = QObject::tr("Weight");
    STR_TR_Zombie = QObject::tr("Zombie");
    STR_TR_PulseRate = QObject::tr("Pulse Rate"); // Pulse / Heart rate
    STR_TR_SpO2 = QObject::tr("SpO2");
    STR_TR_Plethy = QObject::tr("Plethy");        // Plethysomogram
    STR_TR_Pressure = QObject::tr("Pressure");

    STR_TR_Daily = QObject::tr("Daily");
    STR_TR_Profile = QObject::tr("Profile");
    STR_TR_Overview = QObject::tr("Overview");
    STR_TR_Oximetry = QObject::tr("Oximetry");

    STR_TR_Oximeter = QObject::tr("Oximeter");
    STR_TR_EventFlags = QObject::tr("Event Flags");


    STR_TR_Default = QObject::tr("Default");

    // Machine type names.
    STR_TR_CPAP = QObject::tr("CPAP");    // Constant Positive Airway Pressure
    STR_TR_BIPAP = QObject::tr("BiPAP");  // Bi-Level Positive Airway Pressure
    STR_TR_BiLevel = QObject::tr("Bi-Level"); // Another name for BiPAP
    STR_TR_EPAP = QObject::tr("EPAP");    // Expiratory Positive Airway Pressure
    STR_TR_EPAPLo = QObject::tr("Min EPAP"); // Lower Expiratory Positive Airway Pressure
    STR_TR_EPAPHi = QObject::tr("Max EPAP"); // Higher Expiratory Positive Airway Pressure
    STR_TR_IPAP = QObject::tr("IPAP");    // Inspiratory Positive Airway Pressure
    STR_TR_IPAPLo = QObject::tr("Min IPAP"); // Lower Inspiratory Positive Airway Pressure
    STR_TR_IPAPHi = QObject::tr("Max IPAP"); // Higher Inspiratory Positive Airway Pressure
    STR_TR_APAP = QObject::tr("APAP");    // Automatic Positive Airway Pressure
    STR_TR_ASV = QObject::tr("ASV");      // Assisted Servo Ventilator
    STR_TR_AVAPS = QObject::tr("AVAPS");  // Average Volume Assured Pressure Support
    STR_TR_STASV = QObject::tr("ST/ASV");

    STR_TR_Humidifier = QObject::tr("Humidifier");

    STR_TR_H = QObject::tr("H");      // Short form of Hypopnea
    STR_TR_OA = QObject::tr("OA");    // Short form of Obstructive Apnea
    STR_TR_UA = QObject::tr("A");     // Short form of Unspecified Apnea
    STR_TR_CA = QObject::tr("CA");    // Short form of Clear Airway Apnea
    STR_TR_FL = QObject::tr("FL");    // Short form of Flow Limitation
    STR_TR_SA = QObject::tr("SA");    // Short form of Flow Limitation
    STR_TR_LE = QObject::tr("LE");    // Short form of Leak Event
    STR_TR_EP = QObject::tr("EP");    // Short form of Expiratory Puff
    STR_TR_VS = QObject::tr("VS");    // Short form of Vibratory Snore
    STR_TR_VS2 =
        QObject::tr("VS2");  // Short form of Secondary Vibratory Snore (Some Philips Respironics Machines have two sources)
    STR_TR_RERA = QObject::tr("RERA"); // Acronym for Respiratory Effort Related Arousal
    STR_TR_PP = QObject::tr("PP");    // Short form for Pressure Pulse
    STR_TR_P = QObject::tr("P");      // Short form for Pressure Event
    STR_TR_RE = QObject::tr("RE");    // Short form of Respiratory Effort Related Arousal
    STR_TR_NR = QObject::tr("NR");    // Short form of Non Responding event? (forgot sorry)
    STR_TR_NRI = QObject::tr("NRI");  // Sorry I Forgot.. it's a flag on Intellipap machines
    STR_TR_O2 = QObject::tr("O2");    // SpO2 Desaturation
    STR_TR_PC = QObject::tr("PC");    // Short form for Pulse Change
    STR_TR_UF1 = QObject::tr("UF1");    // Short form for User Flag 1
    STR_TR_UF2 = QObject::tr("UF2");    // Short form for User Flag 2
    STR_TR_UF3 = QObject::tr("UF3");    // Short form for User Flag 3

    STR_TR_PS = QObject::tr("PS");    // Short form of Pressure Support
    STR_TR_AHI = QObject::tr("AHI");  // Short form of Apnea Hypopnea Index
    STR_TR_RDI = QObject::tr("RDI");  // Short form of Respiratory Distress Index
    STR_TR_AI = QObject::tr("AI");    // Short form of Apnea Index
    STR_TR_HI = QObject::tr("HI");    // Short form of Hypopnea Index
    STR_TR_UAI = QObject::tr("UAI");  // Short form of Uncatagorized Apnea Index
    STR_TR_CAI = QObject::tr("CAI");  // Short form of Clear Airway Index
    STR_TR_FLI = QObject::tr("FLI");  // Short form of Flow Limitation Index
//    STR_TR_SAI = QObject::tr("SAI");  // Short form of SleepAwake Index
    STR_TR_REI = QObject::tr("REI");  // Short form of RERA Index
    STR_TR_EPI = QObject::tr("EPI");  // Short form of Expiratory Puff Index
    STR_TR_CSR = QObject::tr("ÇSR");  // Short form of Cheyne Stokes Respiration
    STR_TR_PB = QObject::tr("PB");    // Short form of Periodic Breathing


    // Graph Titles
    STR_TR_IE = QObject::tr("IE");    // Inspiratory Expiratory Ratio
    STR_TR_InspTime = QObject::tr("Insp. Time");  // Inspiratory Time
    STR_TR_ExpTime = QObject::tr("Exp. Time");    // Expiratory Time
    STR_TR_RespEvent = QObject::tr("Resp. Event");  // Respiratory Event
    STR_TR_FlowLimitation = QObject::tr("Flow Limitation");
    STR_TR_FlowLimit = QObject::tr("Flow Limit");
    STR_TR_SensAwake = QObject::tr("SensAwake");
    STR_TR_PatTrigBreath = QObject::tr("Pat. Trig. Breath"); // Patient Triggered Breath
    STR_TR_TgtMinVent = QObject::tr("Tgt. Min. Vent");      // Target Minute Ventilation
    STR_TR_TargetVent = QObject::tr("Target Vent.");        // Target Ventilation
    STR_TR_MinuteVent = QObject::tr("Minute Vent."); // Minute Ventilation
    STR_TR_TidalVolume = QObject::tr("Tidal Volume");
    STR_TR_RespRate = QObject::tr("Resp. Rate");  // Respiratory Rate
    STR_TR_Snore = QObject::tr("Snore");
    STR_TR_Leak = QObject::tr("Leak");
    STR_TR_Leaks = QObject::tr("Leaks");
    STR_TR_LargeLeak = QObject::tr("Large Leak");
    STR_TR_LL = QObject::tr("LL"); // Large Leak
    STR_TR_TotalLeaks = QObject::tr("Total Leaks");
    STR_TR_UnintentionalLeaks = QObject::tr("Unintentional Leaks");
    STR_TR_MaskPressure = QObject::tr("MaskPressure");
    STR_TR_FlowRate = QObject::tr("Flow Rate");
    STR_TR_SleepStage = QObject::tr("Sleep Stage");
    STR_TR_Usage = QObject::tr("Usage");
    STR_TR_Sessions = QObject::tr("Sessions");
    STR_TR_PrRelief = QObject::tr("Pr. Relief"); // Pressure Relief

    STR_TR_Bookmarks = QObject::tr("Bookmarks");
    STR_TR_SleepyHead = QObject::tr("SleepyHead");
    STR_TR_AppVersion = QObject::tr("v%1").arg(VersionString);

    STR_TR_Mode = QObject::tr("Mode");
    STR_TR_Model = QObject::tr("Model");
    STR_TR_Brand = QObject::tr("Brand");
    STR_TR_Serial = QObject::tr("Serial");
    STR_TR_Series = QObject::tr("Series");
    STR_TR_Machine = QObject::tr("Machine");
    STR_TR_Channel = QObject::tr("Channel");
    STR_TR_Settings = QObject::tr("Settings");

    STR_TR_Inclination = QObject::tr("Inclination");
    STR_TR_Orientation = QObject::tr("Orientation");

    STR_TR_Name = QObject::tr("Name");
    STR_TR_DOB = QObject::tr("DOB");  // Date of Birth
    STR_TR_Phone = QObject::tr("Phone");
    STR_TR_Address = QObject::tr("Address");
    STR_TR_Email = QObject::tr("Email");
    STR_TR_PatientID = QObject::tr("Patient ID");
    STR_TR_Date = QObject::tr("Date");

    STR_TR_BedTime = QObject::tr("Bedtime");
    STR_TR_WakeUp = QObject::tr("Wake-up");
    STR_TR_MaskTime = QObject::tr("Mask Time");
    STR_TR_Unknown = QObject::tr("Unknown");
    STR_TR_None = QObject::tr("None");
    STR_TR_Ready = QObject::tr("Ready");

    STR_TR_First = QObject::tr("First");
    STR_TR_Last = QObject::tr("Last");
    STR_TR_Start = QObject::tr("Start");
    STR_TR_End = QObject::tr("End");
    STR_TR_On = QObject::tr("On");
    STR_TR_Off = QObject::tr("Off");
    STR_TR_Yes = QObject::tr("Yes");
    STR_TR_No = QObject::tr("No");

    STR_TR_Min = QObject::tr("Min");  // Minimum
    STR_TR_Max = QObject::tr("Max");  // Maximum
    STR_TR_Med = QObject::tr("Med");  // Median

    STR_TR_Average = QObject::tr("Average");
    STR_TR_Median = QObject::tr("Median");
    STR_TR_Avg = QObject::tr("Avg");      // Average
    STR_TR_WAvg = QObject::tr("W-Avg");   // Weighted Average
}




quint32 CRC32(const char * data, quint32 length)
{
  quint32 crc32 = 0xffffffff;

  for (quint32 idx=0; idx<length; idx++) {

    quint32 i = (data[idx]) ^ ((crc32) & 0x000000ff);

    for(int j=8; j > 0; j--) {
        if (i & 1) {
            i = (i >> 1) ^ 0xedb88320;
        } else {
            i >>= 1;
        }
    }

    crc32 = ((crc32) >> 8) ^ i;
  }

  return ~crc32;
}

quint32 crc32buf(const QByteArray& data)
{
    return CRC32(data.constData(), data.size());
}

// Gzip function
QByteArray gCompress(const QByteArray& data)
{
    QByteArray compressedData = qCompress(data);
    //  Strip the first six bytes (a 4-byte length put on by qCompress and a 2-byte zlib header)
    // and the last four bytes (a zlib integrity check).
    compressedData.remove(0, 6);
    compressedData.chop(4);

    QByteArray header;
    QDataStream ds1(&header, QIODevice::WriteOnly);
    // Prepend a generic 10-byte gzip header (see RFC 1952),
    ds1 << quint16(0x1f8b)
        << quint16(0x0800)
        << quint16(0x0000)
        << quint16(0x0000)
        << quint16(0x000b);

    // Append a four-byte CRC-32 of the uncompressed data
    // Append 4 bytes uncompressed input size modulo 2^32
    QByteArray footer;
    QDataStream ds2(&footer, QIODevice::WriteOnly);
    ds2.setByteOrder(QDataStream::LittleEndian);
    ds2 << crc32buf(data)
        << quint32(data.size());

    return header + compressedData + footer;
}


// Pinched from http://stackoverflow.com/questions/2690328/qt-quncompress-gzip-data
QByteArray gUncompress(const QByteArray & data)
{
    if (data.size() <= 4) {
        qWarning("gUncompress: Input data is truncated");
        return QByteArray();
    }

    QByteArray result;

    int ret;
    z_stream strm;
    static const int CHUNK_SIZE = 1048576;
    char *out = new char [CHUNK_SIZE];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = data.size();
    strm.next_in = (Bytef*)(data.data());

    ret = inflateInit2(&strm, 15 +  32); // gzip decoding
    if (ret != Z_OK) {
        delete [] out;
        return QByteArray();
    }

    // run inflate()
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = (Bytef*)(out);

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            qWarning() << "ret == Z_STREAM_ERROR in gzUncompress in common.cpp";
            delete [] out;
            return QByteArray();
        }

        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;     // and fall through
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            Q_UNUSED(ret)
            (void)inflateEnd(&strm);
            delete [] out;
            return QByteArray();
        }

        result.append(out, CHUNK_SIZE - strm.avail_out);
    } while (strm.avail_out == 0);

    // clean up and return
    inflateEnd(&strm);
    delete [] out;
    return result;
}
