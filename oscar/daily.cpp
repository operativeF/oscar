/* Daily Panel
 *
 * Copyright (c) 2019 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QTextCharFormat>
#include <QPalette>
#include <QTextBlock>
#include <QColorDialog>
#include <QSpacerItem>
#include <QBuffer>
#include <QPixmap>
#include <QMessageBox>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSpacerItem>
#include <QFontMetrics>
#include <QLabel>
#include <QMutexLocker>

#include <cmath>

#include "daily.h"
#include "ui_daily.h"

#include "common_gui.h"
#include "SleepLib/profiles.h"
#include "SleepLib/session.h"
#include "Graphs/graphdata_custom.h"
#include "Graphs/gLineOverlay.h"
#include "Graphs/gFlagsLine.h"
#include "Graphs/gFooBar.h"
#include "Graphs/gXAxis.h"
#include "Graphs/gYAxis.h"
#include "Graphs/gSegmentChart.h"
#include "Graphs/gStatsLine.h"
#include "Graphs/gdailysummary.h"
#include "Graphs/MinutesAtPressure.h"

extern MainWindow * mainwin;

QString htmlLeftHeader;
QString htmlLeftAHI;
QString htmlLeftMachineInfo;
QString htmlLeftSleepTime;
QString htmlLeftIndices;
QString htmlLeftPieChart = "";
QString htmlLeftNoHours = "";
QString htmlLeftStatistics;
QString htmlLeftOximeter;
QString htmlLeftMachineSettings;
QString htmlLeftSessionInfo;
QString htmlLeftFooter;


// This was Sean Stangl's idea.. but I couldn't apply that patch.
inline QString channelInfo(ChannelID code) {
    return schema::channel[code].fullname()+"\n"+schema::channel[code].description()+"\n("+schema::channel[code].units()+")";
//    return schema::channel[code].fullname()+"\n"+schema::channel[code].description()
//            + (schema::channel[code].units() != "0" ? "\n("+schema::channel[code].units()+")" : "");
}

// Charts displayed on the Daily page are defined in the Daily::Daily constructor.  They consist of some hard-coded charts and a table
// of channel codes for which charts are generated.  If the list of channel codes is changed, the graph order lists below will need to
// be changed correspondingly.
//
// Note that "graph codes" are strings used to identify graphs and are not the same as "channel codes."  The mapping between channel codes
// and graph codes is found in schema.cpp.  (What we here call 'graph cdoes' are called 'lookup codes' in schema.cpp.)
//
//
// List here the graph codes in the order they are to be displayed.
// Do NOT list a code twice, or Oscar will crash when the profile is closed!
//
// Standard graph order
const QList<QString> standardGraphOrder = {STR_GRAPH_SleepFlags, STR_GRAPH_FlowRate, STR_GRAPH_Pressure, STR_GRAPH_LeakRate, STR_GRAPH_FlowLimitation,
                                           STR_GRAPH_Snore, STR_GRAPH_TidalVolume, STR_GRAPH_MaskPressure, STR_GRAPH_RespRate, STR_GRAPH_MinuteVent,
                                           STR_GRAPH_PTB, STR_GRAPH_RespEvent, STR_GRAPH_Ti, STR_GRAPH_Te,
                                           STR_GRAPH_SleepStage, STR_GRAPH_Inclination, STR_GRAPH_Orientation, STR_GRAPH_TestChan1,
                                           STR_GRAPH_Oxi_Pulse, STR_GRAPH_Oxi_SPO2, STR_GRAPH_Oxi_Perf, STR_GRAPH_Oxi_Plethy,
                                           STR_GRAPH_AHI, STR_GRAPH_TAP
                                          };

// Advanced graph order
const QList<QString> advancedGraphOrder = {STR_GRAPH_SleepFlags, STR_GRAPH_FlowRate, STR_GRAPH_MaskPressure, STR_GRAPH_TidalVolume, STR_GRAPH_MinuteVent,
                                           STR_GRAPH_Ti, STR_GRAPH_Te, STR_GRAPH_FlowLimitation, STR_GRAPH_Pressure, STR_GRAPH_LeakRate, STR_GRAPH_Snore,
                                           STR_GRAPH_RespRate, STR_GRAPH_PTB, STR_GRAPH_RespEvent,
                                           STR_GRAPH_SleepStage, STR_GRAPH_Inclination, STR_GRAPH_Orientation, STR_GRAPH_TestChan1,
                                           STR_GRAPH_Oxi_Pulse, STR_GRAPH_Oxi_SPO2, STR_GRAPH_Oxi_Perf, STR_GRAPH_Oxi_Plethy,
                                           STR_GRAPH_AHI, STR_GRAPH_TAP
                                          };

// CPAP modes that should have Advanced graphs
const QList<int> useAdvancedGraphs = {MODE_ASV, MODE_ASV_VARIABLE_EPAP, MODE_AVAPS};


void Daily::setCalendarVisible(bool visible)
{
    on_calButton_toggled(visible);
}

void Daily::setSidebarVisible(bool visible)
{
    QList<int> a;


    int panel_width = visible ? AppSetting->dailyPanelWidth() : 0;
    qDebug() << "Daily Left Panel Width is " << panel_width;
    a.push_back(panel_width);
    a.push_back(this->width() - panel_width);
    ui->splitter_2->setStretchFactor(1,1);
    ui->splitter_2->setSizes(a);
    ui->splitter_2->setStretchFactor(1,1);
}

Daily::Daily(QWidget *parent,gGraphView * shared)
    :QWidget(parent), ui(new Ui::Daily)
{
    qDebug() << "Creating new Daily object";
    ui->setupUi(this);

    // Remove Incomplete Extras Tab
    //ui->tabWidget->removeTab(3);

    ZombieMeterMoved=false;
    BookmarksChanged=false;

    lastcpapday=nullptr;

    setSidebarVisible(true);

    layout=new QHBoxLayout();
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->setContentsMargins(0,0,0,0);

    dateDisplay=new MyLabel(this);
    dateDisplay->setAlignment(Qt::AlignCenter);
    QFont font = dateDisplay->font();
    font.setPointSizeF(font.pointSizeF()*1.3F);
    dateDisplay->setFont(font);
    QPalette palette = dateDisplay->palette();
    palette.setColor(QPalette::Base, Qt::blue);
    dateDisplay->setPalette(palette);
    //dateDisplay->setTextFormat(Qt::RichText);
    ui->sessionBarLayout->addWidget(dateDisplay,1);

//    const bool sessbar_under_graphs=false;
//    if (sessbar_under_graphs) {
//        ui->sessionBarLayout->addWidget(sessbar,1);
//    } else {
//        ui->splitter->insertWidget(2,sessbar);
//        sessbar->setMaximumHeight(sessbar->height());
//        sessbar->setMinimumHeight(sessbar->height());
//    }


    ui->calNavWidget->setMaximumHeight(ui->calNavWidget->height());
    ui->calNavWidget->setMinimumHeight(ui->calNavWidget->height());
    QWidget *widget = new QWidget(ui->tabWidget);
    sessionbar = new SessionBar(widget);
    sessionbar->setMinimumHeight(25);
    sessionbar->setMouseTracking(true);
    connect(sessionbar, SIGNAL(sessionClicked(Session*)), this, SLOT(doToggleSession(Session*)));
    QVBoxLayout *layout2 = new QVBoxLayout();
    layout2->setMargin(0);
    widget->setLayout(layout2);

    webView=new MyTextBrowser(widget);
    webView->setOpenLinks(false);
    layout2->insertWidget(0,webView, 1);
    layout2->insertWidget(1,sessionbar,0);
    // add the sessionbar after it.

    ui->tabWidget->insertTab(0, widget, QIcon(), tr("Details"));

    ui->graphFrame->setLayout(layout);
    //ui->graphMainArea->setLayout(layout);

    ui->graphMainArea->setAutoFillBackground(false);

    GraphView=new gGraphView(ui->graphFrame,shared);
//    qDebug() << "New GraphView object created in Daily";
//    sleep(3);
    GraphView->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));

    snapGV=new gGraphView(GraphView);
    snapGV->setMinimumSize(172,172);
    snapGV->hideSplitter();
    snapGV->hide();

    scrollbar=new MyScrollBar(ui->graphFrame);
    scrollbar->setOrientation(Qt::Vertical);
    scrollbar->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Expanding);
    scrollbar->setMaximumWidth(20);

    ui->bookmarkTable->setColumnCount(2);
    ui->bookmarkTable->setColumnWidth(0,70);
    //ui->bookmarkTable->setEditTriggers(QAbstractItemView::SelectedClicked);
    //ui->bookmarkTable->setColumnHidden(2,true);
    //ui->bookmarkTable->setColumnHidden(3,true);
    GraphView->setScrollBar(scrollbar);
    layout->addWidget(GraphView,1);
    layout->addWidget(scrollbar,0);

    int default_height = AppSetting->graphHeight();

    gGraph *GAHI = nullptr,
//            *TAP = nullptr,
            *SF = nullptr,
            *AHI = nullptr;

//    const QString STR_GRAPH_DailySummary = "DailySummary";

//    gGraph * SG;
//    graphlist[STR_GRAPH_DailySummary] = SG = new gGraph(STR_GRAPH_DailySummary, GraphView, tr("Summary"), tr("Summary of this daily information"), default_height);
//    SG->AddLayer(new gLabelArea(nullptr),LayerLeft,gYAxis::Margin);
//    SG->AddLayer(new gDailySummary());

    graphlist[STR_GRAPH_SleepFlags] = SF = new gGraph(STR_GRAPH_SleepFlags, GraphView, STR_TR_EventFlags, STR_TR_EventFlags, default_height);
    SF->setPinned(true);

    //============================================
    // Create graphs from 'interesting' CPAP codes
    //
    // If this list of codes is changed, you must
    // also adjust the standard and advanced graph
    // order at the beginning of daily.cpp.
    //============================================
    const ChannelID cpapcodes[] = {
        CPAP_FlowRate, CPAP_Pressure, CPAP_Leak, CPAP_FLG, CPAP_Snore, CPAP_TidalVolume,
        CPAP_MaskPressure, CPAP_RespRate, CPAP_MinuteVent, CPAP_PTB, CPAP_RespEvent, CPAP_Ti, CPAP_Te,
        /*  CPAP_IE, */   ZEO_SleepStage, POS_Inclination, POS_Orientation, CPAP_Test1
    };

    // Create graphs from the cpap code list
    int cpapsize = sizeof(cpapcodes) / sizeof(ChannelID);

    for (int i=0; i < cpapsize; ++i) {
        ChannelID code = cpapcodes[i];
        graphlist[schema::channel[code].code()] = new gGraph(schema::channel[code].code(), GraphView, schema::channel[code].label(), channelInfo(code), default_height);
//        qDebug() << "Creating graph for code" << code << schema::channel[code].code();
    }

    const ChannelID oximetercodes[] = {
        OXI_Pulse, OXI_SPO2, OXI_Perf, OXI_Plethy
    };

    // Add graphs from the Oximeter code list
    int oxisize = sizeof(oximetercodes) / sizeof(ChannelID);

    //int oxigrp=p_profile->ExistsAndTrue("SyncOximetry") ? 0 : 1; // Contemplating killing this setting...
    for (int i=0; i < oxisize; ++i) {
        ChannelID code = oximetercodes[i];
        graphlist[schema::channel[code].code()] = new gGraph(schema::channel[code].code(), GraphView, schema::channel[code].label(), channelInfo(code), default_height);
    }

    // Check for some impossible conditions
    if ( p_profile == nullptr ) {
        qDebug() << "In daily, p_profile is NULL";
        return;
    }
    else if (p_profile->general == nullptr ) {
        qDebug() << "In daily, p_profile->general is NULL";
        return;
    }

    // Decide whether we are using AHI or RDI and create graph for the one we are using
    if (p_profile->general->calculateRDI()) {
        AHI=new gGraph(STR_GRAPH_AHI, GraphView,STR_TR_RDI, channelInfo(CPAP_RDI), default_height);
    } else {
        AHI=new gGraph(STR_GRAPH_AHI, GraphView,STR_TR_AHI, channelInfo(CPAP_AHI), default_height);
    }

    graphlist[STR_GRAPH_AHI] = AHI;

    // Event breakdown graph
    graphlist[STR_GRAPH_EventBreakdown] = GAHI = new gGraph(STR_GRAPH_EventBreakdown, snapGV,tr("Breakdown"),tr("events"),172);
    gSegmentChart * evseg=new gSegmentChart(GST_Pie);
    evseg->AddSlice(CPAP_Hypopnea,QColor(0x40,0x40,0xff,0xff),STR_TR_H);
    evseg->AddSlice(CPAP_Apnea,QColor(0x20,0x80,0x20,0xff),STR_TR_UA);
    evseg->AddSlice(CPAP_Obstructive,QColor(0x40,0xaf,0xbf,0xff),STR_TR_OA);
    evseg->AddSlice(CPAP_ClearAirway,QColor(0xb2,0x54,0xcd,0xff),STR_TR_CA);
    evseg->AddSlice(CPAP_RERA,QColor(0xff,0xff,0x80,0xff),STR_TR_RE);
    evseg->AddSlice(CPAP_NRI,QColor(0x00,0x80,0x40,0xff),STR_TR_NR);
    evseg->AddSlice(CPAP_FlowLimit,QColor(0x40,0x40,0x40,0xff),STR_TR_FL);
    evseg->AddSlice(CPAP_SensAwake,QColor(0x40,0xC0,0x40,0xff),STR_TR_SA);
    if (AppSetting->userEventPieChart()) {
        evseg->AddSlice(CPAP_UserFlag1,QColor(0xe0,0xe0,0xe0,0xff),tr("UF1"));
        evseg->AddSlice(CPAP_UserFlag2,QColor(0xc0,0xc0,0xe0,0xff),tr("UF2"));
    }

    GAHI->AddLayer(evseg);
    GAHI->setMargins(0,0,0,0);

    // Add event flags to the event flags graph
    gFlagsGroup *fg=new gFlagsGroup();
    SF->AddLayer(fg);

    SF->setBlockZoom(true);
    SF->AddLayer(new gShadowArea());

    SF->AddLayer(new gLabelArea(fg),LayerLeft,gYAxis::Margin);

    //SF->AddLayer(new gFooBar(),LayerBottom,0,1);
    SF->AddLayer(new gXAxis(COLOR_Text,false),LayerBottom,0,gXAxis::Margin);


    // Now take care of xgrid/yaxis labels for all graphs

    // The following list contains graphs that don't have standard xgrid/yaxis labels
    QStringList skipgraph;
    skipgraph.push_back(STR_GRAPH_EventBreakdown);
    skipgraph.push_back(STR_GRAPH_SleepFlags);
