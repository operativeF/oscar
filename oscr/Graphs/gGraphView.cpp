/* gGraphView Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "Graphs/gGraphView.h"

#include <QDir>
#include <QFontMetrics>
#include <QLabel>
#include <QPixmapCache>
#include <QTimer>
#include <QFontMetrics>
#include <QWidgetAction>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QDockWidget>
#include <QMainWindow>
# include <QWindow>


#ifdef DEBUG_EFFICIENCY
# include <QElapsedTimer>
#endif

#include <cmath>

#include "mainwindow.h"
#include "Graphs/glcommon.h"
#include "Graphs/gLineChart.h"
#include "Graphs/gSummaryChart.h"
#include "Graphs/gSessionTimesChart.h"
#include "Graphs/gYAxis.h"
#include "Graphs/gFlagsLine.h"
#include "SleepLib/profiles.h"


extern MainWindow *mainwin;

#include <QApplication>

MyLabel::MyLabel(QWidget * parent)
    : QWidget(parent) {
    m_font = QApplication::font();
    time.start();
}
MyLabel::~MyLabel()
{
}
void MyLabel::setText(QString text) {
    m_text = text;
    update();
}
void MyLabel::setFont(QFont & font)
{
    m_font=font;
}
void MyLabel::doRedraw()
{
    update();
}

void MyLabel::setAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
    doRedraw();
}


void MyLabel::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setFont(m_font);
    painter.drawText(rect(), m_alignment, m_text);
}

gToolTip::gToolTip(gGraphView *graphview)
    : m_graphview(graphview)
{

    m_pos.setX(0);
    m_pos.setY(0);
    m_visible = false;
    m_alignment = TT_AlignCenter;
    m_spacer = 8; // pixels around text area
    timer = new QTimer(graphview);
    connect(timer, SIGNAL(timeout()), SLOT(timerDone()));
}

gToolTip::~gToolTip()
{
    disconnect(timer, 0, 0, 0);
    delete timer;
}
//void gToolTip::calcSize(QString text,int &w, int &h)
//{
/*GetTextExtent(text,w,h);
w+=m_spacer*2;
h+=m_spacer*2; */
//}

void gToolTip::display(QString text, int x, int y, ToolTipAlignment align, int timeout)
{
    if (timeout <= 0) {
        timeout = AppSetting->tooltipTimeout();
    }
    m_alignment = align;

    m_text = text;
    m_visible = true;
    // TODO: split multiline here
    //calcSize(m_text,tw,th);

    m_pos.setX(x);
    m_pos.setY(y);

    //tw+=m_spacer*2;
    //th+=m_spacer*2;
    //th*=2;
    if (timer->isActive()) {
        timer->stop();
    }

    timer->setSingleShot(true);
    timer->start(timeout);
    m_invalidate = true;
}

void gToolTip::cancel()
{
    m_visible = false;
    timer->stop();
}

void gToolTip::paint(QPainter &painter)     //actually paints it.
{
    if (!m_visible) { return; }

    int x = m_pos.x();
    int y = m_pos.y();

    QRect rect(x, y, 0, 0);

    painter.setFont(*defaultfont);

    rect = painter.boundingRect(rect, Qt::AlignCenter, m_text);

    int w = rect.width() + m_spacer * 2;
    int xx = rect.x() - m_spacer;

    if (xx < 0) { xx = 0; }

    rect.setLeft(xx);
    rect.setTop(rect.y() - 15);
    rect.setWidth(w);

    int z = rect.x() + rect.width();

    if (z > m_graphview->width() - 10) {
        rect.setLeft(m_graphview->width() - 2 - rect.width());
        rect.setRight(m_graphview->width() - 2);
    }

    int h = rect.height();

    if (rect.y() < 0) {
        rect.setY(0);
        rect.setHeight(h);
    }

    if (m_alignment == TT_AlignRight) {
        rect.moveTopRight(m_pos);
        if ((x-w) < 0) {
            rect.moveLeft(0);
        }
    } else if (m_alignment == TT_AlignLeft) {
        rect.moveTopLeft(m_pos);
    }

    int bot = rect.bottom() - m_graphview->height();
    if (bot > 0) {
        rect.setTop(rect.top()-bot);
        rect.setBottom(m_graphview->height());
    }


    QBrush brush(QColor(255, 255, 128, 230));
    brush.setStyle(Qt::SolidPattern);
    painter.setBrush(brush);
    painter.setPen(QColor(0, 0, 0, 255));

    painter.drawRoundedRect(rect, 5, 5);
    painter.setBrush(Qt::black);

    painter.setFont(*defaultfont);

    painter.drawText(rect, Qt::AlignCenter, m_text);
}

void gToolTip::timerDone()
{
    m_visible = false;
    m_graphview->redraw();
    m_graphview->resetMouse();
}

#ifdef ENABLE_THREADED_DRAWING

gThread::gThread(gGraphView *g)
{
    graphview = g;
    mutex.lock();
}
gThread::~gThread()
{
    if (isRunning()) {
        m_running = false;
        mutex.unlock();
        wait();
        terminate();
    }
}

void gThread::run()
{
    m_running = true;
    gGraph *g;

    while (m_running) {
        mutex.lock();

        //mutex.unlock();
        if (!m_running) { break; }

        do {
            g = graphview->popGraph();

            if (g) {
                g->paint(QRegion(g->m_lastbounds));
                //int i=0;
            } else {
                //mutex.lock();
                graphview->masterlock->release(1); // This thread gives up for now..

            }
        } while (g);
    }
}

#endif // ENABLE_THREADED_DRAWING

void gGraphView::queGraph(gGraph *g, int left, int top, int width, int height)
{
    g->m_rect = QRect(left, top, width, height);
#ifdef ENABLED_THREADED_DRAWING
    dl_mutex.lock();
#endif
    m_drawlist.push_back(g);
#ifdef ENABLED_THREADED_DRAWING
    dl_mutex.unlock();
#endif
}

void gGraphView::trashGraphs(bool destroy)
{
    if (destroy) {
        for (auto & graph : m_graphs) {
            delete graph;
        }
    }
    // Don't actually want to delete them here.. we are just borrowing the graphs
    m_graphs.clear();
    m_graphsbyname.clear();
}

// Take the next graph to render from the drawing list
gGraph *gGraphView::popGraph()
{
    gGraph *g;
#ifdef ENABLED_THREADED_DRAWING
    dl_mutex.lock();
#endif

    if (!m_drawlist.isEmpty()) {
        g = m_drawlist.at(0);
        m_drawlist.pop_front();
    } else { g = nullptr; }

#ifdef ENABLED_THREADED_DRAWING
    dl_mutex.unlock();
#endif
    return g;
}

gGraphView::gGraphView(QWidget *parent, gGraphView *shared)
#ifdef BROKEN_OPENGL_BUILD
    : QWidget(parent),
#elif QT_VERSION < QT_VERSION_CHECK(5,4,0)
    : QGLWidget(QGLFormat(QGL::DoubleBuffer | QGL::DirectRendering | QGL::HasOverlay | QGL::Rgba),parent,shared),
#else
    :QOpenGLWidget(parent),
#endif
      m_offsetY(0), m_offsetX(0), m_scaleY(0.0), m_scrollbar(nullptr)
{

//    this->grabGesture(Qt::SwipeGesture);
//    this->grabGesture(Qt::PanGesture);
//    this->grabGesture(Qt::TapGesture);
//    this->grabGesture(Qt::TapAndHoldGesture);
//    this->grabGesture(Qt::CustomGesture);
    this->grabGesture(Qt::PinchGesture);
    this->setAttribute(Qt::WA_AcceptTouchEvents);
//    this->setAttribute(Qt::WA_TouchPadAcceptSingleTouchEvents);

    m_shared = shared;
    m_sizer_index = m_graph_index = 0;
    m_metaselect = m_button_down = m_graph_dragging = m_sizer_dragging = false;
    m_lastypos = m_lastxpos = 0;
    m_horiz_travel = 0;
    m_minx = m_maxx = 0;
    m_day = nullptr;
    m_selected_graph = nullptr;
    m_scrollbar = nullptr;
    m_point_released = m_point_clicked = QPoint(0,0);
    m_showAuthorMessage = true;

    horizScrollTime.start();
    vertScrollTime.start();

    this->setMouseTracking(true);
    m_emptytext = STR_Empty_NoData;
    InitGraphGlobals(); // FIXME: sstangl: handle error return.
#ifdef ENABLE_THREADED_DRAWING
    m_idealthreads = QThread::idealThreadCount();

    if (m_idealthreads <= 0) { m_idealthreads = 1; }

    masterlock = new QSemaphore(m_idealthreads);
#endif
    m_tooltip = new gToolTip(this);
    /*for (int i=0;i<m_idealthreads;i++) {
        gThread * gt=new gThread(this);
        m_threads.push_back(gt);
        //gt->start();
    }*/

    setFocusPolicy(Qt::StrongFocus);
    m_showsplitter = true;
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), SLOT(refreshTimeout()));
    print_scaleY = print_scaleX = 1.0;

    redrawtimer = new QTimer(this);
    connect(redrawtimer, SIGNAL(timeout()), SLOT(redraw()));

    m_fadingOut = false;
    m_fadingIn = false;
    m_inAnimation = false;
    m_limbo = false;
    m_fadedir = false;
    m_blockUpdates = false;
    use_pixmap_cache = AppSetting->usePixmapCaching();

    pin_graph = nullptr;
    popout_graph = nullptr;
   // pixmapcache.setCacheLimit(10240*2);

    m_dpr = devicePixelRatio();
    m_dpr = 1; // meh???

#ifndef BROKEN_OPENGL_BUILD
    setAutoFillBackground(false);
#if QT_VERSION < QT_VERSION_CHECK(5,4,0)
    // happens no matter what in 5.4+
    setAutoBufferSwap(false);
#endif
#endif

    context_menu = new QMenu(this);
    pin_action = context_menu->addAction(QString(), this, SLOT(togglePin()));
    pin_icon = QPixmap(":/icons/pushpin.png");

    popout_action = context_menu->addAction(QObject::tr("Pop out Graph"), this, SLOT(popoutGraph()));

    snap_action = context_menu->addAction(QString(), this, SLOT(onSnapshotGraphToggle()));
    context_menu->addSeparator();


    zoom100_action = context_menu->addAction(tr("100% zoom level"), this, SLOT(resetZoom()));
    zoom100_action->setToolTip(tr("Restore X-axis zoom too 100% to view entire days data."));

    QAction * action = context_menu->addAction(tr("Reset Graph Layout"), this, SLOT(resetLayout()));
    action->setToolTip(tr("Resets all graphs to a uniform height and default order."));

    context_menu->addSeparator();
    limits_menu = context_menu->addMenu(tr("Y-Axis"));
    plots_menu = context_menu->addMenu(tr("Plots"));
    connect(plots_menu, SIGNAL(triggered(QAction*)), this, SLOT(onPlotsClicked(QAction*)));

//    overlay_menu = context_menu->addMenu("Overlays");

    cpap_menu = context_menu->addMenu(tr("CPAP Overlays"));
    connect(cpap_menu, SIGNAL(triggered(QAction*)), this, SLOT(onOverlaysClicked(QAction*)));

    oximeter_menu = context_menu->addMenu(tr("Oximeter Overlays"));
    connect(oximeter_menu, SIGNAL(triggered(QAction*)), this, SLOT(onOverlaysClicked(QAction*)));

    lines_menu = context_menu->addMenu(tr("Dotted Lines"));
    connect(lines_menu, SIGNAL(triggered(QAction*)), this, SLOT(onLinesClicked(QAction*)));


#if !defined(Q_OS_MAC)
    context_menu->setStyleSheet("QMenu {\
                              background-color: #f0f0f0; /* sets background of the menu */\
                              border: 1px solid black;\
                          }\
                          QMenu::item {\
                              /* sets background of menu item. set this to something non-transparent\
                                  if you want menu color and menu item color to be different */\
                              background-color: #f0f0f0;\
                          }\
                          QMenu::item:selected { /* when user selects item using mouse or keyboard */\
                              background-color: #ABCDEF;\
                          }");

#else
    context_menu->setStyleSheet("QMenu::item:selected { /* when user selects item using mouse or keyboard */\
                                    background-color: #ABCDEF;\
                                }");
#endif
}

void MyDockWindow::closeEvent(QCloseEvent *event)
{
    gGraphView::dock->deleteLater();
    gGraphView::dock=nullptr;
    QMainWindow::closeEvent(event);
}

