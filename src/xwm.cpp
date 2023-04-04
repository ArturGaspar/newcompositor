#include "xwm.h"

#include <QDebug>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QPoint>
#include <QSocketNotifier>
#include <QString>
#include <QWaylandSurface>
#include <QWaylandView>
#include <stdlib.h>
#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <wayland-server.h>

#include "compositor.h"
#include "xwayland.h"
#include "xwmwindow.h"

Xwm::Xwm(Compositor *compositor, Xwayland *xwayland)
    : QObject(compositor)
    , m_compositor(compositor)
    , m_xwayland(xwayland)
{
    connect(m_compositor, &Compositor::surfaceReady,
            this, &Xwm::onSurfaceReady);
    connect(m_compositor, &QWaylandCompositor::surfaceAboutToBeDestroyed,
            this, &Xwm::onSurfaceAboutToBeDestroyed);
    connect(m_xwayland, &Xwayland::displayReady,
            this, &Xwm::initialize);
}

Xwm::~Xwm()
{
    if (m_conn) {
        ::xcb_disconnect(m_conn);
    }
}

void Xwm::initialize() {
    m_conn = ::xcb_connect_to_fd(m_xwayland->wmFd(), NULL);
    if (::xcb_connection_has_error(m_conn)) {
        ::xcb_disconnect(m_conn);
        qCritical("error connecting to Xwayland");
        return;
    }

    m_notifier = new QSocketNotifier(::xcb_get_file_descriptor(m_conn),
                                     QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &Xwm::processEvents);

    m_atom_wlSurfaceId = internAtom("WL_SURFACE_ID");
    m_atom_wmDeleteWindow = internAtom("WM_DELETE_WINDOW");
    m_atom_wmProtocols = internAtom("WM_PROTOCOLS");

    xcb_screen_iterator_t s = ::xcb_setup_roots_iterator(::xcb_get_setup(m_conn));
    xcb_screen_t *screen = s.data;

    const static uint32_t values[] = {(XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                                       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)};
    ::xcb_change_window_attributes(m_conn, screen->root,
                                   XCB_CW_EVENT_MASK, values);

    ::xcb_composite_redirect_subwindows(m_conn, screen->root,
                                        XCB_COMPOSITE_REDIRECT_MANUAL);

    ::xcb_flush(m_conn);
}

void Xwm::syncWindowProperties(xcb_window_t window)
{
    const xcb_atom_t props[] = {XCB_ATOM_WM_TRANSIENT_FOR, m_atom_wmProtocols};
    for (xcb_atom_t prop : props) {
        readWindowProperty(window, prop);
    }
}

xcb_atom_t Xwm::internAtom(const QByteArray &name)
{
    xcb_intern_atom_cookie_t cookie = ::xcb_intern_atom(m_conn, 0, strlen(name),
                                                        name);
    xcb_intern_atom_reply_t *reply = ::xcb_intern_atom_reply(m_conn, cookie,
                                                             NULL);
    xcb_atom_t atom = reply->atom;
    ::free(reply);
    return atom;
}

QWaylandSurface *Xwm::findSurface(uint32_t surfaceId) const
{
    const QList<QWaylandSurface *> surfaces = m_compositor->surfaces();
    for (QWaylandSurface *surface : surfaces) {
        if (surface->resource()->object.id == surfaceId) {
            return surface;
        }
    }
    return nullptr;
}

QWaylandSurface *Xwm::surfaceForWindow(xcb_window_t window) const
{
    if (!m_windows.contains(window)) {
        return nullptr;
    }
    return m_windows[window]->m_surface;
}

void Xwm::onSurfaceReady(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    const QHash<xcb_window_t, XwmWindow*> windows = m_windows;
    for (XwmWindow *xwmWindow : windows) {
        if (xwmWindow->m_surfaceId == surfaceId) {
            xwmWindow->setSurface(surface);
            return;
        }
    }
}

void Xwm::onSurfaceAboutToBeDestroyed(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    m_surfaceWindows.remove(surfaceId);
}

