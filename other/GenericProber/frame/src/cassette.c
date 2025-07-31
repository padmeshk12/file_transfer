/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : cassette.c
 * CREATED  : 15 Dec 1999
 *
 * CONTENTS : Cassette management functions
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Dec 1999, Michael Vogt, created
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
#include "exception.h"
#include "lot.h"
#include "wafer.h"
#include "cassette.h"
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 285 */
#include "lot.h"
#include "wafer.h"
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modification */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * Load a new input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary wafer load actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameCassetteStart(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    int lotStarted = 0;
    long levelEnd;
    static int warnedMissing = 0;
    phEstateCassUsage_t cassUsage = PHESTATE_CASS_NOTLOADED;

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

    phPFuncError_t funcError;

    /* before loading cassette make sure lot has been loaded */
    if (phEstateAGetLotInfo(f->estateId, &lotStarted)
        != PHESTATE_ERR_OK)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
            "could not determine current lot status, giving up");
        return PHTCOM_CI_CALL_ERROR;
    }
    if (!lotStarted)
    {
        retVal=phFrameLotStart(f);
        phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, &levelEnd);
        if ( retVal != PHTCOM_CI_CALL_PASS || levelEnd  )
        {
            return retVal;
        }
    }

    /* check to see if cassette is loaded */
    if (phEstateAGetICassInfo(f->estateId, &cassUsage) 
	!= PHESTATE_ERR_OK)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "could not determine current input cassette status, giving up");
	return PHTCOM_CI_CALL_ERROR;
    }
    if ( cassUsage==PHESTATE_CASS_LOADED || 
         cassUsage==PHESTATE_CASS_LOADED_WAFER_COUNT ) 
    {
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "input cassette is currently loaded, cassette start ignored\n");
	return PHTCOM_CI_CALL_PASS;
    }

    /* first check, whether the plugin provides a cassette level,
       since this is optional. If it doesn't print a warning message
       and return with PASS */

    if (!(f->funcAvail & PHPFUNC_AV_LDCASS) || 
	!(f->funcAvail & PHPFUNC_AV_UNLCASS))
    {
	if (!warnedMissing)
	{
	    phLogFrameMessage(f->logId, LOG_NORMAL,
		"Note: prober driver plugin doesn't support cassette handling");
	    warnedMissing = 1;
	}
        /* ensure input cassette is loaded: counting not used */
        phEstateASetICassInfo(f->estateId, PHESTATE_CASS_LOADED);

	return PHTCOM_CI_CALL_PASS;
    }

    /* Ok now we know, that cassette handling is enabled */
    f->cassetteLevelUsed = 1;

    phLogFrameMessage(f->logId, LOG_DEBUG,
	"loading input cassette");
    
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
        funcError = phPlugPFuncLoadCass(f->funcId);

        /* analyze result from load cassette and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_LDCASS,
                                  &completed, &success, &retVal, &action);
    }

    if ( action == PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE )
    {
        /* no more cassettes available, set level end flag */
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
    }

    return retVal;
}

/*****************************************************************************
 *
 * Unload the input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary wafer unload actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameCassetteDone(
    struct phFrameStruct *f             /* the framework data */
)
{
    phEstateWafUsage_t chuckUsage;
    phTcomUserReturn_t wafRetVal;
    int lotStarted = 0;
    int count;
    int i;

    static int warnedMissing = 0;

    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    phEstateCassUsage_t cassUsage = PHESTATE_CASS_NOTLOADED;

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

    phPFuncError_t funcError;

    /* before ending cassette level make sure wafer level has ended */
    if ( phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_CHUCK,
            &chuckUsage, NULL, NULL) != PHESTATE_ERR_OK)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
            "could not determine current chuck state, giving up");
        return PHTCOM_CI_CALL_ERROR;
    }
    if (chuckUsage == PHESTATE_WAFU_LOADED)
    {
        wafRetVal=phFrameWaferDone(f);
        if ( wafRetVal != PHTCOM_CI_CALL_PASS )
        {
            return wafRetVal;
        }
        /* overwrite potential level end flag resulting from wafer
           unload: We ARE already in the cassette level here! */
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 0L);
    }

    /* check whether the cassette is not loaded*/
    if (phEstateAGetICassInfo(f->estateId, &cassUsage) 
	!= PHESTATE_ERR_OK)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "could not determine current input cassette status, giving up");
	return PHTCOM_CI_CALL_ERROR;
    }
    if (cassUsage == PHESTATE_CASS_NOTLOADED)
    {
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "input cassette is currently not loaded cassette done ignored");

	/* check the lot status here: If the lot is also done, we set
           the level end flag here, to move up to the lot level */
	phEstateAGetLotInfo(f->estateId, &lotStarted) ;
	if (!lotStarted)
	    phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);

	return PHTCOM_CI_CALL_PASS;
    }

    /* check, whether the plugin provides a cassette level,
       since this is optional. If it doesn't print a message
       and return with PASS */
    if (!(f->funcAvail & PHPFUNC_AV_LDCASS) || 
	!(f->funcAvail & PHPFUNC_AV_UNLCASS))
    {
	if (!warnedMissing)
	{
	    phLogFrameMessage(f->logId, LOG_NORMAL,
		"Note: prober driver plugin doesn't support cassette handling.\n");
	    warnedMissing = 1;
	}

        /* remove input cassette and empty output cassette */
        phEstateASetICassInfo(f->estateId, PHESTATE_CASS_NOTLOADED);
        phEstateAGetWafInfo(f->estateId, PHESTATE_WAFL_OUTCASS,
            NULL, NULL, &count);
        for (i=0; i<count; i++)
            phEstateASetWafInfo(f->estateId, PHESTATE_WAFL_OUTCASS,
                PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
	return PHTCOM_CI_CALL_PASS;
    }

    /* Ok now we know, that cassette handling is enabled. */
    f->cassetteLevelUsed = 1;

    phLogFrameMessage(f->logId, LOG_DEBUG,
	"unloading input cassette");
    
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
        funcError = phPlugPFuncUnloadCass(f->funcId);

        /* analyze result from load cassette and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_UNLCASS,
                                  &completed, &success, &retVal, &action);
    }

    if ( action == PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE )
    {
        /* no more cassettes available, set level end flag */
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
    }
    
    return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
