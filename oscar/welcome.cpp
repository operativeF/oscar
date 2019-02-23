/* Welcome page Implementation
 *
 * Copyright (c) 2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of Source Code. */

#include <cmath>

#include "welcome.h"
#include "ui_welcome.h"

#include "mainwindow.h"
extern MainWindow * mainwin;


Welcome::Welcome(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Welcome)
{
    ui->setupUi(this);
    pixmap.load(":/icons/mask.png");


    refreshPage();
}

Welcome::~Welcome()
{
    delete ui;
}

void Welcome::refreshPage()
{
    const auto & mlist = p_profile->GetMachines(MT_CPAP);

    bool b = mlist.size() > 0;
    bool showCardWarning = !b;


    // The SDCard warning does not need to be seen anymore for people who DON'T use ResMed S9's.. show first import and only when S9 is present
    for (auto & mach :mlist) {
        if (mach->series().compare("S9") == 0) showCardWarning = true;
    }
    ui->S9Warning->setVisible(showCardWarning);

    if (!b) {
        ui->cpapIcon->setPixmap(pixmap);
    }
    ui->dailyButton->setEnabled(b);
    ui->overviewButton->setEnabled(b);
    ui->statisticsButton->setEnabled(b);

    ui->cpapInfo->setHtml(GenerateCPAPHTML());
    ui->oxiInfo->setHtml(GenerateOxiHTML());
}

void Welcome::on_dailyButton_clicked()
{

    mainwin->JumpDaily();
}

void Welcome::on_overviewButton_clicked()
{
    mainwin->JumpOverview();
}

void Welcome::on_statisticsButton_clicked()
{
    mainwin->JumpStatistics();
}

void Welcome::on_oximetryButton_clicked()
{
    mainwin->JumpOxiWizard();
}

void Welcome::on_importButton_clicked()
{
    mainwin->JumpImport();
}


extern EventDataType calcAHI(QDate start, QDate end);
extern EventDataType calcFL(QDate start, QDate end);


