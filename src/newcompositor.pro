QT += gui waylandcompositor

HEADERS += \
    compositor.h \
    view.h \
    window.h \
    xwayland.h \
    xwm.h

SOURCES += main.cpp \
    compositor.cpp \
    view.cpp \
    window.cpp \
    xwayland.cpp \
    xwm.cpp

TARGET = newcompositor.bin

target.path = /usr/bin

INSTALLS += target

CONFIG += link_pkgconfig
PKGCONFIG += wayland-server xcb xcb-composite
