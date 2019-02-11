/* Overview GUI Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QCalendarWidget>
#include <QTextCharFormat>
//#include <QSystemLocale>
#include <QDebug>
#include <QDateTimeEdit>
#include <QCalendarWidget>
#include <QFileDialog>
#include <QMessageBox>
//#include <QProgressBar>

#include "SleepLib/profiles.h"
#include "overview.h"
#include "ui_overview.h"
#include "common_gui.h"
#include "Graphs/gXAxis.h"
#include "Graphs/gLineChart.h"
#include "Graphs/gYAxis.h"

#include "mainwindow.h"
extern MainWindow *mainwin;

Overview::Overview(QWidget *parent, gGraphView *shared) :
    QWidget(parent),
    ui(new Ui::Overview),
    m_shared(shared)
{
    ui->setupUi(this);

    // Set Date controls locale to 4 digit years
    QLocale locale = QLocale::system();
    QString shortformat = locale.dateFormat(QLocale::ShortFormat);

    if (!shortformat.toLower().contains("yyyy")) {
        shortformat.replace("yy", "yyyy");
    }

    ui->dateStart->setDisplayFormat(shortformat);
    ui->dateEnd->setDisplayFormat(shortformat);

    Qt::DayOfWeek dow = firstDayOfWeekFromLocale();

    ui->dateStart->calendarWidget()->setFirstDayOfWeek(dow);
    ui->dateEnd->calendarWidget()->setFirstDayOfWeek(dow);


    // Stop both calendar drop downs highlighting weekends in red
    QTextCharFormat format = ui->dateStart->calendarWidget()->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    ui->dateStart->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->dateStart->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    ui->dateEnd->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->dateEnd->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);

    // Connect the signals to update which days have CPAP data when the month is changed
    connect(ui->dateStart->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateStart_currentPageChanged(int, int)));
    connect(ui->dateEnd->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateEnd_currentPageChanged(int, int)));

    QVBoxLayout *framelayout = new QVBoxLayout;
    ui->graphArea->setLayout(framelayout);

    QFrame *border = new QFrame(ui->graphArea);

    framelayout->setMargin(1);
    border->setFrameShape(QFrame::StyledPanel);
    framelayout->addWidget(border,1);

    // Create the horizontal layout to hold the GraphView object and it's scrollbar
    layout = new QHBoxLayout(border);
    layout->setSpacing(0); // remove the ugly margins/spacing
    layout->setMargin(0);
    layout->setContentsMargins(0, 0, 0, 0);
    border->setLayout(layout);
    border->setAutoFillBackground(false);

    // Create the GraphView Object
    GraphView = new gGraphView(ui->graphArea, m_shared);
    GraphView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    GraphView->setEmptyText(STR_Empty_NoData);

    // Create the custom scrollbar and attach to GraphView
    scrollbar = new MyScrollBar(ui->graphArea);
    scrollbar->setOrientation(Qt::Vertical);
    scrollbar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    scrollbar->setMaximumWidth(20);
    GraphView->setScrollBar(scrollbar);


    // Add the graphView and scrollbar to the layout.
    layout->addWidget(GraphView, 1);
    layout->addWidget(scrollbar, 0);
    layout->layout();

    dateLabel = new MyLabel(this);
    dateLabel->setAlignment(Qt::AlignVCenter);
    dateLabel->setText("[Date Widget]");
    QFont font = dateLabel->font();
    font.setPointSizeF(font.pointSizeF()*1.3F);
    dateLabel->setFont(font);
    QPalette palette = dateLabel->palette();
    palette.setColor(QPalette::Base, Qt::blue);
    dateLabel->setPalette(palette);

    ui->dateLayout->addWidget(dateLabel,1);

    RebuildGraphs(false);

    ui->rangeCombo->setCurrentIndex(p_profile->general->lastOverviewRange());

    icon_on = new QIcon(":/icons/session-on.png");
    icon_off = new QIcon(":/icons/session-off.png");

    GraphView->resetLayout();
    GraphView->LoadSettings("Overview"); //no trans

    GraphView->setEmptyImage(QPixmap(":/docs/sheep.png"));

    connect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    connect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    connect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));
}

Overview::~Overview()
{
    disconnect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));
    disconnect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    disconnect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    disconnect(ui->dateEnd->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateEnd_currentPageChanged(int, int)));
    disconnect(ui->dateStart->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateStart_currentPageChanged(int, int)));

    // Save graph orders and pin status, etc...
    GraphView->SaveSettings("Overview");//no trans

    delete ui;
}

void Overview::RebuildGraphs(bool reset)
{
    qint64 minx, maxx;
    if (reset) {
        GraphView->GetXBounds(minx, maxx);
    }

    GraphView->trashGraphs(true);
    ChannelID ahicode = p_profile->general->calculateRDI() ? CPAP_RDI : CPAP_AHI;

    if (ahicode == CPAP_RDI) {
        AHI = createGraph("AHIBreakdown", STR_TR_RDI, tr("Respiratory\nDisturbance\nIndex"));
    } else {
        AHI = createGraph("AHIBreakdown", STR_TR_AHI, tr("Apnea\nHypopnea\nIndex"));
    }

    ahi = new gAHIChart();
    AHI->AddLayer(ahi);

    UC = createGraph(STR_GRAPH_Usage, tr("Usage"), tr("Usage\n(hours)"));
    UC->AddLayer(uc = new gUsageChart());

    STG = createGraph("New Session", tr("Session Times"), tr("Session Times"),  YT_Time);
    stg = new gSessionTimesChart();
    STG->AddLayer(stg);

    PR = createGraph("Pressure Settings", STR_TR_Pressure, STR_TR_Pressure + "\n(" + STR_UNIT_CMH2O + ")");
    pres = new gPressureChart();
    PR->AddLayer(pres);

    TTIA = createGraph("TTIA", tr("Total Time in Apnea"), tr("Total Time in Apnea\n(Minutes)"));
    ttia = new gTTIAChart();
    TTIA->AddLayer(ttia);

    QHash<ChannelID, schema::Channel *>::iterator chit;
    QHash<ChannelID, schema::Channel *>::iterator chit_end = schema::channel.channels.end();
    for (chit = schema::channel.channels.begin(); chit != chit_end; ++chit) {
        schema::Channel * chan = chit.value();

        if (chan->showInOverview()) {
            ChannelID code = chan->id();
            QString name = chan->fullname();
            if (name.length() > 16) name = chan->label();
            gGraph *G = createGraph(chan->code(), name, chan->description());
            if ((chan->type() == schema::FLAG) || (chan->type() == schema::MINOR_FLAG)) {
                gSummaryChart * sc = new gSummaryChart(chan->code(), MT_CPAP);
                sc->addCalc(code, ST_CPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
            } else if (chan->type() == schema::SPAN) {
                gSummaryChart * sc = new gSummaryChart(chan->code(), MT_CPAP);
                sc->addCalc(code, ST_SPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
            } else if (chan->type() == schema::WAVEFORM) {
                G->AddLayer(new gSummaryChart(code, chan->machtype()));
            } else if (chan->type() == schema::UNKNOWN) {
                gSummaryChart * sc = new gSummaryChart(chan->code(), MT_CPAP);
                sc->addCalc(code, ST_CPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
            }
        }

    }

    WEIGHT = createGraph(STR_GRAPH_Weight, STR_TR_Weight, STR_TR_Weight, YT_Weight);
    BMI = createGraph(STR_GRAPH_BMI, STR_TR_BMI, tr("Body\nMass\nIndex"));
    ZOMBIE = createGraph(STR_GRAPH_Zombie, STR_TR_Zombie, tr("How you felt\n(0-10)"));

    if (reset) {
        GraphView->resetLayout();
        GraphView->setDay(nullptr);
        GraphView->SetXBounds(minx, maxx, 0, false);
        GraphView->resetLayout();
        updateGraphCombo();
    }
}

gGraph *Overview::createGraph(QString code, QString name, QString units, YTickerType yttype)
{
    int default_height = AppSetting->graphHeight();
    gGraph *g = new gGraph(code, GraphView, name, units, default_height, 0);

    gYAxis *yt;

    switch (yttype) {
    case YT_Time:
        yt = new gYAxisTime(true); // Time scale
        break;

    case YT_Weight:
        yt = new gYAxisWeight(p_profile->general->unitSystem());
        break;

    default:
        yt = new gYAxis(); // Plain numeric scale
        break;
    }

    g->AddLayer(yt, LayerLeft, gYAxis::Margin);
    gXAxisDay *x = new gXAxisDay();
    g->AddLayer(x, LayerBottom, 0, gXAxisDay::Margin);
    g->AddLayer(new gXGrid());
    return g;
}

void Overview::on_LineCursorUpdate(double time)
{
    if (time > 1) {
        // even though the generated string is displayed to the user
        // no time zone conversion is neccessary, so pass UTC
        // to prevent QT from automatically converting to local time
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(time, Qt::UTC);
        QString txt = dt.toString("dd MMM yyyy (dddd)");
        dateLabel->setText(txt);
    } else dateLabel->setText(QString(GraphView->emptyText()));
}

void Overview::on_RangeUpdate(double minx, double /* maxx */)
{
    if (minx > 1) {
        dateLabel->setText(GraphView->getRangeString());
    } else {
        dateLabel->setText(QString(GraphView->emptyText()));
    }
}

