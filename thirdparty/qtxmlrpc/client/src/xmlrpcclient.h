#ifndef XMLRPCCLIENT_H
#define XMLRPCCLIENT_H

#include "httpclient.h"
#include <QObject>
#include <QString>
#include <QVariant>
#include <QByteArray>
#include <QVariantList>

class XmlRpcClient : public HttpClient
{
        Q_OBJECT
public:
        XmlRpcClient( const QString &host,
                      const quint16 port );

        void execute( const QString &method,
                      const QVariantList &params );

signals:
        void dataReady( const QVariant &data );

private slots:
        void onDataReady( const QByteArray &data );
};

#endif // XMLRPCCLIENT_H
