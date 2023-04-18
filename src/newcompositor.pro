QT += dbus gui waylandcompositor

HEADERS += \
    compositor.h \
    dbuscontainerstate.h \
    view.h \
    window.h

SOURCES += main.cpp \
    compositor.cpp \
    dbuscontainerstate.cpp \
    view.cpp \
    window.cpp

TARGET = newcompositor.bin

target.path = /usr/bin

INSTALLS += target

xwayland {
    message(Xwayland support enabled)

    DEFINES += XWAYLAND

    HEADERS += \
        xwayland.h \
        xwm.h \
        xwmwindow.h

    SOURCES += \
        xwayland.cpp \
        xwm.cpp \
        xwmwindow.cpp

    CONFIG += link_pkgconfig
    PKGCONFIG += wayland-server xcb xcb-composite
} else {
    message(Xwayland support disabled)
}
