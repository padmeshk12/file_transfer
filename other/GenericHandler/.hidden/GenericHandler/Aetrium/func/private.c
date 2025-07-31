/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 26 Jan 2000
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *            09 Jun 2000, Michael Vogt, adapted from prober driver
 *            02 Aug 2000, Michael Vogt, worked in all changes based on test
 *                         at Delta Design
 *            06 Sep 2000, Michael Vogt, adapted from Delta plug-in
 *            12 Sep 2000, Chris Joyce, modify according to answers to queries
 *                         received from Multitest 
 *            20 Mar 2001, Chris Joyce, added CommTest and split init into
 *                         init + comm + config
 *             9 Apr 2001, Siegfried Appich, changed debug_level from LOG_NORMAL
 *                         to LOG_DEBUG
 *             24 May 2006, Xiaofei Han, added processing code for MT99XX which 
 *                         supports octal mode (8-site) testing
 *            21 Dec 2006, Kun Xiao, CR33707
 *              support query commands for Multitest handler;
 *              introduce the stub function "privateGetStatus" as the
 *              universal interface to query all the kinds of information
 *              from Handler; those commands like temperature query (setpoint,
 *              soaktime, tempcontrol, etc.), set point and so on.
 *            Aug 2008, Jiawei Lin
 *              Please note that the CR33707 (above) is not implemented completely!!!
 *              the current Multitest plugin does not support the "ph_get_status" query
 *            Mar 2009, Roger Feng
 *              Enable Multitest 9918, 9928 to use ph_get_status/ph_set_status for
 *              temperature related query/control.
 *            Oct 2009, Roger Feng
 *              Enable Multitest 9510.
 * 
 *            Nov 2011, Jiawei Lin
 *              Implement the InFlip handler driver; introduce the privateLotStart/privateLotDone functions
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
#include  "ph_GuiServer.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_hfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "temperature.h"
#include "ph_hfunc_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/* this macro allows to test a not fully implemented plugin */
/* #define ALLOW_INCOMPLETE */
#undef ALLOW_INCOMPLETE

