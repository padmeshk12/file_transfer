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
/*Begin of Huatek Modifications, Luke Lan, 02/21/2001  */
/*Issue Number: 20    */
/*error: errno not define.  So add include file errno.h */
#include <errno.h>
/*End of Huatek Modifications.  */

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

static phFuncError_t doPauseHandler(phFuncId_t handlerID);
static phFuncError_t doUnpauseHandler(phFuncId_t handlerID);


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

static phFuncError_t extractContactSiteForInFlip(phFuncId_t handlerID, const char *handlerAnswer)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  /*
   * analyze the site contact value 
   *  
   * the contact site value fomrate: 
   *  "strip_ID;Contact_Site_number,row,col;Contact_Site_number,row,col;...
   *  
   *  Example 1:   "mStripID1;1,9,29;2,8,29;3,7,29;4,6,29"  (site 1,2,3,4 have devices)
   *  Example 2:   "mStripID2;1,1,1;2,2,1;9,9,1"           (site 1,2,9 have devices; 3-8 no device)
   *  
   *  Note: there might be 72 site in parral!
   */

  char *ppArray[handlerID->noOfSites+1];
  int length = 0;

  if(phSplitStringByDelimiter(handlerAnswer, ';', ppArray, handlerID->noOfSites+1, &length) == 0 ) {

    //loop from 1 because the first one is the strip ID
    int i = 1;
    char strSiteNum[8];
    for(; i<length; i++ ) {
      //The following line only works for sites 1-9 because it only handles one numeric character
      //BUG   int siteNumber = ppArray[i][0] - 48; //convert the "1" to 1, i.e Site 1
      int j=0;
      while( (ppArray[i][j] != ',') && (ppArray[i][j] != '\0') && (j < 8) ){
         strSiteNum[j] = ppArray[i][j];
         j++;
      }
      int siteNumber = atoi(strSiteNum);        //convert the first part of xx,xx,xx to site number

      //site 1 is stored in the position of 0!
      siteNumber--;
      handlerID->p.devicePending[siteNumber] = 1;
      handlerID->p.oredDevicePending = 1;

      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "new device present at site \"%s\" (SRQ)", 
                       handlerID->siteIds[siteNumber]);                      
    }                  
  } else {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                     "The format of Contact Site Value is invalid; unable to parse it\n");
  }

  //FREE THE MEMORY USED BY phSplitStringByDelimiter
  int k = 0;
  for(k=0; k < length; k++ ) {
    free(ppArray[k]);
  }


  return retVal;
}
static phFuncError_t extractContactSiteForMT99XX(phFuncId_t handlerID, const char* handlerAnswer, long * population)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    *population = 0;
    int contactedSitesNumber = 0;
    int iLoop = 0;
    int iCtrl = strlen(handlerAnswer);
    int iCopy = 0;
    /* caculate the number by the handlerAnswer */
    for(iLoop = 0; iLoop < iCtrl; iLoop++)
    {
        (*population) |= (handlerAnswer[iLoop] & 0x0f) << (4*iLoop);
    }

    /* copy the result for the next caculation */
    iCopy = *population;

    /* accumulate the site number by count the 1 exist in binary format */
    while(0 != iCopy)
    {
        iCopy &= iCopy-1;
        contactedSitesNumber++;
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Contacted sites number is: %d", contactedSitesNumber);
    /* validate the site number */
    if(contactedSitesNumber > handlerID->p.sites)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                "The handler seems to present more devices than configured\n"
                "The driver configuration must be changed to support more sites");
        retVal = PHFUNC_ERR_ANSWER;
    }
    return retVal;
}

static void setStrUpper(char *s){

  if (s == NULL) {
    return;
  }

  int i = 0;
  while (s[i]) {
    s[i] = toupper(s[i]);
    i++;
  }
}

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  static char handlerAnswer[1024] = "";
  long population = 0L, populationOctal = 0L;
  long populationPicked = 0L;
  int i;

  switch(handlerID->model) {
    case PHFUNC_MOD_MT93XX:
      PhFuncTaCheck(phFuncTaSend(handlerID, "SQB?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
      retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
      if(retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
      }
      /* remove the invalid bits */
      population = handlerAnswer[0] & 0x0f;

      break;
    case PHFUNC_MOD_MT9510:
      /* 
             *  following answers to queries from Multitest it appears the 
             *  MT9510 also supports polling
             */
      if(handlerID->noOfSites <= 4){

      PhFuncTaCheck(phFuncTaSend(handlerID, "SQB?%s", handlerID->p.eol));
      retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
      if(retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
      }
      /* remove the invalid bits */
      population = handlerAnswer[0] & 0x0f;
    } else {

      /*
       * if site number > 4, send FULLSITES to query the site population information
       * FULLISITES? => Fullsites 0000ZYXW
       * W-coded contact sites 4...1(binary)
       * X-coded contact sites 8...5(binary)
       * Y-coded contact sites 12...9(binary)
       * Z-coded contact sites 16...13(binary)
       */
      PhFuncTaCheck(phFuncTaSend(handlerID, "FULLSITES?%s", handlerID->p.eol));
      retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
      if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
      }

      setStrUpper(handlerAnswer);
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                  "The handler answer is %s",
                  handlerAnswer);

      if (sscanf(handlerAnswer, "FULLSITES %08lx", &population) != 1) {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
            "site population not received from handler: received \"%s\", format expected \"FULLSITES xxxxxxxx\"\n",
            handlerAnswer);
        handlerID->p.oredDevicePending = 0;
        return PHFUNC_ERR_ANSWER;
      }

      }

      break;
    case PHFUNC_MOD_MT99XX:
      if(handlerID->noOfSites <= 4) {
        /* if site number <=4, send SQB to query the site population information */
        PhFuncTaCheck(phFuncTaSend(handlerID, "SQB?%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        if(retVal != PHFUNC_ERR_OK) {
          phFuncTaRemoveStep(handlerID);
          return retVal;
        }
        /* remove the invalid bits */
        population = handlerAnswer[0] & 0x0f;
      } else {
        /* if site number >4, send SITES to query the site population information */
        PhFuncTaCheck(phFuncTaSend(handlerID, "SITES?%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        if(retVal != PHFUNC_ERR_OK) {
          phFuncTaRemoveStep(handlerID);
          return retVal;
        }
        /* extract the contacted site population information from the handler answer */
        if(PHFUNC_ERR_OK !=(retVal = extractContactSiteForMT99XX(handlerID,handlerAnswer,&population)))
        {
            return retVal;
        }
      }
      break;
    default: 
      break;

  }

  /* check answer from handler, similar for all handlers.  remove
     two steps (the query and the answer) if an empty string was
     received */
  if(handlerID->noOfSites <=4 ) {
    if(strlen(handlerAnswer) != 1 || handlerAnswer[0] < '0' || handlerAnswer[0] > '?')
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                       "site population not received from handler: \"%s\", trying again", handlerAnswer);
      phFuncTaRemoveStep(handlerID);
      phFuncTaRemoveStep(handlerID);
      return PHFUNC_ERR_WAITING;
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
  /* End CR12663 */

  /* check whether we have received to many parts */
  if(population != populationPicked) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                     "The handler seems to present more devices than configured\n"
                     "The driver configuration must be changed to support more sites");
  }

  return retVal;
}



