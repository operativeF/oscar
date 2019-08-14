#-------------------------------------------------
#
# Project created by QtCreator 2011-06-20T22:05:30
#
#-------------------------------------------------

message(Platform is $$QMAKESPEC )

lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,9) {
    message("You need Qt 5.9 to build OSCAR with Help Pages")
    DEFINES += helpless
}
lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,7) {
    error("You need Qt 5.7 or newer to build OSCAR");
}

# get rid of the help browser, at least for now
DEFINES += helpless

QT += core gui network xml printsupport serialport widgets help
contains(DEFINES, helpless) {
    QT -= help
}

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += NO_UPDATER

#OSCAR requires OpenGL 2.0 support to run smoothly
#On platforms where it's not available, it can still be built to work
#provided the BrokenGL DEFINES flag is passed to qmake (eg, qmake [specs] /path/to/OSCAR_QT.pro DEFINES+=BrokenGL) (hint, Projects button on the left)
contains(DEFINES, NoGL) {
    message("Building with QWidget gGraphView to support systems without ANY OpenGL")
    DEFINES += BROKEN_OPENGL_BUILD
    DEFINES += NO_OPENGL_BUILD
} else:contains(DEFINES, BrokenGL) {
    DEFINES += BROKEN_OPENGL_BUILD
    message("Building with QWidget gGraphView to support systems with legacy graphics")
    DEFINES-=BrokenGL
} else {
    QT += opengl
    message("Building with regular OpenGL gGraphView")
}

DEFINES += LOCK_RESMED_SESSIONS

CONFIG += c++11
CONFIG += rtti
CONFIG -= debug_and_release

contains(DEFINES, STATIC) {
    static {
        CONFIG += static
        QTPLUGIN += qgif qpng

        message("Static build.")
    }
}

TARGET = OSCAR
unix:!macx:!haiku {
    TARGET.path=/usr/bin
}

TEMPLATE = app

gitinfotarget.target = git_info.h
gitinfotarget.depends = FORCE

win32 {
    system("$$_PRO_FILE_PWD_/update_gitinfo.bat");
    message("Updating gitinfo.h for Windows build")
    gitinfotarget.commands = "$$_PRO_FILE_PWD_/update_gitinfo.bat"
} else {
    system("/bin/bash $$_PRO_FILE_PWD_/update_gitinfo.sh");
    message("Updating gitinfo.h for non-Windows build")
    gitinfotarget.commands = "/bin/bash $$_PRO_FILE_PWD_/update_gitinfo.sh"
}

PRE_TARGETDEPS += git_info.h
QMAKE_EXTRA_TARGETS += gitinfotarget

!contains(DEFINES, helpless) {
#Build the help documentation
    message("Generating help files");
    qtPrepareTool(QCOLGENERATOR, qcollectiongenerator)

    command=$$QCOLGENERATOR $$PWD/help/index.qhcp -o $$PWD/help/index.qhc
    system($$command)|error("Failed to run: $$command")
    message("Finished generating help files");
}

QMAKE_TARGET_PRODUCT = OSCAR
QMAKE_TARGET_COMPANY = The OSCAR Team
QMAKE_TARGET_COPYRIGHT = © 2019 The OSCAR Team
QMAKE_TARGET_DESCRIPTION = "OpenSource CPAP Analysis Reporter"
VERSION = 0.0.0.0
RC_ICONS = ./icons/logo.ico

macx {
  QMAKE_TARGET_BUNDLE_PREFIX = "org.oscar-team"
# QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.12
  LIBS             += -lz
  ICON              = icons/OSCAR.icns
} else:haiku {
    LIBS            += -lz -lGLU
    DEFINES         += _TTY_POSIX_
} else:unix {
    LIBS            += -lX11 -lz -lGLU
    DEFINES         += _TTY_POSIX_
} else:win32 {
    DEFINES          += WINVER=0x0501 # needed for mingw to pull in appropriate dbt business...probably a better way to do this
    LIBS             += -lsetupapi

    INCLUDEPATH += $$PWD
    INCLUDEPATH += $$[QT_INSTALL_PREFIX]/../src/qtbase/src/3rdparty/zlib

    if (*-msvc*):!equals(TEMPLATE_PREFIX, "vc") {
        LIBS += -ladvapi32
    } else {
        # MingW needs this
        LIBS += -lz
    }

    if (*-msvc*) {
        CONFIG += precompile_header
        PRECOMPILED_HEADER = pch.h
        HEADERS += pch.h

    }

    CONFIG(release, debug|release) {
        contains(DEFINES, OfficialBuild) {
           QMAKE_POST_LINK += "$$PWD/../../scripts/release_tool.sh --testing --source \"$$PWD/..\" --binary \"$${OUT_PWD}/$${TARGET}.exe\""
        }
    }
}

