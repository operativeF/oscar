/* gSessionTimesChart Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <math.h>
#include <QLabel>
#include <QDateTime>

#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "gSessionTimesChart.h"

#include "gYAxis.h"

extern MainWindow * mainwin;

short SummaryCalcItem::midcalc;

gSummaryChart::gSummaryChart(QString label, MachineType machtype)
    :Layer(NoChannel), m_label(label), m_machtype(machtype)
{
    m_layertype = LT_Overview;
    QDateTime d1 = QDateTime::currentDateTime();
    QDateTime d2 = d1;
    d1.setTimeSpec(Qt::UTC);  // CHECK: Does this deal with DST?
    tz_offset = d2.secsTo(d1);
    tz_hours = tz_offset / 3600.0;
    expected_slices = 5;

    idx_end = 0;
    idx_start = 0;
}

gSummaryChart::gSummaryChart(ChannelID code, MachineType machtype)
    :Layer(code), m_machtype(machtype)
{
    m_layertype = LT_Overview;
    QDateTime d1 = QDateTime::currentDateTime();
    QDateTime d2 = d1;
    d1.setTimeSpec(Qt::UTC);  // CHECK: Does this deal with DST?
    tz_offset = d2.secsTo(d1);
    tz_hours = tz_offset / 3600.0;
    expected_slices = 5;

    addCalc(code, ST_MIN, brighten(schema::channel[code].defaultColor() ,0.60f));
    addCalc(code, ST_MID, brighten(schema::channel[code].defaultColor() ,1.20f));
    addCalc(code, ST_90P, brighten(schema::channel[code].defaultColor() ,1.70f));
    addCalc(code, ST_MAX, brighten(schema::channel[code].defaultColor() ,2.30f));

    idx_end = 0;
    idx_start = 0;
}

gSummaryChart::~gSummaryChart()
{
}

void gSummaryChart::SetDay(Day *unused_day)
{
    cache.clear();

    Q_UNUSED(unused_day)
    Layer::SetDay(nullptr);

    firstday = p_profile->FirstDay(m_machtype);
    lastday = p_profile->LastDay(m_machtype);

    dayindex.clear();
    daylist.clear();

    if (!firstday.isValid() || !lastday.isValid()) return;
   // daylist.reserve(firstday.daysTo(lastday)+1);
    QDate date = firstday;
    int idx = 0;
    do {
        auto di = p_profile->daylist.find(date);
        Day * day = nullptr;
        if (di != p_profile->daylist.end()) {
            day = di.value();
        }
        daylist.append(day);
        dayindex[date] = idx;
        idx++;
        date = date.addDays(1);
    } while (date <= lastday);

    m_minx = QDateTime(firstday, QTime(0,0,0), Qt::UTC).toMSecsSinceEpoch();
    m_maxx = QDateTime(lastday, QTime(23,59,59), Qt::UTC).toMSecsSinceEpoch();
    m_miny = 0;
    m_maxy = 20;

    m_empty = false;

}


//QMap<QDate, int> gSummaryChart::dayindex;
//QList<Day *> gSummaryChart::daylist;


bool gSummaryChart::keyPressEvent(QKeyEvent *event, gGraph *graph)
{
    Q_UNUSED(event)
    Q_UNUSED(graph)
    return false;
}

bool gSummaryChart::mouseMoveEvent(QMouseEvent *event, gGraph *graph)
{
    Q_UNUSED(event)
    Q_UNUSED(graph)
    return false;
}

bool gSummaryChart::mousePressEvent(QMouseEvent *event, gGraph *graph)
{
    Q_UNUSED(event)
    Q_UNUSED(graph)
    return false;
}

bool gSummaryChart::mouseReleaseEvent(QMouseEvent *event, gGraph *graph)
{
    if (!(event->modifiers() & Qt::ShiftModifier)) return false;

    float x = event->x() - m_rect.left();
    float y = event->y() - m_rect.top();
    qDebug() << x << y;

    EventDataType miny;
    EventDataType maxy;

    graph->roundY(miny, maxy);

    QDate date = QDateTime::fromMSecsSinceEpoch(m_minx, Qt::UTC).date();

    int days = ceil(double(m_maxx - m_minx) / 86400000.0);

    float barw = float(m_rect.width()) / float(days);

    float idx = x/barw;

    date = date.addDays(idx);

    auto it = dayindex.find(date);
    if (it != dayindex.end()) {
        Day * day = daylist.at(it.value());
        if (day) {
            mainwin->getDaily()->LoadDate(date);
            mainwin->JumpDaily();
        }
    }

    return true;
}

void gSummaryChart::preCalc()
{
    midcalc = p_profile->general->prefCalcMiddle();

    for (auto & calc : calcitems) {
        calc.reset(idx_end - idx_start, midcalc);
    }
}

void gSummaryChart::customCalc(Day *day, QVector<SummaryChartSlice> & slices)
{
    int size = slices.size();
    if (size != calcitems.size()) {
        return;
    }
    float hour = day->hours(m_machtype);

    for (int i=0; i < size; ++i) {
        const SummaryChartSlice & slice = slices.at(i);
        SummaryCalcItem & calc = calcitems[i];

        calc.update(slice.value, hour);
     }
}

void gSummaryChart::afterDraw(QPainter &painter, gGraph &graph, QRectF rect)
{
    if (totaldays == nousedays) return;

    if (calcitems.size() == 0) return;

    QStringList strlist;
    QString txt;

    int midcalc = p_profile->general->prefCalcMiddle();
    QString midstr;
    if (midcalc == 0) {
        midstr = QObject::tr("Med.");
    } else if (midcalc == 1) {
        midstr = QObject::tr("W-Avg");
    } else {
        midstr = QObject::tr("Avg");
    }


    float perc = p_profile->general->prefCalcPercentile();
    QString percstr = QString("%1%").arg(perc, 0, 'f',0);

    schema::Channel & chan = schema::channel[calcitems.at(0).code];

    for (auto & calc : calcitems) {

        if (calcitems.size() == 1) {
            float val = calc.min;
            if (val < 99998)
                strlist.append(QObject::tr("Min: %1").arg(val,0,'f',2));
        }

        float mid = 0;
        switch (midcalc) {
        case 0:
            if (calc.median_data.size() > 0) {
                mid = median(calc.median_data.begin(), calc.median_data.end());
            }
            break;
        case 1:
            mid = calc.wavg_sum / calc.divisor;
            break;
        case 2:
            mid = calc.avg_sum / calc.cnt;
            break;
        }

        float val = 0;
        switch (calc.type) {
        case ST_CPH:
            val = mid;
            txt = midstr+": ";
            break;
        case ST_SPH:
            val = mid;
            txt = midstr+": ";
            break;
        case ST_MIN:
            val = calc.min;
            if (val >= 99998) continue;
            txt = QObject::tr("Min: ");
            break;
        case ST_MAX:
            val = calc.max;
            if (val <= -99998) continue;
            txt = QObject::tr("Max: ");
            break;
        case ST_SETMIN:
            val = calc.min;
            if (val >= 99998) continue;
            txt = QObject::tr("Min: ");
            break;
        case ST_SETMAX:
            val = calc.max;
            if (val <= -99998) continue;
            txt = QObject::tr("Max: ");
            break;
        case ST_MID:
            val = mid;
            txt = QObject::tr("%1: ").arg(midstr);
            break;
        case ST_90P:
            val = mid;
            txt = QObject::tr("%1: ").arg(percstr);
            break;
        default:
            val = mid;
            txt = QObject::tr("???: ");
            break;
        }
        strlist.append(QString("%1%2").arg(txt).arg(val,0,'f',2));
        if (calcitems.size() == 1) {
            val = calc.max;
            if (val > -99998)
                strlist.append(QObject::tr("Max: %1").arg(val,0,'f',2));
        }
    }

    QString str;
    if (totaldays > 1) {
        str = QObject::tr("%1 (%2 days): ").arg(chan.fullname()).arg(totaldays);
    } else {
        str = QObject::tr("%1 (%2 day): ").arg(chan.fullname()).arg(totaldays);
    }
    str += " "+strlist.join(", ");

    QRectF rec(rect.left(), rect.top(), 0,0);
    painter.setFont(*defaultfont);
    rec = painter.boundingRect(rec, Qt::AlignTop, str);
    rec.moveBottom(rect.top()-3*graph.printScaleY());
    painter.drawText(rec, Qt::AlignTop, str);

//    graph.renderText(str, rect.left(), rect.top()-5*graph.printScaleY(), 0);


}

QString gSummaryChart::tooltipData(Day *, int idx)
{
    QString txt;
    const auto & slices = cache[idx];
    for (const auto & slice : slices) {
        txt += QString("\n%1: %2").arg(slice.name).arg(float(slice.value), 0, 'f', 2);

    }
    return txt;
}

void gSummaryChart::populate(Day * day, int idx)
{

    bool good = false;
    for (const auto & item : calcitems) {
        if (day->hasData(item.code, item.type)) {
            good = true;
            break;
        }
    }
    if (!good) return;

    auto & slices = cache[idx];

    float hours = day->hours(m_machtype);
    float base = 0;

    for (auto & item : calcitems) {
        ChannelID code = item.code;
        schema::Channel & chan = schema::channel[code];
        float value = 0;
        QString name;
        QColor color;
        switch (item.type) {
        case ST_CPH:
            value = day->count(code) / hours;
            name = chan.label();
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value, name, color));
            break;
        case ST_SPH:
            value = (100.0 / hours) * (day->sum(code) / 3600.0);
            name = QObject::tr("% in %1").arg(chan.label());
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value, name, color));
            break;
        case ST_HOURS:
            value = hours;
            name = QObject::tr("Hours");
            color = COLOR_LightBlue;
            slices.append(SummaryChartSlice(&item, value, hours, name, color));
            break;
        case ST_MIN:
            value = day->Min(code);
            name = QObject::tr("Min %1").arg(chan.label());
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value - base, name, color));
            base = value;
            break;
        case ST_MID:
            value = day->calcMiddle(code);
            name = day->calcMiddleLabel(code);
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value - base, name, color));
            base = value;
            break;
        case ST_90P:
            value = day->calcPercentile(code);
            name = day->calcPercentileLabel(code);
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value - base, name, color));
            base = value;
            break;
        case ST_MAX:
            value = day->calcMax(code);
            name = day->calcMaxLabel(code);
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value - base, name, color));
            base = value;
            break;
        default:
            break;
        }
    }
}

void gSummaryChart::paint(QPainter &painter, gGraph &graph, const QRegion &region)
{
    QRectF rect = region.boundingRect();

    rect.translate(0.0f, 0.001f);

    painter.setPen(QColor(Qt::black));
    painter.drawRect(rect);

    rect.moveBottom(rect.bottom()+1);

    m_minx = graph.min_x;
    m_maxx = graph.max_x;

    QDateTime date2 = QDateTime::fromMSecsSinceEpoch(m_minx, Qt::UTC);
    QDateTime enddate2 = QDateTime::fromMSecsSinceEpoch(m_maxx, Qt::UTC);

    QDate date = date2.date();
    QDate enddate = enddate2.date();

    int days = ceil(double(m_maxx - m_minx) / 86400000.0);

    //float lasty1 = rect.bottom();

    auto it = dayindex.find(date);
    idx_start = 0;
    if (it == dayindex.end()) {
        it = dayindex.begin();
    } else {
        idx_start = it.value();
    }

    int idx = idx_start;

    auto ite = dayindex.find(enddate);
    idx_end = daylist.size()-1;
    if (ite != dayindex.end()) {
        idx_end = ite.value();
    }

    QPoint mouse = graph.graphView()->currentMousePos();

    nousedays = 0;
    totaldays = 0;

    QRectF hl_rect;
    QDate hl_date;
    Day * hl_day = nullptr;
    int hl_idx = -1;
    bool hl = false;

    if ((daylist.size() == 0) || (it == dayindex.end()))
        return;

    //Day * lastday = nullptr;

    //    int dc = 0;
//    for (int i=idx; i<=idx_end; ++i) {
//        Day * day = daylist.at(i);
//        if (day || lastday) {
//            dc++;
//        }
//        lastday = day;
//    }
//    days = dc;
//    lastday = nullptr;
    float barw = float(rect.width()) / float(days);

    QString hl2_text = "";

    QVector<QRectF> outlines;
    int size = idx_end - idx;
    outlines.reserve(size * expected_slices);

    // Virtual call to setup any custom graph stuff
    preCalc();

    float lastx1 = rect.left();
    float right_edge = (rect.left()+rect.width()+1);


    /////////////////////////////////////////////////////////////////////
    /// Calculate Graph Peaks
    /////////////////////////////////////////////////////////////////////
    peak_value = 0;
    for (int i=idx; i <= idx_end; ++i, lastx1 += barw) {
        Day * day = daylist.at(i);

        if ((lastx1 + barw) > right_edge)
            break;

        if (!day) {
            continue;
        }

        day->OpenSummary();

        auto cit = cache.find(i);

        if (cit == cache.end()) {
            populate(day, i);
            cit = cache.find(i);
        }

        if (cit != cache.end()) {
            float base = 0, val;
            for (const auto & slice : cit.value()) {
                val = slice.height;
                base += val;
            }
            peak_value = qMax(peak_value, base);
        }
    }
    m_miny = 0;
    m_maxy = ceil(peak_value);

    /////////////////////////////////////////////////////////////////////
    /// Y-Axis scaling
    /////////////////////////////////////////////////////////////////////

    EventDataType miny;
    EventDataType maxy;

    graph.roundY(miny, maxy);
    float ymult = float(rect.height()) / (maxy-miny);

    lastx1 = rect.left();

    /////////////////////////////////////////////////////////////////////
    /// Main drawing loop
    /////////////////////////////////////////////////////////////////////
    do {
        Day * day = daylist.at(idx);

        if ((lastx1 + barw) > right_edge)
            break;

        totaldays++;

        if (!day) {//  || !day->hasMachine(m_machtype)) {
           // lasty1 = rect.bottom();
            lastx1 += barw;
            it++;
            nousedays++;
            //lastday = day;
            continue;
        }

        //lastday = day;

        float x1 = lastx1 + barw;

        day->OpenSummary();
        QRectF hl2_rect;

        bool hlday = false;
        QRectF rec2(lastx1, rect.top(), barw, rect.height());
        if (rec2.contains(mouse)) {
            hl_rect = rec2;
            hl_day = day;
            hl_date = it.key();
            hl_idx = idx;

            hl = true;
            hlday = true;
        }

        auto cit = cache.find(idx);

        if (cit == cache.end()) {
            populate(day, idx);
            cit = cache.find(idx);
        }

        float lastval = 0, val, y1,y2;
        if (cit != cache.end()) {
            /////////////////////////////////////////////////////////////////////////////////////
            /// Draw pressure settings
            /////////////////////////////////////////////////////////////////////////////////////
            QVector<SummaryChartSlice> & list = cit.value();
            customCalc(day, list);

            QLinearGradient gradient(lastx1, 0, lastx1 + barw, 0); //rect.bottom(), barw, rect.bottom());

            for (const auto & slice : list) {
                val = slice.height;
                y1 = ((lastval-miny) * ymult);
                y2 = (val * ymult);
                QColor color = slice.color;

                QRectF rec = QRectF(lastx1, rect.bottom() - y1, barw, -y2).intersected(rect);

                if (hlday) {
                    if (rec.contains(mouse.x(), mouse.y())) {
                        color = Qt::yellow;
                        hl2_rect = rec;
                    }
                }

                if (barw <= 3) {
                    painter.fillRect(rec, QBrush(color));
                } else if (barw > 8) {
                    gradient.setColorAt(0,color);
                    gradient.setColorAt(1,brighten(color, 2.0));
                    painter.fillRect(rec, QBrush(gradient));
//                    painter.fillRect(rec, slice.brush);
                    outlines.append(rec);
                } else {
                    painter.fillRect(rec, brighten(color, 1.25));
                    outlines.append(rec);
                }

                lastval += val;
            }
        }

        lastx1 = x1;
        it++;
    } while (++idx <= idx_end);
    painter.setPen(QPen(Qt::black,1));
    painter.drawRects(outlines);

    if (hl) {
        QColor col2(255,0,0,64);
        painter.fillRect(hl_rect, QBrush(col2));

        QString txt = hl_date.toString(Qt::SystemLocaleShortDate)+" ";
        if (hl_day) {
            // grab extra tooltip data
            txt += tooltipData(hl_day, hl_idx);
            if (!hl2_text.isEmpty()) {
                QColor col = Qt::yellow;
                col.setAlpha(255);
           //     painter.fillRect(hl2_rect, QBrush(col));
                txt += hl2_text;
            }
        }

        graph.ToolTip(txt, mouse.x()-15, mouse.y()+5, TT_AlignRight);
    }
    try {
        afterDraw(painter, graph, rect);
    } catch(...) {
        qDebug() << "Bad median call in" << m_label;
    }


    // This could be turning off graphs prematurely..
    if (cache.size() == 0) {

        m_empty = true;
        graph.graphView()->updateScale();
    }

}

QString gUsageChart::tooltipData(Day * day, int)
{
    return QObject::tr("\nHours: %1").arg(day->hours(m_machtype), 0, 'f', 2);
}

void gUsageChart::populate(Day *day, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];

    float hours = day->hours();

    QColor cpapcolor = day->summaryOnly() ? QColor(128,128,128) : calcitems[0].color;
    bool haveoxi = day->hasMachine(MT_OXIMETER);

    QColor goodcolor = haveoxi ? QColor(128,255,196) : cpapcolor;

    QColor color = (hours < compliance_threshold) ? QColor(255,64,64) : goodcolor;
    slices.append(SummaryChartSlice(&calcitems[0], hours, hours, QObject::tr("Hours"), color));
}

void gUsageChart::preCalc()
{
    midcalc = p_profile->general->prefCalcMiddle();

    compliance_threshold = p_profile->cpap->complianceHours();
    incompdays = 0;

    SummaryCalcItem & calc = calcitems[0];
    calc.reset(idx_end - idx_start, midcalc);
}

void gUsageChart::customCalc(Day *, QVector<SummaryChartSlice> &list)
{
    if (list.size() == 0) {
        incompdays++;
        return;
    }

    SummaryChartSlice & slice = list[0];
    SummaryCalcItem & calc = calcitems[0];

    if (slice.value < compliance_threshold) incompdays++;

    calc.update(slice.value, 1);
}

void gUsageChart::afterDraw(QPainter &, gGraph &graph, QRectF rect)
{
    if (totaldays == nousedays) return;

    if (totaldays > 1) {
        float comp = 100.0 - ((float(incompdays + nousedays) / float(totaldays)) * 100.0);

        int midcalc = p_profile->general->prefCalcMiddle();
        float mid = 0;
        SummaryCalcItem & calc = calcitems[0];
        switch (midcalc) {
        case 0: // median
            mid = median(calc.median_data.begin(), calc.median_data.end());
            break;
        case 1: // w-avg
            mid = calc.wavg_sum / calc.divisor;
            break;
        case 2:
            mid = calc.avg_sum / calc.cnt;
            break;
        }

        QString txt = QObject::tr("%1 low usage, %2 no usage, out of %3 days (%4% compliant.) Length: %5 / %6 / %7").
                arg(incompdays).arg(nousedays).arg(totaldays).arg(comp,0,'f',1).arg(calc.min, 0, 'f', 2).arg(mid, 0, 'f', 2).arg(calc.max, 0, 'f', 2);;
        graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);
    }
}


void gSessionTimesChart::preCalc() {

    midcalc = p_profile->general->prefCalcMiddle();

    num_slices = 0;
    num_days = 0;
    total_length = 0;
    SummaryCalcItem  & calc = calcitems[0];

    calc.reset((idx_end - idx_start) * 6, midcalc);

    SummaryCalcItem  & calc1 = calcitems[1];

    calc1.reset(idx_end - idx_start, midcalc);

    SummaryCalcItem  & calc2 = calcitems[2];
    calc2.reset(idx_end - idx_start, midcalc);
}

void gSessionTimesChart::customCalc(Day *, QVector<SummaryChartSlice> & slices) {
    int size = slices.size();
    num_slices += size;

    SummaryCalcItem  & calc1 = calcitems[1];
    calc1.update(slices.size(), 1);

    EventDataType max = 0;

    for (auto & slice : slices) {
        slice.calc->update(slice.height, slice.height);

        max = qMax(slice.height, max);
    }
    SummaryCalcItem  & calc2 = calcitems[2];

    calc2.update(max, max);

    num_days++;
}

void gSessionTimesChart::afterDraw(QPainter & /*painter */, gGraph &graph, QRectF rect)
{
    if (totaldays == nousedays) return;

    SummaryCalcItem  & calc = calcitems[0]; // session length
    SummaryCalcItem  & calc1 = calcitems[1]; // number of sessions
    SummaryCalcItem  & calc2 = calcitems[2]; // number of sessions

    int midcalc = p_profile->general->prefCalcMiddle();

    float mid = 0, mid1 = 0, midlongest = 0;
    switch (midcalc) {
    case 0:
        if (calc.median_data.size() > 0) {
            mid = median(calc.median_data.begin(), calc.median_data.end());
            mid1 = median(calc1.median_data.begin(), calc1.median_data.end());
            midlongest = median(calc2.median_data.begin(), calc2.median_data.end());
        }
        break;
    case 1:
        mid = calc.wavg_sum / calc.divisor;
        mid1 = calc1.wavg_sum / calc1.divisor;
        midlongest = calc2.wavg_sum / calc2.divisor;
        break;
    case 2:
        mid = calc.avg_sum / calc.cnt;
        mid1 = calc1.avg_sum / calc1.cnt;
        midlongest = calc2.avg_sum / calc2.cnt;
        break;
    }


//    float avgsess = float(num_slices) / float(num_days);

    QString txt = QObject::tr("Sessions: %1 / %2 / %3 Length: %4 / %5 / %6 Longest: %7 / %8 / %9")
            .arg(calc1.min, 0, 'f', 2).arg(mid1, 0, 'f', 2).arg(calc1.max, 0, 'f', 2)
            .arg(calc.min, 0, 'f', 2).arg(mid, 0, 'f', 2).arg(calc.max, 0, 'f', 2)
            .arg(calc2.min, 0, 'f', 2).arg(midlongest, 0, 'f', 2).arg(calc2.max, 0, 'f', 2);
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);
}

