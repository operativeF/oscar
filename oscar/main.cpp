/* OSCAR Main
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifdef UNITTEST_MODE
#include "tests/AutoTest.h"
#endif

#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QDebug>
#include <QTranslator>
#include <QSettings>
#include <QFileDialog>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QProgressDialog>

#include "version.h"
#include "logger.h"
#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "translation.h"
#include "SleepLib/common.h"

#include <ctime>
#include <chrono>

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

MainWindow *mainwin = nullptr;

int compareVersion(QString version);

int numFilesCopied = 0;

// Count the number of files in this directory and all subdirectories
int countRecursively(QString sourceFolder) {
    QDir sourceDir(sourceFolder);

    if(!sourceDir.exists())
        return 0;

    int numFiles = sourceDir.count();

    QStringList dirs = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for(int i = 0; i< dirs.count(); i++) {
        QString srcName = sourceFolder + QDir::separator() + dirs[i];
        numFiles += countRecursively(srcName);
    }

    return numFiles;
}

bool copyRecursively(QString sourceFolder, QString destFolder, QProgressDialog& progress) {
    bool success = false;
    QDir sourceDir(sourceFolder);

    if(!sourceDir.exists())
        return false;

    QDir destDir(destFolder);
    if(!destDir.exists())
        destDir.mkdir(destFolder);

    QStringList files = sourceDir.entryList(QDir::Files);
    for(int i = 0; i< files.count(); i++) {
        QString srcName = sourceFolder + QDir::separator() + files[i];
        QString destName = destFolder + QDir::separator() + files[i];
        success = QFile::copy(srcName, destName);
        numFilesCopied++;
        if ((numFilesCopied % 20) == 1) { // Update progress bar every 20 files
            progress.setValue(numFilesCopied);
            QCoreApplication::processEvents();
        }
        if(!success)
            return false;
    }

    files.clear();
    files = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for(int i = 0; i< files.count(); i++) {
        QString srcName = sourceFolder + QDir::separator() + files[i];
        QString destName = destFolder + QDir::separator() + files[i];
//      qDebug() << "Copy from "+srcName+" to "+destName;
        success = copyRecursively(srcName, destName, progress);
        if(!success)
            return false;
    }

    return true;
}

bool processPreferenceFile( QString path ) {
    bool success = true;
    QString fullpath = path + "/Preferences.xml";
    qDebug() << "Process " + fullpath;
    QFile fl(fullpath);
    QFile tmp(fullpath+".tmp");
    QString line;
    fl.open(QIODevice::ReadOnly);
    tmp.open(QIODevice::WriteOnly);
    QTextStream instr(&fl);
    QTextStream outstr(&tmp);
    while (instr.readLineInto(&line)) {
        line.replace("SleepyHead","OSCAR");
        if (line.contains("VersionString")) {
            int rtAngle = line.indexOf(">", 0);
            int lfAngle = line.indexOf("<", rtAngle);
            line.replace(rtAngle+1, lfAngle-rtAngle-1, "1.0.0-beta");
        }
        outstr << line;
    }
    fl.remove();
    success = tmp.rename(fullpath);

    return success;
}

bool processFile( QString fullpath ) {
    bool success = true;
    qDebug() << "Process " + fullpath ;
    QFile fl(fullpath);
    QFile tmp(fullpath+".tmp");
    QString line;
    fl.open(QIODevice::ReadOnly);
    tmp.open(QIODevice::WriteOnly);
    QTextStream instr(&fl);
    QTextStream outstr(&tmp);
    while (instr.readLineInto(&line)) {
        if (line.contains("EnableMultithreading")) {
            if (line.contains("true")) {
                line.replace("true","false");
            }
        }
        line.replace("SleepyHead","OSCAR");
        outstr << line;
    }
    fl.remove();
    success = tmp.rename(fullpath);

    return success;
}

bool process_a_Profile( QString path ) {
    bool success = true;
    qDebug() << "Entering profile directory " + path;
    QDir dir(path);
    QStringList files = dir.entryList(QStringList("*.xml"), QDir::Files);
    for ( int i = 0; success && (i<files.count()); i++) {
        success = processFile( path + "/" + files[i] );
    }
    return success;
}

bool migrateFromSH(QString destDir) {
    QString homeDocs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/";
    QString datadir;
    bool selectingFolder = true;
    bool success = false;
//  long int startTime, countDone, allDone;

    if (destDir.isEmpty()) {
        qDebug() << "Migration path is empty string";
        return success;
    }

    while (selectingFolder) {
        datadir = QFileDialog::getExistingDirectory(nullptr,
                  QObject::tr("Choose the SleepyHead data folder to migrate")+" "+
                  QObject::tr("or CANCEL to skip migration."),
                  homeDocs, QFileDialog::ShowDirsOnly);
        qDebug() << "Migration folder selected: " + datadir;
        if (datadir.isEmpty()) {
            qDebug() << "No migration source directory selected";
            return false;
        } else {                        // We have a folder, see if is a SleepyHead folder
            QDir dir(datadir);
            QFile file(datadir + "/Preferences.xml");
            QDir  dirP(datadir + "/Profiles");

            if (!file.exists() || !dirP.exists()) {       // It doesn't have a Preferences.xml file or a Profiles directory in it
                // Not a new directory.. nag the user.
                if (QMessageBox::warning(nullptr, STR_MessageBox_Error,
                                         QObject::tr("The folder you chose does not contain valid SleepyHead data.") +
                                         "\n\n"+QObject::tr("You cannot use this folder:")+" " + datadir ), QMessageBox::Ok) {
                    continue;   // Nope, don't use it, go around the loop again
                }
            }

            qDebug() << "Migration folder is" << datadir;
            selectingFolder = false;
        }
    }

    auto startTime = std::chrono::steady_clock::now();
    int numFiles = countRecursively(datadir);       // count number of files to be copied
    auto countDone = std::chrono::steady_clock::now();
    qDebug() << "Number of files to migrate: " << numFiles;

    QProgressDialog progress (QObject::tr("Migrating ") + QString::number(numFiles) + QObject::tr(" files")+"\n"+
    						  QObject::tr("from ") + QDir(datadir).dirName() + "\n"+QObject::tr("to ") + 
    						  QDir(destDir).dirName(), QString(), 0, numFiles, 0, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    progress.setValue(0);
    progress.setMinimumWidth(300);
    progress.show();

    success = copyRecursively(datadir, destDir, progress);
    if (success) {
        qDebug() << "Finished copying " + datadir;
    }

    success = processPreferenceFile( destDir );

    QDir profDir(destDir+"/Profiles");
    QStringList names = profDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (int i = 0; success && (i < names.count()); i++) {
        success = process_a_Profile( destDir+"/Profiles/"+names[i] );
    }

    progress.setValue(numFiles);
    auto allDone = std::chrono::steady_clock::now();
    auto elapsedCount = std::chrono::duration_cast<std::chrono::microseconds>(countDone - startTime);
    auto elapsedCopy  = std::chrono::duration_cast<std::chrono::microseconds>(allDone - countDone);
    qDebug() << "Counting files took " << elapsedCount.count() << " microsecs";
    qDebug() << "Migrating files took " << elapsedCopy.count() << " microsecs";

    return success;
}

#ifdef UNITTEST_MODE

int main(int argc, char* argv[])
{
    AutoTest::run(argc, argv);
}

#else

int main(int argc, char *argv[]) {

    QString homeDocs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/";
    QCoreApplication::setApplicationName(getAppName());
    QCoreApplication::setOrganizationName(getDeveloperName());
    QCoreApplication::setOrganizationDomain(getDeveloperDomain());

    QSettings settings;

    QApplication a(argc, argv);
    QStringList args = a.arguments();

    // If shift key was held down when OSCAR was launched, force Software graphics Engine (aka LegacyGFX)
    Qt::KeyboardModifiers keymodifier = QApplication::queryKeyboardModifiers();
    QString forcedEngine = "";
    if (keymodifier == Qt::ShiftModifier){
        settings.setValue(GFXEngineSetting, (unsigned int)GFX_Software);
        forcedEngine = "Software Engine forced by shift key at launch";
    }

    QString lastlanguage = settings.value(LangSetting, "").toString();
    if (lastlanguage.compare("is", Qt::CaseInsensitive))    // Convert code for Hebrew from 'is' to 'he'
        lastlanguage = "he";

    bool dont_load_profile = false;
    bool force_data_dir = false;
    bool changing_language = false;
    QString load_profile = "";

    if (lastlanguage.isEmpty())
        changing_language = true;

    for (int i = 1; i < args.size(); i++) {
        if (args[i] == "-l")
            dont_load_profile = true;
//        else if (args[i] == "-d")
//            force_data_dir = true;
        else if (args[i] == "--language") {
            changing_language = true; // reset to force language dialog
            settings.setValue(LangSetting,"");
        }
        else if (args[i] == "--legacy") {
            settings.setValue(GFXEngineSetting, (unsigned int)GFX_Software);
            forcedEngine = "Software Engine forced by --legacy command line switch";
        }
        else if (args[i] == "-p")
            QThread::msleep(1000);
        else if (args[i] == "--profile") {
            if ((i+1) < args.size())
                load_profile = args[++i];
            else {
                fprintf(stderr, "Missing argument to --profile\n");
                exit(1);
            }
        } else if (args[i] == "--datadir") { // mltam's idea
            QString datadir ;
            if ((i+1) < args.size()) {
                datadir = args[++i];
                if (datadir.length() < 2 || datadir.at(1) != QLatin1Char(':'))  // Allow a Windows drive letter (but not UNC)
                    datadir = homeDocs+datadir;
                settings.setValue("Settings/AppData", datadir);
//            force_data_dir = true;
            } else {
                fprintf(stderr, "Missing argument to --datadir\n");
                exit(1);
            }
        }
    }   // end of for args loop


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

    initializeLogger();
    QThread::msleep(50); // Logger takes a little bit to catch up

#ifdef QT_DEBUG
    QString relinfo = " debug";
#else
    QString relinfo = "";
#endif
    relinfo = "("+QSysInfo::kernelType()+" "+QSysInfo::currentCpuArchitecture()+relinfo+")";
    qDebug() << "OSCAR starting" << QDateTime::currentDateTime();
    qDebug().noquote() << STR_AppName << VersionString << relinfo << "Built with Qt" << QT_VERSION_STR << __DATE__ << __TIME__;

    SetDateFormat();

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Language Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
    initTranslations();

    initializeStrings(); // This must be called AFTER translator is installed, but before mainwindow is setup

//    QFontDatabase::addApplicationFont("://fonts/FreeSans.ttf");
//    a.setFont(QFont("FreeSans", 11, QFont::Normal, false));

    mainwin = new MainWindow;

// Moved buildInfo calls to after translation is available as makeBuildInfo includes tr() calls

    QStringList info = makeBuildInfo(relinfo, forcedEngine);
    for (int i = 0; i < info.size(); ++i)
        qDebug().noquote() << info.at(i);

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

    /*************************************************************************************
    #ifdef BROKEN_OPENGL_BUILD
        Q_UNUSED(bad_graphics)
        Q_UNUSED(intel_graphics)

        const QString BetterBuild = "Settings/BetterBuild";

        if (opengl2supported) {
            if (!settings.value(BetterBuild, false).toBool()) {
                QMessageBox::information(nullptr, QObject::tr("A faster build of OSCAR may be available"),
                    QObject::tr("This build of OSCAR is a compatability version that also works on computers lacking OpenGL 2.0 support.")+"<br/><br/>"+
                    QObject::tr("However it looks like your computer has full support for OpenGL 2.0!") + "<br/><br/>"+
                    QObject::tr("This version will run fine, but a \"<b>%1</b>\" tagged build of OSCAR will likely run a bit faster on your computer.").arg("-OpenGL")+"<br/><br/>"+
                    QObject::tr("You will not be bothered with this message again."), QMessageBox::Ok, QMessageBox::Ok);
                settings.setValue(BetterBuild, true);
            }
        }
    #else
        if (bad_graphics) {
            QMessageBox::warning(nullptr, QObject::tr("Incompatible Graphics Hardware"),
                QObject::tr("This build of OSCAR requires OpenGL 2.0 support to function correctly, and unfortunately your computer lacks this capability.") + "<br/><br/>"+
                QObject::tr("You may need to update your computers graphics drivers from the GPU makers website. %1").
                    arg(intel_graphics ? QObject::tr("(<a href='http://intel.com/support'>Intel's support site</a>)") : "")+"<br/><br/>"+
                QObject::tr("Because graphs will not render correctly, and it may cause crashes, this build will now exit.")+"<br/><br/>"+
                QObject::tr("There is another build available tagged \"<b>-BrokenGL</b>\" that should work on your computer."),
                QMessageBox::Ok, QMessageBox::Ok);
            exit(1);
        }
    #endif
    ****************************************************************************************************************/
    ////////////////////////////////////////////////////////////////////////////////////////////
    // Datafolder location Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
