/* SleepyHead MainWindow Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QHostInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QResource>
#include <QProgressBar>
#include <QTimer>
#include <QSettings>
#include <QPixmap>
#include <QDesktopWidget>
#include <QListView>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QProcess>
#include <QFontMetrics>
#include <QTextDocument>
#include <QTranslator>
#include <QPushButton>
#include <QCalendarWidget>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QScreen>
#include <cmath>

#include "common_gui.h"
#include "version.h"


// Custom loaders that don't autoscan..
#include <SleepLib/loader_plugins/zeo_loader.h>
#include <SleepLib/loader_plugins/somnopose_loader.h>

#ifndef REMSTAR_M_SUPPORT
#include <SleepLib/loader_plugins/mseries_loader.h>
#endif

#include "logger.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "newprofile.h"
#include "exportcsv.h"
#include "SleepLib/schema.h"
#include "Graphs/glcommon.h"
#include "UpdaterWindow.h"
#include "SleepLib/calcs.h"
#include "SleepLib/progressdialog.h"
#include "version.h"

#include "reports.h"
#include "statistics.h"

#if QT_VERSION >= QT_VERSION_CHECK(5,4,0)
#include <QOpenGLFunctions>
#endif

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->logText->setPlainText("00000: Startup: SleepyHead Logger initialized");

    if (logger) {
        connect(logger, SIGNAL(outputLog(QString)), this, SLOT(logMessage(QString)));
    }

    // Initialise sleepyHead app registry stuff
    QSettings settings;

    // Load previous Window geometry (this is currently broken on Mac as of Qt5.2.1)
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());


    // Nifty Notification popups in System Tray (uses Growl on Mac)
    if (QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages()) {
        systray = new QSystemTrayIcon(QIcon(":/icons/bob-v3.0.png"), this);
        systray->show();
        systraymenu = new QMenu(this);
        systray->setContextMenu(systraymenu);
        QAction *a = systraymenu->addAction(STR_TR_SleepyHead + " v" + VersionString);
        a->setEnabled(false);
        systraymenu->addSeparator();
        systraymenu->addAction(tr("&About"), this, SLOT(on_action_About_triggered()));
        systraymenu->addAction(tr("Check for &Updates"), this, SLOT(on_actionCheck_for_Updates_triggered()));
        systraymenu->addSeparator();
        systraymenu->addAction(tr("E&xit"), this, SLOT(close()));
    } else { // if not available, the messages will popup in the taskbar
        systray = nullptr;
        systraymenu = nullptr;
    }

}

void MainWindow::SetupGUI()
{
    QString version = getBranchVersion();
    setWindowTitle(STR_TR_SleepyHead + QString(" %1").arg(version));

#ifdef Q_OS_MAC

    ui->action_About->setMenuRole(QAction::ApplicationSpecificRole);
    ui->action_Preferences->setMenuRole(QAction::ApplicationSpecificRole);
    ui->action_Exit->setMenuRole(QAction::ApplicationSpecificRole);
#endif

    ui->actionToggle_Line_Cursor->setChecked(AppSetting->lineCursorMode());
    ui->actionDebug->setChecked(AppSetting->showDebug());
    ui->actionShow_Performance_Counters->setChecked(AppSetting->showPerformance());

    overview = nullptr;
    daily = nullptr;
    prefdialog = nullptr;
    profileSelector = nullptr;
    welcome = nullptr;

    ui->oximetryButton->setDisabled(true);
    ui->dailyButton->setDisabled(true);
    ui->overviewButton->setDisabled(true);
    ui->statisticsButton->setDisabled(true);
    ui->importButton->setDisabled(true);

    m_inRecalculation = false;
    m_restartRequired = false;
    // Initialize Status Bar objects

    QTextCharFormat format = ui->statStartDate->calendarWidget()->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(Qt::black, Qt::SolidPattern));
    Qt::DayOfWeek dow=firstDayOfWeekFromLocale();
    ui->statStartDate->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->statStartDate->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    ui->statStartDate->calendarWidget()->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    ui->statStartDate->calendarWidget()->setFirstDayOfWeek(dow);
    ui->statEndDate->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->statEndDate->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    ui->statEndDate->calendarWidget()->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    ui->statEndDate->calendarWidget()->setFirstDayOfWeek(dow);

    ui->statEndDate->setVisible(false);
    ui->statStartDate->setVisible(false);

    ui->reportModeRange->setVisible(false);
    int srm = 0;
    if (p_profile) {
        srm = p_profile->general->statReportMode();
    }

    switch(srm) {
        case 0:
            ui->reportModeStandard->setChecked(true);
            break;
        case 1:
            ui->reportModeMonthly->setChecked(true);
            break;
        case 2:
            ui->reportModeRange->setChecked(true);
            ui->statEndDate->setVisible(true);
            ui->statStartDate->setVisible(true);
            break;
        default:
            if (p_profile) {
                p_profile->general->setStatReportMode(0);
            }
            break;
    }
    if (!AppSetting->showDebug()) {
        ui->logText->hide();
    }

#ifdef Q_OS_MAC
    //p_profile->appearance->setAntiAliasing(false);
#endif
    //ui->action_Link_Graph_Groups->setChecked(p_profile->general->linkGroups());

    first_load = true;

    profileSelector = new ProfileSelector(ui->tabWidget);
    ui->tabWidget->insertTab(0, profileSelector, STR_TR_Profile);

    // Profiles haven't been loaded here...
    profileSelector->updateProfileList();

    // Start with the new Profile Tab
    ui->tabWidget->setCurrentWidget(profileSelector); // setting this to daily shows the cube during loading..

    ui->toolBox->setCurrentIndex(0);
    bool b = AppSetting->rightSidebarVisible();
    ui->action_Sidebar_Toggle->setChecked(b);
    ui->toolBox->setVisible(b);

    on_tabWidget_currentChanged(0);

#ifndef REMSTAR_M_SUPPORT
    ui->actionImport_RemStar_MSeries_Data->setVisible(false);
#endif

    qsrand(QDateTime::currentDateTime().toTime_t());

    QList<int> a;

    int panel_width = AppSetting->rightPanelWidth();
    a.push_back(this->width() - panel_width);
    a.push_back(panel_width);
    ui->mainsplitter->setSizes(a);
    ui->mainsplitter->setStretchFactor(0,1);
    ui->mainsplitter->setStretchFactor(1,0);

    QTimer::singleShot(50, this, SLOT(Startup()));

    help = new Help(this);

    ui->tabWidget->addTab(help, tr("Help Browser"));
}

void MainWindow::logMessage(QString msg)
{
    ui->logText->appendPlainText(msg);
}

void MainWindow::closeEvent(QCloseEvent * event)
{
    Q_UNUSED(event);
    static bool runonce = false;
    if (!runonce) {
        if (AppSetting->removeCardReminder()) {
            Notify(QObject::tr("Don't forget to place your datacard back in your CPAP machine"), QObject::tr("SleepyHead Reminder"));
            QThread::msleep(1000);
            QApplication::processEvents();
        }
        schema::channel.Save();
        if (p_profile) {
            CloseProfile();
        }

        // Shutdown and Save the current User profile
        Profiles::Done();

        // Save current window position
        QSettings settings;
        settings.setValue("MainWindow/geometry", saveGeometry());

        // Trash anything allocated by the Graph objects
        DestroyGraphGlobals();

        if (systraymenu) delete systraymenu;

        disconnect(logger, SIGNAL(outputLog(QString)), this, SLOT(logMessage(QString)));
        shutdownLogger();

        runonce = true;
    } else {
        qDebug() << "Qt is still calling closevent multiple times";
        QApplication::processEvents();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::log(QString text)
{
    logger->appendClean(text);
}


void MainWindow::Notify(QString s, QString title, int ms)
{
    if (title.isEmpty()) {
        title = tr("%1 %2").arg(STR_TR_SleepyHead).arg(STR_TR_AppVersion);
    }
    if (systray) {
        QString msg = s;

#ifdef Q_OS_UNIX
        // GNOME3's systray hides the last line of the displayed Qt message.
        // As a workaround, add an extra line to bump the message back
        // into the visible area.
        char *desktop = getenv("DESKTOP_SESSION");

        if (desktop && !strncmp(desktop, "gnome", 5)) {
            msg += "\n";
        }
#endif

        systray->showMessage(title, msg, QSystemTrayIcon::Information, ms);
    }
}

QString getCPAPPixmap(QString mach_class)
{
    QString cpapimage;
    if (mach_class == STR_MACH_ResMed) cpapimage = ":/icons/rms9.png";
    else if (mach_class == STR_MACH_PRS1) cpapimage = ":/icons/prs1.png";
    else if (mach_class == STR_MACH_Intellipap) cpapimage = ":/icons/intellipap.png";
    return cpapimage;
}

//QIcon getCPAPIcon(QString mach_class)
//{
//    QString cpapimage = getCPAPPixmap(mach_class);

//    return QIcon(cpapimage);
//}

void MainWindow::PopulatePurgeMenu()
{
    ui->menu_Rebuild_CPAP_Data->disconnect(ui->menu_Rebuild_CPAP_Data, SIGNAL(triggered(QAction*)), this, SLOT(on_actionRebuildCPAP(QAction *)));
    ui->menu_Rebuild_CPAP_Data->clear();

    ui->menuPurge_CPAP_Data->disconnect(ui->menuPurge_CPAP_Data, SIGNAL(triggered(QAction*)), this, SLOT(on_actionPurgeMachine(QAction *)));
    ui->menuPurge_CPAP_Data->clear();

    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    for (int i=0; i < machines.size(); ++i) {
        Machine *mach = machines.at(i);
        QString name =  mach->brand() + " "+
                        mach->model() + " "+
                        mach->serial();

        QAction * action = new QAction(name.replace("&","&&"), ui->menu_Rebuild_CPAP_Data);
        action->setIconVisibleInMenu(true);
        action->setIcon(mach->getPixmap());
        action->setData(mach->loaderName()+":"+mach->serial());
        ui->menu_Rebuild_CPAP_Data->addAction(action);

        action = new QAction(name.replace("&","&&"), ui->menuPurge_CPAP_Data);
        action->setIconVisibleInMenu(true);
        action->setIcon(mach->getPixmap()); //getCPAPIcon(mach->loaderName()));
        action->setData(mach->loaderName()+":"+mach->serial());

        ui->menuPurge_CPAP_Data->addAction(action);
    }
    ui->menu_Rebuild_CPAP_Data->connect(ui->menu_Rebuild_CPAP_Data, SIGNAL(triggered(QAction*)), this, SLOT(on_actionRebuildCPAP(QAction*)));
    ui->menuPurge_CPAP_Data->connect(ui->menuPurge_CPAP_Data, SIGNAL(triggered(QAction*)), this, SLOT(on_actionPurgeMachine(QAction*)));
}

void MainWindow::firstRunMessage()
{
    if (AppSetting->showAboutDialog() > 0) {
        AboutDialog * about = new AboutDialog(this);

        about->exec();

        AppSetting->setShowAboutDialog(-1);

        about->deleteLater();
    }
}

QString GenerateWelcomeHTML();

bool MainWindow::OpenProfile(QString profileName, bool skippassword)
{
    qDebug() << "Opening profile" << profileName;

    auto pit = Profiles::profiles.find(profileName);
    if (pit == Profiles::profiles.end()) return false;

    Profile * prof = pit.value();
    if (p_profile) {
        if ((prof != p_profile)) {
            CloseProfile();
        } else {
            // Already open
            return false;
        }
    }
    prof = profileSelector->SelectProfile(profileName, skippassword);  // asks for the password and updates stuff in profileSelector tab
    if (!prof) {
        return false;
    }
    // TODO: Check profile password

    // Check Lockfile
    QString lockhost = prof->checkLock();

    if (!lockhost.isEmpty()) {
        if (lockhost.compare(QHostInfo::localHostName()) != 0) {
            if (QMessageBox::warning(nullptr, STR_MessageBox_Warning,
                                  QObject::tr("There is a lockfile already present for this profile '%1', claimed on '%2'.").arg(prof->user->userName()).arg(lockhost)+"\n\n"+
                                  QObject::tr("You can only work with one instance of an individual SleepyHead profile at a time.")+"\n\n"+
                                  QObject::tr("If you are using cloud storage, make sure SleepyHead is closed and syncing has completed first on the other computer before proceeding."),
                                  QMessageBox::Cancel |QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel) {
                return false;
            }
        } // not worried about localhost locks anymore, just silently drop it.

        prof->removeLock();
    }

    p_profile = prof;

    ProgressDialog * progress = new ProgressDialog(this);

    progress->setMessage(QObject::tr("Loading profile \"%1\"...").arg(profileName));
    progress->open();

#ifdef LOCK_RESMED_SESSIONS
    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    for (QList<Machine *>::iterator it = machines.begin(); it != machines.end(); ++it) {
        QString mclass=(*it)->loaderName();
        if (mclass == STR_MACH_ResMed) {
            qDebug() << "ResMed machine found.. locking SleepyHead preferences to suit it's summary system";

            // Have to sacrifice these features to get access to summary data.
            p_profile->session->setCombineCloseSessions(0);
            p_profile->session->setDaySplitTime(QTime(12,0,0));
            p_profile->session->setIgnoreShortSessions(false);
            p_profile->session->setLockSummarySessions(true);
            p_profile->general->setPrefCalcPercentile(95.0);    // 95%
            p_profile->general->setPrefCalcMiddle(0);           // Median (50%)
            p_profile->general->setPrefCalcMax(1);              // Dodgy max

            break;
        }
    }
#endif

    // Todo: move this to AHIWIndow check to profile Load function...
    if (p_profile->cpap->AHIWindow() < 30.0) {
        p_profile->cpap->setAHIWindow(60.0);
    }


    if (p_profile->p_preferences[STR_PREF_ReimportBackup].toBool()) {
        importCPAPBackups();
        p_profile->p_preferences[STR_PREF_ReimportBackup]=false;
    }

    QList<MachineLoader *> loaders = GetLoaders();
    for (int i=0; i<loaders.size(); ++i) {
        connect(loaders.at(i), SIGNAL(machineUnsupported(Machine*)), this, SLOT(MachineUnsupported(Machine*)));
    }

    p_profile->LoadMachineData(progress);
    progress->setMessage(tr("Loading profile \"%1\"").arg(profileName));

    // Show the sheep?
//    QPixmap sheep=QPixmap(":/docs/sheep.png").scaled(64,64);
//    progress->setPixmap(sheep);

    QApplication::processEvents();

    ui->statStartDate->setDate(p_profile->FirstDay());
    ui->statEndDate->setDate(p_profile->LastDay());

    // Reload everything profile related
    if (daily) {
        qCritical() << "OpenProfile called with active Daily object!";
        return false;
    }
    welcome = new Welcome(ui->tabWidget);
    ui->tabWidget->insertTab(1, welcome, tr("Welcome"));

    daily = new Daily(ui->tabWidget, nullptr);
    ui->tabWidget->insertTab(2, daily, STR_TR_Daily);
    daily->ReloadGraphs();

    if (overview) {
        qCritical() << "OpenProfile called with active Overview object!";

        return false;
    }
    overview = new Overview(ui->tabWidget, daily->graphView());
    ui->tabWidget->insertTab(3, overview, STR_TR_Overview);
    overview->ReloadGraphs();

    // Should really create welcome and statistics here, but they need redoing later anyway to kill off webkit
    ui->tabWidget->setCurrentIndex(AppSetting->openTabAtStart());
    GenerateStatistics();
    PopulatePurgeMenu();

    AppSetting->setProfileName(p_profile->user->userName());
    setWindowTitle(STR_TR_SleepyHead + QString(" %1 (" + tr("Profile") + ": %2)").arg(getBranchVersion()).arg(AppSetting->profileName()));

    ui->oximetryButton->setDisabled(false);
    ui->dailyButton->setDisabled(false);
    ui->overviewButton->setDisabled(false);
    ui->statisticsButton->setDisabled(false);
    ui->importButton->setDisabled(false);

    progress->close();
    delete progress;

    return true;
}

void MainWindow::CloseProfile()
{
    if (daily) {
        daily->Unload();
        daily->clearLastDay(); // otherwise Daily will crash
        delete daily;
        daily = nullptr;
    }
    if (welcome) {
        delete welcome;
        welcome = nullptr;
    }
    if (overview) {
        delete overview;
        overview = nullptr;
    }

    if (p_profile) {
        p_profile->StoreMachines();
        p_profile->UnloadMachineData();
        p_profile->saveChannels();
        p_profile->Save();
        p_profile->removeLock();
        p_profile = nullptr;
    }
}


void MainWindow::Startup()
{
    for (auto & loader : GetLoaders()) {
        loader->setParent(this);
    }
    QString lastProfile = AppSetting->profileName();

    firstRunMessage();

    if (Profiles::profiles.contains(lastProfile) && AppSetting->autoOpenLastUsed()) {
        if (OpenProfile(lastProfile)) {
            ui->tabWidget->setCurrentIndex(AppSetting->openTabAtStart());

            if (AppSetting->autoLaunchImport()) {
                on_importButton_clicked();
            }
        }
    } else {
        ui->tabWidget->setCurrentWidget(profileSelector);
    }
}

int MainWindow::importCPAP(ImportPath import, const QString &message)
{
    if (!import.loader) {
        return 0;
    }

    ui->tabWidget->setCurrentWidget(welcome);
    QApplication::processEvents();
    ProgressDialog * progdlg = new ProgressDialog(this);

    QPixmap image = import.loader->getPixmap(import.loader->PeekInfo(import.path).series);
    image = image.scaled(64,64);
    progdlg->setPixmap(image);

    progdlg->addAbortButton();

    progdlg->setWindowModality(Qt::ApplicationModal);
    progdlg->open();
    progdlg->setMessage(message);


    connect(import.loader, SIGNAL(updateMessage(QString)), progdlg, SLOT(setMessage(QString)));
    connect(import.loader, SIGNAL(setProgressMax(int)), progdlg, SLOT(setProgressMax(int)));
    connect(import.loader, SIGNAL(setProgressValue(int)), progdlg, SLOT(setProgressValue(int)));
    connect(progdlg, SIGNAL(abortClicked()), import.loader, SLOT(abortImport()));

    int c = import.loader->Open(import.path);

    if (c > 0) {
        Notify(tr("Imported %1 CPAP session(s) from\n\n%2").arg(c).arg(import.path), tr("Import Success"));
    } else if (c == 0) {
        Notify(tr("Already up to date with CPAP data at\n\n%1").arg(import.path), tr("Up to date"));
    } else {
        Notify(tr("Couldn't find any valid Machine Data at\n\n%1").arg(import.path),tr("Import Problem"));
    }
    disconnect(progdlg, SIGNAL(abortClicked()), import.loader, SLOT(abortImport()));
    disconnect(import.loader, SIGNAL(setProgressMax(int)), progdlg, SLOT(setProgressMax(int)));
    disconnect(import.loader, SIGNAL(setProgressValue(int)), progdlg, SLOT(setProgressValue(int)));
    disconnect(import.loader, SIGNAL(updateMessage(QString)), progdlg, SLOT(setMessage(QString)));

    progdlg->close();

    delete progdlg;

    if (AppSetting->openTabAfterImport()>0) {
        ui->tabWidget->setCurrentIndex(AppSetting->openTabAfterImport());
    }

    return c;
}

void MainWindow::finishCPAPImport()
{
    p_profile->StoreMachines();
    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    for (Machine * mach : machines) {
        mach->saveSessionInfo();
        mach->SaveSummaryCache();
    }

    GenerateStatistics();
    profileSelector->updateProfileList();

    welcome->refreshPage();

    if (overview) { overview->ReloadGraphs(); }
    if (daily) {
//        daily->populateSessionWidget();
        daily->ReloadGraphs();
    }
    if (AppSetting->openTabAfterImport()>0) {
        ui->tabWidget->setCurrentIndex(AppSetting->openTabAfterImport());
    }

}

void MainWindow::importCPAPBackups()
{
    // Get BackupPaths for all CPAP machines
    QList<Machine *> machlist = p_profile->GetMachines(MT_CPAP);
    QList<ImportPath> paths;
    Q_FOREACH(Machine *m, machlist) {
        paths.append(ImportPath(m->getBackupPath(), lookupLoader(m)));
    }

    if (paths.size() > 0) {
        int c=0;
        Q_FOREACH(ImportPath path, paths) {
            c+=importCPAP(path, tr("Please wait, importing from backup folder(s)..."));
        }
        if (c>0) {
            finishCPAPImport();
        }
    }
}

#ifdef Q_OS_UNIX
# include <stdio.h>
# include <unistd.h>

# if defined(Q_OS_MAC) || defined(Q_OS_BSD4)
#  include <sys/mount.h>
# elif defined(Q_OS_HAIKU)
// nothing needed
# else
#  include <sys/statfs.h>
#  include <mntent.h>
# endif // Q_OS_MAC/BSD

#endif // Q_OS_UNIX

//! \brief Returns a list of drive mountpoints
QStringList getDriveList()
{
    QStringList drivelist;

#if defined(Q_OS_MAC) || defined(Q_OS_BSD4)
    struct statfs *mounts;
    int num_mounts = getmntinfo(&mounts, MNT_WAIT);
    if (num_mounts >= 0) {
        for (int i = 0; i < num_mounts; i++) {
            QString name = mounts[i].f_mntonname;

            // Only interested in drives mounted under /Volumes
            if (name.toLower().startsWith("/volumes/")) {
                drivelist.push_back(name);
//                qDebug() << QString("Disk type '%1' mounted at: %2").arg(mounts[i].f_fstypename).arg(mounts[i].f_mntonname);
            }
        }
    }

#elif defined(Q_OS_UNIX) && !defined(Q_OS_HAIKU)
    // Unix / Linux (except BSD)
    FILE *mtab = setmntent("/etc/mtab", "r");
    struct mntent *m;
    struct mntent mnt;
    char strings[4096];

    // NOTE: getmntent_r is a GNU extension, requiring glibc.
    while ((m = getmntent_r(mtab, &mnt, strings, sizeof(strings)))) {

        struct statfs fs;
        if ((mnt.mnt_dir != NULL) && (statfs(mnt.mnt_dir, &fs) == 0)) {
            QString name = mnt.mnt_dir;
            quint64 size = fs.f_blocks * fs.f_bsize;

            if (size > 0) { // this should theoretically ignore /dev, /proc, /sys etc..
                drivelist.push_back(name);
            }
//            quint64 free = fs.f_bfree * fs.f_bsize;
//            quint64 avail = fs.f_bavail * fs.f_bsize;
        }
    }
    endmntent(mtab);

#elif defined(Q_OS_WIN) || defined(Q_OS_HAIKU)
    QFileInfoList list = QDir::drives();

    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        QString name = fileInfo.filePath();
        if (name.at(0).toUpper() != QChar('C')) { // Ignore the C drive
            drivelist.push_back(name);
        }
    }

#endif
    return drivelist;
}

extern MainWindow * mainwin;
void ImportDialogScan::cancelbutton()
{
    mainwin->importScanCancelled = true;
    hide();
}

QList<ImportPath> MainWindow::detectCPAPCards()
{
    const int timeout = 20000;

    QList<ImportPath> detectedCards;

    QList<MachineLoader *>loaders = GetLoaders(MT_CPAP);
    QTime time;
    time.start();

    // Create dialog
    ImportDialogScan popup(this) ;//, Qt::SplashScreen);
    QLabel waitmsg(tr("Please insert your CPAP data card..."));
    QProgressBar progress;
    QVBoxLayout waitlayout;
    popup.setLayout(&waitlayout);

    QHBoxLayout layout2;
    QIcon icon("://icons/sdcard.png");
    QPushButton skipbtn(icon, tr("Choose a folder"));
    QPushButton cancelbtn(STR_MessageBox_Cancel);
    skipbtn.setMinimumHeight(40);
    cancelbtn.setMinimumHeight(40);
    waitlayout.addWidget(&waitmsg,1,Qt::AlignCenter);
    waitlayout.addWidget(&progress,1);
    waitlayout.addLayout(&layout2,1);
    layout2.addWidget(&skipbtn);
    layout2.addWidget(&cancelbtn);
    popup.connect(&skipbtn, SIGNAL(clicked()), &popup, SLOT(hide()));
    popup.connect(&cancelbtn, SIGNAL(clicked()), &popup, SLOT(cancelbutton()));
    progress.setValue(0);
    progress.setMaximum(timeout);
    progress.setVisible(true);
    importScanCancelled = false;
    popup.show();
    QApplication::processEvents();
    QString lastpath = (*p_profile)[STR_PREF_LastCPAPPath].toString();

    do {
        // Rescan in case card inserted
        QStringList AutoScannerPaths = getDriveList();
        //AutoScannerPaths.push_back(lastpath);

        if (!AutoScannerPaths.contains(lastpath)) {
            AutoScannerPaths.append(lastpath);
        }

        Q_FOREACH(const QString &path, AutoScannerPaths) {
            // Scan through available machine loaders and test if this folder contains valid folder structure
            Q_FOREACH(MachineLoader * loader, loaders) {
                if (loader->Detect(path)) {
                    detectedCards.append(ImportPath(path, loader));

                    qDebug() << "Found" << loader->loaderName() << "datacard at" << path;
                }
                QApplication::processEvents();
            }
        }
        int el=time.elapsed();
        progress.setValue(el);
        if (el > timeout) break;
        if (!popup.isVisible()) break;
        // needs a slight delay here
        for (int i=0; i<5; ++i) {
            QThread::msleep(50);
            QApplication::processEvents();
        }
    } while (detectedCards.size() == 0);

    popup.hide();
    popup.disconnect(&skipbtn, SIGNAL(clicked()), &popup, SLOT(hide()));
    popup.disconnect(&cancelbtn, SIGNAL(clicked()), &popup, SLOT(hide()));

    return detectedCards;
}


void MainWindow::on_action_Import_Data_triggered()
{
    static bool in_import = false;
    if (m_inRecalculation) {
        Notify(tr("Access to Import has been blocked while recalculations are in progress."),STR_MessageBox_Busy);
        return;
    }
    if (in_import) {
        Notify(tr("Import is already running in the background."),STR_MessageBox_Busy);
        return;
    }
    in_import=true;

    ui->tabWidget->setCurrentWidget(welcome);
    QApplication::processEvents();

    QList<ImportPath> datacards = detectCPAPCards();

    if (importScanCancelled) {
        in_import = false;
        return;
    }

    QList<MachineLoader *>loaders = GetLoaders(MT_CPAP);

    QTime time;
    time.start();


    bool asknew = false;

    if (datacards.size() > 0) {
        MachineInfo info = datacards[0].loader->PeekInfo(datacards[0].path);
        QString infostr;
        if (!info.model.isEmpty()) {
            QString infostr2 = info.model+" ("+info.serial+")";
            infostr = tr("A %1 file structure for a %2 was located at:").arg(info.brand).arg(infostr2);
        } else {
            infostr = tr("A %1 file structure was located at:").arg(datacards[0].loader->loaderName());
        }

        if (!p_profile->cpap->autoImport()) {
            QMessageBox mbox(QMessageBox::NoIcon,
                             tr("CPAP Data Located"),
                             infostr+"\n\n"+QDir::toNativeSeparators(datacards[0].path)+"\n\n"+
                             tr("Would you like to import from this location?"),
                             QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                             this);
            mbox.setDefaultButton(QMessageBox::Yes);
            mbox.setButtonText(QMessageBox::No, tr("Specify"));

            QPixmap pixmap = datacards[0].loader->getPixmap(datacards[0].loader->PeekInfo(datacards[0].path).series).scaled(64,64);

            //QPixmap pixmap = QPixmap(getCPAPPixmap(datacards[0].loader->loaderName())).scaled(64,64);
            mbox.setIconPixmap(pixmap);
            int res = mbox.exec();

            if (res == QMessageBox::Cancel) {
                // Give the communal progress bar back
                in_import=false;
                return;
            } else if (res == QMessageBox::No) {
                //waitmsg->setText(tr("Please wait, launching file dialog..."));
                datacards.clear();
                asknew = true;
            }
        }
    } else {
        //waitmsg->setText(tr("No CPAP data card detected, launching file dialog..."));
        asknew = true;
    }

    if (asknew) {
       // popup.show();
        mainwin->Notify(tr("Please remember to point the importer at the root folder or drive letter of your data-card, and not a subfolder."),
                        tr("Import Reminder"),8000);

        QFileDialog w(this);

        QString folder;
        if (p_profile->contains(STR_PREF_LastCPAPPath)) {
            folder = (*p_profile)[STR_PREF_LastCPAPPath].toString();
        } else {
            folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        }

        w.setDirectory(folder);
        w.setFileMode(QFileDialog::Directory);
        w.setOption(QFileDialog::ShowDirsOnly, true);

        // This doesn't work on WinXP

#if defined(Q_OS_MAC)
        w.setOption(QFileDialog::DontUseNativeDialog,false);

#elif defined(Q_OS_UNIX)
        w.setOption(QFileDialog::DontUseNativeDialog,false);
#elif defined(Q_OS_WIN)
        // check the Os version.. winxp chokes
        w.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
//#else
//        w.setOption(QFileDialog::DontUseNativeDialog, false);

//        QListView *l = w.findChild<QListView *>("listView");
//        if (l) {
//            l->setSelectionMode(QAbstractItemView::MultiSelection);
//        }

//        QTreeView *t = w.findChild<QTreeView *>();
//        if (t) {
//            t->setSelectionMode(QAbstractItemView::MultiSelection);
//        }

//#endif

        if (w.exec() != QDialog::Accepted) {
            in_import=false;

            return;
        }


        for (int i = 0; i < w.selectedFiles().size(); i++) {
            Q_FOREACH(MachineLoader * loader, loaders) {
                if (loader->Detect(w.selectedFiles().at(i))) {
                    datacards.append(ImportPath(w.selectedFiles().at(i), loader));
                    break;
                }
            }
        }
    }

    bool newdata = false;

//    QStringList goodlocations;

    ProgressDialog * prog = new ProgressDialog(this);
    prog->setMessage(tr("Processing import list..."));
    prog->addAbortButton();
    prog->setWindowModality(Qt::ApplicationModal);

    prog->open();

    int c = -1;
    for (int i = 0; i < datacards.size(); i++) {
        QString dir = datacards[i].path;
        MachineLoader * loader = datacards[i].loader;
        if (!loader) continue;
        connect(loader, SIGNAL(updateMessage(QString)), prog, SLOT(setMessage(QString)));
        connect(loader, SIGNAL(setProgressMax(int)), prog, SLOT(setProgressMax(int)));
        connect(loader, SIGNAL(setProgressValue(int)), prog, SLOT(setProgressValue(int)));
        connect(prog, SIGNAL(abortClicked()), loader, SLOT(abortImport()));

        QPixmap image = loader->getPixmap(loader->PeekInfo(dir).series);
        image = image.scaled(64,64);
        prog->setPixmap(image);

        if (!dir.isEmpty()) {
            c = importCPAP(datacards[i], tr("Importing Data"));
            qDebug() << "Finished Importing data" << c;

            if (c >= 0) {
            //    goodlocations.push_back(dir);
                QDir d(dir.section("/",0,-1));
                (*p_profile)[STR_PREF_LastCPAPPath] = d.absolutePath();
            }

            if (c > 0) {
                newdata = true;
            }
        }
        disconnect(prog, SIGNAL(abortClicked()), loader, SLOT(abortImport()));
        disconnect(loader, SIGNAL(setProgressMax(int)), prog, SLOT(setProgressMax(int)));
        disconnect(loader, SIGNAL(setProgressValue(int)), prog, SLOT(setProgressValue(int)));
        disconnect(loader, SIGNAL(updateMessage(QString)), prog, SLOT(setMessage(QString)));
    }

    if (newdata)  {
        finishCPAPImport();
        PopulatePurgeMenu();
    }

    prog->close();
    prog->deleteLater();
    in_import=false;

}

QMenu *MainWindow::CreateMenu(QString title)
{
    QMenu *menu = new QMenu(title, ui->menubar);
    ui->menubar->insertMenu(ui->menu_Help->menuAction(), menu);
    return menu;
}

void MainWindow::on_action_Fullscreen_triggered()
{
    if (ui->action_Fullscreen->isChecked()) {
        this->showFullScreen();
    } else {
        this->showNormal();
    }
}

void MainWindow::setRecBoxHTML(QString html)
{
    ui->recordsBox->setHtml(html);
}

QString MainWindow::getWelcomeHTML()
{
    // This is messy, but allows it to be translated easier
    return "<html>\n<head>"
           " <style type='text/css'>"
           "  <!--h1,p,a,td,body { font-family: 'FreeSans', 'Sans Serif' } --/>"
           "  p,a,td,body { font-size: 14px }"
           "  a:link,a:visited { color: \"#000020\"; text-decoration: none; font-weight: bold;}"
           "  a:hover { background-color: inherit; color: red; text-decoration:none; font-weight: bold; }"
           " </style>\n"
           "</head>"
           "<body leftmargin=0 topmargin=0 rightmargin=0>"
           "<table width=\"100%\" cellspacing=0 cellpadding=4 border=0 >"
           "<tr><td bgcolor=\"#d0d0d0\" colspan=2 cellpadding=0 valign=center align=center><font color=\"black\" size=+1><b>"
           + tr("Welcome to SleepyHead") + "</b></font></td></tr>"
           "<tr>"
           "<td valign=\"top\" leftmargin=0 cellpadding=6>"
           "<h3>" + tr("About SleepyHead") + "</h3>"
           "<p>" + tr("This software has been created to assist you in reviewing the data produced by CPAP Machines, used in the treatment of various Sleep Disorders.")
           + "</p>"
           "<p>" + tr("SleepyHead has been designed by a software developer with personal experience with a sleep disorder, and shaped by the feedback of many other willing testers dealing with similar conditions.")
           + "</p>"
           "<p><i><b>" + tr("This is a beta release, some features may not yet behave as expected.") +
           "</b></i><br/>" + tr("Please report any bugs you find to SleepyHead's SourceForge page.") + "</p>"

           "<h3>" + tr("Currenly supported machines:") + "</h3>"
           "<b>" + tr("CPAP") + "</b>"
           "<li>" + tr("Philips Respironics System One (CPAP Pro, Auto, BiPAP & ASV models)") + "</li>"
           "<li>" + tr("ResMed S9 models (CPAP, Auto, VPAP)") + "</li>"
           "<li>" + tr("DeVilbiss Intellipap (Auto)") + "</li>"
           "<li>" + tr("Fisher & Paykel ICON (CPAP, Auto)") + "</li>"
           "<b>" + tr("Oximetry") + "</b>"
           "<li>" + tr("Contec CMS50D+, CMS50E and CMS50F (not 50FW) Oximeters") + "</li>"
           "<li>" + tr("ResMed S9 Oximeter Attachment") + "</li>"
           "<p><h3>" + tr("Online Help Resources") + "</h3></p>"
           "<p><b>" + tr("Note:") + "</b>" +
           tr("I don't recommend using this built in web browser to do any major surfing in, it will work, but it's mainly meant as a help browser.")
           +
           tr("(It doesn't support SSL encryption, so it's not a good idea to type your passwords or personal details anywhere.)")
           + "</p>" +

           tr("SleepyHead's Online <a href=\"http://sleepyhead.sourceforge.net/wiki/index.php?title=SleepyHead_Users_Guide\">Users Guide</a><br/>")
           +
           tr("<a href=\"http://sleepyhead.sourceforge.net/wiki/index.php?title=Frequently_Asked_Questions\">Frequently Asked Questions</a><br/>")
           +
           tr("<a href=\"http://sleepyhead.sourceforge.net/wiki/index.php?title=Glossary\">Glossary of Sleep Disorder Terms</a><br/>")
           +
           tr("<a href=\"http://sleepyhead.sourceforge.net/wiki/index.php?title=Main_Page\">SleepyHead Wiki</a><br/>")
           +
           tr("SleepyHead's <a href='http://www.sourceforge.net/projects/sleepyhead'>Project Website</a> on SourceForge<br/>")
           +
           "<p><h3>" + tr("Further Information") + "</h3></p>"
           "<p>" +
           tr("Here are the <a href='qrc:/docs/release_notes.html'>release notes</a> for this version.") +
           "<br/>" +
           tr("Plus a few <a href='qrc:/docs/usage.html'>usage notes</a>, and some important information for Mac users.")
           + "<br/>" +
           "<p>" + tr("About <a href='http://en.wikipedia.org/wiki/Sleep_apnea'>Sleep Apnea</a> on Wikipedia")
           + "</p>"

           "<p>" + tr("Friendly forums to talk and learn about Sleep Apnea:") + "<br/>" +
           tr("<a href='http://www.cpaptalk.com'>CPAPTalk Forum</a>,") +
           tr("<a href='http://www.apneaboard.com/forums/'>Apnea Board</a>") + "</p>"
           "</td>"
           "<td><image src='qrc:/icons/bob-v3.0.png' width=220 height=220><br/>"
           "</td>"
           "</tr>"
           "<tr>"
           "<td colspan=2>"
           "<hr/>"
           "<p><b>" + tr("Copyright:") + "</b> " + tr("&copy;2011-2018") +
           " <a href=\"http://jedimark64.blogspot.com\">Mark Watkins</a> (jedimark)</p>"
           "<p><b>" + tr("License:") + "</b> " +
           tr("This software is released freely under the <a href=\"qrc:/COPYING\">GNU Public License</a>.") +
           "</p>"
           "<hr/>"
           "<p><b>" + tr("DISCLAIMER:") + "</b></p>"
           "<b><p>" +
           tr("This is <font color='red'><u>NOT</u></font> medical software. This application is merely a data viewer, and no guarantee is made regarding accuracy or correctness of any calculations or data displayed.")
           + "</p>"
           "<p>" + tr("The author will NOT be held liable by anyone who harms themselves or others by use or misuse of this software.")
           + "</p>"
           "<p>" + tr("Your doctor should always be your first and best source of guidance regarding the important matter of managing your health.")
           + "</p>"
           "<p>" + tr("*** <u>Use at your own risk</u> ***") + "</p></b>"
           "<hr/>"
           "</td></tr>"
           "</table>"
           "</body>"
           "</html>"

           ;
}

void MainWindow::updateFavourites()
{
    QDate date = p_profile->LastDay(MT_JOURNAL);

    if (!date.isValid()) {
        return;
    }

    QString html = "<html><head><style type='text/css'>"
                   "p,a,td,body { font-family: '" + QApplication::font().family() + "'; }"
                   "p,a,td,body { font-size: " + QString::number(QApplication::font().pointSize() + 2) + "px; }"
                   "a:link,a:visited { color: inherit; text-decoration: none; }" //font-weight: normal;
                   "a:hover { background-color: inherit; color: white; text-decoration:none; font-weight: bold; }"
                   "</style></head><body>"
                   "<table width=100% cellpadding=2 cellspacing=0>";

    do {
        Day *journal = p_profile->GetDay(date, MT_JOURNAL);

        if (journal) {
            if (journal->size() > 0) {
                Session *sess = journal->firstSession(MT_JOURNAL);
                QString tmp;
                bool filtered = !bookmarkFilter.isEmpty();
                bool found = !filtered;

                if (sess->settings.contains(Bookmark_Start)) {
                    //QVariantList start=sess->settings[Bookmark_Start].toList();
                    //QVariantList end=sess->settings[Bookmark_End].toList();
                    QStringList notes = sess->settings[Bookmark_Notes].toStringList();

                    if (notes.size() > 0) {
                        tmp += QString("<tr><td><b><a href='daily=%1'>%2</a></b><br/>")
                               .arg(date.toString(Qt::ISODate))
                               .arg(date.toString());

                        tmp += "<list>";

                        for (int i = 0; i < notes.size(); i++) {
                            //QDate d=start[i].toDate();
                            QString note = notes[i];

                            if (filtered && note.contains(bookmarkFilter, Qt::CaseInsensitive)) {
                                found = true;
                            }

                            tmp += "<li>" + note + "</li>";
                        }

                        tmp += "</list></td>";
                    }
                }

                if (found) { html += tmp; }
            }
        }

        date = date.addDays(-1);
    } while (date >= p_profile->FirstDay(MT_JOURNAL));

    html += "</table></body></html>";
    ui->bookmarkView->setHtml(html);
}

void MainWindow::on_dailyButton_clicked()
{
    if (daily) {
        ui->tabWidget->setCurrentWidget(daily);
        daily->RedrawGraphs();
    }
}
void MainWindow::on_overviewButton_clicked()
{
    if (overview) {
        ui->tabWidget->setCurrentWidget(overview);
    }
}

void MainWindow::aboutBoxLinkClicked(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

void MainWindow::on_action_About_triggered()
{
    AboutDialog * about = new AboutDialog(this);

    about->exec();

    about->deleteLater();
}

void MainWindow::on_actionDebug_toggled(bool checked)
{
    AppSetting->setShowDebug(checked);

    logger->strlock.lock();
    if (checked) {
        ui->logText->show();
    } else {
        ui->logText->hide();
    }
  //  QApplication::processEvents();
    logger->strlock.unlock();
}

void MainWindow::on_action_Reset_Graph_Layout_triggered()
{
    if (daily && (ui->tabWidget->currentWidget() == daily)) { daily->ResetGraphLayout(); }

    if (overview && (ui->tabWidget->currentWidget() == overview)) { overview->ResetGraphLayout(); }
}

void MainWindow::on_action_Preferences_triggered()
{
    if (!p_profile) {
        mainwin->Notify(tr("Please open a profile first."));
        return;
    }

    if (m_inRecalculation) {
        mainwin->Notify(tr("Access to Preferences has been blocked until recalculation completes."));
        return;
    }

    PreferencesDialog pd(this, p_profile);
    prefdialog = &pd;

    if (pd.exec() == PreferencesDialog::Accepted) {
        if (daily) {
            daily->RedrawGraphs();
        }

        if (overview) {
            overview->RebuildGraphs(true);
        }
    }

    prefdialog = nullptr;
}

#include "oximeterimport.h"
QDateTime datetimeDialog(QDateTime datetime, QString message);

void MainWindow::on_oximetryButton_clicked()
{
    if (p_profile) {
        OximeterImport oxiimp(this);
        oxiimp.exec();
    }
}

void MainWindow::CheckForUpdates()
{
    on_actionCheck_for_Updates_triggered();
}

void MainWindow::on_actionCheck_for_Updates_triggered()
{
    UpdaterWindow *w = new UpdaterWindow(this);
    w->checkForUpdates();
}

bool toolbox_visible = false;
void MainWindow::on_action_Screenshot_triggered()
{
    daily->hideSpaceHogs();
    toolbox_visible = ui->toolBox->isVisible();
    ui->toolBox->hide();
    QTimer::singleShot(250, this, SLOT(DelayedScreenshot()));
}
void MainWindow::DelayedScreenshot()
{
    // Make sure to scale for high resolution displays (like Retina)
   // qreal pr = devicePixelRatio();


    QScreen * screen = QApplication::primaryScreen();


    int titleBarHeight = -QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight);
#ifdef Q_OS_WIN
    titleBarHeight += 6;
#endif

    QPixmap pixmap = screen->grabWindow(winId(),0,titleBarHeight);

/*#if defined(Q_OS_WIN) || defined(Q_OS_LINUX) || defined(Q_OS_HAIKU)
     // grab the whole screen
    grab()
     QPixmap desktop = QPixmap::grabWindow(QApplication::desktop()->winId());

     QPixmap pixmap = desktop.copy(x() * pr, y() * pr, (width()+6) * pr, (height()+22) * pr);

#elif defined(Q_OS_MAC)
    QPixmap pixmap = QPixmap::grabWindow(this->winId(), x(), y(), width() / pr, (height() / pr) + 10);
#endif */

    QString a = PREF.Get("{home}/Screenshots");
    QDir dir(a);

    if (!dir.exists()) {
        dir.mkdir(a);
    }

    a += "/screenshot-" + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss") + ".png";

    qDebug() << "Saving screenshot to" << a;

    if (!pixmap.save(a)) {
        Notify(tr("There was an error saving screenshot to file \"%1\"").arg(QDir::toNativeSeparators(a)));
    } else {
        Notify(tr("Screenshot saved to file \"%1\"").arg(QDir::toNativeSeparators(a)));
    }
    daily->showSpaceHogs();
    ui->toolBox->setVisible(toolbox_visible);

}

