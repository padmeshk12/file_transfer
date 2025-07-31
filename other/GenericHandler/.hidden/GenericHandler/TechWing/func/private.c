/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
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
 *            Ken Ward, BitsoftSystems, Mirae revision
 *            Alan LIN & Jiawei Lin, STS-R&D, CR29015
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *            02 Jan 2001, Chris Joyce modified for Mirae-GPIB
 *            23 Mar 2001, Chris Joyce, added CommTest and split init into
 *                         init + comm + config
 *             9 Apr 2001, Siegfried Appich, changed debug_level from LOG_NORMAL
 *                         to LOG_DEBUG for site population and bin info
 *               Jan 2005, Ken Ward, converted to support Mirae MR5800
 *               Dec 2005, Alan LIN, CR29015
 *                 converted to support TechWing TW282 handler.
 *               Dec 2005, Jiawei Lin, ignore the privateLotStart
 *               Aug 2008, Jiawei Lin, disable the GETSTATUS_IMP
 *                 note that the TechWing driver does not support the
 *                 privateGetStatus (ph_get_status) query  privateSetStatus(ph_set_status)
 *                 the historical reason leads to many privateGetStatusXXX function in the private.c
 *               Aug 2008, Xiaofei Han, CR33527:
 *                  1) Added model TW33XX and enable the 256 site testing
 *                  2) Added the processing code for PHKEY_PL_MIRAE_SEND_SITESE parameter
 *                  3) Added the processing code for PHKEY_PL_MIRAE_MASK_DEVICE_PENDING parameter
 *                  4) Added the privateGetStatus()/privateSetStatus() functions
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
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <ctype.h>
/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_hfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
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

/* 
 * this macro allows driver plugin to poll handler
 * if a timeout has occurred while waiting for the SRQ
 * test start
 */
/* #define ALLOW_POLLING_WITH_SRQ */
/* #undef ALLOW_POLLING_WITH_SRQ  */
#undef ALLOW_POLLING_WITH_SRQ


/* 
 *   Place filler for BINON command where there is no operational site 
 */

#define NO_BINNING_RESULT  "A"


/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static void setStrUpper(char *s1)
{
  int i=0;

  while( s1[i] ) {
    s1[i]=toupper( s1[i] );
    ++i;
  }

  return;
}


static const char *findHexadecimalDigit(const char *s)
{
  while( *s && !isxdigit(*s) ) {
    ++s;
  }

  return s;
}

/*********************************************************************
 * If there are less than 64 digital number (256 sites) following the 
 * FULLSITES key words, this function pads zero to the beginning of 
 * the numbers. For example, if the digital number is 
 * "12345678,90123456,78901234,56789012,34567890", 
 * this function will change the string to 
 * "000000000000001234567890123456789012345678901234567890". .
 * The purpose of this function is to facilate the handling of 
 * 64 or more site number (CR33527).
 *
 * Author: Xiaofei Han
 * Date: 08/20/2008
 **********************************************************************/
static const char * paddingZeroToFullSiteMessage(const char *s, char* destString)
{
  char fullSiteMsgWithComma[82] = "";  /* 82 = strlen("FULLSITES ") + 64 + (upto 7 comma) + '\0'*/
  char fullSiteMsgWithoutComma[82] = "";

  char siteSetup [65] = ""; /* 65 = 64 digital + '\0' */
  char paddingString[65] = "";
  int i;
  char *p;

  if(s == NULL || destString == NULL)
  {
    return NULL;
  }

  /* FULLSITE message too long, return error */
  if(strlen(s) > 81)
  {
    return NULL;
  }

  strcpy(fullSiteMsgWithComma, s);

  //strip the comma from the FULLSITE string if any
  p = strtok(fullSiteMsgWithComma, ",");
  while ( p != NULL)
  {
    strcat(fullSiteMsgWithoutComma, p);
    p = strtok(NULL, ",");
  }

  //FULLSITE message too long, return error
  if(strlen(fullSiteMsgWithoutComma) > 74) /* 74 = strlen("FULLSITES ") + 64 digit number*/
  {
    return NULL;
  }

  /* if format error, return error */
  if(sscanf(fullSiteMsgWithoutComma, "FULLSITES %s", siteSetup) != 1)
  {
    return NULL;
  }

  /* padding zero to the beginning of the digital string */
  for(i=0; i<(64-strlen(siteSetup)); i++)
  {
    strcat(paddingString, "0");
  }
  strcat(paddingString, siteSetup);

  /* copy and return */
  strcpy(destString, paddingString);

  return destString;
}

static int localDelay_us(long microseconds) 
{
  long seconds;
  struct timeval delay;

  /* us must be converted to seconds and microseconds ** for use by select(2) */
  if( microseconds >= 1000000L ) {
    seconds = microseconds / 1000000L;
    microseconds = microseconds % 1000000L;
  } else {
    seconds = 0;
  }

  delay.tv_sec = seconds; 
  delay.tv_usec = microseconds; 

  /* return 0, if select was interrupted, 1 otherwise */
  if( select(0, NULL, NULL, NULL, &delay) == -1 &&
      errno == EINTR )
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

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "Entering flushSRQs\n");
  do {
    PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 1));
    phFuncTaRemoveStep(handlerID);
  } while( received );

  return retVal;
}


/* create descriptive status string */
static char *statusString(phFuncId_t handlerID)
{
  static char statusMessage[1024];

  statusMessage[0] = '\0';

  switch( handlerID->model ) {
    case PHFUNC_MOD_TW3XX:
    case PHFUNC_MOD_TW2XX:
      strcat(statusMessage, "handler status: NYI");
      break;
    default: break;
  }

  return statusMessage;
}

/* analyze handler status word and write message, if not empty */
static void analyzeStatus(phFuncId_t handlerID, int warn)
{
  char *message = NULL;

  message = statusString(handlerID);

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "%s", message);

}

/* query the handler's status by sending the command "TESTKIND?" */
/*****************************************************************************
 *
 * Query the handler TestKind status from handler
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *         send the command "TESTKIND?", the response like
 *         TESTKIND 00     -> normal test
 *         TESTKIND 02     -> prime Lot Start
 *         TESTKIND 04     -> first retest
 *         TESTKIND 05     -> second retest
 *         TESTKIND 08     -> prime Lot Start/retest end, before last sorting
 *         TESTKIND 10     -> Lot end, all sorting finished
 *
 ***************************************************************************/
