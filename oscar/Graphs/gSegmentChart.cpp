/* gSegmentChart Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <cmath>
#include "gSegmentChart.h"

gSegmentChart::gSegmentChart(GraphSegmentType type, QColor gradient_color, QColor outline_color)
    : Layer(NoChannel), m_graph_type(type), m_gradient_color(gradient_color),
      m_outline_color(outline_color)
{
    m_empty = true;
}
gSegmentChart::~gSegmentChart()
{
}
void gSegmentChart::AddSlice(ChannelID code, QColor color, QString name)
{
    m_codes.push_back(code);
    m_values.push_back(0);
    m_colors.push_back(color);
    m_names.push_back(name);
    m_total = 0;
}
void gSegmentChart::SetDay(Day *d)
{
    Layer::SetDay(d);
    m_total = 0;

    if (!m_day) { return; }


    for (int c = 0; c < m_codes.size(); c++) {
        auto & mval = m_values[c];
        auto & mcode = m_codes[c];

        mval = 0;

        for (const auto & sess : m_day->sessions) {
            if (sess->enabled() && sess->m_cnt.contains(mcode)) {
                EventDataType cnt = sess->count(mcode);
                mval += cnt;
                m_total += cnt;
            }
        }
    }

    m_empty = true;

    for (const auto & mc : m_codes) {
        if (m_day->count(mc) > 0) {
            m_empty = false;
            break;
        }
    }

}
bool gSegmentChart::isEmpty()
{
    return m_empty;
}

void gSegmentChart::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    QRect rect = region.boundingRect();
    int height = qMin(rect.height(), rect.width());
    int width = qMin(rect.height(), rect.width());

    int left = rect.left();
    int top = rect.top();

    if (rect.width() > rect.height()) {
        left = rect.left() + (rect.width() - rect.height());
    }
    //left --;
    if (!m_visible) { return; }

    if (!m_day) { return; }

    int start_px = left;
    int start_py = top;

    width--;
    float diameter = MIN(width, height);
    diameter -= 8;
    float radius = diameter / 2.0;

    float xmult = float(width) / float(m_total);
    float ymult = float(height) / float(m_total);

    float xp = left;

    int xoffset = width / 2;
    int yoffset = height / 2;

    if (m_total == 0) {
        QColor col = Qt::green;
        QString a = ":-)";
        int x, y;
        GetTextExtent(a, x, y, bigfont);

        w.renderText(a, start_px + xoffset - x / 2, (start_py + yoffset + y / 2), 0, col, bigfont);
        return;
    }

    EventDataType data;
    unsigned size = m_values.size();
    float line_step = float(width) / float(size - 1);
    bool line_first = true;
    int line_last;

    float sum = -90.0;

    painter.setFont(*defaultfont);
    for (unsigned m = 0; m < size; m++) {
        data = m_values[m];

        if (data == 0) { continue; }

        /////////////////////////////////////////////////////////////////////////////////////
        // Pie Chart
        /////////////////////////////////////////////////////////////////////////////////////
        if (m_graph_type == GST_Pie) {
            const QColor col = schema::channel[m_codes[m]].defaultColor();

            // length of this segment in degrees
            float len = 360.0 / m_total * data;

            // Setup the shiny radial gradient

            painter.setRenderHint(QPainter::Antialiasing);
            QRect pierect(start_px+1, start_py+1, width-2, height-2);

            painter.setPen(QPen(col, 0));
            QRadialGradient gradient(pierect.center(), pierect.width()/2, pierect.center());
            gradient.setColorAt(0, Qt::white);
            gradient.setColorAt(1, col);


            // draw filled pie
            painter.setBrush(gradient);
            painter.setBackgroundMode(Qt::OpaqueMode);
            if (m_total == data) {
                painter.drawEllipse(pierect);
            } else {
                painter.drawPie(pierect, -sum * 16.0, -len * 16.0);
            }

            // draw outline
            painter.setBackgroundMode(Qt::TransparentMode);
            painter.setBrush(QBrush(col,Qt::NoBrush));
            painter.setPen(QPen(QColor(Qt::black),1.5));

            if (m_total == data) {
                painter.drawEllipse(pierect);
            } else {
                painter.drawPie(pierect, -sum * 16.0, -len * 16.0);
            }


            // Draw text labels if they fit
            if (len > 20) {
                float angle = (sum+90.0) + len / 2.0;
                double tpx = (pierect.x() + pierect.width()/2)  + (sin((180 - angle) * (M_PI / 180.0)) * (radius / 1.65));
                double tpy = (pierect.y() + pierect.height()/2) + (cos((180 - angle) * (M_PI / 180.0)) * (radius / 1.65));

                QString txt = schema::channel[m_codes[m]].label(); //QString::number(floor(100.0/m_total*data),'f',0)+"%";
                int x, y;
                GetTextExtent(txt, x, y);
                // antialiasing looks like crap here..
                painter.setPen(QColor(Qt::black));
                painter.drawText(tpx - (x / 2.0), tpy+3, txt);
            }
            sum += len;

        } else if (m_graph_type == GST_CandleStick) {
            /////////////////////////////////////////////////////////////////////////////////////
            // CandleStick Chart
            /////////////////////////////////////////////////////////////////////////////////////

            QColor &col = m_colors[m % m_colors.size()];

            float bw = xmult * float(data);

            QLinearGradient linearGrad(QPointF(0, 0), QPointF(bw, 0));
            linearGrad.setColorAt(0, col);
            linearGrad.setColorAt(1, Qt::white);
            painter.fillRect(xp, start_py, bw, height, QBrush(linearGrad));

            painter.setPen(m_outline_color);
            painter.drawLine(xp, start_py, xp + bw, start_py);
            painter.drawLine(xp + bw, start_py + height, xp, start_py + height);

            if (!m_names[m].isEmpty()) {
                int px, py;
                GetTextExtent(m_names[m], px, py);

                if (px + 5 < bw) {
                    w.renderText(m_names[m], (xp + bw / 2) - (px / 2), top + ((height / 2) - (py / 2)), 0, Qt::black);
                }
            }

            xp += bw;
            /////////////////////////////////////////////////////////////////////////////////////
            // Line Chart
            /////////////////////////////////////////////////////////////////////////////////////
        } else if (m_graph_type == GST_Line) {
            QColor col = Qt::black; //m_colors[m % m_colors.size()];
            painter.setPen(col);
            float h = (top + height) - (float(data) * ymult);

            if (line_first) {
                line_first = false;
            } else {
                painter.drawLine(xp, line_last, xp + line_step, h);
                xp += line_step;
            }

            line_last = h;
        }
    }
}


gTAPGraph::gTAPGraph(ChannelID code, GraphSegmentType gt, QColor gradient_color,
                     QColor outline_color)
    : gSegmentChart(gt, gradient_color, outline_color), m_code(code)
{
    m_colors.push_back(Qt::red);
    m_colors.push_back(Qt::green);
}
gTAPGraph::~gTAPGraph()
{
}
void gTAPGraph::SetDay(Day *d)
{
    Layer::SetDay(d);
    m_total = 0;

    if (!m_day) { return; }

    QMap<EventStoreType, qint64> tap;

    EventStoreType data = 0, lastval = 0;
    qint64 time = 0, lasttime = 0;
    //bool first;
    bool rfirst = true;
    //bool changed;
    EventDataType gain = 1, offset = 0;
    QHash<ChannelID, QVector<EventList *> >::iterator ei;

    for (const auto & sess : m_day->sessions) {
        if (!sess->enabled()) { continue; }

        ei = sess->eventlist.find(m_code);
        if (ei == sess->eventlist.end()) { continue; }

        for (const auto & el : ei.value()) {
            lasttime = el->time(0);
            lastval = el->raw(0);

            if (rfirst) {
                gain = el->gain();
                offset = el->offset();
                rfirst = false;
            }

            for (quint32 i=1, end=el->count(); i < end; i++) {
                data = el->raw(i);
                time = el->time(i);

                if (lastval != data) {
                    qint64 v = (time - lasttime);

                    if (tap.find(lastval) != tap.end()) {
                        tap[lastval] += v;
                    } else {
                        tap[lastval] = v;
                    }

                    //changed=true;
                    lasttime = time;
                    lastval = data;
                }
            }

            if (time != lasttime) {
                qint64 v = (time - lasttime);

                if (tap.find(lastval) != tap.end()) {
                    tap[data] += v;
                } else {
                    tap[data] = v;
                }
            }
        }
    }

    m_values.clear();
    m_names.clear();
    m_total = 0;
    EventDataType val;

    for (auto i=tap.begin(), end=tap.end(); i != end; i++) {
        val = float(i.key()) * gain + offset;

        m_values.push_back(i.value() / 1000L);
        m_total += i.value() / 1000L;
        m_names.push_back(QString::number(val, 'f', 2));
    }

    m_empty = m_values.size() == 0;
}
