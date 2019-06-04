/* Session Testing Support
 *
 * Copyright (c) 2019 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QFile>
#include "sessiontests.h"

static QString ts(qint64 msecs)
{
    // TODO: make this UTC so that tests don't vary by where they're run
    return QDateTime::fromMSecsSinceEpoch(msecs).toString(Qt::ISODate);
}

static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}

static QString dur(qint64 msecs)
{
    qint64 s = msecs / 1000L;
    int h = s / 3600; s -= h * 3600;
    int m = s / 60; s -= m * 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

#define ENUMSTRING(ENUM) case ENUM: s = #ENUM; break
static QString eventListTypeName(EventListType t)
{
    QString s;
    switch (t) {
        ENUMSTRING(EVL_Waveform);
        ENUMSTRING(EVL_Event);
        default:
            s = hex(t);
            qDebug() << qPrintable(s);
    }
    return s;
}

// ChannelIDs are not enums. Instead, they are global variables of the ChannelID type.
// This allows definition of different IDs within different loader plugins, while
// using Qt templates (such as QHash) that require a consistent data type for their key.
//
// Ideally there would be a central ChannelID registry class that could be queried
// for names, make sure there aren't duplicate values, etc. For now we just fill the
// below by hand.
#define CHANNELNAME(CH) if (i == CH) { s = #CH; break; }
extern ChannelID PRS1_TimedBreath, PRS1_HeatedTubing;

static QString settingChannel(ChannelID i)
{
    QString s;
    do {
        CHANNELNAME(CPAP_Mode);
        CHANNELNAME(CPAP_Pressure);
        CHANNELNAME(CPAP_PressureMin);
        CHANNELNAME(CPAP_PressureMax);
        CHANNELNAME(CPAP_EPAP);
        CHANNELNAME(CPAP_IPAP);
        CHANNELNAME(CPAP_PS);
        CHANNELNAME(CPAP_EPAPLo);
        CHANNELNAME(CPAP_EPAPHi);
        CHANNELNAME(CPAP_IPAPLo);
        CHANNELNAME(CPAP_IPAPHi);
        CHANNELNAME(CPAP_PSMin);
        CHANNELNAME(CPAP_PSMax);
        CHANNELNAME(CPAP_RampTime);
        CHANNELNAME(CPAP_RampPressure);
        CHANNELNAME(PRS1_FlexMode);
        CHANNELNAME(PRS1_FlexLevel);
        CHANNELNAME(PRS1_HumidStatus);
        CHANNELNAME(PRS1_HeatedTubing);        
        CHANNELNAME(PRS1_HumidLevel);
        CHANNELNAME(PRS1_SysLock);
        CHANNELNAME(PRS1_SysOneResistSet);
        CHANNELNAME(PRS1_SysOneResistStat);
        CHANNELNAME(PRS1_TimedBreath);
        CHANNELNAME(PRS1_HoseDiam);
        CHANNELNAME(PRS1_AutoOn);
        CHANNELNAME(PRS1_AutoOff);
        CHANNELNAME(PRS1_MaskAlert);
        CHANNELNAME(PRS1_ShowAHI);
        s = hex(i);
        qDebug() << qPrintable(s);
    } while(false);
    return s;
}

static QString eventChannel(ChannelID i)
{
    QString s;
    do {
        CHANNELNAME(CPAP_Obstructive);
        CHANNELNAME(CPAP_Hypopnea);
        CHANNELNAME(CPAP_PB);
        CHANNELNAME(CPAP_LeakTotal);
        CHANNELNAME(CPAP_Leak);
        CHANNELNAME(CPAP_LargeLeak);
        CHANNELNAME(CPAP_IPAP);
        CHANNELNAME(CPAP_EPAP);
        CHANNELNAME(CPAP_PS);
        CHANNELNAME(CPAP_IPAPLo);
        CHANNELNAME(CPAP_IPAPHi);
        CHANNELNAME(CPAP_RespRate);
        CHANNELNAME(CPAP_PTB);
        CHANNELNAME(PRS1_TimedBreath);
        CHANNELNAME(CPAP_MinuteVent);
        CHANNELNAME(CPAP_TidalVolume);
        CHANNELNAME(CPAP_ClearAirway);
        CHANNELNAME(CPAP_FlowLimit);
        CHANNELNAME(CPAP_Snore);
        CHANNELNAME(CPAP_VSnore);
        CHANNELNAME(CPAP_VSnore2);
        CHANNELNAME(CPAP_NRI);
        CHANNELNAME(CPAP_RERA);
        CHANNELNAME(OXI_Pulse);
        CHANNELNAME(OXI_SPO2);
        CHANNELNAME(PRS1_BND);
        CHANNELNAME(CPAP_MaskPressureHi);
        CHANNELNAME(CPAP_FlowRate);
        CHANNELNAME(CPAP_Test1);
        CHANNELNAME(CPAP_Test2);
        CHANNELNAME(CPAP_PressurePulse);
        CHANNELNAME(CPAP_Pressure);
        CHANNELNAME(PRS1_00);
        CHANNELNAME(PRS1_01);
        CHANNELNAME(PRS1_08);
        CHANNELNAME(PRS1_0A);
        CHANNELNAME(PRS1_0B);
        CHANNELNAME(PRS1_0C);
        CHANNELNAME(PRS1_0E);
        CHANNELNAME(PRS1_15);
        CHANNELNAME(CPAP_BrokenSummary);
        s = hex(i);
        qDebug() << qPrintable(s);
    } while(false);
    return s;
}

static QString intList(EventStoreType* data, int count, int limit=-1)
{
    if (limit == -1 || limit > count) limit = count;
    QStringList l;
    for (int i = 0; i < limit; i++) {
        l.push_back(QString::number(data[i]));
    }
    if (limit < count) l.push_back("...");
    QString s = "[ " + l.join(",") + " ]";
    return s;
}

static QString intList(quint32* data, int count, int limit=-1)
{
    if (limit == -1 || limit > count) limit = count;
    QStringList l;
    for (int i = 0; i < limit; i++) {
        l.push_back(QString::number(data[i] / 1000));
    }
    if (limit < count) l.push_back("...");
    QString s = "[ " + l.join(",") + " ]";
    return s;
}

void SessionToYaml(QString filepath, Session* session)
{
    QFile file(filepath);
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << filepath;
        Q_ASSERT(false);
    }
    QTextStream out(&file);

    out << "session:" << endl;
    out << "  id: " << session->session() << endl;
    out << "  start: " << ts(session->first()) << endl;
    out << "  end: " << ts(session->last()) << endl;
    
    if (!session->m_slices.isEmpty()) {
        out << "  slices:" << endl;
        for (auto & slice : session->m_slices) {
            QString s;
            switch (slice.status) {
            case MaskOn: s = "mask on"; break;
            case MaskOff: s = "mask off"; break;
            case EquipmentOff: s = "equipment off"; break;
            default: s = "unknown"; break;
            }
            out << "  - status: " << s << endl;
            out << "    start: " << ts(slice.start) << endl;
            out << "    end: " << ts(slice.end) << endl;
        }
    }
    Day day;
    day.addSession(session);
    out << "  total_time: " << dur(day.total_time()) << endl;
    day.removeSession(session);

    out << "  settings:" << endl;

    // We can't get deterministic ordering from QHash iterators, so we need to create a list
    // of sorted ChannelIDs.
    QList<ChannelID> keys = session->settings.keys();
    std::sort(keys.begin(), keys.end());
    for (QList<ChannelID>::iterator key = keys.begin(); key != keys.end(); key++) {
        QVariant & value = session->settings[*key];
        QString s;
        if ((QMetaType::Type) value.type() == QMetaType::Float) {
            s = QString::number(value.toFloat());  // Print the shortest accurate representation rather than QVariant's full precision.
        } else {
            s = value.toString();
        }
        out << "    " << settingChannel(*key) << ": " << s << endl;
    }

    out << "  events:" << endl;

    keys = session->eventlist.keys();
    std::sort(keys.begin(), keys.end());
    for (QList<ChannelID>::iterator key = keys.begin(); key != keys.end(); key++) {
        out << "    " << eventChannel(*key) << ": " << endl;

        // Note that this is a vector of lists
        QVector<EventList *> &ev = session->eventlist[*key];
        int ev_size = ev.size();
        
        // TODO: See what this actually signifies. Some waveform data seems to have to multiple event lists,
        // which might reflect blocks within the original files, or something else.
        if (ev_size > 2) qDebug() << session->session() << eventChannel(*key) << "ev_size =" << ev_size;

        for (int j = 0; j < ev_size; j++) {
            EventList &e = *ev[j];
            out << "    - count: "  << (qint32)e.count() << endl;
            if (e.count() == 0)
                continue;
            out << "      first: " << ts(e.first()) << endl;
            out << "      last: " << ts(e.last()) << endl;
            out << "      type: " << eventListTypeName(e.type()) << endl;
            out << "      rate: " << e.rate() << endl;
            out << "      gain: " << e.gain() << endl;
            out << "      offset: " << e.offset() << endl;
            if (!e.dimension().isEmpty()) {
                out << "      dimension: " << e.dimension() << endl;
            }
            out << "      data:" << endl;
            out << "        min: " << e.Min() << endl;
            out << "        max: " << e.Max() << endl;
            out << "        raw: " << intList((EventStoreType*) e.m_data.data(), e.count(), 100) << endl;
            if (e.type() != EVL_Waveform) {
                out << "        delta: " << intList((quint32*) e.m_time.data(), e.count(), 100) << endl;
            }
            if (e.hasSecondField()) {
                out << "      data2:" << endl;
                out << "        min: " << e.min2() << endl;
                out << "        max: " << e.max2() << endl;
                out << "        raw: " << intList((EventStoreType*) e.m_data2.data(), e.count(), 100) << endl;
            }
        }
    }
    file.close();
}
