#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QObject>
#include <QString>
#include <QTimer>

#include <QAbstractSocket>

// #define DEBUG_PROTOCOL

class Client_ : public QObject
{
        Q_OBJECT
public:
        Client_( const QString &dstHost, const quint16 dstPort );
        ~Client_();

public slots:
        void deferredStart();

protected slots:
        void onSocketStateChanged( QAbstractSocket::SocketState socketState );
        void onSocketError( QAbstractSocket::SocketError socketError );
        void onConnectTimeout();

        virtual void onBytesWritten( qint64 bytes );
        virtual void onReadyRead();

signals:
        void error( const QString &errTxt );
        void done();

protected:
        QAbstractSocket *socket;

        QString dstHost;
        quint16 dstPort;

        int protocolRetry;
        int maxProtocolRetries;
        bool protocolStarted;

        virtual QAbstractSocket *buildSocket();
        virtual void connectSocket();
        virtual void protocolStart();
        virtual void protocolStop();

        void sureWrite( const QByteArray &response );
        void emitError( const QString &error );
        void emitDone();

private:
        void stopTimers();

        QTimer * connectTimeoutTimer;
        QTimer * reconnectSleepTimer;

        static const int connectTimeout = 30000; // msec
        static const int reconnectSleep = 1000; // msec
};

#endif // PROTOCOL_H
