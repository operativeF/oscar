/* gXAxis Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GXAXIS_H
#define GXAXIS_H

#include <QImage>
#include <QPixmap>
#include "Graphs/layer.h"

/*! \class gXAxis
    \brief Draws the XTicker timescales underneath graphs */
class gXAxis: public Layer
{
  public:
    static const int Margin = 30; // How much room does this take up. (Bottom margin)

  public:
    gXAxis(QColor col = Qt::black, bool fadeout = true);
    virtual ~gXAxis();

    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);
    void SetShowMinorLines(bool b) { m_show_minor_lines = b; }
    void SetShowMajorLines(bool b) { m_show_major_lines = b; }
    bool ShowMinorLines() { return m_show_minor_lines; }
    bool ShowMajorLines() { return m_show_major_lines; }
    void SetShowMinorTicks(bool b) { m_show_minor_ticks = b; }
    void SetShowMajorTicks(bool b) { m_show_major_ticks = b; }
    bool ShowMinorTicks() { return m_show_minor_ticks; }
    bool ShowMajorTicks() { return m_show_major_ticks; }
    void setUtcFix(bool b) { m_utcfix = b; }

    void setRoundDays(bool b) { m_roundDays = b; }

    //! \brief Returns the minimum height needed to fit
    virtual int minimumHeight();

    virtual Layer * Clone() {
        gXAxis * xaxis = new gXAxis();
        Layer::CloneInto(xaxis);
        CloneInto(xaxis);
        return xaxis;
    }

    void CloneInto(gXAxis * layer) {
        layer->m_show_major_ticks = m_show_major_ticks;
        layer->m_show_minor_ticks = m_show_minor_ticks;
        layer->m_show_major_lines = m_show_major_lines;
        layer->m_show_minor_lines = m_show_minor_lines;
        layer->m_major_color = m_major_color;
        layer->m_minor_color = m_minor_color;
        layer->m_line_color = m_line_color;
        layer->m_text_color = m_text_color;

        layer->m_utcfix = m_utcfix;
        layer->m_fadeout = m_fadeout;

        layer->tz_offset = tz_offset;
        layer->tz_hours = tz_hours;

        layer->m_image = m_image;
        layer->m_roundDays = m_roundDays;
    }


  protected:
    bool m_show_major_lines;
    bool m_show_minor_lines;
    bool m_show_minor_ticks;
    bool m_show_major_ticks;

    bool m_utcfix;

    QColor m_line_color;
    QColor m_text_color;
    QColor m_major_color;
    QColor m_minor_color;
    bool m_fadeout;
    qint64 tz_offset;
    float tz_hours;

    QImage m_image;

    bool m_roundDays;
};

class gXAxisDay: public Layer
{
  public:
    static const int Margin = 30; // How much room does this take up. (Bottom margin)

  public:
    gXAxisDay(QColor col = Qt::black);
    virtual ~gXAxisDay();

    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);
    void SetShowMinorLines(bool b) { m_show_minor_lines = b; }
    void SetShowMajorLines(bool b) { m_show_major_lines = b; }
    bool ShowMinorLines() { return m_show_minor_lines; }
    bool ShowMajorLines() { return m_show_major_lines; }
    void SetShowMinorTicks(bool b) { m_show_minor_ticks = b; }
    void SetShowMajorTicks(bool b) { m_show_major_ticks = b; }
    bool ShowMinorTicks() { return m_show_minor_ticks; }
    bool ShowMajorTicks() { return m_show_major_ticks; }

    //! \brief Returns the minimum height needed to fit
    virtual int minimumHeight();

    virtual Layer * Clone() {
        gXAxisDay * xaxis = new gXAxisDay();
        Layer::CloneInto(xaxis);
        CloneInto(xaxis);
        return xaxis;
    }

    void CloneInto(gXAxisDay * layer) {
        layer->m_show_major_ticks = m_show_major_ticks;
        layer->m_show_minor_ticks = m_show_minor_ticks;
        layer->m_show_major_lines = m_show_major_lines;
        layer->m_show_minor_lines = m_show_minor_lines;
        layer->m_major_color = m_major_color;
        layer->m_minor_color = m_minor_color;
        layer->m_line_color = m_line_color;
        layer->m_text_color = m_text_color;

        layer->m_image = m_image;
    }


  protected:
    bool m_show_major_lines;
    bool m_show_minor_lines;
    bool m_show_minor_ticks;
    bool m_show_major_ticks;

    QColor m_line_color;
    QColor m_text_color;
    QColor m_major_color;
    QColor m_minor_color;

    QImage m_image;

};

class gXAxisPressure: public Layer
{
public:
  static const int Margin = 30; // How much room does this take up. (Bottom margin)

public:
  gXAxisPressure(QColor col = Qt::black);
  virtual ~gXAxisPressure();

  virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    virtual int minimumHeight();

    virtual Layer * Clone() {
        gXAxisPressure * xaxis = new gXAxisPressure();
        Layer::CloneInto(xaxis);
        CloneInto(xaxis);
        return xaxis;
    }

    void CloneInto(gXAxisPressure * layer) {
        layer->m_show_major_ticks = m_show_major_ticks;
        layer->m_show_minor_ticks = m_show_minor_ticks;
        layer->m_show_major_lines = m_show_major_lines;
        layer->m_show_minor_lines = m_show_minor_lines;
        layer->m_major_color = m_major_color;
        layer->m_minor_color = m_minor_color;
        layer->m_line_color = m_line_color;
        layer->m_text_color = m_text_color;

        //layer->m_image = m_image;
    }

protected:
    bool m_show_major_lines;
    bool m_show_minor_lines;
    bool m_show_minor_ticks;
    bool m_show_major_ticks;

    QColor m_line_color;
    QColor m_text_color;
    QColor m_major_color;
    QColor m_minor_color;
};

#endif // GXAXIS_H
