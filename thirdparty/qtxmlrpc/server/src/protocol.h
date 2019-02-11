#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtDebug>
#include <QObject>
#include <QTimer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>

// #define DEBUG_PROTOCOL

class QTcpSocket;
class ClientProtocol : public QObject
{
Q_OBJECT
public:
      // parent - должен быть ребенком QTcpSocket,
      // от и есть Socket, с которой работаем. Если кто-то удаляет
      // Socket, она удалит нас как детей, таким образом протоколы не будут
      // работать с удаленной Socket
      //
      // если _dstHost == QString::null, то для работы протокола необходим
      // сконнекченный сокет. Иначе выбрасываем error.
      ClientProtocol( QTcpSocket * parent,
                      const QString & _dstHost = QString::null,
                      const quint16 _dstPort = 0 );
#ifdef DEBUG_PROTOCOL
      ~ClientProtocol()
      {
            qDebug() << this << "destructor";
      }
#endif

      void disconnectFromSocketOnDone() { disconnectFromSocketOnDoneFlag = true; }
      QTcpSocket * getSocket() { return socket; }
      static QString unconnectedNoDstHost;
public slots:
      // if socket connected, call protocolStart(), slotReadyRead() if bytes available
      // otherwise connects and do same thing.
      void deferredStart();
protected slots:
      void slotSocketStateChanged ( QAbstractSocket::SocketState socketState );
      void slotSocketError( QAbstractSocket::SocketError socketError );

      // to be overloaded in child protocol, no need to call this implementations
      virtual void slotBytesWritten ( qint64 bytes );
      virtual void slotReadyRead();
      void slotSocketConnectTimeout();
signals:
      void error( const QString& errTxt );
      void done();
protected:
      // socket == parent, сделано чтобы не кастить каждый раз парента
      QTcpSocket * socket;
      QString dstHost;
      quint16 dstPort;
      int protocolRetry;
      int maxProtocolRetries;
      bool protocolStarted;
      bool disconnectFromSocketOnDoneFlag;

      // Cыны обязаны вызвать родителей первой строкой.
      virtual void protocolStop();
      // Может вызываться maxProtocolRetries раз, если в процессе
      // работы происходит Disconnect, либо 1 раз, если дали сконнекченный socket
      virtual void protocolStart();
      // если дают QSSLSocket, можно перекрыть
      // обычное поведение тут (родителя вызывать уже не нужно)
      // основная задача socket->connectToHost..
      virtual void connectSocket();
      // stop() and emit error
      void emitError( const QString & errTxt );
      // disconnect my slots, stop timers, emit done()
      void emitDone();
      // emitError, if can't write a bit
      void sureWrite( const QByteArray & response );
private:
      void stopTimers();

      QTimer * connectTimeoutTimer;
      QTimer * reconnectSleepTimer;

      static const int connectTimeout = 30000; // msec
      static const int reconnectSleep = 1000; // msec
};


// Протокол, который выпадает в emitError("protocol timeout"),
// если работает дольше, чем указали при конструировании
class ClientProtocolWithTimeout : public ClientProtocol
{
Q_OBJECT
public:
      ClientProtocolWithTimeout( QTcpSocket * parent,
                                 const int _protocolTimeout,
                                 const QString & dstHost = QString::null,
                                 const quint16 dstPort = 0 );
#ifdef DEBUG_PROTOCOL
      ~ClientProtocolWithTimeout()
      {
            qDebug() << this << "destructor";
      }
#endif
protected slots:
      void slotProtocolTimeout();
      // restart timer, if protocolStated
      void slotBytesWritten ( qint64 bytes );
      // restart timer, if protocolStated
      void slotReadyRead();

protected:
      void protocolStop();
      void protocolStart();

      int protocolTimeout;
      QTimer * protocolTimeoutTimer;
};


#endif
