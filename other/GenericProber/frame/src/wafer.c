/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : wafer.c
 * CREATED  : 14 Dec 1999
 *
 * CONTENTS : Wafer management functions
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Dec 1999, Michael Vogt, created
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

#include "ph_timer.h"
#include "ph_frame_private.h"
#include "stepcontrol.h"
#include "cassette.h"
#include "exception.h"
#include "clean.h"
#include "user_dialog.h"
#include "wafer.h"
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 285 */
#include "cassette.h"
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modification */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

static const char *locName[] =
{
    "input cassette",
    "output cassette",
    "chuck",
    "reference wafer position",
    "inspection tray",
    NULL
};

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * Load a new wafer
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary wafer load actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameWaferStart(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    phUserDialogError_t userDialogError;
    phEstateCassUsage_t cassUsage = PHESTATE_CASS_NOTLOADED;
    long levelEnd;

    phStateWaferRequest_t waferRequest;
    phEstateWafUsage_t icassUsage;
    phEstateWafUsage_t chuckUsage;
    phEstateWafUsage_t refUsage;
    phEstateWafUsage_t inspUsage;
    phEstateWafType_t chuckType;

/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    phEstateWafLocation_t source=PHESTATE__WAFL_END;
/* End of Huatek Modification */

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    phPFuncError_t funcError=PHPFUNC_ERR_OK;
/* End of Huatek Modification */

    long quitFlag = 0;

    /* before loading wafer make sure cassette has been loaded */
    if (phEstateAGetICassInfo(f->estateId, &cassUsage)
        != PHESTATE_ERR_OK)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
            "could not determine current input cassette status, giving up");
        return PHTCOM_CI_CALL_ERROR;
    }
    if ( cassUsage==PHESTATE_CASS_NOTLOADED )
    {
        retVal=phFrameCassetteStart(f);
        phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
        if ( retVal != PHTCOM_CI_CALL_PASS || levelEnd  )
        {
            return retVal;
        }
    }

    /* check current situation: which wafers are where and what
       is the current wafer request */
    if (phStateGetWaferMode(f->stateId, &waferRequest) 
	!= PHSTATE_ERR_OK ||
	phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_INCASS, 
	    &icassUsage, NULL, NULL) != PHESTATE_ERR_OK ||
	phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_CHUCK, 
	    &chuckUsage, &chuckType, NULL) != PHESTATE_ERR_OK ||
	phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_REFERENCE, 
	    &refUsage, NULL, NULL) != PHESTATE_ERR_OK ||
	phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_INSPTRAY, 
	    &inspUsage, NULL, NULL) != PHESTATE_ERR_OK)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "could not determine current wafer state, giving up");
	return PHTCOM_CI_CALL_ERROR;
    }

    /* first check whether this is an exceptional situation: the chuck
       is loaded but we are currently not in a retest request */
    if (chuckUsage == PHESTATE_WAFU_LOADED &&
	waferRequest != PHSTATE_WR_RETEST && 
	waferRequest != PHSTATE_WR_RET_REF)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "chuck is currently not empty.\n"
	    "Will start retesting current wafer (type '%s') on retry",
	    chuckType == PHESTATE_WAFT_REGULAR ? "regular" : 
	    chuckType == PHESTATE_WAFT_REFERENCE ? "reference" : 
	    "unknown");
	phStateSetWaferMode(f->stateId, PHSTATE_WR_RETEST);
	return PHTCOM_CI_CALL_ERROR;
    }

    switch (waferRequest)
    {
      case PHSTATE_WR_NORMAL:
	/* proceed with normal operation */
	if (inspUsage == PHESTATE_WAFU_LOADED)
	{
	    /* resume operation with wafer from the inspection
               tray. This is usually the situation after a test on the
               reference wafer has been performed */
	    source = PHESTATE_WAFL_INSPTRAY;
	}
	else
	{
	    /* this is the regular situation: take the next wafer from
               the input cassette, nothing special */
	    source = PHESTATE_WAFL_INCASS;
	}
	break;
      case PHSTATE_WR_REFERENCE:
	/* take the reference wafer, if existent and start testing. If
           it does not exist, this is an error. In that case we set
           the request mode back to REGULAR in case the inspection
           tray is loaded and issue an error code. The operator may
           then retry to resume to normal operation */
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "starting test of reference wafer");
	if (refUsage == PHESTATE_WAFU_LOADED)
	{
	    source = PHESTATE_WAFL_REFERENCE;
	}
	else
	{
	    /* reference wafer does not exist. Give the operator the
               chance to fall back to normal operation or to abort */
	    if (inspUsage == PHESTATE_WAFU_LOADED)
		phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		    "reference wafer does not exist.\n"
		    "Will load wafer back from inspection tray on retry");
	    else
		phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		    "reference wafer does not exist.\n"
		    "Will load next wafer from input cassette on retry");
	    phStateSetWaferMode(f->stateId, PHSTATE_WR_NORMAL);
	    return PHTCOM_CI_CALL_ERROR;
	}
	break;
      case PHSTATE_WR_RETEST:
	/* retest the wafer on the chuck. This could be either a
           regular wafer or the reference wafer */
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "starting retest of current wafer");
	source = PHESTATE_WAFL_CHUCK;
	break;
      case PHSTATE_WR_RET_REF:
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "starting retest of reference wafer");
	source = PHESTATE_WAFL_CHUCK;
	break;
    }

    phLogFrameMessage(f->logId, LOG_DEBUG,
	"loading wafer to chuck from %s", locName[source]);
    phEstateSetWafLoc(f->estateId, source);

    /* display User Dialog before performing Wafer Start if necessary */
    userDialogError = displayUserDialog(f->dialog_wafer_start);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"error occured while trying to display [ Wafer Start ] user dialog,\n"
		"giving up");
        return PHTCOM_CI_CALL_ERROR;
    }
    
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
        funcError = phPlugPFuncLoadWafer(f->funcId, source);

        /* analyze result from load wafer call and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_LDWAFER,
                                  &completed, &success, &retVal, &action);
    }

    switch ( action )
    {
      case PHFRAME_EXCEPTION_ERR_ERROR:
        switch ( funcError )
        {
          case PHPFUNC_ERR_OCCUPIED:
	    /* ok on retest, else internal error */
	    if (waferRequest != PHSTATE_WR_RETEST &&
		waferRequest != PHSTATE_WR_RET_REF)
	    {
		phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		    "potential internal error of the prober driver \n"
		    "or the prober driver plugin: wafer location 'chuck'\n"
		    "seems to be occupied, but it shouldn't be at the moment");
		retVal = PHTCOM_CI_CALL_ERROR;
	    }
            break;
          case PHPFUNC_ERR_EMPTY:
	    /* ok for source input cassette (set level end flag), 
	       else internal error */
	    if (source == PHESTATE_WAFL_INCASS)
		phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
	    else
	    {
		phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		    "potential internal error of the prober driver \n"
		    "or the prober driver plugin: wafer location '%s'\n"
		    "seems to be empty, but it shouldn't be at the moment", 
		    locName[source]);
		retVal = PHTCOM_CI_CALL_ERROR;
	    }
            break;
          case PHPFUNC_ERR_WAITING:
	  case PHPFUNC_ERR_TIMEOUT:
          case PHPFUNC_ERR_ANSWER:
            /* user pressed QUIT   */
            /* don't display error */
            break;
          default:
            /* unexpected return value, print message and fail call */
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"potential internal error of the prober driver plugin.\n"
		"Error code %d received during wafer load from '%s'\n"
		"giving up\n", (int) funcError,	locName[source]);
            break;
        }
        break;
      case PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE:
        /* no more wafers available, set level end flag */
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
    }

    /* in any case, reset the current die and sub-die position now */
    phFrameStepResetPattern(&(f->d_sp));
    if (f->isSubdieProbing)
	phFrameStepResetPattern(&(f->sd_sp));

    /* in case the request has been completed with success, check
       whether we need to perform a cleaning cycle */
    if (funcError == PHPFUNC_ERR_OK)
    {
	if (f->cleaningRatePerWafer > 0)
	{
	    /* only to be done, if a per wafer cleaning rate has been
               defined */
	    f->cleaningWaferCount =
		(f->cleaningWaferCount + 1) % f->cleaningRatePerWafer;
	    if (f->cleaningWaferCount == 0)
	    {
		/* perform the cleaning action */
		phLogFrameMessage(f->logId, LOG_DEBUG,
		    "will perform probe needle cleaning because another %d wafers have been processed",
		    f->cleaningRatePerWafer);
		retVal = phFrameCleanProbe(f);
		f->cleaningDieCount = 0;
	    }

	    /* early return in case the above operation ended in a quit situation */
	    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
	    if (quitFlag)
		return retVal;
	}
    }

    return retVal;
}

