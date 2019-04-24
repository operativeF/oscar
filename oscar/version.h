/* Version.h
 *
 * Copyright (c) 2011-2018 Mark Watkins <mark@jedimark.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef VERSION_H
#define VERSION_H

#include <qglobal.h>
#include "build_number.h"

const int major_version = 1;   // incompatible API changes
const int minor_version = 0;   // new features that don't break things
const int revision_number = 1; // bugfixes, revisions
const QString ReleaseStatus = "r"; // testing/nightly/unstable, beta/untamed, rc/almost, r/stable

const QString VersionString = QString("%1.%2.%3-%4-%5").arg(major_version).arg(minor_version).arg(revision_number).arg(ReleaseStatus).arg(build_number);
const QString ShortVersionString = QString("%1.%2.%3").arg(major_version).arg(minor_version).arg(revision_number);

#ifdef Q_OS_MAC
const QString PlatformString = "MacOSX";
#elif defined(Q_OS_WIN32)
const QString PlatformString = "Win32";
#elif defined(Q_OS_WIN64)
const QString PlatformString = "Win64";
#elif defined(Q_OS_LINUX)
const QString PlatformString = "Linux";
#elif defined(Q_OS_HAIKU)
const QString PlatformString = "Haiku";
#endif

#endif // VERSION_H
