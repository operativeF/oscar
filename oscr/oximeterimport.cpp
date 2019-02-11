/* Oximeter Import Wizard Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QThread>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDesktopServices>

#include "Graphs/gYAxis.h"
#include "Graphs/gXAxis.h"

#include "oximeterimport.h"
#include "ui_oximeterimport.h"
#include "SleepLib/calcs.h"
#include "mainwindow.h"

extern MainWindow * mainwin;

#include "SleepLib/loader_plugins/cms50_loader.h"
#include "SleepLib/loader_plugins/cms50f37_loader.h"
#include "SleepLib/loader_plugins/md300w1_loader.h"

Qt::DayOfWeek firstDayOfWeekFromLocale();
QList<SerialOximeter *> GetOxiLoaders();

OximeterImport::OximeterImport(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OximeterImport)
{
    ui->setupUi(this);
    setWindowTitle(tr("Oximeter Import Wizard"));
    ui->stackedWidget->setCurrentIndex(0);
    ui->retryButton->setVisible(false);
    ui->stopButton->setVisible(false);
    ui->saveButton->setVisible(false);
    ui->syncButton->setVisible(false);
    ui->chooseSessionButton->setVisible(false);

    oximodule = nullptr;
    importMode = IM_UNDEFINED;

    liveView = new gGraphView(this);
    liveView->setVisible(false);
    liveView->setShowAuthorMessage(false);
    QVBoxLayout * lvlayout = new QVBoxLayout;
    lvlayout->setMargin(0);
    lvlayout->addWidget(liveView);
    ui->liveViewFrame->setLayout(lvlayout);

    plethyGraph = new gGraph("Plethy", liveView, STR_TR_Plethy, STR_UNIT_Hz);
    plethyGraph->AddLayer(new gYAxis(), LayerLeft, gYAxis::Margin);
    plethyGraph->AddLayer(new gXAxis(), LayerBottom, 0, 20);
    plethyGraph->AddLayer(plethyChart = new gLineChart(OXI_Plethy));
    plethyGraph->setVisible(true);
    plethyGraph->setRecMinY(0);
    plethyGraph->setRecMaxY(128);

    ui->calendarWidget->setFirstDayOfWeek(Qt::Sunday);
    QTextCharFormat format = ui->calendarWidget->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(Qt::black, Qt::SolidPattern));
    ui->calendarWidget->setWeekdayTextFormat(Qt::Saturday, format);
    ui->calendarWidget->setWeekdayTextFormat(Qt::Sunday, format);
    ui->calendarWidget->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    Qt::DayOfWeek dow=firstDayOfWeekFromLocale();
    ui->calendarWidget->setFirstDayOfWeek(dow);

    ui->dateTimeEdit->setMinimumHeight(ui->dateTimeEdit->height() + 10);
    ui->syncCPAPGroup->setVisible(false);

    QVBoxLayout * layout = new QVBoxLayout;
    layout->setMargin(0);
    ui->sessBarFrame->setLayout(layout);
    sessbar = new SessionBar(this);
    sessbar->setSelectMode(true);
    sessbar->setMouseTracking(true);
    sessbar->setMinimumHeight(40);
    connect(sessbar, SIGNAL(sessionClicked(Session*)), this, SLOT(onSessionSelected(Session*)));
    layout->addWidget(sessbar, 1);

    dummyday = nullptr;
    session = nullptr;
    ELplethy = nullptr;

    pulse = spo2 = -1;


    ui->skipWelcomeCheckbox->setChecked(p_profile->oxi->skipOxiIntroScreen());
    if (p_profile->oxi->skipOxiIntroScreen()) {
        ui->stackedWidget->setCurrentWidget(ui->importSelectionPage);
        ui->nextButton->setVisible(false);
        ui->informationButton->setVisible(true);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->welcomePage);
        ui->nextButton->setVisible(true);
        ui->informationButton->setVisible(false);
    }

    ui->dateTimeEdit->setMinimumHeight(ui->dateTimeEdit->height()+10);
    setInformation();

    ui->cms50DeviceName->setText(p_profile->oxi->defaultDevice());
    int oxitype = p_profile->oxi->oximeterType();
    ui->oximeterType->setCurrentIndex(oxitype);
    on_oximeterType_currentIndexChanged(oxitype);
    ui->cms50DeviceName->setEnabled(false);
    ui->cms50SyncTime->setChecked(p_profile->oxi->syncOximeterClock());


}

OximeterImport::~OximeterImport()
{
    if (!dummyday) {
        delete dummyday;
    }
    if (!session) {
        delete session;
    }

    disconnect(sessbar, SIGNAL(sessionClicked(Session*)), this, SLOT(onSessionSelected(Session*)));
    delete ui;
}

void OximeterImport::on_nextButton_clicked()
{
    int i = ui->stackedWidget->currentIndex();
    i++;
    if (i >= ui->stackedWidget->count()) i = 0;

    switch (i) {
    case 0:
        ui->nextButton->setVisible(true);
        ui->nextButton->setText("&Start");
        break;
    case 1:
        ui->nextButton->setVisible(false);
        ui->informationButton->setVisible(true);

        break;
    default:
        ui->informationButton->setVisible(true);

        ui->nextButton->setVisible(true);


    }
    ui->stackedWidget->setCurrentIndex(i);
}

void OximeterImport::updateStatus(QString msg)
{
	qDebug() << "updateStatus to " << msg;
    ui->logBox->appendPlainText(msg);
    ui->directImportStatus->setText(msg);
    ui->liveStatusLabel->setText(msg);
}


SerialOximeter * OximeterImport::detectOximeter()
{
    const int PORTSCAN_TIMEOUT=30000;
    const int delay=100;

	qDebug() << "Attempt to detect Oximeter";
    ui->retryButton->setVisible(false);

    QList<SerialOximeter *> loaders; // GetOxiLoaders();

    if (ui->oximeterType->currentIndex() == 0) { // CMS50F3.7
        SerialOximeter * oxi = qobject_cast<SerialOximeter*>(lookupLoader(cms50f37_class_name));
        loaders.push_back(oxi);
    } else if (ui->oximeterType->currentIndex() == 1) { // CMS50D+/E/F
        SerialOximeter * oxi = qobject_cast<SerialOximeter*>(lookupLoader(cms50_class_name));
        loaders.push_back(oxi);
    } else if (ui->oximeterType->currentIndex() == 2) { // ChoiceMed
        SerialOximeter * oxi = qobject_cast<SerialOximeter*>(lookupLoader(md300w1_class_name));
        loaders.push_back(oxi);
    } else return nullptr;


    updateStatus(tr("Scanning for compatible oximeters"));

    ui->progressBar->setMaximum(PORTSCAN_TIMEOUT);

    QTime time;
    time.start();

    oximodule = nullptr;
    int elapsed=0;
    do {
        for (int i=0; i < loaders.size(); ++i) {
            SerialOximeter * oxi = loaders[i];
            if (oxi->openDevice()) {
                oximodule = oxi;
                break;
            }
         }

         if (oximodule)
            break;

        QThread::msleep(delay);
        elapsed = time.elapsed();
        ui->progressBar->setValue(elapsed);
        QApplication::processEvents();
        if (!isVisible()) {
            return oximodule = nullptr;
        }

    } while (elapsed < PORTSCAN_TIMEOUT);

    if (!oximodule) {
        updateStatus(tr("Could not detect any connected oximeter devices."));
        ui->retryButton->setVisible(true);
        return nullptr;
    }

    QString devicename = oximodule->getDeviceString();
    if (devicename.isEmpty()) oximodule->loaderName();

    updateStatus(tr("Connecting to %1 Oximeter").arg(devicename));

    return oximodule;
}



void OximeterImport::on_directImportButton_clicked()
{
    ui->informationButton->setVisible(false);
    ui->stackedWidget->setCurrentWidget(ui->directImportPage);

	qDebug() << "Direct Import button clicked" ;
    oximodule = detectOximeter();
    if (!oximodule)
        return;

    if (p_profile->oxi->syncOximeterClock()) {
        oximodule->syncClock();
    }

    QString model = oximodule->getModel();
    QString user = oximodule->getUser();
    QString devid = oximodule->getDeviceID();

    if (oximodule->commandDriven()) {
        if (devid != ui->cms50DeviceName->text()) {
            if (ui->cms50CheckName->isChecked()) {
                mainwin->Notify(STR_MessageBox_Information, tr("Renaming this oximeter from '%1' to '%2'").arg(devid).arg(ui->cms50DeviceName->text()));
                oximodule->setDeviceID(ui->cms50DeviceName->text());
            } else {
                QMessageBox::information(this, STR_MessageBox_Information, tr("Oximeter name is different.. If you only have one and are sharing it between profiles, set the name to the same on both profiles."), QMessageBox::Ok);
            }
        }
    }

    oximodule->resetDevice();
    int session_count = oximodule->getSessionCount();

    if (session_count > 1) {
        ui->stackedWidget->setCurrentWidget(ui->chooseSessionPage);
        ui->syncButton->setVisible(false);
        ui->chooseSessionButton->setVisible(true);

        ui->tableOxiSessions->clearContents();
        QTableWidgetItem * item;

        ui->tableOxiSessions->setRowCount(session_count);
        ui->tableOxiSessions->setSelectionBehavior(QAbstractItemView::SelectRows);

        ui->tableOxiSessions->setColumnWidth(0,150);

        int h, m, s;
        for (int i=0; i< session_count; ++i) {
            int duration = oximodule->getDuration(i);
            QDateTime datetime = oximodule->getDateTime(i);
            h = duration / 3600;
            m = (duration / 60) % 60;
            s = duration % 60;

            item = new QTableWidgetItem(datetime.date().toString(Qt::SystemLocaleShortDate)+" "+datetime.time().toString("HH:mm:ss"));
            ui->tableOxiSessions->setItem(i, 0, item);
       //   item->setData(Qt::UserRole+1, datetime);
       //   item->setData(Qt::UserRole, i);
       //   item->setData(Qt::UserRole+2, duration);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);

            item = new QTableWidgetItem(QString(). sprintf("%02i:%02i:%02i", h,m,s));
            ui->tableOxiSessions->setItem(i, 1, item);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);

            item = new QTableWidgetItem(tr("\"%1\", session %2").arg(user).arg(i+1, 0));
            ui->tableOxiSessions->setItem(i, 2, item);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        }

        selecting_session = true;
        ui->tableOxiSessions->selectRow(0);
        return;
    } else if (session_count > 0) {
        chosen_sessions.push_back(0);
        oximodule->getDuration(0);
        oximodule->setStartTime(oximodule->getDateTime(0));
    }
    doImport();
}

void OximeterImport::doImport()
{
	qDebug() << "Starting doImport";
	
    if (oximodule->commandDriven()) {
        if (chosen_sessions.size() == 0) {
            ui->connectLabel->setText("<h2>"+tr("Nothing to import")+"</h2>");

            updateStatus(tr("Your oximeter did not have any valid sessions."));
            ui->cancelButton->setText(tr("Close"));
            return;
        }
        ui->connectLabel->setText("<h2>"+tr("Waiting for %1 to start").arg(oximodule->getModel())+"</h2>");
        updateStatus(tr("Waiting for the device to start the upload process..."));
    } else {
        ui->connectLabel->setText("<h2>"+tr("Select upload option on %1").arg(oximodule->loaderName())+"</h2>");
        ui->logBox->appendPlainText(tr("You need to tell your oximeter to begin sending data to the computer."));

        updateStatus(tr("Please connect your oximeter, enter it's menu and select upload to commence data transfer..."));
    }

    connect(oximodule, SIGNAL(updateProgress(int,int)), this, SLOT(doUpdateProgress(int,int)));

    oximodule->Open("import");

    if (oximodule->commandDriven()) {
        int chosen = chosen_sessions.at(0);
        oximodule->getSessionData(chosen);
    }

    // Wait to start import streaming..
    while (!oximodule->isImporting() && !oximodule->isAborted()) {
//        QThread::msleep(10);
        QApplication::processEvents();
        if (!isVisible()) {
            disconnect(oximodule, SIGNAL(updateProgress(int,int)), this, SLOT(doUpdateProgress(int,int)));
            oximodule->abort();
            break;
        }

    }

    if (!oximodule->isStreaming()) {
        disconnect(oximodule, SIGNAL(updateProgress(int,int)), this, SLOT(doUpdateProgress(int,int)));
        ui->retryButton->setVisible(true);
        ui->progressBar->setValue(0);
        oximodule->abort();
        return;
    }
    ui->connectLabel->setText("<h2>"+tr("%1 device is uploading data...").arg(oximodule->loaderName())+"</h2>");
    updateStatus(tr("Please wait until oximeter upload process completes. Do not unplug your oximeter."));

    importMode = IM_RECORDING;

    // Can't abort this bit or the oximeter will get confused...
    ui->cancelButton->setVisible(false);

    connect(oximodule, SIGNAL(importComplete(SerialOximeter*)), this, SLOT(finishedImport(SerialOximeter*)));
}

void OximeterImport::finishedImport(SerialOximeter * oxi)
{
    Q_UNUSED(oxi);

	qDebug() << "finished Import ";
	
    connect(oximodule, SIGNAL(importComplete(SerialOximeter*)), this, SLOT(finishedImport(SerialOximeter*)));
    ui->cancelButton->setVisible(true);
    disconnect(oximodule, SIGNAL(updateProgress(int,int)), this, SLOT(doUpdateProgress(int,int)));
    updateStatus(tr("Oximeter import completed.."));

    if (oximodule->oxisessions.size() > 1) {
        chooseSession();
    } else {
        on_syncButton_clicked();
    }
}

void OximeterImport::doUpdateProgress(int v, int t)
{
    ui->progressBar->setMaximum(t);
    ui->progressBar->setValue(v);
    QApplication::processEvents();
}


void OximeterImport::on_fileImportButton_clicked()
{

    const QString documentsFolder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

	qDebug() << "File Import button clicked";
	
    QString filename = QFileDialog::getOpenFileName(nullptr , tr("Select a valid oximetry data file"), documentsFolder, tr("Oximetry Files (*.spo *.spor *.spo2 *.SpO2 *.dat)"));

    if (filename.isEmpty())
        return;

    // Make sure filename dialog had time to close properly..
    QApplication::processEvents();

	qDebug() << "Chosen filename is " << filename;
    QList<SerialOximeter *> loaders = GetOxiLoaders();

    bool success = false;

    oximodule = nullptr;
    Q_FOREACH(SerialOximeter * loader, loaders) {
        if (loader->Open(filename)) {
            success = true;
            oximodule = loader;
            break;
        }
    }
    if (!success) {
        QMessageBox::warning(this, STR_MessageBox_Warning, tr("No Oximetry module could parse the given file:")+QString("\n\n%1").arg(filename), QMessageBox::Ok);
        return;
    }
    qDebug() << "Using loader " << oximodule->loaderName();
    
    ui->informationButton->setVisible(false);
    importMode = IM_FILE;


    if (oximodule->oxisessions.size() > 1) {
        chooseSession();
    } else {
    //	oximodule->setStartTime( ??? );  Nope, it was set in the loader module by the file import routime
        on_syncButton_clicked();
    }
    qDebug() << "Finished file import: Oximodule startTime is " << oximodule->startTime().toString("yyyy.MM.dd HH:mm:ss");
}

void OximeterImport::on_liveImportButton_clicked()
{
    ui->informationButton->setVisible(false);

	qDebug() << "Live Import button clicked";
    ui->stackedWidget->setCurrentWidget(ui->liveImportPage);
    ui->liveImportPage->layout()->addWidget(ui->progressBar);
    QApplication::processEvents();

    liveView->setEmptyText(tr("Oximeter not detected"));
    liveView->setVisible(true);
    QApplication::processEvents();

    SerialOximeter * oximodule = detectOximeter();

    if (!oximodule) {
        updateStatus(tr("Couldn't access oximeter"));
        ui->retryButton->setVisible(true);
        ui->progressBar->setValue(0);

        return;
    }

    MachineInfo info = oximodule->newInfo();
    Machine *mach = p_profile->CreateMachine(info);

    connect(oximodule, SIGNAL(updatePlethy(QByteArray)), this, SLOT(on_updatePlethy(QByteArray)));
    ui->liveConnectLabel->setText(tr("Live Oximetry Mode"));


    liveView->setEmptyText(tr("Starting up..."));
    updateStatus(tr("If you can still read this after a few seconds, cancel and try again"));
    ui->progressBar->hide();
    liveView->update();
    oximodule->Open("live");
    ui->stopButton->setVisible(true);

    dummyday = new Day();

    quint32 starttime = oximodule->startTime().toTime_t();
    ti = qint64(starttime) * 1000L;
    start_ti = ti;

    session = new Session(mach, starttime);

    ELplethy = session->AddEventList(OXI_Plethy, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, 20);

    ELplethy->setFirst(start_ti);
    session->really_set_first(start_ti);
    session->setOpened(true);

    dummyday->addSession(session);

    plethyChart->setMinX(start_ti);
    plethyGraph->SetMinX(start_ti);

    liveView->setDay(dummyday);

    updateTimer.setParent(this);
    updateTimer.setInterval(50);
    updateTimer.start();
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateLiveDisplay()));
    connect(ui->stopButton, SIGNAL(clicked()), this, SLOT(finishedRecording()));

    importMode = IM_LIVE;

}

void OximeterImport::finishedRecording()
{
	qDebug() << "Finished Recording";
	
    updateTimer.stop();
    oximodule->closeDevice();
    disconnect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateLiveDisplay()));
    disconnect(ui->stopButton, SIGNAL(clicked()), this, SLOT(finishedRecording()));

    ui->stopButton->setVisible(false);
    ui->liveConnectLabel->setText(tr("Live Import Stopped"));
    liveView->setEmptyText(tr("Live Oximetry Stopped"));
    updateStatus(tr("Live Oximetry import has been stopped"));

    disconnect(oximodule, SIGNAL(updatePlethy(QByteArray)), this, SLOT(on_updatePlethy(QByteArray)));

    ui->syncButton->setVisible(true);

    plethyGraph->SetMinX(start_ti);
    liveView->SetXBounds(start_ti, ti, 0, true);

    plethyGraph->setBlockZoom(false);
}

void OximeterImport::on_retryButton_clicked()
{
	qDebug() << "Retry button clicked";
    if (ui->stackedWidget->currentWidget() == ui->directImportPage) {
        on_directImportButton_clicked();
    } else if (ui->stackedWidget->currentWidget() == ui->liveImportPage) {
        on_liveImportButton_clicked();
    }
}

void OximeterImport::on_stopButton_clicked()
{
	qDebug() << "Stop button clicked";
    if (oximodule) {
        oximodule->abort();
    }
}

void OximeterImport::on_calendarWidget_clicked(const QDate &date)
{
	qDebug() << "Calendar widget clicked " << date.toString("yyyy.MM.dd");
    if (ui->radioSyncCPAP->isChecked()) {
        Day * day = p_profile->GetGoodDay(date, MT_CPAP);

        sessbar->clear();
        if (day) {
            QDateTime time=QDateTime::fromMSecsSinceEpoch(day->first(), Qt::UTC);
            sessbar->clear();
            QList<QColor> colors;
            colors.push_back("#ffffe0");
            colors.push_back("#ffe0ff");
            colors.push_back("#e0ffff");
            QList<Session *>::iterator i;
            int j=0;
            for (i=day->begin(); i != day->end(); ++i) {
                sessbar->add((*i),colors.at(j++ % colors.size()));
            }
            sessbar->setVisible(true);
            ui->sessbarLabel->setText(tr("%1 session(s) on %2, starting at %3").arg(day->size()).arg(time.date().toString(Qt::SystemLocaleLongDate)).arg(time.time().toString("hh:mm:ssap")));
            sessbar->setSelected(0);
            ui->dateTimeEdit->setDateTime(time);
        } else {
            ui->sessbarLabel->setText(tr("No CPAP data available on %1").arg(date.toString(Qt::SystemLocaleLongDate)));
            ui->dateTimeEdit->setDateTime(QDateTime(date,oximodule->startTime().time()));
        }

        sessbar->update();
    } else if (ui->radioSyncOximeter) {
        ui->sessbarLabel->setText(tr("%1").arg(date.toString(Qt::SystemLocaleLongDate)));
        ui->dateTimeEdit->setDateTime(QDateTime(date, ui->dateTimeEdit->dateTime().time()));
    }
}

void OximeterImport::on_calendarWidget_selectionChanged()
{
    on_calendarWidget_clicked(ui->calendarWidget->selectedDate());
}

void OximeterImport::onSessionSelected(Session * session)
{
    QDateTime time=QDateTime::fromMSecsSinceEpoch(session->first(), Qt::UTC);
    ui->dateTimeEdit->setDateTime(time);
}

void OximeterImport::on_sessionBackButton_clicked()
{
	qDebug() << "Session Back button clicked";
    int idx = (sessbar->selected()-1);
    if (idx >= 0) {
        sessbar->setSelected(idx);
        QDateTime datetime = QDateTime::fromMSecsSinceEpoch(sessbar->session(idx)->first(), Qt::UTC);
        ui->dateTimeEdit->setDateTime(datetime);
        sessbar->update();
    }
}

void OximeterImport::on_sessionForwardButton_clicked()
{
	qDebug() << "Session Forward button clicked";
    int idx = (sessbar->selected()+1);
    if (idx < sessbar->count()) {
        sessbar->setSelected(idx);
        QDateTime datetime = QDateTime::fromMSecsSinceEpoch(sessbar->session(idx)->first(), Qt::UTC);
        ui->dateTimeEdit->setDateTime(datetime);
        sessbar->update();
    }
}

void OximeterImport::on_radioSyncCPAP_clicked()
{
    on_calendarWidget_clicked(oximodule->startTime().date());

    ui->syncCPAPGroup->setVisible(true);

}

void OximeterImport::on_radioSyncOximeter_clicked()
{
	qDebug() << "Use OximeterTime button clicked";
    ui->syncCPAPGroup->setVisible(false);
    if ( oximodule ) {
    	if (oximodule->isStartTimeValid()) {
    		qDebug() << "Oximeter time is valid " << oximodule->startTime().toString();
        	ui->calendarWidget->setSelectedDate(oximodule->startTime().date());
        	ui->dateTimeEdit->setDateTime(oximodule->startTime());
        } else
        	qDebug() << "Oximeter time is not valid";
    }
}

void OximeterImport::on_updatePlethy(QByteArray plethy)
{
    if (!session) {
        return;
    }
    int size = plethy.size();
    quint64 dur = qint64(size) * 20L;
    ELplethy->AddWaveform(ti, plethy.data(), size, dur);
    ti += dur;
}

void OximeterImport::updateLiveDisplay()
{
    if (!session) {
        return;
    }

    if (ui->showLiveGraphs->isChecked()) {
        qint64 sti = ti - 20000;
        plethyChart->setMinY(ELplethy->Min());
        plethyChart->setMaxY(ELplethy->Max());
        plethyGraph->SetMinY(ELplethy->Min());
        plethyGraph->SetMaxY(ELplethy->Max());
        plethyGraph->SetMinX(sti);
        plethyGraph->SetMaxX(ti);
        plethyGraph->setBlockZoom(true);
        ELplethy->setLast(ti);
        session->really_set_last(ti);


        //liveView->SetXBounds(sti, ti, 0, true);
        session->setMin(OXI_Plethy, ELplethy->Min());
        session->setMax(OXI_Plethy, ELplethy->Max());
        session->setLast(OXI_Plethy, ti);
        session->setCount(OXI_Plethy, ELplethy->count());

        for (int i = 0; i < liveView->size(); i++) {
            (*liveView)[i]->SetXBounds(sti, ti);
        }

        liveView->updateScale();
        liveView->redraw();
    }

    if (!oximodule->oxirec) return;
    int size = oximodule->oxirec->size();

    if (size > 0) {
        int i = oximodule->startTime().secsTo(QDateTime::currentDateTime());

        int seconds = i % 60;
        int minutes = (i / 60) % 60;
        int hours = i / 3600;

        size--;

        bool datagood = (*(oximodule->oxirec))[size].pulse > 0;


        if (datagood & (pulse <= 0)) {
            QString STR_recording = tr("Recording...");
            updateStatus(STR_recording);
            liveView->setEmptyText(STR_recording);
            if (!ui->showLiveGraphs->isChecked()) {
                liveView->redraw();
            }
        } else if (!datagood & (pulse != 0)) {
            QString STR_nofinger = tr("Finger not detected");
            updateStatus(STR_nofinger);
            liveView->setEmptyText(STR_nofinger);
            if (!ui->showLiveGraphs->isChecked()) {
                liveView->redraw();
            }
        }
        pulse = (*(oximodule->oxirec))[size].pulse;
        spo2 = (*(oximodule->oxirec))[size].spo2;
        if (pulse > 0) {
            ui->pulseDisplay->display(QString().sprintf("%3i", pulse));
        } else {
            ui->pulseDisplay->display("---");
        }
        if (spo2 > 0) {
            ui->spo2Display->display(QString().sprintf("%2i", spo2));
        } else {
            ui->spo2Display->display("--");
        }

        ui->lcdDuration->display(QString().sprintf("%02i:%02i:%02i",hours, minutes, seconds));

    }
}


void OximeterImport::on_cancelButton_clicked()
{
	qDebug() << "Cancel button clicked";
    if (oximodule && oximodule->isStreaming()) {
        oximodule->closeDevice();
        oximodule->trashRecords();
    }
    reject();
}

void OximeterImport::on_showLiveGraphs_clicked(bool checked)
{
    if (checked) {
        updateTimer.setInterval(50);
    } else {
        // Don't need to call the timer so often.. Save a little CPU..
        updateTimer.setInterval(500);
    }
    plethyGraph->setVisible(checked);
    liveView->redraw();
}

void OximeterImport::on_skipWelcomeCheckbox_clicked(bool checked)
{
    p_profile->oxi->setSkipOxiIntroScreen(checked);
}

void OximeterImport::on_informationButton_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->welcomePage);
    ui->nextButton->setVisible(true);
    ui->informationButton->setVisible(false);
}

void OximeterImport::on_syncButton_clicked()
{
	qDebug() << "Sync button clicked";
    if (oximodule == nullptr) {
        qCritical() << "OximeterImport::on_syncButton_clicked called when oximodule is null";
        return;
    }
	qDebug() << "Oximodule Start Time is " << oximodule->startTime().toString("yyyy.MM.dd HH.mm.ss") << "Duration: " << oximodule->getDuration(/* dummy */ 0 );

    ui->stackedWidget->setCurrentWidget(ui->syncPage);

    ui->syncButton->setVisible(false);
    ui->saveButton->setVisible(true);

    QDate first = p_profile->FirstDay();
    QDate last = p_profile->LastDay();

    QDate oxidate = oximodule->startTime().date();
    qDebug() << "Oximodule start date is " << oximodule->startTime().date().toString("yyyy.MM.dd");


    if ((oxidate >= first) && (oxidate <= last)) {
        // TODO: think this through better.. do I need to pick the day before?
        ui->calendarWidget->setMinimumDate(first);
        ui->calendarWidget->setMaximumDate(last);
        ui->calendarWidget->setCurrentPage(oxidate.year(), oxidate.month());
        ui->calendarWidget->setSelectedDate(oxidate);
        on_calendarWidget_clicked(oximodule->startTime().date());
    } else {
        // No CPAP sessions to sync it to.. kill the CPAP sync option
        on_calendarWidget_clicked(last);
        ui->calendarWidget->setCurrentPage(oxidate.year(), oxidate.month());
        ui->calendarWidget->setSelectedDate(oxidate);
    }

    ui->radioSyncOximeter->setChecked(true);
    on_radioSyncOximeter_clicked();

    if (importMode == IM_LIVE) {
        // Live Recording
        ui->labelSyncOximeter->setText(tr("I want to use the time my computer recorded for this live oximetry session."));
    } else if (!oximodule->isStartTimeValid()) {
        // Oximeter doesn't provide a clock
        ui->labelSyncOximeter->setText(tr("I need to set the time manually, because my oximeter doesn't have an internal clock."));
    }
}