void Overview::ReloadGraphs()
{
    GraphView->setDay(nullptr);
    updateCube();

    on_rangeCombo_activated(ui->rangeCombo->currentIndex());
}

void Overview::updateGraphCombo()
{
    ui->graphCombo->clear();
    gGraph *g;
    //    ui->graphCombo->addItem("Show All Graphs");
    //    ui->graphCombo->addItem("Hide All Graphs");
    //    ui->graphCombo->addItem("---------------");

    for (int i = 0; i < GraphView->size(); i++) {
        g = (*GraphView)[i];

        if (g->isEmpty()) { continue; }

        if (g->visible()) {
            ui->graphCombo->addItem(*icon_on, g->title(), true);
        } else {
            ui->graphCombo->addItem(*icon_off, g->title(), false);
        }
    }

    ui->graphCombo->setCurrentIndex(0);
    updateCube();
}

void Overview::ResetGraphs()
{
    QDate start = ui->dateStart->date();
    QDate end = ui->dateEnd->date();
    GraphView->setDay(nullptr);
    updateCube();

    if (start.isValid() && end.isValid()) {
        setRange(start, end);
    }
}

void Overview::ResetGraph(QString name)
{
    gGraph *g = GraphView->findGraph(name);

    if (!g) { return; }

    g->setDay(nullptr);
    GraphView->redraw();
}

