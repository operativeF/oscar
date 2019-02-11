/* SleepLib Event Class Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef EVENT_H
#define EVENT_H

#include <QDateTime>

#include "machine_common.h"

//! \brief EventLists can either be Waveform or Event types
enum EventListType { EVL_Waveform, EVL_Event };

/*! \class EventList
    \author Mark Watkins <jedimark_at_users.sourceforge.net>
    \brief EventLists contains waveforms at a specified rate, or a list of event and time data.
    */
class EventList
{
    friend class Session;

  public:
    EventList(EventListType et, EventDataType gain = 1.0, EventDataType offset = 0.0,
              EventDataType min = 0.0, EventDataType max = 0.0, double rate = 0.0,
              bool second_field = false);

    //! \brief Wipe the event list so it can be reused
    void clear();

    /*! \brief Add an event starting at time, containing data to this event list
      Note, data2 is only used if second_field is specified in the constructor */
    void AddEvent(qint64 time, EventStoreType data);
    void AddEvent(qint64 time, EventStoreType data, EventStoreType data2);
    void AddWaveform(qint64 start, qint16 *data, int recs, qint64 duration);
    void AddWaveform(qint64 start, unsigned char *data, int recs, qint64 duration);
    void AddWaveform(qint64 start, char *data, int recs, qint64 duration);

    //! \brief Returns a count of records contained in this EventList
    inline quint32 count() const { return m_count; }

    //! \brief Manually sets a count of records contained in this EventList
    void setCount(quint32 count) { m_count = count; }

    //! \brief Returns a raw ("ungained") data value from index position i
    inline EventStoreType raw(int i)  const { return m_data[i]; }

    //! \brief Returns a raw ("ungained") data2 value from index position i
    inline EventStoreType raw2(int i) const { return m_data2[i]; }

    //! \brief Returns a data value multiplied by gain from index position i
    EventDataType data(quint32 i);

    //! \brief Returns a data2 value multiplied by gain from index position i
    EventDataType data2(quint32 i);

    //! \brief Returns either the timestamp for the i'th event, or calculates the waveform time position i
    qint64 time(quint32 i) const;

    //! \brief Returns true if this EventList uses the second data field
    bool hasSecondField() { return m_second_field; }

    //! \brief Returns the first events/waveforms starting time in milliseconds since epoch
    inline qint64 first() const { return m_first; }

    //! \brief Returns the last events/waveforms ending time in milliseconds since epoch
    inline qint64 last() const { return m_last; }

    //! \brief Returns the timespan covered by this EventList, in milliseconds since epoch
    inline qint64 duration() { return m_last - m_first; }

    //! \brief Sets the first events/waveforms starting time in milliseconds since epoch
    void setFirst(qint64 val) { m_first = val; }
    //! \brief Sets the last events/waveforms ending time in milliseconds since epoch
    void setLast(qint64 val) { m_last = val; }

    //! \brief Set this EventList to either EVL_Waveform or EVL_Event type
    void setType(EventListType type) { m_type = type; }

    //! \brief Change the gain multiplier value
    void setGain(EventDataType v) { m_gain = v; }

    //! \brief Change the gain offset value
    void setOffset(EventDataType v) { m_offset = v; }

    //! \brief Set the Minimum value for data
    void setMin(EventDataType v) { m_min = v; }

    //! \brief Set the Maximum value for data
    void setMax(EventDataType v) { m_max = v; }

    //! \brief Set the Minimum value for data2
    void setMin2(EventDataType v) { m_min2 = v; }

    //! \brief Set the Maximum value for data2
    void setMax2(EventDataType v) { m_max2 = v; }

    //! \brief Set the sample rate
    void setRate(EventDataType v) { m_rate = v; }

    //void setCode(ChannelID id) { m_code=id; }

    //! \brief Return the Minimum data value
    inline EventDataType Min() { return m_min; }

    //! \brief Return the Maximum data value
     inline EventDataType Max() { return m_max; }

    //! \brief Return the Minimum data2 value
    inline EventDataType min2() { return m_min2; }

    //! \brief Return the Maximum data value
    inline EventDataType max2() { return m_max2; }

    //! \brief Return the gain value
    inline EventDataType gain() const { return m_gain; }

    //! \brief Return the gain offset
    inline EventDataType offset() { return m_offset; }

    //! \brief Return the sample rate
    inline EventDataType rate() { return m_rate; }

    //! \brief Return the EventList type, either EVL_Waveform or EVL_Event
    inline EventListType type() { return m_type; }
    //inline const ChannelID & code() { return m_code; }

    //! \brief Returns whether or not min/max values are updated while adding events
    inline const bool &update_minmax() { return m_update_minmax; }

    //! \brief Returns the dimension (units type) of the contained data object
    QString dimension() { return m_dimension; }

    //! \brief Sets the dimension (units type) of the contained data object
    void setDimension(QString dimension) { m_dimension = dimension; }

    //! \brief Returns the data storage vector
    QVector<EventStoreType> &getData() { return m_data; }

    //! \brief Returns the data2 storage vector
    QVector<EventStoreType> &getData2() { return m_data2; }

    //! \brief Returns the time storage vector (only used in EVL_Event types)
    QVector<quint32> &getTime() { return m_time; }

    // Don't mess with these without considering the consequences
    void rawDataResize(quint32 i) { m_data.resize(i); m_count = i; }
    void rawData2Resize(quint32 i) { m_data2.resize(i); m_count = i; }
    void rawTimeResize(quint32 i) { m_time.resize(i); m_count = i; }
    EventStoreType *rawData() { return m_data.data(); }
    EventStoreType *rawData2() { return m_data2.data(); }
    quint32 *rawTime() { return m_time.data(); }

  protected:
    //! \brief The time storage vector, in 32bits delta format, added as offsets to m_first
    QVector<quint32> m_time;

    //! \brief The "ungained" raw data storage vector
    QVector<EventStoreType> m_data;

    //! \brief The "ungained" raw data2 storage vector
    QVector<EventStoreType> m_data2;
    //ChannelID m_code;

    //! \brief Either EVL_Waveform or EVL_Event
    EventListType m_type;

    //! \brief Count of events
    quint32 m_count;

    EventDataType m_gain;
    EventDataType m_offset;
    EventDataType m_min, m_min2;
    EventDataType m_max, m_max2;
    EventDataType m_rate;     // Waveform sample rate

    QString m_dimension;

    qint64 m_first, m_last;
    bool m_update_minmax;
    bool m_second_field;
};

#endif // EVENT_H
