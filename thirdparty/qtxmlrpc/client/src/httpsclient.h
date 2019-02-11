#ifndef HTTPSCLIENT_H
#define HTTPSCLIENT_H

#include "httpclient.h"

#include <QAbstractSocket>
#include <QSslSocket>
#include <QSslCertificate>
#include <QFile>
#include <QSslKey>
#include <QObject>
#include <QSslError>

// #define HTTPS_CLIENT_DEBUG

class SslParams_ : public QObject
{
      Q_OBJECT

public:
      SslParams_( const QString & certFile = "", const QString & keyFile = "",
                 const QByteArray &passwd = QByteArray(), QObject * parent = 0 );

      QSslCertificate certificate;
      QSslKey privateKey;
};


class HttpsClient : public HttpClient
{
        Q_OBJECT
public:
        HttpsClient( const QString &host, const quint16 port = 443, const QString &path = "/",
                     const HttpClient::HttpMethod method = HttpClient::GET,
                     const QString &cert = "", const QString &key = "", const QByteArray &passwd = QByteArray() );
        ~HttpsClient();

protected:
        QAbstractSocket *buildSocket();
        void connectSocket();
        void protocolStart();

private:
        SslParams_ ssl;

private slots:
        void onEncryptedConnection();
        void onSslErrors(const QList<QSslError> &errors);
};

#endif // HTTPSCLIENT_H
