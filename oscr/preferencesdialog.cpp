/* SleepyHead Preferences Dialog Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QLabel>
#include <QColorDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QProcess>
#include <QDesktopServices>
#include <QFileDialog>
#include <QTextStream>
#include <QCalendarWidget>
#include <QMenuBar>
#include <QDebug>

#include <cmath>

#include "preferencesdialog.h"

#include <Graphs/gGraphView.h>
#include <mainwindow.h>
#include "ui_preferencesdialog.h"
#include "SleepLib/machine_common.h"

extern QFont *defaultfont;
extern QFont *mediumfont;
extern QFont *bigfont;
extern MainWindow *mainwin;

typedef QMessageBox::StandardButton StandardButton;
typedef QMessageBox::StandardButtons StandardButtons;

QHash<schema::ChanType, QString> channeltype;

PreferencesDialog::PreferencesDialog(QWidget *parent, Profile *_profile) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog),
    profile(_profile)
{
    ui->setupUi(this);

    channeltype.clear();
    channeltype[schema::FLAG] = tr("Flag");
    channeltype[schema::MINOR_FLAG] = tr("Minor Flag");
    channeltype[schema::SPAN] = tr("Span");
    channeltype[schema::UNKNOWN] = tr("Always Minor");
    bool haveResMed = false;
    QList<Machine *> machines = profile->GetMachines(MT_CPAP);
    for (QList<Machine *>::iterator it = machines.begin(); it != machines.end(); ++it) {
        const QString & mclass=(*it)->loaderName();
        if (mclass == STR_MACH_ResMed) {
            haveResMed = true;
            break;
        }
    }

#ifdef LOCK_RESMED_SESSIONS
    // Remove access to session splitting options and show ResMed users a notice instead
    ui->ResMedWarning->setText(tr("<p><b>Please Note:</b> SleepyHead's advanced session splitting capabilities are not possible with <b>ResMed</b> machines due to a limitation in the way their settings and summary data is stored, and therefore they have been disabled for this profile.</p><p>On ResMed machines, days will <b>split at noon</b> like in ResMed's commercial software.</p>"));
    ui->ResMedWarning->setVisible(haveResMed);

    if (haveResMed) {
        ui->sessionSplitWidget->setVisible(!haveResMed);
        profile->session->setDaySplitTime(QTime(12,0,0));
        profile->session->setIgnoreShortSessions(0);
        profile->session->setCombineCloseSessions(0);
        profile->session->setLockSummarySessions(true);
        p_profile->general->setPrefCalcPercentile(95.0);    // 95%
        p_profile->general->setPrefCalcMiddle(0);           // Median (50%)
        p_profile->general->setPrefCalcMax(1);              // 99.9th percentile max
        ui->prefCalcMax->setEnabled(false);
        ui->prefCalcMiddle->setEnabled(false);
        ui->prefCalcPercentile->setEnabled(false);
        ui->showUnknownFlags->setEnabled(false);
        ui->calculateUnintentionalLeaks->setEnabled(false);

        p_profile->session->setBackupCardData(true);
        ui->createSDBackups->setChecked(true);
        ui->createSDBackups->setEnabled(false);

    }
    ui->resmedPrefCalcsNotice->setVisible(haveResMed);
#endif

    int gfxEngine=(int)currentGFXEngine();

    int selIdx = 0;
    for (int j=0, i=0; i<=(int)MaxGFXEngine; ++i) {
        if (gfxEgnineIsSupported((GFXEngine) i)) {
            ui->gfxEngineCombo->addItem(GFXEngineNames[i], i);
            if (i==gfxEngine) {
                 selIdx = j;
            }
            ++j;
        }
    }
    ui->gfxEngineCombo->setCurrentIndex(selIdx);


    ui->culminativeIndices->setEnabled(false);

    QLocale locale = QLocale::system();
    QString shortformat = locale.dateFormat(QLocale::ShortFormat);

    if (!shortformat.toLower().contains("yyyy")) {
        shortformat.replace("yy", "yyyy");
    }

//    Qt::DayOfWeek dow = firstDayOfWeekFromLocale();

//    QTextCharFormat format = ui->startedUsingMask->calendarWidget()->weekdayTextFormat(Qt::Saturday);
//    format.setForeground(QBrush(Qt::black, Qt::SolidPattern));

    if (profile == nullptr) {
        qCritical() << "Preferences dialog created without legit profile object";
        return;
    }
    ui->tabWidget->setCurrentIndex(0);

    //i=ui->timeZoneCombo->findText((*profile)["TimeZone"].toString());
    //ui->timeZoneCombo->setCurrentIndex(i);

    ui->showLeakRedline->setChecked(profile->cpap->showLeakRedline());
    ui->leakRedlineSpinbox->setValue(profile->cpap->leakRedline());

    ui->oxiDesaturationThreshold->setValue(schema::channel[OXI_SPO2].lowerThreshold());
    ui->flagPulseAbove->setValue(schema::channel[OXI_Pulse].upperThreshold());
    ui->flagPulseBelow->setValue(schema::channel[OXI_Pulse].lowerThreshold());

    ui->spo2Drop->setValue(profile->oxi->spO2DropPercentage());
    ui->spo2DropTime->setValue(profile->oxi->spO2DropDuration());
    ui->pulseChange->setValue(profile->oxi->pulseChangeBPM());
    ui->pulseChangeTime->setValue(profile->oxi->pulseChangeDuration());
    ui->oxiDiscardThreshold->setValue(profile->oxi->oxiDiscardThreshold());

    ui->eventIndexCombo->setCurrentIndex(profile->general->calculateRDI() ? 1 : 0);

    ui->automaticImport->setChecked(profile->cpap->autoImport()); // Skip extra dialogs, this needs to rename.

    ui->autoLoadLastUsed->setChecked(AppSetting->autoOpenLastUsed());

    ui->timeEdit->setTime(profile->session->daySplitTime());
    int val = profile->session->combineCloseSessions();
    ui->combineSlider->setValue(val);

    ui->openingTabCombo->setCurrentIndex(AppSetting->openTabAtStart());
    ui->importTabCombo->setCurrentIndex(AppSetting->openTabAfterImport());


    if (val > 0) {
        ui->combineLCD->display(val);
    } else { ui->combineLCD->display(STR_TR_Off); }

    val = profile->session->ignoreShortSessions();
    ui->IgnoreSlider->setValue(val);

    if (val > 0) {
        ui->IgnoreLCD->display(val);
    } else { ui->IgnoreLCD->display(STR_TR_Off); }

    ui->LockSummarySessionSplitting->setChecked(profile->session->lockSummarySessions());

    ui->applicationFont->setCurrentFont(QApplication::font());
    //ui->applicationFont->setFont(QApplication::font());
    ui->applicationFontSize->setValue(QApplication::font().pointSize());
    ui->applicationFontBold->setChecked(QApplication::font().weight() == QFont::Bold);
    ui->applicationFontItalic->setChecked(QApplication::font().italic());

    ui->graphFont->setCurrentFont(*defaultfont);
    //ui->graphFont->setFont(*defaultfont);
    ui->graphFontSize->setValue(defaultfont->pointSize());
    ui->graphFontBold->setChecked(defaultfont->weight() == QFont::Bold);
    ui->graphFontItalic->setChecked(defaultfont->italic());

    ui->titleFont->setCurrentFont(*mediumfont);
    //ui->titleFont->setFont(*mediumfont);
    ui->titleFontSize->setValue(mediumfont->pointSize());
    ui->titleFontBold->setChecked(mediumfont->weight() == QFont::Bold);
    ui->titleFontItalic->setChecked(mediumfont->italic());

    ui->bigFont->setCurrentFont(*bigfont);
    //ui->bigFont->setFont(*bigfont);
    ui->bigFontSize->setValue(bigfont->pointSize());
    ui->bigFontBold->setChecked(bigfont->weight() == QFont::Bold);
    ui->bigFontItalic->setChecked(bigfont->italic());

    ui->lineThicknessSlider->setValue(AppSetting->lineThickness()*2.0);

    ui->resyncMachineDetectedEvents->setChecked(profile->cpap->resyncFromUserFlagging());

    ui->useAntiAliasing->setChecked(AppSetting->antiAliasing());
    ui->usePixmapCaching->setChecked(AppSetting->usePixmapCaching());
    ui->useSquareWavePlots->setChecked(AppSetting->squareWavePlots());
    ui->enableGraphSnapshots->setChecked(AppSetting->graphSnapshots());
    ui->graphTooltips->setChecked(AppSetting->graphTooltips());
    ui->allowYAxisScaling->setChecked(AppSetting->allowYAxisScaling());

    ui->autoLaunchImporter->setChecked(AppSetting->autoLaunchImport());
    ui->allowEarlyUpdates->setChecked(AppSetting->allowEarlyUpdates());

    int s = profile->cpap->clockDrift();
    int m = (s / 60) % 60;
    int h = (s / 3600);
    s %= 60;
    ui->clockDriftHours->setValue(h);
    ui->clockDriftMinutes->setValue(m);
    ui->clockDriftSeconds->setValue(s);

    ui->skipEmptyDays->setChecked(profile->general->skipEmptyDays());
    ui->showUnknownFlags->setChecked(profile->general->showUnknownFlags());
    ui->enableMultithreading->setChecked(AppSetting->multithreading());
    ui->removeCardNotificationCheckbox->setChecked(AppSetting->removeCardReminder());
    ui->cacheSessionData->setChecked(AppSetting->cacheSessions());
    ui->preloadSummaries->setChecked(profile->session->preloadSummaries());
    ui->animationsAndTransitionsCheckbox->setChecked(AppSetting->animations());
    ui->complianceCheckBox->setChecked(profile->cpap->showComplianceInfo());
    ui->complianceHours->setValue(profile->cpap->complianceHours());

    ui->prefCalcMiddle->setCurrentIndex(profile->general->prefCalcMiddle());
    ui->prefCalcMax->setCurrentIndex(profile->general->prefCalcMax());
    float f = profile->general->prefCalcPercentile();
    ui->prefCalcPercentile->setValue(f);

    ui->tooltipTimeoutSlider->setValue(AppSetting->tooltipTimeout());
    on_tooltipTimeoutSlider_valueChanged(ui->tooltipTimeoutSlider->value());
    //ui->tooltipMS->display(profile->general->tooltipTimeout());

    ui->scrollDampeningSlider->setValue(AppSetting->scrollDampening() / 10);
    on_scrollDampeningSlider_valueChanged(ui->scrollDampeningSlider->value());

    bool bcd = profile->session->backupCardData();
    ui->createSDBackups->setChecked(bcd);
    ui->compressSDBackups->setEnabled(bcd);
    ui->compressSDBackups->setChecked(profile->session->compressBackupData());
    ui->compressSessionData->setChecked(profile->session->compressSessionData());
    ui->ignoreOlderSessionsCheck->setChecked(profile->session->ignoreOlderSessions());
    ui->ignoreOlderSessionsDate->setDate(profile->session->ignoreOlderSessionsDate().date());

    ui->graphHeight->setValue(AppSetting->graphHeight());

    ui->automaticallyCheckUpdates->setChecked(AppSetting->updatesAutoCheck());

    ui->updateCheckEvery->setValue(AppSetting->updateCheckFrequency());


    if (AppSetting->updatesLastChecked().isValid()) {
        RefreshLastChecked();
    } else { ui->updateLastChecked->setText(tr("Never")); }

    ui->overlayFlagsCombo->setCurrentIndex(AppSetting->overlayType());
    ui->overviewLinecharts->setCurrentIndex(AppSetting->overviewLinechartMode());

    ui->ahiGraphWindowSize->setEnabled(false);
    ui->ahiGraphWindowSize->setValue(profile->cpap->AHIWindow());
    ui->ahiGraphZeroReset->setChecked(profile->cpap->AHIReset());

    ui->customEventGroupbox->setChecked(profile->cpap->userEventFlagging());
    ui->apneaDuration->setValue(profile->cpap->userEventDuration());
    ui->apneaFlowRestriction->setValue(profile->cpap->userEventRestriction());
    ui->apneaDuration2->setValue(profile->cpap->userEventDuration2());
    ui->apneaFlowRestriction2->setValue(profile->cpap->userEventRestriction2());
    ui->userEventDuplicates->setChecked(profile->cpap->userEventDuplicates());
    ui->userEventDuplicates->setVisible(false);

    ui->showUserFlagsInPie->setChecked(AppSetting->userEventPieChart());

    bool b;
    ui->calculateUnintentionalLeaks->setChecked(b=profile->cpap->calculateUnintentionalLeaks());

    ui->maskLeaks4Slider->setValue(profile->cpap->custom4cmH2OLeaks()*10.0);
    ui->maskLeaks20Slider->setValue(profile->cpap->custom20cmH2OLeaks()*10.0);

    ui->maskLeaks4Label->setText(tr("%1 %2").arg(profile->cpap->custom4cmH2OLeaks(), 5, 'f', 1).arg(STR_UNIT_LPM));
    ui->maskLeaks20Label->setText(tr("%1 %2").arg(profile->cpap->custom20cmH2OLeaks(), 5, 'f', 1).arg(STR_UNIT_LPM));

    /*    QLocale locale=QLocale::system();
        QString shortformat=locale.dateFormat(QLocale::ShortFormat);
        if (!shortformat.toLower().contains("yyyy")) {
            shortformat.replace("yy","yyyy");
        }*/

    chanFilterModel = new MySortFilterProxyModel(this);
    chanModel = new QStandardItemModel(this);
    chanFilterModel->setSourceModel(chanModel);
    chanFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    chanFilterModel->setFilterKeyColumn(0);
    ui->chanView->setModel(chanFilterModel);

    InitChanInfo();

    waveFilterModel = new MySortFilterProxyModel(this);
    waveModel = new QStandardItemModel(this);
    waveFilterModel->setSourceModel(waveModel);
    waveFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    waveFilterModel->setFilterKeyColumn(0);
    ui->waveView->setModel(waveFilterModel);
    InitWaveInfo();

    ui->waveView->setSortingEnabled(true);
    ui->chanView->setSortingEnabled(true);

    ui->waveView->sortByColumn(0, Qt::AscendingOrder);
    ui->chanView->sortByColumn(0, Qt::AscendingOrder);

}

