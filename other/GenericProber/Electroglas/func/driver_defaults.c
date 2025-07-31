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

    static char myFamily[] = "Electroglas";

    /* this is the list of accepted handler models by this plugin for
       the abive family. Only provide the GENERIC entry, if the plugin
       can really handle each possible model, otherwise restrict the
       names. E.g. A Seiko-Epson family driver may accept the models
       HM2000, HM3000 and HM3500 */

    static struct modelDef myModels[] =
    {
	/* { PHFUNC_MOD_UNIV_SE_HM3500, "HM3500" }, */
        { EG2001, "2001" },
        { EG2010, "2010" },
        { EG3001, "3001" },
        { EG4085, "4085" },
        { EG4060, "4060" },
        { EG4080, "4080" },
        { EG4090, "4090" },
        { EG4200, "4/200" },
        { EG5300, "5/300" },

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

    /* define, whether a parameter should be taken from the
       configuration file or from the default value defined below. 0
       means to use the configuration file, !0 means use the default
       value. In case of arrays, the number defines the number of
       entries of the array */

    /* return the flags */
    *flags = &f;

    /* here are the default values. they should be defined, even if
       the above flags are set to 0, since they might be used, if a
       parameter is not specified in the configuration file !!! 
       Always define reasonable values for all entries !!!!! */


    /* return the defaults */
    *data = &d;
}
#endif /* GPIB_SUPPORT */

/*****************************************************************************
 * End of file
 *****************************************************************************/