void gSessionTimesChart::paint(QPainter &painter, gGraph &graph, const QRegion &region)
{
    QRectF rect = region.boundingRect();

    painter.setPen(QColor(Qt::black));
    painter.drawRect(rect);

    m_minx = graph.min_x;
    m_maxx = graph.max_x;

    QDateTime date2 = QDateTime::fromMSecsSinceEpoch(m_minx, Qt::UTC);
    QDateTime enddate2 = QDateTime::fromMSecsSinceEpoch(m_maxx, Qt::UTC);

    QDate date = date2.date();
    QDate enddate = enddate2.date();

    int days = ceil(double(m_maxx - m_minx) / 86400000.0);

    float barw = float(rect.width()) / float(days);

    QDateTime splittime;

//    float lasty1 = rect.bottom();
    float lastx1 = rect.left();

    auto it = dayindex.find(date);
    int idx=0;

    if (it == dayindex.end()) {
        it = dayindex.begin();
    } else {
        idx = it.value();
    }

    auto ite = dayindex.find(enddate);
    int idx_end = daylist.size()-1;
    if (ite != dayindex.end()) {
        idx_end = ite.value();
    }

    QPoint mouse = graph.graphView()->currentMousePos();

    if (daylist.size() == 0) return;

    QVector<QRectF> outlines;
    int size = idx_end - idx;
    outlines.reserve(size * 5);

    auto it2 = it;

    /////////////////////////////////////////////////////////////////////
    /// Calculate Graph Peaks
    /////////////////////////////////////////////////////////////////////
    peak_value = 0;
    min_value = 999;
    auto it_end = dayindex.end();

    float right_edge = (rect.left()+rect.width()+1);
    for (int i=idx; (i <= idx_end) && (it2 != it_end); ++i, ++it2, lastx1 += barw) {
        Day * day = daylist.at(i);

        if ((lastx1 + barw) > right_edge)
            break;

        if (!day) {
            continue;
        }

        auto cit = cache.find(i);

        if (cit == cache.end()) {
            day->OpenSummary();
            date = it2.key();
            splittime = QDateTime(date, split);

            QVector<SummaryChartSlice> & slices = cache[i];

            bool haveoxi = day->hasMachine(MT_OXIMETER);

            QColor goodcolor = haveoxi ? QColor(128,255,196) : QColor(64,128,255);

            QString datestr = date.toString(Qt::SystemLocaleShortDate);

            for (const auto & sess : day->sessions) {
                if (!sess->enabled() || (sess->type() != m_machtype)) continue;

                // Look at mask on/off slices...
                if (sess->m_slices.size() > 0) {
                    // segments
                    for (const auto & slice : sess->m_slices) {
                        QDateTime st = QDateTime::fromMSecsSinceEpoch(slice.start, Qt::LocalTime);

                        float s1 = float(splittime.secsTo(st)) / 3600.0;

                        float s2 = double(slice.end - slice.start) / 3600000.0;

                        QColor col = (slice.status == EquipmentOn) ? goodcolor : Qt::black;
                        QString txt = QObject::tr("%1\nLength: %3\nStart: %2\n").arg(datestr).arg(st.time().toString("hh:mm:ss")).arg(s2,0,'f',2);

                        txt += (slice.status == EquipmentOn) ? QObject::tr("Mask On") : QObject::tr("Mask Off");
                        slices.append(SummaryChartSlice(&calcitems[0], s1, s2, txt, col));
                    }
                } else {
                    // otherwise just show session duration
                    qint64 sf = sess->first();
                    QDateTime st = QDateTime::fromMSecsSinceEpoch(sf, Qt::LocalTime);
                    float s1 = float(splittime.secsTo(st)) / 3600.0;

                    float s2 = sess->hours();

                    QString txt = QObject::tr("%1\nLength: %3\nStart: %2").arg(datestr).arg(st.time().toString("hh:mm:ss")).arg(s2,0,'f',2);

                    slices.append(SummaryChartSlice(&calcitems[0], s1, s2, txt, goodcolor));
                }
            }

            cit = cache.find(i);
        }

        if (cit != cache.end()) {
            float peak = 0, base = 999;

            for (const auto & slice : cit.value()) {
                float s1 = slice.value;
                float s2 = slice.height;

                peak = qMax(peak, s1+s2);
                base = qMin(base, s1);
            }
            peak_value = qMax(peak_value, peak);
            min_value = qMin(min_value, base);

        }
    }
    m_miny = (min_value < 999) ? floor(min_value) : 0;
    m_maxy = ceil(peak_value);

    /////////////////////////////////////////////////////////////////////
    /// Y-Axis scaling
    /////////////////////////////////////////////////////////////////////

    EventDataType miny;
    EventDataType maxy;

    graph.roundY(miny, maxy);
    float ymult = float(rect.height()) / (maxy-miny);


    preCalc();
    totaldays = 0;
    nousedays = 0;

    lastx1 = rect.left();

    /////////////////////////////////////////////////////////////////////
    /// Main Loop scaling
    /////////////////////////////////////////////////////////////////////
    do {
        Day * day = daylist.at(idx);

        if ((lastx1 + barw) > right_edge)
            break;

        totaldays++;


        if (!day) { // || !day->hasMachine(m_machtype)) {
           // lasty1 = rect.bottom();
            lastx1 += barw;
            nousedays++;
         //   it++;
            continue;
        }

        auto cit = cache.find(idx);

        float x1 = lastx1 + barw;

        //bool hl = false;

        QRectF rec2(lastx1, rect.top(), barw, rect.height());
        if (rec2.contains(mouse)) {
            QColor col2(255,0,0,64);
            painter.fillRect(rec2, QBrush(col2));
            //hl = true;
        }

        if (cit != cache.end()) {
            QVector<SummaryChartSlice> & slices = cit.value();

            customCalc(day, slices);

            QLinearGradient gradient(lastx1, rect.bottom(), lastx1+barw, rect.bottom());

            for (const auto & slice : slices) {
                float s1 = slice.value - miny;
                float s2 = slice.height;

                float y1 = (s1 * ymult);
                float y2 = (s2 * ymult);

                QColor col = slice.color;

                QRectF rec(lastx1, rect.bottom() - y1 - y2, barw, y2);
                rec = rec.intersected(rect);

                if (rec.contains(mouse)) {
                    col = Qt::yellow;
                    graph.ToolTip(slice.name, mouse.x() - 15,mouse.y() + 15, TT_AlignRight);

                }

                if (barw > 8) {
                    gradient.setColorAt(0,col);
                    gradient.setColorAt(1,brighten(col, 2.0));
                    painter.fillRect(rec, QBrush(gradient));
//                    painter.fillRect(rec, brush);
                    outlines.append(rec);
                } else if (barw > 3) {
                    painter.fillRect(rec, QBrush(brighten(col,1.25)));
                    outlines.append(rec);
                } else {
                    painter.fillRect(rec, QBrush(col));
                }

            }
        }


        lastx1 = x1;
    } while (++idx <= idx_end);

    painter.setPen(QPen(Qt::black,1));
    painter.drawRects(outlines);
    afterDraw(painter, graph, rect);
}

