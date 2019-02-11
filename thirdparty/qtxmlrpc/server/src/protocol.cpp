#include "protocol.h"

QString ClientProtocol::unconnectedNoDstHost = QString("socket unconnected, no dstHost");

ClientProtocol::ClientProtocol( QTcpSocket *parent,
                                const QString & _dstHost,
                                const quint16 _dstPort )
 : QObject(parent), socket( parent ),
   dstHost( _dstHost ), dstPort( _dstPort ),
   protocolRetry( 0 ), maxProtocolRetries( 3 ), protocolStarted( false ),
   disconnectFromSocketOnDoneFlag( false )
{
      Q_ASSERT( socket );

      // check, if no dstHost, socket must be in the ConnectedState
      if( dstHost.isEmpty() &&
          socket->state() != QAbstractSocket::ConnectedState ) {
            qCritical() << this << "constructor, dstHost.isEmpty(), but socket state"
                        << socket->state();
            qFatal( "you must check socket state, before creating protocol" );
      }
      connectTimeoutTimer = new QTimer( this );
      connectTimeoutTimer->setSingleShot( true );
      connect( connectTimeoutTimer, SIGNAL( timeout() ),
               SLOT( slotSocketConnectTimeout() ) );
      reconnectSleepTimer = new QTimer( this );
      reconnectSleepTimer->setSingleShot( true );
      connect( reconnectSleepTimer, SIGNAL( timeout() ),
               SLOT( deferredStart() ) );

      // connect socket signals to my slots
      connect( socket, SIGNAL( error(QAbstractSocket::SocketError) ),
               SLOT( slotSocketError(QAbstractSocket::SocketError) ) );
      connect( socket, SIGNAL( stateChanged(QAbstractSocket::SocketState) ),
               SLOT( slotSocketStateChanged(QAbstractSocket::SocketState) ) );
}


void ClientProtocol::slotSocketStateChanged ( QAbstractSocket::SocketState socketState )
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "slotSocketStateChanged(" << socketState << ")";
#endif
      // Не слишком ли много мы ходили в protocolStart?
      if( protocolRetry >= maxProtocolRetries ) {
            emitError( "maxProtocolRetries reached" );
            return;
      }

      switch( socketState ) {
      case QAbstractSocket::ConnectedState:
      // уже соединены, запускаем протокол
#ifdef DEBUG_PROTOCOL
            qDebug() << this << "slotSocketStateChanged((): connected!"
                        << "protocolRetry" << protocolRetry;
#endif
            protocolStart();
            break;
      case QAbstractSocket::UnconnectedState:
            if( protocolStarted )
                  protocolStop();

            // can reconnect?
            if( dstHost.size() ) {
#ifdef DEBUG_PROTOCOL
                  qDebug() << this
                           << "slotSocketStateChanged(): starting reconnect timer...";
#endif
                  reconnectSleepTimer->start( reconnectSleep );
            }
            else
                  emitError( unconnectedNoDstHost );
            break;
      default:
            ; // do nothing
      }
}


void ClientProtocol::slotSocketError( QAbstractSocket::SocketError socketError )
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "slotSocketError(" << socketError << ")";
#endif
      switch( socketError ) {
      case QAbstractSocket::ConnectionRefusedError:
      case QAbstractSocket::SocketTimeoutError:
      case QAbstractSocket::RemoteHostClosedError:
            // ну чо поделать, как только socketState
            // поменяется в UnconnectedState, начнется новый deferredStart
            break;
      case QAbstractSocket::HostNotFoundError:
            qWarning() << this << "slotSocketError(): host not found"
                       << dstHost << ":" << dstPort;
            emitError( QString("host not found: ") +
                       dstHost + ":" + QString::number(dstPort) );
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
            qCritical() << this << "slotSocketError(): bad socket error, aborting"
                        << socketError;
            emitError( "bad socket error" );
            break;
      default:
            qCritical() << this << "slotSocketError(): unknown socket error"
                        << socketError;
            emitError( "Unknown socket error" );
      }
}


void ClientProtocol::slotSocketConnectTimeout()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "slotSocketConnectTimeout()";
#endif
      emitError( "socket connect timeout" );
}


void ClientProtocol::deferredStart()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "deferredStart()";
#endif
      // Не слишком ли много мы ходили в protocolStart?
      if( protocolRetry >= maxProtocolRetries ) {
            emitError( "maxProtocolRetries reached" );
            return;
      }

      // Не запущен timeoutTimer?
      if( ! connectTimeoutTimer->isActive() )
            connectTimeoutTimer->start( connectTimeout );

      // запущен reconnectSleepTimer?
      if( reconnectSleepTimer->isActive() )
            reconnectSleepTimer->stop();

      switch( socket->state() ) {
            case QAbstractSocket::UnconnectedState:
                  // еще никуда не коннектится, начнем :)
#ifdef DEBUG_PROTOCOL
                  qDebug() << this << "deferredStart(): socket->connectToHost("
                           << dstHost << "," << dstPort << ")";
#endif
                  if( dstHost.size() )
                        connectSocket();
                  else
                        emitError( "dstHost is empty and socket unconnected" );
                  break;
            case QAbstractSocket::ConnectedState:
            // уже соединены, запускаем протокол
#ifdef DEBUG_PROTOCOL
                  qDebug() << this << "deferredStart(): connected! protocolRetry"
                           << protocolRetry;
#endif
                  protocolStart();
                  break;
            default:
                  ; // do nothing
      }
}


