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
 *            02 Jan 2001, Chris Joyce modified for Seiko-Epson-GPIB
 *            23 Mar 2001, Chris Joyce, added CommTest and split init into
 *                         init + comm + config
 *             9 Apr 2001, Siegfried Appich, changed debug_level from LOG_NORMAL
 *                         to LOG_DEBUG for site population and bin info
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
/*Begin of Huatek Modifications, Luke Lan, 04/19/2001*/
/*Issue Number: 20 */
#include <errno.h>
/*End of Huatek Modifications */
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 285 */
#include <ctype.h>
/* End of Huatek Modification */
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

#define NS_SRQ_ERROR (1L << 3)
#define NS_SRQ_ALARM (1L << 2)

/* 
 *   Place filler for BINON command where there is no operational site 
 */

#define NO_BINNING_RESULT      "0"
#define NO_BINNING_RESULT_256  "000"
#define ALL_BARCODE_LENGTH     PHCOM_MAX_MESSAGE_LENGTH
#define DEFAULT_SRQMASK        "FF"

/*--- typedefs --------------------------------------------------------------*/

/*--- variable --------------------------------------------------------------*/
int finalLotEndFlag = 0;                    /* final lot end flag */

/*--- global values ---------------------------------------------------------*/
static char szBarcode[ALL_BARCODE_LENGTH];
static char szSeparateBarcodes[ALL_BARCODE_LENGTH];
static char *szBarcodePerSite[PHESTATE_MAX_SITES];

/*--- functions -------------------------------------------------------------*/
static int separateBarcodes()
{
    /* copy the global barcode prevent to be destroyed */
    strncpy(szSeparateBarcodes, szBarcode, ALL_BARCODE_LENGTH-1);
      /* truncate ";" in the last of string */
      if(';' == szSeparateBarcodes[strlen(szSeparateBarcodes)-1])
      {
        szSeparateBarcodes[strlen(szSeparateBarcodes)-1] = '\0';
      }
      else
      {
        /* means we got unexpected answer or the barcode buffer is overflow */
        return -1;
      }

    char * pBarcodes = strchr(szSeparateBarcodes, ':') + 1;
    if(pBarcodes == NULL)
    {
        /* means we got unexpected answer */
        return -1;
    }
    char * savePtr = NULL;
    char * pBuff = pBarcodes;
    int iLoop = 0;
    int iSiteCount = 0;     /* site count */
    int iReverse = 0;       /* reverse control */

    /* seperated the barcodes by ',', the barcodes will be stored in the char** array */
    while((pBuff = strtok_r(pBuff, ",", &savePtr)) != NULL)
    {
        szBarcodePerSite[iLoop++] = pBuff;
        pBuff = NULL;
    }

    iSiteCount = iLoop;
    iLoop = iLoop-1;
    /* for the barcodes start from the last site, we need to reverse it */
    for(iReverse = 0; iReverse < iLoop; iReverse++)
    {
        pBuff = szBarcodePerSite[iReverse];
        szBarcodePerSite[iReverse] = szBarcodePerSite[iLoop];
        szBarcodePerSite[iLoop--] = pBuff;
    }
    return 0;
}

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
      case PHFUNC_MOD_NS5000:
      case PHFUNC_MOD_NS6000:
      case PHFUNC_MOD_NS7000:
      case PHFUNC_MOD_HONTECH:
      case PHFUNC_MOD_NS6040:
      case PHFUNC_MOD_NS8040:
      case PHFUNC_MOD_CHROMA:
      case PHFUNC_MOD_NS8000:
        if (status & NS_SRQ_ALARM)
        {
            strcat(statusMessage, "handler status: Alarm");
        }
        if (status & NS_SRQ_ERROR)
        {
            if (status & NS_SRQ_ALARM)
            {
                strcat(statusMessage, " & Error of transmission");
            }
            else
            {
                strcat(statusMessage, "handler status: Error of transmission");
            }
        }
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
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

    char handlerAnswer[1024] = "";
    unsigned long population = 0L;
    unsigned long populationPicked = 0L;
    unsigned long i = 0L;
    static bool isParsedSiteInfo = false;
    switch(handlerID->model)
    {
      case PHFUNC_MOD_NS5000:
      case PHFUNC_MOD_NS6000:
      case PHFUNC_MOD_NS7000:
      case PHFUNC_MOD_HONTECH:
      case PHFUNC_MOD_NS6040:
      case PHFUNC_MOD_NS8040:
      case PHFUNC_MOD_CHROMA:
      case PHFUNC_MOD_NS8000:
	    PhFuncTaCheck(phFuncTaSend(handlerID, "FULLSITES?%s", handlerID->p.eol));
//	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer));
        retVal = phFuncTaReceive(handlerID, 1, "%1023[^\n\r]", handlerAnswer);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
        // this means we received the new site info, reset the flag
        if(strlen(handlerAnswer) != 0 )
            isParsedSiteInfo = false;
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
	  default: break;
/* End of Huatek Modification */
    }

     /* ensure string is in upper case */
    setStrUpper(handlerAnswer); 
    if( handlerID->model == PHFUNC_MOD_CHROMA )
    {
        memset(szBarcode, 0, ALL_BARCODE_LENGTH);
        //when model is Chroma, the response format  of "FULLSITES?" is as follows:
        //"FULLSITES 3,1#001,2#002" or "FULLSITES: 3,1#001,2#002"
        // 3 respresents device info,1#001 respresents barcode info or site 1
        // 2#002 respresents barcode info or site 2
        char *token = strtok(handlerAnswer,",");
        int i = 0,tokenNum=0;
        while(token)
        {
            ++tokenNum;
            if(tokenNum == 1)
            {
                char *p = strchr(token,':');
                if(p)
                    sscanf(token,"FULLSITES: %lx",&population);
                else
                    sscanf(token,"FULLSITES %lx",&population);
            }
            else 
            {
                char *ptr = strchr(token,'#');
                if(ptr)
                {
                    szBarcodePerSite[i++] = strdup(ptr+1);
                    strcat(szBarcode,ptr+1);
                }
            }
            token = strtok(NULL,",");
            if(token && tokenNum != 1)
              strcat(szBarcode,",");
        }
        //if tokenNum is 1, that means barcode function is disabled,pop up a warning to tell user!
        if(tokenNum == 1)
             phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"barcode function is disabled by handler side,please check handler!");
    }
    else
    {
      if(!isParsedSiteInfo)
      {
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
        isParsedSiteInfo = true;
      }
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

    /* perform barcode query for hontech only */
    if(PHFUNC_MOD_HONTECH == handlerID->model && 1 == handlerID->p.htRequestBarCode)
    {
        /* initialize bar code */
        memset(szBarcode, 0, ALL_BARCODE_LENGTH);
        memset(handlerAnswer, 0, 512);

        PhFuncTaCheck(phFuncTaSend(handlerID, "BARCODE?%s", handlerID->p.eol));
        /* example of qual site bar code
            BARCODE:0,0,0,0,0,0,0,0, \
                    0,0,0,0,0,0,0,0, \
                    0,0,0,0,0,0,0,0, \
                    0,0,0,0, \
                    MR1200HJKL00000H000000004, \
                    MR1200HJKL00000H000000003, \
                    MR1200HJKL00000H000000002, \
                    MR1200HJKL00000H000000001;
        */
        retVal = phFuncTaReceive(handlerID, 1, "%10239[^\r\n]", szBarcode);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }

        if(handlerID->p.isEchoBarcode)
        {
          PhFuncTaCheck(phFuncTaSend(handlerID, "ECHOCODE:%s%s", szBarcode+strlen("BARCODE:"), handlerID->p.eol));
          PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer));
          if(0 != strcmp(handlerAnswer, "ECHOCODEOK"))
          {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "validation of Barcodes failed");
            retVal = PHFUNC_ERR_ANSWER;
            return retVal;
          }
        }
        if(-1 == separateBarcodes())
        {
	        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "unexpected format in received barcodes");
            retVal = PHFUNC_ERR_ANSWER;
            return retVal;
        }
        
    }

    return retVal;
}

