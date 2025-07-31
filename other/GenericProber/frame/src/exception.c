/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : exception.c
 * CREATED  : 15 Dec 1999
 *
 * CONTENTS : waiting and timeout management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *            Danglin Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Dec 1999, Michael Vogt, created
 *            Dec 2005, Jiawei Lin, CR28409
 *              allow the user to enable/disable diagnose window by specifying
 *              the driver parameter "enable_diagnose_window"
 *            Dec 2008, Danglin Li, CR41221 & CR42599
 *              add exectute gpib cmd and query Plugin Function name
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

#include "ph_tools.h"
#include "ph_keys.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_mhcom.h"
#include "ph_pfunc.h"
#include "ph_phook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"
#include "ph_GuiServer.h"

#include "ph_frame_private.h"
#include "ph_timer.h"
#include "helpers.h"
#include "exception.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

#include "dev/DriverDevKits.hpp"
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static char *getPluginFunctionName(phPFuncAvailability_t p_call)
{
    static char name[64];

    strcpy(name, "(unknown)");

    if (p_call == PHPFUNC_AV_INIT)
	strcpy(name, "init-plugin");
    if (p_call == PHPFUNC_AV_RECONFIGURE)
	strcpy(name, "reconfigure-plugin");
    if (p_call == PHPFUNC_AV_RESET)
	strcpy(name, "reset-plugin");
    if (p_call == PHPFUNC_AV_DRIVERID)
	strcpy(name, "get-driver-id");
    if (p_call == PHPFUNC_AV_EQUIPID)
	strcpy(name, "get-equipment-id");
    if (p_call == PHPFUNC_AV_LDCASS)
	strcpy(name, "load-cassette");
    if (p_call == PHPFUNC_AV_UNLCASS)
	strcpy(name, "unload-cassette");
    if (p_call == PHPFUNC_AV_LDWAFER)
	strcpy(name, "load-wafer");
    if (p_call == PHPFUNC_AV_UNLWAFER)
	strcpy(name, "unload-wafer");
    if (p_call == PHPFUNC_AV_GETDIE)
	strcpy(name, "get-die");
    if (p_call == PHPFUNC_AV_BINDIE)
	strcpy(name, "bin-die");
    if (p_call == PHPFUNC_AV_GETSUBDIE)
	strcpy(name, "get-subdie");
    if (p_call == PHPFUNC_AV_BINSUBDIE)
	strcpy(name, "bin-subdie");
    if (p_call == PHPFUNC_AV_DIELIST)
	strcpy(name, "set-die-list");
    if (p_call == PHPFUNC_AV_SUBDIELIST)
	strcpy(name, "set-subdie-list");
    if (p_call == PHPFUNC_AV_REPROBE)
	strcpy(name, "reprobe");
    if (p_call == PHPFUNC_AV_CLEAN)
	strcpy(name, "clean-probe-needles");
    if (p_call == PHPFUNC_AV_PMI)
	strcpy(name, "probe-mark-inspection");
    if (p_call == PHPFUNC_AV_PAUSE)
	strcpy(name, "pause");
    if (p_call == PHPFUNC_AV_UNPAUSE)
	strcpy(name, "unpause");
    if (p_call == PHPFUNC_AV_TEST)
	strcpy(name, "send-command");
    if (p_call == PHPFUNC_AV_DIAG)
	strcpy(name, "diagnostic");
    if (p_call == PHPFUNC_AV_STATUS)
	strcpy(name, "get-status");
    if (p_call == PHPFUNC_AV_UPDATE)
	strcpy(name, "update-plugin");
    if (p_call == PHPFUNC_AV_CASSID)
	strcpy(name, "get-cassette-id");
    if (p_call == PHPFUNC_AV_WAFID)
	strcpy(name, "get-wafer-id");
    if (p_call == PHPFUNC_AV_PROBEID)
	strcpy(name, "get-prober-id");
    if (p_call == PHPFUNC_AV_LOTID)
	strcpy(name, "get-lot-id");
    if (p_call == PHPFUNC_AV_STARTLOT)
	strcpy(name, "start-lot");
    if (p_call == PHPFUNC_AV_ENDLOT)
	strcpy(name, "end-lot");
    if (p_call == PHPFUNC_AV_DESTROY)
	strcpy(name, "destroy-plugin");
    if (p_call == PHPFUNC_AV_EXECGPIBCMD)
        strcpy(name, "exec-gpib-cmd");
    if (p_call == PHPFUNC_AV_EXECGPIBQUERY)
        strcpy(name, "exec-gpib-query");

    return name;
}


