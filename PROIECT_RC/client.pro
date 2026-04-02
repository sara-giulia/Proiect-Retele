QT += core gui widgets
TARGET = client
TEMPLATE = app
INCLUDEPATH += headers
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    client.c
HEADERS += mainwindow.h \
           postswindow.h \
           profilewindow.h \
           adminwindow.h \
           conversationswindow.h \
           chatwindow.h
LIBS += -lpthread