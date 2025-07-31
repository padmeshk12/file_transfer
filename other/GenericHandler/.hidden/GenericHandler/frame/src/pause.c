/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : pause.c
 * CREATED  : 21 Jan 2000
 *
 * CONTENTS : Handler pause management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 21 Jan 2000, Michael Vogt, created
 *            14 Jun 2000, Michael Vogt, adapted from prober driver
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
    int handlerPaused = 0;
    int success = 0;
    int completed = 0;
    phFuncError_t funcError;
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    /* early return, if plugin does not support pause */
    if (!(f->funcAvail & PHFUNC_AV_PAUSE))
    {
	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "forwarding the pause state to the handler\n"
	    "is not supported by this handler driver, ignored");
	return retVal;
    }

    /* find out more about the current state */
    phEstateAGetPauseInfo(f->estateId, &handlerPaused);

    /* print a message before calling the plugin function */
    if (handlerPaused)
	phLogFrameMessage(f->logId, LOG_NORMAL,
	   "entering SmarTest pause after handler pause has been recognized");
    else
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "entering SmarTest pause. Handler pause was not yet recognized");

    /* send pause to plugin and let plugin decide to pause the handler */
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    
    while (!success && !completed)
    {
	funcError = phPlugFuncSTPaused(f->funcId);
	phFrameHandlePluginResult(f, funcError, PHFUNC_AV_PAUSE, 
	    &completed, &success, &retVal);
    }

    /* find out more about the current state */
    phEstateAGetPauseInfo(f->estateId, &handlerPaused);

    /* print a message before calling the plugin function */
    if (handlerPaused)
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the handler is now paused too");
    else
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the driver assumes that the handler is not yet paused");

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
    int handlerPaused = 0;
    int success = 0;
    int completed = 0;
    phFuncError_t funcError;
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    /* early return, if plugin does not support pause */
    if (!(f->funcAvail & PHFUNC_AV_UNPAUSE))
    {
	phLogFrameMessage(f->logId, LOG_DEBUG,
	    "forwarding the unpause state to the handler\n"
	    "is not supported by this handler driver, ignored");
	return retVal;
    }

    /* find out more about the current state */
    phEstateAGetPauseInfo(f->estateId, &handlerPaused);

    /* print a message before calling the plugin function */
    if (handlerPaused)
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "leaving SmarTest pause while handler is still paused");
    else
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "leaving SmarTest pause. Handler was not known to be paused");

    /* send pause to plugin and let plugin decide to pause the handler */
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    
    while (!success && !completed)
    {
	funcError = phPlugFuncSTUnpaused(f->funcId);
	phFrameHandlePluginResult(f, funcError, PHFUNC_AV_UNPAUSE, 
	    &completed, &success, &retVal);
    }

    /* find out more about the current state */
    phEstateAGetPauseInfo(f->estateId, &handlerPaused);

    /* print a message before calling the plugin function */
    if (handlerPaused)
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the handler still seems to be paused");
    else
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "the handler seems to be out of pause too");

    return retVal;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
