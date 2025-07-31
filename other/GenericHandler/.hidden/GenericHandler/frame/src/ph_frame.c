/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_frame.c
 * CREATED  : 14 Jul 1999
 *
 * CONTENTS : test cell client interface to driver
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 1999, Michael Vogt, created
 *            6 Jan 2015, Magco Li, add ph_strip_start and ph_strip_done
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

#include "reconfigure.h"
#include "phf_init.h"
#include "helpers.h"

#include "ph_timer.h"

#include "driver.h"
#include "configuration.h"
#include "device.h"
#include "pause.h"
#include "idrequest.h"
#include "equipment_cmd.h"
#include "exception.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#define DoCheckInit() \
    if (phFrameCheckAbort(&f)) \
    { \
	comment_out[0] = '\0'; \
	*comlen = 0; \
        *state_out = CI_CALL_PASS; \
	return; \
    } \
    if (!phFrameCheckInit(&f)) \
    { \
	comment_out[0] = '\0'; \
	*comlen = 0; \
        *state_out = CI_CALL_ERROR; \
	return; \
    }

#define PrepareCallResults() \
    { \
        comment_out[0] = '\0'; \
        *comlen = 0; \
        *state_out = CI_CALL_PASS; \
    }


#define CheckLevelChange(x) \
    { \
	phStateError_t stateError; \
        stateError = phStateCheckLevelChange(f->stateId, (x)); \
        if (stateError != PHSTATE_ERR_OK) \
        { \
	    phFrameDescribeStateError(f, stateError, (x)); \
	    if (stateError == PHSTATE_ERR_IGNORED) \
	        return; \
        } \
    }


#define EntrySteps(x) \
    { \
        if (!phFramePerformEntrySteps(f, (x), \
	    parm_list_input, parmcount, comment_out, comlen, state_out)) \
	    return; \
    }

#define ExitSteps(x) \
    { \
        phFramePerformExitSteps(f, (x), \
	    parm_list_input, parmcount, comment_out, comlen, state_out); \
    }

/*--- the module global framework descriptor --------------------------------*/

static struct phFrameStruct *f = NULL;

/*--- functions -------------------------------------------------------------*/

static int tcomResultToCiResult(phTcomUserReturn_t stReturn)
{
    int retVal;

    switch ( stReturn )
    {
        case PHTCOM_CI_CALL_PASS:
            retVal = CI_CALL_PASS;
            break;
        case PHTCOM_CI_CALL_ERROR:
            retVal = CI_CALL_ERROR;
            break;
        case PHTCOM_CI_CALL_BREAKD:
            retVal = CI_CALL_BREAKD;
            break;
        // NOTE: kaw 03Mar2005 - application framework 
        // does not yet understand these new return codes.
        // When calling routines that can return these new error codes, 
        // convert them to one of the above codes before returning.
        case PHTCOM_CI_CALL_JAM:
            retVal = CI_CALL_JAM;
            break;
        case PHTCOM_CI_CALL_LOT_START:
            retVal = CI_CALL_LOT_START;
            break;
        case PHTCOM_CI_CALL_LOT_DONE:
            retVal = CI_CALL_LOT_DONE;
            break;
        case PHTCOM_CI_CALL_DEVICE_START:
            retVal = CI_CALL_DEVICE_START;
            break;
        case PHTCOM_CI_CALL_TIMEOUT:
            retVal = CI_CALL_TIMEOUT;
            break;
        default:
            retVal = CI_CALL_PASS;
            break;
    }
    return retVal;
}

static int sendResult(phLogId_t logger, char *comment_out, int *comlen, 
    const char *result)
{
    /* Copy a maximum of CI_MAX_COMMENT_LEN to the output field, than
       ensure that the output field is 0 terminated (may be not, if
       the last action caused a truncation of the string).  In the
       end, compare the result with the expected result.  return 1 on
       success, 0 on truncation */

    strncpy(comment_out, result, CI_MAX_COMMENT_LEN-1);
    comment_out[CI_MAX_COMMENT_LEN-1] = '\0';
    *comlen = strlen(comment_out);
    if (strcmp(comment_out, result) != 0)
    {
	if (logger)
	    phLogFrameMessage(logger, PHLOG_TYPE_ERROR,
		"the result string of a call was truncated");
	else
	    fprintf(stderr, 
		"PH lib: error from framework (not logged): "
		"the result string of a call was truncated\n");
	return 0;
    }
    else
	return 1;
}

/*****************************************************************************
 *
 * Start a handler driver session (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ***************************************************************************/
void ph_driver_start(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_driver_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_driver_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_DRV_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_DRV_START);


    *state_out = tcomResultToCiResult(phFrameDriverStart(f));
    if (*state_out != CI_CALL_PASS)
    {
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DRV_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DRV_START); 
}    

/*****************************************************************************
 *
 * Stop a handler driver session (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ***************************************************************************/
void ph_driver_done(
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
    /* dump the comm logs */
    if (phFrameCheckInit(&f))
      phLogDumpCommHistory(f->logId);

    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_driver_done(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_driver_done called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_DRV_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_DRV_DONE);

    *state_out = tcomResultToCiResult(phFrameDriverDone(f));
    if (*state_out != CI_CALL_PASS)
    {
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DRV_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DRV_DONE); 
}    

