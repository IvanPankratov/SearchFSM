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


SOURCES += SearchFsm/FsmCreator.cpp \
	Test/main.cpp \
	Test/FsmTest.cpp \
	Test/ShiftRegister.cpp

HEADERS += \
	SearchFsm/Common.h \
	SearchFsm/SearchFsm.h \
	SearchFsm/FsmCreator.h \
	Test/FsmTest.h \
	Test/SearchEngines.h \
	Test/ShiftRegister.h \
	Test/WinTimer.h \
	Test/Lcg.h

