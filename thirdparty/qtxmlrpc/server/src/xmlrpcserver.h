#ifndef XMLRPCSERVER_H
#define XMLRPCSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QList>
#include <QString>
#include <QVariant>
#include <QHttpHeader>

// ssl
#include <QSslCertificate>
#include <QSslKey>

// #define DEBUG_XMLRPC

// use spaces in xmlrpc formatting
#define XMLRPC_WITHSPACES

/*
 * Types from http://www.xmlrpc.com/spec maps into QVariant as:
 *
 * int - int,
 * boolean - bool,
 * string - QString,
 * double - double,
 * dateTime.iso8601 - QDateTime,
 * base64 - QByteArray
 * array - QVariantList
 * struct - QVariantMap
 *
 * Fault method response generated, if return value is
 * QVariantMap with two fields: faultCode, faultString.
 *
 */

// creates xmlrpc fault
// Use this in your callbacks, as return createFault( code, msg );
// For example, in python, xmlrpclib.Fault exception will be raised.
inline QVariant createFault( const int code, const QString & msg )
{
      QVariantMap f;

      f["faultCode"] = code;
      f["faultString"] = msg;

      return f;
}

/*
 * Protocol is base class for Network protocols.
 * This implementation implement timeout, if not data sent/received.
 *
 * Protocol shoud be created as QAbstractSocket child,
 * if you delete socket, protocol will be deleted automaticly
 *
 * Socket can have only one protocol at one time.
 *
 * Protocol have no start/stop policy, it start when construct and stops
 * when destruct.
 */
class Protocol  : public QObject
{
      Q_OBJECT
public:
      // Timeout activates, if it is above zero, msecs.
      Protocol( QAbstractSocket * parent, const int _timeout = 0 );
      ~Protocol();

      void setTimeout( const int _timeout );
      void stopProtocolTimeout() { setTimeout(0); }

      QAbstractSocket * getSocket() { return socket; }
private slots:
      // restarts timeout
      void __slotReadyRead();
      void __slotBytesWritten( qint64 bytes );
signals:
      void protocolTimeout( Protocol * );
protected:
      void timerEvent( QTimerEvent *event );
      void restartProtocolTimeoutTimer();

      int timeout; // таймаут протокола.
      int timeoutTimerId;

      // Just not to cast parent() every time you need socket
      QAbstractSocket * socket;
};


// very basic HttpServer for XmlRpc
class QHttpRequestHeader;
class HttpServer : public Protocol {
      Q_OBJECT
public:
      HttpServer( QAbstractSocket * parent );
public slots:
      // sends xmlrpc response from QVariant param
      void slotSendReply( const QByteArray& );
      void slotSendReply( const QVariant& );
protected slots:
      void slotReadyRead();
      void slotBytesWritten( qint64 bytes );
signals:
      void parseError( HttpServer * );
      void requestReceived(  HttpServer *, const QHttpRequestHeader & h, const QByteArray & body );
      void replySent( HttpServer * );

private:
      bool readRequestHeader();
      bool readRequestBody();
      bool requestContainsBody();
      enum State { ReadingHeader, ReadingBody, WaitingReply, SendingReply, Done } state;

      QString requestHeaderBody;
      QByteArray requestBody;
      QHttpRequestHeader requestHeader;

      qint64 bytesWritten;
      qint64 bytesToWrite;

      const static int defaultTimeout = 100000; // 100 secs
};


// you can deriver from this class and implement deferredRun
// After deferred sql, network and so, you can emit sendReply signal with result.
//
// This signal will be connected to your HttpServer slotSendReply.
//
// DeferredResult will be deleted, as socket child, or you can this->deleteLater(),
// after emmiting result.
class DeferredResult : public QObject
{
      Q_OBJECT
public:
      DeferredResult();
      ~DeferredResult();
signals:
      void sendReply( const QVariant& );
      void sendReply( const QByteArray& );
};


class DeferredEcho : public DeferredResult
{
      Q_OBJECT
public:
      DeferredEcho( const QVariant& e );
private:
      void timerEvent( QTimerEvent * );
      QVariant echo;
};


class SslParams : public QObject
{
      Q_OBJECT
public:
      SslParams( const QString & certFile, const QString & keyFile, QObject * parent = 0 );

      QSslCertificate certificate;
      QList<QSslCertificate> ca;
      QSslKey privateKey;
};


/**
        @author kisel2626@gmail.com
*/
class QDomElement;
class XmlRpcServer : public QTcpServer
{
      Q_OBJECT
public:
      XmlRpcServer( QObject *parent = 0,
                    const QString & cert = "",
                    const QString & key = "",
                    const QByteArray & passwd = QByteArray() );

      // Slots should return QVariant and accept const QVariant& as params.
      // QVariant method( const QVariant &, ... );
      // path + slot should be unique.
      void registerSlot( QObject * receiver, const char * slot, QByteArray path = "/RPC2/" );
public slots:
      QVariant echo( const QVariant& e ) { return e; }
      DeferredResult * deferredEcho( const QVariant& e ) { return new DeferredEcho( e ); }
private slots:
      // we don't support SSL yet,
      // if you need SSL, connection handling design must be revised a bit
      // void slotNewConnection();

      // we don't need socket more, if it's disconnected
      void slotSocketDisconnected();
      // protocol errors
      void slotProtocolTimeout( Protocol * );
      void slotParseError( HttpServer * );
      // when HttpServer receives request
      void slotRequestReceived(  HttpServer *, const QHttpRequestHeader & h, const QByteArray & body );
      // when reply sent
      void slotReplySent( HttpServer * );

      // when registrant dies, we need to remove their callbacks методов.
      void slotReceiverDestroed( QObject * );

protected:
      void incomingConnection ( int socketDescriptor );

private:
      // if Method is deffered
      typedef QPair< QObject *, bool > IsMethodDeffered;
      // fast access for invokeMethod
      typedef QHash< QByteArray, IsMethodDeffered > Callbacks;
      // fast callbacks delete, when registrant dies
      typedef QList< QByteArray > Methods;
      typedef QMap< QObject *, Methods > ObjectMethods;

      Callbacks callbacks;
      ObjectMethods objectMethods;

      SslParams * sslParams;
};

#endif