#include <QItemDelegate>
class SpinBoxDelegate : public QItemDelegate
{

public:
    SpinBoxDelegate(QObject *parent = 0):QItemDelegate(parent) {}

    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

QWidget *SpinBoxDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex &/* index */) const
{
    QDoubleSpinBox *editor = new QDoubleSpinBox(parent);
    //editor->setMinimum(0);
    //editor->setMaximum(100.0);

    return editor;
}

void SpinBoxDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    double value = index.model()->data(index, Qt::EditRole).toDouble();

    QDoubleSpinBox *spinBox = static_cast<QDoubleSpinBox*>(editor);
    spinBox->setMinimum(-9999999.0);
    spinBox->setMaximum(9999999.0);

    spinBox->setValue(value);
}

void SpinBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QDoubleSpinBox *spinBox = static_cast<QDoubleSpinBox*>(editor);
    spinBox->interpretText();
    double value = spinBox->value();

    model->setData(index, value, Qt::EditRole);
}
void SpinBoxDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}


#include <QItemDelegate>
class ComboBoxDelegate : public QItemDelegate
{

public:
    ComboBoxDelegate(QObject *parent = 0):QItemDelegate(parent) {}

    virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    virtual void setEditorData(QWidget *editor, const QModelIndex &index) const;
    virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

QWidget *ComboBoxDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex &/* index */) const
{
    QComboBox *combo = new QComboBox(parent);

    QHash<schema::ChanType, QString>::iterator it;
    for (it = channeltype.begin(); it != channeltype.end(); ++it) {
        if (it.key() == schema::UNKNOWN) continue;
        combo->addItem(it.value());
    }
    //editor->setMinimum(0);
    //editor->setMaximum(100.0);

    return combo;
}

void ComboBoxDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();

    QComboBox *combo = static_cast<QComboBox*>(editor);

    combo->setCurrentText(value);
}

void ComboBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QComboBox *combo = static_cast<QComboBox*>(editor);

    model->setData(index, combo->currentText(), Qt::EditRole);
}
void ComboBoxDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
    editor->setGeometry(option.rect);
}


void PreferencesDialog::InitChanInfo()
{
    QHash<MachineType, int> toprows;

    chanModel->clear();
    toplevel.clear();
    toprows.clear();
    QStringList headers;
    headers.append(tr("Name"));
    headers.append(tr("Color"));
    headers.append(tr("Overview"));
    headers.append(tr("Flag Type"));
    headers.append(tr("Label"));
    headers.append(tr("Details"));
    chanModel->setHorizontalHeaderLabels(headers);
    ui->chanView->setColumnWidth(0, 200);
    ui->chanView->setColumnWidth(1, 40);
    ui->chanView->setColumnWidth(2, 60);
    ui->chanView->setColumnWidth(3, 100);
    ui->chanView->setColumnWidth(4, 100);
    ui->chanView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->chanView->setSelectionBehavior(QAbstractItemView::SelectItems);

    chanModel->setColumnCount(6);

    QStandardItem *hdr = nullptr;

    QMap<MachineType, QString> Section;

    Section[MT_CPAP] = tr("CPAP Events");
    Section[MT_OXIMETER] = tr("Oximeter Events");
    Section[MT_POSITION] = tr("Positional Events");
    Section[MT_SLEEPSTAGE] = tr("Sleep Stage Events");
    Section[MT_UNCATEGORIZED] = tr("Unknown Events");

    QMap<MachineType, QString>::iterator it;

    QHash<QString, schema::Channel *>::iterator ci;

    for (it = Section.begin(); it != Section.end(); ++it) {
        toplevel[it.key()] = hdr = new QStandardItem(it.value());
        hdr->setEditable(false);
        QList<QStandardItem *> list;
        list.append(hdr);
        for (int i=0; i<5; i++) {
            QStandardItem *it = new QStandardItem();
            it->setEnabled(false);
            list.append(it);
        }
        chanModel->appendRow(list);
    }

    ui->chanView->setAlternatingRowColors(true);

    // ui->graphView->setFirstColumnSpanned(0,daily->index(),true); // Crashes on windows.. Why do I need this again?


    ComboBoxDelegate * combobox = new ComboBoxDelegate(ui->waveView);

    ui->chanView->setItemDelegateForColumn(3,combobox);

    int row = 0;
    for (ci = schema::channel.names.begin(); ci != schema::channel.names.end(); ci++) {
        schema::Channel * chan = ci.value();
        if ((chan->type() == schema::DATA) || (chan->type() == schema::SETTING) || chan->type() == schema::WAVEFORM) continue;

        QList<QStandardItem *> items;
        QStandardItem *it = new QStandardItem(chan->fullname());
        it->setCheckable(true);
        it->setCheckState(chan->enabled() ? Qt::Checked : Qt::Unchecked);
        it->setEditable(true);
        it->setData(chan->id(), Qt::UserRole);

        // Dear translators: %1 is a unique ascii english string used to indentify channels in the code, I'd like feedback on how this goes..
        // It's here in case users mess up which field is which.. it will always show the Channel Code underneath in the tooltip.
        it->setToolTip(tr("Double click to change the descriptive name the '%1' channel.").arg(chan->code()));
        items.push_back(it);


        it = new QStandardItem();
        it->setBackground(QBrush(chan->defaultColor()));
        it->setEditable(false);
        it->setData(chan->defaultColor().rgba(), Qt::UserRole);
        it->setToolTip(tr("Double click to change the default color for this channel plot/flag/data."));
        it->setSelectable(false);
        items.push_back(it);

        it = new QStandardItem(QString());
        it->setToolTip(tr("Whether this flag has a dedicated overview chart."));
        it->setCheckable(true);
        it->setCheckState(chan->showInOverview() ? Qt::Checked : Qt::Unchecked);
        it->setTextAlignment(Qt::AlignCenter);
        it->setData(chan->id(), Qt::UserRole);
        items.push_back(it);

        schema::ChanType type = chan->type();

        it = new QStandardItem(channeltype[type]);
        it->setToolTip(tr("Here you can change the type of flag shown for this event"));
        it->setEditable(type != schema::UNKNOWN);
        items.push_back(it);

        it = new QStandardItem(chan->label());
        it->setToolTip(tr("This is the short-form label to indicate this channel on screen."));

        it->setEditable(true);
        items.push_back(it);

        it = new QStandardItem(chan->description());
        it->setToolTip(tr("This is a description of what this channel does."));

        it->setEditable(true);
        items.push_back(it);

        MachineType mt = chan->machtype();
        if (chan->type() == schema::UNKNOWN) mt = MT_UNCATEGORIZED;
        row = toprows[mt]++;
        toplevel[mt]->insertRow(row, items);
    }


    for(QHash<MachineType, QStandardItem *>::iterator i = toplevel.begin(); i != toplevel.end(); ++i) {
        if (i.value()->rowCount() == 0) {
            chanModel->removeRow(i.value()->row());
        }
    }

    ui->chanView->expandAll();
    ui->chanView->setSortingEnabled(true);
}