MyDockWindow * gGraphView::dock = nullptr;
void gGraphView::popoutGraph()
{
    if (popout_graph) {
        if (dock == nullptr) {
            dock = new MyDockWindow(mainwin->getDaily(), Qt::Window);
            dock->resize(width(),0);
         //   QScrollArea
        }
        QDockWidget * widget = new QDockWidget(dock);
        widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        widget->setMouseTracking(true);
        int h = dock->height()+popout_graph->height()+30;
        if (h > height()) h = height();
        dock->resize(dock->width(), h);
        widget->resize(width(), popout_graph->height()+30);

        gGraphView * gv = new gGraphView(widget, this);
        widget->setWidget(gv);
        gv->setMouseTracking(true);
        gv->setDay(this->day());
        dock->addDockWidget(Qt::BottomDockWidgetArea, widget,Qt::Vertical);

        /////// Fix some resize glitches ///////
        // https://stackoverflow.com/questions/26286646/create-a-qdockwidget-that-resizes-to-its-contents?rq=1
        QDockWidget* dummy = new QDockWidget;
        dock->addDockWidget(Qt::BottomDockWidgetArea, dummy);
        dock->removeDockWidget(dummy);

        QPoint mousePos = dock->mapFromGlobal(QCursor::pos());
        mousePos.setY(dock->rect().bottom()+2);
        QCursor::setPos(dock->mapToGlobal(mousePos));
        QMouseEvent* grabSeparatorEvent =
            new QMouseEvent(QMouseEvent::MouseButtonPress,mousePos,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        qApp->postEvent(dock, grabSeparatorEvent);
        /////////////////////////////////////////

//        dock->updateGeometry();
        if (!dock->isVisible()) dock->show();

        gGraph * graph = popout_graph;

        QString basename = graph->title()+" - ";
        if (graph->m_day) {
            // append the date of the graph's left edge to the snapshot name
            // so the user knows what day the snapshot starts
            // because the name is displayed to the user, use local time
            QDateTime date = QDateTime::fromMSecsSinceEpoch(graph->min_x, Qt::LocalTime);
            basename += date.date().toString(Qt::SystemLocaleLongDate);
        }

        QString newname = basename;

        // Find a new name.. How many snapshots for each graph counts as stupid?

        QString newtitle = graph->title();

        widget->setWindowTitle(newname);
        gGraph * newgraph = new gGraph(newname, nullptr, newtitle, graph->units(), graph->height(), graph->group());
        newgraph->setHeight(graph->height());

        short group = 0;
        gv->m_graphs.insert(m_graphs.indexOf(graph)+1, newgraph);
        gv->m_graphsbyname[newname] = newgraph;
        newgraph->m_graphview = gv;

        for (auto & l : graph->m_layers) {
            Layer * layer = l->Clone();
            if (layer) {
                newgraph->m_layers.append(layer);
            }
        }

        for (auto & g : m_graphs) {
            group = qMax(g->group(), group);
        }
        newgraph->setGroup(group+1);
        //newgraph->setMinHeight(pm.height());

        newgraph->setDay(graph->m_day);
        if (graph->m_day) {
            graph->m_day->incUseCounter();
        }
        newgraph->min_x = graph->min_x;
        newgraph->max_x = graph->max_x;

        newgraph->setBlockSelect(false);
        newgraph->setZoomY(graph->zoomY());

        newgraph->setSnapshot(false);
        newgraph->setShowTitle(true);


        gv->resetLayout();
        gv->timedRedraw(0);
        //widget->setUpdatesEnabled(true);

    }
}

void gGraphView::togglePin()
{
    if (pin_graph) {
        pin_graph->setPinned(!pin_graph->isPinned());
        timedRedraw(0);
    }
}

void gGraphView::closeEvent(QCloseEvent * event)
{

    timer->stop();
    redrawtimer->stop();
    disconnect(redrawtimer, 0, 0, 0);
    disconnect(timer, 0, 0, 0);
    timer->deleteLater();
    redrawtimer->deleteLater();
    pixmapcache.clear();
    if (m_scrollbar) {
        this->disconnect(m_scrollbar, SIGNAL(sliderMoved(int)), 0, 0);
    }
#ifdef BROKEN_OPENGL_BUILD
    QWidget::closeEvent(event);
#elif QT_VERSION < QT_VERSION_CHECK(5,4,0)
    QGLWidget::closeEvent(event);
#else
    QOpenGLWidget::closeEvent(event);
#endif
}

gGraphView::~gGraphView()
{
#ifndef BROKEN_OPENGL_BUILD
    doneCurrent();
#endif

#ifdef ENABLE_THREADED_DRAWING

    for (int i=0; i < m_threads.size(); i++) {
        delete m_threads[i];
    }

    delete masterlock;
#endif

    // Note: This will cause a crash if two graphs accidentally have the same name
    for (auto & graph : m_graphs) {
        delete graph;
    }

    delete m_tooltip;
    m_graphs.clear();
}

bool gGraphView::event(QEvent * event)
{
    if (event->type() == QEvent::Gesture) {
        return gestureEvent(static_cast<QGestureEvent *>(event));
    }
    return QWidget::event(event);
}

bool gGraphView::gestureEvent(QGestureEvent * event)
{
    if (QGesture *pinch = event->gesture(Qt::PinchGesture))
        pinchTriggered(static_cast<QPinchGesture *>(pinch));

    return true;
}


bool gGraphView::pinchTriggered(QPinchGesture * gesture)
{
    gGraph * graph = nullptr;
    int group =0;
//    if (!graph) {
        // just pick any graph then
        for (const auto & g : m_graphs) {
            if (!g) continue;
            if (!g->isEmpty()) {
                graph = g;
                group = graph->group();
                break;
            }
        }
//    } else group=graph->group();

    if (!graph) { return true; }

    Q_UNUSED(group)

    //qDebug() << gesture << gesture->scaleFactor();
    if (gesture->state() == Qt::GestureStarted) {
        pinch_min = m_minx;
        pinch_max = m_maxx;
    }

     int origin_px = gesture->centerPoint().x() - titleWidth;

     // could use this instead, and have it more dynamic
     // graph->ZoomX(gesture->scaleFactor(), x);

     static const double zoom_hard_limit = 500.0;

     qint64 min = pinch_min;
     qint64 max = pinch_max;

     int width = graph->m_rect.width() - graph->left - graph->right;

     double hardspan = graph->rmax_x - graph->rmin_x;
     double span = max - min;
     double ww = double(origin_px) / double(width);
     double origin = ww * span;

     double q = span * gesture->totalScaleFactor();

     if (q > hardspan) { q = hardspan; }

     if (q < hardspan / zoom_hard_limit) { q = hardspan / zoom_hard_limit; }

     min = min + origin - (q * ww);
     max = min + q;

     if (min < graph->rmin_x) {
         min = graph->rmin_x;
         max = min + q;
     }

     if (max > graph->rmax_x) {
         max = graph->rmax_x;
         min = max - q;
     }

     //extern const int max_history;

     SetXBounds(min, max, graph->m_group);
    return true;
}


void gGraphView::dumpInfo()
{
    QDate date = mainwin->getDaily()->getDate();
    QString text = "==================== CPAP Information Dump ====================";
    mainwin->log(text);

    Day * day = p_profile->GetGoodDay(date, MT_CPAP);
    if (day) {
        QDateTime dt=QDateTime::fromMSecsSinceEpoch(day->first(), Qt::UTC);

        mainwin->log(QString("Available Channels for %1").arg(dt.toString("MMM dd yyyy")));
        QHash<schema::ChanType, QList<schema::Channel *> > list;

        for (const auto & sess : day->sessions) {
            for (auto it=sess->eventlist.begin(), end=sess->eventlist.end(); it != end; ++it) {
                ChannelID code = it.key();
                schema::Channel * chan = &schema::channel[code];
                list[chan->type()].append(chan);
            }
        }

        QHash<schema::ChanType, QList<schema::Channel *> >::iterator lit;
        for (auto lit = list.begin(), end=list.end(); lit != end; ++lit) {
            switch (lit.key()) {
            case schema::DATA:
                text = "DATA: ";
                break;
            case schema::SETTING:
                text = "SETTING: ";
                break;
            case schema::FLAG:
                text = "FLAG: ";
                break;
            case schema::MINOR_FLAG:
                text = "MINOR_FLAG: ";
                break;
            case schema::SPAN:
                text = "SPAN: ";
                break;
            case schema::WAVEFORM:
                text = "WAVEFORM: ";
                break;
            case schema::UNKNOWN:
                text = "UNKNOWN: ";
                break;
            default:
                break;
            }
            QStringList str;
            for (const auto & chan : lit.value()) {
                str.append(chan->code());
            }
            str.sort();
            text.append(str.join(", "));
            mainwin->log(text);
        }
    }
//    for (int i=0;i<m_graphs.size();i++) {
//        m_graphs[i]->dumpInfo();
//    }
}

// Render graphs with QPainter or pixmap caching, depending on preferences
void gGraphView::DrawTextQue(QPainter &painter)
{
    // process the text drawing queue
    int h,w;

    strings_drawn_this_frame += m_textque.size() + m_textqueRect.size();;

    for (const TextQue & q : m_textque) {
        // can do antialiased text via texture cache fine on mac
        // Just draw the fonts..
        painter.setPen(QColor(q.color));
        painter.setFont(*q.font);

        if (q.angle == 0) {
            painter.drawText(q.x, q.y, q.text);
        } else {
            w = painter.fontMetrics().width(q.text);
            h = painter.fontMetrics().xHeight() + 2;

            painter.translate(q.x, q.y);
            painter.rotate(-q.angle);
            painter.drawText(floor(-w / 2.0)-6, floor(-h / 2.0), q.text);
            painter.rotate(+q.angle);
            painter.translate(-q.x, -q.y);
        }
    }
    m_textque.clear();

    ////////////////////////////////////////////////////////////////////////
    // Text Rectangle Queues..
    ////////////////////////////////////////////////////////////////////////

    for (const TextQueRect & q : m_textqueRect) {
        // Just draw the fonts..

        painter.setPen(QColor(q.color));
        painter.setFont(*q.font);

        if (q.angle == 0) {
            painter.drawText(q.rect, q.flags, q.text);
        } else {
            w = painter.fontMetrics().width(q.text);
            h = painter.fontMetrics().xHeight() + 2;

            painter.translate(q.rect.x(), q.rect.y());
            painter.rotate(-q.angle);
            painter.drawText(floor(-w / 2.0), floor(-h / 2.0), q.text);
            painter.rotate(+q.angle);
            painter.translate(-q.rect.x(), -q.rect.y());
        }

    }
    m_textqueRect.clear();
}


const QString z__cacheStr = "%1:%2:%3";

void gGraphView::DrawTextQueCached(QPainter &painter)
{
    // process the text drawing queue
    int h,w;
    QString hstr;
    QPixmap pm;
    float xxx, yyy;
    const int buf = 8;
    int fonta = defaultfont->pointSize();
    int fontb = mediumfont->pointSize();
    int fontc = bigfont->pointSize();
    int size;


    for (const TextQue & q : m_textque) {
        // can do antialiased text via texture cache fine on mac
        // Generate the pixmap cache "key"
        size = (q.font == defaultfont) ? fonta : (q.font==mediumfont) ? fontb : (q.font == bigfont) ? fontc : q.font->pointSize();

        hstr = z__cacheStr.arg(q.text).arg(q.color.name()).arg(size);

        if (!QPixmapCache::find(hstr, &pm)) {

            QFontMetrics fm(*q.font);
            w = fm.width(q.text);
            h = fm.height()+buf;

            pm = QPixmap(w, h);
            pm.fill(Qt::transparent);

            QPainter imgpainter(&pm);

            imgpainter.setPen(q.color);
            imgpainter.setFont(*q.font);

            imgpainter.setRenderHint(QPainter::TextAntialiasing, q.antialias);
            imgpainter.drawText(0, h-buf, q.text);
            imgpainter.end();

            QPixmapCache::insert(hstr, pm);
        }

        h = pm.height();
        w = pm.width();
        if (q.angle != 0) {
            xxx = q.x - h - (h / 2);
            yyy = q.y + w / 2;

            xxx += 4;
            yyy += 4;

            painter.translate(xxx, yyy);
            painter.rotate(-q.angle);
            painter.drawPixmap(QRect(0, h / 2, w, h), pm);
            painter.rotate(+q.angle);
            painter.translate(-xxx, -yyy);
        } else {
            QRect r1(q.x - buf / 2 + 4, q.y - h + buf, w, h);
            painter.drawPixmap(r1, pm);
        }
    }
    ////////////////////////////////////////////////////////////////////////
    // Text Rectangle Queues..
    ////////////////////////////////////////////////////////////////////////

    for (const TextQueRect & q : m_textqueRect) {
        // can do antialiased text via texture cache fine on mac
        // Generate the pixmap cache "key"

        size = (q.font == defaultfont) ? fonta : (q.font==mediumfont) ? fontb : (q.font == bigfont) ? fontc : q.font->pointSize();

        hstr = z__cacheStr.arg(q.text).arg(q.color.name()).arg(size);

        if (!QPixmapCache::find(hstr, &pm)) {

            w = q.rect.width();
            h = q.rect.height();

            pm = QPixmap(w, h);

            pm.fill(Qt::transparent);

            QPainter imgpainter(&pm);

            imgpainter.setPen(q.color);
            imgpainter.setFont(*q.font);
            imgpainter.setRenderHint(QPainter::TextAntialiasing, true);
            imgpainter.drawText(QRect(0,0, w, h), q.flags, q.text);
            imgpainter.end();

            QPixmapCache::insert(hstr, pm);
        } else {
            h = pm.height();
            w = pm.width();
        }
        if (q.angle != 0) {
            xxx = q.rect.x() - h - (h / 2);
            yyy = q.rect.y() + w / 2;

            xxx += 4;
            yyy += 4;

            painter.translate(xxx, yyy);
            painter.rotate(-q.angle);
            painter.drawPixmap(QRect(0, h / 2, w, h), pm);
            painter.rotate(+q.angle);
            painter.translate(-xxx, -yyy);
        } else {
            painter.drawPixmap(q.rect,pm, QRect(0,0,w,h));
        }
    }

    strings_drawn_this_frame += m_textque.size() + m_textqueRect.size();;
    m_textque.clear();
    m_textqueRect.clear();
}

void gGraphView::AddTextQue(const QString &text, QRectF rect, quint32 flags, float angle, QColor color, QFont *font, bool antialias)
{
#ifdef ENABLED_THREADED_DRAWING
    text_mutex.lock();
#endif
    m_textqueRect.append(TextQueRect(rect,flags,text,angle,color,font,antialias));
#ifdef ENABLED_THREADED_DRAWING
    text_mutex.unlock();
#endif
}

void gGraphView::AddTextQue(const QString &text, short x, short y, float angle, QColor color, QFont *font, bool antialias)
{
#ifdef ENABLED_THREADED_DRAWING
    text_mutex.lock();
#endif
    m_textque.append(TextQue(x,y,angle,text,color,font,antialias));
#ifdef ENABLED_THREADED_DRAWING
    text_mutex.unlock();
#endif
}

void gGraphView::addGraph(gGraph *g, short group)
{
    if (!g) {
        qDebug() << "Attempted to add an empty graph!";
        return;
    }

    if (!m_graphs.contains(g)) {
        g->setGroup(group);
        m_graphs.push_back(g);

        if (!m_graphsbyname.contains(g->name())) {
            m_graphsbyname[g->name()] = g;
        } else {
            qDebug() << "Can't have two graphs with the same code string in the same GraphView!!";
        }

        // updateScrollBar();
    }
}

// Calculate total height of all graphs including spacers
float gGraphView::totalHeight()
{
    float th = 0;

    for (const auto & g : m_graphs) {
        if (g->isEmpty() || (!g->visible())) { continue; }

        th += g->height() + graphSpacer;
    }

    return ceil(th);
}

float gGraphView::findTop(gGraph *graph)
{
    float th = -m_offsetY;

    for (const auto & g : m_graphs) {
        if (g == graph) { break; }

        if (g->isEmpty() || (!g->visible())) { continue; }

        th += g->height() * m_scaleY + graphSpacer;
    }

    return ceil(th);
}

float gGraphView::scaleHeight()
{
    float th = 0;

    for (const auto & graph : m_graphs) {
        if (graph->isEmpty() || (!graph->visible())) { continue; }

        th += graph->height() * m_scaleY + graphSpacer;
    }

    return ceil(th);
}

void gGraphView::updateScale()
{
    if (!isVisible()) {
        m_scaleY = 0.0;
        return;
    }

    float th = totalHeight(); // height of all graphs
    float h = height();     // height of main widget


    if (th < h) {
        th -= graphSpacer;
    //    th -= visibleGraphs() * graphSpacer;   // compensate for spacer height
        m_scaleY = h / th;  // less graphs than fits on screen, so scale to fit
    } else {
        m_scaleY = 1.0;
    }

    updateScrollBar();
}


void gGraphView::resizeEvent(QResizeEvent *e)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,4,0)) && !defined(BROKEN_OPENGL_BUILD)
    // This ques a needed redraw event..
    QOpenGLWidget::resizeEvent(e);