void Overview::RedrawGraphs()
{
    GraphView->redraw();
}

void Overview::UpdateCalendarDay(QDateEdit *dateedit, QDate date)
{
    QCalendarWidget *calendar = dateedit->calendarWidget();
    QTextCharFormat bold;
    QTextCharFormat cpapcol;
    QTextCharFormat normal;
    QTextCharFormat oxiday;
    bold.setFontWeight(QFont::Bold);
    cpapcol.setForeground(QBrush(Qt::blue, Qt::SolidPattern));
    cpapcol.setFontWeight(QFont::Bold);
    oxiday.setForeground(QBrush(Qt::red, Qt::SolidPattern));
    oxiday.setFontWeight(QFont::Bold);
    bool hascpap = p_profile->FindDay(date, MT_CPAP) != nullptr;
    bool hasoxi = p_profile->FindDay(date, MT_OXIMETER) != nullptr;
    //bool hasjournal=p_profile->GetDay(date,MT_JOURNAL)!=nullptr;

    if (hascpap) {
        if (hasoxi) {
            calendar->setDateTextFormat(date, oxiday);
        } else {
            calendar->setDateTextFormat(date, cpapcol);
        }
    } else if (p_profile->GetDay(date)) {
        calendar->setDateTextFormat(date, bold);
    } else {
        calendar->setDateTextFormat(date, normal);
    }

    calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
}
void Overview::dateStart_currentPageChanged(int year, int month)
{
    QDate d(year, month, 1);
    int dom = d.daysInMonth();

    for (int i = 1; i <= dom; i++) {
        d = QDate(year, month, i);
        UpdateCalendarDay(ui->dateStart, d);
    }
}
void Overview::dateEnd_currentPageChanged(int year, int month)
{
    QDate d(year, month, 1);
    int dom = d.daysInMonth();

    for (int i = 1; i <= dom; i++) {
        d = QDate(year, month, i);
        UpdateCalendarDay(ui->dateEnd, d);
    }
}


void Overview::on_dateEnd_dateChanged(const QDate &date)
{
    qint64 d1 = qint64(QDateTime(ui->dateStart->date(), QTime(0, 10, 0), Qt::UTC).toTime_t()) * 1000L;
    qint64 d2 = qint64(QDateTime(date, QTime(23, 0, 0), Qt::UTC).toTime_t()) * 1000L;
    GraphView->SetXBounds(d1, d2);
    ui->dateStart->setMaximumDate(date);
}

void Overview::on_dateStart_dateChanged(const QDate &date)
{
    qint64 d1 = qint64(QDateTime(date, QTime(0, 10, 0), Qt::UTC).toTime_t()) * 1000L;
    qint64 d2 = qint64(QDateTime(ui->dateEnd->date(), QTime(23, 0, 0), Qt::UTC).toTime_t()) * 1000L;
    GraphView->SetXBounds(d1, d2);
    ui->dateEnd->setMinimumDate(date);
}