/* 
 *  following answers to queries from Multitest it appears the 
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

static phFuncError_t completeHandshake(phFuncId_t handlerID)
{

    phFuncError_t retVal = PHFUNC_ERR_OK;
    if(handlerID->p.handShake)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "select Port 4 for complete handshake");
        PhFuncTaCheck(phFuncTaSend(handlerID, "P4X%s", handlerID->p.eol));
        handlerID->p.handShake = 0;
    }
    static char handlerAnswer[1024] = "";
    do {
        PhFuncTaCheck(phFuncTaSend(handlerID, "R0X%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        if(retVal != PHFUNC_ERR_OK)
        {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
    } while(handlerAnswer[0] != '0' && handlerAnswer[1] != '0');

    handlerID->p.pollHandler = 1;
    return retVal;
}


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

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID, char pollResponse[1024])
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  static char handlerAnswer[1024] = "";
  long population = 0L;
  long populationPicked = 0L;
  int i;

  switch(handlerID->model) {
    case PHFUNC_MOD_55v8:
      if(handlerID->p.pollHandler)
      {
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                     "select Port 4 poll handler for start of test");
          PhFuncTaCheck(phFuncTaSend(handlerID, "P4X%s", handlerID->p.eol));
          handlerID->p.pollHandler = 0;
      }
      PhFuncTaCheck(phFuncTaSend(handlerID, "R0X%s", handlerID->p.eol));

      retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
      if(retVal != PHFUNC_ERR_OK)
      {
          phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
          return retVal;
      }
      /* remove the invalid bits */
      if (handlerAnswer[1] < 'A')
      {
          population = handlerAnswer[1] & 0x0f;
      }
      switch(handlerAnswer[1])
      {
          case 'A':
              population = 0x02 | 0x0A;
              break;
          case 'B':
              population = 0x03 | 0x0B;
              break;
          case 'C':
              population = 0x04 | 0x0C;
              break;
          case 'D':
              population = 0x05 | 0x0D;
              break;
          case 'E':
              population = 0x06 | 0x0E;
              break;
          case 'F':
              population = 0x07 | 0x0F;
              break;
          default:
              break;
      }
      break;

    default: 
      break;

  }

  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "handlerAnswer and site number: \"%s\", \"%d\"", handlerAnswer, handlerID->noOfSites);

  if(handlerID->p.masterSot)
  {
      if(handlerAnswer[0] == '0')
      {
          if(handlerAnswer[1] == '0' || handlerAnswer[1] <= 'F')
          {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                               "site population not received from handler: \"%s\", trying again", handlerAnswer);
              phFuncTaRemoveStep(handlerID);
              return PHFUNC_ERR_WAITING;
          }
      }
      else if(handlerAnswer[0] == '1' && handlerAnswer[1] == '0')
      {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                           "site population not received from handler: \"%s\", trying again", handlerAnswer);
          phFuncTaRemoveStep(handlerID);
          return PHFUNC_ERR_WAITING;
      }
  }
  else
  {
      if(handlerAnswer[0] == '0')
      {
          if(handlerAnswer[1] == '0')
          {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                               "site population not received from handler: \"%s\", trying again", handlerAnswer);
              phFuncTaRemoveStep(handlerID);
              return PHFUNC_ERR_WAITING;
          }
      }
  }

  /* correct pending devices information, overwrite all (trust the
     last handler query) */
  handlerID->p.oredDevicePending = 0;
  for(i=0; i<handlerID->noOfSites; i++) {
    if(population & (1L << i)) {
      handlerID->p.devicePending[i] = 1;
      handlerID->p.oredDevicePending = 1;
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "device present at site \"%s\" (polled)", 
                       handlerID->siteIds[i]);

      populationPicked |= (1L << i);
      if (population == 1)
      {
          handlerID->p.sitea = 1;
      }
      if (population == 2)
      {
          handlerID->p.siteb = 1;
      }
      if (population == 4)
      {
          handlerID->p.sitec = 1;
      }
      if (population == 8)
      {
          handlerID->p.sited = 1;
      }
      if (population == 3)
      {
          handlerID->p.sitea = 1;
          handlerID->p.siteb = 1;
      }
      if (population == 5)
      {
          handlerID->p.sitea = 1;
          handlerID->p.sitec = 1;
      }
      if (population == 6)
      {
          handlerID->p.siteb = 1;
          handlerID->p.sitec = 1;
      }
      if (population == 9)
      {
          handlerID->p.sitea = 1;
          handlerID->p.sited = 1;
      }
      if (population == 12)
      {
          handlerID->p.sitec = 1;
          handlerID->p.sited = 1;
      }
      if (population == 10)
      {
          handlerID->p.siteb = 1;
          handlerID->p.sited = 1;
      }
      if (population == 7)
      {
          handlerID->p.sitea = 1;
          handlerID->p.siteb = 1;
          handlerID->p.sitec = 1;
      }
      if (population == 11)
      {
          handlerID->p.sitea = 1;
          handlerID->p.siteb = 1;
          handlerID->p.sited = 1;
      }
      if (population == 13)
      {
          handlerID->p.sitea = 1;
          handlerID->p.sitec = 1;
          handlerID->p.sited = 1;
      }
      if (population == 14)
      {
          handlerID->p.siteb = 1;
          handlerID->p.sitec = 1;
          handlerID->p.sited = 1;
      }
      if (population == 15)
      {
          handlerID->p.sitea = 1;
          handlerID->p.siteb = 1;
          handlerID->p.sitec = 1;
          handlerID->p.sited = 1;
      }
    } else {
      handlerID->p.devicePending[i] = 0;
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "no device at site \"%s\" (polled)", 
                       handlerID->siteIds[i]);
    }
  }

  /* check whether we have received to many parts */
  if(population != populationPicked) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                     "The handler seems to present more devices than configured\n"
                     "The driver configuration must be changed to support more sites");
    retVal = PHFUNC_ERR_FATAL;
  }
  strcpy(pollResponse, handlerAnswer);
  return retVal;
}


