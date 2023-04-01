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

class Xwm : public QObject
{
    Q_OBJECT
public:
    explicit Xwm(Compositor *compositor, Xwayland *xwayland);
    ~Xwm();
    bool isX11Window(QWaylandSurface *surface);
    void closeWindow(QWaylandSurface *surface);
    void raiseWindow(QWaylandSurface *surface);
    void resizeWindow(QWaylandSurface *surface, const QSize &size);
    const QPoint windowPosition(QWaylandSurface *surface);
    QWaylandSurface *parentSurface(QWaylandSurface *surface);
    bool windowIsUnmapped(QWaylandSurface *surface);

signals:
    void windowPositionChanged(QWaylandSurface *surface);

private slots:
    void initialize();
    void processEvents();
    void onSurfaceAboutToBeDestroyed(QWaylandSurface *surface);

private:
    xcb_atom_t internAtom(const QByteArray &name);
    QWaylandSurface *findSurface(xcb_window_t window);
    void setWindowPosition(xcb_window_t window, const QPoint &pos);

    void handleCreateNotify(xcb_generic_event_t *event);
    void handleDestroyNotify(xcb_generic_event_t *event);
    void handleUnmapNotify(xcb_generic_event_t *event);
    void handleMapNotify(xcb_generic_event_t *event);
    void handleConfigureNotify(xcb_generic_event_t *event);
    void handleConfigureRequest(xcb_generic_event_t *event);
    void handleMapRequest(xcb_generic_event_t *event);
    void handleClientMessage(xcb_generic_event_t *event);

    xcb_get_property_reply_t *getWindowProperty(xcb_window_t window,
                                                xcb_atom_t property);
    QVector<xcb_atom_t> readWindowProtocols(xcb_window_t window);
    xcb_window_t readWindowTransientFor(xcb_window_t window);

    Compositor *m_compositor;
    Xwayland *m_xwayland;
    xcb_connection_t *m_conn;
    QSocketNotifier *m_notifier;

    QHash<xcb_window_t, QPoint> m_windowPositions;
    QHash<xcb_window_t, bool> m_windowOverrideRedirects;
    QHash<xcb_window_t, bool> m_windowUnmapped;

    QHash<uint32_t, xcb_window_t> m_surfaceWindows;

    xcb_atom_t m_atom_wlSurfaceId;
    xcb_atom_t m_atom_wmProtocols;
    xcb_atom_t m_atom_wmDeleteWindow;
};

QT_END_NAMESPACE

#endif // XWM_H
