/* SleepyHead MainWindow Headers
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#ifndef BROKEN_OPENGL_BUILD
#include <QGLContext>
#endif
#include <QSystemTrayIcon>
#include <QTimer>

#include "daily.h"
#include "overview.h"
#include "welcome.h"
#include "help.h"

#include "profileselector.h"
#include "preferencesdialog.h"

extern Profile *profile;
QString getCPAPPixmap(QString mach_class);

namespace Ui {
class MainWindow;
}


/*! \mainpage SleepyHead

 \section intro_sec Introduction

 SleepyHead is Cross-Platform Open-Source software for reviewing data from %CPAP machines, which are used in the treatment of Sleep Disorders.

 SleepyHead has been created by <a href="http://jedimark64.blogspot.com">Mark Watkins</a> (JediMark), an Australian software developer.

 This document is an attempt to provide a little technical insight into SleepyHead's program internals.

 \section project_info Further Information
 The project is hosted on sourceforge, and it's project page can be reached at <a href="http://sourceforge.net/projects/sleepyhead">http://sourceforge.net/projects/sleepyhead</a>.

 There is also the <a href="http://sleepyhead.sourceforge.net">SleepyHead Wiki</a> containing further information

 \section structure Program Structure
 SleepyHead is written in C++ using Qt Toolkit library, and comprises of 3 main components
 \list
 \li The SleepLib Database, a specialized database for working with multiple sources of Sleep machine data.
 \li A custom designed, high performance and interactive OpenGL Graphing Library.
 \li and the main Application user interface.
 \endlist

 This document is still a work in progress, right now all the classes and sections are jumbled together.

 */

// * \section install_sec Installation

extern QStatusBar *qstatusbar;

//QString getCPAPPixmap(QString mach_class);


class Daily;
class Report;
class Overview;


/*! \class MainWindow
    \author Mark Watkins
    \brief The Main Application window for SleepyHead
    */

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    //! \brief Setup the rest of the GUI stuff
    void SetupGUI();

    //! \brief Update the list of Favourites (Bookmarks) in the right sidebar.
    void updateFavourites();

    //! \brief Update statistics report
    void GenerateStatistics();

    //! \brief Create a new menu object in the main menubar.
    QMenu *CreateMenu(QString title);

    //! \brief Start the automatic update checker process
    void CheckForUpdates();

    void CloseProfile();
    bool OpenProfile(QString name, bool skippassword = false);

    /*! \fn Notify(QString s,int ms=5000, QString title="SleepyHead v"+VersionString());
        \brief Pops up a message box near the system tray
        \param QString string
        \param title
        \param int ms

        Title is shown in bold
        string is the main message content to show
        ms = millisecond delay of how long to show popup

        Mac needs Growl notification system for this to work
        */
    void Notify(QString s, QString title = "", int ms = 5000);

