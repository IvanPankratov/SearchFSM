#-------------------------------------------------
#
# Project created by QtCreator 2016-05-09T11:50:07
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = FSM
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
	FsmCreator.cpp \
	FsmTest.cpp \
	ShiftRegister.cpp

HEADERS += \
	Common.h \
	SearchFsm.h \
	FsmCreator.h \
	FsmTest.h \
	SearchEngines.h \
	ShiftRegister.h \
	WinTimer.h
