/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : driver_defaults.c
 * CREATED  : 02 Jan 2001
 *
 * CONTENTS : Defaults and use of Configuration
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Ken Ward, Mirae revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 06 Aug 1999, Michael Vogt, created
 *
 *            Converted to support Mirae MR5800 - Jan 2005 - kaw
 *
 *            Jan 2008, CR38199, Jiawei Lin
 *              introduce the new Mirae model, M660
 *                
 *            Sept 2008, CR42150, Xiaofei Han
 *              introdue M330
 *
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

    static char myFamily[] = "Mirae";

    /* this is the list of accepted handler models by this plugin for
       the abive family. Only provide the GENERIC entry, if the plugin
       can really handle each possible model, otherwise restrict the
       names. E.g. A Mirae family driver may accept the models
       MR5800, M660 */

    static struct modelDef myModels[] =
    {
      { PHFUNC_MOD_MR5800, "MR5800" },
      { PHFUNC_MOD_M660, "M660" },
      { PHFUNC_MOD_M330, "M330" },

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