//    skipgraph.push_back(STR_GRAPH_DailySummary);
    skipgraph.push_back(STR_GRAPH_TAP);

    QHash<QString, gGraph *>::iterator it;

    for (it = graphlist.begin(); it != graphlist.end(); ++it) {
        if (skipgraph.contains(it.key())) continue;
        it.value()->AddLayer(new gXGrid());
    }


    gLineChart *l;
    l=new gLineChart(CPAP_FlowRate,false,false);

    gGraph *FRW = graphlist[schema::channel[CPAP_FlowRate].code()];

    // Then the graph itself
    FRW->AddLayer(l);


//    FRW->AddLayer(AddOXI(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2)));

    bool square=AppSetting->squareWavePlots();
    gLineChart *pc=new gLineChart(CPAP_Pressure, square);
    graphlist[schema::channel[CPAP_Pressure].code()]->AddLayer(pc);

  //  graphlist[schema::channel[CPAP_Pressure].code()]->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_Ramp, COLOR_Ramp, schema::channel[CPAP_Ramp].label(), FT_Span)));

    pc->addPlot(CPAP_EPAP, square);
    pc->addPlot(CPAP_IPAPLo, square);
    pc->addPlot(CPAP_IPAP, square);
    pc->addPlot(CPAP_IPAPHi, square);

    // Create Timea at Pressure graph
    gGraph * TAP2;
    graphlist[STR_GRAPH_TAP] = TAP2 = new gGraph(STR_GRAPH_TAP, GraphView, tr("Time at Pressure"), tr("Time at Pressure"), default_height);
    MinutesAtPressure * map;
    TAP2->AddLayer(map = new MinutesAtPressure());
    TAP2->AddLayer(new gLabelArea(map),LayerLeft,gYAxis::Margin);
    TAP2->AddLayer(new gXAxisPressure(),LayerBottom,gXAxisPressure::Margin);
    TAP2->setBlockSelect(true);

    // Fill in the AHI graph
    if (p_profile->general->calculateRDI()) {
        AHI->AddLayer(new gLineChart(CPAP_RDI, square));
//        AHI->AddLayer(AddCPAP(new AHIChart(QColor("#37a24b"))));
    } else {
        AHI->AddLayer(new gLineChart(CPAP_AHI, square));
    }

    // this is class wide because the leak redline can be reset in preferences..
    // Better way would be having a search for linechart layers in graphlist[...]
    gLineChart *leakchart=new gLineChart(CPAP_Leak, square);
  //  graphlist[schema::channel[CPAP_Leak].code()]->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_LargeLeak, COLOR_LargeLeak, STR_TR_LL, FT_Span)));

    leakchart->addPlot(CPAP_LeakTotal, square);
    leakchart->addPlot(CPAP_MaxLeak, square);
//    schema::channel[CPAP_Leak].setUpperThresholdColor(Qt::red);
//    schema::channel[CPAP_Leak].setLowerThresholdColor(Qt::green);

    graphlist[schema::channel[CPAP_Leak].code()]->AddLayer(leakchart);
    //LEAK->AddLayer(AddCPAP(new gLineChart(CPAP_Leak, COLOR_Leak,square)));
    //LEAK->AddLayer(AddCPAP(new gLineChart(CPAP_MaxLeak, COLOR_MaxLeak,square)));
    graphlist[schema::channel[CPAP_Snore].code()]->AddLayer(new gLineChart(CPAP_Snore, true));

    graphlist[schema::channel[CPAP_PTB].code()]->AddLayer(new gLineChart(CPAP_PTB, square));
    graphlist[schema::channel[CPAP_Test1].code()]->AddLayer(new gLineChart(CPAP_Test1, square));
    //graphlist[schema::channel[CPAP_Test2].code()]->AddLayer(new gLineChart(CPAP_Test2, square));


    gLineChart *lc = nullptr;
    graphlist[schema::channel[CPAP_MaskPressure].code()]->AddLayer(new gLineChart(CPAP_MaskPressure, false));
    graphlist[schema::channel[CPAP_RespRate].code()]->AddLayer(lc=new gLineChart(CPAP_RespRate, square));

    graphlist[schema::channel[POS_Inclination].code()]->AddLayer(new gLineChart(POS_Inclination));
    graphlist[schema::channel[POS_Orientation].code()]->AddLayer(new gLineChart(POS_Orientation));

    graphlist[schema::channel[CPAP_MinuteVent].code()]->AddLayer(lc=new gLineChart(CPAP_MinuteVent, square));
    lc->addPlot(CPAP_TgMV, square);

    graphlist[schema::channel[CPAP_TidalVolume].code()]->AddLayer(lc=new gLineChart(CPAP_TidalVolume, square));
    //lc->addPlot(CPAP_Test2,COLOR_DarkYellow,square);

    //graphlist[schema::channel[CPAP_TidalVolume].code()]->AddLayer(AddCPAP(new gLineChart("TidalVolume2", square)));
    graphlist[schema::channel[CPAP_FLG].code()]->AddLayer(new gLineChart(CPAP_FLG, true));
    //graphlist[schema::channel[CPAP_RespiratoryEvent].code()]->AddLayer(AddCPAP(new gLineChart(CPAP_RespiratoryEvent, true)));
//    graphlist[schema::channel[CPAP_IE].code()]->AddLayer(lc=new gLineChart(CPAP_IE, false));      // this should be inverse of supplied value
    graphlist[schema::channel[CPAP_Te].code()]->AddLayer(lc=new gLineChart(CPAP_Te, false));
    graphlist[schema::channel[CPAP_Ti].code()]->AddLayer(lc=new gLineChart(CPAP_Ti, false));
    //lc->addPlot(CPAP_Test2,COLOR:DarkYellow,square);

    graphlist[schema::channel[ZEO_SleepStage].code()]->AddLayer(new gLineChart(ZEO_SleepStage, true));

//    gLineOverlaySummary *los1=new gLineOverlaySummary(STR_UNIT_EventsPerHour,5,-4);
//    gLineOverlaySummary *los2=new gLineOverlaySummary(STR_UNIT_EventsPerHour,5,-4);
//    graphlist[schema::channel[OXI_Pulse].code()]->AddLayer(AddOXI(los1->add(new gLineOverlayBar(OXI_PulseChange, COLOR_PulseChange, STR_TR_PC,FT_Span))));
//    graphlist[schema::channel[OXI_Pulse].code()]->AddLayer(AddOXI(los1));
//    graphlist[schema::channel[OXI_SPO2].code()]->AddLayer(AddOXI(los2->add(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2,FT_Span))));
//    graphlist[schema::channel[OXI_SPO2].code()]->AddLayer(AddOXI(los2));

    graphlist[schema::channel[OXI_Pulse].code()]->AddLayer(new gLineChart(OXI_Pulse, square));
    graphlist[schema::channel[OXI_SPO2].code()]->AddLayer(new gLineChart(OXI_SPO2, true));
    graphlist[schema::channel[OXI_Perf].code()]->AddLayer(new gLineChart(OXI_Perf, false));
    graphlist[schema::channel[OXI_Plethy].code()]->AddLayer(new gLineChart(OXI_Plethy, false));


    // Fix me
//    gLineOverlaySummary *los3=new gLineOverlaySummary(STR_UNIT_EventsPerHour,5,-4);
//    graphlist["INTPULSE"]->AddLayer(AddCPAP(los3->add(new gLineOverlayBar(OXI_PulseChange, COLOR_PulseChange, STR_TR_PC,FT_Span))));
//    graphlist["INTPULSE"]->AddLayer(AddCPAP(los3));
//    graphlist["INTPULSE"]->AddLayer(AddCPAP(new gLineChart(OXI_Pulse, square)));
//    gLineOverlaySummary *los4=new gLineOverlaySummary(STR_UNIT_EventsPerHour,5,-4);
//    graphlist["INTSPO2"]->AddLayer(AddCPAP(los4->add(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2,FT_Span))));
//    graphlist["INTSPO2"]->AddLayer(AddCPAP(los4));
//    graphlist["INTSPO2"]->AddLayer(AddCPAP(new gLineChart(OXI_SPO2, true)));

    graphlist[schema::channel[CPAP_PTB].code()]->setForceMaxY(100);
    graphlist[schema::channel[OXI_SPO2].code()]->setForceMaxY(100);

    for (it = graphlist.begin(); it != graphlist.end(); ++it) {
        if (skipgraph.contains(it.key())) continue;

        it.value()->AddLayer(new gYAxis(),LayerLeft,gYAxis::Margin);
        it.value()->AddLayer(new gXAxis(),LayerBottom,0,gXAxis::Margin);
    }

    if (p_profile->cpap->showLeakRedline()) {
        schema::channel[CPAP_Leak].setUpperThreshold(p_profile->cpap->leakRedline());
    } else {
        schema::channel[CPAP_Leak].setUpperThreshold(0); // switch it off
    }

    layout->layout();

    QTextCharFormat format = ui->calendar->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    ui->calendar->setWeekdayTextFormat(Qt::Saturday, format);
    ui->calendar->setWeekdayTextFormat(Qt::Sunday, format);

    Qt::DayOfWeek dow=firstDayOfWeekFromLocale();

    ui->calendar->setFirstDayOfWeek(dow);

    ui->tabWidget->setCurrentWidget(widget);

    connect(webView,SIGNAL(anchorClicked(QUrl)),this,SLOT(Link_clicked(QUrl)));

    int ews=p_profile->general->eventWindowSize();
    ui->evViewSlider->setValue(ews);
    ui->evViewLCD->display(ews);


    icon_on=new QIcon(":/icons/session-on.png");
    icon_off=new QIcon(":/icons/session-off.png");

    ui->splitter->setVisible(false);

    if (p_profile->general->unitSystem()==US_English) {
        ui->weightSpinBox->setSuffix(STR_UNIT_POUND);
        ui->weightSpinBox->setDecimals(0);
        ui->ouncesSpinBox->setVisible(true);
        ui->ouncesSpinBox->setSuffix(STR_UNIT_OUNCE);
    } else {
        ui->ouncesSpinBox->setVisible(false);
        ui->weightSpinBox->setDecimals(1);
        ui->weightSpinBox->setSuffix(STR_UNIT_KG);
    }

    GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));
    GraphView->setEmptyText(STR_Empty_NoData);
    previous_date=QDate();

    ui->calButton->setChecked(AppSetting->calendarVisible() ? true : false);
    on_calButton_toggled(AppSetting->calendarVisible());

    GraphView->resetLayout();
    GraphView->SaveDefaultSettings();
    GraphView->LoadSettings("Daily");

    connect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    connect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    connect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));
//    qDebug() << "Finished making new Daily object";
//    sleep(3);
}

Daily::~Daily()
{
    disconnect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    disconnect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    disconnect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));

    disconnect(sessionbar, SIGNAL(sessionClicked(Session*)), this, SLOT(doToggleSession(Session*)));
    disconnect(webView,SIGNAL(anchorClicked(QUrl)),this,SLOT(Link_clicked(QUrl)));

    if (previous_date.isValid())
        Unload(previous_date);

    // Save graph orders and pin status, etc...
    GraphView->SaveSettings("Daily");

    delete ui;
    delete icon_on;
    delete icon_off;
}

void Daily::showEvent(QShowEvent *)
{
//    qDebug() << "Start showEvent in Daily object";
//    sleep(3);
    RedrawGraphs();
//    qDebug() << "Finished showEvent  Daily object";
//    sleep(3);
}

void Daily::doToggleSession(Session * sess)
{
    sess->setEnabled(!sess->enabled());

    LoadDate(previous_date);
    mainwin->getOverview()->graphView()->dataChanged();
}

