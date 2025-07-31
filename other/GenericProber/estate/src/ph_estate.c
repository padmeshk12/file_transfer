/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_estate.c
 * CREATED  : 26 Nov 1999
 *
 * CONTENTS : Equipment Specific State Control
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Nov 1999, Michael Vogt, created
 *
 * Instructions:
 *
 * 1) Copy this template to as many .c files as you require
 * * 2) Use the command 'make depend' to make visible the new
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
#include "ph_estate.h"

#include "ph_estate_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/* don't trust anyone .... */
#define PH_HANDLE_CHECK

#ifdef DEBUG
#define PH_HANDLE_CHECK
#endif

#ifdef PH_HANDLE_CHECK
#define CheckInitialized() \
    if (!myLogger) \
        return PHESTATE_ERR_NOT_INIT

#define CheckHandle(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHESTATE_ERR_INVALID_HDL
#else
#define CheckInitialized()
#define CheckHandle(x)
#endif

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

/* Needed because the logger is only passed once during init to this
module but not during all.  Is not stored in the state handle
structure, since it is then impossible to log invalid handle access to
this module */

static phLogId_t myLogger = NULL;

static const char *locString[] =
{
    "input cassette",
    "output cassette",
    "chuck",
    "reference storage",
    "inspection tray"
};

static const char *typeString[] =
{
    "regular",
    "reference",
    "undefined"
};

static const char *usageString[] =
{
    "loaded",
    "unloaded"
};

/*--- functions -------------------------------------------------------------*/

static char *writeSiteIndex(int entries, phEstateSiteUsage_t *populated)
{
    static char siteStr[5+2*PHESTATE_MAX_SITES];
    int i, j;

    j=0;
    siteStr[j++] = '[';
    for (i=0; i<entries && i<PHESTATE_MAX_SITES; i++)
    {
	switch (populated[i])
	{
	  case PHESTATE_SITE_POPULATED:
	    siteStr[j++] = 'P';
	    break;
	  case PHESTATE_SITE_EMPTY:
	    siteStr[j++] = 'E';
	    break;
	  case PHESTATE_SITE_DEACTIVATED:
	    siteStr[j++] = 'D';
	    break;
	  case PHESTATE_SITE_POPDEACT:
	    siteStr[j++] = 'p';
	    break;
	  case PHESTATE_SITE_POPREPROBED:
	    siteStr[j++] = 'R';
	    break;
	  default:
	    siteStr[j++] = '?';
	    break;
	}
    }
    
    siteStr[j++] = ']';
    siteStr[j] = '\0';

    return &siteStr[0];
}

