/* SleepLib Journal Implementation
 *
 * Copyright (C) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#ifndef JOURNAL_H
#define JOURNAL_H

#include "SleepLib/profiles.h"

void BackupJournal(QString filename);

class Bookmark {
public:
    Bookmark() {
        start = end = 0;
    }
    Bookmark(const Bookmark & copy) {
        start = copy.start;
        end = copy.end;
        notes = copy.notes;
    }
    Bookmark(qint64 start, qint64 end, QString notes):
    start(start), end(end), notes(notes) {}

    qint64 start;
    qint64 end;
    QString notes;
};

class JournalEntry
{
public:
    JournalEntry(QDate date);
    ~JournalEntry();
    bool Save();

    QString notes();
    void setNotes(QString notes);

    EventDataType weight();
    void setWeight(EventDataType weight);
    int zombie();
    void setZombie(int zombie);

    QList<Bookmark> & getBookmarks();
    void addBookmark(qint64 start, qint64 end, QString note);
    void delBookmark(qint64 start, qint64 end);


protected:
    QDate m_date;
    QList<Bookmark> bookmarks;
    Day * day;
    Session * session;
    bool newsession;
};

void BackupJournal(QString filename);


class DayController
{
    DayController();
    ~DayController();

    void setDate(QDate date);

    QDate m_date;
    JournalEntry * journal;
    Day * cpap;
    Day * oximeter;

};

#endif // JOURNAL_H
