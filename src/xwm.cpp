#include "xwm.h"

#include <QDebug>
#include <QList>
#include <QProcess>
#include <QPoint>
#include <QSocketNotifier>
#include <QWaylandSurface>
#include <QWaylandView>
#include <stdlib.h>
#include <xcb/composite.h>
#include <wayland-server.h>

#include "compositor.h"
#include "xwayland.h"
#include "xwmwindow.h"

Xwm::Xwm(Compositor *compositor, Xwayland *xwayland)
    : QObject(compositor),
      m_compositor(compositor),
      m_xwayland(xwayland)
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

    xcb_screen_iterator_t s = ::xcb_setup_roots_iterator(xcb_get_setup(m_conn));
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

QWaylandSurface *Xwm::findSurface(xcb_window_t window) const
{
    const QList<QWaylandSurface *> surfaces = m_compositor->surfaces();
    for (QWaylandSurface *surface : surfaces) {
        uint32_t surfaceId = surface->resource()->object.id;
        if (m_surfaceWindows[surfaceId] == window) {
            return surface;
        }
    }
    return nullptr;
}

void Xwm::setWindowPosition(xcb_window_t window, const QPoint &pos)
{
    m_windowPositions[window] = pos;
    QWaylandSurface *surface = findSurface(window);
    if (surface) {
        emit windowPositionChanged(surface, pos);
    }
}

void Xwm::setWindowOverrideRedirect(xcb_window_t window, bool overrideRedirect)
{
    m_windowOverrideRedirects[window] = overrideRedirect;
}

void Xwm::createXwmWindow(QWaylandSurface *surface, xcb_window_t window)
{
    auto *xwmWindow = new XwmWindow(this, surface, window);
    emit xwmWindowCreated(xwmWindow);
}

void Xwm::onSurfaceReady(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    if (m_surfaceWindows.contains(surfaceId)) {
        createXwmWindow(surface, m_surfaceWindows[surfaceId]);
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
    setWindowOverrideRedirect(notify->window, notify->override_redirect);
    const static uint32_t values[] = {XCB_EVENT_MASK_PROPERTY_CHANGE};
    ::xcb_change_window_attributes(m_conn, notify->window,
                                   XCB_CW_EVENT_MASK, values);
    ::xcb_flush(m_conn);
    syncWindowProperties(notify->window);
}

void Xwm::handleDestroyNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_destroy_notify_event_t *>(event);
    xcb_window_t window = notify->window;
    m_windowUnmapped.remove(window);
    m_windowOverrideRedirects.remove(window);
    m_windowProperties.remove(window);
}

void Xwm::handleMapRequest(xcb_generic_event_t *event)
{
    auto request = reinterpret_cast<xcb_map_request_event_t *>(event);
    ::xcb_map_window(m_conn, request->window);
}

void Xwm::handleUnmapNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_unmap_notify_event_t *>(event);
    xcb_window_t window = notify->window;
    m_windowUnmapped[window] = true;
    QWaylandSurface *surface = findSurface(window);
    if (surface) {
        emit surface->hasContentChanged();
    }
}

void Xwm::handleMapNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_map_notify_event_t *>(event);
    xcb_window_t window = notify->window;
    m_windowUnmapped[window] = false;
    setWindowOverrideRedirect(notify->window, notify->override_redirect);
}

void Xwm::handleConfigureNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_configure_notify_event_t *>(event);
    setWindowOverrideRedirect(notify->window, notify->override_redirect);
    setWindowPosition(notify->window, QPoint(notify->x, notify->y));
}

void Xwm::handleConfigureRequest(xcb_generic_event_t *event)
{
    auto request = reinterpret_cast<xcb_configure_request_event_t *>(event);
    setWindowPosition(request->window, QPoint(request->x, request->y));
}

void Xwm::handlePropertyNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_property_notify_event_t *>(event);
    readWindowProperty(notify->window, notify->atom);
}

void Xwm::handleClientMessage(xcb_generic_event_t *event)
{
    auto message = reinterpret_cast<xcb_client_message_event_t *>(event);
    if (message->type == m_atom_wlSurfaceId) {
        xcb_window_t window = message->window;
        uint32_t surfaceId = message->data.data32[0];
        m_surfaceWindows[surfaceId] = window;
        QWaylandSurface *surface = findSurface(window);
        // Surface may have been destroyed or may not exist yet.
        if (surface) {
            createXwmWindow(surface, window);
        }
    }
}

xcb_get_property_reply_t *Xwm::getWindowProperty(xcb_window_t window,
                                                 xcb_atom_t property)
{
    xcb_get_property_cookie_t cookie = ::xcb_get_property(m_conn, 0, window,
                                                          property,
                                                          XCB_ATOM_ANY,
                                                          0, 2048);
    return ::xcb_get_property_reply(m_conn, cookie, NULL);
}

void Xwm::readWindowProperty(xcb_window_t window, xcb_atom_t property)
{
    xcb_get_property_reply_t *reply = getWindowProperty(window, property);
    if (reply == nullptr) {
        return;
    }
    if (property == XCB_ATOM_WM_TRANSIENT_FOR) {
        readWindowTransientFor(window, reply);
    } else if (property == m_atom_wmProtocols) {
        readWindowProtocols(window, reply);
    }
    ::free(reply);
}

void Xwm::readWindowTransientFor(xcb_window_t window,
                                 xcb_get_property_reply_t *reply)
{
    if (reply->type != XCB_ATOM_WINDOW) {
        return;
    }
    m_windowProperties[window].transientFor = *reinterpret_cast<xcb_window_t *>(::xcb_get_property_value(reply));
}

void Xwm::readWindowProtocols(xcb_window_t window,
                              xcb_get_property_reply_t *reply)
{
    m_windowProperties[window].protocols.clear();
    auto *atoms = reinterpret_cast<xcb_atom_t *>(::xcb_get_property_value(reply));
    for (uint32_t i = 0; i < reply->value_len; i++) {
        m_windowProperties[window].protocols.append(atoms[i]);
    }
}

void Xwm::closeWindow(xcb_window_t window)
{
    bool supportsDelete = false;
    const QVector<xcb_atom_t> protocols = m_windowProperties[window].protocols;
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

QPoint Xwm::windowPosition(xcb_window_t window) const
{
    return m_windowPositions[window];
}

QWaylandSurface *Xwm::parentSurface(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    if (!m_surfaceWindows.contains(surfaceId)) {
        return nullptr;
    }
    xcb_window_t window = m_surfaceWindows[surfaceId];
    if (!m_windowOverrideRedirects[window]) {
        return nullptr;
    }
    readWindowProperty(window, XCB_ATOM_WM_TRANSIENT_FOR);
    const xcb_window_t transientFor = m_windowProperties[window].transientFor;
    if (!transientFor) {
        return nullptr;
    }
    return findSurface(transientFor);
}

bool Xwm::windowIsUnmapped(xcb_window_t window) const
{
    if (!m_windowUnmapped.contains(window)) {
        return false;
    }
    return m_windowUnmapped[window];
}
