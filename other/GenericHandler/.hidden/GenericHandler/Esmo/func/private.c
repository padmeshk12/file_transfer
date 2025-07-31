/******************************************************************************
 *
 *       (c) Copyright Advantest Technologies
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 04 Jan 2019
 *
 * CONTENTS : Private handler specific implementation
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
#include "ph_GuiServer.h"

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

/* 
 *  following answers to queries from Esmo it appears the
 *  SQBM command should not be used
 */
#undef ALLOW_SQBM_COMMAND


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
static int localDelay_us(long microseconds) 
{
  long seconds;
  struct timeval delay;

  /* us must be converted to seconds and microseconds ** for use by select(2) */
  if(microseconds >= 1000000L) {
    seconds = microseconds / 1000000L;
    microseconds = microseconds % 1000000L;
  } else {
    seconds = 0;
  }

  delay.tv_sec = seconds; 
  delay.tv_usec = microseconds; 

  /* return 0, if select was interrupted, 1 otherwise */
  if(select(0, NULL, NULL, NULL, &delay) == -1 &&
     errno == EINTR)
    return 0;
  else
    return 1;
}

/* flush any unexpected SRQs */
static phFuncError_t flushSRQs(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  int received = 0;
  int srq = 0;

  do {
    PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 1));
    phFuncTaRemoveStep(handlerID);
  } while(received);

  return retVal;
}

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  static char handlerAnswer[1024] = "";
  int contactedSitesNumber = 0;
  int i = 0;
  int sitesNumber[64] = {0};
  char *p;

  switch(handlerID->model) {
    case PHFUNC_MOD_TALOS:
      PhFuncTaCheck(phFuncTaSend(handlerID, "CRT?%s", handlerID->p.eol));
      retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
      if(retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID);
        return retVal;
      }
      break;
    default: 
      break;
  }

  /* check answer from handler, similar for all handlers.  remove
     two steps (the query and the answer) if string not digital received */
  p = strtok(handlerAnswer, ",");
  while(p) {
    contactedSitesNumber = atoi(p);
    if(contactedSitesNumber < 0 || contactedSitesNumber > 64) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                       "Contacted sites number is: %d, out of range 0..64", contactedSitesNumber);
      phFuncTaRemoveStep(handlerID);
      phFuncTaRemoveStep(handlerID);
      return PHFUNC_ERR_ANSWER;
    }

    sitesNumber[i++] = contactedSitesNumber;
    p = strtok(NULL, ",");
  }

  /* correct pending devices information, overwrite all (trust the
     last handler query) */
  handlerID->p.oredDevicePending = 0;
  for(i=0; i<handlerID->noOfSites; i++) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Contacted sites number is: %d", sitesNumber[i]);

    if(sitesNumber[i] > 0) {
      handlerID->p.devicePending[i] = 1;
      handlerID->p.oredDevicePending = 1;
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                       "device present at site \"%s\" (polled)",
                       handlerID->siteIds[i]);
    } else {
      handlerID->p.devicePending[i] = 0;
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                       "no device at site \"%s\" (polled)",
                       handlerID->siteIds[i]);
    }
  }

  /* ignore SRQs sent during polling if polling found devices - CR12663 */
  if(handlerID->p.oredDevicePending) {
    PhFuncTaCheck(flushSRQs(handlerID));
  }

  return retVal;
}

/* wait for lot start from handler */
static phFuncError_t waitForLotStart(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering waitForLotStart: look for SRQ from handler\n");

  switch( handlerID->model ) {
    //Note that the Talos driver SHALL NOT call the ph_lot_start, but still safe if the user wants
    case PHFUNC_MOD_TALOS:
      retVal = PHFUNC_ERR_OK;
      break;

    default: break;
  }

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Exiting waitForLotStart, retVal = %d\n", retVal);
  return retVal;
}


