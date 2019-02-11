include(../server/server.pri)
include(../client/client.pri)
INCLUDEPATH += ../server/src
INCLUDEPATH += ../client/src
SOURCES += main.cpp server.cpp client.cpp
HEADERS += server.h client.h
