/* Minutes At Pressure Graph Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef MINUTESATPRESSURE_H
#define MINUTESATPRESSURE_H

#include "Graphs/layer.h"
#include "SleepLib/day.h"

class MinutesAtPressure;
struct PressureInfo
{
    PressureInfo()
    {
        code = 0;
        minx = maxx = 0;
        peaktime = peakevents = 0;
        min_pressure = max_pressure = 0;
    }
    PressureInfo(PressureInfo &copy) {
        code = copy.code;
        minx = copy.minx;
        maxx = copy.maxx;
        peaktime = copy.peaktime;
        peakevents = copy.peakevents;
        min_pressure = copy.min_pressure;
        max_pressure = copy.max_pressure;
        times = copy.times;
        events = copy.events;
        chans = copy.chans;
    }

    PressureInfo(ChannelID code, qint64 minx, qint64 maxx) : code(code), minx(minx), maxx(maxx)
    {
        times.resize(300);
    }
    void AddChannel(ChannelID c)
    {
        chans.append(c);
        events[c].resize(300);
    }
    void AddChannels(QList<ChannelID> & chans)
    {
        for (int i=0; i<chans.size(); ++i) {
            AddChannel(chans.at(i));
        }
    }
    void finishCalcs();

    ChannelID code;
    qint64 minx, maxx;
    QVector<int> times;
    int peaktime, peakevents;
    int min_pressure, max_pressure;

    QHash<ChannelID, QVector<int> > events;
    QList<ChannelID> chans;
};

class RecalcMAP:public QRunnable
{
    friend class MinutesAtPressure;
public:
    explicit RecalcMAP(MinutesAtPressure * map) :map(map), m_quit(false), m_done(false) {}
    virtual ~RecalcMAP();
    virtual void run();

    void quit();
protected:
    void updateTimes(PressureInfo & info, Session * sess);
    MinutesAtPressure * map;
    volatile bool m_quit;
    volatile bool m_done;
};

class MinutesAtPressure:public Layer
{
    friend class RecalcMAP;
public:
    MinutesAtPressure();
    virtual ~MinutesAtPressure();

    virtual void recalculate(gGraph * graph);

    virtual void SetDay(Day *d);

    virtual bool isEmpty();
    virtual int minimumHeight();

    //! Draw filled rectangles behind Event Flag's, and an outlines around them all, Calls the individual paint for each gFlagLine
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);
    bool mousePressEvent(QMouseEvent *event, gGraph *graph);
    bool mouseReleaseEvent(QMouseEvent *event, gGraph *graph);

    virtual void recalcFinished();
    virtual Layer * Clone() {
        MinutesAtPressure * map = new MinutesAtPressure();
        Layer::CloneInto(map);
        CloneInto(map);
        return map;
    }

    void CloneInto(MinutesAtPressure * layer) {
        mutex.lock();
        timelock.lock();
        layer->m_empty = m_empty;
        layer->m_minimum_height = m_minimum_height;
        layer->m_lastminx = m_lastminx;
        layer->m_lastmaxx = m_lastmaxx;
        layer->times = times;
        layer->chans = chans;
        layer->events = events;
        layer->maxtime = maxtime;
        layer->maxevents = maxevents;
        layer->m_presChannel = m_presChannel;
        layer->m_minpressure = m_minpressure;
        layer->m_maxpressure = m_maxpressure;
        layer->max_mins = max_mins;

        layer->ahis = ahis;

        timelock.unlock();
        mutex.unlock();
    }
protected:
    QMutex timelock;
    QMutex mutex;

    bool m_empty;
    int m_minimum_height;

    qint64 m_lastminx;
    qint64 m_lastmaxx;
    gGraph * m_graph;
    RecalcMAP * m_remap;
    QMap<EventStoreType, int> times;
    QMap<EventStoreType, int> epap_times;
    QList<ChannelID> chans;
    QHash<ChannelID, QMap<EventStoreType, EventDataType> > events;
    int maxtime;
    int maxevents;
    ChannelID m_presChannel;
    EventStoreType m_minpressure;
    EventStoreType m_maxpressure;

    PressureInfo epap, ipap;

    EventDataType max_mins;

    QMap<EventStoreType, EventDataType> ahis;
};

#endif // MINUTESATPRESSURE_H