////////////////////////////////////////////////////////////////////////////
/// Total Time in Apnea Chart Stuff
////////////////////////////////////////////////////////////////////////////

void gTTIAChart::preCalc()
{
    gSummaryChart::preCalc();
}

void gTTIAChart::customCalc(Day *, QVector<SummaryChartSlice> & slices)
{
    if (slices.size() == 0) return;
    const SummaryChartSlice & slice = slices.at(0);

    calcitems[0].update(slice.value, slice.value);
}

void gTTIAChart::afterDraw(QPainter &, gGraph &graph, QRectF rect)
{
    QStringList txtlist;

    for (auto & calc : calcitems) {
        //ChannelID code = calc.code;
        //schema::Channel & chan = schema::channel[code];
        float mid = 0;
        switch (midcalc) {
        case 0:
            if (calc.median_data.size() > 0) {
                mid = median(calc.median_data.begin(), calc.median_data.end());
            }
            break;
        case 1:
            if (calc.divisor > 0) {
                mid = calc.wavg_sum / calc.divisor;
            }
            break;
        case 2:
            if (calc.divisor > 0) {
                mid = calc.avg_sum / calc.divisor;
            }
            break;
        }

        txtlist.append(QString("%1 %2 / %3 / %4").arg(QObject::tr("TTIA:")).arg(calc.min, 0, 'f', 2).arg(mid, 0, 'f', 2).arg(calc.max, 0, 'f', 2));
    }
    QString txt = txtlist.join(", ");
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);
}

