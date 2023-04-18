#ifndef DBUSCONTAINERSTATE_H
#define DBUSCONTAINERSTATE_H

#include <QObject>

#define FLATPAK_RUNNER_DBUS_CONT_IFACE "org.container"
#define FLATPAK_RUNNER_DBUS_CONT_SERVICE "org.flatpak.sailfish.container"

QT_BEGIN_NAMESPACE

class QDBusConnection;
class QDBusServer;

class Compositor;

class DBusContainerState : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", FLATPAK_RUNNER_DBUS_CONT_IFACE)

    Q_PROPERTY(int activeState READ activeState NOTIFY activeStateChanged)
    Q_PROPERTY(int orientation READ orientation NOTIFY orientationChanged)

public:
    DBusContainerState(Compositor *compositor);

    bool activeState() const { return true; }
    int orientation() const { return m_orientation; }

public slots:
    void keyboardRect(bool active, int x, int y, int width, int height);

signals:
    void activeStateChanged(bool state);
    void orientationChanged(int orientation);

private slots:
    friend class Compositor;
    void onNewConnection(const QDBusConnection &connection);
    void onWindowRotationChanged(int rotation);

private:
    Compositor *m_compositor;
    QDBusServer *m_server;
    int m_orientation = 0;
};

QT_END_NAMESPACE

#endif // DBUSCONTAINERSTATE_H
