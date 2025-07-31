/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : pause.c
 * CREATED  : 21 Jan 2000
 *
 * CONTENTS : Prober pause management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 21 Jan 2000, Michael Vogt, created
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
#include "exception.h"
#include "pause.h"
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

/*****************************************************************************
 *
 * Enter SmarTest pause mode
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary pause actions
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFramePauseStart(
    struct phFrameStruct *f             /* the framework data */
)
{
    int proberPaused = 0;

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

    phPFuncError_t funcError;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    phEventResult_t exceptionResult = PHEVENT_RES_VOID;*/
/*    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;*/
/* End of Huatek Modification */

    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    /* find out more about the current state */
    phEstateAGetPauseInfo(f->estateId, &proberPaused);

    /* print a message before calling the plugin function */
    if (proberPaused)
	phLogFrameMessage(f->logId, LOG_NORMAL,
	   "entering SmarTest pause after prober pause has been recognized");
    else
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "entering SmarTest pause. Prober pause was not yet recognized");

    /* early return, if plugin does not support pause */
    if (!(f->funcAvail & PHPFUNC_AV_PAUSE))
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
	    "forwarding the pause state to the prober\n"
	    "is not supported by this prober driver, ignored");
	return retVal;
    }

    /* send pause to plugin and let plugin decide to pause the prober */
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
        funcError = phPlugPFuncSTPaused(f->funcId);

        /* analyze result from pause call and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_PAUSE,
                                  &completed, &success, &retVal, &action);
    }
    
    /* find out more about the current state */
    phEstateAGetPauseInfo(f->estateId, &proberPaused);

    /* print a message before calling the plugin function */
    if (proberPaused)
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the prober is now paused too");
    else
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the driver assumes that the prober is not yet paused");

    return retVal;
}

/*****************************************************************************
 *
 * Leave SmarTest pause mode
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary unpause actions
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFramePauseDone(
    struct phFrameStruct *f             /* the framework data */
)
{
    int proberPaused = 0;

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

    phPFuncError_t funcError;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    phEventResult_t exceptionResult = PHEVENT_RES_VOID;*/
/*    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;*/
/* End of Huatek Modification */

    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    /* find out more about the current state */
    phEstateAGetPauseInfo(f->estateId, &proberPaused);

    /* print a message before calling the plugin function */
    if (proberPaused)
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "leaving SmarTest pause while prober is still paused");
    else
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "leaving SmarTest pause. Prober was not known to be paused");

    /* early return, if plugin does not support pause */
    if (!(f->funcAvail & PHPFUNC_AV_UNPAUSE))
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
	    "forwarding the unpause state to the prober\n"
	    "is not supported by this prober driver, ignored");
	return retVal;
    }

    /* send pause to plugin and let plugin decide to pause the prober */
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
        funcError = phPlugPFuncSTUnpaused(f->funcId);

        /* analyze result from unpause call and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_UNPAUSE,
                                  &completed, &success, &retVal, &action);
    }

    if ( action == PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE )
    {
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
    }

    /* find out more about the current state */
    phEstateAGetPauseInfo(f->estateId, &proberPaused);

    /* print a message before calling the plugin function */
    if (proberPaused)
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the prober still seems to be paused");
    else
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the prober seems to be out of pause too");

    return retVal;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