/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
  static int srq = 0;
  int srqPicked = 0;
  static int received = 0;
  struct timeval pollStartTime;
  struct timeval pollStopTime;
  int timeout;
  int i;
  int partMissing = 0;
  int populationMask = 0xff;
  char handlerAnswer[1024] = "";

  phFuncError_t retVal = PHFUNC_ERR_OK;

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering waitForParts");
 
  if(handlerID->p.strictPolling) {
    /* apply strict polling loop, ask for site population */
    gettimeofday(&pollStartTime, NULL);

    timeout = 0;
    localDelay_us(handlerID->p.pollingInterval);
    do {
      PhFuncTaCheck(pollParts(handlerID));
      if(!handlerID->p.oredDevicePending) {
        gettimeofday(&pollStopTime, NULL);
        if((pollStopTime.tv_sec - pollStartTime.tv_sec)*1000000L + 
           (pollStopTime.tv_usec - pollStartTime.tv_usec) > 
           handlerID->heartbeatTimeout)
          timeout = 1;
        else
          localDelay_us(handlerID->p.pollingInterval);
      }
    } while(!handlerID->p.oredDevicePending && !timeout);

    /* flush any pending SRQs, we don't care about them but want
             to eat them up. Otherwise reconfiguration to interrupt
             based mode would fail. */
    PhFuncTaCheck(flushSRQs(handlerID));
  } else {
    /* we don't do strict polling, try to receive a start SRQ */
    /* this operation covers the heartbeat timeout only, if there
             are no more unexpected devices pending from a previous call
             (e.g. devices provided during reprobe) */
    PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 
                                  handlerID->p.oredDevicePending));
    
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "get first SRQ 0x%02x in waitForParts", srq);
 
    if(received) {
      switch(handlerID->model) {
        case PHFUNC_MOD_TALOS:
          phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "The site number is %d, try to polling...", handlerID->noOfSites);
          PhFuncTaCheck(pollParts(handlerID));
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
                                  phFuncId_t handlerID     /* driver plugin ID */,
                                  phEstateSiteUsage_t *oldPopulation /* current site population */,
                                  long *perSiteReprobe     /* TRUE, if a device needs reprobe*/,
                                  long *perSiteBinMap      /* valid binning data for each site where
                                                              the above reprobe flag is not set */
                                  )
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  /*
   *  Notes on binningCommand array:
   *  the binningCommand is created on the assumption that the
   *  format for the command will be --XXBINYY --
   *  for each possible site which is a maximum of 7 characters for each site
   *  char binningCommand[PHESTATE_MAX_SITES*7 + 16];
   */
  char binningCommand[4096];
  char tmpCommand[2048];
  int i;
  int sendBinning = 0;

  strcpy(binningCommand, "");

  switch(handlerID->model) {
    case PHFUNC_MOD_TALOS:
      /* 
       *  following answers to queries from Esmo
       *  recommended to send all binning in one go
       */
      for(i=0; i<handlerID->noOfSites; i++) {
        if(handlerID->activeSites[i] && 
           (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
            oldPopulation[i] == PHESTATE_SITE_POPDEACT)) {
          /* there is a device here */
          if(perSiteReprobe && perSiteReprobe[i]) {
            if( !handlerID->p.reprobeBinDefined ) {
              /* no reprobe bin number defined */
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                               "received reprobe command at site \"%s\" but no reprobe bin number defined \n"
                               "please check the configuration values of \"%s\" and \"%s\"",
                               handlerID->siteIds[i], PHKEY_RP_AMODE, PHKEY_RP_BIN);
              retVal =  PHFUNC_ERR_BINNING;
            } else {
              /* reprobe this one */
              sprintf(tmpCommand, "%s BIN %d ",
                      handlerID->siteIds[i], handlerID->p.reprobeBinNumber);
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                               "will reprobe device at site \"%s\"", 
                               handlerID->siteIds[i]);
              strcat(binningCommand, tmpCommand);
              sendBinning = 1;
            }
          } else {
            /* 
             *  following answers to queries from Esmo
             *  maximum bin category is 31
             */
            if(perSiteBinMap[i] < 0 || perSiteBinMap[i] > 31) {
              /* illegal bin code */
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                               "received illegal request for bin %ld at site \"%s\"",
                               perSiteBinMap[i], handlerID->siteIds[i]);
              retVal =  PHFUNC_ERR_BINNING;
            } else {
              /* bin this one */
              sprintf(tmpCommand, "%s BIN %ld ",
                      handlerID->siteIds[i], perSiteBinMap[i]);
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                               "will bin device at site \"%s\" to %ld", 
                               handlerID->siteIds[i], perSiteBinMap[i]);
              strcat(binningCommand, tmpCommand);
              sendBinning = 1;
            }
          }
        } else {
          /* there is a no device here */
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                           "no device to reprobe or bin at site \"%s\"", 
                           handlerID->siteIds[i]);
        }
      }
      break;

    default: 
      break;
  }

  if( retVal==PHFUNC_ERR_OK && sendBinning ) {
    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", 
                               binningCommand, handlerID->p.eol));
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "send command \"%s\"", 
                     binningCommand);
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

  /* common to all models: identify */
  PhFuncTaCheck(phFuncTaSend(handlerID, "IDN?%s", handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", idString);
  if(retVal != PHFUNC_ERR_OK) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
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
  while(strlen(idString) > 0 && isspace(idString[strlen(idString)-1]))
    idString[strlen(idString)-1] = '\0';

  /* try to verify the handler model and handler SW revision */
  if(strlen(idString) > 0) {
    switch(handlerID->model) {
      case PHFUNC_MOD_TALOS:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "received handler ID \"%s\", expecting a Esmo Talos handler (not verified)",
                         idString);
        break;
      default: 
        break;

    }
  } else {
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

  /* model specific part */
  switch(handlerID->model) {
    case PHFUNC_MOD_TALOS:
      /* It should be safe to send the STA! at the beginning */
      PhFuncTaCheck(phFuncTaSend(handlerID, "STA!%s", handlerID->p.eol));
      break;
    default: break;
  }

  /*  
   * flush any pending SRQs (will poll later, if we are missing some
   * test start SRQ here 
   * following answers to queries from Esmo it appears the
   * Talos also supports polling: So may flush for all handlers
   */
  PhFuncTaCheck(flushSRQs(handlerID));

  return retVal;
}


/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  int found = 0;
  phConfError_t confError;
  double dPollingInterval;
  double dReprobeBinNumber;
  char *machineID;
  static int firstTime=1;

  /* chose polling or SRQ interrupt mode */
  phConfConfStrTest(&found, handlerID->myConf, PHKEY_FC_WFPMODE, 
                    "polling", "interrupt", NULL);
  if(found == 1) {
    /* Talos support polling */
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "activated strict polling mode, no SRQ handling");
    handlerID->p.strictPolling = 1;
  } else {
    /* default */
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "activated SRQ based device handling");
    handlerID->p.strictPolling = 0;
  }

  /* retrieve polling interval */
  confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_POLLT);
  if(confError == PHCONF_ERR_OK)
    confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_POLLT, 
                                 0, NULL, &dPollingInterval);
  if(confError == PHCONF_ERR_OK)
    handlerID->p.pollingInterval = labs((long) dPollingInterval);
  else {
    handlerID->p.pollingInterval = 200000L; /* default 0.2 sec */
  }

  /* do we want to pause the handler ? */
  handlerID->p.pauseHandler = 0;

  confError = phConfConfStrTest(&found, handlerID->myConf,
                                PHKEY_OP_PAUPROB, "yes", "no", NULL);
  if(confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
    retVal = PHFUNC_ERR_CONFIG;
  else {
    if(found == 1) {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                       "will pause handler on SmarTest pause");
      handlerID->p.pauseHandler = 1;
    }
  }

  /* 
   * get reprobe bin number
   * following answers to queries from Esmo it appears the
   * the reprobe command could be set-up at the handler for any 
   * of the possible bins: this number should be set as a
   * configuration variable
   */
  confError = phConfConfIfDef(handlerID->myConf, PHKEY_RP_BIN);
  if(confError == PHCONF_ERR_OK) {
    confError = phConfConfNumber(handlerID->myConf, PHKEY_RP_BIN, 
                                 0, NULL, &dReprobeBinNumber);
  }
  if(confError == PHCONF_ERR_OK) {
    handlerID->p.reprobeBinDefined = 1; 
    handlerID->p.reprobeBinNumber = abs((int) dReprobeBinNumber);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "configuration \"%s\" set: will send BIN %d command upon receiving a reprobe",
                     PHKEY_RP_BIN,
                     handlerID->p.reprobeBinNumber);
  } else {
    handlerID->p.reprobeBinDefined = 0; 
    handlerID->p.reprobeBinNumber = 0; 
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "configuration \"%s\" not set: ignore reprobe command",
                     PHKEY_RP_BIN);
  }

  phConfConfStrTest(&found, handlerID->myConf, PHKEY_RP_AMODE,
                    "off", "all", "per-site", NULL);
  if( found != 1 && !handlerID->p.reprobeBinDefined ) {
    /* frame work may send reprobe command but a reprobe bin has not been defined */
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                     "a reprobe mode has been defined without its corresponding bin number\n"
                     "please check the configuration values of \"%s\" and \"%s\"",
                     PHKEY_RP_AMODE, PHKEY_RP_BIN);
    retVal = PHFUNC_ERR_CONFIG;
  }

  /* perform the communication intesive parts */
  if( firstTime ) {
    PhFuncTaCheck(getMachineID(handlerID, &machineID));
    PhFuncTaCheck(verifyModel(handlerID, machineID));
    PhFuncTaCheck(handlerSetup(handlerID));

    /* change equipment state */
    if(handlerID->p.stopped)
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
 * Authors: Zoyi Yu
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
  int i;
  phFuncError_t retVal = PHFUNC_ERR_OK;

  /* abort in case of unsuccessful retry */
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  /* do some really initial settings */
  handlerID->p.sites = handlerID->noOfSites;
  for(i=0; i<handlerID->p.sites; i++) {
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

  strcpy(handlerID->p.eol, "\n");

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
 * Authors: Zoyi Yu
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
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

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
 * Authors: Zoyi Yu
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
 * Authors: Zoyi Yu
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
 * Authors: Zoyi Yu
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
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  /* common to all models: identify */
  phFuncTaStart(handlerID);
  PhFuncTaCheck(phFuncTaSend(handlerID, "IDN?%s", handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", idString);
  if(retVal != PHFUNC_ERR_OK) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    return retVal;
  }

  phFuncTaStop(handlerID);

  /* create result string */
  sprintf(resultString, "%s", idString);

  /* strip of white space at the end of the string */
  while(strlen(resultString) > 0 && isspace(resultString[strlen(resultString)-1]))
    resultString[strlen(resultString)-1] = '\0';

  return retVal;
}
#endif



#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next device
 *
 * Authors: Zoyi Yu
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

  if (!handlerID->p.oredDevicePending)
  {
    retVal = waitForParts(handlerID);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveToMark(handlerID);
        return retVal;
    }
  }
  else
  {
      /* device has already got, do not need to poll */
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_0, "device has already got, do not need to poll");
  }

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
 * Authors: Zoyi Yu
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
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  /* get current site population */
  phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

  phFuncTaStart(handlerID);
  PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation, 
                              NULL, perSiteBinMap));
  phFuncTaStop(handlerID);

  /* modify site population, everything went fine, otherwise we
     would not reach this point */
  for(i=0; i<handlerID->noOfSites; i++) {
    if(handlerID->activeSites[i] && 
       (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
        oldPopulation[i] == PHESTATE_SITE_POPDEACT)) {
      population[i] = 
      oldPopulation[i] == PHESTATE_SITE_POPULATED ?
      PHESTATE_SITE_EMPTY : PHESTATE_SITE_DEACTIVATED;
    } else
      population[i] = oldPopulation[i]; 
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
 * Authors: Zoyi Yu
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
  phFuncError_t retVal = PHFUNC_ERR_OK;
  long perSiteReprobe[PHESTATE_MAX_SITES];
  long perSiteBinMap[PHESTATE_MAX_SITES];
  int i=0; 

  if( !handlerID->p.reprobeBinDefined ) {
    /* no reprobe bin number defined */
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                     "received reprobe command at site \"%s\" but no reprobe bin number defined \n"
                     "please check the configuration values of \"%s\" and \"%s\"",
                     handlerID->siteIds[i], PHKEY_RP_AMODE, PHKEY_RP_BIN);
    retVal=PHFUNC_ERR_BINNING;
  } else {
    /* prepare to reprobe everything */
    for(i=0; i<handlerID->noOfSites; i++) {
      perSiteReprobe[i] = 1L;
      perSiteBinMap[i] = handlerID->p.reprobeBinNumber;
    }
    retVal=privateBinReprobe(handlerID, perSiteReprobe, perSiteBinMap);
  }

  return retVal;
}
#endif