/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
  struct timeval pollStartTime;
  struct timeval pollStopTime;
  int timeout;
  char handlerAnswer[1024];

  phFuncError_t retVal = PHFUNC_ERR_OK;

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering waitForParts");
 
  if(handlerID->p.strictPolling) {
    /* apply strict polling loop, ask for site population */
    gettimeofday(&pollStartTime, NULL);

    timeout = 0;
    localDelay_us(handlerID->p.pollingInterval);
    do {
      PhFuncTaCheck(pollParts(handlerID, handlerAnswer));
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

    if(handlerID->p.masterSot)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering handlerAnswer");
        PhFuncTaCheck(phFuncTaSend(handlerID, "P3X%s", handlerID->p.eol));
        if(handlerAnswer[1] == 'F')
        {
            PhFuncTaCheck(phFuncTaSend(handlerID, "D0FZX%s", handlerID->p.eol));
        }
        else
        {
            handlerAnswer[0] = '0';
            PhFuncTaCheck(phFuncTaSend(handlerID, "D%sZX%s", handlerAnswer, handlerID->p.eol));
        }
    }
    else if(!handlerID->p.masterSot)
    {
        if(handlerAnswer[1] == 'F')
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering handlerAnswer");
            PhFuncTaCheck(phFuncTaSend(handlerID, "P3X%s", handlerID->p.eol));
            PhFuncTaCheck(phFuncTaSend(handlerID, "D0FZX%s", handlerID->p.eol));
        }
        else
        {
            gettimeofday(&pollStartTime, NULL);

            timeout = 0;
            localDelay_us(handlerID->p.pollingInterval);
            do{
                gettimeofday(&pollStopTime, NULL);
                if((pollStopTime.tv_sec - pollStartTime.tv_sec)*1000000L +
                           (pollStopTime.tv_usec - pollStartTime.tv_usec) >
                           handlerID->heartbeatTimeout)
                {
                    timeout = 1;
                }
                if(timeout)
                {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering handlerAnswer");
                    PhFuncTaCheck(phFuncTaSend(handlerID, "P3X%s", handlerID->p.eol));
                    handlerAnswer[0] = '0';
                    PhFuncTaCheck(phFuncTaSend(handlerID, "D%sZX%s", handlerAnswer, handlerID->p.eol));
                }
            }while(!timeout);
        }
    }
  }
  handlerID->p.handShake = 1;
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

  char binNumber[2048];
  int i;
  int sendBinning = 0;

  switch(handlerID->model) {
    case PHFUNC_MOD_55v8:
      /* 
       *  following answers to queries from Aetrium
       *  recommended to send all binning in one go
       */
      for(i=0; i<handlerID->noOfSites; i++) {
        if(handlerID->activeSites[i] && 
           (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
            oldPopulation[i] == PHESTATE_SITE_POPDEACT))
        {
            /* 
             *  following answers to queries from Aetrium
             *  maximum bin category is 15
             */
            if(perSiteBinMap[i] < 0 || perSiteBinMap[i] > 15) {
              /* illegal bin code */
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                               "received illegal request for bin %ld at site \"%s\"",
                               perSiteBinMap[i], handlerID->siteIds[i]);
              retVal =  PHFUNC_ERR_BINNING;
            } else {
              /* bin this one */
              sprintf(binNumber, "%lx",perSiteBinMap[i]);
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                               "will bin device at site \"%s\" to %x",
                               handlerID->siteIds[i], perSiteBinMap[i]);
              binNumber[0] = toupper(binNumber[0]);
              sendBinning = 1;
              if (handlerID->p.sitea == 1)
              {
                  strcat(handlerID->p.bina, binNumber);
                  handlerID->p.sitea = 0;
                  continue;
              }
              if (handlerID->p.siteb == 1)
              {
                  strcat(handlerID->p.binb, binNumber);
                  handlerID->p.siteb = 0;
                  continue;
              }
              if (handlerID->p.sitec == 1)
              {
                  strcat(handlerID->p.binc, binNumber);
                  handlerID->p.sitec = 0;
                  continue;
              }
              if (handlerID->p.sited == 1)
              {
                  strcat(handlerID->p.bind, binNumber);
                  handlerID->p.sited = 0;
                  continue;
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

  if( retVal==PHFUNC_ERR_OK && sendBinning)
  {

      if (strcmp(handlerID->p.binb, "") != 0 || strcmp(handlerID->p.bina, "") != 0)
      {
          PhFuncTaCheck(phFuncTaSend(handlerID, "P1X%s", handlerID->p.eol));
          PhFuncTaCheck(phFuncTaSend(handlerID, "D%s%sZX%s", ((strcmp(handlerID->p.binb, "") == 0)? "0" : handlerID->p.binb) ,
                        ((strcmp(handlerID->p.bina, "") == 0)? "0" : handlerID->p.bina), handlerID->p.eol));
      }
      if (strcmp(handlerID->p.bind, "") != 0 || strcmp(handlerID->p.binc, "") != 0)
      {
          PhFuncTaCheck(phFuncTaSend(handlerID, "P2X%s", handlerID->p.eol));
          PhFuncTaCheck(phFuncTaSend(handlerID, "D%s%sZX%s", ((strcmp(handlerID->p.bind, "") == 0)? "0" : handlerID->p.bind) ,
              ((strcmp(handlerID->p.binc, "") == 0)? "0" : handlerID->p.binc), handlerID->p.eol));
      }
      PhFuncTaCheck(phFuncTaSend(handlerID, "P3X%s", handlerID->p.eol));
      PhFuncTaCheck(phFuncTaSend(handlerID, "D00ZX%s", handlerID->p.eol));
  }


  return retVal;
}

/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  int found = 0;
  phConfError_t confError;
  double dPollingInterval;
  static int firstTime=1;

  /* chose polling or SRQ interrupt mode */
  phConfConfStrTest(&found, handlerID->myConf, PHKEY_FC_WFPMODE, 
                    "polling", "interrupt", NULL);
  if(found == 1) {
    /* default 55v8 support polling */
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "activated strict polling mode, no SRQ handling");
    handlerID->p.strictPolling = 1;
  } else {
    /* support SRQ interrupt */
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

  /* retrieve master sot */
  confError = phConfConfStrTest(&found, handlerID->myConf,
                                  PHKEY_FC_MASTER_SOT, "yes", "no", NULL);
  if(confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
    retVal = PHFUNC_ERR_CONFIG;
  else {
    if(found == 1) {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                       "the master sot is set to yes");
      handlerID->p.masterSot = 1;
    }
  }
  /* perform the communication intensive parts */
  if( firstTime ) {
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
 * Authors: Michael Vogt
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

  handlerID->p.strictPolling = 0;
  handlerID->p.pollingInterval = 200000L;
  handlerID->p.oredDevicePending = 0;
  handlerID->p.stopped = 0;
  handlerID->p.sitea = 0;
  handlerID->p.siteb = 0;
  handlerID->p.sitec = 0;
  handlerID->p.sited = 0;
  strcpy(handlerID->p.bina,"");
  strcpy(handlerID->p.binb,"");
  strcpy(handlerID->p.binc,"");
  strcpy(handlerID->p.bind,"");

  //instresting vendor: they use special EndOfLine for strip handler!
  if( handlerID->model == PHFUNC_MOD_55v8 ) {
      handlerID->p.pollHandler = 1;
      handlerID->p.handShake = 1;
      strcpy(handlerID->p.eol, "\r\n");
      PhFuncTaCheck(phFuncTaSend(handlerID, "C3G1X%s", handlerID->p.eol));
  }

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
 * Authors: Michael Vogt
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
 * Authors: Michael Vogt
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
 * Authors: Michael Vogt
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
 * Authors: Michael Vogt
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
  return PHFUNC_ERR_OK;
}
#endif



#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next device
 *
 * Authors: Michael Vogt
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

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateGetStart");

  /* abort in case of unsuccessful retry */
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  /* remember which devices we expect now */
  for(i=0; i<handlerID->noOfSites; i++)
    handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

  phFuncTaStart(handlerID);

  phFuncTaMarkStep(handlerID);
  retVal = waitForParts(handlerID);
  
    if (retVal == PHFUNC_ERR_JAM) {
        phFuncTaRemoveToMark(handlerID);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "Exiting privateGetStart, PHFUNC_ERR_JAM\n");
        return PHFUNC_ERR_JAM;
    }
    else if (retVal == PHFUNC_ERR_LOT_DONE) {
        phFuncTaRemoveToMark(handlerID);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "Exiting privateGetStart, PHFUNC_ERR_LOT_DONE\n");
        return PHFUNC_ERR_LOT_DONE;
    }
    else if (retVal == PHFUNC_ERR_FATAL )
    {
        return PHFUNC_ERR_FATAL;
    }
    else if (retVal != PHFUNC_ERR_OK)
    {
        phFuncTaRemoveToMark(handlerID);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "Exiting privateGetStart, unknown error: retVal = %d\n", retVal);
        return PHFUNC_ERR_WAITING;
    }

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

  /* we have received devices for test. Change the equipment
     specific state now */
  handlerID->p.oredDevicePending = 0;
  for(i=0; i<handlerID->noOfSites; i++) {
    if(handlerID->activeSites[i] == 1) {
      if(handlerID->p.devicePending[i]) {
        handlerID->p.devicePending[i] = 0;
        population[i] = PHESTATE_SITE_POPULATED;
      } else
        population[i] = PHESTATE_SITE_EMPTY;
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
  phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);

  return retVal;
}
#endif



