#ifndef PRS1TESTS_H
#define PRS1TESTS_H

#include "AutoTest.h"

class PRS1Tests : public QObject
{
 Q_OBJECT

private slots:
 void initTestCase();
 void test1();
 // void test2();
 void cleanupTestCase();
};

DECLARE_TEST(PRS1Tests)

#endif // PRS1TESTS_H

