/* Channel / Schema Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QFile>
#include <QDebug>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QMessageBox>
#include <QApplication>

#include "common.h"
#include "schema.h"
#include "common_gui.h"

#include "SleepLib/profiles.h"

QColor adjustcolor(QColor color, float ar=1.0, float ag=1.0, float ab=1.0)
{
    int r = color.red();
    int g = color.green();
    int b = color.blue();

    r += rand() & 64;
    g += rand() & 64;
    b += rand() & 64;

    r = qMin(int(r * ar), 255);
    g = qMin(int(g * ag), 255);
    b = qMin(int(b * ab), 255);

    return QColor(r,g,b, color.alpha());
}


QColor darken(QColor color, float p);

namespace schema {

void resetChannels();

ChannelList channel;
Channel EmptyChannel;
Channel *SessionEnabledChannel;

QHash<QString, ChanType> ChanTypes;
QHash<QString, DataType> DataTypes;
QHash<QString, ScopeType> Scopes;

bool schema_initialized = false;

void setOrders() {
    schema::channel[CPAP_PB].setOrder(1);
    schema::channel[CPAP_CSR].setOrder(1);
    schema::channel[CPAP_Ramp].setOrder(2);
    schema::channel[CPAP_LargeLeak].setOrder(2);
    schema::channel[CPAP_ClearAirway].setOrder(3);
    schema::channel[CPAP_Obstructive].setOrder(4);
    schema::channel[CPAP_Apnea].setOrder(4);
    schema::channel[CPAP_NRI].setOrder(3);
    schema::channel[CPAP_Hypopnea].setOrder(5);
    schema::channel[CPAP_FlowLimit].setOrder(6);
    schema::channel[CPAP_RERA].setOrder(6);
    schema::channel[CPAP_VSnore].setOrder(7);
    schema::channel[CPAP_VSnore2].setOrder(8);
    schema::channel[CPAP_ExP].setOrder(6);
    schema::channel[CPAP_UserFlag1].setOrder(256);
    schema::channel[CPAP_UserFlag2].setOrder(257);
}

void init()
{
    if (schema_initialized) { return; }

    schema_initialized = true;

    EmptyChannel = Channel(0, DATA, MT_UNKNOWN, DAY, "Empty", "Empty", "Empty Channel", "", "");
    SessionEnabledChannel = new Channel(1, DATA, MT_UNKNOWN, DAY, "Enabled", "Enabled", "Session Enabled", "", "");

    channel.channels[1] = SessionEnabledChannel;
    channel.names["Enabled"] = SessionEnabledChannel;
    SESSION_ENABLED = 1;
    ChanTypes["data"] = DATA;
    //Types["waveform"]=WAVEFORM;
    ChanTypes["setting"] = SETTING;

    Scopes["session"] = SESSION;
    Scopes["day"] = DAY;
    Scopes["machine"] = MACHINE;
    Scopes["global"] = GLOBAL;

    DataTypes[""] = DEFAULT;
    DataTypes["bool"] = BOOL;
    DataTypes["double"] = DOUBLE;
    DataTypes["integer"] = INTEGER;
    DataTypes["string"] = STRING;
    DataTypes["richtext"] = RICHTEXT;
    DataTypes["date"] = DATE;
    DataTypes["datetime"] = DATETIME;
    DataTypes["time"] = TIME;

    // Note: Old channel names stored in channels.xml are not translatable.. they need to be moved to be defined AFTER here instead
    if (!schema::channel.Load(":/docs/channels.xml")) {
        QMessageBox::critical(0, STR_MessageBox_Error,
                              QObject::tr("Couldn't parse Channels.xml, this build is seriously borked, no choice but to abort!!"),
                              QMessageBox::Ok);
        QApplication::exit(-1);
    }


    // Lookup Code strings are used internally and not meant to be tranlsated

    //                  Group                ChannelID            Code    Type     Scope    Lookup Code      Translable Name                 Description                                   Shortened Name              Units String            FieldType   Default Color

    // Pressure Related Settings
    schema::channel.add(GRP_CPAP, new Channel(CPAP_Pressure      = 0x110C, WAVEFORM,    MT_CPAP, SESSION, "Pressure",       STR_TR_Pressure,                QObject::tr("Therapy Pressure"),               STR_TR_Pressure,             STR_UNIT_CMH2O,        DEFAULT,    QColor("red")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_IPAP          = 0x110D, WAVEFORM,    MT_CPAP, SESSION, "IPAP",           STR_TR_IPAP,                    QObject::tr("Inspiratory Pressure"),           STR_TR_IPAP,                 STR_UNIT_CMH2O,        DEFAULT,    QColor("red")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_IPAPLo        = 0x1110, WAVEFORM,    MT_CPAP, SESSION, "IPAPLo",         STR_TR_IPAPLo,                  QObject::tr("Lower Inspiratory Pressure"),     STR_TR_IPAPLo,               STR_UNIT_CMH2O,        DEFAULT,    QColor("orange")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_IPAPHi        = 0x1111, WAVEFORM,    MT_CPAP, SESSION, "IPAPHi",         STR_TR_IPAPHi,                  QObject::tr("Higher Inspiratory Pressure"),    STR_TR_IPAPHi,               STR_UNIT_CMH2O,        DEFAULT,    QColor("orange")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_EPAP          = 0x110E, WAVEFORM,    MT_CPAP, SESSION, "EPAP",           STR_TR_EPAP,                    QObject::tr("Expiratory Pressure"),            STR_TR_EPAP,                 STR_UNIT_CMH2O,        DEFAULT,    QColor("green")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_EPAPLo        = 0x111C, WAVEFORM,    MT_CPAP, SESSION, "EPAPLo",         STR_TR_EPAPLo,                  QObject::tr("Lower Expiratory Pressure"),      STR_TR_EPAPLo,               STR_UNIT_CMH2O,        DEFAULT,    QColor("light blue")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_EPAPHi        = 0x111D, WAVEFORM,    MT_CPAP, SESSION, "EPAPHi",         STR_TR_EPAPHi,                  QObject::tr("Higher Expiratory Pressure"),     STR_TR_EPAPHi,               STR_UNIT_CMH2O,        DEFAULT,    QColor("aqua")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_PS            = 0x110F, WAVEFORM,    MT_CPAP, SESSION, "PS",             STR_TR_PS,                      QObject::tr("Pressure Support"),               STR_TR_PS,                   STR_UNIT_CMH2O,        DEFAULT,    QColor("grey")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_PSMin         = 0x111A, SETTING,     MT_CPAP, SESSION, "PSMin",          QObject::tr("PS Min") ,         QObject::tr("Pressure Support Minimum"),       QObject::tr("PS Min"),       STR_UNIT_CMH2O,        DEFAULT,    QColor("dark cyan")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_PSMax         = 0x111B, SETTING,     MT_CPAP, SESSION, "PSMax",          QObject::tr("PS Max"),          QObject::tr("Pressure Support Maximum"),       QObject::tr("PS Max"),       STR_UNIT_CMH2O,        DEFAULT,    QColor("dark magenta")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_PressureMin   = 0x1020, SETTING,     MT_CPAP, SESSION, "PressureMin",    QObject::tr("Min Pressure"),    QObject::tr("Minimum Therapy Pressure"),       QObject::tr("Pressure Min"), STR_UNIT_CMH2O,        DEFAULT,    QColor("orange")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_PressureMax   = 0x1021, SETTING,     MT_CPAP, SESSION, "PressureMax",    QObject::tr("Max Pressure"),    QObject::tr("Maximum Therapy Pressure"),       QObject::tr("Pressure Max"), STR_UNIT_CMH2O,        DEFAULT,    QColor("light blue")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_RampTime      = 0x1022, SETTING,     MT_CPAP, SESSION, "RampTime",       QObject::tr("Ramp Time") ,      QObject::tr("Ramp Delay Period"),              QObject::tr("Ramp Time"),    STR_UNIT_Minutes,      DEFAULT,    QColor("black")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_RampPressure  = 0x1023, SETTING,     MT_CPAP, SESSION, "RampPressure",   QObject::tr("Ramp Pressure"),   QObject::tr("Starting Ramp Pressure"),         QObject::tr("Ramp Pressure"),STR_UNIT_CMH2O,        DEFAULT,    QColor("black")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_Ramp          = 0x1027, SPAN,        MT_CPAP, SESSION, "Ramp",           QObject::tr("Ramp Event") ,     QObject::tr("Ramp Event"),                     QObject::tr("Ramp"),         STR_UNIT_EventsPerHour,DEFAULT,    QColor("light blue")));
    // Flags
    schema::channel.add(GRP_CPAP, new Channel(CPAP_CSR           = 0x1000, SPAN,        MT_CPAP, SESSION, "CSR",
            QObject::tr("Cheyne Stokes Respiration"), QObject::tr("An abnormal period of Cheyne Stokes Respiration"), QObject::tr("CSR"), STR_UNIT_Percentage,DEFAULT,    COLOR_CSR));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_PB            = 0x1028, SPAN,        MT_CPAP, SESSION, "PB",
            QObject::tr("Periodic Breathing"),QObject::tr("An abnormal period of Periodic Breathing"), QObject::tr("PB"),STR_UNIT_Percentage,   DEFAULT,    COLOR_CSR));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_ClearAirway   = 0x1001, FLAG,        MT_CPAP, SESSION, "ClearAirway",
            QObject::tr("Clear Airway"), QObject::tr("An apnea where the airway is open"), QObject::tr("CA"),       STR_UNIT_EventsPerHour,    DEFAULT,    QColor("purple")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_Obstructive   = 0x1002, FLAG,        MT_CPAP, SESSION, "Obstructive",
            QObject::tr("Obstructive"), QObject::tr("An apnea caused by airway obstruction"), QObject::tr("OA"), STR_UNIT_EventsPerHour,    DEFAULT,    QColor("#40c0ff")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_Hypopnea      = 0x1003, FLAG,        MT_CPAP, SESSION, "Hypopnea",
            QObject::tr("Hypopnea"), QObject::tr("A partially obstructed airway"), QObject::tr("H"),        STR_UNIT_EventsPerHour,    DEFAULT,    QColor("blue")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_Apnea         = 0x1004, FLAG,        MT_CPAP, SESSION, "Apnea",
            QObject::tr("Unclassified Apnea"), QObject::tr("an apnea that couldn't be determined as Central or Obstructive, due to excessive leakage interfering with the classification process."),QObject::tr("UA"),       STR_UNIT_EventsPerHour,    DEFAULT,    QColor("dark green")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_FlowLimit     = 0x1005, FLAG,        MT_CPAP, SESSION, "FlowLimit",
            QObject::tr("Flow Limitation"), QObject::tr("An restriction in breathing from normal, causing a flattening of the flow waveform."), QObject::tr("FL"), STR_UNIT_EventsPerHour,    DEFAULT,    QColor("#404040")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_RERA          = 0x1006, FLAG,        MT_CPAP, SESSION, "RERA",
            QObject::tr("RERA"),QObject::tr("Respiratory Effort Related Arousal: An restriction in breathing that causes an either an awakening or sleep disturbance."), QObject::tr("RE"),       STR_UNIT_EventsPerHour,    DEFAULT,    COLOR_Gold));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_VSnore        = 0x1007, FLAG,        MT_CPAP, SESSION, "VSnore",
            QObject::tr("Vibratory Snore"), QObject::tr("A vibratory snore"), QObject::tr("VS"),       STR_UNIT_EventsPerHour,    DEFAULT,    QColor("red")));
    schema::channel.add(GRP_CPAP, new Channel(CPAP_VSnore2       = 0x1008, FLAG,        MT_CPAP, SESSION, "VSnore2",
            QObject::tr("Vibratory Snore (VS2) "),QObject::tr("A vibratory snore as detcted by a System One machine"),QObject::tr("VS2"),      STR_UNIT_EventsPerHour,    DEFAULT,    QColor("red")));
    // This Large Leak record is just a flag marker, used by Intellipap for one
    schema::channel.add(GRP_CPAP, new Channel(CPAP_LeakFlag      = 0x100a, FLAG,        MT_CPAP, SESSION, "LeakFlag",
            QObject::tr("Leak Flag"), QObject::tr("A large mask leak affecting machine performance."), QObject::tr("LF"),       STR_UNIT_EventsPerHour,    DEFAULT,    QColor("light gray")));

    // The following is a Large Leak record that references a waveform span
    schema::channel.add(GRP_CPAP, new Channel(CPAP_LargeLeak     = 0x1158, SPAN,        MT_CPAP, SESSION, "LeakSpan",
            QObject::tr("Large Leak"),QObject::tr("A large mask leak affecting machine performance."), QObject::tr("LL"),       STR_UNIT_EventsPerHour,    DEFAULT,    QColor("light gray")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_NRI           = 0x100b, FLAG,        MT_CPAP, SESSION, "NRI",
            QObject::tr("Non Responding Event"), QObject::tr("A type of respiratory event that won't respond to a pressure increase."), QObject::tr("NR"),       STR_UNIT_EventsPerHour,    DEFAULT,    QColor("orange")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_ExP           = 0x100c, FLAG,        MT_CPAP, SESSION, "ExP",
            QObject::tr("Expiratory Puff"), QObject::tr("Intellipap event where you breathe out your mouth."), QObject::tr("EP"),       STR_UNIT_EventsPerHour,    DEFAULT,    QColor("dark magenta")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_SensAwake     = 0x100d, FLAG,        MT_CPAP, SESSION, "SensAwake",
            QObject::tr("SensAwake"),QObject::tr("SensAwake feature will reduce pressure when waking is detected."),QObject::tr("SA"),       STR_UNIT_EventsPerHour,    DEFAULT,    COLOR_Gold));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_UserFlag1     = 0x101e, FLAG,        MT_CPAP, SESSION, "UserFlag1",
            QObject::tr("User Flag #1"), QObject::tr("A user definable event detected by SleepyHead's flow waveform processor."), QObject::tr("UF1"),      STR_UNIT_EventsPerHour,    DEFAULT,    QColor(0xc0,0xc0,0xe0)));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_UserFlag2     = 0x101f, FLAG,        MT_CPAP, SESSION, "UserFlag2",
            QObject::tr("User Flag #2"),QObject::tr("A user definable event detected by SleepyHead's flow waveform processor."), QObject::tr("UF2"),      STR_UNIT_EventsPerHour,    DEFAULT,    QColor(0xa0,0xa0,0xc0)));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_UserFlag3     = 0x1024, FLAG,        MT_CPAP, SESSION, "UserFlag3",
            QObject::tr("User Flag #3"),QObject::tr("A user definable event detected by SleepyHead's flow waveform processor."), QObject::tr("UF3"),      STR_UNIT_EventsPerHour,    DEFAULT,    QColor("dark grey")));

    // Oximetry
    schema::channel.add(GRP_OXI, new Channel(OXI_Pulse           = 0x1800, WAVEFORM,    MT_OXIMETER, SESSION, "Pulse",
            QObject::tr("Pulse Rate"),                    QObject::tr("Heart rate in beats per minute"), QObject::tr("Pulse Rate"), STR_UNIT_BPM,     DEFAULT,    QColor("red")));
    schema::channel[OXI_Pulse].setLowerThreshold(40);
    schema::channel[OXI_Pulse].setUpperThreshold(130);

    schema::channel.add(GRP_OXI, new Channel(OXI_SPO2            = 0x1801, WAVEFORM,    MT_OXIMETER, SESSION, "SPO2",
            QObject::tr("SpO2 %"), QObject::tr("Blood-oxygen saturation percentage"), QObject::tr("SpO2"),       STR_UNIT_Percentage,          DEFAULT,    QColor("blue")));
    schema::channel[OXI_SPO2].setLowerThreshold(88);

    schema::channel.add(GRP_OXI, new Channel(OXI_Plethy          = 0x1802, WAVEFORM,    MT_OXIMETER, SESSION, "Plethy",
            QObject::tr("Plethysomogram"), QObject::tr("An optical Photo-plethysomogram showing heart rhythm"), QObject::tr("Plethy"),     STR_UNIT_Hz,           DEFAULT,    QColor("#404040")));

    schema::channel.add(GRP_OXI, new Channel(OXI_Perf             = 0x1805, WAVEFORM,   MT_OXIMETER, SESSION, "Perf. Index",
            QObject::tr("Perfusion Index"), QObject::tr("A relative assessment of the pulse strength at the monitoring site"), QObject::tr("Perf. Index %"), STR_UNIT_Percentage,     DEFAULT,   QColor("magenta")));

    schema::channel.add(GRP_OXI, new Channel(OXI_PulseChange     = 0x1803, FLAG,        MT_OXIMETER, SESSION, "PulseChange",
            QObject::tr("Pulse Change"), QObject::tr("A sudden (user definable) change in heart rate"), QObject::tr("PC"),         STR_UNIT_EventsPerHour,    DEFAULT,   QColor("light grey")));

    schema::channel.add(GRP_OXI, new Channel(OXI_SPO2Drop        = 0x1804, FLAG,        MT_OXIMETER, SESSION, "SPO2Drop",
            QObject::tr("SpO2 Drop"), QObject::tr("A sudden (user definable) drop in blood oxygen saturation"), QObject::tr("SD"),         STR_UNIT_EventsPerHour,    DEFAULT,    QColor("light blue")));


    schema::channel.add(GRP_CPAP, new Channel(CPAP_FlowRate          = 0x1100, WAVEFORM,    MT_CPAP, SESSION, "FlowRate",
            QObject::tr("Flow Rate"), QObject::tr("Breathing flow rate waveform"), QObject::tr("Flow Rate"), STR_UNIT_LPM,    DEFAULT,    QColor("black")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_MaskPressure      = 0x1101, WAVEFORM,    MT_CPAP, SESSION, "MaskPressure",
            QObject::tr("Mask Pressure"), QObject::tr("Mask Pressure"), QObject::tr("Mask Pressure"), STR_UNIT_CMH2O,    DEFAULT,    QColor("blue")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_MaskPressureHi    = 0x1102, WAVEFORM,    MT_CPAP, SESSION, "MaskPressureHi",
            QObject::tr("Mask Pressure"), QObject::tr("Mask Pressure (High resolution)"), QObject::tr("Mask Pressure"), STR_UNIT_CMH2O,    DEFAULT,    QColor("blue"), 0x1101)); // linked to CPAP_MaskPressure

    schema::channel.add(GRP_CPAP, new Channel(CPAP_TidalVolume       = 0x1103, WAVEFORM,    MT_CPAP, SESSION, "TidalVolume",
            QObject::tr("Tidal Volume"), QObject::tr("Amount of air displaced per breath"), QObject::tr("Tidal Volume"), STR_UNIT_ml,    DEFAULT,    QColor("magenta")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_Snore             = 0x1104, WAVEFORM,    MT_CPAP,  SESSION, "Snore",
            QObject::tr("Snore"), QObject::tr("Graph displaying snore volume"), QObject::tr("Snore"),   STR_UNIT_Unknown,       DEFAULT,    QColor("grey")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_MinuteVent        = 0x1105, WAVEFORM,    MT_CPAP, SESSION, "MinuteVent",
            QObject::tr("Minute Ventilation"), QObject::tr("Amount of air displaced per minute"), QObject::tr("Minute Vent."), STR_UNIT_LPM,    DEFAULT,    QColor("dark cyan")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_RespRate          = 0x1106, WAVEFORM,    MT_CPAP, SESSION, "RespRate",
            QObject::tr("Respiratory Rate"), QObject::tr("Rate of breaths per minute"), QObject::tr("Resp. Rate"), STR_UNIT_BreathsPerMinute,      DEFAULT,    QColor("dark magenta")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_PTB               = 0x1107, WAVEFORM,    MT_CPAP, SESSION, "PTB",
            QObject::tr("Patient Triggered Breaths"), QObject::tr("Percentage of breaths triggered by patient"),   QObject::tr("Pat. Trig. Breaths"), STR_UNIT_Percentage,   DEFAULT,    QColor("dark grey")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_Leak              = 0x1108, WAVEFORM,    MT_CPAP, SESSION, "Leak",
            QObject::tr("Leak Rate"), QObject::tr("Rate of detected mask leakage"), QObject::tr("Leak Rate"), STR_UNIT_LPM,    DEFAULT,    QColor("dark green")));
    schema::channel[CPAP_Leak].setLowerThreshold(24.0);

    schema::channel.add(GRP_CPAP, new Channel(CPAP_IE                = 0x1109, WAVEFORM,    MT_CPAP,  SESSION, "IE",
            QObject::tr("I:E Ratio"), QObject::tr("Ratio between Inspiratory and Expiratory time"), QObject::tr("I:E Ratio"), STR_UNIT_Ratio,    DEFAULT,    QColor("dark red")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_Te                = 0x110A, WAVEFORM,    MT_CPAP,  SESSION, "Te",
            QObject::tr("Expiratory Time"), QObject::tr("Time taken to breathe out"), QObject::tr("Exp. Time"),          STR_UNIT_Seconds,  DEFAULT,    QColor("dark green")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_Ti                = 0x110B, WAVEFORM,    MT_CPAP, SESSION, "Ti",
            QObject::tr("Inspiratory Time"), QObject::tr("Time taken to breathe in"), QObject::tr("Insp. Time"),         STR_UNIT_Seconds,  DEFAULT,    QColor("dark blue")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_RespEvent         = 0x1112, DATA,   MT_CPAP,  SESSION, "RespEvent",
            QObject::tr("Respiratory Event"), QObject::tr("A ResMed data source showing Respiratory Events"),  QObject::tr("Resp. Event"), STR_UNIT_EventsPerHour,   DEFAULT,    QColor("black")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_FLG               = 0x1113, WAVEFORM,   MT_CPAP,  SESSION, "FLG",
            QObject::tr("Flow Limitation"), QObject::tr("Graph showing severity of flow limitations"),   QObject::tr("Flow Limit."), STR_UNIT_Severity,      DEFAULT,    QColor("#585858")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_TgMV              = 0x1114, WAVEFORM,  MT_CPAP,   SESSION, "TgMV",
            QObject::tr("Target Minute Ventilation"), QObject::tr("Target Minute Ventilation"), QObject::tr("Target Vent."), STR_UNIT_LPM,       DEFAULT,    QColor("dark red")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_MaxLeak           = 0x1115, WAVEFORM,   MT_CPAP,  SESSION, "MaxLeak",
            QObject::tr("Maximum Leak"), QObject::tr("The maximum rate of mask leakage"), QObject::tr("Max Leaks"), STR_UNIT_LPM,    DEFAULT,    QColor("dark red")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_AHI               = 0x1116, WAVEFORM,   MT_CPAP,  SESSION, "AHI",
            QObject::tr("Apnea Hypopnea Index"), QObject::tr("Graph showing running AHI for the past hour"),  QObject::tr("AHI"), STR_UNIT_EventsPerHour, DEFAULT,  QColor("dark red")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_LeakTotal         = 0x1117, WAVEFORM,   MT_CPAP,  SESSION, "LeakTotal",
            QObject::tr("Total Leak Rate"), QObject::tr("Detected mask leakage including natural Mask leakages"),  QObject::tr("Total Leaks"), STR_UNIT_LPM,    DEFAULT,    QColor("dark green")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_LeakMedian        = 0x1118, WAVEFORM,   MT_CPAP,  SESSION, "LeakMedian",
            QObject::tr("Median Leak Rate"), QObject::tr("Median rate of detected mask leakage"), QObject::tr("Median Leaks"), STR_UNIT_LPM,    DEFAULT,    QColor("dark green")));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_RDI               = 0x1119, WAVEFORM,   MT_CPAP,  SESSION, "RDI",
            QObject::tr("Respiratory Disturbance Index"), QObject::tr("Graph showing running RDI for the past hour"),  QObject::tr("RDI"), STR_UNIT_EventsPerHour, DEFAULT,  QColor("dark red")));

    // Positional sensors
    schema::channel.add(GRP_POS, new Channel(POS_Orientation         = 0x2990, WAVEFORM,   MT_POSITION,  SESSION, "Orientation",
            QObject::tr("Orientation"), QObject::tr("Sleep position in degrees"),  QObject::tr("Orientation"),  STR_UNIT_Degrees, DEFAULT,  QColor("dark blue")));

    schema::channel.add(GRP_POS, new Channel(POS_Inclination         = 0x2991, WAVEFORM,   MT_POSITION, SESSION, "Inclination",
            QObject::tr("Inclination"), QObject::tr("Upright angle in degrees"),  QObject::tr("Inclination"),  STR_UNIT_Degrees, DEFAULT,  QColor("dark magenta")));

    schema::channel.add(GRP_CPAP, new Channel(RMS9_MaskOnTime        = 0x1025, DATA,   MT_CPAP,  SESSION, "MaskOnTime",
            QObject::tr("Mask On Time"), QObject::tr("Time started according to str.edf"),  QObject::tr("Mask On Time"),  STR_UNIT_Unknown, DEFAULT,  Qt::black));

    schema::channel.add(GRP_CPAP, new Channel(CPAP_SummaryOnly       = 0x1026, DATA,   MT_CPAP,  SESSION, "SummaryOnly",
            QObject::tr("Summary Only"), QObject::tr("CPAP Session contains summary data only"),  QObject::tr("Summary Only"),  STR_UNIT_Unknown, DEFAULT,  Qt::black));

    Channel *ch;
    schema::channel.add(GRP_CPAP, ch = new Channel(CPAP_Mode         = 0x1200, SETTING,   MT_CPAP,  SESSION, "PAPMode",
                        QObject::tr("PAP Mode"), QObject::tr("PAP Device Mode"), QObject::tr("PAP Mode"),  QString(), LOOKUP,  Qt::black));

    ch->addOption(0, STR_TR_Unknown);
    ch->addOption(1, STR_TR_CPAP);
    ch->addOption(2, QObject::tr("APAP (Variable)"));
    ch->addOption(3, QObject::tr("Fixed Bi-Level"));
    ch->addOption(4, QObject::tr("Auto Bi-Level (Fixed PS)"));
    ch->addOption(5, QObject::tr("Auto Bi-Level (Variable PS)"));
    ch->addOption(6, QObject::tr("ASV (Fixed EPAP)"));
    ch->addOption(7, QObject::tr("ASV (Variable EPAP)"));
    ch->addOption(8, QObject::tr("AVAPS"));


    /////////////////////////////////////////////////////////////////
    // Old Journal system crap
    /////////////////////////////////////////////////////////////////

    schema::channel.add(GRP_JOURNAL, ch = new Channel(Journal_Weight = 0x0803, DATA,   MT_JOURNAL,  DAY, "Weight",      QObject::tr("Weight"), QObject::tr("Weight"), QObject::tr("Weight"),  STR_UNIT_KG, DOUBLE,  Qt::black));
    schema::channel.add(GRP_JOURNAL, ch = new Channel(0x0804, DATA,   MT_JOURNAL,  DAY, "Height", QObject::tr("Height"), QObject::tr("Physical Height"), QObject::tr("Height"),  STR_UNIT_CM, DOUBLE,  Qt::black));
    schema::channel.add(GRP_JOURNAL, ch = new Channel(Bookmark_Notes=0x0805, DATA,   MT_JOURNAL,  DAY, "BookmarkNotes",      QObject::tr("Notes"), QObject::tr("Bookmark Notes"), QObject::tr("Notes"),  QString(), STRING,  Qt::black));
    // This may as well be calculated
    schema::channel.add(GRP_JOURNAL, ch = new Channel(Journal_BMI = 0x0806, DATA,   MT_JOURNAL,  DAY, "BMI",      QObject::tr("BMI"), QObject::tr("Body Mass Index"), QObject::tr("BMI"),  QString(), DOUBLE,  Qt::black));
    schema::channel.add(GRP_JOURNAL, ch = new Channel(Journal_ZombieMeter = 0x0807, DATA,   MT_JOURNAL,  DAY, "ZombieMeter", QObject::tr("Zombie"), QObject::tr("How you feel (0 = like crap, 10 = unstoppable)"), QObject::tr("Zombie"),  QString(), DOUBLE,  Qt::black));
    schema::channel.add(GRP_JOURNAL, ch = new Channel(Bookmark_Start=0x0808, DATA,   MT_JOURNAL,  DAY, "BookmarkStart",      QObject::tr("Start"), QObject::tr("Bookmark Start"), QObject::tr("Start"),  QString(), INTEGER,  Qt::black));
    schema::channel.add(GRP_JOURNAL, ch = new Channel(Bookmark_End=0x0809, DATA,   MT_JOURNAL,  DAY, "BookmarkEnd",      QObject::tr("End"), QObject::tr("Bookmark End"), QObject::tr("End"),  QString(), DOUBLE,  Qt::black));
    schema::channel.add(GRP_JOURNAL, ch = new Channel(LastUpdated=0x080a, DATA,   MT_JOURNAL,  DAY, "LastUpdated", QObject::tr("Last Updated"), QObject::tr("Last Updated"), QObject::tr("Last Updated"),  QString(), DATETIME,  Qt::black));
    schema::channel.add(GRP_JOURNAL, ch = new Channel(Journal_Notes = 0xd000, DATA,   MT_JOURNAL,  DAY, "Journal",      QObject::tr("Journal Notes"), QObject::tr("Journal Notes"), QObject::tr("Journal"),  QString(), RICHTEXT,  Qt::black));


    //////////////////////////////////////////////////////////////////////
    // Sleep Stage Channels
    //////////////////////////////////////////////////////////////////////
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_SleepStage = 0x2000, WAVEFORM,   MT_SLEEPSTAGE,  SESSION, "SleepStage",
            QObject::tr("Sleep Stage"), QObject::tr("1=Awake 2=REM 3=Light Sleep 4=Deep Sleep"), QObject::tr("Sleep Stage"),  QString(), INTEGER,  Qt::darkGray));
    schema::channel.add(GRP_SLEEP, ch = new Channel(0x2001, WAVEFORM,   MT_SLEEPSTAGE,  SESSION, "ZeoBW",
            QObject::tr("Brain Wave"), QObject::tr("Brain Wave"), QObject::tr("BrainWave"),  QString(), INTEGER,  Qt::black));
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_Awakenings = 0x2002, DATA,   MT_SLEEPSTAGE,  SESSION, "Awakenings",  QObject::tr("Awakenings"), QObject::tr("Number of Awakenings"), QObject::tr("Awakenings"),  QString(), INTEGER,  Qt::black));
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_MorningFeel= 0x2003, DATA,   MT_SLEEPSTAGE,  SESSION, "MorningFeel", QObject::tr("Morning Feel"), QObject::tr("How you felt in the morning"), QObject::tr("Morning Feel"),  QString(), INTEGER,  Qt::black));
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_TimeInWake = 0x2004, DATA,   MT_SLEEPSTAGE,  SESSION, "TimeInWake",  QObject::tr("Time Awake"), QObject::tr("Time spent awake"), QObject::tr("Time Awake"),  STR_UNIT_Minutes, INTEGER,  Qt::black));
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_TimeInREM  = 0x2005, DATA,   MT_SLEEPSTAGE,  SESSION, "TimeInREM",    QObject::tr("Time In REM Sleep"), QObject::tr("Time spent in REM Sleep"), QObject::tr("Time in REM Sleep"),  STR_UNIT_Minutes, INTEGER,  Qt::black));
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_TimeInLight= 0x2006, DATA,   MT_SLEEPSTAGE,  SESSION, "TimeInLight",QObject::tr("Time In Light Sleep"), QObject::tr("Time spent in light sleep"), QObject::tr("Time in Light Sleep"),  STR_UNIT_Minutes, INTEGER,  Qt::black));
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_TimeInDeep = 0x2007, DATA,   MT_SLEEPSTAGE,  SESSION, "TimeInDeep",   QObject::tr("Time In Deep Sleep"), QObject::tr("Time spent in deep sleep"), QObject::tr("Time in Deep Sleep"),  STR_UNIT_Minutes, INTEGER,  Qt::black));
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_TimeInDeep = 0x2008, DATA,   MT_SLEEPSTAGE,  SESSION, "TimeToZ",      QObject::tr("Time to Sleep"), QObject::tr("Time taken to get to sleep"), QObject::tr("Time to Sleep"),  STR_UNIT_Minutes, INTEGER,  Qt::black));
    schema::channel.add(GRP_SLEEP, ch = new Channel(ZEO_ZQ         = 0x2009, DATA,   MT_SLEEPSTAGE,  SESSION, "ZeoZQ", QObject::tr("Zeo ZQ"), QObject::tr("Zeo sleep quality measurement"), QObject::tr("ZEO ZQ"),  QString(), INTEGER,  Qt::black));

    NoChannel = 0;
    CPAP_BrokenSummary = schema::channel["BrokenSummary"].id();
    CPAP_BrokenWaveform = schema::channel["BrokenWaveform"].id();

//    <channel id="0x111e" class="data" name="TestChan1" details="Debugging Channel #2" label="Test #1" unit="" color="pink"/>
//    <channel id="0x111f" class="data" name="TestChan2" details="Debugging Channel #2" label="Test #2" unit="" color="blue"/>

    schema::channel.add(GRP_CPAP, ch=new Channel(CPAP_Test1 = 0x111e, DATA, MT_CPAP, SESSION, "TestChan1", QObject::tr("Debugging channel #1"), QObject::tr("Top secret internal stuff you're not supposed to see ;)"), QObject::tr("Test #1"),  QString(), INTEGER,  QColor("pink")));
    schema::channel.add(GRP_CPAP, ch=new Channel(CPAP_Test2 = 0x111f, DATA, MT_CPAP, SESSION, "TestChan2", QObject::tr("Debugging channel #2"), QObject::tr("Top secret internal stuff you're not supposed to see ;)"), QObject::tr("Test #2"),  QString(), INTEGER,  Qt::blue));

    RMS9_E01 = schema::channel["RMS9_E01"].id();
    RMS9_E02 = schema::channel["RMS9_E02"].id();
    RMS9_SetPressure = schema::channel["SetPressure"].id(); // TODO: this isn't needed anymore
    CPAP_HumidSetting = schema::channel["HumidSet"].id();
    INTELLIPAP_Unknown1 = schema::channel["IntUnk1"].id();
    INTELLIPAP_Unknown2 = schema::channel["IntUnk2"].id();

    schema::channel[CPAP_Leak].setShowInOverview(true);
    schema::channel[CPAP_RespRate].setShowInOverview(true);
    schema::channel[CPAP_MinuteVent].setShowInOverview(true);
    schema::channel[CPAP_TidalVolume].setShowInOverview(true);
    schema::channel[CPAP_CSR].setShowInOverview(true);
    schema::channel[CPAP_PB].setShowInOverview(true);
    schema::channel[CPAP_LargeLeak].setShowInOverview(true);
    schema::channel[CPAP_FLG].setShowInOverview(true);
}


void resetChannels()
{
    schema::channel.channels.clear();
    schema::channel.names.clear();
    schema::channel.groups.clear();

    schema_initialized = false;
    init();

    QList<MachineLoader *> list = GetLoaders();
    for (int i=0; i< list.size(); ++i) {
        MachineLoader * loader = list.at(i);
        loader->initChannels();
    }
    setOrders();
}

Channel::Channel(ChannelID id, ChanType type, MachineType machtype, ScopeType scope, QString code, QString fullname,
                 QString description, QString label, QString unit, DataType datatype, QColor color, int link):
    m_id(id),
    m_type(type),
    m_machtype(machtype),
    m_scope(scope),
    m_code(code),
    m_fullname(fullname),
    m_description(description),
    m_label(label),
    m_unit(unit),
    m_datatype(datatype),
    m_defaultcolor(color),
    m_link(link),
    m_upperThreshold(0),
    m_lowerThreshold(0),
    m_upperThresholdColor(Qt::red),
    m_lowerThresholdColor(Qt::green),
    m_enabled(true),
    m_order(255)
{
    if (type == WAVEFORM) {
        calc[Calc_Min] = ChannelCalc(id, Calc_Min, adjustcolor(color, 0.25f, 1.0f, 1.3f), false);
        calc[Calc_Middle] = ChannelCalc(id, Calc_Middle, adjustcolor(color, 1.3f, 1.0f, 1.0f), false);
        calc[Calc_Perc] = ChannelCalc(id, Calc_Perc, adjustcolor(color, 1.1f, 1.2f, 1.0f), false);
        calc[Calc_Max] = ChannelCalc(id, Calc_Max,  adjustcolor(color, 0.5f, 1.2f, 1.0f), false);
        calc[Calc_Zero] = ChannelCalc(id, Calc_Zero, Qt::red, false);
        calc[Calc_LowerThresh] = ChannelCalc(id, Calc_LowerThresh, Qt::blue, false);
        calc[Calc_UpperThresh] = ChannelCalc(id, Calc_UpperThresh, Qt::red, false);
    }
    m_showInOverview = false;

    default_fullname = fullname;
    default_label = label;
    default_description = description;

}
bool Channel::isNull()
{
    return (this == &EmptyChannel);
}

ChannelList::ChannelList()
    : m_doctype("channels")
{
}
ChannelList::~ChannelList()
{
    for (auto & chan : channels) {
        delete chan;
    }
}
bool ChannelList::Load(QString filename)
{
    QDomDocument doc(m_doctype);
    QFile file(filename);
    qDebug() << "Opening " << filename;

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open" << filename;
        return false;
    }

    QString errorMsg;
    int errorLine;

    if (!doc.setContent(&file, false, &errorMsg, &errorLine)) {
        qWarning() << "Invalid XML Content in" << filename;
        qWarning() << "Error line" << errorLine << ":" << errorMsg;
        return false;
    }

    file.close();


    QDomElement root = doc.documentElement();

    if (root.tagName().toLower() != "channels") {
        return false;
    }

    QString language = root.attribute("language", "en");
    QString version = root.attribute("version", "");

    if (version.isEmpty()) {
        qWarning() << "No Version Field in" << m_doctype << "Schema, assuming 1.0" << filename;
        version = "1.0";
    }

    qDebug() << "Processing xml file:" << m_doctype << language << version;
    QDomNodeList grp = root.elementsByTagName("group");
    QDomNode node, n, ch;
    QDomElement e;

    bool ok;
    int id, linkid;
    QString chantype, scopestr, typestr, name, group, idtxt, details, label, unit, datatypestr, defcolor, link;
    ChanType type;
    DataType datatype;
    Channel *chan;
    QColor color;
    //bool multi;
    ScopeType scope;
    int line;

    for (int i = 0; i < grp.size(); i++) {
        node = grp.at(i);
        group = node.toElement().attribute("name");
        //qDebug() << "Group Name" << group;
        // Why do I have to skip the first node here? (shows up empty)
        n = node.firstChildElement();

        while (!n.isNull()) {
            line = n.lineNumber();
            e = n.toElement();

            if (e.nodeName().toLower() != "channel") {
                qWarning() << "Ignoring unrecognized schema type " << e.nodeName() << "in" << filename << "line" <<
                           line;
                continue;
            }

            ch = n.firstChild();
            n = n.nextSibling();

            idtxt = e.attribute("id");
            id = idtxt.toInt(&ok, 16);

            if (!ok) {
                qWarning() << "Dodgy ID number " << e.nodeName() << "in" << filename << "line" << line;
                continue;
            }

            chantype = e.attribute("class", "data").toLower();

            if (!ChanTypes.contains(chantype)) {
                qWarning() << "Dodgy class " << chantype << "in" << filename << "line" << line;
                continue;
            }

            type = ChanTypes[chantype];

            scopestr = e.attribute("scope", "session");

            if (scopestr.at(0) == QChar('!')) {
                scopestr = scopestr.mid(1);
                //multi=true;
            } //multi=false;

            if (!Scopes.contains(scopestr)) {
                qWarning() << "Dodgy Scope " << scopestr << "in" << filename << "line" << line;
                continue;
            }

            scope = Scopes[scopestr];

            name = e.attribute("name", "");
            details = e.attribute("details", "");
            label = e.attribute("label", "");

            if (name.isEmpty() || details.isEmpty() || label.isEmpty()) {
                qWarning() << "Missing name,details or label attribute in" << filename << "line" << line;
                continue;
            }

            unit = e.attribute("unit");

            defcolor = e.attribute("color", "black");
            color = QColor(defcolor);

            if (!color.isValid()) {
                qWarning() << "Invalid Color " << defcolor << "in" << filename << "line" << line;
                color = Qt::black;
            }

            datatypestr = e.attribute("type", "").toLower();

            link = e.attribute("link", "");

            if (!link.isEmpty()) {
                linkid = link.toInt(&ok, 16);

                if (!ok) {
                    qWarning() << "Dodgy Link ID number " << e.nodeName() << "in" << filename << " line" << line;
                }
            } else { linkid = 0; }

            if (DataTypes.contains(datatypestr)) {
                datatype = DataTypes[typestr];
            } else {
                qWarning() << "Ignoring unrecognized schema datatype in" << filename << "line" << line;
                continue;
            }

            if (channels.contains(id)) {
                qWarning() << "Schema already contains id" << id << "in" << filename << "line" << line;
                continue;
            }

            if (names.contains(name)) {
                qWarning() << "Schema already contains name" << name << "in" << filename << "line" << line;
                continue;
            }

            chan = new Channel(id, type, MT_UNKNOWN, scope, name, name, details, label, unit, datatype, color, linkid);
            channels[id] = chan;
            names[name] = chan;
            //qDebug() << "Channel" << id << name << label;
            groups[group][name] = chan;

            if (linkid > 0) {
                if (channels.contains(linkid)) {
                    Channel *it = channels[linkid];
                    it->m_links.push_back(chan);
                    //int i=0;
                } else {
                    qWarning() << "Linked channel" << name << ":" << linkid << "should be defined first in" << filename << "line" << line;
                }
            }

            // process children
            while (!ch.isNull()) {
                e = ch.toElement();
                QString sub = ch.nodeName().toLower();
                QString id2str, name2str;
                int id2;

                if (sub == "option") {
                    id2str = e.attribute("id");
                    id2 = id2str.toInt(&ok, 10);
                    name2str = e.attribute("value");
                    //qDebug() << sub << id2 << name2str;
                    chan->addOption(id2, name2str);
                } else if (sub == "color") {
                }

                ch = ch.nextSibling();
            }
        }
    }

    return true;
}

void ChannelList::add(QString group, Channel *chan)
{
    if (chan == nullptr) {
        qCritical() << "ChannelList::add called with null chan object";
        return;
    }

    if (channels.contains(chan->id())) {
        qCritical() << "Channels already contains id" << chan->id() << chan->code();
        return;
    }

    if (names.contains(chan->code())) {
        qCritical() << "Channels already contains name" << chan->id() << chan->code();
        return;
    }

    channels[chan->id()] = chan;
    names[chan->code()] = chan;
    groups[group][chan->code()] = chan;

    if (channels.contains(chan->linkid())) {
        Channel *it = channels[chan->linkid()];
        it->m_links.push_back(chan);
        //int i=0;
    } else {
        if (chan->linkid()>0) {
            qWarning() << "Linked channel must be defined first for" << chan->code();
        }
    }
}

bool ChannelList::Save(QString filename)
{
    if (filename.isEmpty())
        return false;

    qDebug() << "In ChannelList::Save() saving " << filename;;

    QDomDocument doc("channels");

    QDomElement droot = doc.createElement(STR_AppName);
    doc.appendChild(droot);

    QDomElement root = doc.createElement("channels");
    droot.appendChild(root);


    for (auto git=groups.begin(), end=groups.end(); git != end; ++git) {
        auto & chanlist = git.value();

        QDomElement grp = doc.createElement("group");
        grp.setAttribute("name", git.key());
        root.appendChild(grp);
        for (auto it = chanlist.begin(), cend=chanlist.end(); it!=cend; ++it) {
            Channel * chan = it.value();
            QDomElement cn = doc.createElement("channel");
            cn.setAttribute("id", chan->id());
            cn.setAttribute("code", it.key());
            cn.setAttribute("label", chan->label());
            cn.setAttribute("name", chan->fullname());
            cn.setAttribute("description", chan->description());
            cn.setAttribute("color", chan->defaultColor().name());
            cn.setAttribute("upper", chan->upperThreshold());
            cn.setAttribute("lower", chan->lowerThreshold());
            cn.setAttribute("order", chan->order());
            cn.setAttribute("type", chan->type());
            cn.setAttribute("datatype", chan->datatype());
            cn.setAttribute("overview", chan->showInOverview());
            for (auto op=chan->m_options.begin(), opend=chan->m_options.end(); op!=opend; ++op) {
                QDomElement c2 = doc.createElement("option");
                c2.setAttribute("key", op.key());
                c2.setAttribute("value", op.value());
                cn.appendChild(c2);
            }

            //cn.appendChild(doc.createTextNode(i.value().toDateTime().toString("yyyy-MM-dd HH:mm:ss")));
            grp.appendChild(cn);
        }
    }

    QFile file(filename);

    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QTextStream ts(&file);
    ts << doc.toString();
    file.close();

    return true;
}

} // namespace

QString ChannelCalc::label()
{
    QString lab = schema::channel[code].label();
    QString m_label;
    switch(type) {
    case Calc_Min:
        m_label = QString("%1 %2").arg(STR_TR_Min).arg(lab);
        break;
    case Calc_Middle:
        m_label = Day::calcMiddleLabel(code);
        break;
    case Calc_Perc:
        m_label = Day::calcPercentileLabel(code);
        break;
    case Calc_Max:
        m_label = Day::calcMaxLabel(code);
        break;
    case Calc_Zero:
        m_label = QObject::tr("Zero");
        break;
    case Calc_UpperThresh:
        m_label = QString("%1 %2").arg(lab).arg(QObject::tr("Upper Threshold"));
        break;
    case Calc_LowerThresh:
        m_label = QString("%1 %2").arg(lab).arg(QObject::tr("Lower Threshold"));
        break;
    }
    return m_label;

}

//typedef schema::Channel * ChannelID;