void gTTIAChart::populate(Day *day, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];
    float ttia = day->sum(CPAP_Obstructive) + day->sum(CPAP_ClearAirway) + day->sum(CPAP_Apnea) + day->sum(CPAP_Hypopnea);
    int h = ttia / 3600;
    int m = int(ttia) / 60 % 60;
    int s = int(ttia) % 60;
    slices.append(SummaryChartSlice(&calcitems[0], ttia / 60.0, ttia / 60.0, QObject::tr("\nTTIA: %1").arg(QString().sprintf("%02i:%02i:%02i",h,m,s)), QColor(255,147,150)));
}

QString gTTIAChart::tooltipData(Day *, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];
    if (slices.size() == 0) return QString();

    const SummaryChartSlice & slice = slices.at(0);
    return slice.name;
}

////////////////////////////////////////////////////////////////////////////
/// AHI Chart Stuff
////////////////////////////////////////////////////////////////////////////
void gAHIChart::preCalc()
{
    gSummaryChart::preCalc();

    ahi_wavg = 0;
    ahi_avg = 0;
    total_days = 0;
    total_hours = 0;
    min_ahi = 99999;
    max_ahi = -99999;

    ahi_data.clear();
    ahi_data.reserve(idx_end-idx_start);
}
void gAHIChart::customCalc(Day *day, QVector<SummaryChartSlice> &list)
{
    int size = list.size();
    if (size == 0) return;
    EventDataType hours = day->hours(m_machtype);
    EventDataType ahi_cnt = 0;

    for (auto & slice : list) {
        SummaryCalcItem * calc = slice.calc;

        EventDataType value = slice.value;
        float valh =  value/ hours;

        switch (midcalc) {
        case 0:
            calc->median_data.append(valh);
            break;
        case 1:
            calc->wavg_sum += value;
            calc->divisor += hours;
        default:
            calc->avg_sum += value;
            calc->cnt++;
            break;
        }

        calc->min = qMin(valh, calc->min);
        calc->max = qMax(valh, calc->max);

        ahi_cnt += value;
    }
    min_ahi = qMin(ahi_cnt / hours, min_ahi);
    max_ahi = qMax(ahi_cnt / hours, max_ahi);

    ahi_data.append(ahi_cnt / hours);

    ahi_wavg += ahi_cnt;
    ahi_avg += ahi_cnt;
    total_hours += hours;
    total_days++;
}
void gAHIChart::afterDraw(QPainter & /*painter */, gGraph &graph, QRectF rect)
{
    if (totaldays == nousedays) return;

    //int size = idx_end - idx_start;

    bool skip = true;
    float med = 0;
    switch (midcalc) {
    case 0:
        if (ahi_data.size() > 0) {
            med = median(ahi_data.begin(), ahi_data.end());
            skip = false;
        }
        break;
    case 1: // wavg
        if (total_hours > 0) {
            med = ahi_wavg / total_hours;
            skip = false;
        }
        break;
    case 2: // avg
        if (total_days > 0) {
            med = ahi_avg / total_days;
            skip = false;
        }
        break;
    }

    QStringList txtlist;
    if (!skip) txtlist.append(QObject::tr("%1 %2 / %3 / %4").arg(STR_TR_AHI).arg(min_ahi, 0, 'f', 2).arg(med, 0, 'f', 2).arg(max_ahi, 0, 'f', 2));

    for (auto & calc : calcitems) {
        ChannelID code = calc.code;
        schema::Channel & chan = schema::channel[code];
        float mid = 0;
        skip = true;
        switch (midcalc) {
        case 0:
            if (calc.median_data.size() > 0) {
                mid = median(calc.median_data.begin(), calc.median_data.end());
                skip = false;
            }
            break;
        case 1:
            if (calc.divisor > 0) {
                mid = calc.wavg_sum / calc.divisor;
                skip = false;
            }
            break;
        case 2:
            if (calc.cnt > 0) {
                mid = calc.avg_sum / calc.cnt;
                skip = false;
            }
            break;
        }

        if (!skip) txtlist.append(QString("%1 %2 / %3 / %4").arg(chan.label()).arg(calc.min, 0, 'f', 2).arg(mid, 0, 'f', 2).arg(calc.max, 0, 'f', 2));
    }
    QString txt = txtlist.join(", ");
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);
}

