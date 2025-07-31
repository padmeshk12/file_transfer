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
 *            Ping Chen, R&D Shanghai, CR28327
 *            Jiawei Lin, R&D Shanghai, CR38119
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
 *             1 Nov 2005, Ping Chen, updated the file to enable customer send
 *                         or not send sendSitesel() based on their requirement.
 *                         --- CR28327
 *             Jan 2008,   CR38119
 *                         introduce of the M660 model. M660 covers the GPIB
 *                         commands from MR5800, and it also supports the additional ones:
 *                         "LOTERRCODE?" (ph_get_status)
 *                         "LOTALMCODE?" (ph_get_status)
 *                         "LOTDATA?" (ph_get_status)
 *                         "LOTCLEAR" (ph_set_status)
 *                         "LOTTOTAL?" (ph_get_status)
 *                         "ACCSORTDVC?" (ph_get_status)
 *                         "ACCJAMCODE?" (ph_get_status)
 *                         "ACCERRCODE?" (ph_get_status)
 *                         "ACCDATA?" (ph_get_status)
 *                         "ACCCLEAR" (ph_set_status)
 *                         "TRAY_ID?" (ph_get_status)
 *                         "TEMPMODE?" (ph_get_status)
 *
 *                         A new internal interface "privateGetStatus" is also implemented to
 *                         retrieve the status from the handler
 *  
 *             Sept 2008,  Xiaofei Han, CR42150
 *                         Introduce M330 model and add code to support more(256) sites.
 *                         The protocol for M330 are similar to that of MR5800 except for the
 *                         format of the "BINON"/"ECHO" message and the TESTSET related commands:
 *                         1)
 *                         M330:      BINON:NNNNNNNN,NNNNNNNN,NNNNNNNN...
 *                                    ECHO:NNNNNNNN,NNNNNNNN,NNNNNNNN...
 *                         MR5800:    BINON NNNNNNNN,NNNNNNNN,NNNNNNNN...
 *                                    ECHO NNNNNNNN,NNNNNNNN,NNNNNNNN
 *                         2)
 *                         M330 doesn't support TESTSET commands but MR5800 does.
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
#include "temperature.h"
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

    while ( s1[i] )
    {
        s1[i]=toupper( s1[i] );
        ++i;
    }

    return;
}

/*
static void setStrLower(char *s1)
{
    int i=0;

    while ( s1[i] )
    {
        s1[i]=tolower( s1[i] );
        ++i;
    }

    return;
}
*/

static const char *findHexadecimalDigit(const char *s)
{
    while ( *s && !isxdigit(*s) )
    {
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
 * 64 or more site number (CR42150).
 *
 * Author: Xiaofei Han
 * Date: 09/10/2008
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
    if ( microseconds >= 1000000L )
    {
        seconds = microseconds / 1000000L;
        microseconds = microseconds % 1000000L;
    }
    else
    {
        seconds = 0;
    }

    delay.tv_sec = seconds; 
    delay.tv_usec = microseconds; 

    /* return 0, if select was interrupted, 1 otherwise */
    if ( select(0, NULL, NULL, NULL, &delay) == -1 &&
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
    do
    {
        PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 1));
        phFuncTaRemoveStep(handlerID);
    } while ( received );

    return retVal;
}


/* create descriptive status string */
static char *statusString(phFuncId_t handlerID)
{
    static char statusMessage[1024];
//    long status = handlerID->p.status;

    statusMessage[0] = '\0';

    switch ( handlerID->model )
    {
        case PHFUNC_MOD_MR5800:
        case PHFUNC_MOD_M660:
        case PHFUNC_MOD_M330:
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

//    if ( handlerID->p.status != 0L && warn )
//        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
//                         "%s", message);
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

    switch ( handlerID->model )
    {
        case PHFUNC_MOD_MR5800:
        case PHFUNC_MOD_M660:
        case PHFUNC_MOD_M330:
            PhFuncTaCheck(phFuncTaSend(handlerID, "FULLSITES?%s", handlerID->p.eol));
            retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
            if ( retVal != PHFUNC_ERR_OK )
            {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }

            break;
        default: break;
    }

    /* ensure string is in upper case */
    setStrUpper(handlerAnswer);
                                                   
    /* check answer from handler, similar for all handlers.  remove
       two steps (the query and the answer) if an empty string was
       received */
    if( paddingZeroToFullSiteMessage(handlerAnswer, paddedSiteSetup) == NULL) 
    {
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
    for( i=0; i<handlerID->noOfSites; i++ ) 
    {
        if( population[i/64] & (1LL << (i%64)) ) 
        {
            handlerID->p.devicePending[i] = 1;
            handlerID->p.oredDevicePending = 1;
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "device present at site \"%s\" (polled)", 
                             handlerID->siteIds[i]);
            populationPicked[i/64] |= (1LL << (i%64));
        }
        else
        {
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
      (population[3] != populationPicked[3])) 
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                         "The handler seems to present more devices than configured\n"
                         "The driver configuration must be changed to support more sites");
    }

    return retVal;
}

