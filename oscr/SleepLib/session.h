/* SleepLib Session Header
 *
 * This stuff contains the session calculation smarts
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SESSION_H
#define SESSION_H

#define SESSION_DEBUG

#include <QDebug>
#include <QHash>
#include <QVector>

#include "SleepLib/machine.h"
#include "SleepLib/schema.h"
#include "SleepLib/event.h"
//class EventList;
class Machine;

enum SliceStatus {
    UnknownStatus=0, EquipmentOff, EquipmentLeaking, EquipmentOn
};

class SessionSlice
{
public:
    SessionSlice() {
        start = end = 0;
        status = UnknownStatus;
    }
    SessionSlice(const SessionSlice & copy) {
        start = copy.start;
        end = copy.end;
        status = copy.status;
    }
    SessionSlice(qint64 start, qint64 end, SliceStatus status):start(start), end(end), status(status) {}

    qint64 start;
    qint64 end;
    SliceStatus status;
};

/*! \class Session
    \brief Contains a single Sessions worth of machine event/waveform information.

    This class also contains all the primary database logic for SleepLib
    */
class Session
{
    friend class Day;
    friend class Machine;
  public:
    /*! \fn Session(Machine *,SessionID);
        \brief Create a session object belonging to Machine, with supplied SessionID
        If sessionID is 0, the next in sequence will be picked
        */
    Session(Machine *, SessionID);
    virtual ~Session();

    inline bool checkInside(qint64 time) {
        return ((time >= s_first) && (time <= s_last));
    }

    //! \brief Stores the session in the directory supplied by path
    bool Store(QString path);

    //! \brief Writes the Sessions Summary Indexes to filename, in SleepLibs custom data format.
    bool StoreSummary();

//    //! \brief Save the Sessions Summary Indexes to the stream
//    void StoreSummaryData(QDataStream & out) const;

    //! \brief Writes the Sessions EventLists to filename, in SleepLibs custom data format.
    bool StoreEvents();

    //bool Load(QString path);

//    //! \brief Loads the Sessions Summary Indexes from stream
//    void LoadSummaryData(QDataStream & in);

    //! \brief Loads the Sessions Summary Indexes from filename, from SleepLibs custom data format.
    bool LoadSummary();

    //! \brief Loads the Sessions EventLists from filename, from SleepLibs custom data format.
    bool LoadEvents(QString filename);

    //! \brief Loads the events for this session when requested (only the summaries are loaded at startup)
    bool OpenEvents();

    //! \brief Put the events away until needed again, freeing memory
    void TrashEvents();

    //! \brief Returns true if session contains an empty duration
    inline bool isEmpty() { return (s_first == s_last); }

    //! \brief Search for Event code happening at supplied time (ms since epoch)
    EventDataType SearchValue(ChannelID code, qint64 time, bool square);

    //! \brief Return the sessionID
    inline const SessionID &session() {
        return s_session;
    }

    //! \brief Returns whether or not session is being used.
    inline bool enabled() const { return s_enabled; }

    //! \brief Sets whether or not session is being used.
    void setEnabled(bool b);

    inline qint64 realFirst() const { return s_first; }
    inline qint64 realLast() const { return s_last; }

    //! \brief Return the start of this sessions time range (in milliseconds since epoch)
    qint64 first();

    //! \brief Return the end of this sessions time range (in milliseconds since epoch)
    qint64 last();

    //! \brief Return the millisecond length of this session
    qint64 length() {
        return s_last - s_first;
//        qint64 t;
//        int size = m_slices.size();
//        if (size == 0) {
//            t = (s_last - s_first);
//        } else {
//            t = 0;
//            for (int i=0; i<size; ++i) {
//                const SessionSlice & slice = m_slices.at(i);
//                if (slice.status == EquipmentOn) {
//                    t += slice.end - slice.start;
//                }
//            }
//        }

//        return t;
    }

    //! \brief Set this Sessions ID (Not does not update indexes)
    void SetSessionID(SessionID s) {
        s_session = s;
    }

    //! \brief Moves all of this session data by offset d milliseconds
    void offsetSession(qint64 d);

    //! \brief Just set the start of the timerange without comparing
    void really_set_first(qint64 d) { s_first = d; }

    //! \brief Just set the end of the timerange without comparing
    void really_set_last(qint64 d) { s_last = d; }

    void set_first(qint64 d) {
        if (!s_first) { s_first = d; }
        else if (d < s_first) { s_first = d; }
    }
    void set_last(qint64 d) {
        if (d <= s_first) {
            qWarning() << "Session::set_last() d<=s_first";
            return;
        }

        if (!s_last) { s_last = d; }
        else if (s_last < d) { s_last = d; }
    }

    //! \brief Return Session Length in decimal hours
    double hours() {
        double t;
        int size = m_slices.size();
        if (size == 0) {
            t = (s_last - s_first) / 3600000.0;
        } else {
            t = 0;
            for (int i=0; i<size; ++i) {
                const SessionSlice & slice = m_slices.at(i);
                if (slice.status == EquipmentOn) {
                    t += slice.end - slice.start;
                }
            }
        }
        return t;
    }

