/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_frame.c
 * CREATED  : 13 Dec 1999
 *
 * CONTENTS : test cell client interface to driver
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR27092 & CR25172
 *            Garry Xie,R&D Shanghai, CR28427
 *            Fabarca & Kun xiao, CR21589
 *            Dangln Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 13 Dec 1999, Michael Vogt, created
 *            Aug 2005, CR27092 & CR 25172
 *              Implement the "ph_get_status" and "ph_set_status" function
 *              call. The former could be used to request parameter/information
 *              from Prober. Thus more datalog are possible, like WCR(Wafer 
 *              Configuration Record),Wafer_ID, Wafer_Number, Cassette Status 
 *              and Chuck Temperature. The latter could be used to set operating
 *              parameter for Prober, now it is only an interface without real
 *              functionality.
 *            February 2006, Garry Xie , CR28427
 *              Modify the "ph_set_status" function call.which is used to set parameters
 *              to the Prober,like Alarm_Buzzer.
 *            July 2006, Fabarca & Kun xiao, CR21589
 *              Implement the "ph_contact_test" function call. This function performs contact 
 *              test to get z contact height. 
 *            Nov 2008, CR41221 & CR42599
 *              Implement the "ph_exec_gpib_cmd" and "ph_exec_gpib_query" functions call.
 *              The function "ph_exec_gpib_cmd" could be used to send prober setup and
 *              action commands to setup prober initial parameters and control prober.
 *              The function "ph_exec_gpib_query" could be used to inquiry prober status
 *              and parameters.
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
#include <sstream>
#include <iostream>
/*--- module includes -------------------------------------------------------*/

#include "ci_types.h"

#include "ph_tools.h"
#include "ph_keys.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_mhcom.h"
#include "ph_phook.h"
#include "ph_tcom.h"
#include "ph_timer.h"
#include "ph_event.h"
#include "ph_pfunc.h"
#include "ph_frame.h"

#include "ph_frame_private.h"

#include "reconfigure.h"
#include "helpers.h"

#include "driver.h"
#include "idrequest.h"
#include "equipment_cmd.h"
#include "configuration.h"
#include "lot.h"
#include "cassette.h"
#include "wafer.h"
#include "die.h"
#include "ph_contacttest_private.h"

#include "pause.h"
#include "clean.h"
/* Begin of Huatek Modification, Donnie Tu, 12/10/2001 */
/* Issue Number: 285 */
#include "clean.h"
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/* End of Huatek Modification */

/*--- defines ---------------------------------------------------------------*/

/* we don't have the following levels for test cell client calls: */

#define IMPLEMENT_SUBDIE_LEVEL
/* #undef IMPLEMENT_SUBDIE_LEVEL */

#define IMPLEMENT_HANDTEST_MODE
/* #undef IMPLEMENT_HANDTEST_MODE */

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

/*--- typedefs --------------------------------------------------------------*/

/*--- the module global framework descriptor --------------------------------*/

static struct phFrameStruct *f = NULL;

/*--- functions -------------------------------------------------------------*/

static int tcomResultToCiResult(phTcomUserReturn_t stReturn)
{
    switch (stReturn)
    {
      case PHTCOM_CI_CALL_PASS:
	return CI_CALL_PASS;
      case PHTCOM_CI_CALL_ERROR:
	return CI_CALL_ERROR;
      case PHTCOM_CI_CALL_BREAKD:
	return CI_CALL_BREAKD;
      case PHTCOM_CI_CALL_TIMEOUT:
    return CI_CALL_TIMEOUT;
      default:
        return CI_CALL_PASS;
    }
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
 * Start a prober driver session (test cell client call)
 *
 * Authors: Michael Vogt
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

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameDriverStart(f));
    if (*state_out != CI_CALL_PASS)
	return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DRV_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DRV_START);
}

/*****************************************************************************
 *
 * Stop a prober driver session (test cell client call)
 *
 * Authors: Michael Vogt
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

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameDriverDone(f));
    if (*state_out != CI_CALL_PASS)
	return;

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
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_lot_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_LOT_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_LOT_START);

    /* do the main work of this driver function */
    *state_out = tcomResultToCiResult(phFrameLotStart(f));
    if (*state_out != CI_CALL_PASS)
	return;

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
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
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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

    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 0);

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_LOT_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_LOT_DONE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameLotDone(f));
    if (*state_out != CI_CALL_PASS)
	return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_LOT_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_LOT_DONE);
}

