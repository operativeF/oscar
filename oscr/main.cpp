/* SleepyHead Main
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QTranslator>
#include <QSettings>
#include <QFileDialog>
#include <QFontDatabase>

#include "version.h"
#include "logger.h"
#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "translation.h"

// Gah! I must add the real darn plugin system one day.
#include "SleepLib/loader_plugins/prs1_loader.h"
#include "SleepLib/loader_plugins/cms50_loader.h"
#include "SleepLib/loader_plugins/cms50f37_loader.h"
#include "SleepLib/loader_plugins/md300w1_loader.h"
#include "SleepLib/loader_plugins/zeo_loader.h"
#include "SleepLib/loader_plugins/somnopose_loader.h"
#include "SleepLib/loader_plugins/resmed_loader.h"
#include "SleepLib/loader_plugins/intellipap_loader.h"
#include "SleepLib/loader_plugins/icon_loader.h"
#include "SleepLib/loader_plugins/weinmann_loader.h"


#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

MainWindow *mainwin = nullptr;

int compareVersion(QString version);



int main(int argc, char *argv[])
{
#ifdef Q_WS_X11
    XInitThreads();
#endif

    QCoreApplication::setApplicationName(getAppName());
    QCoreApplication::setOrganizationName(getDeveloperName());

    QSettings settings;
    GFXEngine gfxEngine = (GFXEngine)qMin((unsigned int)settings.value(GFXEngineSetting, (unsigned int)GFX_OpenGL).toUInt(), (unsigned int)MaxGFXEngine);

    switch (gfxEngine) {
    case 0:
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        break;
    case 1:
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        break;
    case 2:
    default:
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    }

    QString lastlanguage = settings.value(LangSetting, "").toString();

    bool dont_load_profile = false;
    bool force_data_dir = false;
    bool changing_language = false;
    QString load_profile = "";

    QApplication a(argc, argv);
    QStringList args = a.arguments();


    if (lastlanguage.isEmpty())
        changing_language = true;

    for (int i = 1; i < args.size(); i++) {
        if (args[i] == "-l") { dont_load_profile = true; }
        else if (args[i] == "-d") { force_data_dir = true; }
        else if (args[i] == "--language") {
            changing_language = true;

            // reset to force language dialog
            settings.setValue(LangSetting,"");
        } else if (args[i] == "-p") {
            QThread::msleep(1000);
        } else if (args[i] == "--profile") {
            if ((i+1) < args.size()) {
                load_profile = args[++i];
            } else {
                fprintf(stderr, "Missing argument to --profile\n");
                exit(1);
            }
        } else if (args[i] == "--datadir") { // mltam's idea
            QString datadir ;
            if ((i+1) < args.size()) {
              datadir = args[++i];
              settings.setValue("Settings/AppRoot", datadir);
            } else {
              fprintf(stderr, "Missing argument to --datadir\n");
              exit(1);
            }
          }
    }


    initializeLogger();
    QThread::msleep(50); // Logger takes a little bit to catch up
#ifdef QT_DEBUG
    QString relinfo = " debug";
#else
    QString relinfo = "";
#endif
    relinfo = "("+QSysInfo::kernelType()+" "+QSysInfo::currentCpuArchitecture()+relinfo+")";
    qDebug() << STR_AppName.toLocal8Bit().data() << VersionString.toLocal8Bit().data() << relinfo.toLocal8Bit().data() << "built with Qt" << QT_VERSION_STR << "on" << __DATE__ << __TIME__;

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Language Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
    initTranslations();

    initializeStrings(); // This must be called AFTER translator is installed, but before mainwindow is setup

//    QFontDatabase::addApplicationFont("://fonts/FreeSans.ttf");
//    a.setFont(QFont("FreeSans", 11, QFont::Normal, false));

    mainwin = new MainWindow;

    ////////////////////////////////////////////////////////////////////////////////////////////
    // OpenGL Detection
    ////////////////////////////////////////////////////////////////////////////////////////////
    getOpenGLVersion();
    getOpenGLVersionString();

    //bool opengl2supported = glversion >= 2.0;
    //bool bad_graphics = !opengl2supported;
    //bool intel_graphics = false;
//#ifndef NO_OPENGL_BUILD

//#endif

    /*
#ifdef BROKEN_OPENGL_BUILD
    Q_UNUSED(bad_graphics)
    Q_UNUSED(intel_graphics)

    const QString BetterBuild = "Settings/BetterBuild";

    if (opengl2supported) {
        if (!settings.value(BetterBuild, false).toBool()) {
            QMessageBox::information(nullptr, QObject::tr("A faster build of SleepyHead may be available"),
                             QObject::tr("This build of SleepyHead is a compatability version that also works on computers lacking OpenGL 2.0 support.")+"<br/><br/>"+
                             QObject::tr("However it looks like your computer has full support for OpenGL 2.0!") + "<br/><br/>"+
                             QObject::tr("This version will run fine, but a \"<b>%1</b>\" tagged build of SleepyHead will likely run a bit faster on your computer.").arg("-OpenGL")+"<br/><br/>"+
                             QObject::tr("You will not be bothered with this message again."), QMessageBox::Ok, QMessageBox::Ok);
            settings.setValue(BetterBuild, true);
        }
    }
#else
    if (bad_graphics) {
        QMessageBox::warning(nullptr, QObject::tr("Incompatible Graphics Hardware"),
                             QObject::tr("This build of SleepyHead requires OpenGL 2.0 support to function correctly, and unfortunately your computer lacks this capability.") + "<br/><br/>"+
                             QObject::tr("You may need to update your computers graphics drivers from the GPU makers website. %1").
                                arg(intel_graphics ? QObject::tr("(<a href='http://intel.com/support'>Intel's support site</a>)") : "")+"<br/><br/>"+
                             QObject::tr("Because graphs will not render correctly, and it may cause crashes, this build will now exit.")+"<br/><br/>"+
                             QObject::tr("There is another build available tagged \"<b>-BrokenGL</b>\" that should work on your computer.")
                             ,QMessageBox::Ok, QMessageBox::Ok);
        exit(1);
    }
#endif
*/
    ////////////////////////////////////////////////////////////////////////////////////////////
    // Datafolder location Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
    bool change_data_dir = force_data_dir;

    bool havefolder = false;

    if (!settings.contains("Settings/AppRoot")) {
        change_data_dir = true;
    } else {
        QDir dir(GetAppRoot());

        if (!dir.exists()) {
            change_data_dir = true;
        } else { havefolder = true; }
    }

    if (!havefolder && !force_data_dir) {
        if (QMessageBox::question(nullptr, STR_MessageBox_Question,
                QObject::tr("Would you like SleepyHead to use this location for storing its data?")+"\n\n"+
                QDir::toNativeSeparators(GetAppRoot())+"\n\n"+
                QObject::tr("If you are upgrading, don't panic, you just need to make sure this is pointed at your old SleepyHead data folder.")+"\n\n"+
                QObject::tr("(If you have no idea what to do here, just click yes.)"), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            settings.setValue("Settings/AppRoot", GetAppRoot());
            change_data_dir = false;
        }
    }

retry_directory:

    if (change_data_dir) {
        QString datadir = QFileDialog::getExistingDirectory(nullptr,
                          QObject::tr("Choose or create new folder for SleepyHead data"), GetAppRoot(),
                          QFileDialog::ShowDirsOnly);

        if (datadir.isEmpty()) {
            if (!havefolder) {
                QMessageBox::information(nullptr, QObject::tr("Exiting"),
                                         QObject::tr("As you did not select a data folder, SleepyHead will exit.")+"\n\n"+QObject::tr("Next time you run, you will be asked again."));
                return 0;
            } else {
                QMessageBox::information(nullptr, STR_MessageBox_Warning,
                                         QObject::tr("You did not select a directory.")+"\n\n"+QObject::tr("SleepyHead will now start with your old one.")+"\n\n"+
                                         QDir::toNativeSeparators(GetAppRoot()), QMessageBox::Ok);
            }
        } else {
            QDir dir(datadir);
            QFile file(datadir + "/Preferences.xml");

            if (!file.exists()) {
                if (dir.count() > 2) {
                    // Not a new directory.. nag the user.
                    if (QMessageBox::question(nullptr, STR_MessageBox_Warning,
                                              QObject::tr("The folder you chose is not empty, nor does it already contain valid SleepyHead data.")
                                              + "\n\n"+QObject::tr("Are you sure you want to use this folder?")+"\n\n"
                                              + datadir, QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
                        goto retry_directory;
                    }
                }
            }

            settings.setValue("Settings/AppRoot", datadir);
            qDebug() << "Changing data folder to" << datadir;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Initialize preferences system (Don't use PREF before this point)
    ///////////////////////////////////////////////////////////////////////////////////////////
    p_pref = new Preferences("Preferences");
    PREF.Open();
    AppSetting = new AppWideSetting(p_pref);

    QString language = settings.value(LangSetting, "").toString();
    AppSetting->setLanguage(language);

    // Clean up some legacy crap
    QFile lf(PREF.Get("{home}/Layout.xml"));
    if (lf.exists()) {
        lf.remove();
    }

    PREF.Erase(STR_AppName);
    PREF.Erase(STR_GEN_SkipLogin);

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Check when last checked for updates..
    ////////////////////////////////////////////////////////////////////////////////////////////
    QDateTime lastchecked, today = QDateTime::currentDateTime();

    bool check_updates = false;

    if (AppSetting->updatesAutoCheck()) {
        int update_frequency = AppSetting->updateCheckFrequency();
        int days = 1000;
        lastchecked = AppSetting->updatesLastChecked();

        if (lastchecked.isValid()) {
            days = lastchecked.secsTo(today);
            days /= 86400;
        }

        if (days > update_frequency) {
            check_updates = true;
        }
    }

    int vc = compareVersion(AppSetting->versionString());
    if (vc < 0) {
        AppSetting->setShowAboutDialog(1);
        //release_notes();

        check_updates = false;
    } else if (vc > 0) {
        if (QMessageBox::warning(nullptr, STR_MessageBox_Error,
            QObject::tr("The version of SleepyHead you just ran is OLDER than the one used to create this data (%1).").
                        arg(AppSetting->versionString()) +"\n\n"+
            QObject::tr("It is likely that doing this will cause data corruption, are you sure you want to do this?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {

            return 0;
        }

    }

    AppSetting->setVersionString(VersionString);

    //    int id=QFontDatabase::addApplicationFont(":/fonts/FreeSans.ttf");
    //    QFontDatabase fdb;
    //    QStringList ffam=fdb.families();
    //    for (QStringList::iterator i=ffam.begin();i!=ffam.end();i++) {
    //        qDebug() << "Loaded Font: " << (*i);
    //    }


    if (!PREF.contains("Fonts_Application_Name")) {
#ifdef Q_OS_WIN
        // Windows default Sans Serif interpretation sucks
        // Segoe UI is better, but that requires OS/font detection
        PREF["Fonts_Application_Name"] = "Arial";
#else
        PREF["Fonts_Application_Name"] = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#endif
        PREF["Fonts_Application_Size"] = 10;
        PREF["Fonts_Application_Bold"] = false;
        PREF["Fonts_Application_Italic"] = false;
    }


    QApplication::setFont(QFont(PREF["Fonts_Application_Name"].toString(),
                                PREF["Fonts_Application_Size"].toInt(),
                                PREF["Fonts_Application_Bold"].toBool() ? QFont::Bold : QFont::Normal,
                                PREF["Fonts_Application_Italic"].toBool()));

    qDebug() << "Selected Font" << QApplication::font().family();

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Register Importer Modules for autoscanner
    ////////////////////////////////////////////////////////////////////////////////////////////
    schema::init();
    PRS1Loader::Register();
    ResmedLoader::Register();
    IntellipapLoader::Register();
    FPIconLoader::Register();
    WeinmannLoader::Register();
    CMS50Loader::Register();
    CMS50F37Loader::Register();
    MD300W1Loader::Register();

    schema::setOrders(); // could be called in init...

    // Scan for user profiles
    Profiles::Scan();

    Q_UNUSED(changing_language)
    Q_UNUSED(dont_load_profile)


    if (check_updates) { mainwin->CheckForUpdates(); }

    mainwin->SetupGUI();
    mainwin->show();

    return a.exec();
}
