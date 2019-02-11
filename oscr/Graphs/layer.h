/* Graph Layer Implementation
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef graphs_layer_h
#define graphs_layer_h

#include <QKeyEvent>
#include <QMouseEvent>
#include <QRect>
#include <QVector>
#include <QWheelEvent>

#include "SleepLib/common.h"
#include "SleepLib/day.h"
#include "SleepLib/machine_common.h"

class gGraph;
class LayerGroup;

enum LayerPosition { LayerLeft, LayerRight, LayerTop, LayerBottom, LayerCenter, LayerOverlay };

enum ToolTipAlignment { TT_AlignCenter, TT_AlignLeft, TT_AlignRight };

enum LayerType { LT_Other = 0, LT_LineChart, LT_SummaryChart, LT_EventFlags, LT_Spacer, LT_Overview };

/*! \class Layer
    \brief The base component for all individual Graph layers
    */
class Layer
{
    friend class gGraph;
    friend class LayerGroup;

  public:
    Layer(ChannelID code)
        : m_refcount(0),
          m_day(nullptr),
          m_visible(true),
          m_movable(false),
          m_minx(0), m_maxx(0),
          m_miny(0), m_maxy(0),
          m_physminy(0), m_physmaxy(0),
          m_code(code),
          m_width(0), m_height(0),
          m_X(0), m_Y(0),
          m_order(0),
          m_position(LayerCenter),
          m_recalculating(false),
          m_layertype(LT_Other)
    { }

    virtual void recalculate(gGraph * graph) { Q_UNUSED(graph)}
    virtual ~Layer();

    virtual Layer * Clone() { return nullptr; }
    void CloneInto(Layer *);

    //! \brief This gets called on day selection, allowing this layer to precalculate any drawing data
    virtual void SetDay(Day *d);

    //! \brief Set the ChannelID used in this layer
    virtual void SetCode(ChannelID c) { m_code = c; }
    //! \brief Return the ChannelID used in this layer
    const ChannelID & code() { return m_code; }

    const LayerType & layerType() { return m_layertype; }

    //! \brief returns true if this layer contains no data.
    virtual bool isEmpty();

    //! \brief Override and returns true if there are any highlighted components
    virtual bool isSelected() { return false; }

    //! \brief Deselect any highlighted components
    virtual void deselect() { }

    //! \brief Override to set the minimum allowed height for this layer
    virtual int minimumHeight() { return 0; }

    //! \brief Override to set the minimum allowed width for this layer
    virtual int minimumWidth() { return 0; }

    //! \brief Return this layers physical minimum date boundary
    virtual qint64 Minx() { return m_day ? m_day->first() : m_minx; }

    //! \brief Return this layers physical maximum date boundary
    virtual qint64 Maxx() { return m_day ? m_day->last() : m_maxx; }

    //! \brief Return this layers physical minimum Yaxis value
    virtual EventDataType Miny() { return m_miny; }

    //! \brief Return this layers physical maximum Yaxis value
    virtual EventDataType Maxy() { return m_maxy; }

    //! \brief Return this layers physical minimum Yaxis value
    virtual EventDataType physMiny() { return m_physminy; }

    //! \brief Return this layers physical maximum Yaxis value
    virtual EventDataType physMaxy() { return m_physmaxy; }

    //! \brief Set this layers physical minimum date boundary
    virtual void setMinX(qint64 val) { m_minx = val; }

    //! \brief Set this layers physical maximum date boundary
    virtual void setMaxX(qint64 val) { m_maxx = val; }

    //! \brief Set this layers physical minimum Yaxis value
    virtual void setMinY(EventDataType val) { m_miny = val; }

    //! \brief Set this layers physical maximum Yaxis value
    virtual void setMaxY(EventDataType val) { m_maxy = val; }

    //! \brief Set this layers Visibility status
    void setVisible(bool b) { m_visible = b; }

    //! \brief Return this layers Visibility status
    inline bool visible() const { return m_visible; }

    //! \brief Set this layers Moveability status (not really used yet)
    void setMovable(bool b) { m_movable = b; }

    //! \brief Return this layers Moveability status (not really used yet)
    inline bool movable() const { return m_movable; }

    inline bool recalculating() const { return m_recalculating; }

    virtual void dataChanged() {}

    /*! \brief Override this for the drawing code, using GLBuffer components for drawing
        \param gGraph & gv    Graph Object that holds this layer
        \param int left
        \param int top
        \param int width
        \param int height
      */
    virtual void paint(QPainter &painter, gGraph &gv, const QRegion &region) = 0;

    //! \brief Set the layout position and order for this layer.
    void setLayout(LayerPosition position, short width, short height, short order);

    void setPos(short x, short y) { m_X = x; m_Y = y; }

    inline int Width() const { return m_width; }
    inline int Height() const { return m_height; }

    //! \brief Return this Layers Layout Position.
    LayerPosition position() { return m_position; }
    //void X() { return m_X; }
    //void Y() { return m_Y; }

//    //! \brief Draw all this layers custom GLBuffers (ie. the actual OpenGL Vertices)
//    virtual void drawGLBuf(float linesize);