void MainWindow::on_actionView_Oximetry_triggered()
{
    on_oximetryButton_clicked();
}

void MainWindow::on_actionPrint_Report_triggered()
{
    Report report;

    if (ui->tabWidget->currentWidget() == overview) {
        Report::PrintReport(overview->graphView(), STR_TR_Overview);
    } else if (ui->tabWidget->currentWidget() == daily) {
        Report::PrintReport(daily->graphView(), STR_TR_Daily, daily->getDate());
    } else {
        QPrinter printer(QPrinter::HighResolution);
#ifdef Q_WS_X11
        printer.setPrinterName("Print to File (PDF)");
        printer.setOutputFormat(QPrinter::PdfFormat);
        QString name;
        QString datestr;

        if (ui->tabWidget->currentWidget() == ui->statisticsTab) {
            name = "Statistics";
            datestr = QDate::currentDate().toString(Qt::ISODate);
        } else if (ui->tabWidget->currentWidget() == ui->helpTab) {
            name = "Help";
            datestr = QDateTime::currentDateTime().toString(Qt::ISODate);
        } else { name = "Unknown"; }

        QString filename = PREF.Get("{home}/" + name + "_" + p_profile->user->userName() + "_" + datestr + ".pdf");

        printer.setOutputFileName(filename);
#endif
        printer.setPrintRange(QPrinter::AllPages);
//        if (ui->tabWidget->currentWidget() == ui->statisticsTab) {
//            printer.setOrientation(QPrinter::Landscape);
//        } else {
            printer.setOrientation(QPrinter::Portrait);
        //}
        printer.setFullPage(false); // This has nothing to do with scaling
        printer.setNumCopies(1);
        printer.setResolution(1200);
        //printer.setPaperSize(QPrinter::A4);
        //printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setPageMargins(5, 5, 5, 5, QPrinter::Millimeter);
        QPrintDialog pdlg(&printer, this);

        if (pdlg.exec() == QPrintDialog::Accepted) {

            if (ui->tabWidget->currentWidget() == ui->statisticsTab) {

                QTextBrowser b;
                QPainter painter;
                painter.begin(&printer);

                QRect rect = printer.pageRect();
                b.setHtml(ui->statisticsView->toHtml());
                b.resize(rect.width()/4, rect.height()/4);
                b.setFrameShape(QFrame::NoFrame);

                double xscale = printer.pageRect().width()/double(b.width());
                double yscale = printer.pageRect().height()/double(b.height());
                double scale = qMin(xscale, yscale);
                painter.translate(printer.paperRect().x() + printer.pageRect().width()/2, printer.paperRect().y() + printer.pageRect().height()/2);
                painter.scale(scale, scale);
                painter.translate(-b.width()/2, -b.height()/2);

                b.render(&painter, QPoint(0,0));
                painter.end();

            } else if (ui->tabWidget->currentWidget() == help) {
                help->print(&printer);
            }

        }
    }
}