void Daily::Link_clicked(const QUrl &url)
{
    QString code=url.toString().section("=",0,0).toLower();
    QString data=url.toString().section("=",1);
    int sid=data.toInt();
    Day *day=nullptr;

    if (code=="togglecpapsession") { // Enable/Disable CPAP session
        day=p_profile->GetDay(previous_date,MT_CPAP);
        if (!day) return;
        Session *sess=day->find(sid);
        if (!sess)
            return;
//        int i=webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-webView->page()->mainFrame()->scrollBarValue(Qt::Vertical);
        sess->setEnabled(!sess->enabled());

        // Reload day
        LoadDate(previous_date);
  //      webView->page()->mainFrame()->setScrollBarValue(Qt::Vertical, webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-i);
    } else  if (code=="toggleoxisession") { // Enable/Disable Oximetry session
        day=p_profile->GetDay(previous_date,MT_OXIMETER);
        Session *sess=day->find(sid);
        if (!sess)
            return;
//        int i=webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-webView->page()->mainFrame()->scrollBarValue(Qt::Vertical);
        sess->setEnabled(!sess->enabled());

        // Reload day
        LoadDate(previous_date);
  //      webView->page()->mainFrame()->setScrollBarValue(Qt::Vertical, webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-i);
    } else if (code=="cpap")  {
        day=p_profile->GetDay(previous_date,MT_CPAP);
        if (day) {
            Session *sess=day->machine(MT_CPAP)->sessionlist[sid];
            if (sess && sess->enabled()) {
                GraphView->SetXBounds(sess->first(),sess->last());
            }
        }
    } else if (code=="oxi") {
        day=p_profile->GetDay(previous_date,MT_OXIMETER);
        if (day) {
            Session *sess=day->machine(MT_OXIMETER)->sessionlist[sid];
            if (sess && sess->enabled()) {
                GraphView->SetXBounds(sess->first(),sess->last());
            }
        }

    } else if (code=="event")  {
        QList<QTreeWidgetItem *> list=ui->treeWidget->findItems(schema::channel[sid].fullname(),Qt::MatchContains);
        if (list.size()>0) {
            ui->treeWidget->collapseAll();
            ui->treeWidget->expandItem(list.at(0));
            QTreeWidgetItem *wi=list.at(0)->child(0);
            ui->treeWidget->setCurrentItem(wi);
            ui->tabWidget->setCurrentIndex(1);
        } else {
            mainwin->Notify(tr("No %1 events are recorded this day").arg(schema::channel[sid].fullname()),"",1500);
        }
    } else if (code=="graph") {
        qDebug() << "Select graph " << data;
    } else {
        qDebug() << "Clicked on" << code << data;
    }
}

void Daily::ReloadGraphs()
{
//    qDebug() << "Start ReloadGraphs  Daily object";
//    sleep(3);
    GraphView->setDay(nullptr);

    ui->splitter->setVisible(true);
    QDate d;

    if (previous_date.isValid()) {
        d=previous_date;
        //Unload(d);
    }
    QDate lastcpap = p_profile->LastDay(MT_CPAP);
    QDate lastoxi = p_profile->LastDay(MT_OXIMETER);

    d = qMax(lastcpap, lastoxi);

    if (!d.isValid()) {
        d=ui->calendar->selectedDate();
    }
    on_calendar_currentPageChanged(d.year(),d.month());
    // this fires a signal which unloads the old and loads the new
    ui->calendar->blockSignals(true);
    ui->calendar->setSelectedDate(d);
    ui->calendar->blockSignals(false);
    Load(d);
    ui->calButton->setText(ui->calendar->selectedDate().toString(MedDateFormat));
    graphView()->redraw();
//    qDebug() << "Finished ReloadGraphs in Daily object";
//    sleep(3);
}

void Daily::updateLeftSidebar() {
    if (webView && !htmlLeftHeader.isEmpty())
        webView->setHtml(getLeftSidebar(true));
}

void Daily::hideSpaceHogs()
{
    if (AppSetting->calendarVisible()) {
        ui->calendarFrame->setVisible(false);
    }
    if (AppSetting->showPieChart()) {
        webView->setHtml(getLeftSidebar(false));
    }
}
void Daily::showSpaceHogs()
{
    if (AppSetting->calendarVisible()) {
        ui->calendarFrame->setVisible(true);
    }
    if (AppSetting->showPieChart()) {
        webView->setHtml(getLeftSidebar(true));
    }
}

void Daily::on_calendar_currentPageChanged(int year, int month)
{
    QDate d(year,month,1);
    int dom=d.daysInMonth();

    for (int i=1;i<=dom;i++) {
        d=QDate(year,month,i);
        this->UpdateCalendarDay(d);
    }
}

void Daily::UpdateEventsTree(QTreeWidget *tree,Day *day)
{
    tree->clear();
    if (!day) return;

    tree->setColumnCount(1); // 1 visible common.. (1 hidden)

    QTreeWidgetItem *root=nullptr;
    QHash<ChannelID,QTreeWidgetItem *> mcroot;
    QHash<ChannelID,int> mccnt;
    int total_events=0;

    qint64 drift=0, clockdrift=p_profile->cpap->clockDrift()*1000L;

    quint32 chantype = schema::FLAG | schema::SPAN | schema::MINOR_FLAG;
    if (p_profile->general->showUnknownFlags()) chantype |= schema::UNKNOWN;
    QList<ChannelID> chans = day->getSortedMachineChannels(chantype);

    // Go through all the enabled sessions of the day
    for (QList<Session *>::iterator s=day->begin();s!=day->end();++s) {
        Session * sess = *s;
        if (!sess->enabled()) continue;

        // For each session, go through all the channels
        QHash<ChannelID,QVector<EventList *> >::iterator m;
        for (int c=0; c < chans.size(); ++c) {
            ChannelID code = chans.at(c);
            m = sess->eventlist.find(code);
            if (m == sess->eventlist.end()) continue;

            drift=(sess->type() == MT_CPAP) ? clockdrift : 0;

            // Prepare title for this code, if there are any events
            QTreeWidgetItem *mcr;
            if (mcroot.find(code)==mcroot.end()) {
                int cnt=day->count(code);
                if (!cnt) continue; // If no events than don't bother showing..
                total_events+=cnt;
                QString st=schema::channel[code].fullname();
                if (st.isEmpty())  {
                    st=QString("Fixme %1").arg(code);
                }
                st+=" ";
                if (cnt==1) st+=tr("%1 event").arg(cnt);
                else st+=tr("%1 events").arg(cnt);

                QStringList l(st);
                l.append("");
                mcroot[code]=mcr=new QTreeWidgetItem(root,l);
                mccnt[code]=0;
            } else {
                mcr=mcroot[code];
            }

            // number of digits required for count depends on total for day
            int numDigits = ceil(log10(day->count(code)+1));

            // Now we go through the event list for the *session* (not for the day)
            for (int z=0;z<m.value().size();z++) {
                EventList & ev=*(m.value()[z]);

                for (quint32 o=0;o<ev.count();o++) {
                    qint64 t=ev.time(o)+drift;

                    if ((code == CPAP_CSR) || (code == CPAP_PB)) { // center it in the middle of span
                        t -= float(ev.raw(o) / 2.0) * 1000.0;
                    }
                    QStringList a;
                    QDateTime d=QDateTime::fromMSecsSinceEpoch(t); // Localtime
                    QString s=QString("#%1: %2").arg((int)(++mccnt[code]),(int)numDigits,(int)10,QChar('0')).arg(d.toString("HH:mm:ss"));
                    if (m.value()[z]->raw(o) > 0)
                            s += QString(" (%3)").arg(m.value()[z]->raw(o));

                    a.append(s);
                    QTreeWidgetItem *item=new QTreeWidgetItem(a);
                    item->setData(0,Qt::UserRole,t);
                    mcr->addChild(item);
                }
            }
        }
    }
    int cnt=0;
    for (QHash<ChannelID,QTreeWidgetItem *>::iterator m=mcroot.begin();m!=mcroot.end();m++) {
        tree->insertTopLevelItem(cnt++,m.value());
    }

    if (day->hasMachine(MT_CPAP) || day->hasMachine(MT_OXIMETER) || day->hasMachine(MT_POSITION)) {
        QTreeWidgetItem * start = new QTreeWidgetItem(QStringList(tr("Session Start Times")));
        QTreeWidgetItem * end = new QTreeWidgetItem(QStringList(tr("Session End Times")));
        tree->insertTopLevelItem(cnt++ , start);
        tree->insertTopLevelItem(cnt++ , end);
        for (QList<Session *>::iterator s=day->begin(); s!=day->end(); ++s) {
            QDateTime st = QDateTime::fromMSecsSinceEpoch((*s)->first()); // Localtime
            QDateTime et = QDateTime::fromMSecsSinceEpoch((*s)->last()); // Localtime

            QTreeWidgetItem * item = new QTreeWidgetItem(QStringList(st.toString("HH:mm:ss")));
            item->setData(0,Qt::UserRole, (*s)->first());
            start->addChild(item);


            item = new QTreeWidgetItem(QStringList(et.toString("HH:mm:ss")));
            item->setData(0,Qt::UserRole, (*s)->last());
            end->addChild(item);
        }
    }
    //tree->insertTopLevelItem(cnt++,new QTreeWidgetItem(QStringList("[Total Events ("+QString::number(total_events)+")]")));
    tree->sortByColumn(0,Qt::AscendingOrder);
    //tree->expandAll();
}

void Daily::UpdateCalendarDay(QDate date)
{
    QTextCharFormat nodata;
    QTextCharFormat cpaponly;
    QTextCharFormat cpapjour;
    QTextCharFormat oxiday;
    QTextCharFormat oxicpap;
    QTextCharFormat jourday;
    QTextCharFormat stageday;

    cpaponly.setForeground(QBrush(COLOR_Blue, Qt::SolidPattern));
    cpaponly.setFontWeight(QFont::Normal);
    cpapjour.setForeground(QBrush(COLOR_Blue, Qt::SolidPattern));
    cpapjour.setFontWeight(QFont::Bold);
//    cpapjour.setFontUnderline(true);
    oxiday.setForeground(QBrush(COLOR_Red, Qt::SolidPattern));
    oxiday.setFontWeight(QFont::Normal);
    oxicpap.setForeground(QBrush(COLOR_Red, Qt::SolidPattern));
    oxicpap.setFontWeight(QFont::Bold);
    stageday.setForeground(QBrush(COLOR_Magenta, Qt::SolidPattern));
    stageday.setFontWeight(QFont::Bold);
    jourday.setForeground(QBrush(COLOR_DarkYellow, Qt::SolidPattern));
    jourday.setFontWeight(QFont::Bold);
    nodata.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    nodata.setFontWeight(QFont::Normal);

    bool hascpap = p_profile->FindDay(date, MT_CPAP)!=nullptr;
    bool hasoxi = p_profile->FindDay(date, MT_OXIMETER)!=nullptr;
    bool hasjournal = p_profile->FindDay(date, MT_JOURNAL)!=nullptr;
    bool hasstage = p_profile->FindDay(date, MT_SLEEPSTAGE)!=nullptr;
    bool haspos = p_profile->FindDay(date, MT_POSITION)!=nullptr;
    if (hascpap) {
        if (hasoxi) {
            ui->calendar->setDateTextFormat(date, oxicpap);
        } else if (hasjournal) {
            ui->calendar->setDateTextFormat(date, cpapjour);
        } else if (hasstage || haspos) {
            ui->calendar->setDateTextFormat(date, stageday);
        } else {
            ui->calendar->setDateTextFormat(date, cpaponly);
        }
    } else if (hasoxi) {
        ui->calendar->setDateTextFormat(date, oxiday);
    } else if (hasjournal) {
        ui->calendar->setDateTextFormat(date, jourday);
    } else if (hasstage) {
        ui->calendar->setDateTextFormat(date, oxiday);
    } else if (haspos) {
        ui->calendar->setDateTextFormat(date, oxiday);
    } else {
        ui->calendar->setDateTextFormat(date, nodata);
    }
//    if (hasjournal) {
//        ui->calendar->setDateTextFormat(date, cpapjour);
//    }
    ui->calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
}
void Daily::LoadDate(QDate date)
{
    if (!date.isValid()) {
        qDebug() << "LoadDate called with invalid date";
        return;
    }
    ui->calendar->blockSignals(true);
    if (date.month()!=previous_date.month()) {
        on_calendar_currentPageChanged(date.year(),date.month());
    }
    ui->calendar->setSelectedDate(date);
    ui->calendar->blockSignals(false);
    on_calendar_selectionChanged();
}
void Daily::on_calendar_selectionChanged()
{
    QTimer::singleShot(0, this, SLOT(on_ReloadDay()));
}