#ifdef BINREPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Zoyi Yu
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
  phFuncError_t retVal = PHFUNC_ERR_OK;

  phEstateSiteUsage_t *oldPopulation;
  int entries;
  phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
  int i;

  /* abort in case of unsuccessful retry */
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  /* remember which devices we expect now */
  for(i=0; i<handlerID->noOfSites; i++)
    handlerID->p.deviceExpected[i] = 
    (handlerID->activeSites[i] && perSiteReprobe[i]);

  /* get current site population */
  phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

  phFuncTaStart(handlerID);

  PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation, 
                              perSiteReprobe, perSiteBinMap));

  /* wait for reprobed parts */
  phFuncTaMarkStep(handlerID);
  PhFuncTaCheck(waitForParts(handlerID));

  /* do we have at least one part? If not, ask for the current
     status and return with waiting */
  if(!handlerID->p.oredDevicePending) {
    /* during the next call everything up to here should be
             repeated */
    phFuncTaRemoveToMark(handlerID);  

    /* change equipment state */
    if(handlerID->p.stopped)
      phEstateASetPauseInfo(handlerID->myEstate, 1);
    else
      phEstateASetPauseInfo(handlerID->myEstate, 0);

    return PHFUNC_ERR_WAITING;
  }

  phFuncTaStop(handlerID);
  phEstateASetPauseInfo(handlerID->myEstate, 0);

  /* we have received devices for test after a reprobe.
     Change the equipment specific state now but do not present any
     devices that were not existent before this reprobe action */
  handlerID->p.oredDevicePending = 0;
  for(i=0; i<handlerID->noOfSites; i++) {
    if(handlerID->activeSites[i] == 1) {
      /* there are different cases of possible situations before
         and after sending a reprobe/bin query to the handler:

         population    needs reprobe    new population    action

               empty         yes              full              delay, empty (new)
               full          yes              full              full (real reprobe)
               empty         yes              empty             d.c.
               full          yes              empty             empty, error
               empty         no               full              delay, empty (new)
         full          no               full              delay, empty (new)
         empty         no               empty             d.c.
               full          no               empty             empty
      */

      if(perSiteReprobe[i]) {
        if(handlerID->p.devicePending[i]) {
          if(oldPopulation[i] == PHESTATE_SITE_POPULATED ||
             oldPopulation[i] == PHESTATE_SITE_POPDEACT)
            handlerID->p.devicePending[i] = 0;
          else
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                             "new device at site \"%s\" will not be tested during reprobe action (delayed)",
                             handlerID->siteIds[i]);
          population[i] = oldPopulation[i];
        } else {
          if(oldPopulation[i] == PHESTATE_SITE_POPULATED ||
             oldPopulation[i] == PHESTATE_SITE_POPDEACT) {
            /* something is wrong here, we expected a reprobed
               device */
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "did not receive device that was scheduled for reprobe at site \"%s\"",
                             handlerID->siteIds[i]);
            population[i] = PHESTATE_SITE_EMPTY;
          } else
            population[i] = oldPopulation[i];
        }
      } else { /* ! perSiteReprobe[i] */
        switch(handlerID->model) {
          default:
            /* 
                         * following answers to queries from Esmo it appears
                         * the original assumption is correct:
                         * all devices need to receive a bin or reprobe data,
                         * and devices that were not reprobed must be gone 
                         */
            if(handlerID->p.devicePending[i]) {
              /* ignore any new devices */
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                               "new device at site \"%s\" will not be tested during reprobe action (delayed)",
                               handlerID->siteIds[i]);
            }
            population[i] = PHESTATE_SITE_EMPTY;
            if(oldPopulation[i] == PHESTATE_SITE_POPDEACT ||
               oldPopulation[i] == PHESTATE_SITE_DEACTIVATED)
              population[i] = PHESTATE_SITE_DEACTIVATED;
            break;
        }
      }
    } else {
      if(handlerID->p.devicePending[i]) {
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
  phEstateASetSiteInfo(handlerID->myEstate, population,handlerID->noOfSites);

  return retVal;
}
#endif



#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to handler plugin
 *
 * Authors: Zoyi Yu
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
}
#endif



