/* Oximeter Import Wizard Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef OXIMETERIMPORT_H
#define OXIMETERIMPORT_H

#include <QDialog>

#include "Graphs/gGraphView.h"
#include "Graphs/gLineChart.h"
#include "SleepLib/serialoximeter.h"
#include "sessionbar.h"

namespace Ui {
class OximeterImport;
}

enum OximeterImportMode {
    IM_UNDEFINED = 0, IM_LIVE, IM_RECORDING, IM_FILE
};

class OximeterImport : public QDialog
{
    Q_OBJECT

public:
    explicit OximeterImport(QWidget *parent = 0);
    ~OximeterImport();


private slots:
    void on_nextButton_clicked();

    void on_directImportButton_clicked();

    void doUpdateProgress(int, int);
    void on_fileImportButton_clicked();

    void on_liveImportButton_clicked();

    void on_retryButton_clicked();

    void on_stopButton_clicked();

    void on_calendarWidget_clicked(const QDate &date);

    void on_calendarWidget_selectionChanged();

    void onSessionSelected(Session * session);

    void on_sessionBackButton_clicked();

    void on_sessionForwardButton_clicked();

    void on_radioSyncCPAP_clicked();

    void on_radioSyncOximeter_clicked();

    void on_cancelButton_clicked();

    void on_showLiveGraphs_clicked(bool checked);

    void on_skipWelcomeCheckbox_clicked(bool checked);

    void on_informationButton_clicked();

    void on_syncButton_clicked();

    void on_saveButton_clicked();

    void chooseSession();

    void on_chooseSessionButton_clicked();

    void on_oximeterType_currentIndexChanged(int index);

    void on_cms50CheckName_clicked(bool checked);

    void on_cms50SyncTime_clicked(bool checked);

    void on_cms50DeviceName_textEdited(const QString &arg1);

    void on_textBrowser_anchorClicked(const QUrl &arg1);

protected slots:
    void on_updatePlethy(QByteArray plethy);
    void finishedRecording();
    void finishedImport(SerialOximeter*);
    void updateLiveDisplay();

protected:
    SerialOximeter * detectOximeter();
    void updateStatus(QString msg);
    void doImport();
    void setInformation();

private:
    Ui::OximeterImport *ui;
    SerialOximeter * oximodule;
    gGraphView * liveView;
    gGraph * plethyGraph;
    Session * session;
    Day * dummyday;
    gLineChart * plethyChart;
    SessionBar * sessbar;
    EventList * ELplethy;
    qint64 start_ti, ti;
    QTimer updateTimer;
    OximeterImportMode importMode;

    int pulse;
    int spo2;

    bool selecting_session;
    QList<int> chosen_sessions;
};

#endif // OXIMETERIMPORT_H
