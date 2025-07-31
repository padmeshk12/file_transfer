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
 *            29 May 2000, Michael Vogt, adapted for handlers
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

    tmpId->handlerPaused = 0;
    tmpId->handlerLotStarted = 0;

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
    int *paused                         /* set, if handler is paused */
)
{
    phEstateError_t retVal = PHESTATE_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetGetPauseInfo(P%p, P%p)", stateID, paused);
    CheckHandle(stateID);

    if (paused)
	*paused = stateID->handlerPaused;

    return retVal;
}


phEstateError_t phEstateASetPauseInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int paused                          /* set, if handler is paused */
)
{
    phEstateError_t retVal = PHESTATE_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetPauseInfo(P%p, %d)", stateID, paused);
    CheckHandle(stateID);

    stateID->handlerPaused = !(!paused);

    phLogStateMessage(myLogger, LOG_NORMAL,
	"handler pause info set to '%s'",
	stateID->handlerPaused ? "paused" : "not paused");

    return retVal;
}

phEstateError_t phEstateAGetLotInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int *lotStarted                     /* set, if handler was started
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
	*lotStarted = stateID->handlerLotStarted;

    return retVal;
}


phEstateError_t phEstateASetLotInfo(
    phEstateId_t stateID                /* equipment state ID */,
    int lotStarted                          /* set, if handler is lotStarted */
)
{
    phEstateError_t retVal = PHESTATE_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phEstateASetLotInfo(P%p, %d)", stateID, lotStarted);
    CheckHandle(stateID);

    stateID->handlerLotStarted = !(!lotStarted);

    phLogStateMessage(myLogger, LOG_NORMAL,
	"handler lot info set to '%s'",
	stateID->handlerLotStarted ? "lot started" : "lot not yet started");

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