static phFuncError_t specialSRQHandling(phFuncId_t handlerID)
{
    unsigned long srqkind = 0;
    static char handlerAnswer[512] = "";

    PhFuncTaCheck(phFuncTaSend(handlerID, "SRQKIND?%s", handlerID->p.eol));
    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer));

    setStrUpper(handlerAnswer);
    if (sscanf(handlerAnswer, "SRQKIND %lx", &srqkind) != 1)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "SRQKIND not received from handler: \n"
                         "received \"%s\" format expected \"SRQKIND xx\" \n",
                         handlerAnswer);
        return PHFUNC_ERR_ANSWER;
    }


    if (srqkind & 0x06)
    {
        if(srqkind & 0x02)
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Lot start...");
        }
        else
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Retest lot start...");
        }

        /* lot start detected, send comfirmation to handler */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Lot start from handler detected...");
        PhFuncTaCheck(phFuncTaSend(handlerID, "LOTORDER 0%s", handlerID->p.eol));
        handlerID->p.lotStarted = 1;

        /* after the Lot start, just polling parts */
        PhFuncTaCheck(pollParts(handlerID));
        return PHFUNC_ERR_OK;
    }

    if (srqkind & 0x10)
    {
        /* lot end detected, send confirmation to handler */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Final lot end from handler detected...");

        PhFuncTaCheck(phFuncTaSend(handlerID, "LOTORDER 2%s", handlerID->p.eol));
        handlerID->p.lotStarted = 0;

        /* set final lot end flag to exit test program */
        finalLotEndFlag = 1;

        return PHFUNC_ERR_LOT_DONE;
    }

    if (srqkind & 0x08)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Lot end from handler detected...");
        PhFuncTaCheck(phFuncTaSend(handlerID, "LOTRETESTCLEAR?", handlerID->p.eol));
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer));
        if ( strncmp(handlerAnswer, "LOTRETESTCLEAR", strlen("LOTRETESTCLEAR")) != 0)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "Answer is not expected, try again");
            phFuncTaRemoveStep(handlerID);
            phFuncTaRemoveStep(handlerID);
            return PHFUNC_ERR_WAITING;
        }
        else
        {
            PhFuncTaCheck(phFuncTaSend(handlerID, "LOTORDER 2%s", handlerID->p.eol));
            handlerID->p.lotStarted = 0;
            return PHFUNC_ERR_LOT_DONE;
        }
    }

    if (srqkind & 0x80)
    {
        /* double-test(reprobe) detected */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Double-test(reprobe) from handler detected...");
        /* just poll the parts */
        PhFuncTaCheck(pollParts(handlerID));
        return PHFUNC_ERR_OK;
    }

    return PHFUNC_ERR_OK;
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
        /* before any polling, check SRQ to see if lot end is detected */
        PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 1));
        if (received && (srq & 0x80))
        {
            /* special SRQ got */
            PhFuncTaCheck(specialSRQHandling(handlerID));
        }

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
	      case PHFUNC_MOD_NS5000:
	      case PHFUNC_MOD_NS6000:
	      case PHFUNC_MOD_NS7000:
              case PHFUNC_MOD_HONTECH:
              case PHFUNC_MOD_NS6040:
              case PHFUNC_MOD_NS8040:
              case PHFUNC_MOD_CHROMA:
              case PHFUNC_MOD_NS8000:
		if ( srq & 0x01 )
		{
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
			"received generic test start SRQ, need to poll");
                    /* test start SRQ will also set lot start flag even there is no lot start signal received*/
                    handlerID->p.lotStarted = 1;
		    /* now there may be parts, but we don't know where,
		       must poll */
		    PhFuncTaCheck(pollParts(handlerID));
		}
                else if (srq & 0x80)
                {
                    /* special SRQ got */
                    PhFuncTaCheck(specialSRQHandling(handlerID));
                }
                else
                {
                    /* some exceptional SRQ occured */
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                     "received exceptional SRQ 0x%02x:", srq);
                }

                /* check for Alarm and Error of transmission */
                handlerID->p.status = srq & (NS_SRQ_ALARM|NS_SRQ_ERROR);

                if ( handlerID->p.status )
                {
                    /* some exceptional SRQ occured */
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                        "received exceptional SRQ 0x%02x:", srq);

                    analyzeStatus(handlerID, 1);
                }
  	        break;
                default:
                    break;
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
        /* if handler bin id is defined (hardbin/softbin mapping), get the bin code from the bin id list*/
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
            if( 256 == handlerID->p.binningCommandFormat )
            {
                /* 256 bin */
                int i_ret = strtol(handlerID->binIds[binNumber], (char **)NULL, 10);
                sprintf(binCode,"%03d", i_ret);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "using binIds set binNumber %d to binCode \"%s\" ", 
                        binNumber, binCode);
            }
            else
            {
                char binningFormatForChar;
                binningFormatForChar = handlerID->binIds[binNumber][0];
                if('A' <= binningFormatForChar && 'W'>=binningFormatForChar)
                {
                    /* If the binIds is a character between 'A' and 'W', user may use an older binIds format in configuration file */
                    sprintf(binCode, "%s", handlerID->binIds[binNumber]);
                }
                else
                {
                    /* 16 or 32 bin, the binNumber always less than the handlerID->noOfBins */
                    int i_ret = strtol(handlerID->binIds[binNumber], (char **)NULL, 10);
                    if( 9 >= i_ret )
                    {
                        sprintf(binCode, "%c", (char)(0x30+i_ret));
                    }
                    else if( 32>= i_ret)
                    {
                        /* for 16 or 32 bin, we use A B C ... W to represent 10 11 12 ... 32 */
                        sprintf(binCode, "%c", (char)(0x37+i_ret));
                    }
                }
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "using binIds set binNumber %d to binCode \"%s\" ", 
                        binNumber, binCode);
            }
        }
    }
    else
    {
        /* if handler bin id is not defined (default mapping), use the bin number directly */
        if (handlerID->p.binningCommandFormat == 16)
        {
            /* 16-bin format: 1-F */
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
        else if (handlerID->p.binningCommandFormat == 32)
        {
            /* 32-bin format: 1-W */
            if (binNumber < 0 || binNumber > 31)
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                        "received illegal request for bin %lx ",
                        binNumber+1);
                retVal=PHFUNC_ERR_BINNING;
            }
            else
            {
                if( 9 >= binNumber+1 )
                {
                    sprintf(binCode, "%c", (char)(0x30+binNumber+1));
                }
                else if( 32>= binNumber+1)
                {
                    /* for 16 or 32 bin, we use A B C ... W to represent 10 11 12 ... 32 */
                    sprintf(binCode, "%c", (char)(0x37+binNumber+1));
                }
            }
        }
        else if (handlerID->p.binningCommandFormat == 256)
        {
            /* 256-bin format: 1-255 */
            if (binNumber < 0 || binNumber > 255)
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                        "received illegal request for bin %lx ",
                        binNumber+1);
                retVal=PHFUNC_ERR_BINNING;
            }
            else
            {
                sprintf(binCode, "%03d", binNumber+1);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "using binNumber set binNumber %d to binCode \"%s\" ", 
                        binNumber, binCode);
            }
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
    int checkError = 0;
    const char* s=sent;
    const char* r=received;

    /* find first colon */
    s=strchr(s,':');
    r=strchr(r,':');

    if (s==NULL || r==NULL || strlen(s) != strlen(r))
    {
        checkError = 1;
    }
    else
    {
        while ( *s && *r )
        {
            if ( tolower(*s) != tolower(*r) )
            {
                checkError = 1;
                break;
            }
            ++s;
            ++r;
        }
    }

    if ( checkError == 1 )
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "difference found between sent and returned binning information \n"
                         "SENT:    \"%s\"\n"
                         "RECEIVED: \"%s\"",sent,received);
        retVal = PHFUNC_ERR_BINNING;
    }

    return retVal;
}