static phFuncError_t queryTestKind(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  static char handlerAnswer[512] = "";
  int testKind = 0;

  switch( handlerID->model ) {
    case PHFUNC_MOD_TW3XX: 
    case PHFUNC_MOD_TW2XX:
      PhFuncTaCheck(phFuncTaSend(handlerID, "TESTKIND?%s", handlerID->p.eol));
      retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
      if( retVal != PHFUNC_ERR_OK ) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
      } else {
        if(sscanf(handlerAnswer, "TESTKIND %2d", &testKind) != 1) {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                   "invalid format of the response for \"TESTKIND?\" query");
          retVal = PHFUNC_ERR_ANSWER;
        } else {
          switch( testKind ) {
            case 0:
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "receving \"TESTKIND 00\", normal test");
              retVal = PHFUNC_ERR_DEVICE_START;
              break;
            case 2:
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "receving \"TESTKIND 02\", prime Lot start");
              retVal = PHFUNC_ERR_LOT_START;
              break;
            case 8:
              phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, 
                   "receving \"TESTKIND 08\", prime Lot /whole retest end before bin sorting!\n");
              retVal = PHFUNC_ERR_RETEST_DONE;
              break;
            case 10:
              phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, 
                   "receving \"TESTKIND 10\", prime Lot /whole retest end after bin sorting finished\n"
                   "that's, the whole Lot is finished");
              retVal = PHFUNC_ERR_LOT_DONE;
              break;
            case 4:
              phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, 
                   "receving \"TESTKIND 04\", first Retest!\n");
              retVal = PHFUNC_ERR_FIRST_RETEST;
              break;
            case 5:
              phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, 
                   "receving \"TESTKIND 05\", second (i.e, last) Retest!\n");
              retVal = PHFUNC_ERR_SECOND_RETEST;
              break;
            default:
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                   "receving \"TESTKIND %d\", unknown value!\n", testKind);
              retVal = PHFUNC_ERR_ANSWER;
              break;
          }
        }
      }
      break;
    default: 
      break;
  }

  return retVal;
}

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  static char handlerAnswer[512] = "";
  char paddedSiteSetup[65] = "";

  //The following variables are defined to support 256 sites
  unsigned long long population[4] = {0, 0, 0, 0};            
  unsigned long long populationPicked[4] = {0, 0, 0, 0};  
  int i = 0;
 

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "Entering pollParts\n");

  switch( handlerID->model ) {
    case PHFUNC_MOD_TW3XX:
    case PHFUNC_MOD_TW2XX:
      PhFuncTaCheck(phFuncTaSend(handlerID, "FULLSITES?%s", handlerID->p.eol));
      retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
      if( retVal != PHFUNC_ERR_OK ) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
      }

      break;
    default: 
      break;
  }

  /* ensure string is in upper case */
  setStrUpper(handlerAnswer);

  /* check answer from handler, similar for all handlers.  remove
     two steps (the query and the answer) if an empty string was
     received */
  
  if( paddingZeroToFullSiteMessage(handlerAnswer, paddedSiteSetup) == NULL) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                     "site population not received from handler: \n"
                     "received \"%s\" format expected \"FULLSITES x\"\n"
                     "please make sure the site number configured on handler is less or equal to 256 \n",
                     handlerAnswer);
    handlerID->p.oredDevicePending = 0;
    return PHFUNC_ERR_ANSWER;
  }

  sscanf(paddedSiteSetup, "%16llx%16llx%16llx%16llx", &population[3], &population[2], &population[1], &population[0]);

  /* correct pending devices information, overwrite all 
     (trust the last handler query) */
  handlerID->p.oredDevicePending = 0;

  for( i=0; i<handlerID->noOfSites; i++ ) {
    if( population[i/64] & (1LL << (i%64)) ) {
      handlerID->p.devicePending[i] = 1;
      handlerID->p.oredDevicePending = 1;
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "device present at site \"%s\" (polled)", 
                       handlerID->siteIds[i]);
      populationPicked[i/64] |= (1LL << (i%64));
    } else {
      handlerID->p.devicePending[i] = 0;
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "no device at site \"%s\" (polled)", 
                       handlerID->siteIds[i]);
    }
  }

  /* check whether we have received too many parts */

  if( (population[0] != populationPicked[0]) ||
      (population[1] != populationPicked[1]) ||
      (population[2] != populationPicked[2]) ||
      (population[3] != populationPicked[3])) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                     "The handler seems to present more devices than configured\n"
                     "The driver configuration must be changed to support more sites");
  }

  return retVal;
}

#ifdef LOTSTART_IMPLEMENTED
/* wait for lot start from handler */
/* Dec-15-2005, Alan LIN */
static phFuncError_t waitForLotStart(phFuncId_t handlerID)
{
  static int srq = 0;
  static int received = 0;

  phFuncError_t retVal = PHFUNC_ERR_OK;

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "Entering waitForLotStart: look for SRQ from handler\n");
  srq = 0;
  received = 0;
  retVal = phFuncTaTestSRQ(handlerID, &received, &srq, 0);

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "In waitForLotStart, TestSRQ returned retVal = %d, received = %d, srq = %x\n", retVal, received, srq);

  switch( handlerID->model ) {
    case PHFUNC_MOD_TW3XX:
    case PHFUNC_MOD_TW2XX:
      if( srq == 0x46 ) { // 70 decimal = lot start
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "In waitForLotStart, received lot start SRQ");
      } else if( srq == 0x41 ) { // 65decimal = Device ready to test
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
                         "In waitForLotStart, received device start SRQ");
        retVal = PHFUNC_ERR_DEVICE_START;
      } else if( srq == 0x3d ) { // 61 decimal
                                 /* jam */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "In waitForLotStart, received JAM SRQ 0x3D");
        retVal = PHFUNC_ERR_JAM;
      } else if( srq == 0x70 ) { // 112 decimal = Lot End
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
                         "In waitForLotStart, received lot end SRQ");
        retVal = PHFUNC_ERR_LOT_DONE;
      } else if( srq == 0x0 ) { // No SRQ received, timed out
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "In waitForLotStart, timed out waiting for lot start SRQ");
        retVal = PHFUNC_ERR_WAITING;
      }
      /* check for Alarm and Error of transmission */
      else {
        /* some exceptional SRQ occured */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "received exceptional SRQ 0x%02x:", srq);

        analyzeStatus(handlerID, 1);
        retVal = PHFUNC_ERR_ANSWER;
      }
      break;
    default: break;
  }

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "Exiting waitForLotStart, retVal = %d\n", retVal);
  return retVal;
}
#endif

