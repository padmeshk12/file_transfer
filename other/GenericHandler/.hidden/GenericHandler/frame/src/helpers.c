/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : helpers.c
 * CREATED  : 14 Dec 1999
 *
 * CONTENTS : Gobal help functions for the framework
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Dec 1999, Michael Vogt, created
 *            Dec 2005, Jiawei Lin, CR28409
 *              allow the user to enable/disable diagnose window by specifying
 *              the driver parameter "enable_diagnose_window"
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

/*--- module includes -------------------------------------------------------*/

#include "ci_types.h"

#include "ph_tools.h"
#include "ph_keys.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_mhcom.h"
#include "ph_hfunc.h"
#include "ph_hhook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"

#include "ph_frame_private.h"

#include "phf_init.h"
#include "helpers.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static int tcomResultToCiResult(phTcomUserReturn_t stReturn)
{
    switch ( stReturn )
    {
        case PHTCOM_CI_CALL_PASS:
            return CI_CALL_PASS;
        case PHTCOM_CI_CALL_ERROR:
            return CI_CALL_ERROR;
        case PHTCOM_CI_CALL_BREAKD:
            return CI_CALL_BREAKD;
        case PHTCOM_CI_CALL_JAM:
            return CI_CALL_JAM;
        case PHTCOM_CI_CALL_LOT_START:
            return CI_CALL_LOT_START;
        case PHTCOM_CI_CALL_LOT_DONE:
            return CI_CALL_LOT_DONE;
        case PHTCOM_CI_CALL_DEVICE_START:
            return CI_CALL_DEVICE_START;
    }

    return CI_CALL_PASS;
}

int phFrameCheckAbort(struct phFrameStruct **f)
{
  static int askForLog = 1;
  long abortFlag = 0;
  long quitFlag = 0;
  phEventResult_t result;
  phTcomUserReturn_t stReturn;
  long response;

  /* 
  * if the framework is not yet initialized, we can not check the
  * abort condition in a save way, assume that abort is not set
  * then 
  */
  if (*f && (*f)->isInitialized){
    phTcomGetSystemFlag((*f)->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    phTcomGetSystemFlag((*f)->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);

    if (abortFlag){
      phLogFrameMessage((*f)->logId, PHLOG_TYPE_FATAL,
                        "PH driver aborts already, not executing prober/handler driver call");
      if (askForLog){
        if( (*f)->enableDiagnoseWindow == YES ) {
          phEventPopup((*f)->eventId, &result, &stReturn, 0, "menu",
                       "Abort detected by handler driver:\n"
                       "Do you want to...\n"
                       "LOG the driver's internal trace for later analysis before aborting, or\n"
                       "CONTINUE aborting the testprogram without log?", 
                       "", "", "", "", "Log", "Continue", 
                       &response, NULL);
          if (response == 6)
            phLogDump((*f)->logId, PHLOG_MODE_ALL);
        } else {
          phLogFrameMessage((*f)->logId, PHLOG_TYPE_MESSAGE_0, "Abort detected by handler driver\n\n");
        }
      }    
      askForLog = 0;
      phLogDumpCommHistory((*f)->logId);
      return 1;
    }
    else if (quitFlag)
    {
      phLogFrameMessage((*f)->logId, PHLOG_TYPE_FATAL,
                        "PH driver quits already, not executing prober/handler driver call");
      askForLog = 1;
      phLogDumpCommHistory((*f)->logId);
      return 1;
    }
    else
    {
      askForLog = 1;
      return 0;
    }
  } else {
    return 0;
  }
}


int phFrameCheckInit(struct phFrameStruct **f)
{
    if (*f && (*f)->isInitialized)
	return 1;

    if (*f && (*f)->initTried)
	return 0;

    *f = initDriverFramework();
    if (!*f || !(*f)->isInitialized)
    {
	if (*f && (*f)->logId)
	{
	    phLogFrameMessage((*f)->logId, PHLOG_TYPE_FATAL,
	        "handler driver will not work\n"
		BIG_FATAL_STRING
		"\nAll further calls to the handler driver are ignored\n\n"
		"Please ensure, that all configuration files are set up correctly\n");

#ifdef DEBUG
	    phLogFrameMessage((*f)->logId, PHLOG_TYPE_FATAL,
		"Dumping all available information for deeper analysis...\n");
	    phLogDump((*f)->logId, PHLOG_MODE_ALL);
#endif
	}
	else
	{
	    fprintf(stderr, 
		"PH lib: FATAL! Handler driver will not work\n"
                BIG_FATAL_STRING
		"\nAll further calls to the handler driver are ignored\n"
		"Detailed analysis is not possible\n");
	}
	return 0;
    }
    else
    {
        phTcomSimClearBins();
	return 1;
    }
}

int phFramePanic(struct phFrameStruct *frame, const char *message)
{
    phEventResult_t result;
    phTcomUserReturn_t stReturn;
    long response = 0;
    int wasDumped = 0;

    char panicMessage[2048];

    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL, 
                      "PANIC!\n\n%s\n\n"
                      "Please read error report above for more information", 
                      message ? message : "(no message given)");
    do {
      if (!wasDumped) {
        if( frame->enableDiagnoseWindow == YES ) {
          sprintf(panicMessage,
                  "Handler driver PANIC:\n"
                  "\n"
                  "%s\n\n"
                  "Please read error report in UI report window for more information\n"
                  "\n"
                  "Do you want to...\n"
                  "DUMP the driver's internal trace for later analysis,\n"
                  "QUIT the testprogram now, or try to\n"
                  "CONTINUE the testprogram ?",
                  message ? message : "(no message given)");

          phEventPopup(frame->eventId, &result, &stReturn, 1, "menu",
                       panicMessage,
                       "", "", "", "", "", "Dump", 
                       &response, NULL);
          if (result == PHEVENT_RES_ABORT)
            return 1;

          if (response == 7) {
            phLogDump(frame->logId, PHLOG_MODE_ALL);
            wasDumped = 1;
          }
        } else {
          phLogFrameMessage(frame->logId, PHLOG_TYPE_MESSAGE_0, "Handler driver PANIC!!\n");
          /* 
          * we simulate the "quit" action in popup window, but do not set the CI_ABORT flag
          * because the customer of CR28409 dislike it.
          * we need to exit the while loop immediately
          */
          response = 1; /* let the function return value TRUE (1) */
          break;        /* exit the loop immediately, avoid the CI_ABORT flag be changed */
        }
      } else {
        if( frame->enableDiagnoseWindow == YES ) {
          sprintf(panicMessage,
                  "Handler driver PANIC:\n"
                  "\n"
                  "%s\n\n"
                  "Please read error report in UI report window for more information\n"
                  "\n"
                  "Do you want to...\n"
                  "QUIT the testprogram now, or try to\n"
                  "CONTINUE the testprogram ?",
                  message);

          phEventPopup(frame->eventId, &result, &stReturn, 1, "menu",
                       panicMessage,
                       "", "", "", "", "", "", 
                       &response, NULL);
          if (result == PHEVENT_RES_ABORT)
            return 1;
        } else {
          phLogFrameMessage(frame->logId, PHLOG_TYPE_MESSAGE_0, "Handler driver PANIC!!\n");
          /* 
          * we simulate the "quit" action in popup window, but do not set the CI_ABORT flag
          * because the customer of CR28409 dislike it.
          * we need to exit the while loop immediately
          */
          response = 1; /* let the function return value TRUE (1) */
          break;        /* exit the loop immediately, avoid the CI_ABORT flag be changed */
        }
      }
      if (response == 1)
        phTcomSetSystemFlag(frame->testerId, PHTCOM_SF_CI_ABORT, 1L);
    } while (response != 1 && response != 8);

    return (response == 1);
}