void gAHIChart::populate(Day *day, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];

    float hours = day->hours();

    for (auto & calc : calcitems) {
        ChannelID code = calc.code;
        if (!day->hasData(code, ST_CNT)) continue;

        schema::Channel *chan = schema::channel.channels.find(code).value();

        float c = day->count(code);
        slices.append(SummaryChartSlice(&calc, c, c  / hours, chan->label(), calc.color));
    }
}
QString gAHIChart::tooltipData(Day *day, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];
    float total = 0;
    float hour = day->hours(m_machtype);
    QString txt;
    for (const auto & slice : slices) {
        total += slice.value;
        txt += QString("\n%1: %2").arg(slice.name).arg(float(slice.value) / hour, 0, 'f', 2);
    }
    return QString("\n%1: %2").arg(STR_TR_AHI).arg(float(total) / hour,0,'f',2)+txt;
}


gPressureChart::gPressureChart()
    :gSummaryChart("Pressure", MT_CPAP)
{

    // Do not reorder these!!! :P
    addCalc(CPAP_Pressure, ST_SETMAX, schema::channel[CPAP_Pressure].defaultColor());    // 00
    addCalc(CPAP_Pressure, ST_MID, schema::channel[CPAP_Pressure].defaultColor());       // 01
    addCalc(CPAP_Pressure, ST_90P, brighten(schema::channel[CPAP_Pressure].defaultColor(), 1.33f)); // 02
    addCalc(CPAP_PressureMin, ST_SETMIN, schema::channel[CPAP_PressureMin].defaultColor());  // 03
    addCalc(CPAP_PressureMax, ST_SETMAX, schema::channel[CPAP_PressureMax].defaultColor());  // 04

    addCalc(CPAP_EPAP, ST_SETMAX, schema::channel[CPAP_EPAP].defaultColor());      // 05
    addCalc(CPAP_IPAP, ST_SETMAX, schema::channel[CPAP_IPAP].defaultColor());      // 06
    addCalc(CPAP_EPAPLo, ST_SETMAX, schema::channel[CPAP_EPAPLo].defaultColor());    // 07
    addCalc(CPAP_IPAPHi, ST_SETMAX, schema::channel[CPAP_IPAPHi].defaultColor());    // 08

    addCalc(CPAP_EPAP, ST_MID, schema::channel[CPAP_EPAP].defaultColor());         // 09
    addCalc(CPAP_EPAP, ST_90P, brighten(schema::channel[CPAP_EPAP].defaultColor(),1.33f));         // 10
    addCalc(CPAP_IPAP, ST_MID, schema::channel[CPAP_IPAP].defaultColor());         // 11
    addCalc(CPAP_IPAP, ST_90P, brighten(schema::channel[CPAP_IPAP].defaultColor(),1.33f));         // 12
}

