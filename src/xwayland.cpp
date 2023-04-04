#include "xwayland.h"

#include <QDebug>
#include <QFile>
#include <QSocketNotifier>
#include <QWaylandCompositor>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-server-core.h>

Xwayland::Xwayland(QWaylandCompositor *compositor)
    : QProcess(compositor)
    , m_compositor(compositor)
    , m_displayName(":")
{
    setProcessChannelMode(QProcess::ForwardedChannels);
    setProgram("Xwayland");
}

Xwayland::~Xwayland()
{
    if (m_wlClient) {
        ::wl_client_destroy(m_wlClient);
    }
}

void Xwayland::start()
{
    int waylandFd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, waylandFd) != 0) {
        qCritical() << "error creating Xwayland Wayland client socket pair:"
                    << ::strerror(errno);
        return;
    }
    m_wlClient = ::wl_client_create(m_compositor->display(), waylandFd[0]);

    int wmFd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, wmFd) != 0) {
        qCritical() << "error creating Xwayland XWM socket pair:"
                    << ::strerror(errno);
        return;
    }
    m_wmFd = wmFd[0];

    int displayFd[2];
    if (::pipe(displayFd) != 0) {
        qCritical() << "error creating Xwayland display pipe:"
                    << ::strerror(errno);
        return;
    }
    auto *notifier = new QSocketNotifier(displayFd[0], QSocketNotifier::Read,
                                         this);
    connect(notifier, &QSocketNotifier::activated,
            this, &Xwayland::displayPipeReadyRead);

    setArguments(QStringList()
                 << "-rootless"
                 << "-displayfd"
                 << QString::number(displayFd[1])
                 << "-wm"
                 << QString::number(wmFd[1]));
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_SOCKET", QString::number(waylandFd[1]));
    setProcessEnvironment(env);
    QProcess::start();

    ::close(displayFd[1]);
    ::close(waylandFd[1]);
    ::close(wmFd[1]);
}

void Xwayland::displayPipeReadyRead(int fd)
{
    auto *notifier = qobject_cast<QSocketNotifier *>(sender());

    QFile displayPipe;
    if (!displayPipe.open(fd, QIODevice::ReadOnly)) {
        notifier->deleteLater();
        qFatal("error opening Xwayland display pipe: %s",
               qPrintable(displayPipe.errorString()));
        return;
    }

    const QByteArray out = displayPipe.readAll();
    for (const char c : out) {
        if (c == '\n') {
            notifier->deleteLater();
            displayPipe.close();
            qInfo("Xwayland running on DISPLAY=%s", m_displayName.constData());
            emit displayReady();
            break;
        }
        m_displayName.append(c);
    }
}
