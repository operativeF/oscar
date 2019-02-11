#ifndef XMLRPCCONV_H
#define XMLRPCCONV_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QList>
#include <QString>
#include <QVariant>
#include <QHttpHeader>
#include <QDomDocument>
#include <QMetaMethod>
#include <QTimerEvent>
#include <QStringList>
#include <QDateTime>

QByteArray toXmlRpcRequest( const QString m, const QList<QVariant>& l );
QVariant fromXmlRpcResponse( const QString d, QString &err );

#endif

