/* gGraph Header
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef graphs_ggraph_h
#define graphs_ggraph_h

//#define DEBUG_LAYOUT

#include <QFont>
#include <QObject>
#include <QPixmap>
#include <QResizeEvent>
#include <QString>
#include <QPainter>

#include "Graphs/glcommon.h"
#include "Graphs/layer.h"

class gGraphView;

// Graph globals.
extern QFont *defaultfont;
extern QFont *mediumfont;
extern QFont *bigfont;
extern QHash<QString, QImage *> images;

bool InitGraphGlobals();
void DestroyGraphGlobals();

const int mouse_movement_threshold = 6;

float CatmullRomSpline(float p0, float p1, float p2, float p3, float t = 0.5);

inline void GetTextExtent(QString text, int &width, int &height, QFont *font)
{
    QFontMetrics fm(*font);
    QRect r = fm.boundingRect(text);
    width = r.width();
    height = r.height();
}

/*! \class gGraph
    \brief Single Graph object, containing multiple layers and Layer layout code
    */
class gGraph : public QObject
{
    Q_OBJECT
  public:
    friend class gGraphView;

    /*! \brief Creates a new graph object
        \param gGraphView * graphview if not null, links the graph to that object
        \param QString title containing the graph Title which is rendered vertically
        \param int height containing the opening height for this graph
        \param short group containing which graph-link group this graph belongs to
        */
    gGraph(QString name, gGraphView *graphview = nullptr, QString title = "", QString units = "",
           int height = 0, short group = 0);
    virtual ~gGraph();

    //! \brief Tells all Layers to deselect any highlighting
    void deselect();

    //! \brief Returns true if any Layers have anything highlighted
    bool isSelected();

    //! \brief Starts the singleshot Timer running, for ms milliseconds
    void Trigger(int ms);

    /*! \fn QPixmap renderPixmap(int width, int height, float fontscale=1.0);
        \brief Returns a QPixmap containing a snapshot of the graph rendered at size widthxheight
        \param int width    Width of graph 'screenshot'
        \param int height   Height of graph 'screenshot'
        \param float fontscale Scaling value to adjust DPI (when used for HighRes printing)

        Note if width or height is more than the OpenGL system allows, it could result in a crash
        Keeping them under 2048 is a reasonably safe value.
        */
    QPixmap renderPixmap(int width, int height, bool printing = false);

    //! \brief Set Graph visibility status
    void setVisible(bool b) { m_visible = b; }

    //! \brief Return Graph visibility status
    bool visible() { return m_visible; }

    //! \brief Returns true if Graph is currently being snapshotted for printing
    bool printing() { return m_printing; }

    //! \brief Return height element. This is used by the scaler in gGraphView.
    inline const float & height() { return m_height; }

    //! \brief Set the height element. (relative to the total of all heights)
    void setHeight(float height) {
        m_height = height;
        invalidate_yAxisImage = true;
    }

    //! \brief Return minimum height this graph is allowed to (considering layer preferences too)
    int minHeight();
    void setMinHeight(int height) { m_min_height = height; }

    int maxHeight() { return m_max_height; }
    void setMaxHeight(int height) { m_max_height = height; }

    //! \brief Returns true if the vertical graph title is shown
    bool showTitle() { return m_showTitle; }

    //! \brief Set whether or not to render the vertical graph title
    void setShowTitle(bool b) { m_showTitle = b; }

    //! \brief Returns printScaleX, used for DPI scaling in report printing
    float printScaleX();

    //! \brief Returns printScaleY, used for DPI scaling in report printing
    float printScaleY();

    //! \brief Returns true if none of the included layers have data attached
    bool isEmpty();

    //! \brief Returns true if the user is currently dragging the mouse to select an area
    bool selectingArea();

    double currentTime() const;
    void setCurrentTime(double value) { m_currentTime = value; }



    //! \brief Add Layer l to graph object, allowing you to specify position,
    //         margin sizes, order, movability status and offsets
    void AddLayer(Layer *l, LayerPosition position = LayerCenter,
                  short pixelsX = 0, short pixelsY = 0, short order = 0,
                  bool movable = false, short x = 0, short y = 0);

    //! \brief Queues text for gGraphView object to draw it.
    void renderText(QString text, int x, int y, float angle = 0.0, QColor color = Qt::black, QFont *font = defaultfont, bool antialias = true);

    //! \brief Queues text for gGraphView object to draw it, using given rect.
    void renderText(QString text, QRectF rect, quint32 flags = Qt::AlignCenter, float angle = 0.0, QColor color = Qt::black, QFont *font = defaultfont, bool antialias = true);

    //! \brief Rounds Y scale values to make them look nice..
    //         Applies the Graph Preference min/max settings.
    void roundY(EventDataType &miny, EventDataType &maxy);

//    //! \brief Process all Layers GLBuffer (Vertex) objects, drawing the actual OpenGL stuff.
//    void drawGLBuf();