    //! \brief Flag this Session as dirty, so Machine object can save it
    void SetChanged(bool val) {
        s_changed = val;
        s_events_loaded = val; // dirty hack putting this here
    }

    //! \brief Return this Sessions dirty status
    bool IsChanged() {
        return s_changed;
    }

    //! \brief Return the unit type string used by events of supplied channel
    QString dimension(ChannelID);


    //! \brief Contains all the EventLists, indexed by ChannelID
    QHash<ChannelID, QVector<EventList *> > eventlist;

    //! \brief Sessions Settings List, contianing single settings for this session.
    QHash<ChannelID, QVariant> settings;

    // Session caches
    QHash<ChannelID, EventDataType> m_cnt;
    QHash<ChannelID, double> m_sum;
    QHash<ChannelID, EventDataType> m_avg;
    QHash<ChannelID, EventDataType> m_wavg;

    QHash<ChannelID, EventDataType> m_min; // The actual minimum
    QHash<ChannelID, EventDataType> m_max;

    // This could go in channels, but different machines interpret it differently
    // Under the new SleepyLib data Device model this can be done, but unfortunately not here..
    QHash<ChannelID, EventDataType> m_physmin; // The physical minimum for graph display purposes
    QHash<ChannelID, EventDataType> m_physmax; // The physical maximum

    QHash<ChannelID, EventDataType> m_cph; // Counts per hour (eg AHI)
    QHash<ChannelID, EventDataType> m_sph; // % indice (eg % night in CSR)
    QHash<ChannelID, quint64> m_firstchan;
    QHash<ChannelID, quint64> m_lastchan;

    QHash<ChannelID, QHash<EventStoreType, EventStoreType> > m_valuesummary;
    QHash<ChannelID, QHash<EventStoreType, quint32> > m_timesummary;
    QHash<ChannelID, EventDataType> m_gain;

    QHash<ChannelID, EventDataType> m_lowerThreshold;
    QHash<ChannelID, EventDataType> m_timeBelowTheshold;
    QHash<ChannelID, EventDataType> m_upperThreshold;
    QHash<ChannelID, EventDataType> m_timeAboveTheshold;

    QList<ChannelID> m_availableChannels;
    QList<ChannelID> m_availableSettings;

    QVector<SessionSlice> m_slices;

    //! \brief Generates sum and time data for each distinct value in 'code' events..
    void updateCountSummary(ChannelID code);

    //! \brief Destroy any trace of event 'code', freeing any memory if loaded.
    void destroyEvent(ChannelID code);

    // UpdateSummaries may recalculate all these, but it may be faster setting upfront
    void setCount(ChannelID id, EventDataType val) { m_cnt[id] = val; }
    void setSum(ChannelID id, EventDataType val) { m_sum[id] = val; }
    void setMin(ChannelID id, EventDataType val) { m_min[id] = val; }
    void setMax(ChannelID id, EventDataType val) { m_max[id] = val; }
    void setPhysMin(ChannelID id, EventDataType val) { m_physmin[id] = val; }
    void setPhysMax(ChannelID id, EventDataType val) { m_physmax[id] = val; }
    void updateMin(ChannelID id, EventDataType val) {
        QHash<ChannelID, EventDataType>::iterator i = m_min.find(id);

        if (i == m_min.end()) {
            m_min[id] = val;
        } else if (i.value() > val) {
            i.value() = val;
        }
    }
    void updateMax(ChannelID id, EventDataType val) {
        QHash<ChannelID, EventDataType>::iterator i = m_max.find(id);

        if (i == m_max.end()) {
            m_max[id] = val;
        } else if (i.value() < val) {
            i.value() = val;
        }
    }

    void setAvg(ChannelID id, EventDataType val) { m_avg[id] = val; }
    void setWavg(ChannelID id, EventDataType val) { m_wavg[id] = val; }
    //    void setMedian(ChannelID id,EventDataType val) { m_med[id]=val; }
    //    void set90p(ChannelID id,EventDataType val) { m_90p[id]=val; }
    //    void set95p(ChannelID id,EventDataType val) { m_95p[id]=val; }
    void setCph(ChannelID id, EventDataType val) { m_cph[id] = val; }
    void setSph(ChannelID id, EventDataType val) { m_sph[id] = val; }
    void setFirst(ChannelID id, qint64 val) { m_firstchan[id] = val; }
    void setLast(ChannelID id, qint64 val) { m_lastchan[id] = val; }

    EventDataType count(ChannelID id);

    //! \brief Returns the Count of all events of type id between time range
    EventDataType rangeCount(ChannelID id, qint64 first, qint64 last);

    //! \brief Returns the Sum of all events of type id between time range
    double rangeSum(ChannelID id, qint64 first, qint64 last);

    //! \brief Returns the minimum of events of type id between time range
    EventDataType rangeMin(ChannelID id, qint64 first, qint64 last);

    //! \brief Returns the maximum of events of type id between time range
    EventDataType rangeMax(ChannelID id, qint64 first, qint64 last);