/*****************************************************************************
 *
 * Driver state control initialization
 *
 * Authors: Michael Vogt
 *
 * History: 26 Nov 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_estate.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateInit(
    phEstateId_t *estateID            /* the resulting driver state ID */,
    phLogId_t logger                  /* logger to be used */
)
{
    struct phEstateStruct *tmpId = NULL;
    int i;
    phEstateWafLocation_t wafLoc;

    /* use the logger */
    if (logger)
    {
	/* if we got a valid logger, use it and trace the first call
           to this module */
	myLogger = logger;
	phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	    "phEstateInit(P%p, P%p)", estateID, myLogger);
    }
    else
	/* passed logger is not valid at all, give up */
	return PHESTATE_ERR_NOT_INIT;

    /* allocate state data type */
    tmpId = PhNew(struct phEstateStruct);
    if (tmpId == 0)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_FATAL,
	    "not enough memory during call to phEstateInit()");
	return PHESTATE_ERR_MEMORY;
    }

    /* initialize the new handle */
    tmpId->myself = tmpId;
    tmpId->usedSites = 0;
    for (i=0; i<PHESTATE_MAX_SITES; i++)
	tmpId->siteField[i] = PHESTATE_SITE_EMPTY;

    tmpId->icassUsage = PHESTATE_CASS_NOTLOADED;
    
    for (wafLoc = PHESTATE_WAFL_INCASS; wafLoc < PHESTATE__WAFL_END; wafLoc = (phEstateWafLocation_t)(wafLoc + 1))
    {
	tmpId->wafUsage[wafLoc] = PHESTATE_WAFU_NOTLOADED;
	tmpId->wafType[wafLoc] = PHESTATE_WAFT_UNDEFINED;
	tmpId->wafCount[wafLoc] = 0;
    }

    tmpId->proberPaused = 0;
    tmpId->proberLotStarted = 0;
    tmpId->loc = PHESTATE__WAFL_END;

    /* return state controller ID */
    *estateID = tmpId;

    return PHESTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Destroy a Driver state controller
 *
 * Authors: Michael Vogt
 *
 * History: 09 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_estate.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateDestroy(
    phEstateId_t estateID               /* the state ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateDestroy(P%p)", estateID);
    CheckHandle(estateID);
    
    /* destroy the handle only, no memory needs to be freed */
    
    estateID->myself = NULL;

    return PHESTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Get information about the input cassette
 *
 * Authors: Michael Vogt
 *
 * History: 30 Nov 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_estate.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateAGetICassInfo(
    phEstateId_t stateID                /* equipment state ID */,
    phEstateCassUsage_t *usage          /* current input cassette usage */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateAGetICassInfo(P%p, P%p)", stateID, usage);
    CheckHandle(stateID);

    if (usage)
	*usage = stateID->icassUsage;

    return PHESTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Set information about the input cassette
 *
 * Authors: Michael Vogt
 *
 * History: 30 Nov 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_estate.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateASetICassInfo(
    phEstateId_t stateID                /* equipment state ID */,
    phEstateCassUsage_t usage           /* new input cassette usage */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetICassInfo(P%p, 0x%08lx)", stateID, (long) usage);
    CheckHandle(stateID);

    stateID->icassUsage = usage;

    phLogStateMessage(myLogger, LOG_NORMAL,
	"input cassette state set to '%s'", 
	usage == PHESTATE_CASS_LOADED ? "loaded" :
	usage == PHESTATE_CASS_NOTLOADED ? "not loaded" :
	"UNDEFINED");

    return PHESTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Store an arbitrary wafer location
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function sets an arbitrary wafer location. It is used by the
 * framework to hold the current source or destination for a wafer
 * load or unload operation. It may be shared by the event handling
 * module in case a plugin operation fails.
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateSetWafLoc(
    phEstateId_t stateID                /* equipment state ID */,
    phEstateWafLocation_t location      /* the location to store */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateSetWafLoc(P%p, 0x%08lx)", stateID, (long) location);
    CheckHandle(stateID);

    stateID->loc = location;

    phLogStateMessage(myLogger, LOG_NORMAL,
	"wafer location buffer set to '%s'", 
	location >= PHESTATE_WAFL_INCASS && 
	location < PHESTATE__WAFL_END ?
	locString[location] : "UNDEFINED");

    return PHESTATE_ERR_OK;    
}

/*****************************************************************************
 *
 * Recall an arbitrary wafer location
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function recalls an arbitrary wafer location. It is used by the
 * framework to hold the current source or destination for a wafer
 * load or unload operation. It may be shared by the event handling
 * module in case a plugin operation fails.
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateGetWafLoc(
    phEstateId_t stateID                /* equipment state ID */,
    phEstateWafLocation_t *location     /* the location result */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateSetWafLoc(P%p, P%p)", stateID, location);
    CheckHandle(stateID);

    *location = stateID->loc;

    phLogStateMessage(myLogger, LOG_NORMAL,
	"wafer location buffer '%s' recalled", 
	*location >= PHESTATE_WAFL_INCASS && 
	*location < PHESTATE__WAFL_END ?
	locString[*location] : "UNDEFINED");

    return PHESTATE_ERR_OK;    
}

/*****************************************************************************
 *
 * Get information about a wafer location
 *
 * Authors: Michael Vogt
 *
 * History: 30 Nov 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_estate.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateAGetWafInfo(
    phEstateId_t stateID                /* equipment state ID */,
    phEstateWafLocation_t location      /* the location to get the
                                           information for */,
    phEstateWafUsage_t *usage           /* wafer usage of requested
                                           location */,
    phEstateWafType_t *type             /* type of wafer */,
    int *count                          /* number of wafers of type
                                           <type> loaded at <location>
                                           (may be > 1 in case of
                                           location is either
                                           PHESTATE_WAFL_INCASS or
                                           PHESTATE_WAFL_OUTCASS) */
)
{
    phEstateCassUsage_t cassUsage;       /* current input cassette usage */

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateAGetWafInfo(P%p, 0x%08lx, P%p, P%p, P%p)", 
	stateID, (long) location, usage, type, count);
    CheckHandle(stateID);

    if (usage)
	*usage = stateID->wafUsage[location];

    if (type)
	*type = stateID->wafType[location];

    if (count)
	*count = stateID->wafCount[location];
    
    if ( location==PHESTATE_WAFL_INCASS  &&
         phEstateAGetICassInfo(stateID,&cassUsage)==PHESTATE_ERR_OK &&
         cassUsage==PHESTATE_CASS_LOADED )
    {
        if (usage)
        {
            *usage=PHESTATE_WAFU_LOADED;
        }
        if (type)
        {
            *type=PHESTATE_WAFT_REGULAR;
        }
        if (count)
        {
            *count=1; 
            phLogStateMessage(myLogger, PHLOG_TYPE_WARNING, 
                              "wafer count enquiry made for input cassette (counting should be ignored)");
        }
    }

    return PHESTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Set information about a wafer location
 *
 * Authors: Michael Vogt
 *
 * History: 30 Nov 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_estate.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateASetWafInfo(
    phEstateId_t stateID                /* equipment state ID */,
    phEstateWafLocation_t location      /* the location to set the
                                           information for */,
    phEstateWafUsage_t usage            /* new wafer usage of requested
                                           location */,
    phEstateWafType_t type              /* type of wafer */
)
{
    phEstateCassUsage_t cassUsage;       /* current input cassette usage */

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetWafInfo(P%p, 0x%08x, 0x%08x, 0x%08x)",
	stateID, location, usage, type);
    CheckHandle(stateID);


    /* can't load with an undefined wafer */
    if (usage == PHESTATE_WAFU_LOADED && type == PHESTATE_WAFT_UNDEFINED)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_ERROR,
	    "location '%s' requested to state '%s' with wafer type '%s', "
	    "not done",
	    locString[location], usageString[usage], typeString[type]);
	return PHESTATE_ERR_WAFT;
    }

    /* only put regular wafers into input/output cassettes */
    if ((location == PHESTATE_WAFL_INCASS || location == PHESTATE_WAFL_OUTCASS)
	&& usage == PHESTATE_WAFU_LOADED && type != PHESTATE_WAFT_REGULAR)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_ERROR,
	    "location '%s' can not be loaded with wafer type '%s', "
	    "not done",
	    locString[location], typeString[type]);
	return PHESTATE_ERR_WAFT;	
    }

    /* only want reference wafers in refernce wafer location */
    if (location == PHESTATE_WAFL_REFERENCE &&
	usage == PHESTATE_WAFU_LOADED && type != PHESTATE_WAFT_REFERENCE)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_ERROR,
	    "location '%s' can not be loaded with wafer type '%s', "
	    "not done",
	    locString[location], typeString[type]);
	return PHESTATE_ERR_WAFT;	
    }

    /* don't load a wafer onto reference, chuck, or inspection tray if there's one there already */
    if ((location == PHESTATE_WAFL_REFERENCE || 
	location == PHESTATE_WAFL_CHUCK || location == PHESTATE_WAFL_INSPTRAY)
	&& usage == PHESTATE_WAFU_LOADED && stateID->wafCount[location] > 0)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_ERROR,
	    "location '%s' is already occupied.\n"
	    "can not be loaded with wafer type '%s', not done",
	    locString[location], typeString[type]);
	return PHESTATE_ERR_WAFT;	
    }

    /* can't unload an empty location */
    if (usage == PHESTATE_WAFU_NOTLOADED && stateID->wafCount[location] <= 0)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
	    "location '%s' is already empty.\n"
	    "can not be unloaded with wafer type '%s', ignored",
	    locString[location], typeString[type]);
    }

    if (usage == PHESTATE_WAFU_NOTLOADED && type != PHESTATE_WAFT_UNDEFINED)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
	    "location '%s' requested to state '%s' with wafer type '%s', "
	    "using 'undefined' instead",
	    locString[location], usageString[usage], typeString[type]);
	type = PHESTATE_WAFT_UNDEFINED;
    }

    if ( location==PHESTATE_WAFL_INCASS  &&
         phEstateAGetICassInfo(stateID,&cassUsage)==PHESTATE_ERR_OK &&
         cassUsage==PHESTATE_CASS_LOADED )
    {
        phLogStateMessage(myLogger, PHLOG_TYPE_WARNING, 
                          "attempt to %s input cassette with %s wafer (counting should be ignored)",
                          (usage==PHESTATE_WAFU_LOADED) ? "load" : "unload",
	                  typeString[type]);
    }
    else
    {
        if (usage == PHESTATE_WAFU_LOADED)
        {
	    stateID->wafCount[location]++;
	    stateID->wafUsage[location] = usage;
	    stateID->wafType[location] = type;
        }
        else
        {
	    stateID->wafCount[location]--;
	    if (stateID->wafCount[location] <= 0)
	    {
	        stateID->wafCount[location] = 0; /* prevent unloading from
                                                    empty locations */
	        stateID->wafUsage[location] = usage;
	        stateID->wafType[location] = type;
	    }
        }

        phLogStateMessage(myLogger, LOG_NORMAL,
	    "location '%s' set to '%s' with %d '%s' wafer(s)",
	    locString[location], usageString[stateID->wafUsage[location]], 
	    stateID->wafCount[location], typeString[stateID->wafType[location]]);
    }

    return PHESTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Get site population information
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_estate.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateAGetSiteInfo(
    phEstateId_t stateID               /* driver state ID */,
    phEstateSiteUsage_t **populated    /* pointer to array of populated
					 sites */,
    int *entries                      /* size of the array
					 (max. number of sites) */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateAGetSiteInfo(P%p, P%p, P%p)", stateID, populated, entries);
    CheckHandle(stateID);

    /* return the requested value */
    *entries = stateID->usedSites;
    *populated = stateID->siteField;

    return PHESTATE_ERR_OK;    
}