    //! \brief Returns the Graph's (vertical) title
    inline QString & title() { return m_title; }

    //! \brief Returns the Graph's internal name
    inline QString & name() { return m_name; }


    //! \brief Sets the Graph's (vertical) title
    void setTitle(const QString title) { m_title = title; }

    //! \brief Returns the measurement Units the Y scale is referring to
    QString units() { return m_units; }

    //! \brief Sets the measurement Units the Y scale is referring to
    void setUnits(const QString units) { m_units = units; }

    //virtual void repaint(); // Repaint individual graph..

    //! \brief Resets the graphs X & Y dimensions based on the Layer data
    virtual void ResetBounds();

    //! \brief Sets the time range selected for this graph (in milliseconds since 1970 epoch)
    virtual void SetXBounds(qint64 minx, qint64 maxx);

    //! \brief Returns the physical Minimum time for all layers contained (in milliseconds since epoch)
    virtual qint64 MinX();

    //! \brief Returns the physical Maximum time for all layers contained (in milliseconds since epoch)
    virtual qint64 MaxX();

    //! \brief Returns the physical Minimum Y scale value for all layers contained
    virtual EventDataType MinY();

    //! \brief Returns the physical Maximum Y scale value for all layers contained
    virtual EventDataType MaxY();

    //! \brief Returns the physical Minimum Y scale value for all layers contained
    virtual EventDataType physMinY();

    //! \brief Returns the physical Maximum Y scale value for all layers contained
    virtual EventDataType physMaxY();

    //! \brief Sets the physical start of this graphs time range (in milliseconds since epoch)
    virtual void SetMinX(qint64 v);

    //! \brief Sets the physical end of this graphs time range (in milliseconds since epoch)
    virtual void SetMaxX(qint64 v);

    //! \brief Sets the physical Minimum Y scale value used while drawing this graph
    virtual void SetMinY(EventDataType v);

    //! \brief Sets the physical Maximum Y scale value used while drawing this graph
    virtual void SetMaxY(EventDataType v);

    //! \brief Forces Y Minimum to always select this value
    virtual void setForceMinY(EventDataType v) { f_miny = v; m_enforceMinY = true; }
    //! \brief Forces Y Maximum to always select this value
    virtual void setForceMaxY(EventDataType v) { f_maxy = v; m_enforceMaxY = true; }


    virtual EventDataType forceMinY() { return rec_miny; }
    virtual EventDataType forceMaxY() { return rec_maxy; }

    //! \brief Set recommended Y minimum.. It won't go under this unless the data does.
    //         It won't go above this.
    virtual void setRecMinY(EventDataType v) { rec_miny = v; }
    //! \brief Set recommended Y minimum.. It won't go above this unless the data does.
    //         It won't go under this.
    virtual void setRecMaxY(EventDataType v) { rec_maxy = v; }

    //! \brief Returns the recommended Y minimum.. It won't go under this unless the data does.
    //         It won't go above this.
    virtual EventDataType RecMinY() { return rec_miny; }
    //! \brief Returns the recommended Y maximum.. It won't go under this unless the data does.
    //         It won't go above this.
    virtual EventDataType RecMaxY() { return rec_maxy; }

    //! \brief Called when main graph area is resized
    void resize(int width, int height);      // margin recalcs..

    qint64 max_x, min_x, rmax_x, rmin_x;
    EventDataType max_y, min_y, rmax_y, rmin_y, f_miny, f_maxy, rec_miny, rec_maxy;
    EventDataType rphysmin_y, rphysmax_y;

    // not sure why there's two.. I can't remember
    void setEnforceMinY(bool b) { m_enforceMinY = b; }
    void setEnforceMaxY(bool b) { m_enforceMaxY = b; }

    //! \brief Returns whether this graph shows overall timescale, or a zoomed area
    bool blockZoom() { return m_blockzoom; }
    //! \brief Sets whether this graph shows an overall timescale, or a zoomed area.
    void setBlockZoom(bool b) { m_blockzoom = b; }

    //! \brief Flips the GL coordinates from the graphs perspective.. Used in Scissor calculations
    int flipY(int y); // flip GL coordinates

    //! \brief Returns the graph-linking group this Graph belongs in
    short group() { return m_group; }

    //! \brief Sets the graph-linking group this Graph belongs in
    void setGroup(short group) { m_group = group; }

    //! \brief Forces the main gGraphView object to draw all Text Components
    void DrawTextQue(QPainter &painter);

    //! \brief Sends supplied day object to all Graph layers so they can precalculate stuff
    void setDay(Day *day);

    //! \brief Returns the current day object
    Day *day() { return m_day; }

