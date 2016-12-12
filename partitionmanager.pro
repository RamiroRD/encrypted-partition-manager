

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = partmgr
TEMPLATE = app

INCLUDEPATH += include

SOURCES += src/main.cpp \
    src/gui/PartitionManagerWindow.cpp \
    src/gui/PartitionManagerAdapter.cpp \
    src/gui/CreateDialog.cpp \
	src/logic/PartitionManager.cpp \
    src/logic/Crypto.cpp


HEADERS  += \
    include/gui/PartitionManagerWindow.h \
    include/gui/PartitionManagerAdapter.h \
    include/gui/CreateDialog.h \
	include/logic/PartitionManager.h \
    include/logic/Crypto.h

LIBS += -lssl -lcrypto

TRANSLATIONS = partmgr_es.ts