/* create and send bin and reprobe information information */
static phFuncError_t binAndReprobe(
    phFuncId_t handlerID     /* driver plugin ID */,
    phEstateSiteUsage_t *oldPopulation /* current site population */,
    long *perSiteReprobe     /* TRUE, if a device needs reprobe*/,
    long *perSiteBinMap      /* valid binning data for each site where
                                the above reprobe flag is not set */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char testresults[PHESTATE_MAX_SITES + 16];
    char tmpCommand[256] = {0};
    static char querybins[PHESTATE_MAX_SITES + 512];
    char thisbin[32];
    int i;
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
        case PHFUNC_MOD_NS7000:
        case PHFUNC_MOD_NS6000:
        case PHFUNC_MOD_NS5000:
        case PHFUNC_MOD_HONTECH:
        case PHFUNC_MOD_NS6040:
        case PHFUNC_MOD_NS8040:
        case PHFUNC_MOD_CHROMA:
        case PHFUNC_MOD_NS8000:
            strcpy(testresults, "BINON:");
            for ( i=31 ; i>=0 && retVal==PHFUNC_ERR_OK ; --i )
            {
                if ( i < handlerID->noOfSites &&
                        handlerID->activeSites[i] && 
                        (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
                         oldPopulation[i] == PHESTATE_SITE_POPDEACT) )
                {
                    /* there is a device here */
                    if (perSiteReprobe && perSiteReprobe[i])
                    {
                        if ( !handlerID->p.reprobeBinDefined )
                        {
                            /* no reprobe bin number defined */
                            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                                    "received reprobe command at site \"%s\" but no reprobe bin number defined \n"
                                    "please check the configuration values of \"%s\" and \"%s\"",
                                    handlerID->siteIds[i], PHKEY_RP_AMODE, PHKEY_RP_BIN);
                            retVal =  PHFUNC_ERR_BINNING;
                        }
                        else
                        {
                            /* reprobe this one */
                            if( 256 == handlerID->p.binningCommandFormat )
                            {
                                /* 256-bin */
                                sprintf(tmpCommand, "%03d", handlerID->p.reprobeBinNumber);
                            }
                            else
                            {
                                /* 16 or 32 bin*/
                                if(handlerID->p.reprobeBinNumber <= 9)
                                {
                                    sprintf(tmpCommand, "%c", (char)(0x30 +  handlerID->p.reprobeBinNumber) );
                                }
                                else if(handlerID->p.reprobeBinNumber <= 32)
                                {
                                    sprintf(tmpCommand, "%c", (char)(0x37 +  handlerID->p.reprobeBinNumber) );
                                }
                            }

                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                    "will reprobe device at site \"%s\"",
                                    handlerID->siteIds[i]);
                            strcat(testresults, tmpCommand);
                            sendBinning = 1;
                        }
                    }
                    else
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
                }
                else
                {
                    /* there's not a device here */

                    /* for NS5000 or NS6000, we use the default binningCommandFormat value 16 */
                    if ( 256 == handlerID->p.binningCommandFormat )
                    {
                        /* 256-bin */
                        strcat(testresults, NO_BINNING_RESULT_256);
                    }
                    else
                    {
                        /* 16-bin or 32-bin */
                        strcat(testresults, NO_BINNING_RESULT);
                    }

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
	      case PHFUNC_MOD_NS5000:
	      case PHFUNC_MOD_NS6000:
	      case PHFUNC_MOD_NS7000:
              case PHFUNC_MOD_HONTECH:
              case PHFUNC_MOD_NS6040:
              case PHFUNC_MOD_NS8040:
              case PHFUNC_MOD_CHROMA:
              case PHFUNC_MOD_NS8000:
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
	      case PHFUNC_MOD_NS5000:
	      case PHFUNC_MOD_NS6000:
	      case PHFUNC_MOD_NS7000:
              case PHFUNC_MOD_HONTECH:
              case PHFUNC_MOD_NS6040:
              case PHFUNC_MOD_NS8040:
              case PHFUNC_MOD_CHROMA:
              case PHFUNC_MOD_NS8000:
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

    phFuncError_t retVal = PHFUNC_ERR_OK;

    frQuery[0] = '\0';

    *handlerRunningString = frQuery;

    // as chroma handler does not response "FR?" command,return early.
    if(handlerID->model == PHFUNC_MOD_CHROMA)
      return retVal;
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

    /* return result */
    return retVal;
}

