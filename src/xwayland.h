#ifndef XWAYLAND_H
#define XWAYLAND_H

#include <QByteArray>
#include <QProcess>

QT_BEGIN_NAMESPACE

class QWaylandCompositor;
struct xcb_connection_t;
struct wl_client;

class Xwayland : public QProcess
{
    Q_OBJECT
public:
    Xwayland(QWaylandCompositor *compositor);
    ~Xwayland();
    void start();
    QByteArray displayName() const { return m_displayName; }
    int wmFd() const { return m_wmFd; }

signals:
    void displayReady();

private slots:
    void displayPipeReadyRead(int fd);

private:
    QWaylandCompositor *m_compositor;
    QByteArray m_displayName;
    int m_wmFd;
    struct wl_client *m_wlClient;
};

QT_END_NAMESPACE

#endif // XWAYLAND_H