/* wait for lot start from handler */
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

    switch ( handlerID->model )
    {
        case PHFUNC_MOD_MR5800:
        case PHFUNC_MOD_M660:
        case PHFUNC_MOD_M330:
            if ( srq == 0x46 ) // 70 decimal = lot start
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                 "In waitForLotStart, received lot start SRQ");
            }
            else if ( srq == 0x47 ) // 71 decimal = Device ready to test
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
                                 "In waitForLotStart, received device start SRQ");
                retVal = PHFUNC_ERR_DEVICE_START;
            }
            else if ( srq == 0x3d ) // 61 decimal
            {
                /* jam */
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                 "In waitForLotStart, received JAM SRQ 0x3D");
                retVal = PHFUNC_ERR_JAM;
            }
            else if ( srq == 0x48 ) // 72 decimal = Lot End
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
                                 "In waitForLotStart, received lot end SRQ");
                retVal = PHFUNC_ERR_LOT_DONE;
            }
            else if ( srq == 0x0 ) // No SRQ received, timed out
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                 "In waitForLotStart, timed out waiting for lot start SRQ");
                retVal = PHFUNC_ERR_WAITING;
            }
            /* check for Alarm and Error of transmission */
            else
            {
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


/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
    static int srq = 0;
/*    int srqPicked = 0;*/
    static int received = 0;
/*    static char notification[512];*/

    struct timeval pollStartTime;
    struct timeval pollStopTime;
    int timeout;
/*    int i;*/
/*    int partMissing = 0;*/
    
    phFuncError_t retVal = PHFUNC_ERR_OK;

phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                 "Entering waitForParts\n");


    if ( handlerID->p.strictPolling )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "waitForParts: strictPolling is enabled\n");
        /* apply strict polling loop, ask for site population */
        gettimeofday(&pollStartTime, NULL);

        timeout = 0;
        localDelay_us(handlerID->p.pollingInterval);
        do
        {
            PhFuncTaCheck(pollParts(handlerID));
            if ( !handlerID->p.oredDevicePending )
            {
                gettimeofday(&pollStopTime, NULL);
                if ( (pollStopTime.tv_sec - pollStartTime.tv_sec)*1000000L + 
                     (pollStopTime.tv_usec - pollStartTime.tv_usec) > 
                     handlerID->heartbeatTimeout )
                    timeout = 1;
                else
                    localDelay_us(handlerID->p.pollingInterval);
            }
        } while ( !handlerID->p.oredDevicePending && !timeout );

        /* flush any pending SRQs, we don't care about them but want
               to eat them up. Otherwise reconfiguration to interrupt
               based mode would fail. */
        PhFuncTaCheck(flushSRQs(handlerID));
    }
    else
    {
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
//        retVal = phFuncTaTestSRQ(handlerID, &received, &srq, 0);

        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
             "In waitForParts, TestSRQ returned retVal = %d, received = %d, srq = %x\n", retVal, received, srq);
        if (retVal != PHFUNC_ERR_OK) 
        {
            return retVal;
        }

        if ( srq != 0 )
        {
            switch ( handlerID->model )
            {
                case PHFUNC_MOD_MR5800:
                case PHFUNC_MOD_M660:
                case PHFUNC_MOD_M330:
                    if ( srq == 0x46 ) // 70 decimal = lot start
                    {
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                         "received lot start SRQ");
                        retVal = PHFUNC_ERR_LOT_START;
                    }
                    else if (srq == 0x47 ) // 71 decimal = Device ready to test
                    {
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                         "received generic test start SRQ, need to poll");
                        /* now there may be parts, but we don't know where,
                           must poll */
                        PhFuncTaCheck(pollParts(handlerID));
                    }
                    else if (srq == 0x3d ) // 61 decimal
                    {
                        /* jam */
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                         "received JAM SRQ 0x%02x:", srq);
                        analyzeStatus(handlerID, 1);
                        retVal = PHFUNC_ERR_JAM;
                    }
                    else if (srq == 0x48 ) // 72 decimal = Lot End
                    {
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                         "received lot end SRQ");
                        retVal = PHFUNC_ERR_LOT_DONE;
                    }

                    /* check for Alarm and Error of transmission */
                    else
                    {
                        /* some exceptional SRQ occured */
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                         "received exceptional SRQ 0x%02X:", srq);

                        analyzeStatus(handlerID, 1);
                        retVal = PHFUNC_ERR_ANSWER;
                    }
                    break;
                default: break;
            }
        }
        else
        {
#ifdef ALLOW_POLLING_WITH_SRQ
            /* do one poll. Maybe there is a part here, but we have not
                   received an SRQ */
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                             "have not received new devices through SRQ, try to poll...");
            PhFuncTaCheck(pollParts(handlerID));
            if ( handlerID->p.oredDevicePending )
            {
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
    if ( handlerID->binIds )
    {
        if ( binNumber < 0 || binNumber >= handlerID->noOfBins )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "invalid binning data\n"
                             "could not bin to bin index %ld \n"
                             "maximum number of bins %d set by configuration %s",
                             binNumber, handlerID->noOfBins,PHKEY_BI_HBIDS);
            retVal=PHFUNC_ERR_BINNING;
        }
        else
        {
            sprintf(binCode,"%s",handlerID->binIds[ binNumber ]);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                             "using binIds set binNumber %d to binCode \"%s\" ", 
                             binNumber, binCode);
        }
    }
    else
    {
        if ( binNumber < 0 || binNumber >= handlerID->noOfBins  )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                             "received illegal request for bin %lx ",
                             binNumber+1);
            retVal=PHFUNC_ERR_BINNING;
        }
        else
        {
            sprintf(binCode,"%lx",binNumber+1);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                             "using binNumber set binNumber %d to binCode \"%s\" ", 
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
 *   BINON NNNNNNNN,NNNNNNNN,NNNNNNNN,NNNNNNNN  (sent MR5800&M660)
 *   BINON:NNNNNNNN,NNNNNNNN,NNNNNNNN,NNNNNNNN  (sent M330)
 *
 *    ECHO NNNNNNNN,NNNNNNNN,NNNNNNNN,NNNNNNNN  (received MR5800&M660)
 *    ECHO:NNNNNNNN,NNNNNNNN,NNNNNNNN,NNNNNNNN  (received M330)
 *
 * where A is alphanumeric 
 *       N is a hexadecimal number
 *
 * for CR38199 (M660 handler):
 *   there is very special thing that the number of sites to report in
 *   BINON is dependent on TESTSET used! (test_tray_test_in_parallel)
 *   thus, if current testset is "16,4,4", so that the maximum 16 bin characters
 *   are accepted, i.e.
 *      "BINON AAAA5555,5555A555;\r\n"
 *   see handlerSetup()
 *
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

    /* find first space or colon */
    if(handlerID->model == PHFUNC_MOD_MR5800 || handlerID->model == PHFUNC_MOD_M660)
    {
      s=strchr(s,' ');
      r=strchr(r,' ');
    }
    else if(handlerID->model == PHFUNC_MOD_M330)
    {
      s=strchr(s,':');
      r=strchr(r,':');
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "in checkBinningInformation, handlerID->p.numOfSiteForBinON=%d",
                                 handlerID->p.numOfSiteForBinON);

    while ( s && *s &&
            r && *r &&
            binMatch!= handlerID->p.numOfSiteForBinON )
    {
        s=findHexadecimalDigit(s);
        r=findHexadecimalDigit(r);

        if ( isxdigit(*s) && isxdigit(*r) )
        {
            if ( tolower(*s) == tolower(*r) )
            {
                ++binMatch;
            }
            ++s;
            ++r;
        }

    }

    if ( binMatch != handlerID->p.numOfSiteForBinON )
    {
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
    char thisbin[64];
    int sendBinning = 0;
    int retryCount = 0;
    int repeatBinning = 0;
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering binDevice\n");

    do
    {
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

        switch ( handlerID->model )
        {
            case PHFUNC_MOD_MR5800:
            case PHFUNC_MOD_M660:
            case PHFUNC_MOD_M330:
                if(handlerID->model == PHFUNC_MOD_MR5800 || handlerID->model == PHFUNC_MOD_M660)
                {
                  strcpy(testresults, "BINON ");
                }
                else if ( handlerID->model == PHFUNC_MOD_M330 ) 
                {
                  strcpy(testresults, "BINON:");
                }

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "handlerID->p.numOfSiteForBinON=%d",
                                 handlerID->p.numOfSiteForBinON);
                for ( i= handlerID->p.numOfSiteForBinON-1 ; i>=0 && retVal==PHFUNC_ERR_OK ; --i )
                {
                    if ( i < handlerID->noOfSites &&
                         handlerID->activeSites[i] &&
                         (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
                          oldPopulation[i] == PHESTATE_SITE_POPDEACT) )
                    {
                        retVal=setupBinCode(handlerID, perSiteBinMap[i], thisbin);
                        if ( retVal==PHFUNC_ERR_OK )
                        {
                            strcat(testresults, thisbin);
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                             "will bin device at site \"%s\" to %s",
                                             handlerID->siteIds[i], thisbin);
                            sendBinning = 1;
                        }
                        else
                        {
                            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                                             "unable to send binning at site \"%s\"", 
                                             handlerID->siteIds[i]);
                            sendBinning = 0;
                        }
                    }
                    else
                    {
                        /* there's not a device here */
                        strcat(testresults, NO_BINNING_RESULT);
                        if ( i < handlerID->noOfSites )
                        {
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                             "no device to bin at site \"%s\"", 
                                             handlerID->siteIds[i]);
                        }
                    }
                    if ( i == 0 )
                    {
                        strcat(testresults, ";");
                    }
                    else if ( i % 8 == 0 )
                    {
                        strcat(testresults, ",");
                    }
                }
                if ( sendBinning )
                {
                    retVal = phFuncTaSend(handlerID, "%s%s", testresults, handlerID->p.eol);
                    if (retVal != PHFUNC_ERR_OK)
                    {
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "send failed, retVal = %d\n", retVal);
                    }
                }
                break;
            default: break;
        }

        /* verify binning, if necessary */
        if ( (handlerID->p.verifyBins) && (sendBinning) && (retVal ==  PHFUNC_ERR_OK) )
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                             "will perform bin verification ....");
            PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", querybins));

            retVal=checkBinningInformation(handlerID, testresults, querybins);

            if ( retVal == PHFUNC_ERR_OK )
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                 ".... verification succeeded");
                PhFuncTaCheck(phFuncTaSend(handlerID, "ECHOOK%s", 
                                           handlerID->p.eol));
            }
            else
            {
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
        if ( repeatBinning )
        {
            repeatBinning = 1;
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "will try to send bin data %d more time(s) before giving up",
                             1 + handlerID->p.verifyMaxCount - retryCount);

            /* we do a loop, now go back to stored interaction mark */
            phFuncTaRemoveToMark(handlerID);    
            phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                             "  binDevice, try again\n");
        }

    } while ( repeatBinning );

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Exiting binDevice\n");
    return retVal;
}