static phFuncError_t handlerSetup(
    phFuncId_t handlerID    /* driver plugin ID */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    static char handlerAnswer[512] = "";
    int found = 0;
    if (PHCONF_ERR_OK == phConfConfIfDef(handlerID->myConf, PHKEY_AUTO_RETEST))
    {
      phConfConfStrTest(&found, handlerID->myConf, PHKEY_AUTO_RETEST, "yes", "no", NULL);   
      /* "yes" has been found in configuration file */
      if( found == 1 )
      {
        /* set command delimiter */
        PhFuncTaCheck(phFuncTaSend(handlerID, "DL 0%s", handlerID->p.eol));

        /* set enable/disable bits in the status byte for special operation on handler */
        PhFuncTaCheck(phFuncTaSend(handlerID, "SRQMASK %s%s", handlerID->p.srqMask, handlerID->p.eol));

        /* to clear the hanlder lot data */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "To clear the handler lot data...");
        PhFuncTaCheck(phFuncTaSend(handlerID, "LOTCLEAR?%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
        if (retVal != PHFUNC_ERR_OK || strncmp(handlerAnswer, "LOTCLEARED", strlen("LOTCLEARED")) != 0) {
          phFuncTaRemoveStep(handlerID);
          return retVal;
        }
      }

    }
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
    double dReprobeBinNumber;
    char *handlerRunning;
    static int firstTime=1;
    const char* srqMask = NULL;
    long srqMaskLong = 0;
    const char *binningCommandFormat = NULL;      /* binning command format, will be 16-bin , 32-bin or 256-bin */

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

    /* retrieve the se_binnng_command_format */
    handlerID->p.binningCommandFormat = 16; /* seiko default 16-bin format*/
    if (PHCONF_ERR_OK == phConfConfIfDef(handlerID->myConf, PHKEY_PL_SE_BINNING_COMMAND_FORMAT))
    {
        confError = phConfConfString(handlerID->myConf, PHKEY_PL_SE_BINNING_COMMAND_FORMAT,
                0, NULL, &binningCommandFormat);
        if (confError == PHCONF_ERR_OK)
        {
            if(strcmp(binningCommandFormat, "16-bin") == 0)
            {
                handlerID->p.binningCommandFormat = 16;
            }
            else if(strcmp(binningCommandFormat, "32-bin") == 0)
            {
                handlerID->p.binningCommandFormat = 32;
            }
            else if(strcmp(binningCommandFormat, "256-bin") == 0)
            {
                handlerID->p.binningCommandFormat = 256;
            }
        }
    }
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "seiko binning-Command-Format is %d-bin", handlerID->p.binningCommandFormat);

    const char* subType = NULL;
    strcpy(handlerID->p.subType,"none") ; /* seiko default sub type: none*/
    if (PHCONF_ERR_OK == phConfConfIfDef(handlerID->myConf, PHKEY_IN_SUB_TYPE))
    {
        confError = phConfConfString(handlerID->myConf, PHKEY_IN_SUB_TYPE,
                0, NULL, &subType);
        if (confError == PHCONF_ERR_OK)
        {
            strcpy(handlerID->p.subType,subType);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"sub type is set to:%s",handlerID->p.subType);
        }
    }
    /* retrieve hontech barcode configuration */
    if(PHFUNC_MOD_HONTECH == handlerID->model)
    {
        /* default as no */
        handlerID->p.htRequestBarCode = 0;

        confError = phConfConfStrTest(&found, handlerID->myConf, PHKEY_PL_HONTECH_REQUEST_BARCODE, "yes", "no", NULL);
        if( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
        {
            retVal = PHFUNC_ERR_CONFIG;
        }
        else
        {
            /* "yes" has been found in configuration file */
            if( found == 1 )
            {
                handlerID->p.htRequestBarCode = 1;
            }
        }

        handlerID->p.isEchoBarcode = 1;
        confError = phConfConfIfDef(handlerID->myConf,PHKEY_PL_HONTECH_IS_ECHO_BARCODE);
        if(confError == PHCONF_ERR_OK)
            confError = phConfConfStrTest(&found, handlerID->myConf, PHKEY_PL_HONTECH_IS_ECHO_BARCODE, "yes", "no", NULL);
        if( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
        {
            retVal = PHFUNC_ERR_CONFIG;
        }
        else
        {
            /* "no" has been found in configuration file */
            if( found == 2 )
            {
                handlerID->p.isEchoBarcode = 0;
            }
        }
        
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
                    "remote stopping is not (yet) supported by Seiko-Epson handler");
	}
    }

    /*
     * get reprobe bin number
     * following answers to queries from Seiko-Epson it appears the
     * the reprobe command could be set-up at the handler for any
     * of the possible bins: this number should be set as a
     * configuration variable
     */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_RP_BIN);
    if (confError == PHCONF_ERR_OK)
    {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_RP_BIN,
            0, NULL, &dReprobeBinNumber);
    }
    if (confError == PHCONF_ERR_OK)
    {
        handlerID->p.reprobeBinDefined = 1;
        handlerID->p.reprobeBinNumber = abs((int) dReprobeBinNumber);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
            "configuration \"%s\" set: will send BIN %d command upon receiving a reprobe",
            PHKEY_RP_BIN,
            handlerID->p.reprobeBinNumber);
    }
    else
    {
        handlerID->p.reprobeBinDefined = 0;
        handlerID->p.reprobeBinNumber = 0;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
            "configuration \"%s\" not set: ignore reprobe command",
            PHKEY_RP_BIN);
    }

    phConfConfStrTest(&found, handlerID->myConf, PHKEY_RP_AMODE,
        "off", "all", "per-site", NULL);
    if ( (found==2 || found==3) && !handlerID->p.reprobeBinDefined )
    {
        /* frame work may send reprobe command but a reprobe bin has not been defined */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
            "a reprobe mode has been defined without its corresponding bin number\n"
            "please check the configuration values of \"%s\" and \"%s\"",
            PHKEY_RP_AMODE, PHKEY_RP_BIN);
        retVal = PHFUNC_ERR_CONFIG;
    }

    /* retrieve srqmask */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_SU_SRQMASK);
    if (confError == PHCONF_ERR_OK)
        confError = phConfConfString(handlerID->myConf, PHKEY_SU_SRQMASK,
                                     0, NULL, &srqMask);
    if (confError == PHCONF_ERR_OK)
    {
        strncpy(handlerID->p.srqMask, srqMask, 2);
    }
    else
    {
        strcpy(handlerID->p.srqMask, DEFAULT_SRQMASK);
    }
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Set SRQMASK to %s", handlerID->p.srqMask);
    sscanf(handlerID->p.srqMask, "%lx", &srqMaskLong);
    if (!(srqMaskLong & 0x06))
    {
        /* if the lot start/retest lot start bit in status byte is disabled, */
        /* assume the lot is started */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Lot start bits are masked, so assume lot is already started");
        handlerID->p.lotStarted = 1;
    }

    /* perform the communication intesive parts */
    if ( firstTime )
    {
        PhFuncTaCheck(checkHandlerRunning(handlerID, &handlerRunning));
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "FR? handler running returns \"%s\" ",
                         handlerRunning);
        if(strcmp(handlerID->p.subType,"changchuan") != 0)
             PhFuncTaCheck(handlerSetup(handlerID));

        firstTime=0;
    }
    PhFuncTaCheck(reconfigureVerification(handlerID));
    return retVal;
}