QString Welcome::GenerateCPAPHTML()
{
    auto cpap_machines = p_profile->GetMachines(MT_CPAP);
    auto oximeters = p_profile->GetMachines(MT_OXIMETER);
    QList<Machine *> mach;

    mach.append(cpap_machines);
    mach.append(oximeters);

    bool havecpapdata = false;
    bool haveoximeterdata = false;

    for (auto & mach : cpap_machines) {
        int daysize = mach->day.size();
        if (daysize > 0) {
            havecpapdata = true;
            break;
        }
    }
    for (auto & mach : oximeters) {
        int daysize = mach->day.size();
        if (daysize > 0) {
            haveoximeterdata = true;
            break;
        }
    }


    QString html = QString("<html><head>")+
    "</head>"
    "<style type='text/css'>"
    "p,a,td,body { font-family: '"+QApplication::font().family()+"'; }"
    "p,a,td,body { font-size: "+QString::number(QApplication::font().pointSize() + 2)+"px; }"
    "</style>"
    "</head>"
    "<body leftmargin=5 topmargin=10 rightmargin=5 bottommargin=5 vertical-align=center align=center>";

    Machine * cpap = nullptr;
    if (!havecpapdata && !haveoximeterdata) {
        html += "<p>" + tr("It might be a good idea to check preferences first,</br>as there are some options that affect import.")+"</p>"
        "<p>" + tr("First import can take a few minutes.") + "</p>";
    } else {
        QDate date = p_profile->LastDay(MT_CPAP);
        Day *day = p_profile->GetDay(date, MT_CPAP);

        if (havecpapdata && day) {
            cpap = day->machine(MT_CPAP);
        }
        if (day && (cpap != nullptr)) {
            QString cpapimage = cpap->getPixmapPath();

            ui->cpapIcon->setPixmap(QPixmap(cpapimage));

            html+= "<b>"+tr("The last time you used your %1...").arg(cpap->brand()+" "+cpap->series()+" "+cpap->model())+"</b><br/>";

            int daysto = date.daysTo(QDate::currentDate());
            QString daystring;
            if (daysto == 1) daystring += tr("last night");
            else if (daysto == 2) daystring += tr("yesterday");
            else daystring += tr("%2 days ago").arg(date.daysTo(QDate::currentDate()));

            html += tr("was %1 (on %2)").arg(daystring).arg(date.toString(Qt::SystemLocaleLongDate)) + "<br/>";

            EventDataType hours = day->hours();
            html += "<br/>";

            int seconds = int(hours * 3600.0) % 60;
            int minutes = int(hours * 60) % 60;
            int hour = hours;
            QString timestr = tr("%1 hours, %2 minutes and %3 seconds").arg(hour).arg(minutes).arg(seconds);

            const EventDataType compliance_min = 4.0;
            if (hours > compliance_min) html += tr("Your machine was on for %1.").arg(timestr)+"<br/>";
            else html += tr("<font color = red>You only had the mask on for %1.</font>").arg(timestr)+"<br/>";


            int averagedays = 7; // how many days to look back

            QDate starttime = date.addDays(-(averagedays-1));


            EventDataType ahi = (day->count(CPAP_Obstructive) + day->count(CPAP_Hypopnea) + day->count(CPAP_ClearAirway) + day->count(CPAP_Apnea)) / hours;
            EventDataType ahidays = calcAHI(starttime, date);

            const QString under = tr("under");
            const QString over = tr("over");
            const QString close = tr("reasonably close to");
            const QString equal = tr("equal to");


            QString comp;
            if ((ahi < ahidays) && ((ahidays - ahi) >= 0.1)) {
                comp = under;
            } else if ((ahi > ahidays) && ((ahi - ahidays) >= 0.1)) {
                comp = over;
            } else if ((fabs(ahi > ahidays) >= 0.01) ) {
                comp = close;
            } else {
                comp = equal;
            }

            html += tr("You had an AHI of %1, which is %2 your %3 day average of %4.").arg(ahi,0,'f',2).arg(comp).arg(averagedays).arg(ahidays,0,'f',2);

            html += "<br/>";

            CPAPMode cpapmode = (CPAPMode)(int)day->settings_max(CPAP_Mode);
            double perc= p_profile->general->prefCalcPercentile();

            if (cpapmode == MODE_CPAP) {
                EventDataType pressure = day->settings_max(CPAP_Pressure);
                html += tr("Your CPAP machine blasted you with a constant %1%2 of air").arg(pressure).arg(schema::channel[CPAP_Pressure].units());
            } else if (cpapmode == MODE_APAP) {
                EventDataType pressure = day->percentile(CPAP_Pressure, perc/100.0);
                html += tr("Your pressure was under %1%2 for %3% of the time.").arg(pressure).arg(schema::channel[CPAP_Pressure].units()).arg(perc);
            } else if (cpapmode == MODE_BILEVEL_FIXED) {
                EventDataType ipap = day->settings_max(CPAP_IPAP);
                EventDataType epap = day->settings_min(CPAP_EPAP);
                html += tr("Your machine blasted you with a constant %1-%2 %3 of air.").arg(epap).arg(ipap).arg(schema::channel[CPAP_Pressure].units());
            } else if (cpapmode == MODE_BILEVEL_AUTO_FIXED_PS) {
                EventDataType ipap = day->percentile(CPAP_IPAP, perc/100.0);
                EventDataType epap = day->percentile(CPAP_EPAP, perc/100.0);
                html += tr("Your machine was under %1-%2 %3 for %4% of the time.").arg(epap).arg(ipap).arg(schema::channel[CPAP_Pressure].units()).arg(perc);
            } else if (cpapmode == MODE_ASV){
                EventDataType ipap = day->percentile(CPAP_IPAP, perc/100.0);
                EventDataType epap = qRound(day->settings_wavg(CPAP_EPAP));

                html += tr("Your EPAP pressure fixed at %1%2.").arg(epap).arg(schema::channel[CPAP_EPAP].units())+"<br/>";
                html += tr("Your IPAP pressure was under %1%2 for %3% of the time.").arg(ipap).arg(schema::channel[CPAP_IPAP].units()).arg(perc);
            } else if (cpapmode == MODE_ASV_VARIABLE_EPAP){
                EventDataType ipap = day->percentile(CPAP_IPAP, perc/100.0);
                EventDataType epap = day->percentile(CPAP_EPAP, perc/100.0);

                html += tr("Your EPAP pressure was under %1%2 for %3% of the time.").arg(epap).arg(schema::channel[CPAP_EPAP].units()).arg(perc)+"<br/>";
                html += tr("Your IPAP pressure was under %1%2 for %3% of the time.").arg(ipap).arg(schema::channel[CPAP_IPAP].units()).arg(perc);
            }
            html += "<br/>";

            //EventDataType lat = day->timeAboveThreshold(CPAP_Leak, p_profile->cpap->leakRedline())/ 60.0;
            //EventDataType leaks = 1.0/hours * lat;

            EventDataType leak = day->avg(CPAP_Leak);
            EventDataType leakdays = p_profile->calcAvg(CPAP_Leak, MT_CPAP, starttime, date);

            if ((leak < leakdays) && ((leakdays - leak) >= 0.1)) {
                comp = under;
            } else if ((leak > leakdays) && ((leak - leakdays) >= 0.1)) {
                comp = over;
            } else if ((fabs(ahi > ahidays) >= 0.01) ) {
                comp = close;
            } else {
                comp = equal;
            }

            html += tr("Your average leaks were %1 %2, which is %3 your %4 day average of %5.").arg(leak,0,'f',2).arg(schema::channel[CPAP_Leak].units()).arg(comp).arg(averagedays).arg(leakdays,0,'f',2);

            html += "<br/>";


        } else {
            html += "<p>"+tr("No CPAP data has been imported yet.")+"</p>";
        }
    }

    html += "</body></html>";
    return html;
}


