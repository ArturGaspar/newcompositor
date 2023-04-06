#include "xwmwindow.h"

#include <QObject>
#include <QString>
#include <QWaylandSurface>
#include <xcb/xcb.h>

#include "xwm.h"

XwmWindow::XwmWindow(Xwm *xwm, xcb_window_t window)
    : QObject(xwm)
    , m_xwm(xwm)
    , m_window(window)
{
}

void XwmWindow::setSurface(QWaylandSurface *surface)
{
    QWaylandSurface *previousSurface = m_surface;
    if (previousSurface) {
        disconnect(previousSurface, nullptr, this, nullptr);
    }
    m_surface = surface;
    if (surface) {
        connect(surface, &QObject::destroyed,
                this, &XwmWindow::onSurfaceDestroyed,
                Qt::DirectConnection);
        emit m_xwm->windowBoundToSurface(this, previousSurface);
        maybeSetPopup();
    }
}

void XwmWindow::onSurfaceDestroyed()
{
    // The window stays alive and may be bound to another surface later,
    // but current listeners expect a surface, so disconnect them until they
    // connect themselves back on next Xwm::windowBoundToSurface().
    disconnect(this, nullptr, nullptr, nullptr);
    m_surface = nullptr;
}

void XwmWindow::setMapped(bool mapped)
{
    m_mapped = mapped;
    if (m_surface) {
        emit m_surface->hasContentChanged();
    }
}

void XwmWindow::maybeSetPopup()
{
    if (m_overrideRedirect && m_transientFor) {
        QWaylandSurface *surface = m_xwm->surfaceForWindow(m_transientFor);
        emit setPopup(surface, m_position);
    }
}

void XwmWindow::setOverrideRedirect(bool overrideRedirect)
{
    m_overrideRedirect = overrideRedirect;
    maybeSetPopup();
}

void XwmWindow::setTransientFor(xcb_window_t transientFor)
{
    m_transientFor = transientFor;
    maybeSetPopup();
}

void XwmWindow::setPosition(const QPoint &pos)
{
    m_position = pos;
    emit positionChanged(pos);
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

void XwmWindow::setFocus()
{
    m_xwm->setFocusWindow(m_window);
}