void ClientProtocol::connectSocket()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocol::connectSocket()";
#endif

      Q_ASSERT( dstHost.size() );
      socket->connectToHost( dstHost, dstPort );
}


#ifdef DEBUG_PROTOCOL
void ClientProtocol::slotBytesWritten ( qint64 bytes )
{
      qDebug() << this << "ClientProtocol::slotBytesWritten(" << bytes << ")";
#else
void ClientProtocol::slotBytesWritten ( qint64 )
{
#endif
}


void ClientProtocol::slotReadyRead()
{
      // default implementation just read all data
      QByteArray data = socket->readAll();
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocol::slotReadyRead(): " << data;
#endif
}


void ClientProtocol::emitError( const QString & errTxt )
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "emitError(" << errTxt << ")";
#endif

      if( protocolStarted )
            protocolStop();
      else
            stopTimers();

      // disconnect all signals to me
      socket->disconnect( this );
      socket->abort();

      emit error( errTxt );
}


void ClientProtocol::emitDone()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "emitDone()";
#endif

      if( protocolStarted )
            protocolStop();
      else
            stopTimers();

      if( disconnectFromSocketOnDoneFlag )
            socket->disconnect( this );

      emit done();
}


void ClientProtocol::sureWrite( const QByteArray & response )
{
      qint64 len = response.size();
      const char * ptr = response.data();
      while( len ) {
            qint64 res = socket->write( ptr, len );
            if ( res < 0 )
                  break;
            len -= res;
            ptr += res;
      }
      socket->flush();
}


void ClientProtocol::stopTimers()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocol::stopTimers()";
#endif
      // stop my timers
      if( connectTimeoutTimer->isActive() )
            connectTimeoutTimer->stop();

      if( reconnectSleepTimer->isActive() )
            reconnectSleepTimer->stop();
}


void ClientProtocol::protocolStop()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocol::protocolStop()";
#endif
      protocolStarted = false;
      // disconnect socket data signals from protocol handlers
      disconnect( socket, SIGNAL( readyRead() ),
                  this, SLOT( slotReadyRead() ) );
      disconnect( socket, SIGNAL( bytesWritten(qint64) ),
                  this, SLOT( slotBytesWritten(qint64) ) );
}


void ClientProtocol::protocolStart()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocol::protocolStart()";
#endif
      stopTimers();

      protocolRetry++;
      protocolStarted = true;
      // connect socket data signals to protocol handlers
      connect( socket, SIGNAL( readyRead() ),
               this, SLOT( slotReadyRead() ) );
      connect( socket, SIGNAL( bytesWritten(qint64) ),
               this, SLOT( slotBytesWritten(qint64) ) );

      // noProxyMode?
      if( !qstrcmp(metaObject()->className(), "ClientProtocol") )
            emitDone();
}


ClientProtocolWithTimeout::ClientProtocolWithTimeout( QTcpSocket * parent,
                                                      const int _protocolTimeout,
                                                      const QString & dstHost,
                                                      const quint16 dstPort )
  : ClientProtocol( parent, dstHost, dstPort ),
    protocolTimeout( _protocolTimeout )
{
      protocolTimeoutTimer = new QTimer( this );
      protocolTimeoutTimer->setSingleShot( true );
      connect( protocolTimeoutTimer, SIGNAL( timeout() ),
               SLOT( slotProtocolTimeout() ) );
}


void ClientProtocolWithTimeout::protocolStop()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocolWithTimeout::protocolStop():"
               << "stop protocol timeout timer";
#endif
      ClientProtocol::protocolStop();

      if( protocolTimeoutTimer->isActive() )
            protocolTimeoutTimer->stop();
}


void ClientProtocolWithTimeout::protocolStart()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocolWithTimeout::protocolStart():"
               << "start protocol timeout timer"
               << protocolTimeout / 1000 << "secs";
#endif
      ClientProtocol::protocolStart();

      protocolTimeoutTimer->start( protocolTimeout );
}


void ClientProtocolWithTimeout::slotProtocolTimeout()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocolWithTimeout::slotProtocolTimeout()";
#endif
      if( !objectName().length() ) {
            qCritical() << this << "ClientProtocolWithTimeout::slotProtocolTimeout():"
                        << "protocol name empty!"
                        << "Please setObjectName() for your protocols!";
            qFatal( "programming error" );
      }
      emitError( objectName() + " protocol timeout" );
}


#ifdef DEBUG_PROTOCOL
void ClientProtocolWithTimeout::slotBytesWritten( qint64 bytes )
{
      qDebug() << this << "ClientProtocolWithTimeout::slotBytesWritten("
            << bytes << "): restart protocol timeout timer"
            << protocolTimeout / 1000 << "secs";
#else
void ClientProtocolWithTimeout::slotBytesWritten( qint64 )
{
#endif
      Q_ASSERT( protocolStarted );
      Q_ASSERT( protocolTimeoutTimer->isActive() );

      // restart timeout timer.
      protocolTimeoutTimer->start( protocolTimeout );
}


void ClientProtocolWithTimeout::slotReadyRead()
{
#ifdef DEBUG_PROTOCOL
      qDebug() << this << "ClientProtocolWithTimeout::slotReadyRead():"
               << "restart protocol timeout timer"
               << protocolTimeout / 1000 << "secs";
#endif
      Q_ASSERT( protocolStarted );
      Q_ASSERT( protocolTimeoutTimer->isActive() );

      // restart timeout timer.
      protocolTimeoutTimer->start( protocolTimeout );
}