/* send temperature setup to handler, if necessary */
static phFuncError_t reconfigureTemperature(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    struct tempSetup tempData;
    int iValue = 0;
    static char handlerAnswer[512] = "";

    /* get all temperature configuration values */
    if ( !phFuncTempGetConf(handlerID->myConf, handlerID->myLogger,
                            &tempData) )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "temperature control configuration corrupt");
        return PHFUNC_ERR_CONFIG;
    }
    switch ( handlerID->model )
    {
        case PHFUNC_MOD_MR5800:
        case PHFUNC_MOD_M660:
        case PHFUNC_MOD_M330:
            /* if TEMPCTRL = STOP, do not send rest of temp commands */
            if ( (tempData.mode==PHFUNC_TEMP_STOP) || (tempData.mode==PHFUNC_TEMP_AMBIENT) ) {
                return retVal;
            }
            // set temperature
            // tempData.setpoint[0]
            PhFuncTaCheck(phFuncTaSend(handlerID, "SETTEMP %3.1f%s",
                                       tempData.setpoint[0],
                                       handlerID->p.eol));

            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            if ( retVal != PHFUNC_ERR_OK )
            {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
            else
            {
                if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
                     strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                     "\"SETTEMP %3.1f\" command returned an error\n",
                                     tempData.setpoint[0]);
                    retVal = PHFUNC_ERR_ANSWER;
                    return retVal;
                }
                else  // all is good
                {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                     "temp_setpoint set to = %3.1f", tempData.setpoint[0]);
                }
            }

            // set guard band - note that Mirae is symmetrical +/- value, not upper and lower
            // tempData.uguard[0]
            iValue = (int)tempData.uguard[0];
            PhFuncTaCheck(phFuncTaSend(handlerID, "SETBAND %d%s", 
                                       iValue,
                                       handlerID->p.eol));

            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            if ( retVal != PHFUNC_ERR_OK )
            {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
            else
            {
                if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
                     strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                     "\"SETBAND %d\" command returned an error\n",
                                     iValue);
                    retVal = PHFUNC_ERR_ANSWER;
                    return retVal;
                }
                else  // all is good
                {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                     "temp_upper_guard_band set to = %d", iValue);
                }
            }

            // set soak time
            iValue = (int)tempData.soaktime[0];
            PhFuncTaCheck(phFuncTaSend(handlerID, "SETSOAK %d%s", 
                                       iValue,
                                       handlerID->p.eol));

            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            if ( retVal != PHFUNC_ERR_OK )
            {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
            else
            {
                if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
                     strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                     "\"SETSOAK %d\" command returned an error\n",
                                     iValue);
                    retVal = PHFUNC_ERR_ANSWER;
                    return retVal;
                }
                else  // all is good
                {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                     "temp_soaktime set to = %d", iValue);
                }
            }
            break;
        default:
            retVal = PHFUNC_ERR_CONFIG;
            break;
    }
    /* all temperature values configured */
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
    if ( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
        retVal = PHFUNC_ERR_CONFIG;
    else
    {
        switch ( found )
        {
            case 1:
                /* configured "no" */
                switch ( handlerID->model )
                {
                    case PHFUNC_MOD_MR5800:
                    case PHFUNC_MOD_M660:
                    case PHFUNC_MOD_M330:
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                         "will deactivate bin verification mode");
                        handlerID->p.verifyBins = 0;
                        break;
                    default: break;
                }
                break;
            case 2:
                /* configured "yes" */
                switch ( handlerID->model )
                {
                    case PHFUNC_MOD_MR5800:
                    case PHFUNC_MOD_M660:
                    case PHFUNC_MOD_M330:
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
    if ( confError == PHCONF_ERR_OK )
        confError = phConfConfNumber(handlerID->myConf, PHKEY_BI_VERCOUNT, 
                                     0, NULL, &dRetryCount);
    if ( confError == PHCONF_ERR_OK )
        handlerID->p.verifyMaxCount = (int) dRetryCount;
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "\"%s\" not set: assume double retry",
                         PHKEY_BI_VERCOUNT);
        handlerID->p.verifyMaxCount = 2; /* default double retry */
    }

    return retVal;
}

static phFuncError_t sendSitemap(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phConfError_t confError;
    int numSites;
    int i;
    double dValue;
    int iValue;
    char cValue[12];
    char commandString[PHESTATE_MAX_SITES + 16];
    char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering sendSitemap\n");

    confError = phConfConfIfDef(handlerID->myConf, PHKEY_SI_STHSMAP);
    if ( confError != PHCONF_ERR_OK )
    {
        /* configuration not given */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "configuration parameter \"%s\" not given, SITEMAP command will not be sent.", PHKEY_SI_STHSMAP);
        return PHFUNC_ERR_OK;
    }

    strcpy(commandString, "SITEMAP ");
    numSites = handlerID->noOfSites;

    for ( i=0; i<numSites; i++ )
    {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_SI_STHSMAP,
                                     1, &i, &dValue);
        if ( confError != PHCONF_ERR_OK ) dValue = 0.0;
        iValue = (int)dValue;
        sprintf(cValue, "%d", iValue);
        strcat (commandString, cValue);
        if ( i < (numSites - 1) ) strcat (commandString, ",");
    }
    strcat(commandString, ";");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
//    localDelay_us(100000);
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(commandString, handlerAnswer, strlen(commandString)) != 0 ) // ignore any EOL added by handler
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "\"%s\" (length = %d) command returned an error:",
                             commandString, strlen(commandString));
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "\"%s\" (length = %d) was received.",
                             handlerAnswer, strlen(handlerAnswer));
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Exiting sendSitemap\n");
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
    if ( confError != PHCONF_ERR_OK )
    {
        /* configuration not given */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "configuration parameter \"%s\" not given, SITESEL command will not be sent.", PHKEY_SI_HSMASK);
        return PHFUNC_ERR_OK;
    }

    if ( handlerID->model == PHFUNC_MOD_MR5800 || handlerID->model == PHFUNC_MOD_M330) {
      strcpy(commandString, "SITESEL ");
    } else if ( handlerID->model == PHFUNC_MOD_M660 ) {
      strcpy(commandString, "CONTACTSEL ");
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "in sendSiteSel, incorrect handler model is specified");
      return PHFUNC_ERR_NA;
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
    while ( siteCtr >= 0 )
    {
      confError = phConfConfNumber(handlerID->myConf, PHKEY_SI_HSMASK, 1, &siteCtr, &dValue);
      if ( confError != PHCONF_ERR_OK ) dValue = 0.0;
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
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Exiting sendSitesel\n");
    return retVal;
}