#endif

    updateScale();

    if (m_scaleY > 0.0001) {
        for (auto & graph : m_graphs) {
            graph->resize(e->size().width(), graph->height() * m_scaleY);
        }
    }
    e->accept();
}

void gGraphView::scrollbarValueChanged(int val)
{
    //qDebug() << "Scrollbar Changed" << val;
    if (m_offsetY != val) {
        m_offsetY = val;
#if QT_VERSION >= QT_VERSION_CHECK(5,4,0)
        update();
#else
        timedRedraw(); // do this on a timer?
#endif
    }
}

void gGraphView::GetRXBounds(qint64 &st, qint64 &et)
{
    for (const auto & graph : m_graphs) {
        if (graph->group() == 0) {
            st = graph->rmin_x;
            et = graph->rmax_x;
            break;
        }
    }
}

void gGraphView::ResetBounds(bool refresh) //short group)
{
    if (m_graphs.size() == 0) return;
    Q_UNUSED(refresh)
    qint64 m1 = 0, m2 = 0;
    gGraph *graph = nullptr;

    for (auto & g : m_graphs) {
        g->ResetBounds();

        if (!g->min_x) { continue; }

        graph = g;

        if (!m1 || g->min_x < m1) { m1 = g->min_x; }

        if (!m2 || g->max_x > m2) { m2 = g->max_x; }
    }

//    if (p_profile->general->linkGroups()) {
//        for (int i = 0; i < m_graphs.size(); i++) {
//            m_graphs[i]->SetMinX(m1);
//            m_graphs[i]->SetMaxX(m2);
//        }
//    }

    if (!graph) {
        graph = m_graphs[0];
    }

    m_minx = graph->min_x;
    m_maxx = graph->max_x;

    updateScale();
}

void gGraphView::GetXBounds(qint64 &st, qint64 &et)
{
    st = m_minx;
    et = m_maxx;
}

void gGraphView::SetXBounds(qint64 minx, qint64 maxx, short group, bool refresh)
{
    for (auto & graph : m_graphs) {
        if ((graph->group() == group)) {
            graph->SetXBounds(minx, maxx);
        }
    }

    m_minx = minx;
    m_maxx = maxx;

    if (refresh) { timedRedraw(0); }
}

void gGraphView::updateScrollBar()
{
    if (!m_scrollbar || (m_graphs.size() == 0)) {
        return;
    }

    float th = scaleHeight(); // height of all graphs
    float h = height();     // height of main widget

    float vis = 0;

    for (const auto & graph : m_graphs) {
        vis += (graph->isEmpty() || !graph->visible()) ? 0 : 1;
    }

    if (th < h) { // less graphs than fits on screen

        m_scrollbar->setMaximum(0); // turn scrollbar off.

    } else {  // more graphs than fit on screen
        //m_scaleY=1.0;
        float avgheight = th / vis;
        m_scrollbar->setPageStep(avgheight);
        m_scrollbar->setSingleStep(avgheight / 8.0);
        m_scrollbar->setMaximum(th - height());

        if (m_offsetY > th - height()) {
            m_offsetY = th - height();
        }
    }
}

void gGraphView::setScrollBar(MyScrollBar *sb)
{
    m_scrollbar = sb;
    m_scrollbar->setMinimum(0);
    updateScrollBar();
    this->connect(m_scrollbar, SIGNAL(valueChanged(int)), SLOT(scrollbarValueChanged(int)));
}

bool gGraphView::renderGraphs(QPainter &painter)
{
    float px = m_offsetX;
    float py = -m_offsetY;
    int numgraphs = 0;
    float h, w;
    //ax=px;//-m_offsetX;

    //bool threaded;

    // Tempory hack using this pref..
    //#ifdef ENABLED_THREADED_DRAWING
    /*if (profile->session->multithreading()) { // && (m_idealthreads>1)) {
        threaded=true;
        for (int i=0;i<m_idealthreads;i++) {
            if (!m_threads[i]->isRunning())
                m_threads[i]->start();
        }
    } else threaded=false; */
    //#endif
    //threaded=false;
    if (height() < 40) return false;

    if (m_scaleY < 0.0000001) {
        updateScale();
    }

    lines_drawn_this_frame = 0;
    quads_drawn_this_frame = 0;

    // Calculate the height of pinned graphs

    float pinned_height = 0; // pixel height total
    int pinned_graphs = 0; // count

    for (auto & g : m_graphs) {
        int minh = g->minHeight();
        if (g->height() < minh) {
            g->setHeight(minh);
        }
        if (g->isEmpty()) { continue; }

        if (!g->visible()) { continue; }

        if (!g->isPinned()) { continue; }

        h = g->height() * m_scaleY;
        pinned_height += h + graphSpacer;
        pinned_graphs++;
    }

    py += pinned_height; // start drawing at the end of pinned space

    // Draw non pinned graphs
    for (auto & g : m_graphs) {
        if (g->isEmpty()) { continue; }

        if (!g->visible()) { continue; }

        if (g->isPinned()) { continue; }

        numgraphs++;
        h = g->height() * m_scaleY;

        // set clipping?

        if (py > height()) {
            break;    // we are done.. can't draw anymore
        }

        if ((py + h + graphSpacer) >= 0) {
            w = width();
            int tw = 0; // (g->showTitle() ? titleWidth : 0);

            queGraph(g, px + tw, py, width() - tw, h);

            if ((m_graphs.size() > 1) && m_showsplitter) {
                // draw the splitter handle

                painter.setPen(QColor(220, 220, 220, 255));
                painter.drawLine(0, py + h, w, py + h);
                painter.setPen(QColor(158,158,158,255));
                painter.drawLine(0, py + h + 1, w, py + h + 1);
                painter.setPen(QColor(240, 240, 240, 255));
                painter.drawLine(0, py + h + 2, w, py + h + 2);

            }

        }

        py = ceil(py + h + graphSpacer);
    }

    // Physically draw the unpinned graphs
    for (const auto & g : m_drawlist) {
        g->paint(painter, QRegion(g->m_rect));
    }
    m_drawlist.clear();

    if (m_graphs.size() > 1) {
        AppSetting->usePixmapCaching() ? DrawTextQueCached(painter) :DrawTextQue(painter);

        // Draw a gradient behind pinned graphs
        QLinearGradient linearGrad(QPointF(100, 100), QPointF(width() / 2, 100));
        linearGrad.setColorAt(0, QColor(216, 216, 255));
        linearGrad.setColorAt(1, Qt::white);

        painter.fillRect(0, 0, width(), pinned_height, QBrush(linearGrad));
    }

    py = 0; // start drawing at top...

    // Draw Pinned graphs
    for (const auto & g : m_graphs) {
        if (g->isEmpty()) { continue; }

        if (!g->visible()) { continue; }

        if (!g->isPinned()) { continue; }

        h = g->height() * m_scaleY;
        numgraphs++;

        if (py > height()) {
            break;    // we are done.. can't draw anymore
        }

        if ((py + h + graphSpacer) >= 0) {
            w = width();
            int tw = 0; //(g->showTitle() ? titleWidth : 0);

            queGraph(g, px + tw, py, width() - tw, h);

            if ((m_graphs.size() > 1) && m_showsplitter) {
                // draw the splitter handle
                painter.setPen(QColor(220, 220, 220, 255));
                painter.drawLine(0, py + h, w, py + h);
                painter.setPen(QColor(128, 128, 128, 255));
                painter.drawLine(0, py + h + 1, w, py + h + 1);
                painter.setPen(QColor(190, 190, 190, 255));
                painter.drawLine(0, py + h + 2, w, py + h + 2);
            }

        }

        py = ceil(py + h + graphSpacer);
    }

    //int thr=m_idealthreads;
#ifdef ENABLED_THREADED_DRAWING
    if (threaded) {
        for (int i = 0; i < m_idealthreads; i++) {
            masterlock->acquire(1);
            m_threads[i]->mutex.unlock();
        }

        // wait till all the threads are done
        // ask for all the CPU's back..
        masterlock->acquire(m_idealthreads);
        masterlock->release(m_idealthreads);
    } else {
#endif
        for (const auto & g : m_drawlist) {
            g->paint(painter, QRegion(g->m_rect));
        }
        m_drawlist.clear();

#ifdef ENABLED_THREADED_DRAWING
    }
#endif
    //int elapsed=time.elapsed();
    //QColor col=Qt::black;

    //    lines->setSize(linesize);

    AppSetting->usePixmapCaching() ? DrawTextQueCached(painter) :DrawTextQue(painter);
    //glDisable(GL_TEXTURE_2D);
    //glDisable(GL_DEPTH_TEST);

    return numgraphs > 0;
}

