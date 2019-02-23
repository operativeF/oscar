#ifndef LOGGER_H
#define LOGGER_H

#include <QDebug>
#include <QRunnable>
#include <QThreadPool>
#include <QMutex>
#include <QTime>

void initializeLogger();
void shutdownLogger();

void MyOutputHandler(QtMsgType type, const QMessageLogContext &context, const QString &msgtxt);

class LogThread:public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit LogThread() : QRunnable() { running = false; logtime.start(); }
    virtual ~LogThread() {}

    void run();
    void append(QString msg);
    void appendClean(QString msg);
    bool isRunning() { return running; }

    void quit();

    QStringList buffer;
    QMutex strlock;
    QThreadPool *threadpool;
signals:
    void outputLog(QString);
protected:
    volatile bool running;
    QTime logtime;
};

extern LogThread * logger;
extern QThreadPool * otherThreadPool;

#endif // LOGGER_H