void Daily::on_ReloadDay()
{
    static volatile bool inReload = false;

    if (inReload) {
        qDebug() << "attempt to renter on_ReloadDay()";
    }
    inReload = true;
    graphView()->releaseKeyboard();
    QTime time;
    time_t unload_time, load_time, other_time;
    time.start();

    this->setCursor(Qt::BusyCursor);
    if (previous_date.isValid()) {
       // GraphView->fadeOut();
        Unload(previous_date);
    }
    unload_time=time.restart();
    //bool fadedir=previous_date < ui->calendar->selectedDate();
    ZombieMeterMoved=false;
    Load(ui->calendar->selectedDate());
    load_time=time.restart();

    //GraphView->fadeIn(fadedir);
    GraphView->redraw();
    ui->calButton->setText(ui->calendar->selectedDate().toString(MedDateFormat));
    ui->calendar->setFocus(Qt::ActiveWindowFocusReason);

    if (p_profile->general->unitSystem()==US_English) {
        ui->weightSpinBox->setSuffix(STR_UNIT_POUND);
        ui->weightSpinBox->setDecimals(0);
        ui->ouncesSpinBox->setVisible(true);
        ui->ouncesSpinBox->setSuffix(STR_UNIT_OUNCE);
    } else {
        ui->ouncesSpinBox->setVisible(false);
        ui->weightSpinBox->setDecimals(1);
        ui->weightSpinBox->setSuffix(STR_UNIT_KG);
    }
    this->setCursor(Qt::ArrowCursor);
    other_time=time.restart();

    qDebug() << "Page change time (in ms): Unload ="<<unload_time<<"Load =" << load_time << "Other =" << other_time;
    inReload = false;
}
void Daily::ResetGraphLayout()
{
    GraphView->resetLayout();
}
void Daily::ResetGraphOrder()
{
    Day * day = p_profile->GetDay(previous_date,MT_CPAP);

    int cpapMode = day->getCPAPMode();
//    qDebug() << "Daily::ResetGraphOrder cpapMode" << cpapMode;

    if (useAdvancedGraphs.contains(cpapMode))
        GraphView->resetGraphOrder(true, advancedGraphOrder);
    else
        GraphView->resetGraphOrder(true, standardGraphOrder);

    // Enable all graphs (make them not hidden)
    for (int i=0;i<ui->graphCombo->count();i++) {
        // If disabled, emulate a click to enable the graph
        if (!ui->graphCombo->itemData(i,Qt::UserRole).toBool()) {
//            qDebug() << "resetting graph" << i;
            Daily::on_graphCombo_activated(i);
        }
    }

    // Mark all events as active
    for (int i=0;i<ui->eventsCombo->count();i++) {
        // If disabled, emulate a click to enable the event
        ChannelID code = ui->eventsCombo->itemData(i, Qt::UserRole).toUInt();
        schema::Channel * chan = &schema::channel[code];
        if (!chan->enabled()) {
//            qDebug() << "resetting event" << i;
            Daily::on_eventsCombo_activated(i);
        }
    }

    // Reset graph heights (and repaint)
    ResetGraphLayout();
}

void Daily::graphtogglebutton_toggled(bool b)
{
    Q_UNUSED(b)
    for (int i=0;i<GraphView->size();i++) {
        QString title=(*GraphView)[i]->title();
        (*GraphView)[i]->setVisible(GraphToggles[title]->isChecked());
    }
    GraphView->updateScale();
    GraphView->redraw();
}

QString Daily::getSessionInformation(Day * day)
{
    QString html;
    if (!day) return html;

    html="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
    html+=QString("<tr><td colspan=5 align=center><b>"+tr("Session Information")+"</b></td></tr>");
    html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
    QFontMetrics FM(*defaultfont);
//    QRect r=FM.boundingRect('@');

 //   Machine * cpap = day->machine(MT_CPAP);

    QDateTime fd,ld;
    bool corrupted_waveform=false;

    QString type;

    QHash<MachineType, Machine *>::iterator mach_end = day->machines.end();
    QHash<MachineType, Machine *>::iterator mi;

    for (mi = day->machines.begin(); mi != mach_end; ++mi) {
        if (mi.key() == MT_JOURNAL) continue;
        html += "<tr><td colspan=5 align=center><i>";
        switch (mi.key()) {
            case MT_CPAP: type="cpap";
                html+=tr("CPAP Sessions");
                break;
            case MT_OXIMETER: type="oxi";
                html+=tr("Oximetry Sessions");
                break;
            case MT_SLEEPSTAGE: type="stage";
                html+=tr("Sleep Stage Sessions");
                break;
            case MT_POSITION: type="stage";
                html+=tr("Position Sensor Sessions");
                break;

            default:
                type="unknown";
                html+=tr("Unknown Session");
                break;
        }
        html+="</i></td></tr>\n";
        html+=QString("<tr>"
                      "<td>"+STR_TR_On+"</td>"
                      "<td>"+STR_TR_Date+"</td>"
                      "<td>"+STR_TR_Start+"</td>"
                      "<td>"+STR_TR_End+"</td>"
                      "<td>"+tr("Duration")+"</td></tr>");

        QList<Session *> sesslist = day->getSessions(mi.key(), true);

        for (QList<Session *>::iterator s=sesslist.begin(); s != sesslist.end(); ++s) {
            if (((*s)->type() == MT_CPAP) &&
                ((*s)->settings.find(CPAP_BrokenWaveform) != (*s)->settings.end()))
                    corrupted_waveform=true;

            fd=QDateTime::fromTime_t((*s)->first()/1000L);
            ld=QDateTime::fromTime_t((*s)->last()/1000L);
            int len=(*s)->length()/1000L;
            int h=len/3600;
            int m=(len/60) % 60;
            int s1=len % 60;

            Session *sess=*s;

            QString tooltip = tr("Click to %1 this session.").arg(sess->enabled() ? tr("disable") : tr("enable"));
            html+=QString("<tr><td colspan=5 align=center>%2</td></tr>"
                          "<tr>"
                          "<td width=26>"
#ifdef DITCH_ICONS
                          "[<a href='toggle"+type+"session=%1'><b title='"+tooltip+"'>%4</b></a>]"
#else
                          "<a href='toggle"+type+"session=%1'><img src='qrc:/icons/session-%4.png' title=\""+tooltip+"\"></a>"
#endif
                          "</td>"
                          "<td align=left>%5</td>"
                          "<td align=left>%6</td>"
                          "<td align=left>%7</td>"
                          "<td align=left>%3</td></tr>"
                          )
                    .arg((*s)->session())
                    .arg(tr("%1 Session #%2").arg((*s)->machine()->loaderName()).arg((*s)->session(),8,10,QChar('0')))
                    .arg(tr("%1h %2m %3s").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s1,2,10,QChar('0')))
                    .arg((sess->enabled() ? "on" : "off"))
                    .arg(fd.date().toString(Qt::SystemLocaleShortDate))
                    .arg(fd.toString("HH:mm:ss"))
                    .arg(ld.toString("HH:mm:ss"));
#ifdef SESSION_DEBUG
            for (int i=0; i< sess->session_files.size(); ++i) {
                html+=QString("<tr><td colspan=5 align=center>%1</td></tr>").arg(sess->session_files[i].section("/",-1));
            }
#endif
        }
    }

    if (corrupted_waveform) {
        html+=QString("<tr><td colspan=5 align=center><i>%1</i></td></tr>").arg(tr("One or more waveform record(s) for this session had faulty source data. Some waveform overlay points may not match up correctly."));
    }
    html+="</table>";
    return html;
}

QString Daily::getMachineSettings(Day * day) {
    QString html;

    Machine * cpap = day->machine(MT_CPAP);
    if (cpap && day->hasEnabledSessions(MT_CPAP)) {
        html="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
        html+=QString("<tr><td colspan=5 align=center><b>%1</b></td></tr>").arg(tr("Machine Settings"));
        html+="<tr><td colspan=5>&nbsp;</td></tr>";

        if (day->noSettings(cpap)) {
            html+="<tr><td colspan=5 align=center><i>"+tr("<b>Please Note:</b> All settings shown below are based on assumptions that nothing has changed since previous days.")+"</i></td></tr>\n";
        } else if ((day->settingExists(CPAP_BrokenSummary))) {
            html+="<tr><td colspan=5 align=center><i>"+tr("Machine Settings Unavailable")+"</i></td></tr></table><hr/>\n";
            return html;
        }

        QMap<QString, QString> other;
        Session * sess = day->firstSession(MT_CPAP);

        QHash<ChannelID, QVariant>::iterator it;
        QHash<ChannelID, QVariant>::iterator it_end;
        if (sess) {
            it_end = sess->settings.end();
            it = sess->settings.begin();
        }
        QMap<int, QString> first;

        CPAPLoader * loader = qobject_cast<CPAPLoader *>(cpap->loader());

        ChannelID cpapmode = loader->CPAPModeChannel();
        schema::Channel & chan = schema::channel[cpapmode];
        first[cpapmode] = QString("<tr><td><p title='%2'>%1</p></td><td colspan=4>%3</td></tr>")
                .arg(chan.label())
                .arg(chan.description())
                .arg(day->getCPAPModeStr());

        if (sess) for (; it != it_end; ++it) {
            ChannelID code = it.key();

            if ((code <= 1) || (code == RMS9_MaskOnTime) || (code == CPAP_Mode) || (code == cpapmode) || (code == CPAP_SummaryOnly)) continue;

            schema::Channel & chan = schema::channel[code];

            QString data;

            if (chan.datatype() == schema::LOOKUP) {
                data = chan.option(it.value().toInt());
            } else if (chan.datatype() == schema::BOOL) {
                data = (it.value().toBool() ? STR_TR_Yes : STR_TR_No);
            } else if (chan.datatype() == schema::DOUBLE) {
                data = QString().number(it.value().toDouble(),'f',2) + " "+chan.units();
            } else if (chan.datatype() == schema::DEFAULT) {
                data = QString().number(it.value().toDouble(),'f',2) + " "+chan.units();
            } else {

                data = it.value().toString() + " "+ chan.units();
            }
            if (code ==0xe202)      // Format EPR relief correctly
                data = formatRelief(data);

            QString tmp = QString("<tr><td><p title='%2'>%1</p></td><td colspan=4>%3</td></tr>")
                    .arg(schema::channel[code].label())
                    .arg(schema::channel[code].description())
                    .arg(data);

            if ((code == CPAP_IPAP)
            || (code == CPAP_EPAP)
            || (code == CPAP_IPAPHi)
            || (code == CPAP_EPAPHi)
            || (code == CPAP_IPAPLo)
            || (code == CPAP_EPAPLo)
            || (code == CPAP_PressureMin)
            || (code == CPAP_PressureMax)
            || (code == CPAP_Pressure)
            || (code == CPAP_PSMin)
            || (code == CPAP_PSMax)
            || (code == CPAP_PS)) {
                first[code] = tmp;
            } else {
                other[schema::channel[code].label()] = tmp;
            }
        }

        ChannelID order[] = { cpapmode, CPAP_Pressure, CPAP_PressureMin, CPAP_PressureMax, CPAP_EPAP, CPAP_EPAPLo, CPAP_EPAPHi, CPAP_IPAP, CPAP_IPAPLo, CPAP_IPAPHi, CPAP_PS, CPAP_PSMin, CPAP_PSMax };
        int os = sizeof(order) / sizeof(ChannelID);
        for (int i=0 ;i < os; ++i) {
            if (first.contains(order[i])) html += first[order[i]];
        }

        for (QMap<QString,QString>::iterator it = other.begin(); it != other.end(); ++it) {
            html += it.value();
        }

  /*      ChannelID pr_level_chan = NoChannel;
        ChannelID pr_mode_chan = NoChannel;
        ChannelID hum_stat_chan = NoChannel;
        ChannelID hum_level_chan = NoChannel;
        CPAPLoader * loader = dynamic_cast<CPAPLoader *>(cpap->machine->loader());
        if (loader) {
            pr_level_chan = loader->PresReliefLevel();
            pr_mode_chan = loader->PresReliefMode();
            hum_stat_chan = loader->HumidifierConnected();
            hum_level_chan = loader->HumidifierLevel();
        }

        if ((pr_level_chan != NoChannel) && (cpap->settingExists(pr_level_chan))) {
            QString flexstr = cpap->getPressureRelief();

            html+=QString("<tr><td><a class='info' href='#'>%1<span>%2</span></a></td><td colspan=4>%3</td></tr>")
                    .arg(schema::channel[pr_mode_chan].label())
                    .arg(schema::channel[pr_mode_chan].description())
                    .arg(flexstr);
        }


        if (cpap->settingExists(hum_level_chan)) {
            int humid=round(cpap->settings_wavg(hum_level_chan));
            html+=QString("<tr><td><a class='info' href='#'>"+schema::channel[hum_level_chan].label()+"<span>%1</span></a></td><td colspan=4>%2</td></tr>")
                .arg(schema::channel[hum_level_chan].description())
                .arg(humid == 0 ? STR_GEN_Off : "x"+QString::number(humid));
        } */
        html+="</table>";
        html+="<hr/>\n";
    }
    return html;
}

QString Daily::getOximeterInformation(Day * day)
{
    QString html;
    Machine * oxi = day->machine(MT_OXIMETER);
    if (oxi && day->hasEnabledSessions(MT_OXIMETER)) {
        html="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
        html+=QString("<tr><td colspan=5 align=center><b>%1</b></td></tr>\n").arg(tr("Oximeter Information"));
        html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
        html+="<tr><td colspan=5 align=center>"+oxi->brand()+" "+oxi->model()+"</td></tr>\n";
        html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
        html+=QString("<tr><td colspan=5 align=center>%1: %2 (%3%)</td></tr>").arg(tr("SpO2 Desaturations")).arg(day->count(OXI_SPO2Drop)).arg((100.0/day->hours(MT_OXIMETER)) * (day->sum(OXI_SPO2Drop)/3600.0),0,'f',2);
        html+=QString("<tr><td colspan=5 align=center>%1: %2 (%3%)</td></tr>").arg(tr("Pulse Change events")).arg(day->count(OXI_PulseChange)).arg((100.0/day->hours(MT_OXIMETER)) * (day->sum(OXI_PulseChange)/3600.0),0,'f',2);
        html+=QString("<tr><td colspan=5 align=center>%1: %2%</td></tr>").arg(tr("SpO2 Baseline Used")).arg(day->settings_wavg(OXI_SPO2Drop),0,'f',2); // CHECKME: Should this value be wavg OXI_SPO2 isntead?
        html+="</table>\n";
        html+="<hr/>\n";
    }
    return html;
}