#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to handler plugin
 *
 * Authors: Zoyi Yu
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
}
#endif



#ifdef DIAG_IMPLEMENTED
/*****************************************************************************
 *
 * retrieve diagnostic information
 *
 * Authors: Zoyi Yu
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
 * Authors: Zoyi Yu
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
  phFuncError_t retVal = PHFUNC_ERR_OK;

  /* don't change state counters */
  /* phFuncTaStart(handlerID); */
  switch(action) {
    case PHFUNC_SR_QUERY:
      /* we can handle this query globaly */
      if(lastCall)
        phFuncTaGetLastCall(handlerID, lastCall);
      break;
    case PHFUNC_SR_RESET:
      /* The incomplete function should reset and retried as soon as
               it is called again. In case last incomplete function is
               called again it should now start from the beginning */
      phFuncTaDoReset(handlerID);
      break;
    case PHFUNC_SR_HANDLED:
      /* in case last incomplete function is called again it should
               now start from the beginning */
      phFuncTaDoReset(handlerID);
      break;
    case PHFUNC_SR_ABORT:
      /* the incomplete function should abort as soon as it is
               called again */
      phFuncTaDoAbort(handlerID);
      break;
  }

  /* don't change state counters */
  /* phFuncTaStop(handlerID); */
  return retVal;
}
#endif



