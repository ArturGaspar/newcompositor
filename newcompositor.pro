TEMPLATE = subdirs

SUBDIRS = \
    src/newcompositor.pro \
    hacks/hacks.pro

OTHER_FILES += \
    newcompositor.sh.in \
    rpm/newcompositor.spec

runner.path = /usr/bin
runner.files = newcompositor
INSTALLS += runner
