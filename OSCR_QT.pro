lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,9) {
    message("You need to Qt 5.9 or newer to build OSCR with Help Pages")
    lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,7) {
        error("You need Qt 5.7 or newer to build OSCR")
    }
}

TEMPLATE = subdirs

SUBDIRS += oscr

CONFIG += ordered