void OximeterImport::on_saveButton_clicked()
{
	qDebug() << "Oximeter Save button clicked";
    if (!oximodule) return;

    QVector<OxiRecord> * oxirec = nullptr;

    if (!oximodule->oxisessions.contains(oximodule->startTime())) {
        QMessageBox::warning(this, STR_MessageBox_Error, tr("Something went wrong getting session data"), QMessageBox::Ok);
        reject();
        return;
    }
    oxirec = oximodule->oxisessions[oximodule->startTime()];
    if (oxirec->size() < 10)
        return;


    // this can move to SerialOximeter class process function...
    MachineInfo info = oximodule->newInfo();
    Machine * mach = p_profile->CreateMachine(info);
    SessionID sid = ui->dateTimeEdit->dateTime().toUTC().toTime_t();
    quint64 start = quint64(sid) * 1000L;


    if (!session) {
        session = new Session(mach, sid);
        session->really_set_first(start);
    } else {
        // Live recording...
        if (dummyday) {
            dummyday->removeSession(session);
            delete dummyday;
            dummyday = nullptr;
        }
        session->SetSessionID(sid);
        session->really_set_first(start);
        ELplethy->setFirst(start);
        session->setFirst(OXI_Plethy, start);
        quint64 duration = start + (ELplethy->count() * ELplethy->rate());
        quint64 end = start + duration;
        session->setLast(OXI_Plethy, end);

        session->count(OXI_Plethy);
        session->Min(OXI_Plethy);
        session->Max(OXI_Plethy);
    }
    EventList * ELpulse = nullptr;
    EventList * ELspo2 = nullptr;
    EventList * ELperf = nullptr;

    quint16 lastpulse = 0;
    quint16 lastspo2 = 0;
    quint16 lastperf = 0;
    quint16 lastgoodpulse = 0;
    quint16 lastgoodspo2 = 0;
    quint16 lastgoodperf = 0;

    bool haveperf = oximodule->havePerfIndex();

    quint64 ti = start;


    qint64 step = (importMode == IM_LIVE) ? oximodule->liveResolution() : oximodule->importResolution();
    int size = oxirec->size();

    // why was I skipping the first sample? not priming it anymore..
    for (int i=0; i < size; ++i) {
        OxiRecord * rec = &(*oxirec)[i];

        if (rec->pulse > 0) {
            if (lastpulse == 0) {
                ELpulse = session->AddEventList(OXI_Pulse, EVL_Event);
            }
            if (lastpulse != rec->pulse) {
                if (lastpulse > 0) {
                    ELpulse->AddEvent(ti, lastpulse);
                }
                ELpulse->AddEvent(ti, rec->pulse);
            }
            lastgoodpulse = rec->pulse;
        } else {
            // end section properly
            if (lastgoodpulse > 0) {
                ELpulse->AddEvent(ti, lastpulse);
                session->setLast(OXI_Pulse, ti);
                lastgoodpulse = 0;
            }
        }

        lastpulse = rec->pulse;

        if (rec->spo2 > 0) {
            if (lastspo2 == 0) {
                ELspo2 = session->AddEventList(OXI_SPO2, EVL_Event);
            }
            if (lastspo2 != rec->spo2) {
                if (lastspo2 > 0) {
                    ELspo2->AddEvent(ti, lastspo2);
                }
                ELspo2->AddEvent(ti, rec->spo2);
            }
            lastgoodspo2 = rec->spo2;
        } else {
            // end section properly
            if (lastgoodspo2 > 0) {
                ELspo2->AddEvent(ti, lastspo2);
                session->setLast(OXI_SPO2, ti);
                lastgoodspo2 = 0;
            }
        }
        lastspo2 = rec->spo2;

        if (haveperf) {
            // Perfusion Index
            if (rec->perf > 0) {
                if (lastperf == 0) {
                    ELperf = session->AddEventList(OXI_Perf, EVL_Event, 0.01f);
                }
                if (lastperf != rec->perf) {
                    if (lastperf > 0) {
                        ELperf->AddEvent(ti, lastperf);
                    }
                    ELperf->AddEvent(ti, rec->perf);
                }
                lastgoodperf = rec->perf;
            } else {
                // end section properly
                if (lastgoodperf > 0) {
                    ELperf->AddEvent(ti, lastperf);
                    session->setLast(OXI_Perf, ti);
                    lastgoodperf = 0;
                }
            }
            lastperf = rec->perf;
        }

        ti += step;
    }
    ti -= step;
    if (ELpulse && (lastpulse > 0)) {
        ELpulse->AddEvent(ti, lastpulse);
        session->setLast(OXI_Pulse, ti);
    }

    if (ELspo2 && (lastspo2 > 0)) {
        ELspo2->AddEvent(ti, lastspo2);
        session->setLast(OXI_SPO2, ti);
    }


    if (haveperf && ELperf && lastperf > 0) {
        ELperf->AddEvent(ti, lastperf);
        session->setLast(OXI_Perf, ti);
    }

    if (haveperf) {
        session->first(OXI_Perf);
        session->last(OXI_Perf);
        session->count(OXI_Perf);
        session->Min(OXI_Perf);
        session->Max(OXI_Perf);
    }

    calcSPO2Drop(session);
    calcPulseChange(session);

    mach->setModel(oximodule->getModel());
    mach->setBrand(oximodule->getVendor());

    session->first(OXI_Pulse);
    session->first(OXI_SPO2);
    session->last(OXI_Pulse);
    session->last(OXI_SPO2);

    session->first(OXI_PulseChange);
    session->first(OXI_SPO2Drop);
    session->last(OXI_PulseChange);
    session->last(OXI_SPO2Drop);

    session->cph(OXI_PulseChange);
    session->sph(OXI_PulseChange);

    session->cph(OXI_SPO2Drop);
    session->sph(OXI_SPO2Drop);

    session->count(OXI_Pulse);
    session->count(OXI_SPO2);
    session->count(OXI_PulseChange);
    session->count(OXI_SPO2Drop);
    session->Min(OXI_Pulse);
    session->Min(OXI_SPO2);
    session->Max(OXI_Pulse);
    session->Max(OXI_SPO2);

    session->really_set_last(ti);
    session->SetChanged(true);

    session->setOpened(true);

    mach->AddSession(session);
    mach->Save();
    mach->SaveSummaryCache();
    p_profile->StoreMachines();

    mainwin->getDaily()->LoadDate(mainwin->getDaily()->getDate());
    mainwin->getOverview()->ReloadGraphs();

    ELplethy = nullptr;
    session = nullptr;

    oximodule->trashRecords();
    accept();
}