//this function actually is specific to MR5800 plugin
static phFuncError_t sendTestSet (phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phConfError_t confError;
    int sendTestset = 1;
    double dValue = 0.0;
    int iValue = 0;
    int nibbleValue = 0;
    int bitCtr = 0;
    static char handlerAnswer[512] = "";

    /*************************************************************************/
    /* retrieve test tray configuration                                      */
    /*  Para = 32/64/128 (test in parallel)                                 */
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
    if ( confError == PHCONF_ERR_OK )
    {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_TESTSET_PARA,
                                    0, NULL, &dValue);
        if ( (confError == PHCONF_ERR_OK) )
        {
            iValue = (int)dValue; /* convert */
            handlerID->p.testset_parallel = iValue;
            if ( ! ((iValue == 32) || (iValue == 64) || (iValue == 128)) )
            {
                // warning: invalid value
//                handlerID->p.testset_parallel = 32; /* a safe value */
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                "Value for test_tray_test_in_parallel should be 128, 64, or 32,\n"
                                " and the value of test_tray_test_in_parallel * test_tray_step \n"
                                " should be equal to 128.\n");
//            " Value has been set to %d.", handlerID->p.testset_parallel);
            }
        }
        else
        {
            // error: invalid value
            handlerID->p.testset_parallel = 32; /* a safe value */
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                            "Value for test_tray_test_in_parallel should be 128, 64, or 32,\n"
                            " and the value of test_tray_test_in_parallel * test_tray_step \n"
                            " should be equal to 128.\n"
                            " Value has been set to %d.", handlerID->p.testset_parallel);
        }
    }
    else // Don't send!
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                        "configuration parameter \"%s\" not given, TESTSET command will not be sent.", PHKEY_FC_TESTSET_PARA);
        sendTestset = 0;
    }


    if ( sendTestset == 1 )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                        "test_tray_in_parallel set to = %d", handlerID->p.testset_parallel);

        confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_TESTSET_STEP);
        if ( confError == PHCONF_ERR_OK )
        {
            confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_TESTSET_STEP, 
                                        0, NULL, &dValue);
            if ( (confError == PHCONF_ERR_OK) )
            {
                iValue = (int)dValue; /* convert */
                handlerID->p.testset_step = iValue;
                switch ( iValue )
                {
                    case 1:
//                    if ( handlerID->p.testset_parallel == 128 )
//                    {
//                        handlerID->p.testset_step = iValue;
//                    }
                        break;
                    case 2:
//                    if ( handlerID->p.testset_parallel == 64 )
//                    {
//                        handlerID->p.testset_step = iValue;
//                    }
                        break;
                    case 4:
//                    if ( handlerID->p.testset_parallel == 32 )
//                    {
//                        handlerID->p.testset_step = iValue;
//                    }
                        break;
                    default:
                        // warning: invalid value
//                    handlerID->p.testset_step = 64/handlerID->p.testset_parallel;
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                        "Value for test_tray_step should be 1, 2, or 4, and \n,"
                                        " the value of test_tray_parallel * test_tray_step \n"
                                        " should be equal to 128.\n");
//                                     " Value has been set to %d.",
//                                     handlerID->p.testset_step);
                        break;
                }
            }
            else
            {
                // error: invalid value
                handlerID->p.testset_step = 4;
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                "Value for test_tray_step should be 1, 2, or 4, and \n,"
                                " the value of test_tray_parallel * test_tray_step \n"
                                " should be equal to 128.\n"
                                " Value has been set to %d.",
                                handlerID->p.testset_step);
            }
        }
        else // Default value
        {
            handlerID->p.testset_step = 4;
        }
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                        "test_tray_step set to = %d", handlerID->p.testset_step);

        confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_TESTSET_ROW);
        if ( confError == PHCONF_ERR_OK )
        {
            nibbleValue = 0;
            for ( bitCtr = 0; bitCtr < 4; ++bitCtr )
            {
                confError = phConfConfNumber(handlerID->myConf,
                                            PHKEY_FC_TESTSET_ROW, 1,
                                            &bitCtr, &dValue);
                if ( confError != PHCONF_ERR_OK ) dValue = 0.0;
                iValue = (int)dValue;
                iValue = iValue << bitCtr;
                nibbleValue += iValue;
            }
            handlerID->p.testset_row = nibbleValue;
        }
        else
        {
            handlerID->p.testset_row = 0xf; /* default to all four rows */
        }
        /* done with retrieve of test tray configuration                         */

        PhFuncTaCheck(phFuncTaSend(handlerID, "TESTSET %d,%d,%X%s",
                                  handlerID->p.testset_parallel,
                                  handlerID->p.testset_step,
                                  handlerID->p.testset_row,
                                  handlerID->p.eol));

        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        if ( retVal != PHFUNC_ERR_OK )
        {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
        else
        {
            if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
                 strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                "\"TESTSET %d,%d,%X\" command returned an error\n",
                                handlerID->p.testset_parallel,
                                handlerID->p.testset_step,
                                handlerID->p.testset_row);
                retVal = PHFUNC_ERR_ANSWER;
                return retVal;
            }
            else  // all is good
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                "test_tray_test_in_parallel set to = %d", handlerID->p.testset_parallel);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                "test_tray_step set to = %d", handlerID->p.testset_step);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                "test_tray_row set to = %X", handlerID->p.testset_row);
            }
        }
    }

    return retVal;
}

static phFuncError_t sendBinCategories(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    phConfError_t confError;
//    int numBins = 13;  // Fixed for Mirae5500
    int numBins = 15;  // Fixed for Mirae5800
    int i;
    int iValue;
    double dValue;
    char cValue[12];
    char commandString[PHESTATE_MAX_SITES + 16];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering sendBinCategories\n");

    confError = phConfConfIfDef(handlerID->myConf, PHKEY_BI_CATEGORIES);
    if ( confError != PHCONF_ERR_OK )
    {
        /* configuration not given */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "configuration parameter \"%s\" not given, SETBIN command will not be sent.", PHKEY_BI_CATEGORIES);
        return PHFUNC_ERR_OK;
    }

    strcpy(commandString, "SETBIN ");

    for ( i=0; i < numBins; i++ )
    {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_BI_CATEGORIES,
                                     1, &i, &dValue);
        if ( confError != PHCONF_ERR_OK ) dValue = 0.0;
        iValue = (int)dValue;
        handlerID->siteCategories[i] = iValue;
        sprintf(cValue, "%d", iValue);
        strcat (commandString, cValue);
        if ( i < (numBins - 1) ) strcat (commandString, ",");
    }
    strcat(commandString, ";");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
//    localDelay_us(100000);
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, retVal = : %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0  &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Exiting sendBinCategories\n");
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
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }

    localDelay_us(100000);

    return retVal;
}

