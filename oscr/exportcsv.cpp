/* ExportCSV module implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QFileDialog>
#include <QLocale>
#include <QMessageBox>
#include <QCalendarWidget>
#include <QTextCharFormat>
#include "SleepLib/profiles.h"
#include "SleepLib/day.h"
#include "exportcsv.h"
#include "ui_exportcsv.h"
#include "mainwindow.h"

extern MainWindow *mainwin;

ExportCSV::ExportCSV(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportCSV)
{
    ui->setupUi(this);
    ui->rb1_Summary->setChecked(true);
    ui->quickRangeCombo->setCurrentIndex(0);

    // Set Date controls locale to 4 digit years
    QLocale locale = QLocale::system();
    QString shortformat = locale.dateFormat(QLocale::ShortFormat);

    if (!shortformat.toLower().contains("yyyy")) {
        shortformat.replace("yy", "yyyy");
    }

    ui->startDate->setDisplayFormat(shortformat);
    ui->endDate->setDisplayFormat(shortformat);
    // Stop both calendar drop downs highlighting weekends in red
    QTextCharFormat format = ui->startDate->calendarWidget()->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(Qt::black, Qt::SolidPattern));
    ui->startDate->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->startDate->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    ui->endDate->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->endDate->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);

    Qt::DayOfWeek dow = firstDayOfWeekFromLocale();

    ui->startDate->calendarWidget()->setFirstDayOfWeek(dow);
    ui->endDate->calendarWidget()->setFirstDayOfWeek(dow);

    // Connect the signals to update which days have CPAP data when the month is changed
    connect(ui->startDate->calendarWidget(), SIGNAL(currentPageChanged(int, int)),
            SLOT(startDate_currentPageChanged(int, int)));
    connect(ui->endDate->calendarWidget(), SIGNAL(currentPageChanged(int, int)),
            SLOT(endDate_currentPageChanged(int, int)));

    on_quickRangeCombo_activated(tr("Most Recent Day"));
    ui->rb1_details->clearFocus();
    ui->quickRangeCombo->setFocus();
    ui->exportButton->setEnabled(false);
}

ExportCSV::~ExportCSV()
{
    delete ui;
}

void ExportCSV::on_filenameBrowseButton_clicked()
{
    QString timestamp = tr("SleepyHead_");
    timestamp += p_profile->Get("Username") + "_";

    if (ui->rb1_details->isChecked()) { timestamp += tr("Details_"); }

    if (ui->rb1_Sessions->isChecked()) { timestamp += tr("Sessions_"); }

    if (ui->rb1_Summary->isChecked()) { timestamp += tr("Summary_"); }

    timestamp += ui->startDate->date().toString(Qt::ISODate);

    if (ui->startDate->date() != ui->endDate->date()) { timestamp += "_" + ui->endDate->date().toString(Qt::ISODate); }

    timestamp += ".csv";
    QString name = QFileDialog::getSaveFileName(this, tr("Select file to export to"),
                   PREF.Get("{home}/") + timestamp, tr("CSV Files (*.csv)"));

    if (name.isEmpty()) {
        ui->exportButton->setEnabled(false);
        return;
    }

    if (!name.toLower().endsWith(".csv")) {
        name += ".csv";
    }

    ui->filenameEdit->setText(name);
    ui->exportButton->setEnabled(true);
}

void ExportCSV::on_quickRangeCombo_activated(const QString &arg1)
{
    QDate first = p_profile->FirstDay();
    QDate last = p_profile->LastDay();

    if (arg1 == tr("Custom")) {
        ui->startDate->setEnabled(true);
        ui->endDate->setEnabled(true);
        ui->startLabel->setEnabled(true);
        ui->endLabel->setEnabled(true);
    } else {
        ui->startDate->setEnabled(false);
        ui->endDate->setEnabled(false);
        ui->startLabel->setEnabled(false);
        ui->endLabel->setEnabled(false);

        if (arg1 == tr("Everything")) {
            ui->startDate->setDate(first);
            ui->endDate->setDate(last);
        } else if (arg1 == tr("Most Recent Day")) {
            ui->startDate->setDate(last);
            ui->endDate->setDate(last);
        } else if (arg1 == tr("Last Week")) {
            ui->startDate->setDate(last.addDays(-7));
            ui->endDate->setDate(last);
        } else if (arg1 == tr("Last Fortnight")) {
            ui->startDate->setDate(last.addDays(-14));
            ui->endDate->setDate(last);
        } else if (arg1 == tr("Last Month")) {
            ui->startDate->setDate(last.addMonths(-1));
            ui->endDate->setDate(last);
        } else if (arg1 == tr("Last 6 Months")) {
            ui->startDate->setDate(last.addMonths(-6));
            ui->endDate->setDate(last);
        } else if (arg1 == tr("Last Year")) {
            ui->startDate->setDate(last.addYears(-1));
            ui->endDate->setDate(last);
        }
    }
}

void ExportCSV::on_exportButton_clicked()
{
    QFile file(ui->filenameEdit->text());
    file.open(QFile::WriteOnly);
    QString header;
    const QString sep = ",";
    const QString newline = "\n";

    //    if (ui->rb1_details->isChecked()) {
    //        fields.append(DumpField(NoChannel,MT_CPAP,ST_DATE));
    //    } else {
    //        header=tr("DateTime")+sep+tr("Session")+sep+tr("Event")+sep+tr("Data/Duration");
    //    } else {
    //        if (ui->rb1_Summary->isChecked()) {
    //            header=tr("Date")+sep+tr("Session Count")+sep+tr("Start")+sep+tr("End")+sep+tr("Total Time")+sep+tr("AHI");
    //        } else if (ui->rb1_Sessions->isChecked()) {
    //            header=tr("Date")+sep+tr("Session")+sep+tr("Start")+sep+tr("End")+sep+tr("Total Time")+sep+tr("AHI");
    //        }
    //    }
    //    fields.append(DumpField(NoChannel,MT_CPAP,ST_SESSIONS));


    QList<ChannelID> countlist, avglist, p90list, maxlist;
    countlist.append(CPAP_Hypopnea);
    countlist.append(CPAP_Obstructive);
    countlist.append(CPAP_Apnea);
    countlist.append(CPAP_ClearAirway);
    countlist.append(CPAP_VSnore);
    countlist.append(CPAP_VSnore2);
    countlist.append(CPAP_RERA);
    countlist.append(CPAP_FlowLimit);
    countlist.append(CPAP_SensAwake);
    countlist.append(CPAP_NRI);
    countlist.append(CPAP_ExP);
    countlist.append(CPAP_LeakFlag);
    countlist.append(CPAP_UserFlag1);
    countlist.append(CPAP_UserFlag2);
    countlist.append(CPAP_PressurePulse);

    avglist.append(CPAP_Pressure);
    avglist.append(CPAP_IPAP);
    avglist.append(CPAP_EPAP);
    avglist.append(CPAP_FLG);        // Pholynyk, 25Aug2015, add ResMed Flow Limitation

    p90list.append(CPAP_Pressure);
    p90list.append(CPAP_IPAP);
    p90list.append(CPAP_EPAP);
    p90list.append(CPAP_FLG);
       
    float percentile=p_profile->general->prefCalcPercentile()/100.0;                   // Pholynyk, 18Aug2015
    EventDataType percent = percentile;                                                // was 0.90F

    maxlist.append(CPAP_Pressure);    // Pholynyk, 18Aug2015, add maximums
    maxlist.append(CPAP_IPAP);
    maxlist.append(CPAP_EPAP);
    maxlist.append(CPAP_FLG);

    // Not sure this section should be translateable.. :-/
    if (ui->rb1_details->isChecked()) {
        header = tr("DateTime") + sep + tr("Session") + sep + tr("Event") + sep + tr("Data/Duration");
    } else {
        if (ui->rb1_Summary->isChecked()) {
            header = tr("Date") + sep + tr("Session Count") + sep + tr("Start") + sep + tr("End") + sep +
                     tr("Total Time") + sep + tr("AHI");
        } else if (ui->rb1_Sessions->isChecked()) {
            header = tr("Date") + sep + tr("Session") + sep + tr("Start") + sep + tr("End") + sep +
                     tr("Total Time") + sep + tr("AHI");
        }

        for (int i = 0; i < countlist.size(); i++) {
            header += sep + schema::channel[countlist[i]].label() + tr(" Count");
        }

        for (int i = 0; i < avglist.size(); i++) {
            header += sep + Day::calcMiddleLabel(avglist[i]);        // Pholynyk, 18Aug2015
        }

        for (int i = 0; i < p90list.size(); i++) {
            header += sep + tr("%1% ").arg(percent*100.0, 0, 'f', 0) + schema::channel[p90list[i]].label();
        }

        for (int i = 0; i < maxlist.size(); i++) {
            header += sep + Day::calcMaxLabel(maxlist[i]);                                     // added -- Pholynyk, 18Aug2015
        }
    }

    header += newline;
    file.write(header.toLatin1());
    QDate date = ui->startDate->date();
    Daily *daily = mainwin->getDaily();
    QDate daily_date = daily->getDate();

    ui->progressBar->setValue(0);
    ui->progressBar->setMaximum(p_profile->daylist.count());

    do {
        ui->progressBar->setValue(ui->progressBar->value() + 1);
        QApplication::processEvents();

        Day *day = p_profile->GetDay(date, MT_CPAP);

        if (day) {
            QString data;

            if (ui->rb1_Summary->isChecked()) {
                QDateTime start = QDateTime::fromTime_t(day->first() / 1000L);
                QDateTime end = QDateTime::fromTime_t(day->last() / 1000L);
                data = date.toString(Qt::ISODate);
                data += sep + QString::number(day->size(), 10);
                data += sep + start.toString(Qt::ISODate);
                data += sep + end.toString(Qt::ISODate);
                int time = day->total_time() / 1000L;
                int h = time / 3600;
                int m = int(time / 60) % 60;
                int s = int(time) % 60;
                data += sep + QString().sprintf("%02i:%02i:%02i", h, m, s);
                float ahi = day->count(CPAP_Obstructive) + day->count(CPAP_Hypopnea) + day->count(
                                CPAP_Apnea) + day->count(CPAP_ClearAirway);
                ahi /= day->hours();
                data += sep + QString::number(ahi, 'f', 3);

                for (int i = 0; i < countlist.size(); i++) {
                    data += sep + QString::number(day->count(countlist.at(i)));
                }

                for (int i = 0; i < avglist.size(); i++) {
                    float avg = day->calcMiddle(avglist.at(i));
                    data += sep + QString::number(avg);                // Pholynyk, 11Aug2015
                }

                for (int i = 0; i < p90list.size(); i++) {
                    float p90 = day->percentile(p90list.at(i), percent);
                    data += sep + QString::number(p90);                // Pholynyk, 11Aug2015
                }

                for (int i = 0; i < maxlist.size(); i++) {
                    float max = day->calcMax(maxlist.at(i));
                    data += sep + QString::number(max);                // added -- Pholynyk, 18Aug2015
                }

                data += newline;
                file.write(data.toLatin1());

            } else if (ui->rb1_Sessions->isChecked()) {
                for (int i = 0; i < day->size(); i++) {
                    Session *sess = (*day)[i];
                    QDateTime start = QDateTime::fromTime_t(sess->first() / 1000L);
                    QDateTime end = QDateTime::fromTime_t(sess->last() / 1000L);

                    data = date.toString(Qt::ISODate);
                    data += sep + QString::number(sess->session(), 10);
                    data += sep + start.toString(Qt::ISODate);
                    data += sep + end.toString(Qt::ISODate);
                    int time = sess->length() / 1000L;
                    int h = time / 3600;
                    int m = int(time / 60) % 60;
                    int s = int(time) % 60;
                    data += sep + QString().sprintf("%02i:%02i:%02i", h, m, s);

                    float ahi = sess->count(CPAP_Obstructive) + sess->count(CPAP_Hypopnea) + sess->count(
                                    CPAP_Apnea) + sess->count(CPAP_ClearAirway);
                    ahi /= sess->hours();
                    data += sep + QString::number(ahi, 'f', 3);

                    for (int j = 0; j < countlist.size(); j++) {
                        data += sep + QString::number(sess->count(countlist.at(j)));
                    }

                    for (int j = 0; j < avglist.size(); j++) {
                        data += sep + QString::number(sess->calcMiddle(avglist.at(j)));                // Pholynyk, 11Aug2015
                    }

                    for (int j = 0; j < p90list.size(); j++) {
                        data += sep + QString::number(sess->percentile(p90list.at(j), percent));               // Pholynyk, 11Aug2015
                    }

                    for (int i = 0; i < maxlist.size(); i++) {
                        data += sep + QString::number(sess->calcMax(maxlist.at(i)));                // Pholynyk, 11Aug2015
                    }

                    data += newline;
                    file.write(data.toLatin1());
                }
            } else if (ui->rb1_details->isChecked()) {
                QList<ChannelID> all = countlist;
                all.append(avglist);

                for (int i = 0; i < day->size(); i++) {
                    Session *sess = (*day)[i];
                    sess->OpenEvents();
                    QHash<ChannelID, QVector<EventList *> >::iterator fnd;

                    for (int j = 0; j < all.size(); j++) {
                        ChannelID key = all.at(j);
                        fnd = sess->eventlist.find(key);

                        if (fnd != sess->eventlist.end()) {
                            //header="DateTime"+sep+"Session"+sep+"Event"+sep+"Data/Duration";
                            for (int e = 0; e < fnd.value().size(); e++) {
                                EventList *ev = fnd.value()[e];

                                for (quint32 q = 0; q < ev->count(); q++) {
                                    data = QDateTime::fromTime_t(ev->time(q) / 1000L).toString(Qt::ISODate);
                                    data += sep + QString::number(sess->session());
                                    data += sep + schema::channel[key].code();
                                    data += sep + QString::number(ev->data(q), 'f', 2);
                                    data += newline;
                                    file.write(data.toLatin1());
                                }
                            }
                        }
                    }

                    if (daily_date != date) {
                        sess->TrashEvents();
                    }
                }
            }
        }

        date = date.addDays(1);
    } while (date <= ui->endDate->date());

    file.close();
    ExportCSV::accept();
}


void ExportCSV::UpdateCalendarDay(QDateEdit *dateedit, QDate date)
{
    QCalendarWidget *calendar = dateedit->calendarWidget();
    QTextCharFormat bold;
    QTextCharFormat cpapcol;
    QTextCharFormat normal;
    QTextCharFormat oxiday;
    bold.setFontWeight(QFont::Bold);
    cpapcol.setForeground(QBrush(Qt::blue, Qt::SolidPattern));
    cpapcol.setFontWeight(QFont::Bold);
    oxiday.setForeground(QBrush(Qt::red, Qt::SolidPattern));
    oxiday.setFontWeight(QFont::Bold);
    bool hascpap = p_profile->GetDay(date, MT_CPAP) != nullptr;
    bool hasoxi = p_profile->GetDay(date, MT_OXIMETER) != nullptr;
    //bool hasjournal=p_profile->GetDay(date,MT_JOURNAL)!=nullptr;

    if (hascpap) {
        if (hasoxi) {
            calendar->setDateTextFormat(date, oxiday);
        } else {
            calendar->setDateTextFormat(date, cpapcol);
        }
    } else if (p_profile->GetDay(date)) {
        calendar->setDateTextFormat(date, bold);
    } else {
        calendar->setDateTextFormat(date, normal);
    }

    calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
}
void ExportCSV::startDate_currentPageChanged(int year, int month)
{
    QDate d(year, month, 1);
    int dom = d.daysInMonth();

    for (int i = 1; i <= dom; i++) {
        d = QDate(year, month, i);
        UpdateCalendarDay(ui->startDate, d);
    }
}
void ExportCSV::endDate_currentPageChanged(int year, int month)
{
    QDate d(year, month, 1);
    int dom = d.daysInMonth();

    for (int i = 1; i <= dom; i++) {
        d = QDate(year, month, i);
        UpdateCalendarDay(ui->endDate, d);
    }
}