void OximeterImport::chooseSession()
{
	qDebug() << "Oximeter Choose Session called";
    selecting_session = false;

    ui->stackedWidget->setCurrentWidget(ui->chooseSessionPage);
    ui->syncButton->setVisible(false);
    ui->chooseSessionButton->setVisible(true);
    QMap<QDateTime, QVector<OxiRecord> *>::iterator it;

    ui->tableOxiSessions->clearContents();
    int row = 0;
    QTableWidgetItem * item;
    QVector<OxiRecord> * oxirec;

    ui->tableOxiSessions->setRowCount(oximodule->oxisessions.size());
    ui->tableOxiSessions->setSelectionBehavior(QAbstractItemView::SelectRows);

    for (it = oximodule->oxisessions.begin(); it != oximodule->oxisessions.end(); ++it) {
        const QDateTime & key = it.key();
        oxirec = it.value();
        item = new QTableWidgetItem(key.toString(Qt::ISODate));
        ui->tableOxiSessions->setItem(row, 0, item);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);

		long int duration = oxirec->size() * oximodule->importResolution() / 1000L;
        int  h = duration / 3600;
        int  m = (duration / 60) % 60;
        int  s = duration % 60;
 
        item = new QTableWidgetItem(QString(). sprintf("%02i:%02i:%02i", h,m,s));
        ui->tableOxiSessions->setItem(row, 1, item);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);

        item = new QTableWidgetItem(tr("Oximeter Session %1").arg(row+1, 0));
        ui->tableOxiSessions->setItem(row, 2, item);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);

        row++;
    }

    ui->tableOxiSessions->selectRow(0);
}