void MainWindow::on_action_Edit_Profile_triggered()
{
    NewProfile *newprof = new NewProfile(this);
    QString name = AppSetting->profileName();
    newprof->edit(name);
    newprof->setWindowModality(Qt::ApplicationModal);
    newprof->setModal(true);
    newprof->exec();
    qDebug()  << newprof;
    delete newprof;
}

void MainWindow::on_action_CycleTabs_triggered()
{
    int i;
    qDebug() << "Switching Tabs";
    i = ui->tabWidget->currentIndex() + 1;

    if (i >= ui->tabWidget->count()) {
        i = 0;
    }

    ui->tabWidget->setCurrentIndex(i);
}

void MainWindow::on_actionOnline_Users_Guide_triggered()
{
    QDesktopServices::openUrl(QUrl("http://sleepyhead.sourceforge.net/wiki/index.php?title=SleepyHead_Users_Guide"));
}

void MainWindow::on_action_Frequently_Asked_Questions_triggered()
{
    QDesktopServices::openUrl(QUrl("http://sleepyhead.sourceforge.net/wiki/index.php?title=Frequently_Asked_Questions"));
}

void packEventList(EventList *el, EventDataType minval = 0)
{
    if (el->count() < 2) { return; }

    EventList nel(EVL_Waveform);
    EventDataType t = 0, lastt = 0; //el->data(0);
    qint64 ti = 0; //=el->time(0);
    //nel.AddEvent(ti,lastt);
    bool f = false;
    qint64 lasttime = 0;
    EventDataType min = 999, max = 0;

    for (quint32 i = 0; i < el->count(); i++) {
        t = el->data(i);
        ti = el->time(i);
        f = false;

        if (t > minval) {
            if (t != lastt) {
                if (!lasttime) {
                    nel.setFirst(ti);
                }

                nel.AddEvent(ti, t);

                if (t < min) { min = t; }

                if (t > max) { max = t; }

                lasttime = ti;
                f = true;
            }
        } else {
            if (lastt > minval) {
                nel.AddEvent(ti, lastt);
                lasttime = ti;
                f = true;
            }
        }

        lastt = t;
    }

    if (!f) {
        if (t > minval) {
            nel.AddEvent(ti, t);
        }
    }

    el->setFirst(nel.first());
    el->setLast(nel.last());
    el->setMin(min);
    el->setMax(max);

    el->getData().clear();
    el->getTime().clear();
    el->setCount(nel.count());

    el->getData() = nel.getData();
    el->getTime() = nel.getTime();
}

