/* Overview GUI Headers
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef OVERVIEW_H
#define OVERVIEW_H

#include <QWidget>
#ifndef BROKEN_OPENGL_BUILD
#include <QGLContext>
#endif
#include <QHBoxLayout>
#include <QDateEdit>
#include "SleepLib/profiles.h"
#include "Graphs/gGraphView.h"
#include "Graphs/gSummaryChart.h"
#include "Graphs/gSessionTimesChart.h"

namespace Ui {
class Overview;
}

class Report;

enum YTickerType { YT_Number, YT_Time, YT_Weight };

/*! \class Overview
    \author Mark Watkins <jedimark_at_users.sourceforge.net>
    \brief Overview tab, showing overall summary data
    */
class Overview : public QWidget
{
    Q_OBJECT

  public:
    explicit Overview(QWidget *parent, gGraphView *shared = nullptr);
    ~Overview();

    //! \brief Returns Overview gGraphView object containing it's graphs
    gGraphView *graphView() { return GraphView; }

    //! \brief Recalculates Overview chart info
    void ReloadGraphs();

    //! \brief Recalculates Overview chart info, but keeps the date set
    void ResetGraphs();

    //! \brief Reset graphs to uniform heights
    void ResetGraphLayout();

    //! \brief Calls updateGL to redraw the overview charts
    void RedrawGraphs();

    //! \brief Sets the currently selected date range of the overview display
    void setRange(QDate start, QDate end);

    /*! \brief Create an overview graph, adding it to the overview gGraphView object
        \param QString name  The title of the graph
        \param QString units The units of measurements to show in the popup */
    gGraph *createGraph(QString code, QString name, QString units = "", YTickerType yttype = YT_Number);
    gGraph *AHI, *AHIHR, *UC, *FL, *SA, *US, *PR, *LK, *NPB, *SET, *SES, *RR, *MV, *TV, *PTB, *PULSE, *SPO2, *NLL,
           *WEIGHT, *ZOMBIE, *BMI, *TGMV, *TOTLK, *STG, *SN, *TTIA;
    SummaryChart *bc, *sa, *us, *pr,  *set, *ses,  *ptb, *pulse, *spo2,
                 *weight, *zombie, *bmi, *ahihr, *tgmv, *totlk;

    gSummaryChart * stg, *uc, *ahi, * pres, *lk, *npb, *rr, *mv, *tv, *nll, *sn, *ttia;

    //! \breif List of SummaryCharts shown on the overview page
    QVector<SummaryChart *> OverviewCharts;

    void ResetGraph(QString name);

    void RebuildGraphs(bool reset = true);

  public slots:
    void onRebuildGraphs() { RebuildGraphs(true); }

  private slots:
    void updateGraphCombo();

    //! \brief Resets the graph view because the Start date has been changed
    void on_dateStart_dateChanged(const QDate &date);

    //! \brief Resets the graph view because the End date has been changed
    void on_dateEnd_dateChanged(const QDate &date);

    //! \brief Updates the calendar highlighting when changing to a new month
    void dateStart_currentPageChanged(int year, int month);

    //! \brief Updates the calendar highlighting when changing to a new month
    void dateEnd_currentPageChanged(int year, int month);

    //! \brief Resets view to currently shown start & end dates
    void on_toolButton_clicked();

    //void on_printDailyButton_clicked();

    void on_rangeCombo_activated(int index);

    void on_graphCombo_activated(int index);

    void on_toggleVisibility_clicked(bool checked);

    void on_LineCursorUpdate(double time);
    void on_RangeUpdate(double minx, double maxx);


  private:
    Ui::Overview *ui;
    gGraphView *GraphView;
    MyScrollBar *scrollbar;
    QHBoxLayout *layout;
    gGraphView *m_shared;
    QIcon *icon_on;
    QIcon *icon_off;
    MyLabel *dateLabel;

    //! \brief Updates the calendar highlighting for the calendar object for this date.
    void UpdateCalendarDay(QDateEdit *calendar, QDate date);
    void updateCube();

    Day *day; // dummy in this case

};

#endif // OVERVIEW_H
