/* Custom CPAP/Oximetry Calculations Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef CALCS_H
#define CALCS_H

#include "day.h"

//! param samples Number of samples
//! width number of surrounding samples to consider
//! percentile fractional percentage, between 0 and 1
void percentileFilter(EventDataType *input, EventDataType *output, int samples, int width,
                      EventDataType percentile);
void xpassFilter(EventDataType *input, EventDataType *output, int samples, EventDataType weight);

enum FilterType { FilterNone = 0, FilterPercentile, FilterXPass };

struct Filter {
    Filter(FilterType t, EventDataType p1, EventDataType p2, EventDataType p3) {
        type = t;
        param1 = p1;
        param2 = p2;
        param3 = p3;
    }
    Filter() {
        type = FilterNone;
        param1 = 0;
        param2 = 0;
        param3 = 0;
    }
    Filter(const Filter &copy) {
        type = copy.type;
        param1 = copy.param1;
        param2 = copy.param2;
        param3 = copy.param3;
    }

    FilterType type;
    EventDataType param1;
    EventDataType param2;
    EventDataType param3;
};

struct BreathPeak {
    BreathPeak() { min = 0; max = 0; start = 0; middle = 0; end = 0; } // peakmin=0; peakmax=0;  }
    BreathPeak(EventDataType _min, EventDataType _max, qint32 _start, qint32 _middle,
               qint32 _end) {//, qint64 _peakmin, qint64 _peakmax) {
        min = _min;
        max = _max;
        start = _start;
        middle = _middle;
        end = _end;
        //peakmax=_peakmax;
        //peakmin=_peakmin;
    }
    BreathPeak(const BreathPeak &copy) {
        min = copy.min;
        max = copy.max;
        start = copy.start;
        middle = copy.middle;
        end = copy.end;
        //peakmin=copy.peakmin;
        //peakmax=copy.peakmax;
    }
    int samplelength() { return end - start; }
    int upperLength() { return middle - start; }
    int lowerLength() { return end - middle; }

    EventDataType min;   // peak value
    EventDataType max;   // peak value
    qint32 start;       // beginning zero cross
    qint32 middle;         // ending zero cross
    qint32 end;         // ending zero cross
    //qint64 peakmin;     // min peak index
    //qint64 peakmax;     // max peak index
};

bool operator<(const BreathPeak &p1, const BreathPeak &p2);

const int num_filter_buffers = 2;

const int max_filter_buf_size = 2097152 * sizeof(EventDataType);

//! \brief Class to process Flow Rate waveform data
class FlowParser
{
  public:
    FlowParser();
    ~FlowParser();

    //! \brief Clears the (input) filter chain
    void clearFilters();

    //! \brief Applies the filter chain to input, with supplied number of samples
    EventDataType *applyFilters(EventDataType *input, int samples);

    //! \brief Add the filter
    void addFilter(FilterType ft, EventDataType p1 = 0, EventDataType p2 = 0, EventDataType p3 = 0) {
        m_filters.push_back(Filter(ft, p1, p2, p3));
    }
    //! \brief Opens the flow rate EventList, applies the input filter chain, and calculates peaks
    void openFlow(Session *session, EventList *flow);

    //! \brief Calculates the upper and lower breath peaks
    void calcPeaks(EventDataType *input, int samples);

    // Minute vent needs Resp & TV calcs made here..
    void calc(bool calcResp, bool calcTv, bool calcTi, bool calcTe, bool calcMv);
    void flagEvents();
    void flagUserEvents(ChannelID code, EventDataType restriction, EventDataType duration);

    /*void calcTidalVolume();
    void calcRespRate();
    void calcMinuteVent(); */


    QList<Filter> m_filters;
  protected:
    QVector<BreathPeak> breaths;

    int m_samples;
    EventList *m_flow;
    Session *m_session;
    EventDataType m_gain;
    EventDataType m_rate;
    EventDataType m_minutes;
    //! \brief The filtered waveform
    EventDataType *m_filtered;
    //! \brief BreathPeak's start on positive cycle?
    bool m_startsUpper;
  private:
    EventDataType *m_buffers[num_filter_buffers];
};

bool SearchApnea(Session *session, qint64 time, double dur);

//! \brief Calculate Respiratory Rate, Tidal Volume & Minute Ventilation for PRS1 data
void calcRespRate(Session *session, FlowParser *flowparser = nullptr);

//! \brief Calculates the sliding window AHI graph
int calcAHIGraph(Session *session);

//! \brief Calculates AHI for a session between start & end (a support function for the sliding window graph)
EventDataType calcAHI(Session *session, qint64 start = -1, qint64 end = -1);

//! \brief Scans for leaks over Redline and flags as large leaks, unless machine provided them already
void flagLargeLeaks(Session *session);

//! \brief Leaks calculations for PRS1
int calcLeaks(Session *session);

//! \brief Calculate Pulse change flagging, according to preferences
int calcPulseChange(Session *session);

//! \brief Calculate SPO2 Drop flagging, according to preferences
int calcSPO2Drop(Session *session);


#endif // CALCS_H
