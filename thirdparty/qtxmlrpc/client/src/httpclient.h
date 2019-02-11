#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "clientprotocol.h"

#include <QUrl>
#include <QMap>
#include <QList>
#include <QByteArray>
#include <QHttpResponseHeader>

#include <QNetworkCookieJar>
#include <QNetworkCookie>

//#define HTTP_CLIENT_DEBUG
//#define TRACE_HTTP


class HttpClient : public Client_
{
        Q_OBJECT
public:
        enum HttpMethod { GET, POST };

        HttpClient( const QString &host, const quint16 port = 80, const QString &path = "/", const HttpMethod method = GET );
        ~HttpClient();

        inline void setPostData( const QByteArray &ba ) { postData = ba; }
        inline void setReferer( const QString &value ) { referer = value; }

signals:
        void dataRecieved();
        void dataReady( const QByteArray &data );

protected slots:
        void onReadyRead();
        void onProtocolDone();

protected:
        void protocolStop();
        void protocolStart();

private:
        enum HttpState { Waiting, Sending, ReadingResponseHeader, ReadingResponseBody } httpState;

        bool readResponseHeader();
        bool readResponseBody();
        bool readChunked();
        bool readContentLength();

        QUrl url;
        HttpMethod method;
        QByteArray postData;
        QString referer;
        QNetworkCookieJar cookieJar;

        QHttpResponseHeader responseHeader;
        QByteArray responseHeaderData;
        QByteArray responseBodyData;

        qint64 chunkedSize;

        static const int requestTimeout = 60000; // msecs
};

#endif // HTTPCLIENT_H
