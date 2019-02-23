/* Statistics Report Generator Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SUMMARY_H
#define SUMMARY_H

#include <QObject>
#include <QHash>
#include <QList>
#include "SleepLib/schema.h"
#include "SleepLib/machine.h"

enum StatCalcType {
    SC_UNDEFINED=0, SC_COLUMNHEADERS, SC_HEADING, SC_SUBHEADING, SC_MEDIAN, SC_AVG, SC_WAVG, SC_90P, SC_MIN, SC_MAX, SC_CPH, SC_SPH, SC_AHI, SC_HOURS, SC_COMPLIANCE, SC_DAYS, SC_ABOVE, SC_BELOW
};

struct StatisticsRow {
    StatisticsRow() { calc=SC_UNDEFINED; }
    StatisticsRow(QString src, QString calc, QString type) {
        this->src = src;
        this->calc = lookupCalc(calc);
        this->type = lookupType(type);
    }
    StatisticsRow(QString src, StatCalcType calc, MachineType type) {
        this->src = src;
        this->calc = calc;
        this->type = type;
    }
    StatisticsRow(const StatisticsRow &copy) {
        src=copy.src;
        calc=copy.calc;
        type=copy.type;
    }
    QString src;
    StatCalcType calc;
    MachineType type;

    StatCalcType lookupCalc(QString calc)
    {
        if (calc.compare("avg",Qt::CaseInsensitive)==0) {
            return SC_AVG;
        } else if (calc.compare("w-avg",Qt::CaseInsensitive)==0) {
            return SC_WAVG;
        } else if (calc.compare("median",Qt::CaseInsensitive)==0) {
            return SC_MEDIAN;
        } else if (calc.compare("90%",Qt::CaseInsensitive)==0) {
            return SC_90P;
        } else if (calc.compare("min", Qt::CaseInsensitive)==0) {
            return SC_MIN;
        } else if (calc.compare("max", Qt::CaseInsensitive)==0) {
            return SC_MAX;
        } else if (calc.compare("cph", Qt::CaseInsensitive)==0) {
            return SC_CPH;
        } else if (calc.compare("sph", Qt::CaseInsensitive)==0) {
            return SC_SPH;
        } else if (calc.compare("ahi", Qt::CaseInsensitive)==0) {
            return SC_AHI;
        } else if (calc.compare("hours", Qt::CaseInsensitive)==0) {
            return SC_HOURS;
        } else if (calc.compare("compliance", Qt::CaseInsensitive)==0) {
            return SC_COMPLIANCE;
        } else if (calc.compare("days", Qt::CaseInsensitive)==0) {
            return SC_DAYS;
        } else if (calc.compare("heading", Qt::CaseInsensitive)==0) {
            return SC_HEADING;
        } else if (calc.compare("subheading", Qt::CaseInsensitive)==0) {
            return SC_SUBHEADING;
        }
        return SC_UNDEFINED;
    }

    MachineType lookupType(QString type)
    {
        if (type.compare("cpap", Qt::CaseInsensitive)==0) {
            return MT_CPAP;
        } else if (type.compare("oximeter", Qt::CaseInsensitive)==0) {
            return MT_OXIMETER;
        } else if (type.compare("sleepstage", Qt::CaseInsensitive)==0) {
            return MT_SLEEPSTAGE;
        }
        return MT_UNKNOWN;
    }


    ChannelID channel() {
        return schema::channel[src].id();
    }

    QString value(QDate start, QDate end);
};

class RXItem {
public:
    RXItem() {
        machine = nullptr;
        ahi = rdi = 0;
        highlight = 0;
        hours = 0;
    }
    RXItem(const RXItem & copy) {
        start = copy.start;
        end = copy.end;
        days = copy.days;
        s_count = copy.s_count;
        s_sum = copy.s_sum;
        ahi = copy.ahi;
        rdi = copy.rdi;
        hours = copy.hours;
        machine = copy.machine;
        relief = copy.relief;
        mode = copy.mode;
        pressure = copy.pressure;
        dates = copy.dates;
        highlight = copy.highlight;
    }
    inline quint64 count(ChannelID id) const {
        QHash<ChannelID, quint64>::const_iterator it = s_count.find(id);
        if (it == s_count.end()) return 0;
        return it.value();
    }
    inline double sum(ChannelID id) const{
        QHash<ChannelID, double>::const_iterator it = s_sum.find(id);
        if (it == s_sum.end()) return 0;
        return it.value();
    }
    QDate start;
    QDate end;
    int days;
    QHash<ChannelID, quint64> s_count;
    QHash<ChannelID, double> s_sum;
    quint64 ahi;
    quint64 rdi;
    double hours;
    Machine * machine;
    QString relief;
    QString mode;
    QString pressure;
    QMap<QDate, Day *> dates;
    short highlight;
};



class Statistics : public QObject
{
    Q_OBJECT
  public:
    explicit Statistics(QObject *parent = 0);

    void loadRXChanges();
    void saveRXChanges();
    void updateRXChanges();

    QString GenerateHTML();
    QString GenerateMachineList();
    QString GenerateRXChanges();

    void UpdateRecordsBox();


  protected:
    QString htmlHeader(bool showheader);
    QString htmlFooter(bool showinfo=true);

    // Using a map to maintain order
    QList<StatisticsRow> rows;
    QMap<StatCalcType, QString> calcnames;
    QMap<MachineType, QString> machinenames;

    QMap<QDate, RXItem> rxitems;

    QList<QDate> record_best_ahi;
    QList<QDate> record_worst_ahi;

  signals:

  public slots:

};

#endif // SUMMARY_H