static phFuncError_t handlerSetup(
                                 phFuncId_t handlerID    /* driver plugin ID */
                                 )
{
    int found = 0;
    phConfError_t confError;
    double dPollingInterval;
    phFuncError_t retVal = PHFUNC_ERR_OK;

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "entering handlerSetup");


    /* chose polling or SRQ interrupt mode */
    phConfConfStrTest(&found, handlerID->myConf, PHKEY_FC_WFPMODE,
                      "polling", "interrupt", NULL);
    if ( found == 1 )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "activated strict polling mode, no SRQ handling");
        handlerID->p.strictPolling = 1;
    }
    else
    {
        /* default */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "activated SRQ based device handling");
        handlerID->p.strictPolling = 0;
    }

    /* retrieve polling interval */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_POLLT);
    if ( confError == PHCONF_ERR_OK )
        confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_POLLT,
                                     0, NULL, &dPollingInterval);
    if ( confError == PHCONF_ERR_OK )
        handlerID->p.pollingInterval = labs((long) dPollingInterval);
    else
    {
        handlerID->p.pollingInterval = 200000L; /* default 0.2 sec */
    }

    confError = phConfConfStrTest(&found, handlerID->myConf,
                                  PHKEY_OP_PAUPROB, "yes", "no", NULL);
    if ( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
        retVal = PHFUNC_ERR_CONFIG;
    else
    {
        if ( found == 1 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                             "remote stopping is not (yet) supported by Mirae handler");
        }
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "handlerSetup, calling reconfigureTemperature");
    PhFuncTaCheck(reconfigureTemperature(handlerID));

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "handlerSetup, calling reconfigureVerification");
    PhFuncTaCheck(reconfigureVerification(handlerID));


    if ( handlerID->model == PHFUNC_MOD_MR5800 || handlerID->model == PHFUNC_MOD_M330) {
      if(handlerID->model == PHFUNC_MOD_MR5800) 
      {
        //send TESTSET command
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "handlerSetup, calling sendTestSet");
        PhFuncTaCheck(sendTestSet(handlerID));

        // Send SITEMAP command
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "handlerSetup, calling sendSitemap");
        PhFuncTaCheck(sendSitemap(handlerID));
      }

      // Send SITESEL command

      /* check if PHKEY_PL_MIRAE_SEND_SITESEL is already defined. */
      confError = phConfConfIfDef(handlerID->myConf, PHKEY_PL_MIRAE_SEND_SITESEL);
      if ( confError == PHCONF_ERR_OK ){
        /*
        * if mirae_send_siteselection_cmd is set to yes, then set
        * sendSiteSelection to YES to call sendSitesel().
        * if mirae_send_siteselection_cmd is set to no, then set
        * sendSiteSelection to NO to not to call sendSitesel().
        */
        confError = phConfConfStrTest(&found, handlerID->myConf, PHKEY_PL_MIRAE_SEND_SITESEL,
                                      "yes", "no", NULL);
        if ( confError != PHCONF_ERR_OK ) {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                          "configuration value of \"%s\" must be either \"yes\" or \"no\"\n",
                          PHKEY_PL_MIRAE_SEND_SITESEL);
          retVal = PHFUNC_ERR_CONFIG;
        } else {
          switch(found) {
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

      if ( handlerID->p.sendSiteSelection == YES ){
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                        "handlerSetup, calling sendSitesel");
        PhFuncTaCheck(sendSitesel(handlerID));
      }

      if(handlerID->model == PHFUNC_MOD_MR5800)
      {
        // Send SETBIN command
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                        "handlerSetup, calling sendBinCategories");
        PhFuncTaCheck(sendBinCategories(handlerID));
      }
    }

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

    /*
     * retrieve the value of test_tray_test_in_parallel, and
     * set the site number for BINON.
     *
     * the default value
     *   MR5800: 32
     *   M660: 16
     */

    int bTestsetParaSpecified = 0;
    double dValue = 0.0;
    phConfError_t confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_TESTSET_PARA);
    if ( confError == PHCONF_ERR_OK ) {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_TESTSET_PARA,
                                    0, NULL, &dValue);       
        if ( (confError == PHCONF_ERR_OK) ) {
            handlerID->p.testset_parallel = (int)dValue;;
            bTestsetParaSpecified = 1;
        }
    }

    if ( bTestsetParaSpecified == 0 ) {
        if ( handlerID->model == PHFUNC_MOD_MR5800 ) {
            handlerID->p.testset_parallel = 32;
            //handlerID->p.testset_step = 4;
            //handlerID->p.testset_row = 1;
        } else if ( handlerID->model == PHFUNC_MOD_M660 ) {
            handlerID->p.testset_parallel = 16;
            //handlerID->p.testset_step = 4;
            //handlerID->p.testset_row = 1;
        } else {
            handlerID->p.testset_parallel = 0;  //otherwise
            //handlerID->p.testset_step = 0;
            //handlerID->p.testset_row = 0;
        }
    }

    if(handlerID->model == PHFUNC_MOD_M330)
    {
      handlerID->p.numOfSiteForBinON = handlerID->noOfSites;
    }
    else
    {
      /* always let the site number for BINON be equal to TESTSET setting*/
      handlerID->p.numOfSiteForBinON = handlerID->p.testset_parallel;
    }
 
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "in doReconfigure, handlerID->p.numOfSiteForBinON=%d",
                                 handlerID->p.numOfSiteForBinON);

    if ( firstTime == 1 )
    {
        retVal = checkHandlerRunning(handlerID, &handlerRunning);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "doReconfigure (first time): FR? handler running returns \"%s\" ",
                         handlerRunning);

        // only send setup if handler not running
        if ( retVal == PHFUNC_ERR_OK )
        {
            if ( strcasecmp(handlerRunning, "FR 0") == 0 || strcasecmp(handlerRunning, "FR0") == 0 ) // handler stopped
            {
                PhFuncTaCheck(handlerSetup(handlerID));
            } else if ( handlerID->model == PHFUNC_MOD_M660 )
            {
              /*
               * though M660 handler is running, we still do the configure for the binVerfication
               * this is specially designed for the M660 handler
               */
               phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "in doReconfigure, always calling reconfigureVerification for M660 handler though it is running");
               PhFuncTaCheck(reconfigureVerification(handlerID));
            }
            else
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "in doReconfigure: handler is running, will not be reconfigured.");
            }
            firstTime = 0;  // only count as firstTime if successfully sent FR?
        }
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "repeat call to doReconfigure, handler will not be reconfigured.");
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "exiting doReconfigure");

    return retVal;
}


/*****************************************************************************
 * the following functions are called by privateGetStatus to retrieve
 * the status or configuration from the handler
 ***************************************************************************/
#ifdef GETSTATUS_IMPLEMENTED

