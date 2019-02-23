/* SleepLib Somnopose Loader Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the somnopose_data_version in somnopose_loader.h when making changes to this loader
// that change loader behaviour or modify channels.
//********************************************************************************************

#include <QDir>
#include <QTextStream>
#include "somnopose_loader.h"
#include "SleepLib/machine.h"

SomnoposeLoader::SomnoposeLoader()
{
    m_type = MT_POSITION;
}

SomnoposeLoader::~SomnoposeLoader()
{
}
int SomnoposeLoader::Open(const QString & dirpath)
{
    QString newpath;

    QString dirtag = "somnopose";

    // Could Scan the ZEO folder for a list of CSVs

    QString path(dirpath);
    path = path.replace("\\", "/");

    if (path.toLower().endsWith("/" + dirtag)) {
        return 0;
        //newpath=path;
    } else {
        newpath = path + "/" + dirtag.toUpper();
    }

    //QString filename;

    // Somnopose folder structure detection stuff here.

    return 0; // number of machines affected
}

int SomnoposeLoader::OpenFile(const QString & filename)
{
    QFile file(filename);

    if (filename.toLower().endsWith(".csv")) {
        if (!file.open(QFile::ReadOnly)) {
            qDebug() << "Couldn't open Somnopose data file" << filename;
            return 0;
        }
    } else {
        return 0;
    }

    qDebug() << "Opening file" << filename;
    QTextStream ts(&file);

    // Read header line and determine order of fields
    QString hdr = ts.readLine();
    QStringList headers = hdr.split(",");

    int col_inclination = -1, col_orientation = -1, col_timestamp = -1;

    int hdr_size = headers.size();

    for (int i = 0; i < hdr_size; i++) {
        if (headers.at(i).compare("timestamp", Qt::CaseInsensitive) == 0) {
            col_timestamp = i;
        }

        if (headers.at(i).compare("inclination", Qt::CaseInsensitive) == 0) {
            col_inclination = i;
        }

        if (headers.at(i).compare("orientation", Qt::CaseInsensitive) == 0) {
            col_orientation = i;
        }
    }

    // Check we have all fields available
    if ((col_timestamp < 0) || (col_inclination < 0) || (col_orientation < 0)) {
        return 0;
    }

    QDateTime epoch(QDate(2001, 1, 1), QTime(0, 0, 0));
    qint64 ep = qint64(epoch.toTime_t()) * 1000, time;

    double timestamp, orientation, inclination;
    QString data;
    QStringList fields;
    bool ok;

    bool first = true;
    MachineInfo info = newInfo();
    Machine *mach = p_profile->CreateMachine(info);
    Session *sess = nullptr;
    SessionID sid;

    EventList *ev_orientation = nullptr, *ev_inclination = nullptr;

    while (!(data = ts.readLine()).isEmpty()) {
        fields = data.split(",");

        if (fields.size() < hdr_size) { // missing fields.. skip this record
            continue;
        }

        timestamp = fields[col_timestamp].toDouble(&ok);

        if (!ok) { continue; }

        orientation = fields[col_orientation].toDouble(&ok);

        if (!ok) { continue; }

        inclination = fields[col_inclination].toDouble(&ok);

        if (!ok) { continue; }

        // convert to milliseconds since epoch
        time = (timestamp * 1000.0) + ep;

        if (first) {
            sid = time / 1000;

            if (mach->SessionExists(sid)) {
                return 0; // Already imported
            }

            sess = new Session(mach, sid);
            sess->really_set_first(time);
            ev_orientation = sess->AddEventList(POS_Orientation, EVL_Event, 1, 0, 0, 0);
            ev_inclination = sess->AddEventList(POS_Inclination, EVL_Event, 1, 0, 0, 0);
            first = false;
        }

        sess->set_last(time);
        ev_orientation->AddEvent(time, orientation);
        ev_inclination->AddEvent(time, inclination);
    }

    if (sess) {
        if (ev_orientation && ev_inclination) {
            sess->setMin(POS_Orientation, ev_orientation->Min());
            sess->setMax(POS_Orientation, ev_orientation->Max());
            sess->setMin(POS_Inclination, ev_inclination->Min());
            sess->setMax(POS_Inclination, ev_inclination->Max());
        }

        sess->really_set_last(time);
        sess->SetChanged(true);

        mach->AddSession(sess);

        mach->Save();
    }

    return true;
}


static bool somnopose_initialized = false;

void SomnoposeLoader::Register()
{
    if (somnopose_initialized) { return; }

    qDebug("Registering SomnoposeLoader");
    RegisterLoader(new SomnoposeLoader());
    //InitModelMap();
    somnopose_initialized = true;
}

