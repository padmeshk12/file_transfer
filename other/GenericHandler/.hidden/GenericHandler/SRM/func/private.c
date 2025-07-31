/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 08 Dec 2010
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Xiaofei Han, STS R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Xiaofei Han, created
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
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_hfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "temperature.h"
#include "ph_hfunc_private.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/* this macro allows to test a not fully implemented plugin */
/* #define ALLOW_INCOMPLETE */
#undef ALLOW_INCOMPLETE

#ifdef ALLOW_INCOMPLETE
#define ReturnNotYetImplemented(x) \
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, \
        "%s not yet implemented (ignored)", x); \
    return PHFUNC_ERR_OK
#else
#define ReturnNotYetImplemented(x) \
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, \
        "%s not yet implemented", x); \
    return PHFUNC_ERR_NA
#endif

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/


/* flush any unexpected SRQs */
static phFuncError_t flushSRQs(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int received = 0;
    int srq = 0;

    do
    {
	PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 1));
	phFuncTaRemoveStep(handlerID);
    } while (received);

    return retVal;
}

/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
    static int srq = 0;
    int srqPicked = 0;
    static int received = 0;
    int i;
    int partMissing = 0;
    int populationMask = 0;

    phFuncError_t retVal = PHFUNC_ERR_OK;

    if (handlerID->p.strictPolling)
    {
    }
    else
    {
      /* Receive a start SRQ */
      PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, handlerID->p.oredDevicePending));

      if (handlerID->model == PHFUNC_MOD_SRM && handlerID->noOfSites <= 4) 
      {
        populationMask = 0x0f;
      }

      if (received)
      {
        switch (handlerID->model)
        {
          case PHFUNC_MOD_SRM:
            if (srq & populationMask)
            {
              /* there may be parts given in the SRQ byte */
              partMissing = 0;
              for (i=0; i<handlerID->noOfSites; i++)
              {
                if (srq & (1 << i))
                {
                  handlerID->p.devicePending[i] = 1;
                  handlerID->p.oredDevicePending = 1;
                  if(handlerID->noOfSites <= 4) 
                  {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "new device present at site \"%s\" (SRQ)", 
				     handlerID->siteIds[i]);
                  }
                  else
                  {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "new device present at site \"%s\" (SITES)", 
                                     handlerID->siteIds[i]);
                    srqPicked |= (1 << i);
                  }
                }
                else 
                {
                  handlerID->p.devicePending[i] = 0;
                  if (handlerID->p.deviceExpected[i])
                     partMissing = 1;
                }
              }

              /* 
               * check whether some parts seem to be missing. In
               * case of a reprobe, we must at least receive the
               * reprobed devices. In all other cases we should
               * receive devices in all active sites. 
               */
              if (partMissing)
              {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                 "some expected device(s) seem(s) to be missing");
                PhFuncTaCheck(flushSRQs(handlerID));
              }

              /* check whether we have received to many parts */
              if ((srq & populationMask) != srqPicked)
              {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                                 "The handler seems to present more devices than configured\n"
                                 "The driver configuration must be changed to support more sites");
              }
            }
            else
            {
              /* some exceptional SRQ occured do nothing */
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "received exceptional SRQ 0x%02x", srq);
            }
            break;
          default: 
            break;
        }
      }
    }

    return retVal;
}


/* create and send bin and reprobe information */
static phFuncError_t binAndReprobe(
    phFuncId_t handlerID,
    phEstateSiteUsage_t *oldPopulation,
    long *perSiteReprobe, 
    long *perSiteBinMap     
                           
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    /*
     * The binning command of SRM handler is bSORTxn, where
     * x is the site name "A", "B", "C" or "D" and n is the
     * bin number 1-8.
     */
    char binningCommand[9];
    int i;

    strcpy(binningCommand,"");
    
    switch(handlerID->model)
    {
      case PHFUNC_MOD_SRM:

	for (i=0; i<handlerID->noOfSites; i++)
	{
	    if (handlerID->activeSites[i] && 
		(oldPopulation[i] == PHESTATE_SITE_POPULATED ||
		 oldPopulation[i] == PHESTATE_SITE_POPDEACT))
	    {
              if (perSiteBinMap[i] < 1 || perSiteBinMap[i] > 8)
              {
                /* illegal bin code */
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                                 "received illegal request for bin %ld at site \"%s\"",
                                 perSiteBinMap[i], handlerID->siteIds[i]);
                retVal =  PHFUNC_ERR_BINNING;
              }
              else
              {
                /* bin this one */
                sprintf(binningCommand, "bSORT%s%ld", handlerID->siteIds[i], perSiteBinMap[i]);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "will bin device at site \"%s\" to %ld", 
                                 handlerID->siteIds[i], perSiteBinMap[i]);
                PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", binningCommand, handlerID->p.eol));
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "send command \"%s\"", binningCommand);
              }
	    }
            else
            {
              /* there is a no device here */
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "no device to reprobe or bin at site \"%s\"", 
                               handlerID->siteIds[i]);
	    }
	}
        break;

      default: 
        break;
    }


    return retVal;
}