/*****************************************************************************
 *
 * wait for something
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * please refer to exception.h
 *
 ***************************************************************************/

static void phFrameWaiting(
    struct phFrameStruct *f             /* the framework data */, 
    phPFuncAvailability_t p_call        /* current call to the plugin */,
    phEventResult_t *result             /* the result of this call */,
    phTcomUserReturn_t *stReturn        /* the proposed SmarTest
                                           return value in case of
                                           result = PHEVENT_RES_ABORT */
)
{
    long abortFlag = 0;
    long resetFlag = 0;
    long quitFlag = 0;
    long pauseFlag = 0;

    phEventResult_t tmpResult;
    phTcomUserReturn_t tmpStReturn;

    int confFound;
    int doTimeout;

    phEventDataUnion_t eventData;
    phEventOpAction_t opAction;

    double maxTime;
    double totalElapsedTime;
    double shortElapsedTime;

    const char *timeConf;
    const char *actionConf;

    char response[128];

    /* first log this call */
    phLogFrameMessage(f->logId, LOG_DEBUG,
	"waiting for operation to complete, heartbeat timeout");
    
    /* define a default result */
    *result = PHEVENT_RES_VOID;

    /* get the current flag situation. The operator may have set some
       flag */
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_RESET, &resetFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_PAUSE, &pauseFlag);
    
    /* return, if the operator wants to stop */
    if (abortFlag || resetFlag || (quitFlag && !f->initialQuitFlag))
    {
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "aborting current operation due to set system flags");

	*result = PHEVENT_RES_ABORT;
	*stReturn = PHTCOM_CI_CALL_PASS;
	return;
    }

    /* take the time. */
    totalElapsedTime = phFrameStopTimer(f->totalTimer);
    shortElapsedTime = phFrameStopTimer(f->shortTimer);

    /* call the waiting event */
    if (f->eventId)
    {
	phEventWaiting(f->eventId, (long) totalElapsedTime, 
	    &tmpResult, &tmpStReturn);
	if (tmpResult == PHEVENT_RES_ABORT)
	{
	    *result = tmpResult;
	    *stReturn = tmpStReturn;
	    return;
	}
    }

    /* determine the timeout configuration and the operator action */
    timeConf = NULL;
    actionConf = NULL;
    switch (p_call)
    {
      case PHPFUNC_AV_INIT:
      case PHPFUNC_AV_STARTLOT:
      case PHPFUNC_AV_ENDLOT:
	timeConf = PHKEY_OP_LOTTOTIME;
	actionConf = PHKEY_OP_LOTTOACT;
	break;
      case PHPFUNC_AV_LDCASS:
      case PHPFUNC_AV_UNLCASS:
	timeConf = PHKEY_OP_CASTOTIME;
	actionConf = PHKEY_OP_CASTOACT;
	break;
      case PHPFUNC_AV_LDWAFER:
      case PHPFUNC_AV_UNLWAFER:
	timeConf = PHKEY_OP_WAFTOTIME;
	actionConf = PHKEY_OP_WAFTOACT;
	break;
      case PHPFUNC_AV_GETDIE:
      case PHPFUNC_AV_BINDIE:
      case PHPFUNC_AV_GETSUBDIE:
      case PHPFUNC_AV_BINSUBDIE:
      case PHPFUNC_AV_REPROBE:
	timeConf = PHKEY_OP_DIETOTIME;
	actionConf = PHKEY_OP_DIETOACT;
	break;
      case PHPFUNC_AV_CLEAN:
      default:
	timeConf = PHKEY_OP_GENTOTIME;
	actionConf = PHKEY_OP_GENTOACT;
	break;
    }

    /* If a timeout value is not defined, assume 2 minutes */
    maxTime = 120;
    if (timeConf && phConfConfIfDef(f->specificConfId, timeConf) == PHCONF_ERR_OK)
    {
	if(phConfConfNumber(f->specificConfId, timeConf, 0, NULL, &maxTime) != PHCONF_ERR_OK)
        {
            maxTime = 120;
        }
    }
    
    /* check timeout conditions */
    doTimeout = (pauseFlag && !f->initialPauseFlag) || 
	(shortElapsedTime > maxTime);

    /* perform timeout */
    if (doTimeout)
    {
	/* call the timeout event handler. Prepare event data first */
	confFound = 0;
	if (actionConf)
	    phConfConfStrTest(&confFound, f->specificConfId, actionConf, 
		"continue", "operator-help", "skip", NULL);

        if(confFound == 1 )
        {
            opAction = PHEVENT_OPACTION_CONTINUE;
        }
        else if(confFound == 3)
        {
            opAction = PHEVENT_OPACTION_SKIP;
        }
        else
        {
            opAction = PHEVENT_OPACTION_ASK;
        }

	eventData.to.am_call = f->currentAMCall;
	eventData.to.p_call = p_call;
	eventData.to.elapsedTimeSec = (long) totalElapsedTime;
	eventData.to.opAction = opAction;

	if (f->eventId)
	{
	    phEventError(f->eventId, PHEVENT_CAUSE_WP_TIMEOUT, 
                 &eventData, result, stReturn, f->enableDiagnoseWindow);
	}
	else
	{
	    /* the only case this could happen is during driver
               initialization, while waiting for the plugin init to
               complete */
	    if (p_call == PHPFUNC_AV_INIT)
	    {
		if (opAction == PHEVENT_OPACTION_ASK || pauseFlag)
		{
		    response[0] = '\0';
		    phGuiShowOptionDialog(f->logId, PH_GUI_YES_NO,
			"Waiting for prober's lot start signal timed out.\n"
			"\n"
			"Press YES only, if the prober is completely\n"
			"initialized and ready to probe the (next) lot.\n"
			"(Waiting for the lot start signal will be skipped)\n"
			"\n"
			"Press NO, if the prober is not yet ready to\n"
			"probe a lot and will send a lot start signal soon.\n"
			"(Waiting for the lot start signal will continue)\n"
			"\n"
			"Has the prober already send the log start signal?\n",
			response);
		    if (strcasecmp(response, "yes") == 0)
		    {
			/* lot is started */
			phEstateASetLotInfo(f->estateId, 1);
			/* don't set HANDLED, since the init function of
			   the plugin must be called again ! */
			*result = PHEVENT_RES_CONTINUE;
		    }
		}
		else
		{
		    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
			"prober not yet ready to probe a lot, initialization is ongoing.\n"
			"set pause flag to bring up a test start dialog, or\n"
			"set abort flag to abort the testprogram");
		}
	    }
	}

	/* recheck the abort condition */
	phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
	if (abortFlag)
	{
	    /* overwrite result from event handler, abort was set in
               the meantime, return immediately */
	    *result = PHEVENT_RES_ABORT;
	    *stReturn = PHTCOM_CI_CALL_PASS;
	    return;
	}

	/* If we came here because the pause flag was set while 
	   waiting for parts and wasn't set as we started waiting for 
	   parts, then reset the pause flag, to avoid calling the 
	   pause actions in the test cell client */
	
	if (pauseFlag && !f->initialPauseFlag)
	    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_PAUSE, 0L);

	/* restart short timer for next try... */
	phFrameStartTimer(f->shortTimer);
    }
}