//  bool change_data_dir = force_data_dir;
//
//  bool havefolder = false;

    if (!settings.contains("Settings/AppData")) {       // This is first time execution
        if ( settings.contains("Settings/AppRoot") ) {  // allow for old AppRoot here - not really first time
            settings.setValue("Settings/AppData", settings.value("Settings/AppRoot"));
        } else {
            settings.setValue("Settings/AppData", homeDocs + getModifiedAppData());    // set up new data directory path
        }
        qDebug() << "First time: Setting " + GetAppData();
    }

    QDir dir(GetAppData());

    if ( ! dir.exists() ) {             // directory doesn't exist, verify user's choice
        if ( ! force_data_dir ) {       // unless they explicitly selected it by --datadir param
            if (QMessageBox::question(nullptr, STR_MessageBox_Question,
                                      QObject::tr("OSCAR will set up a folder for your data.")+"\n"+
                                      QObject::tr("If you have been using SleepyHead, OSCAR can copy your old data to this folder later.")+"\n"+
                                      QObject::tr("We suggest you use this folder: ")+QDir::toNativeSeparators(GetAppData())+"\n"+
                                      QObject::tr("Click Ok to accept this, or No if you want to use a different folder."),
                                      QMessageBox::Ok | QMessageBox::No, QMessageBox::Ok) == QMessageBox::No) {
                // User wants a different folder for data
                bool change_data_dir = true;
                while (change_data_dir) {           // Create or select an acceptable folder
                    QString datadir = QFileDialog::getExistingDirectory(nullptr,
                                      QObject::tr("Choose or create a new folder for OSCAR data"), homeDocs, QFileDialog::ShowDirsOnly);

                    if (datadir.isEmpty()) {        // User hit Cancel instead of selecting or creating a folder
                        QMessageBox::information(nullptr, QObject::tr("Exiting"),
                                                 QObject::tr("As you did not select a data folder, OSCAR will exit.")+"\n"+
                                                 QObject::tr("Next time you run OSCAR, you will be asked again."));
                        return 0;
                    } else {                        // We have a folder, see if is already an OSCAR folder
                        QDir dir(datadir);
                        QFile file(datadir + "/Preferences.xml");
                        QDir  dirP(datadir + "/Profiles");

                        if (!file.exists() || !dirP.exists()) {       // It doesn't have a Preferences.xml file or a Profiles directory in it
                            if (dir.count() > 2) {  // but it has more than dot and dotdot
                                // Not a new directory.. nag the user.
                                if (QMessageBox::question(nullptr, STR_MessageBox_Warning,
                                                          QObject::tr("The folder you chose is not empty, nor does it already contain valid OSCAR data.") +
                                                          "\n\n"+QObject::tr("Are you sure you want to use this folder?")+"\n\n" +
                                                          datadir, QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
                                    continue;   // Nope, don't use it, go around the loop again
                                }
                            }
                        }

                        settings.setValue("Settings/AppData", datadir);
                        qDebug() << "Changing data folder to" << datadir;
                        change_data_dir = false;
                    }
                }           // the while loop
            }           // user wants a different folder
        }           // user used --datadir folder to select a folder
    }           // The folder doesn't exist
    else
        qDebug() << "AppData folder already exists, so ...";
    qDebug() << "Using " + GetAppData() + " as OSCAR data folder";

    addBuildInfo("");
    QString path = GetAppData();
    addBuildInfo(QObject::tr("Data directory:") + " <a href=\"file:///" + path + "\">" + path + "</a>");

    QDir newDir(GetAppData());
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    if ( ! newDir.exists() || newDir.count() == 0 ) {     // directoy doesn't exist yet or is empty, try to migrate old data
#else
    if ( ! newDir.exists() || newDir.isEmpty() ) {        // directoy doesn't exist yet or is empty, try to migrate old data
#endif
        if (QMessageBox::question(nullptr, QObject::tr("Migrate SleepyHead Data?"),
                                  QObject::tr("On the next screen OSCAR will ask you to select a folder with SleepyHead data") +"\n" +
                                  QObject::tr("Click [OK] to go to the next screen or [No] if you do not wish to use any SleepyHead data."),
                                  QMessageBox::Ok|QMessageBox::No, QMessageBox::Ok) == QMessageBox::Ok) {
            migrateFromSH( GetAppData() );              // doesn't matter if no migration
        }
    }


    ///////////////////////////////////////////////////////////////////////////////////////////
    // Initialize preferences system (Don't use p_pref before this point!)
    ///////////////////////////////////////////////////////////////////////////////////////////
    p_pref = new Preferences("Preferences");
    p_pref->Open();
    AppSetting = new AppWideSetting(p_pref);

    QString language = settings.value(LangSetting, "").toString();
    AppSetting->setLanguage(language);

    // Set fonts from preferences file
    validateAllFonts();
    setApplicationFont();

    // one-time translate GraphSnapshots to ShowPieChart
    p_pref->Rename(STR_AS_GraphSnapshots, STR_AS_ShowPieChart);

    p_pref->Erase(STR_AppName);
    p_pref->Erase(STR_GEN_SkipLogin);

#ifndef NO_UPDATER
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
#endif

    int vc = compareVersion(AppSetting->versionString());
    if (vc < 0) {
        AppSetting->setShowAboutDialog(1);
//      release_notes();
//      check_updates = false;
    } else if (vc > 0) {
        if (QMessageBox::warning(nullptr, STR_MessageBox_Error,
                                 QObject::tr("The version of OSCAR you just ran is OLDER than the one used to create this data (%1).").
                                 arg(AppSetting->versionString()) +"\n\n"+
                                 QObject::tr("It is likely that doing this will cause data corruption, are you sure you want to do this?"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {

            return 0;
        }
    }

    AppSetting->setVersionString(VersionString);

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

#ifndef NO_UPDATER
    if (check_updates) {
        mainwin->CheckForUpdates();
    }
#endif

    mainwin->SetupGUI();
    mainwin->show();

    return a.exec();
}

#endif // !UNITTEST_MODE
