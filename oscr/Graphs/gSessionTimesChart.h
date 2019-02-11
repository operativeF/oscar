/* gSessionTimesChart Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GSESSIONTIMESCHART_H
#define GSESSIONTIMESCHART_H

#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "gGraphView.h"


struct TimeSpan
{
public:
    TimeSpan():begin(0), end(0) {}
    TimeSpan(float b, float e) : begin(b), end(e) {}
    TimeSpan(const TimeSpan & copy) {
        begin = copy.begin;
        end = copy.end;
    }
    ~TimeSpan() {}
    float begin;
    float end;
};

struct SummaryCalcItem {
    SummaryCalcItem() {
        code = 0;
        type = ST_CNT;
        color = Qt::black;
        wavg_sum = 0;
        avg_sum = 0;
        cnt = 0;
        divisor = 0;
        min = 0;
        max = 0;
    }
    SummaryCalcItem(const SummaryCalcItem & copy) {
        code = copy.code;
        type = copy.type;
        color = copy.color;

        wavg_sum = 0;
        avg_sum = 0;
        cnt = 0;
        divisor = 0;
        min = 0;
        max = 0;
        midcalc = p_profile->general->prefCalcMiddle();

    }

    SummaryCalcItem(ChannelID code, SummaryType type, QColor color)
        :code(code), type(type), color(color) {
    }
    float mid()
    {
        float val = 0;
        switch (midcalc) {
        case 0:
            if (median_data.size() > 0)
                val = median(median_data.begin(), median_data.end());
            break;
        case 1:
            if (divisor > 0)
                val = wavg_sum / divisor;
            break;
        case 2:
            if (cnt > 0)
                val = avg_sum / cnt;
        }
        return val;
    }


    inline void update(float value, float weight) {
        if (midcalc == 0) {
            median_data.append(value);
        }

        avg_sum += value;
        cnt++;
        wavg_sum += value * weight;
        divisor += weight;
        min = qMin(min, value);
        max = qMax(max, value);
    }

    void reset(int reserve, short mc) {
        midcalc = mc;

        wavg_sum = 0;
        avg_sum = 0;
        divisor = 0;
        cnt = 0;
        min = 99999;
        max = -99999;
        median_data.clear();
        if (midcalc == 0) {
            median_data.reserve(reserve);
        }
    }
    ChannelID code;
    SummaryType type;
    QColor color;

    double wavg_sum;
    double divisor;
    double avg_sum;
    int cnt;
    EventDataType min;
    EventDataType max;
    static short midcalc;

    QList<float> median_data;

};

struct SummaryChartSlice {
    SummaryChartSlice() {
        calc = nullptr;
        height = 0;
        value = 0;
        name = ST_CNT;
        color = Qt::black;
    }
    SummaryChartSlice(const SummaryChartSlice & copy) {
        calc = copy.calc;
        value = copy.value;
        height = copy.height;
        name = copy.name;
        color = copy.color;
//        brush = copy.brush;
    }

    SummaryChartSlice(SummaryCalcItem * calc, EventDataType value, EventDataType height, QString name, QColor color)
        :calc(calc), value(value), height(height), name(name), color(color) {
//        QLinearGradient gradient(0, 0, 1, 0);
//        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
//        gradient.setColorAt(0,color);
//        gradient.setColorAt(1,brighten(color));
//        brush = QBrush(gradient);
    }
    SummaryCalcItem * calc;
    EventDataType value;
    EventDataType height;
    QString name;
    QColor color;
//    QBrush brush;
};

class gSummaryChart : public Layer
{
public:
    gSummaryChart(QString label, MachineType machtype);
    gSummaryChart(ChannelID code, MachineType machtype);
    virtual ~gSummaryChart();

    //! \brief Renders the graph to the QPainter object
    virtual void paint(QPainter &, gGraph &, const QRegion &);

    //! \brief Called whenever data model changes underneath. Day object is not needed here, it's just here for Layer compatability.
    virtual void SetDay(Day *day = nullptr);

    //! \brief Returns true if no data was found for this day during SetDay
    virtual bool isEmpty() { return m_empty; }

    virtual void populate(Day *, int idx);

    //! \brief Override to setup custom stuff before main loop
    virtual void preCalc();

    //! \brief Override to call stuff in main loop
    virtual void customCalc(Day *, QVector<SummaryChartSlice> &);

    //! \brief Override to call stuff after draw is complete
    virtual void afterDraw(QPainter &, gGraph &, QRectF);

    //! \brief Return any extra data to show beneath the date in the hover over tooltip
    virtual QString tooltipData(Day *, int);

    virtual void dataChanged() {
        cache.clear();
    }


    void addCalc(ChannelID code, SummaryType type, QColor color) {
        calcitems.append(SummaryCalcItem(code, type, color));
    }
    void addCalc(ChannelID code, SummaryType type) {
        calcitems.append(SummaryCalcItem(code, type, schema::channel[code].defaultColor()));
    }

    virtual Layer * Clone() {
        gSummaryChart * sc = new gSummaryChart(m_label, m_machtype);
        Layer::CloneInto(sc);
        CloneInto(sc);

        // copy this here, because only base summary charts need it
        sc->calcitems = calcitems;

        return sc;
    }

    void CloneInto(gSummaryChart * layer) {
        layer->m_empty = m_empty;
        layer->firstday = firstday;
        layer->lastday = lastday;
        layer->expected_slices = expected_slices;
        layer->nousedays = nousedays;
        layer->totaldays = totaldays;
        layer->peak_value = peak_value;
        layer->idx_start = idx_start;
        layer->idx_end = idx_end;
        layer->cache.clear();
        layer->dayindex = dayindex;
        layer->daylist = daylist;
    }

protected:
    //! \brief Key was pressed that effects this layer
    virtual bool keyPressEvent(QKeyEvent *event, gGraph *graph);

    //! \brief Mouse moved over this layers area (shows the hover-over tooltips here)
    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);

    //! \brief Mouse Button was pressed over this area
    virtual bool mousePressEvent(QMouseEvent *event, gGraph *graph);

    //! \brief Mouse Button was released over this area. (jumps to daily view here)
    virtual bool mouseReleaseEvent(QMouseEvent *event, gGraph *graph);

    QString m_label;
    MachineType m_machtype;
    bool m_empty;
    int hl_day;
    int tz_offset;
    float tz_hours;
    QDate firstday;
    QDate lastday;

    QMap<QDate, int> dayindex;
    QList<Day *> daylist;

    QHash<int, QVector<SummaryChartSlice> > cache;
    QVector<SummaryCalcItem> calcitems;

    int expected_slices;

    int nousedays;
    int totaldays;

    EventDataType peak_value;
    EventDataType min_value;

    int idx_start;
    int idx_end;

    short midcalc;
};


/*! \class gSessionTimesChart
    \brief Displays a summary of session times
    */
