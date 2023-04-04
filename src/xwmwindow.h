#ifndef XWMWINDOW_H
#define XWMWINDOW_H

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <xcb/xcb.h>

QT_BEGIN_NAMESPACE

class QWaylandSurface;

class Xwm;

class XwmWindow : public QObject
{
    Q_OBJECT
public:
    XwmWindow(Xwm *xwm, xcb_window_t window);

    QWaylandSurface *surface() const { return m_surface; }

    void resize(const QSize &size);
    void sendClose();
    void raise();

    bool isMapped() const { return m_mapped; }
    QString className() const { return m_className; }
    QString title() const { return m_title; }

signals:
    void positionChanged(const QPoint &pos);
    void setPopup(QWaylandSurface *parentSurface, const QPoint &pos);

private slots:
    void onSurfaceDestroyed();

private:
    friend class Xwm;

    void maybeSetPopup();

    void setSurface(QWaylandSurface *surface);
    void setMapped(bool mapped);
    void setOverrideRedirect(bool overrideRedirect);
    void setTransientFor(xcb_window_t transientFor);
    void setPosition(const QPoint &pos);
    void setTitle(const QString &title) { m_title = title; }
    void setClassName(const QString &className) { m_className = className; }

    Xwm *m_xwm;
    uint32_t m_surfaceId;
    QWaylandSurface *m_surface = nullptr;
    xcb_window_t m_window;

    QPoint m_position;
    bool m_overrideRedirect;
    bool m_mapped = true;

    QString m_title;
    QString m_className;
    xcb_window_t m_transientFor;
    QVector<xcb_atom_t> m_protocols;
};

QT_END_NAMESPACE

#endif // XWMWINDOW_H
