/* glcommon GL code & font stuff Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GLCOMMON_H
#define GLCOMMON_H

#include <QColor>

#ifndef nullptr
#define nullptr NULL
#endif

//! \brief Returns the grayscale brightness (between 0 and 1) of a color
float brightness(QColor color);

#define MIN(a,b) (((a)<(b)) ? (a) : (b));
#define MAX(a,b) (((a)<(b)) ? (b) : (a));

const QColor COLOR_Black = Qt::black;
const QColor COLOR_LightGreen = QColor("light green");
const QColor COLOR_DarkGreen = Qt::darkGreen;
const QColor COLOR_Purple = QColor("purple");
const QColor COLOR_Aqua = QColor("#40c0ff");
const QColor COLOR_Magenta = Qt::magenta;
const QColor COLOR_Blue = Qt::blue;
const QColor COLOR_LightBlue = QColor("light blue");
const QColor COLOR_Gray = Qt::gray;
const QColor COLOR_LightGray = Qt::lightGray;
const QColor COLOR_DarkGray = Qt::darkGray;
const QColor COLOR_Cyan = Qt::cyan;
const QColor COLOR_DarkCyan = Qt::darkCyan;
const QColor COLOR_DarkBlue = Qt::darkBlue;
const QColor COLOR_DarkMagenta = Qt::darkMagenta;
const QColor COLOR_Gold = QColor("gold");
const QColor COLOR_White = Qt::white;
const QColor COLOR_Red = Qt::red;
const QColor COLOR_Pink = QColor("pink");
const QColor COLOR_DarkRed = Qt::darkRed;
const QColor COLOR_Yellow = Qt::yellow;
const QColor COLOR_DarkYellow = Qt::darkYellow;
const QColor COLOR_Orange = QColor("orange");
const QColor COLOR_Green = Qt::green;
const QColor COLOR_Brown = QColor("brown");

const QColor COLOR_Text = Qt::black;
const QColor COLOR_Outline = Qt::black;

const QColor COLOR_ALT_BG1 = QColor(0xc8, 0xff, 0xc8, 0x7f); // Alternating Background Color 1 (Event Flags)
const QColor COLOR_ALT_BG2 = COLOR_White;                    // Alternating Background Color 2 (Event Flags)


QColor brighten(QColor color, float mult = 2.0);

const int max_history = 50;

#ifndef M_PI
const double M_PI = 3.141592653589793;
#endif

// Visual C++ earlier than 2013 doesn't have round in it's maths header..
#if defined(_MSC_VER) && (_MSC_VER < 1800)
double round(double number);
#endif

#endif // GLCOMMON_H