int phFramePerformEntrySteps(
    struct phFrameStruct *frame,
    phFrameAction_t call,
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
    phEventResult_t eventResult = PHEVENT_RES_VOID;
    phTcomUserReturn_t eventReturn = PHTCOM_CI_CALL_PASS;

    frame->currentAMCall = call;

    /* if necessary, start logging the flag activity */
    if (frame->createTestLog)
    {
	phTcomLogStartGetList(frame->testerId);
	phTcomLogStartSetList(frame->testerId);
    }

    /* read some flag values here */
    phTcomGetSystemFlag(frame->testerId, PHTCOM_SF_CI_PAUSE, 
	&(frame->initialPauseFlag));
    phTcomGetSystemFlag(frame->testerId, PHTCOM_SF_CI_QUIT, 
	&(frame->initialQuitFlag));

    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    phEventAction(frame->eventId, call, &eventResult, 
	parm_list_input, parmcount, comment_out, comlen, &eventReturn);
    if (eventResult == PHEVENT_RES_ABORT)
    {
	/* in case, the event handler has done the full job, it also
           has defined the return value. translate it and return to
           the test cell client */
	*state_out = tcomResultToCiResult(eventReturn);
	return 0;
    }
    else
	return 1;
}

static char *getShortName(phFrameAction_t call)
{
    static char name[8];
    switch ( call )
    {
        case PHFRAME_ACT_DRV_START:
            strcpy(name, "DRVS"); break;
        case PHFRAME_ACT_DRV_DONE:
            strcpy(name, "DRVD"); break;
        case PHFRAME_ACT_LOT_START:
            strcpy(name, "LOTS"); break;
        case PHFRAME_ACT_LOT_DONE:
            strcpy(name, "LOTD"); break;
        case PHFRAME_ACT_DEV_START:
            strcpy(name, "DEVS"); break;
        case PHFRAME_ACT_DEV_DONE:
            strcpy(name, "DEVD"); break;
        case PHFRAME_ACT_PAUSE_START:
            strcpy(name, "PAUS"); break;
        case PHFRAME_ACT_PAUSE_DONE:
            strcpy(name, "PAUD"); break;
        case PHFRAME_ACT_HANDTEST_START:
            strcpy(name, "HANDS"); break;
        case PHFRAME_ACT_HANDTEST_STOP:
            strcpy(name, "HANDD"); break;
        case PHFRAME_ACT_STEPMODE_START:
            strcpy(name, "STEPS"); break;
        case PHFRAME_ACT_STEPMODE_STOP:
            strcpy(name, "STEPD"); break;
        case PHFRAME_ACT_REPROBE:
            strcpy(name, "REPR"); break;
        case PHFRAME_ACT_ACT_CONFIG:
            strcpy(name, "ACONF"); break;
        case PHFRAME_ACT_SET_CONFIG:
            strcpy(name, "SCONF"); break;
        case PHFRAME_ACT_GET_CONFIG:
            strcpy(name, "GCONF"); break;
        case PHFRAME_ACT_GET_ID:
            strcpy(name, "GETID"); break;
        case PHFRAME_ACT_TIMER_START:
            strcpy(name, "TIMS"); break;
        case PHFRAME_ACT_TIMER_STOP:
            strcpy(name, "TIMD"); break;
        case PHFRAME_ACT_GET_STATUS:
            strcpy(name, "GETSTAT"); break;
        case PHFRAME_ACT_SET_STATUS:
            strcpy(name, "SETSTAT"); break;
        default:
            strcpy(name, "NOTDEF"); break;
    }

    return name;
}

