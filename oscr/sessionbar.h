/* SessionBar Graph Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SESSIONBAR_H
#define SESSIONBAR_H

#include <QVector>
#include <QColor>
#include <QWidget>
#include <QTimer>

#include "SleepLib/session.h"

typedef qint64 SegType;

class SBSeg
{
  public:
    SBSeg();
    SBSeg(Session *sess, QColor col);
    //    SBSeg(const SBSeg & a);

    QColor color;
    bool highlight;
    Session *session;
};


class SessionBar : public QWidget
{
    Q_OBJECT
  public:
    SessionBar(QWidget *parent = 0);
    //    // Q_DECLARE_METATYPE requires a copy-constructor
    //  SessionBar(const SessionBar &);

    virtual ~SessionBar();
    void clear() { segments.clear(); m_selectIDX = -1; }
    void add(Session *sess, QColor col) { if (sess) { segments.push_back(SBSeg(sess, col)); } }
    void setSelectMode(bool b) { m_selectMode = b; }
    void setSelectColor(QColor col) { m_selectColor = col; }
    int count() { return segments.size(); }
    int selected() { return m_selectIDX; }
    Session * session(int idx);
    void setSelected(int idx) { m_selectIDX = idx; }

  protected slots:
    void updateTimer();
  signals:
    void sessionClicked(Session *sess);
  protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    SegType min();
    SegType max();

    QVector<SBSeg> segments;
    QTimer timer;
    int m_selectIDX;
    bool m_selectMode;
    QColor m_selectColor;
};
//Q_DECLARE_METATYPE(SessionBar)

#endif
