/* ResMed Unit Tests
 *
 * Copyright (c) The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef RESMEDTESTS_H
#define RESMEDTESTS_H

#include "AutoTest.h"
#include "../SleepLib/loader_plugins/resmed_loader.h"

class ResmedTests : public QObject
{
    Q_OBJECT
 
private slots:
    void initTestCase();
    //void testChunksToYaml();
    void testSessionsToYaml();
    void cleanupTestCase();
};

DECLARE_TEST(ResmedTests)

#endif // RESMEDTESTS_H

