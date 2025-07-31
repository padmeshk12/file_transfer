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
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Dec 1999, Michael Vogt, created
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
#include "ph_GuiServer.h"

#include "ph_frame_private.h"
#include "ph_timer.h"
#include "exception.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

#include "dev/DriverDevKits.hpp"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static char *getPluginFunctionName(phFuncAvailability_t p_call)
{
    static char name[64];

    strcpy(name, "(unknown)");

    if (p_call == PHFUNC_AV_INIT)
	strcpy(name, "init-plugin");
    if (p_call == PHFUNC_AV_RECONFIGURE)
	strcpy(name, "reconfigure-plugin");
    if (p_call == PHFUNC_AV_RESET)
	strcpy(name, "reset-plugin");
    if (p_call == PHFUNC_AV_DRIVERID)
	strcpy(name, "get-driver-id");
    if (p_call == PHFUNC_AV_EQUIPID)
	strcpy(name, "get-equipment-id");
    if (p_call == PHFUNC_AV_START)
	strcpy(name, "get-device");
    if (p_call == PHFUNC_AV_BIN)
	strcpy(name, "bin-device");
    if (p_call == PHFUNC_AV_REPROBE)
	strcpy(name, "reprobe");
    if (p_call == PHFUNC_AV_COMMAND)
	strcpy(name, "send-command");
    if (p_call == PHFUNC_AV_QUERY)
	strcpy(name, "send-query");
    if (p_call == PHFUNC_AV_DIAG)
	strcpy(name, "diagnostic");
    if (p_call == PHFUNC_AV_BINREPR)
	strcpy(name, "bin-and-reprobe-device");
    if (p_call == PHFUNC_AV_PAUSE)
	strcpy(name, "pause");
    if (p_call == PHFUNC_AV_UNPAUSE)
	strcpy(name, "unpause");
    if (p_call == PHFUNC_AV_STATUS)
	strcpy(name, "get-status");
    if (p_call == PHFUNC_AV_UPDATE)
	strcpy(name, "update-plugin");
    if (p_call == PHFUNC_AV_DESTROY)
	strcpy(name, "destroy-plugin");
    if (p_call == PHFUNC_AV_STRIP_INDEXID)
    strcpy(name, "stripindex-plugin");
    if (p_call == PHFUNC_AV_STRIPID)
    strcpy(name, "stripmaterial-plugin");
    if (p_call == PHFUNC_AV_GET_STATUS)
    strcpy(name, "getstatus-plugin");
    if (p_call == PHFUNC_AV_SET_STATUS)
    strcpy(name, "setstatus-plugin");
    if (p_call == PHFUNC_AV_LOT_START)
    strcpy(name, "lotstart-plugin");
    if (p_call == PHFUNC_AV_LOT_DONE)
    strcpy(name, "lotdone-plugin");
    if (p_call == PHFUNC_AV_EXECGPIBCMD)
        strcpy(name, "exec-gpib-cmd");
    if (p_call == PHFUNC_AV_EXECGPIBQUERY)
        strcpy(name, "exec-gpib-query");
    if (p_call == PHFUNC_AV_GETSRQSTATUSBYTE)
        strcpy(name, "exec-gpib-getsrqstatusbute");
     
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
    phFuncAvailability_t p_call         /* current call to the plugin */,
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

    phStateDrvMode_t driverMode;

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
      case PHFUNC_AV_INIT:
	timeConf = PHKEY_OP_LOTTOTIME;
	actionConf = PHKEY_OP_LOTTOACT;
	break;
      case PHFUNC_AV_START:
      case PHFUNC_AV_BIN:
      case PHFUNC_AV_REPROBE:
      case PHFUNC_AV_BINREPR:
	timeConf = PHKEY_OP_DEVTOTIME;
	actionConf = PHKEY_OP_DEVTOACT;
	break;
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

    /* overrule time based timeout, if working in handtest mode */
    phStateGetDrvMode(f->stateId, &driverMode);
    if (driverMode == PHSTATE_DRV_HAND || driverMode == PHSTATE_DRV_SST_HAND)
	doTimeout = 1;

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
	    if (p_call == PHFUNC_AV_INIT)
	    {
		if (opAction == PHEVENT_OPACTION_ASK || pauseFlag)
		{
		    response[0] = '\0';
		    phGuiShowOptionDialog(f->logId, PH_GUI_YES_NO,
			"Waiting for handler communication timed out.\n"
			"\n"
			"Press YES only, if the handler is completely\n"
			"initialized and ready to handle the (next) lot of devices.\n"
			"(Remaining handler setup will be skipped)\n"
			"\n"
			"Press NO, if the handler is not yet ready to\n"
			"run and will be able to receive setup information soon.\n"
			"(Handler initialization will continue)\n"
			"\n"
			"Is the handler completely initialized?\n",
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
			"handler not yet ready to handle devices, initialization is ongoing.\n"
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
    phFuncAvailability_t p_call         /* current call to the plugin */,
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
    phFuncAvailability_t p_call         /* current call to the plugin */,
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
                              phFuncError_t funcError             /* the plugin result */,
                              phFuncAvailability_t p_call         /* current call to the plugin */,

                              int *completed                      /* plugin call is completed or
                                                                     aborted */,
                              int *success                        /* plugin call or exception
                                                                     handling was successful */,
                              phTcomUserReturn_t *retVal          /* possibly changed return
                                                                     value, in case of completed
                                                                     call */

                              )
{
    driver_dev_kits::AlarmController alarmController;

    phEventResult_t exceptionResult = PHEVENT_RES_VOID;
    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;

    int tmpCompleted = 0;
    int tmpSuccess = 0;
    int tmpRetVal = retVal ? *retVal : PHTCOM_CI_CALL_PASS;

    switch ( funcError )
    {
        case PHFUNC_ERR_OK:
            /* call has been reprobed, everything went fine */
            tmpCompleted = 1; 
            tmpSuccess = 1;
            break;
        case PHFUNC_ERR_JAM:
            alarmController.raiseMaterialHandlingAlarm("The handler is in a jammed state.");
            /* handler has reported a jam */
            phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
                    "Handler has reported a jam\n"
                    "during execution of:", (int) funcError, getPluginFunctionName(p_call));
            tmpRetVal = PHTCOM_CI_CALL_JAM;
            tmpCompleted = 1;   
            break;
        case PHFUNC_ERR_LOT_START:
            /* not really an error: handler has signeled start of lot */
            phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
                    "Handler has reported lot start\n"
                    "during execution of:", (int) funcError, getPluginFunctionName(p_call));
            if (p_call != PHFUNC_AV_LOT_START) {
                tmpRetVal = PHTCOM_CI_CALL_LOT_START;
            }
            tmpCompleted = 1; 
            break;
        case PHFUNC_ERR_LOT_DONE:
            /* not really an error: handler has signeled end of lot */
            phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
                    "Handler has reported lot end\n"
                    "during execution of:", (int) funcError, getPluginFunctionName(p_call));
            if (p_call != PHFUNC_AV_LOT_DONE) {
                tmpRetVal = PHTCOM_CI_CALL_LOT_DONE;
            }
            tmpCompleted = 1; 
            break;
        case PHFUNC_ERR_DEVICE_START:
            /* not really an error: handler has signeled end of lot */
            phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_0,
                    "Handler has reported device start\n"
                    "during execution of:", (int) funcError, getPluginFunctionName(p_call));
            if (p_call != PHFUNC_AV_START) {
                tmpRetVal = PHTCOM_CI_CALL_DEVICE_START;
            }
            tmpCompleted = 1; 
            break;
        case PHFUNC_ERR_NOT_INIT:
        case PHFUNC_ERR_INVALID_HDL:
        case PHFUNC_ERR_NA:
        case PHFUNC_ERR_MEMORY:
            /* fatal cases */
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "potential internal error of the handler driver framework.\n"
                    "Handler driver plugin call could not be completed.\n"
                    "Error code %d received during %s\n"
                    "giving up", (int) funcError, getPluginFunctionName(p_call));
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;   
            break;
        case PHFUNC_ERR_MODEL:
            alarmController.raiseMaterialHandlingAlarm("Illegal prober/handler model.");
            /* fatal cases */
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "Prober model is not supported.\n"
                    "Error received during %s\n"
                    "giving up", getPluginFunctionName(p_call));
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;   
            break;
        case PHFUNC_ERR_BINNING:
            alarmController.raiseMaterialHandlingAlarm("Error when binning the tested devices.");
        case PHFUNC_ERR_CONFIG:
            /* error conditions during specific calls */
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "%s plugin call failed", getPluginFunctionName(p_call));
            tmpCompleted = 1;
            break;
        case PHFUNC_ERR_TIMEOUT:
        case PHFUNC_ERR_WAITING:
            /* plugin call is ongoing, doing a flag check loop */
            phFrameWaiting(f, p_call, &exceptionResult, &exceptionReturn);
            switch ( exceptionResult )
            {
                case PHEVENT_RES_VOID:
                case PHEVENT_RES_CONTINUE:
                    /* go on waiting */
                    break;
                case PHEVENT_RES_ABORT:
                    /* aborted, take over proposed return value and fall
                       through to handled case */
                    tmpRetVal = exceptionReturn;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_HANDLED:
                    tmpCompleted = 1;
                    tmpSuccess = 1;
                    break;
            }
            break;
        case PHFUNC_ERR_LAN:
            /* error condition on LAN, call the event handler */
            phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, 1);
            phFrameGpibProblem(f, p_call, &exceptionResult, &exceptionReturn);
            switch ( exceptionResult )
            {
                case PHEVENT_RES_VOID:
                    /* this is a fatal situation, the error was not handled */
                    tmpRetVal = PHTCOM_CI_CALL_ERROR;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_ABORT:
                    /* aborted, take over proposed return value and fall
                       through to handled case */
                    tmpRetVal = exceptionReturn;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_CONTINUE:
                case PHEVENT_RES_HANDLED:
                    /* the problem was solved, but we still have to go on
                       to complete the plugin call */
                    break;
            }
            break;
        case PHFUNC_ERR_GPIB:
        case PHFUNC_ERR_RS232:
            /* error condition on GPIB, call the event handler */
            phFrameGpibProblem(f, p_call, &exceptionResult, &exceptionReturn);
            switch ( exceptionResult )
            {
                case PHEVENT_RES_VOID:
                    /* this is a fatal situation, the GPIB error was not
                       handled */
                    tmpRetVal = PHTCOM_CI_CALL_ERROR;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_ABORT:
                    /* aborted, take over proposed return value and fall
                       through to handled case */
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
        case PHFUNC_ERR_ABORTED:
            alarmController.raiseMaterialHandlingAlarm("PH driver is aborted.");
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "%s was aborted", getPluginFunctionName(p_call));
            tmpCompleted = 1;
            phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, 1L);
            break;
        case PHFUNC_ERR_ANSWER:
            alarmController.raiseMaterialHandlingAlarm("Unexpected answer received from prober/handler.");
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "%s plugin call received unexpected answer from handler",
                    getPluginFunctionName(p_call));
            phFrameErrorAnswer(f, p_call, &exceptionResult, &exceptionReturn);
            switch ( exceptionResult )
            {
                case PHEVENT_RES_VOID:
                    /* this is a fatal situation, the GPIB error was not
                       handled */
                    tmpRetVal = PHTCOM_CI_CALL_ERROR;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_ABORT:
                    /* aborted, take over proposed return value and fall
                       through to handled case */
                    tmpRetVal = PHTCOM_CI_CALL_ERROR;
                    tmpCompleted = 1;
                    break;
                case PHEVENT_RES_CONTINUE:
                case PHEVENT_RES_HANDLED:
                    /* the GPIB problem was solved, but we still have to go on
                       to complete the plugin call */
                    break;
            }
            break;
        case PHFUNC_ERR_FATAL:
            alarmController.raiseMaterialHandlingAlarm("The prober/handler is in a fatal state.");
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "handler driver plugin call %s ended in a fatal situation\n"
                    "giving up", getPluginFunctionName(p_call));
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;   
            break;
        default:
            /* unexpected return value, print message and fail call */
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "potential internal error of the handler driver plugin.\n"
                    "Error code %d received during %s\n"
                    "giving up", (int) funcError, getPluginFunctionName(p_call));
            tmpRetVal = PHTCOM_CI_CALL_ERROR;
            tmpCompleted = 1;
            break;
    }

    if ( completed )
        *completed = tmpCompleted;
    if ( success )
        *success = tmpSuccess;
    if ( retVal )
        *retVal = (phTcomUserReturn_t)tmpRetVal;
      
  /* if the diagnose window for user is disable, always return OK, CR28409 */
  if( f->enableDiagnoseWindow == NO ) {   
    /* specific to Infenion TUI, we hide the diagnose window */
    /**completed = 1;
    *success = 1;*/
    if ( retVal )
        *retVal = PHTCOM_CI_CALL_PASS;
  }

}

/*****************************************************************************
 * End of file
 *****************************************************************************/
