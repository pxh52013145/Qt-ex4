QT += core gui widgets network

CONFIG += c++17

SOURCES += \
    chatclient.cpp \
    clientwindow.cpp \
    main.cpp

HEADERS += \
    chatclient.h \
    clientwindow.h

FORMS += \
    clientwindow.ui

INCLUDEPATH += $$PWD/../common

