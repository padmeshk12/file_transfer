/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : driver.c
 * CREATED  : 09 Feb 2001
 *
 * CONTENTS : Driver level actions
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 09 Feb 2001, Michael Vogt, created
 *                         Chris Joyce, taken from ph_frame.c
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

#include "ph_timer.h"
#include "ph_frame_private.h"

#include "driver.h"
#include "reconfigure.h"
#include "exception.h"
#include "user_dialog.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */


/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static int confYesNo(struct phFrameStruct *frame, const char *key)
{
    phConfError_t confError;
    const char *confString = "";
    phConfType_t confType = PHCONF_TYPE_NUM;
    int size;
    int retVal = -1;

    confError = phConfConfIfDef(frame->specificConfId, key);
    if (confError == PHCONF_ERR_UNDEF)
	return -1;

    confError = phConfConfType(frame->specificConfId, key, 0, NULL, 
	&confType, &size);
    if (confType != PHCONF_TYPE_STR)
	retVal = -2;
    else
    {
	phConfConfString(frame->specificConfId, key, 0, NULL, &confString);
	if (strcasecmp(confString, "yes") == 0)
	    retVal = 1;
	else if (strcasecmp(confString, "no") == 0)
	    retVal = 0;
	else
	    retVal = -2;
    }

    if (retVal < 0)
	phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
	    "configuration of \"%s\" not understood\n"
	    "should be either \"yes\" or \"no\"", key);

    return retVal;
}