void PreferencesDialog::InitWaveInfo()
{
    QHash<MachineType, int> toprows;

    waveModel->clear();
    machlevel.clear();
    toprows.clear();
    QStringList headers;
    headers.append(tr("Name"));
    headers.append(tr("Color"));
    headers.append(tr("Overview"));
    headers.append(tr("Lower"));
    headers.append(tr("Upper"));
    headers.append(tr("Label"));
    headers.append(tr("Details"));
    waveModel->setHorizontalHeaderLabels(headers);
    ui->waveView->setColumnWidth(0, 200);
    ui->waveView->setColumnWidth(1, 40);
    ui->waveView->setColumnWidth(2, 60);
    ui->waveView->setColumnWidth(3, 50);
    ui->waveView->setColumnWidth(4, 50);
    ui->waveView->setColumnWidth(5, 100);
    ui->waveView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->waveView->setSelectionBehavior(QAbstractItemView::SelectItems);

    waveModel->setColumnCount(7);

    QStandardItem *hdr = nullptr;

    QMap<MachineType, QString> Section;

    Section[MT_CPAP] = tr("CPAP Waveforms");
    Section[MT_OXIMETER] = tr("Oximeter Waveforms");
    Section[MT_POSITION] = tr("Positional Waveforms");
    Section[MT_SLEEPSTAGE] = tr("Sleep Stage Waveforms");

    QMap<MachineType, QString>::iterator it;

    for (it = Section.begin(); it != Section.end(); ++it) {
        machlevel[it.key()] = hdr = new QStandardItem(it.value());
        hdr->setEditable(false);
        QList<QStandardItem *> list;
        list.append(hdr);
        for (int i=0; i<6; i++) {
            QStandardItem *it = new QStandardItem();
            it->setEnabled(false);
            list.append(it);
        }
        waveModel->appendRow(list);
    }

    ui->waveView->setAlternatingRowColors(true);

    // ui->graphView->setFirstColumnSpanned(0,daily->index(),true); // Crashes on windows.. Why do I need this again?

    QHash<QString, schema::Channel *>::iterator ci;

    SpinBoxDelegate * spinbox = new SpinBoxDelegate(ui->waveView);

    ui->waveView->setItemDelegateForColumn(3,spinbox);
    ui->waveView->setItemDelegateForColumn(4,spinbox);

    int row = 0;
    for (ci = schema::channel.names.begin(); ci != schema::channel.names.end(); ci++) {
        schema::Channel * chan = ci.value();
        if (chan->type() != schema::WAVEFORM) continue;

        QList<QStandardItem *> items;
        QStandardItem *it = new QStandardItem(chan->fullname());

        it->setCheckable(true);
        it->setCheckState(chan->enabled() ? Qt::Checked : Qt::Unchecked);
        it->setEditable(true);
        it->setData(chan->id(), Qt::UserRole);
        it->setToolTip(tr("Double click to change the descriptive name this channel."));
        items.push_back(it);

        it = new QStandardItem();
        it->setBackground(QBrush(chan->defaultColor()));
        it->setEditable(false);
        it->setData(chan->defaultColor().rgba(), Qt::UserRole);
        it->setToolTip(tr("Double click to change the default color for this channel plot/flag/data."));
        it->setSelectable(false);
        items.push_back(it);

        it = new QStandardItem();
        it->setCheckable(true);
        it->setCheckState(chan->showInOverview() ? Qt::Checked : Qt::Unchecked);
        it->setEditable(true);
        it->setData(chan->id(), Qt::UserRole);
        it->setToolTip(tr("Whether a breakdown of this waveform displays in overview."));
        items.push_back(it);


        it = new QStandardItem(QString::number(chan->lowerThreshold(),'f',1));
        it->setToolTip(tr("Here you can set the <b>lower</b> threshold used for certain calculations on the %1 waveform").arg(chan->fullname()));
        it->setEditable(true);
        items.push_back(it);

        it = new QStandardItem(QString::number(chan->upperThreshold(),'f',1));
        it->setToolTip(tr("Here you can set the <b>upper</b> threshold used for certain calculations on the %1 waveform").arg(chan->fullname()));
        it->setEditable(true);
        items.push_back(it);

        it = new QStandardItem(chan->label());
        it->setToolTip(tr("This is the short-form label to indicate this channel on screen."));

        it->setEditable(true);
        items.push_back(it);

        it = new QStandardItem(chan->description());
        it->setToolTip(tr("This is a description of what this channel does."));

        it->setEditable(true);
        items.push_back(it);

        row = toprows[chan->machtype()]++;
        machlevel[chan->machtype()]->insertRow(row, items);
    }

    for(QHash<MachineType, QStandardItem *>::iterator i = machlevel.begin(); i != machlevel.end(); ++i) {
        if (i.value()->rowCount() == 0) {
            waveModel->removeRow(i.value()->row());
        }
    }

    ui->waveView->expandAll();
    ui->waveView->setSortingEnabled(true);
}


PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

bool PreferencesDialog::Save()
{
    bool recompress_events = false;
    bool recalc_events = false;
    bool needs_restart = false;
    bool needs_reload = false;

    if (ui->ahiGraphZeroReset->isChecked() != profile->cpap->AHIReset()) { recalc_events = true; }

    if (ui->useSquareWavePlots->isChecked() != AppSetting->squareWavePlots()) {
        needs_reload  = true;
    }

    if ((profile->session->daySplitTime() != ui->timeEdit->time()) ||
            (profile->session->combineCloseSessions() != ui->combineSlider->value()) ||
            (profile->session->ignoreShortSessions() != ui->IgnoreSlider->value())) {
        needs_reload = true;
    }

    if (profile->session->lockSummarySessions() != ui->LockSummarySessionSplitting->isChecked()) {
        needs_reload = true;
    }

    if (AppSetting->userEventPieChart() != ui->showUserFlagsInPie->isChecked()) {
        // lazy.. fix me
        needs_reload = true;
    }

    if ((GFXEngine)ui->gfxEngineCombo->currentData().toUInt() != currentGFXEngine()) {
        setCurrentGFXEngine((GFXEngine)ui->gfxEngineCombo->currentData().toUInt());
        needs_restart = true;
    }

    int rdi_set = profile->general->calculateRDI() ? 1 : 0;
    if (rdi_set != ui->eventIndexCombo->currentIndex()) {
        //recalc_events=true;
        needs_reload  = true;
    }

    if ((profile->general->prefCalcMiddle() != ui->prefCalcMiddle->currentIndex())
            || (profile->general->prefCalcMax() != ui->prefCalcMax->currentIndex())
            || (profile->general->prefCalcPercentile() != ui->prefCalcPercentile->value())) {
        needs_reload = true;
    }

    if (profile->cpap->leakRedline() != ui->leakRedlineSpinbox->value()) {
        needs_reload = true;
    }


    if (profile->cpap->userEventFlagging() &&
            (profile->cpap->userEventDuration() != ui->apneaDuration->value() ||
             profile->cpap->userEventDuration2() != ui->apneaDuration2->value() ||
             profile->cpap->userEventDuplicates() != ui->userEventDuplicates->isChecked() ||
             profile->cpap->userEventRestriction2() != ui->apneaFlowRestriction2->value() ||
             profile->cpap->userEventRestriction() != ui->apneaFlowRestriction->value())) {
        recalc_events = true;
    }

    // Restart if turning user event flagging on/off
    if (profile->cpap->userEventFlagging() != ui->customEventGroupbox->isChecked()) {
        // if (profile->cpap->userEventFlagging()) {
        // Don't bother recalculating, just switch off
        needs_reload = true;
        //} else
        recalc_events = true;
    }

    if (profile->session->compressSessionData() != ui->compressSessionData->isChecked()) {
        recompress_events = true;
    }

    if (recompress_events) {
        if (profile->countDays(MT_CPAP, profile->FirstDay(), profile->LastDay()) > 0) {
            if (QMessageBox::question(this, tr("Data Processing Required"),
                                      tr("A data re/decompression proceedure is required to apply these changes. This operation may take a couple of minutes to complete.\n\nAre you sure you want to make these changes?"),
                                      QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
                return false;
            }
        } else { recompress_events = false; }
    }
    if (recalc_events) {
        if (profile->countDays(MT_CPAP, profile->FirstDay(), profile->LastDay()) > 0) {
            if (QMessageBox::question(this, tr("Data Reindex Required"),
                                      tr("A data reindexing proceedure is required to apply these changes. This operation may take a couple of minutes to complete.\n\nAre you sure you want to make these changes?"),
                                      QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
                return false;
            }
        } else { recalc_events = false; }
    } else if (needs_restart) {
        if (QMessageBox::question(this, tr("Restart Required"),
                                  tr("One or more of the changes you have made will require this application to be restarted, in order for these changes to come into effect.\n\nWould you like do this now?"),
                                  QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
            return false;
        }
    }

    schema::channel[OXI_SPO2].setLowerThreshold(ui->oxiDesaturationThreshold->value());
    schema::channel[OXI_Pulse].setLowerThreshold(ui->flagPulseBelow->value());
    schema::channel[OXI_Pulse].setUpperThreshold(ui->flagPulseAbove->value());


    AppSetting->setUserEventPieChart(ui->showUserFlagsInPie->isChecked());
    profile->session->setLockSummarySessions(ui->LockSummarySessionSplitting->isChecked());

    AppSetting->setOpenTabAtStart(ui->openingTabCombo->currentIndex());
    AppSetting->setOpenTabAfterImport(ui->importTabCombo->currentIndex());

    AppSetting->setAllowYAxisScaling(ui->allowYAxisScaling->isChecked());
    AppSetting->setGraphTooltips(ui->graphTooltips->isChecked());

    AppSetting->setAntiAliasing(ui->useAntiAliasing->isChecked());
    AppSetting->setUsePixmapCaching(ui->usePixmapCaching->isChecked());
    AppSetting->setSquareWavePlots(ui->useSquareWavePlots->isChecked());
    AppSetting->setGraphSnapshots(ui->enableGraphSnapshots->isChecked());
    AppSetting->setLineThickness(float(ui->lineThicknessSlider->value()) / 2.0);

    profile->general->setSkipEmptyDays(ui->skipEmptyDays->isChecked());

    AppSetting->setTooltipTimeout(ui->tooltipTimeoutSlider->value());
    AppSetting->setScrollDampening(ui->scrollDampeningSlider->value() * 10);

    profile->general->setShowUnknownFlags(ui->showUnknownFlags->isChecked());
    AppSetting->setMultithreading(ui->enableMultithreading->isChecked());
    AppSetting->setRemoveCardReminder(ui->removeCardNotificationCheckbox->isChecked());

    AppSetting->setCacheSessions(ui->cacheSessionData->isChecked());
    profile->session->setPreloadSummaries(ui->preloadSummaries->isChecked());
    AppSetting->setAnimations(ui->animationsAndTransitionsCheckbox->isChecked());

    profile->cpap->setShowLeakRedline(ui->showLeakRedline->isChecked());
    profile->cpap->setLeakRedline(ui->leakRedlineSpinbox->value());

    profile->cpap->setShowComplianceInfo(ui->complianceCheckBox->isChecked());
    profile->cpap->setComplianceHours(ui->complianceHours->value());

    if (ui->graphHeight->value() != AppSetting->graphHeight()) {
        AppSetting->setGraphHeight(ui->graphHeight->value());
        mainwin->getDaily()->ResetGraphLayout();
        mainwin->getOverview()->ResetGraphLayout();
    }

    profile->general->setPrefCalcMiddle(ui->prefCalcMiddle->currentIndex());
    profile->general->setPrefCalcMax(ui->prefCalcMax->currentIndex());
    profile->general->setPrefCalcPercentile(ui->prefCalcPercentile->value());

    profile->general->setCalculateRDI((ui->eventIndexCombo->currentIndex() == 1));
    profile->session->setBackupCardData(ui->createSDBackups->isChecked());
    profile->session->setCompressBackupData(ui->compressSDBackups->isChecked());
    profile->session->setCompressSessionData(ui->compressSessionData->isChecked());

    profile->session->setCombineCloseSessions(ui->combineSlider->value());
    profile->session->setIgnoreShortSessions(ui->IgnoreSlider->value());
    profile->session->setDaySplitTime(ui->timeEdit->time());
    profile->session->setIgnoreOlderSessions(ui->ignoreOlderSessionsCheck->isChecked());
    profile->session->setIgnoreOlderSessionsDate(ui->ignoreOlderSessionsDate->date());

    int s = ui->clockDriftHours->value() * 3600 + ui->clockDriftMinutes->value() * 60 + ui->clockDriftSeconds->value();
    profile->cpap->setClockDrift(s);

    AppSetting->setOverlayType((OverlayDisplayType)ui->overlayFlagsCombo->currentIndex());
    AppSetting->setOverviewLinechartMode((OverviewLinechartModes)ui->overviewLinecharts->currentIndex());

    profile->oxi->setSpO2DropPercentage(ui->spo2Drop->value());
    profile->oxi->setSpO2DropDuration(ui->spo2DropTime->value());
    profile->oxi->setPulseChangeBPM(ui->pulseChange->value());
    profile->oxi->setPulseChangeDuration(ui->pulseChangeTime->value());
    profile->oxi->setOxiDiscardThreshold(ui->oxiDiscardThreshold->value());

    profile->cpap->setAHIWindow(ui->ahiGraphWindowSize->value());
    profile->cpap->setAHIReset(ui->ahiGraphZeroReset->isChecked());

    profile->cpap->setAutoImport(ui->automaticImport->isChecked()); // delete me???
    AppSetting->setAutoOpenLastUsed(ui->autoLoadLastUsed->isChecked());

    profile->cpap->setUserEventFlagging(ui->customEventGroupbox->isChecked());

    profile->cpap->setResyncFromUserFlagging(ui->resyncMachineDetectedEvents->isChecked());
    profile->cpap->setUserEventDuration(ui->apneaDuration->value());
    profile->cpap->setUserEventRestriction(ui->apneaFlowRestriction->value());
    profile->cpap->setUserEventDuration2(ui->apneaDuration2->value());
    profile->cpap->setUserEventRestriction2(ui->apneaFlowRestriction2->value());

    profile->cpap->setUserEventDuplicates(ui->userEventDuplicates->isChecked());



    if ((ui->calculateUnintentionalLeaks->isChecked() != profile->cpap->calculateUnintentionalLeaks())
      || (fabs((ui->maskLeaks4Slider->value()/10.0)-profile->cpap->custom4cmH2OLeaks())>.1)
      || (fabs((ui->maskLeaks20Slider->value()/10.0)-profile->cpap->custom20cmH2OLeaks())>.1)) {
           recalc_events = true;
    }

    profile->cpap->setCalculateUnintentionalLeaks(ui->calculateUnintentionalLeaks->isChecked());
    profile->cpap->setCustom4cmH2OLeaks(double(ui->maskLeaks4Slider->value()) / 10.0f);
    profile->cpap->setCustom20cmH2OLeaks(double(ui->maskLeaks20Slider->value()) / 10.0f);

    AppSetting->setAutoLaunchImport(ui->autoLaunchImporter->isChecked());

    AppSetting->setUpdatesAutoCheck(ui->automaticallyCheckUpdates->isChecked());
    AppSetting->setUpdateCheckFrequency(ui->updateCheckEvery->value());
    AppSetting->setAllowEarlyUpdates(ui->allowEarlyUpdates->isChecked());


    PREF["Fonts_Application_Name"] = ui->applicationFont->currentText();
    PREF["Fonts_Application_Size"] = ui->applicationFontSize->value();
    PREF["Fonts_Application_Bold"] = ui->applicationFontBold->isChecked();
    PREF["Fonts_Application_Italic"] = ui->applicationFontItalic->isChecked();


    PREF["Fonts_Graph_Name"] = ui->graphFont->currentText();
    PREF["Fonts_Graph_Size"] = ui->graphFontSize->value();
    PREF["Fonts_Graph_Bold"] = ui->graphFontBold->isChecked();
    PREF["Fonts_Graph_Italic"] = ui->graphFontItalic->isChecked();

    PREF["Fonts_Title_Name"] = ui->titleFont->currentText();
    PREF["Fonts_Title_Size"] = ui->titleFontSize->value();
    PREF["Fonts_Title_Bold"] = ui->titleFontBold->isChecked();
    PREF["Fonts_Title_Italic"] = ui->titleFontItalic->isChecked();

    PREF["Fonts_Big_Name"] = ui->bigFont->currentText();
    PREF["Fonts_Big_Size"] = ui->bigFontSize->value();
    PREF["Fonts_Big_Bold"] = ui->bigFontBold->isChecked();
    PREF["Fonts_Big_Italic"] = ui->bigFontItalic->isChecked();

    QFont font = ui->applicationFont->currentFont();
    font.setPointSize(ui->applicationFontSize->value());
    font.setWeight(ui->applicationFontBold->isChecked() ? QFont::Bold : QFont::Normal);
    font.setItalic(ui->applicationFontItalic->isChecked());
    QApplication::setFont(font);
    mainwin->menuBar()->setFont(font);

    *defaultfont = ui->graphFont->currentFont();
    defaultfont->setPointSize(ui->graphFontSize->value());
    defaultfont->setWeight(ui->graphFontBold->isChecked() ? QFont::Bold : QFont::Normal);
    defaultfont->setItalic(ui->graphFontItalic->isChecked());

    *mediumfont = ui->titleFont->currentFont();
    mediumfont->setPointSize(ui->titleFontSize->value());
    mediumfont->setWeight(ui->titleFontBold->isChecked() ? QFont::Bold : QFont::Normal);
    mediumfont->setItalic(ui->titleFontItalic->isChecked());

    *bigfont = ui->bigFont->currentFont();
    bigfont->setPointSize(ui->bigFontSize->value());
    bigfont->setWeight(ui->bigFontBold->isChecked() ? QFont::Bold : QFont::Normal);
    bigfont->setItalic(ui->bigFontItalic->isChecked());


    saveChanInfo();
    saveWaveInfo();
    //qDebug() << "TODO: Save channels.xml to update channel data";

    PREF.Save();
    profile->Save();

    if (recompress_events) {
        mainwin->recompressEvents();
    } else if (recalc_events) {
        // send a signal instead?
        mainwin->reprocessEvents(needs_restart);
    } else if (needs_reload) {
        QTimer::singleShot(0, mainwin, SLOT(reloadProfile()));
    } else if (needs_restart) {
        mainwin->RestartApplication();
    } else {
        mainwin->getDaily()->LoadDate(mainwin->getDaily()->getDate());
        // Save early.. just in case..
        mainwin->getDaily()->graphView()->SaveSettings("Daily");
        mainwin->getOverview()->graphView()->SaveSettings("Overview");
    }

    return true;
}

void PreferencesDialog::saveChanInfo()
{
    // Change focus to force save of any open editors..
    ui->channelSearch->setFocus();

    int toprows = chanModel->rowCount();
    bool ok;

    for (int i=0; i < toprows; i++) {
        QStandardItem * topitem = chanModel->item(i,0);

        if (!topitem) continue;
        int rows = topitem->rowCount();
        for (int j=0; j< rows; ++j) {
            QStandardItem * item = topitem->child(j, 0);
            if (!item) continue;

            ChannelID id = item->data(Qt::UserRole).toUInt(&ok);
            schema::Channel & chan = schema::channel[id];
            if (chan.isNull()) continue;
            chan.setEnabled(item->checkState() == Qt::Checked ? true : false);
            chan.setFullname(item->text());
            chan.setDefaultColor(QColor(topitem->child(j,1)->data(Qt::UserRole).toUInt()));
            chan.setShowInOverview(topitem->child(j,2)->checkState() == Qt::Checked);
            QString ts = topitem->child(j,3)->text();
            schema::ChanType type = schema::MINOR_FLAG;
            for (QHash<schema::ChanType, QString>::iterator it = channeltype.begin(); it!= channeltype.end(); ++it) {
                if (it.value() == ts) {
                    type = it.key();
                    break;
                }
            }
            chan.setType(type);
            chan.setLabel(topitem->child(j,4)->text());
            chan.setDescription(topitem->child(j,5)->text());
        }
    }
}
void PreferencesDialog::saveWaveInfo()
{
    // Change focus to force save of any open editors..
    ui->waveSearch->setFocus();

    int toprows = waveModel->rowCount();

    bool ok;
    for (int i=0; i < toprows; i++) {
        QStandardItem * topitem = waveModel->item(i,0);

        if (!topitem) continue;
        int rows = topitem->rowCount();
        for (int j=0; j< rows; ++j) {
            QStandardItem * item = topitem->child(j, 0);
            if (!item) continue;

            ChannelID id = item->data(Qt::UserRole).toUInt(&ok);
            schema::Channel & chan = schema::channel[id];
            if (chan.isNull()) continue;
            chan.setEnabled(item->checkState() == Qt::Checked ? true : false);
            chan.setFullname(item->text());
            chan.setDefaultColor(QColor(topitem->child(j,1)->data(Qt::UserRole).toUInt()));
            chan.setShowInOverview(topitem->child(j,2)->checkState() == Qt::Checked);
            chan.setLowerThreshold(topitem->child(j,3)->text().toDouble());
            chan.setUpperThreshold(topitem->child(j,4)->text().toDouble());
            chan.setLabel(topitem->child(j,5)->text());
            chan.setDescription(topitem->child(j,6)->text());
        }
    }
}

void PreferencesDialog::on_combineSlider_valueChanged(int position)
{
    if (position > 0) {
        ui->combineLCD->display(position);
    } else { ui->combineLCD->display(STR_TR_Off); }
}

void PreferencesDialog::on_IgnoreSlider_valueChanged(int position)
{
    if (position > 0) {
        ui->IgnoreLCD->display(position);
    } else { ui->IgnoreLCD->display(STR_TR_Off); }
}

#include "mainwindow.h"
extern MainWindow *mainwin;
void PreferencesDialog::RefreshLastChecked()
{
    ui->updateLastChecked->setText(AppSetting->updatesLastChecked().toString(Qt::SystemLocaleLongDate));
}

void PreferencesDialog::on_checkForUpdatesButton_clicked()
{
    mainwin->CheckForUpdates();
}

MySortFilterProxyModel::MySortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{

}

bool MySortFilterProxyModel::filterAcceptsRow(int source_row,
        const QModelIndex &source_parent) const
{
    if (source_parent == qobject_cast<QStandardItemModel *>
            (sourceModel())->invisibleRootItem()->index()) {
        // always accept children of rootitem, since we want to filter their children
        return true;
    }

    return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

// Might still be useful..
//void PreferencesDialog::on_resetGraphButton_clicked()
//{
//    QString title = tr("Confirmation");
//    QString text  = tr("Are you sure you want to reset your graph preferences to the defaults?");
//    StandardButtons buttons = QMessageBox::Yes | QMessageBox::No;
//    StandardButton  defaultButton = QMessageBox::No;

//    // Display confirmation dialog.
//    StandardButton choice = QMessageBox::question(this, title, text, buttons, defaultButton);

//    if (choice == QMessageBox::No) {
//        return;
//    }

//    gGraphView *views[3] = {0};
//    views[0] = mainwin->getDaily()->graphView();
//    views[1] = mainwin->getOverview()->graphView();

//    // Iterate over all graph containers.
//    for (unsigned j = 0; j < 3; j++) {
//        gGraphView *view = views[j];

//        if (!view) {
//            continue;
//        }

//        // Iterate over all contained graphs.
//        for (int i = 0; i < view->size(); i++) {
//            gGraph *g = (*view)[i];
//            g->setRecMaxY(0); // FIXME: should be g->reset(), but need other patches to land.
//            g->setRecMinY(0);
//            g->setVisible(true);
//        }

//        view->updateScale();
//    }

//    resetGraphModel();
//    ui->graphView->update();
//}

void PreferencesDialog::on_createSDBackups_toggled(bool checked)
{
    if (profile->session->backupCardData() && !checked) {
        QList<Machine *> mach = p_profile->GetMachines(MT_CPAP);
        bool haveS9 = false;

        for (int i = 0; i < mach.size(); i++) {
            if (mach[i]->loaderName() == STR_MACH_ResMed) {
                haveS9 = true;
                break;
            }
        }

        if (haveS9
                && QMessageBox::question(this, tr("This may not be a good idea"),
                                         tr("ResMed S9 machines routinely delete certain data from your SD card older than 7 and 30 days (depending on resolution).")
                                         + " " + tr("If you ever need to reimport this data again (whether in SleepyHead or ResScan) this data won't come back.")
                                         + " " + tr("If you need to conserve disk space, please remember to carry out manual backups.") +
                                         " " + tr("Are you sure you want to disable these backups?"), QMessageBox::Yes,
                                         QMessageBox::No) == QMessageBox::No) {
            ui->createSDBackups->setChecked(true);
            return;
        }
    }

    if (!checked) { ui->compressSDBackups->setChecked(false); }

    ui->compressSDBackups->setEnabled(checked);
}

void PreferencesDialog::on_okButton_clicked()
{
    if (Save()) {
        accept();
    }
}

void PreferencesDialog::on_scrollDampeningSlider_valueChanged(int value)
{
    if (value > 0) {
        ui->scrollDampDisplay->setText(QString("%1%2").arg(value * 10).arg(STR_UNIT_ms));
    } else { ui->scrollDampDisplay->setText(STR_TR_Off); }
}

void PreferencesDialog::on_tooltipTimeoutSlider_valueChanged(int value)
{
    ui->tooltipTimeoutDisplay->setText(QString("%1%2").arg(double(value)/1000.0,0,'f',1).arg(STR_UNIT_s));
}

void PreferencesDialog::on_resetChannelDefaults_clicked()
{
    if (QMessageBox::question(this, STR_MessageBox_Warning, QObject::tr("Are you sure you want to reset all your channel colors and settings to defaults?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        schema::resetChannels();
        saveWaveInfo();
        InitChanInfo();
    }
}

void PreferencesDialog::on_createSDBackups_clicked(bool checked)
{
    if (!checked && p_profile->session->backupCardData()) {
        if (QMessageBox::question(this, STR_MessageBox_Warning, tr("Switching off automatic backups is not a good idea, because SleepyHead needs these to rebuild the database if errors are found.")+"\n\n"+
                              tr("Are you really sure you want to do this?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        } else {
            ui->createSDBackups->setChecked(true);
        }
    }
}

void PreferencesDialog::on_channelSearch_textChanged(const QString &arg1)
{
    chanFilterModel->setFilterFixedString(arg1);
}

void PreferencesDialog::on_chanView_doubleClicked(const QModelIndex &index)
{
    if (index.column() == 1) {
        QColorDialog a;

        if (!(index.flags() & Qt::ItemIsEnabled)) return;
        quint32 color = index.data(Qt::UserRole).toUInt();

        a.setCurrentColor(QColor((QRgb)color));

        if (a.exec() == QColorDialog::Accepted) {
            quint32 cv = a.currentColor().rgba();

            chanFilterModel->setData(index, cv, Qt::UserRole);
            chanFilterModel->setData(index, a.currentColor(), Qt::BackgroundRole);

        }

    }
}

void PreferencesDialog::on_waveSearch_textChanged(const QString &arg1)
{
    waveFilterModel->setFilterFixedString(arg1);
}

void PreferencesDialog::on_resetWaveformChannels_clicked()
{
    if (QMessageBox::question(this, STR_MessageBox_Warning, QObject::tr("Are you sure you want to reset all your waveform channel colors and settings to defaults?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        schema::resetChannels();
        saveChanInfo(); // reset clears EVERYTHING, so have to put these back in case they cancel.
        InitWaveInfo();
    }
}

void PreferencesDialog::on_waveView_doubleClicked(const QModelIndex &index)
{
    if (index.column() == 1) {
        QColorDialog a;

        if (!(index.flags() & Qt::ItemIsEnabled)) return;
        quint32 color = index.data(Qt::UserRole).toUInt();

        a.setCurrentColor(QColor((QRgb)color));

        if (a.exec() == QColorDialog::Accepted) {
            quint32 cv = a.currentColor().rgba();

            waveFilterModel->setData(index, cv, Qt::UserRole);
            waveFilterModel->setData(index, a.currentColor(), Qt::BackgroundRole);

        }

    }
}

void PreferencesDialog::on_maskLeaks4Slider_valueChanged(int value)
{
    ui->maskLeaks4Label->setText(tr("%1 %2").arg(value/10.0f, 5,'f',1).arg(STR_UNIT_LPM));
}

void PreferencesDialog::on_maskLeaks20Slider_valueChanged(int value)
{
    ui->maskLeaks20Label->setText(tr("%1 %2").arg(value/10.0f, 5,'f',1).arg(STR_UNIT_LPM));
}

void PreferencesDialog::on_calculateUnintentionalLeaks_toggled(bool)
{

}
