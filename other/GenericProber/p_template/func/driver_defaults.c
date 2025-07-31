/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : driver_defaults.c
 * CREATED  : 06 Aug 1999
 *
 * CONTENTS : Defaults and use of Configuration
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 06 Aug 1999, Michael Vogt, created
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

#include <stdlib.h>
#include <stdio.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_mhcom.h"
#include "ph_state.h"
#include "ph_pfunc.h"
#include "driver_defaults.h"
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

void phFuncPredefModels(
    char **family,
    struct modelDef **models
)
{
    /* this is the handler family name which must be unique for all
       handler driver plugins. It is checked against the the
       configuration key PHKEY_IN_HFAM */

    static char myFamily[] = "p_template";

    /* this is the list of accepted handler models by this plugin for
       the abive family. Only provide the GENERIC entry, if the plugin
       can really handle each possible model, otherwise restrict the
       names. E.g. A Seiko-Epson family driver may accept the models
       HM2000, HM3000 and HM3500 */

    static struct modelDef myModels[] =
    {
	{ PHPFUNC_MOD_TEL_P8, "P8" },
	{ PHPFUNC_MOD_TEL_1000, "1000" },
	{ PHPFUNC_MOD_TEL_1200, "2000" },

	/* delete next entry, if not applicable */
	{ PHPFUNC_MOD_GENERIC, "*" },
	/* don't change next line ! */
	{ PHPFUNC_MOD__END, "" }
    };

    *family = myFamily;
    *models = myModels;
}

#ifdef GPIB_SUPPORT
/*****************************************************************************
 *
 * Get GPIB configuration defaults
 *
 * Authors: Michael Vogt
 *
 ***************************************************************************/
void phFuncPredefGpibParams(
    struct gpibStruct **data, 
    struct gpibFlagsStruct **flags
)
{
    static struct gpibStruct d;
    static struct gpibFlagsStruct f;


    fprintf(stderr, "phFuncPredefGpibParams() in %s not yet implented !!!!!\n",
	__FILE__);

    *flags = &f;
    *data = &d;
}
#endif /* GPIB_SUPPORT */

/*****************************************************************************
 * End of file
 *****************************************************************************/
