#include <QCoreApplication>
#include "server.h"
#include "client.h"

int main( int argc, char **argv )
{
      QCoreApplication app( argc, argv );

      QString address = "127.0.0.1";
      quint16 port = 8080;

      Server s( address, port );
      Client c( address, port  );

      /* NB: Responses are not syncronized */

      c.testFunc( QVariant( "test" ) );
      c.testFunc( QVariant( 1 ) );
      c.testFunc( QVariant( true ) );
      c.testFunc( QVariant( 1.5 ) );

      QVariantMap m;
      m["one"] = 1;
      m["two"] = 2;
      m["three"] = 3;
      c.testFunc( QVariant( m ) );

      return app.exec();
}

