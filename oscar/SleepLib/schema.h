/* Schema Header (Parse Channel XML data)
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SCHEMA_H
#define SCHEMA_H

#include <QColor>
#include <QHash>
#include <QVariant>
#include <QString>
#include "machine_common.h"

namespace GraphFlags {
const quint32 Shadow = 1;
const quint32 Foobar = 2;
const quint32 XTicker = 4;
const quint32 YTicker = 8;
const quint32 XGrid = 16;
const quint32 YGrid = 32;
}

enum ChannelCalcType {
    Calc_Zero, Calc_Min, Calc_Middle, Calc_Perc, Calc_Max, Calc_UpperThresh, Calc_LowerThresh
};

struct ChannelCalc {
public:
    ChannelCalc() {
        code = 0;
        enabled = false;
        color = Qt::black;
        type = Calc_Zero;
    }
    ChannelCalc(const ChannelCalc & copy) {
        code = copy.code;
        color = copy.color;
        enabled = copy.enabled;
        type = copy.type;
    }
    ChannelCalc(ChannelID code, ChannelCalcType type, QColor color, bool enabled):
        code(code), type(type), color(color), enabled(enabled) {}

    QString label();

    ChannelID code;
    ChannelCalcType type;
    QColor color;
    bool enabled;
};

namespace schema {
void resetChannels();
void setOrders();


enum Function {
    NONE = 0, AVG, WAVG, MIN, MAX, SUM, CNT, P90, CPH, SPH, HOURS, SET
};

enum ChanType {
    DATA = 1,
    SETTING = 2,
    FLAG = 4,
    MINOR_FLAG = 8,
    SPAN = 16,
    WAVEFORM = 32,
    UNKNOWN = 64,

    ALL = 0xFFFF
};

enum DataType {
    DEFAULT = 0, INTEGER, BOOL, DOUBLE, STRING, RICHTEXT, DATE, TIME, DATETIME, LOOKUP
};
enum ScopeType {
    GLOBAL = 0, MACHINE, DAY, SESSION
};
class Channel;
extern Channel EmptyChannel;

/*! \class Channel
    \brief Contains information about a SleepLib data Channel (aka signals)
    */
class Channel
{
  public:
    Channel() { m_id = 0; m_upperThreshold = 0; m_lowerThreshold = 0; m_enabled = true; m_order = 255; m_machtype = MT_UNKNOWN; m_showInOverview = false; }
    Channel(ChannelID id, ChanType type, MachineType machtype, ScopeType scope, QString code, QString fullname,
            QString description, QString label, QString unit, DataType datatype = DEFAULT, QColor = Qt::black,
            int link = 0);
    void addColor(Function f, QColor color) { m_colors[f] = color; }
    void addOption(int i, QString option) { m_options[i] = option; }

    inline ChannelID id() const { return m_id; }
    inline ChanType type() const { return m_type; }
    inline DataType datatype() const { return m_datatype; }
    inline MachineType machtype() const { return m_machtype; }
    const QString &code() { return m_code; }
    const QString &fullname() { return m_fullname; }
    const QString &description() { return m_description; }
    const QString &label() { return m_label; }
    const QString &units() { return m_unit; }
    inline short order() const { return m_order; }

    bool showInOverview() { return m_showInOverview; }

    inline EventDataType upperThreshold() const { return m_upperThreshold; }
    inline EventDataType lowerThreshold() const { return m_lowerThreshold; }
    inline QColor upperThresholdColor() const { return m_upperThresholdColor; }
    inline QColor lowerThresholdColor() const { return m_lowerThresholdColor; }

    inline ChannelID linkid() const { return m_link; }


    void setFullname(QString fullname) { m_fullname = fullname; }
    void setLabel(QString label) { m_label = label; }
    void setType(ChanType type) { m_type = type; }
    void setUnit(QString unit) { m_unit = unit; }
    void setDescription(QString desc) { m_description = desc; }
    void setUpperThreshold(EventDataType value) { m_upperThreshold = value; }
    void setUpperThresholdColor(QColor color) { m_upperThresholdColor = color; }
    void setLowerThreshold(EventDataType value) { m_lowerThreshold = value; }
    void setLowerThresholdColor(QColor color) { m_lowerThresholdColor = color; }
    void setOrder(short order) { m_order = order; }

    void setShowInOverview(bool b) { m_showInOverview = b; }

    QString option(int i) {
        if (m_options.contains(i)) {
            return m_options[i];
        }

        return QString();
    }
    inline QColor defaultColor() const { return m_defaultcolor; }
    inline void setDefaultColor(QColor color) { m_defaultcolor = color; }
    QHash<int, QString> m_options;
    QHash<Function, QColor> m_colors;
    QList<Channel *> m_links;              // better versions of this data type
    bool isNull();