#include "version.h"
#ifdef BROKEN_OPENGL_BUILD
void gGraphView::paintEvent(QPaintEvent *)
#else
void gGraphView::paintGL()
#endif
{

    if (!isVisible()) {
        // wtf is this even getting CALLED??
        return;
    }
#ifdef DEBUG_EFFICIENCY
    QElapsedTimer time;
    time.start();
#endif

    if (redrawtimer->isActive()) {
        redrawtimer->stop();
    }

    bool render_cube = false; //p_profile->appearance->animations(); // do something to

    if (width() <= 0) { return; }
    if (height() <= 0) { return; }


    // Create QPainter object, note this is only valid from paintGL events!
    QPainter painter(this);
    painter.setRenderHint(QPainter::HighQualityAntialiasing, true);

    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRect bgrect(0, 0, width(), height());
    painter.fillRect(bgrect,QBrush(QColor(255,255,255)));

    bool graphs_drawn = true;

    lines_drawn_this_frame = 0;
    quads_drawn_this_frame = 0;
    strings_drawn_this_frame = 0;
    strings_cached_this_frame = 0;

    graphs_drawn = renderGraphs(painter);

    if (!graphs_drawn) { // No graphs drawn? show something useful :)
        QString txt;
        if (m_showAuthorMessage) {
            if (emptyText() == STR_Empty_Brick) {
                txt = QObject::tr("\nI'm very sorry your machine doesn't record useful data to graph in Daily View :(");
            } else {
                // not proud of telling them their machine is a Brick.. ;)
                txt = QObject::tr("SleepyHead is proudly brought to you by JediMark.");
            }
        }
//        int x2, y2;
//        GetTextExtent(m_emptytext, x2, y2, bigfont);
//        int tp2, tp1;

        if (!m_emptyimage.isNull()) {
            painter.drawPixmap(width() /2 - m_emptyimage.width() /2, height() /2 - m_emptyimage.height() /2 , m_emptyimage);
//            tp2 = height() /2 + m_emptyimage.height()/2  + y2;

        } /*else {

            tp2 = height() / 2 + y2;
        }*/
        QColor col = Qt::black;
        painter.setPen(col);

//        painter.setFont(*bigfont);
//        painter.drawText((width() / 2) - x2 / 2, tp2, m_emptytext);

        QRectF rec(0,0,width(),0);
        painter.setFont(*defaultfont);
        rec = painter.boundingRect(rec, Qt::AlignHCenter | Qt::AlignBottom, txt);
        rec.moveBottom(height()-5);

        painter.drawText(rec, Qt::AlignHCenter | Qt::AlignBottom, txt);
    }
    if (AppSetting->lineCursorMode()) {
       emit updateCurrentTime(graphs_drawn ? m_currenttime : 0.0F);
    } else {
       emit updateRange(graphs_drawn ? m_minx : 0.0F, m_maxx);
    }
    AppSetting->usePixmapCaching() ? DrawTextQueCached(painter) :DrawTextQue(painter);

    m_tooltip->paint(painter);

#ifdef DEBUG_EFFICIENCY
    const int rs = 20;
    static double ring[rs] = {0};
    static int rp = 0;

    // Show FPS and draw time
    if (m_showsplitter && AppSetting->showPerformance()) {
        QString ss;
        qint64 ela = time.nsecsElapsed();
        double ms = double(ela) / 1000000.0;
        ring[rp++] = 1000.0 / ms;
        rp %= rs;
        double v = 0;

        for (int i = 0; i < rs; i++) {
            v += ring[i];
        }

        double fps = v / double(rs);
        ss = "Debug Mode " + QString::number(fps, 'f', 1) + "fps "
                + QString::number(lines_drawn_this_frame, 'f', 0) + " lines "
//                + QString::number(quads_drawn_this_frame, 'f', 0) + " quads "
                + QString::number(strings_drawn_this_frame, 'f', 0) + " strings "
                + QString::number(strings_cached_this_frame, 'f', 0) + " cached ";

        int w, h;
        // this uses tightBoundingRect, which is different on Mac than it is on Windows & Linux.
        GetTextExtent(ss, w, h);
        QColor col = Qt::white;

        if (m_graphs.size() > 0) {
            painter.fillRect(width() - m_graphs[0]->marginRight(), 0, m_graphs[0]->marginRight(), w, QBrush(col));
        }
#ifndef Q_OS_MAC
        //   if (usePixmapCache()) xx+=4; else xx-=3;
#endif
        AddTextQue(ss, width(), w / 2, 90, QColor(Qt::black), defaultfont);
        AppSetting->usePixmapCaching() ? DrawTextQueCached(painter) :DrawTextQue(painter);
    }
//    painter.setPen(Qt::lightGray);
//    painter.drawLine(0, 0, 0, height());
//    painter.drawLine(0, 0, width(), 0);
//    painter.setPen(Qt::darkGray);
    //painter.drawLine(width(), 0, width(), height());

#endif

    painter.end();

#ifndef BROKEN_OPENGL_BUILD
#if QT_VERSION < QT_VERSION_CHECK(5,4,0)
    swapBuffers();
#endif
#endif
    if (this->isVisible() && !graphs_drawn && render_cube) { // keep the cube spinning
        redrawtimer->setInterval(1000.0 / 50); // 50 FPS
        redrawtimer->setSingleShot(true);
        redrawtimer->start();
    }

}

QString gGraphView::getRangeString()
{
    // a note about time zone usage here
    // even though this string will be displayed to the user
    // the graph is drawn using UTC times, so no conversion
    // is needed to format the date and time for the user
    // i.e. if the graph says the cursor is at 5pm, then that
    // is what we should display.
    // passing in UTC below is necessary to prevent QT
    // from automatically converting the time to local time
    QString fmt;

    qint64 diff = m_maxx - m_minx;

    if (diff > 86400000) {
        int days = ceil(double(m_maxx-m_minx) / 86400000.0);

        qint64 minx = floor(double(m_minx)/86400000.0);
        minx *= 86400000L;

        qint64 maxx = minx + 86400000L * qint64(days)-1;

        QDateTime st = QDateTime::fromMSecsSinceEpoch(minx, Qt::UTC);
        QDateTime et = QDateTime::fromMSecsSinceEpoch(maxx, Qt::UTC);

        QString txt = st.toString("d MMM") + " - " +  et.addDays(-1).toString("d MMM yyyy");
        return txt;
    } else if (diff > 60000) {
        fmt = "HH:mm:ss";
    } else {
        fmt = "HH:mm:ss:zzz";
    }
    QDateTime st = QDateTime::fromMSecsSinceEpoch(m_minx, Qt::UTC);
    QDateTime et = QDateTime::fromMSecsSinceEpoch(m_maxx, Qt::UTC);

    QString txt = st.toString(QObject::tr("d MMM [ %1 - %2 ]").arg(fmt).arg(et.toString(fmt))) ;

    return txt;
}

void gGraphView::leaveEvent(QEvent * event)
{
    Q_UNUSED(event);
    if (m_metaselect) {
        m_metaselect = false;
        timedRedraw(0);
    }
    releaseKeyboard();
}


// For manual scrolling
void gGraphView::setOffsetY(int offsetY)
{
    if (m_offsetY != offsetY) {
        m_offsetY = offsetY;
        redraw(); //issue full redraw..
    }
}

// For manual X scrolling (not really needed)
void gGraphView::setOffsetX(int offsetX)
{
    if (m_offsetX != offsetX) {
        m_offsetX = offsetX;
        redraw(); //issue redraw
    }
}

void gGraphView::mouseMoveEvent(QMouseEvent *event)
{
    grabKeyboard();

    int x = event->x();
    int y = event->y();

    m_mouse = QPoint(x, y);

    if (m_sizer_dragging) { // Resize handle being dragged
        float my = y - m_sizer_point.y();
        //qDebug() << "Sizer moved vertically" << m_sizer_index << my*m_scaleY;
        float h = m_graphs[m_sizer_index]->height();
        h += my / m_scaleY;

        if (h > m_graphs[m_sizer_index]->minHeight()) {
            m_graphs[m_sizer_index]->setHeight(h);
            m_sizer_point.setX(x);
            m_sizer_point.setY(y);
            updateScrollBar();
            timedRedraw();
        }

        return;
    }

    if (m_graph_dragging) { // Title bar being dragged to reorder
        gGraph *p;
        int yy = m_sizer_point.y();
        bool empty;

        if (y < yy) {

            for (int i = m_graph_index - 1; i >= 0; i--) {
                if (m_graphs[i]->isPinned() != m_graphs[m_graph_index]->isPinned()) {
                    // fix cursor
                    continue;
                }

                empty = m_graphs[i]->isEmpty() || (!m_graphs[i]->visible());
                // swapping upwards.
                int yy2 = yy - graphSpacer - m_graphs[i]->height() * m_scaleY;
                yy2 += m_graphs[m_graph_index]->height() * m_scaleY;

                if (y < yy2) {
                    //qDebug() << "Graph Reorder" << m_graph_index;
                    p = m_graphs[m_graph_index];
                    m_graphs[m_graph_index] = m_graphs[i];
                    m_graphs[i] = p;

                    if (!empty) {
                        m_sizer_point.setY(yy - graphSpacer - m_graphs[m_graph_index]->height()*m_scaleY);
                        redraw();
                    }

                    m_graph_index--;
                }

                if (!empty) { break; }

            }
        } else if (y > yy + graphSpacer + m_graphs[m_graph_index]->height()*m_scaleY) {
            // swapping downwards
            //qDebug() << "Graph Reorder" << m_graph_index;
            for (int i = m_graph_index + 1; i < m_graphs.size(); i++) {
                if (m_graphs[i]->isPinned() != m_graphs[m_graph_index]->isPinned()) {
                    //m_graph_dragging=false;
                    // fix cursor
                    continue;
                }

                empty = m_graphs[i]->isEmpty() || (!m_graphs[i]->visible());
                p = m_graphs[m_graph_index];
                m_graphs[m_graph_index] = m_graphs[i];
                m_graphs[i] = p;

                if (!empty) {
                    m_sizer_point.setY(yy + graphSpacer + m_graphs[m_graph_index]->height()*m_scaleY);
                    timedRedraw();
                }

                m_graph_index++;

                if (!empty) { break; }
            }
        }

        return;
    }

    float py = 0, pinned_height = 0, h;
    bool done = false;

    // Do pinned graphs first
    for (const auto & graph : m_graphs) {

        if (graph->isEmpty() || !graph->visible() || !graph->isPinned()) {
            continue;
        }

        h = graph->height() * m_scaleY;
        pinned_height += h + graphSpacer;

        if (py > height()) {
            break;    // we are done.. can't draw anymore
        }

        if (!((y >= py + graph->top) && (y < py + h - graph->bottom))) {
            if (graph->isSelected()) {
                graph->deselect();
                timedRedraw(150);
            }
        }

        // Update Mouse Cursor shape
        if ((y >= py + h - 1) && (y < (py + h + graphSpacer))) {
            this->setCursor(Qt::SplitVCursor);
            done = true;
        } else if ((y >= py + 1) && (y < py + h)) {
            if (x >= titleWidth + 10) {
                this->setCursor(Qt::ArrowCursor);
            } else {
                m_tooltip->display("Double click title to pin / unpin\nClick and drag to reorder graphs", x + 10, y, TT_AlignLeft);
                timedRedraw(0);

                this->setCursor(Qt::OpenHandCursor);
            }


            m_horiz_travel += qAbs(x - m_lastxpos) + qAbs(y - m_lastypos);
            m_lastxpos = x;
            m_lastypos = y;
            //           QPoint p(x,y);
            //           QMouseEvent e(event->type(),p,event->button(),event->buttons(),event->modifiers());

            graph->mouseMoveEvent(event);

            done = true;
        }

        py += h + graphSpacer;

    }

    py = -m_offsetY;
    py += pinned_height;

    // Propagate mouseMove events to relevant graphs
    if (!done)
        for (const auto & graph : m_graphs) {

            if (graph->isEmpty() || !graph->visible() || graph->isPinned()) {
                continue;
            }

            h = graph->height() * m_scaleY;

            if (py > height()) {
                break;    // we are done.. can't draw anymore
            }

            if (!((y >= py + graph->top) && (y < py + h - graph->bottom))) {
                if (graph->isSelected()) {
                    graph->deselect();
                    timedRedraw(150);
                }
            }

            // Update Mouse Cursor shape
            if ((y >= py + h - 1) && (y < (py + h + graphSpacer))) {
                this->setCursor(Qt::SplitVCursor);
            } else if ((y >= py + 1) && (y < py + h)) {
                if (x >= titleWidth + 10) {
                    this->setCursor(Qt::ArrowCursor);
                } else {
                    m_tooltip->display("Double click title to pin / unpin\nClick and drag to reorder graphs", x + 10, y, TT_AlignLeft);
                    timedRedraw(0);

                    this->setCursor(Qt::OpenHandCursor);
                }


                m_horiz_travel += qAbs(x - m_lastxpos) + qAbs(y - m_lastypos);
                m_lastxpos = x;
                m_lastypos = y;
                if (graph) {
                    graph->mouseMoveEvent(event);
                }
            }

            /*            else if (!m_button_down && (y >= py) && (y < py+m_graphs[i]->top)) {
                            // Mouse cursor is in top graph margin.
                        } else if (!m_button_down && (y >= py+h-m_graphs[i]->bottom) && (y <= py+h)) {
                            // Mouse cursor is in bottom grpah margin.
                        } else if (m_button_down || ((y >= py+m_graphs[i]->top) && (y < py + h-m_graphs[i]->bottom))) {
                            if (m_button_down || (x >= titleWidth+10)) { //(gYAxis::Margin-5)
                                this->setCursor(Qt::ArrowCursor);
                                m_horiz_travel+=qAbs(x-m_lastxpos)+qAbs(y-m_lastypos);
                                m_lastxpos=x;
                                m_lastypos=y;
                                QPoint p(x-titleWidth,y-py);
                                QMouseEvent e(event->type(),p,event->button(),event->buttons(),event->modifiers());

                                m_graphs[i]->mouseMoveEvent(&e);
                                if (!m_button_down && (x<=titleWidth+(gYAxis::Margin-5))) {
                                    //qDebug() << "Hovering over" << m_graphs[i]->title();
                                    if (m_graphsbytitle[STR_TR_EventFlags]==m_graphs[i]) {
                                        QVector<Layer *> & layers=m_graphs[i]->layers();
                                        gFlagsGroup *fg;
                                        for (int i=0;i<layers.size();i++) {
                                            if ((fg=dynamic_cast<gFlagsGroup *>(layers[i]))!=nullptr) {
                                                float bh=fg->barHeight();
                                                int count=fg->count();
                                                float yp=py+m_graphs[i]->marginTop();
                                                    yp=y-yp;
                                                float th=(float(count)*bh);
                                                if (yp>=0 && yp<th) {
                                                    int i=yp/bh;
                                                    if (i<count) {
                                                        ChannelID code=fg->visibleLayers()[i]->code();
                                                        QString ttip=schema::channel[code].description();
                                                        m_tooltip->display(ttip,x,y-20,AppSetting->tooltipTimeout());
                                                        redraw();
                                                        //qDebug() << code << ttip;
                                                    }
                                                }

                                                break;
                                            }
                                        }
                                    } else {
                                        if (!m_graphs[i]->units().isEmpty()) {
                                            m_tooltip->display(m_graphs[i]->units(),x,y-20,AppSetting->tooltipTimeout());
                                            redraw();
                                        }
                                    }
                                }
                            } else {

                                this->setCursor(Qt::OpenHandCursor);
                            }

                        } */

            //   }
            py += h + graphSpacer;
        }

}

