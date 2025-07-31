/******************************************************************************
 *
 *       (c) Copyright Advantest 2019
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : driver_defaults.h
 * CREATED  : 04 Jan 2019
 *
 * CONTENTS : Defaults and use of Configuration
 *
 * AUTHORS  : Zoyi Yu, SH-R&D
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 04 Jan 2019, Zoyi Yu, created
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
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
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

    static char myFamily[] = "Esmo";

    /* this is the list of accepted handler models by this plugin for
       the abive family. Only provide the GENERIC entry, if the plugin
       can really handle each possible model, otherwise restrict the
       names. E.g. Esmo family driver may accept the models TALOS2*/

    static struct modelDef myModels[] =
    {
	{ PHFUNC_MOD_TALOS, "Talos" },
	/* don't change next line ! */
	{ PHFUNC_MOD__END, "" }
    };

    *family = myFamily;
    *models = myModels;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