    inline bool enabled() const { return m_enabled; }
    void setEnabled(bool value) { m_enabled = value; }

    QHash<ChannelCalcType, ChannelCalc> calc;

  protected:


    int m_id;

    ChanType m_type;
    MachineType m_machtype;
    ScopeType m_scope;

    QString m_code; // Untranslatable

    QString m_fullname; // Translatable Name
    QString m_description;
    QString m_label;
    QString m_unit;

    QString default_fullname;
    QString default_label;
    QString default_description;

    DataType m_datatype;
    QColor m_defaultcolor;


    int m_link;

    EventDataType m_upperThreshold;
    EventDataType m_lowerThreshold;
    QColor m_upperThresholdColor;
    QColor m_lowerThresholdColor;


    bool m_enabled;
    short m_order;

    bool m_showInOverview;
};

/*! \class ChannelList
    \brief A list containing a group of Channel objects, and XML storage and retrieval capability
    */
class ChannelList
{
  public:
    ChannelList();
    virtual ~ChannelList();

    //! \brief Loads Channel list from XML file specified by filename
    bool Load(QString filename);

    //! \brief Stores Channel list to XML file specified by filename
    bool Save(QString filename = QString());

    void add(QString group, Channel *chan);

    //! \brief Looks up Channel in this List with the index idx, returns EmptyChannel if not found
    Channel & operator[](ChannelID idx) {
        if (channels.contains(idx)) {
            return *channels[idx];
        } else {
            return EmptyChannel;
        }
    }
    //! \brief Looks up Channel from this list by name, returns Empty Channel if not found.
    Channel &operator[](QString name) {
        if (names.contains(name)) {
            return *names[name];
        } else {
            return EmptyChannel;
        }
    }

    //! \brief Channel List indexed by integer ID
    QHash<ChannelID, Channel *> channels;

    //! \brief Channel List index by name
    QHash<QString, Channel *> names;

    //! \brief Channel List indexed by group
    QHash<QString, QHash<QString, Channel *> > groups;
    QString m_doctype;
};
extern ChannelList channel;

/*enum LayerType {
    UnspecifiedLayer, Waveform, Flag, Overlay, Group
};


// ?????
class Layer
{
public:
    Layer(ChannelID code, QColor colour, QString label=QString());
    virtual ~Layer();
    Layer *addLayer(Layer *layer);// { m_layers.push_back(layer); return layer; }
    void setMin(EventDataType min) { m_min=min; m_hasmin=true; }
    void setMax(EventDataType max) { m_max=max; m_hasmax=true; }
    EventDataType Min() { return m_min; }
    EventDataType Max() { return m_max; }
    bool visible() { return m_visible; }
    void setVisible(bool b) { m_visible=b; }
protected:
    LayerType m_type;
    ChannelID m_code;
    QColor m_colour;
    QString m_label;
    EventDataType m_min;
    EventDataType m_max;
    bool m_hasmin;
    bool m_hasmax;
    bool m_visible;
    QVector<Layer *> m_layers;
};

class WaveformLayer: public Layer
{
public:
    WaveformLayer(ChannelID code, QColor colour, float min=0, float max=0);
    virtual ~WaveformLayer();
};

enum FlagVisual { Bar, Span, Dot };
class OverlayLayer: public Layer
{
public:
    OverlayLayer(ChannelID code, QColor colour, FlagVisual visual=Bar);
    virtual ~OverlayLayer();
protected:
    FlagVisual m_visual;
};
class GroupLayer: public Layer // Effectively an empty Layer container
{
public:
    GroupLayer();
    virtual ~GroupLayer();
};
class FlagGroupLayer: public GroupLayer
{
public:
    FlagGroupLayer();
    virtual ~FlagGroupLayer();
};
class FlagLayer: public Layer
{
public:
    FlagLayer(ChannelID code, QColor colour, FlagVisual visual=Bar);
    virtual ~FlagLayer();
protected:
    FlagVisual m_visual;
};
class Graph
{
public:
    Graph(QString name,quint32 flags=GraphFlags::XTicker | GraphFlags::YTicker | GraphFlags::XGrid);
    Layer *addLayer(Layer *layer) { m_layers.push_back(layer); return layer; }
    int height() { if (m_visible) return m_height; else return 0;}
    void setHeight(int h) { m_height=h; }
    bool visible() { return m_visible; }
    void setVisible(bool b) { m_visible=b; }
protected:
    QString m_name;
    int m_height;
    QVector<Layer *> m_layers;
    bool m_visible;
};
class GraphGroup
{
public:
    GraphGroup(QString name);
    GraphGroup();
    Graph *addGraph(Graph *graph) { m_graphs.push_back(graph); return graph; }
protected:
    QVector<Graph *>m_graphs;
}; */

void init();

} // namespace

#endif // SCHEMA_H