/*****************************************************************************
 *
 * Start a new lot of devices (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_lot_start(
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
    long levelEnd = 0;
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_lot_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
	"ph_lot_start called with parm ->%s<-", parm_list_input);

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_DEBUG, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_LOT_START);

    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_LOT_START);

    /* do the main work of this driver function */
    *state_out = tcomResultToCiResult(phFrameLotStart(f, parm_list_input));
    if (*state_out != CI_CALL_PASS)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
        "ph_lot_start, phFrameLotStart did not return CI_CALL_PASS, returned = ->%d<-.  Will return CI_CALL_ERROR", *state_out);
        *state_out = CI_CALL_ERROR; // framework does not understand new return codes - kaw
        return;
    }

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_DEBUG, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_LOT_START);
    sendResult(f->logId, comment_out, comlen, "YES");

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_LOT_START); 
}    

/*****************************************************************************
 *
 * Complete a lot of devices (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_lot_done(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_lot_done(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_lot_done called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_LOT_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_LOT_DONE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameLotDone(f, parm_list_input));

//    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
//    "ph_lot_done, after call to phFrameLotDone, parm_list_input = ->%s<-",
//    parm_list_input);

    if (*state_out != CI_CALL_PASS)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
        "ph_lot_done, phFrameLotDone did not return CI_CALL_PASS, returned = ->%d<-.  Will return CI_CALL_ERROR", *state_out);
        *state_out = CI_CALL_ERROR; // framework does not understand new return codes - kaw
        return;
    }

    /* change the state of the driver : leave the lot level */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_LOT_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_LOT_DONE); 

    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
    "Exiting ph_lot_done");
}

/*****************************************************************************
 *
 * Wait for device insertion (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_device_start(
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
    long flagValue = 0;

    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_device_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_device_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_DEV_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_DEV_START);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameDeviceStart(f));

    /* going to get messy now - kaw 01Mar2005  */
    /* If ph_device_start detects end of lot,  we will force exit of this level */
    /*
    * (1)For Mirae, while waiting for the device start, we will
    *    encounter the LOT START, LOT DONE; and only LOT DONE need to be handled
    * (2)For Techwing, while waiting for the device start, we will
    *    encounter the LOT START, FIRST RETEST, SECOND RETEST, LOT DONE
    *    and only LOT DONE need to be handled
    *
    *  Hence, here we need to perform right action ( set App Model Level End )
    *                              Jiawei Lin, Jan 2006
    */       
    if (*state_out == CI_CALL_LOT_DONE)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
                          "ph_device_start detected end of lot, will exit level");
        /* phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L); */
        /* device are not available */
        sendResult(f->logId, comment_out, comlen, "NO");
        *state_out = CI_CALL_PASS;
        return;
    }

    /* this is the standard error case */
    /* NOTE: this used to be in phFrameDeviceStart, but was pulled up to here to allow lot_done - kaw 01Mar05 */
    if (*state_out != CI_CALL_PASS)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                          "ph_device_start, phFrameDeviceStart did not return CI_CALL_PASS, returned = ->%d<-.  Will return CI_CALL_ERROR", *state_out);
        *state_out = CI_CALL_ERROR; // framework does not understand new return codes - kaw
        return;
    }
    /* END: going to get messy now - kaw 01Mar2005  */

    sendResult(f->logId, comment_out, comlen, "YES");
    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
                      "ph_device_start, device start detected");
    /* this is the normal case */
    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DEV_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DEV_START); 
}

/*****************************************************************************
 *
 * Bin devices (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_device_done(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_device_done(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_device_done called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_DEV_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_DEV_DONE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameDeviceDone(f));
    if (*state_out != CI_CALL_PASS)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                          "ph_device_done, phFrameDeviceDone did not return CI_CALL_PASS, returned = ->%d<-.  Will return CI_CALL_ERROR", *state_out);
        *state_out = CI_CALL_ERROR; // framework does not understand new return codes - kaw
        return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DEV_DONE);

    /* clear the bins */
    phTcomSimClearBins();

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DEV_DONE); 
}

/*****************************************************************************
 *
 * Enter pause mode in test cell client (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_pause_start(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_pause_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_pause_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_PAUSE_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_PAUSE_START);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFramePauseStart(f));
    if (*state_out != CI_CALL_PASS)
    {
	return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_PAUSE_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_PAUSE_START); 
}

/*****************************************************************************
 *
 * Leave pause mode in test cell client (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_pause_done(
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
/*    long abortFlag;*/
/*    long quitFlag;*/
/*    long response;*/
/*    phEventResult_t result;*/
/*    phTcomUserReturn_t stReturn;*/

    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_pause_done(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_pause_done called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_PAUSE_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_PAUSE_DONE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFramePauseDone(f));
    if (*state_out != CI_CALL_PASS)
    {
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_PAUSE_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_PAUSE_DONE); 
}    

