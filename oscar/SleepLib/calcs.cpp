/* Custom CPAP/Oximetry Calculations Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QMutex>
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <cmath>
#include <algorithm>

#include "calcs.h"
#include "profiles.h"

bool SearchEvent(Session * session, ChannelID code, qint64 time, int dur, bool update=true)
{
    qint64 t, start;
    auto it = session->eventlist.find(code);
    quint32 *tptr;

    EventStoreType *dptr;

    int cnt;

    //qint64 rate;
//    bool fixdurations = (session->machine()->loaderName() != STR_MACH_ResMed);

    if (!p_profile->cpap->resyncFromUserFlagging()) {
        update=false;
    }

    auto evend = session->eventlist.end();
    if (it != evend) {
        for (const auto & el : it.value()) {
            //rate=el->rate();
            cnt = el->count();

            // why would this be necessary???
            if (el->type() == EVL_Waveform) {
                qDebug() << "Called SearchEvent on a waveform object!";
                return false;
            } else {
                start = el->first();
                tptr = el->rawTime();
                dptr = el->rawData();

                for (int j = 0; j < cnt; j++) {
                    t = start + *tptr;

                    // Move the position and set the duration
                    qint64 end1 = time + 5000L;
                    qint64 start1 = end1 - quint64(dur+10)*1000L;

                    qint64 end2 = t + 5000L;
                    qint64 start2 = end2 - quint64(*dptr+10)*1000L;

                    bool overlap = (start1 <= end2) && (start2 <= end1);

                    if (overlap) {
                        if (update) {
                            qint32 delta = time-start;
                            if (delta >= 0) {
                                *tptr = delta;
                                *dptr = (EventStoreType)dur;
                            }
                        }
                        return true;
                    }
                    tptr++;
                    dptr++;
                }
            }
        }
    }

    return false;
}

bool SearchApnea(Session *session, qint64 time, double dur)
{
    if (SearchEvent(session, CPAP_Obstructive, time, dur))
        return true;

    if (SearchEvent(session, CPAP_Apnea, time, dur))
        return true;

    if (SearchEvent(session, CPAP_ClearAirway, time, dur))
        return true;

    if (SearchEvent(session, CPAP_Hypopnea, time, dur))
        return true;

    if (SearchEvent(session, CPAP_UserFlag1, time, dur, false))
        return true;

    if (SearchEvent(session, CPAP_UserFlag2, time, dur, false))
        return true;

    return false;
}

// Sort BreathPeak by peak index
bool operator<(const BreathPeak &p1, const BreathPeak &p2)
{
    return p1.start < p2.start;
}

//! \brief Filters input to output with a percentile filter with supplied width.
//! \param samples Number of samples
//! \param width number of surrounding samples to consider
//! \param percentile fractional percentage, between 0 and 1
void percentileFilter(EventDataType *input, EventDataType *output, int samples, int width,
                      EventDataType percentile)
{
    if (samples <= 0) {
        return;
    }

    if (percentile > 1) {
        percentile = 1;
    }

    QVector<EventDataType> buf(width);
    int s, e;
    int z1 = width / 2;
    int z2 = z1 + (width % 2);
    int nm1 = samples - 1;
    //int j;

    // Scan through all of input
    for (int k = 0; k < samples; k++) {

        s = k - z1;
        e = k + z2;

        // Cap bounds
        if (s < 0) { s = 0; }

        if (e > nm1) { e = nm1; }

        //
        int j = 0;

        for (int i = s; i < e; i++) {
            buf[j++] = input[i];
        }

        j--;

        EventDataType val = j * percentile;
        EventDataType fl = floor(val);

        // If even percentile, or already max value..
        if ((val == fl) || (j >= width - 1)) {
            nth_element(buf.begin(), buf.begin() + j, buf.begin() + width - 1);
            val = buf[j];
        } else {
            // Percentile lies between two points, interpolate.
            double v1, v2;
            nth_element(buf.begin(), buf.begin() + j, buf.begin() + width - 1);
            v1 = buf[j];
            nth_element(buf.begin(), buf.begin() + j + 1, buf.begin() + width - 1);
            v2 = buf[j + 1];

            val = v1 + (v2 - v1) * (val - fl);
        }

        output[k] = val;
    }
}

void xpassFilter(EventDataType *input, EventDataType *output, int samples, EventDataType weight)
{
    // prime the first value
    output[0] = input[0];

    for (int i = 1; i < samples; i++) {
        output[i] = weight * input[i] + (1.0 - weight) * output[i - 1];
    }

    //output[samples-1]=input[samples-1];
}

FlowParser::FlowParser()
{
    m_session = nullptr;
    m_flow = nullptr;
    m_filtered = nullptr;
    m_gain = 1;
    m_samples = 0;
    m_startsUpper = true;

    // Allocate filter chain buffers..
    m_filtered = (EventDataType *) malloc(max_filter_buf_size);
}
FlowParser::~FlowParser()
{
    free(m_filtered);
    //    for (int i=0;i<num_filter_buffers;i++) {
    //        free(m_buffers[i]);
    //    }
}
void FlowParser::clearFilters()
{
    m_filters.clear();
}

EventDataType *FlowParser::applyFilters(EventDataType *data, int samples)
{
    EventDataType *in = nullptr, *out = nullptr;

    if (m_filters.size() == 0) {
        //qDebug() << "Trying to apply empty filter list in FlowParser..";
        return nullptr;
    }

    for (int i = 0; i < num_filter_buffers; i++) {
        m_buffers[i] = (EventDataType *) malloc(max_filter_buf_size);
    }

    int numfilt = m_filters.size();

    for (int i = 0; i < numfilt; i++) {
        if (i == 0) {
            in = data;
            out = m_buffers[0];

            if (in == out) {
                //qDebug() << "Error: If you need to use internal m_buffers as initial input, use the second one. No filters were applied";
                return nullptr;
            }
        } else {
            in = m_buffers[(i + 1) % num_filter_buffers];
            out = m_buffers[i % num_filter_buffers];
        }

        // If final link in chain, pass it back out to input data
        if (i == numfilt - 1) {
            out = data;
        }

        Filter &filter = m_filters[i];

        if (filter.type == FilterNone) {
            // Just copy it..
            memcpy(in, out, samples * sizeof(EventDataType));
        } else if (filter.type == FilterPercentile) {
            percentileFilter(in, out, samples, filter.param1, filter.param2);
        } else if (filter.type == FilterXPass) {
            xpassFilter(in, out, samples, filter.param1);
        }
    }

    for (int i = 0; i < num_filter_buffers; i++) {
        free(m_buffers[i]);
    }

    return out;
}

// Opens the flow rate EventList, applies the input filter chain, and calculates peaks
void FlowParser::openFlow(Session *session, EventList *flow)
{
    if (!flow) {
        qDebug() << "called FlowParser::processFlow() with a null EventList!";
        return;
    }

    m_session = session;
    m_flow = flow;
    m_gain = flow->gain();
    m_rate = flow->rate();
    m_samples = flow->count();
    EventStoreType *inraw = flow->rawData();

    // Make sure we won't overflow internal buffers
    if (m_samples > max_filter_buf_size) {
        qDebug() << "Error: Sample size exceeds max_filter_buf_size in FlowParser::openFlow().. Capping!!!";
        m_samples = max_filter_buf_size;
    }

    // Begin with the second internal buffer
    EventDataType *buf = m_filtered;
    // Apply gain to waveform
    EventStoreType *eptr = inraw + m_samples;

    // Convert from store type to floats..
    for (; inraw < eptr; ++inraw) {
        *buf++ = EventDataType(*inraw) * m_gain;
    }

    // Apply the rest of the filters chain
    buf = applyFilters(m_filtered, m_samples);
    Q_UNUSED(buf)


    // Scan for and create an index of each breath
    calcPeaks(m_filtered, m_samples);
}

// Calculates breath upper & lower peaks for a chunk of EventList data
void FlowParser::calcPeaks(EventDataType *input, int samples)
{
    if (samples <= 0) {
        return;
    }

    EventDataType min = 0, max = 0, c, lastc = 0;

    EventDataType zeroline = 0;

    double rate = m_flow->rate();

    double flowstart = m_flow->first();
    double time; //, lasttime;

    //double peakmax = flowstart,
    //double peakmin = flowstart;

    // lasttime =
    time = flowstart;
    breaths.clear();

    // Estimate storage space needed using typical average breaths per minute.
    m_minutes = double(m_flow->last() - m_flow->first()) / 60000.0;

    const double avgbpm = 20; // average breaths per minute of a standard human
    int guestimate = m_minutes * avgbpm;

    // reserve some memory
    breaths.reserve(guestimate);

    // Prime min & max, and see which side of the zero line we are starting from.
    c = input[0];
    min = max = c;
    lastc = c;
    m_startsUpper = (c >= zeroline);

    qint32 start = 0, middle = 0;


    int sps = 1000 / m_rate;
    int len = 0;
    //int lastk = 0;

    //qint64 sttime = time;

    // For each samples, find the peak upper and lower breath components
    for (int k = 0; k < samples; k++) {
        c = input[k];

        if (c >= zeroline) {

            // Did we just cross the zero line going up?
            if (lastc < zeroline) {
                // This helps filter out dirty breaths..
                len = k - start;

                if ((max > 3) && ((max - min) > 8) && (len > sps) && (middle > start))  {

                    // peak detection may not be needed..
                    breaths.push_back(BreathPeak(min, max, start, middle, k));

                    // Set max for start of the upper breath cycle
                    max = c;
                    //peakmax = time;

                    // Starting point of next breath cycle
                    start = k;
                    //sttime = time;
                }
            } else if (c > max) {
                // Update upper breath peak
                max = c;
                //peakmax = time;
            }
        }

        if (c < zeroline) {
            // Did we just cross the zero line going down?

            if (lastc >= zeroline) {
                // Set min for start of the lower breath cycle
                min = c;
                //peakmin = time;
                middle = k;

            } else if (c < min) {
                // Update lower breath peak
                min = c;
                //peakmin = time;
            }

        }

        //lasttime = time;
        time += rate;
        lastc = c;
        //lastk = k;
    }
}


//! \brief Calculate Respiratory Rate, TidalVolume, Minute Ventilation, Ti & Te..
// These are grouped together because, a) it's faster, and b) some of these calculations rely on others.
void FlowParser::calc(bool calcResp, bool calcTv, bool calcTi, bool calcTe, bool calcMv)
{
    if (!m_session) {
        return;
    }

    // Don't even bother if only a few breaths in this chunk
    const int lowthresh = 4;
    int nm = breaths.size();

    if (nm < lowthresh) {
        return;
    }

    const qint64 minute = 60000;


    double start = m_flow->first();
    double time = start;

    int bs, be, bm;
    double st, et=0, mt;

    /////////////////////////////////////////////////////////////////////////////////
    // Respiratory Rate setup
    /////////////////////////////////////////////////////////////////////////////////
    EventDataType minrr = 0, maxrr = 0;
    EventList *RR = nullptr;
    quint32 *rr_tptr = nullptr;
    EventStoreType *rr_dptr = nullptr;

    if (calcResp) {
        RR = m_session->AddEventList(CPAP_RespRate, EVL_Event);
        minrr = RR->Min(), maxrr = RR->Max();
        RR->setGain(0.2F);
        RR->setFirst(time + minute);
        RR->getData().resize(nm);
        RR->getTime().resize(nm);
        rr_tptr = RR->rawTime();
        rr_dptr = RR->rawData();
    }

    int rr_count = 0;

    double len, st2, et2, adj, stmin, b, rr = 0;
    double len2;

    /////////////////////////////////////////////////////////////////////////////////
    // Inspiratory / Expiratory Time setup
    /////////////////////////////////////////////////////////////////////////////////
    double lastte2 = 0, lastti2 = 0, lastte = 0, lastti = 0, te, ti, ti1, te1, c;
    EventList *Te = nullptr, * Ti = nullptr;

    if (calcTi) {
        Ti = m_session->AddEventList(CPAP_Ti, EVL_Event);
        Ti->setGain(0.02F);
    }

    if (calcTe) {
        Te = m_session->AddEventList(CPAP_Te, EVL_Event);
        Te->setGain(0.02F);
    }


    /////////////////////////////////////////////////////////////////////////////////
    // Tidal Volume setup
    /////////////////////////////////////////////////////////////////////////////////
    EventList *TV = nullptr;
    EventDataType mintv = 0, maxtv = 0, tv = 0;
    double val1, val2;
    quint32 *tv_tptr = nullptr;
    EventStoreType *tv_dptr = nullptr;
    int tv_count = 0;

    if (calcTv) {
        TV = m_session->AddEventList(CPAP_TidalVolume, EVL_Event);
        mintv = TV->Min(), maxtv = TV->Max();
        TV->setGain(20);
        TV->setFirst(start);
        TV->getData().resize(nm);
        TV->getTime().resize(nm);
        tv_tptr = TV->rawTime();
        tv_dptr = TV->rawData();
    }

    /////////////////////////////////////////////////////////////////////////////////
    // Minute Ventilation setup
    /////////////////////////////////////////////////////////////////////////////////
    EventList *MV = nullptr;
    EventDataType mv;

    if (calcMv) {
        MV = m_session->AddEventList(CPAP_MinuteVent, EVL_Event);
        MV->setGain(0.125); // gain set to 1/8th
    }

    EventDataType sps = (1000.0 / m_rate); // Samples Per Second


    qint32 timeval = 0; // Time relative to start


    BreathPeak * bpstr = breaths.data();
    BreathPeak * bpend = bpstr + nm;

    // For each breath...
    for (BreathPeak * bp = bpstr; bp != bpend; ++bp) {
        bs = bp->start;
        bm = bp->middle;
        be = bp->end;

        // Calculate start, middle and end time of this breath
        st = start + bs * m_rate;
        mt = start + bm * m_rate;

        timeval = be * m_rate;

        et = start + timeval;

        /////////////////////////////////////////////////////////////////////
        // Calculate Inspiratory Time (Ti) for this breath
        /////////////////////////////////////////////////////////////////////
        if (calcTi) {
            // Ti is simply the time between the start of a breath and it's midpoint

            // Note the 50.0 is the gain value, to give it better resolution
            // (and mt and st are in milliseconds)
            ti = ((mt - st) / 1000.0) * 50.0;

            // Average for a little smoothing
            ti1 = (lastti2 + lastti + ti * 2) / 4.0;

            Ti->AddEvent(mt, ti1);   // Add the event

            // Track the last two values to use for averaging
            lastti2 = lastti;
            lastti = ti;
        }

        /////////////////////////////////////////////////////////////////////
        // Calculate Expiratory Time (Te) for this breath
        /////////////////////////////////////////////////////////////////////
        if (calcTe) {
            // Te is simply the time between the second half of the breath
            te = ((et - mt) / 1000.0) * 50.0;

            // Average last three values..
            te1 = (lastte2 + lastte + te * 2 ) / 4.0;
            Te->AddEvent(mt, te1);

            lastte2 = lastte;
            lastte = te;
        }

        /////////////////////////////////////////////////////////////////////
        // Calculate TidalVolume
        /////////////////////////////////////////////////////////////////////
        if (calcTv) {
            val1 = 0, val2 = 0;

            // Scan the upper breath
            for (int j = bs; j < bm; j++)  {
                // convert flow to ml/s to L/min and divide by samples per second
                c = double(qAbs(m_filtered[j])) * 1000.0 / 60.0 / sps;
                val2 += c;
                //val2+=c*c; // for RMS
            }
            tv = val2;

            bool usebothhalves = false;
            if (usebothhalves) {
                for (int j = bm; j < be; j++)  {
                    // convert flow to ml/s to L/min and divide by samples per second
                    c = double(qAbs(m_filtered[j])) * 1000.0 / 60.0 / sps;
                    val1 += c;
                    //val1 += c*c; // for RMS
                }
                tv = (qAbs(val2) + qAbs(val1)) / 2;
            }

            // Add the other half here and average might make it more accurate
            // But last time I tried it was pretty messy

            // Perhaps needs a bit of history averaging..

            // calculate root mean square
            //double n=bm-bs;
            //double q=(1/n)*val2;
            //double x=sqrt(q)*2;
            //val2=x;

            if (tv < mintv) { mintv = tv; }

            if (tv > maxtv) { maxtv = tv; }

            *tv_tptr++ = timeval;
            *tv_dptr++ = tv / 20.0;
            tv_count++;

        }

        /////////////////////////////////////////////////////////////////////
        // Respiratory Rate Calculations
        /////////////////////////////////////////////////////////////////////
        if (calcResp) {
            stmin = et - minute;

            if (stmin < start) {
                stmin = start;
            }

            //len = et - stmin;
            rr = 0;
            len2 = 0;

            // Step back through last minute and count breaths
            BreathPeak *bpstr1 = bpstr-1;
            for (BreathPeak *p = bp; p != bpstr1; --p) {
                st2 = start + double(p->start) * m_rate;
                et2 = start + double(p->end) * m_rate;

                if (et2 < stmin) {
                    break;
                }

                len = et2 - st2;

                if (st2 < stmin) {
                    // Partial breath
                    st2 = stmin;
                    adj = et2 - st2;
                    b = (1.0 / len) * adj;
                    len2 += adj;
                } else {
                    b = 1;
                    len2 += len;
                }

                rr += b;
            }

            if (len2 < minute) {
                rr *= minute / len2;
            }

            // Calculate min & max
            if (rr < minrr) {
                minrr = rr;
            }

            if (rr > maxrr) {
                maxrr = rr;
            }

            // Add manually.. (much quicker)
            *rr_tptr++ = timeval;

            // Use the same gains as ResMed..

            *rr_dptr++ = rr * 5.0;
            rr_count++;
        }

        if (calcMv && calcResp && calcTv) {
            // Minute Ventilation is tidal volume times respiratory rate
            mv = (tv / 1000.0) * rr;

            // The 8.0 is the gain of the MV EventList to boost resolution
            MV->AddEvent(et, mv * 8.0);
        }
    }

    /////////////////////////////////////////////////////////////////////
    // Respiratory Rate post filtering
    /////////////////////////////////////////////////////////////////////

    if (calcResp) {
        RR->setMin(minrr);
        RR->setMax(maxrr);
        RR->setFirst(start);
        RR->setLast(et);
        RR->setCount(rr_count);
    }

    /////////////////////////////////////////////////////////////////////
    // Tidal Volume post filtering
    /////////////////////////////////////////////////////////////////////

    if (calcTv) {
        TV->setMin(mintv);
        TV->setMax(maxtv);
        TV->setFirst(start);
        TV->setLast(et);
        TV->setCount(tv_count);
    }
}

void FlowParser::flagUserEvents(ChannelID code, EventDataType restriction, EventDataType duration)
{
    int numbreaths = breaths.size();
    //EventDataType mx, mn;

    QVector<EventDataType> br;
    QVector<qint32> bstart;
    QVector<qint32> bend;

    // Allocate some memory beforehand so it doesn't have to slow it down mid calculations
    bstart.reserve(numbreaths * 2);
    bend.reserve(numbreaths * 2);
    br.reserve(numbreaths * 2);

    double start = m_flow->first();
    double st, et, dur;
    qint64 len;

    bool allowDuplicates = p_profile->cpap->userEventDuplicates();

    // Get the Breath list, which is calculated by the previously run breath marker algorithm.
    BreathPeak *bpstr = breaths.data();
    BreathPeak *bpend = bpstr + numbreaths;

    // Create a list containing the abs of min and max waveform flow for each breath
    for (BreathPeak *p = bpstr; p != bpend; ++p) {
        br.push_back(qAbs(p->max));
        br.push_back(qAbs(p->min));
    }

    // The following ignores the outliers to get a cleaner cutoff value
    // 60th percentile was chosen because it's a little more than median, and, well, reasons I can't remember specifically.. :-}

    // Look for the 60th percentile of the abs'ed min/max values
    const EventDataType perc = 0.6F;
    int idx = float(br.size()) * perc;
    nth_element(br.begin(), br.begin() + idx, br.end() - 1);

    // Take this value as the peak
    EventDataType peak = br[idx]; ;

    // Scale the restriction percentage to the peak to find the cutoff value
    EventDataType cutoffval = peak * (restriction / 100.0F);

    int bs, bm, be, bs1, bm1, be1;

    // For each Breath, search for flow restrictions
    for (BreathPeak *p = bpstr; p != bpend; ++p) {

        // Todo: Define these markers in the comments better
        bs = p->start;   // breath start
        bm = p->middle;  // breath middle
        be = p->end;     // breath end

//        mx = p->max;
//        mn = p->min;

        //val = mx - mn;   // the total height from top to bottom of this breath

        // Scan the breath in the flow data and stop at the first location more than the cutoff value
        // (Only really needs to scan to the middle.. I'm not sure why I made it go all the way to the end.)
        for (bs1 = bs; bs1 < be; bs1++) {
            if (qAbs(m_filtered[bs1]) > cutoffval) {
                break;
            }
        }

        // if bs1 has reached the end, this means the entire marked breath is within the restriction

        // Scan backwards from the middle to the start, stopping at the first value past the cutoff value
        for (bm1 = bm; bm1 > bs; bm1--) {
            if (qAbs(m_filtered[bm1]) > cutoffval) {
                break;
            }
        }

        // Now check if a value was found in the first half of the breath above the cutoff value
        if (bm1 >= bs1) {
            // Good breath... And add it as a beginning/end marker for the next stage
            bstart.push_back(bs1);
            bend.push_back(bm1);
        } // else we crossed over and the first half of the breath is under the threshold, therefore has a flow restricted


        // Now do the other half of the breath....

        // Scan from middle to end of breath, stopping at first cutoff value
        for (bm1 = bm; bm1 < be; bm1++) {
            if (qAbs(m_filtered[bm1]) > cutoffval) {
                break;
            }
        }

        // Scan backwards from the end to the middle of the breath, stopping at the first cutoff value
        for (be1 = be; be1 > bm; be1--) {
            if (qAbs(m_filtered[be1]) > cutoffval) {
                break;
            }
        }

        // Check for crossover again
        if (be1 >= bm1) {
            // Good strong healthy breath.. add the beginning and end to the breath markers.
            bstart.push_back(bm1);
            bend.push_back(be1);
        } // else crossed over again.. breathe damn you!

        // okay, this looks like cruft..
//        st = start + bs1 * m_rate;
//        et = start + be1 * m_rate;
    }

    int bsize = bstart.size();   // Number of breath components over cutoff threshold
    EventList *uf = nullptr;

    // For each good breath marker, look at the gaps in between
    for (int i = 0; i < bsize - 1; i++) {
        bs = bend[i];         // start  at the end of the healthy breath
        be = bstart[i + 1];   // look ahead to the beginning of the next one

        // Calculate the start and end timestamps
        st = start + bs * m_rate;
        et = start + be * m_rate;

        // Calculate the length of the flow restriction
        len = et - st;
        dur = len / 1000.0; // (scale to seconds, not milliseconds)


        if (dur >= duration) { // is the event greater than the duration threshold?

            // Unless duplicates have been specifically allowed, scan for any apnea's already detected by the machine
            if (allowDuplicates || !SearchApnea(m_session, et, dur)) {
                if (!uf) {
                    // Create event list if not already done
                    uf = m_session->AddEventList(code, EVL_Event);
                }

                // Add the user flag at the end
                uf->AddEvent(et, dur);
            }
        }
    }
}

void FlowParser::flagEvents()
{
    if (!p_profile->cpap->userEventFlagging()) { return; }

    int numbreaths = breaths.size();

    if (numbreaths < 5) { return; }

    flagUserEvents(CPAP_UserFlag1, p_profile->cpap->userEventRestriction(), p_profile->cpap->userEventDuration());
    flagUserEvents(CPAP_UserFlag2, p_profile->cpap->userEventRestriction2(), p_profile->cpap->userEventDuration2());
}

void calcRespRate(Session *session, FlowParser *flowparser)
{
    if (session->type() != MT_CPAP) { return; }

    //    if (session->machine()->loaderName() != STR_MACH_PRS1) return;

    if (!session->eventlist.contains(CPAP_FlowRate)) {
        //qDebug() << "calcRespRate called without FlowRate waveform available";
        return; //need flow waveform
    }

    bool trashfp;

    if (!flowparser) {
        flowparser = new FlowParser();
        trashfp = true;
        //qDebug() << "calcRespRate called without valid FlowParser object.. using a slow throw-away!";
        //return;
    } else {
        trashfp = false;
    }

    bool calcResp = !session->eventlist.contains(CPAP_RespRate);
    bool calcTv = !session->eventlist.contains(CPAP_TidalVolume);
    bool calcTi = !session->eventlist.contains(CPAP_Ti);
    bool calcTe = !session->eventlist.contains(CPAP_Te);
    bool calcMv = !session->eventlist.contains(CPAP_MinuteVent);


    int z = (calcResp ? 1 : 0) + (calcTv ? 1 : 0) + (calcMv ? 1 : 0);

    // If any of these three missing, remove all, and switch all on
    if (z > 0 && z < 3) {
        if (!calcResp && !calcTv && !calcMv) {
            calcTv = calcMv = calcResp = true;
        }

        auto & list = session->eventlist[CPAP_RespRate];
        for (auto & l : list) {
            delete l;
        }
        session->eventlist[CPAP_RespRate].clear();

        auto & list2 = session->eventlist[CPAP_TidalVolume];
        for (auto & l2 : list2) {
            delete l2;
        }
        session->eventlist[CPAP_TidalVolume].clear();

        auto & list3 = session->eventlist[CPAP_MinuteVent];
        for (auto & l3 : list3) {
            delete l3;
        }
        session->eventlist[CPAP_MinuteVent].clear();
    }

    flowparser->clearFilters();

    // No filters works rather well with the new peak detection algorithm..
    // Although the output could use filtering.

    //flowparser->addFilter(FilterPercentile,7,0.5);
    //flowparser->addFilter(FilterPercentile,5,0.5);
    //flowparser->addFilter(FilterXPass,0.5);

    auto & EVL = session->eventlist[CPAP_FlowRate];
    for (auto & flow : EVL) {
        if (flow->count() > 20) {
            flowparser->openFlow(session, flow);
            flowparser->calc(calcResp, calcTv, calcTi , calcTe, calcMv);
            flowparser->flagEvents();
        }
    }

    if (trashfp) {
        delete flowparser;
    }
}


EventDataType calcAHI(Session *session, qint64 start, qint64 end)
{
    bool rdi = p_profile->general->calculateRDI();

    double hours, ahi, cnt;

    if (start < 0) {
        // much faster..
        hours = session->hours();
        cnt = session->count(CPAP_Obstructive)
              + session->count(CPAP_Hypopnea)
              + session->count(CPAP_ClearAirway)
              + session->count(CPAP_Apnea);

        if (rdi) {
            cnt += session->count(CPAP_RERA);
        }

        ahi = cnt / hours;
    } else {
        hours = double(end - start) / 3600000L;

        if (hours == 0) { return 0; }

        cnt = session->rangeCount(CPAP_Obstructive, start, end)
              + session->rangeCount(CPAP_Hypopnea, start, end)
              + session->rangeCount(CPAP_ClearAirway, start, end)
              + session->rangeCount(CPAP_Apnea, start, end);

        if (rdi) {
            cnt += session->rangeCount(CPAP_RERA, start, end);
        }

        ahi = cnt / hours;
    }

    return ahi;
}

int calcAHIGraph(Session *session)
{
    bool calcrdi = session->machine()->loaderName() == "PRS1";

    const qint64 window_step = 30000; // 30 second windows
    double window_size = p_profile->cpap->AHIWindow();
    qint64 window_size_ms = window_size * 60000L;

    bool zeroreset = p_profile->cpap->AHIReset();

    if (session->type() != MT_CPAP) { return 0; }

    bool hasahi = session->eventlist.contains(CPAP_AHI);
    bool hasrdi = session->eventlist.contains(CPAP_RDI);

    if (hasahi && hasrdi) {
        return 0;    // abort if already there
    }

    if (!(!hasahi && !hasrdi)) {
        session->destroyEvent(CPAP_AHI);
        session->destroyEvent(CPAP_RDI);
    }

    if (!session->channelExists(CPAP_Obstructive) &&
            !session->channelExists(CPAP_Hypopnea) &&
            !session->channelExists(CPAP_Apnea) &&
            !session->channelExists(CPAP_ClearAirway) &&
            !session->channelExists(CPAP_RERA)
       ) { return 0; }

    qint64 first = session->first(),
           last = session->last(),
           f;

    EventList *AHI = new EventList(EVL_Event);
    AHI->setGain(0.02F);
    session->eventlist[CPAP_AHI].push_back(AHI);

    EventList *RDI = nullptr;

    if (calcrdi) {
        RDI = new EventList(EVL_Event);
        RDI->setGain(0.02F);
        session->eventlist[CPAP_RDI].push_back(RDI);
    }

    EventDataType ahi, rdi;

    qint64 ti = first; //, lastti = first;

    double avgahi = 0;
    double avgrdi = 0;
    int cnt = 0;

    double events;
    double hours = (window_size / 60.0F);

    if (zeroreset) {
        // I personally don't see the point of resetting each hour.
        do {
            // For each window, in 30 second increments
            for (qint64 t = ti; t < ti + window_size_ms; t += window_step) {
                if (t > last) {
                    break;
                }

                events = session->rangeCount(CPAP_Obstructive, ti, t)
                         + session->rangeCount(CPAP_Hypopnea, ti, t)
                         + session->rangeCount(CPAP_ClearAirway, ti, t)
                         + session->rangeCount(CPAP_Apnea, ti, t);

                ahi = events / hours;

                AHI->AddEvent(t, ahi * 50);
                avgahi += ahi;

                if (calcrdi) {
                    events += session->rangeCount(CPAP_RERA, ti, t);
                    rdi = events / hours;
                    RDI->AddEvent(t, rdi * 50);
                    avgrdi += rdi;
                }

                cnt++;
            }

            //lastti = ti;
            ti += window_size_ms;
        } while (ti < last);

    } else {
        for (ti = first; ti < last; ti += window_step) {
            f = ti - window_size_ms;
            //hours=window_size; //double(ti-f)/3600000L;

            events = session->rangeCount(CPAP_Obstructive, f, ti)
                     + session->rangeCount(CPAP_Hypopnea, f, ti)
                     + session->rangeCount(CPAP_ClearAirway, f, ti)
                     + session->rangeCount(CPAP_Apnea, f, ti);

            ahi = events / hours;
            avgahi += ahi;
            AHI->AddEvent(ti, ahi * 50);

            if (calcrdi) {
                events += session->rangeCount(CPAP_RERA, f, ti);
                rdi = events / hours;
                RDI->AddEvent(ti, rdi * 50);
                avgrdi += rdi;
            }

            cnt++;
            //lastti = ti;
            ti += window_step;
        }
    }

    AHI->AddEvent(last, 0);

    if (calcrdi) {
        RDI->AddEvent(last, 0);
    }

    if (!cnt) {
        avgahi = 0;
        avgrdi = 0;
    } else {
        avgahi /= double(cnt);
        avgrdi /= double(cnt);
    }

    cnt++;
    session->setAvg(CPAP_AHI, avgahi);

    if (calcrdi) {
        session->setAvg(CPAP_RDI, avgrdi);
    }

    return cnt;
}
struct TimeValue {
    TimeValue() {
        time = 0;
        value = 0;
    }
    TimeValue(qint64 t, EventStoreType v) {
        time = t;
        value = v;
    }
    TimeValue(const TimeValue &copy) {
        time = copy.time;
        value = copy.value;
    }
    qint64 time;
    EventStoreType value;
};

struct zMaskProfile {
  public:
    zMaskProfile(MaskType type, QString name);
    ~zMaskProfile();

    void reset() { pressureleaks.clear(); }
    void scanLeaks(Session *session);
    void scanPressure(Session *session);
    void updatePressureMin();
    void updateProfile(Session *session);

    void load(Profile *profile);
    void save();

    QMap<EventStoreType, EventDataType> pressuremax;
    QMap<EventStoreType, EventDataType> pressuremin;
    QMap<EventStoreType, EventDataType> pressurecount;
    QMap<EventStoreType, EventDataType> pressuretotal;
    QMap<EventStoreType, EventDataType> pressuremean;
    QMap<EventStoreType, EventDataType> pressurestddev;

    QVector<TimeValue> Pressure;

    EventDataType calcLeak(EventStoreType pressure);

  protected:
    static const quint32 version = 0;
    void scanLeakList(EventList *leak);
    void scanPressureList(Session *session, ChannelID code);

    MaskType    m_type;
    QString     m_name;
    Profile      *m_profile;
    QString     m_filename;

    QMap<EventStoreType, QMap<EventStoreType, quint32> > pressureleaks;
    EventDataType maxP, minP, maxL, minL, m_factor;
};


bool operator<(const TimeValue &p1, const TimeValue &p2)
{
    return p1.time < p2.time;
}

zMaskProfile::zMaskProfile(MaskType type, QString name)
    : m_type(type), m_name(name)
{
}
zMaskProfile::~zMaskProfile()
{
    save();
}

void zMaskProfile::load(Profile *profile)
{
    m_profile = profile;
    m_filename = m_profile->Get("{" + STR_GEN_DataFolder + "}/MaskProfile.mp");
    QFile f(m_filename);

    if (!f.open(QFile::ReadOnly)) {
        return;
    }

    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_4_6);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 m, v;
    in >> m;
    in >> v;

    if (m != magic) {
        qDebug() << "Magic wrong in zMaskProfile::load";
    }

    in >> pressureleaks;
    f.close();
}
void zMaskProfile::save()
{
    if (m_filename.isEmpty()) {
        return;
    }

    QFile f(m_filename);

    if (!f.open(QFile::WriteOnly)) {
        return;
    }

    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_4_6);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)magic;
    out << (quint32)version;

    out << pressureleaks;
    f.close();
}

void zMaskProfile::scanPressureList(Session *session, ChannelID code)
{
    auto it = session->eventlist.find(code);
    if (it == session->eventlist.end()) return;

    int prescnt = session->count(code);
    Pressure.reserve(Pressure.size() + prescnt);

    auto & EVL = it.value();

    for (const auto & el : EVL) {
        qint64 start = el->first();
        int count = el->count();
        EventStoreType *dptr = el->rawData();
        EventStoreType *eptr = dptr + count;
        quint32 *tptr = el->rawTime();
        qint64 time;
        EventStoreType pressure;

        for (; dptr < eptr; dptr++) {
            time = start + *tptr++;
            pressure = *dptr;
            Pressure.push_back(TimeValue(time, pressure));
        }
    }
}
void zMaskProfile::scanPressure(Session *session)
{
    Pressure.clear();

    scanPressureList(session, CPAP_Pressure);
    scanPressureList(session, CPAP_IPAP);

    // Sort by time
    std::sort(Pressure.begin(), Pressure.end());
}
void zMaskProfile::scanLeakList(EventList *el)
{
    // Scans through Leak list, and builds a histogram of each leak value for each pressure.

    qint64 start = el->first();
    int count = el->count();
    EventStoreType *dptr = el->rawData();
    EventStoreType *eptr = dptr + count;
    quint32 *tptr = el->rawTime();
    //EventDataType gain=el->gain();

    EventStoreType pressure, leak;

    //EventDataType fleak;
    qint64 ti;
    bool found;

    int psize = Pressure.size();
    if (psize == 0) return;
    TimeValue *tvstr = Pressure.data();
    TimeValue *tvend = tvstr + (psize - 1);
    TimeValue *p1, *p2;

    // Scan through each leak item in event list
    for (; dptr < eptr; dptr++) {
        leak = *dptr;
        ti = start + *tptr++;

        found = false;
        pressure = Pressure[0].value;

        if (psize > 1) {
            // Scan through pressure list to find pressure at this particular leak time
            for (p1 = tvstr; p1 != tvend; ++p1) {
                p2 = p1+1;

                if ((p2->time > ti) && (p1->time <= ti)) {
                    // leak within current pressure range
                    pressure = p1->value;
                    found = true;
                    break;
                } else if (p2->time == ti) {
                    // can't remember why a added this condition...
                    pressure = p2->value;
                    found = true;
                    break;
                }


//            for (int i = 0; i < Pressure.size() - 1; i++) {
//                const TimeValue &p1 = Pressure[i];
//                const TimeValue &p2 = Pressure[i + 1];

//                if ((p2.time > ti) && (p1.time <= ti)) {
//                    pressure = p1.value;
//                    found = true;
//                    break;
//                } else if (p2.time == ti) {
//                    pressure = p2.value;
//                    found = true;
//                    break;
//                }
            }
        } else {
            // were talking CPAP here with no ramp..
            found = true;
        }

        if (found) {
            // update the histogram of leak values for this pressure
            pressureleaks[pressure][leak]++;
            //            pmin=pressuremin.find(pressure);
            //            fleak=EventDataType(leak) * gain;
            //            if (pmin==pressuremin.end()) {
            //                pressuremin[pressure]=fleak;
            //                pressurecount[pressure]=pressureleaks[pressure][leak];
            //            } else {
            //                if (pmin.value() > fleak) {
            //                    pmin.value()=fleak;
            //                    pressurecount[pressure]=pressureleaks[pressure][leak];
            //                }
            //            }
        }
    }


}
void zMaskProfile::scanLeaks(Session *session)
{
    auto ELV = session->eventlist.find(CPAP_LeakTotal);
    if (ELV == session->eventlist.end()) return;

    auto & elv = ELV.value();

    for (auto & it : elv) {
        scanLeakList(it);
    }
}
void zMaskProfile::updatePressureMin()
{
    EventStoreType pressure;
    //EventDataType perc=0.1;
    //EventDataType tmp,l1,l2;
    //int idx1, idx2,
    //int lks;
    double SN;
    double percentile = 0.40;

    double p = 100.0 * percentile;
    double nth, nthi;

    int sum1, sum2, w1, w2, N, k;

    // Calculate a weighted percentile of all leak values contained within each pressure, using pressureleaks histogram
    for (auto it = pressureleaks.begin(), plend=pressureleaks.end(); it != plend; it++) {
        pressure = it.key();
        QMap<EventStoreType, quint32> &leakmap = it.value();
        //lks = leakmap.size();
        SN = 0;

        auto lmend = leakmap.end();
        // First sum total counts of all leaks
        for (auto lit = leakmap.begin(); lit != lmend; ++lit) {
            SN += lit.value();
        }


        nth = double(SN) * percentile; // index of the position in the unweighted set would be
        nthi = floor(nth);

        sum1 = 0, sum2 = 0;
        w1 = 0, w2 = 0;

        EventDataType v1 = 0, v2;

        N = leakmap.size();
        k = 0;

        bool found = false;
        double total = 0;

        // why do this effectively twice? and k = size
        for (auto lit = leakmap.begin(); lit != lmend; ++lit, ++k) {
            total += lit.value();
        }

        pressuretotal[pressure] = total;

        k = 0;
        for (auto lit=leakmap.begin(); lit != lmend; ++lit, ++k) {
            //for (k=0;k < N;k++) {
            v1 = lit.key();
            w1 = lit.value();
            sum1 += w1;

            if (sum1 > nthi) {
                pressuremin[pressure] = v1;
                pressurecount[pressure] = w1;
                found = true;
                break;
            }

            if (sum1 == nthi) {
                break; // boundary condition
            }
        }

        if (found) { continue; }

        if (k >= N) {
            pressuremin[pressure] = v1;
            pressurecount[pressure] = w1;
            continue;
        }

        v2 = (leakmap.begin() + (k + 1)).key();
        w2 = (leakmap.begin() + (k + 1)).value();
        sum2 = sum1 + w2;
        // value lies between v1 and v2

        double px = 100.0 / double(SN); // Percentile represented by one full value

        // calculate percentile ranks
        double p1 = px * (double(sum1) - (double(w1) / 2.0));
        double p2 = px * (double(sum2) - (double(w2) / 2.0));

        // calculate linear interpolation
        double v = v1 + ((p - p1) / (p2 - p1)) * (v2 - v1);
        pressuremin[pressure] = v;
        pressurecount[pressure] = w1;

    }
}

// Returns what the nominal leak SHOULD be at the given pressure
EventDataType zMaskProfile::calcLeak(EventStoreType pressure)
{

    // Average mask leak minimum at pressure 4 = 20.167
    // Average mask slope = 1.76
    // Min/max Slope = 1.313 - 2.188
    // Min/max leak at pressure 4 = 18 - 22
    // Min/max leak at pressure 20 = 42 - 54

    float leak; // = 0.0;

//    if (p_profile->cpap->calculateUnintentionalLeaks()) {
        float lpm4 = p_profile->cpap->custom4cmH2OLeaks();
        float lpm20 = p_profile->cpap->custom20cmH2OLeaks();

        float lpm = lpm20 - lpm4;
        float ppm = lpm / 16.0;

        float p = (pressure/10.0f) - 4.0;

        leak = p * ppm + lpm4;
//    } else {
        // the old way sucks!

//        if (maxP == minP) {
//            leak = pressuremin[pressure];
//        } else {
//            leak = (pressure - minP) * (m_factor) + minL;
//        }
//    }
    // Generic Average of Masks from a SpreadSheet... will add two sliders to tweak this between the ranges later
//    EventDataType

    return leak;
}

void zMaskProfile::updateProfile(Session *session)
{
    scanPressure(session);
    scanLeaks(session);

    // new method doesn't require any of this, so bail here.
    return;

    // Do it the old way
/*    updatePressureMin();

    // PressureMin contains the baseline for each Pressure

    // There is only one pressure (CPAP).. so switch off baseline and just use linear calculations
    if (pressuremin.size() <= 1) {
        maxP = minP = 0;
        maxL = minL = 0;
        m_factor = 0;
        return;
    }

    EventDataType p, l, tmp, mean, sum;
    minP = 250, maxP = 0;
    minL = 1000, maxL = 0;

    long cnt = 0, vcnt;
    int n;

    EventDataType maxcnt, maxval, lastval; //, lastcnt;


    auto plend = pressureleaks.end();
    auto it = pressureleaks.begin();

    QMap<EventStoreType, quint32>::iterator lit;
    QMap<EventStoreType, quint32>::iterator lvend;
    for (; it != plend; ++it) {
        p = it.key();
        l = pressuremin[p];
        QMap<EventStoreType, quint32> &leakval = it.value();
        cnt = 0;
        vcnt = -1;
        n = leakval.size();
        sum = 0;

        maxcnt = 0, maxval = 0, lastval = 0;//, lastcnt = 0;

        lvend = leakval.end();
        for (lit = leakval.begin(); lit != lvend; ++lit) {
            cnt += lit.value();

            if (lit.value() > maxcnt) {
                //lastcnt = maxcnt;
                maxcnt = lit.value();
                lastval = maxval;
                maxval = lit.key();

            }

            sum += lit.key() * lit.value();

            if (lit.key() == (EventStoreType)l) {
                vcnt = lit.value();
            }
        }

        pressuremean[p] = mean = sum / EventDataType(cnt);

        if (lastval > 0) {
            maxval = qMax(maxval, lastval); //((maxval*maxcnt)+(lastval*lastcnt)) / (maxcnt+lastcnt);
        }

        pressuremax[p] = lastval;
        sum = 0;

        for (lit = leakval.begin(); lit != lvend; lit++) {
            tmp = lit.key() - mean;
            sum += tmp * tmp;
        }

        pressurestddev[p] = tmp = sqrt(sum / EventDataType(n));

        if (vcnt >= 0) {
            tmp = 1.0 / EventDataType(cnt) * EventDataType(vcnt);

        }
    }

    QMap<EventStoreType, EventDataType> pressureval;
    QMap<EventStoreType, EventDataType> pressureval2;
    EventDataType max = 0, tmp2, tmp3;

    auto ptend = pressuretotal.end();
    for (auto ptit = pressuretotal.begin(); ptit != ptend; ++ptit) {
        if (max < ptit.value()) { max = ptit.value(); }
    }

    for (auto ptit = pressuretotal.begin(); ptit != ptend; ptit++) {
        p = ptit.key();
        tmp = pressurecount[p];
        tmp2 = ptit.value();

        tmp3 = (tmp / tmp2) * (tmp2 / max);

        if (tmp3 > 0.05) {
            tmp = pressuremean[p];

            if (tmp > 0) {
                pressureval[p] = tmp; // / tmp2;

                if (p < minP) { minP = p; }

                if (p > maxP) { maxP = p; }

                if (tmp < minL) { minL = tmp; }

                if (tmp > maxL) { maxL = tmp; }
            }
        }
    }

    if ((maxP == minP)  || (minL == maxL) || (minP == 250) || (minL == 1000)) {
        // crappy data set
        maxP = minP = 0;
        maxL = minL = 0;
        m_factor = 0;
        return;
    }

    m_factor = (maxL - minL) / (maxP - minP);

    //    for (auto it=pressuremin.begin();it!=pressuremin.end()-1;it++) {
    //        p1=it.key();
    //        p2=(it+1).key();
    //        l1=it.value();
    //        l2=(it+1).value();
    //        if (l2 > l1) {
    //            factor=(l2 - l1) / (p2 - p1);
    //            sum+=factor;
    //            cnt++;
    //        }
    //    }

    //    m_factor=sum/double(cnt);

    //    int i=0;
    //    if (i==1) {
    //        QFile f("/home/mark/leaks.csv");
    //        f.open(QFile::WriteOnly);
    //        QTextStream out(&f);
    //        EventDataType p,l,c;
    //        QString fmt;
    //        for (auto it=pressuremin.begin();it!=pressuremin.end();it++) {
    //            p=EventDataType(it.key()/10.0);
    //            l=it.value();
    //            fmt=QString("%1,%2\n").arg(p,0,'f',1).arg(l);
    //            out << fmt;
    //        }

    // cruft
    //        for (auto it=pressureleaks.begin();it!=pressureleaks.end();it++)  {
    //            auto & leakval=it.value();
    //            for (auto lit=leakval.begin();lit!=leakval.end();lit++) {
    //                l=lit.key();
    //                c=lit.value();
    //                fmt=QString("%1,%2,%3\n").arg(p,0,'f',2).arg(l).arg(c);
    //                out << fmt;
    //            }
    //        }
    //        f.close();
    //    }*/
}