void Xwm::processEvents()
{
    int count = 0;
    while (xcb_generic_event_t *event = ::xcb_poll_for_event(m_conn)) {
        switch (event->response_type & ~0x80) {
        case 0:
            break;
        case XCB_CREATE_NOTIFY:
            handleCreateNotify(event);
            break;
        case XCB_DESTROY_NOTIFY:
            handleDestroyNotify(event);
            break;
        case XCB_UNMAP_NOTIFY:
            handleUnmapNotify(event);
            break;
        case XCB_MAP_NOTIFY:
            handleMapNotify(event);
            break;
        case XCB_MAP_REQUEST:
            handleMapRequest(event);
            break;
        case XCB_CONFIGURE_NOTIFY:
            handleConfigureNotify(event);
            break;
        case XCB_CONFIGURE_REQUEST:
            handleConfigureRequest(event);
            break;
        case XCB_PROPERTY_NOTIFY:
            handlePropertyNotify(event);
            break;
        case XCB_CLIENT_MESSAGE:
            handleClientMessage(event);
            break;
        case XCB_MAPPING_NOTIFY:
            break;
        default:
            qDebug() << "Unknown event" << (event->response_type & ~0x80);
        }
        ::free(event);
        count++;
    }
    if (count) {
        ::xcb_flush(m_conn);
    }
}

void Xwm::handleCreateNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_create_notify_event_t *>(event);
    const xcb_window_t window = notify->window;
    auto *xwmWindow = new XwmWindow(this, window);
    xwmWindow->setOverrideRedirect(notify->override_redirect);
    m_windows[window] = xwmWindow;
    const static uint32_t values[] = {XCB_EVENT_MASK_PROPERTY_CHANGE};
    ::xcb_change_window_attributes(m_conn, window, XCB_CW_EVENT_MASK, values);
    ::xcb_flush(m_conn);
    syncWindowProperties(window);
}

void Xwm::handleDestroyNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_destroy_notify_event_t *>(event);
    if (!m_windows.contains(notify->window)) {
        return;
    }
    m_windows[notify->window]->deleteLater();
    m_windows.remove(notify->window);
}

void Xwm::handleMapRequest(xcb_generic_event_t *event)
{
    auto request = reinterpret_cast<xcb_map_request_event_t *>(event);
    ::xcb_map_window(m_conn, request->window);
}

void Xwm::handleUnmapNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_unmap_notify_event_t *>(event);
    if (!m_windows.contains(notify->window)) {
        return;
    }
    m_windows[notify->window]->setMapped(false);
}

void Xwm::handleMapNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_map_notify_event_t *>(event);
    if (!m_windows.contains(notify->window)) {
        return;
    }
    m_windows[notify->window]->setMapped(true);
    m_windows[notify->window]->setOverrideRedirect(notify->override_redirect);
}

void Xwm::handleConfigureNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_configure_notify_event_t *>(event);
    if (!m_windows.contains(notify->window)) {
        return;
    }
    m_windows[notify->window]->setPosition(QPoint(notify->x, notify->y));
    m_windows[notify->window]->setOverrideRedirect(notify->override_redirect);
}

void Xwm::handleConfigureRequest(xcb_generic_event_t *event)
{
    auto request = reinterpret_cast<xcb_configure_request_event_t *>(event);
    if (!m_windows.contains(request->window)) {
        return;
    }
    m_windows[request->window]->setPosition(QPoint(request->x, request->y));
}

void Xwm::handlePropertyNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_property_notify_event_t *>(event);
    if (!m_windows.contains(notify->window)) {
        return;
    }
    readWindowProperty(notify->window, notify->atom);
}

void Xwm::handleClientMessage(xcb_generic_event_t *event)
{
    auto message = reinterpret_cast<xcb_client_message_event_t *>(event);
    if (!m_windows.contains(message->window)) {
        return;
    }
    if (message->type == m_atom_wlSurfaceId) {
        uint32_t surfaceId = message->data.data32[0];
        XwmWindow *xwmWindow = m_windows[message->window];
        xwmWindow->m_surfaceId = surfaceId;
        QWaylandSurface *surface = findSurface(surfaceId);
        // Surface may have been destroyed or may not exist yet.
        if (surface) {
            xwmWindow->setSurface(surface);
        }
    }
}

