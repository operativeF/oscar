/* Multilingual Support header
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <QSettings>

const QString LangSetting = "Settings/Language";

void initTranslations(QSettings & settings);

#endif // TRANSLATION_H