#ifdef UPDATE_IMPLEMENTED
/*****************************************************************************
 *
 * update the equipment state
 *
 * Authors: Zoyi Yu
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
 * Authors: Zoyi Yu
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

  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  phFuncTaStart(handlerID);
  retVal = getMachineID(handlerID, &machineID);
  phFuncTaStop(handlerID);

  /* return result, there is no real check for the model here (so far) */
  if(testPassed)
    *testPassed = (retVal == PHFUNC_ERR_OK) ? 1 : 0;
  return retVal;
}
#endif



#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * for a certain GetStatus query, get the corresponding GPIB command
 *
 * Authors: Zoyi Yu
 *
 * Description:
 *    all the GPIB commands are stored locally for each Esmo model, this
 *    function will retrieve the corresponding command with regard to a
 *    certain query. For example, 
 *    (1)for the query 
 *      "PROB_HND_CALL(ph_get_status testedDevicesNumber)" ,
 *      this function will return the GPIB command:    "TPS?"
 *    (2)for the query 
 *      "PROB_HND_CALL(ph_get_status lastPickPosition)" ,
 *      this function will return the GPIB command:    "PIPOS?"
 *
 * Return:
 *    function return SUCCEED if everything is OK, FAIL if error; and the found 
 *    GPIB command will be brought out via the parameter "pGpibCommand"
 *
 ***************************************************************************/
