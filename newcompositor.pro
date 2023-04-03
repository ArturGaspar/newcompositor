TEMPLATE = subdirs

SUBDIRS = \
    src/newcompositor.pro \
    hacks/hacks.pro

OTHER_FILES += \
    LICENSE \
    newcompositor.sh.in \
    rpm/newcompositor.spec

runner.path = /usr/bin
runner.files = newcompositor
INSTALLS += runner
