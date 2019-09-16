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
    ChannelCalc(const ChannelCalc & copy) = default;
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

///
/// \brief The ChanType enum defines the type of data channel.  Bit flags so multiple settings are possible.
/// DATA: A single number such as Height, ZombieMeter.
/// SETTING: Machine setting, such as EPR, temperature, Ramp enabled.
/// FLAG: Event flags reported by CPAP machine.  Each flag has its own channel.
/// MINOR_FLAG: More event flags such as PressurePulse and TimedBreath.
/// SPAN: A flag that has a timespan associated with it (CSR, LeakSpan, Ramp, ...).
/// WAVEFORM: A waveform such as flow rate.
/// UNKNOWN: Some PRS1 flags, but not sure what they are for.  Considered to be minor flags.
///
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

    //! \brief Unique identifier of channel.  Value set when channel is created.  See schema.cpp and loader modules.
    inline ChannelID id() const { return m_id; }

    //! \brief Type of channel, such as WAVEFORM, FLAG, etc.  See ChanType enum.
    inline ChanType type() const { return m_type; }

    //! \brief Data format such as integer vs RTF, called Field Type in channel initializers in schema.cpp
    inline DataType datatype() const { return m_datatype; }

    //! \brief Type of machine (CPAP, Oximeter, Journal, etc.) as defined in machine_common.h. Set in channel initializers in schema.cpp
    inline MachineType machtype() const { return m_machtype; }

    //! \brief Unique string identifier for this channel.  Must not be translated.  Later used as a unique key to identify graph derived from this channel.
    const QString &code() { return m_code; }

    //! \brief Full name of channel.  Translatable.  Used generally for channel names such as rows on Statistics page.
    const QString &fullname() { return m_fullname; }

    //! \brief Short description of what this channel does.  Translatable.  Used in tooltips for graphs to explain what the graph shows.
    const QString &description() { return m_description; }

    //! \brief Short-form label to indicate this channel on screen.  Translatable.  Used for vertical labels in graphs.
    //! Can be changed in Preferences dialog.
    const QString &label() { return m_label; }

    //! \brief Units, such as cmH2O, events per hour, etc.  See STR_UNIT_* for possible values.
    const QString &units() { return m_unit; }

    //! \brief Seems to be some kind of sort order for event flags.  Not sure this is used.
    inline short order() const { return m_order; }

    //! \brief Whether or not chart of this channel is to be shown on Overview page
    //! Initial settings of this flag for individual channels set in schema.cpp.
    //! May be changed by user in Preferences Dialog.
    bool showInOverview() { return m_showInOverview; }

    //! \brief Upper threshold for channel, apparently used in Statistics.cpp for calculation purposes.  Not sure if it is used elsewhere.
    inline EventDataType upperThreshold() const { return m_upperThreshold; }

    //! \brief Lower threshold for channel, apparently used in Statistics.cpp for calculation purposes.  Not sure if it is used elsewhere.
    inline EventDataType lowerThreshold() const { return m_lowerThreshold; }

    //! \brief Does not appear to be used?
    inline QColor upperThresholdColor() const { return m_upperThresholdColor; }

    //! \brief Does not appear to be used?
    inline QColor lowerThresholdColor() const { return m_lowerThresholdColor; }


    //! \brief Links channels.  Links to better versions of this data type.
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

    //! \brief Retrieves options that may have been set for the channel.  Used for CPAP Mode, EPR level.
    QString option(int i) {
        if (m_options.contains(i)) {
            return m_options[i];
        }

        return QString();
    }

    //! \brief Default color for plotting this channel
    inline QColor defaultColor() const { return m_defaultcolor; }
    inline void setDefaultColor(QColor color) { m_defaultcolor = color; }
    QHash<int, QString> m_options;
    QHash<Function, QColor> m_colors;
    QList<Channel *> m_links;              // better versions of this data type
    bool isNull();

    inline bool enabled() const { return m_enabled; }
    void setEnabled(bool value) { m_enabled = value; }

    //! \brief Types of calculations that can be plotted on this channel and color to be used for plotting
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

void init();

} // namespace schema

#endif // SCHEMA_H