void OximeterImport::on_chooseSessionButton_clicked()
{
	qDebug() << "Chosen session clicked";
    ui->chooseSessionButton->setVisible(false);

    QTableWidgetItem * item_0 = ui->tableOxiSessions->item(ui->tableOxiSessions->currentRow(),0);
    QTableWidgetItem * item_1 = ui->tableOxiSessions->item(ui->tableOxiSessions->currentRow(),1);

    if (!item_0 || !item_1) return;
    QDateTime datetime = item_0->data(Qt::DisplayRole).toDateTime();
    oximodule->setStartTime(datetime);
    oximodule->setDuration(item_1->data(Qt::DisplayRole).toInt());

    if (selecting_session) {
        ui->stackedWidget->setCurrentWidget(ui->directImportPage);
        chosen_sessions.push_back(ui->tableOxiSessions->currentRow());

        // go back and start import
        doImport();
    } else {
        on_syncButton_clicked();
    }
}

void OximeterImport::setInformation()
{
    QString html="<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">"
    "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">"
    "p, li { white-space: pre-wrap; }"
    "</style></head><body style=\" font-family:'.Lucida Grande UI'; font-size:13pt; font-weight:400; font-style:normal;\">"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:18pt;\">"
            +tr("Welcome to the Oximeter Import Wizard")+"</span></p>"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
            +tr("Pulse Oximeters are medical devices used to measure blood oxygen saturation. During extended Apnea events and abnormal breathing patterns, blood oxygen saturation levels can drop significantly, and can indicate issues that need medical attention.")+"</p>"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
            +tr("SleepyHead gives you the ability to track Oximetry data alongside CPAP session data, which can give valuable insight into the effectiveness of CPAP treatment. It will also work standalone with your Pulse Oximeter, allowing you to store, track and review your recorded data.")+"</p>"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
            +tr("SleepyHead is currently compatible with Contec CMS50D+, CMS50E, CMS50F and CMS50I serial oximeters.<br/>(Note: Direct importing from bluetooth models is <span style=\" font-weight:600;\">probaby not</span> possible yet)")+"</p>"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
        +tr("You may wish to note, other companies, such as Pulox, simply rebadge Contec CMS50's under new names, such as the Pulox PO-200, PO-300, PO-400. These should also work.")+"</p>"

    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
            +tr("It also can read from ChoiceMMed MD300W1 oximeter .dat files.")+"</p>"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
            "<span style=\" font-weight:600;\">"+tr("Please remember:")+" </span>"
            "<span style=\" font-weight:600; font-style:italic;\">"
            +tr("If you are trying to sync oximetry and CPAP data, please make sure you imported your CPAP sessions first before proceeding!")+"</span></p>"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
            "<span style=\" font-weight:600;\">"+tr("Important Notes:")+" </span>"
            +tr("For SleepyHead to be able to locate and read directly from your Oximeter device, you need to ensure the correct device drivers (eg. USB to Serial UART) have been installed on your computer. For more information about this, %1click here%2.").arg("<a href=\"http://www.silabs.com/products/mcu/pages/usbtouartbridgevcpdrivers.aspx\"><span style=\" text-decoration: underline; color:#0000ff;\">").arg("</span></a>")+"</p>"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
            +tr("Contec CMS50D+ devices do not have an internal clock, and do not record a starting time. If you do not have a CPAP session to link a recording to, you will have to enter the start time manually after the import process is completed.")+"</p>"
    "<p style=\" margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"
            +tr("Even for devices with an internal clock, it is still recommended to get into the habit of starting oximeter records at the same time as CPAP sessions, because CPAP internal clocks tend to drift over time, and not all can be reset easily.")+"</p></body></html>";
    ui->textBrowser->setHtml(html);
    ui->textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->textBrowser->setOpenExternalLinks(true);
}