/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
  static int srq = 0;
  static int received = 0;

  struct timeval pollStartTime;
  struct timeval pollStopTime;
  int timeout;
  static char handlerAnswer[512] = "";

  phFuncError_t retVal = PHFUNC_ERR_OK;

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "Entering waitForParts\n");


  if( handlerID->p.strictPolling ) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "waitForParts: strictPolling is enabled\n");
    /* apply strict polling loop, ask for site population */
    gettimeofday(&pollStartTime, NULL);

    timeout = 0;
    localDelay_us(handlerID->p.pollingInterval);
    do {
      PhFuncTaCheck(pollParts(handlerID));
      if( !handlerID->p.oredDevicePending ) {
        gettimeofday(&pollStopTime, NULL);
        if( (pollStopTime.tv_sec - pollStartTime.tv_sec)*1000000L + 
            (pollStopTime.tv_usec - pollStartTime.tv_usec) > 
            handlerID->heartbeatTimeout )
          timeout = 1;
        else
          localDelay_us(handlerID->p.pollingInterval);
      }
    } while( !handlerID->p.oredDevicePending && !timeout );

    /* flush any pending SRQs, we don't care about them but want
           to eat them up. Otherwise reconfiguration to interrupt
           based mode would fail. */
    PhFuncTaCheck(flushSRQs(handlerID));
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "waitForParts: look for SRQ from handler\n");
    /* we don't do strict polling, try to receive a start SRQ */
    /* this operation covers the heartbeat timeout only, if there
           are no more unexpected devices pending from a previous call
           (e.g. devices provided during reprobe) */

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "waitForParts: handlerID->p.oredDevicePending = %d\n", 
                     handlerID->p.oredDevicePending);
    srq = 0;
    retVal = phFuncTaTestSRQ(handlerID, &received, &srq, handlerID->p.oredDevicePending);

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "In waitForParts, TestSRQ returned retVal = %d, received = %d, srq = %x\n", retVal, received, srq);
    if(retVal != PHFUNC_ERR_OK) {
      return retVal;
    }

    if( srq != 0 ) {
      switch( handlerID->model ) {
        case PHFUNC_MOD_TW3XX:
        case PHFUNC_MOD_TW2XX:
          if( srq == 0x46 ) { // 70 decimal = lot start
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                             "received lot start SRQ");
            retVal = PHFUNC_ERR_LOT_START;
          } else if(srq == 0x41 ) { // 65 decimal = Device ready to test
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                             "received generic test start SRQ (0x41), need to poll");
            /*
            * need to send "TESTKIND?" to the TechWing to ask its status
            * the responses from handler include:
            *   TESTKIND 00       -> normal test ( not used )
            *   TESTKIND 02       -> prime Lot start
            *   TESTKIND 04       -> first retest
            *   TESTKIND 05       -> second retest
            *   TESTKIND 08       -> prime Lot/retest End, but bin sorting not finished
            *   TESTKIND 10       -> lot End, all the bin sorting finished
            */
            retVal = queryTestKind(handlerID);

            if( retVal == PHFUNC_ERR_DEVICE_START ) {
              /* now there may be parts, but we don't know where,
               must poll */
              PhFuncTaCheck(pollParts(handlerID));
              
              /* just change to PHFUNC_ERR_OK for everything is normal */
              retVal = PHFUNC_ERR_OK;
            } else if( retVal == PHFUNC_ERR_LOT_START) {
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                             "prime Lot start (TESTKIND 02)");
              /*
              * for normal test,the TechWing always send "TESTKIND 02", so here
              * it is identical to Device Start
              */
              PhFuncTaCheck(pollParts(handlerID));
              
              /* just change to PHFUNC_ERR_OK for everything is normal */
              retVal = PHFUNC_ERR_OK;
            } else if( retVal == PHFUNC_ERR_LOT_DONE) {
              /* need to send "LOTENDTOTAL?" to handler and query the Lot statistics*/
              PhFuncTaCheck(phFuncTaSend(handlerID, "LOTENDTOTAL?%s", handlerID->p.eol));
              retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
              if( retVal != PHFUNC_ERR_OK ) {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
              }
              /* need to send "LOTCMD 2" to handler */
              PhFuncTaCheck(phFuncTaSend(handlerID, "LOTCMD 2%s", handlerID->p.eol));

              /*
              * this will termniate the current applicaton Level, see
              * ph_device_start in "ph_frame.c"
              */
              retVal = PHFUNC_ERR_LOT_DONE; 
            } else if ( retVal == PHFUNC_ERR_FIRST_RETEST ){
              /* need to send "LOTCMD 0" to handler */
              PhFuncTaCheck(phFuncTaSend(handlerID, "LOTCMD 0%s", handlerID->p.eol));              

              /* Now we need to poll again for the device which is in retest */
              PhFuncTaCheck(pollParts(handlerID));

              /* just change to PHFUNC_ERR_OK for everything is normal */
              retVal = PHFUNC_ERR_OK;
            } else if ( retVal == PHFUNC_ERR_SECOND_RETEST ) {
              /* need to send "LOTCMD 0" to handler */
              PhFuncTaCheck(phFuncTaSend(handlerID, "LOTCMD 0%s", handlerID->p.eol));

              /* Now we need to poll again for the device which is in retest */
              PhFuncTaCheck(pollParts(handlerID));

              /* just change to PHFUNC_ERR_OK for everything is normal */
              retVal = PHFUNC_ERR_OK;
            } else if ( retVal == PHFUNC_ERR_RETEST_DONE ) {
              /* need to send "LOTCMD 0" to handler */
              PhFuncTaCheck(phFuncTaSend(handlerID, "LOTCMD 0%s", handlerID->p.eol));

              /* just change to PHFUNC_ERR_OK for everything is normal */
              retVal = PHFUNC_ERR_OK;              
            } else {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                             "unsupported responsed of value for \"TESTKIND?\" query!\n");              
              retVal = PHFUNC_ERR_ANSWER;
            }            
          } else if(srq == 0x3d ) { // 61 decimal
                                    /* jam */
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                             "received JAM SRQ 0x%02x:", srq);
            analyzeStatus(handlerID, 1);
            retVal = PHFUNC_ERR_JAM;
          } else if(srq == 0x48 ) { // 72 decimal = Lot End
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                             "received lot end SRQ");
            retVal = PHFUNC_ERR_LOT_DONE;
          }

          /* check for Alarm and Error of transmission */
          else {
            /* some exceptional SRQ occured */
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "received exceptional SRQ 0x%02X:", srq);

            analyzeStatus(handlerID, 1);
            retVal = PHFUNC_ERR_ANSWER;
          }
          break;
        default: 
          break;
      }
    } else {
#ifdef ALLOW_POLLING_WITH_SRQ
      /* do one poll. Maybe there is a part here, but we have not
             received an SRQ */
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "have not received new devices through SRQ, try to poll...");
      PhFuncTaCheck(pollParts(handlerID));
      if( handlerID->p.oredDevicePending ) {
        /* flush any pending SRQs that may have occured 
           during population query */
        PhFuncTaCheck(flushSRQs(handlerID));
      }
#endif
    }
  }

  return retVal;
}

/*****************************************************************************
 *
 * Setup bin code
 *
 * Description:
 *   For given bin information setup correct binCode.
 *
 ***************************************************************************/
static phFuncError_t setupBinCode(
                                 phFuncId_t handlerID     /* driver plugin ID */,
                                 long binNumber            /* for bin number */,
                                 char *binCode             /* array to setup bin code */
                                 )
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  strcpy(binCode,"");
  if( (handlerID->binIds!=NULL) && (handlerID->noOfBins!=0) ) {
    if( binNumber < 0 || binNumber >= handlerID->noOfBins ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "invalid binning data\n"
                       "could not bin to bin index %ld \n"
                       "maximum number of bins %d set by configuration %s",
                       binNumber, handlerID->noOfBins,PHKEY_BI_HBIDS);
      retVal=PHFUNC_ERR_BINNING;
    } else {
      sprintf(binCode,"%s",handlerID->binIds[ binNumber ]);
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "using binIds set binNumber %d to binCode \"%s\" ", 
                       binNumber, binCode);
    }
  } else {
    if( binNumber < 0 /*|| binNumber >= handlerID->noOfBins*/  ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                       "received illegal request for bin %lx ",
                       binNumber+1);
      retVal=PHFUNC_ERR_BINNING;
    } else {
      /*
      * here, the bin_mapping must be "default", so SmarTest hardbin number is used for
      * bincode, and SmarTest hardware bin starts from 0, but usually Handler
      * starts the bin from 1
      */
      sprintf(binCode,"%lx",binNumber+1);
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "Please note binNumber is %d, but set binCode to \"%s\" for TechWing numberings bin from \"1\"!", 
                       binNumber, binCode);
    }
  }

  return retVal;
}

/*****************************************************************************
 *
 * Compare bin information between the sent binning command
 * and the received echoed binning information.
 *
 * the format of each string is as follows:
 *   BINON:NNNNNNNN,NNNNNNNN,NNNNNNNN,NNNNNNNN;  (sent)
 *    ECHO:NNNNNNNN,NNNNNNNN,NNNNNNNN,NNNNNNNN;  (received)
 * 
 * where A is alphanumeric 
 *       N is a hexadecimal number 
 *
 ***************************************************************************/
static phFuncError_t checkBinningInformation(
                                            phFuncId_t handlerID     /* driver plugin ID */,
                                            const char *sent         /* sent binning command */,
                                            const char *received     /* returned binning info */
                                            )
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  int binMatch=0;
  const char* s=sent;
  const char* r=received;

  /* find first space */
  s=strchr(s, ':');
  if( s )
    s++;
  r=strchr(r, ':');
  if( r )
    r++;

  while( s && *s &&
         r && *r &&
         binMatch!= handlerID->noOfSites ) {
    s=findHexadecimalDigit(s);
    r=findHexadecimalDigit(r);

    if( isxdigit(*s) && isxdigit(*r) ) {
      if( tolower(*s) == tolower(*r) ) {
        ++binMatch;
      }
      ++s;
      ++r;
    }

  }

  if( binMatch != handlerID->noOfSites ) {  /*xiaofei*/
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                     "difference found between sent and returned binning information \n"
                     "SENT:    \"%s\"\n"
                     "RECEIVED: \"%s\"",sent,received);
    retVal = PHFUNC_ERR_BINNING;
  }

  return retVal;
}