/*****************************************************************************
 *
 * Get Status Handler VERSION?
 *
 *         ---> "VERSION?"
 *         <--- "MRxxx Rev.x.x.x", example:
 *              "M660 Rev.1.0.1"
 *
 * Authors: Ken Ward, Jiawei Lin
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
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "VERSION?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
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
 *       ----> "NAME?"
 *       <---- "Name MR9908RDRAM"
 *
 * Authors: Ken Ward, Jiawei Lin
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
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusName\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "NAME?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "Name %s", argumentString);

    return retVal;
}




/*****************************************************************************
 *
 * Get Status Handler TESTSET?
 *
 * Authors: Ken Ward, Jiawei Lin
 *
 *         ---> "TESTSET?"
 *         <--- "TestSet para,step,row"  example:
 *              "TestSet 32,2,4"
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
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusTestset\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "TESTSET?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "TestSet %s", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler SETTEMP?
 *
 *       ---> "SETTEMP?"
 *       <--- "SetTemp +ddd.d" or "SetTemp -ddd.d"
 *
 * Authors: Ken Ward, Jiawei Lin
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
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering privateGetStatusSettemp\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "SETTEMP?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "SetTemp %s", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler MEASTEMP d?
 *        here the "d" value ranges in:
 *                  1: the average temperature of the soaking chamber
 *                  2: the average temperature of the test site
 *                  3: the average temperature of the defrosting chamber
 *
 *   ---> "MEASTEMP 1?" ( ask the soaking chamber's temperature)
 *   <--- "MeasTemp 1 +ddd.d" or "MeasTemp 1 -ddd.d"
 *
 * Authors: Ken Ward
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusMeastemp(phFuncId_t handlerID, const char *parm, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusVersion\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    sprintf(commandString, "MEASTEMP %s?", parm);

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "MeasTemp %*s %s", argumentString); //ignore the test site

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler STATUS?
 *
 * Authors: Ken Ward / Jiawei Lin
 *
 *    ---> "HDLSTATUS?"
 *    <--  "HDLStatus 0001"
 * or
 *    ---> "STATUS?"
 *    <--- "STATUS 0001"
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusStatus(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[32];
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering privateGetStatusStatus\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;


    if ( handlerID->model == PHFUNC_MOD_MR5800 || handlerID->model == PHFUNC_MOD_M330 ) {
      strcpy(commandString, "STATUS?");
    } else if ( handlerID->model == PHFUNC_MOD_M660 ) {
      strcpy(commandString, "HDLSTATUS?");
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "in privateGetStatusStatus, incorrect handler model is specified");
      return PHFUNC_ERR_NA;
    }

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    if ( handlerID->model == PHFUNC_MOD_MR5800 || handlerID->model == PHFUNC_MOD_M330 ) {
      sscanf(handlerAnswer, "Status %s", argumentString);
    } else if ( handlerID->model == PHFUNC_MOD_M660 ) {
      sscanf(handlerAnswer, "HDLStatus %s", argumentString);
    }

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler JAM?
 *
 *   --->  "JAM?"
 *   <---  "Jam 0"
 *
 * Authors: Ken Ward, Jiawei Lin
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
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusJam\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "JAM?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "Jam %s", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler JAMCODE?
 *
 *  ---> "JAMCODE?"
 *  <--- "JamCode 120161 17"
 *
 * Authors: Ken Ward, Jiawei Lin
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
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering privateGetStatusJamcode\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "JAMCODE?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "JamCode %4095[^\r\n]", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler JAMQUE?
 *
 *    ---> "JAMQUE?"
 *    <--- "JamQue 120161, 030205, 020301"
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
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusJamQue\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "JAMQUE?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "JamQue %4095[^\r\n]", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler JAMCOUNT?
 *
 *    ---> "JAMCOUNT?"
 *    <--- "JamCount 3"
 *
 * Authors: Ken Ward, Jiawei Lin
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
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusJamcount\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "JAMCOUNT?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "JamCount %s", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler SETLAMP?
 *
 *      ---> "SETLAMP?"
 *      <--- "SetLamp 123"
 *
 * Authors: Ken Ward, Jiawei Lin
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
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusSetlamp\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "SETLAMP?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "SetLamp %s", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler LOTERRCODE?
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 *   --->  "LOTERRCODE?"
 *   <---  "ERROR0"
 *      or "ERROR4:1-03002,2-040002,3-050002,4-06002"
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusLotErrCode(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    static char handlerAnswer[MAX_STATUS_MSG] = "";
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusLotErrCode\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "LOTERRCODE?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler LOTALMCODE?
 *
 *   ---> "LOTALMCODE?"
 *   <--- "LOTALMCODE 1"
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusLotAlmCode(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering privateGetStatusLotAlmCode\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "LOTALMCODE?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "LOTALMCODE %s", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler LOTDATA?
 *
 *   ---> "LOTDATA?"
 *   <--- "times Operation_times:HiAlarm_count:LowAlarm_count"  example
 *        "times 2260:0:0"
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusLotData(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    static char handlerAnswer[MAX_STATUS_MSG] = "";
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusLotData\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "LOTDATA?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated

    return retVal;
}

/*****************************************************************************
 *
 * Get Status Handler ACCSORTDVC?
 *
 *   ---> "ACCSORTDVC?"
 *   <--- "TOTAL 65403 ; 19782 ; 52687 ; ......"
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusAccSortDvc(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    static char handlerAnswer[MAX_STATUS_MSG] = "";
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering privateGetStatusAccSortDvc\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "ACCSORTDVC?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated

    return retVal;
}
/*****************************************************************************
 *
 * Get Status Handler ACCJAMCODE?
 *
 *   ---> "ACCJAMCODE?"
 *   <--- "JAM0"
 *    or  "JAM4:1-030001,2-040001,3-050001,4-060001"
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusAccJamCode(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    static char handlerAnswer[MAX_STATUS_MSG] = "";
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering privateGetStatusAccJamCode\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "ACCJAMCODE?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated

    return retVal;
}

/*****************************************************************************
 *
 * Get Status Handler ACCERRCODE?
 *
 *   ---> "ACCERRCODE?"
 *   <--- "ERROR0"
 *     or "ERROR4:1-030002,2-040002,3-050002,4-060002"
 *
 * Authors: H.Kerschbaum
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusAccErrCode(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    static char handlerAnswer[MAX_STATUS_MSG] = "";
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusAccErrCode\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "ACCERRCODE?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated

    return retVal;
}

/*****************************************************************************
 *
 * Get Status Handler ACCDATA?
 *
 *   ---> "ACCDATA?"
 *   <--- "times Operation_times;HiAlarm_count;LowAlarm_count" for example
 *        "times 2600;0;0"
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusAccData(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    static char handlerAnswer[MAX_STATUS_MSG] = "";
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusAccData\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "ACCDATA?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler TRAY_ID?
 *
 *   ---> "TRAY_ID?"
 *   <--- "TRAY_ID 4"
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusTrayId(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusTrayId\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "TRAY_ID?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "TRAYID %s", argumentString);

    return retVal;
}

/*****************************************************************************
 *
 * Get Status Handler TEMPMODE? (temperature control mode)
 *
 *   ---> "TEMPMODE?"
 *   <--- "TEMPMODE 0" (no temperature control)
 *     or "TEMPMODE 1" (hot mode)
 *     or "TEMPMODE 2" (room mode, i.e. ambient mode)
 *     or "TEMPMODE 3" (cold mode, i.e. cca mode)
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusTempMode(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    char handlerAnswer[MAX_STATUS_MSG] = "";
    static char argumentString[MAX_STATUS_MSG] = "";
    argumentString[0] = '\0';
    *responseString = argumentString;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateGetStatusTempMode\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "TEMPMODE?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated
    sscanf(handlerAnswer, "TEMPMODE %s", argumentString);

    return retVal;
}


/*****************************************************************************
 *
 * Get Status Handler LOTTOAL2?
 *
 *   ---> "LOTTOTAL2?"
 *   <--- "TOTAL_3024 ; 170 ; 3024 ......"
 *
 * Authors: H.Kerschbaum
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusLotTotal(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    static char handlerAnswer[MAX_STATUS_MSG] = "";
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering privateGetStatusLotTotal\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */


    strcpy(commandString, "LOTTOTAL2?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated

    return retVal;
}

/*****************************************************************************
 *
 * Get Status Handler ALMRECORD?
 *
 *   ---> "ALMRECORD?"
 *   <--- "-tbd-"
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateGetStatusAlarmRecord(phFuncId_t handlerID, char **responseString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[24];
    static char handlerAnswer[MAX_STATUS_MSG] = "";
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                     "Entering privateAlarmRecord\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    flushSRQs(handlerID); /* flush the JAM srq if not already receieved */

    strcpy(commandString, "ALMRECORD?");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\n\r]", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "%s command returned an error, return code =: %d\n",
                         commandString, retVal);
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, 
                         "Sent: %s\nReceived: %s", commandString, handlerAnswer);
    }

    strcat(handlerAnswer, "\0");  // ensure null terminated

    return retVal;
}

#endif



/*****************************************************************************
 * the following functions are called by privateSetStatus to retrieve
 * the status or configuration from the handler
 ***************************************************************************/
#ifdef SETSTATUS_IMPLEMENTED

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
static phFuncError_t privateSetStatusStart(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[12];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusStart\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "START");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}



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
static phFuncError_t privateSetStatusStop(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[12];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusStop\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "STOP");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}




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
static phFuncError_t privateSetStatusLoader(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[12];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusLoader\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "LOADER");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}




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
static phFuncError_t privateSetStatusPlunger(phFuncId_t handlerID, int contactorONOFF)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char cValue [4];
    char commandString[12];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusPlunger\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "PLUNGER ");
    if ((contactorONOFF == 0) || (contactorONOFF == 1))
    {
        sprintf(cValue, "%d", contactorONOFF);
        strcat(commandString, cValue);
    }
    else
    {
        retVal = PHFUNC_ERR_CONFIG;
        return retVal;
    }


    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}




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
static phFuncError_t privateSetStatusName(phFuncId_t handlerID, const char *nameString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[128];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusName\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    if (nameString == NULL)
    {
        retVal = PHFUNC_ERR_CONFIG;
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "privateSetStatusName invalid argument: NULL");
        return retVal;
    }
    if ((strlen(nameString) == 0) || (strlen(nameString) > 12))
    {
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
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}


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
static phFuncError_t privateSetStatusRunmode(phFuncId_t handlerID, int mode)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char cValue [4];
    char commandString[12];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusRunmode\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "RUNMODE ");
    if ((mode == 0) || (mode == 1))
    {
        sprintf(cValue, "%d", mode);
        strcat(commandString, cValue);
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3,
                         "in setStatusRunmode, invalid parameter: mode = %d\n", mode);
        retVal = PHFUNC_ERR_CONFIG;
        return retVal;
    }


    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}


