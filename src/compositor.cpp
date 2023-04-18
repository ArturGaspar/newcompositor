/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Wayland module
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "compositor.h"

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QSysInfo>
#include <QVector>
#include <QWaylandOutput>
#include <QWaylandSeat>
#include <QWaylandSurface>
#include <QWaylandWlShell>
#include <QWaylandWlShellSurface>
#include <QWaylandXdgPopup>
#include <QWaylandXdgShell>
#include <QWaylandXdgToplevel>
#include <QWaylandXdgDecorationManagerV1>
#include <QWindow>

#include "dbuscontainerstate.h"
#include "view.h"
#include "window.h"
#ifdef XWAYLAND
#include "xwayland.h"
#include "xwm.h"
#include "xwmwindow.h"
#endif

Compositor::Compositor()
    : m_dbusContainerState(new DBusContainerState(this))
    , m_wlShell(new QWaylandWlShell(this))
    , m_xdgShell(new QWaylandXdgShell(this))
    , m_xdgDecorationManager(new QWaylandXdgDecorationManagerV1)
#ifdef XWAYLAND
    , m_xwayland(new Xwayland(this))
    , m_xwm(new Xwm(this, m_xwayland))
#endif
{
    m_xdgDecorationManager->setParent(this);
}

Compositor::~Compositor()
{
}

void Compositor::create()
{
    connect(this, &QWaylandCompositor::outputAdded,
            this, &Compositor::onOutputAdded);

    connect(this, &QWaylandCompositor::surfaceCreated,
            this, &Compositor::onSurfaceCreated);
    connect(this, &QWaylandCompositor::subsurfaceChanged,
            this, &Compositor::onSubsurfaceChanged);

    m_wlShell->initialize();
    connect(m_wlShell, &QWaylandWlShell::wlShellSurfaceCreated,
            this, &Compositor::onWlShellSurfaceCreated);

    m_xdgShell->initialize();
    connect(m_xdgShell, &QWaylandXdgShell::toplevelCreated,
            this, &Compositor::onXdgToplevelCreated);
    connect(m_xdgShell, &QWaylandXdgShell::popupCreated,
            this, &Compositor::onXdgPopupCreated);

    m_xdgDecorationManager->initialize();
    m_xdgDecorationManager->setPreferredMode(QWaylandXdgToplevel::ServerSideDecoration);

    // Some clients, e.g. Xwayland in rootful mode, expect to know output
    // size before they create any surfaces.
//    auto *output = new QWaylandOutput(this, nullptr);
//    QScreen *screen = qApp->primaryScreen();
//    output->setManufacturer(screen->manufacturer());
//    output->setModel(screen->model());
//    output->setPhysicalSize(screen->physicalSize().toSize());
//    QWaylandOutputMode mode(screen->size(), screen->refreshRate() * 1000);
//    output->addMode(mode, true);
//    output->setCurrentMode(mode);

    QWaylandCompositor::create();

    qInfo("Compositor running on WAYLAND_DISPLAY=%s", socketName().constData());

#ifdef XWAYLAND
    connect(m_xwm, &Xwm::windowBoundToSurface,
            this, &Compositor::onXwmWindowBoundToSurface);
    m_xwayland->start();
#endif
}

void Compositor::onSurfaceCreated(QWaylandSurface *surface)
{
    new View(this, surface);
    connect(surface, &QWaylandSurface::hasContentChanged,
            this, &Compositor::surfaceHasContentChanged);
    connect(surface, &QWaylandSurface::redraw,
            this, &Compositor::onSurfaceRedraw);
    connect(surface, &QWaylandSurface::subsurfacePositionChanged,
            this, &Compositor::onSubsurfacePositionChanged);
    emit surfaceReady(surface);
}

bool Compositor::surfaceHasContent(QWaylandSurface *surface) const
{
    bool hasContent = surface->hasContent();
    if (hasContent) {
        auto *view = qobject_cast<View *>(surface->primaryView());
        Q_ASSERT(view);
        if (view->m_hide) {
            hasContent = false;
#ifdef XWAYLAND
        } else if (view->m_xwmWindow) {
            hasContent = view->m_xwmWindow->isMapped();
#endif
        }
    }
    return hasContent;
}

void Compositor::surfaceHasContentChanged()
{
    auto *surface = qobject_cast<QWaylandSurface *>(sender());
    auto *view = qobject_cast<View *>(surface->primaryView());
    if (!view) {
        return;
    }
    if (surfaceHasContent(surface) && !surface->isCursorSurface()) {
        auto *view = qobject_cast<View *>(surface->primaryView());
        Q_ASSERT(view);
        Window *window = ensureWindowForView(view);
        if (window->isActive()) {
            setFocusSurface(surface);
        }
    }
    triggerRender(surface);
}

