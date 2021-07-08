QT += core gui network sql serialport widgets websockets

#message(QMAKESPEC $$QMAKESPEC)
#message(PKG_CONFIG $$PKG_CONFIG)

TARGET = softrx
TEMPLATE = app

SOURCES += main.cpp \
        mainwindow.cpp \
        serial.cpp \
        delegates.cpp \

HEADERS += mainwindow.hpp \
        serial.hpp \
        defines.hpp \
        delegates.hpp \
        cron.hpp \

FORMS += mainwindow.ui \

unix|win32-g++ {
    CONFIG += link_pkgconfig
    PKGCONFIG += hamlib
    QMAKE_CXXFLAGS += -O2 -Wall
}

