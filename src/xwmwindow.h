#ifndef XWMWINDOW_H
#define XWMWINDOW_H

#include <QObject>
#include <QString>
#include <xcb/xcb.h>

QT_BEGIN_NAMESPACE

class QWaylandSurface;

class Xwm;

class XwmWindow : public QObject
{
    Q_OBJECT
public:
    XwmWindow(Xwm *xwm, QWaylandSurface *surface, xcb_window_t window);

    QWaylandSurface *surface() const;

    void resize(const QSize &size);
    void sendClose();
    void raise();

    bool isUnmapped() const;
    QString className() const;
    QString title() const;

    QPoint position() const;
    QWaylandSurface *parentSurface();

private:
    Xwm *m_xwm;
    QWaylandSurface *m_surface;
    xcb_window_t m_window;
};

QT_END_NAMESPACE

#endif // XWMWINDOW_H
