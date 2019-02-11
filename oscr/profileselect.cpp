/* Profile Select Implementation (Login Screen)
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "profileselect.h"
#include <QDebug>
#include <QStringListModel>
#include <QStandardItem>
#include <QDialog>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QHostInfo>
#include <QTimer>
#include <QFontMetrics>
#include <QDir>
#include "ui_profileselect.h"
#include "SleepLib/profiles.h"
#include "newprofile.h"
#include "mainwindow.h"

extern MainWindow * mainwin;

ProfileSelect::ProfileSelect(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProfileSelect)
{
    ui->setupUi(this);
    QStringList str;
    model = new QStandardItemModel(0, 0);

    int i = 0;
    int sel = -1;
    QString name;

    QIcon icon(":/icons/moon.png");

    int w=0;
    QFont font("Sans Serif", 18, QFont::Bold, false);
    ui->listView->setFont(font);

    QFontMetrics fm(font);
    QMap<QString, Profile *>::iterator p;
    for (p = Profiles::profiles.begin(); p != Profiles::profiles.end(); p++) {
        name = p.key();

        if (AppSetting->profileName() == name) {
            sel = i;
        }

        QStandardItem *item = new QStandardItem(name);

        item->setData(p.key(), Qt::UserRole+2);
        item->setEditable(false);

        if (!(*p)->checkLock().isEmpty()) {
            item->setForeground(QBrush(Qt::red));
            item->setText(name+" (open)");
        }

        QRect rect = fm.boundingRect(name);
        if (rect.width() > w) w = rect.width();

        // Profile fonts arern't loaded yet.. Using generic font.
        item->setFont(font);
        model->appendRow(item);
        i++;
    }
    w+=20;
    ui->listView->setMinimumWidth(w);

    proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(model);
    proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    ui->listView->setModel(proxy);
    ui->listView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);

    if (sel >= 0) { ui->listView->setCurrentIndex(proxy->index(sel,0)); } //model->item(sel)->index()); }

    proxy->sort(0, Qt::AscendingOrder);

    m_tries = 0;

    popupMenu = new QMenu(this);
    popupMenu->addAction(tr("Open Profile"), this, SLOT(openProfile()));
    popupMenu->addAction(tr("Edit Profile"), this, SLOT(editProfile()));
    popupMenu->addSeparator();
    popupMenu->addAction(tr("Delete Profile"), this, SLOT(deleteProfile()));

    ui->labelAppName->setText(STR_TR_SleepyHead);
    ui->labelVersion->setText(STR_TR_AppVersion);
    //    if (GIT_BRANCH!="master")
    //        ui->labelBuild->setText(GIT_BRANCH);
    //    else ui->labelBuild->setText(QString());
    ui->labelFolder->setText(GetAppRoot());
    ui->labelFolder->setToolTip("Current SleepyHead data folder\n" + GetAppRoot());

    ui->listView->verticalScrollBar()->setStyleSheet("QScrollBar:vertical {border: 0px solid grey; background: transparent; }"
    "QScrollBar::handle:vertical {"
    "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
    "    stop: 0  rgb(230, 230, 230), stop: 0.5 rgb(255, 255, 255),  stop:1 rgb(230, 230, 230));"
    "    min-height: 0px;"
    "    border: 1px solid gray;"
    "   border-radius: 5px;"
    "}");
}

ProfileSelect::~ProfileSelect()
{
    delete model; // why is this not being cleaned up by Qt?
    delete popupMenu;
    delete ui;
}
void ProfileSelect::earlyExit()
{
    accept();
}
void ProfileSelect::editProfile()
{
    QString name = ui->listView->currentIndex().data(Qt::UserRole+2).toString();
    Profile *profile = Profiles::Get(name);

    if (!profile) { return; }

    bool reallyEdit = false;

    if (profile->user->hasPassword()) {
        QDialog dialog(this, Qt::Dialog);
        QLineEdit *e = new QLineEdit(&dialog);
        e->setEchoMode(QLineEdit::Password);
        dialog.connect(e, SIGNAL(returnPressed()), &dialog, SLOT(accept()));
        dialog.setWindowTitle(tr("Enter Password for %1").arg(name));
        dialog.setMinimumWidth(300);
        QVBoxLayout *lay = new QVBoxLayout();
        dialog.setLayout(lay);
        lay->addWidget(e);
        int tries = 0;

        do {
            e->setText("");

            if (dialog.exec() != QDialog::Accepted) { break; }

            tries++;

            if (profile->user->checkPassword(e->text())) {
                reallyEdit = true;
                break;
            } else {
                if (tries < 3) {
                    QMessageBox::warning(this, STR_MessageBox_Error, tr("Incorrect Password"), QMessageBox::Ok);
                } else {
                    QMessageBox::warning(this, STR_MessageBox_Error, tr("You entered the password wrong too many times."),
                                         QMessageBox::Ok);
                    reject();
                }
            }
        } while (tries < 3);
    } else { reallyEdit = true; }

    if (reallyEdit) {
        NewProfile newprof(this);
        newprof.edit(name);
        newprof.exec();
    }

    //qDebug() << "edit" << name;
}
void ProfileSelect::deleteProfile()
{
    QString name = ui->listView->currentIndex().data(Qt::UserRole+2).toString();

    QDialog confirmdlg;
    QVBoxLayout layout(&confirmdlg);
    QLabel message(QString("<b>"+STR_MessageBox_Warning+":</b> "+tr("You are about to destroy profile '%1'.")+"<br/><br/>"+tr("Enter the word DELETE below to confirm.")).arg(name), &confirmdlg);
    layout.insertWidget(0,&message,1);
    QLineEdit lineedit(&confirmdlg);
    layout.insertWidget(1, &lineedit, 1);
    QHBoxLayout layout2;
    layout.insertLayout(2,&layout2,1);
    QPushButton cancel(QString("&Cancel"), &confirmdlg);
    QPushButton accept(QString("&Delete Profile"), &confirmdlg);
    layout2.addWidget(&cancel);
    layout2.addStretch(1);
    layout2.addWidget(&accept);
    confirmdlg.connect(&cancel, SIGNAL(clicked()), &confirmdlg, SLOT(reject()));
    confirmdlg.connect(&accept, SIGNAL(clicked()), &confirmdlg, SLOT(accept()));
    confirmdlg.connect(&lineedit, SIGNAL(returnPressed()), &confirmdlg, SLOT(accept()));

    if (confirmdlg.exec() != QDialog::Accepted)
        return;

    if (lineedit.text().compare("DELETE")!=0) {
        QMessageBox::information(NULL, tr("Sorry"), tr("You need to enter DELETE in capital letters."), QMessageBox::Ok);
        return;
    }

    Profile * profile = Profiles::profiles[name];
    p_profile = profile;
    // Hmmmmm.....
//    if (!profile->Load()) {
//        QMessageBox::warning(this, STR_MessageBox_Error,
//            QString(tr("Could not open profile.. You will need to delete this profile directory manually")+
//            "\n\n"+tr("You will find it under the following location:")+"\n\n%1").arg(QDir::toNativeSeparators(GetAppRoot() + "/Profiles/" + profile->user->userName())), QMessageBox::Ok);
//            return;
//    }
    bool reallydelete = false;
    if (profile->user->hasPassword()) {
        QDialog dialog(this, Qt::Dialog);
        QLineEdit *e = new QLineEdit(&dialog);
        e->setEchoMode(QLineEdit::Password);
        dialog.connect(e, SIGNAL(returnPressed()), &dialog, SLOT(accept()));
        dialog.setWindowTitle(tr("Enter Password for %1").arg(name));
        dialog.setMinimumWidth(300);
        QVBoxLayout *lay = new QVBoxLayout();
        dialog.setLayout(lay);
        lay->addWidget(e);
        int tries = 0;

        do {
            e->setText("");

            if (dialog.exec() != QDialog::Accepted) { break; }

            tries++;

            if (profile->user->checkPassword(e->text())) {
                reallydelete = true;
                break;
            } else {
                if (tries < 3) {
                    QMessageBox::warning(this, STR_MessageBox_Error, tr("You entered an incorrect password"), QMessageBox::Ok);
                } else {
                    QMessageBox::warning(this, STR_MessageBox_Error,
                                         tr("If you're trying to delete because you forgot the password, you need to delete it manually."),
                                         QMessageBox::Ok);
                }
            }
        } while (tries < 3);
    } else { reallydelete = true; }

    if (reallydelete) {
        QString path = profile->Get(PrefMacro(STR_GEN_DataFolder));

        if (!path.isEmpty()) {
            if (!removeDir(path)) {
                QMessageBox::information(this, STR_MessageBox_Error,
                                         tr("There was an error deleting the profile directory, you need to manually remove it.")+QString("\n\n%1").arg(path),
                                         QMessageBox::Ok);
            }
            qDebug() << "Delete" << path;
            QMessageBox::information(this, STR_MessageBox_Information, tr("Profile '%1' was succesfully deleted").arg(name),QMessageBox::Ok);
        }

        int row = ui->listView->currentIndex().row();
        proxy->removeRow(row);
        delete p_profile;
        p_profile = nullptr;
    }
}

//! \fn ProfileSelect::QuickLogin()
//! \brief For programmatically bypassing the login window
void ProfileSelect::QuickLogin()
{
    on_listView_activated(ui->listView->currentIndex());
}

void ProfileSelect::on_selectButton_clicked()
{
    on_listView_activated(ui->listView->currentIndex());
}
void ProfileSelect::openProfile()
{
    on_listView_activated(ui->listView->currentIndex());
}

void ProfileSelect::on_newProfileButton_clicked()
{
    NewProfile newprof(this);
    newprof.skipWelcomeScreen();
    newprof.setWindowTitle(tr("Create new profile"));

    if (newprof.exec() == NewProfile::Rejected) {
        //        reject();
    } else { accept(); }
}


//! \fn ProfileSelect::on_listView_activated(const QModelIndex &index)
//! \brief Process the selected login, requesting passwords if required.
void ProfileSelect::on_listView_activated(const QModelIndex &index)
{
    QString name = index.data(Qt::UserRole+2).toString();
    Profile *profile = Profiles::profiles[name];

    if (!profile) { return; }
    if (!profile->isOpen()) {
        p_profile = profile;
        // Do this in case user renames the directory (otherwise it won't load)
        // Essentially makes the folder name the user name, but whatever..
        // TODO: Change the profile editor one day to make it rename the actual folder
        profile->p_preferences[STR_UI_UserName] = name;
    }

    if (!profile->user->hasPassword()) {
        m_selectedProfile = name;
        AppSetting->setProfileName(name);
        accept();
        return;
    } else {
        int tries = 0;

        do {
            QDialog dialog(this, Qt::Dialog);
            QLineEdit *e = new QLineEdit(&dialog);
            e->setEchoMode(QLineEdit::Password);
            dialog.connect(e, SIGNAL(returnPressed()), &dialog, SLOT(accept()));
            dialog.setWindowTitle(tr("Enter Password"));
            QVBoxLayout *lay = new QVBoxLayout();
            dialog.setLayout(lay);
            lay->addWidget(e);
            dialog.exec();

            if (profile->user->checkPassword(e->text())) {
                m_selectedProfile = name;
                AppSetting->setProfileName(name);
                accept();
                return;
            }

            tries++;

            if (tries < 3) {
                QMessageBox::warning(this, STR_MessageBox_Error, tr("Incorrect Password"), QMessageBox::Ok);
            } else {
                QMessageBox::warning(this, STR_MessageBox_Error,
                                     tr("You entered an Incorrect Password too many times. Exiting!"), QMessageBox::Ok);
            }
        } while (tries < 3);
    }

    reject();
    return;
}

void ProfileSelect::on_listView_customContextMenuRequested(const QPoint &pos)
{
    popupMenu->popup(QWidget::mapToGlobal(pos));
}

void ProfileSelect::on_pushButton_clicked()
{
    mainwin->RestartApplication(false, "-d");
}

void ProfileSelect::on_filter_textChanged(const QString &arg1)
{
    QRegExp regExp("*"+arg1+"*", Qt::CaseInsensitive, QRegExp::Wildcard);
    proxy->setFilterRegExp(regExp);
}