#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested device
 *
 * Authors: Michael Vogt
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
  strcpy(handlerID->p.bina,"");
  strcpy(handlerID->p.binb,"");
  strcpy(handlerID->p.binc,"");
  strcpy(handlerID->p.bind,"");

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

  phFuncTaStart(handlerID);
  PhFuncTaCheck(completeHandshake(handlerID));
  phFuncTaStop(handlerID);

  return retVal;
}
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Michael Vogt
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
  return PHFUNC_ERR_OK;
}
#endif



#ifdef BINREPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Michael Vogt
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
  return PHFUNC_ERR_OK;
}
#endif



#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to handler plugin
 *
 * Authors: Michael Vogt
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
    return PHFUNC_ERR_OK;
}
#endif



#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to handler plugin
 *
 * Authors: Michael Vogt
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
    return PHFUNC_ERR_OK;
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
 * Authors: Michael Vogt
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
  return PHFUNC_ERR_OK;
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
 * Authors: Michael Vogt
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

  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  phFuncTaStart(handlerID);
  phFuncTaStop(handlerID);

  /* return result, there is no real check for the model here (so far) */
  if(testPassed)
    *testPassed = 1;
  return retVal;
}
#endif


#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The function for get status
 *
 * Authors: Kun Xiao
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
    return PHFUNC_ERR_OK;
}
#endif


#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The function to set status of handler
 *
 * Authors: Roger Feng
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
    return PHFUNC_ERR_OK;
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
 * Authors: Jiawei Lin
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
    return PHFUNC_ERR_OK;

}
#endif


#ifdef LOTDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Cleanup after lot end
 *
 * Authors: Jiawei Lin
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
    return PHFUNC_ERR_OK;
}
#endif

/*****************************************************************************
 * End of file
 *****************************************************************************/