QMutex zMaskmutex;
zMaskProfile mmaskProfile(Mask_NasalPillows, "ResMed Swift FX");
bool mmaskFirst = true;

int calcLeaks(Session *session)
{
    if (!p_profile->cpap->calculateUnintentionalLeaks()) { return 0; }

    if (session->type() != MT_CPAP) { return 0; }

    if (session->eventlist.contains(CPAP_Leak)) { return 0; } // abort if already there

    if (!session->eventlist.contains(CPAP_LeakTotal)) { return 0; } // can't calculate without this..

    zMaskmutex.lock();
    zMaskProfile *maskProfile = &mmaskProfile;

    if (mmaskFirst) {
        mmaskFirst = false;
        maskProfile->load(p_profile);
    }

    //    if (!maskProfile) {
    //        maskProfile=new zMaskProfile(Mask_NasalPillows,"ResMed Swift FX");
    //    }
    maskProfile->reset();
    maskProfile->updateProfile(session);

    EventList *leak = session->AddEventList(CPAP_Leak, EVL_Event, 1);

    auto & EVL = session->eventlist[CPAP_LeakTotal];

    TimeValue *p2, *pstr, *pend;

    // can this go out of the loop?
    int mppressize = maskProfile->Pressure.size();
    pstr = maskProfile->Pressure.data();
    pend = maskProfile->Pressure.data()+(mppressize-1);

    // For each sessions Total Leaks list
    EventDataType gain, tmp, val;
    int count;
    EventStoreType *dptr, *eptr, pressure;
    quint32 * tptr;
    qint64 start, ti;
    bool found;

    for (auto & el : EVL) {
        gain = el->gain();
        count = el->count();
        dptr = el->rawData();
        eptr = dptr + count;
        tptr = el->rawTime();
        start = el->first();

        // Scan through this Total Leak list's data
        for (; dptr < eptr; ++dptr) {
            tmp = EventDataType(*dptr) * gain;
            ti = start + *tptr++;

            found = false;

            // Find the current pressure at this moment in time
            pressure = pstr->value;

            for (TimeValue *p1 = pstr; p1 != pend; ++p1) {
                p2 = p1+1;
                if ((p2->time > ti) && (p1->time <= ti)) {
                    pressure = p1->value;
                    found = true;
                    break;
                } else if (p2->time == ti) {
                    pressure = p2->value;
                    found = true;
                    break;
                }
            }

            if (found) {
                // lookup and subtract the calculated leak baseline for this pressure
                val = tmp - maskProfile->calcLeak(pressure);

                if (val < 0) {
                    val = 0;
                }

                leak->AddEvent(ti, val);
            }
        }
    }

    zMaskmutex.unlock();

    return leak->count();
}

