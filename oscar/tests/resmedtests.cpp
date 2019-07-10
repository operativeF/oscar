/* ResMed Unit Tests
 *
 * Copyright (c) 2019 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "resmedtests.h"
//#include "sessiontests.h"

#define TESTDATA_PATH "./testdata/"

static ResmedLoader* s_loader = nullptr;
static void iterateTestCards(const QString & root, void (*action)(const QString &));

void ResmedTests::initTestCase(void)
{
    initializeStrings();
    qDebug() << STR_TR_OSCAR + " " + getBranchVersion();
    QString profile_path = TESTDATA_PATH "profile/";
    Profiles::Create("test", &profile_path);
    p_pref = new Preferences("Preferences");
    p_pref->Open();
    AppSetting = new AppWideSetting(p_pref);

    schema::init();
    ResmedLoader::Register();
    s_loader = dynamic_cast<ResmedLoader*>(lookupLoader(resmed_class_name));
}

void ResmedTests::cleanupTestCase(void)
{
}


// ====================================================================================================


static void parseAndEmitSessionYaml(const QString & path)
{
    qDebug() << path;

    // This blindly calls ResmedLoader::Open() so that we can run address and
    // leak sanitizers against the ResMed loader.
    //
    // Once the ResMed loader is refactored support importing without writing
    // to the database, this can be updated to pass the imported Session objects
    // to SessionToYaml like the PRS1 tests do.
    s_loader->Open(path);
}

void ResmedTests::testSessionsToYaml()
{
    iterateTestCards(TESTDATA_PATH "resmed/input/", parseAndEmitSessionYaml);
}


// ====================================================================================================

void iterateTestCards(const QString & root, void (*action)(const QString &))
{
    QDir dir(root);
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    // Look through each folder in the given root
    for (int i = 0; i < flist.size(); i++) {
        QFileInfo fi = flist.at(i);
        if (fi.isDir()) {
            // If it contains a DATALOG folder, it's a ResMed SD card
            QDir datalog(fi.canonicalFilePath() + QDir::separator() + "DATALOG");
            if (datalog.exists()) {
                // Confirm that it contains the file that the ResMed loader expects
                QFileInfo idfile(fi.canonicalFilePath() + QDir::separator() + "Identification.tgt");
                if (idfile.exists()) {
                    action(fi.canonicalFilePath());
                }
            }
        }
    }
}
