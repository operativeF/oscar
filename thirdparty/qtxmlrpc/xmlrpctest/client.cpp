#include "client.h"
#include "xmlrpcclient.h"

#include <QVariantList>

Client::Client( const QString &address, quint16 port, QObject *parent )
            : QObject( parent ), address( address ), port( port )
{

}


void Client::testFunc( const QVariant &param )
{
      XmlRpcClient *client = new XmlRpcClient( address, port );
      connect( client, SIGNAL( dataReady(QVariant) ), this, SLOT( onDataReady(QVariant) ) );
      client->execute( "testFunc", QVariantList() << param );
}


void Client::onDataReady( const QVariant &response )
{
      qDebug() << response;
}
