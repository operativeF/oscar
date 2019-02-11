/* SleepLib Progress Dialog Header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "progressdialog.h"

ProgressDialog::ProgressDialog(QWidget * parent):
    QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
{
    statusMsg = new QLabel(QObject::tr("Please Wait..."));
    hlayout = new QHBoxLayout;

    imglabel = new QLabel(this);

    vlayout = new QVBoxLayout;
    progress = new QProgressBar(this);
    this->setLayout(vlayout);
    vlayout->addLayout(hlayout);
    hlayout->addWidget(imglabel);
    hlayout->addWidget(statusMsg,1,Qt::AlignCenter);
    vlayout->addWidget(progress,1);
    progress->setMaximum(100);
    abortButton = nullptr;
    setWindowModality(Qt::ApplicationModal);

}

ProgressDialog::~ProgressDialog()
{
    if (abortButton) {
        disconnect(abortButton, SIGNAL(released()), this, SLOT(onAbortClicked()));
    }
}

void ProgressDialog::setProgressMax(int max)
{
    progress->setMaximum(max);
}

void ProgressDialog::setProgressValue(int val)
{
    progress->setValue(val);
}


void ProgressDialog::setMessage(QString msg) {
    statusMsg->setText(msg);
}

void ProgressDialog::addAbortButton()
{
    abortButton = new QPushButton(tr("Abort"),this);
    connect(abortButton, SIGNAL(released()), this, SLOT(onAbortClicked()));
    hlayout->addWidget(abortButton);
}

void ProgressDialog::onAbortClicked()
{
    emit abortClicked();
}
