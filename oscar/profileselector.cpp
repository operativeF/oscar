/* Profile Selector Implementation
 *
 * Copyright (c) 2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QMessageBox>

#include "profileselector.h"
#include "ui_profileselector.h"


#include "SleepLib/profiles.h"
#include "daily.h"
#include "overview.h"
#include "statistics.h"
#include "mainwindow.h"
#include "newprofile.h"

extern MainWindow * mainwin;

MySortFilterProxyModel2::MySortFilterProxyModel2(QObject *parent)
    : QSortFilterProxyModel(parent)
{

}

bool MySortFilterProxyModel2::filterAcceptsRow(int sourceRow,
        const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    QModelIndex index1 = sourceModel()->index(sourceRow, 1, sourceParent);
    QModelIndex index2 = sourceModel()->index(sourceRow, 2, sourceParent);
    QModelIndex index5 = sourceModel()->index(sourceRow, 5, sourceParent);

    return (sourceModel()->data(index0).toString().contains(filterRegExp())
            || sourceModel()->data(index1).toString().contains(filterRegExp())
            || sourceModel()->data(index2).toString().contains(filterRegExp())
            || sourceModel()->data(index5).toString().contains(filterRegExp())
           );
}


ProfileSelector::ProfileSelector(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProfileSelector)
{
    ui->setupUi(this);
    model = nullptr;
    proxy = nullptr;

    showDiskUsage = false;  // in case I want to preference it later
    on_diskSpaceInfo_linkActivated(showDiskUsage ? "show" : "hide");

    ui->versionLabel->setText(VersionString);
    ui->diskSpaceInfo->setVisible(false);

    QItemSelectionModel * sm = ui->profileView->selectionModel();
    if (sm) connect(sm, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(on_selectionChanged(QModelIndex,QModelIndex)));
    ui->buttonEditProfile->setEnabled(false);
}

ProfileSelector::~ProfileSelector()
{
    QItemSelectionModel * sm = ui->profileView->selectionModel();
    disconnect(sm, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(on_selectionChanged(QModelIndex,QModelIndex)));
    delete ui;
}

const Qt::GlobalColor openProfileHighlightColor = Qt::darkGreen;

void ProfileSelector::updateProfileList()
{
    QItemSelectionModel * sm = ui->profileView->selectionModel();
    if (sm) disconnect(sm, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(on_selectionChanged(QModelIndex,QModelIndex)));

    QString name;
    int w=0;
    if (proxy) delete proxy;
    if (model) delete model;

    const int columns = 6;
    model = new QStandardItemModel(0, columns, this);
    model->setHeaderData(0, Qt::Horizontal, tr("Profile"));
    model->setHeaderData(1, Qt::Horizontal, tr("Ventilator Brand"));
    model->setHeaderData(2, Qt::Horizontal, tr("Ventilator Model"));
    model->setHeaderData(3, Qt::Horizontal, tr("Other Data"));
    model->setHeaderData(4, Qt::Horizontal, tr("Last Imported"));
    model->setHeaderData(5, Qt::Horizontal, tr("Name"));

    ui->profileView->setStyleSheet("QHeaderView::section { background-color:lightgrey }");

    int row = 0;
//    int sel = -1;

    QFontMetrics fm(ui->profileView->font());
    //#if QT_VERSION < QT_VERSION_CHECK(5,11,0) // not sure when this was fixed
        QPalette palette = ui->profileView->palette();
        palette.setColor(QPalette::Highlight, "#3a7fc2");
        palette.setColor(QPalette::HighlightedText, "white");
        ui->profileView->setPalette(palette);
    //#endif

    ui->profileView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->profileView->setFocusPolicy(Qt::NoFocus);
//    ui->profileView->setSelectionMode(QAbstractItemView::NoSelection);
    ui->profileView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->profileView->setSelectionMode(QAbstractItemView::SingleSelection);


    QMap<QString, Profile *>::iterator pi;
    for (pi = Profiles::profiles.begin(); pi != Profiles::profiles.end(); pi++) {
        Profile *prof = pi.value();
        name = pi.key();

//        if (AppSetting->profileName() == name) {
//            sel = row;
//        }

        Machine * mach = prof->GetMachine(MT_CPAP);  // only interested in last cpap machine...
        if (!mach) {
            qDebug() << "Couldn't find machine info for" << name;
        }

        model->insertRows(row, 1, QModelIndex());
        // Problem: Can't access profile details until it's loaded.
        QString usersname;
        if (!prof->user->lastName().isEmpty()) {
            usersname = tr("%1, %2").arg(prof->user->lastName()).arg(prof->user->firstName());
        }

        model->setData(model->index(row, 0, QModelIndex()), name);

        model->setData(model->index(row, 0, QModelIndex()), name, Qt::UserRole+2);
        model->setData(model->index(row, 5, QModelIndex()), usersname);
        if (mach) {
            model->setData(model->index(row, 1, QModelIndex()), mach->brand());
            model->setData(model->index(row, 2, QModelIndex()), mach->series()+" "+mach->model());
            model->setData(model->index(row, 4, QModelIndex()), mach->lastImported().toString(Qt::SystemLocaleShortDate));
        }
        QBrush bg = QColor(Qt::black);
        QFont font = QApplication::font();
        if (prof == p_profile) {
            bg = QBrush(openProfileHighlightColor);
            font.setBold(true);
        }
        for (int i=0; i<columns; i++) {
            model->setData(model->index(row, i, QModelIndex()), bg, Qt::ForegroundRole);
            //model->setData(model->index(row, i, QModelIndex()), font, Qt::FontRole);
        }

        QRect rect = fm.boundingRect(name);
        if (rect.width() > w) w = rect.width();

        // Profile fonts arern't loaded yet.. Using generic font.
        //item->setFont(font);
        //model->appendRow(item);
        row++;
    }
    w+=20;
//    ui->profileView->setMinimumWidth(w);

    proxy = new MySortFilterProxyModel2(this);
    proxy->setSourceModel(model);
    proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    ui->profileView->setModel(proxy);

    QHeaderView *headerView = ui->profileView->horizontalHeader();
    headerView->setStretchLastSection(true);
    headerView->setSectionResizeMode(QHeaderView::Stretch);

    sm = ui->profileView->selectionModel();
    connect(sm, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(on_selectionChanged(QModelIndex,QModelIndex)));

}

void ProfileSelector::updateProfileHighlight(QString name)
{

    QFont font = QApplication::font();
    font.setBold(false);
    QBrush bg = QColor(Qt::black);
    for (int row=0;row < model->rowCount(); row++) {
        for (int i=0; i<model->columnCount(); i++) {
            QModelIndex idx = model->index(row, i, QModelIndex());
            model->setData(idx, bg, Qt::ForegroundRole);
            model->setData(idx, font, Qt::FontRole);
        }
    }

    bg = QBrush(openProfileHighlightColor);
    font.setBold(true);

    for (int row=0;row < proxy->rowCount(); row++) {
        QModelIndex idx = proxy->index(row, 0, QModelIndex());

        if (proxy->data(idx).toString().compare(name)==0) {
            on_selectionChanged(idx, QModelIndex());

            for (int i=0; i<proxy->columnCount(); i++) {
                idx = proxy->index(row, i, QModelIndex());

                proxy->setData(idx, bg, Qt::ForegroundRole);
                proxy->setData(idx, font, Qt::FontRole);
            }
            break;
        }

    }
}

Profile *ProfileSelector::SelectProfile(QString profname, bool skippassword=false)
{
    auto pit = Profiles::profiles.find(profname);
    if (pit == Profiles::profiles.end()) return nullptr;

    Profile * prof = pit.value();

    if (prof != p_profile) {
        if (prof->user->hasPassword() && !skippassword) {
            QDialog dialog(this, Qt::Dialog);
            QLineEdit *e = new QLineEdit(&dialog);
            e->setEchoMode(QLineEdit::Password);
            dialog.connect(e, SIGNAL(returnPressed()), &dialog, SLOT(accept()));
            dialog.setWindowTitle(tr("Enter Password for %1").arg(profname));
            dialog.setMinimumWidth(300);
            QVBoxLayout *lay = new QVBoxLayout();
            dialog.setLayout(lay);
            lay->addWidget(e);
            int tries = 0;
            bool succeeded = false;

            do {
                e->setText("");

                if (dialog.exec() != QDialog::Accepted) { break; }

                tries++;

                if (prof->user->checkPassword(e->text())) {
                    succeeded = true;
                    break;
                } else {
                    if (tries < 3) {
                        QMessageBox::warning(this, STR_MessageBox_Error, tr("You entered an incorrect password"), QMessageBox::Ok);
                    } else {
                        QMessageBox::warning(this, STR_MessageBox_Error,
                                             tr("Forgot your password?")+"\n"+tr("Ask on the forums how to reset it, it's actually pretty easy."),
                                             QMessageBox::Ok);
                    }
                }
            } while (tries < 3);
            if (!succeeded) return nullptr;
        }

        // Unselect everything in ProfileView

        updateProfileHighlight(profname);
    }

    return prof;
}

void ProfileSelector::on_profileView_doubleClicked(const QModelIndex &index)
{
    QModelIndex idx = proxy->index(index.row(), 0, QModelIndex());
    QString profname = proxy->data(idx, Qt::UserRole+2).toString();

    //if (SelectProfile(profname)) {
    mainwin->OpenProfile(profname);
    //}
}

void ProfileSelector::on_profileFilter_textChanged(const QString &arg1)
{
    QRegExp regExp("*"+arg1+"*", Qt::CaseInsensitive, QRegExp::Wildcard);
    proxy->setFilterRegExp(regExp);
}

void ProfileSelector::on_buttonOpenProfile_clicked()
{
    if (ui->profileView->currentIndex().isValid()) {
        QString name = proxy->data(proxy->index(ui->profileView->currentIndex().row(), 0, QModelIndex()), Qt::UserRole+2).toString();
        mainwin->OpenProfile(name);
    }
}

void ProfileSelector::on_buttonEditProfile_clicked()
{
    if (ui->profileView->currentIndex().isValid()) {
        QString name = proxy->data(proxy->index(ui->profileView->currentIndex().row(), 0, QModelIndex()), Qt::UserRole+2).toString();
        qDebug() << "Editing" << name;

        Profile * prof = Profiles::profiles[name];
        //SelectProfile(name); // may not be necessary...

        NewProfile *newprof = new NewProfile(this);
        newprof->edit(name);
        newprof->setWindowModality(Qt::ApplicationModal);
        newprof->setModal(true);
        if (newprof->exec() != NewProfile::Rejected) {
            QString usersname;
            if (!prof->user->lastName().isEmpty()) {
                usersname = tr("%1, %2").arg(prof->user->lastName()).arg(prof->user->firstName());
            }

            proxy->setData(proxy->index(ui->profileView->currentIndex().row(), 5, QModelIndex()), usersname);
            //updateProfileList();
            if (prof == p_profile) updateProfileHighlight(name);
        }

        delete newprof;
    } else {
        QMessageBox::information(this, STR_MessageBox_Information, tr("Select a profile first"), QMessageBox::Ok);
    }
}

void ProfileSelector::on_buttonNewProfile_clicked()
{
    if (p_profile)
        mainwin->CloseProfile();

    NewProfile *newprof = new NewProfile(this);
    newprof->skipWelcomeScreen();
    newprof->setWindowModality(Qt::ApplicationModal);
    newprof->setModal(true);
    if (newprof->exec() == NewProfile::Accepted) {
        p_profile = Profiles::Get(AppSetting->profileName());
        if (p_profile != nullptr) {
            QString name = p_profile->user->userName();
            p_profile = nullptr;
            mainwin->OpenProfile(name, true); // open profile, skipping the already entered password
        } else {
            qWarning() << AppSetting->profileName() << "yielded a null profile";
            p_profile=nullptr;
        }
        updateProfileList();
    }
    delete newprof;
}

void ProfileSelector::on_buttonDestroyProfile_clicked()
{
    if (ui->profileView->currentIndex().isValid()) {
        QString name = proxy->data(proxy->index(ui->profileView->currentIndex().row(), 0, QModelIndex()), Qt::UserRole+2).toString();
        Profile * profile = Profiles::profiles[name];
        QString path = profile->Get(PrefMacro(STR_GEN_DataFolder));

        bool verified = true;
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
                    verified = true;
                    break;
                } else {
                    if (tries < 3) {
                        QMessageBox::warning(this, STR_MessageBox_Error, tr("You entered an incorrect password"), QMessageBox::Ok);
                    } else {
                        QMessageBox::warning(this, STR_MessageBox_Error,
                                             tr("If you're trying to delete because you forgot the password, you need to either reset it or delete the profile folder manually."),
                                             QMessageBox::Ok);
                    }
                }
            } while (tries < 3);
            if (!verified) return;
        }

        QDialog confirmdlg;
        QVBoxLayout layout(&confirmdlg);
        QLabel message(QString("<b>"+STR_MessageBox_Warning+":</b> "+tr("You are about to destroy profile '<b>%1</b>'.")+"<br/><br/>"+tr("Think carefully, as this will irretrievably delete the profile along with all <b>backup data</b> stored under<br/>%2.")+"<br/><br/>"+tr("Enter the word <b>DELETE</b> below (exactly as shown) to confirm.")).arg(name).arg(path), &confirmdlg);
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

        if (lineedit.text().compare(tr("DELETE"))!=0) {
            QMessageBox::information(NULL, tr("Sorry"), tr("You need to enter DELETE in capital letters."), QMessageBox::Ok);
            return;
        }
        qDebug() << "Deleting Profile" << name;
        if (profile == p_profile) {
            // Shut down if active
            mainwin->CloseProfile();
        }
        Profiles::profiles.remove(name);

        if (!path.isEmpty()) {
            if (!removeDir(path)) {
                QMessageBox::information(this, STR_MessageBox_Error,
                                         tr("There was an error deleting the profile directory, you need to manually remove it.")+QString("\n\n%1").arg(path),
                                         QMessageBox::Ok);
            }
            qDebug() << "Delete" << path;
            QMessageBox::information(this, STR_MessageBox_Information, tr("Profile '%1' was succesfully deleted").arg(name),QMessageBox::Ok);
        }

        updateProfileList();

    }
}

QString ProfileSelector::formatSize(qint64 size)
{
    QStringList units = { tr("Bytes"), tr("KB"), tr("MB"), tr("GB"), tr("TB"), tr("PB") };
    int i;
    double outputSize = size;
    for (i=0; i<units.size()-1; i++) {
        if (outputSize < 1024) break;
        outputSize = outputSize/1024;
    }
    return QString("%0 %1").arg(outputSize, 0, 'f', 2).arg(units[i]);
}


QString ProfileSelector::getProfileDiskInfo(Profile *profile)
{
    QString html;
    if (profile) {
        qint64 sizeSummaries = profile->diskSpaceSummaries();
        qint64 sizeEvents = profile->diskSpaceEvents();
        qint64 sizeBackups = profile->diskSpaceBackups();

        html += "<table>"
                "<tr><td align=right>"+tr("Summaries:")+"</td><td>"+formatSize(sizeSummaries)+"</td></tr>"
                "<tr><td align=right>"+tr("Events:")+"</td><td>"+formatSize(sizeEvents)+"</td></tr>"
                "<tr><td align=right>"+tr("Backups:")+"</td><td>"+formatSize(sizeBackups)+"</td></tr>"
                "</table>";
    }
    return html;

}

void ProfileSelector::on_diskSpaceInfo_linkActivated(const QString &link)
{
    QString html;

    if (link == "show") {
        html += "<a href='hide'>"+tr("Hide disk usage information")+"</a>"+getProfileDiskInfo(p_profile);
        showDiskUsage = true;
    } else {
        html += "<a href='show'>"+tr("Show disk usage information")+"</a>";
        showDiskUsage = false;
    }
    ui->diskSpaceInfo->setText(html);
}

void ProfileSelector::on_selectionChanged(const QModelIndex &index, const QModelIndex &)
{
    bool enabled = false;
    if (index.isValid()) {
        QString name = proxy->data(proxy->index(index.row(), 0, QModelIndex()), Qt::UserRole+2).toString();
        auto prof = Profiles::profiles.find(name);
        if (prof != Profiles::profiles.end()) {
            enabled = true;
            QString html = QString();
            Profile * profile = prof.value();

            if (!profile->user->lastName().isEmpty() && !profile->user->firstName().isEmpty()) {
                html += tr("Name: %1, %2").arg(profile->user->lastName()).arg(profile->user->firstName())+"<br/>";
            }
            if (!profile->user->phone().isEmpty()) {
                html += tr("Phone: %1").arg(profile->user->phone())+"<br/>";
            }
            if (!profile->user->email().isEmpty()) {
                html += tr("Email: <a href='mailto:%1'>%1</a>").arg(profile->user->email())+"<br/>";
            }
            if (!profile->user->address().isEmpty()) {
                html += "<br/>"+tr("Address:")+"<br/>"+profile->user->address().trimmed().replace("\n","<br/>")+"<br/>";
            }
            if (html.isEmpty()) {
                html += tr("No profile information given")+"<br/>";
            }
            ui->diskSpaceInfo->setVisible(true);
            ui->profileInfoGroupBox->setTitle(tr("Profile: %1").arg(name));
            ui->profileInfoLabel->setText(html);

            if (showDiskUsage) { // ugly, but uh....
                ui->diskSpaceInfo->setText("<a href='hide'>"+tr("Hide disk usage information")+"</a>"+getProfileDiskInfo(prof.value()));
            }
        } else {
           ui->diskSpaceInfo->setText("Something went wrong");
        }
    }
    ui->buttonEditProfile->setEnabled(enabled);
}
