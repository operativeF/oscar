/* gStatsLine Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "SleepLib/day.h"
#include "gYAxis.h"
#include "gStatsLine.h"

gStatsLine::gStatsLine(ChannelID code, QString label, QColor textcolor)
    : Layer(code), m_label(label), m_textcolor(textcolor)
{
}
void gStatsLine::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    Q_UNUSED(painter)

    int left = region.boundingRect().left();
    int top = region.boundingRect().top();
    int width = region.boundingRect().width();
    int height = region.boundingRect().height();

    if (!m_visible) { return; }

    //if (m_empty) return;
    Q_UNUSED(height);

    int z = (width + gYAxis::Margin) / 5;
    int p = left - gYAxis::Margin;

    top += 4;
    w.renderText(m_label, p, top);

    //w.renderText(m_text,p,top,0,m_textcolor);

    p += z;
    w.renderText(st_min, p, top);

    p += z;
    w.renderText(st_avg, p, top);

    p += z;
    w.renderText(st_p90, p, top);

    p += z;
    w.renderText(st_max, p, top);

}

void gStatsLine::SetDay(Day *d)
{
    Layer::SetDay(d);

    if (!m_day) { return; }

    m_min = d->Min(m_code);
    m_max = d->Max(m_code);
    m_avg = d->wavg(m_code);
    m_p90 = d->p90(m_code);

    st_min = "Min=" + QString::number(m_min, 'f', 2);
    st_max = "Max=" + QString::number(m_max, 'f', 2);
    st_avg = "Avg=" + QString::number(m_avg, 'f', 2);
    st_p90 = "90%=" + QString::number(m_p90, 'f', 2);

}