static int getGpibCommandForGetStatusQuery(
                                          phFuncId_t handlerID,
                                          char **pGpibCommand, 
                                          const char *token,
                                          const char *param
                                          )
{
  // Talos
  const char sTalosQryTransMatrix[5][10] = {"TPS\?", "PIPOS\?", "PLPOS\?", "TMP\?", "EOTH\?"};

  const int   MAX_INTERNAL_BUF_LEN     = 128;
  static char buffer[MAX_STATUS_MSG]   = "";
  char        internalQuery[MAX_INTERNAL_BUF_LEN] = "";
  int         i;

  // Clear the return cache
  *pGpibCommand = NULL;

  // Validation input query
  for(i=0; i < sizeof(sTalosQryTransMatrix)/sizeof(sTalosQryTransMatrix[0]); i++) {
    if(strcmp(sTalosQryTransMatrix[i], token) == 0) {
      strcpy(internalQuery, token);
      break;
    }
  }

  if(strlen(internalQuery) == 0) {
    strcpy(buffer, "The query of \"%s %s\" to handler model %s is not supported!");
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, param, handlerID->cmodelName);
    return FAIL;
  }

  // Concatenate command and parameter
  sprintf(buffer, "%s", internalQuery);
  *pGpibCommand = buffer;

  return SUCCEED;
}
#endif


#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The function for get status
 *
 * Authors: Zoyi Yu
 *
 * Description:
 *  use this function to retrieve the information/parameter 
 *  from handler.
 *
 ***************************************************************************/
phFuncError_t privateGetStatus(
                              phFuncId_t handlerID,       /* driver plugin ID */
                              const char *commandString,  /* the string of command, i.e. the key to
                                                             get the information from Handler */
                              const char *paramString,    /* the parameter for command string */
                              char **responseString       /* output of the response string */
                              )
{

  static char response[MAX_STATUS_MSG] = "";
  phFuncError_t retVal = PHFUNC_ERR_OK;
  const char *token = commandString;
  const char *param = paramString;
  char *gpibCommand = NULL;
  int found = FAIL;
  response[0] = '\0';
  *responseString = response;


  if( phFuncTaAskAbort(handlerID) ) return PHFUNC_ERR_ABORTED;

  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                   "privateGetStatus, token = ->%s<-, param = ->%s<-", token, param);

  found = getGpibCommandForGetStatusQuery(handlerID, &gpibCommand, token, param);
  if( found == SUCCEED ) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateGetStatus, gpibCommand = ->%s<-", gpibCommand);
    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", gpibCommand, handlerID->p.eol));
    localDelay_us(100000);
    retVal = phFuncTaReceive(handlerID, 1, "%s", response);
    if(retVal != PHFUNC_ERR_OK) {
      phFuncTaRemoveStep(handlerID);  /* force re-send of command */
      return retVal;
    }
  } else {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "The key \"%s\" is not available, or may not be supported yet", token);
    sprintf(response, "%s_KEY_NOT_AVAILABLE", token);
  }


  /* ensure null terminated */
  response[strlen(response)] = '\0';

  phFuncTaStop(handlerID);

  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                   "privateGetStatus, answer ->%s<-, length = %d",
                   response, strlen(response));

  return retVal;
}
#endif


#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Return the corresponding GPIB command for specified SetStatus query 
 *
 * Author: Roger Feng
 *
 * Description:
 *    all the GPIB commands are stored locally for each Esmo model, this
 *    function will retrieve the corresponding lower level handler command with 
 *    regard to a PROB_HND_CALL ph_set_status command.
 *
 * Input
 *  handlerID:  driver plugin ID
 *  token:      the string of the command in PROB_HND_CALL ph_set_status
 *  param:      the parameters for the command token
 
 * Output
 *  pGpibCommand: a pointer to the concatrated command
 *
 * Return:
 *    function return SUCCEED if everything is OK, FAIL otherwise; 
 *    The found GPIB command will be brought out via the parameter "pGpibCommand"
 *
 *
 ***************************************************************************/
