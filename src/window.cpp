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

#include "window.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLTextureBlitter>
#include <QOpenGLWindow>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QScreen>
#include <QSet>
#include <QTouchEvent>
#include <QTransform>
#include <QWaylandOutput>
#include <QWaylandOutputMode>
#include <QWaylandSeat>
#include <QWaylandView>

#include "compositor.h"

Window::Window(Compositor *compositor) :
    m_compositor(compositor)
{
    // TODO: handle screen changes.
    connect(screen(), &QScreen::orientationChanged,
            this, &Window::onScreenOrientationChanged);
    screen()->setOrientationUpdateMask(Qt::LandscapeOrientation |
                                       Qt::PortraitOrientation |
                                       Qt::InvertedLandscapeOrientation |
                                       Qt::InvertedPortraitOrientation);
}

void Window::addView(View *view)
{
    connect(view, &QWaylandView::surfaceDestroyed,
            this, &Window::viewSurfaceDestroyed);
    m_views << view;
}

void Window::viewSurfaceDestroyed()
{
    View *view = qobject_cast<View *>(sender());
    m_views.removeAll(view);
    if (m_views.empty()) {
        delete this;
    }
}

void Window::initializeGL()
{
    m_textureBlitter.create();
}

void Window::resizeGL(int w, int h)
{
    QImage backgroundImage(w, h, QImage::Format_RGB32);
    backgroundImage.fill(Qt::white);
    QPainter p(&backgroundImage);
    p.fillRect(0, 0, w, h, Qt::Dense4Pattern);
    p.end();
    m_backgroundTexture = new QOpenGLTexture(backgroundImage,
                                             QOpenGLTexture::DontGenerateMipMaps);
}

QTransform Window::orientationTransform() const
{
    QTransform t;
    t.translate(width() / 2, height() / 2);
    t.rotate(m_rotation);
    if (m_rotation == Rotate90 || m_rotation == Rotate270) {
        t.translate(-height() / 2, -width() / 2);
    } else {
        t.translate(-width() / 2, -height() / 2);
    }
    return t;
}

void Window::paintGL()
{
    QWaylandOutput *output = m_compositor->outputFor(this);
    if (output) {
        output->frameStarted();
    }

    QOpenGLFunctions *functions = context()->functions();
    functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_textureBlitter.bind();

    functions->glEnable(GL_BLEND);
    functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QRect viewportRect(QPoint(), size());

    m_textureBlitter.blit(m_backgroundTexture->textureId(),
                          QOpenGLTextureBlitter::targetTransform(viewportRect,
                                                                 viewportRect),
                          QOpenGLTextureBlitter::OriginTopLeft);

    QTransform t = orientationTransform();

    GLenum currentTarget = GL_TEXTURE_2D;
    for (View *view : qAsConst(m_views)) {
        QOpenGLTexture *texture = view->getTexture();
        if (!texture) {
            continue;
        }
        if (texture->target() != currentTarget) {
            currentTarget = texture->target();
            m_textureBlitter.bind(currentTarget);
        }
        QWaylandSurface *surface = view->surface();
        if (surface && surface->hasContent()) {
            QSize destSize = surface->destinationSize();
            if (destSize.isEmpty()) {
                continue;
            }
            QRectF targetRect = t.mapRect(QRectF(view->position(), destSize));
            QMatrix4x4 m = QOpenGLTextureBlitter::targetTransform(targetRect,
                                                                  viewportRect);
            m.rotate(-m_rotation, 0, 0, 1);
            m_textureBlitter.blit(texture->textureId(), m,
                                  view->textureOrigin());
        }
    }
    functions->glDisable(GL_BLEND);

    m_textureBlitter.release();

    if (output) {
        output->sendFrameCallbacks();
    }
}

void Window::onScreenOrientationChanged(Qt::ScreenOrientation orientation)
{
    Q_UNUSED(orientation);
    updateOutputMode();
}

void Window::updateOutputMode()
{
    QSize outputSize = size();
    if (outputSize.isEmpty()) {
        return;
    }

    QWaylandOutput *output = m_compositor->outputFor(this);
    if (!output) {
        return;
    }

    int rotation = 0;
    int refreshRate;

    if (screen()) {
        Qt::ScreenOrientation outputSizeOrientation;
        switch (screen()->orientation()) {
        case Qt::InvertedPortraitOrientation:
            outputSizeOrientation = Qt::PortraitOrientation;
            rotation += 180;
            break;
        case Qt::InvertedLandscapeOrientation:
            outputSizeOrientation = Qt::LandscapeOrientation;
            rotation += 180;
            break;
        default:
            outputSizeOrientation = screen()->orientation();
        }
        if (screen()->primaryOrientation() != outputSizeOrientation) {
            rotation += 90;
            outputSize.transpose();
        }
        refreshRate = screen()->refreshRate() * 1000;
    } else {
        refreshRate = 60 * 1000;
    }

    m_rotation = static_cast<Rotation>(rotation);

    QWaylandOutputMode mode(outputSize, refreshRate);
    output->addMode(mode, false);
    output->setCurrentMode(mode);
}


