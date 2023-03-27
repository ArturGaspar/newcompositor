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

#include "view.h"

#include <QWaylandBufferRef>
#include <QWaylandOutput>
#include <QWaylandSurface>
#include <QWaylandWlShellSurface>
#include <QWaylandXdgPopup>
#include <QWaylandXdgToplevel>

#include "compositor.h"

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

void View::onOffsetForNextFrame(const QPoint &offset)
{
    m_offset = offset;
    m_position += offset;
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