/*****************************************************************************
 *
 * Start up the Driver
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary driver start actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameDriverStart(
    struct phFrameStruct *f             /* the framework data */
)
{
    phConfError_t confError;
    phUserDialogError_t userDialogError;
    const char *key;
    char *oneDef;
    long response;
    phEventResult_t result;
    phTcomUserReturn_t stReturn;

    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    int errors = 0;
    int warnings = 0;

    int success = 0;
    int completed = 0;
    int comTestPassed = 0;

    phFuncError_t funcError;

    /* initialize the binning management */
    reconfigureBinManagement(f, &errors, &warnings);
    if (errors)
    {
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, 1);
        return PHTCOM_CI_CALL_ERROR;
    }

    reconfigureFramework(f, &errors, &warnings);
    if (errors)
    {
	phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, 1);
	return PHTCOM_CI_CALL_ERROR;
    }

    /* check whether the configuration should be logged */
    if (confYesNo(f, PHKEY_MO_LOGCONF) == 1)
    {
	/* ok, log the configuration as messages to the general
	   log output */
	    
	phLogFrameMessage(f->logId, LOG_NORMAL, 
	    "Logging the current configuration....");
	phLogFrameMessage(f->logId, LOG_NORMAL, "");

	confError = phConfConfFirstKey(f->globalConfId, &key);
	while (confError == PHCONF_ERR_OK)
	{
	    confError = phConfGetConfDefinition(
		f->globalConfId, key, &oneDef);
	    phLogFrameMessage(f->logId, LOG_NORMAL, "%s", oneDef);
	    confError = phConfConfNextKey(f->globalConfId, &key);
	}

	confError = phConfConfFirstKey(f->specificConfId, &key);
	while (confError == PHCONF_ERR_OK)
	{
	    confError = phConfGetConfDefinition(
		f->specificConfId, key, &oneDef);
	    phLogFrameMessage(f->logId, LOG_NORMAL, "%s", oneDef);
	    confError = phConfConfNextKey(f->specificConfId, &key);
	}

	phLogFrameMessage(f->logId, LOG_NORMAL, "");
	phLogFrameMessage(f->logId, LOG_NORMAL, 
	    "End of configuration log");

	/* check whether operator should confirm the configuration */
	if (confYesNo(f, PHKEY_OP_ASKCONF) == 1)
	{
	    /* ok, now ask the operator */
	    do
	    {
		response = 8;
		phEventPopup(f->eventId, &result, &stReturn, 1, "menu",
		    "Please confirm the logged configuration setting.\n"
		    "You may...\n"
		    "CONTINUE, if the handler driver configuration is correct,\n"
		    "ABORT the testprogram, or\n"
		    "QUIT the testprogram",
		    "Abort", "", "", "", "", "", &response, NULL);

		if (result == PHEVENT_RES_ABORT)
		{
		    return stReturn;
		}
	    } while (response != 1 && response != 2 && response != 8);

	    /* now decide, whether to abort, quit or go on.... */
	    switch (response)
	    {
	      case 1:
		/* quit pressed, set quit flag */
		phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_QUIT, 1);
		return retVal;
	      case 2:
		/* abort pressed or undefined, set abort flag */
		phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, 1);
		return retVal;
	      case 8:
		/* continue pressed, nothing to do */
		break;
	    }
	}
    }

    /* display User Dialog before performing Comm Test if necessary */
    userDialogError = displayUserDialog(f->dialog_comm_start);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"error occured while trying to display [ Comm Start ] user dialog,\n"
		"giving up");
        return PHTCOM_CI_CALL_ERROR;
    }

    /* finalize the plugin initialization. This is the new concept. So
       far only the plugin handle was initialized but no action has
       been done. Now we try the first communication step (if this
       function is implemented for the plugin) and call the plugin
       reconfiguration function. This is where the plugin becomes
       active communicating with the handler */

    /* ping the handler ... */
    if ((f->funcAvail & PHFUNC_AV_COMMTEST))
    {
	success=0;
	completed=0;

	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "going to test communication link to handler...");

	while (!success && !completed)
	{
	    funcError = phPlugFuncCommTest(f->funcId, &comTestPassed);
	    /* we do not care whether it is the expected handler, we
	       only care about whether we can communicate at all or
	       not, so the comTestPassed is ignored but the return
	       value is checked */

	    /* analyze result from die list call and take action */
	    phFrameHandlePluginResult(f, funcError, PHFUNC_AV_COMMTEST,
		&completed, &success, &retVal);
	}

	if (!success || !completed)
	{
	    /* big problem, we can not communicate */
	    phLogFrameMessage(f->logId, PHLOG_TYPE_FATAL,
		"the handler driver is finally not able to communicate with the handler,\n"
		"giving up");
	    return PHTCOM_CI_CALL_ERROR;
	}
	else if ( retVal==PHTCOM_CI_CALL_PASS )
        {
	    phLogFrameMessage(f->logId, LOG_DEBUG,
	     "communication test has passed");
        }
    }
    else
	 phLogFrameMessage(f->logId, LOG_DEBUG,
	     "communication test not performed, not implemented for this plugin");

    /* display User Dialog before performing (Re) Configure if necessary */
    userDialogError = displayUserDialog(f->dialog_config_start);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"error occured while trying to display [ Configure ] user dialog,\n"
		"giving up");
        return PHTCOM_CI_CALL_ERROR;
    }

    /* (re)conigure the handler plugin and the handler */
    if ((f->funcAvail & PHFUNC_AV_RECONFIGURE) && retVal==PHTCOM_CI_CALL_PASS )
    {
	success=0;
	completed=0;

	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "going to configure the handler driver plugin...");

	while (!success && !completed)
	{
	    funcError = phPlugFuncReconfigure(f->funcId);

	    /* analyze result from reconfigure and take action */
	    phFrameHandlePluginResult(f, funcError, PHFUNC_AV_RECONFIGURE,
		&completed, &success, &retVal);
	}

	if (!success || !completed )
	{
	    /* big problem, we can not communicate */
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"the handler driver is finally not able to configure the driver plugin,\n"
		"giving up");
	    return PHTCOM_CI_CALL_ERROR;
	}
	else
	    phLogFrameMessage(f->logId, LOG_DEBUG,
	     "initial configuration has succeeded");
    }
    else
	 phLogFrameMessage(f->logId, LOG_DEBUG,
	     "initial configuration not performed, not implemented for this plugin");

    return retVal;
}


/*****************************************************************************
 *
 * Stop up the Driver
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary driver stop actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameDriverDone(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    /* nothing special to do ....... */

    return retVal;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
