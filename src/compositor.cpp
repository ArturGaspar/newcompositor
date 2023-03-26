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

#include <QEvent>
#include <QScreen>
#include <QSysInfo>
#include <QWaylandBufferRef>
#include <QWaylandOutput>
#include <QWaylandSeat>
#include <QWaylandSurface>
#include <QWaylandTouch>
#include <QWaylandWlShell>
#include <QWaylandWlShellSurface>
#include <QWaylandXdgPopup>
#include <QWaylandXdgShell>
#include <QWaylandXdgToplevel>
#include <QWaylandXdgDecorationManagerV1>

#include "window.h"

View::View(Compositor *compositor) :
    m_compositor(compositor)
{
}

QOpenGLTexture *View::getTexture()
{
    bool newContent = advance();
    QWaylandBufferRef buf = currentBuffer();
    if (newContent) {
        m_texture = buf.toOpenGLTexture();
        if (surface()) {
            switch (buf.origin()) {
            case QWaylandSurface::OriginTopLeft:
                m_origin = QOpenGLTextureBlitter::OriginTopLeft;
                break;
            case QWaylandSurface::OriginBottomLeft:
                m_origin = QOpenGLTextureBlitter::OriginBottomLeft;
                break;
            }
        }
    } else if (!buf.hasContent()) {
        m_texture = nullptr;
    }
    return m_texture;
}

QOpenGLTextureBlitter::Origin View::textureOrigin() const
{
    return m_origin;
}

void View::setParentView(View *parent) {
    Q_ASSERT(!m_parentView);
    Q_ASSERT(!output());
    m_parentView = parent;
}

void View::onOutputGeometryChanged()
{
    if (m_wlShellSurface) {
        m_wlShellSurface->sendConfigure(output()->geometry().size(),
                                        QWaylandWlShellSurface::NoneEdge);
    }
    if (m_xdgToplevel) {
        m_xdgToplevel->sendMaximized(output()->geometry().size());
    }
}

void View::updateMode()
{
    auto *window = qobject_cast<Window *>(output()->window());
    if (!window) {
        return;
    }

    QSize size = window->size();
    if (size.isEmpty()) {
        return;
    }

    int rotation = 0;
    int refreshRate;

    QScreen *screen = window->screen();
    if (screen) {
        Qt::ScreenOrientation sizeOrientation;
        switch (screen->orientation()) {
        case Qt::InvertedPortraitOrientation:
            sizeOrientation = Qt::PortraitOrientation;
            rotation += 180;
            break;
        case Qt::InvertedLandscapeOrientation:
            sizeOrientation = Qt::LandscapeOrientation;
            rotation += 180;
            break;
        default:
            sizeOrientation = screen->orientation();
        }
        if (screen->primaryOrientation() != sizeOrientation) {
            rotation += 90;
            size.transpose();
        }
        refreshRate = screen->refreshRate() * 1000;
    } else {
        refreshRate = 60 * 1000;
    }

    window->setRotation(static_cast<Window::Rotation>(rotation));

    QWaylandOutputMode mode(size, refreshRate);
    output()->addMode(mode, false);
    output()->setCurrentMode(mode);
}

void View::onScreenOrientationChanged(Qt::ScreenOrientation orientation)
{
    Q_UNUSED(orientation);
    updateMode();
}

void View::onWindowSizeChanged()
{
    updateMode();
}

void View::onOffsetForNextFrame(const QPoint &offset)
{
    m_offset = offset;
    setPosition(position() + offset);
}

void View::sendClose()
{
    if (m_xdgToplevel) {
        m_xdgToplevel->sendClose();
    } else if (m_xdgPopup) {
        m_xdgPopup->sendPopupDone();
    } else if (surface()) {
        m_compositor->destroyClientForSurface(surface());
    }
}

QString View::appId() const
{
    if (m_xdgToplevel) {
        return m_xdgToplevel->appId();
    }
    if (m_wlShellSurface) {
        return m_wlShellSurface->className();
    }
    return QString();
}

QString View::title() const
{
    if (m_xdgToplevel) {
        return m_xdgToplevel->title();
    }
    if (m_wlShellSurface) {
        return m_wlShellSurface->title();
    }
    return QString();
}


Compositor::Compositor() :
    m_wlShell(new QWaylandWlShell(this)),
    m_xdgShell(new QWaylandXdgShell(this)),
    m_xdgDecorationManager(new QWaylandXdgDecorationManagerV1)
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

    connect(this, &QWaylandCompositor::surfaceCreated,
            this, &Compositor::onSurfaceCreated);
    connect(this, &QWaylandCompositor::subsurfaceChanged,
            this, &Compositor::onSubsurfaceChanged);

    auto *output = new QWaylandOutput(this, nullptr);
    setDefaultOutput(output);
    QWaylandCompositor::create();
}