void MainWindow::on_action_Rebuild_Oximetry_Index_triggered()
{
    QVector<ChannelID> valid;
    valid.push_back(OXI_Pulse);
    valid.push_back(OXI_SPO2);
    valid.push_back(OXI_Plethy);
    //valid.push_back(OXI_PulseChange); // Delete these and recalculate..
    //valid.push_back(OXI_SPO2Drop);

    QVector<ChannelID> invalid;

    QList<Machine *> machines = p_profile->GetMachines(MT_OXIMETER);

    qint64 f = 0, l = 0;

    int discard_threshold = p_profile->oxi->oxiDiscardThreshold();
    Machine *m;

    for (int z = 0; z < machines.size(); z++) {
        m = machines.at(z);
        //m->sessionlist.erase(m->sessionlist.find(0));

        // For each Session
        for (QHash<SessionID, Session *>::iterator s = m->sessionlist.begin(); s != m->sessionlist.end();
                s++) {
            Session *sess = s.value();

            if (!sess) { continue; }

            sess->OpenEvents();

            // For each EventList contained in session
            invalid.clear();
            f = 0;
            l = 0;

            for (QHash<ChannelID, QVector<EventList *> >::iterator e = sess->eventlist.begin();
                    e != sess->eventlist.end(); e++) {

                // Discard any non data events.
                if (!valid.contains(e.key())) {
                    // delete and push aside for later to clean up
                    for (int i = 0; i < e.value().size(); i++)  {
                        delete e.value()[i];
                    }

                    e.value().clear();
                    invalid.push_back(e.key());
                } else {
                    // Valid event


                    //                    // Clean up outliers at start of eventlist chunks
                    //                    EventDataType baseline=sess->wavg(OXI_SPO2);
                    //                    if (e.key()==OXI_SPO2) {
                    //                        const int o2start_threshold=10000; // seconds since start of event

                    //                        EventDataType zz;
                    //                        int ii;

                    //                        // Trash suspect outliers in the first o2start_threshold milliseconds
                    //                        for (int j=0;j<e.value().size();j++) {
                    //                            EventList *ev=e.value()[j];
                    //                            if ((ev->count() <= (unsigned)discard_threshold))
                    //                                continue;

                    //                            qint64 ti=ev->time(0);

                    //                            zz=-1;
                    //                            // Peek o2start_threshold ms ahead and grab the value
                    //                            for (ii=0;ii<ev->count();ii++) {
                    //                                if (((ev->time(ii)-ti) > o2start_threshold)) {
                    //                                    zz=ev->data(ii);
                    //                                    break;
                    //                                }
                    //                            }
                    //                            if (zz<0)
                    //                                continue;
                    //                            // Trash any suspect outliers
                    //                            for (int i=0;i<ii;i++) {
                    //                                if (ev->data(i) < baseline) { //(zz-10)) {

                    //                                    ev->getData()[i]=0;
                    //                                }
                    //                            }
                    //                        }
                    //                    }
                    QVector<EventList *> newlist;

                    for (int i = 0; i < e.value().size(); i++)  {
                        if (e.value()[i]->count() > (unsigned)discard_threshold) {
                            newlist.push_back(e.value()[i]);
                        } else {
                            delete e.value()[i];
                        }
                    }

                    for (int i = 0; i < newlist.size(); i++) {
                        packEventList(newlist[i], 8);

                        EventList *el = newlist[i];

                        if (!f || f > el->first()) { f = el->first(); }

                        if (!l || l < el->last()) { l = el->last(); }
                    }

                    e.value() = newlist;
                }
            }

            for (int i = 0; i < invalid.size(); i++) {
                sess->eventlist.erase(sess->eventlist.find(invalid[i]));
            }

            if (f) { sess->really_set_first(f); }

            if (l) { sess->really_set_last(l); }

            sess->m_cnt.clear();
            sess->m_sum.clear();
            sess->m_min.clear();
            sess->m_max.clear();
            sess->m_cph.clear();
            sess->m_sph.clear();
            sess->m_avg.clear();
            sess->m_wavg.clear();
            sess->m_valuesummary.clear();
            sess->m_timesummary.clear();
            sess->m_firstchan.clear();
            sess->m_lastchan.clear();
            sess->SetChanged(true);
        }

    }

    for (int i = 0; i < machines.size(); i++) {
        Machine *m = machines[i];
        m->Save();
        m->SaveSummaryCache();
    }

    daily->LoadDate(daily->getDate());
    overview->ReloadGraphs();
}
void MainWindow::reloadProfile()
{
    QString username = p_profile->user->userName();
    int tabidx = ui->tabWidget->currentIndex();
    CloseProfile();
    OpenProfile(username);
    ui->tabWidget->setCurrentIndex(tabidx);
}