/* wait for lot start from handler */
static phFuncError_t waitForLotStart(phFuncId_t handlerID)
{
  char handlerAnswer[1024] = "";
  phFuncError_t retVal = PHFUNC_ERR_OK;

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering waitForLotStart: look for SRQ from handler\n");

  switch( handlerID->model ) {
    //Note that all the MT9xxx driver SHALL NOT call the ph_lot_start, but still safe if the user wants
    case PHFUNC_MOD_MT9510:
    case PHFUNC_MOD_MT93XX:        
    case PHFUNC_MOD_MT99XX:
      retVal = PHFUNC_ERR_OK;
      break;

    case PHFUNC_MOD_INFLIP:
        phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "new Lot Ready for test");
        PhFuncTaCheck(phFuncTaSend(handlerID, "LOTID?%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        if(retVal != PHFUNC_ERR_OK) {
          phFuncTaRemoveStep(handlerID);
          return retVal;
        }
        phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "the Lot ID is %s", handlerAnswer);
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
  int populationMask = 0;
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
 
    if(handlerID->model == PHFUNC_MOD_MT9510 || handlerID->model == PHFUNC_MOD_MT93XX || (handlerID->model == PHFUNC_MOD_MT99XX && handlerID->noOfSites <= 4)) {
      populationMask = 0x0f;
    } else if(handlerID->model == PHFUNC_MOD_MT99XX && handlerID->noOfSites > 4) {
      populationMask = 0xff;
    }

    if(received) {
      switch(handlerID->model) {
        case PHFUNC_MOD_MT99XX:
          break;
        case PHFUNC_MOD_MT9510:
        case PHFUNC_MOD_MT93XX:
          /* extend MT9510 x16 */
          if(handlerID->model == PHFUNC_MOD_MT9510&&handlerID->noOfSites > 4){
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "The site number is %d,try to polling...", handlerID->noOfSites);
            PhFuncTaCheck(pollParts(handlerID));
            break;
          }
          if(srq & populationMask) {
            /* there may be parts given in the SRQ byte */
            partMissing = 0;
            for(i=0; i<handlerID->noOfSites; i++) {
              if(srq & (1 << i)) {
                handlerID->p.devicePending[i] = 1;
                handlerID->p.oredDevicePending = 1;
                if(handlerID->noOfSites <= 4)
                  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                   "new device present at site \"%s\" (SRQ)", 
                                   handlerID->siteIds[i]);
                else
                  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                   "new device present at site \"%s\" (SITES)", 
                                   handlerID->siteIds[i]);
                srqPicked |= (1 << i);
              } else if(handlerID->p.deviceExpected[i])
                partMissing = 1;
            }

            /* check whether some parts seem to be missing. In
               case of a reprobe, we must at least receive the
               reprobed devices. In all other cases we should
               receive devices in all active sites. In case
               something seems to be missing, we double check:
               flush all remaining SRQs and than poll for the
               current population */

            if(partMissing) {
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                               "some expected device(s) seem(s) to be missing, polling once to be sure");
              PhFuncTaCheck(pollParts(handlerID));
              PhFuncTaCheck(flushSRQs(handlerID));
            }

            /* check whether we have received to many parts */

            if((srq & populationMask) != srqPicked) {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                               "The handler seems to present more devices than configured\n"
                               "The driver configuration must be changed to support more sites");
            }
          } else {
            /* some exceptional SRQ occured */
            /* do nothing, status is analyzed later */
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
                             "received exceptional SRQ 0x%02x", srq);
          }

          break;

        case PHFUNC_MOD_INFLIP:
          if( (srq&0x42) == 0x42 ) {
            //receive the SRQ x1xxxx1x, means "lot done"!
            phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "Lot completed");
            retVal = PHFUNC_ERR_LOT_DONE;
          } else if( srq  == 0xd1 ) {
            phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "contacted site is ready for test");
            PhFuncTaCheck(phFuncTaSend(handlerID, "CS?%s", handlerID->p.eol));
            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            if(retVal != PHFUNC_ERR_OK) {
              phFuncTaRemoveStep(handlerID);
              return retVal;
            }
            phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "the strip ID is %s", handlerAnswer);

            phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "query the GAA?");
            PhFuncTaCheck(phFuncTaSend(handlerID, "GAA?%s", handlerID->p.eol));
            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            if(retVal != PHFUNC_ERR_OK) {
              phFuncTaRemoveStep(handlerID);
              return retVal;
            }
            phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "the response of GAA? query %s", handlerAnswer);

            phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "get the value of Contact Site");
            PhFuncTaCheck(phFuncTaSend(handlerID, "CS1?%s", handlerID->p.eol));
            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            if(retVal != PHFUNC_ERR_OK) {
              phFuncTaRemoveStep(handlerID);
              return retVal;
            }
            phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "the value of Contact Site is %s", handlerAnswer);
            extractContactSiteForInFlip(handlerID, handlerAnswer);                  
          } else if( srq == 0x4d || srq == 0x41 || srq == 0x40 || srq == 0x4c ) {
            phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "No ready for test, waiting to finish the action");
            int newSrq = 0;
            int newReceived = 0;
            while(1) {
              newReceived = 0;              
              newSrq = 0;
              PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &newReceived, &newSrq, 0));
              if( newReceived ) {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "get second SRQ 0x%02x in waitForParts", srq);
                if ( newSrq ==  0xd1 ) {
                  phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "contacted site is ready for test");
                  PhFuncTaCheck(phFuncTaSend(handlerID, "CS?%s", handlerID->p.eol));
                  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
                  if(retVal != PHFUNC_ERR_OK) {
                    phFuncTaRemoveStep(handlerID);
                    return retVal;
                  }
                  phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "the strip ID is %s", handlerAnswer);

                  phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "query the GAA?");
                  PhFuncTaCheck(phFuncTaSend(handlerID, "GAA?%s", handlerID->p.eol));
                  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
                  if(retVal != PHFUNC_ERR_OK) {
                    phFuncTaRemoveStep(handlerID);
                    return retVal;
                  }
                  phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "the response of GAA? query %s", handlerAnswer);

                  phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "get the value of Contact Site");
                  PhFuncTaCheck(phFuncTaSend(handlerID, "CS1?%s", handlerID->p.eol));
                  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
                  if(retVal != PHFUNC_ERR_OK) {
                    phFuncTaRemoveStep(handlerID);
                    return retVal;
                  }
                  phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "the value of Contact Site is %s", handlerAnswer);
                  extractContactSiteForInFlip(handlerID, handlerAnswer);                  
		  break;
                } else {
                  phLogFuncMessage (handlerID->myLogger, LOG_DEBUG, "No ready for test, waiting to finish the action");
                }
              } //~ if(newReceived)
              else {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "no SRQ received in waitForParts");
                return PHFUNC_ERR_WAITING;
              }
            } //~ while(1)
          } else {
            phLogFuncMessage (handlerID->myLogger, PHLOG_TYPE_ERROR, "receive unknown SRQ 0x%02x; continue to wait", srq);
            retVal = PHFUNC_ERR_ANSWER;
          }

          break;
        default: 
          break;
      }
    } else {
      /* do one poll. Maybe there is a part here, but we have not
         received an SRQ */
      if( handlerID->model == PHFUNC_MOD_INFLIP) {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "have not received new devices through SRQ, continue to wait in case of InFlip model");

      } else {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "have not received new devices through SRQ, try to poll...");
        PhFuncTaCheck(pollParts(handlerID));
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
   *  format for the command will be --A bin XX -- for each
   *  possible site which is a maximum of 9 characters for each 
   *  site
   *  char binningCommand[PHESTATE_MAX_SITES*9 + 16];
   */
  //char binningCommand[PHESTATE_MAX_SITES*9 + 16];
  char binningCommand[4096];
  char tmpCommand[2048];
  int i;
  int sendBinning = 0;

  strcpy(binningCommand,"");

  switch(handlerID->model) {
    case PHFUNC_MOD_MT93XX:
    case PHFUNC_MOD_MT9510:
    case PHFUNC_MOD_MT99XX:
      /* 
       *  following answers to queries from Multitest 
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
             *  following answers to queries from Multitest 
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

    case PHFUNC_MOD_INFLIP:
      strcpy(binningCommand, "BINX ");
      sendBinning = 1;
      for(i=0; i<handlerID->noOfSites; i++) {
        if(handlerID->activeSites[i] && 
           (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
            oldPopulation[i] == PHESTATE_SITE_POPDEACT)) {
            
            //the valid bin number range is [0..9]
            if(perSiteBinMap[i] < 0 || perSiteBinMap[i] > 9) {
              /* illegal bin code */
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                               "received illegal request for bin %ld at site \"%s\"",
                               perSiteBinMap[i], handlerID->siteIds[i]);
              retVal =  PHFUNC_ERR_BINNING;
            } else {
              /* bin this one */
              sprintf(tmpCommand, "%ld", perSiteBinMap[i]);
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                               "will bin device at site \"%s\" to %ld", 
                               handlerID->siteIds[i], perSiteBinMap[i]);

              if(i != 0) {
                strcat(binningCommand, ",");           //the bin number is delimters by ','
              }
              strcat(binningCommand, tmpCommand);             
            }
        } else {
          /* there is a no device here */
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                           "no device to reprobe or bin at site \"%s\"; will bin it as 'A'", 
                           handlerID->siteIds[i]);

          if(i != 0 ) {
            strcat(binningCommand,",");             //the bin number is delimters by ','
          }
          strcat(binningCommand, "A");
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

    if(handlerID->model == PHFUNC_MOD_INFLIP) {
      char handlerAnswer[1024] = "";
      PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "received message: \"%s\"", handlerAnswer);

      if(strcmp(handlerAnswer, "OK") != 0)
      {
        long response = -1;
        char strMsg[128] = {0};
        snprintf(strMsg, 127, "After sent BINX to handle and get responsibled message: \"%s\", Do you want to abort or continue?", handlerAnswer);
        phGuiMsgDisplayMsgGetResponse(handlerID->myLogger, 0,
          "Info", strMsg, "abort", "continue", NULL, NULL, NULL, NULL, &response);
        if ( response == 3)
        {
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "ignore message: \"%s\" and continue", handlerAnswer);
        }
        else if ( response == 2 )
        {
          retVal = PHFUNC_ERR_ABORTED;
        }
        else
        {
          retVal = PHFUNC_ERR_BINNING;
        }
      }
    }
  }

  return retVal;
}