void OximeterImport::on_oximeterType_currentIndexChanged(int index)
{
    switch (index) {
    case 0: //New CMS50's
        ui->directImportButton->setEnabled(true);
        ui->liveImportButton->setEnabled(false);
        ui->fileImportButton->setEnabled(true);
        ui->oldCMS50specific->setVisible(false);
        ui->newCMS50settingsPanel->setVisible(true);
        break;
    case 1:
        ui->directImportButton->setEnabled(true);
        ui->liveImportButton->setEnabled(true);
        ui->fileImportButton->setEnabled(true);
        ui->oldCMS50specific->setVisible(true);
        ui->newCMS50settingsPanel->setVisible(false);
        break;
    default: // ChoiceMMed oximeters, and others?
        ui->directImportButton->setEnabled(false);
        ui->liveImportButton->setEnabled(false);
        ui->fileImportButton->setEnabled(true);
        ui->oldCMS50specific->setVisible(false);
        ui->newCMS50settingsPanel->setVisible(false);
    }
    p_profile->oxi->setOximeterType(index);

}

void OximeterImport::on_cms50CheckName_clicked(bool checked)
{
    ui->cms50DeviceName->setEnabled(checked);
    if (checked) {
        ui->cms50DeviceName->setFocus();
        ui->cms50DeviceName->setCursorPosition(0);
    }
}

void OximeterImport::on_cms50SyncTime_clicked(bool checked)
{
    p_profile->oxi->setSyncOximeterClock(checked);
}

void OximeterImport::on_cms50DeviceName_textEdited(const QString &arg1)
{
    p_profile->oxi->setDefaultDevice(arg1);
}

void OximeterImport::on_textBrowser_anchorClicked(const QUrl &arg1)
{
    QDesktopServices::openUrl(arg1);
}