void MainWindow::RestartApplication(bool force_login, QString cmdline)
{
    CloseProfile();
    PREF.Save();

    QString apppath;
#ifdef Q_OS_MAC
    // In Mac OS the full path of aplication binary is:
    //    <base-path>/myApp.app/Contents/MacOS/myApp

    // prune the extra bits to just get the app bundle path
    apppath = QApplication::instance()->applicationDirPath().section("/", 0, -3);

    QStringList args;
    args << "-n"; // -n option is important, as it opens a new process
    args << apppath;

    args << "--args"; // SleepyHead binary options after this
    args << "-p"; // -p starts with 1 second delay, to give this process time to save..


    if (force_login) { args << "-l"; }

    args << cmdline;

    if (QProcess::startDetached("/usr/bin/open", args)) {
        QApplication::instance()->exit();
    } else { QMessageBox::warning(nullptr, tr("Gah!"), tr("If you can read this, the restart command didn't work. Your going to have to do it yourself manually."), QMessageBox::Ok); }

#else
    apppath = QApplication::instance()->applicationFilePath();

    // If this doesn't work on windoze, try uncommenting this method
    // Technically should be the same thing..

    //if (QDesktopServices::openUrl(apppath)) {
    //    QApplication::instance()->exit();
    //} else
    QStringList args;
    args << "-p";

    if (force_login) { args << "-l"; }

    args << cmdline;

    //if (change_datafolder) { args << "-d"; }

    if (QProcess::startDetached(apppath, args)) {
        QApplication::instance()->exit();

//        ::exit(0);
    } else { QMessageBox::warning(nullptr, tr("Gah!"), tr("If you can read this, the restart command didn't work. Your going to have to do it yourself manually."), QMessageBox::Ok); }

#endif
}