/*****************************************************************************
 *
 * Go into hand test mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_handtest_start(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_handtest_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_handtest_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_HANDTEST_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_HANDTEST_START);

    /* do the main work of this driver function */

    /* ... */
    /* nothing more to do, all important steps are done by the state
       controller */

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_HANDTEST_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_HANDTEST_START); 
}

/*****************************************************************************
 *
 * Leave hand test mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_handtest_stop(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_handtest_stop(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_handtest_stop called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_HANDTEST_STOP);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_HANDTEST_STOP);

    /* do the main work of this driver function */

    /* ... */
    /* nothing more to do, all important steps are done by the state
       controller */

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_HANDTEST_STOP);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_HANDTEST_STOP); 
}   

/*****************************************************************************
 *
 * Go into single step mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_stepmode_start(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_stepmode_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_stepmode_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_STEPMODE_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_STEPMODE_START);

    /* do the main work of this driver function */

    /* ... */
    /* nothing more to do, all important steps are done by the state
       controller */

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_STEPMODE_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_STEPMODE_START); 
}    

/*****************************************************************************
 *
 * Leave single step mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_stepmode_stop(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_stepmode_stop(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_stepmode_stop called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_STEPMODE_STOP);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_STEPMODE_STOP);

    /* do the main work of this driver function */

    /* ... */
    /* nothing more to do, all important steps are done by the state
       controller */

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_STEPMODE_STOP);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_STEPMODE_STOP); 
}    

/*****************************************************************************
 *
 * Issue a reprobe command to the handler (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_reprobe(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_reprobe(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_reprobe called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_REPROBE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_REPROBE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameTryReprobe(f));
    if (*state_out != CI_CALL_PASS)
    {
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_REPROBE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_REPROBE); 
}    

/*****************************************************************************
 *
 * Reconfigure the driver interactively (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_interactive_configuration(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_interactive_configuration(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_interactive_configuration called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_ACT_CONFIG);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_ACT_CONFIG);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameInteractiveConfiguration(f));
    if (*state_out != CI_CALL_PASS)
    {
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_ACT_CONFIG);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_ACT_CONFIG); 
}    

/*****************************************************************************
 *
 * Set or change a configuration value (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_set_configuration(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_set_configuration(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_set_configuration called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_SET_CONFIG);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_SET_CONFIG);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameSetConfiguration(f, parm_list_input));
    if (*state_out != CI_CALL_PASS)
    {
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_SET_CONFIG);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_SET_CONFIG); 
}

/*****************************************************************************
 *
 * Get a configuration value (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_get_configuration(
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
    char *result;

    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_get_configuration(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_get_configuration called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_GET_CONFIG);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_GET_CONFIG);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameGetConfiguration(f, parm_list_input, &result));
    if (*state_out != CI_CALL_PASS)
    {
      return;
    }
    sendResult(f->logId, comment_out, comlen, result);

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_GET_CONFIG);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_GET_CONFIG); 
}

/*****************************************************************************
 *
 * Get ID information (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ****************************************************************************/
void ph_get_id(
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
    const char *result;

    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_get_id(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_get_id called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_GET_ID);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_GET_ID);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameGetID(f, const_cast<char*>(parm_list_input?parm_list_input:"(NULL)"), &result));
    if (*state_out != CI_CALL_PASS)
    {
      return;
    }
    sendResult(f->logId, comment_out, comlen, result);

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_GET_ID);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_GET_ID); 
}

/*****************************************************************************
 *
 * Load a new strip
 *
 * History: 24 Oct 2014, magco li added function
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_strip_start(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
        "ph_strip_start(\"%s\")",
        parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
        "ph_strip_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_STRIP_START);

    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_STRIP_START);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameStripStart(f, parm_list_input));
    if (*state_out != CI_CALL_PASS)
        return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_STRIP_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_STRIP_START);
}

/*****************************************************************************
 *
 * Unload the current strip
 *
 * History: 24 Oct 2014, magco li added function
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_strip_done(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
        "ph_strip_done(\"%s\")",
        parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
        "ph_strip_done called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_STRIP_DONE);

    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_STRIP_DONE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameStripDone(f, parm_list_input));
    if (*state_out != CI_CALL_PASS)
        return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_STRIP_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_STRIP_DONE);
}

/*****************************************************************************
 *
 * Get STATUS information (application model call)
 *
 * Authors: Ken Ward
 *
 * Description:
 * 
 * The ph_get_status function can be used to query various operating states
 * of the handler:
 *   VERSION?
 *   NAME?
 *   TESTSET?
 *   SETTEMP?
 *   MEASTEMP [1 - 9] ?
 *   STATUS?
 *   JAM?
 *   JAMCODE?
 *   JAMQUE?
 *   SETLAMP?
 *
 * i.e. from the DUT board, the handler, the handlers software
 * revision, or the handler driver revision. These IDs may only be
 * retrieved, if the HW supports this feature. Otherwise nothing
 * happens (an empty ID string is returned and an error message is
 * printed).  
 *
 * Input parameter format:
 *
 * The input parameter is a key name for the requested ID. The
 * following IDs may be requested:
 *
 * driver: the drivers version number and suported handler family
 *
 * handler: the connected handler model and family
 *
 * firmware: the handlers firmware revision
 *
 * dut_board: the dut board ID
 *
 * 
 ***************************************************************************/
