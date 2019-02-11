/* gYAxis Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "Graphs/gYAxis.h"

#include <QDebug>

#include <math.h>

#include "Graphs/glcommon.h"
#include "Graphs/gGraph.h"
#include "Graphs/gGraphView.h"
#include "SleepLib/profiles.h"

#include <QFontMetrics>

gXGrid::gXGrid(QColor col)
    : Layer(NoChannel)
{
    Q_UNUSED(col)

    m_major_color = QColor(180, 180, 180, 64);
    //m_major_color=QColor(180,180,180,92);
    m_minor_color = QColor(230, 230, 230, 64);
    m_show_major_lines = true;
    m_show_minor_lines = true;
}
gXGrid::~gXGrid()
{
}
void gXGrid::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    float left = region.boundingRect().left();
    float top = region.boundingRect().top()+0.001f;
    float width = region.boundingRect().width();
    float height = region.boundingRect().height();

    //int x, y;

    EventDataType miny = w.physMinY();
    EventDataType maxy = w.physMaxY();

    w.roundY(miny, maxy);

    //EventDataType dy=maxy-miny;

    if (height < 0) { return; }

//    static QString fd = "0";
//    GetTextExtent(fd, x, y);
    QFontMetrics fm(*defaultfont);
    int y=fm.height();

    double max_yticks = round(height / (y + 14.0*w.printScaleY())); // plus spacing between lines
    //double yt=1/max_yticks;

    double mxy = maxy; //MAX(fabs(maxy), fabs(miny));
    double mny = miny;

//    if (miny < 0) {
//        mny = -mxy;
//    }

    double rxy = mxy - mny;

    int myt;
    bool fnd = false;

    for (myt = max_yticks; myt >= 1.001; myt--) {
        float v = rxy / float(myt);

        if (float(v) == int(v)) {
            fnd = true;
            break;
        }
    }

    if (fnd) { max_yticks = myt; }
    else {
        max_yticks = 2;
    }

    double yt = 1 / max_yticks;

    double ymult = height / rxy;

    double min_ytick = rxy * yt;

    float ty, h;

    if (min_ytick <= 0) {
        qDebug() << "min_ytick error in gXGrid::paint() in" << w.title();
        return;
    }

    if (min_ytick >= 1000000) {
        min_ytick = 100;
    }
    QVector<QLineF> majorlines;
    QVector<QLineF> minorlines;

    for (double i = miny; i <= maxy + min_ytick + 0.001; i += min_ytick) {
        ty = (i - miny) * ymult;
        h = top + height - ty;

        if (m_show_major_lines && (i > miny)) {
            majorlines.append(QLineF(left, h, left + width, h));
        }

        double z = (min_ytick / 4) * ymult;
        double g = h;

        for (int i = 0; i < 3; i++) {
            g += z;

            if (g > top + height) { break; }

            //if (vertcnt>=maxverts) {
            //  qWarning() << "vertarray bounds exceeded in gYAxis for " << w.title() << "graph" << "MinY =" <<miny << "MaxY =" << maxy << "min_ytick=" <<min_ytick;
            //                break;
            //          }
            if (m_show_minor_lines) {// && (i > miny)) {
                minorlines.append(QLineF(left, g, left + width, g));
            }
        }
    }
    painter.setPen(QPen(m_major_color,1));
    painter.drawLines(majorlines);
    painter.setPen(QPen(m_minor_color,1));
    painter.drawLines(minorlines);
    w.graphView()->lines_drawn_this_frame += majorlines.size() + minorlines.size();
}




gYAxis::gYAxis(QColor col)
    : Layer(NoChannel)
{
    m_line_color = col;
    m_text_color = col;
    m_yaxis_scale = 1;
}
gYAxis::~gYAxis()
{
}

int gYAxis::minimumWidth()
{
    return Margin;
}

void gYAxis::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    float left = region.boundingRect().left();
    float top = region.boundingRect().top()+0.001;
    float width = region.boundingRect().width();
    float height = region.boundingRect().height();

    int x, y; //,yh=0;

    //Todo: clean this up as there is a lot of duplicate code between the sections

    QFontMetrics fm(*defaultfont);
    static QString fd;

    if (0) {
    } else {
        if (height < 0) { return; }

        if (height > 4000) { return; }

        int labelW = 0;


        EventDataType miny = w.physMinY();
        EventDataType maxy = w.physMaxY();

        w.roundY(miny, maxy);

        EventDataType dy = maxy - miny;

//        GetTextExtent(fd, x, y);
        y=fm.height();
        //x=0;

#ifdef DEBUG_LAYOUT
        painter.setPen(Qt::green);
        painter.drawLine(0,top,100,top);
#endif

        double max_yticks = round(height / (y + 14.0)); // plus spacing between lines

        double mxy = maxy; // MAX(fabs(maxy), fabs(miny));
        double mny = miny;

        if (miny < 0) {
//            mny = -mxy;
        }

        double rxy = mxy - mny;

        int myt;
        bool fnd = false;

        for (myt = max_yticks; myt > 2; myt--) {
            float v = rxy / float(myt);

            if (qAbs(v - int(v)) < 0.000001) {
                fnd = true;
                break;
            }
        }

        if (fnd) { max_yticks = myt; }

        double yt = 1 / max_yticks;

        double ymult = height / rxy;

        double min_ytick = rxy * yt;


        //if (dy>5) {
        //    min_ytick=round(min_ytick);
        //} else {

        //}

        float ty, h;

        if (min_ytick <= 0) {
            qDebug() << "min_ytick error in gYAxis::paint() in" << w.title();
            return;
        }

        if (min_ytick >= 1000000) {
            min_ytick = 100;
        }

        QVector<QLineF> ticks;

        QRect r2;
        float shorttick = 4.0 * w.printScaleX();
        for (double i = miny; i <= maxy + min_ytick + 0.001; i += min_ytick) {
            ty = (i - miny) * ymult;

            if (dy < 5) {
                fd = Format(i * m_yaxis_scale, 2);
            } else {
                fd = Format(i * m_yaxis_scale, 1);
            }

            r2 = fm.boundingRect(fd);
            x = r2.width();
            y = r2.height();
            //GetTextExtent(fd, x, y); // performance bottleneck..

            if (x > labelW) { labelW = x; }

            h = top + height - ty;

            if (h < top-0.002) { continue; }

            w.renderText(fd, left + width - shorttick*2 - x, (h + (y / 2.0)), 0, m_text_color, defaultfont);

            ticks.append(QLineF(left + width - shorttick, h, left + width, h));

            double z = (min_ytick / 4) * ymult;
            double g = h;

            for (int i = 0; i < 3; i++) {
                g += z;

                if (g > top + height + 0.002) { break; }

                ticks.append(QLineF(left + width - shorttick/2, g, left + width, g));
            }
        }
        painter.setPen(Qt::black);
        painter.drawLines(ticks);
        w.graphView()->lines_drawn_this_frame += ticks.size();

    }
}
const QString gYAxis::Format(EventDataType v, int dp)
{
    return QString::number(v, 'f', dp);
}

bool gYAxis::mouseMoveEvent(QMouseEvent *event, gGraph *graph)
{
    if (!AppSetting->graphTooltips()) {
        return false;
    }

    graph->timedRedraw(0);

    int x = event->x();
    int y = event->y();

    if (!graph->units().isEmpty()) {
        graph->ToolTip(graph->units(), x+10, y+10, TT_AlignLeft);
        //   graph->redraw();
    }

    return false;
}

bool gYAxis::mouseDoubleClickEvent(QMouseEvent *event, gGraph *graph)
{
    if (graph) {
        //        int x=event->x();
        //        int y=event->y();
        short z = (graph->zoomY() + 1) % gGraph::maxZoomY;
        graph->setZoomY(z);
        qDebug() << "Mouse double clicked for" << graph->name() << z;
    }

    Q_UNUSED(event);
    return false;
}

const QString gYAxisTime::Format(EventDataType v, int dp)
{
    int h = int(v) % 24;
    int m = int(v * 60) % 60;
    int s = int(v * 3600) % 60;

    char pm[3] = {"pm"};

    if (show_12hr) {

        h >= 12 ? pm[0] = 'a' : pm[0] = 'p';
        h %= 12;

        if (h == 0) { h = 12; }
    } else {
        pm[0] = 0;
    }

    if (dp > 2) { return QString().sprintf("%02i:%02i:%02i%s", h, m, s, pm); }

    return QString().sprintf("%i:%02i%s", h, m, pm);
}

const QString gYAxisWeight::Format(EventDataType v, int dp)
{
    Q_UNUSED(dp)
    return weightString(v, m_unitsystem);
}