TRANSLATIONS = $$files($$PWD/../Translations/*.ts)

qtPrepareTool(LRELEASE, lrelease)

for(file, TRANSLATIONS) {

 qmfile = $$absolute_path($$basename(file), $$PWD/translations/)
 qmfile ~= s,.ts$,.qm,

 qmdir = $$PWD/translations

 !exists($$qmdir) {
     mkpath($$qmdir)|error("Aborting.")
 }
 qmout = $$qmfile
 command = $$LRELEASE -removeidentical $$file -qm $$qmfile
 system($$command)|error("Failed to run: $$command")
 TRANSLATIONS_FILES += $$qmfile
}

HTML_FILES = $$files($$PWD/../Htmldocs/*.html)

#copy the Translation and Help files to where the test binary wants them
message("Setting up Translations & Help Transfers")
macx {
    !contains(DEFINES, helpless) {
        HelpFiles.files = $$files($$PWD/help/*.qch)
        HelpFiles.path = Contents/Resources/Help
        QMAKE_BUNDLE_DATA += HelpFiles
    }
    QMAKE_BUNDLE_DATA += TransFiles
} else {
    !contains(DEFINES, helpless) {
        HELPDIR = $$OUT_PWD/Help
        HELP_FILES += $$PWD/help/*.qch
    }
    DDIR = $$OUT_PWD/Translations
    HTMLDIR = $$OUT_PWD/Html

    TRANS_FILES += $$PWD/translations/*.qm

    win32 {
        TRANS_FILES_WIN = $${TRANS_FILES}
        TRANS_FILES_WIN ~= s,/,\\,g
        DDIR ~= s,/,\\,g
        !exists($$quote($$DDIR)): system(mkdir $$quote($$DDIR))
        for(FILE,TRANS_FILES_WIN) {
            system(xcopy /y $$quote($$FILE) $$quote($$DDIR))
        }

        HTML_FILES_WIN = $${HTML_FILES}
        HTML_FILES_WIN ~= s,/,\\,g
        HTMLDIR ~= s,/,\\,g
        !exists($$quote($$HTMLDIR)): system(mkdir $$quote($$HTMLDIR))
        for(FILE,HTML_FILES_WIN) {
            system(xcopy /y $$quote($$FILE) $$quote($$HTMLDIR))
        }

        !contains(DEFINES, helpless) {
            HELP_FILES_WIN = $${HELP_FILES}
            HELP_FILES_WIN ~= s,/,\\,g
            HELPDIR ~= s,/,\\,g
            !exists($$quote($$HELPDIR)): system(mkdir $$quote($$HELPDIR))
            for(FILE,HELP_FILES_WIN) {
                system(xcopy /y $$quote($$FILE) $$quote($$HELPDIR))
            }
        }
    } else {
        system(mkdir -p $$quote($$DDIR))
        for(FILE,TRANS_FILES) {
            system(cp $$quote($$FILE) $$quote($$DDIR))
        }

        system(mkdir -p $$quote($$HTMLDIR))
        for(FILE,HTML_FILES) {
            system(cp $$quote($$FILE) $$quote($$HTMLDIR))
        }

        !contains(DEFINES, helpless) {
            system(mkdir -p $$quote($$HELPDIR))
            for(FILE,HELP_FILES) {
                system(cp $$quote($$FILE) $$quote($$HELPDIR))
            }
        }
    }
}

lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,12) {
    unix {
        system("/bin/bash $$_PRO_FILE_PWD_/fix_5-12_UI_files.sh");
        message("Fixing UI files for old QT versions")
    }
}

SOURCES += \
    common_gui.cpp \
    cprogressbar.cpp \
    daily.cpp \
    exportcsv.cpp \
    main.cpp \
    mainwindow.cpp \
    newprofile.cpp \
    overview.cpp \
    preferencesdialog.cpp \
    profileselect.cpp \
    reports.cpp \
    sessionbar.cpp \
    updateparser.cpp \
    UpdaterWindow.cpp \
    Graphs/gFlagsLine.cpp \
    Graphs/gFooBar.cpp \
    Graphs/gGraph.cpp \
    Graphs/gGraphView.cpp \
    Graphs/glcommon.cpp \
    Graphs/gLineChart.cpp \
    Graphs/gLineOverlay.cpp \
    Graphs/gSegmentChart.cpp \
    Graphs/gspacer.cpp \
    Graphs/gStatsLine.cpp \
    Graphs/gSummaryChart.cpp \
    Graphs/gXAxis.cpp \
    Graphs/gYAxis.cpp \
    Graphs/layer.cpp \
    SleepLib/calcs.cpp \
    SleepLib/common.cpp \
    SleepLib/day.cpp \
    SleepLib/event.cpp \
    SleepLib/machine.cpp \
    SleepLib/machine_loader.cpp \
    SleepLib/preferences.cpp \
    SleepLib/profiles.cpp \
    SleepLib/schema.cpp \
    SleepLib/session.cpp \
    SleepLib/loader_plugins/cms50_loader.cpp \
    SleepLib/loader_plugins/icon_loader.cpp \
    SleepLib/loader_plugins/intellipap_loader.cpp \
    SleepLib/loader_plugins/mseries_loader.cpp \
    SleepLib/loader_plugins/prs1_loader.cpp \
    SleepLib/loader_plugins/resmed_loader.cpp \
    SleepLib/loader_plugins/somnopose_loader.cpp \
    SleepLib/loader_plugins/zeo_loader.cpp \
    translation.cpp \
    statistics.cpp \
    oximeterimport.cpp \
    SleepLib/serialoximeter.cpp \
    SleepLib/loader_plugins/md300w1_loader.cpp \
    Graphs/gSessionTimesChart.cpp \
    logger.cpp \
    SleepLib/machine_common.cpp \
    SleepLib/loader_plugins/weinmann_loader.cpp \
    Graphs/gdailysummary.cpp \
    Graphs/MinutesAtPressure.cpp \
    SleepLib/journal.cpp \
    SleepLib/progressdialog.cpp \
    SleepLib/loader_plugins/cms50f37_loader.cpp \
    profileselector.cpp \
    SleepLib/loader_plugins/edfparser.cpp \
    aboutdialog.cpp \
    welcome.cpp
!contains(DEFINES, helpless) {
    SOURCES += help.cpp
}

HEADERS  += \
    common_gui.h \
    cprogressbar.h \
    daily.h \
    exportcsv.h \
    mainwindow.h \
    newprofile.h \
    overview.h \
    preferencesdialog.h \
    profileselect.h \
    reports.h \
    sessionbar.h \
    updateparser.h \
    UpdaterWindow.h \
    version.h \
    Graphs/gFlagsLine.h \
    Graphs/gFooBar.h \
    Graphs/gGraph.h \
    Graphs/gGraphView.h \
    Graphs/glcommon.h \
    Graphs/gLineChart.h \
    Graphs/gLineOverlay.h \
    Graphs/gSegmentChart.h\
    Graphs/gspacer.h \
    Graphs/gStatsLine.h \
    Graphs/gSummaryChart.h \
    Graphs/gXAxis.h \
    Graphs/gYAxis.h \
    Graphs/layer.h \
    SleepLib/calcs.h \
    SleepLib/common.h \
    SleepLib/day.h \
    SleepLib/event.h \
    SleepLib/machine.h \
    SleepLib/machine_common.h \
    SleepLib/machine_loader.h \
    SleepLib/preferences.h \
    SleepLib/profiles.h \
    SleepLib/schema.h \
    SleepLib/session.h \
    SleepLib/loader_plugins/cms50_loader.h \
    SleepLib/loader_plugins/icon_loader.h \
    SleepLib/loader_plugins/intellipap_loader.h \
    SleepLib/loader_plugins/mseries_loader.h \
    SleepLib/loader_plugins/prs1_loader.h \
    SleepLib/loader_plugins/resmed_loader.h \
    SleepLib/loader_plugins/somnopose_loader.h \
    SleepLib/loader_plugins/zeo_loader.h \
    translation.h \
    statistics.h \
    oximeterimport.h \
    SleepLib/serialoximeter.h \
    SleepLib/loader_plugins/md300w1_loader.h \
    Graphs/gSessionTimesChart.h \
    logger.h \
    SleepLib/loader_plugins/weinmann_loader.h \
    Graphs/gdailysummary.h \
    Graphs/MinutesAtPressure.h \
    SleepLib/journal.h \
    SleepLib/progressdialog.h \
    SleepLib/loader_plugins/cms50f37_loader.h \
    build_number.h \
    profileselector.h \
    SleepLib/appsettings.h \
    SleepLib/loader_plugins/edfparser.h \
    aboutdialog.h \
    welcome.h \
    mytextbrowser.h \
    git_info.h
!contains(DEFINES, helpless) {
    HEADERS += help.h
}

FORMS += \
    daily.ui \
    overview.ui \
    mainwindow.ui \
    oximetry.ui \
    preferencesdialog.ui \
    profileselect.ui \
    newprofile.ui \
    exportcsv.ui \
    UpdaterWindow.ui \
    oximeterimport.ui \
    profileselector.ui \
    aboutdialog.ui \
    welcome.ui
!contains(DEFINES, helpless) {
    FORMS += help.ui
}
equals(QT_MAJOR_VERSION,5) {
    lessThan(QT_MINOR_VERSION,12) {
        FORMS += reports.ui
    }
}

RESOURCES += \
    Resources.qrc

OTHER_FILES += \
    docs/index.html \
    docs/schema.xml \
    docs/graphs.xml \
    docs/channels.xml \
    docs/release_notes.html \
    docs/about.html \
    docs/credits.html \
    docs/startup_tips.txt \
    docs/countries.txt \
    docs/tz.txt \
    ../LICENSE.txt \
    docs/tooltips.css \
    docs/script.js \
    ../update.xml \
    docs/changelog.txt \
    docs/intro.html \
    docs/statistics.xml \
    update_gitinfo.bat \
    update_gitinfo.sh

DISTFILES += \
    ../README
!contains(DEFINES, helpless) {
DISTFILES += help/default.css \
    help/help_en/daily.html \
    help/help_en/glossary.html \
    help/help_en/import.html \
    help/help_en/index.html \
    help/help_en/overview.html \
    help/help_en/oximetry.html \
    help/help_en/statistics.html \
    help/help_en/supported.html \
    help/help_en/gettingstarted.html \
    help/help_en/tipsntricks.html \
    help/help_en/faq.html \
    help/help_nl/daily.html \
    help/help_nl/faq.html \
    help/help_nl/gettingstarted.html \
    help/help_nl/glossary.html \
    help/help_nl/import.html \
    help/help_nl/index.html \
    help/help_nl/overview.html \
    help/help_nl/oximetry.html \
    help/help_nl/statistics.html \
    help/help_nl/supported.html \
    help/help_nl/tipsntricks.html \
    help/help_en/reportingbugs.html \
    help/help_nl/OSCAR_Guide_nl.qhp \
    help/help_en/OSCAR_Guide_en.qhp \
    help/index.qhcp
}


# Always treat warnings as errors, even (especially!) in release
QMAKE_CFLAGS += -Werror
QMAKE_CXXFLAGS += -Werror

# Make deprecation warnings just warnings
QMAKE_CFLAGS += -Wno-error=deprecated-declarations
QMAKE_CXXFLAGS += -Wno-error=deprecated-declarations


# Create a debug GUI build by adding "CONFIG+=memdebug" to your qmake command
memdebug {
    !win32 {  # add memory checking on Linux and macOS debug builds
        QMAKE_CFLAGS += -g -Werror -fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope
        QMAKE_CXXFLAGS += -g -Werror -fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope
        QMAKE_LFLAGS += -fsanitize=address
    }
}

# Turn on unit testing by adding "CONFIG+=test" to your qmake command
test {
    TARGET = test
    DEFINES += UNITTEST_MODE

    QT += testlib
    QT -= gui
    CONFIG += console debug
    CONFIG -= app_bundle
    !win32 {  # add memory checking on Linux and macOS test builds
        QMAKE_CFLAGS += -Werror -fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope
        QMAKE_CXXFLAGS += -Werror -fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope
        QMAKE_LFLAGS += -fsanitize=address
    }

    SOURCES += \
        tests/prs1tests.cpp \
        tests/resmedtests.cpp \
        tests/sessiontests.cpp

    HEADERS += \
        tests/AutoTest.h \
        tests/prs1tests.h \
        tests/resmedtests.h \
        tests/sessiontests.h
}

# On macOS put a custom Info.plist into the bundle that disables dark mode on Mojave
macx {
    QMAKE_INFO_PLIST = "../Building/MacOS/Info.plist.in"
}