void ph_get_status(
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
    char *result;

    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
    "ph_get_status(\"%s\")",
    parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
    "ph_get_status called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
    "ph_get_status, after PrepareCallResults");

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_GET_STATUS);
    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
    "ph_get_status, after CheckLevelChange");

    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_GET_STATUS);
    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
    "ph_get_status, after EntrySteps");

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameGetStatus(f, parm_list_input, &result));

    phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
    "ph_get_status, after call to phFrameGetStatus, parm_list_input = ->%s<-, result ->%s<-",
    parm_list_input, result);

    if (*state_out != CI_CALL_PASS)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
        "ph_get_status, phFrameGetStatus did not return CI_CALL_PASS, returned = ->%d<-.  Will return CI_CALL_ERROR", *state_out);
        *state_out = CI_CALL_ERROR;
        return;
    }
    sendResult(f->logId, comment_out, comlen, result);

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_GET_STATUS);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_GET_STATUS); 

}

/*****************************************************************************
 *
 * Set STATUS information (test cell client call)
 *
 * Authors: Ken Ward
 *
 * Description:
 * 
 * The ph_set_status function can be used to set operating conditions
 * for the handler:
 *   LOADER
 *   START
 *   STOP
 *   PLUNGER [0 or 1]
 *   SETNAME [string - 1 to 12 chars]
 *   RUNMODE [0 or 1]
 * 
 * i.e. from the DUT board, the handler, the handlers software
 * revision, or the handler driver revision. These IDs may only be
 * retrieved, if the HW supports this feature. Otherwise nothing
 * happens (an empty ID string is returned and an error message is
 * printed).  
 *
 * Input parameter format:
 *
 * The input parameter is a key name for the requested ID. The
 * following IDs may be requested:
 *
 * driver: the drivers version number and suported handler family
 *
 * handler: the connected handler model and family
 *
 * firmware: the handlers firmware revision
 *
 * dut_board: the dut board ID
 *
 * 
 ***************************************************************************/
void ph_set_status(
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
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_set_status(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_set_status called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_SET_STATUS);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_SET_STATUS);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameSetStatus(f, const_cast<char *>(parm_list_input?parm_list_input:"(NULL)")));
    if (*state_out != CI_CALL_PASS)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_3,
        "ph_set_status, phFrameSetStatus did not return CI_CALL_PASS, returned = ->%d<-.  Will return CI_CALL_ERROR", *state_out);
        *state_out = CI_CALL_ERROR;
        return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_SET_STATUS);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_SET_STATUS); 
}


/*****************************************************************************
 *
 * Execute handler's setup and action command by GPIB
 *
 * Authors: Jiawei Lin
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_exec_gpib_cmd(
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
  char *result = NULL; 

  /* check the initialization status and log the incoming call */
  DoCheckInit();

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
    "ph_exec_gpib_cmd(\"%s\")",
    parm_list_input ? parm_list_input : "(NULL)");
  phLogFrameMessage(f->logId, LOG_NORMAL,
    "ph_exec_gpib_cmd called");

  /* prepare result of this call, always return PASS */
  PrepareCallResults();

  /* check whether this call is legal in the current state */
  CheckLevelChange(PHFRAME_ACT_EXEC_GPIB_CMD);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_exec_gpib_cmd, after CheckLevelChange");

  /*
  *  give the event handler (and some hook functions) the chance to
  *  jump in to perform the job or to do a single step dialog
  */
  EntrySteps(PHFRAME_ACT_EXEC_GPIB_CMD);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_exec_gpib_cmd, afer EntrySteps");

  /* do the main work of this driver function */
  *state_out = tcomResultToCiResult(phFrameExecGpibCmd(f, parm_list_input, &result));
  phLogFrameMessage(f->logId, LOG_DEBUG,
                    "ph_exec_gpib_cmd, after call to phFrameExecGpibCmd, parm_list_input = ->%s<-, "
                    "result ->%s<-", parm_list_input, result);

  if( *state_out != CI_CALL_PASS )
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_exec_gpib_cmd, phFrameExecGpibCmd did not return "
                      "CI_CALL_PASS, returned value = ->%d<-. Will return CI_CALL_ERROR",
                      *state_out);
    *state_out = CI_CALL_ERROR;
    return;
  }

  sendResult(f->logId, comment_out, comlen, result);

  /* change the state of the driver */
  phStateChangeLevel(f->stateId, PHFRAME_ACT_EXEC_GPIB_CMD);
  /*
  * give the event handler (and some hook functions) the chance to
  * log the final state
  */
  ExitSteps(PHFRAME_ACT_EXEC_GPIB_CMD);
}