void flagLargeLeaks(Session *session)
{
    // Already contains?
    if (session->eventlist.contains(CPAP_LargeLeak))
        return;

    if (!session->eventlist.contains(CPAP_Leak))
        return;

    EventDataType threshold = p_profile->cpap->leakRedline();

    if (threshold <= 0) {
        return;
    }


    QVector<EventList *> & EVL = session->eventlist[CPAP_Leak];
    int evlsize = EVL.size();

    if (evlsize == 0)
        return;

    EventList * LL = nullptr;
    qint64 time = 0;
    EventDataType value, lastvalue=-1;
    qint64 leaktime=0;
    int count;

    for (auto & el : EVL) {
        count = el->count();
        if (!count) continue;

        leaktime = 0;
        //EventDataType leakvalue = 0;
        lastvalue = -1;

        for (int i=0; i < count; ++i) {
            time = el->time(i);
            value = el->data(i);
            if (value >= threshold) {
                if (lastvalue < threshold) {
                    leaktime = time;
                    //leakvalue = value;
                }
            } else if (lastvalue > threshold) {
                if (!LL) {
                    LL=session->AddEventList(CPAP_LargeLeak, EVL_Event);
                }
                int duration = (time - leaktime) / 1000L;
                LL->AddEvent(time, duration);
            }
            lastvalue = value;
        }

    }

    if (lastvalue > threshold) {
        if (!LL) {
            LL=session->AddEventList(CPAP_LargeLeak, EVL_Event);
        }
        int duration = (time - leaktime) / 1000L;
        LL->AddEvent(time, duration);
    }
}

