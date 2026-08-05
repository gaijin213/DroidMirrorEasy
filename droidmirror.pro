QT += core gui widgets dbus
CONFIG += c++17
TARGET = droidmirror-easy
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/adbmanager.cpp \
    src/mirrorlauncher.cpp \
    src/qrpairer.cpp \
    src/notificationwatcher.cpp

HEADERS += \
    src/mainwindow.h \
    src/adbmanager.h \
    src/mirrorlauncher.h \
    src/qrpairer.h \
    src/notificationwatcher.h

DEFINES += QT_DEPRECATED_WARNINGS