Layer * gGraphView::findLayer(gGraph * graph, LayerType type)
{
    for (auto & layer : graph->m_layers) {
        if (layer->layerType() == type) {
            return layer;
        }
    }
    return nullptr;
}

class MyWidgetAction : public QWidgetAction
{
public:
    MyWidgetAction(ChannelID code, QObject * parent = nullptr) :QWidgetAction(parent), code(code) { chbox = nullptr; }
protected:
    virtual QWidget * createWidget(QWidget * /*parent*/) {
        connect(chbox, SIGNAL(toggled(bool)), this, SLOT(setChecked(bool)));
        connect(chbox, SIGNAL(clicked()), this, SLOT(trigger()));

        return chbox;
    }
    QCheckBox * chbox;
    ChannelID code;
};


MinMaxWidget::MinMaxWidget(gGraph * graph, QWidget *parent)
:QWidget(parent), graph(graph)
{
    step = 1;
    createLayout();
}

void MinMaxWidget::onMinChanged(double d)
{
    graph->rec_miny = d;
    graph->timedRedraw(0);
}
void MinMaxWidget::onMaxChanged(double d)
{
    graph->rec_maxy = d;
    graph->timedRedraw(0);
}
void MinMaxWidget::onResetClicked()
{
    int tmp = graph->zoomY();
    graph->setZoomY(0);
    EventDataType miny = graph->MinY(),
                  maxy = graph->MaxY();

    graph->roundY(miny, maxy);
    setMin(graph->rec_miny = miny);
    setMax(graph->rec_maxy = maxy);

    float r = maxy-miny;
    if (r > 400) {
        step = 50;
    } else if (r > 100) {
        step = 10;
    } else if (r > 50) {
        step = 5;
    } else {
        step = 1;
    }

    graph->setZoomY(tmp);
}

void MinMaxWidget::onComboChanged(int idx)
{
    minbox->setEnabled(idx == 2);
    maxbox->setEnabled(idx == 2);
    reset->setEnabled(idx == 2);

    graph->setZoomY(idx);

    if (idx == 2) {
        if (qAbs(graph->rec_maxy - graph->rec_miny) < 0.0001) {
            onResetClicked();
        }
    }
}

void MinMaxWidget::createLayout()
{
    QGridLayout * layout = new QGridLayout;
    layout->setMargin(4);
    layout->setSpacing(4);

    combobox = new QComboBox(this);
    combobox->addItem(tr("Auto-Fit"), 0);
    combobox->addItem(tr("Defaults"), 1);
    combobox->addItem(tr("Override"), 2);
    combobox->setToolTip(tr("The Y-Axis scaling mode, 'Auto-Fit' for automatic scaling, 'Defaults' for settings according to manufacturer, and 'Override' to choose your own."));
    connect(combobox, SIGNAL(activated(int)), this, SLOT(onComboChanged(int)));

    minbox = new QDoubleSpinBox(this);
    maxbox = new QDoubleSpinBox(this);

    minbox->setToolTip(tr("The Minimum Y-Axis value.. Note this can be a negative number if you wish."));
    maxbox->setToolTip(tr("The Maximum Y-Axis value.. Must be greater than Minimum to work."));

    int idx = graph->zoomY();
    combobox->setCurrentIndex(idx);

    minbox->setEnabled(idx == 2);
    maxbox->setEnabled(idx == 2);

    minbox->setAlignment(Qt::AlignRight);
    maxbox->setAlignment(Qt::AlignRight);

    minbox->setMinimum(-9999.0);
    maxbox->setMinimum(-9999.0);
    minbox->setMaximum(9999.99);
    maxbox->setMaximum(9999.99);
    setMin(graph->rec_miny);
    setMax(graph->rec_maxy);

    float r = graph->rec_maxy - graph->rec_miny;
    if (r > 400) {
        step = 50;
    } else if (r > 100) {
        step = 10;
    } else if (r > 50) {
        step = 5;
    } else {
        step = 1;
    }

    minbox->setSingleStep(step);
    maxbox->setSingleStep(step);

    connect(minbox, SIGNAL(valueChanged(double)), this, SLOT(onMinChanged(double)));
    connect(maxbox, SIGNAL(valueChanged(double)), this, SLOT(onMaxChanged(double)));

    QLabel * label = new QLabel(tr("Scaling Mode"));
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label,0,0);
    layout->addWidget(combobox,1,0);

    label = new QLabel(STR_TR_Min);
    label->setFont(font);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label,0,1);
    layout->addWidget(minbox,1,1);

    label = new QLabel(STR_TR_Max);
    label->setFont(font);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label,0,2);
    layout->addWidget(maxbox,1,2);

    reset = new QToolButton(this);
    reset->setIcon(QIcon(":/icons/refresh.png"));
    reset->setToolTip(tr("This button resets the Min and Max to match the Auto-Fit"));
    reset->setEnabled(idx == 2);

    layout->addWidget(reset,1,3);
    connect(reset, SIGNAL(clicked()), this, SLOT(onResetClicked()));
    this->setLayout(layout);
}

void gGraphView::populateMenu(gGraph * graph)
{
    QAction * action;

    if (graph->isSnapshot()) {
        snap_action->setText(tr("Remove Clone"));
        snap_action->setData(graph->name()+"|remove");
       // zoom100_action->setVisible(false);
    } else {
        snap_action->setText(tr("Clone %1 Graph").arg(graph->title()));
        snap_action->setData(graph->name()+"|snapshot");
      //  zoom100_action->setVisible(true);
    }

    // Menu title fonts
    QFont font = QApplication::font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 3);

    gLineChart * lc = dynamic_cast<gLineChart *>(findLayer(graph,LT_LineChart));
    SummaryChart * sc = dynamic_cast<SummaryChart *>(findLayer(graph,LT_SummaryChart));
    gSummaryChart * stg = dynamic_cast<gSummaryChart *>(findLayer(graph,LT_Overview));

    limits_menu->clear();
    if (lc || sc || stg) {
        QWidgetAction * widget = new QWidgetAction(this);
        MinMaxWidget * minmax = new MinMaxWidget(graph, this);

        widget->setDefaultWidget(minmax);

        limits_menu->addAction(widget);
        limits_menu->menuAction()->setVisible(true);
    } else {
        limits_menu->menuAction()->setVisible(false);
    }

    // First check for any linechart for this graph..
    if (lc) {
        lines_menu->clear();
        for (int i=0, end=lc->m_dotlines.size(); i < end; i++) {
            const DottedLine & dot = lc->m_dotlines[i];

            if (!lc->m_enabled[dot.code]) continue;

            schema::Channel &chan = schema::channel[dot.code];

            if (dot.available) {

                QWidgetAction * widget = new QWidgetAction(context_menu);

                QCheckBox *chbox = new QCheckBox(chan.calc[dot.type].label(), context_menu);
                chbox->setMouseTracking(true);
                chbox->setStyleSheet(QString("QCheckBox:hover { background: %1; }").arg(QApplication::palette().highlight().color().name()));

                widget->setDefaultWidget(chbox);

                widget->setCheckable(true);
                widget->setData(QString("%1|%2").arg(graph->name()).arg(i));

                connect(chbox, SIGNAL(toggled(bool)), widget, SLOT(setChecked(bool)));
                connect(chbox, SIGNAL(clicked()), widget, SLOT(trigger()));

                bool b = lc->m_dot_enabled[dot.code][dot.type]; //chan.calc[dot.type].enabled;
                chbox->setChecked(b);
                lines_menu->addAction(widget);
            }
        }

        lines_menu->menuAction()->setVisible(lines_menu->actions().size() > 0);

        if (lines_menu->actions().size() > 0) {
            lines_menu->insertSeparator(lines_menu->actions()[0]);
            action = new QAction(QObject::tr("%1").arg(graph->title()), lines_menu);
            lines_menu->insertAction(lines_menu->actions()[0], action);
            action->setFont(font);
            action->setData(QString(""));
            action->setEnabled(false);

        }

        //////////////////////////////////////////////////////////////////////////////////////
        // Populate Plots Menus
        //////////////////////////////////////////////////////////////////////////////////////

        plots_menu->clear();

        for (const auto code : lc->m_codes) {
            if (lc->m_day && !lc->m_day->channelHasData(code)) continue;

            QWidgetAction * widget = new QWidgetAction(context_menu);

            QCheckBox *chbox = new QCheckBox(schema::channel[code].label(), context_menu);
            chbox->setMouseTracking(true);
            chbox->setToolTip(schema::channel[code].description());
            chbox->setStyleSheet(QString("QCheckBox:hover { background: %1; }").arg(QApplication::palette().highlight().color().name()));


            widget->setDefaultWidget(chbox);

            widget->setCheckable(true);
            widget->setData(QString("%1|%2").arg(graph->name()).arg(code));

            connect(chbox, SIGNAL(toggled(bool)), widget, SLOT(setChecked(bool)));
            connect(chbox, SIGNAL(clicked()), widget, SLOT(trigger()));

            bool b = lc->m_enabled[code];
            chbox->setChecked(b);

            plots_menu->addAction(widget);
        }

        plots_menu->menuAction()->setVisible((plots_menu->actions().size() > 1));

        if (plots_menu->actions().size() > 0) {
            plots_menu->insertSeparator(plots_menu->actions()[0]);
            action = new QAction(QObject::tr("%1").arg(graph->title()), plots_menu);
            plots_menu->insertAction(plots_menu->actions()[0], action);

            action->setFont(font);
            action->setData(QString(""));
            action->setEnabled(false);
        }

        //////////////////////////////////////////////////////////////////////////////////////
        // Populate Event Menus
        //////////////////////////////////////////////////////////////////////////////////////
        oximeter_menu->clear();
        cpap_menu->clear();

        using namespace schema;
        quint32 showflags = schema::FLAG | schema::MINOR_FLAG | schema::SPAN;
        if (p_profile->general->showUnknownFlags()) showflags |= schema::UNKNOWN;
        QList<ChannelID> chans = lc->m_day->getSortedMachineChannels(showflags);

        QHash<MachineType, int> Vis;

        for (const auto code : chans) {
            schema::Channel & chan = schema::channel[code];

            QWidgetAction * widget = new QWidgetAction(context_menu);

            QCheckBox *chbox = new QCheckBox(schema::channel[code].fullname(), context_menu);
            chbox->setPalette(context_menu->palette());
            chbox->setMouseTracking(true);
            chbox->setToolTip(schema::channel[code].description());
            chbox->setStyleSheet(QString("QCheckBox:hover { background: %1; }").arg(QApplication::palette().highlight().color().name()));

            widget->setDefaultWidget(chbox);

            widget->setCheckable(true);
            widget->setData(QString("%1|%2").arg(graph->name()).arg(code));

            connect(chbox, SIGNAL(toggled(bool)), widget, SLOT(setChecked(bool)));
            connect(chbox, SIGNAL(clicked()), widget, SLOT(trigger()));

            bool b = lc->m_flags_enabled[code];
            chbox->setChecked(b);
            Vis[chan.machtype()] += b ? 1 : 0;

            action = nullptr;
            if (chan.machtype() == MT_OXIMETER) {
                oximeter_menu->insertAction(nullptr, widget);
            } else if ( chan.machtype() == MT_CPAP) {
                cpap_menu->insertAction(nullptr,widget);
            }
        }

        QString HideAllEvents = QObject::tr("Hide All Events");
        QString ShowAllEvents = QObject::tr("Show All Events");

        oximeter_menu->menuAction()->setVisible(oximeter_menu->actions().size() > 0);
        cpap_menu->menuAction()->setVisible(cpap_menu->actions().size() > 0);


        if (cpap_menu->actions().size() > 0) {
            cpap_menu->addSeparator();
            if (Vis[MT_CPAP] > 0) {
                action = cpap_menu->addAction(HideAllEvents);
                action->setData(QString("%1|HideAll:CPAP").arg(graph->name()));
            } else {
                action = cpap_menu->addAction(ShowAllEvents);
                action->setData(QString("%1|ShowAll:CPAP").arg(graph->name()));
            }

            // Show CPAP Events menu Header...
            cpap_menu->insertSeparator(cpap_menu->actions()[0]);
            action = new QAction(QObject::tr("%1").arg(graph->title()), cpap_menu);
            cpap_menu->insertAction(cpap_menu->actions()[0], action);
            action->setFont(font);
            action->setData(QString(""));
            action->setEnabled(false);
        }
        if (oximeter_menu->actions().size() > 0) {
            oximeter_menu->addSeparator();
            if (Vis[MT_OXIMETER] > 0) {
                action = oximeter_menu->addAction(HideAllEvents);
                action->setData(QString("%1|HideAll:OXI").arg(graph->name()));
            } else {
                action = oximeter_menu->addAction(ShowAllEvents);
                action->setData(QString("%1|ShowAll:OXI").arg(graph->name()));
            }

            oximeter_menu->insertSeparator(oximeter_menu->actions()[0]);
            action = new QAction(QObject::tr("%1").arg(graph->title()), oximeter_menu);
            oximeter_menu->insertAction(oximeter_menu->actions()[0], action);
            action->setFont(font);
            action->setData(QString(""));
            action->setEnabled(false);
        }

    } else {
        lines_menu->clear();
        lines_menu->menuAction()->setVisible(false);
        plots_menu->clear();
        plots_menu->menuAction()->setVisible(false);
        oximeter_menu->clear();
        oximeter_menu->menuAction()->setVisible(false);
        cpap_menu->clear();
        cpap_menu->menuAction()->setVisible(false);
    }
}

