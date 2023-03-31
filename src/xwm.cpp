#include "xwm.h"

#include <QList>
#include <QProcess>
#include <QPoint>
#include <QSocketNotifier>
#include <QWaylandView>
#include <stdlib.h>
#include <xcb/composite.h>
#include <wayland-server.h>

#include "compositor.h"
#include "xwayland.h"

Xwm::Xwm(Compositor *compositor, Xwayland *xwayland)
    : QObject(compositor),
      m_compositor(compositor),
      m_xwayland(xwayland)
{
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
            // TODO: use unmap notify to hide surface sooner.
            break;
        case XCB_MAP_NOTIFY:
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

void Xwm::onSurfaceAboutToBeDestroyed(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    m_surfaceWindows.remove(surfaceId);
}

void Xwm::handleCreateNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_create_notify_event_t *>(event);
    m_windowOverrideRedirects[notify->window] = notify->override_redirect;
}

void Xwm::handleDestroyNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_destroy_notify_event_t *>(event);
    xcb_window_t window = notify->window;
    m_windowOverrideRedirects.remove(window);
    m_windowPositions.remove(window);
}

void Xwm::handleMapRequest(xcb_generic_event_t *event)
{
    auto request = reinterpret_cast<xcb_map_request_event_t *>(event);
    ::xcb_map_window(m_conn, request->window);
}

void Xwm::handleConfigureNotify(xcb_generic_event_t *event)
{
    auto notify = reinterpret_cast<xcb_configure_notify_event_t *>(event);
    m_windowPositions[notify->window] = QPoint(notify->x, notify->y);
}

void Xwm::handleConfigureRequest(xcb_generic_event_t *event)
{
    auto request = reinterpret_cast<xcb_configure_request_event_t *>(event);
    m_windowPositions[request->window] = QPoint(request->x, request->y);
}

void Xwm::handleClientMessage(xcb_generic_event_t *event)
{
    auto message = reinterpret_cast<xcb_client_message_event_t *>(event);
    if (message->type == m_atom_wlSurfaceId) {
        xcb_window_t window = message->window;
        uint32_t surfaceId = message->data.data32[0];
        m_surfaceWindows[surfaceId] = window;
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

QVector<xcb_atom_t> Xwm::readWindowProtocols(xcb_window_t window)
{
    xcb_get_property_reply_t *reply = getWindowProperty(window,
                                                        m_atom_wmProtocols);

    QVector<xcb_atom_t> protocols;
    if (reply == nullptr) {
        return protocols;
    }

    auto *atoms = reinterpret_cast<xcb_atom_t *>(::xcb_get_property_value(reply));
    for (uint32_t i = 0; i < reply->value_len; i++) {
        protocols.append(atoms[i]);
        atoms++;
    }

    ::free(reply);

    return protocols;
}

xcb_window_t Xwm::readWindowTransientFor(xcb_window_t window)
{
    xcb_get_property_reply_t *reply = getWindowProperty(window,
                                                        XCB_ATOM_WM_TRANSIENT_FOR);
    if (reply == nullptr) {
        return 0;
    }
    if (reply->type != XCB_ATOM_WINDOW) {
        return 0;
    }
    auto parent = *reinterpret_cast<xcb_window_t *>(::xcb_get_property_value(reply));
    ::free(reply);
    return parent;
}

bool Xwm::isX11Window(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    return m_surfaceWindows.contains(surfaceId);
}

void Xwm::closeWindow(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    if (!m_surfaceWindows.contains(surfaceId)) {
        return;
    }
    const xcb_window_t window = m_surfaceWindows[surfaceId];
    bool supportsDelete = false;
    const QVector<xcb_atom_t> protocols = readWindowProtocols(window);
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

void Xwm::raiseWindow(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    if (!m_surfaceWindows.contains(surfaceId)) {
        return;
    }
    const xcb_window_t window = m_surfaceWindows[surfaceId];
    uint16_t mask = XCB_CONFIG_WINDOW_STACK_MODE;
    uint32_t values[] = {XCB_STACK_MODE_ABOVE};
    ::xcb_configure_window(m_conn, window, mask, values);
    ::xcb_flush(m_conn);
}

void Xwm::resizeWindow(QWaylandSurface *surface, const QSize &size)
{
    uint32_t surfaceId = surface->resource()->object.id;
    if (!m_surfaceWindows.contains(surfaceId)) {
        return;
    }
    const xcb_window_t window = m_surfaceWindows[surfaceId];
    uint16_t mask = (XCB_CONFIG_WINDOW_WIDTH |
                     XCB_CONFIG_WINDOW_HEIGHT);
    uint32_t values[] = {(uint32_t) size.width(), (uint32_t) size.height()};
    ::xcb_configure_window(m_conn, window, mask, values);
    ::xcb_flush(m_conn);
}

const QPoint Xwm::windowPosition(QWaylandSurface *surface)
{
    uint32_t surfaceId = surface->resource()->object.id;
    if (!m_surfaceWindows.contains(surfaceId)) {
        return QPoint();
    }
    const xcb_window_t window = m_surfaceWindows[surfaceId];
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

    const xcb_window_t transientFor = readWindowTransientFor(window);
    if (!transientFor) {
        return nullptr;
    }

    const QList<QWaylandSurface *> surfaces = m_compositor->surfaces();
    for (QWaylandSurface *aSurface : surfaces) {
        uint32_t aSurfaceId = aSurface->resource()->object.id;
        if (m_surfaceWindows[aSurfaceId] == transientFor) {
            return aSurface;
        }
    }

    return nullptr;
}
