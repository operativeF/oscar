/* PRS1 Unit Tests
 *
 * Copyright (c) 2019 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "prs1tests.h"
#include "sessiontests.h"

#define TESTDATA_PATH "./testdata/"

static PRS1Loader* s_loader = nullptr;
static void iterateTestCards(const QString & root, void (*action)(const QString &));
static QString prs1OutputPath(const QString & inpath, const QString & serial, int session, const QString & suffix);

void PRS1Tests::initTestCase(void)
{
    QString profile_path = TESTDATA_PATH "profile/";
    Profiles::Create("test", &profile_path);

    schema::init();
    PRS1Loader::Register();
    s_loader = dynamic_cast<PRS1Loader*>(lookupLoader(prs1_class_name));
}

void PRS1Tests::cleanupTestCase(void)
{
}


void parseAndEmitSessionYaml(const QString & path)
{
    qDebug() << path;

    // This mirrors the functional bits of PRS1Loader::OpenMachine.
    // Maybe there's a clever way to add parameters to OpenMachine that
    // would make it more amenable to automated tests. But for now
    // something is better than nothing.

    QStringList paths;
    QString propertyfile;
    int sessionid_base;
    sessionid_base = s_loader->FindSessionDirsAndProperties(path, paths, propertyfile);

    Machine *m = s_loader->CreateMachineFromProperties(propertyfile);
    Q_ASSERT(m != nullptr);

    s_loader->ScanFiles(paths, sessionid_base, m);
    
    // Each session now has a PRS1Import object in m_tasklist
    QList<ImportTask*>::iterator i;
    while (!s_loader->m_tasklist.isEmpty()) {
        ImportTask* task = s_loader->m_tasklist.takeFirst();

        // Run the parser
        PRS1Import* import = dynamic_cast<PRS1Import*>(task);
        import->ParseSession();
        
        // Emit the parsed session data to compare against our regression benchmarks
        Session* session = import->session;
        QString outpath = prs1OutputPath(path, m->serial(), session->session(), "-session.yml");
        SessionToYaml(outpath, session);
        
        delete session;
        delete task;
   }
}

void PRS1Tests::testSessionsToYaml()
{
    iterateTestCards(TESTDATA_PATH "prs1/input/", parseAndEmitSessionYaml);
}


// ====================================================================================================

QString prs1OutputPath(const QString & inpath, const QString & serial, int session, const QString & suffix)
{
    // Output to prs1/output/FOLDER/SERIAL-000000(-session.yml, etc.)
    QDir path(inpath);
    QStringList pathlist = QDir::toNativeSeparators(inpath).split(QDir::separator(), QString::SkipEmptyParts);
    pathlist.pop_back();  // drop serial number directory
    pathlist.pop_back();  // drop P-Series directory
    QString foldername = pathlist.last();

    QDir outdir(TESTDATA_PATH "prs1/output/" + foldername);
    outdir.mkpath(".");
    
    QString filename = QString("%1-%2%3")
                        .arg(serial)
                        .arg(session, 6, 10, QChar('0'))
                        .arg(suffix);
    return outdir.path() + QDir::separator() + filename;
}

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
            // If it contains a P-Series folder, it's a PRS1 SD card
            QDir pseries(fi.canonicalFilePath() + QDir::separator() + "P-Series");
            if (pseries.exists()) {
                pseries.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
                pseries.setSorting(QDir::Name);
                QFileInfoList plist = pseries.entryInfoList();

                // Look for machine directories (containing a PROP.TXT or properties.txt)
                for (int j = 0; j < plist.size(); j++) {
                    QFileInfo pfi = plist.at(j);
                    if (pfi.isDir()) {
                        QString machinePath = pfi.canonicalFilePath();
                        QDir machineDir(machinePath);
                        QFileInfoList mlist = machineDir.entryInfoList();
                        for (int k = 0; k < mlist.size(); k++) {
                            QFileInfo mfi = mlist.at(k);
                            if (QDir::match("PROP*.TXT", mfi.fileName())) {
                                // Found a properties file, this is a machine folder
                                action(machinePath);
                            }
                        }
                    }
                }
            }
        }
    }
}
