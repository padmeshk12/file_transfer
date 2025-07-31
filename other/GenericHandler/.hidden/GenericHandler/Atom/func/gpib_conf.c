/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : gpib_conf.c
 * CREATED  : 01 Jul 2023
 *
 * CONTENTS : Configuration of GPIB specific parts
 *
 * AUTHORS  : Zuria Zhu, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, created
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
#include "ph_hfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "driver_defaults.h"
#include "ph_hfunc_private.h"
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
 * Authors: Zuria Zhu
 *
 * History: 01 Jul 2023, Zuria Zhu, created
 *
 * Description: 
 * please refer to gpib_conf.h
 *
 ***************************************************************************/
int phFuncGpibReconfigure(struct phFuncStruct *myself)
{
    phLogFuncMessage(myself->myLogger, PHLOG_TYPE_TRACE,
	"phFuncGpibReconfigureGpib(P%p)", myself);

    /* nothing to do */

    /*
    phLogFuncMessage(myself->myLogger, PHLOG_TYPE_ERROR,
	"phFuncGpibReconfigureGpib() not yet implemented");
    */

    /* to be defined */
    return 1;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