static phFuncError_t getMachineID(
    phFuncId_t handlerID    /* driver plugin ID */,
    char **machineIdString  /* resulting pointer to equipment ID string */
)
{
    static char idString[512] = "";

    idString[0] = '\0';

    phFuncError_t retVal = PHFUNC_ERR_OK;

    *machineIdString = idString;

    PhFuncTaCheck(phFuncTaSend(handlerID, "*IDN?%s", handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", idString);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); 
        return retVal;
    }
    return retVal;
}

static phFuncError_t verifyModel(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *idString            /* resulting pointer to equipment ID string */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    /* strip of white space at the end of the string */
    while (strlen(idString) > 0 && isspace(idString[strlen(idString)-1]))
        idString[strlen(idString)-1] = '\0';

    /* try to verify the handler model and handler SW revision */
    if (strlen(idString) > 0)
    {
        switch(handlerID->model)
        {
          case PHFUNC_MOD_SRM:
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                "received handler ID \"%s\", expecting a SRM Handler (not verified)",
                idString);
            break;

          default: 
            break;
        }
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
            "empty ID string received from handler, ignored");
    }
    return retVal;
}

static phFuncError_t handlerSetup(
    phFuncId_t handlerID    /* driver plugin ID */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    return retVal;
}


/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phConfError_t confError;
    double dPollingInterval;
    char *machineID;
    static int firstTime=1;

    /* chose polling or SRQ interrupt mode */
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "activated SRQ based device handling");
    handlerID->p.strictPolling = 0;

    /* retrieve polling interval */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_POLLT);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_POLLT, 
	    0, NULL, &dPollingInterval);
    if (confError == PHCONF_ERR_OK)
	handlerID->p.pollingInterval = labs((long) dPollingInterval);
    else
    {
	handlerID->p.pollingInterval = 200000L; /* default 0.2 sec */
    }

    /* do we want to pause the handler ? */
    handlerID->p.pauseHandler = 0;

    /* perform the communication intesive parts */
    if ( firstTime )
    {
        PhFuncTaCheck(getMachineID(handlerID, &machineID));
        PhFuncTaCheck(verifyModel(handlerID, machineID));
        PhFuncTaCheck(handlerSetup(handlerID));

        /* change equipment state */
        if (handlerID->p.stopped)
	    phEstateASetPauseInfo(handlerID->myEstate, 1);
        else
	    phEstateASetPauseInfo(handlerID->myEstate, 0);

        firstTime=0;
    }

    return retVal;
}


#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateInit(
    phFuncId_t handlerID      /* the prepared driver plugin ID */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static int firstCall = 1;*/
/*    static int isConfigured = 0;*/
/*    static char idString[512] = "";*/
/* End of Huatek Modification */

    int i;
    phFuncError_t retVal = PHFUNC_ERR_OK;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    /* do some really initial settings */
    handlerID->p.sites = handlerID->noOfSites;
    for (i=0; i<handlerID->p.sites; i++)
    {
        handlerID->p.siteUsed[i] = handlerID->activeSites[i];
        handlerID->p.devicePending[i] = 0;
        handlerID->p.deviceExpected[i] = 0;
    }

    handlerID->p.pauseHandler = 0;
    handlerID->p.pauseInitiatedByST = 0;

    handlerID->p.strictPolling = 0;
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
    handlerID->p.stopped = 0;

    strcpy(handlerID->p.eol, "\r");
	
    /* assume, that the interface was just opened and cleared, so
       wait 1 second to give the handler time to be reset the
       interface */
    sleep(1);

    return retVal;
}
#endif


#ifdef RECONFIGURE_IMPLEMENTED
/*****************************************************************************
 *
 * reconfigure driver plugin
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReconfigure(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);
    PhFuncTaCheck(doReconfigure(handlerID));
    phFuncTaStop(handlerID);
    
    return retVal;
}
#endif


#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the handler
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReset(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateReset");
}
#endif


#ifdef DRIVERID_IMPLEMENTED
/*****************************************************************************
 *
 * return the driver identification string
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDriverID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
)
{
    /* the resulting Id String is already composed by the code in
       ph_hfunc.c. We don't need to do anything more here and just
       return with OK */

    return PHFUNC_ERR_OK;
}
#endif



#ifdef EQUIPID_IMPLEMENTED
/*****************************************************************************
 *
 * return the handler identification string
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateEquipID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    static char idString[512] = "";
    static char resultString[1024] = "";
    idString[0] = '\0';
    *equipIdString = idString;
    phFuncError_t retVal = PHFUNC_ERR_OK;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    /* common to all models: identify */
    phFuncTaStart(handlerID);
    PhFuncTaCheck(phFuncTaSend(handlerID, "*IDN?%s", handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", idString);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); 
        return retVal;
    }

    phFuncTaStop(handlerID);

    /* create result string */
    sprintf(resultString, "%s", idString);

    /* strip of white space at the end of the string */
    while (strlen(resultString) > 0 && isspace(resultString[strlen(resultString)-1]))
	resultString[strlen(resultString)-1] = '\0';
    return retVal;
}
#endif