/* send temperature setup to handler, if necessary */
static phFuncError_t reconfigureTemperature(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  struct tempSetup tempData;
  int i;
  char handlerAnswer[1024] = "";

  /* get all temperature configuration values */
  if(!phFuncTempGetConf(handlerID->myConf, handlerID->myLogger,
                        &tempData)) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                     "temperature control configuration corrupt");
    return PHFUNC_ERR_CONFIG;
  }


  /* OK, we have it, now check what we need to do */
  if(tempData.mode == PHFUNC_TEMP_MANUAL) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "temperature control must be setup manually at the handler");
    return retVal;
  } else if(tempData.mode == PHFUNC_TEMP_AMBIENT) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "temperature control will be set to \"ambient\" by the driver");
    switch(handlerID->model) {
      case PHFUNC_MOD_INFLIP:
        PhFuncTaCheck(phFuncTaSend(handlerID, "TMP OFF%s", handlerID->p.eol));
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
        break;
      case PHFUNC_MOD_MT93XX:
        PhFuncTaCheck(phFuncTaSend(handlerID, "TEMP OFF%s", handlerID->p.eol));
        break;
      case PHFUNC_MOD_MT9510:
      case PHFUNC_MOD_MT99XX:
        /* not supported */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "remote GPIB temperature control is not supported for MT9510/MT99XX handler");
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
    }
    return retVal;
  }

  /* tempData.mode == PHFUNC_TEMP_ACTIVE */

  /* this is the real job, define and activate all temperature
     parameters, check for correct chamber names first */
  switch(handlerID->model) {
    case PHFUNC_MOD_INFLIP:
    case PHFUNC_MOD_MT93XX:
      for(i=0; i<tempData.chambers; i++) {
        if(strcasecmp(tempData.name[i], "0")   != 0) {
          /* illegal chamber name */
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                           "the handler does not provide a temperature chamber with name \"%s\"\n"
                           "please check the configuration of \"%s\"",
                           tempData.name[i], PHKEY_TC_CHAMB);
          retVal = PHFUNC_ERR_CONFIG;
        }
      }
      break;      
    case PHFUNC_MOD_MT9510:
    case PHFUNC_MOD_MT99XX:
      /* not supported */
      break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
    default: break;
