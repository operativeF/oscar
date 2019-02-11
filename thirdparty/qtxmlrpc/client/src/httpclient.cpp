#include "httpclient.h"


HttpClient::HttpClient( const QString &host, const quint16 port, const QString &path, const HttpMethod method )
                : Client_( host, port ), httpState( Waiting ), method( method )
{
#ifdef HTTP_CLIENT_DEBUG
        qDebug() << this << "HttpClient(" << host << "," << (method==GET?"GET":"POST") << ")";
#endif
        url.setScheme( "http" );
        url.setHost( host );
        url.setPort( port );
        url.setPath( path );
        connect( this, SIGNAL( done() ), SLOT( onProtocolDone() ) );
}

HttpClient::~HttpClient() {}


void HttpClient::onReadyRead()
{
        switch( httpState ) {
        case Sending:
#ifdef HTTP_CLIENT_DEBUG
                qDebug() << this << "onReadyRead(), Sending";
#endif
                httpState = ReadingResponseHeader;
        case ReadingResponseHeader:
#ifdef HTTP_CLIENT_DEBUG
                qDebug() << this << "onReadyRead(), ReadingResponseHeader";
#endif
                // если не дочитан
                if( !readResponseHeader() )
                        break;
                if( responseHeader.statusCode() == 100 ) {
                        // Continue
                        // это нам говорят продолжай слать пост, игнорируем,
                        // опять будем читать хидер
                        break;
                }
                else if ( responseHeader.statusCode() == 302 ) {
                        // Moved temporary
                        if ( responseHeader.hasKey( "Location" ) ) {
                                QString location = responseHeader.value( "Location" );
                                if ( location.at( 0 ) == '/' )
                                        url.setPath( location );
                                else
                                        url.setUrl( location );
                                method = GET;
                        }
                        break;
                }
                httpState = ReadingResponseBody;
        case ReadingResponseBody:
#ifdef HTTP_CLIENT_DEBUG
                qDebug() << this << "onReadyRead(), ReadingResponseBody";
#endif
                // если не дочитан
                if( !readResponseBody() )
                        break;
                emitDone();
                break;
        default:
                qCritical() << this << "onReadyRead(): unknown httpState";
                qFatal( "programming error" );
        }
}


void HttpClient::protocolStop()
{
        Client_::protocolStop();
        httpState = Waiting;
}

void HttpClient::protocolStart()
{
        Client_::protocolStart();

        Q_ASSERT( httpState == Waiting );
        QString path = url.path();
        if ( path.isEmpty() )
                path = "/";
        if ( method == GET && url.hasQuery() )
                path += "?" + url.encodedQuery();

        QHttpRequestHeader h( method==GET?"GET":"POST", path );
        h.setValue( "Host", QUrl::toAce( url.host() ) );
        h.setValue( "User-Agent", "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; SV1; .NET CLR 2.0.50727; .NET CLR 3.0.04506.30; .NET CLR 3.0.04506.648)" );
        h.setValue( "Accept", "*/*" );
        if ( !referer.isEmpty() )
                h.setValue( "Referer", referer );

        QList<QNetworkCookie> cookieList = cookieJar.cookiesForUrl( url );
        foreach ( QNetworkCookie nc, cookieList ) {
                QByteArray cookieNameVal;
                cookieNameVal.append( nc.name() );
                cookieNameVal.append( "=" );
                cookieNameVal.append( nc.value() );
                h.addValue( "Cookie", cookieNameVal );
        }

        if ( method == POST ) {
                h.setValue( "Content-Type", "application/x-www-form-urlencoded" );
                h.setValue( "Content-Length", QString::number( postData.length() ) );
        }

        QByteArray requestHeader( h.toString().toLatin1() );
        if ( method == POST )
                requestHeader.append( postData );

#ifdef TRACE_HTTP
        qDebug() << "--- request header ---" << endl << requestHeader;
#endif

        sureWrite( requestHeader );

        httpState = Sending;
        responseHeaderData.clear();
        responseBodyData.clear();
}


bool HttpClient::readResponseHeader()
{
#ifdef HTTP_CLIENT_DEBUG
        qDebug() << this << "HttpClient::readResponseHeader()";
#endif
        bool end = false;
        QByteArray tmp;
        QByteArray rn( "\r\n", 2 ), n( "\n",1 );
        while ( !end && socket->canReadLine() ) {
                tmp = socket->readLine();
                if ( tmp == rn || tmp == n || tmp.isEmpty() )
                        end = true;
                else
                        responseHeaderData += tmp;
        }

        if( !end )
                return false;
        responseHeader = QHttpResponseHeader( QString(responseHeaderData) );
#ifdef TRACE_HTTP
        qDebug() << "--- response header ---" << endl << responseHeader.toString();
#endif
        if( responseHeader.isValid() ) {
                if ( responseHeader.hasKey( "Set-Cookie" ) ) {
                        QByteArray cba;
                        QList<QPair<QString, QString> > keys = responseHeader.values();
                        QPair<QString, QString> p;
                        foreach( p, keys ) {
                                if ( p.first == "Set-Cookie" ) {
                                        cba.append( p.second );
                                        cba.append( "\n" );
                                }
                        }
                        QList<QNetworkCookie> cookieList = QNetworkCookie::parseCookies( cba );
                        cookieJar.setCookiesFromUrl( cookieList, url );
                }
                return true;
        }

        qWarning() << this << "readResponseHeader(): invalid responseHeader";
        emitError( "Invalid response header" );
        return false;
}

bool HttpClient::readResponseBody()
{
        if ( responseHeader.hasContentLength() )
                return readContentLength();
        else if ( responseHeader.value( "Connection" ) == "close" ) {
                responseBodyData += socket->readAll();
                return false;
        }
        else {
                QByteArray data = socket->readAll();
                qCritical() << this << "XmlRpcClient::readResponseBody():"
                                << "unknown content read method" << endl
                                << responseHeader.toString() << endl;
                emitError( "Unknown content read method" );
        }

        return false;
}

bool HttpClient::readContentLength()
{
        qint64 l = (qint64)responseHeader.contentLength();
        qint64 n = qMin( socket->bytesAvailable(), l - responseBodyData.size() );
        QByteArray readed( socket->read(n) );
        responseBodyData += readed;
#ifdef HTTP_CLIENT_DEBUG
        qDebug() << this << "readContentLength(), left:" << (l - responseBodyData.length() );
#endif
        return responseBodyData.length() == l;
}


void HttpClient::onProtocolDone()
{
#ifdef HTTP_CLIENT_DEBUG
        qDebug() << this << "onProtocolDone()";
#endif
        emit dataRecieved();
        emit dataReady( responseBodyData );
}

