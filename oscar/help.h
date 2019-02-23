/* SleepyHead Help Header
 *
 * Copyright (c) 2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef HELP_H
#define HELP_H

#include <QPrinter>
#include <QHelpEngine>
#include <QTabWidget>
#include <QWidget>
#include "mytextbrowser.h"

namespace Ui {
class Help;
}

class HelpBrowser : public QTextBrowser
{
public:
    HelpBrowser(QHelpEngine* helpEngine, QWidget* parent = 0);
    virtual QVariant loadResource(int type, const QUrl &url) Q_DECL_OVERRIDE;
    QString helpNamespace;
private:
    QHelpEngine * helpEngine;
};

class Help : public QWidget
{
    Q_OBJECT

public:
    explicit Help(QWidget *parent = 0);
    ~Help();

    void print(QPrinter * printer) { helpBrowser->print(printer); }

private slots:
    void on_backButton_clicked();

    void on_forwardButton_clicked();

    void on_homeButton_clicked();

    void startup();

    void on_searchBar_returnPressed();
    void on_searchComplete(int results);

    void indexingFinished();

    void forwardAvailable(bool b);
    void backwardAvailable(bool b);
    void requestShowLink(const QUrl & link);
    void on_languageWarningCheckbox_clicked(bool checked);

private:
    Ui::Help *ui;
    QHelpEngine *helpEngine;
    QTabWidget * tabWidget;
    HelpBrowser * helpBrowser;
    MyTextBrowser * resultWidget;
    bool searchReady;
    QString helpNamespace;
    bool helpLoaded;
    QString language;
};

#endif // HELP_H
