/* Multilingual Support header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <QString>
const QString DefaultLanguage = "en_US";
const QString LangSetting = "Settings/Language";

void initTranslations();
QString currentLanguage();
QString lookupLanguageName(QString language);

#endif // TRANSLATION_H