/*****************************************************************************
 *
 * Execute handler's query command by GPIB
 *
 * Authors: Jiawei Lin
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_exec_gpib_query(
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
  char *result = NULL;

  /* check the initialization status and log the incoming call */
  DoCheckInit();

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
    "ph_exec_gpib_query(\"%s\")",
    parm_list_input ? parm_list_input : "(NULL)");
  phLogFrameMessage(f->logId, LOG_NORMAL,
    "ph_exec_gpib_query called");

  /* prepare result of this call, always return PASS */
  PrepareCallResults();

  /* check whether this call is legal in the current state */
  CheckLevelChange(PHFRAME_ACT_EXEC_GPIB_QUERY);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_exec_gpib_query, afer CheckLevelChange");

  /*
  *  give the event handler (and some hook functions) the chance to
  *  jump in to perform the job or to do a single step dialog
  */
  EntrySteps(PHFRAME_ACT_EXEC_GPIB_QUERY);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_exec_gpib_query, afer EntrySteps");

  /* do the main work of this driver function */
  *state_out = tcomResultToCiResult(phFrameExecGpibQuery(f, parm_list_input, &result));
  phLogFrameMessage(f->logId, LOG_DEBUG,
                    "ph_exec_gpib_query, after call to phFrameExecGpibQuery, parm_list_input = ->%s<-, "
                    "result ->%s<-", parm_list_input, result);

  if( *state_out != CI_CALL_PASS )
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_exec_gpib_query, phFrameExecGpibQuery did not return "
                      "CI_CALL_PASS, returned value = ->%d<-. Will return CI_CALL_ERROR",
                      *state_out);
    *state_out = CI_CALL_ERROR;
    return;
  }

  sendResult(f->logId, comment_out, comlen, result);

  /* change the state of the driver */
  phStateChangeLevel(f->stateId, PHFRAME_ACT_EXEC_GPIB_QUERY);
  /*
  * give the event handler (and some hook functions) the chance to
  * log the final state
  */
  ExitSteps(PHFRAME_ACT_EXEC_GPIB_QUERY);
}

/*****************************************************************************
 *
 * send command to handler
 *
 * Authors: Adam Huang
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_send(
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
  char *result = NULL; 
  /* check the initialization status and log the incoming call */
  DoCheckInit();

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
    "ph_send(\"%s\")",
    parm_list_input ? parm_list_input : "(NULL)");
  phLogFrameMessage(f->logId, LOG_NORMAL,
    "ph_send called");

  /* prepare result of this call, always return PASS */
  PrepareCallResults();

  /* check whether this call is legal in the current state */
  CheckLevelChange(PHFRAME_ACT_SEND_MESSAGE);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_send, after CheckLevelChange");

  /*
  *  give the event handler (and some hook functions) the chance to
  *  jump in to perform the job or to do a single step dialog
  */
  EntrySteps(PHFRAME_ACT_SEND_MESSAGE);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_send, afer EntrySteps");

  /* do the main work of this driver function */
  *state_out = tcomResultToCiResult(phFrameSend(f, parm_list_input, &result));
  phLogFrameMessage(f->logId, LOG_DEBUG,
                    "ph_send, after call to phFrameSend, parm_list_input = ->%s<-, "
                    "result ->%s<-", parm_list_input, result);

  if(*state_out == CI_CALL_TIMEOUT)
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_send, phFrameSend returned time out"
                      "returned value = ->%d<-. Will return CI_CALL_TIMEOUT",
                      *state_out);
    return;
  }

  if( *state_out != CI_CALL_PASS )
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_send, phFrameSend did not return "
                      "CI_CALL_PASS, returned value = ->%d<-. Will return CI_CALL_ERROR",
                      *state_out);
    *state_out = CI_CALL_ERROR;
    return;
  }

  sendResult(f->logId, comment_out, comlen, result);

  /* change the state of the driver */
  phStateChangeLevel(f->stateId, PHFRAME_ACT_SEND_MESSAGE);
  /*
  * give the event handler (and some hook functions) the chance to
  * log the final state
  */
  ExitSteps(PHFRAME_ACT_SEND_MESSAGE);
}

/*****************************************************************************
 *
 * receive reply from handler
 *
 * Authors: Adam Huang
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_receive(
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
  char *result = NULL;

  /* check the initialization status and log the incoming call */
  DoCheckInit();

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
    "ph_receive(\"%s\")",
    parm_list_input ? parm_list_input : "(NULL)");
  phLogFrameMessage(f->logId, LOG_NORMAL,
    "ph_receive called");

  /* prepare result of this call, always return PASS */
  PrepareCallResults();

  /* check whether this call is legal in the current state */
  CheckLevelChange(PHFRAME_ACT_RECEIVE_MESSAGE);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_receive, afer CheckLevelChange");

  /*
  *  give the event handler (and some hook functions) the chance to
  *  jump in to perform the job or to do a single step dialog
  */
  EntrySteps(PHFRAME_ACT_RECEIVE_MESSAGE);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_receive, afer EntrySteps");

  /* do the main work of this driver function */
  *state_out = tcomResultToCiResult(phFrameReceive(f, parm_list_input, &result));
  phLogFrameMessage(f->logId, LOG_DEBUG,
                    "ph_receive, after call to phFrameReceive, parm_list_input = ->%s<-, "
                    "result ->%s<-", parm_list_input, result);
  if(*state_out == CI_CALL_TIMEOUT)
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_receive, phFrameReceive returned time out"
                      "returned value = ->%d<-. Will return CI_CALL_TIMEOUT",
                      *state_out);
    return;
  }

  if( *state_out != CI_CALL_PASS )
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_receive, phFrameReceive did not return "
                      "CI_CALL_PASS, returned value = ->%d<-. Will return CI_CALL_ERROR",
                      *state_out);
    *state_out = CI_CALL_ERROR;
    return;
  }

  sendResult(f->logId, comment_out, comlen, result);

  /* change the state of the driver */
  phStateChangeLevel(f->stateId, PHFRAME_ACT_RECEIVE_MESSAGE);
  /*
  * give the event handler (and some hook functions) the chance to
  * log the final state
  */
  ExitSteps(PHFRAME_ACT_RECEIVE_MESSAGE);
}

