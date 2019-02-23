/* Graph Layer Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "Graphs/layer.h"

Layer::~Layer()
{
//    for (int i = 0; i < mgl_buffers.size(); i++) {
//        delete mgl_buffers[i];
//    }

//    for (int i = 0; i < mv_buffers.size(); i++) {
//        delete mv_buffers[i];
//    }
}

//void Layer::drawGLBuf(float linesize)
//{
//    int type;
//    float size;

//    if (!m_visible) { return; }

//    GLBuffer *buf;
//    gVertexBuffer *vb;

//    for (int i = 0; i < mv_buffers.size(); i++) {
//        vb = mv_buffers[i];
//        size = vb->size();
//        type = vb->type();

//        if ((linesize > size) && ((type == GL_LINES) || (type == GL_LINE_LOOP))) {
//            vb->setSize(linesize);
//        }

//        vb->draw();
//        vb->setSize(size);
//    }

//    for (int i = 0; i < mgl_buffers.size(); i++) {
//        buf = mgl_buffers[i];
//        size = buf->size();
//        type = buf->type();

//        if ((linesize > size) && ((type == GL_LINES) || (type == GL_LINE_LOOP))) {
//            buf->setSize(linesize);
//        }

//        buf->draw();
//        //if ((linesize>size) && ((type==GL_LINES) || (type==GL_LINE_LOOP))) {
//        buf->setSize(size);
//        //}
//    }
//}

void Layer::CloneInto(Layer * layer)
{
    layer->m_refcount = m_refcount;
    layer->m_day = m_day;
    layer->m_visible = m_visible;
    layer->m_movable = m_movable;
    layer->m_minx = m_minx;
    layer->m_maxx = m_maxx;
    layer->m_miny = m_miny;
    layer->m_maxy = m_maxy;
    layer->m_physmaxy = m_physmaxy;
    layer->m_physminy = m_physminy;
    layer->m_code = m_code;
    layer->m_width = m_width;
    layer->m_height = m_height;
    layer->m_X = m_X;
    layer->m_Y = m_Y;
    layer->m_order = m_order;
    layer->m_position = m_position;
    layer->m_rect = m_rect;
    layer->m_mouseover = m_mouseover;
    layer->m_recalculating = m_recalculating;
    layer->m_layertype = m_layertype;
}

void Layer::SetDay(Day *d)
{
    m_day = d;
    if (d) {
        m_minx = d->first(m_code);
        m_maxx = d->last(m_code);
        m_miny = d->Min(m_code);
        m_maxy = d->Max(m_code);
    } else { m_day = nullptr; }

}

bool Layer::isEmpty()
{
    //if (m_day && (m_day->count(m_code)>0))
    if (m_day && (m_day->channelExists(m_code))) {
        return false;
    }

    return true;
}
void Layer::setLayout(LayerPosition position, short width, short height, short order)
{
    m_position = position;
    m_width = width;
    m_height = height;
    m_order = order;
}

LayerGroup::~LayerGroup()
{
    for (int i = 0; i < layers.size(); i++) {
        delete layers[i];
    }
}
bool LayerGroup::isEmpty()
{
    if (!m_day) {
        return true;
    }

    bool empty = true;

    for (int i = 0; i < layers.size(); i++) {
        if (layers[i]->isEmpty()) {
            empty = false;
            break;
        }
    }

    return empty;
}
//void LayerGroup::drawGLBuf(float linesize)
//{
//    Layer::drawGLBuf(linesize);

//    for (int i = 0; i < layers.size(); i++) {
//        layers[i]->drawGLBuf(linesize);
//    }
//}

void LayerGroup::SetDay(Day *d)
{
    m_day = d;

    for (int i = 0; i < layers.size(); i++) {
        layers[i]->SetDay(d);
    }
}

void LayerGroup::AddLayer(Layer *l)
{
    layers.push_back(l);
    l->addref();
}

qint64 LayerGroup::Minx()
{
    bool first = true;
    qint64 m = 0, t;

    for (int i = 0; i < layers.size(); i++)  {
        t = layers[i]->Minx();

        if (!t) { continue; }

        if (first) {
            m = t;
            first = false;
        } else if (m > t) { m = t; }
    }

    return m;
}
qint64 LayerGroup::Maxx()
{
    bool first = true;
    qint64 m = 0, t;

    for (int i = 0; i < layers.size(); i++)  {
        t = layers[i]->Maxx();

        if (!t) { continue; }

        if (first) {
            m = t;
            first = false;
        } else if (m < t) { m = t; }
    }

    return m;
}

EventDataType LayerGroup::Miny()
{
    bool first = true;
    EventDataType m = 0, t;

    for (int i = 0; i < layers.size(); i++)  {
        t = layers[i]->Miny();

        if (t == layers[i]->Maxy()) { continue; }

        if (first) {
            m = t;
            first = false;
        } else {
            if (m > t) { m = t; }
        }
    }

    return m;
}

EventDataType LayerGroup::Maxy()
{
    bool first = true;
    EventDataType m = 0, t;

    for (int i = 0; i < layers.size(); i++)  {
        t = layers[i]->Maxy();

        if (t == layers[i]->Miny()) { continue; }

        if (first) {
            m = t;
            first = false;
        } else if (m < t) { m = t; }
    }

    return m;
}

//! \brief Mouse wheel moved somewhere over this layer
bool LayerGroup::wheelEvent(QWheelEvent *event, gGraph *graph)
{
    for (int i = 0; i < layers.size(); i++)
        if (layers[i]->wheelEvent(event, graph)) {
            return true;
        }

    return false;
}

//! \brief Mouse moved somewhere over this layer
bool LayerGroup::mouseMoveEvent(QMouseEvent *event, gGraph *graph)
{
    for (int i = 0; i < layers.size(); i++)
        if (layers[i]->mouseMoveEvent(event, graph)) { return true; }

    return false;
}

//! \brief Mouse left or right button pressed somewhere on this layer
bool LayerGroup::mousePressEvent(QMouseEvent *event, gGraph *graph)
{
    for (int i = 0; i < layers.size(); i++)
        if (layers[i]->mousePressEvent(event, graph)) { return true; }

    return false;
}

//! \brief Mouse button released that was originally pressed somewhere on this layer
bool LayerGroup::mouseReleaseEvent(QMouseEvent *event, gGraph *graph)
{
    for (int i = 0; i < layers.size(); i++)
        if (layers[i]->mouseReleaseEvent(event, graph)) { return true; }

    return false;
}

//! \brief Mouse button double clicked somewhere on this layer
bool LayerGroup::mouseDoubleClickEvent(QMouseEvent *event, gGraph *graph)
{
    for (int i = 0; i < layers.size(); i++)
        if (layers[i]->mouseDoubleClickEvent(event, graph)) { return true; }

    return false;
}

//! \brief A key was pressed on the keyboard while the graph area was focused.
bool LayerGroup::keyPressEvent(QKeyEvent *event, gGraph *graph)
{
    for (int i = 0; i < layers.size(); i++)
        if (layers[i]->keyPressEvent(event, graph)) { return true; }

    return false;
}