/*****************************************************************************
 *
 * for a certain SetStatus, get the corresponding GPIB command
 *
 * Authors: Zoyi Yu
 *
 * Description:
 *    all the actual GPIB commands are stored locally, this function retrieves the
 *    command corresponding to the "token".
 *
 * Return:
 *    function return SUCCEED if everything is OK, FAIL if error; and the found
 *    GPIB command will be returned with the parameter "pGpibCommand"
 *
 ***************************************************************************/
static int getGpibCommandForSetStatusQuery(
                                          phFuncId_t handlerID,
                                          char **pGpibCommand,
                                          const char *token,
                                          const char *param
                                          )
{
    static const  phStringPair_t sGpibCommands[] = {
      {PHKEY_NAME_HANDLER_STATUS_SET_INPUT_QTY, "inputqty"}
    };


    static char buffer[MAX_STATUS_MSG] = "";
    int retVal = SUCCEED;
    const phStringPair_t *pStrPair = NULL;
    int paramSpecified = NO;
    int ignoreParam = NO;

    if (strlen(param) > 0 )
    {
        paramSpecified = YES;
    }

    if (retVal == SUCCEED)
    {
        pStrPair = phBinSearchStrValueByStrKey(sGpibCommands, LENGTH_OF_ARRAY(sGpibCommands), token);
    }

    if (pStrPair != NULL)
    {
        strcpy(buffer,"");
        if (paramSpecified == YES && ignoreParam == NO)
        {
            /* add a space character between command and parameter */
            sprintf(buffer, "%s ", pStrPair->value);
            strcat(buffer, param);
        }
        else
        {
            sprintf(buffer, "%s", pStrPair->value);
        }
        *pGpibCommand = buffer;
    }
    else
    {
        retVal = FAIL;
    }

    return retVal;
}


