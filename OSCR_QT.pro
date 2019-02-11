lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,9) {
    error("You need to Qt 5.9 or newer to build SleepyHead");
}

TEMPLATE = subdirs

SUBDIRS += sleepyhead

CONFIG += ordered
