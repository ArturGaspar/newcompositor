TEMPLATE = subdirs

SUBDIRS = \
    src/newcompositor.pro \
    hacks/hacks.pro

OTHER_FILES += \
    LICENSE \
    newcompositor.sh.in \
    rpm/newcompositor.spec

DISTFILES += \
    service/newcompositor.service

runner.path = /usr/bin
runner.files = newcompositor
INSTALLS += runner

service.path = /usr/lib/systemd/user
service.files = service/newcompositor.service
INSTALLS += service
