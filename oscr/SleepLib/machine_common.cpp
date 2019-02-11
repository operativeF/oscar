/* SleepLib Common Machine Stuff
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#include "machine_common.h"


ChannelID NoChannel, SESSION_ENABLED, CPAP_SummaryOnly;
ChannelID CPAP_IPAP, CPAP_IPAPLo, CPAP_IPAPHi, CPAP_EPAP, CPAP_EPAPLo, CPAP_EPAPHi, CPAP_Pressure,
          CPAP_PS, CPAP_Mode, CPAP_AHI,
          CPAP_PressureMin, CPAP_PressureMax, CPAP_Ramp, CPAP_RampTime, CPAP_RampPressure, CPAP_Obstructive,
          CPAP_Hypopnea,
          CPAP_ClearAirway, CPAP_Apnea, CPAP_PB, CPAP_CSR, CPAP_LeakFlag, CPAP_ExP, CPAP_NRI, CPAP_VSnore,
          CPAP_VSnore2,
          CPAP_RERA, CPAP_PressurePulse, CPAP_FlowLimit, CPAP_SensAwake, CPAP_FlowRate, CPAP_MaskPressure,
          CPAP_MaskPressureHi,
          CPAP_RespEvent, CPAP_Snore, CPAP_MinuteVent, CPAP_RespRate, CPAP_TidalVolume, CPAP_PTB, CPAP_Leak,
          CPAP_LeakMedian, CPAP_LeakTotal, CPAP_MaxLeak, CPAP_FLG, CPAP_IE, CPAP_Te, CPAP_Ti, CPAP_TgMV,
          CPAP_UserFlag1, CPAP_UserFlag2, CPAP_UserFlag3, CPAP_BrokenSummary, CPAP_BrokenWaveform, CPAP_RDI,
          CPAP_PresReliefMode, CPAP_PresReliefLevel, CPAP_PSMin, CPAP_PSMax, CPAP_Test1,
          CPAP_Test2, CPAP_HumidSetting;


ChannelID RMS9_E01, RMS9_E02, RMS9_SetPressure, RMS9_MaskOnTime;
ChannelID INTELLIPAP_Unknown1, INTELLIPAP_Unknown2;

ChannelID PRS1_00, PRS1_01, PRS1_08, PRS1_0A, PRS1_0B, PRS1_0C, PRS1_0E, PRS1_0F, CPAP_LargeLeak, PRS1_12, PRS1_13, PRS1_15,
          PRS1_BND, PRS1_FlexMode, PRS1_FlexLevel, PRS1_HumidStatus, PRS1_HumidLevel, PRS1_SysLock,
          PRS1_SysOneResistStat,
          PRS1_SysOneResistSet, PRS1_HoseDiam, PRS1_AutoOn, PRS1_AutoOff, PRS1_MaskAlert, PRS1_ShowAHI;

ChannelID OXI_Pulse, OXI_SPO2, OXI_Perf, OXI_PulseChange, OXI_SPO2Drop, OXI_Plethy;

ChannelID Journal_Notes, Journal_Weight, Journal_BMI, Journal_ZombieMeter, LastUpdated,
        Bookmark_Start, Bookmark_End, Bookmark_Notes;


ChannelID ZEO_SleepStage, ZEO_ZQ, ZEO_TotalZ, ZEO_TimeToZ, ZEO_TimeInWake, ZEO_TimeInREM,
          ZEO_TimeInLight, ZEO_TimeInDeep, ZEO_Awakenings,
          ZEO_AlarmReason, ZEO_SnoozeTime, ZEO_WakeTone, ZEO_WakeWindow, ZEO_AlarmType, ZEO_MorningFeel,
          ZEO_FirmwareVersion,
          ZEO_FirstAlarmRing, ZEO_LastAlarmRing, ZEO_FirstSnoozeTime, ZEO_LastSnoozeTime, ZEO_SetAlarmTime,
          ZEO_RiseTime;

ChannelID POS_Orientation, POS_Inclination;