/* End of Huatek Modification */
  }
  if(retVal != PHFUNC_ERR_OK)
    return retVal;

  /* 
   * set the soaktime, guard band (lower, upper) for the Multitest strip handler 
   *  
   * leave the improvment space for other model (regarding those Multitest model, right now the customer 
   * seems to do not have the reqruiement that setting of the soaktime, desoaktime, guardbands 
   * in the driver configuration file. Jiawei Lin 2011) 
   */



  

  /* chamber names seem to be OK, define the values now */
  for(i=0; i<tempData.chambers; i++) {
    if(tempData.control[i] == PHFUNC_TEMP_ON) {
      /* setting the guardbands is handler specific */
      switch(handlerID->model) {
        case PHFUNC_MOD_INFLIP:
          //set the guardbands (lower, upper)
          if ( (tempData.lguard[i] >= 0.0) && (tempData.uguard[i] >= 0.0) ) {
            PhFuncTaCheck(phFuncTaSend(handlerID, "ATGB!%d,%d%s", 
                                       tempData.lguard[i], tempData.uguard[i], handlerID->p.eol));            
            phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);            
          }

          //set the soak time
          if (tempData.soaktime[i] >= 0){
            PhFuncTaCheck(phFuncTaSend(handlerID, "ST! %d%s", 
                                       tempData.soaktime[i], handlerID->p.eol));
            phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);        
          }

          //now realy set the setpoint temperature for the handler
          if( strcasecmp(tempData.name[i], "strip") == 0 ) {
            PhFuncTaCheck(phFuncTaSend(handlerID, "TMP %+d%s", 
                                     (int) tempData.setpoint[i], handlerID->p.eol));
          } else {
            PhFuncTaCheck(phFuncTaSend(handlerID, "TMP %s,%+d%s", 
                                     tempData.name[i], (int) tempData.setpoint[i], handlerID->p.eol));
          }
          phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);          
          break;
        case PHFUNC_MOD_MT93XX:
          PhFuncTaCheck(phFuncTaSend(handlerID, "TEMP %s,%+d%s", 
                                     tempData.name[i], (int) tempData.setpoint[i], handlerID->p.eol));
          break;
        case PHFUNC_MOD_MT9510:
        case PHFUNC_MOD_MT99XX:
          /* not supported */
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                           "remote GPIB temperature control is not supported for MT9510/MT99XX handler");
          break;

        default: 
          break;
      }
    }
  }

  /* all temperature values configured */
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
  PhFuncTaCheck(phFuncTaSend(handlerID, "ID?%s", handlerID->p.eol));