void gPressureChart::afterDraw(QPainter &, gGraph &graph, QRectF rect)
{
    int pressure_cnt = calcitems[0].cnt;
    int pressuremin_cnt = calcitems[3].cnt;
    int epap_cnt = calcitems[5].cnt;
    int ipap_cnt = calcitems[6].cnt;
    int ipaphi_cnt = calcitems[8].cnt;
    int epaplo_cnt = calcitems[7].cnt;

    QStringList presstr;

    float mid = 0;

    if (pressure_cnt > 0) {
        mid = calcitems[0].mid();
        presstr.append(QString("%1 %2/%3/%4").
                arg(STR_TR_CPAP).
                arg(calcitems[0].min,0,'f',1).
                arg(mid, 0, 'f', 1).
                arg(calcitems[0].max,0,'f',1));
    }
    if (pressuremin_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4/%5").
                arg(STR_TR_APAP).
                arg(calcitems[3].min,0,'f',1).
                arg(calcitems[1].mid(), 0, 'f', 1).
                arg(calcitems[2].mid(),0,'f',1).
                arg(calcitems[4].max, 0, 'f', 1));

    }
    if (epap_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4").
                arg(STR_TR_EPAP).
                arg(calcitems[5].min,0,'f',1).
                arg(calcitems[5].mid(), 0, 'f', 1).
                arg(calcitems[5].max, 0, 'f', 1));
    }
    if (ipap_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4").
             arg(STR_TR_IPAP).
             arg(calcitems[6].min,0,'f',1).
             arg(calcitems[6].mid(), 0, 'f', 1).
             arg(calcitems[6].max, 0, 'f', 1));
    }
    if (epaplo_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4").
            arg(STR_TR_EPAPLo).
            arg(calcitems[7].min,0,'f',1).
            arg(calcitems[7].mid(), 0, 'f', 1).
            arg(calcitems[7].max, 0, 'f', 1));
    }

    if (ipaphi_cnt > 0) {
        presstr.append(QString("%1 %2/%3/%4").
            arg(STR_TR_IPAPHi).
            arg(calcitems[8].min,0,'f',1).
            arg(calcitems[8].mid(), 0, 'f', 1).
            arg(calcitems[8].max, 0, 'f', 1));
    }
    QString txt = presstr.join(" ");
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);

}