int calcPulseChange(Session *session)
{
    if (session->eventlist.contains(OXI_PulseChange)) { return 0; }

    auto it = session->eventlist.find(OXI_Pulse);

    if (it == session->eventlist.end()) { return 0; }

    EventDataType val, val2, change, tmp;
    qint64 time, time2;
    qint64 window = p_profile->oxi->pulseChangeDuration();
    window *= 1000;

    change = p_profile->oxi->pulseChangeBPM();

    EventList *pc = new EventList(EVL_Event, 1, 0, 0, 0, 0, true);
    pc->setFirst(session->first(OXI_Pulse));
    qint64 lastt;
    EventDataType lv = 0;
    int li = 0;

    int max;

    int elcount;
    for (auto & el : it.value()) {
        elcount = el->count();
        for (int i = 0; i < elcount; ++i) {
            val = el->data(i);
            time = el->time(i);

            lastt = 0;
            lv = change;
            max = 0;

            for (int j = i + 1; j < elcount; ++j) { // scan ahead in the window
                time2 = el->time(j);

                if (time2 > time + window) { break; }

                val2 = el->data(j);
                tmp = qAbs(val2 - val);

                if (tmp > lv) {
                    lastt = time2;

                    if (tmp > max) { max = tmp; }

                    //lv=tmp;
                    li = j;
                }
            }

            if (lastt > 0) {
                qint64 len = (lastt - time) / 1000.0;
                pc->AddEvent(lastt, len, tmp);
                i = li;

            }
        }
    }

    if (pc->count() == 0) {
        delete pc;
        return 0;
    }

    session->eventlist[OXI_PulseChange].push_back(pc);
    session->setMin(OXI_PulseChange, pc->Min());
    session->setMax(OXI_PulseChange, pc->Max());
    session->setCount(OXI_PulseChange, pc->count());
    session->setFirst(OXI_PulseChange, pc->first());
    session->setLast(OXI_PulseChange, pc->last());
    return pc->count();
}


