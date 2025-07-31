/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : lot.c
 * CREATED  : 20 Jan 2000
 *
 * CONTENTS : Lot level actions
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 20 Jan 2000, Michael Vogt, created
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
#include "cassette.h"
#include "exception.h"
#include "user_dialog.h"
#include "lot.h"
/* Begin of Huatek Modification, Donnie Tu, 12/14/2001 */
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

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * Start up a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary driver start actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameLotStart(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    long levelEnd;*/
/* End of Huatek Modification */
    static int warnedMissing = 0;
    int lotStarted = 0;

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

    phPFuncError_t funcError;
    phUserDialogError_t userDialogError;

    /* assume driver_start has been executed */

    /* Check state of current lot */
    if (phEstateAGetLotInfo(f->estateId, &lotStarted) 
	!= PHESTATE_ERR_OK)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "could not determine current lot status, giving up");
	return PHTCOM_CI_CALL_ERROR;
    }
    if (lotStarted)
    {
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "lot is already started, lot start ignored\n");
	return PHTCOM_CI_CALL_PASS;
    }

    /* display User Dialog before performing Lot Start if necessary */
    userDialogError = displayUserDialog(f->dialog_lot_start);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"error occured while trying to display [ Lot Start ] user dialog,\n"
		"giving up");
        return PHTCOM_CI_CALL_ERROR;
    }

    /* check, whether the plugin provides a lot level,
       since this is optional. If it doesn't print a message
       and return with PASS */
    if (!(f->funcAvail & PHPFUNC_AV_STARTLOT) || 
	!(f->funcAvail & PHPFUNC_AV_ENDLOT))
    {
	if (!warnedMissing)
	{
	    phLogFrameMessage(f->logId, LOG_NORMAL,
		"Note: prober driver plugin doesn't support lot handling.\n");
	    warnedMissing = 1;
	}

        /* ensure lot is started */
        phEstateASetLotInfo(f->estateId, 1);

	return PHTCOM_CI_CALL_PASS;
    }

    /* Ok now we know, that lot handling is enabled. */
    f->lotLevelUsed = 1;

    phLogFrameMessage(f->logId, LOG_DEBUG,
	"starting lot");
    
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
	funcError = phPlugPFuncStartLot(f->funcId);
        
        /* analyze result from get start lot call and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_STARTLOT, 
                                  &completed, &success, &retVal, &action);
    }

    if ( action == PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE )
    {
        /* no more lots, set level end flag */
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
    }
    
    return retVal;
}


/*****************************************************************************
 *
 * Stop a Lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary driver stop actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameLotDone(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    phEstateCassUsage_t cassUsage = PHESTATE_CASS_NOTLOADED;
    static int warnedMissing = 0;
    int lotStarted = 0;

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

    phPFuncError_t funcError;

    /* before ending lot level make sure cassette level has been ended */
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
        retVal=phFrameCassetteDone(f);
        if ( retVal != PHTCOM_CI_CALL_PASS )
        {
            return retVal;
        }
        /* overwrite potential level end flag resulting from cassette
           unload: We ARE already in the lot level here! */
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 0L);
    }

    /* Check state of current lot */
    if (phEstateAGetLotInfo(f->estateId, &lotStarted) 
	!= PHESTATE_ERR_OK)
    {
	phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
	    "could not determine current lot status, giving up");
	return PHTCOM_CI_CALL_ERROR;
    }
    if (!lotStarted)
    {
	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "lot is currently not started lot done ignored");
	return PHTCOM_CI_CALL_PASS;
    }

    /* check, whether the plugin provides a lot level,
       since this is optional. If it doesn't print a warning message
       and return with PASS */
    if (!(f->funcAvail & PHPFUNC_AV_STARTLOT) || 
	!(f->funcAvail & PHPFUNC_AV_ENDLOT))
    {
	if (!warnedMissing)
	{
	    phLogFrameMessage(f->logId, LOG_NORMAL,
		"Note: prober driver plugin doesn't support lot handling.\n");
	    warnedMissing = 1;
	}

        /* ensure lot is ended */
        phEstateASetLotInfo(f->estateId, 0);

	return PHTCOM_CI_CALL_PASS;
    }

    f->lotLevelUsed = 1;

    phLogFrameMessage(f->logId, LOG_DEBUG,
	"end current lot");
    
    phFrameStartTimer(f->totalTimer);    
    phFrameStartTimer(f->shortTimer);    

    while (!success && !completed)
    {
	funcError = phPlugPFuncEndLot(f->funcId);
        
        /* analyze result from get start lot call and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_STARTLOT, 
                                  &completed, &success, &retVal, &action);
    }

    if ( action == PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE )
    {
        /* no more lots, set level end flag */
        phTcomSetSystemFlag(f->testerId, PHTCOM_SF_CI_LEVEL_END, 1L);
    }
    
    return retVal;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
