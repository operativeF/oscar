/* SleepLib Journal Implementation
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#include "journal.h"
#include "machine_common.h"
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>

const int journal_data_version = 1;

JournalEntry::JournalEntry(QDate date)
{
    Machine * jmach = p_profile->GetMachine(MT_JOURNAL);
    if (jmach == nullptr) { // Create Journal machine record if it doesn't already exist
        MachineInfo info(MT_JOURNAL,0, "Journal", QObject::tr("Journal Data"), QString(), QString(), QString(), QObject::tr("SleepyHead"), QDateTime::currentDateTime(), journal_data_version);

        // Using machine ID 1 rather than a random number, so in future, if profile.xml gets screwed up they'll get their data back..
        // TODO: Perhaps search for unlinked journal folders here to save some anger and frustration? :P

        MachineID machid = 1;
        QString path = p_profile->Get("{" + STR_GEN_DataFolder + "}");
        QDir dir(path);
        QStringList filters;
        filters << "Journal_*";
        QStringList dirs = dir.entryList(filters,QDir::Dirs);
        int journals = dirs.size();
        if (journals > 0) {
            QString tmp = dirs[0].section("_", -1);
            bool ok;
            machid = tmp.toUInt(&ok, 16);
            if (!ok) {
                QMessageBox::warning(nullptr, STR_MessageBox_Warning,
                    QObject::tr("SleepyHead found an old Journal folder, but it looks like it's been renamed:")+"\n\n"+
                    QString("%1").arg(dirs[0])+
                    QObject::tr("SleepyHead will not touch this folder, and will create a new one instead.")+"\n\n"+
                    QObject::tr("Please be careful when playing in SleepyHead's profile folders :-P"), QMessageBox::Ok);

                // User renamed the folder.. report this
                machid = 1;
            }
            if (journals > 1) {
                QMessageBox::warning(nullptr, STR_MessageBox_Warning,
                    QObject::tr("For some reason, sleepyHead couldn't find a journal object record in your profile, but did find multiple Journal data folders.")+"\n\n"+
                    QObject::tr("SleepyHead picked only the first one of these, and will use it in future:")+"\n\n"+
                    QString("%1").arg(dirs[0])+
                    QObject::tr("If your old data is missing, copy the contents of all the other Journal_XXXXXXX folders to this one manually."), QMessageBox::Ok);
                // more then one.. report this.
            }
        }
        jmach = p_profile->CreateMachine(info, machid);
    }

    m_date = date;
    session = nullptr;
    day = p_profile->GetDay(date, MT_JOURNAL);
    if (day != nullptr) {
        session = day->firstSession(MT_JOURNAL);
    } else {
        // Doesn't exist.. create a new one..
        session = new Session(jmach,0);
        qint64 st,et;
        QDateTime dt(date,QTime(22,0)); // 10pm localtime
        st=qint64(dt.toTime_t())*1000L;
        et=st+3600000L;
        session->set_first(st);
        session->set_last(et);

        // Let it live in memory...but not on disk unless data is changed...
        jmach->AddSession(session);

        // and where does day get set??? does day actually need to be set??
        day = p_profile->GetDay(date, MT_JOURNAL);
    }
}
JournalEntry::~JournalEntry()
{
    if (session && session->IsChanged()) {
        Save();
    }
}


bool JournalEntry::Save()
{
    if (session && session->IsChanged()) {
        qDebug() << "Saving journal session for" << m_date;

        // just need to write bookmarks, the rest are already stored in the session
        QVariantList start;
        QVariantList end;
        QStringList notes;

        int size = bookmarks.size();
        for (int i=0; i<size; ++i) {
            const Bookmark & bm = bookmarks.at(i);
            start.append(bm.start);
            end.append(bm.end);
            notes.append(bm.notes);
        }
        session->settings[Bookmark_Start] = start;
        session->settings[Bookmark_End] = end;
        session->settings[Bookmark_Notes] = notes;

        session->settings[LastUpdated] = QDateTime::currentDateTime().toTime_t();

        session->StoreSummary();
        return true;
    }
    return false;
}

QString JournalEntry::notes()
{
    QHash<ChannelID, QVariant>::iterator it;
    if (session && ((it=session->settings.find(Journal_Notes)) != session->settings.end())) {
        return it.value().toString();
    }
    return QString();
}
void JournalEntry::setNotes(QString notes)
{
    if (!session) return;
    session->settings[Journal_Notes] = notes;
    session->SetChanged(true);
}
EventDataType JournalEntry::weight()
{
    QHash<ChannelID, QVariant>::iterator it;
    if (session && ((it = session->settings.find(Journal_Weight)) != session->settings.end())) {
        return it.value().toFloat();
    }
    return 0;
}
void JournalEntry::setWeight(EventDataType weight)
{
    if (!session) return;
    session->settings[Journal_Weight] = weight;
    session->SetChanged(true);
}
int JournalEntry::zombie()
{
    QHash<ChannelID, QVariant>::iterator it;
    if (session && ((it = session->settings.find(Journal_ZombieMeter)) != session->settings.end())) {
        return it.value().toFloat();
    }
    return 0;
}
void JournalEntry::setZombie(int zombie)
{
    if (!session) return;
    session->settings[Journal_ZombieMeter] = zombie;
    session->SetChanged(true);
}

QList<Bookmark> & JournalEntry::getBookmarks()
{
    bookmarks.clear();
    if (!session || !session->settings.contains(Bookmark_Start)) {
        return bookmarks;
    }

    QVariantList start=session->settings[Bookmark_Start].toList();
    QVariantList end=session->settings[Bookmark_End].toList();
    QStringList notes=session->settings[Bookmark_Notes].toStringList();

    int size = start.size();
    for (int i=0; i < size; ++i) {
        bookmarks.append(Bookmark(start.at(i).toLongLong(), end.at(i).toLongLong(), notes.at(i)));
    }
    return bookmarks;
}

void JournalEntry::addBookmark(qint64 start, qint64 end, QString note)
{
    bookmarks.append(Bookmark(start,end,note));
    session->SetChanged(true);
}

void JournalEntry::delBookmark(qint64 start, qint64 end)
{
    bool removed;
    do {
        removed = false;
        int size = bookmarks.size();
        for (int i=0; i<size; ++i) {
            const Bookmark & bm = bookmarks.at(i);
            if ((bm.start == start) && (bm.end == end)) {
                bookmarks.removeAt(i);
                session->SetChanged(true); // make sure it gets saved later..
                removed=true;
                break;
            }
        }
    } while (removed); // clean up any stupid duplicates just in case.. :P
    // if I wanted to be nice above, I could add the note string to the search as well..
    // (some users might be suprised to see the lot go with the same start and end index)
}

void BackupJournal(QString filename)
{
    QDomDocument doc("SleepyHead Journal");

    QDomElement droot = doc.createElement(STR_AppName);
    doc.appendChild(droot);

    QDomElement root = doc.createElement("Journal");
    root.setAttribute("username", p_profile->user->userName());
    droot.appendChild(root);


    QDate first = p_profile->FirstDay(MT_JOURNAL);
    QDate last = p_profile->LastDay(MT_JOURNAL);

    QDate date = first.addDays(-1);
    do {
        date = date.addDays(1);

        Day * journal = p_profile->GetDay(date, MT_JOURNAL);
        if (!journal) continue;
        Session * sess = journal->firstSession(MT_JOURNAL);
        if (!sess) continue;
        QDomElement day = doc.createElement("day");
        day.setAttribute("date", date.toString());

        //notes.setAttribute("date", date.toString());

        if (journal->settingExists(Journal_Notes)) {
            QString notedata = sess->settings[Journal_Notes].toString();
            QDomElement notes = doc.createElement("note");
            notes.appendChild(doc.createCDATASection(notedata));
            day.appendChild(notes);
        }
        if (journal->settingExists(Journal_Weight)) {
            EventDataType weight = sess->settings[Journal_Weight].toFloat();
            day.setAttribute("weight", weight);
        }
        if (journal->settingExists(Journal_ZombieMeter)) {
            int zombie = sess->settings[Journal_ZombieMeter].toInt();
            day.setAttribute("zombie", zombie);
        }
        if (journal->settingExists(LastUpdated)) {
            QDateTime dt = sess->settings[LastUpdated].toDateTime();
            day.setAttribute("lastupdated", dt.toTime_t());
        }
        if (journal->settingExists(Bookmark_Start)) {
            QVariantList start=sess->settings[Bookmark_Start].toList();
            QVariantList end=sess->settings[Bookmark_End].toList();
            QStringList notes=sess->settings[Bookmark_Notes].toStringList();
            QDomElement bookmarks = doc.createElement("bookmarks");

            int size = start.size();
            for (int i=0; i< size; i++) {
                QDomElement bookmark = doc.createElement("bookmark");
                bookmark.setAttribute("start",start.at(i).toString());
                bookmark.setAttribute("end",end.at(i).toString());
                bookmark.setAttribute("notes",notes.at(i));
                bookmarks.appendChild(bookmark);
            }
            day.appendChild(bookmarks);
        }
        root.appendChild(day);

    } while (date <= last);
    QFile file(filename);

    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QTextStream ts(&file);
    ts << doc.toString();
    file.close();
}

DayController::DayController()
{
    journal = nullptr;
    cpap = nullptr;
    oximeter = nullptr;
}
DayController::~DayController()
{
    delete journal;
}

void DayController::setDate(QDate date)
{
    if (journal) {
        delete journal;
    }
    journal = new JournalEntry(date);
}