void MainWindow::on_actionPurge_Current_Day_triggered()
{
    if (!daily) return;
    QDate date = daily->getDate();
    daily->Unload(date);
    Day *day = p_profile->GetDay(date, MT_CPAP);
    Machine *cpap = nullptr;
    if (day) cpap = day->machine(MT_CPAP);

    if (cpap) {
        QList<Session *>::iterator s;

        QList<Session *> list;
        QList<SessionID> sidlist;
        for (s = day->begin(); s != day->end(); ++s) {
            list.push_back(*s);
            sidlist.push_back((*s)->session());
        }

        QHash<QString, SessionID> skipfiles;
        // Read the already imported file list

        QFile impfile(cpap->getDataPath()+"/imported_files.csv");
        if (impfile.exists()) {
            if (impfile.open(QFile::ReadOnly)) {
                QTextStream impstream(&impfile);
                QString serial;
                impstream >> serial;
                if (cpap->serial() == serial) {
                    QString line, file, str;
                    SessionID sid;
                    bool ok;
                    do {
                        line = impstream.readLine();
                        file = line.section(',',0,0);
                        str = line.section(',',1);
                        sid = str.toInt(&ok);
                        if (!sidlist.contains(sid)) {
                            skipfiles[file] = sid;
                        }
                    } while (!impstream.atEnd());
                }
            }
            impfile.close();
            // Delete the file
            impfile.remove();

            // Rewrite the file without the sessions being removed.
            if (impfile.open(QFile::WriteOnly)) {
                QTextStream out(&impfile);
                out << cpap->serial();
                QHash<QString, SessionID>::iterator skit;
                QHash<QString, SessionID>::iterator skit_end = skipfiles.end();
                for (skit = skipfiles.begin(); skit != skit_end; ++skit) {
                    QString a = QString("%1,%2\n").arg(skit.key()).arg(skit.value());;
                    out << a;
                }
                out.flush();
            }
            impfile.close();
        }

        QFile rxcache(p_profile->Get("{" + STR_GEN_DataFolder + "}/RXChanges.cache" ));
        rxcache.remove();

        QFile sumfile(cpap->getDataPath()+"Summaries.xml.gz");
        sumfile.remove();

//        m->day.erase(m->day.find(date));

        for (int i = 0; i < list.size(); i++) {
            Session *sess = list.at(i);
            sess->Destroy();
            delete sess;
        }

    }
    day = p_profile->GetDay(date, MT_CPAP);
    Q_UNUSED(day);

    daily->clearLastDay();
    daily->LoadDate(date);
}