QString Daily::getCPAPInformation(Day * day)
{
    QString html;
    if (!day)
        return html;

    Machine * cpap = day->machine(MT_CPAP);
    if (!cpap) return html;

    MachineInfo info = cpap->getInfo();

    html="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

    QString tooltip=tr("Model %1 - %2").arg(info.modelnumber).arg(info.serial);
    tooltip=tooltip.replace(" ","&nbsp;");
    html+="<tr><td align=center><p title=\""+tooltip+"\">"+info.brand+"<br/>"+info.model+"</p></td></tr>\n";
    html+="<tr><td align=center>";

    html+=tr("PAP Mode: %1").arg(day->getCPAPModeStr())+"<br/>";
    html+= day->getPressureSettings();
    html+="</td></tr>\n";
    if (day->noSettings(cpap)) {
        html+=QString("<tr><td colspan=2 align=center><i>%1</i></td></tr>").arg(tr("(Mode/Pressure settings are guessed on this day.)"));
    }

    html+="</table>\n";
    html+="<hr/>\n";
    return html;
}


QString Daily::getStatisticsInfo(Day * day)
{
    if (day == nullptr) return QString();

    Machine *cpap = day->machine(MT_CPAP);
//            *oxi = day->machine(MT_OXIMETER),
//            *pos = day->machine(MT_POSITION);

    int mididx=p_profile->general->prefCalcMiddle();
    SummaryType ST_mid = ST_AVG;

    if (mididx==0) ST_mid=ST_PERC;
    if (mididx==1) ST_mid=ST_WAVG;
    if (mididx==2) ST_mid=ST_AVG;

    float percentile=p_profile->general->prefCalcPercentile()/100.0;

    SummaryType ST_max=p_profile->general->prefCalcMax() ? ST_PERC : ST_MAX;
    const EventDataType maxperc=0.995F;

    QString midname;
    if (ST_mid==ST_WAVG) midname=STR_TR_WAvg;
    else if (ST_mid==ST_AVG) midname=STR_TR_Avg;
    else if (ST_mid==ST_PERC) midname=STR_TR_Med;

    QString html;

    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
    html+=QString("<tr><td colspan=5 align=center><b>%1</b></td></tr>\n").arg(tr("Statistics"));
    html+=QString("<tr><td><b>%1</b></td><td><b>%2</b></td><td><b>%3</b></td><td><b>%4</b></td><td><b>%5</b></td></tr>")
            .arg(STR_TR_Channel)
            .arg(STR_TR_Min)
            .arg(midname)
            .arg(tr("%1%2").arg(percentile*100,0,'f',0).arg(STR_UNIT_Percentage))
            .arg(STR_TR_Max);

    ChannelID chans[]={
        CPAP_Pressure,CPAP_EPAP,CPAP_IPAP,CPAP_PS,CPAP_PTB,
        CPAP_MinuteVent, CPAP_RespRate, CPAP_RespEvent,CPAP_FLG,
        CPAP_Leak, CPAP_LeakTotal, CPAP_Snore,  /*  CPAP_IE,   */  CPAP_Ti,CPAP_Te, CPAP_TgMV,
        CPAP_TidalVolume, OXI_Pulse, OXI_SPO2, POS_Inclination, POS_Orientation
    };
    int numchans=sizeof(chans)/sizeof(ChannelID);
    int ccnt=0;
    EventDataType tmp,med,perc,mx,mn;

    for (int i=0;i<numchans;i++) {

        ChannelID code=chans[i];

        if (!day->channelHasData(code))
            continue;

        QString tooltip=schema::channel[code].description();

        if (!schema::channel[code].units().isEmpty()) tooltip+=" ("+schema::channel[code].units()+")";
//        if (!schema::channel[code].units().isEmpty() && schema::channel[code].units() != "0")
//            tooltip+=" ("+schema::channel[code].units()+")";

        if (ST_max == ST_MAX) {
            mx=day->Max(code);
        } else {
            mx=day->percentile(code,maxperc);
        }

        mn=day->Min(code);
        perc=day->percentile(code,percentile);

        if (ST_mid == ST_PERC) {
            med=day->percentile(code,0.5);
            tmp=day->wavg(code);
            if (tmp>0 || mx==0) {
                tooltip+=QString("<br/>"+STR_TR_WAvg+": %1").arg(tmp,0,'f',2);
            }
        } else if (ST_mid == ST_WAVG) {
            med=day->wavg(code);
            tmp=day->percentile(code,0.5);
            if (tmp>0 || mx==0) {
                tooltip+=QString("<br/>"+STR_TR_Median+": %1").arg(tmp,0,'f',2);
            }
        } else if (ST_mid == ST_AVG) {
            med=day->avg(code);
            tmp=day->percentile(code,0.5);
            if (tmp>0 || mx==0) {
                tooltip+=QString("<br/>"+STR_TR_Median+": %1").arg(tmp,0,'f',2);
            }
        }

        html+=QString("<tr><td align=left title='%6'>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
            .arg(schema::channel[code].label())
            .arg(mn,0,'f',2)
            .arg(med,0,'f',2)
            .arg(perc,0,'f',2)
            .arg(mx,0,'f',2)
            .arg(tooltip);
        ccnt++;

    }

    if (GraphView->isEmpty() && ((ccnt>0) || (cpap && day->summaryOnly()))) {
        html+="<tr><td colspan=5>&nbsp;</td></tr>\n";
        html+=QString("<tr><td colspan=5 align=center><i>%1</i></td></tr>").arg("<b>"+STR_MessageBox_PleaseNote+"</b> "+ 
                    tr("This day just contains summary data, only limited information is available."));
    } else if (cpap) {
        html+="<tr><td colspan=5>&nbsp;</td></tr>";

        if ((cpap->loaderName() == STR_MACH_ResMed) || ((cpap->loaderName() == STR_MACH_PRS1) && (p_profile->cpap->resyncFromUserFlagging()))) {
            int ttia = day->sum(CPAP_Obstructive) + day->sum(CPAP_ClearAirway) + day->sum(CPAP_Apnea) + day->sum(CPAP_Hypopnea);
            int h = ttia / 3600;
            int m = ttia / 60 % 60;
            int s = ttia % 60;
            if (ttia > 0) {
                html+="<tr><td colspan=3 align='left' bgcolor='white'><b>"+tr("Total time in apnea") +
                       QString("</b></td><td colspan=2 bgcolor='white'>%1</td></tr>").arg(QString().sprintf("%02i:%02i:%02i",h,m,s));
            }

        }
        float hours = day->hours(MT_CPAP);

        if (p_profile->cpap->showLeakRedline()) {
            float rlt = day->timeAboveThreshold(CPAP_Leak, p_profile->cpap->leakRedline()) / 60.0;
            float pc = 100.0 / hours * rlt;
            html+="<tr><td colspan=3 align='left' bgcolor='white'><b>"+tr("Time over leak redline")+
                    QString("</b></td><td colspan=2 bgcolor='white'>%1%</td></tr>").arg(pc, 0, 'f', 3);
        }
        int l = day->sum(CPAP_Ramp);

        if (l > 0) {
            html+="<tr><td colspan=3 align='left' bgcolor='white'>"+tr("Total ramp time")+
                    QString("</td><td colspan=2 bgcolor='white'>%1:%2:%3</td></tr>").arg(l / 3600, 2, 10, QChar('0')).arg((l / 60) % 60, 2, 10, QChar('0')).arg(l % 60, 2, 10, QChar('0'));
            float v = (hours - (float(l) / 3600.0));
            int q = v * 3600.0;
            html+="<tr><td colspan=3 align='left' bgcolor='white'>"+tr("Time outside of ramp")+
                    QString("</td><td colspan=2 bgcolor='white'>%1:%2:%3</td></tr>").arg(q / 3600, 2, 10, QChar('0')).arg((q / 60) % 60, 2, 10, QChar('0')).arg(q % 60, 2, 10, QChar('0'));

//            EventDataType hc = day->count(CPAP_Hypopnea) - day->countInsideSpan(CPAP_Ramp, CPAP_Hypopnea);
//            EventDataType oc = day->count(CPAP_Obstructive) - day->countInsideSpan(CPAP_Ramp, CPAP_Obstructive);

            //EventDataType tc = day->count(CPAP_Hypopnea) + day->count(CPAP_Obstructive);
//            EventDataType ahi = (hc+oc) / v;
            // Not sure if i was trying to be funny, and left on my replication of Devilbiss's bug here...  :P

//            html+="<tr><td colspan=3 align='left' bgcolor='white'>"+tr("AHI excluding ramp")+
//                    QString("</td><td colspan=2 bgcolor='white'>%1</td></tr>").arg(ahi, 0, 'f', 2);
        }

    }


    html+="</table>\n";
    html+="<hr/>\n";
    return html;
}

QString Daily::getEventBreakdown(Day * cpap)
{
    Q_UNUSED(cpap)
    QString html;
    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

    html+="</table>";
    return html;
}

QString Daily::getSleepTime(Day * day)
{
    //cpap, Day * oxi
    QString html;

    if (!day || (day->hours() < 0.0000001))
        return html;

    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
    html+="<tr><td align='center'><b>"+STR_TR_Date+"</b></td><td align='center'><b>"+tr("Start")+"</b></td><td align='center'><b>"+tr("End")+"</b></td><td align='center'><b>"+STR_UNIT_Hours+"</b></td></tr>";
    int tt=qint64(day->total_time())/1000L;
    QDateTime date=QDateTime::fromTime_t(day->first()/1000L);
    QDateTime date2=QDateTime::fromTime_t(day->last()/1000L);

    int h=tt/3600;
    int m=(tt/60)%60;
    int s=tt % 60;
    html+=QString("<tr><td align='center'>%1</td><td align='center'>%2</td><td align='center'>%3</td><td align='center'>%4</td></tr>\n")
            .arg(date.date().toString(Qt::SystemLocaleShortDate))
            .arg(date.toString("HH:mm:ss"))
            .arg(date2.toString("HH:mm:ss"))
            .arg(QString().sprintf("%02i:%02i:%02i",h,m,s));
    html+="</table>\n";
//    html+="<hr/>";

    return html;
}

QString Daily::getPieChart (float values, Day * day) {
//    qDebug() << "Daily:getPieChart, values" << values;
    QString html = "<table cellspacing=0 cellpadding=0 border=0 width='100%'>";
    if (values > 0) {
//        html += "<tr><td align=center>&nbsp;</td></tr>";
        html += QString("<tr><td align=center><b>%1</b></td></tr>").arg(tr("Event Breakdown"));
        eventBreakdownPie()->setShowTitle(false);

        int w=155;
        int h=155;
        QPixmap pixmap=eventBreakdownPie()->renderPixmap(w,h,false);
        if (!pixmap.isNull()) {
            QByteArray byteArray;
            QBuffer buffer(&byteArray); // use buffer to store pixmap into byteArray
            buffer.open(QIODevice::WriteOnly);
            pixmap.save(&buffer, "PNG");
            html += "<tr><td align=center><img src=\"data:image/png;base64," + byteArray.toBase64() + "\"></td></tr>\n";
        } else {
            html += "<tr><td align=center>"+tr("Unable to display Pie Chart on this system")+"</td></tr>\n";
        }
    } else if (   day->channelHasData(CPAP_Obstructive)
               || day->channelHasData(CPAP_Hypopnea)
               || day->channelHasData(CPAP_ClearAirway)
               || day->channelHasData(CPAP_RERA)
               || day->channelHasData(CPAP_Apnea)
               || day->channelHasData(CPAP_FlowLimit)
               || day->channelHasData(CPAP_SensAwake)
               ) {
            html += "<tr><td align=center><img src=\"qrc:/docs/0.0.gif\"></td></tr>\n";
    }
    html+="</table>\n";
    html+="<hr/>\n";

    return html;
}

// honorPieChart true - show pie chart if it is enabled.  False, do not show pie chart
QString Daily::getLeftSidebar (bool honorPieChart) {
    QString html =   htmlLeftHeader
                   + htmlLeftAHI
                   + htmlLeftMachineInfo
                   + htmlLeftSleepTime
                   + htmlLeftIndices;
    // Include pie chart if wanted and enabled.
    if (honorPieChart && AppSetting->showPieChart())
        html += htmlLeftPieChart;

    html +=   htmlLeftNoHours
            + htmlLeftStatistics
            + htmlLeftOximeter
            + htmlLeftMachineSettings
            + htmlLeftSessionInfo
            + htmlLeftFooter;

    return html;
}

QVariant MyTextBrowser::loadResource(int type, const QUrl &url)
{
    if (type == QTextDocument::ImageResource && url.scheme().compare(QLatin1String("data"), Qt::CaseInsensitive) == 0) {
        static QRegularExpression re("^image/[^;]+;base64,.+={0,2}$");
        QRegularExpressionMatch match = re.match(url.path());
        if (match.hasMatch())
            return QVariant();
    }
    return QTextBrowser::loadResource(type, url);
}


void Daily::Load(QDate date)
{
    dateDisplay->setText("<i>"+date.toString(Qt::SystemLocaleLongDate)+"</i>");
    previous_date=date;

    Day * day = p_profile->GetDay(date);
    Machine *cpap = nullptr,
            *oxi = nullptr,
            //*stage = nullptr,
            *posit = nullptr;

    if (day) {
        cpap = day->machine(MT_CPAP);
        oxi = day->machine(MT_OXIMETER);
   //     stage = day->machine(MT_SLEEPSTAGE);
        posit = day->machine(MT_POSITION);
    }

    if (!AppSetting->cacheSessions()) {
        // Getting trashed on purge last day...

        // lastcpapday can get purged and be invalid
        if (lastcpapday && (lastcpapday!=day)) {
            for (QMap<QDate, Day *>::iterator di = p_profile->daylist.begin(); di!= p_profile->daylist.end(); ++di) {
                Day * d = di.value();
                if (d->eventsLoaded()) {
                    if (d->useCounter() == 0) {
                        d->CloseEvents();
                    }
                }
            }
        }
    }

    lastcpapday=day;

    // Clear the components of the left sidebar prior to recreating them
    htmlLeftAHI.clear();
    htmlLeftMachineInfo.clear();
    htmlLeftSleepTime.clear();
    htmlLeftIndices.clear();
    htmlLeftPieChart.clear();
    htmlLeftNoHours.clear();
    htmlLeftStatistics.clear();
    htmlLeftOximeter.clear();
    htmlLeftMachineSettings.clear();
    htmlLeftSessionInfo.clear();

    htmlLeftHeader = "<html><head>"
    "</head>"
    "<body leftmargin=0 rightmargin=0 topmargin=0 marginwidth=0 marginheight=0>";

    if (day) {
        day->OpenEvents();
    }
    GraphView->setDay(day);


    UpdateEventsTree(ui->treeWidget, day);

    // FIXME:
    // Generating entire statistics because bookmarks may have changed.. (This updates the side panel too)
    mainwin->GenerateStatistics();

    snapGV->setDay(day);

    QString modestr;
    CPAPMode mode=MODE_UNKNOWN;
    QString a;
    bool isBrick=false;

    updateGraphCombo();
    ui->eventsCombo->clear();

    quint32 chans = schema::SPAN | schema::FLAG | schema::MINOR_FLAG;
    if (p_profile->general->showUnknownFlags()) chans |= schema::UNKNOWN;

    QList<ChannelID> available;
    if (day) available.append(day->getSortedMachineChannels(chans));

    for (int i=0; i < available.size(); ++i) {
        ChannelID code = available.at(i);
        schema::Channel & chan = schema::channel[code];
        ui->eventsCombo->addItem(chan.enabled() ? *icon_on : * icon_off, chan.label(), code);
        ui->eventsCombo->setItemData(i, chan.fullname(), Qt::ToolTipRole);

    }

    if (!cpap) {
        GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));
    }
    if (cpap) {
        float hours=day->hours(MT_CPAP);
        if (GraphView->isEmpty() && (hours>0)) {
            if (!p_profile->hasChannel(CPAP_Obstructive) && !p_profile->hasChannel(CPAP_Hypopnea)) {
                GraphView->setEmptyText(STR_Empty_Brick);

                GraphView->setEmptyImage(QPixmap(":/icons/sadface.png"));
                isBrick=true;
            } else {
                GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));
            }
        }

        mode=(CPAPMode)(int)day->settings_max(CPAP_Mode);

        modestr=schema::channel[CPAP_Mode].m_options[mode];

        EventDataType ahi=(day->count(CPAP_Obstructive)+day->count(CPAP_Hypopnea)+day->count(CPAP_ClearAirway)+day->count(CPAP_Apnea));
        if (p_profile->general->calculateRDI()) ahi+=day->count(CPAP_RERA);
        ahi/=hours;

        if (hours>0) {
            htmlLeftAHI="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
            htmlLeftAHI+="<tr>";
            if (!isBrick) {
                ChannelID ahichan=CPAP_AHI;
                QString ahiname=STR_TR_AHI;
                if (p_profile->general->calculateRDI()) {
                    ahichan=CPAP_RDI;
                    ahiname=STR_TR_RDI;
                }
                htmlLeftAHI+=QString("<td colspan=4 bgcolor='%1' align=center><p title='%4'><font size=+4 color='%2'><b>%3</b></font></p> &nbsp; <font size=+4 color='%2'><b>%5</b></font></td>\n")
                        .arg("#F88017").arg(COLOR_Text.name()).arg(ahiname).arg(schema::channel[ahichan].fullname()).arg(ahi,0,'f',2);
            } else {
                htmlLeftAHI+=QString("<td colspan=5 bgcolor='%1' align=center><font size=+4 color='yellow'>%2</font></td>\n")
                        .arg("#F88017").arg(tr("BRICK! :("));
            }
            htmlLeftAHI+="</tr>\n";
            htmlLeftAHI+="</table>\n";

            htmlLeftMachineInfo = getCPAPInformation(day);

            htmlLeftSleepTime = getSleepTime(day);

            htmlLeftIndices = "<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

            quint32 zchans = schema::SPAN | schema::FLAG;
            bool show_minors = true;
            if (p_profile->general->showUnknownFlags()) zchans |= schema::UNKNOWN;

            if (show_minors) zchans |= schema::MINOR_FLAG;
            QList<ChannelID> available = day->getSortedMachineChannels(zchans);

            EventDataType val;
            QHash<ChannelID, EventDataType> values;
            for (int i=0; i < available.size(); ++i) {
                ChannelID code = available.at(i);
                schema::Channel & chan = schema::channel[code];
//                if (!chan.enabled()) continue;
                QString data;
                if (chan.type() == schema::SPAN) {
                    val = (100.0 / hours)*(day->sum(code)/3600.0);
                    data = QString("%1%").arg(val,0,'f',2);
                } else {
                    val = day->count(code) / hours;
                    data = QString("%1").arg(val,0,'f',2);
                }
                // TODO: percentage would be another useful option here for things like
                // percentage of patient-triggered breaths, which is much more useful
                // than the duration of timed breaths per hour.
                values[code] = val;
                QColor altcolor = (brightness(chan.defaultColor()) < 0.3) ? Qt::white : Qt::black; // pick a contrasting color
                htmlLeftIndices+=QString("<tr><td align='left' bgcolor='%1'><b><font color='%2'><a href='event=%5' style='text-decoration:none;color:%2'>%3</a></font></b></td><td width=20% bgcolor='%1'><b><font color='%2'>%4</font></b></td></tr>")
                        .arg(chan.defaultColor().name()).arg(altcolor.name()).arg(chan.fullname()).arg(data).arg(code);
            }

            htmlLeftIndices+="</table><hr/>";

            htmlLeftPieChart = getPieChart((values[CPAP_Obstructive] + values[CPAP_Hypopnea] +
                                            values[CPAP_ClearAirway] + values[CPAP_Apnea] + values[CPAP_RERA] +
                                            values[CPAP_FlowLimit] + values[CPAP_SensAwake]), day);

        } else {  // No hours
            htmlLeftNoHours+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
            if (!isBrick) {
                htmlLeftNoHours+="<tr><td colspan='5'>&nbsp;</td></tr>\n";
                if (day->size()>0) {
                    htmlLeftNoHours+="<tr><td colspan=5 align='center'><font size='+3'>"+tr("Sessions all off!")+"</font></td></tr>";
                    htmlLeftNoHours+="<tr><td colspan=5 align='center><img src='qrc:/icons/logo-md.png'></td></tr>";
                    htmlLeftNoHours+="<tr bgcolor='#89abcd'><td colspan=5 align='center'><i><font color=white size=+1>"+tr("Sessions exist for this day but are switched off.")+"</font></i></td></tr>\n";
                    GraphView->setEmptyText(STR_Empty_NoSessions);
                } else {
                    htmlLeftNoHours+="<tr><td colspan=5 align='center'><b><h2>"+tr("Impossibly short session")+"</h2></b></td></tr>";
                    htmlLeftNoHours+="<tr><td colspan=5 align='center'><i>"+tr("Zero hours??")+"</i></td></tr>\n";
                }
            } else { // machine is a brick
                htmlLeftNoHours+="<tr><td colspan=5 align='center'><b><h2>"+tr("BRICK :(")+"</h2></b></td></tr>";
                htmlLeftNoHours+="<tr><td colspan=5 align='center'><i>"+tr("Sorry, this machine only provides compliance data.")+"</i></td></tr>\n";
                htmlLeftNoHours+="<tr><td colspan=5 align='center'><i>"+tr("Complain to your Equipment Provider!")+"</i></td></tr>\n";
            }
            htmlLeftNoHours+="<tr><td colspan='5'>&nbsp;</td></tr>\n";
            htmlLeftNoHours+="</table>\n";
        }

    } // if (!CPAP)
    else htmlLeftSleepTime = getSleepTime(day);

    if ((cpap && !isBrick && (day->hours()>0)) || oxi || posit) {

        htmlLeftStatistics = getStatisticsInfo(day);

    } else {
        if (cpap && day->hours(MT_CPAP)<0.0000001) {
        } else if (!isBrick) {
            htmlLeftStatistics ="<table cellspacing=0 cellpadding=0 border=0 height=100% width=100%>";
            htmlLeftStatistics+="<tr height=25%><td align=center></td></tr>";
            htmlLeftStatistics+="<tr><td align=center><font size='+3'>"+tr("\"Nothing's here!\"")+"</font></td></tr>";
            htmlLeftStatistics+="<tr><td align=center><img src='qrc:/icons/logo-md.png'></td></tr>";
            htmlLeftStatistics+="<tr height=5px><td align=center></td></tr>";
            htmlLeftStatistics+="<tr bgcolor='#89abcd'><td align=center><i><font size=+1 color=white>"+tr("No data is available for this day.")+"</font></i></td></tr>";
            htmlLeftStatistics+="<tr height=25%><td align=center></td></tr>";
            htmlLeftStatistics+="</table>\n";
        }

    }
    if (day) {
        htmlLeftOximeter = getOximeterInformation(day);
        htmlLeftMachineSettings = getMachineSettings(day);
        htmlLeftSessionInfo= getSessionInformation(day);
    }

    htmlLeftFooter ="</body></html>";

    QColor cols[]={
        COLOR_Gold,
        QColor("light blue"),
    };
    const int maxcolors=sizeof(cols)/sizeof(QColor);
    QList<Session *>::iterator i;

    sessionbar->clear();    // clear sessionbar as some days don't have sessions

    if (cpap) {
        int c=0;

        for (i=day->begin();i!=day->end();++i) {
            Session * s=*i;
            if ((*s).type() == MT_CPAP)
                sessionbar->add(s, cols[c % maxcolors]);
            c++;
        }
    }
    //sessbar->update();

