QT *= network xml

SOURCES += $$PWD/src/clientprotocol.cpp \
    $$PWD/src/httpclient.cpp \
    $$PWD/src/httpsclient.cpp \
    $$PWD/src/xmlrpcconv.cpp \
    $$PWD/src/xmlrpcclient.cpp \

HEADERS += $$PWD/src/clientprotocol.h \
    $$PWD/src/httpclient.h \
    $$PWD/src/httpsclient.h \
    $$PWD/src/xmlrpcconv.h \
    $$PWD/src/xmlrpcclient.h