class gSessionTimesChart : public gSummaryChart
{
public:
    gSessionTimesChart()
        :gSummaryChart("SessionTimes", MT_CPAP) {
        addCalc(NoChannel, ST_SESSIONS, QColor(64,128,255));
        addCalc(NoChannel, ST_SESSIONS, QColor(64,128,255));
        addCalc(NoChannel, ST_SESSIONS, QColor(64,128,255));
    }
    virtual ~gSessionTimesChart() {}

    virtual void SetDay(Day * day = nullptr) {
        gSummaryChart::SetDay(day);
        split = p_profile->session->daySplitTime();

        m_miny = 0;
        m_maxy = 28;
    }

    virtual void preCalc();
    virtual void customCalc(Day *, QVector<SummaryChartSlice> & slices);
    virtual void afterDraw(QPainter &, gGraph &, QRectF);

    //! \brief Renders the graph to the QPainter object
    virtual void paint(QPainter &painter, gGraph &graph, const QRegion &region);

    virtual Layer * Clone() {
        gSessionTimesChart * sc = new gSessionTimesChart();
        gSummaryChart::CloneInto(sc);
        CloneInto(sc);
        return sc;
    }

    void CloneInto(gSessionTimesChart * layer) {
        layer->split = split;
    }
    QTime split;
    int num_slices;
    int num_days;
    int total_slices;
    double total_length;
    QList<float> session_data;

};


class gUsageChart : public gSummaryChart
{
public:
    gUsageChart()
        :gSummaryChart("Usage", MT_CPAP) {
        addCalc(NoChannel, ST_HOURS, QColor(64,128,255));
    }
    virtual ~gUsageChart() {}

