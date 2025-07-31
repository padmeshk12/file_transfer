/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 29 Dec 2006
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Kun Xiao, STS R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Dec 2006, Kun Xiao, created
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
 *   SRQ Status bits
 *   SRQ position b3 - Error of transmission
 *   SRQ position b2 - Alarm
 */

#define HYAC_SRQ_ERROR (1L << 3)
#define HYAC_SRQ_ALARM (1L << 2)

/* 
 *   Place filler for BINON command where there is no operational site 
 */

#define NO_BINNING_RESULT  "0"


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
static const char *findHexadecimalDigit(const char *s)
{
    while( *s && !isxdigit(*s) )
    {
        ++s;
    }

    return s;
}

static int localDelay_us(long microseconds) 
{
    long seconds;
    struct timeval delay;
 
    /* us must be converted to seconds and microseconds ** for use by select(2) */
    if (microseconds >= 1000000L) 
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
    if (select(0, NULL, NULL, NULL, &delay) == -1 &&
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

    do
    {
	PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 1));
	phFuncTaRemoveStep(handlerID);
    } while (received);

    return retVal;
}


/* create descriptive status string */
static char *statusString(phFuncId_t handlerID)
{
    static char statusMessage[1024];
    long status = handlerID->p.status;

    statusMessage[0] = '\0';

    switch (handlerID->model)
    {
      case PHFUNC_MOD_HYAC8086:
        if (status & HYAC_SRQ_ALARM)
        {
            strcat(statusMessage, "handler status: Alarm");
        }
        if (status & HYAC_SRQ_ERROR)
        {
            if (status & HYAC_SRQ_ALARM)
            {
                strcat(statusMessage, " & Error of transmission");
            }
            else
            {
                strcat(statusMessage, "handler status: Error of transmission");
            }
        }
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

    if (handlerID->p.status != 0L && warn)
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
            "%s", message);
}


/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    static char handlerAnswer[512] = "";
    unsigned long population;
    unsigned long populationPicked = 0L;
    int i;

    switch(handlerID->model)
    {
      case PHFUNC_MOD_HYAC8086:
	    PhFuncTaCheck(phFuncTaSend(handlerID, "FULLSITES?%s", handlerID->p.eol));
//	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer));
        retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
        if (retVal != PHFUNC_ERR_OK) {
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
    if (sscanf(handlerAnswer, "FULLSITES %lx", &population) != 1)
    {
	phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
	    "site population not received from handler: \n"
            "received \"%s\" format expected \"FULLSITES xxxxxxxx\" \n",
            handlerAnswer);
        handlerID->p.oredDevicePending = 0;
	return PHFUNC_ERR_ANSWER;
    }

    /* correct pending devices information, overwrite all (trust the
       last handler query) */
    handlerID->p.oredDevicePending = 0;
    for (i=0; i<handlerID->noOfSites; i++)
    {
	if (population & (1L << i))
	{
	    handlerID->p.devicePending[i] = 1;
	    handlerID->p.oredDevicePending = 1;
	    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
		"device present at site \"%s\" (polled)", 
		handlerID->siteIds[i]);
	    populationPicked |= (1L << i);
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
    
    if (population != populationPicked)
    {
	phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
	    "The handler seems to present more devices than configured\n"
	    "The driver configuration must be changed to support more sites");
    }

    return retVal;
}

