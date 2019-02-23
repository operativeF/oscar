/* gLineOverlayBar Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <math.h>
#include "SleepLib/profiles.h"
#include "gLineOverlay.h"

gLineOverlayBar::gLineOverlayBar(ChannelID code, QColor color, QString label, FlagType flt)
    : Layer(code), m_flag_color(color), m_label(label), m_flt(flt), m_odt(ODT_TopAndBottom)
{
    m_hover = false;
    m_blockhover = false;
}
gLineOverlayBar::~gLineOverlayBar()
{
}

QColor brighten(QColor, float);

void gLineOverlayBar::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    m_hover = false;
    if (!schema::channel[m_code].enabled())
        return;


    int left = region.boundingRect().left();
    int topp = region.boundingRect().top(); // FIXME: Misspelling intentional.
    double width = region.boundingRect().width();
    int height = region.boundingRect().height();

    if (!m_visible) { return; }

    if (!m_day) { return; }

    int start_py = topp;

    double xx = w.max_x - w.min_x;
    //double yy = w.max_y - w.min_y;
    double jj = width / xx;


    if (xx <= 0) { return; }

    double x1, x2;

    int x, y;

    float bottom = start_py + height - 25 * w.printScaleY(), top = start_py + 25 * w.printScaleY();

    qint64 X;
    qint64 Y;

    QPoint mouse=w.graphView()->currentMousePos();

    m_count = 0;
    m_sum = 0;
    m_flag_color = schema::channel[m_code].defaultColor();


    if (m_flt == FT_Span) {
        m_flag_color.setAlpha(128);
    }

    painter.setPen(m_flag_color);

    EventStoreType raw;

    quint32 *tptr;
    EventStoreType *dptr, *eptr;
    qint64 stime;

    OverlayDisplayType odt = m_odt;
    QHash<ChannelID, QVector<EventList *> >::iterator cei;
    int count;

    qint64 clockdrift = qint64(p_profile->cpap->clockDrift()) * 1000L;
    qint64 drift = 0;
    //bool hover = false;

    int tooltipTimeout = AppSetting->tooltipTimeout();

    // For each session, process it's eventlist
    for (const auto sess : m_day->sessions) {
        if (!sess->enabled()) { continue; }

        cei = sess->eventlist.find(m_code);

        if (cei == sess->eventlist.end()) { continue; }

        if (cei.value().size() == 0) { continue; }

        drift = (sess->type() == MT_CPAP) ? clockdrift : 0;

        // Could loop through here, but nowhere uses more than one yet..
        for (const auto & el : cei.value()) {
            count = el->count();
            stime = el->first() + drift;
            dptr = el->rawData();
            eptr = dptr + count;
            tptr = el->rawTime();

            ////////////////////////////////////////////////////////////////////////////
            // Skip data previous to minx bounds
            ////////////////////////////////////////////////////////////////////////////

            for (; dptr < eptr; ++dptr) {

                if ((stime + *tptr) >= w.min_x) {
                    break;
                }

                ++tptr;
            }

            if (m_flt == FT_Span) {
                ////////////////////////////////////////////////////////////////////////////
                // FT_Span
                ////////////////////////////////////////////////////////////////////////////
                QBrush brush(m_flag_color);
                for (; dptr < eptr; dptr++) {

                    X = stime + *tptr++;
                    raw = *dptr;
                    Y = X - (qint64(raw) * 1000.0L); // duration

                    if (Y > w.max_x) {
                        break;
                    }
                    m_sum += raw;
                    ++m_count;

                    x1 = jj * double(X - w.min_x);
                    x2 = jj * double(Y - w.min_x);

                    x2 += (int(x1)==int(x2)) ? 1 : 0;

                    x2 = qMax(0.0, x2)+left;
                    x1 = qMin(width, x1)+left;

                    painter.fillRect(QRect(x2, start_py, x1-x2, height), brush);
                }
            }/* else if (m_flt == FT_Dot) {
                ////////////////////////////////////////////////////////////////////////////
                // FT_Dot
                ////////////////////////////////////////////////////////////////////////////
                for (; dptr < eptr; dptr++) {
                    hover = false;

                    X = stime + *tptr++; //el.time(i);
                    raw = *dptr; //el.data(i);

                    if (X > w.max_x) {
                        break;
                    }

                    x1 = jj * double(X - w.min_x) + left;
                    m_count++;
                    m_sum += raw;

//                    if ((odt == ODT_Bars) || (xx < 3600000)) {
                        // show the fat dots in the middle
                        painter.setPen(QPen(m_flag_color,4));

                        painter.drawPoint(x1, double(height) / double(yy)*double(-20 - w.min_y) + topp);
                        painter.setPen(QPen(m_flag_color,1));

//                    } else {
//                        // thin lines down the bottom
//                        painter.drawLine(x1, start_py + 1, x1, start_py + 1 + 12);
//                    }
                }
            } */else if ((m_flt == FT_Bar) || (m_flt == FT_Dot)) {
                ////////////////////////////////////////////////////////////////////////////
                // FT_Bar
                ////////////////////////////////////////////////////////////////////////////
                QColor col = m_flag_color;

                QString lab = QString("%1").arg(m_label);
                GetTextExtent(lab, x, y);

                //int lx,ly;

                for (; dptr < eptr; dptr++) {
                   // hover = false;
                    X = stime + *tptr++;
                    raw = *dptr;

                    if (X > w.max_x) {
                        break;
                    }

                    x1 = jj * double(X - w.min_x) + left;
                    m_count++;
                    m_sum += raw;
                    int z = start_py + height;

                    double d1 = jj * double(raw) * 1000.0;


                    if ((m_flt == FT_Bar) && (odt == ODT_Bars)) {
                        QRect rect(x1-d1, top, d1+4, height);

                        painter.setPen(QPen(col,4));
                        painter.drawPoint(x1, top);

                        if (!w.selectingArea() && !m_blockhover && rect.contains(mouse) && !m_hover) {
                            m_hover = true;

                            QColor col2(230,230,230,128);
                            QRect rect((x1-d1), start_py+2, d1, height-2);
                            if (rect.x() < left) {
                                rect.setX(left);
                            }

                            painter.fillRect(rect, QBrush(col2));
                            painter.setPen(col);
                            painter.drawRect(rect);

                            // Queue tooltip
                            QString lab2 = QString("%1 (%2)").arg(schema::channel[m_code].fullname()).arg(raw);
                            w.ToolTip(lab2, x1 - 10, start_py + 24 + (3 * w.printScaleY()), TT_AlignRight, AppSetting->tooltipTimeout());

                            painter.setPen(QPen(col,3));
                        } else {
                            painter.setPen(QPen(col,1));
                        }
                        painter.drawLine(x1, top, x1, bottom);
                        if (xx < (3600000)) {
                            w.renderText(lab, x1 - (x / 2), top - y + (5 * w.printScaleY()),0);
                        }


                    } else {
                        //////////////////////////////////////////////////////////////////////////////////////
                        // Top and bottom markers
                        //////////////////////////////////////////////////////////////////////////////////////
                        //bool b = false;
                        if (!w.selectingArea() && !m_blockhover && QRect(x1-2, topp, 6, height).contains(mouse) && !m_hover) {
                            // only want to draw the highlight/label once per frame
                            m_hover = true;

                            // Draw text label
                            QString lab = QString("%1 (%2)").arg(schema::channel[m_code].fullname()).arg(raw);
                            GetTextExtent(lab, x, y, defaultfont);

                            w.ToolTip(lab, x1 - 10, start_py + 24 + (3 * w.printScaleY()), TT_AlignRight, tooltipTimeout);

                            QColor col = m_flag_color;
                            col.setAlpha(60);
                            painter.setPen(QPen(col, 4));

                            painter.drawLine(x1, start_py+14, x1, z - 12);
                            painter.setPen(QPen(m_flag_color,4));

                            painter.drawLine(x1, z, x1, z - 14);
                            painter.drawLine(x1, start_py+2, x1, start_py + 16);
                        } else {
                            QColor col = m_flag_color;
                            col.setAlpha(10);
                            painter.setPen(QPen(col,1));
                            painter.drawLine(x1, start_py+14, x1, z);
                            painter.setPen(QPen(m_flag_color,1));
                            painter.drawLine(x1, start_py+2, x1, start_py + 14);
                        }
                    }
                }
            }
        }
    }
}
bool gLineOverlayBar::mouseMoveEvent(QMouseEvent *event, gGraph *graph)
{
    Q_UNUSED(event)
    Q_UNUSED(graph)
    return false;
}