    virtual void preCalc();
    virtual void customCalc(Day *, QVector<SummaryChartSlice> &);
    virtual void afterDraw(QPainter &, gGraph &, QRectF);
    virtual void populate(Day *day, int idx);

    virtual QString tooltipData(Day * day, int);


    virtual Layer * Clone() {
        gUsageChart * sc = new gUsageChart();
        gSummaryChart::CloneInto(sc);
        CloneInto(sc);
        return sc;
    }

    void CloneInto(gUsageChart * layer) {
        layer->incompdays = incompdays;
        layer->compliance_threshold = compliance_threshold;
    }

private:
    int incompdays;
    EventDataType compliance_threshold;
};

class gTTIAChart : public gSummaryChart
{
public:
    gTTIAChart()
        :gSummaryChart("TTIA", MT_CPAP) {
        addCalc(NoChannel, ST_CNT, QColor(255,147,150));
    }
    virtual ~gTTIAChart() {}

    virtual void preCalc();
    virtual void customCalc(Day *, QVector<SummaryChartSlice> &);
    virtual void afterDraw(QPainter &, gGraph &, QRectF);
    virtual void populate(Day *day, int idx);
    virtual QString tooltipData(Day * day, int);

    virtual Layer * Clone() {
        gTTIAChart * sc = new gTTIAChart();
        gSummaryChart::CloneInto(sc);
        CloneInto(sc);
        return sc;
    }

    void CloneInto(gTTIAChart * /* layer*/) {
    }

private:
};

class gAHIChart : public gSummaryChart
{
public:
    gAHIChart()
        :gSummaryChart("AHIChart", MT_CPAP) {
        addCalc(CPAP_ClearAirway, ST_CPH);
        addCalc(CPAP_Obstructive, ST_CPH);
        addCalc(CPAP_Apnea, ST_CPH);
        addCalc(CPAP_Hypopnea, ST_CPH);
        if (p_profile->general->calculateRDI())
            addCalc(CPAP_RERA, ST_CPH);
    }
    virtual ~gAHIChart() {}

    virtual void preCalc();
    virtual void customCalc(Day *, QVector<SummaryChartSlice> &);
    virtual void afterDraw(QPainter &, gGraph &, QRectF);

    virtual void populate(Day *, int idx);

    virtual QString tooltipData(Day * day, int);

    virtual Layer * Clone() {
        gAHIChart * sc = new gAHIChart();
        gSummaryChart::CloneInto(sc);
        CloneInto(sc);
        return sc;
    }

    void CloneInto(gAHIChart * /* layer */) {
//        layer->ahicalc = ahicalc;
//        layer->ahi_wavg = ahi_wavg;
//        layer->ahi_avg = ahi_avg;
//        layer->total_hours = total_hours;
//        layer->max_ahi = max_ahi;
//        layer->min_ahi = min_ahi;
//        layer->total_days = total_days;
//        layer->ahi_data = ahi_data;
    }

  //  SummaryCalcItem ahicalc;
    double ahi_wavg;
    double ahi_avg;

    double total_hours;
    float max_ahi;
    float min_ahi;

    int total_days;
    QList<float> ahi_data;
};


class gPressureChart : public gSummaryChart
{
public:
    gPressureChart();
    virtual ~gPressureChart() {}

    virtual Layer * Clone() {
        gPressureChart * sc = new gPressureChart();
        gSummaryChart::CloneInto(sc);
        return sc;
    }

//    virtual void preCalc();
    virtual void customCalc(Day *day, QVector<SummaryChartSlice> &slices) {
        int size = slices.size();
        float hour = day->hours(m_machtype);
        for (int i=0; i < size; ++i) {
            SummaryChartSlice & slice = slices[i];
            SummaryCalcItem * calc = slices[i].calc;

            calc->update(slice.value, hour);
         }
    }
    virtual void afterDraw(QPainter &, gGraph &, QRectF);

    virtual void populate(Day * day, int idx);

    virtual QString tooltipData(Day * day, int idx) {
        return day->getCPAPMode() + "\n" + day->getPressureSettings() + gSummaryChart::tooltipData(day, idx);
    }

};

#endif // GSESSIONTIMESCHART_H
