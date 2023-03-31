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

#ifndef WINDOW_H
#define WINDOW_H

#include <QOpenGLWindow>
#include <QOpenGLTexture>
#include <QOpenGLTextureBlitter>
#include <QPointer>
#include <QTimer>
#include <QTransform>
#include <QVector>

QT_BEGIN_NAMESPACE

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QScreen;
class QShowEvent;
class QTouchEvent;

class Compositor;
class View;

class Window : public QOpenGLWindow
{
    Q_OBJECT
public:
    Window(Compositor *compositor);

    void addView(View *view);

signals:
    void empty();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    bool event(QEvent *e) override;

    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;

    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;

    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;

    void touchEvent(QTouchEvent *e) override;

private slots:
    void viewSurfaceDestroyed();
    void onScreenChanged(QScreen *screen);
    void onScreenOrientationChanged(Qt::ScreenOrientation orientation);

private:
    void updateOutputMode();

    void showAgain();

    View *viewAt(const QPointF &point);
    void sendMouseEvent(QMouseEvent *e, View *view);

    QPointF mapInputPoint(const QPointF &point) const;

    QOpenGLTextureBlitter m_textureBlitter;
    QOpenGLTexture *m_backgroundTexture;
    Compositor *m_compositor = nullptr;
    QVector<View *> m_views;
    QPointer<View> m_mouseView;

    QPointer<QScreen> m_previousScreen;
    int m_rotation = 0;
    QTransform m_transform;
    QTransform m_inverseTransform;
};

QT_END_NAMESPACE

#endif // WINDOW_H
