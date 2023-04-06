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
    void closeWindow(xcb_window_t window);
    void raiseWindow(xcb_window_t window);
    void resizeWindow(xcb_window_t window, const QSize &size);
    void setFocusWindow(xcb_window_t window);

    QWaylandSurface *findSurface(uint32_t surfaceId) const;
    QWaylandSurface *surfaceForWindow(xcb_window_t window) const;

signals:
    void windowBoundToSurface(XwmWindow *XwmWindow,
                              QWaylandSurface *previousSurface);

private slots:
    void initialize();
    void onSurfaceReady(QWaylandSurface *surface);
    void onSurfaceAboutToBeDestroyed(QWaylandSurface *surface);
    void processEvents();

private:
    xcb_atom_t internAtom(const QByteArray &name);

    void syncWindowProperties(xcb_window_t window);

    void handleCreateNotify(xcb_generic_event_t *event);
    void handleDestroyNotify(xcb_generic_event_t *event);
    void handleUnmapNotify(xcb_generic_event_t *event);
    void handleMapNotify(xcb_generic_event_t *event);
    void handleMapRequest(xcb_generic_event_t *event);
    void handleConfigureNotify(xcb_generic_event_t *event);
    void handleConfigureRequest(xcb_generic_event_t *event);
    void handlePropertyNotify(xcb_generic_event_t *event);
    void handleClientMessage(xcb_generic_event_t *event);

    void readWindowProperty(xcb_window_t window, xcb_atom_t property);
    void readWindowTitle(xcb_window_t window, xcb_get_property_reply_t *reply);
    void readWindowClass(xcb_window_t window, xcb_get_property_reply_t *reply);
    void readWindowTransientFor(xcb_window_t window,
                                xcb_get_property_reply_t *reply);
    void readWindowProtocols(xcb_window_t window,
                             xcb_get_property_reply_t *reply);

    Compositor *m_compositor;
    Xwayland *m_xwayland;
    xcb_connection_t *m_conn;
    QSocketNotifier *m_notifier;

    QHash<xcb_window_t, XwmWindow*> m_windows;
    QHash<uint32_t, xcb_window_t> m_surfaceWindows;

    xcb_atom_t m_atom_wlSurfaceId;
    xcb_atom_t m_atom_wmProtocols;
    xcb_atom_t m_atom_wmDeleteWindow;
};

QT_END_NAMESPACE

#endif // XWM_H