bool Window::event(QEvent *e)
{
    if (e->type() == QEvent::Close) {
        if (!m_views.empty()) {
            m_views.first()->sendClose();
            e->ignore();
            return false;
        }
    }
    return QOpenGLWindow::event(e);
}

void Window::resizeEvent(QResizeEvent *e)
{
    updateOutputMode();
    QOpenGLWindow::resizeEvent(e);
}

void Window::showEvent(QShowEvent *e)
{
    updateOutputMode();
    QOpenGLWindow::showEvent(e);
}

View *Window::viewAt(const QPointF &point)
{
    const QPointF mappedPoint = mapInputPoint(point);
    const auto views = m_views;
    for (auto i = views.crbegin(), end = views.crend(); i != end; ++i) {
        View *view = *i;
        QWaylandSurface *surface = view->surface();
        if (surface && surface->hasContent()) {
            QRectF geom(view->position(), surface->destinationSize());
            if (geom.contains(mappedPoint)) {
                return view;
            }
        }
    }
    return nullptr;
}

QPointF Window::mapInputPoint(const QPointF &point) const {
    bool invertible;
    QTransform t = orientationTransform().inverted(&invertible);
    Q_ASSERT(invertible);
    return t.map(point);
}

void Window::mousePressEvent(QMouseEvent *e)
{
    if (m_mouseView.isNull()) {
        m_mouseView = viewAt(e->localPos());
        if (!m_mouseView) {
            return;
        }
        QMouseEvent moveEvent(QEvent::MouseMove, e->localPos(), e->globalPos(),
                              Qt::NoButton, Qt::NoButton, e->modifiers());
        mouseMoveEvent(&moveEvent);
    }
    QWaylandSeat *seat = m_compositor->seatFor(e);
    seat->sendMousePressEvent(e->button());
    QWaylandSurface *surface = m_mouseView->surface();
    if (surface != seat->keyboardFocus() &&
            m_compositor->surfaceIsFocusable(surface)) {
        seat->setKeyboardFocus(surface);
    }
}

void Window::mouseReleaseEvent(QMouseEvent *e)
{
    m_compositor->seatFor(e)->sendMouseReleaseEvent(e->button());
    if (e->buttons() == Qt::NoButton) {
        m_mouseView = nullptr;
    }
}

void Window::mouseMoveEvent(QMouseEvent *e)
{
    View *view;
    if (m_mouseView) {
        view = m_mouseView;
    } else {
        view = viewAt(e->localPos());
    }
    if (!view) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    QPointF mappedPos = mapInputPoint(e->localPos()) - view->position();
    m_compositor->seatFor(e)->sendMouseMoveEvent(view, mappedPos);
}

void Window::keyPressEvent(QKeyEvent *e)
{
    m_compositor->seatFor(e)->sendFullKeyEvent(e);
}

void Window::keyReleaseEvent(QKeyEvent *e)
{
    m_compositor->seatFor(e)->sendFullKeyEvent(e);
}

void Window::touchEvent(QTouchEvent *e)
{
    bool hadTouchClient = false;
    QSet<QWaylandClient *> clients;
    QWaylandSeat *seat = m_compositor->seatFor(e);
    for (const QTouchEvent::TouchPoint &p : e->touchPoints()) {
        QPointF pos = p.pos();
        View *view = viewAt(pos);
        if (!view) {
            continue;
        }
        if (p.state() == Qt::TouchPointReleased) {
            seat->setKeyboardFocus(view->surface());
        }
        bool touchClient = false;
        // TODO: Look at how wlroots detects touch-able clients instead of
        //       whitelisting KDE clients.
        // XXX: It is intentional we don't give touches to popups that don't
        //      have the app id even though their ancestor does. That is weird
        //      also on KDE apps.
        if (view->appId().startsWith("org.kde.")) {
            touchClient = true;
        }
        if (touchClient) {
            hadTouchClient = true;
        } else {
            continue;
        }
        QPointF mappedPos = mapInputPoint(pos);
        mappedPos -= view->position();
        seat->sendTouchPointEvent(view->surface(), p.id(), mappedPos,
                                  p.state());
        clients.insert(view->surface()->client());
    }
    for (QWaylandClient *client : clients) {
        seat->sendTouchFrameEvent(client);
    }
    if (!hadTouchClient) {
        // Make Qt synthesise a mouse event for it.
        e->ignore();
    }
}