//    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", idString));
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
      case PHFUNC_MOD_MT93XX:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "received handler ID \"%s\", expecting a Multitest 93XX handler (not verified)",
                         idString);
        break;
      case PHFUNC_MOD_MT9510:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "received handler ID \"%s\", expecting a Multitest 9510 handler (not verified)",
                         idString);
        break;        
      case PHFUNC_MOD_MT99XX:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "received handler ID \"%s\", expecting a Multitest 99XX handler (not verified)",
                         idString);
        break;        
      case PHFUNC_MOD_INFLIP:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "received handler ID \"%s\", expecting a Multitest InFlip handler (not verified)",
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
    case PHFUNC_MOD_INFLIP:
      break;
    case PHFUNC_MOD_MT93XX:
      /* It should be safe to send the STA! at the beginning */
      PhFuncTaCheck(phFuncTaSend(handlerID, "STA!%s", handlerID->p.eol));
      break;
    case PHFUNC_MOD_MT9510:
    case PHFUNC_MOD_MT99XX:

#ifdef ALLOW_SQBM_COMMAND
      PhFuncTaCheck(phFuncTaSend(handlerID, "SQBM 15%s", handlerID->p.eol));
#endif

      PhFuncTaCheck(phFuncTaSend(handlerID, "STA!%s", handlerID->p.eol));
      break;
    default: break;

  }

  /*  
   * flush any pending SRQs (will poll later, if we are missing some
   * test start SRQ here 
   * following answers to queries from Multitest it appears the 
   * MT9510 also supports polling: So may flush for all handlers
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
    /* MT93xx, MT9510 and MT99XX all support polling */
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
   * following answers to queries from Multitest it appears the 
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
  PhFuncTaCheck(reconfigureTemperature(handlerID));

  return retVal;
}


/* perform real pause the handler operation */
static phFuncError_t doPauseHandler(phFuncId_t handlerID)
{
  /* check whether the handler is already paused too */
  phFuncTaMarkStep(handlerID);

  if(!handlerID->p.stopped) {
    /* SmarTest is paused, but the handler is not yet paused */
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "sending stop command to handler");
    switch(handlerID->model) {
      case PHFUNC_MOD_INFLIP:
      case PHFUNC_MOD_MT93XX:
      case PHFUNC_MOD_MT9510:
      case PHFUNC_MOD_MT99XX:
        PhFuncTaCheck(phFuncTaSend(handlerID, "STO!%s", handlerID->p.eol));
        break;
      default: break;
    }

    handlerID->p.stopped = 1;
    handlerID->p.pauseInitiatedByST = 1;
  }

  /* handler should now be stopped, otherwise return with
     WAITING and try again */
  if(!handlerID->p.stopped) {
    phFuncTaRemoveToMark(handlerID);  
    return PHFUNC_ERR_WAITING;
  }

  return PHFUNC_ERR_OK;
}