/*****************************************************************************
 *
 * handle a gpib communication problem
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * please refer to exception.h
 *
 ***************************************************************************/

static void phFrameGpibProblem(
    struct phFrameStruct *f             /* the framework data */, 
    phPFuncAvailability_t p_call        /* current call to the plugin */,
    phEventResult_t *result             /* the result of this call */,
    phTcomUserReturn_t *stReturn        /* the proposed SmarTest
                                           return value in case of
                                           result = PHEVENT_RES_ABORT */
)
{
    long abortFlag = 0;
    long resetFlag = 0;
    long quitFlag = 0;
    long pauseFlag = 0;

    phEventDataUnion_t eventData;

    eventData.intf.am_call = f->currentAMCall;
    eventData.intf.p_call = p_call;

    /* define a default result */
    *result = PHEVENT_RES_VOID;

    /* get the current flag situation. The operator may have set some
       flag */
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_RESET, &resetFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_PAUSE, &pauseFlag);
    
    /* return, if the operator wants to stop */
    if (abortFlag || resetFlag || (quitFlag && !f->initialQuitFlag))
    {
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "aborting current operation due to set system flags");

	*result = PHEVENT_RES_ABORT;
	*stReturn = PHTCOM_CI_CALL_PASS;
	return;
    }

    phEventError(f->eventId, PHEVENT_CAUSE_IF_PROBLEM,
                 &eventData, result, stReturn, f->enableDiagnoseWindow);

    /* recheck the abort condition */
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if (abortFlag)
    {
	/* overwrite result from event handler, abort was set in
	   the meantime, return immediately */
	*result = PHEVENT_RES_ABORT;
	*stReturn = PHTCOM_CI_CALL_PASS;
    }
}


