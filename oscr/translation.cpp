/* Multilingual Support files
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QDebug>
#include <QStringList>
#include <QList>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QDir>
#include <QSettings>
#include <QTranslator>
#include <QListWidget>

#include "SleepLib/common.h"
#include "translation.h"

QHash<QString, QString> langNames;

QString currentLanguage()
{
    QSettings settings;
    return settings.value(LangSetting).toString();
}
QString lookupLanguageName(QString language)
{
    auto it = langNames.find(language);
    if (it != langNames.end()) {
        return it.value();
    }
    return language;
}

void initTranslations()
{
    // Add any languages with special character set needs to this list
    langNames["zh"] = "\xe6\xbc\xa2\xe8\xaa\x9e\xe7\xb9\x81\xe9\xab\x94\xe5\xad\x97";
    langNames["es"] = "Espa\xc3\xb1ol";
    langNames["bg"] = "\xd0\xb1\xd1\x8a\xd0\xbb\xd0\xb3\xd0\xb0\xd1\x80\xd1\x81\xd0\xba\xd0\xb8";
    langNames["fr"] = "\x46\x72\x61\x6e\xc3\xa7\x61\x69\x73";
    langNames["en_UK"] = "English (UK)";
    langNames["nl"] = "Nederlands";

    langNames[DefaultLanguage]="English (US)";

    QHash<QString, QString> langFiles;
    QHash<QString, QString> langPaths;

    langFiles[DefaultLanguage] = "English (US).en_US.qm";

    QSettings settings;
    QString language = settings.value(LangSetting).toString();


    QString inbuiltPath = ":/translations";
    QStringList inbuilt(DefaultLanguage);

    QDir dir(inbuiltPath);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("*.qm"));

    QFileInfoList list = dir.entryInfoList();
    for (const auto & fi : list) {
        QString code = fi.fileName().section('.', 1, 1);

        if (!langNames.contains(code)) langNames[code]=fi.fileName().section('.', 0, 0);

        inbuilt.push_back(code);

        langFiles[code] = fi.fileName();
        langPaths[code] = inbuiltPath;
    }
    std::sort(inbuilt.begin(), inbuilt.end());
    qDebug() << "Inbuilt Translations:" << QString(inbuilt.join(", ")).toLocal8Bit().data();;

    QString externalPath = appResourcePath() +"/Translations";
    dir.setPath(externalPath);
    list = dir.entryInfoList();

    // Add default language (English)

    // Scan through available translations, and add them to the list
    QStringList extratrans, replaced;
    for (const auto & fi : list) {
        QString code = fi.fileName().section('.', 1, 1);

        if(!langNames.contains(code)) langNames[code] = fi.fileName().section('.', 0, 0);
        if (inbuilt.contains(code)) replaced.push_back(code); else extratrans.push_back(code);

        langFiles[code] = fi.fileName();
        langPaths[code] = externalPath;
    }
    std::sort(replaced.begin(), replaced.end());
    std::sort(extratrans.begin(), extratrans.end());
    if (replaced.size()>0) qDebug() << "Overridden Tranlsations:" << QString(replaced.join(", ")).toLocal8Bit().data();
    if (extratrans.size()>0) qDebug() << "Extra Translations:" << QString(extratrans.join(", ")).toLocal8Bit().data();

    if (language.isEmpty() || !langNames.contains(language)) {
        QDialog langsel(nullptr, Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        QFont font;
        font.setPointSize(20);
        langsel.setFont(font);
        langsel.setWindowTitle("Language / Taal / Sprache / Langue / \xe8\xaf\xad\xe8\xa8\x80 / ... ");
        QHBoxLayout lang_layout(&langsel);

        QLabel img;
        img.setPixmap(QPixmap(":/docs/sheep.png"));

        QPushButton lang_okbtn("->", &langsel); // hard coded non translatable

        QVBoxLayout layout1;
        QVBoxLayout layout2;

        layout2.setMargin(6);
        lang_layout.setContentsMargins(4,4,4,4);
        lang_layout.setMargin(6);
        layout2.setSpacing(6);
        QListWidget langlist;

        lang_layout.addLayout(&layout1);
        lang_layout.addLayout(&layout2);

        layout1.addWidget(&img);
        layout2.addWidget(&langlist, 1);

        langlist.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        int row = 0;
        for (QHash<QString, QString>::iterator it = langNames.begin(); it != langNames.end(); ++it) {
            const QString & code = it.key();
            const QString & name = it.value();
            if (!langFiles.contains(code) || langFiles[code].isEmpty())
                continue;

            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, code);
            langlist.insertItem(row++, item);
            // Todo: Use base system language code if available.
            if (code.compare(DefaultLanguage) == 0) {
                langlist.setCurrentItem(item);
            }
        }
        langlist.sortItems();
        layout2.addWidget(&lang_okbtn);

        langsel.connect(&langlist, SIGNAL(itemDoubleClicked(QListWidgetItem*)), &langsel, SLOT(close()));
        langsel.connect(&lang_okbtn, SIGNAL(clicked()), &langsel, SLOT(close()));

        langsel.exec();
        langsel.disconnect(&lang_okbtn, SIGNAL(clicked()), &langsel, SLOT(close()));
        langsel.disconnect(&langlist, SIGNAL(itemDoubleClicked(QListWidgetItem*)), &langsel, SLOT(close()));
        language = langlist.currentItem()->data(Qt::UserRole).toString();
        settings.setValue(LangSetting, language);
    }

    QString langname=langNames[language];
    QString langfile=langFiles[language];
    QString langpath=langPaths[language];

    if (language.compare(DefaultLanguage) != 0) {
        qDebug() << "Loading" << langname << "translation" << langfile.toLocal8Bit().data() << "from" << langpath.toLocal8Bit().data();
        QTranslator * translator = new QTranslator();

        if (!langfile.isEmpty() && !translator->load(langfile, langpath)) {
            qWarning() << "Could not load translation" << langfile << "reverting to english :(";
        }

        qApp->installTranslator(translator);
    } else {
        qDebug() << "Using default language" << language.toLocal8Bit().data();
    }
}