QString Welcome::GenerateOxiHTML()
{
    auto oximeters = p_profile->GetMachines(MT_OXIMETER);

    bool haveoximeterdata = false;

    for (auto & mach : oximeters) {
        int daysize = mach->day.size();
        if (daysize > 0) {
            haveoximeterdata = true;
            break;
        }
    }

    QString html = QString("<html><head>")+
    "</head>"
    "<style type='text/css'>"
    "p,a,td,body { font-family: '"+QApplication::font().family()+"'; }"
    "p,a,td,body { font-size: "+QString::number(QApplication::font().pointSize() + 2)+"px; }"
    "</style>"
    "</head>"
    "<body leftmargin=5 topmargin=5 rightmargin=5 bottommargin=5 valign=center align=center>";

    if (haveoximeterdata) {
        QDate oxidate=p_profile->LastDay(MT_OXIMETER);
        int daysto = oxidate.daysTo(QDate::currentDate());

        html += "<p>"+QObject::tr("Most recent Oximetry data: <a onclick='alert(\"daily=%2\");'>%1</a> ").arg(oxidate.toString(Qt::SystemLocaleLongDate)).arg(oxidate.toString(Qt::ISODate));
        if (daysto == 1) html += QObject::tr("(last night)");
        else if (daysto == 2) html += QObject::tr("(yesterday)");
        else html += QObject::tr("(%2 day ago)").arg(oxidate.daysTo(QDate::currentDate()));
        html+="</p>";
        ui->oxiIcon->setVisible(true);
        ui->oxiInfo->setVisible(true);
    } else {
        html += "<p>"+QObject::tr("No oximetry data has been imported yet.")+"</p>";
        ui->oxiIcon->setVisible(false);
        ui->oxiInfo->setVisible(false);
    }

    html += "</body></html>";
    return html;
}

