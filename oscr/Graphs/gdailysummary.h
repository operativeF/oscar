/* gDailySummary Graph Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GDAILYSUMMARY_H
#define GDAILYSUMMARY_H

#include "Graphs/layer.h"
#include "SleepLib/day.h"

class gDailySummary:public Layer
{
public:
    gDailySummary();
    virtual ~gDailySummary() {}

    virtual void SetDay(Day *d);

    virtual bool isEmpty();

    //! Draw filled rectangles behind Event Flag's, and an outlines around them all, Calls the individual paint for each gFlagLine
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    virtual int minimumHeight() { return m_minimum_height; }
    bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);
    bool mousePressEvent(QMouseEvent *event, gGraph *graph);
    bool mouseReleaseEvent(QMouseEvent *event, gGraph *graph);

protected:
    QList<QString> flag_values;
    QList<QString> flag_labels;
    QList<ChannelID> flag_codes;
    QList<QColor> flag_foreground;
    QList<QColor> flag_background;


    QList<ChannelID> pie_chan;
    QList<EventDataType> pie_data;
    QList<QString> pie_labels;
    EventDataType pie_total;

    QList<QString> settings;
    QList<QString> info;
    QList<QColor> info_background;
    QList<QColor> info_foreground;

    float flag_height;
    float flag_label_width;
    float flag_value_width;

    double ahi;

    int info_height;
    int info_width;

    int m_minimum_height;
    bool m_empty;
};

#endif // GDAILYSUMMARY_H
