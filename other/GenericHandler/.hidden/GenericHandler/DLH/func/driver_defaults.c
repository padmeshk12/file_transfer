/******************************************************************************
 *
 *       (c) Copyright Advantest Ltd., 2023
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : driver_defaults.c
 * CREATED  : 2023
 *
 * CONTENTS : Defaults and use of Configuration
 *
 * AUTHORS  : Runqiu Cao, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 2023, Runqiu Cao, created
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

/*--- module includes -------------------------------------------------------*/

#include "ph_mhcom.h"
#include "ph_estate.h"
#include "ph_hfunc.h"
#include "gpib_conf.h"
#include "driver_defaults.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

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

    static char myFamily[] = "Advantest";

    /* this is the list of accepted handler models by this plugin for
       the abive family. Only provide the GENERIC entry, if the plugin
       can really handle each possible model, otherwise restrict the
       names. E.g. A Seiko-Epson family driver may accept the models
       HM2000, HM3000 and HM3500 */

    static struct modelDef myModels[] =
    {
	{ PHFUNC_MOD_DLH, "DLH" },
	{ PHFUNC_MOD__END, "" }
    };

    *family = myFamily;
    *models = myModels;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
