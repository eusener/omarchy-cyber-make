QT += core gui qml quick quickcontrols2 concurrent
CONFIG += c++17 release
TARGET = cyber-make-studio

HEADERS += src/profile.h src/desktop.h src/backend.h src/desktoptheme.h
SOURCES += src/main.cpp src/profile.cpp src/desktop.cpp src/backend.cpp src/desktoptheme.cpp
RESOURCES += src/resources.qrc
