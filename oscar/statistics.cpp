/* Statistics Report Generator Implementation
 *
 * Copyright (c) 2019 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QFile>
#include <QDataStream>
#include <QBuffer>
#include <cmath>

#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QMainWindow>

#include "mainwindow.h"
#include "statistics.h"
#include "cprogressbar.h"
#include "SleepLib/common.h"

extern MainWindow *mainwin;

// HTML components that make up Statistics page and printed report
QString htmlReportHeader = "";      // Page header
QString htmlReportHeaderPrint = "";      // Page header
QString htmlUsage = "";             // CPAP and Oximetry
QString htmlMachineSettings = "";   // Machine (formerly Rx) changes
QString htmlMachines = "";          // Machines used in this profile
QString htmlReportFooter = "";      // Page footer

QString resizeHTMLPixmap(QPixmap &pixmap, int width, int height) {
    QByteArray byteArray;
    QBuffer buffer(&byteArray); // use buffer to store pixmap into byteArray
    buffer.open(QIODevice::WriteOnly);
    pixmap.scaled(width, height, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG");
    return QString("<img src=\"data:image/png;base64,"+byteArray.toBase64()+"\">");
}

QString formatTime(float time)
{
    int hours = time;
    int seconds = time * 3600.0;
    int minutes = (seconds / 60) % 60;
    //seconds %= 60;
    return QString().sprintf("%02i:%02i", hours, minutes); //,seconds);
}


QDataStream & operator>>(QDataStream & in, RXItem & rx)
{
    in >> rx.start;
    in >> rx.end;
    in >> rx.days;
    in >> rx.ahi;
    in >> rx.rdi;
    in >> rx.hours;

    QString loadername;
    in >> loadername;
    QString serial;
    in >> serial;

    MachineLoader * loader = GetLoader(loadername);
    if (loader) {
        rx.machine = p_profile->lookupMachine(serial, loadername);
    } else {
        qDebug() << "Bad machine object" << loadername << serial;
        rx.machine = nullptr;
    }

    in >> rx.relief;
    in >> rx.mode;
    in >> rx.pressure;

    QList<QDate> list;
    in >> list;

    rx.dates.clear();
    for (int i=0; i<list.size(); ++i) {
        QDate date = list.at(i);
        rx.dates[date] = p_profile->FindDay(date, MT_CPAP);
    }

    in >> rx.s_count;
    in >> rx.s_sum;

    return in;
}
QDataStream & operator<<(QDataStream & out, const RXItem & rx)
{
    out << rx.start;
    out << rx.end;
    out << rx.days;
    out << rx.ahi;
    out << rx.rdi;
    out << rx.hours;

    out << rx.machine->loaderName();
    out << rx.machine->serial();

    out << rx.relief;
    out << rx.mode;
    out << rx.pressure;
    out << rx.dates.keys();
    out << rx.s_count;
    out << rx.s_sum;

    return out;
}
void Statistics::loadRXChanges()
{
    QString path = p_profile->Get("{" + STR_GEN_DataFolder + "}/RXChanges.cache" );
    QFile file(path);
    if (!file.open(QFile::ReadOnly)) {
        return;
    }
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 mag32;
    if (in.version() != QDataStream::Qt_5_0) {
    }

    in >> mag32;

    if (mag32 != magic) {
        return;
    }
    quint16 version;
    in >> version;

    in >> rxitems;

}
void Statistics::saveRXChanges()
{
    QString path = p_profile->Get("{" + STR_GEN_DataFolder + "}/RXChanges.cache" );
    QFile file(path);
    if (!file.open(QFile::WriteOnly)) {
        return;
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setVersion(QDataStream::Qt_5_0);
    out << magic;
    out << (quint16)0;
    out << rxitems;

}

bool rxAHILessThan(const RXItem * rx1, const RXItem * rx2)
{

    return (double(rx1->ahi) / rx1->hours) < (double(rx2->ahi) / rx2->hours);
}

void Statistics::updateRXChanges()
{
    // Set conditional progress bar.
    CProgressBar * progress = new CProgressBar (QObject::tr("Updating Statistics cache"), mainwin, p_profile->daylist.count());

    // Clear loaded rx cache
    rxitems.clear();

    // Read the cache from disk
    loadRXChanges();
    QMap<QDate, Day *>::iterator di;

    QMap<QDate, Day *>::iterator it;
    QMap<QDate, Day *>::iterator it_end = p_profile->daylist.end();

    QMap<QDate, RXItem>::iterator ri;
    QMap<QDate, RXItem>::iterator ri_end = rxitems.end();


    quint64 tmp;

    // Scan through each daylist in ascending date order
    for (it = p_profile->daylist.begin(); it != it_end; ++it) {
        const QDate & date = it.key();
        Day * day = it.value();

        progress->add (1);          // Increment progress bar

        Machine * mach = day->machine(MT_CPAP);
        if (mach == nullptr)
            continue;

        if (day->first() == 0) {  // Ignore invalid dates
            qDebug() << "Statistics::updateRXChanges ignoring day with first=0";
            continue;
        }

        bool fnd = false;


        // Scan through pre-existing rxitems list and see if this day is already there.
        ri_end = rxitems.end();
        for (ri = rxitems.begin(); ri != ri_end; ++ri) {
            RXItem & rx = ri.value();

            // Is it date between this rxitems entry date range?
            if ((date >= rx.start) && (date <= rx.end)) {

                if (rx.dates.contains(date)) {
                    // Already there, abort.
                    fnd = true;
                    break;
                }

                // First up, check if fits in date range, but isn't loaded for some reason

                // Need summaries for this, so load them if not present.
                day->OpenSummary();

                // Get list of Event Flags used in this day
                QList<ChannelID> flags = day->getSortedMachineChannels(MT_CPAP, schema::FLAG | schema::MINOR_FLAG | schema::SPAN);

                // Generate the pressure/mode/relief strings
                QString relief = day->getPressureRelief();
                QString mode = day->getCPAPMode();
                QString pressure = day->getPressureSettings();

                // Do this days settings match this rx cache entry?
                if ((rx.relief == relief) && (rx.mode == mode) && (rx.pressure == pressure) && (rx.machine == mach)) {

                    // Update rx cache summaries for each event flag
                    for (int i=0; i < flags.size(); i++) {
                        ChannelID code  = flags.at(i);
                        rx.s_count[code] += day->count(code);
                        rx.s_sum[code] += day->sum(code);
                    }

                    // Update AHI/RDI/Time counts
                    tmp = day->count(CPAP_Hypopnea) + day->count(CPAP_Obstructive) + day->count(CPAP_Apnea) + day->count(CPAP_ClearAirway);
                    rx.ahi += tmp;
                    rx.rdi += tmp + day->count(CPAP_RERA);
                    rx.hours += day->hours(MT_CPAP);

                    // Add this date to RX cache
                    rx.dates[date] = day;
                    rx.days = rx.dates.size();

                    // and we are done
                    fnd = true;
                    break;
                } else {
                    // In this case, the day is within the rx date range, but settings doesn't match the others
                    // So we need to split the rx cache record and insert the new record as it's own.

                    RXItem rx1, rx2;

                    // So first create the new cache entry for current day we are looking at.
                    rx1.start = date;
                    rx1.end = date;
                    rx1.days = 1;

                    // Only this days AHI/RDI counts
                    tmp = day->count(CPAP_Hypopnea) + day->count(CPAP_Obstructive) + day->count(CPAP_Apnea) + day->count(CPAP_ClearAirway);
                    rx1.ahi = tmp;
                    rx1.rdi = tmp + day->count(CPAP_RERA);

                    // Sum and count event flags for this day
                    for (int i=0; i < flags.size(); i++) {
                        ChannelID code  = flags.at(i);
                        rx1.s_count[code] = day->count(code);
                        rx1.s_sum[code] = day->sum(code);
                    }

                    //The rest of this cache record for this day
                    rx1.hours = day->hours(MT_CPAP);
                    rx1.relief = relief;
                    rx1.mode = mode;
                    rx1.pressure = pressure;
                    rx1.machine = mach;
                    rx1.dates[date] = day;

                    // Insert new entry into rx cache
                    rxitems.insert(date, rx1);

                    // now zonk it so we can reuse the variable later
                    //rx1 = RXItem();

                    // Now that's out of the way, we need to splitting the old rx into two,
                    // and recalculate everything before and after today

                    // Copy the old rx.dates, which contains the list of Day records
                    QMap<QDate, Day *> datecopy = rx.dates;

                    // now zap it so we can start fresh
                    rx.dates.clear();

                    rx2.end = rx2.start = rx.end;
                    rx.end = rx.start;

                    // Zonk the summary data, as it needs redoing
                    rx2.ahi = 0;
                    rx2.rdi = 0;
                    rx2.hours = 0;
                    rx.ahi = 0;
                    rx.rdi = 0;
                    rx.hours = 0;
                    rx.s_count.clear();
                    rx2.s_count.clear();
                    rx.s_sum.clear();
                    rx2.s_sum.clear();

                    // Now go through day list and recalculate according to split
                    for (di = datecopy.begin(); di != datecopy.end(); ++di) {

                        // Split everything before date
                        if (di.key() < date) {
                            // Get the day record for this date
                            Day * dy = rx.dates[di.key()] = p_profile->GetDay(di.key(), MT_CPAP);

                            // Update AHI/RDI counts
                            tmp = dy->count(CPAP_Hypopnea) + dy->count(CPAP_Obstructive) + dy->count(CPAP_Apnea) + dy->count(CPAP_ClearAirway);;
                            rx.ahi += tmp;
                            rx.rdi += tmp + dy->count(CPAP_RERA);

                            // Get Event Flags list
                            QList<ChannelID> flags2 = dy->getSortedMachineChannels(MT_CPAP, schema::FLAG | schema::MINOR_FLAG | schema::SPAN);

                            // Update flags counts and sums
                            for (int i=0; i < flags2.size(); i++) {
                                ChannelID code  = flags2.at(i);
                                rx.s_count[code] += dy->count(code);
                                rx.s_sum[code] += dy->sum(code);
                            }

                            // Update time sum
                            rx.hours += dy->hours(MT_CPAP);

                            // Update the last date of this cache entry
                            // (Max here should be unnessary, this should be sequential because we are processing a QMap.)
                            rx.end = di.key(); //qMax(di.key(), rx.end);
                        }

                        // Split everything after date
                        if (di.key() > date) {
                            // Get the day record for this date
                            Day * dy = rx2.dates[di.key()] = p_profile->GetDay(di.key(), MT_CPAP);

                            // Update AHI/RDI counts
                            tmp = dy->count(CPAP_Hypopnea) + dy->count(CPAP_Obstructive) + dy->count(CPAP_Apnea) + dy->count(CPAP_ClearAirway);;
                            rx2.ahi += tmp;
                            rx2.rdi += tmp + dy->count(CPAP_RERA);

                            // Get Event Flags list
                            QList<ChannelID> flags2 = dy->getSortedMachineChannels(MT_CPAP, schema::FLAG | schema::MINOR_FLAG | schema::SPAN);

                            // Update flags counts and sums
                            for (int i=0; i < flags2.size(); i++) {
                                ChannelID code  = flags2.at(i);
                                rx2.s_count[code] += dy->count(code);
                                rx2.s_sum[code] += dy->sum(code);
                            }

                            // Update time sum
                            rx2.hours += dy->hours(MT_CPAP);

                            // Update start and end
                            //rx2.end = qMax(di.key(), rx2.end); // don't need to do this, the end won't change from what the old one was.

                            // technically only need to capture the first??
                            rx2.start = qMin(di.key(), rx2.start);
                        }
                    }

                    // Set rx records day counts
                    rx.days = rx.dates.size();
                    rx2.days = rx2.dates.size();

                    // Copy the pressure/mode/etc settings, because they haven't changed.
                    rx2.pressure = rx.pressure;
                    rx2.mode = rx.mode;
                    rx2.relief = rx.relief;
                    rx2.machine = rx.machine;

                    // Insert the newly split rx record
                    rxitems.insert(rx2.start, rx2);  // hmmm. this was previously set to the end date.. that was a silly plan.
                    fnd = true;

                    break;
                }
            }
        }


        if (fnd) continue; // already in rx list, move onto the next daylist entry

        // So in this condition, daylist isn't in rx cache, and doesn't match date range of any previous rx cache entry.


        // Need to bring in summaries for this
        day->OpenSummary();

        // Get Event flags list
        QList<ChannelID> flags3 = day->getSortedMachineChannels(MT_CPAP, schema::FLAG | schema::MINOR_FLAG | schema::SPAN);

        // Generate pressure/mode/`strings
        QString relief = day->getPressureRelief();
        QString mode = day->getCPAPMode();
        QString pressure = day->getPressureSettings();

        // Now scan the rxcache to find the most previous entry, and the right place to insert

        QMap<QDate, RXItem>::iterator lastri = rxitems.end();

        for (ri = rxitems.begin(); ri != ri_end; ++ri) {
//            RXItem & rx = ri.value();

            // break after any date newer
            if (ri.key() > date)
                break;

            // Keep this.. we need the last one.
            lastri = ri;
        }

        // lastri should no be the last entry before this date, or the end
        if (lastri != rxitems.end()) {
            RXItem & rx = lastri.value();

            // Does it match here?
            if ((rx.relief == relief) && (rx.mode == mode) && (rx.pressure == pressure) && (rx.machine == mach) ) {

                // Update AHI/RDI
                tmp = day->count(CPAP_Hypopnea) + day->count(CPAP_Obstructive) + day->count(CPAP_Apnea) + day->count(CPAP_ClearAirway);
                rx.ahi += tmp;
                rx.rdi += tmp + day->count(CPAP_RERA);

                // Update event flags
                for (int i=0; i < flags3.size(); i++) {
                    ChannelID code  = flags3.at(i);
                    rx.s_count[code] += day->count(code);
                    rx.s_sum[code] += day->sum(code);
                }

                // Update hours
                rx.hours += day->hours(MT_CPAP);

                // Add day to this RX Cache
                rx.dates[date] = day;
                rx.end = date;
                rx.days = rx.dates.size();

                fnd = true;
            }
        }

        if (!fnd) {
            // Okay, couldn't find a match, create a new rx cache record for this day.

            RXItem rx;
            rx.start = date;
            rx.end = date;
            rx.days = 1;

            // Set AHI/RDI for just this day
            tmp = day->count(CPAP_Hypopnea) + day->count(CPAP_Obstructive) + day->count(CPAP_Apnea) + day->count(CPAP_ClearAirway);
            rx.ahi = tmp;
            rx.rdi = tmp + day->count(CPAP_RERA);

            // Set counts and sums for this day
            for (int i=0; i < flags3.size(); i++) {
                ChannelID code  = flags3.at(i);
                rx.s_count[code] = day->count(code);
                rx.s_sum[code] = day->sum(code);
            }

            rx.hours = day->hours();

            // Store settings, etc..
            rx.relief = relief;
            rx.mode = mode;
            rx.pressure = pressure;
            rx.machine = mach;

            // add this day to this rx record
            rx.dates.insert(date, day);

            // And insert into rx record into the rx cache
            rxitems.insert(date, rx);
        }
    }
    // Store RX cache to disk
    saveRXChanges();

    // Now do the setup for the best worst highlighting
    QList<RXItem *> list;
    ri_end = rxitems.end();

    for (ri = rxitems.begin(); ri != ri_end; ++ri) {
        list.append(&ri.value());
        ri.value().highlight = 0;
    }

    std::sort(list.begin(), list.end(), rxAHILessThan);

    if (list.size() >= 4) {
        list[0]->highlight = 1; // best
        list[1]->highlight = 2; // best
        int ls = list.size() - 1;
        list[ls-1]->highlight = 3; // best
        list[ls]->highlight = 4;
    } else if (list.size() >= 2) {
        list[0]->highlight = 1; // best
        int ls = list.size() - 1;
        list[ls]->highlight = 4;
    } else if (list.size() > 0) {
        list[0]->highlight = 1; // best
    }

    // Close the progress bar
    progress->close();
}


// Statistics constructor is responsible for creating list of rows that will on the Statistics page
// and skeletons of column 1 text that correspond to each calculation type.
// Actual column 1 text is combination of skeleton for the row's calculation time and the text of the row.
// Also creates "machine" names for machine types.
Statistics::Statistics(QObject *parent) :
    QObject(parent)
{
    rows.push_back(StatisticsRow(tr("CPAP Statistics"), SC_HEADING, MT_CPAP));
    rows.push_back(StatisticsRow("",   SC_DAYS, MT_CPAP));
    rows.push_back(StatisticsRow("", SC_COLUMNHEADERS, MT_CPAP));
    rows.push_back(StatisticsRow(tr("CPAP Usage"),  SC_SUBHEADING, MT_CPAP));
    rows.push_back(StatisticsRow(tr("Average Hours per Night"),      SC_HOURS,     MT_CPAP));
    rows.push_back(StatisticsRow(tr("Compliance (%1 hrs/day)"),  SC_COMPLIANCE,  MT_CPAP));

    rows.push_back(StatisticsRow(tr("Therapy Efficacy"),  SC_SUBHEADING, MT_CPAP));
    rows.push_back(StatisticsRow("AHI",        SC_AHI,     MT_CPAP));
    rows.push_back(StatisticsRow("Obstructive",   SC_CPH,     MT_CPAP));
    rows.push_back(StatisticsRow("Hypopnea",   SC_CPH,     MT_CPAP));
    rows.push_back(StatisticsRow("ClearAirway",   SC_CPH,     MT_CPAP));
    rows.push_back(StatisticsRow("FlowLimit",  SC_CPH,     MT_CPAP));
    rows.push_back(StatisticsRow("RERA",       SC_CPH,     MT_CPAP));
    rows.push_back(StatisticsRow("SensAwake",       SC_CPH,     MT_CPAP));
    rows.push_back(StatisticsRow("CSR", SC_SPH, MT_CPAP));

    rows.push_back(StatisticsRow(tr("Leak Statistics"),  SC_SUBHEADING, MT_CPAP));
    rows.push_back(StatisticsRow("Leak",       SC_WAVG,    MT_CPAP));
    rows.push_back(StatisticsRow("Leak",       SC_90P,     MT_CPAP));
    rows.push_back(StatisticsRow("Leak",       SC_ABOVE,   MT_CPAP));

    rows.push_back(StatisticsRow(tr("Pressure Statistics"),  SC_SUBHEADING, MT_CPAP));
    rows.push_back(StatisticsRow("Pressure",   SC_WAVG,    MT_CPAP));
    rows.push_back(StatisticsRow("Pressure",   SC_MIN,     MT_CPAP));
    rows.push_back(StatisticsRow("Pressure",   SC_MAX,     MT_CPAP));
    rows.push_back(StatisticsRow("Pressure",   SC_90P,     MT_CPAP));
    rows.push_back(StatisticsRow("EPAP",       SC_WAVG,    MT_CPAP));
    rows.push_back(StatisticsRow("EPAP",       SC_MIN,     MT_CPAP));
    rows.push_back(StatisticsRow("EPAP",       SC_MAX,     MT_CPAP));
    rows.push_back(StatisticsRow("IPAP",       SC_WAVG,    MT_CPAP));
    rows.push_back(StatisticsRow("IPAP",       SC_90P,     MT_CPAP));
    rows.push_back(StatisticsRow("IPAP",       SC_MIN,     MT_CPAP));
    rows.push_back(StatisticsRow("IPAP",       SC_MAX,     MT_CPAP));

    rows.push_back(StatisticsRow("", SC_HEADING, MT_OXIMETER));         // Just adds some space
    rows.push_back(StatisticsRow(tr("Oximeter Statistics"), SC_HEADING, MT_OXIMETER));
    rows.push_back(StatisticsRow("",           SC_DAYS,    MT_OXIMETER));
    rows.push_back(StatisticsRow("",           SC_COLUMNHEADERS, MT_OXIMETER));

    rows.push_back(StatisticsRow(tr("Blood Oxygen Saturation"),  SC_SUBHEADING, MT_CPAP));
    rows.push_back(StatisticsRow("SPO2",       SC_WAVG,     MT_OXIMETER));
    rows.push_back(StatisticsRow("SPO2",       SC_MIN,     MT_OXIMETER));
    rows.push_back(StatisticsRow("SPO2Drop",   SC_CPH,     MT_OXIMETER));
    rows.push_back(StatisticsRow("SPO2Drop",   SC_SPH,     MT_OXIMETER));
    rows.push_back(StatisticsRow(tr("Pulse Rate"),  SC_SUBHEADING, MT_CPAP));
    rows.push_back(StatisticsRow("Pulse",      SC_WAVG,     MT_OXIMETER));
    rows.push_back(StatisticsRow("Pulse",      SC_MIN,     MT_OXIMETER));
    rows.push_back(StatisticsRow("Pulse",      SC_MAX,     MT_OXIMETER));
    rows.push_back(StatisticsRow("PulseChange",   SC_CPH,     MT_OXIMETER));

    // These are for formatting the headers for the first column
    int percentile=trunc(p_profile->general->prefCalcPercentile());                    // Pholynyk, 10Mar2016
    char perCentStr[20];
    snprintf(perCentStr, 20, "%d%% %%1", percentile);          // 
    calcnames[SC_UNDEFINED] = "";
    calcnames[SC_MEDIAN] = tr("%1 Median");
    calcnames[SC_AVG] = tr("Average %1");
    calcnames[SC_WAVG] = tr("Average %1");
    calcnames[SC_90P] = tr(perCentStr); // this gets converted to whatever the upper percentile is set to
    calcnames[SC_MIN] = tr("Min %1");
    calcnames[SC_MAX] = tr("Max %1");
    calcnames[SC_CPH] = tr("%1 Index");
    calcnames[SC_SPH] = tr("% of time in %1");
    calcnames[SC_ABOVE] = tr("% of time above %1 threshold");
    calcnames[SC_BELOW] = tr("% of time below %1 threshold");

    machinenames[MT_UNKNOWN] = STR_TR_Unknown;
    machinenames[MT_CPAP] = STR_TR_CPAP;
    machinenames[MT_OXIMETER] = STR_TR_Oximetry;
    machinenames[MT_SLEEPSTAGE] = STR_TR_SleepStage;
//        { MT_JOURNAL, STR_TR_Journal },
//        { MT_POSITION, STR_TR_Position },

}

// Get the user information block for displaying at top of page
QString Statistics::getUserInfo () {
    QString address = p_profile->user->address();
    address.replace("\n", "<br/>");

    QString userinfo = "";

    if (!p_profile->user->firstName().isEmpty()) {
        userinfo = tr("Name: %1, %2").arg(p_profile->user->lastName()).arg(p_profile->user->firstName()) + "<br/>";
        if (!p_profile->user->DOB().isNull()) {
            userinfo += tr("DOB: %1").arg(p_profile->user->DOB().toString(MedDateFormat)) + "<br/>";
        }
        if (!p_profile->user->phone().isEmpty()) {
            userinfo += tr("Phone: %1").arg(p_profile->user->phone()) + "<br/>";
        }
        if (!p_profile->user->email().isEmpty()) {
            userinfo += tr("Email: %1").arg(p_profile->user->email()) + "<br/><br/>";
        }
        if (!p_profile->user->address().isEmpty()) {
            userinfo +=  tr("Address:")+"<br/>"+address;
        }
    }

    while (userinfo.length() > 0 && userinfo.endsWith("<br/>"))  // Strip trailing newlines
        userinfo = userinfo.mid(0, userinfo.length()-5);

    return userinfo;
}

const QString table_width = "width=99%";

// Create the page header in HTML.  Includes everything from <head> through <body>
QString Statistics::generateHeader(bool onScreen)
{
    QString html = QString("<html><head>")+
            "<style type='text/css'>";
    if (onScreen) {
        html += "p,a,td,body { font-family: '" + QApplication::font().family() + "'; }"
                "p,a,td,body { font-size: " + QString::number(QApplication::font().pointSize() + 2) + "px; }";
    } else {
        html += "p,a,td,body { font-family: 'Helvetica'; }";
//                "p,a,td,body { font-size: 10px; }";
    }
//    qDebug() << "generateHeader font" << html;
    html += "table.curved {"  // Borders not supported without webkit
//            "border: 1px solid gray;"
//            "border-radius:10px;"
//            "-moz-border-radius:10px;"
//            "-webkit-border-radius:10px;"
//            "page-break-after:auto;"
//            "-fs-table-paginate: paginate;"
            "}"

            "tr.datarow:nth-child(even) {"
            "background-color: #f8f8f8;"
            "}"
            "table { page-break-after:auto; -fs-table-paginate: paginate; }"
            "tr    { page-break-inside:avoid; page-break-after:auto }"
            "td    { page-break-inside:avoid; page-break-after:auto }"
            "thead { display:table-header-group; }"
            "tfoot { display:table-footer-group; }"

            "</style>"

            "<link rel='stylesheet' type='text/css' href='qrc:/docs/tooltips.css' />"

            "<script type='text/javascript'>"
            "function ChangeColor(tableRow, highLight)"
            "{ tableRow.style.backgroundColor = highLight; }"
            "function Go(url) { throw(url); }"
            "</script>"

    "</head>"

    "<body leftmargin=0 topmargin=5 rightmargin=0>";

    QPixmap logoPixmap(":/icons/logo-lg.png");

        html += "<div align=center><table class=curved width='100%'>"
            "<tr>"
                "<td align='left' valign='middle'>" + getUserInfo() + "</td>"
                "<td align='right' valign='middle' width='200'>"
                    "<font size='+2'>" + STR_TR_OSCAR + "&nbsp;&nbsp;&nbsp;</font><br/>"
                    "<font size='+1'>" + QObject::tr("Usage Statistics") + "&nbsp;&nbsp;&nbsp;</font>"
                "</td>"
                "<td align='right' valign='middle' width='110'>" + resizeHTMLPixmap(logoPixmap,80,80)+"&nbsp;&nbsp;&nbsp;<br/>"
                "</td>"
            "</tr>"
            "</table>"
            "</div>";

    return html;
}

// HTML for page footer
QString Statistics::generateFooter(bool showinfo)
{
    QString html;

    if (showinfo) {
        html += "<hr><div align=center><font size='-1'><i>";
        html += tr("This report was generated by OSCAR v%1").arg(ShortVersionString) + "<br/>"
           +tr("OSCAR is free open-source CPAP report software");
        html += "</i></font></div>";
    }

    html += "</body></html>";
    return html;
}


// Calculate AHI for a period as total # of events / total hours used
// Add RERA if calculating RDI instead of just AHI
EventDataType calcAHI(QDate start, QDate end)
{
    EventDataType val = (p_profile->calcCount(CPAP_Obstructive, MT_CPAP, start, end)
                         + p_profile->calcCount(CPAP_Hypopnea, MT_CPAP, start, end)
                         + p_profile->calcCount(CPAP_ClearAirway, MT_CPAP, start, end)
                         + p_profile->calcCount(CPAP_Apnea, MT_CPAP, start, end));

    if (p_profile->general->calculateRDI()) {
        val += p_profile->calcCount(CPAP_RERA, MT_CPAP, start, end);
    }

    EventDataType hours = p_profile->calcHours(MT_CPAP, start, end);

    if (hours > 0) {
        val /= hours;
    } else {
        val = 0;
    }

    return val;
}

// Calculate flow limits per hour
EventDataType calcFL(QDate start, QDate end)
{
    EventDataType val = (p_profile->calcCount(CPAP_FlowLimit, MT_CPAP, start, end));
    EventDataType hours = p_profile->calcHours(MT_CPAP, start, end);

    if (hours > 0) {
        val /= hours;
    } else {
        val = 0;
    }

    return val;
}

// Calculate ...(what are these?)
EventDataType calcSA(QDate start, QDate end)
{
    EventDataType val = (p_profile->calcCount(CPAP_SensAwake, MT_CPAP, start, end));
    EventDataType hours = p_profile->calcHours(MT_CPAP, start, end);

    if (hours > 0) {
        val /= hours;
    } else {
        val = 0;
    }

    return val;
}

// Structure for recording Prescription Changes (now called Machine Settings Changes)
struct RXChange {
    RXChange() { highlight = 0; machine = nullptr; }
    RXChange(const RXChange &copy) {
        first = copy.first;
        last = copy.last;
        days = copy.days;
        ahi = copy.ahi;
        fl = copy.fl;
        mode = copy.mode;
        min = copy.min;
        max = copy.max;
        ps = copy.ps;
        pshi = copy.pshi;
        maxipap = copy.maxipap;
        machine = copy.machine;
        per1 = copy.per1;
        per2 = copy.per2;
        highlight = copy.highlight;
        weighted = copy.weighted;
        pressure_string = copy.pressure_string;
        pr_relief_string = copy.pr_relief_string;
    }
    QDate first;
    QDate last;
    int days;
    EventDataType ahi;
    EventDataType fl;
    CPAPMode mode;
    QString pressure_string;
    QString pr_relief_string;
    EventDataType min;
    EventDataType max;
    EventDataType ps;
    EventDataType pshi;
    EventDataType maxipap;
    EventDataType per1;
    EventDataType per2;
    EventDataType weighted;
    Machine *machine;
    short highlight;
};

struct UsageData {
    UsageData() { ahi = 0; hours = 0; }
    UsageData(QDate d, EventDataType v, EventDataType h) { date = d; ahi = v; hours = h; }
    UsageData(const UsageData &copy) { date = copy.date; ahi = copy.ahi; hours = copy.hours; }
    QDate date;
    EventDataType ahi;
    EventDataType hours;
};

bool operator <(const UsageData &c1, const UsageData &c2)
{
    if (c1.ahi < c2.ahi) {
        return true;
    }

    if ((c1.ahi == c2.ahi) && (c1.date > c2.date)) { return true; }

    return false;
}

struct Period {
    Period() {
    }
    Period(QDate start, QDate end, QString header) {
        this->start = start;
        this->end = end;
        this->header = header;
    }
    Period(const Period & copy) {
        start=copy.start;
        end=copy.end;
        header=copy.header;
    }
    QDate start;
    QDate end;
    QString header;
};

const QString heading_color="#ffffff";
const QString subheading_color="#e0e0e0";
//const int rxthresh = 5;

QString Statistics::GenerateMachineList()
{
    QList<Machine *> cpap_machines = p_profile->GetMachines(MT_CPAP);
    QList<Machine *> oximeters = p_profile->GetMachines(MT_OXIMETER);
    QList<Machine *> mach;

    mach.append(cpap_machines);
    mach.append(oximeters);

    QString html;
    if (mach.size() > 0) {
        html += "<div align=center><br/>";

        html += QString("<table class=curved style=\"page-break-before:auto;\" "+table_width+">");

        html += "<thead>";
        html += "<tr bgcolor='"+heading_color+"'><th colspan=7 align=center><font size=+2>" + tr("Machine Information") + "</font></th></tr>";

        html += QString("<tr><td><b>%1</b></td><td><b>%2</b></td><td><b>%3</b></td><td><b>%4</b></td><td><b>%5</b></td></tr>")
                .arg(STR_TR_Brand)
                .arg(STR_TR_Model)
                .arg(STR_TR_Serial)
                .arg(tr("First Use"))
                .arg(tr("Last Use"));

        html += "</thead>";

        Machine *m;

        for (int i = 0; i < mach.size(); i++) {
            m = mach.at(i);

            if (m->type() == MT_JOURNAL) { continue; }

            QDate d1 = m->FirstDay();
            QDate d2 = m->LastDay();
            QString mn = m->modelnumber();
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
                    .arg(m->brand())
                    .arg(m->model() +
                         (mn.isEmpty() ? "" : QString(" (") + mn + QString(")")))
                    .arg(m->serial())
                    .arg(d1.toString(MedDateFormat))
                    .arg(d2.toString(MedDateFormat));
//            .arg(d1.toString(Qt::SystemLocaleShortDate))
//            .arg(d2.toString(Qt::SystemLocaleShortDate));

        }

        html += "</table>";
        html += "</div>";
    }
    return html;
}
QString Statistics::GenerateRXChanges()
{
    // do the actual data sorting...
    updateRXChanges();

    QString ahitxt;

    bool rdi = p_profile->general->calculateRDI();
    if (rdi) {
        ahitxt = STR_TR_RDI;
    } else {
        ahitxt = STR_TR_AHI;
    }


    QString html = "<div align=center><br/>";
    html += QString("<table class=curved style=\"page-break-before:always;\" "+table_width+" width=100%>");
    html += "<thead>";
    html += "<tr bgcolor='"+heading_color+"'><th colspan=9 align=center><font size=+2>" + tr("Changes to Machine Settings") + "</font></th></tr>";

//    QString extratxt;

//    QString tooltip;
    QStringList hdrlist;
    hdrlist.push_back(STR_TR_First);
    hdrlist.push_back(STR_TR_Last);
    hdrlist.push_back(tr("Days"));
    hdrlist.push_back(ahitxt);
    hdrlist.push_back(STR_TR_FL);
    hdrlist.push_back(STR_TR_Machine);
    hdrlist.push_back(tr("Pressure Relief"));
    hdrlist.push_back(STR_TR_Mode);
    hdrlist.push_back(tr("Pressure Settings"));

    html+="<tr>\n";
    for (int i=0; i < hdrlist.size(); ++i) {
        html+=QString(" <th align=left><b>%1</b></th>\n").arg(hdrlist.at(i));
    }
    html+="</tr>\n";
    html += "</thead>";
//    html += "<tfoot>";
//    html += "<tr><td colspan=10 align=center>";
//    html += QString("<i>") +
//            tr("Efficacy highlighting ignores prescription settings with less than %1 days of recorded data.").
//            arg(rxthresh) + QString("</i><br/>");
//    html += "</td></tr>";
//    html += "</tfoot>";

    QMapIterator<QDate, RXItem> it(rxitems);
    it.toBack();
    while (it.hasPrevious()) {
        it.previous();
        const RXItem & rx = it.value();

        QString color;

        if (rx.highlight == 1) {
            color = "#c0ffc0";
        } else if (rx.highlight == 2) {
            color = "#e0ffe0";
        } else if (rx.highlight == 3) {
            color = "#ffe0e0";
        } else if (rx.highlight == 4) {
            color = "#ffc0c0";
        } else { color = ""; }

        QString datarowclass;
        if (rx.highlight == 0) datarowclass="class=datarow";
        html += QString("<tr %4 bgcolor='%1' onmouseover='ChangeColor(this, \"#dddddd\");' onmouseout='ChangeColor(this, \"%1\");' onclick='alert(\"overview=%2,%3\");'>")
                .arg(color)
                .arg(rx.start.toString(Qt::ISODate))
                .arg(rx.end.toString(Qt::ISODate))
                .arg(datarowclass);

        double ahi = rdi ? (double(rx.rdi) / rx.hours) : (double(rx.ahi) /rx.hours);
        double fli = double(rx.count(CPAP_FlowLimit)) / rx. hours;

        html += QString("<td>%1</td>").arg(rx.start.toString(MedDateFormat))+
                QString("<td>%1</td>").arg(rx.end.toString(MedDateFormat))+
                QString("<td>%1</td>").arg(rx.days)+
                QString("<td>%1</td>").arg(ahi, 0, 'f', 2)+
                QString("<td>%1</td>").arg(fli, 0, 'f', 2)+
                QString("<td>%1 (%2)</td>").arg(rx.machine->model()).arg(rx.machine->modelnumber())+
                QString("<td>%1</td>").arg(formatRelief(rx.relief))+
                QString("<td>%1</td>").arg(rx.mode)+
                QString("<td>%1</td>").arg(rx.pressure)+
                "</tr>";
    }
    html+="</table></div>";

    return html;
}

// Report no data available
QString Statistics::htmlNoData()
{
            QString html = "<div align=center>";
            html += QString( "<p><font size=\"+3\"><br />" + tr("No data found?!?") + "</font></p>"+
                    "<p><img src='qrc:/icons/logo-lm.png' width=\"100\" height=\"100\"></p>"
                    "<p><i>"+tr("Oscar has no data to report :(")+"</i></p>");
            return html;
}

// Get RDI or AHI text depending on user preferences
QString Statistics::getRDIorAHIText() {
    if (p_profile->general->calculateRDI()) {
        return STR_TR_RDI;
    }
    return STR_TR_AHI;
}

// Create the HTML for CPAP and Oximetry usage
QString Statistics::GenerateCPAPUsage()
{
    QList<Machine *> cpap_machines = p_profile->GetMachines(MT_CPAP);
    QList<Machine *> oximeters = p_profile->GetMachines(MT_OXIMETER);
    QList<Machine *> mach;

    mach.append(cpap_machines);
    mach.append(oximeters);

    // Go through all CPAP and Oximeter machines and see if any data is present
    bool havedata = false;
    for (int i=0; i < mach.size(); ++i) {
        int daysize = mach[i]->day.size();
        if (daysize > 0) {
            havedata = true;
            break;
        }
    }

    QString html = "";

    // If we don't have any data, return HTML that says that and we are done
    if (!havedata) {
        return "";
    }

    // Find first and last days with valid CPAP data
    QDate lastcpap = p_profile->LastGoodDay(MT_CPAP);
    QDate firstcpap = p_profile->FirstGoodDay(MT_CPAP);

    // Get dates for standard report (last week, month, 6 months, year)
    QDate cpapweek = lastcpap.addDays(-6);
    QDate cpapmonth = lastcpap.addDays(-29);
    QDate cpap6month = lastcpap.addMonths(-6);
    QDate cpapyear = lastcpap.addMonths(-12);

    // but not before the first available date of course
    if (cpapweek   < firstcpap) { cpapweek   = firstcpap; }
    if (cpapmonth  < firstcpap) { cpapmonth  = firstcpap; }
    if (cpap6month < firstcpap) { cpap6month = firstcpap; }
    if (cpapyear   < firstcpap) { cpapyear   = firstcpap; }


    QString ahitxt = getRDIorAHIText();

    // Prepare top of table
    html += "<div align=center>";
    html += "<table class=curved width="+table_width+">";

    // Compute number of monthly periods for a monthly rather than standard time distribution
    int number_periods = 0;
    if (p_profile->general->statReportMode() == STAT_MODE_MONTHLY) {
        int firstMonth = firstcpap.month();
        int lastMonth = lastcpap.month();
        int years = lastcpap.year() - firstcpap.year();
        lastMonth += (12 * years); // handle time extending to next year
        number_periods = lastMonth - firstMonth + 1;

        if (number_periods < 1) {
            qDebug() << "*** Begin" << firstcpap << "beginMonth" << firstMonth << "lastMonth" << lastMonth << "periods" << number_periods;
            number_periods = 1;
        }
        qDebug() << "Number of months for stats (trim to 12 max)" << number_periods;
        // But not more than one year
        if (number_periods > 12) {
            number_periods = 12;
        }
    }

    QDate last = lastcpap, first = lastcpap;

    QList<Period> periods;

    bool skipsection = false;;
    // Loop through all rows of the Statistics report
    for (QList<StatisticsRow>::iterator i = rows.begin(); i != rows.end(); ++i) {
        StatisticsRow &row = (*i);
        QString name;

        if (row.calc == SC_HEADING) {  // All sections begin with a heading
            last = p_profile->LastGoodDay(row.type);
            first = p_profile->FirstGoodDay(row.type);

            // Clear the periods (columns)
            periods.clear();
            if (p_profile->general->statReportMode() == STAT_MODE_STANDARD) {
                periods.push_back(Period(last,last,tr("Most Recent")));
                periods.push_back(Period(qMax(last.addDays(-6), first), last, tr("Last Week")));
                periods.push_back(Period(qMax(last.addDays(-29),first), last, tr("Last 30 Days")));
                periods.push_back(Period(qMax(last.addMonths(-6), first), last, tr("Last 6 Months")));
                periods.push_back(Period(qMax(last.addMonths(-12), first), last, tr("Last Year")));
            } else {            // STAT_MODE_MONTHLY or STAT_MODE_RANGE
                QDate l=last,s=last;

                periods.push_back(Period(last,last,tr("Last Session")));

                //bool done=false;
                int j=0;
                do {
                    s=QDate(l.year(), l.month(), 1);
                    if (s < first) {
                        //done = true;
                        s = first;
                    }
                    if (p_profile->countDays(row.type, s, l) > 0) {
                        periods.push_back(Period(s, l, s.toString("MMMM")));
                        j++;
                    }
                    l = s.addDays(-1);
                } while ((l > first) && (j < number_periods));

                for (; j < number_periods; ++j) {
                    periods.push_back(Period(last,last, ""));
                }
            }

            int days = p_profile->countDays(row.type, first, last);
            skipsection = (days == 0);
            if (days > 0) {
                html+=QString("<tr bgcolor='%1'><th colspan=%2 align=center><font size=+2>%3</font></th></tr>\n").
                        arg(heading_color).arg(periods.size()+1).arg(row.src);
            }
            continue;
        }

        // Bypass this entire section if no data is present
        if (skipsection) continue;

        if (row.calc == SC_AHI) {
            name = ahitxt;
        } else if (row.calc == SC_HOURS) {
            name = row.src;
        } else if (row.calc == SC_COMPLIANCE) {
            name = QString(row.src).arg(p_profile->cpap->m_complianceHours);
        } else if (row.calc == SC_COLUMNHEADERS) {
            html += QString("<tr><td><b>%1</b></td>\n").arg(tr("Details"));
            for (int j=0; j < periods.size(); j++) {
                html += QString("<td onmouseover='ChangeColor(this, \"#eeeeee\");' onmouseout='ChangeColor(this, \"#ffffff\");' onclick='alert(\"overview=%1,%2\");'><b>%3</b></td>\n").arg(periods.at(j).start.toString(Qt::ISODate)).arg(periods.at(j).end.toString(Qt::ISODate)).arg(periods.at(j).header);
            }
            html += "</tr>\n";
            continue;
        } else if (row.calc == SC_DAYS) {
            QDate first=p_profile->FirstGoodDay(row.type);
            QDate last=p_profile->LastGoodDay(row.type);
            QString & machine = machinenames[row.type];
            int value=p_profile->countDays(row.type, first, last);

            if (value == 0) {
                html+=QString("<tr><td colspan=%1 align=center>%2</td></tr>\n").arg(periods.size()+1).
                        arg(tr("No %1 data available.").arg(machine));
            } else if (value == 1) {
                html+=QString("<tr><td colspan=%1 align=center>%2</td></tr>\n").arg(periods.size()+1).
                        arg(tr("%1 day of %2 Data on %3")
                            .arg(value)
                            .arg(machine)
                            .arg(last.toString(MedDateFormat)));
            } else {
                html+=QString("<tr><td colspan=%1 align=center>%2</td></tr>\n").arg(periods.size()+1).
                        arg(tr("%1 days of %2 Data, between %3 and %4")
                            .arg(value)
                            .arg(machine)
                            .arg(first.toString(MedDateFormat))
                            .arg(last.toString(MedDateFormat)));
            }
            continue;
        } else if (row.calc == SC_SUBHEADING) {  // subheading..
            html+=QString("<tr bgcolor='%1'><td colspan=%2 align=center><b>%3</b></td></tr>\n").
                    arg(subheading_color).arg(periods.size()+1).arg(row.src);
            continue;
        } else if (row.calc == SC_UNDEFINED) {
            continue;
        } else {
            ChannelID id = schema::channel[row.src].id();
            if ((id == NoChannel) || (!p_profile->channelAvailable(id))) {
                continue;
            }
            name = calcnames[row.calc].arg(schema::channel[id].fullname());
        }
        QString line;
        line += QString("<tr class=datarow><td width=24%>%1</td>").arg(name);
        int np = periods.size();
        int width;
        for (int j=0; j < np; j++) {
            if (p_profile->general->statReportMode() == STAT_MODE_MONTHLY) {
                width = j < np-1 ? 6 : 100 - (24 + 6*(np-1));
            } else {
                width = 76/np;
            }

            line += QString("<td width=%1%>").arg(width);
            if (!periods.at(j).header.isEmpty()) {
                line += row.value(periods.at(j).start, periods.at(j).end);
            } else {
                line +="&nbsp;";
            }
            line += "</td>";
        }
        html += line;
        html += "</tr>\n";
    }

    html += "</table>";
    html += "</div>";

    return html;
}

// Create the HTML that will be the Statistics page.
QString Statistics::GenerateHTML()
{
    htmlReportHeader = generateHeader(true);
    htmlReportHeaderPrint = generateHeader(false);
    htmlReportFooter = generateFooter(true);

    htmlUsage = GenerateCPAPUsage();

    if (htmlUsage == "") {
        return htmlReportHeader + htmlNoData() + htmlReportFooter;
    }

    htmlMachineSettings = GenerateRXChanges();
    htmlMachines = GenerateMachineList();

    QString htmlScript = "<script type='text/javascript' language='javascript' src='qrc:/docs/script.js'></script>";

    return htmlReportHeader + htmlUsage + htmlMachineSettings + htmlMachines + htmlScript + htmlReportFooter;
}

// Print the Statistics page on printer
void Statistics::printReport(QWidget * parent) {

    QPrinter printer(QPrinter::ScreenResolution); // ScreenResolution required for graphics sizing

#ifdef Q_OS_LINUX
    printer.setPrinterName("Print to File (PDF)");
    printer.setOutputFormat(QPrinter::PdfFormat);
    QString name = "Statistics";
    QString datestr = QDate::currentDate().toString(Qt::ISODate);

    QString filename = p_pref->Get("{home}/") + name + "_" + p_profile->user->userName() + "_" + datestr + ".pdf";

    printer.setOutputFileName(filename);
#endif

    printer.setPrintRange(QPrinter::AllPages);
    printer.setOrientation(QPrinter::Portrait);
    printer.setFullPage(false);     // Print only on printable area of page and not in non-printable margins
    printer.setNumCopies(1);
    printer.setPageMargins(10, 10, 10, 10, QPrinter::Millimeter);

    // Show print dialog to user and allow them to change settings as desired
    QPrintDialog pdlg(&printer, parent);

    if (pdlg.exec() == QPrintDialog::Accepted) {

        QTextDocument doc;
        QSizeF printArea = printer.pageRect().size();
        doc.setPageSize(printArea);  // Set document to print area, removing default 2cm margins
    qDebug() << "print area" << printArea;

//        QFont font = doc.defaultFont();
        QFont font = QFont("Helvetica");
        font.setPointSize(10 * (printArea.width()/1200.0)); // Scale the font
        doc.setDefaultFont(font);
    qDebug() << "Printer font set to" << font << "and printer default font is now" << doc.defaultFont();

        doc.setHtml(htmlReportHeaderPrint + htmlUsage + htmlMachineSettings + htmlMachines + htmlReportFooter);

        doc.print(&printer);
    }
}

QString Statistics::UpdateRecordsBox()
{
    QString html = "<html><head><style type='text/css'>"
                     "p,a,td,body { font-family: '" + QApplication::font().family() + "'; }"
                     "p,a,td,body { font-size: " + QString::number(QApplication::font().pointSize() + 2) + "px; }"
                     "a:link,a:visited { color: inherit; text-decoration: none; }" //font-weight: normal;
                     "a:hover { background-color: inherit; color: white; text-decoration:none; font-weight: bold; }"
                     "</style></head><body>";

    Machine * cpap = p_profile->GetMachine(MT_CPAP);
    if (cpap) {
        QDate first = p_profile->FirstGoodDay(MT_CPAP);
        QDate last = p_profile->LastGoodDay(MT_CPAP);

        /////////////////////////////////////////////////////////////////////////////////////
        /// Compliance and usage information
        /////////////////////////////////////////////////////////////////////////////////////
        int totaldays = p_profile->countDays(MT_CPAP, first, last);
        int compliant = p_profile->countCompliantDays(MT_CPAP, first, last);

        float comperc = (100.0 / float(totaldays)) * float(compliant);

        html += "<b>"+tr("CPAP Usage")+"</b><br/>";
        html += tr("Days Used: %1").arg(totaldays) + "<br/>";
        html += tr("Low Use Days: %1").arg(totaldays - compliant) + "<br/>";
        html += tr("Compliance: %1%").arg(comperc, 0, 'f', 1)  + "<br/>";

        /////////////////////////////////////////////////////////////////////////////////////
        /// AHI Records
        /////////////////////////////////////////////////////////////////////////////////////

        if (p_profile->session->preloadSummaries()) {
            const int show_records = 5;
            QMultiMap<float, QDate>::iterator it;
            QMultiMap<float, QDate>::iterator it_end;

            QMultiMap<float, QDate> ahilist;
            int baddays = 0;

            for (QDate date = first; date <= last; date = date.addDays(1)) {
                Day * day = p_profile->GetGoodDay(date, MT_CPAP);
                if (!day) continue;

                float ahi = day->calcAHI();
                if (ahi >= 5) {
                    baddays++;
                }
                ahilist.insert(ahi, date);
            }
            html += tr("Days AHI of 5 or greater: %1").arg(baddays) + "<br/><br/>";


            if (ahilist.size() > (show_records * 2)) {
                it = ahilist.begin();
                it_end = ahilist.end();

                html += "<b>"+tr("Best AHI")+"</b><br/>";

                for (int i=0; (i<show_records) && (it != it_end); ++i, ++it) {
                    html += QString("<a href='daily=%1'>").arg(it.value().toString(Qt::ISODate))
                            +tr("Date: %1 AHI: %2").arg(it.value().toString(Qt::SystemLocaleShortDate)).arg(it.key(), 0, 'f', 2) + "</a><br/>";

                }

                html += "<br/>";

                html += "<b>"+tr("Worst AHI")+"</b><br/>";

                it = ahilist.end() - 1;
                it_end = ahilist.begin();
                for (int i=0; (i<show_records) && (it != it_end); ++i, --it) {
                    html += QString("<a href='daily=%1'>").arg(it.value().toString(Qt::ISODate))
                        +tr("Date: %1 AHI: %2").arg(it.value().toString(Qt::SystemLocaleShortDate)).arg(it.key(), 0, 'f', 2) + "</a><br/>";

                }

                html += "<br/>";
            }

            /////////////////////////////////////////////////////////////////////////////////////
            /// Flow Limitation Records
            /////////////////////////////////////////////////////////////////////////////////////

            ahilist.clear();
            for (QDate date = first; date <= last; date = date.addDays(1)) {
                Day * day = p_profile->GetGoodDay(date, MT_CPAP);
                if (!day) continue;

                float val = 0;
                if (day->channelHasData(CPAP_FlowLimit)) {
                    val = day->calcIdx(CPAP_FlowLimit);
                } else if (day->channelHasData(CPAP_FLG)) {
                    // Use 90th percentile
                    val = day->calcPercentile(CPAP_FLG);
                }
                ahilist.insert(val, date);
            }

            int cnt = 0;
            if (ahilist.size() > (show_records * 2)) {
                it = ahilist.begin();
                it_end = ahilist.end();

                html += "<b>"+tr("Best Flow Limitation")+"</b><br/>";

                for (int i=0; (i<show_records) && (it != it_end); ++i, ++it) {
                    html += QString("<a href='daily=%1'>").arg(it.value().toString(Qt::ISODate))
                        +tr("Date: %1 FL: %2").arg(it.value().toString(Qt::SystemLocaleShortDate)).arg(it.key(), 0, 'f', 2) + "</a><br/>";

                }

                html += "<br/>";

                html += "<b>"+tr("Worst Flow Limtation")+"</b><br/>";

                it = ahilist.end() - 1;
                it_end = ahilist.begin();
                for (int i=0; (i<show_records) && (it != it_end); ++i, --it) {
                    if (it.key() > 0) {
                        html += QString("<a href='daily=%1'>").arg(it.value().toString(Qt::ISODate))
                            +tr("Date: %1 FL: %2").arg(it.value().toString(Qt::SystemLocaleShortDate)).arg(it.key(), 0, 'f', 2) + "</a><br/>";
                        cnt++;
                    }
                }
                if (cnt == 0) {
                    html+= "<i>"+tr("No Flow Limitation on record")+"</i><br/>";
                }

                html += "<br/>";
            }

            /////////////////////////////////////////////////////////////////////////////////////
            /// Large Leak Records
            /////////////////////////////////////////////////////////////////////////////////////

            ahilist.clear();
            for (QDate date = first; date <= last; date = date.addDays(1)) {
                Day * day = p_profile->GetGoodDay(date, MT_CPAP);
                if (!day) continue;

                float leak = day->calcPON(CPAP_LargeLeak);
                ahilist.insert(leak, date);
            }

            cnt = 0;
            if (ahilist.size() > (show_records * 2)) {
                html += "<b>"+tr("Worst Large Leaks")+"</b><br/>";

                it = ahilist.end() - 1;
                it_end = ahilist.begin();

                for (int i=0; (i<show_records) && (it != it_end); ++i, --it) {
                    if (it.key() > 0) {
                        html += QString("<a href='daily=%1'>").arg(it.value().toString(Qt::ISODate))
                            +tr("Date: %1 Leak: %2%").arg(it.value().toString(Qt::SystemLocaleShortDate)).arg(it.key(), 0, 'f', 2) + "</a><br/>";
                        cnt++;
                    }

                }
                if (cnt == 0) {
                    html+= "<i>"+tr("No Large Leaks on record")+"</i><br/>";
                }

                html += "<br/>";
            }


            /////////////////////////////////////////////////////////////////////////////////////
            /// ÇSR Records
            /////////////////////////////////////////////////////////////////////////////////////

            cnt = 0;
            if (p_profile->hasChannel(CPAP_CSR)) {
                ahilist.clear();
                for (QDate date = first; date <= last; date = date.addDays(1)) {
                    Day * day = p_profile->GetGoodDay(date, MT_CPAP);
                    if (!day) continue;

                    float leak = day->calcPON(CPAP_CSR);
                    ahilist.insert(leak, date);
                }

                if (ahilist.size() > (show_records * 2)) {
                    html += "<b>"+tr("Worst CSR")+"</b><br/>";

                    it = ahilist.end() - 1;
                    it_end = ahilist.begin();
                    for (int i=0; (i<show_records) && (it != it_end); ++i, --it) {

                        if (it.key() > 0) {
                            html += QString("<a href='daily=%1'>").arg(it.value().toString(Qt::ISODate))
                                +tr("Date: %1 CSR: %2%").arg(it.value().toString(Qt::SystemLocaleShortDate)).arg(it.key(), 0, 'f', 2) + "</a><br/>";
                            cnt++;
                        }
                    }
                    if (cnt == 0) {
                        html+= "<i>"+tr("No CSR on record")+"</i><br/>";
                    }

                    html += "<br/>";
                }
            }
            if (p_profile->hasChannel(CPAP_PB)) {
                ahilist.clear();
                for (QDate date = first; date <= last; date = date.addDays(1)) {
                    Day * day = p_profile->GetGoodDay(date, MT_CPAP);
                    if (!day) continue;

                    float leak = day->calcPON(CPAP_PB);
                    ahilist.insert(leak, date);
                }

                if (ahilist.size() > (show_records * 2)) {
                    html += "<b>"+tr("Worst PB")+"</b><br/>";

                    it = ahilist.end() - 1;
                    it_end = ahilist.begin();
                    for (int i=0; (i < show_records) && (it != it_end); ++i, --it) {

                        if (it.key() > 0) {
                            html += QString("<a href='daily=%1'>").arg(it.value().toString(Qt::ISODate))
                                +tr("Date: %1 PB: %2%").arg(it.value().toString(Qt::SystemLocaleShortDate)).arg(it.key(), 0, 'f', 2) + "</a><br/>";
                            cnt++;
                        }
                    }
                    if (cnt == 0) {
                        html+= "<i>"+tr("No PB on record")+"</i><br/>";
                    }

                    html += "<br/>";
                }
            }

        } else {
            html += "<br/><b>"+tr("Want more information?")+"</b><br/>";
            html += "<i>"+tr("OSCAR needs all summary data loaded to calculate best/worst data for individual days.")+"</i><br/><br/>";
            html += "<i>"+tr("Please enable Pre-Load Summaries checkbox in preferences to make sure this data is available.")+"</i><br/><br/>";
        }


        /////////////////////////////////////////////////////////////////////////////////////
        /// Sort the RX list to get best and worst settings.
        /////////////////////////////////////////////////////////////////////////////////////
        QList<RXItem *> list;
        QMap<QDate, RXItem>::iterator ri_end = rxitems.end();
        QMap<QDate, RXItem>::iterator ri;

        for (ri = rxitems.begin(); ri != ri_end; ++ri) {
            list.append(&ri.value());
            ri.value().highlight = 0;
        }

        std::sort(list.begin(), list.end(), rxAHILessThan);


        if (list.size() >= 2) {
            html += "<b>"+tr("Best RX Setting")+"</b><br/>";
            const RXItem & rxbest = *list.at(0);
            html += QString("<a href='overview=%1,%2'>").arg(rxbest.start.toString(Qt::ISODate)).arg(rxbest.end.toString(Qt::ISODate)) +
                tr("Date: %1 - %2").arg(rxbest.start.toString(Qt::SystemLocaleShortDate)).arg(rxbest.end.toString(Qt::SystemLocaleShortDate)) + "</a><br/>";
            html += QString("%1").arg(rxbest.machine->model()) + "<br/>";
            html += QString("Serial: %1").arg(rxbest.machine->serial()) + "<br/>";
            html += tr("AHI: %1").arg(double(rxbest.ahi) / rxbest.hours, 0, 'f', 2) + "<br/>";
            html += tr("Total Hours: %1").arg(rxbest.hours, 0, 'f', 2) + "<br/>";
            html += QString("%1").arg(rxbest.pressure) + "<br/>";
            html += QString("%1").arg(formatRelief(rxbest.relief)) + "<br/>";
            html += "<br/>";

            html += "<b>"+tr("Worst RX Setting")+"</b><br/>";
            const RXItem & rxworst = *list.at(list.size() -1);
            html += QString("<a href='overview=%1,%2'>").arg(rxworst.start.toString(Qt::ISODate)).arg(rxworst.end.toString(Qt::ISODate)) +
                    tr("Date: %1 - %2").arg(rxworst.start.toString(Qt::SystemLocaleShortDate)).arg(rxworst.end.toString(Qt::SystemLocaleShortDate)) + "</a><br/>";
            html += QString("%1").arg(rxworst.machine->model()) + "<br/>";
            html += QString("Serial: %1").arg(rxworst.machine->serial()) + "<br/>";
            html += tr("AHI: %1").arg(double(rxworst.ahi) / rxworst.hours, 0, 'f', 2) + "<br/>";
            html += tr("Total Hours: %1").arg(rxworst.hours, 0, 'f', 2) + "<br/>";

            html += QString("%1").arg(rxworst.pressure) + "<br/>";
            html += QString("%1").arg(formatRelief(rxworst.relief)) + "<br/>";
        }
    }



    html += "</body></html>";

    return html;
}



QString StatisticsRow::value(QDate start, QDate end)
{
    const int decimals=2;
    QString value;
    float days = p_profile->countDays(type, start, end);

    float percentile=p_profile->general->prefCalcPercentile()/100.0;    // Pholynyk, 10Mar2016
    EventDataType percent = percentile;                                 // was 0.90F

    // Handle special data sources first
    if (calc == SC_AHI) {
        value = QString("%1").arg(calcAHI(start, end), 0, 'f', decimals);
    } else if (calc == SC_HOURS) {
        value = QString("%1").arg(formatTime(p_profile->calcHours(type, start, end) / days));
    } else if (calc == SC_COMPLIANCE) {
        float c = p_profile->countCompliantDays(type, start, end);
//        float p = (100.0 / days) * c;
        float realDays = qAbs(start.daysTo(end)) + 1;
        float p = (100.0 / realDays) * c;
        value = QString("%1%").arg(p, 0, 'f', 0);
    } else if (calc == SC_DAYS) {
        value = QString("%1").arg(p_profile->countDays(type, start, end));
    } else if ((calc == SC_COLUMNHEADERS) || (calc == SC_SUBHEADING) || (calc == SC_UNDEFINED))  {
    } else {
        //
        ChannelID code=channel();

        EventDataType val = 0;
        QString fmt = "%1";
        if (code != NoChannel) {
            switch(calc) {
            case SC_AVG:
                val = p_profile->calcAvg(code, type, start, end);
                break;
            case SC_WAVG:
                val = p_profile->calcWavg(code, type, start, end);
                break;
            case SC_MEDIAN:
                val = p_profile->calcPercentile(code, 0.5F, type, start, end);
                break;
            case SC_90P:
                val = p_profile->calcPercentile(code, percent, type, start, end);
                break;
            case SC_MIN:
                val = p_profile->calcMin(code, type, start, end);
                break;
            case SC_MAX:
                val = p_profile->calcMax(code, type, start, end);
                break;
            case SC_CPH:
                val = p_profile->calcCount(code, type, start, end) / p_profile->calcHours(type, start, end);
                break;
            case SC_SPH:
                fmt += "%";
                val = 100.0 / p_profile->calcHours(type, start, end) * p_profile->calcSum(code, type, start, end) / 3600.0;
                break;
            case SC_ABOVE:
                fmt += "%";
                val = 100.0 / p_profile->calcHours(type, start, end) * (p_profile->calcAboveThreshold(code, schema::channel[code].upperThreshold(), type, start, end) / 60.0);
                break;
            case SC_BELOW:
                fmt += "%";
                val = 100.0 / p_profile->calcHours(type, start, end) * (p_profile->calcBelowThreshold(code, schema::channel[code].lowerThreshold(), type, start, end) / 60.0);
                break;
            default:
                break;
            };
        }

        if ((val == std::numeric_limits<EventDataType>::min()) || (val == std::numeric_limits<EventDataType>::max())) {
            value = "Err";
        } else {
            value = fmt.arg(val, 0, 'f', decimals);
        }
    }

    return value;
}