void gGraphView::onSnapshotGraphToggle()
{
    QString name = snap_action->data().toString().section("|",0,0);
    QString cmd = snap_action->data().toString().section("|",-1).toLower();
    auto it = m_graphsbyname.find(name);
    if (it == m_graphsbyname.end()) return;

    gGraph * graph = it.value();

    if (cmd == "snapshot") {

        QString basename = name+";";
        if (graph->m_day) {
            // append the date of the graph's left edge to the snapshot name
            // so the user knows what day the snapshot starts
            // because the name is displayed to the user, use local time
            QDateTime date = QDateTime::fromMSecsSinceEpoch(graph->min_x, Qt::LocalTime);
            basename += date.date().toString(Qt::SystemLocaleLongDate);
        }
        QString newname;

        // Find a new name.. How many snapshots for each graph counts as stupid?
        for (int i=1;i < 100;i++) {
            newname = basename+" ("+QString::number(i)+")";

            it = m_graphsbyname.find(newname);
            if (it == m_graphsbyname.end()) {
                break;
            }
        }

        QString newtitle;
        bool fnd = false;
        // someday, some clown will keep adding new graphs to break this..
        for (int i=1; i < 100; i++) {
            newtitle = graph->title()+"-"+QString::number(i);
            fnd = false;
            for (const auto & graph : m_graphs) {
                if (graph->title() == newtitle) {
                    fnd = true;
                    break;
                }
            }
            if (!fnd) break;
        }
        if (fnd) {
            // holy crap.. what patience. but not what I meant by as many as you like ;)
            return;
        }

        gGraph * newgraph = new gGraph(newname, nullptr, newtitle, graph->units(), graph->height(), graph->group());
       // newgraph->setBlockSelect(true);
        newgraph->setHeight(graph->height());

        short group = 0;
        m_graphs.insert(m_graphs.indexOf(graph)+1, newgraph);
        m_graphsbyname[newname] = newgraph;
        newgraph->m_graphview = this;

        for (const auto & l : graph->m_layers) {
            Layer * layer = l->Clone();
            if (layer) {
                newgraph->m_layers.append(layer);
            }
        }

        for (const auto & g : m_graphs) {
            group = qMax(g->group(), group);
        }
        newgraph->setGroup(group+1);
        //newgraph->setMinHeight(pm.height());

        newgraph->setDay(graph->m_day);
        if (graph->m_day) {
            graph->m_day->incUseCounter();
        }
        newgraph->min_x = graph->min_x;
        newgraph->max_x = graph->max_x;
        if (graph->blockZoom()) {
            newgraph->setBlockZoom(graph->blockZoom());
            newgraph->setBlockSelect(true);
        }
        if (graph->blockSelect()) {
            newgraph->setBlockSelect(true);
        }
        newgraph->setZoomY(graph->zoomY());

        newgraph->setSnapshot(true);

        emit GraphsChanged();

//        addGraph(newgraph);
        updateScale();
        timedRedraw(0);
    } else if (cmd == "remove") {
        if (graph->m_day) {
            graph->m_day->decUseCounter();
            if (graph->m_day->useCounter() == 0) {
            }
        }
        m_graphsbyname.remove(graph->name());
        m_graphs.removeAll(it.value());
        delete graph;

        updateScale();
        timedRedraw(0);

        emit GraphsChanged();
    }
    qDebug() << cmd << name;
}

bool gGraphView::hasSnapshots()
{
    bool snap = false;
    for (const auto & graph : m_graphs) {
        if (graph->isSnapshot()) {
            snap = true;
            break;
        }
    }
    return snap;
}


void gGraphView::onPlotsClicked(QAction *action)
{
    QString name = action->data().toString().section("|",0,0);
    ChannelID code = action->data().toString().section("|",-1).toInt();

    auto it = m_graphsbyname.find(name);
    if (it == m_graphsbyname.end()) return;

    gGraph * graph = it.value();

    gLineChart * lc = dynamic_cast<gLineChart *>(findLayer(graph, LT_LineChart));

    if (!lc) return;

    lc->m_enabled[code] = !lc->m_enabled[code];
    graph->min_y = graph->MinY();
    graph->max_y = graph->MaxY();
    graph->timedRedraw(0);
//    lc->Miny();
//    lc->Maxy();
}

void gGraphView::onOverlaysClicked(QAction *action)
{
    QString name = action->data().toString().section("|",0,0);
    QString data = action->data().toString().section("|",-1);
    auto it = m_graphsbyname.find(name);
    if (it == m_graphsbyname.end()) return;
    gGraph * graph = it.value();

    gLineChart * lc = dynamic_cast<gLineChart *>(findLayer(graph, LT_LineChart));

    if (!lc) return;

    bool ok;
    ChannelID code = data.toInt(&ok);
    if (ok) {
        // Just toggling a flag on/off
        bool b = ! lc->m_flags_enabled[code];
        lc->m_flags_enabled[code] = b;
        QWidgetAction * widget = qobject_cast<QWidgetAction *>(action);
        if (widget) {
            widget->setChecked(b);
        }
        timedRedraw(0);
        return;
    }

    QString hideall = data.section(":",0,0);

    if ((hideall == "HideAll") || (hideall == "ShowAll")) {

        bool value = (hideall == "HideAll") ? false : true;
        QString group = data.section(":",-1).toUpper();
        MachineType mtype;
        if (group == "CPAP") mtype = MT_CPAP;
        else if (group == "OXI") mtype = MT_OXIMETER;
        else mtype = MT_UNKNOWN;

        // First toggle the actual flag bits
        for (auto it=lc->m_flags_enabled.begin(), end=lc->m_flags_enabled.end(); it != end; ++it) {
            if (schema::channel[it.key()].machtype() == mtype) {
                lc->m_flags_enabled[it.key()] = value;
            }
        }

        // Now toggle the menu actions.. bleh
        if (mtype == MT_CPAP) {
            for (auto & action : cpap_menu->actions()) {
                if (action->isCheckable())  {
                    action->setChecked(value);
                }
            }
        } else if (mtype == MT_OXIMETER) {
            for (auto & action : oximeter_menu->actions()) {
                if (action->isCheckable())  {
                    action->setChecked(value);
                }
            }
        }
    }

}


void gGraphView::onLinesClicked(QAction *action)
{
    QString name = action->data().toString().section("|",0,0);
    QString data = action->data().toString().section("|",-1);

    auto it = m_graphsbyname.find(name);
    if (it == m_graphsbyname.end()) return;

    gGraph * graph = it.value();

    gLineChart * lc = dynamic_cast<gLineChart *>(findLayer(graph, LT_LineChart));

    if (!lc) return;

    bool ok;
    int i = data.toInt(&ok);
    if (ok) {
        DottedLine & dot = lc->m_dotlines[i];
        schema::Channel &chan = schema::channel[dot.code];

        chan.calc[dot.type].enabled = !chan.calc[dot.type].enabled;
        lc->m_dot_enabled[dot.code][dot.type] = !lc->m_dot_enabled[dot.code][dot.type];

    }
    timedRedraw(0);
}