/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
    static int srq = 0;
    static int received = 0;

    struct timeval pollStartTime;
    struct timeval pollStopTime;
    int timeout;


    phFuncError_t retVal = PHFUNC_ERR_OK;

    if (handlerID->p.strictPolling)
    {
	/* apply strict polling loop, ask for site population */
	gettimeofday(&pollStartTime, NULL);

	timeout = 0;
	localDelay_us(handlerID->p.pollingInterval);
	do
	{
	    PhFuncTaCheck(pollParts(handlerID));
	    if (!handlerID->p.oredDevicePending)
	    {
		gettimeofday(&pollStopTime, NULL);
		if ((pollStopTime.tv_sec - pollStartTime.tv_sec)*1000000L + 
		    (pollStopTime.tv_usec - pollStartTime.tv_usec) > 
		    handlerID->heartbeatTimeout)
		    timeout = 1;
		else
		    localDelay_us(handlerID->p.pollingInterval);
	    }
	} while (!handlerID->p.oredDevicePending && !timeout);

	/* flush any pending SRQs, we don't care about them but want
           to eat them up. Otherwise reconfiguration to interrupt
           based mode would fail. */
	PhFuncTaCheck(flushSRQs(handlerID));
    }
    else
    {
	/* we don't do strict polling, try to receive a start SRQ */
	/* this operation covers the heartbeat timeout only, if there
           are no more unexpected devices pending from a previous call
           (e.g. devices provided during reprobe) */
	PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 
	    handlerID->p.oredDevicePending));

	if (received)
	{
	    switch (handlerID->model)
	    {
	      case PHFUNC_MOD_HYAC8086:
           	if ( srq & 0x01 )
		    {
		      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                               "received generic test start SRQ, need to poll");
		      /* now there may be parts, but we don't know where,
		         must poll */
		      PhFuncTaCheck(pollParts(handlerID));
		    }

                /* check for Alarm and Error of transmission */
                handlerID->p.status = srq & (HYAC_SRQ_ALARM|HYAC_SRQ_ERROR);

            if ( handlerID->p.status )
            {
               /* some exceptional SRQ occured */
               phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                "received exceptional SRQ 0x%02x:", srq);

               analyzeStatus(handlerID, 1);
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
	    if (handlerID->p.oredDevicePending)
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
    if (  handlerID->binIds )
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
        if (binNumber < 0 || binNumber > 14)
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
 *   BINON: NNNNNNNN,NNNNNNNN,NNNNNNNN,NNNNNNNN  (sent)
 *    ECHO: NNNNNNNN,NNNNNNNN,NNNNNNNN,NNNNNNNN  (received)
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

    /* find first colon */
    s=strchr(s,':');
    r=strchr(r,':');

    while ( s && *s &&
            r && *r &&
            binMatch!=32 )
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

    if ( binMatch != 32 )
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
    char thisbin[32];
    int sendBinning = 0;
    int retryCount = 0;
    int repeatBinning = 0;

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

	switch(handlerID->model)
	{
	  case PHFUNC_MOD_HYAC8086:
            strcpy(testresults, "BINON:");
            for ( i=31 ; i>=0 && retVal==PHFUNC_ERR_OK ; --i )
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
            if (sendBinning)
            {
                PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", testresults, handlerID->p.eol));
            }
            break;
  	  default: break;
	}

	/* verify binning, if necessary */
	if (handlerID->p.verifyBins && sendBinning)
	{
	    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
		"will perform bin verification ....");
	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", querybins));

        retVal=checkBinningInformation(handlerID, testresults, querybins);

	    if (retVal == PHFUNC_ERR_OK)
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

	/* in case of receiving bad bin data during verification, we may want to try again */
	retryCount++;
	repeatBinning = (retVal == PHFUNC_ERR_BINNING &&
	    retryCount <= handlerID->p.verifyMaxCount &&
	    handlerID->p.verifyBins && 
	    sendBinning);
	
	if (repeatBinning)
	{
	    repeatBinning = 1;
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
		"will try to send bin data %d more time(s) before giving up",
		1 + handlerID->p.verifyMaxCount - retryCount);

	    /* we do a loop, now go back to stored interaction mark */
	    phFuncTaRemoveToMark(handlerID);	
	}

    } while (repeatBinning);

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
	PHKEY_BI_VERIFY, "yes", "no", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
	  retVal = PHFUNC_ERR_CONFIG;
    else
    {
	switch (found)
	{
	  case 1:
	    /* configured "yes" */
	    switch(handlerID->model)
	    {
	      case PHFUNC_MOD_HYAC8086:
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "will activate bin verification mode");
                handlerID->p.verifyBins = 1;
                break;
	      default: break;
            }
            break;
	  case 2:
	    /* configured "no" */
	    switch(handlerID->model)
	    {
	      case PHFUNC_MOD_HYAC8086:
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "will deactivate bin verification mode");
                handlerID->p.verifyBins = 0;
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
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfNumber(handlerID->myConf, PHKEY_BI_VERCOUNT, 
	    0, NULL, &dRetryCount);
    if (confError == PHCONF_ERR_OK)
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
//    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", frQuery));
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", frQuery);
    if (retVal != PHFUNC_ERR_OK) {
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
    phFuncError_t retVal = PHFUNC_ERR_OK;

    /* flush any pending SRQs (will poll later, if we are missing some
       test start SRQ here */
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
    char *handlerRunning;
    static int firstTime=1;

    /* chose polling or SRQ interrupt mode */
    phConfConfStrTest(&found, handlerID->myConf, PHKEY_FC_WFPMODE, 
	"polling", "interrupt", NULL);
    if (found == 1)
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
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_POLLT, 
	    0, NULL, &dPollingInterval);
    if (confError == PHCONF_ERR_OK)
	handlerID->p.pollingInterval = labs((long) dPollingInterval);
    else
    {
	handlerID->p.pollingInterval = 200000L; /* default 0.2 sec */
    }

    confError = phConfConfStrTest(&found, handlerID->myConf,
	PHKEY_OP_PAUPROB, "yes", "no", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
	retVal = PHFUNC_ERR_CONFIG;
    else
    {
	if (found == 1)
	{
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                    "remote stopping is not (yet) supported by YAC handler");
	}
    }

    /* perform the communication intesive parts */
    if ( firstTime )
    {
        PhFuncTaCheck(checkHandlerRunning(handlerID, &handlerRunning));

        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "FR? handler running returns \"%s\" ",
                         handlerRunning);
        PhFuncTaCheck(handlerSetup(handlerID));
        firstTime=0;
    }
    PhFuncTaCheck(reconfigureVerification(handlerID));
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
    handlerID->p.verifyBins = 0;
    handlerID->p.verifyMaxCount = 0;

    handlerID->p.strictPolling = 0;
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
    handlerID->p.status = 0L;
    handlerID->p.stopped = 0;

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
    static int displayPressStart = 0;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    /* remember which devices we expect now */
    for (i=0; i<handlerID->noOfSites; i++)
	handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

    if ( !displayPressStart )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_0,
                         "PRESS START BUTTON ON HANDLER");
        displayPressStart=1;
    }

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

	return PHFUNC_ERR_WAITING;
    }
    
    phFuncTaStop(handlerID);

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
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    /* get current site population */
    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

    phFuncTaStart(handlerID);
    PhFuncTaCheck(binDevice(handlerID, oldPopulation, perSiteBinMap));
    phFuncTaStop(handlerID);

    /* modify site population, everything went fine, otherwise we
       would not reach this point */
    for	(i=0; i<handlerID->noOfSites; i++)
    {
	if (handlerID->activeSites[i] && 
	    (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
		oldPopulation[i] == PHESTATE_SITE_POPDEACT))
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
    *diag = statusString(handlerID); 
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
    switch (action)
    {
      case PHFUNC_SR_QUERY:
	/* we can handle this query globaly */
	if (lastCall)
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

    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);

    retVal = checkHandlerRunning(handlerID, &handlerRunning);

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "FR? handler running returns \"%s\" ",
                     handlerRunning);

    phFuncTaStop(handlerID);

    /* return result, there is no real check for the model here (so far) */
    if (testPassed)
        *testPassed = (retVal == PHFUNC_ERR_OK) ? 1 : 0;
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


/*****************************************************************************
 * End of file
 *****************************************************************************/
