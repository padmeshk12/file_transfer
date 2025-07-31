/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 26 Jan 2000
 *
 * CONTENTS : Private prober specific implementation
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
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
#include <assert.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_pfunc.h"

#include "gpib_conf.h"
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

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize prober specific plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateInit(
    phPFuncId_t driverID      /* the prepared driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif


#ifdef RECONFIGURE_IMPLEMENTED
/*****************************************************************************
 *
 * reconfigure driver plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateReconfigure(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif


#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateReset(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif


#ifdef DRIVERID_IMPLEMENTED
/*****************************************************************************
 *
 * return the driver identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDriverID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
)
{
    /* the resulting Id String is already composed by the code in
       ph_pfunc.c. We don't need to do anything more here and just
       return with OK */

    return PHPFUNC_ERR_OK;
}
#endif



#ifdef EQUIPID_IMPLEMENTED
/*****************************************************************************
 *
 * return the prober identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateEquipID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef LDCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Load an input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLoadCass(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef UNLCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Unload an input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateUnloadCass(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef LDWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Load a wafer to the chuck
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLoadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t source        /* where to load the wafer
                                           from. PHESTATE_WAFL_OUTCASS
                                           is not a valid option! */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef UNLWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Unload a wafer from the chuck
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateUnloadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t destination   /* where to unload the wafer
                                           to. PHESTATE_WAFL_INCASS is
                                           not valid option! */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef GETDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateGetDie(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *dieX                /* in explicit probing mode
                                 (stepping_controlled_by is set to
                                 "smartest"), this is the absolute X
                                 coordinate of the current die
                                 request, as sent to the prober. In
                                 autoindex mode
                                 (stepping_controlled_by is set to
                                 "prober" or "learnlist"), the
                                 coordinate is returned in this
                                 field. Any necessary transformations
                                 (index to microns, coordinate
                                 systems, etc.) must be performed by
                                 the calling framework */,
    long *dieY                /* same for Y coordinate */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef BINDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateBinDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL, if
                                sub-die probing is active */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis or
                                NULL, if sub-die probing is active */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef GETSUBDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next sub-die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateGetSubDie(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *subdieX             /* in explicit probing mode
                                 (stepping_controlled_by is set to
                                 "smartest"), this is the relative X
                                 coordinate of the current sub die
                                 request, as sent to the prober. In
                                 autoindex mode
                                 (stepping_controlled_by is set to
                                 "prober" or "learnlist"), the
                                 coordinate is returned in this
                                 field. Any necessary transformations
                                 (index to microns, coordinate
                                 systems) must be performed by the
                                 calling framework */,
    long *subdieY             /* same for Y coordinate */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef BINSUBDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested sub-dice
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateBinSubDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL if working
                                with sub die level */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef DIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDieList(
    phPFuncId_t proberID      /* driver plugin ID */,
    int count                 /* number of entries in <dieX> and <dieY> */,
    long *dieX                /* this is the list of absolute X
                                 coordinates of the die learnlist as
                                 to be sent to the prober. Any
                                 necessary transformations (index to
                                 microns, coordinate systems, etc.)
                                 must be performed by the calling
                                 framework */,
    long *dieY                 /* same for Y coordinate */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef SUBDIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send sub-die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSubDieList(
    phPFuncId_t proberID      /* driver plugin ID */,
    int count                 /* number of entries in <dieX> and <dieY> */,
    long *subdieX             /* this is the list of relative X
                                 coordinates of the sub-die learnlist as
                                 to be sent to the prober. Any
                                 necessary transformations (index to
                                 microns, coordinate systems, etc.)
                                 must be performed by the calling
                                 framework */,
    long *subdieY             /* same for Y coordinate */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe dice
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateReprobe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef CLEAN_IMPLEMENTED
/*****************************************************************************
 *
 * Clean the probe needles
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCleanProbe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef PMI_IMPLEMENTED
/*****************************************************************************
 *
 * Perform a probe mark inspection
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privatePMI(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSTPaused(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSTUnpaused(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef TEST_IMPLEMENTED
/*****************************************************************************
 *
 * send a command string to the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateTestCommand(
    phPFuncId_t proberID     /* driver plugin ID */,
    char *command            /* command string */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef DIAG_IMPLEMENTED
/*****************************************************************************
 *
 * retrieve diagnostic information
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDiag(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **diag              /* pointer to probers diagnostic output */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef STATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateStatus(
    phPFuncId_t proberID                /* driver plugin ID */,
    phPFuncStatRequest_t action         /* the current status action
                                           to perform */,
    phPFuncAvailability_t *lastCall     /* the last call to the
                                           plugin, not counting calls
                                           to this function */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef UPDATE_IMPLEMENTED
/*****************************************************************************
 *
 * update the equipment state
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateUpdateState(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef CASSID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current cassette ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCassID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **cassIdString      /* resulting pointer to cassette ID string */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef WAFID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current wafer ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateWafID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **wafIdString      /* resulting pointer to wafer ID string */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef PROBEID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current probe card ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateProbeID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **probeIdString     /* resulting pointer to probe card ID string */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef LOTID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current lot ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLotID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **lotIdString       /* resulting pointer to lot ID string */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef STARTLOT_IMPLEMENTED
/*****************************************************************************
 *
 * Start a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateStartLot(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef ENDLOT_IMPLEMENTED
/*****************************************************************************
 *
 * End a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateEndLot(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef COMMTEST_IMPLEMENTED
/*****************************************************************************
 *
 * Communication test
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCommTest(
    phPFuncId_t proberID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDestroy(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif


/*****************************************************************************
 * End of file
 *****************************************************************************/
