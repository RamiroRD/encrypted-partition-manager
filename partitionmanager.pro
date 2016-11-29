

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = partmgr
TEMPLATE = app

INCLUDEPATH += include

SOURCES += src/main.cpp \
    src/gui/PartitionManagerWindow.cpp \
    src/gui/PartitionManagerAdapter.cpp \
    src/gui/CreateDialog.cpp
    src/logic/PartitionManager.h

HEADERS  += \
    include/gui/PartitionManagerWindow.h \
    include/gui/PartitionManagerAdapter.h \
    include/gui/CreateDialog.h \
    include/logic/PartitionManager.h