int calcSPO2Drop(Session *session)
{
    if (session->eventlist.contains(OXI_SPO2Drop)) { return 0; }

    auto it = session->eventlist.find(OXI_SPO2);
    if (it == session->eventlist.end()) { return 0; }

    EventDataType val, val2, change, tmp;
    qint64 time, time2;
    qint64 window = p_profile->oxi->spO2DropDuration();
    window *= 1000;
    change = p_profile->oxi->spO2DropPercentage();

    EventList *pc = new EventList(EVL_Event, 1, 0, 0, 0, 0, true);
    qint64 lastt;
    //EventDataType lv = 0;
    int li = 0;

    // Fix me.. Time scale varies.
    //const unsigned ringsize=30;
    //EventDataType ring[ringsize]={0};
    //qint64 rtime[ringsize]={0};
    //int rp=0;
    int min;
    int cnt = 0;
    tmp = 0;

    qint64 start = 0;

    // Calculate median baseline
    QList<EventDataType> med;

    int elcount;
    for (auto & el : it.value()) {
        elcount = el->count();
        for (int i = 0; i < elcount; i++) {
            val = el->data(i);
            time = el->time(i);

            if (val > 0) { med.push_back(val); }

            if (!start) { start = time; }

            if (time > start + 3600000) { break; } // just look at the first hour

            tmp += val;
            cnt++;
        }
    }

    EventDataType baseline = 0;

    if (med.size() > 0) {
        std::sort(med.begin(), med.end());

        int midx = float(med.size()) * 0.90;

        if (midx > med.size() - 1) { midx = med.size() - 1; }

        if (midx < 0) { midx = 0; }

        baseline = med[midx];
    }

    session->settings[OXI_SPO2Drop] = baseline;
    //EventDataType baseline=round(tmp/EventDataType(cnt));
    EventDataType current;
    qDebug() << "Calculated baseline" << baseline;

    for (auto & el : it.value()) {
        elcount = el->count();
        for (int i = 0; i < elcount; ++i) {
            current = el->data(i);

            if (!current) { continue; }

            time = el->time(i);
            /*ring[rp]=val;
            rtime[rp]=time;
            rp++;
            rp=rp % ringsize;
            if (i<ringsize)  {
                for (unsigned j=i;j<ringsize;j++) {
                    ring[j]=val;
                    rtime[j]=0;
                }
            }
            tmp=0;
            cnt=0;
            for (unsigned j=0;j<ringsize;j++) {
                if (rtime[j] > time-300000) {  // only look at recent entries..
                    tmp+=ring[j];
                    cnt++;
                }
            }
            if (!cnt) {
                unsigned j=abs((rp-1) % ringsize);
                tmp=(ring[j]+val)/2;
            } else tmp/=EventDataType(cnt); */

            val = baseline;
            lastt = 0;
            //lv = val;


            min = val;

            for (int j = i; j < elcount; ++j) { // scan ahead in the window
                time2 = el->time(j);
                //if (time2 > time+window) break;
                val2 = el->data(j);

                if (val2 > baseline - change) { break; }

                lastt = time2;
                li = j + 1;
            }

            if (lastt > 0) {
                qint64 len = (lastt - time);

                if (len >= window) {
                    pc->AddEvent(lastt, len / 1000, val - min);
                    i = li;
                }
            }
        }
    }

    if (pc->count() == 0) {
        delete pc;
        return 0;
    }

    session->eventlist[OXI_SPO2Drop].push_back(pc);
    session->setMin(OXI_SPO2Drop, pc->Min());
    session->setMax(OXI_SPO2Drop, pc->Max());
    session->setCount(OXI_SPO2Drop, pc->count());
    session->setFirst(OXI_SPO2Drop, pc->first());
    session->setLast(OXI_SPO2Drop, pc->last());
    return pc->count();
}
