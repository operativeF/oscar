#include "server.h"
#include "xmlrpcserver.h"

#include <QHostAddress>

Server::Server( const QString &address, quint16 port, QObject *parent )
      : QObject( parent )
{
      XmlRpcServer *srv = new XmlRpcServer;
      if ( srv->listen( QHostAddress( address ), port ) ) {
            srv->registerSlot( this, SLOT( testFunc(QVariant) ) );
      }
}


QVariant Server::testFunc( const QVariant &param )
{
      return param;
}
