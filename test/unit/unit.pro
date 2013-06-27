TEMPLATE = app
TARGET = testrunner

QT       += testlib v8 qml quick
QT       -= gui

CONFIG   += console
CONFIG   -= app_bundle

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += testrunner.cpp

OTHER_FILES += \
    ../cases/*.js \
    ../cases/*.qml

include(../../src/src.pri)
include(../../qml-privates.pri)

