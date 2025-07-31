/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_timer.c
 * CREATED  : 13 Dec 1999
 *
 * CONTENTS : test cell client interface to driver
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 13 Dec 1999, Michael Vogt, created
 *
 * Instructions:
 *
 * 1) Copy this template to as many .c files as you require
 *
 * 2) Use the command 'make depend' to make visible the new
 *    source files to the makefile utility
 *
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h> 

/*--- module includes -------------------------------------------------------*/

#include "ci_types.h"

#include "ph_frame.h"
#include "ph_timer.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

struct timerElement {
    struct timeval theStartTime;   /* this is the actual time val for the
				      timer start */
    struct timeval theStopTime;   /* this is the actual time val for the
				      timer stop */
    struct timerElement *next; /* unused structures are hold in a free list */
};

/*--- some global variable for timer management -----------------------------*/

static struct timerElement *timerStack = NULL;
static struct timerElement *timerFreeList = NULL;

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 * Timer management
 * should go into a separate file if there is more time......
 ****************************************************************************/

/*****************************************************************************
 * get an empty timer element
 *
 * Description: 
 * Get the new element either from the free list or from the heap 
 ***************************************************************************/
struct timerElement *phFrameGetNewTimerElement(void)
{
    struct timerElement *newElement = NULL;

    if (timerFreeList)
    {
	/* get first element out of the free list, if available */
	newElement = timerFreeList;
	timerFreeList = timerFreeList->next;
    }
    else
	/* free list is empty, allocat a new slot from heap */
	newElement = 
	    (struct timerElement *) malloc(sizeof(struct timerElement));

    /* check whether we got a new element and initialize it */
    if (newElement)
    {
	newElement->theStartTime.tv_sec = 0L;
	newElement->theStartTime.tv_usec = 0L;
	newElement->theStopTime.tv_sec = 0L;
	newElement->theStopTime.tv_usec = 0L;
	newElement->next = NULL;
    }

    /* return either the new element or NULL, if not available */
    if (newElement)
	phFrameStartTimer(newElement);
    return newElement;
}

/*****************************************************************************
 * free an unused timer element
 *
 * Description: 
 * Put the element into the free list for future use
 ***************************************************************************/
void phFrameFreeTimerElement(struct timerElement *oldElement)
{
    /* just add it to the list */
    oldElement->next = timerFreeList;
    timerFreeList = oldElement;
}

/*****************************************************************************
 * start a timer
 ***************************************************************************/
void phFrameStartTimer(struct timerElement *timerElement)
{
    gettimeofday(&timerElement->theStartTime, NULL);
}

/*****************************************************************************
 * stop a timer
 ***************************************************************************/
double phFrameStopTimer(struct timerElement *timerElement)
{
    double elapsed = 0.0;

    gettimeofday(&timerElement->theStopTime, NULL);
    elapsed = 
	timerElement->theStopTime.tv_sec-timerElement->theStartTime.tv_sec;
    elapsed += 
	(timerElement->theStopTime.tv_usec-timerElement->theStartTime.tv_usec)/
	1000000.0;

    return elapsed;
}

/*****************************************************************************
 * push timer on stack
 ***************************************************************************/
static void pushTimer(struct timerElement *timerElement)
{
    timerElement->next = timerStack;
    timerStack = timerElement;
}

/*****************************************************************************
 * get timer from stack
 ***************************************************************************/
static struct timerElement *popTimer(void)
{
    struct timerElement *result = NULL;

    if (timerStack)
    {
	result = timerStack;
	timerStack = timerStack->next;
	result->next = NULL;
    }

    return result;
}

/*****************************************************************************
 *
 * Start a nested timer (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 *
 * Hint: This call does not follow the usual procedure of checking,
 * logging etc. It can therfore not be replaced by some hook function
 * nor will the output go into any log file. This is done for
 * performance reasons: If timers are used for measurement, overhead
 * must be reduced. Also it is then possible to measure the time
 * consumption of the initialization and startup process.
 * 
 ****************************************************************************/
void ph_timer_start(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
)
{
    struct timerElement *newTimer; /* the new timer */

    /* prepare default return values for this call: success and empty
       result string */
    comment_out[0] = '\0';
    *comlen = 0;
    *state_out = CI_CALL_PASS;

    /* main body of this function */

    newTimer = phFrameGetNewTimerElement();
    if (newTimer)
    {
	/* we got the timer, start it and push it to the stack */
	phFrameStartTimer(newTimer);
	pushTimer(newTimer);
    }
    else
    {
	/* could not get a new timer, produce an error */
	fprintf(stderr, 
	    "PH lib: error from framework (not logged): "
	    "timer stack overflow\n");
    }
}

/*****************************************************************************
 *
 * Stop a nested timer (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 * Hint: This call does not follow the usual procedure of checking,
 * logging etc. It can therfore not be replaced by some hook function
 * nor will the output go into any log file. This is done for
 * performance reasons: If timers are used for measurement, overhead
 * must be reduced. Also it is then possible to measure the time
 * consumption of the initialization and startup process.
 * 
 ****************************************************************************/
void ph_timer_stop(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
)
{
    struct timerElement *usedTimer; /* the runing timer */
    double elapsedTime = 0.0;       /* elapsed time for this timer */
    char tempOutputBuffer[80];      /* temporary result string */

    /* prepare default return values for this call: success and empty
       result string */
    comment_out[0] = '\0';
    *comlen = 0;
    *state_out = CI_CALL_PASS;

    /* main body of this function */

    /* get the most recent timer from the stack */
    usedTimer = popTimer();
    if (usedTimer)
    {
	elapsedTime = phFrameStopTimer(usedTimer);
	phFrameFreeTimerElement(usedTimer);

	/* in any case, a printed double should be shorter than the
           output buffer, but let's do it the save way:
	   Print the result to a buffer which is known to be big enough */
	
	sprintf(tempOutputBuffer, "%g", elapsedTime);
	strncpy(comment_out, tempOutputBuffer, CI_MAX_COMMENT_LEN-1);
	comment_out[CI_MAX_COMMENT_LEN-1] = '\0';
	*comlen = strlen(comment_out);
    }
    else
    {
	/* the timer stack was already empty! */
	fprintf(stderr, 
	    "PH lib: error from framework (not logged): "
	    "timer stack underflow\n");
    }
}

/*Begin of Huatek Modifications, Donnie Tu, 04/24/2002*/
/*Issue Number: 334*/
 
/*****************************************************************************
 * Free the empty timer elements in the free list
 *
 * Description:
 ***************************************************************************/
void phFrameFreeListOfTimerElement(void)
{
struct timerElement * next;
    while(timerFreeList)
    {
        next=timerFreeList->next;
        free(timerFreeList);
        timerFreeList=next;
    }
}
/*End of Huatek Modifications*/

/*****************************************************************************
 * End of file
 *****************************************************************************/