void gPressureChart::populate(Day * day, int idx)
{
    float tmp;
    CPAPMode mode =  (CPAPMode)(int)qRound(day->settings_wavg(CPAP_Mode));
    QVector<SummaryChartSlice> & slices = cache[idx];

    if (mode == MODE_CPAP) {
        float pr = day->settings_max(CPAP_Pressure);
        slices.append(SummaryChartSlice(&calcitems[0], pr, pr, schema::channel[CPAP_Pressure].label(), calcitems[0].color));
    } else if (mode == MODE_APAP) {
        float min = day->settings_min(CPAP_PressureMin);
        float max = day->settings_max(CPAP_PressureMax);

        tmp = min;

        slices.append(SummaryChartSlice(&calcitems[3], min, min, schema::channel[CPAP_PressureMin].label(), calcitems[3].color));
        if (!day->summaryOnly()) {
            float med = day->calcMiddle(CPAP_Pressure);
            slices.append(SummaryChartSlice(&calcitems[1], med, med - tmp, day->calcMiddleLabel(CPAP_Pressure), calcitems[1].color));
            tmp += med - tmp;

            float p90 = day->calcPercentile(CPAP_Pressure);
            slices.append(SummaryChartSlice(&calcitems[2], p90, p90 - tmp, day->calcPercentileLabel(CPAP_Pressure), calcitems[2].color));
            tmp += p90 - tmp;
        }
        slices.append(SummaryChartSlice(&calcitems[4], max, max - tmp, schema::channel[CPAP_PressureMax].label(), calcitems[4].color));

    } else if (mode == MODE_BILEVEL_FIXED) {
        float epap = day->settings_max(CPAP_EPAP);
        float ipap = day->settings_max(CPAP_IPAP);

        slices.append(SummaryChartSlice(&calcitems[5], epap, epap, schema::channel[CPAP_EPAP].label(), calcitems[5].color));
        slices.append(SummaryChartSlice(&calcitems[6], ipap, ipap - epap, schema::channel[CPAP_IPAP].label(), calcitems[6].color));

    } else if (mode == MODE_BILEVEL_AUTO_FIXED_PS) {
        float epap = day->settings_max(CPAP_EPAPLo);
        tmp = epap;
        float ipap = day->settings_max(CPAP_IPAPHi);

        slices.append(SummaryChartSlice(&calcitems[7], epap, epap, schema::channel[CPAP_EPAPLo].label(), calcitems[7].color));
        if (!day->summaryOnly()) {

            float e50 = day->calcMiddle(CPAP_EPAP);
            slices.append(SummaryChartSlice(&calcitems[9], e50, e50 - tmp, day->calcMiddleLabel(CPAP_EPAP), calcitems[9].color));
            tmp += e50 - tmp;

            float e90 = day->calcPercentile(CPAP_EPAP);
            slices.append(SummaryChartSlice(&calcitems[10], e90, e90 - tmp, day->calcPercentileLabel(CPAP_EPAP), calcitems[10].color));
            tmp += e90 - tmp;

            float i50 = day->calcMiddle(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[11], i50, i50 - tmp, day->calcMiddleLabel(CPAP_IPAP), calcitems[11].color));
            tmp += i50 - tmp;

            float i90 = day->calcPercentile(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[12], i90, i90 - tmp, day->calcPercentileLabel(CPAP_IPAP), calcitems[12].color));
            tmp += i90 - tmp;
        }
        slices.append(SummaryChartSlice(&calcitems[8], ipap, ipap - tmp, schema::channel[CPAP_IPAPHi].label(), calcitems[8].color));
    } else if ((mode == MODE_BILEVEL_AUTO_VARIABLE_PS) || (mode == MODE_ASV_VARIABLE_EPAP)) {
        float epap = day->settings_max(CPAP_EPAPLo);
        tmp = epap;

        slices.append(SummaryChartSlice(&calcitems[7], epap, epap, schema::channel[CPAP_EPAPLo].label(), calcitems[7].color));
        if (!day->summaryOnly()) {
            float e50 = day->calcMiddle(CPAP_EPAP);
            slices.append(SummaryChartSlice(&calcitems[9], e50, e50 - tmp, day->calcMiddleLabel(CPAP_EPAP), calcitems[9].color));
            tmp += e50 - tmp;

            float e90 = day->calcPercentile(CPAP_EPAP);
            slices.append(SummaryChartSlice(&calcitems[10], e90, e90 - tmp, day->calcPercentileLabel(CPAP_EPAP), calcitems[10].color));
            tmp += e90 - tmp;

            float i50 = day->calcMiddle(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[11], i50, i50 - tmp, day->calcMiddleLabel(CPAP_IPAP), calcitems[11].color));
            tmp += i50 - tmp;

            float i90 = day->calcPercentile(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[12], i90, i90 - tmp, day->calcPercentileLabel(CPAP_IPAP), calcitems[12].color));
            tmp += i90 - tmp;
        }
        float ipap = day->settings_max(CPAP_IPAPHi);
        slices.append(SummaryChartSlice(&calcitems[8], ipap, ipap - tmp, schema::channel[CPAP_IPAPHi].label(), calcitems[8].color));
    } else if (mode == MODE_ASV) {
        float epap = day->settings_max(CPAP_EPAP);
        tmp = epap;

        slices.append(SummaryChartSlice(&calcitems[5], epap, epap, schema::channel[CPAP_EPAP].label(), calcitems[5].color));
        if (!day->summaryOnly()) {
            float i50 = day->calcMiddle(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[11], i50, i50 - tmp, day->calcMiddleLabel(CPAP_IPAP), calcitems[11].color));
            tmp += i50 - tmp;

            float i90 = day->calcPercentile(CPAP_IPAP);
            slices.append(SummaryChartSlice(&calcitems[12], i90, i90 - tmp, day->calcPercentileLabel(CPAP_IPAP), calcitems[12].color));
            tmp += i90 - tmp;
        }
        float ipap = day->settings_max(CPAP_IPAPHi);
        slices.append(SummaryChartSlice(&calcitems[8], ipap, ipap - tmp, schema::channel[CPAP_IPAPHi].label(), calcitems[8].color));
    }

}

//void gPressureChart::afterDraw(QPainter &painter, gGraph &graph, QRect rect)
//{
//}