/*****************************************************************************
 *
 * Get a SRQ status byte
 *
 * Authors: Xiaofei Han
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_get_srq_status_byte(
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
  char *result = NULL;

  /* check the initialization status and log the incoming call */
  DoCheckInit();

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
    "ph_get_srq_status_byte(\"%s\")",
    parm_list_input ? parm_list_input : "(NULL)");
  phLogFrameMessage(f->logId, LOG_NORMAL,
    "ph_get_srq_status_byte called");

  /* prepare result of this call, always return PASS */
  PrepareCallResults();

  /* check whether this call is legal in the current state */
  CheckLevelChange(PHFRAME_ACT_GET_SRQ_STATUS_BYTE);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_get_srq_status_byte, afer CheckLevelChange");

  /*
  *  give the event handler (and some hook functions) the chance to
  *  jump in to perform the job or to do a single step dialog
  */
  EntrySteps(PHFRAME_ACT_GET_SRQ_STATUS_BYTE);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_get_srq_status_byte, afer EntrySteps");

  /* do the main work of this driver function */
  *state_out = tcomResultToCiResult(phFrameGetSrqStatusByte(f, parm_list_input, &result));
  phLogFrameMessage(f->logId, LOG_DEBUG,
                    "ph_get_srq_status_byte, after call to phFrameGetSrqStatusByte, parm_list_input = ->%s<-, "
                    "result ->%s<-", parm_list_input, result);

  if( *state_out != CI_CALL_PASS)
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_get_srq_status_byte, phFrameGetSrqStatusByte did not return "
                      "CI_CALL_PASS, returned value = ->%d<-. Will return CI_CALL_ERROR",
                      *state_out);
    *state_out = CI_CALL_ERROR;
    return;
  }

  sendResult(f->logId, comment_out, comlen, result);

  /* change the state of the driver */
  phStateChangeLevel(f->stateId, PHFRAME_ACT_GET_SRQ_STATUS_BYTE);
  /*
  * give the event handler (and some hook functions) the chance to
  * log the final state
  */
  ExitSteps(PHFRAME_ACT_GET_SRQ_STATUS_BYTE);
}

/*****************************************************************************
 *
 * Get a SRQ status byte
 *
 * Authors: Adam Huang
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_read_status_byte(
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
  char *result = NULL;

  /* check the initialization status and log the incoming call */
  DoCheckInit();

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
    "ph_read_status_byte(\"%s\")",
    parm_list_input ? parm_list_input : "(NULL)");
  phLogFrameMessage(f->logId, LOG_NORMAL,
    "ph_read_status_byte called");

  /* prepare result of this call, always return PASS */
  PrepareCallResults();

  /* check whether this call is legal in the current state */
  CheckLevelChange(PHFRAME_ACT_READ_STATUS_BYTE);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_read_status_byte, afer CheckLevelChange");

  /*
  *  give the event handler (and some hook functions) the chance to
  *  jump in to perform the job or to do a single step dialog
  */
  EntrySteps(PHFRAME_ACT_READ_STATUS_BYTE);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_read_status_byte, afer EntrySteps");

  /* do the main work of this driver function */
  *state_out = tcomResultToCiResult(phFrameGetSrqStatusByte(f, parm_list_input, &result));
  phLogFrameMessage(f->logId, LOG_DEBUG,
                    "ph_read_status_byte, after call to phFrameGetSrqStatusByte, parm_list_input = ->%s<-, "
                    "result ->%s<-", parm_list_input, result);

  if(*state_out == CI_CALL_TIMEOUT)
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_read_status_byte, phFrameGetSrqStatusByte returned time out"
                      "returned value = ->%d<-. Will return CI_CALL_TIMEOUT",
                      *state_out);
    return;
  }

  if( *state_out != CI_CALL_PASS )
  {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_read_status_byte, phFrameGetSrqStatusByte did not return "
                      "CI_CALL_PASS, returned value = ->%d<-. Will return CI_CALL_ERROR",
                      *state_out);
    *state_out = CI_CALL_ERROR;
    return;
  }

  sendResult(f->logId, comment_out, comlen, result);

  /* change the state of the driver */
  phStateChangeLevel(f->stateId, PHFRAME_ACT_READ_STATUS_BYTE);
  /*
  * give the event handler (and some hook functions) the chance to
  * log the final state
  */
  ExitSteps(PHFRAME_ACT_READ_STATUS_BYTE);
}

