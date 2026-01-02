QT += core gui widgets network

CONFIG += c++17

SOURCES += \
    chatserver.cpp \
    clientworker.cpp \
    main.cpp \
    serverwindow.cpp

HEADERS += \
    chatserver.h \
    clientworker.h \
    serverwindow.h

FORMS += \
    serverwindow.ui

INCLUDEPATH += $$PWD/../common

