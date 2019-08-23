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

#include <QCoreApplication>

#include "cprogressbar.h"

CProgressBar::CProgressBar(QString title, QWidget * parent, long maxValue)
{
    savedTitle = title;
    maxSteps = maxValue;
    progress = nullptr;
    showProgress = false;
    this->parent = parent;
}

void CProgressBar::start () {
    timeChecked = false;
    timer.start();
}

void CProgressBar::setMaximum (long value) {
    maxSteps = value;
}

void CProgressBar::setTitle (QString title) {
    savedTitle = title;
}

void CProgressBar::setWidth (int width) {
    this->width = width;
}

void CProgressBar::add (long count) {
    numDone += count;

    // If timer not started, start it now.
    if (!timer.isValid())
        timer.start();

    // See if timer limit has passed
    if (!progress && !timeChecked && (timer.elapsed() > timerLimit)) {
        maxSteps -= numDone;       // maxSteps now is number of steps remaining
        numDone = 1;               // and we figure only one processed so far

        progress = new QProgressDialog(savedTitle,
                                QString(), 0, maxSteps, parent,
                                Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
        progress->setWindowModality(Qt::WindowModal);
        progress->setValue(0);
        progress->setMinimumWidth(width);
        progress->show();

        QCoreApplication::processEvents();
        showProgress = true;
    }

    // Update progress bar if one is displayed and more than 1% different from last display
    if (showProgress) {
        int pctDone = (100 * numDone) / maxSteps;
        if (pctDone != lastPctDone) {
            lastPctDone = pctDone;
            progress->setValue(numDone);
            QCoreApplication::processEvents();
        }
    }
}

void CProgressBar::close () {
    if (progress) {
        progress->setValue(maxSteps);
    }
}
