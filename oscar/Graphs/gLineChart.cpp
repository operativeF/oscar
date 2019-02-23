/* gLineChart Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "Graphs/gLineChart.h"

#include <QString>
#include <QDebug>

#include <math.h>

#include "Graphs/glcommon.h"
#include "Graphs/gGraph.h"
#include "Graphs/gGraphView.h"
#include "SleepLib/profiles.h"
#include "Graphs/gLineOverlay.h"

#define EXTRA_ASSERTS 1

QDataStream & operator<<(QDataStream & stream, const DottedLine & dot)
{
    stream << dot.code;
    stream << dot.type;
    stream << dot.value;
    stream << dot.visible;
    stream << dot.available;
    return stream;
}
QDataStream & operator>>(QDataStream & stream, DottedLine & dot)
{
    quint32 tmp;
    stream >> dot.code;
    stream >> tmp;
    stream >> dot.value;
    stream >> dot.visible;
    stream >> dot.available;
    dot.type = (ChannelCalcType)tmp;
    return stream;
}


QColor darken(QColor color, float p)
{
    int r = qMin(int(color.red() * p), 255);
    int g = qMin(int(color.green() * p), 255);
    int b = qMin(int(color.blue() * p), 255);


    return QColor(r,g,b, color.alpha());
}

gLineChart::gLineChart(ChannelID code, bool square_plot, bool disable_accel)
    : Layer(code), m_square_plot(square_plot), m_disable_accel(disable_accel)
{
    addPlot(code, square_plot);
    m_report_empty = false;
    lines.reserve(50000);
    lasttime = 0;
    m_layertype = LT_LineChart;
}
gLineChart::~gLineChart()
{
    for (auto fit = flags.begin(), end=flags.end(); fit != end; ++fit) {
        // destroy any overlay bar from previous day
        delete fit.value();
    }

    flags.clear();

}

bool gLineChart::isEmpty()
{
    if (!m_day) { return true; }

    for (const auto code : m_codes) {
        for (int i=0, end=m_day->size(); i < end; i++) {
            Session *sess = m_day->sessions[i];

            if (sess->channelExists(code)) {
                return false;
            }
        }
    }

    return true;
}


void gLineChart::SetDay(Day *d)
{
    //    Layer::SetDay(d);
    m_day = d;

    m_minx = 0, m_maxx = 0;
    m_miny = 0, m_maxy = 0;
    m_physminy = 0, m_physmaxy = 0;

    if (!d) {
        return;
    }

    qint64 t64;
    EventDataType tmp;

    bool first = true;

    for (auto & code : m_codes) {

        for (int i=0, end=d->size(); i < end; i++) {
            Session *sess = d->sessions[i];
            if (!sess->enabled()) continue;

            CPAPMode mode = (CPAPMode)sess->settings[CPAP_Mode].toInt();

            if (mode >= MODE_BILEVEL_FIXED) {
                m_enabled[CPAP_Pressure] = false;
                m_enabled[CPAP_IPAP] = true;
                m_enabled[CPAP_EPAP] = true;
            }

            if (code == CPAP_MaskPressure) {
                if (sess->channelExists(CPAP_MaskPressureHi)) {
                    code = CPAP_MaskPressureHi; // verify setting m_codes[]
                    m_enabled[code] = schema::channel[CPAP_MaskPressureHi].enabled();

                    goto skipcheck; // why not :P
                }
            }

            if (!sess->channelExists(code)) {
                continue;
            }

skipcheck:
            if (first) {
                m_miny = sess->Min(code);
                m_maxy = sess->Max(code);
                m_physminy = sess->physMin(code);
                m_physmaxy = sess->physMax(code);
                m_minx = sess->first(code);
                m_maxx = sess->last(code);
                first = false;
            } else {
                tmp = sess->physMin(code);

                if (m_physminy > tmp) {
                    m_physminy = tmp;
                }

                tmp = sess->physMax(code);

                if (m_physmaxy < tmp) {
                    m_physmaxy = tmp;
                }

                tmp = sess->Min(code);

                if (m_miny > tmp) {
                    m_miny = tmp;
                }

                tmp = sess->Max(code);

                if (m_maxy < tmp) {
                    m_maxy = tmp;
                }

                t64 = sess->first(code);

                if (m_minx > t64) {
                    m_minx = t64;
                }

                t64 = sess->last(code);

                if (m_maxx < t64) {
                    m_maxx = t64;
                }
            }
        }

    }

    subtract_offset = 0;

    for (auto fit = flags.begin(), end=flags.end(); fit != end; ++fit) {
        // destroy any overlay bar from previous day
        delete fit.value();
    }

    flags.clear();

    quint32 z = schema::FLAG | schema::MINOR_FLAG | schema::SPAN;
    if (p_profile->general->showUnknownFlags()) z |= schema::UNKNOWN;
    QList<ChannelID> available = m_day->getSortedMachineChannels(z);

    for (const auto & code : available) {
        if (!m_flags_enabled.contains(code)) {
            bool b = false;

            if (((m_codes[0] == CPAP_FlowRate) || ((m_codes[0] == CPAP_MaskPressureHi))) && (schema::channel[code].machtype() == MT_CPAP)) b = true;
            if ((m_codes[0] == CPAP_Leak) && (code == CPAP_LargeLeak)) b = true;
            m_flags_enabled[code] = b;
        }
        if (!m_day->channelExists(code)) continue;

        schema::Channel * chan = &schema::channel[code];
        gLineOverlayBar * lob = nullptr;

        if (chan->type() == schema::FLAG) {
            lob = new gLineOverlayBar(code, chan->defaultColor(), chan->label(), FT_Bar);
        } else if ((chan->type() == schema::MINOR_FLAG) || (chan->type() == schema::UNKNOWN)) {
            lob = new gLineOverlayBar(code, chan->defaultColor(), chan->label(), FT_Dot);
        } else if (chan->type() == schema::SPAN) {
            lob = new gLineOverlayBar(code, chan->defaultColor(), chan->label(), FT_Span);
        }
        if (lob != nullptr) {
            lob->setOverlayDisplayType(((m_codes[0] == CPAP_FlowRate))? (OverlayDisplayType)AppSetting->overlayType() : ODT_TopAndBottom);
            lob->SetDay(m_day);
            flags[code] = lob;
        }
    }

    m_dotlines.clear();

    for (const auto & code : m_codes) {
        const schema::Channel & chan = schema::channel[code];
        addDotLine(DottedLine(code, Calc_Max,chan.calc[Calc_Max].enabled));
        if ((code != CPAP_FlowRate) && (code != CPAP_MaskPressure) && (code != CPAP_MaskPressureHi)) {
            addDotLine(DottedLine(code, Calc_Perc,chan.calc[Calc_Perc].enabled));
            addDotLine(DottedLine(code, Calc_Middle, chan.calc[Calc_Middle].enabled));
        }
        if ((code != CPAP_Snore) && (code != CPAP_FlowLimit) && (code != CPAP_RDI) && (code != CPAP_AHI)) {
            addDotLine(DottedLine(code, Calc_Min, chan.calc[Calc_Min].enabled));
        }
    }
    if (m_codes[0] == CPAP_Leak) {
        addDotLine(DottedLine(CPAP_Leak, Calc_UpperThresh, schema::channel[CPAP_Leak].calc[Calc_UpperThresh].enabled));
    } else if (m_codes[0] == CPAP_FlowRate) {
        addDotLine(DottedLine(CPAP_FlowRate, Calc_Zero, schema::channel[CPAP_FlowRate].calc[Calc_Zero].enabled));
    } else if (m_codes[0] == OXI_Pulse) {
        addDotLine(DottedLine(OXI_Pulse, Calc_UpperThresh, schema::channel[OXI_Pulse].calc[Calc_UpperThresh].enabled));
        addDotLine(DottedLine(OXI_Pulse, Calc_LowerThresh, schema::channel[OXI_Pulse].calc[Calc_LowerThresh].enabled));
    } else if (m_codes[0] == OXI_SPO2) {
        addDotLine(DottedLine(OXI_SPO2, Calc_LowerThresh, schema::channel[OXI_SPO2].calc[Calc_LowerThresh].enabled));
    }


    if (m_day) {
        for (auto & dot : m_dotlines) {
            dot.calc(m_day);

            ChannelID code = dot.code;
            ChannelCalcType type = dot.type;

            bool b = false; // default value

            const auto & cit = m_dot_enabled.find(code);
            if (cit == m_dot_enabled.end()) {
                m_dot_enabled[code].insert(type, b);
            } else {
                const auto & it = cit.value().find(type);
                if (it == cit.value().end()) {
                    cit.value().insert(type, b);
                }
            }
        }
    }
}
EventDataType gLineChart::Miny()
{
    if (m_codes.size() == 0) return 0;
    if (!m_day) return 0;

    bool first = false;
    EventDataType min = 0, tmp;

    for (const auto code : m_codes) {
        if (!m_enabled[code] || !m_day->channelExists(code)) continue;

        tmp = m_day->Min(code);

        if (!first) {
            min = tmp;
            first = true;
        } else {
            min = qMin(tmp, min);
        }
    }
    if (!first) min = 0;

    m_miny = min;

    return min;
}
EventDataType gLineChart::Maxy()
{
    if (m_codes.size() == 0) return 0;
    if (!m_day) return 0;

    bool first = false;
    EventDataType max = 0, tmp;

    for (const auto code : m_codes) {
        if (!m_enabled[code] || !m_day->channelExists(code)) continue;

        tmp = m_day->Max(code);
        if (!first) {
            max = tmp;
            first = true;
        } else {
            max = qMax(tmp, max);
        }
    }
    if (!first) max = 0;
    m_maxy = max;
    return max;

//    return Layer::Maxy() - subtract_offset;
}

bool gLineChart::mouseMoveEvent(QMouseEvent *event, gGraph *graph)
{
    Q_UNUSED(event)
    Q_UNUSED(graph)
    graph->timedRedraw(0);

    return false;
}

QString gLineChart::getMetaString(qint64 time)
{
    if (!m_day) return lasttext;
    lasttext = QString();

    EventDataType val;

    EventDataType ipap = 0, epap = 0;
    bool addPS = false;

    for (const auto code : m_codes) {
        if (m_day->channelHasData(code)) {
            val = m_day->lookupValue(code, time, m_square_plot);
            lasttext += " "+QString("%1: %2").arg(schema::channel[code].label()).arg(val,0,'f',2); //.arg(schema::channel[code].units());

            if (code == CPAP_IPAP) {
                ipap = val;
                addPS = true;
            }
            if (code == CPAP_EPAP) {
                epap = val;
            }
        }

    }
    if (addPS) {
        val = ipap - epap;
        lasttext += " "+QString("%1: %2").arg(schema::channel[CPAP_PS].label()).arg(val,0,'f',2);//.arg(schema::channel[CPAP_PS].units());
    }

    lasttime = time;
    return lasttext;
}

// Time Domain Line Chart
void gLineChart::paint(QPainter &painter, gGraph &w, const QRegion &region)
{
    QRectF rect = region.boundingRect();
    rect.translate(0.0f, 0.001f);
    // TODO: Just use QRect directly.
    int left = rect.left();
    int top = rect.top();
    int width = rect.width();
    int height = rect.height();

    if (!m_visible) {
        return;
    }

    if (!m_day) {
        return;
    }

    //if (!m_day->channelExists(m_code)) return;

    if (width < 0) {
        return;
    }


    top++;

    double minx, maxx;


    if (w.blockZoom()) {
        minx = w.rmin_x, maxx = w.rmax_x;
    } else {
        maxx = w.max_x, minx = w.min_x;
    }


    // hmmm.. subtract_offset..

    EventDataType miny = m_physminy;
    EventDataType maxy = m_physmaxy;

    w.roundY(miny, maxy);

//#define DEBUG_AUTOSCALER
#ifdef DEBUG_AUTOSCALER
    QString a = QString().sprintf("%.2f - %.2f",miny, maxy);
    w.renderText(a,width/2,top-5);
#endif

    // the middle of minx and maxy does not have to be the center...

    double logX = painter.device()->logicalDpiX();
    double physX = painter.device()->physicalDpiX();
    double ratioX = physX / logX * w.printScaleX();
    double logY = painter.device()->logicalDpiY();
    double physY = painter.device()->physicalDpiY();
    double ratioY = physY / logY * w.printScaleY();

    double xx = maxx - minx;
    double xmult = double(width) / xx;

    EventDataType yy = maxy - miny;
    EventDataType ymult = EventDataType(height - 3) / yy; // time to pixel conversion multiplier

    // Return on screwy min/max conditions
    if (xx < 0) {
        return;
    }

    if (yy <= 0) {
        if (miny == 0) {
            return;
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, AppSetting->antiAliasing());

    //bool mouseover = false;
    if (rect.contains(w.graphView()->currentMousePos())) {
        //mouseover = true;

        painter.fillRect(rect, QBrush(QColor(255,255,245,128)));
    }


    bool linecursormode = AppSetting->lineCursorMode();
    ////////////////////////////////////////////////////////////////////////
    // Display Line Cursor
    ////////////////////////////////////////////////////////////////////////
    if (linecursormode) {
        double time = w.currentTime();

        if ((time > minx) && (time < maxx)) {
            double xpos = (time - double(minx)) * xmult;
            painter.setPen(QPen(QBrush(QColor(0,255,0,255)),1));
            painter.drawLine(left+xpos, top-w.marginTop()-3, left+xpos, top+height+w.bottom-1);
        }

        if ((time != lasttime) || lasttext.isEmpty()) {
            getMetaString(time);
        }

        if (m_codes[0] != CPAP_FlowRate) {
            QString text = lasttext;

            int wid, h;
            GetTextExtent(text, wid, h);
            w.renderText(text, left , top-6); //(h+(4 * w.printScaleY())));  //+ width/2 - wid/2
        }
    }

    double lastpx, lastpy;
    double px, py;
    int idx;
    bool done;
    double x0, xL;
    double sr = 0.0;
    int sam;
    int minz, maxz;

    // Draw bounding box
    painter.setPen(QColor(Qt::black));
    painter.drawRect(rect);

    width--;
    height -= 2;

    int num_points = 0;
    int visible_points = 0;
    int total_points = 0;
    int total_visible = 0;
    bool square_plot, accel;
    qint64 clockdrift = qint64(p_profile->cpap->clockDrift()) * 1000L;
    qint64 drift = 0;

    QHash<ChannelID, QVector<EventList *> >::iterator ci;

    //m_line_color=schema::channel[m_code].defaultColor();
    int legendx = left + width;

    int codepoints;

    painter.setClipRect(left, top, width, height+1);
    painter.setClipping(true);

    painter.setFont(*defaultfont);
    bool showDottedLines = true;

    // Unset Dotted lines visible status, so we only draw necessary labels later
    for (auto & dot : m_dotlines) {
        dot.visible = false;
    }

    ChannelID code;

    float lineThickness = AppSetting->lineThickness()+0.001F;

    for (const auto & code : m_codes) {
        const schema::Channel &chan = schema::channel[code];

        ////////////////////////////////////////////////////////////////////////
        // Draw the Channel Threshold dotted lines, and flow waveform centreline
        ////////////////////////////////////////////////////////////////////////
        if (showDottedLines) {
            for (auto & dot : m_dotlines) {
                if ((dot.code != code) || (!m_dot_enabled[dot.code][dot.type]) || (!dot.available) || (!m_enabled[dot.code])) {
                    continue;
                }
                //schema::Channel & chan = schema::channel[code];

                dot.visible = true;
                QColor color = chan.calc[dot.type].color;
                color.setAlpha(200);
                painter.setPen(QPen(QBrush(color), lineThickness, Qt::DotLine));
                EventDataType y=top + height + 1 - ((dot.value - miny) * ymult);
                painter.drawLine(left + 1, y, left + 1 + width, y);

            }
        }
        if (!m_enabled[code]) continue;


        lines.clear();

        codepoints = 0;

        // For each session...
        for (const auto & sess : m_day->sessions) {
            if (!sess) {
                qWarning() << "gLineChart::Plot() nullptr Session Record.. This should not happen";
                continue;
            }

            drift = (sess->type() == MT_CPAP) ? clockdrift : 0;

            if (!sess->enabled()) { continue; }

            schema::Channel ch = schema::channel[code];
            bool fndbetter = false;

            for (const auto & c : ch.m_links) {
                ci = sess->eventlist.find(c->id());

                if (ci != sess->eventlist.end()) {
                    fndbetter = true;
                    break;
                }

            }

            if (!fndbetter) {
                ci = sess->eventlist.find(code);

                if (ci == sess->eventlist.end()) { continue; }
            }

            num_points = 0;

            for (const auto & ni : ci.value()) {
                num_points += ni->count();
            }

            total_points += num_points;
            codepoints += num_points;

            // Max number of samples taken from samples per pixel for better min/max values
            const int num_averages = 20;

            for (auto & ni : ci.value()) {
                EventList & el = (*ni);

                accel = (el.type() == EVL_Waveform); // Turn on acceleration if this is a waveform.

                if (accel) {
                    sr = el.rate();         // Time distance between samples

                    if (sr <= 0) {
                        qWarning() << "qLineChart::Plot() assert(sr>0)";
                        continue;
                    }
                }

                if (m_disable_accel) { accel = false; }

                square_plot = m_square_plot;

                if (accel || num_points > 20000) { // Don't square plot if too many points or waveform
                    square_plot = false;
                }

                int siz = el.count();

                if (siz <= 1) { continue; } // Don't bother drawing 1 point or less.

                x0 = el.time(0) + drift;
                xL = el.time(siz - 1) + drift;

                if (maxx < x0) { continue; }

                if (xL < minx) { continue; }

                if (x0 > xL) {
                    if (siz == 2) { // this happens on CPAP
                        quint32 t = el.getTime()[0];
                        el.getTime()[0] = el.getTime()[1];
                        el.getTime()[1] = t;
                        EventStoreType d = el.getData()[0];
                        el.getData()[0] = el.getData()[1];
                        el.getData()[1] = d;

                    } else {
                        qDebug() << "Reversed order sample fed to gLineChart - ignored.";
                        continue;
                        //assert(x1<x2);
                    }
                }

                if (accel) {
                    //x1=el.time(1);

                    double XR = xx / sr;
                    double Z1 = MAX(x0, minx);
                    double Z2 = MIN(xL, maxx);
                    double ZD = Z2 - Z1;
                    double ZR = ZD / sr;
                    double ZQ = ZR / XR;
                    double ZW = ZR / (width * ZQ);
                    visible_points += ZR * ZQ;

//                    if (accel && n > 0) {
//                        sam = 1;
//                    }

                    if (ZW < num_averages) {
                        sam = 1;
                        accel = false;
                    } else {
                        sam = ZW / num_averages;

                        if (sam < 1) {
                            sam = 1;
                            accel = false;
                        }
                    }

                    // Prepare the min max y values if we still are accelerating this plot
                    if (accel) {
                        for (int i = 0; i < width; i++) {
                            m_drawlist[i].setX(height);
                            m_drawlist[i].setY(0);
                        }

                        minz = width;
                        maxz = 0;
                    }

                    total_visible += visible_points;
                } else {
                    sam = 1;
                }

                // these calculations over estimate
                // The Z? values are much more accurate

                idx = 0;

                if (el.type() == EVL_Waveform)  {
                    // We can skip data previous to minx if this is a waveform

                    if (minx > x0) {
                        double j = minx - x0; // == starting min of first sample in this segment
                        idx = (j / sr);
                        //idx/=(sam*num_averages);
                        //idx*=(sam*num_averages);
                        // Loose the precision
                        idx += sam - (idx % sam);

                    } // else just start from the beginning
                }

                int xst = left + 1;
                int yst = top + height + 1;

                double time;
                EventDataType data;
                EventDataType gain = el.gain();

                done = false;

                if (el.type() == EVL_Waveform) { // Waveform Plot
                    if (idx > sam) { idx -= sam; }

                    time = el.time(idx) + drift;
                    double rate = double(sr) * double(sam);
                    EventStoreType *ptr = el.rawData() + idx;
                    if ((unsigned) siz > el.count())
                        siz = el.count();

                    if (accel) {
                        //////////////////////////////////////////////////////////////////
                        // Accelerated Waveform Plot
                        //////////////////////////////////////////////////////////////////

                        for (int i = idx; i <= siz; i += sam, ptr += sam) {
                            time += rate;
                            // This is much faster than QVector access.
                            data = *ptr * gain;

                            // Scale the time scale X to pixel scale X
                            px = ((time - minx) * xmult);

                            // Same for Y scale, with gain factored in nmult
                            py = ((data - miny) * ymult);

                            // In accel mode, each pixel has a min/max Y value.
                            // m_drawlist's index is the pixel index for the X pixel axis.
                            //int z = round(px); // Hmmm... round may screw this up.
                            int z = (px>=0.5)?(int(px)+1):int(px);

                            if (z < minz) {
                                minz = z;    // minz=First pixel
                            }

                            if (z > maxz) {
                                maxz = z;    // maxz=Last pixel
                            }

                            if (minz < 0) {
                                qDebug() << "gLineChart::Plot() minz<0  should never happen!! minz =" << minz;
                                minz = 0;
                            }

                            if (maxz > max_drawlist_size) {
                                qDebug() << "gLineChart::Plot() maxz>max_drawlist_size!!!! maxz = " << maxz <<
                                         " max_drawlist_size =" << max_drawlist_size;
                                maxz = max_drawlist_size;
                            }

                            // Update the Y pixel bounds.
                            if (py < m_drawlist[z].x()) {
                                m_drawlist[z].setX(py);
                            }

                            if (py > m_drawlist[z].y()) {
                                m_drawlist[z].setY(py);
                            }

                            if (time > maxx) {
                                done = true;
                                break;
                            }

                        }

                        // Plot compressed accelerated vertex list
                        if (maxz > width) {
                            maxz = width;
                        }

                        float ax1, ay1;
                        QPointF *drl = m_drawlist + minz;
                        // Don't need to cap VertexBuffer here, as it's limited to max_drawlist_size anyway


                        // Cap within VertexBuffer capacity, one vertex per line point
//                        int np = (maxz - minz) * 2;

                        for (int i = minz; i < maxz; i++, drl++) {
                            ax1 = drl->x();
                            ay1 = drl->y();
                            lines.append(QLine(xst + i, yst - ax1, xst + i, yst - ay1));
                        }

                    } else { // Zoomed in Waveform
                        //////////////////////////////////////////////////////////////////
                        // Normal Waveform Plot
                        //////////////////////////////////////////////////////////////////
                        // Prime first point
                        data = (*ptr + el.offset()) * gain;
                        lastpx = xst + ((time - minx) * xmult);
                        lastpy = yst - ((data - miny) * ymult);
                        siz--;
                        for (int i = idx; i < siz; i += sam) {
                            ptr += sam;
                            time += rate;

                            data = (*ptr + el.offset()) * gain;

                            px = xst + ((time - minx) * xmult); // Scale the time scale X to pixel scale X
                            py = yst - ((data - miny) * ymult); // Same for Y scale, with precomputed gain
                            //py=yst-((data - ymin) * nmult);   // Same for Y scale, with precomputed gain

                            lines.append(QLine(lastpx, lastpy, px, py));

                            lastpx = px;
                            lastpy = py;

                            if (time >= maxx) {
                                done = true;
                                break;
                            }
                        }
                    }

                    painter.setPen(QPen(chan.defaultColor(), lineThickness));
                    painter.drawLines(lines);
                    w.graphView()->lines_drawn_this_frame += lines.count();
                    lines.clear();

                } else  {
                    //////////////////////////////////////////////////////////////////
                    // Standard events/zoomed in Plot
                    //////////////////////////////////////////////////////////////////

                    double start = el.first() + drift;

                    quint32 *tptr = el.rawTime();

                    int idx = 0;

                    if (siz > 15) {
                        // Prime a bit...
                        for (; idx < siz; ++idx) {
                            time = start + *tptr++;

                            if (time >= minx) {
                                break;
                            }
                        }

                        if (idx > 0) {
                            idx--;
                        }
                    }

                    // Step one backwards if possible (to draw through the left margin)
                    EventStoreType *dptr = el.rawData() + idx;
                    tptr = el.rawTime() + idx;

                    time = start + *tptr++;
                    data = (*dptr++ + el.offset()) * gain;

                    idx++;

                    lastpx = xst + ((time - minx) * xmult); // Scale the time scale X to pixel scale X
                    lastpy = yst - ((data - miny) * ymult); // Same for Y scale without precomputed gain

                    siz -= idx;

//                    int gs = siz << 1;

//                    if (square_plot) {
//                        gs <<= 1;
//                    }

                    // Unrolling square plot outside of loop to gain a minor speed improvement.
                    EventStoreType *eptr = dptr + siz;

                    if (square_plot) {
                        for (; dptr < eptr; dptr++) {
                            time = start + *tptr++;
                            data = gain * (*dptr + el.offset());

                            px = xst + ((time - minx) * xmult); // Scale the time scale X to pixel scale X
                            py = yst - ((data - miny) * ymult); // Same for Y scale without precomputed gain

                            // Horizontal lines are easy to cap
                            if (py == lastpy) {
                                // Cap px to left margin
                                if (lastpx < xst) { lastpx = xst; }

                                // Cap px to right margin
                                if (px > xst + width) { px = xst + width; }

//                                lines.append(QLine(lastpx, lastpy, px, lastpy));
//                                lines.append(QLine(px, lastpy, px, py));
                            } // else {
                                // Letting the scissor do the dirty work for non horizontal lines
                                // This really should be changed, as it might be cause that weird
                                // display glitch on Linux..

                                lines.append(QLine(lastpx, lastpy, px, lastpy));
                                lines.append(QLine(px, lastpy, px, py));
//                            }

                            lastpx = px;
                            lastpy = py;

                            if (time > maxx) {
                                done = true; // Let this iteration finish.. (This point will be in far clipping)
                                break;
                            }
                        }
                    } else {
                        for (; dptr < eptr; dptr++) {
                            //for (int i=0;i<siz;i++) {
                            time = start + *tptr++;
                            data = gain * (*dptr + el.offset());

                            px = xst + ((time - minx) * xmult); // Scale the time scale X to pixel scale X
                            py = yst - ((data - miny) * ymult); // Same for Y scale without precomputed gain

                            // Horizontal lines are easy to cap
                            if (py == lastpy) {
                                // Cap px to left margin
                                if (lastpx < xst) { lastpx = xst; }

                                // Cap px to right margin
                                if (px > xst + width) { px = xst + width; }

                              //  lines.append(QLine(lastpx, lastpy, px, py));
                            } //else {
                                // Letting the scissor do the dirty work for non horizontal lines
                                // This really should be changed, as it might be cause that weird
                                // display glitch on Linux..
                                lines.append(QLine(lastpx, lastpy, px, py));
                            //}

                            lastpx = px;
                            lastpy = py;

                            if (time > maxx) { // Past right edge, abort further drawing..
                                done = true;
                                break;
                            }
                        }
                    }
                    painter.setPen(QPen(chan.defaultColor(), lineThickness));
                    painter.drawLines(lines);
                    w.graphView()->lines_drawn_this_frame+=lines.count();
                    lines.clear();

                }

                if (done) { break; }
            }
        }


//        painter.setPen(QPen(m_colors[gi],lineThickness));
//        painter.drawLines(lines);
//        w.graphView()->lines_drawn_this_frame+=lines.count();
//        lines.clear();

        ////////////////////////////////////////////////////////////////////
        // Draw Legends on the top line
        ////////////////////////////////////////////////////////////////////

        if ((codepoints > 0)) {
            QString text = schema::channel[code].label();
            QRectF rec(0, rect.top()-3, 0,0);
            rec = painter.boundingRect(rec, Qt::AlignBottom | Qt::AlignLeft, text);
            rec.moveRight(legendx);
            legendx -= rec.width();
            painter.setClipping(false);
            painter.setPen(Qt::black);
            painter.drawText(rec, Qt::AlignBottom | Qt::AlignRight, text);

            float ps = 2.0 * ratioY;
            ps = qMax(ps, 1.0f);

            painter.setPen(QPen(chan.defaultColor(), ps));
            int linewidth = (10 * ratioX);
            int yp = rec.top()+(rec.height()/2);
            painter.drawLine(QLineF(rec.left()-linewidth, yp+0.001f , rec.left()-(2 * ratioX), yp));

            painter.setClipping(true);

            legendx -= linewidth + (2*ratioX);
        }
    }
    painter.setClipping(false);

    ////////////////////////////////////////////////////////////////////
    // Draw Channel Threshold legend markers
    ////////////////////////////////////////////////////////////////////
    for (const auto & dot : m_dotlines) {
        if (!dot.visible) continue;
        code = dot.code;
        schema::Channel &chan = schema::channel[code];
        int linewidth = (10 * ratioX);
        QRectF rec(0, rect.top()-3, 0,0);

        QString text = chan.calc[dot.type].label();
        rec = painter.boundingRect(rec, Qt::AlignBottom | Qt::AlignLeft, text);
        rec.moveRight(legendx);
        legendx -= rec.width();
        painter.setPen(Qt::black);
        painter.drawText(rec, Qt::AlignBottom | Qt::AlignRight, text);

        QColor color = chan.calc[dot.type].color;
        color.setAlpha(200);
        float ps = 2.0 * ratioY;
        ps = qMax(ps, 1.0f);
        painter.setPen(QPen(QBrush(color), ps,Qt::DotLine));

        int yp = rec.top()+(rec.height()/2);
        painter.drawLine(QLineF(rec.left()-linewidth, yp+0.001f, rec.left()-(2 * ratioX), yp));
        legendx -= linewidth + (2*ratioX);
    }

    painter.setClipping(true);

    if (!total_points) { // No Data?
            QString msg = QObject::tr("Plots Disabled");
            int x, y;
            GetTextExtent(msg, x, y, bigfont);
            w.renderText(msg, rect, Qt::AlignCenter, 0, Qt::gray, bigfont);
    }

    painter.setClipping(false);

    // Calculate combined session times within selected area...
    double first, last;
    double time = 0;

    // Calculate the CPAP session time.
    for (auto & sess : m_day->sessions) {
        if (!sess->enabled() || (sess->type() != MT_CPAP)) continue;

        first = sess->first();
        last = sess->last();

        if (last < w.min_x) { continue; }
        if (first > w.max_x) { continue; }

        if (first < w.min_x) {
            first = w.min_x;
        }

        if (last > w.max_x) {
            last = w.max_x;
        }

        time += last - first;
    }


    time /= 1000;

    QList<ChannelID> ahilist;
    ahilist.push_back(CPAP_Hypopnea);
    ahilist.push_back(CPAP_Obstructive);
    ahilist.push_back(CPAP_Apnea);
    ahilist.push_back(CPAP_ClearAirway);

    QList<ChannelID> extras;
    extras.push_back(CPAP_NRI);
    extras.push_back(CPAP_UserFlag1);
    extras.push_back(CPAP_UserFlag2);

    double sum = 0;
    int cnt = 0;

    // Draw the linechart overlays
    if (m_day && (AppSetting->lineCursorMode() || (m_codes[0]==CPAP_FlowRate))) {
        bool blockhover = false;
        for (auto fit=flags.begin(), end=flags.end(); fit != end; ++fit) {
            code = fit.key();
            if ((!m_flags_enabled[code]) || (!m_day->channelExists(code))) continue;
            gLineOverlayBar * lob = fit.value();
            lob->setBlockHover(blockhover);
            lob->paint(painter, w, region);
            if (lob->hover()) blockhover = true; // did it render a hover over?

            if (ahilist.contains(code)) {
                sum += lob->sum();
                cnt += lob->count();
            }
        }
    }
    if (m_codes[0] == CPAP_FlowRate) {
        float hours = time / 3600.0;
        int h = time / 3600;
        int m = int(time / 60) % 60;
        int s = int(time) % 60;

        double f = double(cnt) / hours; // / (sum / 3600.0);
        QString txt = QObject::tr("Duration %1:%2:%3").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s,2,10,QChar('0')) + " "+
                QObject::tr("AHI %1").arg(f,0,'f',2);// +" " +
//                QObject::tr("Events %1").arg(cnt) + " " +
//                QObject::tr("Hours %1").arg(hours,0,'f',2);

        if (linecursormode) txt+=lasttext;

        w.renderText(txt,left,top-5);
    }


   // painter.setRenderHint(QPainter::Antialiasing, false);
}