void Compositor::onSurfaceCreated(QWaylandSurface *surface)
{
    connect(surface, &QWaylandSurface::surfaceDestroyed,
            this, &Compositor::surfaceDestroyed);
    connect(surface, &QWaylandSurface::hasContentChanged,
            this, &Compositor::surfaceHasContentChanged);
    connect(surface, &QWaylandSurface::redraw,
            this, &Compositor::onSurfaceRedraw);
    connect(surface, &QWaylandSurface::subsurfacePositionChanged,
            this, &Compositor::onSubsurfacePositionChanged);

    View *view = new View(this);
    view->setSurface(surface);
    connect(surface, &QWaylandSurface::offsetForNextFrame,
            view, &View::onOffsetForNextFrame);
    connect(surface, &QWaylandSurface::surfaceDestroyed,
            view, &QObject::deleteLater);
}

void Compositor::surfaceHasContentChanged()
{
    auto *surface = qobject_cast<QWaylandSurface *>(sender());
    if (!surface->role()) {
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

void Compositor::surfaceDestroyed()
{
    auto *surface = qobject_cast<QWaylandSurface *>(sender());
    triggerRender(surface);
}

void Compositor::onSurfaceRedraw()
{
    auto *surface = qobject_cast<QWaylandSurface *>(sender());
    triggerRender(surface);
}

bool Compositor::surfaceIsFocusable(QWaylandSurface *surface)
{
    return (surface == nullptr ||
            (surface->role() == QWaylandWlShellSurface::role() ||
             surface->role() == QWaylandXdgToplevel::role() ||
             surface->role() == QWaylandXdgPopup::role()));
}

Window *Compositor::createWindow(View *view)
{
    auto window = new Window(this);

    auto *output = new QWaylandOutput(this, window);
    output->setParent(window);

    view->setOutput(output);
    window->addView(view);

    connect(output, &QWaylandOutput::geometryChanged,
            view, &View::onOutputGeometryChanged);

    connect(window, &QWindow::heightChanged,
            view, &View::onWindowSizeChanged);
    connect(window, &QWindow::widthChanged,
            view, &View::onWindowSizeChanged);

    QScreen *screen = window->screen();
    connect(screen, &QScreen::orientationChanged,
            view, &View::onScreenOrientationChanged);
    screen->setOrientationUpdateMask(Qt::LandscapeOrientation |
                                     Qt::PortraitOrientation |
                                     Qt::InvertedLandscapeOrientation |
                                     Qt::InvertedPortraitOrientation);

    if (QSysInfo::productType() == "sailfishos") {
        window->showFullScreen();
    } else {
        window->resize(360, 640);
        window->show();
    }

    return window;
}

Window *Compositor::ensureWindowForView(View *view)
{
    Window *window;
    if (view->output()) {
        Q_ASSERT(view->output()->window());
        window = qobject_cast<Window *>(view->output()->window());
        Q_ASSERT(window);
    } else {
        if (view->parentView()) {
            window = ensureWindowForView(view->parentView());
            view->setOutput(outputFor(window));
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
    view->setParentView(parentView);
    view->setPosition(parentView->position() + relativeToParent);
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
    view->setParentView(parentView);
    view->setPosition(parentView->position() + relativeToParent);
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
    view->setParentView(parentView);
    view->setPosition(parentView->position() +
                      QPoint(popup->anchorRect().x(), popup->anchorRect().y()) +
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
    view->setParentView(parentView);
}

void Compositor::onSubsurfacePositionChanged(const QPoint &position)
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    if (!surface) {
        return;
    }
    Q_ASSERT(surface->primaryView());
    auto *view = qobject_cast<View *>(surface->primaryView());
    Q_ASSERT(view);
    view->setPosition(position);
    triggerRender(surface);
}

void Compositor::triggerRender(QWaylandSurface *surface)
{
    if (surface->primaryView() && surface->primaryView()->output() &&
            surface->primaryView()->output()->window()) {
        surface->primaryView()->output()->window()->requestUpdate();
    }
}

void Compositor::handleMouseEvent(View *target, QMouseEvent *me)
{
    QWaylandSeat *seat = seatFor(me);
    QWaylandSurface *surface;
    if (target) {
        surface = target->surface();
    } else {
        surface = nullptr;
    }
    switch (me->type()) {
    case QEvent::MouseButtonPress:
        seat->sendMousePressEvent(me->button());
        if (surface != seat->keyboardFocus() && surfaceIsFocusable(surface)) {
            seat->setKeyboardFocus(surface);
        }
        break;
    case QEvent::MouseButtonRelease:
         seat->sendMouseReleaseEvent(me->button());
         break;
    case QEvent::MouseMove:
        seat->sendMouseMoveEvent(target, me->localPos(), me->globalPos());
        break;
    default:
        break;
    }
}
