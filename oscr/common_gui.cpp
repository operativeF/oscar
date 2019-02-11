/* Common GUI Functions Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */
#include <QColor>
#include "Graphs/glcommon.h"

// Flag Colors
QColor COLOR_Hypopnea       = Qt::blue;
QColor COLOR_Obstructive    = COLOR_Aqua;
QColor COLOR_Apnea          = Qt::darkGreen;
QColor COLOR_CSR            = COLOR_LightGreen;
QColor COLOR_LargeLeak      = COLOR_LightGray;
QColor COLOR_Ramp           = COLOR_LightBlue;
QColor COLOR_ClearAirway    = QColor("#b254cd");
QColor COLOR_RERA           = COLOR_Gold;
QColor COLOR_VibratorySnore = QColor("#ff4040");
QColor COLOR_FlowLimit      = QColor("#404040");
QColor COLOR_SensAwake      = COLOR_Gold;
QColor COLOR_LeakFlag       = QColor("#40c0c0"); // Qt::darkBlue;
QColor COLOR_NRI            = QColor("orange"); //COLOR_ClearAirway;
QColor COLOR_ExP            = Qt::darkCyan;
QColor COLOR_PressurePulse  = Qt::red;
QColor COLOR_PulseChange    = COLOR_LightGray;
QColor COLOR_SPO2Drop       = COLOR_LightBlue;
QColor COLOR_UserFlag1      = QColor("#e0e0e0");
QColor COLOR_UserFlag2      = QColor("#c0c0e0");

// Chart Colors
QColor COLOR_EPAP         = Qt::blue;
QColor COLOR_IPAP         = Qt::red;
QColor COLOR_IPAPLo       = Qt::darkRed;
QColor COLOR_IPAPHi       = Qt::darkRed;
QColor COLOR_Plethy       = Qt::darkBlue;
QColor COLOR_Pulse        = Qt::red;
QColor COLOR_SPO2         = Qt::blue;
QColor COLOR_FlowRate     = Qt::black;
QColor COLOR_Pressure     = Qt::darkGreen;
QColor COLOR_RDI          = COLOR_LightGreen;
QColor COLOR_AHI          = COLOR_LightGreen;
QColor COLOR_Leak         = COLOR_DarkMagenta;
QColor COLOR_LeakTotal    = COLOR_DarkYellow;
QColor COLOR_MaxLeak      = COLOR_DarkRed;
QColor COLOR_Snore        = COLOR_DarkGray;
QColor COLOR_RespRate     = COLOR_DarkBlue;
QColor COLOR_MaskPressure = COLOR_Blue;
QColor COLOR_PTB          = COLOR_Gray;       // Patient-Triggered Breathing
QColor COLOR_MinuteVent   = COLOR_Cyan;
QColor COLOR_TgMV         = COLOR_DarkCyan;
QColor COLOR_TidalVolume  = COLOR_Magenta;
QColor COLOR_FLG          = COLOR_DarkBlue;   // Flow Limitation Graph
QColor COLOR_IE           = COLOR_DarkRed;    // Inspiratory Expiratory Ratio
QColor COLOR_Te           = COLOR_DarkGreen;
QColor COLOR_Ti           = COLOR_DarkBlue;
QColor COLOR_SleepStage   = COLOR_Gray;

//#include <windows.h>
//#include <tchar.h>

//typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

//LPFN_ISWOW64PROCESS fnIsWow64Process;

//BOOL IsWow64()
//{
//    BOOL bIsWow64 = FALSE;

//    //IsWow64Process is not available on all supported versions of Windows.
//    //Use GetModuleHandle to get a handle to the DLL that contains the function
//    //and GetProcAddress to get a pointer to the function if available.

//    fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(
//        GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

//    if(NULL != fnIsWow64Process)
//    {
//        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
//        {
//            //handle error
//        }
//    }
//    return bIsWow64;
//}