/*****************************************************************************
 *
 * Start a new cassette of untested wafers
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_cassette_start(
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
	"ph_cassette_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_cassette_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_CASS_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_CASS_START);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameCassetteStart(f));
    if (*state_out != CI_CALL_PASS)
	return;

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_CASS_START);

    sendResult(f->logId, comment_out, comlen, "YES");


    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_CASS_START);
}

/*****************************************************************************
 *
 * End a new cassette of untested wafers
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_cassette_done(
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
	"ph_cassette_done(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_cassette_done called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 0);

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_CASS_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_CASS_DONE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameCassetteDone(f));
    if (*state_out != CI_CALL_PASS)
	return;

     /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_CASS_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_CASS_DONE);
}

/*****************************************************************************
 *
 * Load a new wafer
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_wafer_start(
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
	"ph_wafer_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_wafer_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_WAF_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_WAF_START);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameWaferStart(f));
    if (*state_out != CI_CALL_PASS)
	return;

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_WAF_START);

    sendResult(f->logId, comment_out, comlen, "YES");


    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_WAF_START);
}

/*****************************************************************************
 *
 * Unload the current wafer
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_wafer_done(
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
	"ph_wafer_done(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_wafer_done called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 0);

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_WAF_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_WAF_DONE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameWaferDone(f));
    if (*state_out != CI_CALL_PASS)
	return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_WAF_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_WAF_DONE);
}

/*****************************************************************************
 *
 * Wait for die probing (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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
    long levelEnd = 0;
    /* check the initialization status and log the incoming call */
    DoCheckInit();
    phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, 
	"ph_device_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_device_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_DIE_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_DIE_START);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameDieStart(f));
    if (*state_out != CI_CALL_PASS)
	return;

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DIE_START);

    sendResult(f->logId, comment_out, comlen, "YES");

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DIE_START);
}

/*****************************************************************************
 *
 * Bin dies (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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

    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 0);

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_DIE_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_DIE_DONE);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameDieDone(f));
    if (*state_out != CI_CALL_PASS)
	return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DIE_DONE);

    /* clear the bins */
    phTcomSimClearBins();

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DIE_DONE);
}

#ifdef IMPLEMENT_SUBDIE_LEVEL

/*****************************************************************************
 *
 * Wait for die probing (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_subdie_start(
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
	"ph_subdie_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_subdie_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_SUBDIE_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_SUBDIE_START);

    /* do the main work of this driver function */

    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, 
	"ph_subdie_start not yet implemented, ignored");

    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
    if(levelEnd == 1)
    {
      phLogFrameMessage(f->logId, LOG_NORMAL, "level end detected, send NO");
      sendResult(f->logId, comment_out, comlen, "NO");
      return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_SUBDIE_START);

    sendResult(f->logId, comment_out, comlen, "YES");


    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_SUBDIE_START);
}

/*****************************************************************************
 *
 * Bin subdies (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_subdie_done(
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
	"ph_subdie_done(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_subdie_done called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_SUBDIE_DONE);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_SUBDIE_DONE);

    /* do the main work of this driver function */

    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, 
	"ph_subdie_done not yet implemented, ignored");
    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_SUBDIE_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_SUBDIE_DONE);
}

#endif /* IMPLEMENT_SUBDIE_LEVEL */

/*****************************************************************************
 *
 * Enter pause mode in test cell client (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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
	return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_PAUSE_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_PAUSE_START);
}

/*****************************************************************************
 *
 * Leave puase mode in test cell client (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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
	return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_PAUSE_DONE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_PAUSE_DONE);
}

#ifdef IMPLEMENT_HANDTEST_MODE

/*****************************************************************************
 *
 * Go into hand test mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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

    /* nothing more to do, all important steps are done by the state
       controller */
    /* ... */

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
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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

    /* nothing more to do, all important steps are done by the state
       controller */
    /* ... */

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_HANDTEST_STOP);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_HANDTEST_STOP);
}

#endif /* IMPLEMENT_HANDTEST_MODE */

/*****************************************************************************
 *
 * Go into single step mode (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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

    /* nothing more to do, all important steps are done by the state
       controller */
    /* ... */

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
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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

    /* nothing more to do, all important steps are done by the state
       controller */
    /* ... */

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_STEPMODE_STOP);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_STEPMODE_STOP);
}

