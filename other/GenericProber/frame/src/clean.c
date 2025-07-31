/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : clean.c
 * CREATED  : 11 Oct 2000
 *
 * CONTENTS : Apply probe needle cleaning
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 11 Oct 2000, Michael Vogt, created
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
#include "exception.h"
#include "clean.h"
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
 * Perform an explicit probe needle cleaning (issued from test cell client)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs necessary cleaning actions
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameCleanProbe(
    struct phFrameStruct *f             /* the framework data */
)
{
    phStateDrvMode_t driverMode;
    phPFuncError_t funcError;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
    /*phEventError_t eventError;*/
    /*phEventResult_t eventResult;*/
    /*phTcomUserReturn_t eventStReturn;*/

    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;
   /* phEventResult_t exceptionResult = PHEVENT_RES_VOID;*/
   /* phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;*/

    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
/* End of Huatek Modification */

    if ((f->funcAvail & PHPFUNC_AV_CLEAN))
    {
	phStateGetDrvMode(f->stateId, &driverMode);
	if (driverMode == PHSTATE_DRV_HAND)
	{
	    phLogFrameMessage(f->logId, LOG_NORMAL,
		"cleaning probe needles in hand test mode, no action performed");
	    funcError = PHPFUNC_ERR_OK;
	}
	else
	{
	    /* this is the regular case !!!!!!! */
            while (!success && !completed)
            {
		funcError = phPlugPFuncCleanProbe(f->funcId);

                phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_CLEAN,
                                          &completed, &success, &retVal, &action);
	    }
	}
    }
    else	
	phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
	    "probe needle cleaning is not available for the current prober driver");

    /* reset cleaning counters */
    f->cleaningDieCount = 0;
    f->cleaningWaferCount = 0;

    /* not yet implemented */
    return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