void Compositor::onSurfaceRedraw()
{
    auto *surface = qobject_cast<QWaylandSurface *>(sender());
    triggerRender(surface);
}

void Compositor::onSubsurfacePositionChanged(const QPoint &position)
{
    auto *surface = qobject_cast<QWaylandSurface *>(sender());
    if (!surface) {
        return;
    }
    Q_ASSERT(surface->primaryView());
    auto *view = qobject_cast<View *>(surface->primaryView());
    Q_ASSERT(view);
    view->m_position = position;
    triggerRender(surface);
}

void Compositor::onSubsurfaceChanged(QWaylandSurface *child,
                                     QWaylandSurface *parent)
{
    auto *view = qobject_cast<View *>(child->primaryView());
    Q_ASSERT(view);
    auto *parentView = qobject_cast<View *>(parent->primaryView());
    Q_ASSERT(parentView);
    view->m_parentView = parentView;
}

void Compositor::setFocusSurface(QWaylandSurface *surface)
{
    if (surface == nullptr) {
        defaultSeat()->setMouseFocus(nullptr);
        defaultSeat()->setKeyboardFocus(nullptr);
        return;
    }
    if (surface->role() == QWaylandXdgPopup::role()) {
        return;
    }
    defaultSeat()->setMouseFocus(surface->primaryView());
    defaultSeat()->setKeyboardFocus(surface);
#ifdef XWAYLAND
    auto *view = qobject_cast<View *>(surface->primaryView());
    Q_ASSERT(view);
    if (view->m_xwmWindow) {
        view->m_xwmWindow->raise();
        view->m_xwmWindow->setFocus();
    }
#endif
}

Window *Compositor::createWindow(View *view)
{
    auto *window = new Window(this);
    connect(window, &Window::rotationChanged,
            m_dbusContainerState, &DBusContainerState::onWindowRotationChanged);

    auto *output = new QWaylandOutput(this, window);
    output->setParent(window);

    QScreen *screen = window->screen();
    output->setManufacturer(screen->manufacturer());
    output->setModel(screen->model());
    output->setPhysicalSize(screen->physicalSize().toSize());

    view->setOutput(output);
    window->addView(view);

    connect(output, &QWaylandOutput::geometryChanged,
            view, &View::onOutputGeometryChanged);

    return window;
}

void Compositor::onOutputAdded(QWaylandOutput *output)
{
    QWindow *window = output->window();
    if (window) {
        if (QSysInfo::productType() == "sailfishos") {
            window->showFullScreen();
        } else {
            window->resize(360, 640);
            window->show();
        }
        m_showAgainWindow = qobject_cast<Window *>(window);
    }
}

Window *Compositor::showAgainWindow()
{
    return m_showAgainWindow.data();
}

void Compositor::setShowAgainWindow(Window *window)
{
    m_showAgainWindow = window;
}

Window *Compositor::ensureWindowForView(View *view)
{
    Window *window;
    if (view->output()) {
        Q_ASSERT(view->output()->window());
        window = qobject_cast<Window *>(view->output()->window());
        Q_ASSERT(window);
    } else if (view->m_parentView) {
        window = ensureWindowForView(view->m_parentView);
        QWaylandOutput *output = outputFor(window);
        Q_ASSERT(output);
        view->setOutput(output);
        window->addView(view);
    } else {
        window = createWindow(view);
    }
    return window;
}

void Compositor::onWlShellSurfaceCreated(QWaylandWlShellSurface *wlShellSurface)
{
    connect(wlShellSurface, &QWaylandWlShellSurface::setTransient,
            this, &Compositor::onWlShellSurfaceSetTransient);
    connect(wlShellSurface, &QWaylandWlShellSurface::setPopup,
            this, &Compositor::onWlShellSurfaceSetPopup);
    auto *view = qobject_cast<View *>(wlShellSurface->surface()->primaryView());
    Q_ASSERT(view);
    view->m_wlShellSurface = wlShellSurface;
}