/* create and send bin information */
static phFuncError_t binDevice(
                              phFuncId_t handlerID     /* driver plugin ID */,
                              phEstateSiteUsage_t *oldPopulation /* current site population */,
                              long *perSiteBinMap      /* valid binning data for each site */
                              )
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  int i;
  char testresults[PHESTATE_MAX_SITES + 16];
  static char querybins[PHESTATE_MAX_SITES + 512];
  char thisbin[32];
  int sendBinning = 0;
  int retryCount = 0;
  int repeatBinning = 0;
  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering binDevice\n");

  do {
    retVal = PHFUNC_ERR_OK;

    /* in case we loop because of bad verification results, we
           must reset the transaction back to the first step of this
           loop. Otherwise it may happen, that an operation in a
           second or third loop times out, leading to a return of this
           function, being called again later. Then the step counters
           would indicate the second or third iteration but the static
           fields may cause a different number of loops. Drawback:
           calling this function must not be preceeded by a MarkStep
           call */
    phFuncTaMarkStep(handlerID);

    switch( handlerID->model ) {
      case PHFUNC_MOD_TW3XX:
      case PHFUNC_MOD_TW2XX:
        strcpy(testresults, "BINON:");
        for( i= handlerID->noOfSites-1; i>=0 && retVal==PHFUNC_ERR_OK ; --i ) {
          if( i < handlerID->noOfSites &&
              ( handlerID->p.maskDevicePendingStatus == NO || handlerID->activeSites[i]) && 
              (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
               oldPopulation[i] == PHESTATE_SITE_POPDEACT) ) {
            retVal=setupBinCode(handlerID, perSiteBinMap[i], thisbin);
            /*xiaofei*/
            if( retVal==PHFUNC_ERR_OK ) {
              strcat(testresults, thisbin);
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                               "will bin device at site \"%s\" to %s", 
                               handlerID->siteIds[i], thisbin);
              sendBinning = 1;
            } else {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                               "unable to send binning at site \"%s\"", 
                               handlerID->siteIds[i]);
              sendBinning = 0;
            }
          } else {
            /* there's not a device here, we give bin ID 'A' */
            strcat(testresults, NO_BINNING_RESULT);
            if( i < handlerID->noOfSites ) {
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                               "no device to bin at site \"%s\"", 
                               handlerID->siteIds[i]);
            }
          }
          if( i == 0 ) {
            strcat(testresults, ";");
          } else if( i % 8 == 0 ) {
            strcat(testresults, ",");
          }
        }
        if( sendBinning ) {
          retVal = phFuncTaSend(handlerID, "%s%s", testresults, handlerID->p.eol);
          if(retVal != PHFUNC_ERR_OK) {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "send failed, retVal = %d\n", retVal);
          }
        }
        break;
      default: 
        break;
    }
    
    if( (handlerID->p.verifyBins) && (sendBinning) && (retVal ==  PHFUNC_ERR_OK) ) {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                       "will perform bin verification ....");
      PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", querybins));

      retVal=checkBinningInformation(handlerID, testresults, querybins);

      if( retVal == PHFUNC_ERR_OK ) {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         ".... verification succeeded");
        PhFuncTaCheck(phFuncTaSend(handlerID, "ECHOOK%s", 
                                   handlerID->p.eol));
      } else {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         ".... verification failed");
        PhFuncTaCheck(phFuncTaSend(handlerID, "ECHONG%s", 
                                   handlerID->p.eol));
      }
    }

    /* in case of receiving bad bin data during verification, or sending bin data timed out, we may want to try again */
    retryCount++;
    repeatBinning = (((retVal == PHFUNC_ERR_BINNING) &&
                      (retryCount <= handlerID->p.verifyMaxCount) &&
                      (handlerID->p.verifyBins) && 
                      (sendBinning)) || ((retVal == PHFUNC_ERR_TIMEOUT) && (sendBinning)));

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "repeatBinning = %s, retryCount = %d, maxRetries = %d\n", repeatBinning ? "true" : "false", retryCount, handlerID->p.verifyMaxCount);
    if( repeatBinning ) {
      repeatBinning = 1;
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "will try to send bin data %d more time(s) before giving up",
                       1 + handlerID->p.verifyMaxCount - retryCount);

      /* we do a loop, now go back to stored interaction mark */
      phFuncTaRemoveToMark(handlerID);    
      phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                       "  binDevice, try again\n");
    }

  } while( repeatBinning );

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Exiting binDevice\n");
  return retVal;
}

/* send verification mode, if necessary */
static phFuncError_t reconfigureVerification(phFuncId_t handlerID)
{
  int found = 0;
  phConfError_t confError;
  phFuncError_t retVal = PHFUNC_ERR_OK;
  double dRetryCount;

  /* look for the verification request */
  confError = phConfConfStrTest(&found, handlerID->myConf,
                                PHKEY_BI_VERIFY, "no", "yes", NULL);
  if( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
    retVal = PHFUNC_ERR_CONFIG;
  else {
    switch( found ) {
      case 1:
        /* configured "no" */
        switch( handlerID->model ) {
          case PHFUNC_MOD_TW3XX:
          case PHFUNC_MOD_TW2XX:
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "will deactivate bin verification mode");
            handlerID->p.verifyBins = 0;
            break;
          default: break;
        }
        break;
      case 2:
        /* configured "yes" */
        switch( handlerID->model ) {
          case PHFUNC_MOD_TW3XX:
          case PHFUNC_MOD_TW2XX:
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "will activate bin verification mode");
            handlerID->p.verifyBins = 1;
            break;
          default: break;
        }
        break;
      default:
        /* not configured, assume bin verification is not set up */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "\"%s\" not set: assume no bin verification",
                         PHKEY_BI_VERIFY);
        handlerID->p.verifyBins = 0;
        break;
    }
  }

  /* look for the maximum verification count */
  confError = phConfConfIfDef(handlerID->myConf, PHKEY_BI_VERCOUNT);
  if( confError == PHCONF_ERR_OK )
    confError = phConfConfNumber(handlerID->myConf, PHKEY_BI_VERCOUNT, 
                                 0, NULL, &dRetryCount);
  if( confError == PHCONF_ERR_OK )
    handlerID->p.verifyMaxCount = (int) dRetryCount;
  else {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "\"%s\" not set: assume double retry",
                     PHKEY_BI_VERCOUNT);
    handlerID->p.verifyMaxCount = 2; /* default double retry */
  }

  return retVal;
}

static phFuncError_t sendSitesel(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  phConfError_t confError;
  int numSites;
  int siteCtr;
  int iValue;
  double dValue;
  char commandString[PHESTATE_MAX_SITES + 16];
  int nibbleValue;
  int nibbleCtr;
  char nibbleString[2];
  static char handlerAnswer[512] = "";

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering sendSitesel\n");

  confError = phConfConfIfDef(handlerID->myConf, PHKEY_SI_HSMASK);
  if( confError != PHCONF_ERR_OK ) {
    /* configuration not given */
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "configuration parameter \"%s\" not given, SITESEL command will not be sent.", PHKEY_SI_HSMASK);
    return PHFUNC_ERR_OK;
  }

  switch( handlerID->model ) {
    case PHFUNC_MOD_TW3XX:
      strcpy(commandString, "CONTACTSEL ");
    break;
    case PHFUNC_MOD_TW2XX:
      strcpy(commandString, "SITESEL ");
    break;
    default:
      strcpy(commandString, "SITESEL ");
    break;
  }

  nibbleCtr = 0;
  numSites = handlerID->noOfSites;
  siteCtr = numSites - 1;
  nibbleValue = 0;
  /*
   * CR46389, Xiaofei Han, July 2009
   * Change the method of constructing the SITESEL command string to accommodate the non-fourfold
   * site configuration.
   */
  while( siteCtr >= 0 ) {
    confError = phConfConfNumber(handlerID->myConf, PHKEY_SI_HSMASK, 1, &siteCtr, &dValue);
    if( confError != PHCONF_ERR_OK ) dValue = 0.0;
    iValue = (int)dValue;
    iValue = iValue << siteCtr;
    nibbleValue += iValue;
    --siteCtr;
  }
  sprintf (nibbleString, "%X", nibbleValue);
  strcat(commandString, nibbleString);

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
//    localDelay_us(100000);
  retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    if( strncasecmp(TECHWING_OK, handlerAnswer, strlen(TECHWING_OK)) != 0 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "%s command returned an error: %s\n",
                       commandString, handlerAnswer);
      retVal = PHFUNC_ERR_ANSWER;
    } else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
  }
  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Exiting sendSitesel\n");
  return retVal;
}