/*****************************************************************************
 *
 * handle error answer problem
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * please refer to exception.h
 *
 ***************************************************************************/

static void phFrameErrorAnswer(
    struct phFrameStruct *f             /* the framework data */, 
    phPFuncAvailability_t p_call        /* current call to the plugin */,
    phEventResult_t *result             /* the result of this call */,
    phTcomUserReturn_t *stReturn        /* the proposed SmarTest
                                           return value in case of
                                           result = PHEVENT_RES_ABORT */
)
{
    long abortFlag = 0;
    long resetFlag = 0;
    long quitFlag = 0;
    long pauseFlag = 0;

    phEventDataUnion_t eventData;

    eventData.intf.am_call = f->currentAMCall;
    eventData.intf.p_call = p_call;

    /* define a default result */
    *result = PHEVENT_RES_VOID;

    /* get the current flag situation. The operator may have set some
       flag */
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_RESET, &resetFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_PAUSE, &pauseFlag);
    
    /* return, if the operator wants to stop */
    if (abortFlag || resetFlag || (quitFlag && !f->initialQuitFlag))
    {
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "aborting current operation due to set system flags");

	*result = PHEVENT_RES_ABORT;
	*stReturn = PHTCOM_CI_CALL_PASS;
	return;
    }

    phEventError(f->eventId, PHEVENT_CAUSE_ANSWER_PROBLEM,
                 &eventData, result, stReturn, f->enableDiagnoseWindow);

    /* recheck the abort condition */
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if (abortFlag)
    {
	/* overwrite result from event handler, abort was set in
	   the meantime, return immediately */
	*result = PHEVENT_RES_ABORT;
	*stReturn = PHTCOM_CI_CALL_PASS;
    }
}


/*****************************************************************************
 *
 * handle plugin return value
 *
 * Authors: Michael Vogt / Chris Joyce
 *
 * Description:
 *
 *   typedef enum {
 *       PHFRAMESTEP_ERR_OK, converted to      PHFRAME_EXCEPTION_ERR_OK
 *       PHFRAMESTEP_ERR_PC,                   PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE
 *       PHFRAMESTEP_ERR_ERROR,                PHFRAME_EXCEPTION_ERR_ERROR
 *       PHFRAMESTEP_ERR_PAUSED                PHFRAME_EXCEPTION_ERR_PAUSED
 *   } stepActionError_t;
 *
 * please refer to exception.h
 *
 ***************************************************************************/

