#include "dbuscontainerstate.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusServer>
#include <QDebug>

#include "compositor.h"

DBusContainerState::DBusContainerState(Compositor *compositor)
    : QObject(compositor)
    , m_compositor(compositor)
{
    QStringList arguments = QCoreApplication::instance()->arguments();

    const int addressArg = arguments.indexOf("--qt-runner-address");
    if (addressArg != -1 && addressArg + 1 < arguments.size()) {
        QString address = arguments.at(addressArg + 1);
        m_server = new QDBusServer(address, this);
    } else {
        m_server = new QDBusServer(this);
    }
    m_server->setAnonymousAuthenticationAllowed(true);

    qInfo("qt-runnner compatibility server running on FLATPAK_MALIIT_CONTAINER_DBUS=%s",
          m_server->address().toLocal8Bit().constData());

    connect(m_server, &QDBusServer::newConnection,
            this, &DBusContainerState::onNewConnection);
}

void DBusContainerState::onNewConnection(const QDBusConnection &connection)
{
    QDBusConnection con(connection);
    con.registerObject("/", this,
                       QDBusConnection::ExportAllProperties |
                       QDBusConnection::ExportAllSignals |
                       QDBusConnection::ExportAllSlots);
    con.registerService(FLATPAK_RUNNER_DBUS_CONT_SERVICE);
}

void DBusContainerState::keyboardRect(bool active, int x, int y,
                                      int width, int height)
{
    emit m_compositor->keyboardRect(active, x, y, width, height);
}

void DBusContainerState::onWindowRotationChanged(int rotation)
{
    m_orientation = rotation;
    emit orientationChanged(m_orientation);
}
