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

#include "view.h"
#include "window.h"
#include "xwayland.h"
#include "xwm.h"

Compositor::Compositor() :
    m_wlShell(new QWaylandWlShell(this)),
    m_xdgShell(new QWaylandXdgShell(this)),
    m_xdgDecorationManager(new QWaylandXdgDecorationManagerV1),
    m_xwayland(new Xwayland(this)),
    m_xwm(new Xwm(this, m_xwayland))
{
    m_xdgDecorationManager->setParent(this);
}

Compositor::~Compositor()
{
}

void Compositor::create()
{
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

    connect(this, &QWaylandCompositor::outputAdded,
            this, &Compositor::onOutputAdded);

    connect(this, &QWaylandCompositor::surfaceCreated,
            this, &Compositor::onSurfaceCreated);
    connect(this, &QWaylandCompositor::subsurfaceChanged,
            this, &Compositor::onSubsurfaceChanged);

    // Some clients, e.g. Xwayland in rootful mode, expect to know output
    // size before they create any surfaces.
    auto *output = new QWaylandOutput(this, nullptr);
    QScreen *screen = qApp->primaryScreen();
    output->setAvailableGeometry(screen->geometry());
    QWaylandOutputMode mode(screen->size(), screen->refreshRate() * 1000);
    output->addMode(mode, true);
    output->setCurrentMode(mode);
    setDefaultOutput(output);

    QWaylandCompositor::create();

    qInfo("Compositor running on WAYLAND_DISPLAY=%s", socketName().constData());

    connect(m_xwm, &Xwm::windowPositionChanged,
            this, &Compositor::onXwmWindowPositionChanged);
    m_xwayland->start();
}

void Compositor::onSurfaceCreated(QWaylandSurface *surface)
{
    connect(surface, &QWaylandSurface::hasContentChanged,
            this, &Compositor::surfaceHasContentChanged);
    connect(surface, &QWaylandSurface::redraw,
            this, &Compositor::onSurfaceRedraw);
    connect(surface, &QWaylandSurface::subsurfacePositionChanged,
            this, &Compositor::onSubsurfacePositionChanged);

    View *view = new View(this, m_xwm);
    view->setSurface(surface);
    connect(surface, &QWaylandSurface::offsetForNextFrame,
            view, &View::onOffsetForNextFrame);
    connect(surface, &QWaylandSurface::surfaceDestroyed,
            view, &QObject::deleteLater);
}

bool Compositor::surfaceHasContent(QWaylandSurface *surface)
{
    bool hasContent = surface->hasContent();
    if (hasContent && m_xwm->isX11Window(surface)) {
        hasContent = !m_xwm->windowIsUnmapped(surface);
    }
    return hasContent;
}

void Compositor::surfaceHasContentChanged()
{
    auto *surface = qobject_cast<QWaylandSurface *>(sender());
    if (!surface->role() && !m_xwm->isX11Window(surface)) {
        return;
    }
    if (surface->primaryView()) {
        if (surface->hasContent() && !surface->isCursorSurface()) {
            if (surfaceIsFocusable(surface)) {
                defaultSeat()->setKeyboardFocus(surface);
            }
            auto *view = qobject_cast<View *>(surface->primaryView());
            ensureWindowForView(view);
        }
        triggerRender(surface);
    }
}

void Compositor::onSurfaceRedraw()
{
    auto *surface = qobject_cast<QWaylandSurface *>(sender());
    triggerRender(surface);
}

void Compositor::setFocus(QWaylandSurface *surface)
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
    if (m_xwm->isX11Window(surface)) {
        m_xwm->raiseWindow(surface);
    }
}

bool Compositor::surfaceIsFocusable(QWaylandSurface *surface)
{
    return (surface == nullptr ||
            surface->role() == QWaylandWlShellSurface::role() ||
            surface->role() == QWaylandXdgToplevel::role() ||
            m_xwm->isX11Window(surface));
}

void Compositor::onWindowEmpty()
{
    auto *window = qobject_cast<Window *>(sender());
    m_freeWindows.append(window);
}

Window *Compositor::createWindow(View *view)
{
    Window *window;
    if (m_freeWindows.isEmpty()) {
        window = new Window(this);
        connect(window, &Window::empty,
                this, &Compositor::onWindowEmpty);
    } else {
        window = m_freeWindows.takeLast();
    }
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
    } else {
        if (!view->parentView() && m_xwm->isX11Window(view->surface())) {
            QWaylandSurface *parentSurface = m_xwm->parentSurface(view->surface());
            if (parentSurface) {
                auto *parentView = qobject_cast<View *>(parentSurface->primaryView());
                Q_ASSERT(parentView);
                view->m_parentView = parentView;
                view->m_position = m_xwm->windowPosition(view->surface());
            }
        }
        if (view->parentView()) {
            window = ensureWindowForView(view->parentView());
            QWaylandOutput *output = outputFor(window);
            Q_ASSERT(output);
            view->setOutput(output);
            window->addView(view);
        } else {
            window = createWindow(view);
        }
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
    auto *wlShellSurface = qobject_cast<QWaylandWlShellSurface*>(sender());
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
    auto *wlShellSurface = qobject_cast<QWaylandWlShellSurface*>(sender());
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

void Compositor::onSubsurfaceChanged(QWaylandSurface *child,
                                     QWaylandSurface *parent)
{
    auto *view = qobject_cast<View *>(child->primaryView());
    Q_ASSERT(view);
    auto *parentView = qobject_cast<View *>(parent->primaryView());
    Q_ASSERT(parentView);
    view->m_parentView = parentView;
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

void Compositor::onXwmWindowPositionChanged(QWaylandSurface *surface)
{
    auto *view = qobject_cast<View *>(surface->primaryView());
    Q_ASSERT(view);
    view->m_position = m_xwm->windowPosition(view->surface());
    triggerRender(surface);
}

void Compositor::triggerRender(QWaylandSurface *surface)
{
    if (surface->primaryView() && surface->primaryView()->output()) {
        Q_ASSERT(surface->primaryView()->output()->window());
        surface->primaryView()->output()->window()->requestUpdate();
    }
}