void phFrameHandlePluginResult(
    struct phFrameStruct *f             /* the framework data */,
    phPFuncError_t funcError            /* the plugin result */,
    phPFuncAvailability_t p_call        /* current call to the plugin */,

    int *completed                      /* plugin call is completed or
                                           aborted */,
    int *success                        /* plugin call or exception
                                           handling was successful */,
    phTcomUserReturn_t *retVal          /* possibly changed return
                                           value, in case of completed
                                           call */,
    exceptionActionError_t *action           /* action to be performed
                                           as result of plugin call */
)
{
    driver_dev_kits::AlarmController alarmController;

    phEventResult_t exceptionResult = PHEVENT_RES_VOID;
    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;

    int tmpCompleted = 0;
    int tmpSuccess = 0;
    int tmpRetVal = retVal ? *retVal : PHTCOM_CI_CALL_PASS;
    int tmpAction = action ? *action : PHFRAME_EXCEPTION_ERR_OK;

    /* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
    /* Issue Number: 315 */
    /*    int paused;*/
    /*    long pauseFlag;*/
    /* End of Huatek Modification */

    switch (funcError)
    {
        case PHPFUNC_ERR_OK:
            /* call has been completed, everything went fine */
            tmpCompleted = 1; 
            tmpSuccess = 1;
            break;
        case PHPFUNC_ERR_NOT_INIT:
        case PHPFUNC_ERR_INVALID_HDL:
        case PHPFUNC_ERR_NA:
        case PHPFUNC_ERR_MEMORY:
            /* fatal cases */
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "potential internal error of the prober driver framework.\n"
                    "Prober driver plugin call could not be completed.\n"
                    "Error code %d received during %s\n"
                    "giving up", (int) funcError, getPluginFunctionName(p_call));
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;
            break;
        case PHPFUNC_ERR_MODEL:
            alarmController.raiseMaterialHandlingAlarm("Illegal prober/handler model.");
            /* fatal cases */
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "Prober model is not supported.\n"
                    "Error received during %s\n"
                    "giving up", getPluginFunctionName(p_call));
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;
            break;
        case PHPFUNC_ERR_CONFIG:
            /* plugin configuration error */
            tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
            tmpCompleted = 1;
            break;
        case PHPFUNC_ERR_BINNING:
            alarmController.raiseMaterialHandlingAlarm("Error when binning the tested devices.");
            /* print panic on incorrect binning and allow to go on... */
            if (phFramePanic(f, "SEVERE BINNING ERROR WHICH MIGHT AFFECT TEST QUALITY"))
                tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
            tmpCompleted = 1;
            break;
        case PHPFUNC_ERR_TIMEOUT:
        case PHPFUNC_ERR_WAITING:
            /* plugin call is ongoing, doing a flag check loop */
            phFrameWaiting(f, p_call, &exceptionResult, &exceptionReturn);
            switch (exceptionResult)
            {
                case PHEVENT_RES_VOID:
                case PHEVENT_RES_CONTINUE:
                    /* go on waiting */
                    break;
                case PHEVENT_RES_ABORT:
                    /* aborted, take over proposed return value and fall
                       through to handled case */
                    tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
                    tmpRetVal = exceptionReturn;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_HANDLED:
                    tmpCompleted = 1;
                    tmpSuccess = 1;
                    break;
            }
            break;
        case PHPFUNC_ERR_GPIB:
            /* error condition on GPIB, call the event handler */
            phFrameGpibProblem(f, p_call, &exceptionResult, &exceptionReturn);
            switch (exceptionResult)
            {
                case PHEVENT_RES_VOID:
                    /* this is a fatal situation, the GPIB error was not
                       handled */
                    tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
                    tmpRetVal = PHTCOM_CI_CALL_ERROR;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_ABORT:
                    /* aborted, take over proposed return value and fall
                       through to handled case */
                    tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
                    tmpRetVal = exceptionReturn;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_CONTINUE:
                case PHEVENT_RES_HANDLED:
                    /* the GPIB problem was solved, but we still have to go on
                       to complete the plugin call */
                    break;
            }
            break;
        case PHPFUNC_ERR_PAT_DONE:
            tmpCompleted = 1;
            tmpAction = PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE;
            break;
        case PHPFUNC_ERR_PAT_REPEAT:
            switch (p_call)
            {
                case PHPFUNC_AV_GETDIE:
                case PHPFUNC_AV_BINDIE:
                    /* the current wafer should be retested */
                    phLogFrameMessage(f->logId, LOG_NORMAL,
                            "scheduling current wafer for retest");
                    phStateSetWaferMode(f->stateId, PHSTATE_WR_RETEST);
                    break;
                default:
                    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                            "'pattern repeat' action is not supported for %s, treated as 'pattern done'", 
                            getPluginFunctionName(p_call));
                    break;
            }
            tmpCompleted = 1;
            tmpAction = PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE;
            break;
        case PHPFUNC_ERR_ABORTED:
            alarmController.raiseMaterialHandlingAlarm("PH driver is aborted.");
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "%s was aborted", getPluginFunctionName(p_call));
            tmpCompleted = 1;
            tmpAction = PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE;
            /* tmpRetVal = PHTCOM_CI_CALL_ERROR; */
            phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, 1L);
            break;
        case PHPFUNC_ERR_EMPTY:
            tmpCompleted = 1;
            tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
            break;
        case PHPFUNC_ERR_OCCUPIED:
            tmpCompleted = 1;
            tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
            break;
        case PHPFUNC_ERR_PARAM:
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "%s plugin called with parameter outside range",
                    getPluginFunctionName(p_call));
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;
            break;
        case PHPFUNC_ERR_ANSWER:
            alarmController.raiseMaterialHandlingAlarm("Unexpected answer received from prober/handler.");
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "%s plugin call received unexpected answer from prober",
                    getPluginFunctionName(p_call));
            phFrameErrorAnswer(f, p_call, &exceptionResult, &exceptionReturn);
            switch (exceptionResult)
            {
                case PHEVENT_RES_VOID:
                    /* this is a fatal situation, the GPIB error was not
                       handled */
                    tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
                    tmpRetVal = PHTCOM_CI_CALL_ERROR;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_ABORT:
                    /* aborted, take over proposed return value and fall
                       through to handled case */
                    tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
                    tmpRetVal = PHTCOM_CI_CALL_ERROR;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_CONTINUE:
                case PHEVENT_RES_HANDLED:
                    /* the answer problem was solved, but we still have to go on
                       to complete the plugin call */
                    break;
            }
            /*   tmpRetVal = PHTCOM_CI_CALL_ERROR;  */
            /*   tmpCompleted = 1;                  */
            break;
        case PHPFUNC_ERR_FATAL:
            alarmController.raiseMaterialHandlingAlarm("The prober/handler is in a fatal state.");
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "prober driver plugin call %s ended in a fatal situation\n"
                    "testprogram will be aborted ", getPluginFunctionName(p_call));
            phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, 1L);
            tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;
            break;
        default:
            /* unexpected return value, print message and fail call */
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "potential internal error of the prober driver plugin.\n"
                    "Error code %d received during %s\n"
                    "giving up", (int) funcError, getPluginFunctionName(p_call));
            tmpAction = PHFRAME_EXCEPTION_ERR_ERROR;
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;
            break;
    }

    if (completed)
        *completed = tmpCompleted;
    if (success)
        *success = tmpSuccess;
    if (action)
        *action = (exceptionActionError_t)tmpAction;
    if (retVal)
        *retVal = (phTcomUserReturn_t)tmpRetVal;

    /* if the diagnose window for user is disable, always return OK, CR28409 */
    if( f->enableDiagnoseWindow == NO ) {   
        /* specific to Infenion TUI, we hide the diagnose window */
        /**completed = 1;
         *success = 1;*/
        if (retVal)
            *retVal = PHTCOM_CI_CALL_PASS;
    }
}



/*****************************************************************************
 * End of file
 *****************************************************************************/
