/* OSCAR C[onditional] Progress Bar
 *
 * Copyright (C) 2019 Guy Scharf
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details.
 *
 * The C[onditional] progress bar causes the progress bar window to show only if the task
 * is not half done in two seconds (number of seconds is variable).  The CProgressBar can
 * thus be used in all situations in which a progress bar might be appropriate without
 * determining ahead of time whether the task will take long enough to warrant use of a progress bar.
 *
 * This is especially useful when even though the number of iterations is known, the time required
 * to perform each iteration is not known ahead of time.  For example, reading summary files and
 * discovering that some need to be updated but others do not.
 */

#ifndef CPROGRESSBAR_H
#define CPROGRESSBAR_H

#include <QProgressDialog>
#include <QElapsedTimer>


class CProgressBar
{
public:
    CProgressBar(QString title, QWidget * parent, long maxValue);

    void setMaximum (long max);
    void add (long count);
    void setWidth (int width);
    void setTitle (QString title);

    void start ();
    void close ();

private:
    bool showProgress = false;
    bool timeChecked = false;

    int lastPctDone = 0;
    long timerLimit = 2000;
    long maxSteps = 0;
    long numDone = 0;
    int width = 250;
    QString savedTitle;

    QWidget * parent = nullptr;
    QProgressDialog * progress = nullptr;
    QElapsedTimer timer;
};

#endif // CPROGRESSBAR_H
