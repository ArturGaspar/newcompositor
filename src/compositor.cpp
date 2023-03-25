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
#include <QWaylandBufferRef>
#include <QWaylandOutput>
#include <QWaylandSeat>
#include <QWaylandSurface>
#include <QWaylandTouch>
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
            m_size = surface()->destinationSize();
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

QPointF View::parentPosition() const
{
    if (m_parentView) {
        return m_parentView->position() + m_parentView->parentPosition();
    } else {
        return QPointF();
    }
}

QSize View::size() const
{
    if (surface()) {
        return surface()->destinationSize();
    } else {
        return m_size;
    }
}

QSize View::outputRealSize() const
{
    if (output()) {
        return (output()->geometry().size() /
                output()->window()->devicePixelRatio());
    } else {
        return QSize();
    }
}

void View::onOutputGeometryChanged()
{
    if (m_xdgToplevel) {
        m_xdgToplevel->sendMaximized(outputRealSize());
    }
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
    return QString();
}

QString View::title() const
{
    if (m_xdgToplevel) {
        return m_xdgToplevel->title();
    }
    return QString();
}


Compositor::Compositor() :
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
    new QWaylandOutput(this, nullptr);
    QWaylandCompositor::create();

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
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    if (!surface->role()) {
        return;
    }
    if (surface->primaryView()) {
        if (surface->hasContent() && !surface->isCursorSurface()) {
            if (surface->role() == QWaylandXdgToplevel::role()
                    || surface->role() == QWaylandXdgPopup::role()) {
                defaultSeat()->setKeyboardFocus(surface);
            }
            auto view = qobject_cast<View *>(surface->primaryView());
            ensureWindowForView(view);
        }
        triggerRender(surface);
    }
}

void Compositor::surfaceDestroyed()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    triggerRender(surface);
}

void Compositor::onSurfaceRedraw()
{
    QWaylandSurface *surface = qobject_cast<QWaylandSurface *>(sender());
    triggerRender(surface);
}

Window *Compositor::ensureWindowForView(View *view)
{
    Window *window;
    if (view->output()) {
        Q_ASSERT(view->output()->window());
        window = qobject_cast<Window *>(view->output()->window());
    } else if (view->parentView()) {
        window = ensureWindowForView(view->parentView());
        view->setOutput(outputFor(window));
        window->addView(view);
    } else {
        window = new Window(this);
        window->show();
        auto *output = new QWaylandOutput(this, window);
        output->setParent(window);
        output->setSizeFollowsWindow(true);
        view->setOutput(output);
        window->addView(view);
        connect(output, &QWaylandOutput::geometryChanged,
                view, &View::onOutputGeometryChanged);
    }
    return window;
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
    auto *parentView = qobject_cast<View *>(parent->primaryView());
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
        if (surface != seat->keyboardFocus() &&
                (surface == nullptr ||
                 surface->role() == QWaylandXdgToplevel::role() ||
                 surface->role() == QWaylandXdgPopup::role())) {
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
