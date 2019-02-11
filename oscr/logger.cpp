/* SleepyHead Logger module implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "logger.h"

#define ASSERTS_SUCK

QThreadPool * otherThreadPool = NULL;

void MyOutputHandler(QtMsgType type, const QMessageLogContext &context, const QString &msgtxt)
{
    Q_UNUSED(context)

    if (!logger) {
        fprintf(stderr, "Pre/Post: %s\n", msgtxt.toLocal8Bit().constData());
        return;
    }

    QString msg, typestr;

    switch (type) {
    case QtWarningMsg:
        typestr = QString("Warning: ");
        break;

    case QtFatalMsg:
        typestr = QString("Fatal: ");
        break;

    case QtCriticalMsg:
        typestr = QString("Critical: ");
        break;

    default:
        typestr = QString("Debug: ");
        break;
    }

    msg = typestr + msgtxt; //+QString(" (%1:%2, %3)").arg(context.file).arg(context.line).arg(context.function);


    if (logger && logger->isRunning()) {
        logger->append(msg);
    }
#ifdef ASSERTS_SUCK
//    else {
//        fprintf(stderr, "%s\n", msg.toLocal8Bit().data());
//    }
#endif

    if (type == QtFatalMsg) {
        abort();
    }

}

void initializeLogger()
{
    logger = new LogThread();
    otherThreadPool = new QThreadPool();
    bool b = otherThreadPool->tryStart(logger);
    qInstallMessageHandler(MyOutputHandler);
    if (b) {
        qDebug() << "Started logging thread";
    } else {
        qWarning() << "Logging thread did not start correctly";
    }

}

void shutdownLogger()
{
    if (logger) {
        logger->quit();
        otherThreadPool->waitForDone(-1);
        logger = NULL;
    }
    delete otherThreadPool;
}

LogThread * logger = NULL;

void LogThread::append(QString msg)
{
    QString tmp = QString("%1: %2").arg(logtime.elapsed(), 5, 10, QChar('0')).arg(msg);
    //QStringList appears not to be threadsafe
    strlock.lock();
    buffer.append(tmp);
    strlock.unlock();
}

void LogThread::appendClean(QString msg)
{
    strlock.lock();
    buffer.append(msg);
    strlock.unlock();
}


void LogThread::quit() {
    qDebug() << "Shutting down logging thread";
    running = false;
    strlock.lock();
    while (!buffer.isEmpty()) {
        QString msg = buffer.takeFirst();
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    }
    strlock.unlock();
}


void LogThread::run()
{
    running = true;
    do {
        strlock.lock();
        //int r = receivers(SIGNAL(outputLog(QString())));
        while (!buffer.isEmpty()) {
            QString msg = buffer.takeFirst();
                fprintf(stderr, "%s\n", msg.toLocal8Bit().data());
                emit outputLog(msg);
        }
        strlock.unlock();
        QThread::msleep(1000);
    } while (running);
}


