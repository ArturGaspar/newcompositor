TEMPLATE = subdirs

SUBDIRS = \
    src/newcompositor.pro \
    hacks/hacks.pro

OTHER_FILES += \
    LICENSE \
    newcompositor.sh.in \
    rpm/newcompositor.spec

DISTFILES += \
    qt-runner \
    service/newcompositor.service

runner.path = /usr/bin
runner.files = newcompositor
INSTALLS += runner

service.path = /usr/lib/systemd/user
service.files = service/newcompositor.service
INSTALLS += service

qt-runner-compat {
    qt-runner.path = /usr/bin
    qt-runner.files = qt-runner
    INSTALLS += qt-runner
}