/* perform real unpause the handler operation */
static phFuncError_t doUnpauseHandler(phFuncId_t handlerID)
{
  /* check whether the handler is already out of pause */
  phFuncTaMarkStep(handlerID);

  if(handlerID->p.stopped) {
    if(handlerID->p.pauseInitiatedByST) {
      /* SmarTest paused the handler and should unpause it now */
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                       "sending start command to handler");
      switch(handlerID->model) {
        case PHFUNC_MOD_INFLIP:
        case PHFUNC_MOD_MT93XX:
        case PHFUNC_MOD_MT9510:
        case PHFUNC_MOD_MT99XX:
          PhFuncTaCheck(phFuncTaSend(handlerID, "STA!%s", handlerID->p.eol));
          break;
        default: break;
      }

      handlerID->p.stopped = 0;
      handlerID->p.pauseInitiatedByST = 0;
    }
  }

  /* handler should now be back up running, otherwise return
     with WAITING and try again */
  if(handlerID->p.stopped) {
    phFuncTaRemoveToMark(handlerID);  
    return PHFUNC_ERR_WAITING;
  }

  return PHFUNC_ERR_OK;
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
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static int firstCall = 1;*/
/*    static int isConfigured = 0;*/
/*    static char idString[512] = "";*/
/* End of Huatek Modification */

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

  //instresting vendor: they use special EndOfLine for strip handler!
  if( handlerID->model == PHFUNC_MOD_INFLIP ) {
    strcpy(handlerID->p.eol, "");
  } else {
    strcpy(handlerID->p.eol, "\r\n");
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
  static char idString[512] = "";
  static char resultString[1024] = "";
  idString[0] = '\0';
  *equipIdString = idString;
  phFuncError_t retVal = PHFUNC_ERR_OK;

  /* abort in case of unsuccessful retry */
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  /* common to all models: identify */
  phFuncTaStart(handlerID);
  PhFuncTaCheck(phFuncTaSend(handlerID, "ID?%s", handlerID->p.eol));
//    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", idString));
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
                         * following answers to queries from Multitest it appears
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
  int isPaused;

  /* abort in case of unsuccessful retry */
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  if(handlerID->p.pauseHandler) {
    phEstateAGetPauseInfo(handlerID->myEstate, &isPaused);
    if(!isPaused) {
      phFuncTaStart(handlerID);
      PhFuncTaCheck(doPauseHandler(handlerID));
      phFuncTaStop(handlerID);
    }
    if(handlerID->p.stopped)
      phEstateASetPauseInfo(handlerID->myEstate, 1);
  }
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
  int isPaused;

  /* abort in case of unsuccessful retry */
  if(phFuncTaAskAbort(handlerID)) return PHFUNC_ERR_ABORTED;

  phEstateAGetPauseInfo(handlerID->myEstate, &isPaused);
  if(isPaused) {
    phFuncTaStart(handlerID);
    PhFuncTaCheck(doUnpauseHandler(handlerID));
    phFuncTaStop(handlerID);
  }
  if(!handlerID->p.stopped)
    phEstateASetPauseInfo(handlerID->myEstate, 0);

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
 * Authors: Roger Feng
 *
 * Description:
 *    all the GPIB commands are stored locally for each Multitest model, this
 *    function will retrieve the corresponding command with regard to a
 *    certain query. For example, 
 *    (1)for the query 
 *      "PROB_HND_CALL(ph_get_status temperature_temperature soakarea)" ,
 *      this function will return the GPIB command:    "TMP?"
 *    (2)for the query 
 *      "PROB_HND_CALL(ph_get_status temperature_setpoint)" ,
 *      this function will return the GPIB command:    "SETTMP?"
 *
 * Return:
 *    function return SUCCEED if everything is OK, FAIL if error; and the found 
 *    GPIB command will be brought out via the parameter "pGpibCommand"
 *
 * Change:
 *
 *    Oct 29th 2008, Roger Feng, 
 *     Realize MT9510/MT9918/MT9928 GPIB temperature queries and commands.
 *
 ***************************************************************************/
static int getGpibCommandForGetStatusQuery(
                                          phFuncId_t handlerID,
                                          char **pGpibCommand, 
                                          const char *token,
                                          const char *param
                                          )
{
  // Parameter Validation Matrix. Format: Token vs Parameter format(RE) in pair
  // Valid parameter format are given in regular expression
  // Regular Expr  Remark
  //  ".*"          The token possibly has parameters but not a must.
  //  ""           The token doesn't allow parameters

  static const phStringPair_t sMT9510_QryVsParamFormat[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_GET_BARCODE, "^[0-9]+$"},
    {PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID, "^[0-9]+$"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE, "^(contactarea|CONTACTAREA)$"}
  };

  static const phStringPair_t sMT9918_QryVsParamFormat[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE, 
      "^(soakarea|SOAKAREA|soakcontact|SOAKCONTACT|contactarea|CONTACTAREA|(sensor|SENSOR)([1-9]|1[0-2]))$"}
  };

  static const phStringPair_t sMT9928_QryVsParamFormat[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT,    ""},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE, "^soakarea$|^soakcontact$|^contactarea$|^sensor([1-9]|1[0-2])$"}
  };

  // Query usage message
  static const phStringPair_t sMT_QryHelp[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_GET_BARCODE, "N (e.g. 1 or 2 or 16)"},
    {PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID, "N (e.g. 1 or 2 or 16)"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT,    "(no parameters)"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE, "<MT99x8>: contactarea, <MT9928>: soakarea|soakcontact|sensor[1~12]"}
  };

  // Internal Token Translation Matrix. The internal tokens are translated into handler acknowledged tokens.
  // Format: Internal Token vs Low Level Handler GPIB Command in pair
  // Internal tokens are derived from original token and the param, connected with underscore "_".
  //  e.g. temperature_temperature soakarea -> internal: temperature_temperature_soakarea
  //  only unitary parameter is expected currently.

  static const phStringPair_t sMT9510_QryTransMatrix[] = {
    // These static array must be ordered by the first field for binary search
    {"barcode",                                                         "HVC"},
    {"deviceid",                                                        "HVC"},
    {"temperature_temperature_contactarea",                        "TMP?"}
  };

  // MT99x8 (x=1,2)
  static const phStringPair_t sMT99x8_QryTransMatrix[] = {
    // These static array must be ordered by the first field for binary search
    {"temperature_setpoint",                                       "SETTMP?"},
    {"temperature_temperature_contactarea",                        "TMPCS?"},
    {"temperature_temperature_sensor",                             "SNS "},
    {"temperature_temperature_soakarea",                           "TMP?"},
    {"temperature_temperature_soakcontact",                        "TMPCH?"}
  };

  const int   MAX_INTERNAL_BUF_LEN     = 128;
  static char buffer[MAX_STATUS_MSG]   = "";
  char        internalQuery[MAX_INTERNAL_BUF_LEN];
  char        internalParam[MAX_INTERNAL_BUF_LEN];
  const       phStringPair_t *pStrPair = NULL;
  const char* pPattern                 = NULL;
  int         ret = -1;

  // Clear the return cache
  *pGpibCommand = NULL;

  // Validation input query and parameters

  if(strcasecmp(handlerID->cmodelName, "MT9510") == 0) {
    pStrPair = phBinSearchStrValueByStrKey(sMT9510_QryVsParamFormat, LENGTH_OF_ARRAY(sMT9510_QryVsParamFormat), token);
  } else if(strcasecmp(handlerID->cmodelName, "MT9918") == 0) {
    pStrPair = phBinSearchStrValueByStrKey(sMT9918_QryVsParamFormat, LENGTH_OF_ARRAY(sMT9918_QryVsParamFormat), token);
  } else if(strcasecmp(handlerID->cmodelName, "MT9928") == 0) {
    pStrPair = phBinSearchStrValueByStrKey(sMT9928_QryVsParamFormat, LENGTH_OF_ARRAY(sMT9928_QryVsParamFormat), token);
  } else {
    pStrPair = NULL;
  }

  if(pStrPair != NULL) {
    pPattern = pStrPair->value;
    ret = phToolsMatch(param, pPattern);

    if(ret != 1) {
      pStrPair = phBinSearchStrValueByStrKey(sMT_QryHelp, LENGTH_OF_ARRAY(sMT_QryHelp), token);
      if(pStrPair != NULL) {
        strcpy(buffer, "The query parameter \"%s\" for query \"%s\" is invalid!\n Usage: \"%s %s\"");
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, buffer, param, token, token, pStrPair->value);
        return FAIL;
      } else {
        strcpy(buffer, "Internal error: getGpibCommandForGetStatusQuery(): sMT_QryHelp pair does not cover all the queries!\n");
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, buffer);
        return FAIL;
      }
    }
  } else {
    strcpy(buffer, "The query of \"%s\" to current handler model %s is not supported!");
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, handlerID->cmodelName);
    return FAIL;
  }

  // Preprocess the query and params. Convert into internal query and params.

  // a. "Token + params" to identify the internal command
  if(strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE) == 0) {
    // i.internal parameter is required
    //  convertion:       internal cmd /internal params. e.g: sensor<n> -> "temperature_temperature_sensor"/"n?"
    if(strncasecmp("sensor", param, strlen("sensor")) == 0) {
      sprintf(internalQuery, "%s_%s", token, "sensor");
      sscanf(param, "sensor%s", internalParam);
      strcat(internalParam, "?");
    } else {
      // ii. no internal parameter
      sprintf(internalQuery, "%s_%s", token, param);
      strcpy(internalParam, "");
    }
  }else if(strcasecmp(token,PHKEY_NAME_HANDLER_STATUS_GET_BARCODE)==0||strcasecmp(token,PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID)==0){
    strcpy(internalQuery,token);
    sprintf(internalParam,"%02d", atoi(param));
  }
  // b."Token" only to identify the internal command
  else {
    strcpy(internalQuery, token);
    strcpy(internalParam, param);
  }


