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

#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandView>
#include <QOpenGLTextureBlitter>
#include <QPointF>
#include <QPoint>
#include <QSize>
#include <QString>

QT_BEGIN_NAMESPACE

class QOpenGLTexture;
class QWaylandOutput;
class QWaylandWlShellSurface;
class QWaylandXdgPopup;
class QWaylandXdgToplevel;

class Compositor;

class View : public QWaylandView
{
    Q_OBJECT
public:
    View(Compositor *compositor);
    QOpenGLTexture *getTexture();
    QOpenGLTextureBlitter::Origin textureOrigin() const;
    QPointF position() const { return m_position; }
    void setPosition(const QPointF &pos) { m_position = pos; }
    QSize size() const;
    void setParentView(View *parent);
    View *parentView() const { return m_parentView; }
    QPointF parentPosition() const;
    QPoint offset() const { return m_offset; }
    QString appId() const;
    QString title() const;

private:
    QSize outputRealSize() const;
    friend class Compositor;
    Compositor *m_compositor = nullptr;
    GLenum m_textureTarget = GL_TEXTURE_2D;
    QOpenGLTexture *m_texture = nullptr;
    QOpenGLTextureBlitter::Origin m_origin;
    QPointF m_position;
    QSize m_size;
    QWaylandWlShellSurface *m_wlShellSurface = nullptr;
    QWaylandXdgToplevel *m_xdgToplevel = nullptr;
    QWaylandXdgPopup *m_xdgPopup = nullptr;
    View *m_parentView = nullptr;
    QPoint m_offset;

public slots:
    void onOutputGeometryChanged();
    void onOffsetForNextFrame(const QPoint &offset);
    void sendClose();
};

class Window;

class QMouseEvent;
class QWaylandSurface;
class QWaylandWlShell;
class QWaylandXdgShell;
class QWaylandXdgDecorationManagerV1;
class QWaylandXdgSurface;

class Compositor : public QWaylandCompositor
{
    Q_OBJECT
public:
    Compositor();
    ~Compositor() override;
    void create() override;

    void handleMouseEvent(View *target, QMouseEvent *me);

signals:
    void frameOffset(const QPoint &offset);

private slots:
    void triggerRender(QWaylandSurface *surface);
    void surfaceHasContentChanged();
    void surfaceDestroyed();
    void onSurfaceRedraw();

    void onSurfaceCreated(QWaylandSurface *surface);
    void onWlShellSurfaceCreated(QWaylandWlShellSurface *wlShellSurface);
    void onWlShellSurfaceSetTransient(QWaylandSurface *parentSurface,
                                      const QPoint &relativeToParent,
                                      bool inactive);
    void onWlShellSurfaceSetPopup(QWaylandSeat *seat,
                                  QWaylandSurface *parentSurface,
                                  const QPoint &relativeToParent);
    void onXdgToplevelCreated(QWaylandXdgToplevel *toplevel,
                              QWaylandXdgSurface *xdgSurface);
    void onXdgPopupCreated(QWaylandXdgPopup *popup,
                           QWaylandXdgSurface *xdgSurface);

    void onSubsurfaceChanged(QWaylandSurface *child, QWaylandSurface *parent);
    void onSubsurfacePositionChanged(const QPoint &position);

private:
    bool surfaceIsFocusable(QWaylandSurface *surface);
    Window *ensureWindowForView(View *view);
    QWaylandWlShell *m_wlShell;
    QWaylandXdgShell *m_xdgShell;
    QWaylandXdgDecorationManagerV1 *m_xdgDecorationManager;
};

QT_END_NAMESPACE

#endif // COMPOSITOR_H
