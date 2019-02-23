/* gFooBar Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "Graphs/gFooBar.h"

#include <cmath>

#include "Graphs/gGraph.h"
#include "Graphs/gYAxis.h"

gShadowArea::gShadowArea(QColor shadow_color, QColor line_color)
    : Layer(NoChannel), m_shadow_color(shadow_color), m_line_color(line_color)
{
}
gShadowArea::~gShadowArea()
{
}
void gShadowArea::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    float left = region.boundingRect().left()+1.0f;
    float top = region.boundingRect().top()+0.001f;
    float width = region.boundingRect().width();
    float height = region.boundingRect().height();

    if (!m_visible) { return; }

    double xx = w.max_x - w.min_x;

    if (xx == 0) {
        return;
    }

    float start_px = left - 1;
    float end_px = left + width;

    double rmx = w.rmax_x - w.rmin_x;
    double px = ((1.0 / rmx) * (w.min_x - w.rmin_x)) * width;
    double py = ((1.0 / rmx) * (w.max_x - w.rmin_x)) * width;

    painter.fillRect(start_px, top, px, height, QBrush(m_shadow_color));
    painter.fillRect(start_px + py, top, end_px-start_px-py, height, QBrush(m_shadow_color));

    painter.setPen(QPen(m_line_color,2));

    painter.drawLine(QLineF(start_px + px, top, start_px + py, top));
    painter.drawLine(QLineF(start_px + px, top + height + 1, start_px + py, top + height + 1));
}

gFooBar::gFooBar(int offset, QColor handle_color, QColor line_color)
    : Layer(NoChannel), m_offset(offset), m_handle_color(handle_color), m_line_color(line_color)
{
}

gFooBar::~gFooBar()
{
}

void gFooBar::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    Q_UNUSED(painter);
    Q_UNUSED(region);

    if (!m_visible) { return; }

    double xx = w.max_x - w.min_x;

    if (xx == 0) {
        return;
    }

    //int start_px=left;
    //int end_px=left+width;

    //float h=top;

    /*  glLineWidth(1);
      glBegin(GL_LINES);
      w.qglColor(m_line_color);
      glVertex2f(start_px, h);
      glVertex2f(start_px+width, h);
      glEnd();

      double rmx=w.rmax_x-w.rmin_x;
      double px=((1/rmx)*(w.min_x-w.rmin_x))*width;
      double py=((1/rmx)*(w.max_x-w.rmin_x))*width;

      int extra=0;
      if (fabs(px-py)<2) extra=2;

      int hh=25;
      glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
      glEnable(GL_BLEND);
      glBegin(GL_QUADS);

      w.qglColor(m_handle_color);
      glVertex2f(start_px+px-extra,top-hh);
      glVertex2f(start_px+py+extra,top-hh);
      //glColor4ub(255,255,255,128);
      glColor4ub(255,255,255,128);
      glVertex2f(start_px+py+extra,top-hh/2.0);
      glVertex2f(start_px+px-extra,top-hh/2.0);
    //    glColor4ub(255,255,255,128);
      glColor4ub(255,255,255,128);
      glVertex2f(start_px+px-extra,top-hh/2.0);
      glVertex2f(start_px+py+extra,top-hh/2.0);
      w.qglColor(m_handle_color);
    //    glColor4ub(192,192,192,128);
      glVertex2f(start_px+py+extra,h);
      glVertex2f(start_px+px-extra,h);
      glEnd();
      glDisable(GL_BLEND);

      w.qglColor(m_handle_color);
      glBegin(GL_LINE_LOOP);
      glVertex2f(start_px+px-extra,top-hh);
      glVertex2f(start_px+py+extra,top-hh);
      glVertex2f(start_px+py+extra,h);
      glVertex2f(start_px+px-extra,h);
      glEnd();

      glLineWidth(3);
      glBegin(GL_LINES);
      w.qglColor(m_handle_color);
      glVertex2f(start_px+px-extra,h);
      glVertex2f(start_px+py+extra,h);
      glEnd();

      glLineWidth(1);  */

}

