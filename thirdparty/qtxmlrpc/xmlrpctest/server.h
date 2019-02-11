#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QVariant>

class Server : QObject
{
      Q_OBJECT

public:
      Server( const QString &address, quint16 port, QObject *parent = 0 );

private slots:
      QVariant testFunc( const QVariant &param );
};

#endif // SERVER_H
