/* gLineChart Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GLINECHART_H
#define GLINECHART_H

#include <QPainter>
#include <QVector>

#include "Graphs/layer.h"
#include "SleepLib/event.h"
#include "SleepLib/day.h"
#include "Graphs/gLineOverlay.h"

enum DottedLineCalc {
    DLC_Zero, DLC_Min, DLC_Mid, DLC_Perc, DLC_Max, DLC_UpperThresh, DLC_LowerThresh
};

QColor darken(QColor color, float p = 0.5);

struct DottedLine {
public:
    DottedLine() {
        code = NoChannel;
        type = Calc_Zero;
        value = 0;
        visible = false;
        available = false;
    }
    DottedLine(const DottedLine & copy) {
        code = copy.code;
        type = copy.type;
        value = copy.value;
        available = copy.available;
        visible = copy.visible;
    }
    DottedLine(ChannelID code, ChannelCalcType type, bool available = false):
        code(code), type(type), available(available) {}

    EventDataType calc(Day * day) {
        if (day == nullptr) {
            qWarning() << "DottedLine::calc called with null day object";
            return 0;
        }

        available = day->channelExists(code);
        value = day->calc(code, type);
        return value;
    }

    ChannelID code;
    ChannelCalcType type;
    EventDataType value;
    bool visible;
    bool available;
};
QDataStream & operator<<(QDataStream &, const DottedLine &);
QDataStream & operator>>(QDataStream &, DottedLine &);


/*! \class gLineChart
    \brief Draws a 2D linechart from all Session data in a day. EVL_Waveforms typed EventLists are accelerated.
    */
class gLineChart: public Layer
{
    friend class gGraphView;
  public:
    /*! \brief Creates a new 2D gLineChart Layer
        \param code  The Channel that gets drawn by this layer
        \param square_plot Whether or not to use square plots (only effective for EVL_Event typed EventList data)
        \param disable_accel Whether or not to disable acceleration for EVL_Waveform typed EventList data
        */
    gLineChart(ChannelID code, bool square_plot = false, bool disable_accel = false);
    virtual ~gLineChart();

    //! \brief The drawing code that fills the vertex buffers
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    //! \brief Set Use Square plots for non EVL_Waveform data
    void SetSquarePlot(bool b) { m_square_plot = b; }

    //! \brief Returns true if using Square plots for non EVL_Waveform data
    bool GetSquarePlot() { return m_square_plot; }

    //! \brief Set this if you want this layer to draw it's empty data message
    //! They don't show anymore due to the changes in gGraphView. Should modify isEmpty if this still gets to live
    void ReportEmpty(bool b) { m_report_empty = b; }

    //! \brief Returns whether or not to show the empty text message
    bool GetReportEmpty() { return m_report_empty; }

    //! \brief Sets the ability to Disable waveform plot acceleration (which hides unseen data)
    void setDisableAccel(bool b) { m_disable_accel = b; }

    //! \brief Returns true if waveform plot acceleration is disabled
    bool disableAccel() { return m_disable_accel; }

    //! \brief Sets the Day object containing the Sessions this linechart draws from
    virtual void SetDay(Day *d);

    //! \brief Returns Minimum Y-axis value for this layer
    virtual EventDataType Miny();

    //! \brief Returns Maximum Y-axis value for this layer
    virtual EventDataType Maxy();

    //! \brief Returns true if all subplots contain no data
    virtual bool isEmpty();

    //! \brief Add Subplot 'code'. Note the first one is added in the constructor.
    void addPlot(ChannelID code, bool square=false) { m_codes.push_back(code); m_enabled[code] = true; m_square.push_back(square); }

    //! \brief Returns true of the subplot 'code' is enabled.
    bool plotEnabled(ChannelID code) { if ((m_enabled.contains(code)) && m_enabled[code]) { return true; } else { return false; } }

    //! \brief Enable or Disable the subplot identified by code.
    void setPlotEnabled(ChannelID code, bool b) { m_enabled[code] = b; }

    QString getMetaString(qint64 time);

    void addDotLine(DottedLine dot) { m_dotlines.append(dot); }
    QVector<DottedLine> m_dotlines;
    QHash<ChannelID, bool> m_flags_enabled;
    QHash<ChannelID, QHash<quint32, bool> > m_dot_enabled;

    virtual Layer * Clone() {
        gLineChart * lc = new gLineChart(m_code);
        Layer::CloneInto(lc);
        CloneInto(lc);
        return lc;
    }

    void CloneInto(gLineChart * layer) {
        layer->m_dotlines = m_dotlines;
        layer->m_flags_enabled = m_flags_enabled;
        layer->m_dot_enabled = m_dot_enabled;
        layer->m_report_empty = m_report_empty;
        layer->m_square_plot = m_square_plot;
        layer->m_disable_accel = m_disable_accel;
        layer->subtract_offset = subtract_offset;
        layer->m_codes = m_codes;
        layer->m_threshold = m_threshold;
        layer->m_square = m_square;
        layer->m_enabled = m_enabled;
        layer->lines = lines;
        layer->lasttext = lasttext;
        layer->lasttime = lasttime;
    }


  protected:
    //! \brief Mouse moved over this layers area (shows the hover-over tooltips here)
    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);


    bool m_report_empty;
    bool m_square_plot;
    bool m_disable_accel;

    //! \brief Used by accelerated waveform plots. Must be >= Screen Resolution (or at least graph width)
    static const int max_drawlist_size = 10000;

    //! \brief The list of screen points used for accelerated waveform plots..
    QPointF m_drawlist[max_drawlist_size];

    int subtract_offset;

    QVector<ChannelID> m_codes;
    QStringList m_threshold;
    QVector<bool> m_square;
    QHash<ChannelID, bool> m_enabled; // plot enabled
    QHash<ChannelID, gLineOverlayBar *> flags;

    QVector<QLine> lines;

    QString lasttext;
    qint64 lasttime;
};

#endif // GLINECHART_H
