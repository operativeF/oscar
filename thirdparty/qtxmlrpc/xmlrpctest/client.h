#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QVariant>


class Client : QObject
{
      Q_OBJECT

public:
      Client( const QString &address, quint16 port, QObject *parent = 0 );

      void testFunc( const QVariant &param );

private slots:
      void onDataReady( const QVariant &response );

private:
      QString address;
      quint16 port;

};

#endif // CLIENT_H
