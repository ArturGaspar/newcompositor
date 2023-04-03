#include "xwmwindow.h"

#include <QObject>
#include <QString>
#include <QWaylandSurface>
#include <xcb/xcb.h>

#include "xwm.h"

XwmWindow::XwmWindow(Xwm *xwm, QWaylandSurface *surface, xcb_window_t window) :
    QObject(surface),
    m_xwm(xwm),
    m_surface(surface),
    m_window(window)
{
}

QWaylandSurface *XwmWindow::surface() const
{
    return m_surface;
}

void XwmWindow::resize(const QSize &size)
{
    m_xwm->resizeWindow(m_window, size);
}

void XwmWindow::sendClose()
{
    m_xwm->closeWindow(m_window);
}

void XwmWindow::raise()
{
    m_xwm->raiseWindow(m_window);
}

bool XwmWindow::isUnmapped() const
{
    return m_xwm->windowIsUnmapped(m_window);
}

QString XwmWindow::className() const
{
    return "";
}

QString XwmWindow::title() const
{
    return "";
}

QPoint XwmWindow::position() const
{
    return m_xwm->windowPosition(m_window);
}

QWaylandSurface *XwmWindow::parentSurface()
{
    return m_xwm->parentSurface(m_surface);
}
