QT += gui waylandcompositor

HEADERS += \
    compositor.h \
    view.h \
    window.h \
#    xwayland.h \
#    xwm.h

SOURCES += main.cpp \
    compositor.cpp \
    view.cpp \
    window.cpp \
#    xwayland.cpp \
#    xwm.cpp

TARGET = newcompositor.bin

target.path = /opt/newcompositor/bin

INSTALLS += target