/*****************************************************************************
 *
 * Issue a reprobe command to the prober (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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
	return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_REPROBE);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_REPROBE);
}

/*****************************************************************************
 *
 * Perform a probe clean action on the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_clean_probe(
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
	"ph_clean_probe(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_clean_probe called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_CLEAN);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_CLEAN);

    /* do the main work of this driver function */

    *state_out = tcomResultToCiResult(phFrameCleanProbe(f));
    if (*state_out != CI_CALL_PASS)
	return;

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_CLEAN);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_CLEAN);
}

/*****************************************************************************
 *
 * Perform a probe mark inspection
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_pmi(
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
	"ph_pmi(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_pmi called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_PMI);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_PMI);

    /* do the main work of this driver function */

    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, 
	"ph_pmi not yet implemented, ignored");

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_PMI);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_PMI);
}

/*****************************************************************************
 *
 * Move current wafer to the inspection tray
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_inspect_wafer(
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
	"ph_inspect_wafer(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_inspect_wafer called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_INSPECT);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_INSPECT);

    /* do the main work of this driver function */

    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, 
	"ph_inspect_wafer not yet implemented, ignored");

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_INSPECT);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_INSPECT);
}

/*****************************************************************************
 *
 * Leave an test cell client level (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_leave_level(
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
	"ph_leave_level(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_leave_level called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_LEAVE_LEVEL);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_LEAVE_LEVEL);

    /* do the main work of this driver function */

    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, 
	"ph_leave_level not yet implemented, ignored");

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_LEAVE_LEVEL);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_LEAVE_LEVEL);
}

/*****************************************************************************
 *
 * Confirm the prober configuration (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_confirm_configuration(
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
	"ph_confirm_configuration(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_confirm_configuration called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_CONFIRM_CONFIG);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_CONFIRM_CONFIG);

    /* do the main work of this driver function */

    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, 
	"ph_confirm_configuration not yet implemented, ignored");

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_CONFIRM_CONFIG);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_CONFIRM_CONFIG);
}

/*****************************************************************************
 *
 * Reconfigure the driver interactively (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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
	return;

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
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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
	return;

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
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
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
	return;
    sendResult(f->logId, comment_out, comlen, result);

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_GET_CONFIG);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_GET_CONFIG);
}

/*****************************************************************************
 *
 * Start or resume the data logging (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_datalog_start(
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
	"ph_datalog_start(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_datalog_start called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_DATALOG_START);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_DATALOG_START);

    /* do the main work of this driver function */

    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, 
	"ph_datalog_start not yet implemented, ignored");

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DATALOG_START);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DATALOG_START);
}

/*****************************************************************************
 *
 * Stop (suspend) the data logging  (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/
void ph_datalog_stop(
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
	"ph_datalog_stop(\"%s\")",
	parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
	"ph_datalog_stop called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_DATALOG_STOP);
    
    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog */
    EntrySteps(PHFRAME_ACT_DATALOG_STOP);

    /* do the main work of this driver function */

    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, 
	"ph_datalog_stop not yet implemented, ignored");

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_DATALOG_STOP);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_DATALOG_STOP);
}

/*****************************************************************************
 *
 * Get ID information (test cell client call)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_frame.h
 * 
 ***************************************************************************/
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

    *state_out = tcomResultToCiResult(phFrameGetID(f, const_cast<char*>(parm_list_input ? parm_list_input : "(NULL)"), &result));
    if (*state_out != CI_CALL_PASS)
	return;
    sendResult(f->logId, comment_out, comlen, result);

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_GET_ID);

    /* give the event handler (and some hook functions) the chance to
       log the final state */
    ExitSteps(PHFRAME_ACT_GET_ID);
}