/*****************************************************************************
 *
 * Free the driver framework
 *
 * Authors: Donnie Tu
 *
 * Description:
 * Free the driver framework just before exiting the program
 *
 * Returns: None
 *
 ***************************************************************************/

void freeDriverFrameworkF(void)
{
    if (f==NULL)
	return;

    if( f->globalConfId !=NULL)
    {
	phConfDestroyConf(f->globalConfId);
    	free(f->globalConfId);
    	f->globalConfId=NULL;
    }
    if( f->specificConfId !=NULL)
    {
	phConfDestroyConf( f->specificConfId);
	free(f->specificConfId);
	f->specificConfId=NULL;
    }
    if ( f->attrId!=NULL)
    {
	phConfDestroyAttr(f->attrId);
	free(f->attrId);
	f->attrId=NULL;
    }
    if (f->testerId!=NULL)
    { 
        phTcomDestroy(f->testerId);
	f->testerId=NULL;
    }
    if( f->comId)
    { 
      if(f->connectionType == PHCOM_IFC_GPIB)
      {
        phComGpibClose(f->comId);
      }
      else if(f->connectionType == PHCOM_IFC_LAN || f->connectionType == PHCOM_IFC_LAN_SERVER)
      {
        phComLanClose(f->comId);
      }
      else if(f->connectionType == PHCOM_IFC_RS232)
      {
        phComRs232Close(f->comId);
      }

	free(f->comId);
	f->comId=NULL;
     }

    phTcomDestroy(f->testerId);
    freeBinMapping(&(f->binMapHdl));

    phEventFree(&(f->eventId));
    phEstateFree(&(f->estateId));
    phFrameFreeTimerElement(f->totalTimer);
    phFrameFreeTimerElement(f->shortTimer);
    phFrameFreeListOfTimerElement();
 
    phStateFree(&(f->stateId));


    phLogFree(&(f->logId));
    freeUserDialog(&(f->dialog_comm_start));
    freeUserDialog(&(f->dialog_config_start));

    free(f);
    f=NULL;
}



/*****************************************************************************
 *
 * Free all the data structures created by ph_driver_start. 
 *
 * Authors: Xiaofei Han
 *
 * History: 10 Sept 2011, Xiaofei Han, created
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ***************************************************************************/
void ph_driver_free(
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
    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    freeDriverFrameworkF();

    return;
}    


/*****************************************************************************
 *
 * Get the site related information from the driver.
 *
 * Authors: Xiaofei Han
 *
 * History: 2 Apr 2015, Xiaofei Han, created
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_get_site_config(
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
    phEstateSiteUsage_t *sitePopulation = NULL;
    static phEstateSiteUsage_t savedSitePopulation;
    int entries = 0;
    int i;
    int site;
    const char *equipmentSiteName = NULL;
    char dummyString[CI_MAX_COMMENT_LEN-1] = "";

    /* site info*/
    char equipmentSiteIDList[CI_MAX_COMMENT_LEN-1] = ""; /* equipment enabled site ids */
    char siteIDList[CI_MAX_COMMENT_LEN-1] = ""; /* tester enabled site numbers */

#if 0
    /* bin info*/
    char equipmentBinList[CI_MAX_COMMENT_LEN-1] = ""; /* equipment bin ids for enabled sites */
    char hardBinList[CI_MAX_COMMENT_LEN-1] = ""; /* tester hard bin ids for enabled sites */
    char softBinList[CI_MAX_COMMENT_LEN-1] = ""; /* tester soft bin ids for enabled sites */