/*****************************************************************************
 *
 * Set site population information
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_estate.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phEstateError_t phEstateASetSiteInfo(
    phEstateId_t stateID               /* driver state ID */,
    phEstateSiteUsage_t *populated     /* pointer to array of populated
					 sites */,
    int entries                       /* entries of the array
					 (max. number of sites) */
)
{
    phEstateError_t retVal = PHESTATE_ERR_OK;
    int i;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetSiteInfo(P%p, %s, %d)", stateID, 
	writeSiteIndex(entries, populated), entries);
    CheckHandle(stateID);

    /* check valid range */
    if (entries <= 0 || entries > PHESTATE_MAX_SITES)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_ERROR,
	    "the passed site population is invalid, %d entries requested", 
	    entries);
	return PHESTATE_ERR_SITE;
    }

    /* check, whether the number of sites has changed */
    if (entries != stateID->usedSites)
    {
	if (stateID->usedSites != 0)
	{
	    /* the number of sites was already defined and now
               changes. This is strange.... */
	    phLogStateMessage(myLogger, LOG_NORMAL,
		"the number of site changes, %d entries requested (was %d)", 
		entries, stateID->usedSites);
	}
	stateID->usedSites = entries;
    }

    /* copy site setting to state controller */
    for (i=0; i<entries; i++)
	stateID->siteField[i] = populated[i];

    phLogStateMessage(myLogger, LOG_NORMAL,
	"site usage set to %s", 
	writeSiteIndex(stateID->usedSites, stateID->siteField));

    return retVal;
}

