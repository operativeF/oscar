/* SleepLib Event Class Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDebug>
#include "event.h"

EventList::EventList(EventListType et, EventDataType gain, EventDataType offset, EventDataType min,
                     EventDataType max, double rate, bool second_field)
    : m_type(et), m_gain(gain), m_offset(offset), m_min(min), m_max(max), m_rate(rate),
      m_second_field(second_field)
{
    m_first = m_last = 0;
    m_count = 0;

    if (min == max) { // Update Min & Max unless forceably set here..
        m_update_minmax = true;
        m_min2 = m_min = 999999999.0F;
        m_max2 = m_max = -999999999.0F;
    } else {
        m_update_minmax = false;
    }

    m_data.reserve(2048);

    // Reserve a few to increase performace??
}

void EventList::clear()
{
    m_min2 = m_min = 999999999.0F;
    m_max2 = m_max = -999999999.0F;
    m_update_minmax = true;
    m_first = m_last = 0;
    m_count = 0;

    m_data.clear();
    m_data2.clear();
    m_time.clear();

}

qint64 EventList::time(quint32 i) const
{
    if (m_type == EVL_Event) {
        return m_first + qint64(m_time[i]);
    }

    return m_first + qint64((EventDataType(i) * m_rate));
}

EventDataType EventList::data(quint32 i)
{
    return EventDataType(m_data[i]) * m_gain;
}

EventDataType EventList::data2(quint32 i)
{
    return EventDataType(m_data2[i]);
}

void EventList::AddEvent(qint64 time, EventStoreType data)
{
    // Apply gain & offset
    EventDataType val = EventDataType(data) * m_gain; // ignoring m_offset

    if (m_update_minmax) {
        if (m_count == 0) {
            m_max = m_min = val;
        } else {
            m_min = (val < m_min) ? val : m_min;
            m_max = (val > m_max) ? val : m_max;
        }
    }

    if (!m_first) {
        m_first = time;
        m_last = time;
    }

    if (m_first > time) {
        // Crud.. Update all the previous records
        // This really shouldn't happen.
        qDebug() << "Unordered time detected in AddEvent().";

        qint32 delta = (m_first - time);

        for (quint32 i = 0; i < m_count; ++i) {
            m_time[i] -= delta;
        }

        m_first = time;
    }

    if (m_last < time) {
        m_last = time;
    }

    quint32 delta = (time - m_first);

    m_data.push_back(data);
    m_time.push_back(delta);
    m_count++;
}

void EventList::AddEvent(qint64 time, EventStoreType data, EventStoreType data2)
{
    AddEvent(time, data);

    if (!m_second_field)
        return;

    m_min2 = (data2 < m_min2) ? data2 : m_min2;
    m_max2 = (data2 > m_max2) ? data2 : m_max2;

    m_data2.push_back(data2);
}

// Adds a consecutive waveform chunk
void EventList::AddWaveform(qint64 start, qint16 *data, int recs, qint64 duration)
{
    if (m_type != EVL_Waveform) {
        qWarning() << "Attempted to add waveform data to non-waveform object";
        return;
    }

    if (!m_rate) {
        qWarning() << "Attempted to add waveform without setting sample rate";
        return;
    }

    qint64 last = start + duration;

    if (!m_first) {
        m_first = start;
        m_last = last;
    }

    if (m_last > start) {
        //qWarning() << "Attempted to add waveform with previous timestamp";
        // return;

        // technically start should equal m_last+1 sample.. check this too.
    }

    if (m_last < last) {
        m_last = last;
    }

    // TODO: Check waveform chunk really is contiguous
    //double rate=duration/recs;

    //realloc buffers.
    int r = m_count;
    m_count += recs;
    m_data.resize(m_count);

  //  EventStoreType *edata = m_data.data();

    //EventStoreType raw;
    const qint16 *sp = data;
    const qint16 *ep = data + recs;
    EventStoreType *dp = (EventStoreType *)m_data.data()+r;
//    EventStoreType *dp = &edata[r];

    if (m_update_minmax) {
        EventDataType min = m_min, max = m_max, val, gain = m_gain;

        memcpy(dp, sp, recs*2);

        for (sp = data; sp < ep; ++sp) {
//            *dp++ = raw = *sp;
            val = EventDataType(*sp) * gain + m_offset;

            if (min > val) { min = val; }

            if (max < val) { max = val; }
        }

        m_min = min;
        m_max = max;
    } else {
        //register EventDataType val,gain=m_gain;
        for (int i=0; i < recs; ++i) {
            m_data[i] = *sp++;
        }
//        for (sp = data; sp < ep; ++sp) {
//            *dp++ = *sp;
//            //val=EventDataType(raw)*gain;
//        }
    }

}
void EventList::AddWaveform(qint64 start, unsigned char *data, int recs, qint64 duration)
{
    if (m_type != EVL_Waveform) {
        qWarning() << "Attempted to add waveform data to non-waveform object";
        return;
    }

    if (!m_rate) {
        qWarning() << "Attempted to add waveform without setting sample rate";
        return;
    }

    // duration=recs*rate;
    qint64 last = start + duration;

    if (!m_first) {
        m_first = start;
        m_last = last;
    }

    if (m_last > start) {
        //qWarning() << "Attempted to add waveform with previous timestamp";
        // return;

        // technically start should equal m_last+1 sample.. check this too.
    }

    if (m_last < last) {
        m_last = last;
    }

    // TODO: Check waveform chunk really is contiguos

    //realloc buffers.
    int r = m_count;
    m_count += recs;
    m_data.resize(m_count);

    EventStoreType *edata = m_data.data();
    EventStoreType raw;
    EventDataType val;

    unsigned char *sp;
    unsigned char *ep = data + recs;
    EventStoreType *dp = &edata[r];

    if (m_update_minmax) {
        // ignoring m_offset
        for (sp = data; sp < ep; ++sp) {
            raw = *sp;
            val = EventDataType(raw) * m_gain;

            if (m_min > val) { m_min = val; }

            if (m_max < val) { m_max = val; }

            *dp++ = raw;
        }
    } else {
        for (sp = data; sp < ep; ++sp) {
            raw = *sp;
            //val = EventDataType(raw) * m_gain;
            *dp++ = raw;
        }
    }
}

void EventList::AddWaveform(qint64 start, char *data, int recs, qint64 duration)
{
    if (m_type != EVL_Waveform) {
        qWarning() << "Attempted to add waveform data to non-waveform object";
        return;
    }

    if (!m_rate) {
        qWarning() << "Attempted to add waveform without setting sample rate";
        return;
    }

    // duration=recs*rate;
    qint64 last = start + duration;

    if (!m_first) {
        m_first = start;
        m_last = last;
    } else {
        if (m_last > start) {
            //qWarning() << "Attempted to add waveform with previous timestamp";
            //return;

            // technically start should equal m_last+1 sample.. check this too.
        }

        if (m_last < last) {
            m_last = last;
        }
    }

    // TODO: Check waveform chunk really is contiguos

    //realloc buffers.

    int r = m_count;
    m_count += recs;
    m_data.resize(m_count);

    EventStoreType *edata = m_data.data();
    EventStoreType raw;
    EventDataType val; // FIXME: sstangl: accesses random memory

    char *sp;
    char *ep = data + recs;
    EventStoreType *dp = &edata[r];

    if (m_update_minmax) {
        for (sp = data; sp < ep; ++sp) {
            raw = *sp;
            val = EventDataType(raw) * m_gain + m_offset;

            if (m_min > val) { m_min = val; }
            if (m_max < val) { m_max = val; }

            *dp++ = raw;
        }
    } else {
        for (sp = data; sp < ep; ++sp) {
            raw = *sp;
     //       val = EventDataType(raw) * m_gain + m_offset;
            *dp++ = raw;
        }
    }
}
