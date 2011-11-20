/*
 SleepyHead Preferences Dialog GUI Headers
 Copyright (c)2011 Mark Watkins <jedimark@users.sourceforge.net>
 License: GPL
*/

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QModelIndex>
#include <QStringListModel>
#include "SleepLib/profiles.h"

namespace Ui {
    class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent, Profile * _profile);
    ~PreferencesDialog();
    void Save();
    void RefreshLastChecked();
private slots:
    void on_eventTable_doubleClicked(const QModelIndex &index);
    void on_combineSlider_valueChanged(int value);

    void on_IgnoreSlider_valueChanged(int value);

    void on_useGraphSnapshots_toggled(bool checked);

    void on_checkForUpdatesButton_clicked();

    void on_addImportLocation_clicked();

    void on_removeImportLocation_clicked();

private:
    Ui::PreferencesDialog *ui;
    Profile * profile;
    QHash<int,QColor> m_new_colors;
    QStringList importLocations;
    QStringListModel *importModel;

};

#endif // PREFERENCESDIALOG_H
