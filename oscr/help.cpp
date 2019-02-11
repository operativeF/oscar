/* SleepyHead Help Implementation
 *
 * Copyright (c) 2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#include <QtHelp>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QDir>

#include "SleepLib/common.h"
#include "translation.h"
#include "help.h"
#include "ui_help.h"

Help::Help(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Help)
{
    ui->setupUi(this);

    QString helpRoot = appResourcePath() + "/Help/";
    QString helpIndex = helpRoot + "index.qhc";

    QDir dir(helpRoot);
    QStringList nameFilters = QStringList("*.qch");
    QStringList helpfiles = dir.entryList(nameFilters, QDir::Files | QDir::Readable);

    language = currentLanguage();
    QString helpFile;

    for (const QString & file : helpfiles) {
        if (file.endsWith(language+".qch")) {
            helpFile = helpRoot+file;
            break;
        }
    }
    if (helpFile.isEmpty() && (language != DefaultLanguage)) {
        ui->languageWarningMessage->setText(tr("Help Files are not yet available for %1 and will display in %2.").arg(lookupLanguageName(language)).arg(lookupLanguageName(DefaultLanguage)));
        language = DefaultLanguage;
        for (const QString & file : helpfiles) {
            if (file.endsWith(language+".qch")) {
                helpFile = helpRoot+file;
            }
            break;
        }
    }
    if (helpFile.isEmpty()) {
        ui->languageWarningMessage->setText(tr("Help files do not appear to be present."));
        // Still empty, install is dodgy
        // Copy en_US out of resource??
        // For now I just don't care, if the user screws up.. tough
    }

    helpLoaded = false;
    // Delete the crappy qhc so we can generate our own.
    if (QFile::exists(helpIndex)) QFile::remove(helpIndex);

    helpEngine = new QHelpEngine(helpIndex);
    helpNamespace = "jedimark.net.SleepyHeadGuide";

    if (!helpFile.isEmpty()) {
        if (!helpEngine->setupData()) {
            ui->languageWarningMessage->setText(tr("HelpEngine did not set up correctly"));
        } else if (helpEngine->registerDocumentation(helpFile)) {
            qDebug() << "Registered" << helpFile;
            helpLoaded = true;
            ui->languageWarning->setVisible(false);
        } else {
            ui->languageWarningMessage->setText(tr("HelpEngine could not register documentation correctly."));
            qDebug() << helpEngine->error();
        }
    }

    helpBrowser = new HelpBrowser(helpEngine);

    tabWidget = new QTabWidget;
    tabWidget->setMaximumWidth(250);

    tabWidget->addTab(helpEngine->contentWidget(), tr("Contents"));
    tabWidget->addTab(helpEngine->indexWidget(), tr("Index"));

    resultWidget = new MyTextBrowser(this);
    resultWidget->setOpenLinks(false);
    tabWidget->addTab(resultWidget, tr("Search"));

    ui->splitter->insertWidget(0, tabWidget);
    ui->splitter->insertWidget(1, helpBrowser);

    ui->forwardButton->setEnabled(false);
    ui->backButton->setEnabled(false);


    if (!helpLoaded) {
        QString html = "<html><body><div align=\"center\" valign=\"center\"><img src=\"qrc://docs/sheep.png\"><br/><h2>"+tr("No documentation available")+"</h2></div></body></html>";
        helpBrowser->setHtml(html);
        return;
    } else {
        QTimer::singleShot(50,this, SLOT(startup()));

        connect(helpBrowser, SIGNAL(forwardAvailable(bool)), this, SLOT(forwardAvailable(bool)));
        connect(helpBrowser, SIGNAL(backwardAvailable(bool)), this, SLOT(backwardAvailable(bool)));
        connect(helpEngine->contentWidget(), SIGNAL(linkActivated(QUrl)), helpBrowser, SLOT(setSource(QUrl)));
        connect(helpEngine->indexWidget(), SIGNAL(linkActivated(QUrl, QString)), helpBrowser, SLOT(setSource(QUrl)));

        connect(helpEngine->searchEngine(), SIGNAL(searchingFinished(int)), this, SLOT(on_searchComplete(int)));
        connect(helpEngine->searchEngine(), SIGNAL(indexingFinished()), this, SLOT(indexingFinished()));

        connect(resultWidget, SIGNAL(anchorClicked(QUrl)), this, SLOT(requestShowLink(QUrl)));

        searchReady = false;
        helpEngine->searchEngine()->reindexDocumentation();
        helpBrowser->setSource(QUrl(QString("qthelp://%1/doc/index.html").arg(helpNamespace)));
    }
}

Help::~Help()
{
    disconnect(resultWidget, SIGNAL(anchorClicked(QUrl)), this, SLOT(requestShowLink(QUrl)));
    disconnect(helpEngine->searchEngine(), SIGNAL(indexingFinished()), this, SLOT(indexingFinished()));
    disconnect(helpEngine->searchEngine(), SIGNAL(searchingFinished(int)), this, SLOT(on_searchComplete(int)));

    disconnect(helpEngine->contentWidget(), SIGNAL(linkActivated(QUrl)), helpBrowser, SLOT(setSource(QUrl)));
    disconnect(helpEngine->indexWidget(), SIGNAL(linkActivated(QUrl, QString)), helpBrowser, SLOT(setSource(QUrl)));
    disconnect(helpBrowser, SIGNAL(backwardAvailable(bool)), this, SLOT(backwardAvailable(bool)));
    disconnect(helpBrowser, SIGNAL(forwardAvailable(bool)), this, SLOT(forwardAvailable(bool)));

    delete helpEngine;
    delete ui;
}

void Help::startup()
{
    helpEngine->contentWidget()->expandToDepth(0);
}

HelpBrowser::HelpBrowser(QHelpEngine* helpEngine, QWidget* parent):
    QTextBrowser(parent), helpEngine(helpEngine)
{
}

QVariant HelpBrowser::loadResource(int type, const QUrl &name)
{
    if (name.scheme() == "qthelp") {
        qDebug() << "Loading" << name.toString();
        return QVariant(helpEngine->fileData(name));
    } else

//    QString path = name.path(QUrl::FullyEncoded).replace("/./","/");
//    if (!path.startsWith("/doc")) path="/doc"+path;
//    QString urlstr = "qthelp://"+helpNamespace+path;
//    QByteArray bytes = helpEngine->fileData(urlstr);
//    if (bytes.size()>0) return QVariant(bytes);

    if (type == QTextDocument::ImageResource && name.scheme().compare(QLatin1String(""), Qt::CaseInsensitive) == 0) {
       static QRegularExpression re("^image/[^;]+;base64,.+={0,2}$");
       QRegularExpressionMatch match = re.match(name.path());
       if (match.hasMatch())
         return QVariant();
     }
     return QTextBrowser::loadResource(type, name);
}

void Help::on_backButton_clicked()
{
    helpBrowser->backward();
}

void Help::on_forwardButton_clicked()
{
    helpBrowser->forward();
}

void Help::on_homeButton_clicked()
{
    if (!helpLoaded) return;
    QByteArray index = helpEngine->fileData(QUrl(QString("qthelp://%1/doc/index.html").arg(helpNamespace)));
    helpBrowser->setHtml(index);
}
void Help::on_searchComplete(int count)
{
    if (!searchReady) {
        QString html = "<h1>"+tr("Please wait a bit.. Indexing still in progress")+"</h1>";
        helpBrowser->setText(html);
        return;
    }

    QHelpSearchEngine * search = helpEngine->searchEngine();
    QVector<QHelpSearchResult> results = search->searchResults(0, count);

//    ui->searchBar->blockSignals(true);
    ui->searchBar->setText(QString());
//    ui->searchBar->blockSignals(false);

    QString html = "<html><head><style type=\"text/css\">"
                   "a:link, a:visited { color: inherit; text-decoration: none; font-weight: normal;}"
                   "a:hover { background-color: inherit; color: #ff8888; text-decoration: underline; font-weight: bold; }"
                   "</style></head><body>";

    QString queryString;
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QList<QHelpSearchQuery> results = search->query();
    if (results.size()>0) {
        queryString=results[0].wordList.at(0);
    }
#else
    queryString = search->searchInput();
#endif
    QString a = (results.size() == 0) ? tr("No") : QString("%1").arg(results.size());
    html+="<font size=+1><b>"+ tr("%1 result(s) for \"%2\"").arg(a).arg(queryString)+"</b></font>";
    if (results.size()>0) html += " [<font size=-1><a href='clear'>"+tr("clear")+"</a></font>]";

    for (QHelpSearchResult & result : results) {
        QString title = result.url().toString().section("/",-1);
        html += QString("<p><a href=\"%1\"><b>%2:</b> %3</a></p>").arg(result.url().toString()).arg(title).arg(result.snippet());
    }

    html += "</body></html>";
    resultWidget->setText(html);
    tabWidget->setCurrentWidget(resultWidget);
}

void Help::on_searchBar_returnPressed()
{
    if (!helpLoaded) {
        ui->searchBar->clear();
        return;
    }
    QHelpSearchEngine * search = helpEngine->searchEngine();

    QString str=ui->searchBar->text();

 #if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QList<QHelpSearchQuery> query;
    query.push_back(QHelpSearchQuery(QHelpSearchQuery::FUZZY, QStringList(str)));
    search->search(query);
#else
    search->search(str);
#endif
}

void Help::indexingFinished()
{
    searchReady = true;
}

void Help::forwardAvailable(bool b)
{
    ui->forwardButton->setEnabled(b);
}
void Help::backwardAvailable(bool b)
{
    ui->backButton->setEnabled(b);
}

void Help::requestShowLink(const QUrl & link)
{
    if (link.toString() == "clear") {
        resultWidget->clear();
    } else {
        helpBrowser->setSource(link);
    }
}

void Help::on_languageWarningCheckbox_clicked(bool checked)
{
    ui->languageWarning->setVisible(!checked);
}
