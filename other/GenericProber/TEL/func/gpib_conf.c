/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : gpib_conf.c
 * CREATED  : 10 Dec 1999
 *
 * CONTENTS : Configuration of GPIB specific parts
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 10 Dec 1999, Michael Vogt, created
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
#include <time.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_pfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "driver_defaults.h"
#include "ph_pfunc_private.h"
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
 * Reconfigure plugin specific GPIB definitions
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, created
 *
 * Description: 
 * please refer to gpib_conf.h
 *
 ***************************************************************************/
int phFuncGpibReconfigure(struct phPFuncStruct *myself)
{
    int resultValue = 1;
    struct gpibStruct safeConfig;
    struct gpibStruct *defaultConfig = NULL;
    struct gpibFlagsStruct *flags = NULL;

    phLogFuncMessage(myself->myLogger, PHLOG_TYPE_TRACE,
	"phFuncGpibReconfigureGpib(P%p)", myself);

    /* save old configuration, in case we run into fatal situations */
    safeConfig = myself->u.gpib;

    /* get the defaults and set them, they may later be overridden by
       values coming from the configuration files */

    phFuncPredefGpibParams(&defaultConfig, &flags);
    if (!defaultConfig || !flags)
    {
	phLogFuncMessage(myself->myLogger, PHLOG_TYPE_FATAL,
	    "can not retrieve default GPIB config values");
	myself->u.gpib = safeConfig;
	return 0;
    }
    myself->u.gpib = *defaultConfig;

    /* do the read of GPIB specific configuration parameters here */

    /* return with success status, recover old configuration in case
       of fatal errors */
    /* coverity eliminate
    if (! resultValue)
    {
	phLogFuncMessage(myself->myLogger, PHLOG_TYPE_ERROR,
	    "GPIB specific parts of driver plugin not (re)configured\n"
	    "due to previous error messages");
	myself->u.gpib = safeConfig;
    }
    */

    return resultValue;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
