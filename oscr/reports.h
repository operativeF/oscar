/* Reports Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef REPORTS_H
#define REPORTS_H
#include "Graphs/gGraphView.h"

class Report
{
  public:
    Report();

    /*! \fn void PrintReport gGraphView *gv,QString name, QDate date=QDate::currentDate());
        \brief Prepares a report using gGraphView object, and sends to a created QPrinter object
        \param gGraphView *gv  GraphView Object containing which graph set to print
        \param QString name   Report Title
        \param QDate date
        */
    static void PrintReport(gGraphView *gv, QString name, QDate date = QDate::currentDate());
};

#endif // REPORTS_H