void Xwm::readWindowProperty(xcb_window_t window, xcb_atom_t property)
{
    xcb_get_property_cookie_t cookie = ::xcb_get_property(m_conn, 0, window,
                                                          property,
                                                          XCB_ATOM_ANY,
                                                          0, 2048);
    xcb_get_property_reply_t *reply = ::xcb_get_property_reply(m_conn, cookie,
                                                               NULL);
    if (reply == nullptr) {
        return;
    }
    if (property == XCB_ATOM_WM_NAME) {
        readWindowTitle(window, reply);
    } else if (property == XCB_ATOM_WM_CLASS) {
        readWindowClass(window, reply);
    } else if (property == XCB_ATOM_WM_TRANSIENT_FOR) {
        readWindowTransientFor(window, reply);
    } else if (property == m_atom_wmProtocols) {
        readWindowProtocols(window, reply);
    }
    ::free(reply);
}

void Xwm::readWindowTitle(xcb_window_t window,
                          xcb_get_property_reply_t *reply)
{
    int len = ::xcb_get_property_value_length(reply);
    auto *title = reinterpret_cast<char *>(::xcb_get_property_value(reply));
    m_windows[window]->setTitle(QString::fromLocal8Bit(title, len));
}

void Xwm::readWindowClass(xcb_window_t window,
                          xcb_get_property_reply_t *reply)
{
    int len = ::xcb_get_property_value_length(reply);
    auto *className = reinterpret_cast<char *>(::xcb_get_property_value(reply));
    m_windows[window]->setClassName(QString::fromLocal8Bit(className, len));
}

void Xwm::readWindowTransientFor(xcb_window_t window,
                                 xcb_get_property_reply_t *reply)
{
    xcb_window_t transientFor = *reinterpret_cast<xcb_window_t *>(::xcb_get_property_value(reply));
    m_windows[window]->setTransientFor(transientFor);
}

void Xwm::readWindowProtocols(xcb_window_t window,
                              xcb_get_property_reply_t *reply)
{
    m_windows[window]->m_protocols.clear();
    // XXX: not sure if this is the way to do it.
    uint32_t len = ::xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
    auto *atoms = reinterpret_cast<xcb_atom_t *>(::xcb_get_property_value(reply));
    for (uint32_t i = 0; i < len; i++) {
        m_windows[window]->m_protocols.append(atoms[i]);
    }
}

void Xwm::closeWindow(xcb_window_t window)
{
    if (!m_windows.contains(window)) {
        return;
    }
    bool supportsDelete = false;
    const QVector<xcb_atom_t> protocols = m_windows[window]->m_protocols;
    for (xcb_atom_t protocol : protocols) {
        if (protocol == m_atom_wmDeleteWindow) {
            supportsDelete = true;
            break;
        }
    }
    if (supportsDelete) {
        xcb_client_message_data_t messageData = {0};
        messageData.data32[0] = m_atom_wmDeleteWindow;
        messageData.data32[1] = XCB_CURRENT_TIME;
        xcb_client_message_event_t event = {
            .response_type = XCB_CLIENT_MESSAGE,
            .format = 32,
            .sequence = 0,
            .window = window,
            .type = m_atom_wmProtocols,
            .data = messageData
        };
        ::xcb_send_event(m_conn, 0, window, XCB_EVENT_MASK_NO_EVENT,
                         reinterpret_cast<const char *>(&event));
    } else {
        ::xcb_kill_client(m_conn, window);
    }
    ::xcb_flush(m_conn);
}

void Xwm::raiseWindow(xcb_window_t window)
{
    uint16_t mask = XCB_CONFIG_WINDOW_STACK_MODE;
    uint32_t values[] = {XCB_STACK_MODE_ABOVE};
    ::xcb_configure_window(m_conn, window, mask, values);
    ::xcb_flush(m_conn);
}

void Xwm::resizeWindow(xcb_window_t window, const QSize &size)
{
    uint16_t mask = (XCB_CONFIG_WINDOW_WIDTH |
                     XCB_CONFIG_WINDOW_HEIGHT);
    uint32_t values[] = {(uint32_t) size.width(), (uint32_t) size.height()};
    ::xcb_configure_window(m_conn, window, mask, values);
    ::xcb_flush(m_conn);
}
