/* PRS1 Unit Tests
 *
 * Copyright (c) The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef PRS1TESTS_H
#define PRS1TESTS_H

#include "AutoTest.h"
#include "../SleepLib/loader_plugins/prs1_loader.h"

class PRS1Tests : public QObject
{
    Q_OBJECT
 
private slots:
    void initTestCase();
    void testChunksToYaml();
    void testSessionsToYaml();
    // void test2();
    void cleanupTestCase();
};

DECLARE_TEST(PRS1Tests)

#endif // PRS1TESTS_H