gLineOverlaySummary::gLineOverlaySummary(QString text, int x, int y)
    : Layer(CPAP_Obstructive), m_text(text), m_x(x), m_y(y) // The Layer code is a dummy here.
{
}

gLineOverlaySummary::~gLineOverlaySummary()
{
}

void gLineOverlaySummary::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    Q_UNUSED(painter)

    int left = region.boundingRect().left();
    int top = region.boundingRect().top();
    int width = region.boundingRect().width();
    int height = region.boundingRect().height();

    if (!m_visible) { return; }

    if (!m_day) { return; }


    Q_UNUSED(width);
    Q_UNUSED(height);
    float cnt = 0;
    double sum = 0;
    bool isSpan = false;

    for (const auto & lobar : m_overlays) {
        cnt += lobar->count();
        sum += lobar->sum();

        if (lobar->flagtype() == FT_Span) { isSpan = true; }
    }

    double val, first, last;
    double time = 0;

    // Calculate the session time.
    for (const auto & sess : m_day->sessions) {
        if (!sess->enabled()) { continue; }

        first = sess->first();
        last = sess->last();

        if (last < w.min_x) { continue; }

        if (first > w.max_x) { continue; }

        if (first < w.min_x) {
            first = w.min_x;
        }

        if (last > w.max_x) {
            last = w.max_x;
        }

        time += last - first;
    }

    val = 0;

    time /= 1000;
    int h = time / 3600;
    int m = int(time / 60) % 60;
    int s = int(time) % 60;


    time /= 3600;

    //if (time<1) time=1;

    if (time > 0) { val = cnt / time; }

    QString a;

    if (0) { //(w.graphView()->selectionInProgress())) { // || w.graphView()->metaSelect()) && (!w.selDurString().isEmpty())) {
        a = QObject::tr("Duration")+": "+w.selDurString();
    } else {
        a = QObject::tr("Events") + ": " + QString::number(cnt) + ", " +
            QObject::tr("Duration") + " " + QString().sprintf("%02i:%02i:%02i", h, m, s) + ", " +
            m_text + ": " + QString::number(val, 'f', 2);
    }
    if (isSpan) {
        float sph;

        if (!time) { sph = 0; }
        else {
            sph = (100.0 / float(time)) * (sum / 3600.0);

            if (sph > 100) { sph = 100; }
        }

        // eg: %num of time in a span, like Periodic Breathing
        a += " " + QObject::tr("(% %1 in events)").arg(sph, 0, 'f', 2);
    }

    w.renderText(a, left + m_x, top + m_y+2);
}