#endif

    /* device id info */
    char xCoordList[CI_MAX_COMMENT_LEN-1] = ""; /* x coordinates for enabled sites */
    char yCoordList[CI_MAX_COMMENT_LEN-1] = ""; /* y coordinates for enabled sites */

    /* final string */
    char fullList[CI_MAX_COMMENT_LEN-1] = "";

    long dieX = 0, dieY = 0;

    phStateLevel_t cLevelState = PHSTATE_LEV_START;

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* get handler site information */
    phStateGetLevel(f->stateId, &cLevelState);
    if(cLevelState == PHSTATE_LEV_DEV_STARTED)
    {
      /* only read site configration when ph_device_start is called*/
      phEstateAGetSiteInfo(f->estateId, &sitePopulation, &entries);
      savedSitePopulation = *sitePopulation;
    } 
    else if (cLevelState < PHSTATE_LEV_DEV_STARTED)
    {
      /* if calling this function before ph_device_start, empty the population data*/
      sitePopulation = NULL;
    }
    else
    {
      /* the driver clears the estate data after ph_device_done is called */
      /* so save the data */
      sitePopulation = &savedSitePopulation;
    }
    
    /* loop through all sites */
    if(sitePopulation != NULL)
    {
      for (i = 0; i < f->numSites; i++)
      {
        site = i;
        if(sitePopulation != NULL &&
          (sitePopulation[i] == PHESTATE_SITE_POPULATED ||
           sitePopulation[i] == PHESTATE_SITE_POPDEACT))
        {
          phConfConfString(f->specificConfId, PHKEY_SI_HSIDS, 1, &site, &equipmentSiteName);

          /* append the equipment site ID to the equipment site id list */
          strcpy(dummyString, equipmentSiteIDList);
          snprintf(equipmentSiteIDList,
                   CI_MAX_COMMENT_LEN-1,
                   (i==f->numSites-1)?"%s %s":"%s %s ",
                   dummyString,
                   equipmentSiteName);

          /* append the tester site number to the tester site number list */
          strcpy(dummyString, siteIDList);
          snprintf(siteIDList,
                   CI_MAX_COMMENT_LEN-1,
                   (i==f->numSites-1)?"%s %ld":"%s %ld ",
                   dummyString,
                   f->stToHandlerSiteMap[i]);

          phTcomGetDiePosXYOfSite(f->testerId, f->stToHandlerSiteMap[i], &dieX, &dieY);

          /* make the die X string */
          strcpy(dummyString, xCoordList);
          snprintf(xCoordList,
                   CI_MAX_COMMENT_LEN-1,
                   (i==f->numSites-1)?"%s %ld":"%s %ld ",
                   dummyString, dieX);

          /* make the die X string */
          strcpy(dummyString, yCoordList);
          snprintf(yCoordList,
                   CI_MAX_COMMENT_LEN-1,
                   (i==f->numSites-1)?"%s %ld":"%s %ld ",
                   dummyString, dieY);
        }
      }
    }

    /* make the final result */
    snprintf(fullList,
             CI_MAX_COMMENT_LEN-2,
             "EQPSITE:%s|SITE:%s|XCOORD:%s|YCOORD:%s",
             equipmentSiteIDList,
             siteIDList,
             xCoordList,
             yCoordList);

    sendResult(f->logId, comment_out, comlen, fullList);

    return;
}


void ph_set_test_results(
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
  char *temp = parm_list_input, *tempSaved = NULL;
  char *sites = NULL, *sitesSaved = NULL, *site = NULL;
  char *hbs = NULL, *hbsSaved = NULL, *hb = NULL;
  char *sbs = NULL, *sbsSaved = NULL, *sb = NULL;
  char *pfs = NULL, *pfsSaved = NULL, *pf = NULL;

  PrepareCallResults();

  //get site list/hb list/sb list
  sites = strtok_r(temp, "|", &tempSaved);
  hbs = strtok_r(NULL, "|", &tempSaved);
  sbs = strtok_r(NULL, "|", &tempSaved);
  pfs = strtok_r(NULL, "|", &tempSaved);

  if(sites == NULL || hbs == NULL || sbs == NULL || pfs == NULL)
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "Illegal parameter:%s; expect sites|hardbins|softbins|passfails", parm_list_input);
    *state_out = CI_CALL_ERROR;
    return;
  }

  while(1)
  {
    site = strtok_r(sites, " ", &sitesSaved);
    hb = strtok_r(hbs, " ", &hbsSaved);
    sb = strtok_r(sbs, " ", &sbsSaved);
    pf = strtok_r(pfs, " ", &pfsSaved);

    if(site != NULL && hb != NULL && sb != NULL && pf != NULL)
    {
      phTcomSetBinOfSite(f->testerId, atol(site), sb, atol(hb));
      long passed = strchr(pf, 'P')!=NULL?1:0;
      phTcomSetSystemFlagOfSite(f->testerId, PHTCOM_SF_CI_GOOD_PART, atol(site), passed);
      phTcomSetSystemFlagOfSite(f->testerId, PHTCOM_SF_CI_BAD_PART, atol(site), !passed);
    }
    else
    {
      break;
    }

    sites = NULL;
    hbs = NULL;
    sbs = NULL;
    pfs = NULL;
  }

  if(site != NULL || hb != NULL || sb != NULL || pf != NULL)
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "The sites/hardbins/softbins/passfails don't match in the parameter:%s", parm_list_input);
    *state_out = CI_CALL_ERROR;
    return;
  }

  *state_out = CI_CALL_PASS;
  return;

}

void ph_set_equipment_test_results(
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
}

void ph_get_error_message(
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
  PrepareCallResults();
  strncpy(comment_out, phLogGetSavedErrorMsg(), CI_MAX_COMMENT_LEN-1);
  comment_out[CI_MAX_COMMENT_LEN-1] = '\0';
  *comlen = strlen(comment_out);
  phLogClearSavedErrorMsg();
  *state_out = CI_CALL_PASS;
  return;
}

void ph_interrupt(
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
  PrepareCallResults();
  phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT,1);
  *state_out = CI_CALL_PASS;
  return;
}

void ph_get_stepping_pattern_file(
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
  char filePath[CI_MAX_COMMENT_LEN-1] = "";

  PrepareCallResults();

  phTcomGetWaferDescriptionFile(filePath);
  sendResult(f->logId, comment_out, comlen, filePath);
  *state_out = CI_CALL_PASS;

  return;
}

void ph_set_stepping_pattern_file(
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
  PrepareCallResults();

  phTcomSetWaferDescriptionFile(parm_list_input);

  *state_out = CI_CALL_PASS;

  return;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