#ifdef DEBUG_DAILY_HTML
    QString name = GetAppData()+"/test.html";
    QFile file(name);
    if (file.open(QFile::WriteOnly)) {
        const QByteArray & b = html.toLocal8Bit();
        file.write(b.data(), b.size());
        file.close();
    }
#endif

    webView->setHtml(getLeftSidebar(true));

    ui->JournalNotes->clear();

    ui->bookmarkTable->clearContents();
    ui->bookmarkTable->setRowCount(0);
    QStringList sl;
    //sl.append(tr("Starts"));
    //sl.append(tr("Notes"));
    ui->bookmarkTable->setHorizontalHeaderLabels(sl);
    ui->ZombieMeter->blockSignals(true);
    ui->weightSpinBox->blockSignals(true);
    ui->ouncesSpinBox->blockSignals(true);

    ui->weightSpinBox->setValue(0);
    ui->ouncesSpinBox->setValue(0);
    ui->ZombieMeter->setValue(5);
    ui->ouncesSpinBox->blockSignals(false);
    ui->weightSpinBox->blockSignals(false);
    ui->ZombieMeter->blockSignals(false);
    ui->BMI->display(0);
    ui->BMI->setVisible(false);
    ui->BMIlabel->setVisible(false);
    ui->toggleGraphs->setVisible(false);
    ui->toggleEvents->setVisible(false);

    BookmarksChanged=false;
    Session *journal=GetJournalSession(date);
    if (journal) {
        bool ok;
        if (journal->settings.contains(Journal_Notes))
            ui->JournalNotes->setHtml(journal->settings[Journal_Notes].toString());

        if (journal->settings.contains(Journal_Weight)) {
            double kg=journal->settings[Journal_Weight].toDouble(&ok);

            if (p_profile->general->unitSystem()==US_Metric) {
                ui->weightSpinBox->setDecimals(1);
                ui->weightSpinBox->blockSignals(true);
                ui->weightSpinBox->setValue(kg);
                ui->weightSpinBox->blockSignals(false);
                ui->ouncesSpinBox->setVisible(false);
                ui->weightSpinBox->setSuffix(STR_UNIT_KG);
            } else {
                float ounces=(kg*1000.0)/ounce_convert;
                int pounds=ounces/16.0;
                double oz;
                double frac=modf(ounces,&oz);
                ounces=(int(ounces) % 16)+frac;
                ui->weightSpinBox->blockSignals(true);
                ui->ouncesSpinBox->blockSignals(true);
                ui->weightSpinBox->setValue(pounds);
                ui->ouncesSpinBox->setValue(ounces);
                ui->ouncesSpinBox->blockSignals(false);
                ui->weightSpinBox->blockSignals(false);

                ui->weightSpinBox->setSuffix(STR_UNIT_POUND);
                ui->weightSpinBox->setDecimals(0);
                ui->ouncesSpinBox->setVisible(true);
                ui->ouncesSpinBox->setSuffix(STR_UNIT_OUNCE);
            }
            double height=p_profile->user->height()/100.0;
            if (height>0 && kg>0) {
                double bmi=kg/(height*height);
                ui->BMI->setVisible(true);
                ui->BMIlabel->setVisible(true);
                ui->BMI->display(bmi);
            }
        }

        if (journal->settings.contains(Journal_ZombieMeter)) {
            ui->ZombieMeter->blockSignals(true);
            ui->ZombieMeter->setValue(journal->settings[Journal_ZombieMeter].toDouble(&ok));
            ui->ZombieMeter->blockSignals(false);
        }

        if (journal->settings.contains(Bookmark_Start)) {
            QVariantList start=journal->settings[Bookmark_Start].toList();
            QVariantList end=journal->settings[Bookmark_End].toList();
            QStringList notes=journal->settings[Bookmark_Notes].toStringList();

            ui->bookmarkTable->blockSignals(true);


            qint64 clockdrift=p_profile->cpap->clockDrift()*1000L,drift;
            Day * dday=p_profile->GetDay(previous_date,MT_CPAP);
            drift=(dday!=nullptr) ? clockdrift : 0;

            bool ok;
            for (int i=0;i<start.size();i++) {
                qint64 st=start.at(i).toLongLong(&ok)+drift;
                qint64 et=end.at(i).toLongLong(&ok)+drift;

                QDateTime d=QDateTime::fromTime_t(st/1000L);
                //int row=ui->bookmarkTable->rowCount();
                ui->bookmarkTable->insertRow(i);
                QTableWidgetItem *tw=new QTableWidgetItem(notes.at(i));
                QTableWidgetItem *dw=new QTableWidgetItem(d.time().toString("HH:mm:ss"));
                dw->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
                ui->bookmarkTable->setItem(i,0,dw);
                ui->bookmarkTable->setItem(i,1,tw);
                tw->setData(Qt::UserRole,st);
                tw->setData(Qt::UserRole+1,et);
            } // for (int i
            ui->bookmarkTable->blockSignals(false);
        } // if (journal->settings.contains(Bookmark_Start))
    } // if (journal)
}