/*****************************************************************************
 *
 * Unload the current wafer
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary wafer unload actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameWaferDone(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    phStateWaferRequest_t waferRequest;
    phEstateWafUsage_t icassUsage;
    phEstateWafUsage_t chuckUsage;
    phEstateWafUsage_t refUsage;
    phEstateWafUsage_t inspUsage;
    phEstateWafType_t chuckType;

/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    phEstateWafLocation_t destination=PHESTATE__WAFL_END;
/* End of Huatek Modification */
    phEstateCassUsage_t cassUsage = PHESTATE_CASS_NOTLOADED;

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    phPFuncError_t funcError=PHPFUNC_ERR_OK;
/* End of Huatek Modification */
    long response;
    phEventResult_t exceptionResult = PHEVENT_RES_VOID;
    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;

    /* first check current situation: which wafers are where and what
       is the current wafer request */
    if (phStateGetWaferMode(f->stateId, &waferRequest) 
	!= PHSTATE_ERR_OK ||
	phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_INCASS, 
	    &icassUsage, NULL, NULL) != PHESTATE_ERR_OK ||
	phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_CHUCK, 
	    &chuckUsage, &chuckType, NULL) != PHESTATE_ERR_OK ||
	phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_REFERENCE, 
	    &refUsage, NULL, NULL) != PHESTATE_ERR_OK ||
	phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_INSPTRAY, 
	    &inspUsage, NULL, NULL) != PHESTATE_ERR_OK)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "could not determine current wafer state, giving up");
	return PHTCOM_CI_CALL_ERROR;
    }

    /* check whether the wafer is not loaded*/
    if (chuckUsage == PHESTATE_WAFU_NOTLOADED)
    {
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the chuck is currently empty. Call to wafer done ignored");

	/* check the cassette status here: If the input cassette is
           also no longer loaded, we set the level end flag here, to
           move up to the cassette level */
	phEstateAGetICassInfo(f->estateId, &cassUsage);
	if (cassUsage == PHESTATE_CASS_NOTLOADED)
	    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);

	return PHTCOM_CI_CALL_PASS;
    }

    switch (waferRequest)
    {
      case PHSTATE_WR_NORMAL:
	/* this is the regular situation: move the current wafer to
	   the output cassette, nothing special */
	if (chuckType == PHESTATE_WAFT_REFERENCE)
	    destination = PHESTATE_WAFL_REFERENCE;
	else
	    destination = PHESTATE_WAFL_OUTCASS;
	break;
      case PHSTATE_WR_REFERENCE:
	/* check, whether we can perform a reference wafer test at all */
	if (refUsage != PHESTATE_WAFU_LOADED)
	{
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"Automatic loading of a reference wafer\n"
		"is not supported by this prober driver plug-in.");
	    response = 2;
	    phEventPopup(f->eventId, &exceptionResult, &exceptionReturn, 0, "menu",
		"Automatic loading of a reference wafer\n"
		"is not supported by this prober driver plug-in.\n"
		"You may now press\n"
		"\n"
		"RETEST to retest the current wafer\n"
		"NEXT   to go on with the next wafer",
		"Retest", "Next", "", "", "", "", &response, NULL);
	    if (response == 2)
	    {
		phLogFrameMessage(f->logId, LOG_NORMAL,
		    "scheduling retest of the current wafer");
		phStateSetWaferMode(f->stateId, PHSTATE_WR_RETEST);
		destination = PHESTATE_WAFL_CHUCK;
	    }
	    else
	    {
		phLogFrameMessage(f->logId, LOG_NORMAL,
		    "going on with next wafer");
		phStateSetWaferMode(f->stateId, PHSTATE_WR_NORMAL);
		if (chuckType == PHESTATE_WAFT_REFERENCE)
		    destination = PHESTATE_WAFL_REFERENCE;
		else
		    destination = PHESTATE_WAFL_OUTCASS;
	    }
	    break;
	}

	/* take the current wafer to the inspection tray and let
           ph_wafer_start move in the reference wafer. */
	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "scheduling test of reference wafer");
	if (inspUsage == PHESTATE_WAFU_LOADED)
	{
	    /* exception: we can not move the current wafer to the
               inspection tray, since it is occupied (it even may not
               exist). Instead give the operator the chance to retest
               the current wafer on retry */
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"can not unload current wafer to the inspection tray\n"
		"to change to reference wafer. Will start retesting current\n"
		"wafer on retry instead.\n");
	    phStateSetWaferMode(f->stateId, PHSTATE_WR_RETEST);
	    return PHTCOM_CI_CALL_ERROR;
	}
	destination = PHESTATE_WAFL_INSPTRAY;
	break;
      case PHSTATE_WR_RETEST:
	/* retest the wafer on the chuck. This could be either a
           regular wafer or the reference wafer */
	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "scheduling retest of the current wafer");
	destination = PHESTATE_WAFL_CHUCK;
	break;
      case PHSTATE_WR_RET_REF:
	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "scheduling retest of reference wafer");
	destination = PHESTATE_WAFL_CHUCK;
	break;
    }

    phLogFrameMessage(f->logId, LOG_DEBUG,
	"unloading wafer on chuck to %s", locName[destination]);
    phEstateSetWafLoc(f->estateId, destination);

    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
        funcError = phPlugPFuncUnloadWafer(f->funcId, destination);

        /* analyze result from load wafer call and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_UNLWAFER,
                                  &completed, &success, &retVal, &action);
    }

    switch ( action )
    {
      case PHFRAME_EXCEPTION_ERR_ERROR:
        switch ( funcError )
        {
          case PHPFUNC_ERR_OCCUPIED:
	    /* ok on retest, else internal error */
	    if (waferRequest != PHSTATE_WR_RETEST &&
		waferRequest != PHSTATE_WR_RET_REF)
	    {
		phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		    "potential internal error of the prober driver \n"
		    "or the prober driver plugin: wafer location '%s'\n"
		    "seems to be occupied, but it shouldn't be at the moment",
		    locName[destination]);
		retVal = PHTCOM_CI_CALL_ERROR;
	    }
            break;
          case PHPFUNC_ERR_EMPTY:
	    /* ok for source input cassette (set level end flag), 
	       else internal error */
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"potential internal error of the prober driver \n"
		"or the prober driver plugin: wafer location 'chuck'\n"
		"seems to be empty, but it shouldn't be at the moment");
	    retVal = PHTCOM_CI_CALL_ERROR;
	    break;
          case PHPFUNC_ERR_WAITING:
	  case PHPFUNC_ERR_TIMEOUT:
          case PHPFUNC_ERR_ANSWER:
            /* user pressed QUIT   */
            /* don't display error */
            break;
          default:
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"potential internal error of the prober driver plugin.\n"
		"Error code %d received, during wafer unload to '%s'\n"
		"giving up\n", (int) funcError,	locName[destination]);
            break;
        }
        break;
      case PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE:
        /* wafer has been unloaded and there are no more wafers
           available, set level end flag */
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
     default: break;
/* End of Huatek Modification */
    }

    return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