/*****************************************************************************
 *
 * Get current prober driver parameters
 *
 * Authors: Michael Vogt
 *          Jiawei Lin
 *
 * Description:
 * Please refer to ph_frame.h
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
  char *result = NULL;

  /* check the initialization status and log the incoming call */
  DoCheckInit();

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "ph_get_status(\"%s\")",
                    parm_list_input ? parm_list_input : "(NULL)");
  phLogFrameMessage(f->logId, LOG_NORMAL, "ph_get_status called");

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_GET_STATUS);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_get_status, afer CheckLevelChange");
    
  /* 
  *  give the event handler (and some hook functions) the chance to
  *  jump in to perform the job or to do a single step dialog 
  */
    EntrySteps(PHFRAME_ACT_GET_STATUS);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_get_status, afer EntrySteps");

  /* do the main work of this driver function */
  *state_out = tcomResultToCiResult(phFrameGetStatus(f, parm_list_input, &result));

  phLogFrameMessage(f->logId, LOG_DEBUG,
                    "ph_get_status, after call to phFrameGetStatus, parm_list_input = ->%s<-, "
                    "result ->%s<-", parm_list_input, result);

  if( *state_out != CI_CALL_PASS ) {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_get_status, phFrameGetStatus did not return "
                      "CI_CALL_PASS, returned value = ->%d<-. Will return CI_CALL_ERROR",
                      *state_out);
    *state_out = CI_CALL_ERROR;
    return;
  }

  sendResult(f->logId, comment_out, comlen, result);

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_GET_STATUS);

  /* 
  * give the event handler (and some hook functions) the chance to
  * log the final state 
  */
    ExitSteps(PHFRAME_ACT_GET_STATUS);
}

/*****************************************************************************
 *
 * Set current prober driver parameters
 *
 * Authors: Jiawei Lin
 *
 * Description:
 * Please refer to ph_frame.h
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
  char *result=NULL;
  /* check the initialization status and log the incoming call */
  DoCheckInit();

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "ph_set_status(\"%s\")",
                    parm_list_input ? parm_list_input : "(NULL)");
  phLogFrameMessage(f->logId, LOG_NORMAL, "ph_set_status called");

  /* prepare result of this call, always return PASS */
  PrepareCallResults();

  /* check whether this call is legal in the current state */
  CheckLevelChange(PHFRAME_ACT_SET_STATUS);
    
  /* 
  *  give the event handler (and some hook functions) the chance to
  *  jump in to perform the job or to do a single step dialog 
  */
  EntrySteps(PHFRAME_ACT_SET_STATUS);
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_set_status, afer EntrySteps");
 /* do the main work of this driver function */
  *state_out = tcomResultToCiResult(phFrameSetStatus(f, parm_list_input, &result));

  phLogFrameMessage(f->logId, LOG_DEBUG,
                    "ph_set_status, after call to phFrameSetStatus, parm_list_input = ->%s<-, "
                    "result ->%s<-", parm_list_input, result);

  if( *state_out != CI_CALL_PASS ) {
    phLogFrameMessage(f->logId, LOG_DEBUG, "ph_set_status, phFrameSetStatus did not return "
                      "CI_CALL_PASS, returned value = ->%d<-. Will return CI_CALL_ERROR",
                      *state_out);
    *state_out = CI_CALL_ERROR;
    return;
  }

  sendResult(f->logId, comment_out, comlen, result);


  /* change the state of the driver */
  phStateChangeLevel(f->stateId, PHFRAME_ACT_SET_STATUS);

  /* 
  * give the event handler (and some hook functions) the chance to
  * log the final state 
  */
  ExitSteps(PHFRAME_ACT_SET_STATUS);
}

/*Begin of Huatek Modifications, Donnie Tu, 04/23/2002*/
/*Issue Number: 334*/
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
        phComGpibClose(f->comId);
	free(f->comId);
	f->comId=NULL;
     }

    phTcomDestroy(f->testerId);
    freeBinMapping(&(f->binMapHdl));
    freeActBinMapping(&(f->binActMapHdl));  // changed Nov 15 2004, kaw - CR15686

    phEventFree(&(f->eventId));
    phEstateFree(&(f->estateId));
    phFrameFreeTimerElement(f->totalTimer);
    phFrameFreeTimerElement(f->shortTimer);
    phFrameFreeListOfTimerElement();
  
    phStateFree(&(f->stateId));
 
   


    phLogFree(&(f->logId));
    freeUserDialog(&(f->dialog_comm_start));
    freeUserDialog(&(f->dialog_config_start));
    freeUserDialog(&(f->dialog_lot_start));
    freeUserDialog(&(f->dialog_wafer_start));



    free(f);
    f=NULL;
}
/*End of Huatek Modifications*/

/*****************************************************************************
 *
 * contact test
 *
 * Authors: fabarca & Kun Xiao
 *         
 * Description:
 * Please refer to ph_frame.h
 *
 ***************************************************************************/