//    /*! \fn gGraphView *snapshotGraph()
//        \brief Returns the current snapshotGraph object used by the report printing system */
//    gGraphView *snapshotGraph() { return SnapshotGraph; }

    //! \brief Returns the Daily Tab object
    Daily *getDaily() { return daily; }

    //! \brief Returns the Overview Tab object
    Overview *getOverview() { return overview; }

    /*! \fn void RestartApplication(bool force_login=false);
        \brief Closes down SleepyHead and restarts it
        \param bool force_login

        If force_login is set, it will return to the login menu even if it's set to skip
        */
    void RestartApplication(bool force_login = false, QString cmdline = QString());

    void JumpDaily();
    void JumpOverview();
    void JumpStatistics();
    void JumpImport();
    void JumpOxiWizard();

    void sendStatsUrl(QString msg) { on_recordsBox_anchorClicked(QUrl(msg)); }

    //! \brief Sets up recalculation of all event summaries and flags
    void reprocessEvents(bool restart = false);
    void recompressEvents();


    //! \brief Internal function to set Records Box html from statistics module
    void setRecBoxHTML(QString html);
    int importCPAP(ImportPath import, const QString &message);

    void startImportDialog() { on_action_Import_Data_triggered(); }

    void log(QString text);

    bool importScanCancelled;
    void firstRunMessage();


  public slots:
    //! \brief Recalculate all event summaries and flags
    void doReprocessEvents();
    void doRecompressEvents();

    void MachineUnsupported(Machine * m);


  protected:
    void closeEvent(QCloseEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;

  private slots:
    /*! \fn void on_action_Import_Data_triggered();
        \brief Provide the file dialog for selecting import location, and start the import process
        This is called when the Import button is clicked
        */
    void on_action_Import_Data_triggered();

    //! \brief Toggle Fullscreen (currently F11)
    void on_action_Fullscreen_triggered();

    //! \brief Selects the Daily page and redraws the graphs
    void on_dailyButton_clicked();

    //! \brief Selects the Overview page and redraws the graphs
    void on_overviewButton_clicked();

    //! \brief Display About Dialog
    void on_action_About_triggered();

    //! \brief Called after a timeout to initiate loading of all summary data for this profile
    void Startup();

    //! \brief Toggle the Debug pane on and off
    void on_actionDebug_toggled(bool arg1);

    //! \brief passes the ResetGraphLayout menu click to the Daily & Overview views
    void on_action_Reset_Graph_Layout_triggered();

    //! \brief Opens the Preferences Dialog, and saving changes if OK is pressed
    void on_action_Preferences_triggered();

    //! \brief Opens and/or shows the Oximetry page
    void on_oximetryButton_clicked();

    //! \brief Creates the UpdaterWindow object that actually does the real check for updates
    void on_actionCheck_for_Updates_triggered();

    //! \brief Attempts to do a screenshot of the application window
    //! \note This is currently broken on Macs
    void on_action_Screenshot_triggered();

    //! \brief This is the actual screenshot code.. It's delayed with a QTimer to give the menu time to close.
    void DelayedScreenshot();

    //! \brief a slot that calls the real Oximetry tab selector
    void on_actionView_Oximetry_triggered();

    //! \brief Passes the Daily, Overview & Oximetry object to Print Report, based on current tab
    void on_actionPrint_Report_triggered();

    //! \brief Opens the Profile Editor
    void on_action_Edit_Profile_triggered();

    //! \brief Selects the next view tab
    void on_action_CycleTabs_triggered();

    //! \brief Opens the User Guide at the wiki in the welcome browser.
    void on_actionOnline_Users_Guide_triggered();

    //! \brief Opens the Frequently Asked Questions at the wiki in the welcome browser.
    void on_action_Frequently_Asked_Questions_triggered();

    /*! \fn void on_action_Rebuild_Oximetry_Index_triggered();
        \brief This function scans over all oximetry data and reindexes and tries to fix any inconsistencies.
        */
    void on_action_Rebuild_Oximetry_Index_triggered();

    //! \brief Destroy the CPAP data for the currently selected day, so it can be freshly imported again
    void on_actionPurge_Current_Day_triggered();

    void on_action_Sidebar_Toggle_toggled(bool arg1);

        void on_helpButton_clicked();

    void on_actionView_Statistics_triggered();

    //void on_favouritesList_itemSelectionChanged();

    //void on_favouritesList_itemClicked(QListWidgetItem *item);

    void on_tabWidget_currentChanged(int index);

    void on_filterBookmarks_editingFinished();

    void on_filterBookmarksButton_clicked();

    void on_actionImport_ZEO_Data_triggered();

    void on_actionImport_RemStar_MSeries_Data_triggered();

    void on_actionSleep_Disorder_Terms_Glossary_triggered();

    void on_actionHelp_Support_SleepyHead_Development_triggered();

    void aboutBoxLinkClicked(const QUrl &url);

    void on_actionChange_Language_triggered();

    void on_actionChange_Data_Folder_triggered();

    void on_actionImport_Somnopose_Data_triggered();

    //! \brief Populates the statistics with information.
    void on_statisticsButton_clicked();

    void on_reportModeMonthly_clicked();

    void on_reportModeStandard_clicked();

    void on_actionRebuildCPAP(QAction *action);

    void on_actionPurgeMachine(QAction *action);

    void on_reportModeRange_clicked();

    void on_actionPurgeCurrentDaysOximetry_triggered();

    void logMessage(QString msg);

    void on_importButton_clicked();

    void on_actionToggle_Line_Cursor_toggled(bool arg1);

    void on_actionLeft_Daily_Sidebar_toggled(bool arg1);

    void on_actionDaily_Calendar_toggled(bool arg1);

    void on_actionExport_Journal_triggered();

    void on_actionShow_Performance_Counters_toggled(bool arg1);

    void on_actionExport_CSV_triggered();

    void on_actionExport_Review_triggered();

    void on_mainsplitter_splitterMoved(int pos, int index);

    void on_actionReport_a_Bug_triggered();

    void on_profilesButton_clicked();

    void reloadProfile();

    void on_bookmarkView_anchorClicked(const QUrl &arg1);

    void on_recordsBox_anchorClicked(const QUrl &linkurl);

    void on_statisticsView_anchorClicked(const QUrl &url);


private:
    void importCPAPBackups();
    void finishCPAPImport();
    QList<ImportPath> detectCPAPCards();

    QString getWelcomeHTML();
    void FreeSessions();

    Ui::MainWindow *ui;
    Daily *daily;
    Overview *overview;
    ProfileSelector *profileSelector;
    Welcome * welcome;
    Help * help;
    bool first_load;
    PreferencesDialog *prefdialog;
    QTime logtime;
    QSystemTrayIcon *systray;
    QMenu *systraymenu;
//    gGraphView *SnapshotGraph;
    QString bookmarkFilter;
    bool m_restartRequired;
    volatile bool m_inRecalculation;

    void PopulatePurgeMenu();

    //! \brief Destroy ALL the CPAP data for the selected machine
    void purgeMachine(Machine *);

    int warnidx;
    QStringList warnmsg;
    QTimer wtimer;
};

class ImportDialogScan:public QDialog
{
    Q_OBJECT
public:
    ImportDialogScan(QWidget * parent) :QDialog(parent, Qt::SplashScreen)
    {
    }
    virtual ~ImportDialogScan()
    {
    }
public slots:
    virtual void cancelbutton();
};


#endif // MAINWINDOW_H
