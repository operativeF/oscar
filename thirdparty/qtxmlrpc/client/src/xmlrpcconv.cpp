#include <QDomDocument>
#include <QMetaMethod>
#include <QTimerEvent>
#include <QStringList>
#include <QDateTime>
#include <QDebug>

#include "xmlrpcconv.h"

#define XMLRPC_WITHSPACES

// QVariant to xml conversions
// use QByteArray & reference, becouse it is faster, then return QByteArray
static void toXmlRpcValue( const int spaces, const QVariant & child, QByteArray & b );
static void toXmlRpcStruct( const int spaces, const QVariantMap & child, QByteArray & b );
static void toXmlRpcArray( const int spaces, const QVariantList & child, QByteArray & b );

// xml to QVariant conversions
static QVariant parseXmlRpcValue( const QDomElement & e, QString& err );
static QVariant parseXmlRpcStruct( const QDomElement & e, QString& err );
static QVariant parseXmlRpcArray( const QDomElement & e, QString& err );


QVariant fromXmlRpcResponse( const QString d, QString &err )
{
      QVariant res = QVariant::Invalid;

      QDomDocument doc;
      if ( doc.setContent( d, &err ) ) {

            QDomElement methodResponse = doc.firstChildElement("methodResponse");
            if ( methodResponse.isNull() )
                  err = "Element 'methodResponse' is absent in response";
            else {            
                  QDomElement result = methodResponse.firstChildElement();
                  if ( result.isNull() ) {
                        err = "Element 'params' is absent in response";
                        return QVariant::Invalid;
                  }
                  else if ( result.tagName() == "params" ) {
                        QDomElement param = result.firstChild().firstChild().toElement();
                        res = parseXmlRpcValue( param, err );
                  }
                  else {
                        err = result.text();
                  }
            }
      }
      return res;
}

QByteArray toXmlRpcRequest( const QString m, const QList<QVariant>& l )
{
#ifdef XMLRPC_WITHSPACES
      QByteArray r( "<?xml version=\"1.0\"?>\n<methodCall>" );
#else
      QByteArray r( "<?xml version=\"1.0\"?><methodCall>" );
#endif

#ifdef XMLRPC_WITHSPACES
      r.append( "\n  <methodName>" );
      r.append( m );
      r.append( "</methodName>" );
#else
      r.append( "<methodName>" );
      r.append( m );
      r.append( "</methodName>" );
#endif

#ifdef XMLRPC_WITHSPACES
      r.append( "\n    <params>" );
#else
      r.append( "<params>" );
#endif
      for ( int i = 0; i < l.size(); i++ )
      {
#ifdef XMLRPC_WITHSPACES
            r.append( "\n      <param>" );
#else
            r.append( "<param>" );
#endif      
            toXmlRpcValue( 6, l.at(i), r );
#ifdef XMLRPC_WITHSPACES
            r.append( "\n      </param>" );
#else
            r.append( "</param>" );
#endif            
      }
      
#ifdef XMLRPC_WITHSPACES
      r.append( "\n    </params>" );
#else
      r.append( "</params>" );
#endif  

#ifdef XMLRPC_WITHSPACES
      r.append( "\n</methodCall>" );
#else
      r.append( "</methodCall>" );
#endif

      return r;
}

static void toXmlRpcArray( const int spaces, const QVariantList & child, QByteArray & b )
{
#ifdef DEBUG_XMLRPC
      qDebug() << "toXmlRpcArray()";
#endif

      QListIterator< QVariant > i(child);

      while( i.hasNext() )
            toXmlRpcValue( spaces + 2, i.next(), b );
}


static void toXmlRpcStruct( const int spaces, const QVariantMap & child, QByteArray & b )
{
#ifdef DEBUG_XMLRPC
      qDebug() << "toXmlRpcStruct()";
#endif

      QMapIterator< QString, QVariant > i(child);

      while( i.hasNext() ) {
            i.next();
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "<member>" );
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces + 2, ' ') );
#endif
            b.append( "<name>" + i.key() + "</name>" );
            toXmlRpcValue( spaces + 2, i.value(), b );
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "</member>" );
      }
}