void Overview::on_toolButton_clicked()
{
    qint64 d1 = qint64(QDateTime(ui->dateStart->date(), QTime(0, 10, 0), Qt::UTC).toTime_t()) * 1000L;
    qint64 d2 = qint64(QDateTime(ui->dateEnd->date(), QTime(23, 00, 0), Qt::UTC).toTime_t()) * 1000L;
    GraphView->SetXBounds(d1, d2);
}

void Overview::ResetGraphLayout()
{
    GraphView->resetLayout();
}

void Overview::on_rangeCombo_activated(int index)
{
    p_profile->general->setLastOverviewRange(index);
    ui->dateStart->setMinimumDate(p_profile->FirstDay());
    ui->dateEnd->setMaximumDate(p_profile->LastDay());

    QDate end = p_profile->LastDay();
    QDate start;

    if (index == 8) { // Custom
        ui->dateStartLabel->setEnabled(true);
        ui->dateEndLabel->setEnabled(true);
        ui->dateEnd->setEnabled(true);
        ui->dateStart->setEnabled(true);

        ui->dateStart->setMaximumDate(ui->dateEnd->date());
        ui->dateEnd->setMinimumDate(ui->dateStart->date());
        return;
    }

    ui->dateEnd->setEnabled(false);
    ui->dateStart->setEnabled(false);
    ui->dateStartLabel->setEnabled(false);
    ui->dateEndLabel->setEnabled(false);

    if (index == 0) {
        start = end.addDays(-6);
    } else if (index == 1) {
        start = end.addDays(-13);
    } else if (index == 2) {
        start = end.addMonths(-1).addDays(1);
    } else if (index == 3) {
        start = end.addMonths(-2).addDays(1);
    } else if (index == 4) {
        start = end.addMonths(-3).addDays(1);
    } else if (index == 5) {
        start = end.addMonths(-6).addDays(1);
    } else if (index == 6) {
        start = end.addYears(-1).addDays(1);
    } else if (index == 7) { // Everything
        start = p_profile->FirstDay();
    }

    if (start < p_profile->FirstDay()) { start = p_profile->FirstDay(); }

    setRange(start, end);
}
void Overview::setRange(QDate start, QDate end)
{
    ui->dateEnd->blockSignals(true);
    ui->dateStart->blockSignals(true);
    ui->dateStart->setMaximumDate(end);
    ui->dateEnd->setMinimumDate(start);
    ui->dateStart->setDate(start);
    ui->dateEnd->setDate(end);
    ui->dateEnd->blockSignals(false);
    ui->dateStart->blockSignals(false);
    this->on_toolButton_clicked();
    updateGraphCombo();

}

void Overview::on_graphCombo_activated(int index)
{
    if (index < 0) {
        return;
    }

    gGraph *g;
    QString s;
    s = ui->graphCombo->currentText();
    bool b = !ui->graphCombo->itemData(index, Qt::UserRole).toBool();
    ui->graphCombo->setItemData(index, b, Qt::UserRole);

    if (b) {
        ui->graphCombo->setItemIcon(index, *icon_on);
    } else {
        ui->graphCombo->setItemIcon(index, *icon_off);
    }

    g = GraphView->findGraphTitle(s);
    g->setVisible(b);

    updateCube();
    GraphView->updateScale();
    GraphView->redraw();
}
void Overview::updateCube()
{
    if ((GraphView->visibleGraphs() == 0)) {
        ui->toggleVisibility->setArrowType(Qt::UpArrow);
        ui->toggleVisibility->setToolTip(tr("Show all graphs"));
        ui->toggleVisibility->blockSignals(true);
        ui->toggleVisibility->setChecked(true);
        ui->toggleVisibility->blockSignals(false);

        if (ui->graphCombo->count() > 0) {
            GraphView->setEmptyText(STR_Empty_NoGraphs);

        } else {
            GraphView->setEmptyText(STR_Empty_NoData);
        }
    } else {
        ui->toggleVisibility->setArrowType(Qt::DownArrow);
        ui->toggleVisibility->setToolTip(tr("Hide all graphs"));
        ui->toggleVisibility->blockSignals(true);
        ui->toggleVisibility->setChecked(false);
        ui->toggleVisibility->blockSignals(false);
    }
}

void Overview::on_toggleVisibility_clicked(bool checked)
{
    gGraph *g;
    QString s;
    QIcon *icon = checked ? icon_off : icon_on;

    for (int i = 0; i < ui->graphCombo->count(); i++) {
        s = ui->graphCombo->itemText(i);
        ui->graphCombo->setItemIcon(i, *icon);
        ui->graphCombo->setItemData(i, !checked, Qt::UserRole);
        g = GraphView->findGraphTitle(s);
        g->setVisible(!checked);
    }

    updateCube();
    GraphView->updateScale();
    GraphView->redraw();
}