#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The stub function for set status
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *  use this function to set status control command to the handler.
 *
 ***************************************************************************/
phFuncError_t privateSetStatus(
                              phFuncId_t handlerID,       /* driver plugin ID */
                              const char *token,          /* the string of command, i.e. the key to
                                                             get the information from Handler */
                              const char *param           /* the parameter for command string */
                              )
{
    static char response[MAX_STATUS_MSG] = "";
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char *gpibCommand = NULL;
    int found = FAIL;

    if ( phFuncTaAskAbort(handlerID) )
    {
        return PHFUNC_ERR_ABORTED;
    }

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "privateSetStatus, token = ->%s<-, param = ->%s<-", token, param);

    found = getGpibCommandForSetStatusQuery(handlerID, &gpibCommand, token, param);
    if ( found == SUCCEED )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "privateSetStatus, gpibCommand = ->%s<-", gpibCommand);
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", gpibCommand, handlerID->p.eol));
        localDelay_us(100000);
        retVal = phFuncTaReceive(handlerID, 1, "%4095[^\r\n]", response);
        if (retVal != PHFUNC_ERR_OK)
        {
            phFuncTaRemoveStep(handlerID);
            return retVal;
        }
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "The key \"%s\" is not available, or may not be supported yet", token);
        sprintf(response, "%s_KEY_NOT_AVAILABLE", token);
    }


    /* ensure null terminated */
    response[strlen(response)] = '\0';

    if(strcmp(response, "SETTINGNG") == 0)
    {
        /* command is not executd successfully */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "Handler replied \"%s\" ", response);
        retVal = PHFUNC_ERR_ANSWER;
    }

    phFuncTaStop(handlerID);

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
    handlerID->p.htRequestBarCode = 0;
    handlerID->p.isEchoBarcode = 1;

    strcpy(handlerID->p.subType,"none");
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
    static char resultString[MAX_STATUS_MSG] = "";
    idString[0] = '\0';
    strncpy(resultString, "equipment_id_not_available", MAX_STATUS_MSG-1);
    *equipIdString = resultString;
    phFuncError_t retVal = PHFUNC_ERR_OK;

    if(handlerID->model == PHFUNC_MOD_HONTECH)
    { 
        /* abort in case of unsuccessful retry */
        if (phFuncTaAskAbort(handlerID))
        {
            return PHFUNC_ERR_ABORTED;
        }

        phFuncTaStart(handlerID);
        PhFuncTaCheck(phFuncTaSend(handlerID, "Handler ID?%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%s", idString);
        if (retVal != PHFUNC_ERR_OK)
        {
            phFuncTaRemoveStep(handlerID);
            return retVal;
        }

        phFuncTaStop(handlerID);

        sprintf(resultString, "%s", idString);

        /* strip of white space at the end of the string */
        while (strlen(resultString) > 0 && isspace(resultString[strlen(resultString)-1]))
        {
            resultString[strlen(resultString)-1] = '\0';
        }
    }

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
    phFuncError_t retVal = PHFUNC_ERR_OK;
    long perSiteReprobe[PHESTATE_MAX_SITES];
    long perSiteBinMap[PHESTATE_MAX_SITES];
    int i=0;

    if ( !handlerID->p.reprobeBinDefined )
    {
        /* no reprobe bin number defined */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
            "received reprobe command at site \"%s\" but no reprobe bin number defined \n"
            "please check the configuration values of \"%s\" and \"%s\"",
            handlerID->siteIds[i], PHKEY_RP_AMODE, PHKEY_RP_BIN);
        retVal=PHFUNC_ERR_BINNING;
    }
    else
    {
        /* prepare to reprobe everything */
        for (i=0; i<handlerID->noOfSites; i++)
        {
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
    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;

    /* remember which devices we expect now */
    for (i=0; i<handlerID->noOfSites; i++)
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
    if (!handlerID->p.oredDevicePending)
    {
        /* during the next call everything up to here should be
           repeated */
        phFuncTaRemoveToMark(handlerID);

        return PHFUNC_ERR_WAITING;
    }

    phFuncTaStop(handlerID);
    phEstateASetPauseInfo(handlerID->myEstate, 0);

    /* we have received devices for test after a reprobe.
       Change the equipment specific state now but do not present any
       devices that were not existent before this reprobe action */
    handlerID->p.oredDevicePending = 0;
    for (i=0; i<handlerID->noOfSites; i++)
    {
        if (handlerID->activeSites[i] == 1)
        {
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

            if (perSiteReprobe[i])
            {
                if (handlerID->p.devicePending[i])
                {
                    if (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
                        oldPopulation[i] == PHESTATE_SITE_POPDEACT)
                        handlerID->p.devicePending[i] = 0;
                    else
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                            "new device at site \"%s\" will not be tested during reprobe action (delayed)",
                            handlerID->siteIds[i]);
                    population[i] = oldPopulation[i];
                }
                else
                {
                    if (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
                        oldPopulation[i] == PHESTATE_SITE_POPDEACT)
                    {
                        /* something is wrong here, we expected a reprobed
                           device */
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                            "did not receive device that was scheduled for reprobe at site \"%s\"",
                            handlerID->siteIds[i]);
                        population[i] = PHESTATE_SITE_EMPTY;
                    }
                    else
                        population[i] = oldPopulation[i];
                }
            }
            else /* ! perSiteReprobe[i] */
            {
                /*
                 * all devices need to receive a bin or reprobe data,
                 * and devices that were not reprobed must be gone
                 */
                if (handlerID->p.devicePending[i])
                {
                    /* ignore any new devices */
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                        "new device at site \"%s\" will not be tested during reprobe action (delayed)",
                        handlerID->siteIds[i]);
                }
                population[i] = PHESTATE_SITE_EMPTY;
                if (oldPopulation[i] == PHESTATE_SITE_POPDEACT ||
                    oldPopulation[i] == PHESTATE_SITE_DEACTIVATED)
                    population[i] = PHESTATE_SITE_DEACTIVATED;
            }
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



#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * for a certain GetStatus query, get the corresponding GPIB command
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *    all the actual GPIB commands are stored locally, this function retrieves the 
 *    command corresponding to the "token".      
 *
 * Return:
 *    function return SUCCEED if everything is OK, FAIL if error; and the found 
 *    GPIB command will be returned with the parameter "pGpibCommand"
 *
 ***************************************************************************/
static int getGpibCommandForGetStatusQuery(
                                          phFuncId_t handlerID,
                                          char **pGpibCommand, 
                                          const char *token,
                                          const char *param
                                          )
{
    /* these static array must be ordered by the first field */
    static const  phStringPair_t sHonTechGpibCommands[] = 
    {
        {PHKEY_NAME_HANDLER_STATUS_GET_BINCOUNT, "BinCount?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_FORCE, "Force?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_SETSITEMAP, "SETSITEMAP?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_SETSOAK, "SETSOAK?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_SETTEMP,"SETTEMP?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_TEMPARM, "TempArm?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_TESTARM, "Test Arm?"} 
    };

    static const  phStringPair_t sSeikoEpsonGpibCommands[] =
    { };

    static char buffer[MAX_STATUS_MSG] = "";
    int retVal = SUCCEED;
    const phStringPair_t *pStrPair = NULL;
    int paramSpecified = NO;
    int ignoreParam = NO;

    if (strlen(param) > 0)
    {
        paramSpecified = YES;
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                         "the status query does not require any parameters!\n"
                         "Ignore the parameter %s.", 
                         param);
        ignoreParam = YES;
        retVal = SUCCEED;
    }

    if (retVal == SUCCEED)
    {
        if(handlerID->model == PHFUNC_MOD_HONTECH)
        {
            pStrPair = phBinSearchStrValueByStrKey(sHonTechGpibCommands, LENGTH_OF_ARRAY(sHonTechGpibCommands), token);
        }
        else
        {
            pStrPair = phBinSearchStrValueByStrKey(sSeikoEpsonGpibCommands, LENGTH_OF_ARRAY(sSeikoEpsonGpibCommands), token);
        }
    }

    if (pStrPair != NULL)
    {
        strcpy(buffer,"");
        sprintf(buffer, "%s", pStrPair->value);
        if (paramSpecified == YES && ignoreParam == NO)
        {
            strcat(buffer, param);
        }
        *pGpibCommand = buffer;
    }
    else
    {
        retVal = FAIL;
    }

    return retVal;
}
#endif