void Daily::UnitsChanged()
{
    double kg;
    if (p_profile->general->unitSystem()==US_English) {
        kg=ui->weightSpinBox->value();
        float ounces=(kg*1000.0)/ounce_convert;
        int pounds=ounces/16;
        float oz=fmodf(ounces,16);
        ui->weightSpinBox->setValue(pounds);
        ui->ouncesSpinBox->setValue(oz);

        ui->weightSpinBox->setDecimals(0);
        ui->weightSpinBox->setSuffix(STR_UNIT_POUND);
        ui->ouncesSpinBox->setVisible(true);
        ui->ouncesSpinBox->setSuffix(STR_UNIT_OUNCE);
    } else {
        kg=(ui->weightSpinBox->value()*(ounce_convert*16.0))+(ui->ouncesSpinBox->value()*ounce_convert);
        kg/=1000.0;
        ui->weightSpinBox->setDecimals(1);
        ui->weightSpinBox->setValue(kg);
        ui->ouncesSpinBox->setVisible(false);
        ui->weightSpinBox->setSuffix(STR_UNIT_KG);
    }
}

void Daily::clearLastDay()
{
    lastcpapday=nullptr;
}


void Daily::Unload(QDate date)
{
    if (!date.isValid()) {
        date = getDate();
        if (!date.isValid()) {
            graphView()->setDay(nullptr);
            return;
        }
    }

    // Update the journal notes
    Session *journal = GetJournalSession(date);

    bool editorHasContent = !ui->JournalNotes->toPlainText().isEmpty(); // have a look as plaintext to see if really empty.

    if (!journal && editorHasContent) {
        journal = CreateJournalSession(date);
    }

    if (journal) {
        auto jit = journal->settings.find(Journal_Notes);

        if (jit != journal->settings.end()) {
            // we do have a journal_notes record
            if (editorHasContent) {
                const QString & html = ui->JournalNotes->toHtml();

                if (jit.value() != html) { // has the content of it changed?
                    jit.value() = html;
                    journal->SetChanged(true);
                }
            } else {
                // empty, so don't need this notes setting anymore
                journal->settings.erase(jit);
                journal->SetChanged(true);
            }
        } else if (editorHasContent) {
            // Create the note
            journal->settings[Journal_Notes] = ui->JournalNotes->toHtml();
            journal->SetChanged(true);
        }

        if (journal->IsChanged()) {
            journal->settings[LastUpdated] = QDateTime::currentDateTime();
            journal->machine()->SaveSummaryCache();
            journal->SetChanged(false); // save summary doesn't automatically do this
        }
    }
    UpdateCalendarDay(date);
}

void Daily::on_JournalNotesItalic_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    format.setFontItalic(!format.fontItalic());


    cursor.mergeCharFormat(format);
   //ui->JournalNotes->mergeCurrentCharFormat(format);

}

void Daily::on_JournalNotesBold_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    int fw=format.fontWeight();
    if (fw!=QFont::Bold)
        format.setFontWeight(QFont::Bold);
    else
        format.setFontWeight(QFont::Normal);

    cursor.mergeCharFormat(format);
    //ui->JournalNotes->mergeCurrentCharFormat(format);

}

void Daily::on_JournalNotesFontsize_activated(int index)
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    QFont font=format.font();
    int fontsize=10;

    if (index==1) fontsize=15;
    else if (index==2) fontsize=25;

    font.setPointSize(fontsize);
    format.setFont(font);

    cursor.mergeCharFormat(format);
}

void Daily::on_JournalNotesColour_clicked()
{
    QColor col=QColorDialog::getColor(COLOR_Black,this,tr("Pick a Colour")); //,QColorDialog::NoButtons);
    if (!col.isValid()) return;

    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QBrush b(col);
    QPalette newPalette = palette();
    newPalette.setColor(QPalette::ButtonText, col);
    ui->JournalNotesColour->setPalette(newPalette);

    QTextCharFormat format=cursor.charFormat();

    format.setForeground(b);

    cursor.setCharFormat(format);
}
Session * Daily::CreateJournalSession(QDate date)
{
    Machine *m = p_profile->GetMachine(MT_JOURNAL);
    if (!m) {
        m=new Machine(p_profile, 0);
        MachineInfo info;
        info.loadername = "Journal";
        info.serial = m->hexid();
        info.brand = "Journal";
        info.type = MT_JOURNAL;
        m->setInfo(info);
        m->setType(MT_JOURNAL);
        p_profile->AddMachine(m);
    }

    Session *sess=new Session(m,0);
    qint64 st,et;

    Day *cday=p_profile->GetDay(date);
    if (cday) {
        st=cday->first();
        et=cday->last();
    } else {
        QDateTime dt(date,QTime(20,0));
        st=qint64(dt.toTime_t())*1000L;
        et=st+3600000L;
    }
    sess->SetSessionID(st / 1000L);
    sess->set_first(st);
    sess->set_last(et);
    m->AddSession(sess);
    return sess;
}
Session * Daily::GetJournalSession(QDate date) // Get the first journal session
{
    Day *day=p_profile->GetDay(date, MT_JOURNAL);
    if (day) {
        Session * session = day->firstSession(MT_JOURNAL);
        if (!session) {
            session = CreateJournalSession(date);
        }
        return session;
    }
    return nullptr;
}


void Daily::RedrawGraphs()
{
    // setting this here, because it needs to be done when preferences change
    if (p_profile->cpap->showLeakRedline()) {
        schema::channel[CPAP_Leak].setUpperThreshold(p_profile->cpap->leakRedline());
    } else {
        schema::channel[CPAP_Leak].setUpperThreshold(0); // switch it off
    }

    QFont appFont = QApplication::font();
    dateDisplay->setFont(appFont);

    GraphView->redraw();
}

void Daily::on_LineCursorUpdate(double time)
{
    if (time > 1) {
        // use local time since this string is displayed to the user
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(time, Qt::LocalTime);
        QString txt = dt.toString("MMM dd HH:mm:ss.zzz");
        dateDisplay->setText(txt);
    } else dateDisplay->setText(QString(GraphView->emptyText()));
}

void Daily::on_RangeUpdate(double minx, double /*maxx*/)
{
    if (minx > 1) {
        dateDisplay->setText(GraphView->getRangeString());
    } else {
        dateDisplay->setText(QString(GraphView->emptyText()));
    }
}


void Daily::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    QDateTime d;
    if (!item->data(0,Qt::UserRole).isNull()) {
        qint64 winsize=qint64(p_profile->general->eventWindowSize())*60000L;
        qint64 t=item->data(0,Qt::UserRole).toLongLong();

        double st=t-(winsize/2);
        double et=t+(winsize/2);

        gGraph *g=GraphView->findGraph(STR_GRAPH_SleepFlags);
        if (!g) return;
        if (st<g->rmin_x) {
            st=g->rmin_x;
            et=st+winsize;
        }
        if (et>g->rmax_x) {
            et=g->rmax_x;
            st=et-winsize;
        }
        GraphView->SetXBounds(st,et);
    }
}

void Daily::on_treeWidget_itemSelectionChanged()
{
    if (ui->treeWidget->selectedItems().size()==0) return;
    QTreeWidgetItem *item=ui->treeWidget->selectedItems().at(0);
    if (!item) return;
    on_treeWidget_itemClicked(item, 0);
}

void Daily::on_JournalNotesUnderline_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    format.setFontUnderline(!format.fontUnderline());

    cursor.mergeCharFormat(format);
   //ui->JournalNotes->mergeCurrentCharFormat(format);
}

void Daily::on_prevDayButton_clicked()
{
    if (!p_profile->ExistsAndTrue("SkipEmptyDays")) {
        LoadDate(previous_date.addDays(-1));
    } else {
        QDate d=previous_date;
        for (int i=0;i<90;i++) {
            d=d.addDays(-1);
            if (p_profile->GetDay(d)) {
                LoadDate(d);
                break;
            }
        }
    }
}

void Daily::on_nextDayButton_clicked()
{
    if (!p_profile->ExistsAndTrue("SkipEmptyDays")) {
        LoadDate(previous_date.addDays(1));
    } else {
        QDate d=previous_date;
        for (int i=0;i<90;i++) {
            d=d.addDays(1);
            if (p_profile->GetDay(d)) {
                LoadDate(d);
                break;
            }
        }
    }
}

void Daily::on_calButton_toggled(bool checked)
{
    bool b=checked;
    ui->calendarFrame->setVisible(b);
    AppSetting->setCalendarVisible(b);

    if (!b) {
        ui->calButton->setArrowType(Qt::DownArrow);
    } else {
        ui->calButton->setArrowType(Qt::UpArrow);
    }
}


void Daily::on_todayButton_clicked()
{
//    QDate d=QDate::currentDate();
//    if (d > p_profile->LastDay()) {
        QDate lastcpap = p_profile->LastDay(MT_CPAP);
        QDate lastoxi = p_profile->LastDay(MT_OXIMETER);

        QDate d = qMax(lastcpap, lastoxi);
//    }
    LoadDate(d);
}

void Daily::on_evViewSlider_valueChanged(int value)
{
    ui->evViewLCD->display(value);
    p_profile->general->setEventWindowSize(value);

    int winsize=value*60;

    gGraph *g=GraphView->findGraph(STR_GRAPH_SleepFlags);
    if (!g) return;
    qint64 st=g->min_x;
    qint64 et=g->max_x;
    qint64 len=et-st;
    qint64 d=st+len/2.0;

    st=d-(winsize/2)*1000;
    et=d+(winsize/2)*1000;
    if (st<g->rmin_x) {
        st=g->rmin_x;
        et=st+winsize*1000;
    }
    if (et>g->rmax_x) {
        et=g->rmax_x;
        st=et-winsize*1000;
    }
    GraphView->SetXBounds(st,et);
}