    //! \brief The Layer, layout and title drawing code
    virtual void paint(QPainter &painter, const QRegion &region);

    //! \brief Gives the supplied data to the main ToolTip object for display
    void ToolTip(QString text, int x, int y, ToolTipAlignment align = TT_AlignCenter, int timeout = 0);

    //! \brief Public version of updateGL(), to redraw all graphs.. Not for normal use
    void redraw();

    //! \brief Asks the main gGraphView to redraw after ms milliseconds
    void timedRedraw(int ms);

    double screenToTime(int xpos);

    void dataChanged();

    //! \brief Sets the margins for the four sides of this graph.
    void setMargins(short left, short right, short top, short bottom) {
        m_marginleft = left;
        m_marginright = right;
        m_margintop = top;
        m_marginbottom = bottom;
    }

    //! \brief Returns this graphs left margin
    inline short marginLeft() { return m_marginleft; }
    //! \brief Returns this graphs right margin
    inline short marginRight() { return m_marginright; }
    //! \brief Returns this graphs top margin
    inline short marginTop() { return m_margintop; }
    //! \brief Returns this graphs bottom margin
    inline short marginBottom() { return m_marginbottom; }

    const inline QRect &rect() const { return m_rect; }

    bool isPinned() { return m_pinned; }
    void setPinned(bool b) { m_pinned = b; }

    bool isSnapshot() { return m_snapshot; }
    void setSnapshot(bool b) { m_snapshot = b; }

    short left, right, top, bottom; // dirty magin hacks..

    Layer *getLineChart();
    QTimer *timer;

    // This gets set to true to force a redraw of the yAxis tickers when graphs are resized.
    bool invalidate_yAxisImage;
    bool invalidate_xAxisImage;

    //! \brief Returns a Vector reference containing all this graphs layers
    QVector<Layer *> &layers() { return m_layers; }

    gGraphView *graphView() { return m_graphview; }
    short m_marginleft, m_marginright, m_margintop, m_marginbottom;

    inline short zoomY() { return m_zoomY; }
    void setZoomY(short zoom);

    static const short maxZoomY = 2;

    inline qint64 selectedDuration() const { return m_selectedDuration; }
    inline QString selDurString() const { return m_selDurString; }

    inline bool blockSelect() const { return m_block_select; }
    void setBlockSelect(bool b) { m_block_select = b; }

    inline bool printing() const { return m_printing; }

  protected:
    //! \brief Mouse Wheel events
    virtual void wheelEvent(QWheelEvent *event);

    //! \brief Mouse Movement events
    virtual void mouseMoveEvent(QMouseEvent *event);

    //! \brief Mouse Button Pressed events
    virtual void mousePressEvent(QMouseEvent *event);

    //! \brief Mouse Button Released events
    virtual void mouseReleaseEvent(QMouseEvent *event);

    //! \brief Mouse Button Double Clicked events
    virtual void mouseDoubleClickEvent(QMouseEvent *event);

    //! \brief Key Pressed event
    virtual void keyPressEvent(QKeyEvent *event);

    //! \brief Key Pressed event
    virtual void keyReleaseEvent(QKeyEvent *event);

    void cancelSelection() {
        m_selecting_area = false;
        m_selection = QRect(0,0,0,0);
    }

    void dumpInfo();

    //! \brief Change the current selected time boundaries by mult, from origin position origin_px
    void ZoomX(double mult, int origin_px);

    //! \brief The Main gGraphView object holding this graph
    //         (this can be pinched temporarily by print code)
    QString m_name;

    gGraphView *m_graphview;
    QString m_title;
    QString m_units;

    //! \brief Vector containing all this graphs Layers
    QVector<Layer *> m_layers;
    float m_height, m_width;

    int m_min_height;
    int m_max_height;
    bool m_visible;
    bool m_blockzoom;
    QRect m_selection;
    bool m_selecting_area;
    QPoint m_current;
    short m_group;
    short m_lastx23;
    Day *m_day;
    bool m_enforceMinY, m_enforceMaxY;
    bool m_showTitle;
    bool m_printing;
    bool m_pinned;
    short m_zoomY;
    bool m_block_select;
    QRect m_rect;

    qint64 m_selectedDuration;

    double m_currentTime;
    qint64 m_clickTime;

    QString m_selDurString;

    bool m_snapshot;

  protected slots:
    //! \brief Deselects any highlights, and schedules a main gGraphView redraw
    void Timeout();
};

/*! \brief Gets the width and height parameters for supplied text
    \param QString text - The text string in question
    \param int & width
    \param int & height
    \param QFont * font - The selected font used in the size calculations
    */
void GetTextExtent(QString text, int &width, int &height, QFont *font = defaultfont);

//! \brief Return the height of the letter x for the selected font.
int GetXHeight(QFont *font = defaultfont);

#endif // graphs_ggraph_h
