/* gYAxis Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GYAXIS_H
#define GYAXIS_H

#include <QImage>
#include "Graphs/layer.h"

/*! \class gXGrid
    \brief Draws the horizintal major/minor grids over graphs
   */
class gXGrid: public Layer
{
  public:
    //! \brief Constructs an gXGrid object with default settings, and col for line colour.
    gXGrid(QColor col = QColor("black"));
    virtual ~gXGrid();

    //! \brief Draw the horizontal lines by adding the to the Vertex GLbuffers
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    //! \brief set the visibility status of Major lines
    void setShowMinorLines(bool b) { m_show_minor_lines = b; }

    //! \brief set the visibility status of Minor lines
    void setShowMajorLines(bool b) { m_show_major_lines = b; }

    //! \brief Returns the visibility status of minor lines
    bool showMinorLines() { return m_show_minor_lines; }

    //! \brief Returns the visibility status of Major lines
    bool showMajorLines() { return m_show_major_lines; }

    virtual Layer * Clone() {
        gXGrid * grid = new gXGrid();
        Layer::CloneInto(grid);
        CloneInto(grid);
        return grid;
    }

    void CloneInto(gXGrid * layer) {
        layer->m_show_major_lines = m_show_major_lines;
        layer->m_show_minor_lines = m_show_minor_lines;
        layer->m_major_color = m_major_color;
        layer->m_minor_color = m_minor_color;
    }

protected:
    bool m_show_major_lines;
    bool m_show_minor_lines;
    QColor m_major_color;
    QColor m_minor_color;
};

/*! \class gYAxis
   \brief Draws the YAxis tick markers, and numeric labels
   */
class gYAxis: public Layer
{
  public:
    //! \brief Left Margin space in pixels
    static const int Margin = 60;

  public:
    //! \brief Construct a gYAxis object, with QColor col for tickers & text
    gYAxis(QColor col = Qt::black);
    virtual ~gYAxis();

    //! \brief Draw the horizontal tickers display
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    //! \brief Sets the visibility status of minor ticks
    void SetShowMinorTicks(bool b) { m_show_minor_ticks = b; }

    //! \brief Sets the visibility status of Major ticks
    void SetShowMajorTicks(bool b) { m_show_major_ticks = b; }

    //! \brief Returns the visibility status of Minor ticks
    bool ShowMinorTicks() { return m_show_minor_ticks; }

    //! \brief Returns the visibility status of Major ticks
    bool ShowMajorTicks() { return m_show_major_ticks; }

    //! \brief Formats the ticker value.. Override to implement other types
    virtual const QString Format(EventDataType v, int dp);

    virtual int minimumWidth();

    //! \brief Set the scale of the Y axis values.. Values can be multiplied by this to convert formats
    void SetScale(float f) { m_yaxis_scale = f; }

    //! \brief Returns the scale of the Y axis values..
    //         Values can be multiplied by this to convert formats
    float Scale() { return m_yaxis_scale; }

  protected:
    bool m_show_minor_ticks;
    bool m_show_major_ticks;
    float m_yaxis_scale;

    QColor m_line_color;
    QColor m_text_color;
    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);
    virtual bool mouseDoubleClickEvent(QMouseEvent *event, gGraph *graph);

    QImage m_image;

    virtual Layer * Clone() {
        gYAxis * yaxis = new gYAxis();
        Layer::CloneInto(yaxis);
        CloneInto(yaxis);
        return yaxis;
    }

    void CloneInto(gYAxis * layer) {
        layer->m_show_major_ticks = m_show_major_ticks;
        layer->m_show_minor_ticks = m_show_minor_ticks;
        layer->m_line_color = m_line_color;
        layer->m_text_color = m_text_color;
        layer->m_image = m_image;
    }

};

/*! \class gYAxisTime
   \brief Draws the YAxis tick markers, and labels in time format
   */
class gYAxisTime: public gYAxis
{
  public:
    //! \brief Construct a gYAxisTime object, with QColor col for tickers & times
    gYAxisTime(bool hr12 = true, QColor col = Qt::black) : gYAxis(col), show_12hr(hr12) {}
    virtual ~gYAxisTime() {}
  protected:
    //! \brief Overrides gYAxis Format to display Time format
    virtual const QString Format(EventDataType v, int dp);

    //! \brief Whether to format as 12 or 24 hour times
    bool show_12hr;

    virtual Layer * Clone() {
        gYAxisTime * yaxis = new gYAxisTime();
        Layer::CloneInto(yaxis);
        CloneInto(yaxis);
        return yaxis;
    }

    void CloneInto(gYAxisTime * layer) {
        gYAxis::CloneInto(layer);
        layer->show_12hr = show_12hr;
    }

};


/*! \class gYAxisWeight
   \brief Draws the YAxis tick markers, and labels in weight format
   */
class gYAxisWeight: public gYAxis
{
  public:
    //! \brief Construct a gYAxisWeight object, with QColor col for tickers & weight values
    gYAxisWeight(UnitSystem us = US_Metric, QColor col = Qt::black) : gYAxis(col), m_unitsystem(us) {}
    virtual ~gYAxisWeight() {}

    //! \brief Returns the current UnitSystem displayed (eg, US_Metric (the rest of the world), US_Archiac (American) )
    UnitSystem unitSystem() { return m_unitsystem; }

    //! \brief Set the unit system displayed by this YTicker
    void setUnitSystem(UnitSystem us) { m_unitsystem = us; }
  protected:
    //! \brief Overrides gYAxis Format to display Time format
    virtual const QString Format(EventDataType v, int dp);
    UnitSystem m_unitsystem;

    virtual Layer * Clone() {
        gYAxisWeight * yaxis = new gYAxisWeight();
        Layer::CloneInto(yaxis);
        CloneInto(yaxis);
        return yaxis;
    }

    void CloneInto(gYAxisWeight * layer) {
        gYAxis::CloneInto(layer);
        layer->m_unitsystem = m_unitsystem;
    }
};


#endif // GYAXIS_H