void MainWindow::on_actionRebuildCPAP(QAction *action)
{
    ui->tabWidget->setCurrentWidget(welcome); // Daily view can't run during rebuild
    QApplication::processEvents();

    QString data = action->data().toString();
    QString cls = data.section(":",0,0);
    QString serial = data.section(":", 1);
    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    Machine * mach = nullptr;
    for (int i=0; i < machines.size(); ++i) {
        Machine * m = machines.at(i);
        if ((m->loaderName() == cls) && (m->serial() == serial)) {
            mach = m;
            break;
        }
    }
    if (!mach) return;
    QString bpath = mach->getBackupPath();
    bool backups = (dirCount(bpath) > 0) ? true : false;

    if (backups) {
        if (QMessageBox::question(this,
                              STR_MessageBox_Question,
                              tr("Are you sure you want to rebuild all CPAP data for the following machine:")+ "\n\n" +
                              mach->brand() + " " + mach->model() + " " +
                              mach->modelnumber() + " (" + mach->serial() + ")" + "\n\n"+
                              tr("Please note, that this could result in loss of graph data if SleepyHead's internal backups have been disabled or interfered with in any way."),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) == QMessageBox::No) {
            return;
        }
    } else {
        if (QMessageBox::question(this,
                              STR_MessageBox_Warning,
                              "<p><b>"+STR_MessageBox_Warning+": </b>"+tr("For some reason, SleepyHead does not have internal backups for the following machine:")+ "</p>" +
                              "<p>"+mach->brand() + " " + mach->model() + " " +
                              mach->modelnumber() + " (" + mach->serial() + ")" + "</p>"+
                              "<p>"+tr("Provided you have made <i>your <b>own</b> backups for ALL of your CPAP data</i>, you can still complete this operation, but you will have to restore from your backups manually.")+"</p>"
                              "<p><b>"+tr("Are you really sure you want to do this?")+"</b></p>",
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) == QMessageBox::No) {
            return;
        }
    }

    QString path = mach->getBackupPath();
    MachineLoader *loader = lookupLoader(mach);

    purgeMachine(mach); // purge destroys machine record

    if (backups) {
        importCPAP(ImportPath(path, loader), tr("Please wait, importing from backup folder(s)..."));
    } else {
        if (QMessageBox::information(this, STR_MessageBox_Warning,
                                 tr("Because there are no internal backups to rebuild from, you will have to restore from your own.")+"\n\n"+
                                 tr("Would you like to import from your own backups now? (you will have no data visible for this machine until you do)"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            on_action_Import_Data_triggered();
        } else {
        }
    }
    if (overview) overview->ReloadGraphs();
    if (daily) {
        daily->Unload();
        daily->clearLastDay(); // otherwise Daily will crash
        daily->ReloadGraphs();
    }
    if (welcome) welcome->refreshPage();
    PopulatePurgeMenu();
    GenerateStatistics();
    p_profile->StoreMachines();
}

void MainWindow::on_actionPurgeMachine(QAction *action)
{
    QString data = action->data().toString();
    QString cls = data.section(":",0,0);
    QString serial = data.section(":", 1);
    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    Machine * mach = nullptr;
    for (int i=0; i < machines.size(); ++i) {
        Machine * m = machines.at(i);
        if ((m->loaderName() == cls) && (m->serial() == serial)) {
            mach = m;
            break;
        }
    }
    if (!mach) return;

    if (QMessageBox::question(this, STR_MessageBox_Warning, "<p><b>"+STR_MessageBox_Warning+":</b> "+tr("You are about to <font size=+2>obliterate</font> SleepyHead's machine database for the following machine:")+"</p>"+
                              "<p>"+mach->brand() + " " + mach->model() + " " +
                              mach->modelnumber() + " (" + mach->serial() + ")" + "</p>"+
                              "<p>"+tr("Note as a precaution, the backup folder will be left in place.")+"</p>"+
                              "<p>"+tr("Are you <b>absolutely sure</b> you want to proceed?")+"</p>", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {

        purgeMachine(mach);
    }

}

void MainWindow::purgeMachine(Machine * mach)
{
    // detect backups
    daily->Unload(daily->getDate());

    // Technicially the above won't sessions under short session limit.. Using Purge to clean up the rest.
    if (mach->Purge(3478216)) {
        mach->sessionlist.clear();
        mach->day.clear();
        QDir dir;
        QString path = mach->getDataPath();
        path.chop(1);

        p_profile->DelMachine(mach);
        delete mach;
        // remove the directory unless it's got unexpected crap in it..
        bool deleted = false;
        if (!dir.remove(path)) {
#ifdef Q_OS_WIN
            wchar_t* directoryPtr = (wchar_t*)path.utf16();
            SetFileAttributes(directoryPtr, GetFileAttributes(directoryPtr) & ~FILE_ATTRIBUTE_READONLY);
            if (!::RemoveDirectory(directoryPtr)) {
               DWORD lastError = ::GetLastError();
               qDebug() << "RemoveDirectory GetLastError: " << lastError;

            } else {
               qDebug() << "Success on second attempt deleting folder with windows API " << path;
               deleted = true;
            }
#else
            qDebug() << "Couldn't remove directory" << path;
#endif
        } else {
            deleted = true;
        }
        if (!deleted) {
            qDebug() << "Leaving backup folder intact";
        }

        PopulatePurgeMenu();
        p_profile->StoreMachines();
    } else {
        QMessageBox::warning(this, STR_MessageBox_Error,
                             tr("A file permission error or simillar screwed up the purge process, you will have to delete the following folder manually:")
                             +"\n\n"+
                             QDir::toNativeSeparators(mach->getDataPath()), QMessageBox::Ok, QMessageBox::Ok);
        if (overview) overview->ReloadGraphs();

        if (daily) {
            daily->clearLastDay(); // otherwise Daily will crash
            daily->ReloadGraphs();
        }
        if (welcome) welcome->refreshPage();

        //GenerateStatistics();
        return;
    }


    if (overview) overview->ReloadGraphs();
    QFile rxcache(p_profile->Get("{" + STR_GEN_DataFolder + "}/RXChanges.cache" ));
    rxcache.remove();

    if (daily) {
        daily->clearLastDay(); // otherwise Daily will crash
        daily->ReloadGraphs();
    }
    if (welcome) welcome->refreshPage();

    QApplication::processEvents();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    //qDebug() << "Keypress:" << event->key();
}

void MainWindow::on_action_Sidebar_Toggle_toggled(bool visible)
{
    ui->toolBox->setVisible(visible);
    AppSetting->setRightSidebarVisible(visible);
}

void MainWindow::on_helpButton_clicked()
{
    ui->tabWidget->setCurrentWidget(help);
}

void MainWindow::on_actionView_Statistics_triggered()
{
    ui->tabWidget->setCurrentWidget(ui->statisticsTab);
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    Q_UNUSED(index);
   // QWidget *widget = ui->tabWidget->currentWidget();
}


void MainWindow::on_filterBookmarks_editingFinished()
{
    bookmarkFilter = ui->filterBookmarks->text();
    updateFavourites();
}

void MainWindow::on_filterBookmarksButton_clicked()
{
    if (!bookmarkFilter.isEmpty()) {
        ui->filterBookmarks->setText("");
        bookmarkFilter = "";
        updateFavourites();
    }
}

void MainWindow::recompressEvents()
{
    QTimer::singleShot(0, this, SLOT(doRecompressEvents()));
}
void MainWindow::reprocessEvents(bool restart)
{
    m_restartRequired = restart;
    QTimer::singleShot(0, this, SLOT(doReprocessEvents()));
}


void MainWindow::FreeSessions()
{
    QDate first = p_profile->FirstDay();
    QDate date = p_profile->LastDay();
    Day *day;
    QDate current = daily->getDate();

    do {
        day = p_profile->GetDay(date, MT_CPAP);

        if (day) {
            if (date != current) {
                day->CloseEvents();
            }
        }

        date = date.addDays(-1);
    }  while (date >= first);
}

void MainWindow::MachineUnsupported(Machine * m)
{
    if (m == nullptr) {
        qCritical() << "MainWindow::MachineUnsupported called with null machine object";
        return;
    }
    QMessageBox::information(this, STR_MessageBox_Error, QObject::tr("Sorry, your %1 %2 machine is not currently supported.").arg(m->brand()).arg(m->model()), QMessageBox::Ok);
}

void MainWindow::doRecompressEvents()
{
    if (!p_profile) return;
    ProgressDialog progress(this);
    progress.setMessage("Recompressing Session Files");
    progress.setProgressMax(p_profile->daylist.size());
    QPixmap icon = QPixmap(":/docs/sheep.png").scaled(64,64);
    progress.setPixmap(icon);
    progress.open();

    bool isopen;
    int idx = 0;
    for (Day * day : p_profile->daylist) {
        for (Session * sess : day->sessions) {
            isopen = sess->eventsLoaded();
            // Load the events and summary if they aren't loaded already
            sess->LoadSummary();
            sess->OpenEvents();
            sess->SetChanged(true);
            sess->machine()->SaveSession(sess);

            if (!isopen) {
                sess->TrashEvents();
            }
        }
        progress.setProgressValue(++idx);
        QApplication::processEvents();
    }
    progress.close();
}
void MainWindow::doReprocessEvents()
{
    if (!p_profile) return;

    ProgressDialog progress(this);
    progress.setMessage("Recalculating summaries");
    progress.setProgressMax(p_profile->daylist.size());
    QPixmap icon = QPixmap(":/docs/sheep.png").scaled(64,64);
    progress.setPixmap(icon);
    progress.open();

    if (daily) {
        daily->Unload();
        daily->clearLastDay(); // otherwise Daily will crash
        delete daily;
        daily = nullptr;
    }
    if (welcome) {
        delete welcome;
        welcome = nullptr;
    }
    if (overview) {
        delete overview;
        overview = nullptr;
    }

    for (Day * day : p_profile->daylist) {
        for (Session * sess : day->sessions) {
            bool isopen = sess->eventsLoaded();

            // Load the events if they aren't loaded already
            sess->LoadSummary();
            sess->OpenEvents();

            // Destroy any current user flags..
            sess->destroyEvent(CPAP_UserFlag1);
            sess->destroyEvent(CPAP_UserFlag2);
            sess->destroyEvent(CPAP_UserFlag3);

            // AHI flags
            sess->destroyEvent(CPAP_AHI);
            sess->destroyEvent(CPAP_RDI);

            if (sess->machine()->loaderName() != STR_MACH_PRS1) {
                sess->destroyEvent(CPAP_LargeLeak);
            } else {
                sess->destroyEvent(CPAP_Leak);
            }

            sess->SetChanged(true);

            sess->UpdateSummaries();
            sess->machine()->SaveSession(sess);

            if (!isopen) {
                sess->TrashEvents();
            }
        }
    }
    progress.close();

    welcome = new Welcome(ui->tabWidget);
    ui->tabWidget->insertTab(1, welcome, tr("Welcome"));

    daily = new Daily(ui->tabWidget, nullptr);
    ui->tabWidget->insertTab(2, daily, STR_TR_Daily);
    daily->ReloadGraphs();

    overview = new Overview(ui->tabWidget, daily->graphView());
    ui->tabWidget->insertTab(3, overview, STR_TR_Overview);
    overview->ReloadGraphs();

    // Should really create welcome and statistics here, but they need redoing later anyway to kill off webkit
    ui->tabWidget->setCurrentIndex(AppSetting->openTabAtStart());
    GenerateStatistics();
    PopulatePurgeMenu();

}

void MainWindow::on_actionImport_ZEO_Data_triggered()
{
    QFileDialog w;
    w.setFileMode(QFileDialog::ExistingFiles);
    w.setOption(QFileDialog::ShowDirsOnly, false);
    w.setOption(QFileDialog::DontUseNativeDialog, true);
    w.setNameFilters(QStringList("Zeo CSV File (*.csv)"));

    ZEOLoader zeo;

    if (w.exec() == QFileDialog::Accepted) {
        QString filename = w.selectedFiles()[0];

        if (!zeo.OpenFile(filename)) {
            Notify(tr("There was a problem opening ZEO File: ") + filename);
            return;
        }

        Notify(tr("Zeo CSV Import complete"));
        daily->LoadDate(daily->getDate());
    }


}

void MainWindow::on_actionImport_RemStar_MSeries_Data_triggered()
{
#ifdef REMSTAR_M_SUPPORT
    QFileDialog w;
    w.setFileMode(QFileDialog::ExistingFiles);
    w.setOption(QFileDialog::ShowDirsOnly, false);
    w.setOption(QFileDialog::DontUseNativeDialog, true);
    w.setNameFilters(QStringList("M-Series data file (*.bin)"));

    MSeriesLoader mseries;

    if (w.exec() == QFileDialog::Accepted) {
        QString filename = w.selectedFiles()[0];

        if (!mseries.Open(filename, p_profile)) {
            Notify(tr("There was a problem opening MSeries block File: ") + filename);
            return;
        }

        Notify(tr("MSeries Import complete"));
        daily->LoadDate(daily->getDate());
    }

#endif
}

void MainWindow::on_actionSleep_Disorder_Terms_Glossary_triggered()
{
    QDesktopServices::openUrl(QUrl("http://sleepyhead.sourceforge.net/wiki/index.php?title=Glossary"));
}

void MainWindow::on_actionHelp_Support_SleepyHead_Development_triggered()
{
    QDesktopServices().openUrl(QUrl("https://sleepyhead.jedimark.net/donate.php"));
}

void MainWindow::on_actionChange_Language_triggered()
{
//    if (QMessageBox::question(this,STR_MessageBox_Warning,tr("Changing the language will reset custom Event and Waveform names/labels/descriptions.")+"\n\n"+tr("Are you sure you want to do this?"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
//        return;
//    }

    RestartApplication(true, "--language");
}

void MainWindow::on_actionChange_Data_Folder_triggered()
{
    if (p_profile) {
        p_profile->Save();
        p_profile->removeLock();
    }
    PREF.Save();

    RestartApplication(false, "-d");
}

void MainWindow::on_actionImport_Somnopose_Data_triggered()
{
    QFileDialog w;
    w.setFileMode(QFileDialog::ExistingFiles);
    w.setOption(QFileDialog::ShowDirsOnly, false);
    w.setOption(QFileDialog::DontUseNativeDialog, true);
    w.setNameFilters(QStringList("Somnopause CSV File (*.csv)"));

    SomnoposeLoader somno;

    if (w.exec() == QFileDialog::Accepted) {
        QString filename = w.selectedFiles()[0];

        if (!somno.OpenFile(filename)) {
            Notify(tr("There was a problem opening Somnopose Data File: ") + filename);
            return;
        }

        Notify(tr("Somnopause Data Import complete"));
        daily->LoadDate(daily->getDate());
    }

}

void MainWindow::GenerateStatistics()
{
    QDate first = p_profile->FirstDay();
    QDate last = p_profile->LastDay();
    ui->statStartDate->setMinimumDate(first);
    ui->statStartDate->setMaximumDate(last);

    ui->statEndDate->setMinimumDate(first);
    ui->statEndDate->setMaximumDate(last);

    Statistics stats;
    QString html = stats.GenerateHTML();

    updateFavourites();

    //QWebFrame *frame=ui->statisticsView->page()->currentFrame();
    //frame->addToJavaScriptWindowObject("mainwin",this);
    //ui->statisticsView->setHtml(html);
    ui->statisticsView->setHtml(html);

}


void MainWindow::JumpDaily()
{
    ui->tabWidget->setCurrentWidget(daily);
}
void MainWindow::JumpOverview()
{
    ui->tabWidget->setCurrentWidget(overview);
}
void MainWindow::JumpStatistics()
{
    ui->tabWidget->setCurrentWidget(ui->statisticsTab);
}
void MainWindow::JumpImport()
{
    on_importButton_clicked();
}
void MainWindow::JumpOxiWizard()
{
    on_oximetryButton_clicked();
}


void MainWindow::on_statisticsButton_clicked()
{
    if (p_profile) {
        ui->tabWidget->setCurrentWidget(ui->statisticsTab);
    }
}

void MainWindow::on_reportModeMonthly_clicked()
{
    ui->statStartDate->setVisible(false);
    ui->statEndDate->setVisible(false);
    if (p_profile->general->statReportMode() != 1) {
        p_profile->general->setStatReportMode(1);
        GenerateStatistics();
    }
}

void MainWindow::on_reportModeStandard_clicked()
{
    ui->statStartDate->setVisible(false);
    ui->statEndDate->setVisible(false);
    if (p_profile->general->statReportMode() != 0) {
        p_profile->general->setStatReportMode(0);
        GenerateStatistics();
    }
}


void MainWindow::on_reportModeRange_clicked()
{
    ui->statStartDate->setVisible(true);
    ui->statEndDate->setVisible(true);
    if (p_profile->general->statReportMode() != 2) {
        p_profile->general->setStatReportMode(2);
        GenerateStatistics();
    }
}

void MainWindow::on_actionPurgeCurrentDaysOximetry_triggered()
{
    if (!daily)
        return;
    QDate date = daily->getDate();
    Day * day = p_profile->GetDay(date, MT_OXIMETER);
    if (day) {
        if (QMessageBox::question(this, STR_MessageBox_Warning,
            tr("Are you sure you want to delete oximetry data for %1").
                arg(daily->getDate().toString(Qt::DefaultLocaleLongDate))+"<br/><br/>"+
            tr("<b>Please be aware you can not undo this operation!</b>"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {
            return;
        }

        QList<Session *> sessionlist=day->getSessions(MT_OXIMETER);

        for (auto & sess : sessionlist) {
            sess->Destroy();
            delete sess;
        }


        if (daily) {
            daily->clearLastDay(); // otherwise Daily will crash
            daily->ReloadGraphs();
        }
        if (overview) overview->ReloadGraphs();
        if (welcome) welcome->refreshPage();
    } else {
        QMessageBox::information(this, STR_MessageBox_Information,
            tr("Select the day with valid oximetry data in daily view first."),QMessageBox::Ok);
    }
}

void MainWindow::on_importButton_clicked()
{
    on_action_Import_Data_triggered();
}


void MainWindow::on_actionToggle_Line_Cursor_toggled(bool b)
{
    AppSetting->setLineCursorMode(b);
    if (ui->tabWidget->currentWidget() == daily) {
        daily->graphView()->timedRedraw(0);
    } else if (ui->tabWidget->currentWidget() == overview) {
        overview->graphView()->timedRedraw(0);
    }
}

void MainWindow::on_actionLeft_Daily_Sidebar_toggled(bool visible)
{
    if (daily) daily->setSidebarVisible(visible);
}

void MainWindow::on_actionDaily_Calendar_toggled(bool visible)
{
    if (daily) daily->setCalendarVisible(visible);
}

#include "SleepLib/journal.h"

void MainWindow::on_actionExport_Journal_triggered()
{
    QString folder;

    folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    folder += QDir::separator() + tr("%1's Journal").arg(p_profile->user->userName()) + ".xml";

    QString filename = QFileDialog::getSaveFileName(this, tr("Choose where to save journal"), folder, tr("XML Files (*.xml)"));

    BackupJournal(filename);
}

void MainWindow::on_actionShow_Performance_Counters_toggled(bool arg1)
{
    AppSetting->setShowPerformance(arg1);
}

void MainWindow::on_actionExport_CSV_triggered()
{
    ExportCSV ex(this);

    if (ex.exec() == ExportCSV::Accepted) {
    }
}

void MainWindow::on_actionExport_Review_triggered()
{
    QMessageBox::information(nullptr, STR_MessageBox_Information, QObject::tr("Sorry, this feature is not implemented yet"), QMessageBox::Ok);
}

void MainWindow::on_mainsplitter_splitterMoved(int, int)
{
    AppSetting->setRightPanelWidth(ui->mainsplitter->sizes()[1]);
}

#include "translation.h"
void MainWindow::on_actionReport_a_Bug_triggered()
{
    QSettings settings;
    QString language = settings.value(LangSetting).toString();

    QDesktopServices::openUrl(QUrl(QString("https://sleepyhead.jedimark.net/report_bugs.php?lang=%1&version=%2&platform=%3").arg(language).arg(VersionString).arg(PlatformString)));
}

void MainWindow::on_profilesButton_clicked()
{
    ui->tabWidget->setCurrentWidget(profileSelector);
}

void MainWindow::on_bookmarkView_anchorClicked(const QUrl &arg1)
{
    on_recordsBox_anchorClicked(arg1);
}

void MainWindow::on_recordsBox_anchorClicked(const QUrl &linkurl)
{
    QString link = linkurl.toString().section("=", 0, 0).toLower();
    QString data = linkurl.toString().section("=", 1).toLower();
    qDebug() << linkurl.toString() << link << data;

    if (link == "daily") {
        QDate date = QDate::fromString(data, Qt::ISODate);
        ui->tabWidget->setCurrentWidget(daily);
        QApplication::processEvents();
        daily->LoadDate(date);
    } else if (link == "overview") {
        QString date1 = data.section(",", 0, 0);
        QString date2 = data.section(",", 1);

        QDate d1 = QDate::fromString(date1, Qt::ISODate);
        QDate d2 = QDate::fromString(date2, Qt::ISODate);
        overview->setRange(d1, d2);
        ui->tabWidget->setCurrentWidget(overview);
    } else if (link == "import") {
        // Don't run the importer directly from here because it destroys the object that called this function..
        // Schedule it instead
        if (data == "cpap") QTimer::singleShot(0, mainwin, SLOT(on_importButton_clicked()));
        if (data == "oximeter") QTimer::singleShot(0, mainwin, SLOT(on_oximetryButton_clicked()));
    } else if (link == "statistics") {
        ui->tabWidget->setCurrentWidget(ui->statisticsTab);
    }
}

void MainWindow::on_statisticsView_anchorClicked(const QUrl &url)
{
    on_recordsBox_anchorClicked(url);
}