void phFramePerformExitSteps(
    struct phFrameStruct *frame,
    phFrameAction_t call,
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
    phTcomUserReturn_t eventReturn;

    switch (*state_out)
    {
      case CI_CALL_ERROR:
	eventReturn = PHTCOM_CI_CALL_ERROR;
	break;
      case CI_CALL_BREAKD:
	eventReturn = PHTCOM_CI_CALL_BREAKD;
	break;
      case CI_CALL_PASS:
      default:
	eventReturn = PHTCOM_CI_CALL_PASS;
	break;
    }

    /* give the event handler and the hook functions a chance to log
       the finished event */

    phEventDoneAction(frame->eventId, call,
	parm_list_input, parmcount, comment_out, comlen, &eventReturn);

    /* if necessary, stop logging the flag activity and create the
       testlog string */
    if (frame->createTestLog && parm_list_input != NULL && comment_out != NULL)
    {
	fprintf(frame->testLogOut, "%s|%c%s%c|[%s]|%c%s%c|[%s]|%s\n",
	    getShortName(call), 
	    strchr(parm_list_input, '\"') ? '\'' : '\"',
	    parm_list_input,
	    strchr(parm_list_input, '\"') ? '\'' : '\"',
	    phTcomLogStopGetList(frame->testerId),
	    strchr(comment_out, '\"') ? '\'' : '\"',
	    comment_out, 
	    strchr(comment_out, '\"') ? '\'' : '\"',
	    phTcomLogStopSetList(frame->testerId),
	    eventReturn == PHTCOM_CI_CALL_PASS ? "PASS" :
	    eventReturn == PHTCOM_CI_CALL_ERROR ? "ERROR" :
	    "BREAK");
	fflush(frame->testLogOut);
    }

    *state_out = tcomResultToCiResult(eventReturn);
}

void phFrameDescribeStateError(
    struct phFrameStruct *frame,
    phStateError_t error, 
    phFrameAction_t action)
{
    switch (error)
    {
      case PHSTATE_ERR_OK:
	break;
      case PHSTATE_ERR_ILLTRANS:
	phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
	    "An unexpected sequence of driver calls is requested\n"
	    "by the test cell client. This may be a fault in the test cell client.\n"
	    "It is tried, to complete the call but the applied actions may fail.\n"
	    "There will be more messages about this situation coming up.");
	break;
      case PHSTATE_ERR_IGNORED:
	switch (action)
	{
	  case PHFRAME_ACT_PAUSE_START:
	  case PHFRAME_ACT_PAUSE_DONE:
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
		"The current driver call is an exception and will be ignored");
	    break;
	  default:
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
		"The current driver call from the test cell client\n"
		"would have no effect and will be ignored");
	}
	break;	
      case PHSTATE_ERR_NOT_INIT:
      case PHSTATE_ERR_INVALID_HDL:
      case PHSTATE_ERR_MEMORY:
	phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
	    "The driver's state control reports an unexpected error (%d),\n"
	    "we try to ignore this and proceed", error);
	break;
    }
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
