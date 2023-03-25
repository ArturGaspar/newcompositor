TEMPLATE = subdirs

SUBDIRS = \
    src/newcompositor.pro \
    hacks/hacks.pro

OTHER_FILES += \
    newcompositor.sh \
    rpm/newcompositor.spec

runner.path = /opt/newcompositor/bin
runner.extra = cp newcompositor.sh newcompositor
runner.files = newcompositor
INSTALLS += runner
