/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : driver_defaults.c
 * CREATED  : 06 Sep 2000
 *
 * CONTENTS : Defaults and use of Configuration
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Chris Joyce, modified
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 06 Aug 1999, Michael Vogt, created
 *            06 Feb 2002, Chris Joyce, modified from Multitest to Tesec
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

    static char myFamily[] = "Tesec";

    /* this is the list of accepted handler models by this plugin for
       the abive family. Only provide the GENERIC entry, if the plugin
       can really handle each possible model, otherwise restrict the
       names. E.g. A Seiko-Epson family driver may accept the models
       HM2000, HM3000 and HM3500 */

    static struct modelDef myModels[] =
    {
    { PHFUNC_MOD_3270_IH, "3270" },
	{ PHFUNC_MOD_9588_IH, "9588" },
    { PHFUNC_MOD_5170_IH, "5170" },

	/* delete next entry, if not applicable */
	/* { PHFUNC_MOD_GENERIC, "*" }, */
	/* don't change next line ! */
	{ PHFUNC_MOD__END, "" }
    };

    *family = myFamily;
    *models = myModels;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