/* check handler is running */
static phFuncError_t checkHandlerRunning(
                                        phFuncId_t handlerID         /* driver plugin ID */,
                                        char **handlerRunningString  /* resulting pointer to running query string */
                                        )
{
  static char frQuery[512] = "";

  //reset the value
  frQuery[0] = '\0';

  phFuncError_t retVal = PHFUNC_ERR_OK;

  //return the string address
  *handlerRunningString = frQuery;

  /* no identification but may ask if handler is running */
  PhFuncTaCheck(phFuncTaSend(handlerID, "FR?%s", handlerID->p.eol));
  localDelay_us(100000);
  retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", frQuery);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    return retVal;
  }

  localDelay_us(100000);

#if 0
  strcpy(frQuery, "FR1");
  *handlerRunningString = frQuery;
#endif

  return retVal;
}

static phFuncError_t handlerSetup(
                                 phFuncId_t handlerID    /* driver plugin ID */
                                 )
{
  int found = 0;
  phConfError_t confError;
  double dPollingInterval;
  double dValue;
  int iValue;
  int nibbleValue;
  int bitCtr;
  /* static char handlerAnswer[512] = ""; */
  int sendTestset = 1;
  phFuncError_t retVal = PHFUNC_ERR_OK;

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "entering handlerSetup");

  /* chose polling or SRQ interrupt mode */
  phConfConfStrTest(&found, handlerID->myConf, PHKEY_FC_WFPMODE, 
                    "polling", "interrupt", NULL);
  if( found == 1 ) {
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
  if( confError == PHCONF_ERR_OK )
    confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_POLLT, 
                                 0, NULL, &dPollingInterval);
  if( confError == PHCONF_ERR_OK )
    handlerID->p.pollingInterval = labs((long) dPollingInterval);
  else {
    handlerID->p.pollingInterval = 200000L; /* default 0.2 sec */
  }

  confError = phConfConfStrTest(&found, handlerID->myConf,
                                PHKEY_OP_PAUPROB, "yes", "no", NULL);
  if( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
    retVal = PHFUNC_ERR_CONFIG;
  else {
    if( found == 1 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                       "remote stopping is not (yet) supported by TechWing handler");
    }
  }

  /*
   * Check if PHKEY_PL_MIRAE_SEND_SITESEL is already defined. 
   * Added by Alex Lee; Consolidated by Xiaofei Han
   */
  confError = phConfConfIfDef(handlerID->myConf, PHKEY_PL_MIRAE_SEND_SITESEL);
  if ( confError == PHCONF_ERR_OK )
  {
    /*
     * if mirae_send_siteselection_cmd is set to yes, then set
     * sendSiteSelection to YES to call sendSitesel().
     * if mirae_send_siteselection_cmd is set to no, then set
     * sendSiteSelection to NO to not to call sendSitesel().
     */
    confError = phConfConfStrTest(&found, handlerID->myConf, PHKEY_PL_MIRAE_SEND_SITESEL, "yes", "no", NULL);

    if ( confError != PHCONF_ERR_OK ) 
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "configuration value of \"%s\" must be either \"yes\" or \"no\"\n",
                       PHKEY_PL_MIRAE_SEND_SITESEL);
      retVal = PHFUNC_ERR_CONFIG;
    } 
    else 
    {
      switch(found) 
      {
        case 1:   /* yes */
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                           "'sendsite selection' command will be sent to Handler");
          handlerID->p.sendSiteSelection = YES;
        break;
        case 2:   /* no */
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                           "'sendsite selection' command will NOT be sent to Handler");
          handlerID->p.sendSiteSelection = NO;
        break;
        default:
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL, "internal error at line: %d, "
                           "file: %s\n", __LINE__, __FILE__);
        break;
      }
    }
  }

  /* 
   * check if PHKEY_PL_MIRAE_MASK_DEVICE_PENDING is already defined.
   * Added by Alex Lee, Consolidated by Xiaofei Han
   */
  confError = phConfConfIfDef(handlerID->myConf, PHKEY_PL_MIRAE_MASK_DEVICE_PENDING);
  if ( confError == PHCONF_ERR_OK ) 
  {
    /*
     * if mirae_mask_device_pending is set to yes, then the 
     * device pending status received from handler will be masked 
     * according to the value of the parameter handler_site_mask.
     * Otherwise handler_site_mask doesn't take effect. 
     */
    confError = phConfConfStrTest(&found, handlerID->myConf, PHKEY_PL_MIRAE_MASK_DEVICE_PENDING,
                                  "yes", "no", NULL);
    if ( confError != PHCONF_ERR_OK ) 
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "configuration value of \"%s\" must be either \"yes\" or \"no\"\n",
                       PHKEY_PL_MIRAE_MASK_DEVICE_PENDING);
      retVal = PHFUNC_ERR_CONFIG;
    } 
    else 
    {
      switch(found) 
      {
        case 1:   /* yes */
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                           "device pending status received from handler  will be masked.");
          handlerID->p.maskDevicePendingStatus = YES;
        break;
        case 2:   /* no */
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                           "device pending status received from handler  will NOT be masked.");
          handlerID->p.maskDevicePendingStatus = NO;
        break;
        default:
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL, "internal error at line: %d, "
                           "file: %s\n", __LINE__, __FILE__);
        break;
      }
    }
  }


  /*************************************************************************/
  /* retrieve test tray configuration                                      */
  /*  Para = 16/32/64 (test in parallel)                                 */
  /*  Step = 1/2/4 (number of test step)                                   */
  /*  Row = 1 <= row <= F (usage of rows of the test tray, para*step <= 64 */
  /*  struct pluginPrivate (private.h):                                    */
  /*      Para : int testset_parallel;                                     */
  /*      Step:  int testset_step;                                         */
  /*      Row:   int testset_row;                                          */
  /*  ph_keys.h:                                                           */
  /*      #define PHKEY_FC_TESTSET_PARA     "test_tray_test_in_parallel"   */
  /*      #define PHKEY_FC_TESTSET_STEP     "test_tray_step"               */
  /*      #define PHKEY_FC_TESTSET_ROW      "test_tray_row"                */
  /*                                                                       */
  confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_TESTSET_PARA);
  if( confError == PHCONF_ERR_OK ) {
    confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_TESTSET_PARA, 
                                 0, NULL, &dValue);
    if( (confError == PHCONF_ERR_OK) ) {
      iValue = (int)dValue; /* convert */
      handlerID->p.testset_parallel = iValue;
      if( ! ((iValue == 16) || (iValue == 32) || (iValue == 64)) ) {
        /* we will default use 16, a safe value */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "Value for 'test_tray_test_in_parallel' should be 64, 32, or 16,\n"
                         " and the value of test_tray_test_in_parallel * test_tray_step \n"
                         " should be equal to 64.\n");
      }
    } else {
      handlerID->p.testset_parallel = 16; /* a safe value */
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                       "Value for test_tray_test_in_parallel should be 64, 32, or 16,\n"
                       " and the value of test_tray_test_in_parallel * test_tray_step \n"
                       " should be equal to 64.\n"
                       " Value has been set to %d.", handlerID->p.testset_parallel);
    }
  } else { /* Don't send! */
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "configuration parameter \"%s\" not given, TESTSET command will not be sent.", PHKEY_FC_TESTSET_PARA);
    sendTestset = 0;
  }
  if( sendTestset == 1 ) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "test_tray_in_parallel set to = %d", handlerID->p.testset_parallel);

    confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_TESTSET_STEP);
    if( confError == PHCONF_ERR_OK ) {
      confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_TESTSET_STEP, 
                                   0, NULL, &dValue);
      if( (confError == PHCONF_ERR_OK) ) {
        iValue = (int)dValue; /* convert */
        handlerID->p.testset_step = iValue;
        switch( iValue ) {
          case 1:
            break;
          case 2:
            break;
          case 4:
            break;
          default:
            /*
            * warning: invalid value
            * handlerID->p.testset_step = 64/handlerID->p.testset_parallel;
            */
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                             "Value for test_tray_step should be 1, 2, or 4, and \n,"
                             " the value of test_tray_parallel * test_tray_step \n"
                             " should be equal to 64.\n");
            break;
        }
      } else {
        handlerID->p.testset_step = 4;
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "Value for test_tray_step should be 1, 2, or 4, and \n,"
                         " the value of test_tray_parallel * test_tray_step \n"
                         " should be equal to 64.\n"
                         " Value has been set to %d.",
                         handlerID->p.testset_step);
      }
    } else {
      handlerID->p.testset_step = 4;
    }
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "test_tray_step set to = %d", handlerID->p.testset_step);

    confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_TESTSET_ROW);
    if( confError == PHCONF_ERR_OK ) {
      nibbleValue = 0;
      for( bitCtr = 0; bitCtr < 4; ++bitCtr ) {
        confError = phConfConfNumber(handlerID->myConf, 
                                     PHKEY_FC_TESTSET_ROW, 1, 
                                     &bitCtr, &dValue);
        if( confError != PHCONF_ERR_OK ) dValue = 0.0;
        iValue = (int)dValue;
        iValue = iValue << bitCtr;
        nibbleValue += iValue;
      }
      handlerID->p.testset_row = nibbleValue;
    } else {
      handlerID->p.testset_row = 0xf; /* default to all four rows */
    }
    /* done with retrieve of test tray configuration */
    
 #if 0
    PhFuncTaCheck(phFuncTaSend(handlerID, "TESTSET %d,%d,%X%s", 
                               handlerID->p.testset_parallel,
                               handlerID->p.testset_step,
                               handlerID->p.testset_row,
                               handlerID->p.eol));

    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if( retVal != PHFUNC_ERR_OK ) {
      phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
      return retVal;
    } else {
      if( strncasecmp(TECHWING_OK, handlerAnswer, strlen(TECHWING_OK)) != 0 ) {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "\"TESTSET %d,%d,%X\" command returned an error\n",
                         handlerID->p.testset_parallel,
                         handlerID->p.testset_step,
                         handlerID->p.testset_row);
        retVal = PHFUNC_ERR_ANSWER;
        return retVal;
      } else {  // all is good
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "test_tray_test_in_parallel set to = %d", handlerID->p.testset_parallel);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "test_tray_step set to = %d", handlerID->p.testset_step);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "test_tray_row set to = %X", handlerID->p.testset_row);
      }
    }