static void toXmlRpcValue( const int spaces, const QVariant & child, QByteArray & b )
{
#ifdef DEBUG_XMLRPC
      qDebug() << "toXmlRpcValue()";
#endif
      switch( child.type() ) {
      case QVariant::Int:
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "<value><int>" + QString::number(child.toInt()) + "</int></value>" );
            break;
      case QVariant::Bool:
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( QString("<value><boolean>") + (child.toBool() ? "1" : "0") + "</boolean></value>" );
            break;
      case QVariant::String:
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "<value><string>" + child.toString() + "</string></value>" );
            break;
      case QVariant::Double:
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "<value><double>" + QString::number(child.toDouble()) + "</double></value>" );
            break;
      case QVariant::DateTime:
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "<value><dateTime.iso8601>" + child.toDateTime().toString("yyyyMMddTHH:mm:ss") +
                      "</dateTime.iso8601></value>" );
            break;
      case QVariant::ByteArray:
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "<value><base64>" + child.toByteArray().toBase64() + "</base64></value>" );
            break;
      case QVariant::List:
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "<array><data>" );
            toXmlRpcArray( spaces + 2, child.toList(), b );
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "</data></array>" );
            break;
      case QVariant::Map:
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "<value><struct>" );
            toXmlRpcStruct( spaces + 2, child.toMap(), b );
#ifdef XMLRPC_WITHSPACES
            b.append( '\n' );
            b.append( QByteArray(spaces, ' ') );
#endif
            b.append( "</struct></value>" );
            break;
      default:
            qCritical() << "toXmlRpcValue(): unknown return xmlrpc type"
                        << child.typeName() << endl << child;
            qFatal("programming error");
      }
}

static QVariant parseXmlRpcValue( const QDomElement & e, QString& err )
{
#ifdef DEBUG_XMLRPC
      qDebug() << "parseXmlRpcValue():" << e.tagName();
#endif
      QVariant v = QVariant::Invalid;

      QString tagName = e.tagName();
      if( tagName != "value" ) {
            err = "first param tag is not value";
            return v;
      }

      QDomElement t = e.firstChild().toElement();
      QString type = t.tagName();
      if( type == "int" || type == "i4" ) {
            bool ok;
            v = t.firstChild().toText().data().toInt( &ok );
            if( !ok )
                  err = "Can't convert int text '" + t.firstChild().toText().data() + "' to number";
      }
      else if( type == "boolean" )
            v = t.firstChild().toText().data() == "1" ? true : false;
      else if( type == "string" )
            v = t.firstChild().toText().data();
      else if( type == "double" ) {
            bool ok;
            v = t.firstChild().toText().data().toDouble( &ok );
            if( !ok )
                  err = "Can't convert int text '" + t.firstChild().toText().data() + "' to number";
      }
      else if( type == "dateTime.iso8601" )
            v = QDateTime::fromString( t.firstChild().toText().data(), "yyyyMMddTHH:mm:ss" );
      else if( type == "base64" )
            v = QByteArray::fromBase64( t.firstChild().toText().data().toLatin1() );
      else if ( type == "array" )
            v = parseXmlRpcArray( t.firstChild().toElement(), err );
      else if ( type == "struct" )
            v = parseXmlRpcStruct( t.firstChild().toElement() , err );
      else if ( type.length() == 0 )
            v = e.toElement().firstChild().toText().data();
      else
            err = "unknown type: '" + type + "'";

      return v;
}


static QVariant parseXmlRpcStruct( const QDomElement &e, QString &err )
{
#ifdef DEBUG_XMLRPC
      qDebug() << "parseXmlRpcStruct():" << e.tagName();
#endif
      QVariantMap r;

      QDomElement t = e;

      while( !t.isNull() ) {
            if( t.tagName() != "member" ) {
                  err = "no member tag in struct, tag " + e.tagName();
                  return r;
            }
            QDomElement s = t.firstChild().toElement();
            if( s.tagName() != "name" ) {
                  err = "no name tag in member struct, tag " + s.tagName();
                  return r;
            }
            // set map value
            r[ s.firstChild().toText().data() ] = parseXmlRpcValue( s.nextSibling().toElement(), err );

            if( !err.isEmpty() )
                  return r;

            t = t.nextSibling().toElement();
      }

      return r;
}


static QVariant parseXmlRpcArray( const QDomElement &e, QString &err )
{
#ifdef DEBUG_XMLRPC
      qDebug() << "parseXmlRpcArray():" << e.tagName();
#endif
      QVariantList r;
      if( e.tagName() != "data" ) {
            err = "no data tag in array, tag " + e.tagName();
            return r;
      }

      QDomElement t = e.firstChild().toElement();

      while( !t.isNull() ) {
            r.append( parseXmlRpcValue(t,err) );
            if( !err.isEmpty() )
                  return r;

            t = t.nextSibling().toElement();
      }

      return r;
}