void ph_contact_test(
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
        "ph_contact_test(\"%s\")",
        parm_list_input ? parm_list_input : "(NULL)");
    phLogFrameMessage(f->logId, LOG_NORMAL,
        "ph_contact_test called");

    /* prepare result of this call, always return PASS    */
    PrepareCallResults();

    /* check whether this call is legal in the current state */
    CheckLevelChange(PHFRAME_ACT_CONTACT_TEST);

    /* give the event handler (and some hook functions) the chance to
       jump in to perform the job or to do a single step dialog   */
    EntrySteps(PHFRAME_ACT_CONTACT_TEST);

   /* do the main work of this driver function */
    *state_out = tcomResultToCiResult(phFrameContacttest(f, parm_list_input));
    if (*state_out != CI_CALL_PASS)
    {
        return;
    }

    /* change the state of the driver */
    phStateChangeLevel(f->stateId, PHFRAME_ACT_CONTACT_TEST);

    /* 
     * give the event handler (and some hook functions) the chance to
     * log the final state 
    */
    ExitSteps(PHFRAME_ACT_CONTACT_TEST);
}

/*****************************************************************************
 *
 * Execute prober setup and action command by GPIB
 *
 * Authors: Danglin Li
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
  phLogFrameMessage(f->logId, LOG_DEBUG, "ph_exec_gpib_cmd, afer CheckLevelChange");

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

  if( *state_out != CI_CALL_PASS ) {
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
 * Execute prober query command by GPIB
 *
 * Authors: Danglin Li
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

  if( *state_out != CI_CALL_PASS ) {
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

  if( *state_out != CI_CALL_PASS )
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

    /* site info*/
    static std::stringstream equipmentSiteIDListStream ;/* equipment enabled site ids */
    static std::stringstream siteIDListStream ; /* tester enabled site numbers */
    /* device id info */
    static std::stringstream xCoordListStream ;
    static std::stringstream yCoordListStream ;
    /* final string */
    std::stringstream fullListStream ;
#if 0
    /* bin info*/
    char equipmentBinList[CI_MAX_COMMENT_LEN-1] = ""; /* equipment bin ids for enabled sites */
    char hardBinList[CI_MAX_COMMENT_LEN-1] = ""; /* tester hard bin ids for enabled sites */
    char softBinList[CI_MAX_COMMENT_LEN-1] = ""; /* tester soft bin ids for enabled sites */
#endif

    long dieX = 0, dieY = 0;

    phStateLevel_t cLevelState = (phStateLevel_t)0;

    /* prepare result of this call, always return PASS */
    PrepareCallResults();

    /* get handler site information */
    phStateGetLevel(f->stateId, &cLevelState);
    if(cLevelState == PHSTATE_LEV_DIE_STARTED)
    {
      /* only read site configration when ph_device_start is called*/
      phEstateAGetSiteInfo(f->estateId, &sitePopulation, &entries);
      savedSitePopulation = *sitePopulation;
    }
    else if (cLevelState < PHSTATE_LEV_DIE_STARTED)
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

    equipmentSiteIDListStream.clear();
    equipmentSiteIDListStream.str("");
    siteIDListStream.clear();
    siteIDListStream.str("");
    xCoordListStream.clear();
    xCoordListStream.str("");
    yCoordListStream.clear();
    yCoordListStream.str("");

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
          if(i==f->numSites-1)
          {
            equipmentSiteIDListStream << " " << equipmentSiteName;
            siteIDListStream << " " << f->stToProberSiteMap[i];
            phTcomGetDiePosXYOfSite(f->testerId, f->stToProberSiteMap[i], &dieX, &dieY);
            xCoordListStream << " " << dieX;
            yCoordListStream << " " << dieY;
          }
          else
          {
            equipmentSiteIDListStream << " " << equipmentSiteName << " ";
            siteIDListStream << " " << f->stToProberSiteMap[i] << " ";
            phTcomGetDiePosXYOfSite(f->testerId, f->stToProberSiteMap[i], &dieX, &dieY);
            xCoordListStream << " " << dieX << " ";
            yCoordListStream << " " << dieY << " ";
          }
        }
      }
    }

    /* make the final result */
    fullListStream << "EQPSITE:" << equipmentSiteIDListStream.str() << "|SITE:"
     << siteIDListStream.str() << " |XCOORD:" << xCoordListStream.str() << "|YCOORD:"
     << yCoordListStream.str();
    sendResult(f->logId, comment_out, comlen, fullListStream.str().c_str());

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