#endif    
  }
  /* Done sending TESTSET command */
  
#if 0
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "handlerSetup, calling reconfigureTemperature");
  PhFuncTaCheck(reconfigureTemperature(handlerID));
#endif

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "handlerSetup, calling reconfigureVerification");
  PhFuncTaCheck(reconfigureVerification(handlerID));

#if 0 
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "handlerSetup, calling sendSitemap");
  PhFuncTaCheck(sendSitemap(handlerID));
#endif  

if ( handlerID->p.sendSiteSelection == YES )
{
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "handlerSetup, calling sendSitesel");
  PhFuncTaCheck(sendSitesel(handlerID));
}

#if 0
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "handlerSetup, calling sendBinCategories");
  PhFuncTaCheck(sendBinCategories(handlerID));
#endif  

  /* flush any pending SRQs (will poll later, if we are missing some
     test start SRQ here */
  PhFuncTaCheck(flushSRQs(handlerID));

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "exiting handlerSetup");

  return retVal;
}


/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  char *handlerRunning;
  static int firstTime=1;

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "entering doReconfigure");

  if( firstTime == 1 ) {
    retVal = checkHandlerRunning(handlerID, &handlerRunning);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "doReconfigure (first time): FR? handler running returns \"%s\" ",
                     handlerRunning);

    // only send setup if handler not running
    if( retVal == PHFUNC_ERR_OK ) {
      if( strcasecmp(handlerRunning, "FR 0") == 0 ) { // handler stopped      
        PhFuncTaCheck(handlerSetup(handlerID));      
      } else {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "in doReconfigure: handler is running, will not be reconfigured.");
      }
      firstTime = 0;  // only count as firstTime if successfully sent FR?
    }
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "repeat call to doReconfigure, handler will not be reconfigured.");
  }

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "exiting doReconfigure");

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
  phFuncError_t retVal = PHFUNC_ERR_OK;
  int i;

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  /* do some really initial settings */
  handlerID->p.sites = handlerID->noOfSites;
  for( i=0; i<handlerID->p.sites; i++ ) {
    handlerID->p.siteUsed[i] = handlerID->activeSites[i];
    handlerID->p.devicePending[i] = 0;
    handlerID->p.deviceExpected[i] = 0;
  }
  handlerID->p.verifyBins = 1;    /* according to the setting of TechWing, always verify */
  handlerID->p.verifyMaxCount = 0;

  handlerID->p.strictPolling = 0;
  handlerID->p.pollingInterval = 200000L;
  handlerID->p.oredDevicePending = 0;
  handlerID->p.status = 0L;
  handlerID->p.stopped = 0;
  handlerID->p.runmode = 0; /* "normal" */
  handlerID->p.testset_parallel = 16;
  handlerID->p.testset_step = 4;
  handlerID->p.testset_row = 1;
  handlerID->binIds = NULL;
  handlerID->p.sendSiteSelection = NO;
  handlerID->p.maskDevicePendingStatus = YES;

  strcpy(handlerID->p.eol, "\r\n");

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
  if( phFuncTaAskAbort(handlerID) )
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
  /* by default not implemented */
  ReturnNotYetImplemented("privateEquipID");
}
#endif