void Daily::on_bookmarkTable_itemClicked(QTableWidgetItem *item)
{
    int row=item->row();
    qint64 st,et;

//    qint64 clockdrift=p_profile->cpap->clockDrift()*1000L,drift;
//    Day * dday=p_profile->GetDay(previous_date,MT_CPAP);
//    drift=(dday!=nullptr) ? clockdrift : 0;

    QTableWidgetItem *it=ui->bookmarkTable->item(row,1);
    bool ok;
    st=it->data(Qt::UserRole).toLongLong(&ok);
    et=it->data(Qt::UserRole+1).toLongLong(&ok);
    qint64 st2=0,et2=0,st3,et3;
    Day * day=p_profile->GetGoodDay(previous_date,MT_CPAP);
    if (day) {
        st2=day->first();
        et2=day->last();
    }
    Day * oxi=p_profile->GetGoodDay(previous_date,MT_OXIMETER);
    if (oxi) {
        st3=oxi->first();
        et3=oxi->last();
    }
    if (oxi && day) {
        st2=qMin(st2,st3);
        et2=qMax(et2,et3);
    } else if (oxi) {
        st2=st3;
        et2=et3;
    } else if (!day) return;
    if ((et<st2) || (st>et2)) {
        mainwin->Notify(tr("This bookmark is in a currently disabled area.."));
        return;
    }

    if (st<st2) st=st2;
    if (et>et2) et=et2;
    GraphView->SetXBounds(st,et);
    GraphView->redraw();
}

void Daily::addBookmark(qint64 st, qint64 et, QString text)
{
    ui->bookmarkTable->blockSignals(true);
    QDateTime d=QDateTime::fromTime_t(st/1000L, Qt::LocalTime);
    int row=ui->bookmarkTable->rowCount();
    ui->bookmarkTable->insertRow(row);
    QTableWidgetItem *tw=new QTableWidgetItem(text);
    QTableWidgetItem *dw=new QTableWidgetItem(d.time().toString("HH:mm:ss"));
    dw->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    ui->bookmarkTable->setItem(row,0,dw);
    ui->bookmarkTable->setItem(row,1,tw);
    qint64 clockdrift=p_profile->cpap->clockDrift()*1000L,drift;
    Day * day=p_profile->GetDay(previous_date,MT_CPAP);
    drift=(day!=nullptr) ? clockdrift : 0;

    // Counter CPAP clock drift for storage, in case user changes it later on
    // This won't fix the text string names..

    tw->setData(Qt::UserRole,st-drift);
    tw->setData(Qt::UserRole+1,et-drift);

    ui->bookmarkTable->blockSignals(false);
    update_Bookmarks();
    mainwin->updateFavourites();

}

void Daily::on_addBookmarkButton_clicked()
{
    qint64 st,et;
    GraphView->GetXBounds(st,et);
    QDateTime d=QDateTime::fromTime_t(st/1000L, Qt::LocalTime);

    addBookmark(st,et, tr("Bookmark at %1").arg(d.time().toString("HH:mm:ss")));
}
void Daily::update_Bookmarks()
{
    QVariantList start;
    QVariantList end;
    QStringList notes;
    QTableWidgetItem *item;
    for (int row=0;row<ui->bookmarkTable->rowCount();row++) {
        item=ui->bookmarkTable->item(row,1);

        start.push_back(item->data(Qt::UserRole));
        end.push_back(item->data(Qt::UserRole+1));
        notes.push_back(item->text());
    }
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }

    journal->settings[Bookmark_Start]=start;
    journal->settings[Bookmark_End]=end;
    journal->settings[Bookmark_Notes]=notes;
    journal->settings[LastUpdated]=QDateTime::currentDateTime();
    journal->SetChanged(true);
    BookmarksChanged=true;
    mainwin->updateFavourites();
}

void Daily::on_removeBookmarkButton_clicked()
{
    int row=ui->bookmarkTable->currentRow();
    if (row>=0) {
        ui->bookmarkTable->blockSignals(true);
        ui->bookmarkTable->removeRow(row);
        ui->bookmarkTable->blockSignals(false);
        update_Bookmarks();
    }
    mainwin->updateFavourites();
}
void Daily::on_ZombieMeter_valueChanged(int action)
{
    Q_UNUSED(action);
    ZombieMeterMoved=true;
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }
    journal->settings[Journal_ZombieMeter]=ui->ZombieMeter->value();
    journal->SetChanged(true);
}

void Daily::on_bookmarkTable_itemChanged(QTableWidgetItem *item)
{
    Q_UNUSED(item);
    update_Bookmarks();
}

void Daily::on_weightSpinBox_valueChanged(double arg1)
{
    // Update the BMI display
    double kg;
    if (p_profile->general->unitSystem()==US_English) {
         kg=((arg1*pound_convert) + (ui->ouncesSpinBox->value()*ounce_convert)) / 1000.0;
    } else kg=arg1;
    double height=p_profile->user->height()/100.0;
    if ((height>0) && (kg>0)) {
        double bmi=kg/(height * height);
        ui->BMI->display(bmi);
        ui->BMI->setVisible(true);
        ui->BMIlabel->setVisible(true);
    } else {
        ui->BMI->setVisible(false);
        ui->BMIlabel->setVisible(false);
    }
}

void Daily::on_weightSpinBox_editingFinished()
{
    double arg1=ui->weightSpinBox->value();

    double height=p_profile->user->height()/100.0;
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }

    double kg;
    if (p_profile->general->unitSystem()==US_English) {
            kg=((arg1*pound_convert) + (ui->ouncesSpinBox->value()*ounce_convert)) / 1000.0;
    } else {
            kg=arg1;
    }
    journal->settings[Journal_Weight]=kg;
    gGraphView *gv=mainwin->getOverview()->graphView();
    gGraph *g;
    if (gv) {
        g=gv->findGraph(STR_GRAPH_Weight);
        if (g) g->setDay(nullptr);
    }
    if ((height>0) && (kg>0)) {
        double bmi=kg/(height * height);
        ui->BMI->display(bmi);
        ui->BMI->setVisible(true);
        ui->BMIlabel->setVisible(true);
        journal->settings[Journal_BMI]=bmi;
        if (gv) {
            g=gv->findGraph(STR_GRAPH_BMI);
            if (g) g->setDay(nullptr);
        }
    } else {
        ui->BMI->setVisible(false);
        ui->BMIlabel->setVisible(false);
    }
    journal->SetChanged(true);
}

void Daily::on_ouncesSpinBox_valueChanged(int arg1)
{
    // just update for BMI display
    double height=p_profile->user->height()/100.0;
    double kg=((ui->weightSpinBox->value()*pound_convert) + (arg1*ounce_convert)) / 1000.0;
    if ((height>0) && (kg>0)) {
        double bmi=kg/(height * height);
        ui->BMI->display(bmi);
        ui->BMI->setVisible(true);
        ui->BMIlabel->setVisible(true);
    } else {
        ui->BMI->setVisible(false);
        ui->BMIlabel->setVisible(false);
    }
}

void Daily::on_ouncesSpinBox_editingFinished()
{
    double arg1=ui->ouncesSpinBox->value();
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }
    double height=p_profile->user->height()/100.0;
    double kg=((ui->weightSpinBox->value()*pound_convert) + (arg1*ounce_convert)) / 1000.0;
    journal->settings[Journal_Weight]=kg;

    gGraph *g;
    if (mainwin->getOverview()) {
        g=mainwin->getOverview()->graphView()->findGraph(STR_GRAPH_Weight);
        if (g) g->setDay(nullptr);
    }

    if ((height>0) && (kg>0)) {
        double bmi=kg/(height * height);
        ui->BMI->display(bmi);
        ui->BMI->setVisible(true);
        ui->BMIlabel->setVisible(true);

        journal->settings[Journal_BMI]=bmi;
        if (mainwin->getOverview()) {
            g=mainwin->getOverview()->graphView()->findGraph(STR_GRAPH_BMI);
            if (g) g->setDay(nullptr);
        }
    } else {
        ui->BMI->setVisible(false);
        ui->BMIlabel->setVisible(false);
    }
    journal->SetChanged(true);
}

QString Daily::GetDetailsText()
{
    QTextDocument * doc = webView->document();
    QString content = doc->toHtml();

    return content;
}

void Daily::on_graphCombo_activated(int index)
{
    if (index<0)
        return;

    gGraph *g;
    QString s;
    s=ui->graphCombo->currentText();
    bool b=!ui->graphCombo->itemData(index,Qt::UserRole).toBool();
    ui->graphCombo->setItemData(index,b,Qt::UserRole);
    if (b) {
        ui->graphCombo->setItemIcon(index,*icon_on);
    } else {
        ui->graphCombo->setItemIcon(index,*icon_off);
    }
    g=GraphView->findGraphTitle(s);
    g->setVisible(b);

    updateCube();
    GraphView->updateScale();
    GraphView->redraw();
}

void Daily::updateCube()
{
    //brick..
    if ((GraphView->visibleGraphs()==0)) {
        ui->toggleGraphs->setVisible(false);
//        ui->toggleGraphs->setArrowType(Qt::UpArrow);
//        ui->toggleGraphs->setToolTip(tr("Show all graphs"));
//        ui->toggleGraphs->blockSignals(true);
//        ui->toggleGraphs->setChecked(true);
//        ui->toggleGraphs->blockSignals(false);

        if (ui->graphCombo->count() > 0) {
            GraphView->setEmptyText(STR_Empty_NoGraphs);
        } else {
            if (!p_profile->GetGoodDay(getDate(), MT_CPAP)
              && !p_profile->GetGoodDay(getDate(), MT_OXIMETER)
              && !p_profile->GetGoodDay(getDate(), MT_SLEEPSTAGE)
              && !p_profile->GetGoodDay(getDate(), MT_POSITION)) {
                GraphView->setEmptyText(STR_Empty_NoData);

            } else {
                if (GraphView->emptyText() != STR_Empty_Brick)
                    GraphView->setEmptyText(STR_Empty_SummaryOnly);
            }
        }
    } else {
        ui->toggleGraphs->setVisible(false);
//        ui->toggleGraphs->setArrowType(Qt::DownArrow);
//        ui->toggleGraphs->setToolTip(tr("Hide all graphs"));
//        ui->toggleGraphs->blockSignals(true);
//        ui->toggleGraphs->setChecked(false);
//        ui->toggleGraphs->blockSignals(false);
    }
}


void Daily::on_toggleGraphs_clicked(bool /*checked*/)
{
    QString s;
    //QIcon *icon=checked ? icon_off : icon_on;
    if (ui->graphCombo == nullptr )
        qDebug() << "ToggleGraphs clicked with null graphCombo ptr";
    else
        qDebug() << "ToggleGraphs clicked with non-null graphCombo ptr";
    return;
/*
    for (int i=0;i<ui->graphCombo->count();i++) {
        s=ui->graphCombo->itemText(i);
        ui->graphCombo->setItemIcon(i,*icon);
        ui->graphCombo->setItemData(i,!checked,Qt::UserRole);
    }
    for (int i=0;i<GraphView->size();i++) {
        (*GraphView)[i]->setVisible(!checked);
    }

    updateCube();
    GraphView->updateScale();
    GraphView->redraw();
    */
}

void Daily::updateGraphCombo()
{
    ui->graphCombo->clear();
    gGraph *g;

    for (int i=0;i<GraphView->size();i++) {
        g=(*GraphView)[i];
        if (g->isEmpty()) continue;


        if (g->visible()) {
            ui->graphCombo->addItem(*icon_on,g->title(),true);
        } else {
            ui->graphCombo->addItem(*icon_off,g->title(),false);
        }
    }
    ui->graphCombo->setCurrentIndex(0);

    updateCube();
}

void Daily::on_eventsCombo_activated(int index)
{
    if (index<0)
        return;

    ChannelID code = ui->eventsCombo->itemData(index, Qt::UserRole).toUInt();
    schema::Channel * chan = &schema::channel[code];

    bool b = !chan->enabled();
    chan->setEnabled(b);

    ui->eventsCombo->setItemIcon(index,b ? *icon_on : *icon_off);

    GraphView->redraw();
}

void Daily::on_toggleEvents_clicked(bool /*checked*/)
{
    QString s;
    //QIcon *icon=checked ? icon_on : icon_off;

    if (ui->toggleEvents == nullptr )
        qDebug() << "ToggleEvents clicked with null toggleEvents ptr";
    else
        qDebug() << "ToggleEvents clicked with non-null toggleEvents ptr";
    return;
/*
    ui->toggleEvents->setArrowType(checked ? Qt::DownArrow : Qt::UpArrow);
    ui->toggleEvents->setToolTip(checked ? tr("Hide all events") : tr("Show all events"));
//    ui->toggleEvents->blockSignals(true);
//    ui->toggleEvents->setChecked(false);
//    ui->toggleEvents->blockSignals(false);

    for (int i=0;i<ui->eventsCombo->count();i++) {
//        s=ui->eventsCombo->itemText(i);
        ui->eventsCombo->setItemIcon(i,*icon);
        ChannelID code = ui->eventsCombo->itemData(i).toUInt();
        schema::channel[code].setEnabled(checked);
    }

    updateCube();
    GraphView->redraw();
    */
}

//void Daily::on_sessionWidget_itemSelectionChanged()
//{
//    int row = ui->sessionWidget->currentRow();
//    QTableWidgetItem *item = ui->sessionWidget->item(row, 0);
//    if (item) {
//        QDate date = item->data(Qt::UserRole).toDate();
//        LoadDate(date);
//        qDebug() << "Clicked.. changing date to" << date;

//       // ui->sessionWidget->setCurrentItem(item);
//    }
//}

void Daily::on_splitter_2_splitterMoved(int, int)
{
    int size = ui->splitter_2->sizes()[0];
    if (size == 0) return;
//  qDebug() << "Left Panel width set to " << size;
    AppSetting->setDailyPanelWidth(size);
}
