/* gXAxis Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDebug>
#include <QFontMetrics>

#include <math.h>

#include "Graphs/gXAxis.h"
#include "SleepLib/profiles.h"
#include "Graphs/glcommon.h"
#include "Graphs/gGraph.h"
#include "Graphs/gGraphView.h"

// These divisors are used to round xaxis timestamps to reasonable increments
const quint64 divisors[] = {
    15552000000ULL, 7776000000ULL, 5184000000ULL, 2419200000ULL, 1814400000ULL, 1209600000L, 604800000L, 259200000L,
    172800000L, 86400000, 2880000, 14400000, 7200000, 3600000, 2700000,
    1800000, 1200000, 900000, 600000, 300000, 120000, 60000, 45000, 30000,
    20000, 15000, 10000, 5000, 2000, 1000, 500, 100, 50, 10, 1
};
const int divcnt = sizeof(divisors) / sizeof(quint64);

gXAxis::gXAxis(QColor col, bool fadeout)
    : Layer(NoChannel)
{
    m_line_color = col;
    m_text_color = col;
    m_major_color = Qt::darkGray;
    m_minor_color = Qt::lightGray;
    m_show_major_lines = false;
    m_show_minor_lines = false;
    m_show_minor_ticks = true;
    m_show_major_ticks = true;
    m_utcfix = false;
    m_fadeout = fadeout;

    tz_offset = timezoneOffset();
    tz_hours = tz_offset / 3600000.0;

    m_roundDays = false;
}
gXAxis::~gXAxis()
{
}

int gXAxis::minimumHeight()
{
    QFontMetrics fm(*defaultfont);
    int h = fm.height();
#if defined(Q_OS_MAC)
    return 9+h;
#else
    return 11+h;
#endif
}

const QString months[] = {
    QObject::tr("Jan"), QObject::tr("Feb"), QObject::tr("Mar"), QObject::tr("Apr"), QObject::tr("May"), QObject::tr("Jun"),
    QObject::tr("Jul"), QObject::tr("Aug"), QObject::tr("Sep"), QObject::tr("Oct"), QObject::tr("Nov"), QObject::tr("Dec")
};
//static QString dow[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};


void gXAxis::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    float left = region.boundingRect().left();
    float top = region.boundingRect().top()-0.999f;
    float width = region.boundingRect().width();
    float height = region.boundingRect().height();


    QVector<QLineF> ticks;


    QPainter painter2; // Only need this for pixmap caching

    // pixmap caching screws font size when printing


    QFontMetrics fm(*defaultfont);

    bool usepixmap = AppSetting->usePixmapCaching(); // Whether or not to use pixmap caching

    if (!usepixmap || (usepixmap && w.invalidate_xAxisImage)) {
        // Redraw graph xaxis labels and ticks either to pixmap or directly to screen

        if (usepixmap) {
            // Initialize a new cache image
            m_image = QImage(width + 22, height + 4, QImage::Format_ARGB32_Premultiplied);
            m_image.fill(Qt::transparent);
            painter2.begin(&m_image);
            painter2.setPen(Qt::black);
            painter2.setFont(*defaultfont);
        }

        double px, py;

        int start_px = left;
        //int start_py=top;
        //int width=scrx-(w.GetLeftMargin()+w.GetRightMargin());
        //    float height=scry-(w.GetTopMargin()+w.GetBottomMargin());

        if (width < 40) {
            return;
        }

        qint64 minx;
        qint64 maxx;

        if (w.blockZoom()) {
            // Lock zoom to entire data range
            minx = w.rmin_x;
            maxx = w.rmax_x;
        } else {
            // Allow zoom
            minx = w.min_x;
            maxx = w.max_x;

        }

        int days = ceil(double(maxx-minx) / 86400000.0);

        if (m_roundDays) {
            minx = floor(double(minx)/86400000.0);
            minx *= 86400000L;

            maxx = minx + 86400000L * qint64(days);
        }

        // duration of graph display window in milliseconds.
        qint64 xx = maxx - minx;

        // shouldn't really be negative, but this is safer than an assert
        if (xx <= 0) {
            return;
        }

        //Most of this could be precalculated when min/max is set..
        QString fd, tmpstr;
        int divmax, dividx;
        int fitmode;

        // Have a quick look at the scale and prep the autoscaler a little faster
        if (xx >= 86400000L) {       // Day
            fd = "Mjj 00";
            dividx = 0;
            divmax = 10;
            fitmode = 0;
        } else if (xx > 1800000) {  // Minutes
            fd = " j0:00";
            dividx = 10;
            divmax = 21;
            fitmode = 1;
        } else if (xx > 5000) {    // Seconds
            fd = " j0:00:00";
            dividx = 16;
            divmax = 29;
            fitmode = 2;
        } else {                   // Microseconds
            fd = "j0:00:00:000";
            dividx = 28;
            divmax = divcnt;
            fitmode = 3;
        }

        //if (divmax>divcnt) divmax=divcnt;

        int x, y;
        // grab the text extent of the dummy text fields above to know how much space is needed
        QRect r2 = fm.boundingRect(fd);
        x = r2.width();
        y = r2.height();

        // Not sure when this was a problem...
        if (x<=0) {
            qWarning() << "gXAxis::Paint called with x<=0";
            return;
        }

        // Max number of ticks that will fit, with a bit of room for a buffer
        int max_ticks = width / (x + 15);

        int fit_ticks = 0;
        int div = -1;
        qint64 closest = 0, tmp, tmpft;

        // Scan through divisor list with the index range given above, to find which
        // gives the closest number of ticks to the maximum that will physically fit

        for (int i = dividx; i < divmax; i++) {
            tmpft = xx / divisors[i];
            tmp = max_ticks - tmpft;

            if (tmp < 0) { continue; }

            if (tmpft > closest) {   // Find the closest scale to the number
                closest = tmpft;     // that will fit
                div = i;
                fit_ticks = tmpft;
            }
        }

        if (fit_ticks == 0) {
            qDebug() << "gXAxis::Plot() Couldn't fit ticks.. Too short?" << minx << maxx << xx;
            return;
        }

        if ((div < 0) || (div > divcnt)) {
            qDebug() << "gXAxis::Plot() div out of bounds";
            return;
        }

        qint64 step = divisors[div];

        //Align left minimum to divisor by losing precision
        qint64 aligned_start = minx / step;
        aligned_start *= step;

        while (aligned_start < minx) {
            aligned_start += step;
        }

        painter.setPen(QColor(Qt::black));


        //int utcoff=m_utcfix ? tz_hours : 0;

        //utcoff=0;
        int num_minor_ticks;

        if (step >= 86400000) {
            qint64 i = step / 86400000L; // number of days

            if (i > 14) { i /= 2; }

            if (i < 0) { i = 1; }

            num_minor_ticks = i;
        } else { num_minor_ticks = 10; }

        float xmult = double(width) / double(xx);
        float step_pixels = double(step / float(num_minor_ticks)) * xmult;

        py = left + float(aligned_start - minx) * xmult;

        //py+=usepixmap ? 20 : left;

        int mintop = top + 3.0 * (float(y) / 10.0);
        int majtop = top + 6.0 * (float(y) / 10.0);
        int texttop = majtop + y; // 18*w.printScaleY();
#if defined (Q_OS_MAC)
           texttop += 2;
#endif

        // Fill in the minor tick marks up to the first major alignment tick

        for (int i = 0; i < num_minor_ticks; i++) {
            py -= step_pixels;

            if (py < start_px) { continue; }

            if (usepixmap) {
                ticks.append(QLineF(py - left + 20, 0, py - left + 20, mintop - top));
            } else {
                ticks.append(QLineF(py, top+2, py, mintop+2));
            }
        }

        int ms, m, h, s, d;
        qint64 j;

        for (qint64 i = aligned_start; i < maxx; i += step) {
            px = (i - minx) * xmult;
            px += left;

            if (usepixmap) {
                ticks.append(QLineF(px - left + 20, 0, px - left + 20, majtop - top));
            } else {
                ticks.append(QLineF(px, top+2, px, majtop+2));
            }

            j = i;

            if (!m_utcfix) { j += tz_offset; }

            ms = j % 1000;
            s = (j / 1000L) % 60L;
            m = (j / 60000L) % 60L;
            h = (j / 3600000L) % 24L;
            //int d=(j/86400000) % 7;

            if (fitmode == 0) {
                d = (j / 1000);
                QDateTime dt = QDateTime::fromTime_t(d).toUTC();
                QDate date = dt.date();
                // SLOW SLOW SLOW!!! On Mac especially, this function is pathetically slow.
                //dt.toString("MMM dd");

                // Doing it this way instead because it's MUUUUUUCH faster
                tmpstr = QString("%1 %2").arg(months[date.month() - 1]).arg(date.day());
                //} else if (fitmode==0) {
                //            tmpstr=QString("%1 %2:%3").arg(dow[d]).arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0'));
            } else if (fitmode == 1) { // minute
                tmpstr = QString("%1:%2").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'));
            } else if (fitmode == 2) { // second
                tmpstr = QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
            } else if (fitmode == 3) { // milli
                tmpstr = QString("%1:%2:%3:%4").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0')).arg(ms, 3, 10, QChar('0'));
            }

            int tx = px - x / 2.0;

            if (m_utcfix) {
                tx += step_pixels / 2.0;
            }

            if ((tx + x) < (left + width)) {
                if (!usepixmap) { w.renderText(tmpstr, tx, texttop, 0, Qt::black, defaultfont); }
                else { painter2.drawText(tx - left + 20, texttop - top, tmpstr); }
            }

            py = px;

            for (int j = 1; j < num_minor_ticks; j++) {
                py += step_pixels;

                if (py >= left + width) { break; }

                if (usepixmap) {
                    ticks.append(QLineF(py - left + 20, 0, py - left + 20, mintop - top));
                } else {
                    ticks.append(QLineF(py, top+2, py, mintop+2));
                }
            }
        }

        if (usepixmap) {
            painter2.drawLines(ticks);
            painter2.end();
        } else {
            painter.drawLines(ticks);
        }
        w.graphView()->lines_drawn_this_frame += ticks.size();

        w.invalidate_xAxisImage = false;
    }

    if (usepixmap && !m_image.isNull()) {
        painter.drawImage(QPoint(left - 20, top + height - m_image.height() + 5), m_image);
    }
}


gXAxisDay::gXAxisDay(QColor col)
    :Layer(NoChannel)
{
    m_line_color = col;
    m_text_color = col;
    m_major_color = Qt::darkGray;
    m_minor_color = Qt::lightGray;
    m_show_major_lines = false;
    m_show_minor_lines = false;
    m_show_minor_ticks = true;
    m_show_major_ticks = true;
}
gXAxisDay::~gXAxisDay()
{
}

int gXAxisDay::minimumHeight()
{
    QFontMetrics fm(*defaultfont);
    int h = fm.height();
#if defined(Q_OS_MAC)
    return 9+h;
#else
    return 11+h;
#endif
}

void gXAxisDay::paint(QPainter &painter, gGraph &graph, const QRegion &region)
{
    float left = region.boundingRect().left();
    float top = region.boundingRect().top();
    float width = region.boundingRect().width();
    //float height = region.boundingRect().height();

    QString months[] = {
        QObject::tr("Jan"), QObject::tr("Feb"), QObject::tr("Mar"), QObject::tr("Apr"), QObject::tr("May"), QObject::tr("Jun"),
        QObject::tr("Jul"), QObject::tr("Aug"), QObject::tr("Sep"), QObject::tr("Oct"), QObject::tr("Nov"), QObject::tr("Dec")
    };
    qint64 minx;
    qint64 maxx;

    minx = graph.min_x;
    maxx = graph.max_x;

    QDateTime date2 = QDateTime::fromMSecsSinceEpoch(minx, Qt::UTC);
 //   QDateTime enddate2 = QDateTime::fromMSecsSinceEpoch(maxx, Qt::UTC);

    //qInfo() << "Drawing date axis from " << date2 << " to " << enddate2;

    QDate date = date2.date();
//    QDate enddate = enddate2.date();

    int days = ceil(double(maxx - minx) / 86400000.0);

    float barw = width / float(days);

    qint64 xx = maxx - minx;

    // shouldn't really be negative, but this is safer than an assert
    if (xx <= 0) {
        return;
    }


    float lastx = left;
    float y1 = top;

    QString fd = "Mjj 00";
    int x,y;
    GetTextExtent(fd, x, y);
    float xpos = (barw / 2.0) - (float(x) / 2.0);

    float lastxpos = 0;
    QVector<QLineF> lines;
    for (int i=0; i < days; i++) {
        if ((lastx + barw) > (left + width + 1))
            break;

        QString tmpstr = QString("%1 %2").arg(months[date.month() - 1]).arg(date.day(), 2, 10, QChar('0'));

        float x1 = lastx + xpos;
        //lines.append(QLine(lastx, top, lastx, top+6));
        if (x1 > (lastxpos + x + 8*graph.printScaleX())) {
            graph.renderText(tmpstr, x1, y1 + y + 8);
            lastxpos = x1;
            lines.append(QLineF(lastx+barw/2, top, lastx+barw/2, top+6));
        }
        lastx = lastx + barw;
        date = date.addDays(1);
    }
    painter.setPen(QPen(Qt::black,1));
    painter.drawLines(lines);

}


gXAxisPressure::gXAxisPressure(QColor col)
    :Layer(NoChannel)
{
    m_line_color = col;
    m_text_color = col;
    m_major_color = Qt::darkGray;
    m_minor_color = Qt::lightGray;
    m_show_major_lines = false;
    m_show_minor_lines = false;
    m_show_minor_ticks = true;
    m_show_major_ticks = true;
}
gXAxisPressure::~gXAxisPressure()
{
}

int gXAxisPressure::minimumHeight()
{
    QFontMetrics fm(*defaultfont);
    int h = fm.height();
#if defined(Q_OS_MAC)
    return 9+h;
#else
    return 11+h;
#endif
}

void gXAxisPressure::paint(QPainter & /*painter*/, gGraph &/*graph*/, const QRegion &/*region*/)
{
}
