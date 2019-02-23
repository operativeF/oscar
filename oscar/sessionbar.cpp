/* SessionBar Graph Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QPainter>
#include <QLinearGradient>
#include <QBrush>
#include <QRect>
#include <QMouseEvent>
#include <QDebug>

#include "sessionbar.h"

SBSeg::SBSeg()
{
    session = nullptr;
    color = QColor();
    highlight = false;
}

SBSeg::SBSeg(Session *sess, QColor col)
{
    session = sess;
    color = col;
    highlight = false;
}

//SBSeg::SBSeg(const SBSeg & a)
//{
//    session=(Session *)a.session;
//    color=a.color;
//    highlight=a.highlight;
//}

SessionBar::SessionBar(QWidget *parent) :
    QWidget(parent)
{
    timer.setParent(this);
    m_selectIDX = -1;
    m_selectColor = Qt::red;
    m_selectMode = false;
}
//SessionBar::SessionBar(const SessionBar & copy)
//    :QWidget(this)
//{
//    timer.setParent(this);
//    QVector<SBSeg>::const_iterator i;
//    for (i=copy.segments.begin();i!=copy.segments.end();++i) {
//        segments.push_back(*i);
//    }
//}

SessionBar::~SessionBar()
{
}
void SessionBar::updateTimer()
{
    if (!underMouse()) {
        QVector<SBSeg>::iterator i;

        for (i = segments.begin(); i != segments.end(); ++i) {
            (*i).highlight = false;
        }
    } else {
        timer.singleShot(50, this, SLOT(updateTimer()));
    }

    update();
}
Session * SessionBar::session(int idx)
{
    if (idx >= segments.size()) {
        qCritical() << "SessionBar::session called with out of range index";
        return nullptr;
    }
    return segments[idx].session;
}

SegType SessionBar::min()
{
    if (segments.isEmpty()) {
        return 0;
    }

    QVector<SBSeg>::iterator i = segments.begin();
    SegType min = (*i).session->first();
    i++;

    qint64 val;

    for (; i != segments.end(); ++i) {
        val = (*i).session->first();

        if (min > val) {
            min = val;
        }
    }

    return min;
}

SegType SessionBar::max()
{
    if (segments.isEmpty()) {
        return 0;
    }

    QVector<SBSeg>::iterator i = segments.begin();
    SegType max = (*i).session->last();
    i++;
    qint64 val;

    for (; i != segments.end(); ++i) {
        val = (*i).session->last();

        if (max < val) {
            max = val;
        }
    }

    return max;
}

QColor brighten(QColor, float f);

void SessionBar::mousePressEvent(QMouseEvent *ev)
{
    SegType mn = min();
    SegType mx = max();

    if (mx < mn) {
        return;
    }

    SegType total = mx - mn;
    double px = double(width() ) / double(total);

    double sx, ex;
    QVector<SBSeg>::iterator i;

    int cnt = 0;

    for (i = segments.begin(); i != segments.end(); ++i) {
        Session *sess = (*i).session;
        sx = double(sess->first() - mn) * px;
        ex = double(sess->last() - mn) * px;

        if (ex > width()) { ex = width(); }

        //ex-=sx;

        if ((ev->x() >= sx) && (ev->x() < ex)
                && (ev->y() > 0) && (ev->y() < height())) {
            m_selectIDX = cnt;
            emit sessionClicked((*i).session);
            break;
        }

        cnt++;
    }

    if (timer.isActive()) { timer.stop(); }

    timer.singleShot(50, this, SLOT(updateTimer()));

}

void SessionBar::mouseMoveEvent(QMouseEvent *ev)
{
    SegType mn = min();
    SegType mx = max();

    if (mx < mn) {
        return;
    }

    SegType total = mx - mn;
    double px = double(width() - 5) / double(total);

    double sx, ex;
    QVector<SBSeg>::iterator i;

    for (i = segments.begin(); i != segments.end(); ++i) {
        SBSeg &seg = *i;
        sx = double(seg.session->first() - mn) * px;
        ex = double(seg.session->last() - mn) * px;

        if (ex > width() - 5) { ex = width() - 5; }

        //ex-=sx;

        if ((ev->x() > sx) && (ev->x() < ex)
                && (ev->y() > 0) && (ev->y() < height())) {
            seg.highlight = true;
        } else { seg.highlight = false; }
    }

    if (timer.isActive()) { timer.stop(); }

    timer.singleShot(50, this, SLOT(updateTimer()));
}


void SessionBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QRect rect(1, 1, width() - 2, height() - 2);

    painter.drawRect(rect);

    SegType mn = min();
    SegType mx = max();

    if (mx < mn) {
        return;
    }

    SegType total = mx - mn;
    double px = double(width() - 5) / double(total);

    double sx, ex;
    QVector<SBSeg>::iterator i;
    //QRect selectRect;

    int cnt = 0;
    for (i = segments.begin(); i != segments.end(); ++i) {
        SBSeg &seg = *i;
        qint64 mm = seg.session->first(), MM = seg.session->last(), L = MM - mm;
        sx = double(mm - mn) * px;
        ex = double(MM - mn) * px;

        if (ex > width() - 5) { ex = width() - 5; }

        ex -= sx;

        int len = L / 1000L;
        int h = len / 3600;
        int m = (len / 60) % 60;
        //int s=len % 60;

        QString msg = tr("%1h %2m").arg((short)h, 1, 10, QChar('0')).arg((short)m, 1, 10, QChar('0'));
        //painter.setBrush(QBrush((*i).color);
        QRect segrect(3 + sx, 3, ex, height() - 6);

        if (seg.session->enabled()) {
            QLinearGradient linearGrad(QPointF(0, 0), QPointF(0, height() / 2));
            linearGrad.setSpread(QGradient::ReflectSpread);
            QColor col = seg.color;
            if (m_selectMode && (cnt == m_selectIDX)) {
//                col = m_selectColor;
            } else if (seg.highlight) { col = brighten(col); }

            linearGrad.setColorAt(0, col);
            linearGrad.setColorAt(1, brighten(col));
            QBrush brush(linearGrad);
            painter.fillRect(segrect, brush);
        } else {
            if (seg.highlight) {
                QColor col = QColor("#f0f0f0");
                painter.fillRect(segrect, col);
            }

            //msg="Off";
        }

        QRect rect = painter.boundingRect(segrect, Qt::AlignCenter, msg);

        if (rect.width() < segrect.width()) {
            painter.setPen(Qt::black);
            painter.drawText(segrect, Qt::AlignCenter, msg);
        }


        if (m_selectMode && (cnt == m_selectIDX)) {
            painter.setPen(QPen(m_selectColor, 3));
        } else {
            painter.setPen(QPen(Qt::black, 1));
        }
        painter.drawRect(segrect);
        cnt++;

    }
    if (!cnt) {
        QString msg = tr("No Sessions Present");
        QRect rct = painter.boundingRect(this->rect(), Qt::AlignCenter, msg);
        painter.setPen(Qt::black);
        painter.drawText(rct, Qt::AlignCenter, msg);
    }
}