static int getGpibCommandForSetStatusCommand(
                                            phFuncId_t handlerID,
                                            char **pGpibCommand, 
                                            const char *token,
                                            const char *param
                                            )
{
  // Special handle for ST!XX
  const char sTalosCmdTransMatrix[7][10] = {"TMP", "TMPM", "TADJ", "TMHU", "SHTDN", "RESTART", "EOCH"};

  static const phStringPair_t sTalos_CmdVsParamFormat[] = {
    // These static array must be ordered by the first field for binary search
    {"TADJ", "^[+-][0-9]+(\\.)*[0-9]*$"},
    {"TMP", "^[+-][0-9]+(\\.)*[0-9]*$"},
    {"TMPM", ".*"}
  };

  // Command usage message
  static const phStringPair_t sMT_CmdHelp[] = {
    // These static array must be ordered by the first field for binary search
    {"TADJ", "+123.4."},
    {"TMP", "+123.4."},
    {"TMPM", "1,XXX. XXX=amb,tmp,busy"},
  };

  const int   MAX_INTERNAL_BUF_LEN     = 128;
  static char buffer[MAX_STATUS_MSG]   = "";
  char        scratch[MAX_STATUS_MSG];
  char        internalCommand[MAX_INTERNAL_BUF_LEN] = {0};
  char        internalParam[MAX_INTERNAL_BUF_LEN] = {0};
  char        localParam[MAX_INTERNAL_BUF_LEN] = {0};
  const       phStringPair_t *pStrPair = NULL;
  const char* pMarker                  = NULL;
  const char* pLocalParam              = NULL;
  const char* pPattern                 = NULL;
  int         soakTime                 = -1;
  int         ret                      = -1;
  const unsigned int  SOAK_TIME_LIMIT  = 5994;
  int         i;

  // Clear the return cache
  *pGpibCommand = NULL;

  // Validation input command and parameters for TMP TMPM TADJ
  if ((strcasecmp(token, "TMP") == 0) ||
      (strcasecmp(token, "TMPM") == 0) ||
      (strcasecmp(token, "TADJ") == 0)) {
    pStrPair = phBinSearchStrValueByStrKey(sTalos_CmdVsParamFormat, LENGTH_OF_ARRAY(sTalos_CmdVsParamFormat), token);

    if(pStrPair != NULL) {
      pPattern = pStrPair->value;
      ret = phToolsMatch(param, pPattern);

      if(ret != 1) {
        pStrPair = phBinSearchStrValueByStrKey(sMT_CmdHelp, LENGTH_OF_ARRAY(sMT_CmdHelp), token);
        if(pStrPair != NULL) {
          strcpy(buffer, "The parameter \"%s\" for command \"%s\" is invalid!\n Usage: \"%s %s\"");
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, buffer, param, token, token, pStrPair->value);
          return FAIL;
        } else {
          strcpy(buffer, "Internal error:getGpibCommandForSetStatusCommand(): sMT_CmdHelp pair does not cover all the commands!\n");
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, buffer);
          return FAIL;
        }
      }
    } else {
      strcpy(buffer, "The command of \"%s\" to current handler model %s is not supported!");
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, handlerID->cmodelName);
      return FAIL;
    }
  }

  // Special handle for ST!XX
  if(phToolsMatch(token, "ST![0-9]+") == 1 ) {
    sscanf(token+3, "%d", &soakTime);
    if(soakTime < 0 ) {
      strcpy(buffer, "The parameter \"%s\" for command \"%s\" is invalid!\n Usage: %s XX. XX must be non-negative.");
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, buffer, param, token, token );
      return FAIL;
    }

    soakTime = soakTime/6; // sets time in unit of 1/10 minute(=6 sec).
    if(soakTime > SOAK_TIME_LIMIT) {
      strcpy(buffer, "The parameter \"%s\" for command \"%s\" is invalid!\n Usage: %s XX. XX=0~%d(sec).");
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, buffer, param, token, token, SOAK_TIME_LIMIT*6);
      return FAIL;
    }
    sprintf(internalCommand, "ST!%d", soakTime);
  }

  // Command translation. For instance:
  // "ph_set_status temperature_tmp sensor1 +80" -> "TMP 1,+80"
  // Get low level command

  // Validation input command
  switch(handlerID->model) {
    case PHFUNC_MOD_TALOS:
      for(i=0; i < sizeof(sTalosCmdTransMatrix) / sizeof(sTalosCmdTransMatrix[0]); i++) {
        if(strcmp(sTalosCmdTransMatrix[i], token) == 0) {
          strcpy(internalCommand, token);
          break;
        }
      }

      if(strlen(internalCommand) == 0) {
        strcpy(buffer, "The command of \"%s %s\" to handler model %s is not supported!");
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, param, handlerID->cmodelName);
        return FAIL;
      } else if ((strcasecmp(token, "TMP") == 0) ||
                (strcasecmp(token, "TMPM") == 0) ||
                (strcasecmp(token, "TADJ") == 0)) {
        strcat(internalCommand, " ");
      }
      break;
    default:
      strcpy(buffer, "The command of \"%s\" to handler model %s is not supported!");
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, handlerID->cmodelName);
      return FAIL;
      break;
  }

  // convert space delimiter to comma in parameters and append the command
  pLocalParam = param;
  if( strlen(pLocalParam) > 0) {
    sprintf(scratch, "%s", pLocalParam);

    pMarker = strtok(scratch, " \t[]");
    strcpy(buffer, "");

    while(pMarker != NULL) {
      strcat(buffer, pMarker);
      pMarker = strtok(NULL, " \t[]");
      if(pMarker != NULL) {
        strcat(buffer, ",");
      }
    }
    strcpy(internalParam, buffer);
  }