phEstateError_t phEstateAGetPauseInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int *paused                         /* set, if prober is paused */
)
{
    phEstateError_t retVal = PHESTATE_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetGetPauseInfo(P%p, P%p)", stateID, paused);
    CheckHandle(stateID);

    if (paused)
	*paused = stateID->proberPaused;

    return retVal;
}


phEstateError_t phEstateASetPauseInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int paused                          /* set, if prober is paused */
)
{
    phEstateError_t retVal = PHESTATE_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetPauseInfo(P%p, %d)", stateID, paused);
    CheckHandle(stateID);

    stateID->proberPaused = !(!paused);

    phLogStateMessage(myLogger, LOG_NORMAL,
	"prober pause info set to '%s'",
	stateID->proberPaused ? "paused" : "not paused");

    return retVal;
}

phEstateError_t phEstateAGetLotInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int *lotStarted                     /* set, if prober was started
                                           and ready for test */
)
{
    phEstateError_t retVal = PHESTATE_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetGetLotInfo(P%p, P%p)", stateID, lotStarted);
    CheckHandle(stateID);

    if (lotStarted)
	*lotStarted = stateID->proberLotStarted;

    return retVal;
}


phEstateError_t phEstateASetLotInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int lotStarted                          /* set, if prober is lotStarted */
)
{
    phEstateError_t retVal = PHESTATE_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetLotInfo(P%p, %d)", stateID, lotStarted);
    CheckHandle(stateID);

    stateID->proberLotStarted = !(!lotStarted);

    phLogStateMessage(myLogger, LOG_NORMAL,
	"prober lot info set to '%s'",
	stateID->proberLotStarted ? "lot started" : "lot not yet started");

    return retVal;
}

/*Begin of Huatek Modifications, Donnie Tu, 04/23/2002*/
/*Issue Number: 334*/
void phEstateFree(phEstateId_t *estateID)
{
   if(*estateID)
   {
        free(*estateID);
        *estateID=NULL;
   }
}
/*End of Huatek Modifications*/

/*****************************************************************************
 * End of file
 *****************************************************************************/
