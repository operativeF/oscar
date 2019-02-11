/* SleepyHead AboutDialog Implementation
 *
 * Date created: 7/5/2018
 *
 * Copyright (c) 2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Source Directory. */

#include <QDesktopServices>
#include <QFile>

#include "version.h"
#include "SleepLib/appsettings.h"
#include "aboutdialog.h"
#include "ui_aboutdialog.h"


AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    ui->licenseText->setHtml(getLicense());
    ui->relnotesText->setHtml(getRelnotes());
    ui->versionLabel->setText(VersionString);

    QString gitrev = gitRevision();

    if (!gitrev.isEmpty()) {
        gitrev = tr("Revision: %1").arg(QString("<a href='https://gitlab.com/sleepyhead/sleepyhead-code/commit/%1'>%1</a>").arg(gitrev))+"<br/>"
                +tr("Branch: %1").arg(QString("<a href='https://gitlab.com/sleepyhead/sleepyhead-code/commits/%1'>%1</a>").arg(gitBranch()))+"<br/>"
                +tr("Build Date: %1").arg(__DATE__)+"<br/>"
                +tr("Graphics Engine: %1").arg(getGraphicsEngine());
    }

    QString path = GetAppRoot();
    QString text = gitrev + "<br/><br/><a href=\"file:///"+path+"\">"+tr("Show data folder")+"</a>";
    ui->infoLabel->setText(text);


    setWindowTitle(tr("About SleepyHead"));
    setMinimumSize(QSize(400,400));
    connect(ui->closeButton, SIGNAL(clicked(bool)), this, SLOT(accept()));

    int idx=AppSetting->showAboutDialog();
    if (idx<0) idx = 0; // start in about tab
    ui->tabWidget->setCurrentIndex(idx);
}

AboutDialog::~AboutDialog()
{
    disconnect(ui->closeButton, SIGNAL(clicked(bool)), this, SLOT(accept()));
    delete ui;
}

void AboutDialog::on_donateButton_clicked()
{
    QDesktopServices::openUrl(QUrl("http://sleepyhead.jedimark.net/donate.php"));
}

QString AboutDialog::getRelnotes()
{
    QFile clfile(":/docs/release_notes.html");
    QString changeLog = tr("Sorry, could not locate changelog.");
    if (clfile.open(QIODevice::ReadOnly)) {
        //Todo, write XML parser and only show the latest..
        //QTextStream ts(&clfile);
        changeLog = clfile.readAll();
    }

    QString text = "<html>"
    "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"></head>"
    "<body><span style=\" font-size:20pt;\">"+tr("Release Notes")+"</span><br/>"
    "<span style=\" font-size:14pt;\">"+tr("SleepyHead v%1").arg(VersionString)+"</span>"
    "<hr/>";
    if (ReleaseStatus != "r") {
        text += "<p><font color='red' size=+1><b>"+tr("Important:")+"</b></font> "
        "<font size=+1><i>"+tr("As this is a pre-release version, it is recommended that you <b>back up your data folder manually</b> before proceding, because attempting to roll back later may break things.")+"</i></font></p><hr/>";
    }
    text += changeLog;
    text += "</body></html>";
    return text;
}

QString AboutDialog::getLicense()
{
    QString text;
    QString licenseFile = ":/docs/GPLv3-"+AppSetting->language();
    if (!QFile::exists(licenseFile)) {
        ui->licenceLabel->setText(tr("To see if the license text is available in your language, see %1.").arg("<a href=\"https://www.gnu.org/licenses/translations.en.html\">https://www.gnu.org/licenses/translations.en.html</a>"));
        ui->licenceLabel->setVisible(true);
        licenseFile = ":/docs/GPLv3-en_US";
    } else {
        ui->licenceLabel->setVisible(false);
    }

    QFile file(licenseFile);
    if (file.open(QIODevice::ReadOnly)) {
        text = "<div align=center>"+QString(file.readAll()).replace("\n","<br/>")+"</div>";
    }
    return text;
}