#ifdef DEBUG
  strcpy(buffer, "The query \"%s %s\" to handler model %s: internal query=%s,internal param=%s");
  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_0, buffer, token, param, handlerID->cmodelName, internalQuery, internalParam);
#endif

  // Internal Query Translation..
  switch(handlerID->model) {
    case PHFUNC_MOD_MT9510:
    case PHFUNC_MOD_MT93XX:
      pStrPair = phBinSearchStrValueByStrKey(sMT9510_QryTransMatrix, LENGTH_OF_ARRAY(sMT9510_QryTransMatrix), internalQuery);
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_0, "The internalQuery:%s has successfully Loaded!", internalQuery);
      break;
    case PHFUNC_MOD_MT99XX:
      pStrPair = phBinSearchStrValueByStrKey(sMT99x8_QryTransMatrix, LENGTH_OF_ARRAY(sMT99x8_QryTransMatrix), internalQuery);
      break;
    default:
      strcpy(buffer, "The query of \"%s\" to handler model %s is not supported!");
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, handlerID->cmodelName);
      return FAIL;
      break;
  }

  if( pStrPair != NULL ) {
    strcpy(internalQuery, pStrPair->value);
  } else {
    strcpy(buffer, "The query of \"%s %s\" to handler model %s is not supported!");
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, param, handlerID->cmodelName);
    return FAIL;
  }

  // Concatenate command and parameter
  sprintf(buffer, "%s%s", internalQuery, internalParam);
  *pGpibCommand = buffer;

  return SUCCEED;
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

  phFuncTaStart(handlerID);

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

    phFuncTaStop(handlerID);
    sprintf(response, "%s_KEY_NOT_AVAILABLE", token);
  }


  /* ensure null terminated */
  response[strlen(response)] = '\0';

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
 *    all the GPIB commands are stored locally for each Multitest model, this
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
  // Parameter Validation Matrix. Format: Token vs Parameter format(RE) in pair
  // Valid parameter format are given in regular expression
  // Regular Expr  Remark
  //  ".*"          The token possibly has parameters but not a must.
  //  ""           The token doesn't allow parameters

  // Multitest 9510
  static const phStringPair_t sMT9510_CmdVsParamFormat[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPCONTROL,         "^(off|OFF)$"},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPERATURE, "^(sensor|SENSOR)([1-9]|1[0-2])[ \t]+(\\+([0-9]|[1-9][0-9]|1[0-5][0-9]|160)|\\-[0-9]|-[1-5][0-9]|-60)$"}
  };

  // Multitest 9918, 9928 (x=1,2).
  static const phStringPair_t sMT99x8_CmdVsParamFormat[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_LOWERUPPERGUARDBAND, "^(sensor|SENSOR)([0-9]|1[0-2])[ \t]+\\[[ \t]*[0-5][ \t]+[0-5][ \t]*\\]$"},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_SOAKTIME,            ".*"},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPCONTROL,         "^(off|OFF)$"},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPERATURE, "^(sensor|SENSOR)([1-9]|1[0-2])[ \t]+(\\+)?([0-9]|[1-9][0-9]|1[0-5][0-5]|\\-[0-9]|-[1-5][0-5])$"}
  };

  // Command usage message
  static const phStringPair_t sMT_CmdHelp[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_LOWERUPPERGUARDBAND, "XX [Y Z]. XX=sensor1,..,sensor12; Y=0~5; Z=0~5."},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_SOAKTIME,            "XX. XX=time in seconds."},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPCONTROL,         "off"},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPERATURE,         "XX YZZZ. XX=sensor1,..,sensor24; Y=[+-]; YZZZ=-55~155 (MT99x8)|-60~+160(MT9510)."}
  };

  // Multitest 9510
  static const  phStringPair_t sMT9510_CmdTransMatrix[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPCONTROL,         "TMP "},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPERATURE,         "TMP "}
  };

  // Multitest 9918, 9928 (x=1,2).
  static const  phStringPair_t sMT99x8_CmdTransMatrix[] = {
    // These static array must be ordered by the first field for binary search
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_LOWERUPPERGUARDBAND, "ATGB!"},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_SOAKTIME,            "ST!"},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPCONTROL,         "TMP!"},
    {PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPERATURE,         "TMP!"}
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
  const unsigned int  SOAK_TIME_LIMIT  = 5994; // time limit for soak time setting in MT9918

  // Clear the return cache
  *pGpibCommand = NULL;

  // Validation input query and parameters

  if((strcasecmp(handlerID->cmodelName, "MT9918") == 0) || (strcasecmp(handlerID->cmodelName, "MT9928") == 0)) {
    pStrPair = phBinSearchStrValueByStrKey(sMT99x8_CmdVsParamFormat, LENGTH_OF_ARRAY(sMT99x8_CmdVsParamFormat), token);
  } else if(strcasecmp(handlerID->cmodelName, "MT9510") == 0) {
    pStrPair = phBinSearchStrValueByStrKey(sMT9510_CmdVsParamFormat, LENGTH_OF_ARRAY(sMT9510_CmdVsParamFormat), token);
  } else {
    pStrPair = NULL;
  }

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

  // Preprocess the command and params. Convert into internal command and params.

  // a. "Token + params" to identify the internal command
  pLocalParam = param;
  if((strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPERATURE) == 0) ||
     (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_LOWERUPPERGUARDBAND) == 0)) {
    // i.internal parameter is required
    //  convertion:       internal params. e.g: "sensor10 20" -> "10 20"
    pMarker= strstr(param, "sensor");
    if(pMarker != NULL) {
      pLocalParam = pMarker + strlen("sensor");
    }
  } else if(strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_SOAKTIME) == 0 ) {

    sscanf(pLocalParam, "%d", &soakTime);
    if(soakTime < 0 ) {
      strcpy(buffer, "The parameter \"%s\" for command \"%s\" is invalid!\n Usage: %s XX. XX must be non-negative.");
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, buffer, param, token, token );
      return FAIL;
    }

    if(strcasecmp("MT9918", handlerID->cmodelName) == 0 ) {
      soakTime = soakTime/6; // MT9918 sets time in unit of 1/10 minute(=6 sec).
      if(soakTime > SOAK_TIME_LIMIT) {
        strcpy(buffer, "The parameter \"%s\" for command \"%s\" is invalid!\n Usage: %s XX. XX=0~%d(sec).");
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, buffer, param, token, token, SOAK_TIME_LIMIT*6);
        return FAIL;
      }
    }
    sprintf(localParam, "%d", soakTime);
    pLocalParam = localParam;
  }

  strcpy(internalCommand, token);


  // Command translation. For instance:
  // "ph_set_status temperature_temperature sensor1 +80" -> "TMP!1,+80"
  // Get low level command

  switch(handlerID->model) {
    case PHFUNC_MOD_MT9510:
    case PHFUNC_MOD_MT93XX:
      pStrPair = phBinSearchStrValueByStrKey(sMT9510_CmdTransMatrix, LENGTH_OF_ARRAY(sMT9510_CmdTransMatrix), internalCommand);
      break;
    case PHFUNC_MOD_MT99XX:
      pStrPair = phBinSearchStrValueByStrKey(sMT99x8_CmdTransMatrix, LENGTH_OF_ARRAY(sMT99x8_CmdTransMatrix), internalCommand);
      break;
    default:
      strcpy(buffer, "The command of \"%s\" to handler model %s is not supported!");
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, handlerID->cmodelName);
      return FAIL;
      break;
  }

  if(pStrPair != NULL) {
    sprintf(internalCommand, "%s",pStrPair->value);
  } else {
    strcpy(buffer, "The command of \"%s %s\" to handler model %s is not supported!");
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, buffer, token, param, handlerID->cmodelName);
    return FAIL;
  }

  // convert space delimiter to comma in parameters and append the command
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
  const char *token = commandString;
  const char *param = paramString;
  char *gpibCommand = NULL;
  int found = FAIL;

  if( phFuncTaAskAbort(handlerID) ) return PHFUNC_ERR_ABORTED;

  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                   "privateSetStatus, token = ->%s<-, param = ->%s<-", token, param);

  found = getGpibCommandForSetStatusCommand(handlerID, &gpibCommand, token, param);
  if( found == SUCCEED ) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateSetStatus, gpibCommand = ->%s<-", gpibCommand);
    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", gpibCommand, handlerID->p.eol));
    // No ack message back
  } else {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "The command \"%s %s\" is invalid or not supported yet", token, param);
  }


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