#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next device
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateGetStart(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int i;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    /* remember which devices we expect now */
    for (i=0; i<handlerID->noOfSites; i++)
	handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

    phFuncTaStart(handlerID);

    phFuncTaMarkStep(handlerID);
    PhFuncTaCheck(waitForParts(handlerID));

    /* do we have at least one part? If not, ask for the current
       status and return with waiting */
    if (!handlerID->p.oredDevicePending)
    {
	/* during the next call everything up to here should be
           repeated */
	phFuncTaRemoveToMark(handlerID);

	/* change equipment state */
	if (handlerID->p.stopped)
	    phEstateASetPauseInfo(handlerID->myEstate, 1);
	else
	    phEstateASetPauseInfo(handlerID->myEstate, 0);
	
	return PHFUNC_ERR_WAITING;
    }
    
    phFuncTaStop(handlerID);
    phEstateASetPauseInfo(handlerID->myEstate, 0);

    /* we have received devices for test. Change the equipment
       specific state now */
    handlerID->p.oredDevicePending = 0;
    for (i=0; i<handlerID->noOfSites; i++)
    {
	if (handlerID->activeSites[i] == 1)
	{
	    if (handlerID->p.devicePending[i])
	    {
		handlerID->p.devicePending[i] = 0;
		population[i] = PHESTATE_SITE_POPULATED;
	    }
	    else
		population[i] = PHESTATE_SITE_EMPTY;
	}
	else
	{
	    if (handlerID->p.devicePending[i])
	    {
		/* something is wrong here, we got a device for an
                   inactive site */
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
		    "received device for deactivated site \"%s\"",
		    handlerID->siteIds[i]);
	    }
	    population[i] = PHESTATE_SITE_DEACTIVATED;
	}
	handlerID->p.oredDevicePending |= handlerID->p.devicePending[i];
    }	
    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);

    return retVal;
}
#endif



#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested device
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateBinDevice(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    phEstateSiteUsage_t *oldPopulation;
    int entries;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    int i;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    /* get current site population */
    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

    phFuncTaStart(handlerID);
    PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation, NULL, perSiteBinMap));
    phFuncTaStop(handlerID);

    /* modify site population, everything went fine, otherwise we
       would not reach this point */
    for	(i=0; i<handlerID->noOfSites; i++)
    {
      if (handlerID->activeSites[i] && 
          (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
          oldPopulation[i] == PHESTATE_SITE_POPDEACT))
      {
        population[i] = oldPopulation[i] == PHESTATE_SITE_POPULATED ?
                        PHESTATE_SITE_EMPTY : PHESTATE_SITE_DEACTIVATED;
      }
      else
      {
        population[i] = oldPopulation[i];	
      }
    }

    /* change site population */
    phEstateASetSiteInfo(handlerID->myEstate, population,handlerID->noOfSites);

    return retVal;
}
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReprobe(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateReprobe");

    return PHFUNC_ERR_OK;
}
#endif



#ifdef BINREPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateBinReprobe(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteReprobe     /* TRUE, if a device needs reprobe*/,
    long *perSiteBinMap      /* valid binning data for each site where
                                the above reprobe flag is not set */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateBinReprobe");

    return PHFUNC_ERR_OK;

}
#endif



#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to handler plugin
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSTPaused(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateSTPaused");

    return PHFUNC_ERR_OK;

}
#endif



#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to handler plugin
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSTUnpaused(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateSTUnpaused");

    return PHFUNC_ERR_OK;
}
#endif



#ifdef DIAG_IMPLEMENTED
/*****************************************************************************
 *
 * retrieve diagnostic information
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDiag(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **diag              /* pointer to handlers diagnostic output */
)
{
    *diag = phLogGetLastErrorMessage();
    return PHFUNC_ERR_OK;
}
#endif



#ifdef STATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStatus(
    phFuncId_t handlerID                /* driver plugin ID */,
    phFuncStatRequest_t action          /* the current status action
                                           to perform */,
    phFuncAvailability_t *lastCall      /* the last call to the
                                           plugin, not counting calls
                                           to this function */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateStatus");

    return PHFUNC_ERR_OK;

}
#endif



#ifdef UPDATE_IMPLEMENTED
/*****************************************************************************
 *
 * update the equipment state
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateUpdateState(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateUpdateState");
}
#endif



#ifdef COMMTEST_IMPLEMENTED
/*****************************************************************************
 *
 * Communication test
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateCommTest(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char *machineID;

    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);
    PhFuncTaCheck(getMachineID(handlerID, &machineID));
    phFuncTaStop(handlerID);

    /* return result, there is no real check for the model here (so far) */
    if (testPassed)
        *testPassed = 0;
    return retVal;
}
#endif


#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDestroy(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateDestroy");
}
#endif



/*****************************************************************************
 * End of file
 *****************************************************************************/