#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot end signal from handler
 *
 * Authors: Ken Ward
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
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  phFuncTaStart(handlerID);

  if( (timeoutFlag == 0) && ( handlerID->interfaceType == PHFUNC_IF_GPIB ) ) {
    // Make this look like a repeated call, so the abort will work
    phFuncTaSetCall(handlerID, PHFUNC_AV_LOT_START);
    // Same as ph_hfunc.c calling:
    // RememberCall(handlerID, PHFUNC_AV_LOT_START);
  }

  retVal = waitForLotStart(handlerID);
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "privateLotStart, after waitForLotStart, retVal = %d\n", retVal);

  while( (timeoutFlag == 0) && (retVal != PHFUNC_ERR_OK) )
  //    while ( (timeoutFlag == 0) && (retVal == PHFUNC_ERR_WAITING) )
  {
    if( phFuncTaAskAbort(handlerID) ) {
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
 * Authors: Ken Ward
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
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  phFuncTaMarkStep(handlerID);


  // flush any extra pending SRQs
  // privateGetStart() got the lot_done SRQ
  retVal = flushSRQs(handlerID);

  if( retVal == PHFUNC_ERR_OK ) {
    phFuncTaStop(handlerID);
  } else {
    phFuncTaRemoveToMark(handlerID);
  }

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "exiting privateLotDone, retVal = %d\n", retVal);
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

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                   "Entering privateGetStart\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  /* remember which devices we expect now */
  for( i=0; i<handlerID->noOfSites; i++ )
    handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

  phFuncTaMarkStep(handlerID);

  retVal = waitForParts(handlerID);

  if(retVal == PHFUNC_ERR_JAM) {
    phFuncTaRemoveToMark(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "Exiting privateGetStart, PHFUNC_ERR_JAM\n");
    return PHFUNC_ERR_JAM;
  } else if(retVal == PHFUNC_ERR_LOT_DONE) {
    phFuncTaRemoveToMark(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "Exiting privateGetStart, PHFUNC_ERR_LOT_DONE\n");
    return PHFUNC_ERR_LOT_DONE;
  } else if(retVal != PHFUNC_ERR_OK) {
    phFuncTaRemoveToMark(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                     "Exiting privateGetStart, unknown error: retVal = %d\n", retVal);
    return PHFUNC_ERR_WAITING;
  }

  /* do we have at least one part? If not, ask for the current
     status and return with waiting */
  if( !handlerID->p.oredDevicePending ) {
    /* during the next call everything up to here should be
           repeated */
    phFuncTaRemoveToMark(handlerID);

    return PHFUNC_ERR_WAITING;
  }

  phFuncTaStop(handlerID);

  /* we have received devices for test. Change the equipment
     specific state now */
  handlerID->p.oredDevicePending = 0;
  for( i=0; i<handlerID->noOfSites; i++ ) {
    if (handlerID->p.maskDevicePendingStatus == YES) 
    {  
      /* 
       * Set the site configuration according to handler_site_mask.
       * If the site is NOT active and there is device on the site,
       * don't perform test on this site. 
       */
      if( handlerID->activeSites[i] == 1 ) 
      {
        if( handlerID->p.devicePending[i] ) 
        {
          handlerID->p.devicePending[i] = 0;
          population[i] = PHESTATE_SITE_POPULATED;
        } 
        else
        {
          population[i] = PHESTATE_SITE_EMPTY;
        }
      } 
      else 
      {
        if( handlerID->p.devicePending[i] ) 
        {
          /* something is wrong here, we got a device for an inactive site */
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                           "received device for deactivated site \"%s\"",
                           handlerID->siteIds[i]);
        }
        population[i] = PHESTATE_SITE_DEACTIVATED;
      }
    }
    else
    {
      /* 
       * Don't consider the parameter handler_site_mask.
       * Set the site populated if there's device on the site 
       * regardless of the active/inactive state of the site.
       */
      if ( handlerID->p.devicePending[i] )
      {
        handlerID->p.devicePending[i] = 0;
        population[i] = PHESTATE_SITE_POPULATED;
      }   
      else 
      {
        population[i] = PHESTATE_SITE_EMPTY;
      }
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
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  /* get current site population */
  phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

  phFuncTaStart(handlerID);
  PhFuncTaCheck(binDevice(handlerID, oldPopulation, perSiteBinMap));
  phFuncTaStop(handlerID);

  /* modify site population, everything went fine, otherwise we
     would not reach this point */
  for( i=0; i<handlerID->noOfSites; i++ ) {
    if( handlerID->p.maskDevicePendingStatus == YES )
    {
      if( handlerID->activeSites[i] && 
          (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
           oldPopulation[i] == PHESTATE_SITE_POPDEACT) )
      {
        population[i] = (oldPopulation[i] == PHESTATE_SITE_POPULATED) ? 
                      PHESTATE_SITE_EMPTY : PHESTATE_SITE_DEACTIVATED;
      }  
      else
      {
        population[i] = oldPopulation[i];   
      }
    }
    else
    {
      if (oldPopulation[i] == PHESTATE_SITE_POPULATED || oldPopulation[i] == PHESTATE_SITE_POPDEACT)
      { 
        population[i] = PHESTATE_SITE_EMPTY;
      }
      else
      {
        population[i] = oldPopulation[i];
      }
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
  /* by default not implemented */
  ReturnNotYetImplemented("privateReprobe");
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
  /* by default not implemented */
  ReturnNotYetImplemented("privateBinReprobe");
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
  /* by default not implemented */
  ReturnNotYetImplemented("privateSTPaused");
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
  /* by default not implemented */
  ReturnNotYetImplemented("privateSTUnpaused");
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
  switch( action ) {
    case PHFUNC_SR_QUERY:
      /* we can handle this query globaly */
      if( lastCall )
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
  char *handlerRunning;


  if( phFuncTaAskAbort(handlerID) ) {
    retVal = PHFUNC_ERR_ABORTED;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateCommTest: call to phFuncTaAskAbort returned TRUE\n");
    if( testPassed != NULL )
      *testPassed = 0;
    return retVal;
  }

  phFuncTaStart(handlerID);

  retVal = checkHandlerRunning(handlerID, &handlerRunning);


  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                   "privateCommTest: FR? handler running returns \"%s\", retVal = %d",
                   handlerRunning, retVal);

  if(retVal != PHFUNC_ERR_OK) {
    if( testPassed != NULL )
      *testPassed = 0;
    return retVal;
  }

  phFuncTaStop(handlerID);

  /* return result, there is no real check for the model here (so far) */
  if( testPassed != NULL )
    *testPassed = 0;
  return retVal;
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

#ifdef SETSTATUS_IMPLEMENTED


  #ifdef SETSTATUS_START_IMPLEMENTED
/*****************************************************************************
 *
 * Set handler status to START
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSetStatusStart(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[12];
  static char handlerAnswer[512] = "";

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering setStatusStart\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "START");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    if( strncasecmp(TECHWING_DONE, handlerAnswer, strlen(TECHWING_DONE)) != 0 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "%s command returned an error: %s\n",
                       commandString, handlerAnswer);
      retVal = PHFUNC_ERR_ANSWER;
    } else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
  }
  return retVal;
}
  #endif


  #ifdef SETSTATUS_STOP_IMPLEMENTED
/*****************************************************************************
 *
 * Set handler status to STOP
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSetStatusStop(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[12];
  static char handlerAnswer[512] = "";

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering setStatusStop\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "STOP");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    if( strncasecmp(TECHWING_DONE, handlerAnswer, strlen(TECHWING_DONE)) != 0 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "%s command returned an error: %s\n",
                       commandString, handlerAnswer);
      retVal = PHFUNC_ERR_ANSWER;
    } else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
  }
  return retVal;
}
  #endif


  #ifdef SETSTATUS_LOADER_IMPLEMENTED
/*****************************************************************************
 *
 * Set handler status to LOADER
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSetStatusLoader(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[12];
  static char handlerAnswer[512] = "";

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering setStatusLoader\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "LOADER");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    if( strncasecmp(TECHWING_DONE, handlerAnswer, strlen(TECHWING_DONE)) != 0 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "%s command returned an error: %s\n",
                       commandString, handlerAnswer);
      retVal = PHFUNC_ERR_ANSWER;
    } else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
  }
  return retVal;
}
  #endif


  #ifdef SETSTATUS_PLUNGER_IMPLEMENTED
/*****************************************************************************
 *
 * Set handler status of PLUNGER to ON (1) or OFF (0)
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSetStatusPlunger(phFuncId_t handlerID, int contactorONOFF)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char cValue [4];
  char commandString[12];
  static char handlerAnswer[512] = "";

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering setStatusPlunger\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "PLUNGER ");
  if((contactorONOFF == 0) || (contactorONOFF == 1)) {
    sprintf(cValue, "%d", contactorONOFF);
    strcat(commandString, cValue);
  } else {
    retVal = PHFUNC_ERR_CONFIG;
    return retVal;
  }


  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    if( strncasecmp(TECHWING_DONE, handlerAnswer, strlen(TECHWING_DONE)) != 0 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "%s command returned an error: %s\n",
                       commandString, handlerAnswer);
      retVal = PHFUNC_ERR_ANSWER;
    } else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
  }
  return retVal;
}
  #endif


  #ifdef SETSTATUS_NAME_IMPLEMENTED
/*****************************************************************************
 *
 * Set handler name
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSetStatusName(phFuncId_t handlerID, char *nameString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[512] = "";

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering setStatusName\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  if(nameString == NULL) {
    retVal = PHFUNC_ERR_CONFIG;
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "privateSetStatusName invalid argument: NULL");
    return retVal;
  }
  if((strlen(nameString) == 0) || (strlen(nameString) > 12)) {
    retVal = PHFUNC_ERR_CONFIG;
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "privateSetStatusName invalid argument: %s",
                     nameString);
    return retVal;
  }

  strcpy(commandString, "SETNAME ");
  strcat(commandString, nameString);

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    if( strncasecmp(TECHWING_OK, handlerAnswer, strlen(TECHWING_OK)) != 0 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "%s command returned an error: %s\n",
                       commandString, handlerAnswer);
      retVal = PHFUNC_ERR_ANSWER;
    } else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
  }
  return retVal;
}
  #endif


  #ifdef SETSTATUS_RUNMODE_IMPLEMENTED
/*****************************************************************************
 *
 * Set handler status of RUNMODE to "normal" (0) or "flush" (0)
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSetStatusRunmode(phFuncId_t handlerID, int mode)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char cValue [4];
  char commandString[12];
  static char handlerAnswer[512] = "";

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering setStatusRunmode\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "RUNMODE ");
  if((mode == 0) || (mode == 1)) {
    sprintf(cValue, "%d", mode);
    strcat(commandString, cValue);
  } else {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3, 
                     "in setStatusRunmode, invalid parameter: mode = %d\n", mode);
    retVal = PHFUNC_ERR_CONFIG;
    return retVal;
  }


  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    if( strncasecmp(TECHWING_OK, handlerAnswer, strlen(TECHWING_OK)) != 0 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "%s command returned an error: %s\n",
                       commandString, handlerAnswer);
      retVal = PHFUNC_ERR_ANSWER;
    } else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
  }
  return retVal;
}
  #endif


#endif /* SETSTATUS_IMPLEMENTED */


#ifdef GETSTATUS_IMPLEMENTED

/*****************************************************************************
 *
 * Get Status Handler VERSION?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusVersion(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusVersion\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "VERSION?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}

/*****************************************************************************
 *
 * Get Status Handler NAME?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusName(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusName\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "NAME?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler TESTSET?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusTestset(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusTestset\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "TESTSET?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}

/*****************************************************************************
 *
 * Get Status Handler SETTEMP?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusSettemp(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusSettemp\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "SETTEMP?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler MEASTEMP d?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusMeastemp(phFuncId_t handlerID, char *parm, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusVersion\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  sprintf(commandString, "MEASTEMP %s?", parm);

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}

/*****************************************************************************
 *
 * Get Status Handler STATUS?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateGetStatusStatus(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusStatus\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "STATUS?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler JAM?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusJam(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusJam\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

  strcpy(commandString, "JAM?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler JAMCODE?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusJamcode(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusJamcode\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

  strcpy(commandString, "JAMCODE?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler JAMQUE?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusJamQue(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusJamQue\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

  strcpy(commandString, "JAMQUE?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler JAMCODE?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusJamcount(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusJamcount\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

  strcpy(commandString, "JAMCOUNT?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler SETLAMP?
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusSetlamp(phFuncId_t handlerID, char **responseString)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[24];
  static char handlerAnswer[MAX_STATUS_MSG] = "";
  handlerAnswer[0] = '\0';
  *responseString = handlerAnswer;

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering privateGetStatusSetlamp\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  strcpy(commandString, "SETLAMP?");

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "%s command returned an error, return code =: %d\n",
                     commandString, retVal);
    phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
  } else {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Sent: %s\nReceived: %s", commandString, handlerAnswer);
  }

  strcat(handlerAnswer, "\0");  // ensure null terminated

  return retVal;
}

/*****************************************************************************
 *
 * the interface function to retrive the status from handler
 *
 * Authors: Xiaofei Han 
 *
 * Description:
 *   this function is called by the phFuncGetStatus
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateGetStatus( phFuncId_t handlerID, const char *commandString, const char *paramString, char **answer)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  /* Currently no get status functionality is avaliable for TechWing*/

  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3,
                  "in privateGetStatus, token: ->%s<- not implemented for Mirae driver.\n", commandString);
  retVal = PHFUNC_ERR_NA;

  return retVal;
}

#endif /* GETSTATUS_IMPLEMENTED */

#ifdef SETSTATUS_IMPLEMENTED

/*****************************************************************************
 *
 * Set handler Sites Enable/Disable
 *
 * Authors: Vincent Liu
 *          Consolidated by Xiaofei Han
 *
 * Description:
 *       Command: "CONTACTSEL dddddd......"
 *       Total 64 number to control 256 sites
 *       Number : 0~1, A~F
 *                0=0000
 *                1=0001
 *                2=0010
 *                ...
 *                9=1001
 *                A=1010
 *                B =1011
 *                ...
 *                F=1111
 *                1=ON, 0=OFF
 * Example:  control 256 sites on/off
 *       "CONTACTSEL 93acbd444455556765478964531234ff93acbd444455556765478964531234ff"
 *                   |                                                              |
 *                   site 253~256                                                   sites 1~4
 ***************************************************************************/
static phFuncError_t privateSetStatusContactSel(phFuncId_t handlerID, const char *ContactSel)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char commandString[80];
  static char handlerAnswer[512] = "";

  phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                   "Entering setStatusContactSel\n");

  /* abort in case of unsuccessful retry */
  if( phFuncTaAskAbort(handlerID) )
    return PHFUNC_ERR_ABORTED;

  if(ContactSel == NULL) {
    retVal = PHFUNC_ERR_CONFIG;
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "privateSetStatusContactSel invalid argument: NULL");
    return retVal;
  }

  if((strlen(ContactSel) == 0) || (strlen(ContactSel) > 64)) {
    retVal = PHFUNC_ERR_CONFIG;
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "privateSetStatusContactSel invalid argument: %s",
                     ContactSel);
    return retVal;
  }

  switch( handlerID->model ) {
    case PHFUNC_MOD_TW3XX:
      strcpy(commandString, "CONTACTSEL ");
    break;
    case PHFUNC_MOD_TW2XX:
      strcpy(commandString, "SITESEL ");
    break;
    default:
      strcpy(commandString, "SITESEL ");
    break;
  }
  strcat(commandString, ContactSel);

  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
  retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
  if( retVal != PHFUNC_ERR_OK ) {
    phFuncTaRemoveStep(handlerID); 
  } else {
    if( strncasecmp(TECHWING_OK, handlerAnswer, strlen(TECHWING_OK)) != 0 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "%s command returned an error: %s\n",
                       commandString, handlerAnswer);
      retVal = PHFUNC_ERR_ANSWER;
    } else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
  }
  return retVal;
}

/*****************************************************************************
 *
 * the interface function to set the configuration for handler
 *
 * Authors:  Xiaofei Han
 *
 * Description:
 *   this function is called by phFuncSetStatus
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSetStatus(phFuncId_t handlerID, const char *token, const char *param)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  if ( (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_CONTACTSEL)==0) ||
       (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_SITESEL)==0) ) 
  {
    if (strlen(param) > 0) 
    {
      retVal = privateSetStatusContactSel(handlerID, param);
    } 
    else 
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE, "privateSetStatus CONTACTSEL, no param specified");
      retVal = PHFUNC_ERR_NA;
    }
  } 
  else 
  {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3,
                    "in privateSetStatus, token: ->%s<- not implemented for Mirae driver.\n", token);
    retVal = PHFUNC_ERR_NA;
  }

  return retVal;

}

#endif //SETSTATUS_IMPLEMENTED


/*****************************************************************************
 * End of file
 *****************************************************************************/
 
