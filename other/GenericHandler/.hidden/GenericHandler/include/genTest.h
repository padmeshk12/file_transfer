/*
 *
 *
 * File:         test.h
 * Description:  There exist a few support functions for 
 *               test purpose
 *
 * Language:     C++
 * Author:       Winfried Merz
 * Date:         95/03/31
 *
 * (C) Copyright Hewlett-Packard 1995.  All Rights Reserved.
 *
 *  History
 *  ---------- 
 *
 *  Author:
 *  Change Date:
 *  
 */


#ifndef STTEST_H
#define STTEST_H


extern void test_start(char* name);
extern void test_begin(const char* msg);
extern void test_perform(int success);
extern void test_perform_perf(int success);
extern void exitIfFail(void);
extern int test_summary(void);


/*
 * STTestStart starts counting of passing and failing tests.
 * It prints out "Start Testing" and the test name passed in
 * the parameter 's'. 's' must be of type char*
 */
#define STTestStart(s) test_start(s);

/*
 * STTestSummary stops counting of passing and failing tests
 * It prints out the test name passed in STTestStart and 
 * Test Summary" and the number of passed and failed tests
 * It returns STTrue if all tests passed else STFalse
 */
#define STTestSummary test_summary()

/*
 * STTestOut prints out the passed 'msg'
 */
extern void STTestOut(const char* msg);

/*
 * The following 3 macros can be used for test purpose
 * STSimpleTest performs the check 'p' and checks if it is
 * equal STTrue. Then it prints out the
 * current test number, the check p and if passed or failed.
 */
#define STSimpleTest(p)		\
 {				\
  test_begin(#p);		\
  test_perform((p)==1);	\
 }


/*
 * STTest performs the check 'p' and checks if it is
 * equal 'v'. 'v' must be either STTrue or STFalse.
 * Then it prints out the current test number, 
 * the message passed in 's' and if passed or failed.
 * s' must be of type const char*.
 */
#define STTest(s,p,v) 		\
 {				\
  test_begin(s);		\
  test_perform((p)==v);		\
 }

/*
 * STPerformanceTest checks if a measured performance number is inside
 * a given range. Then it prints out the current test number, the
 * message passed in 's' and if passed or failed. 's' must be of type
 * const char*. Performance test fails are not counted with other fails
 * but as a separate class of fails.
 */
#define STPerformanceTest(s,act,min,max)	\
 {						\
  test_begin(s);				\
  test_perform_perf((act>=min)&&(act<=max));	\
 }

/*
 * STTestRun first executes the function x and 
 * then performs the check 
 * p' and checks if it is
 * equal 'v'. 'v' must be either STTrue or STFalse.
 * Then it prints out the current test number, 
 * the message passed in 's' and if passed or failed.
 * s' must be of type const char*.
 */
#define STTestRun(s,x,p,v) 	\
 {				\
  x;				\
  test_begin(s);		\
  test_perform((p)==v);		\
 }

/* 
 * STExitIfFail exit test program if any failure has
 * occurred.
 */
#define STExitIfFail() exitIfFail()

/*
 * Example of Usage
 *
 * include <st/types.h>
 * void pre(STInt32& a)
 *
 *   a +=4;
 *
 * main ()
 *
 *  int returnValue;
 *  STTestStart("types.h");
 *  STTestOut("Test of Absolute Value of an Integer\n");
 *  {
 *    STInt32 a(STSafeMinInt32);
 *    STSimpleTest(STAbs(a) == a);
 *  }
 *  {
 *    STInt32 b(-4);
 *    STTest("Absolute Value of -4", STAbs(b) == 3, STTrue);
 *  }
 *  {
 *     STInt32 c(-2);
 *     STTestRun("Absolute Value of 2",pre(c),STAbs(c)==2, STTrue); 
 *  }
 *  returnValue = STTestSummary;
 *  return(returnValue);
 *
 *
 * Printout
 *
 * Start Testing types.h:
 *
 *
 * Test of Absolute Value of an Integer
 * Test   1 STAbs(a) == a                           PASSED
 * Test   2 Absolute Value of -4                  **FAILED**
 * Test   3 Absolute Value of 2                     PASSED
 *
 * types.h Test Summary: 2 passed, 1 failed
 *
 */

#endif