/*****************************************************************************
 *
 * Clear Lot Counters
 *
 * Authors: H.Kerschbaum
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateSetStatusLotClear(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[12];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusLotClear\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "LOTCLEAR");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}

/*****************************************************************************
 *
 * Clear Accumulated Counters
 *
 * Authors: H.Kerschbaum
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateSetStatusAccClear(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[12];
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusAccClear\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    strcpy(commandString, "ACCCLEAR");

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}


/*****************************************************************************
 *
 * Set handler status of Contact Selector Mask
 *
 * Authors: H.Kerschbaum
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateSetStatusContactSel(phFuncId_t handlerID, const char *maskString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[128] = "";
    static char handlerAnswer[512] = "";

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering setStatusContactSel\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    if ((strlen(maskString) == 0) || (strlen(maskString) > 64))
    {
        retVal = PHFUNC_ERR_CONFIG;
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "privateSetStatusContactSel invalid argument: %s",
                         maskString);
        return retVal;
    }

    if ( handlerID->model == PHFUNC_MOD_MR5800 || handlerID->model == PHFUNC_MOD_M330 ) {
      strcpy(commandString, "SITESEL ");
    } else if ( handlerID->model == PHFUNC_MOD_M660 ) {
      strcpy(commandString, "CONTACTSEL ");
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "in privateSetStatusContactSel, unsupported handler model is specified");
      return PHFUNC_ERR_NA;
    }
    strcat(commandString, maskString);

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK )
    {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    }
    else
    {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        }
        else phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
    }
    return retVal;
}

/*****************************************************************************
 *
 * Set handler status of Temperature Control mode
 *
 * Authors: H.Kerschbaum, Jiawei Lin
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
static phFuncError_t privateSetStatusTempCtrl(phFuncId_t handlerID, const char *modeString)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char commandString[80];
    static char handlerAnswer[512] = "";
    int iTempMode = -1;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "Entering privateSetStatusTempCtrl\n");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) ) {
      return PHFUNC_ERR_ABORTED;
    }

    if ( strcasecmp(modeString, "off") == 0 ) {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "temperature control now turned off");
      iTempMode = PHFUNC_TEMP_STOP;
    } else if ( strcasecmp(modeString, "hot") == 0 ) {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                       "temperature control will be now set to \"hot\" by the driver");
      iTempMode = PHFUNC_TEMP_HOT;
    } else if ( (strcasecmp(modeString, "ambient")==0) || (strcasecmp(modeString, "room")==0) ) {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                       "temperature control will be now set to \"ambient\" or \"room\" by the driver");
      iTempMode = PHFUNC_TEMP_AMBIENT;
    } else if ( (strcasecmp(modeString, "cca")==0) || (strcasecmp(modeString, "cold")==0) ) {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                       "temperature control will be now set to \"cca\" or \"cold\" by the driver");
      iTempMode = PHFUNC_TEMP_CCA;
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "privateSetStatusTempCtrl, the parameter: %s is unknown;\n"
                         "the valid value is 'off', 'ambient', 'hot' and 'cca'");
      return PHFUNC_ERR_NA;
    }

    sprintf(commandString, "%s %d", "TEMPCTRL", iTempMode);

    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", commandString, handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if ( retVal != PHFUNC_ERR_OK ) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
    } else {
        if ( strncasecmp(MIRAE_OK, handlerAnswer, strlen(MIRAE_OK)) != 0 &&
             strncasecmp(MIRAE_DONE, handlerAnswer, strlen(MIRAE_DONE)) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "%s command returned an error: %s\n",
                             commandString, handlerAnswer);
            retVal = PHFUNC_ERR_ANSWER;
        } else {
          phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE, "Sent: %s", commandString);
        }
    }

    return retVal;
}

#endif


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
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    /* do some really initial settings */
    handlerID->p.sites = handlerID->noOfSites;
    for ( i=0; i<handlerID->p.sites; i++ )
    {
        handlerID->p.siteUsed[i] = handlerID->activeSites[i];
        handlerID->p.devicePending[i] = 0;
        handlerID->p.deviceExpected[i] = 0;
    }
    handlerID->p.verifyBins = 0;
    handlerID->p.verifyMaxCount = 0;

    handlerID->p.strictPolling = 0;
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
    handlerID->p.status = 0L;
    handlerID->p.stopped = 0;
    handlerID->p.runmode = 0; // "normal"
    handlerID->p.testset_parallel = 0;
    handlerID->p.numOfSiteForBinON = 0; //as above
    handlerID->p.testset_step = 0;
    handlerID->p.testset_row = 0;
    handlerID->binIds = NULL;
    handlerID->p.sendSiteSelection = NO;

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
    if ( phFuncTaAskAbort(handlerID) )
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
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);

    if ( (timeoutFlag == 0) && ( handlerID->interfaceType == PHFUNC_IF_GPIB ) )
    {
        // Make this look like a repeated call, so the abort will work
        phFuncTaSetCall(handlerID, PHFUNC_AV_LOT_START);
        // Same as ph_hfunc.c calling:
        // RememberCall(handlerID, PHFUNC_AV_LOT_START);
    }

    retVal = waitForLotStart(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateLotStart, after waitForLotStart, retVal = %d\n", retVal);

    while ( (timeoutFlag == 0) && (retVal != PHFUNC_ERR_OK) )
//    while ( (timeoutFlag == 0) && (retVal == PHFUNC_ERR_WAITING) )
    {
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
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

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
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    /* remember which devices we expect now */
    for ( i=0; i<handlerID->noOfSites; i++ )
        handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

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
    if ( !handlerID->p.oredDevicePending )
    {
        /* during the next call everything up to here should be
               repeated */
        phFuncTaRemoveToMark(handlerID);

        return PHFUNC_ERR_WAITING;
    }

    phFuncTaStop(handlerID);

    /* we have received devices for test. Change the equipment
       specific state now */
    handlerID->p.oredDevicePending = 0;
    for ( i=0; i<handlerID->noOfSites; i++ )
    {
        if ( handlerID->activeSites[i] == 1 )
        {
            if ( handlerID->p.devicePending[i] )
            {
                handlerID->p.devicePending[i] = 0;
                population[i] = PHESTATE_SITE_POPULATED;
            }
            else
                population[i] = PHESTATE_SITE_EMPTY;
        }
        else
        {
            if ( handlerID->p.devicePending[i] )
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
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    /* get current site population */
    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

    phFuncTaStart(handlerID);
    PhFuncTaCheck(binDevice(handlerID, oldPopulation, perSiteBinMap));
    phFuncTaStop(handlerID);

    /* modify site population, everything went fine, otherwise we
       would not reach this point */
    for ( i=0; i<handlerID->noOfSites; i++ )
    {
        if ( handlerID->activeSites[i] && 
             (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
              oldPopulation[i] == PHESTATE_SITE_POPDEACT) )
        {
            population[i] = 
            oldPopulation[i] == PHESTATE_SITE_POPULATED ?
            PHESTATE_SITE_EMPTY : PHESTATE_SITE_DEACTIVATED;
        }
        else
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
    switch ( action )
    {
        case PHFUNC_SR_QUERY:
            /* we can handle this query globaly */
            if ( lastCall )
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

    if ( phFuncTaAskAbort(handlerID) )
    {
        retVal = PHFUNC_ERR_ABORTED;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "privateCommTest: call to phFuncTaAskAbort returned TRUE\n");
        if ( testPassed != NULL )
            *testPassed = 0;
        return retVal;
    }

    phFuncTaStart(handlerID);

    retVal = checkHandlerRunning(handlerID, &handlerRunning);

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateCommTest: FR? handler running returns \"%s\", retVal = %d",
                     handlerRunning, retVal);

    if (retVal != PHFUNC_ERR_OK)
    {
        if ( testPassed != NULL )
            *testPassed = 0;
        return retVal;
    }

    phFuncTaStop(handlerID);

    /* return result, there is no real check for the model here (so far) */
    if ( testPassed != NULL )
        *testPassed = 1;
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
/*****************************************************************************
 *
 * the interface function to set the configuration for handler
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *   this function is called by phFuncSetStatus
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSetStatus(phFuncId_t handlerID,
  const char *token,
  const char *param
)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  if ( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_NAME) == 0 ) {
    if (strlen(param) > 0) {
      retVal = privateSetStatusName(handlerID, param);
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                      "privateSetStatus SETNAME, no param specified");
      retVal = PHFUNC_ERR_NA;
    }
  } else if ( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_LOADER) == 0 ) {
    retVal = privateSetStatusLoader(handlerID);
  } else if ( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_START) == 0 ) {
    retVal = privateSetStatusStart(handlerID);
  } else if ( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_STOP) == 0 ) {
    retVal = privateSetStatusStop(handlerID);
  } else if ( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PLUNGER) == 0 ) {
    int iValue = 0;
    if ( sscanf(param, "%d", &iValue) == 1 ) {
      retVal = privateSetStatusPlunger(handlerID, iValue);
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                      "privateSetStatus PLUNGER, no param specified");
      retVal = PHFUNC_ERR_NA;
    }
  } else if ( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_RUNMODE) == 0 ) {
    int iValue = 0;
    if ( sscanf(param, "%d", &iValue) == 1 ) {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3,
                      "in privateSetStatus, RUNMODE, param = %d\n", iValue);
      retVal = privateSetStatusRunmode(handlerID, iValue);
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3,
                      "privateSetStatus RUNMODE, no param specified");
      retVal = PHFUNC_ERR_NA;
    }
  } else if ( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_LOTCLEAR) == 0 ) {
    retVal = privateSetStatusLotClear(handlerID);
  } else if ( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_ACCCLEAR) == 0 ) {
    retVal = privateSetStatusAccClear(handlerID);
  } else if ( (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_CONTACTSEL)==0) ||
              (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_SITESEL)==0) ) {
    if (strlen(param) > 0) {
      retVal = privateSetStatusContactSel(handlerID, param);
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                      "privateSetStatus CONTACTSEL, no param specified");
      retVal = PHFUNC_ERR_NA;
    }
  } else if ( (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_TEMPMODE)==0) ||
              (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_TEMPERATURE_TEMPCONTROL)==0) ) {
    if (strlen(param) > 0) {
      retVal = privateSetStatusTempCtrl(handlerID, param);
    } else {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                      "privateSetStatus TEMPCTRL (temperature control mode), no param specified");
      retVal = PHFUNC_ERR_NA;
    }
  } else {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3,
                    "in privateSetStatus, token: ->%s<- not implemented for Mirae driver.\n", token);
    retVal = PHFUNC_ERR_NA; 
  }

  /* CR42623/CR33707 Xiaofei Han, make the behavior of Mirae consistent with other driver plugins */
  if(retVal == PHFUNC_ERR_NA)
  {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                    "The command \"%s %s\" is invalid or not supported yet", token, param);
    retVal = PHFUNC_ERR_OK;
  }


  return retVal;

}
#endif /* SETSTATUS_IMPLEMENTED */


#ifdef GETSTATUS_IMPLEMENTED

/*****************************************************************************
 *
 * the interface function to retrive the status from handler
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *   this function is called by the phFuncGetStatus
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateGetStatus(
  phFuncId_t handlerID,
  const char *commandString,
  const char *paramString,
  char **answer)
{
  static char response[MAX_STATUS_MSG] = "";
  phFuncError_t retVal = PHFUNC_ERR_OK;

  if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_VERSION) == 0 )
  {
    retVal = privateGetStatusVersion(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_NAME) == 0 )
  {
    retVal = privateGetStatusName(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_TESTSET) == 0 )
  {
    retVal = privateGetStatusTestset(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_SETTEMP) == 0 )
  {
    retVal = privateGetStatusSettemp(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_MEASTEMP) == 0 )
  {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                    "privateGetStatus MEASTEMP, params ->%s<-", paramString);
    if ( strlen(paramString) > 0 )
    {
      retVal = privateGetStatusMeastemp(handlerID, paramString, answer);
    }
    else
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                      "privateGetStatus MEASTEMP, no param specified");
      retVal = PHFUNC_ERR_NA;
    }
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_STATUS) == 0 )
  {
    retVal = privateGetStatusStatus(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_JAM) == 0 )
  {
    retVal = privateGetStatusJam(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_JAMCODE) == 0 )
  {
    retVal = privateGetStatusJamcode(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_JAMQUE) == 0 )
  {
    retVal = privateGetStatusJamQue(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_JAMCOUNT) == 0 )
  {
    retVal = privateGetStatusJamcount(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_SETLAMP) == 0 )
  {
    retVal = privateGetStatusSetlamp(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_LOTERRCODE) == 0 )
  {
    retVal = privateGetStatusLotErrCode(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_LOTALMCODE) == 0 )
  {
    retVal = privateGetStatusLotAlmCode(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_LOTDATA) == 0 )
  {
    retVal = privateGetStatusLotData(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_ACCSORTDVC) == 0 )
  {
    retVal = privateGetStatusAccSortDvc(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_ACCJAMCODE) == 0 )
  {
    retVal = privateGetStatusAccJamCode(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_ACCERRCODE) == 0 )
  {
    retVal = privateGetStatusAccErrCode(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_ACCDATA) == 0 )
  {
    retVal = privateGetStatusAccData(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_TRAYID) == 0 )
  {
    retVal = privateGetStatusTrayId(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_ALARMRECORD) == 0 )
  {
    retVal = privateGetStatusAlarmRecord(handlerID, answer);
  }
  else if ( (strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_TEMPMODE)==0) ||
            (strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPCONTROL)==0) )
  {
    /**
     * the key could be "tempmode" or "temperature_tempcontrol", i.e.
     *  abc123=PROB_HND_CALL(ph_get_status tempmode)
     *  abc456=PROB_HND_CALL(ph_get_status temperature_tempcontrol)
     *  will return the same value
     */
    retVal = privateGetStatusTempMode(handlerID, answer);
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_HANDLER_STATUS_GET_LOTTOTAL) == 0 )
  {
    retVal = privateGetStatusLotTotal(handlerID, answer);
  } else {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3,
                    "in privateGetStatus, token: ->%s<- not implemented for Mirae driver.\n", commandString);
    retVal = PHFUNC_ERR_NA; 
  }

  /* CR42623/CR33707 Xiaofei Han, make the behavior of Mirae consistent with other driver plugins */
  if(retVal == PHFUNC_ERR_NA)
  {
    sprintf(response, "%s_KEY_NOT_AVAILABLE", commandString);
    *answer = response;
    retVal = PHFUNC_ERR_OK;
  }

  return retVal;
}
#endif /* GETSTATUS_IMPLEMENTED */

/*****************************************************************************
 * End of file
 *****************************************************************************/
