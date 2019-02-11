/* UpdaterWindow
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef UPDATEWINDOW_H
#define UPDATEWINDOW_H

#include <QSystemTrayIcon>
#include <QNetworkAccessManager>
#include <QTableWidgetItem>
#include <QMenu>
#include <QMainWindow>
#include <QUrl>

#include "version.h"
#include "updateparser.h"

namespace Ui {
class UpdaterWindow;
}

/*! \enum RequestMode
    \brief Used in replyFinished() to differentiate the current update task.
*/
enum RequestMode { RM_None, RM_CheckUpdates, RM_GetFile };


/*! \class UpdaterWindow
    \brief Auto-Update Module for SleepyHead

    This class handles the complete Auto-Update procedure for SleepyHead, it does the network checks,
    parses the update.xml from SourceForge host, checks for any new updates, and provides the UI
    and mechanisms to download and replace the binaries according to what is specified in update.xml.
  */
class UpdaterWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit UpdaterWindow(QWidget *parent = 0);
    ~UpdaterWindow();

    //! Start the
    void checkForUpdates();

    /*! \fn ParseUpdateXML(QIODevice * dev)
        \brief Parses the update.xml from either QFile or QNetworkReply source
        */
    //void ParseUpdateXML(QIODevice *dev);
    void ParseUpdatesXML(QIODevice *dev);
    void ParseLatestVersion(QIODevice *dev);

  protected slots:
    void updateFinished(QNetworkReply *reply);

   // //! \brief Network reply completed
    //void replyFinished(QNetworkReply *reply);

    ////! \brief Update the progress bars as data is received
    //void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

    ////! \brief Save incomming data
    //void dataReceived();

//    //! \brief Request a file to download
//    void requestFile();

    //! \brief Request the update.xml file
    void downloadUpdateXML();

  private slots:
    //! \brief Just close the Updater window
    void on_CloseButton_clicked();

  //  //! \brief Start processing the download que, and applying the updates
//    void on_upgradeButton_clicked();

//    //! \brief Selects the next file in the download queue
//    void upgradeNext();

    //! \brief Click on finished, restart if app has been upgraded, otherwise just close the window.
    void on_FinishedButton_clicked();

  private:

    //! \brief Holds the results of parsing the update.xml file
    UpdateParser updateparser;

    // new parser
    UpdatesParser updatesparser;

    Ui::UpdaterWindow *ui;

    RequestMode requestmode;
    QTime dltime;

    Update *update;
    Release *release;
    QFile file;
    QNetworkAccessManager *netmanager;
    QNetworkReply *reply;
    QList<Update *> updates;
    int current_row;
    bool success;
    QUrl update_url; // for update.xml redirects..
};

#endif // UPDATEWINDOW_H