    //! \brief not sure why I needed the reference counting stuff.
    short m_refcount;
    void addref() { m_refcount++; }
    bool unref() {
        m_refcount--;
        return (m_refcount <= 0);
    }

  protected:
//    //! \brief Add a GLBuffer (vertex) object customized to this layer
//    void addGLBuf(GLBuffer *buf) { mgl_buffers.push_back(buf); }
//    void addVertexBuffer(gVertexBuffer *buf) { mv_buffers.push_back(buf); }

    //QRect bounds; // bounds, relative to top of individual graph.
    Day *m_day;
    bool m_visible;
    bool m_movable;
    qint64 m_minx, m_maxx;
    EventDataType m_miny, m_maxy;
    EventDataType m_physminy, m_physmaxy;
    ChannelID m_code;
    short m_width;                   // reserved x pixels needed for this layer.  0==Depends on position..
    short m_height;                  // reserved y pixels needed for this layer.  both 0 == expand to all free area.
    short m_X;                       // offset for repositionable layers..
    short m_Y;
    short m_order;                     // order for positioning..
    LayerPosition m_position;
    QRect m_rect;
    bool m_mouseover;
    volatile bool m_recalculating;
    LayerType m_layertype;
public:

//    //! \brief A vector containing all this layers custom drawing buffers
//    QVector<GLBuffer *> mgl_buffers;
//    QVector<gVertexBuffer *> mv_buffers;

    //! \brief Mouse wheel moved somewhere over this layer
    virtual bool wheelEvent(QWheelEvent *event, gGraph *graph) {
        Q_UNUSED(event);
        Q_UNUSED(graph);
        return false;
    }
    //! \brief Mouse moved somewhere over this layer
    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph) {
        Q_UNUSED(event);
        Q_UNUSED(graph);
        return false;
    }
    //! \brief Mouse left or right button pressed somewhere on this layer
    virtual bool mousePressEvent(QMouseEvent *event, gGraph *graph) {
        Q_UNUSED(event);
        Q_UNUSED(graph);
        return false;
    }
    //! \brief Mouse button released that was originally pressed somewhere on this layer
    virtual bool mouseReleaseEvent(QMouseEvent *event, gGraph *graph) {
        Q_UNUSED(event);
        Q_UNUSED(graph);
        return false;
    }
    //! \brief Mouse button double clicked somewhere on this layer
    virtual bool mouseDoubleClickEvent(QMouseEvent *event, gGraph *graph) {
        Q_UNUSED(event);
        Q_UNUSED(graph);
        return false;
    }
    //! \brief A key was pressed on the keyboard while the graph area was focused.
    virtual bool keyPressEvent(QKeyEvent *event, gGraph *graph) {
        Q_UNUSED(event);
        Q_UNUSED(graph);
        return false;
    }
};

/*! \class LayerGroup
    \brief Contains a list of graph Layer objects
    */
class LayerGroup : public Layer
{
  public:
    LayerGroup()
        : Layer(NoChannel)
    { }

    virtual ~LayerGroup();

    //! \brief Add Layer to this Layer Group
    virtual void AddLayer(Layer *l);

    //! \brief Returns the minimum time value for all Layers contained in this group (milliseconds since epoch)
    virtual qint64 Minx();

    //! \brief Returns the maximum time value for all Layers contained in this group (milliseconds since epoch)
    virtual qint64 Maxx();

    //! \brief Returns the minimum Y-axis value for all Layers contained in this group
    virtual EventDataType Miny();

    //! \brief Returns the maximum Y-axis value for all Layers contained in this group
    virtual EventDataType Maxy();

    //! \brief Check all layers contained and return true if none contain data
    virtual bool isEmpty();

    //! \brief Calls SetDay for all Layers contained in this object
    virtual void SetDay(Day *d);

//    //! \brief Calls drawGLBuf for all Layers contained in this object
//    virtual void drawGLBuf(float linesize);

    //! \brief Return the list of Layers this object holds
    QVector<Layer *> &getLayers() { return layers; }

  protected:
    //! \brief Contains all Layer objects in this group
    QVector<Layer *> layers;

    //! \brief Mouse wheel moved somewhere over this LayerGroup
    virtual bool wheelEvent(QWheelEvent *event, gGraph *graph);

    //! \brief Mouse moved somewhere over this LayerGroup
    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);

    //! \brief Mouse left or right button pressed somewhere on this LayerGroup
    virtual bool mousePressEvent(QMouseEvent *event, gGraph *graph);

    //! \brief Mouse button released that was originally pressed somewhere on this LayerGroup
    virtual bool mouseReleaseEvent(QMouseEvent *event, gGraph *graph);

    //! \brief Mouse button double clicked somewhere on this layerGroup
    virtual bool mouseDoubleClickEvent(QMouseEvent *event, gGraph *graph);

    //! \brief A key was pressed on the keyboard while the graph area was focused.
    virtual bool keyPressEvent(QKeyEvent *event, gGraph *graph);

};

#endif // graphs_layer_h
