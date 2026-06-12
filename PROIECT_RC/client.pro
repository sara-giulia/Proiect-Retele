QT += core gui widgets
TARGET = client
TEMPLATE = app

DESTDIR = bin

INCLUDEPATH += headers

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/client.c

HEADERS += headers/mainwindow.h \
           headers/postswindow.h \
           headers/profilewindow.h \
           headers/adminwindow.h \
           headers/conversationswindow.h \
           headers/chatwindow.h

LIBS += -lpthread