void Compositor::onWlShellSurfaceSetTransient(QWaylandSurface *parentSurface,
                                              const QPoint &relativeToParent,
                                              bool inactive)
{
    Q_UNUSED(inactive);
    auto *wlShellSurface = qobject_cast<QWaylandWlShellSurface *>(sender());
    auto *view = qobject_cast<View *>(wlShellSurface->surface()->primaryView());
    Q_ASSERT(view);
    auto *parentView = qobject_cast<View *>(parentSurface->primaryView());
    Q_ASSERT(parentView);
    view->m_parentView = parentView;
    view->m_position = parentView->position() + relativeToParent;
}

void Compositor::onWlShellSurfaceSetPopup(QWaylandSeat *seat,
                                          QWaylandSurface *parentSurface,
                                          const QPoint &relativeToParent)
{
    Q_UNUSED(seat);
    auto *wlShellSurface = qobject_cast<QWaylandWlShellSurface *>(sender());
    auto *view = qobject_cast<View *>(wlShellSurface->surface()->primaryView());
    Q_ASSERT(view);
    auto *parentView = qobject_cast<View *>(parentSurface->primaryView());
    Q_ASSERT(parentView);
    view->m_parentView = parentView;
    view->m_position = parentView->position() + relativeToParent;
}

void Compositor::onXdgToplevelCreated(QWaylandXdgToplevel *toplevel,
                                      QWaylandXdgSurface *xdgSurface)
{
    auto *view = qobject_cast<View *>(xdgSurface->surface()->primaryView());
    Q_ASSERT(view);
    view->m_xdgToplevel = toplevel;
}

void Compositor::onXdgPopupCreated(QWaylandXdgPopup *popup,
                                   QWaylandXdgSurface *xdgSurface)
{
    auto *view = qobject_cast<View *>(xdgSurface->surface()->primaryView());
    Q_ASSERT(view);
    auto *parentView = qobject_cast<View *>(popup->parentXdgSurface()->surface()->primaryView());
    Q_ASSERT(parentView);
    view->m_parentView = parentView;
    view->m_position = (parentView->position() +
                        popup->anchorRect().topLeft() +
                        popup->offset());
    view->m_xdgPopup = popup;
}

#ifdef XWAYLAND
void Compositor::onXwmWindowBoundToSurface(XwmWindow *xwmWindow,
                                           QWaylandSurface *previousSurface)
{
    if (previousSurface) {
        auto *previousView = qobject_cast<View *>(previousSurface->primaryView());
        if (previousView) {
            previousView->m_xwmWindow = nullptr;
            // Xwayland will eventually hide and destroy the surface.
            previousView->m_hide = true;
        }
    }
    auto *view = qobject_cast<View *>(xwmWindow->surface()->primaryView());
    Q_ASSERT(view);
    view->m_xwmWindow = xwmWindow;
    connect(xwmWindow, &QObject::destroyed,
            view, [view] { view->m_xwmWindow = nullptr; },
            Qt::DirectConnection);
    connect(xwmWindow, &XwmWindow::positionChanged,
            this, &Compositor::onXwmWindowPositionChanged);
    connect(xwmWindow, &XwmWindow::setPopup,
            this, &Compositor::onXwmWindowSetPopup);
}

void Compositor::onXwmWindowPositionChanged(const QPoint &pos)
{
    auto *xwmWindow = qobject_cast<XwmWindow *>(sender());
    auto *view = qobject_cast<View *>(xwmWindow->surface()->primaryView());
    Q_ASSERT(view);
    if (view->m_parentView) {
        view->m_position = pos;
    }
}

void Compositor::onXwmWindowSetPopup(QWaylandSurface *parentSurface,
                                     const QPoint &pos)
{
    auto *xwmWindow = qobject_cast<XwmWindow *>(sender());
    auto *view = qobject_cast<View *>(xwmWindow->surface()->primaryView());
    Q_ASSERT(view);
    View *parentView = nullptr;
    if (parentSurface) {
        parentView = qobject_cast<View *>(parentSurface->primaryView());
        Q_ASSERT(parentView);
    } else {
        // If parent surface is unknown, make it a popup of the active window.
        auto *window = qobject_cast<Window *>(QGuiApplication::focusWindow());
        if (window) {
            const QVector<View *> views = window->views();
            if (!views.isEmpty()) {
                parentView = views.first();
            }
        }
    }
    if (parentView) {
        view->m_parentView = parentView;
        view->m_position = pos;
    }
}
#endif // XWAYLAND

void Compositor::triggerRender(QWaylandSurface *surface)
{
    if (surface->primaryView() && surface->primaryView()->output()) {
        Q_ASSERT(surface->primaryView()->output()->window());
        surface->primaryView()->output()->window()->requestUpdate();
    }
}
