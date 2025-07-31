/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : genTest.c
 * CREATED  : 17 Jul 1999
 *
 * CONTENTS : General test functions taken from /comfort software and 
 *            converted into c functions.
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Jul 1999, Chris Joyce, created
 *
 * Instructions:
 *
 * 1) Copy this template to as many .c files as you require
 *
 * 2) Use the command 'make depend' to make visible the new
 *    source files to the makefile utility
 *
 *****************************************************************************/



#include <stdio.h>           /* include the Stream class header file */
#include <stdlib.h>
#include "genTest.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */


static int num_test;
static int tests_passed;
static int tests_failed;
static int tests_perf_failed;
static char* test_name;

void STTestOut (const char *msg)
{
  fprintf(stderr,"%s",msg);  
  fflush(stderr); 
}

void test_start(char* name) {
  num_test = 0;
  tests_passed = 0;
  tests_failed = 0;
  test_name = name;
  fprintf(stderr,"-----------------------------------------------------------------------------\n");
  fprintf(stderr,"Start Testing");
  if (test_name != 0) fprintf(stderr," %s",test_name);
  fprintf(stderr,":\n");
  fprintf(stderr,"-----------------------------------------------------------------------------\n");
 }

void test_begin(const char* msg) {
  num_test++;
  fprintf(stderr," Test ");
  fprintf(stderr,"%3d %-53s", num_test,msg);
  fflush(stderr); 
}

/*
 * NOTE: We don't pass in the message (see test_begin) because
 *       we want to ensure that the message is printed BEFORE
 *       the test is executed.  This way when a test crashes
 *       we can tell if it was during a test, or between tests.
 */
void test_perform(int success) {
  if (success) {
    tests_passed++;
    fprintf(stderr,"  PASSED\n");
  } else {
    tests_failed++;
    fprintf(stderr,"**FAILED**\n");
  }
}

void test_perform_perf(int success) {
  if (success) {
    tests_passed++;
    fprintf(stderr,"  PASSED\n");
  } else {
    tests_perf_failed++;
    fprintf(stderr,"**PERF_FAILED**");
  }
}

int test_summary(void) {
  fprintf(stderr,"-----------------------------------------------------------------------------\n");
  if (test_name != NULL) fprintf(stderr,"%s ",test_name);
  fprintf(stderr,"Test Summary: ");
  fprintf(stderr,"%d passed, %d failed",tests_passed,tests_failed); 
  if (tests_perf_failed > 0) fprintf(stderr,", %d perf_failed", tests_perf_failed );
  if ((tests_failed > 0) || (tests_perf_failed > 0)) fprintf(stderr,"\t\t\t*****");
  fprintf(stderr,"   \n ");
  fprintf(stderr,"-----------------------------------------------------------------------------\n");
  return ( tests_failed > 0 ? 1 : 0 );
}


/* exit test program if any tests have failed */


void exitIfFail(void) {

  if ( tests_failed )
  {
      exit ( test_summary() );
  }
}


