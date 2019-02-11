#include "clientprotocol.h"
#include <QTcpSocket>


Client_::Client_( const QString &dstHost, const quint16 dstPort )
                : dstHost( dstHost ), dstPort( dstPort ),
                protocolRetry( 0 ), maxProtocolRetries( 10 ), protocolStarted( false )
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "Protocol(...)";
#endif
        connectTimeoutTimer = new QTimer( this );
        connectTimeoutTimer->setSingleShot( true );
        connect( connectTimeoutTimer, SIGNAL( timeout() ), SLOT( onConnectTimeout() ) );

        reconnectSleepTimer = new QTimer( this );
        reconnectSleepTimer->setSingleShot( true );
        connect( reconnectSleepTimer, SIGNAL( timeout() ), SLOT( deferredStart() ) );
}


Client_::~Client_()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "~Protocol()";
#endif
}


QAbstractSocket *Client_::buildSocket()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "buildSocket()";
#endif
        return new QTcpSocket;
}


void Client_::deferredStart()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "deferredStart()";
#endif
        if ( protocolRetry == 0 ) {
                socket = buildSocket();
                connect( socket, SIGNAL( error(QAbstractSocket::SocketError) ),
                         SLOT( onSocketError(QAbstractSocket::SocketError) ) );
                connect( socket, SIGNAL( stateChanged(QAbstractSocket::SocketState) ),
                         SLOT( onSocketStateChanged(QAbstractSocket::SocketState) ) );
                setParent( socket );
        }

        if ( protocolRetry >= maxProtocolRetries ) {
                emitError( "Maximum protocol retries has reached" );
                return;
        }

        if ( !connectTimeoutTimer->isActive() )
                connectTimeoutTimer->start( connectTimeout );

        if ( reconnectSleepTimer->isActive() )
                reconnectSleepTimer->stop();

        switch ( socket->state() ) {
        case QAbstractSocket::UnconnectedState: connectSocket(); break;
        case QAbstractSocket::ConnectedState: protocolStart(); break;
        default:;
        }
}


void Client_::connectSocket()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "connectSocket()";
#endif
        socket->connectToHost( dstHost, dstPort );
}


void Client_::onConnectTimeout()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "onConnectTimeout()";
#endif
        emitError( "Connect timeout" );
}


void Client_::onSocketStateChanged( QAbstractSocket::SocketState socketState )
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "onSocketStateChanged(" << socketState << ")";
#endif
        if( protocolRetry >= maxProtocolRetries ) {
                emitError( "maxProtocolRetries reached" );
                return;
        }

        switch( socketState ) {
        case QAbstractSocket::ConnectedState:
                if( !protocolStarted )
                        protocolStart();
                break;
        case QAbstractSocket::UnconnectedState:
                if( protocolStarted )
                        protocolStop();
                reconnectSleepTimer->start( reconnectSleep );
                break;
        default:;
        }
}


void Client_::onSocketError( QAbstractSocket::SocketError socketError )
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "onSocketError(" << socketError << ")";
#endif
        switch( socketError ) {
        case QAbstractSocket::ConnectionRefusedError:
        case QAbstractSocket::SocketTimeoutError:
        case QAbstractSocket::RemoteHostClosedError:
                break;
        case QAbstractSocket::HostNotFoundError:
                qWarning() << this << "onSocketError(): host not found" << dstHost << ":" << dstPort;
                emitError( QString("Host not found: ") + dstHost + ":" + QString::number(dstPort) );
                break;
        case QAbstractSocket::SocketAccessError:
        case QAbstractSocket::SocketResourceError:
        case QAbstractSocket::DatagramTooLargeError:
        case QAbstractSocket::AddressInUseError:
        case QAbstractSocket::NetworkError:
        case QAbstractSocket::SocketAddressNotAvailableError:
        case QAbstractSocket::UnsupportedSocketOperationError:
        case QAbstractSocket::ProxyAuthenticationRequiredError:
        case QAbstractSocket::UnknownSocketError:
                qCritical() << this << "onSocketError(): bad socket error, aborting" << socketError;
                emitError( "Bad socket error" );
                break;
        default:
                qCritical() << this << "onSocketError(): unknown socket error" << socketError;
                emitError( "Unknown socket error" );
        }
}


void Client_::protocolStart()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "protocolStart()";
#endif
        stopTimers();

        protocolRetry++;
        protocolStarted = true;

        connect( socket, SIGNAL( readyRead() ), this, SLOT( onReadyRead() ) );
        connect( socket, SIGNAL( bytesWritten(qint64) ), this, SLOT( onBytesWritten(qint64) ) );
}


void Client_::protocolStop()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "protocolStop()";
#endif
        protocolStarted = false;
        disconnect( socket, SIGNAL( readyRead() ), this, SLOT( onReadyRead() ) );
        disconnect( socket, SIGNAL( bytesWritten(qint64) ), this, SLOT( onBytesWritten(qint64) ) );
}


void Client_::onBytesWritten( qint64 /*bytes*/ )
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "onBytesWritten(" << bytes << ")";
#endif
}


void Client_::onReadyRead()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "onReadyRead()";
#endif
        QByteArray data = socket->readAll();
#ifdef DEBUG_PROTOCOL
        qDebug() << data;
#endif
}


void Client_::emitError( const QString &errTxt )
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "emitError(...)";
#endif
        if ( protocolStarted )
                protocolStop();
        else
                stopTimers();

        socket->disconnect( this );
        socket->abort();

        emit error( errTxt );
}


void Client_::emitDone()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "emitDone()";
#endif
        if ( protocolStarted )
                protocolStop();
        else
                stopTimers();

        socket->disconnect( this );

        emit done();
}


void Client_::sureWrite( const QByteArray &response )
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "sureWrite(...)" << endl << response;
#endif
        qint64 len = response.size();
        const char * ptr = response.data();
        while ( len ) {
                qint64 res = socket->write( ptr, len );
                if ( res < 0 ) break;
                len -= res;
                ptr += res;
        }
        socket->flush();
}


void Client_::stopTimers()
{
#ifdef DEBUG_PROTOCOL
        qDebug() << this << "stopTimers()";
#endif
        if( connectTimeoutTimer->isActive() ) connectTimeoutTimer->stop();
        if( reconnectSleepTimer->isActive() ) reconnectSleepTimer->stop();
}