#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The private function for getting status
 *
 * Authors: Xiaofei Han
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

    if ( phFuncTaAskAbort(handlerID) )
    {
        return PHFUNC_ERR_ABORTED;
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateGetStatus, token = ->%s<-, param = ->%s<-", token, param);

    if( 0 == strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_BARCODE) || 0 == strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID) )
    {
      if( (PHFUNC_MOD_CHROMA == handlerID->model) || (PHFUNC_MOD_HONTECH == handlerID->model && 1 == handlerID->p.htRequestBarCode) )
      {
        if (strlen(param) > 0)
        {
          /* return the barcode for specific site id */
          int iSiteNum = atoi(param);
          if(iSiteNum <= 0 || iSiteNum > handlerID->p.sites)
          {
            strcpy(*responseString, "");
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "The site number is illegal!");
          }
          else
          {
            *responseString = szBarcodePerSite[iSiteNum-1];
          }
        }
        else
        {
          /* return the stored barcode for hontech driver */
          *responseString = szBarcode;
        }
        return retVal;
      }
    }

    found = getGpibCommandForGetStatusQuery(handlerID, &gpibCommand, token, param);
    if ( found == SUCCEED )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "privateGetStatus, gpibCommand = ->%s<-", gpibCommand);
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", gpibCommand, handlerID->p.eol));
        localDelay_us(100000);
        retVal = phFuncTaReceive(handlerID, 1, "%4095[^\r\n]", response);
        if (retVal != PHFUNC_ERR_OK)
        {
            phFuncTaRemoveStep(handlerID);  /* force re-send of command */
            return retVal;
        }
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "The key \"%s\" is not available, or may not be supported yet", token);
        sprintf(response, "%s_KEY_NOT_AVAILABLE", token);
    }


    /* ensure null terminated */
    response[strlen(response)] = '\0';

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "privateGetStatus, answer ->%s<-, length = %d",
                     response, strlen(response));

    phFuncTaStop(handlerID);

    return retVal;
}
#endif

