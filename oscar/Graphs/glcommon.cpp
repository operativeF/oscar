/* glcommon GL code & font stuff
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <cmath>
#include "glcommon.h"

float brightness(QColor color) {
    return color.redF()*0.299 + color.greenF()*0.587 + color.blueF()*0.114;
}


QColor brighten(QColor color, float mult)
{
    int cr, cg, cb;

    cr = color.red();
    cg = color.green();
    cb = color.blue();

    if (cr < 64) { cr = 64; }

    if (cg < 64) { cg = 64; }

    if (cb < 64) { cb = 64; }

    cr *= mult;
    cg *= mult;
    cb *= mult;

    if (cr > 255) { cr = 255; }

    if (cg > 255) { cg = 255; }

    if (cb > 255) { cb = 255; }

    return QColor(cr, cg, cb, 255);

}

#if defined(_MSC_VER) && (_MSC_VER < 1800)
double round(double number)
{
    return number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5);
}
#endif