#ifdef DEBUG
  strcpy(buffer, "The command \"%s %s\" to handler model %s: internal command=%s,internal param=%s");
  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_0, buffer, token, param, handlerID->cmodelName, internalCommand, internalParam);
#endif

  // Concatenate command and parameter
  sprintf(buffer, "%s%s", internalCommand, internalParam);
  *pGpibCommand = buffer;

  return SUCCEED;
}
#endif

#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The function to set status of handler
 *
 * Authors: Zoyi Yu
 *
 * Description:
 *  use this function to set/config the information/parameter of handler.
 *
 ***************************************************************************/
phFuncError_t privateSetStatus(
                              phFuncId_t handlerID,       /* driver plugin ID */
                              const char *commandString,  /* the string of command, i.e. the key to
                                                             set the information to Handler */
                              const char *paramString    /* the parameter for command string */
                              )
{
  const char *token = commandString;
  const char *param = paramString;
  char *gpibCommand = NULL;
  int found = FAIL;
  int bRecieve = FAIL;
  int i;
  static char response[MAX_STATUS_MSG] = "";
  phFuncError_t retVal = PHFUNC_ERR_OK;
  const char sTalosCmdNoReplyMatrix[4][10] = {"TMHU", "SHTDN", "RESTART", "EOCH"};

  if( phFuncTaAskAbort(handlerID) ) return PHFUNC_ERR_ABORTED;

  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                   "privateSetStatus, token = ->%s<-, param = ->%s<-", token, param);

  found = getGpibCommandForSetStatusCommand(handlerID, &gpibCommand, token, param);

  if( found == SUCCEED ) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateSetStatus, gpibCommand = ->%s<-", gpibCommand);
    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", gpibCommand, handlerID->p.eol));
    localDelay_us(100000);
    bRecieve = SUCCEED;
    for(i=0; i < 4; i++) {
      if(strcmp(sTalosCmdNoReplyMatrix[i], token) == 0) {
        bRecieve = FAIL;
        break;
      }
    }

    if( bRecieve == SUCCEED ) {
      retVal = phFuncTaReceive(handlerID, 1, "%4095[^\r\n]", response);
      if (retVal != PHFUNC_ERR_OK)
      {
          phFuncTaRemoveStep(handlerID);
          return retVal;
      }
    }
  } else {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "The command \"%s %s\" is invalid or not supported yet", token, param);
  }

  phFuncTaStop(handlerID);

  return PHFUNC_ERR_OK;
}
#endif

#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Zoyi Yu
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

#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot end signal from handler
 *
 * Authors: Zoyi Yu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateLotStart(
                              phFuncId_t handlerID,      /* driver plugin ID */
                              int timeoutFlag
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "Entering privateLotStart, timeoutFlag = %s\n", timeoutFlag ? "TRUE" : "FALSE");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) ) return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);
    if ( (timeoutFlag == 0) && ( handlerID->interfaceType == PHFUNC_IF_GPIB ) ) {
        // Make this look like a repeated call, so the abort will work
        phFuncTaSetCall(handlerID, PHFUNC_AV_LOT_START);
        // Same as ph_hfunc.c calling:
        // RememberCall(handlerID, PHFUNC_AV_LOT_START);
    }

    retVal = waitForLotStart(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateLotStart, after waitForLotStart, retVal = %d\n", retVal);

    while ( (timeoutFlag == 0) && (retVal != PHFUNC_ERR_OK) ) {
        if ( phFuncTaAskAbort(handlerID) )
        {
            phFuncTaRemoveToMark(handlerID);
            return PHFUNC_ERR_ABORTED;
        }

        retVal = waitForLotStart(handlerID);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "privateLotStart, after repeating waitForLotStart, retVal = %d\n", retVal);
    }

    phFuncTaStop(handlerID);

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "exiting privateLotStart, retVal = %d\n", retVal);
    return retVal;

}
#endif

#ifdef LOTDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Cleanup after lot end
 *
 * Authors: Zoyi Yu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateLotDone(
                              phFuncId_t handlerID,      /* driver plugin ID */
                              int timeoutFlag
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "Entering privateLotDone\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) ) return PHFUNC_ERR_ABORTED;

    phFuncTaMarkStep(handlerID);
    // flush any extra pending SRQs
    // privateGetStart() got the lot_done SRQ
    retVal = flushSRQs(handlerID);

    if ( retVal == PHFUNC_ERR_OK )
    {
        phFuncTaStop(handlerID);
    }
    else
    {
        phFuncTaRemoveToMark(handlerID);
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "exiting privateLotDone, retVal = %d\n", retVal);
    return retVal;
}
#endif

/*****************************************************************************
 * End of file
 *****************************************************************************/
