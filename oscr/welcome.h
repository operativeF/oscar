/* Welcome page Header
 *
 * Copyright (c) 2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of Source Code. */

#ifndef WELCOME_H
#define WELCOME_H

#include <QWidget>

namespace Ui {
class Welcome;
}

class Welcome : public QWidget
{
    Q_OBJECT

public:
    explicit Welcome(QWidget *parent = 0);
    ~Welcome();

    void refreshPage();

private slots:
    void on_dailyButton_clicked();

    void on_overviewButton_clicked();

    void on_statisticsButton_clicked();

    void on_oximetryButton_clicked();

    void on_importButton_clicked();

private:
    QString GenerateCPAPHTML();
    QString GenerateOxiHTML();
    QPixmap pixmap;
    Ui::Welcome *ui;
};

#endif // WELCOME_H