#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot start signal from handler
 *
 * Authors: Adam Huang
 *
 * Description:
 *
 ***************************************************************************/
phFuncError_t privateLotStart(
                              phFuncId_t handlerID,      /* driver plugin ID */
                              int timeoutFlag
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateLotStart, timeoutFlag = %s", timeoutFlag ? "TRUE" : "FALSE");
    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
    {
        return PHFUNC_ERR_ABORTED;
    }

    phFuncTaStart(handlerID);
    phFuncTaMarkStep(handlerID);

    /***************************************************************************
     * for lot start signal, it will call pollParts() in waitForParts(),
     * and if FULLSITES correctly return, then oredDevicePending will be set
     * after lot start, the application model file will call ph_device_start(),
     * it will check oredDevicePending, if the oredDevicePending has been set,
     * the test will directly start without calling waitForParts()
     **************************************************************************/
    retVal = waitForParts(handlerID);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveToMark(handlerID);
        return retVal;
    }

    if (0 == handlerID->p.lotStarted)
    {
        phFuncTaRemoveToMark(handlerID);
        return PHFUNC_ERR_WAITING;
    }

    phFuncTaStop(handlerID);

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Exiting privateLotStart, retVal = %d", retVal);
    return retVal;
}
#endif

#ifdef LOTDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot end signal from handler
 *
 * Authors: Adam Huang
 *
 * Description:
 *
 ***************************************************************************/
phFuncError_t privateLotDone(
                              phFuncId_t handlerID,      /* driver plugin ID */
                              int timeoutFlag
                              )
{
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateLotDone");
  phFuncError_t retVal = PHFUNC_ERR_OK;

  if (1 == finalLotEndFlag)
  {
      /* exit the lot level if final lot end flag has been set */
      phTcomSetSystemFlag(handlerID->myTcom, PHTCOM_SF_CI_LEVEL_END, 1L);
      finalLotEndFlag = 0;
  }

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Exiting privateLotDone, retVal = %d", retVal);
  return retVal;
}
#endif

/*****************************************************************************
 * End of file
 *****************************************************************************/
