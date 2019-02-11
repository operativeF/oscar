#include "httpsclient.h"


SslParams_::SslParams_( const QString & certFile, const QString & keyFile,
                      const QByteArray &passwd, QObject * parent )
: QObject( parent )
{
        if ( !certFile.isEmpty() ) {
                QFile fc( certFile, this );
                fc.open( QFile::ReadOnly );
                certificate = QSslCertificate( fc.readAll() );
                fc.close();
        }

        if ( !keyFile.isEmpty() ) {
                QFile fk( keyFile, this );
                fk.open( QFile::ReadOnly );
                privateKey = QSslKey( fk.readAll(), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, passwd );
                fk.close();
        }
}


HttpsClient::HttpsClient( const QString &host, const quint16 port, const QString &path,
                          const HttpClient::HttpMethod method,
                          const QString &cert, const QString &key, const QByteArray &passwd )
: HttpClient( host, port, path, method ), ssl( cert, key, passwd, this )
{
}


HttpsClient::~HttpsClient()
{
}


QAbstractSocket *HttpsClient::buildSocket()
{
        QSslSocket *socket = new QSslSocket;
        if ( !ssl.certificate.isNull() )
                socket->setLocalCertificate( ssl.certificate );
        if ( !ssl.privateKey.isNull() )
                socket->setPrivateKey( ssl.privateKey );

        connect( socket, SIGNAL( encrypted() ), this, SLOT( onEncryptedConnection() ) );
        connect( socket, SIGNAL( sslErrors(QList<QSslError>) ), this, SLOT( onSslErrors( QList<QSslError> ) ) );

        return socket;
}


void HttpsClient::onEncryptedConnection()
{
#ifdef HTTPS_CLIENT_DEBUG
        qDebug() << this << "HttpsClient::onEncryptedConnection()";
#endif
        protocolStart();
}


void HttpsClient::connectSocket()
{
        QSslSocket *socketEnc = qobject_cast<QSslSocket *>( socket );
        socketEnc->connectToHostEncrypted( dstHost, dstPort );
}


void HttpsClient::protocolStart()
{
        if ( qobject_cast<QSslSocket *>( socket )->isEncrypted() )
                HttpClient::protocolStart();
}


void HttpsClient::onSslErrors( const QList<QSslError> & )
{
        qobject_cast<QSslSocket *>( socket )->ignoreSslErrors();
}
