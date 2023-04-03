#ifndef XWM_H
#define XWM_H

#include <cstdint>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPoint>
#include <QSize>
#include <QVector>
#include <xcb/xcb.h>

QT_BEGIN_NAMESPACE

class QSocketNotifier;
class QWaylandSurface;

class Compositor;
class Xwayland;
class XwmWindow;

class Xwm : public QObject
{
    Q_OBJECT
public:
    Xwm(Compositor *compositor, Xwayland *xwayland);
    ~Xwm();
    void syncWindowProperties(xcb_window_t window);
    void closeWindow(xcb_window_t window);
    void raiseWindow(xcb_window_t window);
    void resizeWindow(xcb_window_t window, const QSize &size);
    bool windowIsUnmapped(xcb_window_t window) const;

    QPoint windowPosition(xcb_window_t window) const;
    QWaylandSurface *parentSurface(QWaylandSurface *surface);

signals:
    void windowPositionChanged(QWaylandSurface *surface, const QPoint &pos);
    void xwmWindowCreated(XwmWindow *XwmWindow);

private slots:
    void initialize();
    void onSurfaceReady(QWaylandSurface *surface);
    void onSurfaceAboutToBeDestroyed(QWaylandSurface *surface);
    void processEvents();

private:
    xcb_atom_t internAtom(const QByteArray &name);
    void createXwmWindow(QWaylandSurface *surface, xcb_window_t window);
    QWaylandSurface *findSurface(xcb_window_t window) const;
    void setWindowPosition(xcb_window_t window, const QPoint &pos);
    void setWindowOverrideRedirect(xcb_window_t window, bool overrideRedirect);

    void handleCreateNotify(xcb_generic_event_t *event);
    void handleDestroyNotify(xcb_generic_event_t *event);
    void handleUnmapNotify(xcb_generic_event_t *event);
    void handleMapNotify(xcb_generic_event_t *event);
    void handleMapRequest(xcb_generic_event_t *event);
    void handleConfigureNotify(xcb_generic_event_t *event);
    void handleConfigureRequest(xcb_generic_event_t *event);
    void handlePropertyNotify(xcb_generic_event_t *event);
    void handleClientMessage(xcb_generic_event_t *event);

    xcb_get_property_reply_t *getWindowProperty(xcb_window_t window,
                                                xcb_atom_t property);
    void readWindowProperty(xcb_window_t window, xcb_atom_t property);
    void readWindowTransientFor(xcb_window_t window,
                                xcb_get_property_reply_t *reply);
    void readWindowProtocols(xcb_window_t window,
                             xcb_get_property_reply_t *reply);

    Compositor *m_compositor;
    Xwayland *m_xwayland;
    xcb_connection_t *m_conn;
    QSocketNotifier *m_notifier;

    QHash<xcb_window_t, QPoint> m_windowPositions;

    QHash<xcb_window_t, bool> m_windowOverrideRedirects;
    QHash<xcb_window_t, bool> m_windowUnmapped;

    struct WindowProperty {
        xcb_window_t transientFor;
        QVector<xcb_atom_t> protocols;
    };
    QHash<xcb_window_t, WindowProperty> m_windowProperties;

    QHash<uint32_t, xcb_window_t> m_surfaceWindows;

    xcb_atom_t m_atom_wlSurfaceId;
    xcb_atom_t m_atom_wmProtocols;
    xcb_atom_t m_atom_wmDeleteWindow;
};

QT_END_NAMESPACE

#endif // XWM_H