    //! \brief Returns the count of code events inside span flag event durations
    EventDataType countInsideSpan(ChannelID span, ChannelID code);

    //! \brief Returns (and caches) the Sum of all events of type id
    double sum(ChannelID id);

    //! \brief Returns (and caches) the Average of all events of type id
    EventDataType avg(ChannelID id);

    //! \brief Returns (and caches) the Time Weighted Average of all events of type id
    EventDataType wavg(ChannelID i);

    //! \brief Returns (and caches) the Minimum of all events of type id
    EventDataType Min(ChannelID id);

    //! \brief Returns (and caches) the Maximum of all events of type id
    EventDataType Max(ChannelID id);

    //! \brief Returns (and caches) the Minimum of all events of type id
    EventDataType physMin(ChannelID id);

    //! \brief Returns (and caches) the Maximum of all events of type id
    EventDataType physMax(ChannelID id);

    //! \brief Returns (and caches) the 90th Percentile of all events of type id
    EventDataType p90(ChannelID id);

    //! \brief Returns (and caches) the 95th Percentile of all events of type id
    EventDataType p95(ChannelID id);

    //! \brief Returns (and caches) the Median (50th Perc) of all events of type id
    EventDataType median(ChannelID id);

    //! \brief Returns (and caches) the Count-Per-Hour of all events of type id
    EventDataType cph(ChannelID id);

    //! \brief Returns (and caches) the Sum-Per-Hour of all events of type id
    EventDataType sph(ChannelID id);

    //! \brief Returns (without caching) the requested Percentile of all events of type id
    EventDataType percentile(ChannelID id, EventDataType percentile);

    //! \brief Returns the amount of time (in decimal minutes) the Channel spent above the threshold
    EventDataType timeAboveThreshold(ChannelID id, EventDataType threshold);

    //! \brief Returns the amount of time (in decimal minutes) the Channel spent below the threshold
    EventDataType timeBelowThreshold(ChannelID id, EventDataType threshold);

    //! \brief According to preferences..
    EventDataType calcMiddle(ChannelID code);
    EventDataType calcMax(ChannelID code);
    EventDataType calcPercentile(ChannelID code);
    
    //! \brief Returns true if the channel has events loaded, or a record of a count for when they are not
    bool channelExists(ChannelID name);

    //! \brief Returns true if the channel has event data available (must be loaded first)
    bool channelDataExists(ChannelID id);

    bool IsLoneSession() { return s_lonesession; }
    void SetLoneSession(bool b) { s_lonesession = b; }

    bool eventsLoaded() { return s_events_loaded; }

    //! \brief Update this sessions first time if it's less than the current record
    inline void updateFirst(qint64 v) { if (!s_first) { s_first = v; } else if (s_first > v) { s_first = v; } }

    //! \brief Update this sessions latest time if it's more than the current record
    inline void updateLast(qint64 v) { if (!s_last) { s_last = v; } else if (s_last < v) { s_last = v; } }

    //! \brief Returns (and caches) the first time for Channel code
    qint64 first(ChannelID code);

    //! \brief Returns (and caches) the last time for Channel code
    qint64 last(ChannelID code);

    //! \brief Regenerates the Session Index Caches, and calls the fun calculation functions
    void UpdateSummaries();

    //! \brief Creates and returns a new EventList for the supplied Channel code
    EventList *AddEventList(ChannelID code, EventListType et, EventDataType gain = 1.0,
                            EventDataType offset = 0.0, EventDataType min = 0.0, EventDataType max = 0.0,
                            EventDataType rate = 0.0, bool second_field = false);

    //! \brief Returns this sessions MachineID
    Machine *machine() { return s_machine; }

    //! \brief Returns true if session only contains summary data
    inline bool summaryOnly() { return s_summaryOnly; }
    inline bool noSettings() { return s_noSettings; }

    inline void setSummaryOnly(bool b) { s_summaryOnly = b; }
    inline void setNoSettings(bool b) { s_noSettings = b; }

    void setOpened(bool b = true) {
        s_events_loaded = b;
        s_summary_loaded = b;
    }

    //! \brief Completely purges Session from memory and disk.
    bool Destroy();

    void wipeSummary() {
        s_first = s_last = 0;
        s_enabled = true;
        m_cph.clear();
        m_sum.clear();
        m_cnt.clear();
    }


    QString eventFile() const;

    MachineType type() { return s_machtype; }



#if defined(SESSION_DEBUG)
    QStringList session_files;
#endif

protected:
    SessionID s_session;

    Machine *s_machine;
    qint64 s_first;
    qint64 s_last;
    bool s_changed;
    bool s_lonesession;
    bool s_evchecksum_checked;
    bool _first_session;
    bool s_summaryOnly;
    bool s_noSettings;

    bool s_summary_loaded;
    bool s_events_loaded;
    bool s_enabled;

    // for debugging
    bool destroyed;
    MachineType s_machtype;
};

QDataStream & operator<<(QDataStream & out, const Session & session);
QDataStream & operator>>(QDataStream & in, Session & session);

#endif // SESSION_H
