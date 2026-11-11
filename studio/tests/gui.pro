QT += core gui qml quick quickcontrols2 concurrent testlib
CONFIG += c++17 testcase
TARGET = cyber-make-studio-gui-test

HEADERS += ../src/profile.h ../src/desktop.h ../src/backend.h ../src/desktoptheme.h
SOURCES += gui.cpp ../src/profile.cpp ../src/desktop.cpp ../src/backend.cpp ../src/desktoptheme.cpp
RESOURCES += ../src/resources.qrc