void gGraphView::mousePressEvent(QMouseEvent *event)
{
    int x = event->x();
    int y = event->y();

    float h, pinned_height = 0, py = 0;

    bool done = false;

    // first handle pinned graphs.
    // Calculate total height of all pinned graphs
    for (int i=0, end=m_graphs.size(); i<end; ++i) {
        gGraph * g = m_graphs[i];

        if (!g || g->isEmpty() || !g->visible() || !g->isPinned()) {
            continue;
        }

        h = g->height() * m_scaleY;
        pinned_height += h + graphSpacer;

        if (py > height()) {
            break;
        }

        if ((py + h + graphSpacer) >= 0) {
            if ((y >= py + h - 1) && (y <= py + h + graphSpacer)) {
                this->setCursor(Qt::SplitVCursor);
                m_sizer_dragging = true;
                m_sizer_index = i;
                m_sizer_point.setX(x);
                m_sizer_point.setY(y);
                done = true;
            } else if ((y >= py) && (y < py + h)) {
                //qDebug() << "Clicked" << i;
                if ((event->button() == Qt::LeftButton) && (x < titleWidth + 20)) {
                    // clicked on title to drag graph..
                    // Note: reorder has to be limited to pinned graphs.
                    m_graph_dragging = true;
                    m_tooltip->cancel();

                    timedRedraw(50);
                    m_graph_index = i;
                    m_sizer_point.setX(x);
                    m_sizer_point.setY(py); // point at top of graph..
                    this->setCursor(Qt::ClosedHandCursor);
                    //done=true;
                } else if ((event->button() == Qt::RightButton) && (x < (titleWidth + gYAxis::Margin))) {
                    this->setCursor(Qt::ArrowCursor);
                    pin_action->setText(QObject::tr("Unpin %1 Graph").arg(g->title()));
                    pin_graph = g;
                    popout_action->setText(QObject::tr("Popout %1 Graph").arg(g->title()));
                    popout_graph = g;

                    populateMenu(g);
                    context_menu->popup(event->globalPos());
                    //done=true;
                } else if (!g->blockSelect()) {
                    if (m_metaselect) {
                        if (m_selected_graph) {
                            m_selected_graph->m_selecting_area = false;
                        }
                    }
                    // send event to graph..
                    m_point_clicked = QPoint(event->x(), event->y());

                    //QMouseEvent e(event->type(),m_point_clicked,event->button(),event->buttons(),event->modifiers());


                    m_button_down = true;
                    m_metaselect = event->modifiers() && Qt::AltModifier;
                    m_horiz_travel = 0;
                    m_graph_index = i;
                    m_selected_graph = g;
                    g->mousePressEvent(event);
                }

                done = true;
            }

        }

        py += h + graphSpacer;
    }



    // then handle the remainder...
    py = -m_offsetY;
    py += pinned_height;

    if (!done) {
        for (int i=0, end=m_graphs.size(); i<end; ++i) {
            gGraph * g = m_graphs[i];

            if (!g || g->isEmpty() || !g->visible() || g->isPinned()) { continue; }

            h = g->height() * m_scaleY;

            if (py > height()) {
                break;
            }

            if ((py + h + graphSpacer) >= 0) {
                if ((y >= py + h - 1) && (y <= py + h + graphSpacer)) {
                    this->setCursor(Qt::SplitVCursor);
                    m_sizer_dragging = true;
                    m_sizer_index = i;
                    m_sizer_point.setX(x);
                    m_sizer_point.setY(y);
                    //qDebug() << "Sizer clicked" << i;
                    //done=true;
                } else if ((y >= py) && (y < py + h)) {
                    //qDebug() << "Clicked" << i;

                    if ((event->button() == Qt::LeftButton) && (x < (titleWidth + 20))) { // clicked on title to drag graph..
                        m_graph_dragging = true;
                        m_tooltip->cancel();
                        redraw();
                        m_graph_index = i;
                        m_sizer_point.setX(x);
                        m_sizer_point.setY(py); // point at top of graph..
                        this->setCursor(Qt::ClosedHandCursor);
                        //done=true;
                    } else if ((event->button() == Qt::RightButton) && (x < (titleWidth + gYAxis::Margin))) {
                        this->setCursor(Qt::ArrowCursor);
                        popout_action->setText(QObject::tr("Popout %1 Graph").arg(g->title()));
                        popout_graph = g;
                        pin_action->setText(QObject::tr("Pin %1 Graph").arg(g->title()));
                        pin_graph = g;
                        populateMenu(g);

                        context_menu->popup(event->globalPos());
                        //done=true;
                    } else if (!g->blockSelect()) {
                       if (m_metaselect) {
                            if (m_selected_graph) {
                                m_selected_graph->m_selecting_area = false;
                            }
                        }
                        // send event to graph..
                        m_point_clicked = QPoint(event->x(), event->y());
                        //QMouseEvent e(event->type(),m_point_clicked,event->button(),event->buttons(),event->modifiers());
                        m_button_down = true;
                        m_metaselect = event->modifiers() && Qt::AltModifier;

                        m_horiz_travel = 0;
                        m_graph_index = i;
                        m_selected_graph = g;
                        g->mousePressEvent(event);
                    }
                }

            }

            py += h + graphSpacer;
            done=true;
        }
    }
    if (!done) {
//        if (event->button() == Qt::RightButton) {
//            this->setCursor(Qt::ArrowCursor);
//            context_menu->popup(event->globalPos());
//            done=true;
//        }
    }
}

void gGraphView::mouseReleaseEvent(QMouseEvent *event)
{

    int x = event->x();
    int y = event->y();

    float h, py = 0, pinned_height = 0;
    bool done = false;


    // Copy to a local variable to make sure this gets cleared
    bool button_down = m_button_down;
    m_button_down = false;

    // Handle pinned graphs first
    for (const auto & g : m_graphs) {

        if (!g || g->isEmpty() || !g->visible() || !g->isPinned()) {
            continue;
        }

        h = g->height() * m_scaleY;
        pinned_height += h + graphSpacer;

        if (py > height()) {
            break;    // we are done.. can't draw anymore
        }

        if ((y >= py + h - 1) && (y < (py + h + graphSpacer))) {
            this->setCursor(Qt::SplitVCursor);
            done = true;
        } else if ((y >= py + 1) && (y <= py + h)) {

            //            if (!m_sizer_dragging && !m_graph_dragging) {
            //               g->mouseReleaseEvent(event);
            //            }

            if (x >= titleWidth + 10) {
                this->setCursor(Qt::ArrowCursor);
            } else {
                this->setCursor(Qt::OpenHandCursor);
            }

            done = true;
        }

        py += h + graphSpacer;
    }

    // Now do the unpinned ones
    py = -m_offsetY;
    py += pinned_height;

    if (done)
        for (const auto & g : m_graphs) {

            if (!g || g->isEmpty() || !g->visible() || g->isPinned()) {
                continue;
            }

            h = g->height() * m_scaleY;

            if (py > height()) {
                break;    // we are done.. can't draw anymore
            }

            if ((y >= py + h - 1) && (y < (py + h + graphSpacer))) {
                this->setCursor(Qt::SplitVCursor);
            } else if ((y >= py + 1) && (y <= py + h)) {

                //            if (!m_sizer_dragging && !m_graph_dragging) {
                //                g->mouseReleaseEvent(event);
                //            }

                if (x >= titleWidth + 10) {
                    this->setCursor(Qt::ArrowCursor);
                } else {
                    this->setCursor(Qt::OpenHandCursor);
                }
            }

            py += h + graphSpacer;
        }

    if (m_sizer_dragging) {
        m_sizer_dragging = false;
        return;
    }

    if (m_graph_dragging) {
        m_graph_dragging = false;

        // not sure why the cursor code doesn't catch this..
        if (x >= titleWidth + 10) {
            this->setCursor(Qt::ArrowCursor);
        } else {
            this->setCursor(Qt::OpenHandCursor);
        }

        return;
    }

    // The graph that got the button press gets the release event
    if (button_down) {
//        m_button_down = false;
        m_metaselect = event->modifiers() & Qt::AltModifier;
        saveHistory();

        if (m_metaselect) {
            m_point_released = event->pos();
        } else {
            if ((m_graph_index >= 0) && (m_graphs[m_graph_index])) {
                m_graphs[m_graph_index]->mouseReleaseEvent(event);
            }
        }
    }
    timedRedraw(0);
}

void gGraphView::keyReleaseEvent(QKeyEvent *event)
{
    if (m_metaselect && !(event->modifiers() & Qt::AltModifier)) {
        QMouseEvent mevent(QEvent::MouseButtonRelease, m_point_released, Qt::LeftButton, Qt::LeftButton, event->modifiers());
        if (m_graph_index>=0) {
            m_graphs[m_graph_index]->mouseReleaseEvent(&mevent);
        }

        m_metaselect = false;
        timedRedraw(50);
    }
    if (event->key() == Qt::Key_Escape) {
        if (history.size() > 0) {
            SelectionHistoryItem h = history.takeFirst();
            SetXBounds(h.minx, h.maxx);

            // could Forward push this to another list?
        } else {
            ResetBounds();

        }
        return;
    }
#ifdef BROKEN_OPENGL_BUILD
        QWidget::keyReleaseEvent(event);
#elif QT_VERSION < QT_VERSION_CHECK(5,4,0)
        QGLWidget::keyReleaseEvent(event);
#else
        QOpenGLWidget::keyReleaseEvent(event);
#endif
}


void gGraphView::mouseDoubleClickEvent(QMouseEvent *event)
{
    mousePressEvent(event); // signal missing.. a qt change might "fix" this if we are not careful.

    int x = event->x();
    int y = event->y();

    float h, py = 0, pinned_height = 0;
    bool done = false;

    // Handle pinned graphs first
    for (const auto & g : m_graphs) {
        if (!g || g->isEmpty() || !g->visible() || !g->isPinned()) {
            continue;
        }

        h = g->height() * m_scaleY;
        pinned_height += h + graphSpacer;

        if (py > height()) {
            break;    // we are done.. can't draw anymore
        }

        if ((py + h + graphSpacer) >= 0) {
            if ((y >= py) && (y <= py + h)) {
                if (x < titleWidth) {
                    // What to do when double clicked on the graph title ??

                    g->mouseDoubleClickEvent(event);
                    // pin the graph??
                    g->setPinned(false);
                    redraw();
                } else {
                    // send event to graph..
                    g->mouseDoubleClickEvent(event);
                }

                done = true;
            } else if ((y >= py + h) && (y <= py + h + graphSpacer + 1)) {
                // What to do when double clicked on the resize handle?
                done = true;
            }
        }

        py += h;
        py += graphSpacer; // do we want the extra spacer down the bottom?
    }


    py = -m_offsetY;
    py += pinned_height;

    if (!done) // then handle unpinned graphs
        for (const auto & g : m_graphs) {
            if (!g || g->isEmpty() || !g->visible() || g->isPinned()) {
                continue;
            }

            h = g->height() * m_scaleY;

            if (py > height()) {
                break;
            }

            if ((py + h + graphSpacer) >= 0) {
                if ((y >= py) && (y <= py + h)) {
                    if (x < titleWidth) {
                        // What to do when double clicked on the graph title ??
                        g->mouseDoubleClickEvent(event);

                        g->setPinned(true);
                        redraw();
                    } else {
                        // send event to graph..
                        g->mouseDoubleClickEvent(event);
                    }
                } else if ((y >= py + h) && (y <= py + h + graphSpacer + 1)) {
                    // What to do when double clicked on the resize handle?
                }

            }

            py += h;
            py += graphSpacer; // do we want the extra spacer down the bottom?
        }
}

void gGraphView::wheelEvent(QWheelEvent *event)
{
    // Hmm.. I could optionalize this to change mousewheel behaviour without affecting the scrollbar now..

    if (m_button_down)
        return;

    if (event->modifiers() == Qt::NoModifier) {
        int scrollDampening = AppSetting->scrollDampening();

        if (event->orientation() == Qt::Vertical) { // Vertical Scrolling
            if (horizScrollTime.elapsed() < scrollDampening) {
                return;
            }

            if (m_scrollbar)
                m_scrollbar->SendWheelEvent(event); // Just forwarding the event to scrollbar for now..
            m_tooltip->cancel();
            vertScrollTime.start();
            return;
        }

        // (This is a total pain in the butt on MacBook touchpads..)

        if (vertScrollTime.elapsed() < scrollDampening) {
            return;
        }
        horizScrollTime.start();
    }

    gGraph *graph = nullptr;
    int group = 0;
    //int x = event->x();
    int y = event->y();

    float h, py = 0, pinned_height = 0;


    // Find graph hovered over
    for (const auto & g : m_graphs) {

        if (!g || g->isEmpty() || !g->visible() || !g->isPinned()) {
            continue;
        }

        h = g->height() * m_scaleY;
        pinned_height += h + graphSpacer;

        if (py > height()) {
            break;    // we are done.. can't draw anymore
        }

        if ((py + h + graphSpacer) >= 0) {
            if ((y >= py) && (y <= py + h)) {
                graph = g;
                break;
            } else if ((y >= py + h) && (y <= py + h + graphSpacer + 1)) {
                // What to do when double clicked on the resize handle?
                graph = g;
                break;
            }
        }

        py += h;
        py += graphSpacer; // do we want the extra spacer down the bottom?
    }
    if (!graph) {
        py = -m_offsetY;
        py += pinned_height;

        for (const auto & g : m_graphs) {
            if (!g || g->isEmpty() || !g->visible() || g->isPinned()) {
                continue;
            }

            h = g->height() * m_scaleY;

            if (py > height()) {
                break;
            }

            if ((py + h + graphSpacer) >= 0) {
                if ((y >= py) && (y <= py + h)) {
                    graph = g;
                    break;
                } else if ((y >= py + h) && (y <= py + h + graphSpacer + 1)) {
                    // What to do when double clicked on the resize handle?
                    graph = g;
                    break;
                }

            }

            py += h;
            py += graphSpacer; // do we want the extra spacer down the bottom?
        }
    }


    if (event->modifiers() == Qt::NoModifier) {
        if (!graph) {
            // just pick any graph then
            for (const auto & g : m_graphs) {
                if (!g) continue;
                if (!g->isEmpty()) {
                    graph = g;
                    group = graph->group();
                    break;
                }
            }
        } else group=graph->group();

        if (!graph) { return; }

        double xx = (graph->max_x - graph->min_x);
        double zoom = 240.0;

        int delta = event->delta();

        if (delta > 0) {
            graph->min_x -= (xx / zoom) * (float)abs(delta);
        } else {
            graph->min_x += (xx / zoom) * (float)abs(delta);
        }

        graph->max_x = graph->min_x + xx;

        if (graph->min_x < graph->rmin_x) {
            graph->min_x = graph->rmin_x;
            graph->max_x = graph->rmin_x + xx;
        }

        if (graph->max_x > graph->rmax_x) {
            graph->max_x = graph->rmax_x;
            graph->min_x = graph->max_x - xx;
        }

        saveHistory();
        SetXBounds(graph->min_x, graph->max_x, group);

    } else if ((event->modifiers() & Qt::ControlModifier)) {

        if (graph) graph->wheelEvent(event);
//        int x = event->x();
//        int y = event->y();

//        float py = -m_offsetY;
//        float h;



//        for (int i = 0; i < m_graphs.size(); i++) {
//            gGraph *g = m_graphs[i];
//            if (!g || g->isEmpty() || !g->visible()) { continue; }

//            h = g->height() * m_scaleY;

//            if (py > height()) {
//                break;
//            }

//            if ((py + h + graphSpacer) >= 0) {
//                if ((y >= py) && (y <= py + h)) {
//                    if (x < titleWidth) {
//                        // What to do when ctrl+wheel is used on the graph title ??
//                    } else {
//                        // send event to graph..
//                        g->wheelEvent(event);
//                    }
//                } else if ((y >= py + h) && (y <= py + h + graphSpacer + 1)) {
//                    // What to do when the wheel is used on the resize handle?
//                }

//            }

//            py += h;
//            py += graphSpacer; // do we want the extra spacer down the bottom?
//        }
    }
}

