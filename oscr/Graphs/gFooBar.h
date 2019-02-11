/* gFooBar Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GFOOBAR_H
#define GFOOBAR_H

#include "Graphs/layer.h"

/*! \class gShadowArea
    \brief Displays a Shadow for all graph areas not highlighted (used in Event Flags)
    */
class gShadowArea: public Layer
{
  public:
    gShadowArea(QColor shadow_color = QColor(40, 40, 40, 40), QColor line_color = Qt::blue);
    virtual ~gShadowArea();

    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

  protected:
    QColor m_shadow_color;
    QColor m_line_color;
};

/*! \class gFooBar
    \brief Was a kind of scrollbar thingy that used to be used for representing the overall graph areas.
    Currently Unused and empty.
    */
class gFooBar: public Layer
{
  public:
    static const int Margin = 15;

  public:
    gFooBar(int offset = 10, QColor handle_color = QColor("orange"),
            QColor line_color = QColor("dark grey"));
    virtual ~gFooBar();

    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

  protected:
    int m_offset;
    QColor m_handle_color;
    QColor m_line_color;
};

#endif // GFOOBAR_H
