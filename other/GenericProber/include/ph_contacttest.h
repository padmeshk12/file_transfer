/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_contacttest.h
 * CREATED  : 22 August 2006
 *
 * CONTENTS : definitions for contact test 
 *
 * AUTHORS  : EDO, R&D Shangai
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 16 Oct 2005, 
 *
 * Instructions:
 *
 * 1) Copy this template to as many .h files as you require
 *
 * 2) Use the command 'make depend' to make the new
 *    source files visible to the makefile utility
 *
 * 3) To support automatic man page (documentation) generation, follow the
 *    instructions given for the function template below.
 * 
 * 4) Put private functions and other definitions into separate header
 * files. (divide interface and private definitions)
 *
 *****************************************************************************/
#ifndef _PH_CONTACTTEST_H_
#define _PH_CONTACTTEST_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_conf.h"

/*--- defines ---------------------------------------------------------------*/

#define PH_MAX_NUM_COORD 4  /* define number of coordinates for contacttest */

/*--- typedefs --------------------------------------------------------------*/

typedef struct _BULinkedItem
{
    const void           * data;  
    struct _BULinkedItem * next; 
} BULinkedItem;

typedef struct _BULinkedList
{
    BULinkedItem * first;
    BULinkedItem * last;
} BULinkedList;

typedef enum {
  DEFAULT
}phZSearchAlgorithm_t;

typedef enum {
  SELECTION_FIRST_SOME_PASS,
  SELECTION_ALL_PASS
}phContactPointSelection_t;

typedef struct dieCoordinate {
  int x;
  int y;
}phDieCoordinate_t;

typedef struct phContacttestSetup {
  phZSearchAlgorithm_t algorithm;
  phContactPointSelection_t contactPointSelection;
  int largeStep;
  int smallStep;
  int zLimit;
  int safetyWindow;
  int overdrive;
  phDieCoordinate_t dieCoordinate[4];
  double settlingTime;
  BULinkedList *digitalPingroup;
  BULinkedList *analogPingroup;
}*phContacttestSetup_t;

typedef struct phTestCondition {
  char pinGroupName[32];
  double forceCurrent;
  double lowLimit;
  double highLimit;
}*phTestCondition_t;

typedef enum phDCContinuityTestResult
{
  START_POINT,
  ALL_FAIL,
  ALL_PASS,
  SOME_PASS,
  FIRST_SOME_PASS
}phDCContinuityTestResult_t;

typedef enum {
  BASELINE,
  UP,
  DOWN
}phDirection_t;


/*--- external variables ----------------------------------------------------*/

/*--- external function -----------------------------------------------------*/

#endif /* _PH_CONTACTTEST_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