void gGraphView::getSelectionTimes(qint64 & start, qint64 & end)
{
    if (m_graph_index >= 0) {
        gGraph *g = m_graphs[m_graph_index];
        if (!g) {
            start = 0;
            end = 0;
            return;
        }
        int x1 = g->m_selection.x() + titleWidth;
        int x2 = x1 + g->m_selection.width();
        start = g->screenToTime(x1);
        end = g->screenToTime(x2);
    }
}

void gGraphView::keyPressEvent(QKeyEvent *event)
{
    m_metaselect = event->modifiers() & Qt::AltModifier;
    if (m_metaselect && ((event->key() == Qt::Key_B) || (event->key() == 8747))) {
        if (mainwin->getDaily()->graphView() == this) {
            if (m_graph_index >= 0) {
                m_metaselect=false;
                qint64 start,end;
                getSelectionTimes(start,end);
                QDateTime d1 = QDateTime::fromMSecsSinceEpoch(start, Qt::UTC);

                mainwin->getDaily()->addBookmark(start, end, QString("Bookmark at %1").arg(d1.time().toString("HH:mm:ss")));
                m_graphs[m_graph_index]->cancelSelection();
                m_graph_index = -1;
                timedRedraw(0);
            }
            event->accept();
            return;
        }
    }

    if ((m_metaselect) && (event->key() >= Qt::Key_0) && (event->key() <= Qt::Key_9)) {
        //int bk = (int)event->key()-Qt::Key_0;
        m_metaselect = false;

        timedRedraw(0);
    }

    if (event->key() == Qt::Key_F3) {
        AppSetting->setLineCursorMode(!AppSetting->lineCursorMode());
        timedRedraw(0);
    }
    if ((event->key() == Qt::Key_F1)) {
        dumpInfo();
    }

    if (event->key() == Qt::Key_Tab) {
        event->ignore();
        return;
    }

    if (event->key() == Qt::Key_PageUp) {
        if (m_scrollbar) {
            m_offsetY -= AppSetting->graphHeight() * 3 * m_scaleY;
            m_scrollbar->setValue(m_offsetY);
            m_offsetY = m_scrollbar->value();
            redraw();
        }
        return;
    } else if (event->key() == Qt::Key_PageDown) {
        if (m_scrollbar) {
            m_offsetY += AppSetting->graphHeight() * 3 * m_scaleY;

            if (m_offsetY < 0) { m_offsetY = 0; }

            m_scrollbar->setValue(m_offsetY);
            m_offsetY = m_scrollbar->value();
            redraw();
        }
        return;
        //        redraw();
    }

    gGraph *g = nullptr;
    int group = 0;

    // Pick the first valid graph in the primary group
    for (const auto & gr : m_graphs) {
        if (gr->group() == group) {
            if (!gr->isEmpty() && gr->visible()) {
                g = gr;
                break;
            }
        }
    }

    if (!g) {
        for (const auto & gr : m_graphs) {
            if (!gr->isEmpty()) {
                g = gr;
                group = g->group();
                break;
            }
        }
    }

    if (!g) { return; }

    g->keyPressEvent(event);

    if (event->key() == Qt::Key_Left) {
        double xx = g->max_x - g->min_x;
        double zoom = 8.0;

        if (event->modifiers() & Qt::ControlModifier) { zoom /= 4; }

        g->min_x -= xx / zoom;;
        g->max_x = g->min_x + xx;

        if (g->min_x < g->rmin_x) {
            g->min_x = g->rmin_x;
            g->max_x = g->rmin_x + xx;
        }

        saveHistory();
        SetXBounds(g->min_x, g->max_x, group);
    } else if (event->key() == Qt::Key_Right) {
        double xx = g->max_x - g->min_x;
        double zoom = 8.0;

        if (event->modifiers() & Qt::ControlModifier) { zoom /= 4; }

        g->min_x += xx / zoom;
        g->max_x = g->min_x + xx;

        if (g->max_x > g->rmax_x) {
            g->max_x = g->rmax_x;
            g->min_x = g->rmax_x - xx;
        }

        saveHistory();
        SetXBounds(g->min_x, g->max_x, group);
    } else if (event->key() == Qt::Key_Up) {
        float zoom = 0.75F;

        if (event->modifiers() & Qt::ControlModifier) { zoom /= 1.5; }

        g->ZoomX(zoom, 0); // zoom in.
    } else if (event->key() == Qt::Key_Down) {
        float zoom = 1.33F;

        if (event->modifiers() & Qt::ControlModifier) { zoom *= 1.5; }

        g->ZoomX(zoom, 0); // Zoom out
    }

    //qDebug() << "Keypress??";
}


void gGraphView::setDay(Day *day)
{

    m_day = day;

    for (const auto & g : m_graphs) {
        if (g) g->setDay(day);
    }

    ResetBounds(false);
}
bool gGraphView::isEmpty()
{
    bool res = true;

    for (const auto & graph : m_graphs) {
        if (!graph->isSnapshot() && !graph->isEmpty()) {
            res = false;
            break;
        }
    }

    return res;
}

void gGraphView::refreshTimeout()
{
    if (this->isVisible()) {
        redraw();
    }
}
void gGraphView::timedRedraw(int ms)
{

    if (timer->isActive()) {
        if (ms == 0) {
            timer->stop();
        } else {
            int m = timer->remainingTime();
            if (m > ms) {
                timer->stop();
            } else return;
       }
    }
    timer->setSingleShot(true);
    timer->start(ms);
}
void gGraphView::resetLayout()
{
    int default_height = AppSetting->graphHeight();

    for (auto & graph : m_graphs) {
        if (graph) graph->setHeight(default_height);
    }

    updateScale();
    timedRedraw(0);
}
void gGraphView::deselect()
{
    for (auto & graph : m_graphs) {
        if (graph) graph->deselect();
    }
}

const quint32 gvmagic = 0x41756728;
const quint16 gvversion = 4;

void gGraphView::SaveSettings(QString title)
{
    qDebug() << "Saving" << title << "settings";
    QString filename = p_profile->Get("{DataFolder}/") + title.toLower() + ".shg";
    QFile f(filename);
    f.open(QFile::WriteOnly);
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_4_6);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (quint32)gvmagic;
    out << (quint16)gvversion;

    out << (qint16)size();

    for (auto & graph : m_graphs) {
        if (!graph) continue;
        if (graph->isSnapshot()) continue;

        out << graph->name();
        out << graph->height();
        out << graph->visible();
        out << graph->RecMinY();
        out << graph->RecMaxY();
        out << graph->zoomY();
        out << (bool)graph->isPinned();

        gLineChart * lc = dynamic_cast<gLineChart *>(findLayer(graph, LT_LineChart));
        if (lc) {
            out << (quint32)LT_LineChart;
            out << lc->m_flags_enabled;
            out << lc->m_enabled;
            out << lc->m_dot_enabled;
        } else {
            out << (quint32)LT_Other;
        }


    }

    f.close();
}

bool gGraphView::LoadSettings(QString title)
{
    QString filename = p_profile->Get("{DataFolder}/") + title.toLower() + ".shg";
    QFile f(filename);

    if (!f.exists()) {
        return false;
    }

    f.open(QFile::ReadOnly);
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_4_6);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 t1;
    quint16 version;

    in >> t1;

    if (t1 != gvmagic) {
        qDebug() << "gGraphView" << title << "settings magic doesn't match" << t1 << gvmagic;
        return false;
    }

    in >> version;

    if (version < gvversion) {
        qDebug() << "gGraphView" << title << "settings will be upgraded.";
    }

    qint16 siz;
    in >> siz;
    QString name;
    float hght;
    bool vis;
    EventDataType recminy, recmaxy;
    bool pinned;

    short zoomy = 0;

    QList<gGraph *> neworder;
    QHash<QString, gGraph *>::iterator gi;

    for (int i = 0; i < siz; i++) {
        in >> name;
        in >> hght;
        in >> vis;
        in >> recminy;
        in >> recmaxy;

        if (gvversion >= 1) {
            in >> zoomy;
        }

        if (gvversion >= 2) {
            in >> pinned;
        }

        QHash<ChannelID, bool> flags_enabled;
        QHash<ChannelID, bool> plots_enabled;
        QHash<ChannelID, QHash<quint32, bool> > dot_enabled;

        // Warning: Do not break the follow section up!!!
        quint32 layertype;
        if (gvversion >= 4) {
            in >> layertype;
            if (layertype == LT_LineChart) {
                in >> flags_enabled;
                in >> plots_enabled;
                in >> dot_enabled;
            }
        }

        gGraph *g = nullptr;

        if (version <= 2) {
            continue;
//            // Names were stored as translated strings, so look up title instead.
//            g = nullptr;
//            for (int z=0; z<m_graphs.size(); ++z) {
//                if (m_graphs[z]->title() == name) {
//                    g = m_graphs[z];
//                    break;
//                }
//            }
        } else {
            gi = m_graphsbyname.find(name);
            if (gi == m_graphsbyname.end()) {
                qDebug() << "Graph" << name << "has been renamed or removed";
            } else {
                g = gi.value();
            }
        }
        if (g) {
            neworder.push_back(g);
            g->setHeight(hght);
            g->setVisible(vis);
            g->setRecMinY(recminy);
            g->setRecMaxY(recmaxy);
            g->setZoomY(zoomy);
            g->setPinned(pinned);

            if (gvversion >= 4) {
                if (layertype == LT_LineChart) {
                    gLineChart * lc = dynamic_cast<gLineChart *>(findLayer(g, LT_LineChart));
                    if (lc) {
                        lc->m_flags_enabled = flags_enabled;
                        lc->m_enabled = plots_enabled;

                        lc->m_dot_enabled = dot_enabled;
                    }
                }
            }

        }
    }

    if (neworder.size() == m_graphs.size()) {
        m_graphs = neworder;
    }

    f.close();
    updateScale();
    return true;
}

gGraph *gGraphView::findGraph(QString name)
{
    auto it = m_graphsbyname.find(name);

    if (it == m_graphsbyname.end()) { return nullptr; }

    return it.value();
}

gGraph *gGraphView::findGraphTitle(QString title)
{
    for (auto & graph : m_graphs) {
        if (graph->title() == title) return graph;
    }
    return nullptr;
}

int gGraphView::visibleGraphs()
{
    int cnt = 0;

    for (auto & graph : m_graphs) {
        if (graph->visible() && !graph->isEmpty()) { cnt++; }
    }

    return cnt;
}

void gGraphView::dataChanged()
{
    for (auto & graph : m_graphs) {
        graph->dataChanged();
    }
}


void gGraphView::redraw()
{
#ifdef BROKEN_OPENGL_BUILD
    repaint();
#else
    update();
#endif
}
