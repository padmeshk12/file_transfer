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
 *            Jiawei Lin, R&D Shanghai, CR27090&CR27346
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *            09 Jun 2000, Michael Vogt, adapted from prober driver
 *            02 Aug 2000, Michael Vogt, worked in all changes based on test
 *                         at Delta Design
 *            20 Mar 2001, Chris Joyce, added CommTest and split init into
 *                         init + comm + config
 *             6 Apr 2001, Siegfried Appich, changed debug_level from LOG_NORMAL
 *                         to LOG_DEBUG
 *            14 Oct 2004, Ryan Lessman, Ken Ward - added Delta Orion commands
 *            Nov 2005, Jiawei Lin, CR27090&CR27346
 *              support more query commands for Castle and Delta handler;
 *              introduce the stub function "privateGetStatusStub" as the
 *              universal interface to query all the kinds of information
 *              from Handler; those commands like temperature query (setpoint,
 *              soaktime, tempcontrol, etc.), site mapping and so on.
 *            18 Sep 2006, Xiaofei Han, CR32720
 *              Clear the SRQ queue before the binning operation to avoid 
 *              unexpected incorrect test result.
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

#define DEFAULT_SRQMASK "63"

/*--- typedefs --------------------------------------------------------------*/

/*--- variable --------------------------------------------------------------*/
int finalLotEndFlag = 0;                    /* final lot end flag */

/*--- functions -------------------------------------------------------------*/

static phFuncError_t doPauseHandler(phFuncId_t handlerID);
static phFuncError_t doUnpauseHandler(phFuncId_t handlerID);

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

    if (status != 0L)
	sprintf(statusMessage, "last received handler status word: 0x%08lx, explanation:\n", status);
    else
	sprintf(statusMessage, "last received handler status word: 0x%08lx\n", status);	

    switch (handlerID->model)
    {
      case PHFUNC_MOD_SUMMIT:
	if (status & (1L << 0))
	    strcat(statusMessage, "handler status: part(s) invalid\n");
	if (status & (1L << 1))
	    strcat(statusMessage, "handler status: out of trays for sort\n");
	if (status & (1L << 2))
	    strcat(statusMessage, "handler status: handler empty\n");
	if (status & (1L << 3))
	    strcat(statusMessage, "handler status: handler in manual mode\n");
	if (status & (1L << 4))
	    strcat(statusMessage, "handler status: test mode\n");
	if (status & (1L << 5))
	    strcat(statusMessage, "handler status: handler input cover is open\n");
	if (status & (1L << 6))
	    strcat(statusMessage, "handler status: handler sort cover is open\n");
	if (status & (1L << 7))
	    strcat(statusMessage, "handler status: out of guard bands\n");
	if (status & (1L << 8))
	    strcat(statusMessage, "handler status: jammed\n");
	if (status & (1L << 9))
	    strcat(statusMessage, "handler status: stopped\n");
	if (status & (1L << 10))
	    strcat(statusMessage, "handler status: parts are soaking\n");
	if (status & (1L << 11))
	    strcat(statusMessage, "handler status: handler door is open somewhere\n");
	break;
      case PHFUNC_MOD_CASTLE_LX:
	if (status & (1L << 0))
	    strcat(statusMessage, "handler status: a part needs retest\n");
	if (status & (1L << 1))
	    strcat(statusMessage, "handler status: main or auxiliary buffer is full\n");
	if (status & (1L << 2))
	    strcat(statusMessage, "handler status: handler empty\n");
	if (status & (1L << 3))
	    strcat(statusMessage, "handler status: handler in manual mode\n");
	if (status & (1L << 4))
	    strcat(statusMessage, "handler status: dynamic contactor calibration enabled\n");
	if (status & (1L << 5))
	    strcat(statusMessage, "handler status: VAT water level calibration enabled\n");
	if (status & (1L << 6))
	    strcat(statusMessage, "handler status: air pressure is low\n");
	if (status & (1L << 7))
	    strcat(statusMessage, "handler status: out of guard bands\n");
	if (status & (1L << 8))
	    strcat(statusMessage, "handler status: jammed\n");
	if (status & (1L << 9))
	    strcat(statusMessage, "handler status: stopped\n");
	if (status & (1L << 10))
	    strcat(statusMessage, "handler status: parts are soaking\n");
	if (status & (1L << 11))
	    strcat(statusMessage, "handler status: the chamber door is open\n");
	if (status & (1L << 12))
	    strcat(statusMessage, "handler status: the VAT or ATL door is open\n");
	if (status & (1L << 13))
	    strcat(statusMessage, "handler status: GPIB error\n");
	break;
    case PHFUNC_MOD_MATRIX:
      if (status & (1L << 0))
        strcat(statusMessage, "handler status: a part needs retest\n");
      if (status & (1L << 1))
        strcat(statusMessage, "handler status: not used\n");
      if (status & (1L << 2))
        strcat(statusMessage, "handler status: handler empty\n");
      if (status & (1L << 3))
        strcat(statusMessage, "handler status: handler in manual mode\n");
      if (status & (1L << 4))
        strcat(statusMessage, "handler status: dynamic contactor calibration enabled\n");
      if (status & (1L << 5))
        strcat(statusMessage, "handler status: not used\n");
      if (status & (1L << 6))
        strcat(statusMessage, "handler status: air pressure is low\n");
      if (status & (1L << 7))
        strcat(statusMessage, "handler status: out of guard bands\n");
      if (status & (1L << 8))
        strcat(statusMessage, "handler status: jammed\n");
      if (status & (1L << 9))
        strcat(statusMessage, "handler status: stopped\n");
      if (status & (1L << 10))
        strcat(statusMessage, "handler status: parts are soaking\n");
      if (status & (1L << 11))
        strcat(statusMessage, "handler status: tray access door is open\n");
      if (status & (1L << 12))
        strcat(statusMessage, "handler status: handler access door is open\n");
      if (status & (1L << 13))
        strcat(statusMessage, "handler status: GPIB error\n");
      if (status & (1L << 14))
        strcat(statusMessage, "handler status: Active jam but the Handler is still able to test parts\n");
      if (status & (1L << 15))
        strcat(statusMessage, "handler status: Contactors are being cleaned\n");
      if (status & (1L << 16))
        strcat(statusMessage, "handler status: Auto Contactor Learning error\n");
      if (status & (1L << 17))
        strcat(statusMessage, "handler status: Machine is initializing\n");
      if (status & (1L << 18))
        strcat(statusMessage, "handler status: Partial input tray warning is active\n");
      if (status & (1L << 19))
        strcat(statusMessage, "handler status: Tray service required\n");
      if (status & (1L << 20))
        strcat(statusMessage, "handler status: Handler Idle\n");
      if (status & (1L << 21))
        strcat(statusMessage, "handler status: Gone To Retest memory data buffer full\n");
      if (status & (1L << 22))
        strcat(statusMessage, "handler status: Sort Tray to Input Tray transfer is active\n");
      if (status & (1L << 23))
        strcat(statusMessage, "handler status: Handler loading devices\n");
      if (status & (1L << 24))
        strcat(statusMessage, "handler status: Handler unloading devices\n");
      if (status & (1L << 25))
        strcat(statusMessage, "handler status: Out of SCD\n");
      break;
      case PHFUNC_MOD_FLEX:
	if (status & (1L << 0))
	    strcat(statusMessage, "handler status: a part needs retest\n");
	if (status & (1L << 1))
	    strcat(statusMessage, "handler status: output full\n");
	if (status & (1L << 2))
	    strcat(statusMessage, "handler status: handler empty\n");
	if (status & (1L << 3))
	    strcat(statusMessage, "handler status: motor diagnostics enabled\n");
	if (status & (1L << 4))
	    strcat(statusMessage, "handler status: dynamic contactor calibration enabled\n");
	if (status & (1L << 5))
	    strcat(statusMessage, "handler status: input inhibited\n");
	if (status & (1L << 6))
	    strcat(statusMessage, "handler status: sort inhibited\n");
	if (status & (1L << 7))
	    strcat(statusMessage, "handler status: out of guard bands\n");
	if (status & (1L << 8))
	    strcat(statusMessage, "handler status: jammed\n");
	if (status & (1L << 9))
	    strcat(statusMessage, "handler status: stopped\n");
	if (status & (1L << 10))
	    strcat(statusMessage, "handler status: parts are soaking\n");
	if (status & (1L << 11))
	    strcat(statusMessage, "handler status: input or sort inhibited or chamber door is open\n");
	break;
      case PHFUNC_MOD_RFS:
	if (status & (1L << 0))
	    strcat(statusMessage, "handler status: part(s) invalid\n");
	if (status & (1L << 1))
	    strcat(statusMessage, "handler status: out of trays for sort\n");
	if (status & (1L << 2))
	    strcat(statusMessage, "handler status: handler empty\n");
	if (status & (1L << 3))
	    strcat(statusMessage, "handler status: handler in manual mode\n");
	if (status & (1L << 4))
	    strcat(statusMessage, "handler status: handler in cone test mode\n");
	if (status & (1L << 5))
	    strcat(statusMessage, "handler status: handler input cover is open\n");
	if (status & (1L << 6))
	    strcat(statusMessage, "handler status: handler sort cover is open\n");
	if (status & (1L << 7))
	    strcat(statusMessage, "handler status: out of guard bands\n");
	if (status & (1L << 8))
	    strcat(statusMessage, "handler status: jammed\n");
	if (status & (1L << 9))
	    strcat(statusMessage, "handler status: stopped\n");
	if (status & (1L << 10))
	    strcat(statusMessage, "handler status: parts are soaking\n");
	if (status & (1L << 11))
	    strcat(statusMessage, "handler status: handler door is open somewhere\n");
	break;
      case PHFUNC_MOD_ORION:
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "Orion does not report status messages.");
    break;
      case PHFUNC_MOD_ECLIPSE:
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "Eclipse does not report status messages.");
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

    handlerID->p.stopped = (handlerID->p.status & (1L << 9)) ? 1 : 0;
}

/* query handler's status and analyze it */
static phFuncError_t queryStatus(phFuncId_t handlerID, int warn)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    static char status[64];

    switch (handlerID->model)
    {
    case PHFUNC_MOD_CASTLE_LX:
    case PHFUNC_MOD_MATRIX:
    case PHFUNC_MOD_SUMMIT:
    case PHFUNC_MOD_FLEX:
    case PHFUNC_MOD_RFS:

        /* it may happen, that the handler returns an empty string and not
           the status information, therefore we must ask for a string here
           and not for a number. The transaction module is able to return
           an empty string on "%s" format (basic scanf is not) */
        PhFuncTaCheck(phFuncTaSend(handlerID, "status?%s", handlerID->p.eol));
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", status));
        if (sscanf(status, "%ld", &handlerID->p.status) != 1)
        {
	        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
	        "status word not received from handler: \"%s\", trying again", status);
	        phFuncTaRemoveStep(handlerID);
	        phFuncTaRemoveStep(handlerID);
	        return PHFUNC_ERR_WAITING;
        }

        analyzeStatus(handlerID, warn);
        break;
    case PHFUNC_MOD_ORION:
    case PHFUNC_MOD_ECLIPSE:
        /* does not support status query at this time. */
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

    static char handlerAnswer[64] = "";
    unsigned long population;
    long populationPicked = 0L;
    int i;
    int wasSRQquery = 0;
    char *tempChr = NULL;

    switch(handlerID->model)
    {
      case PHFUNC_MOD_SUMMIT:
	/* summit does not support srqbyte? and it does not understand
           testpartsready? */
	    PhFuncTaCheck(phFuncTaSend(handlerID, "testparts?%s", handlerID->p.eol));
//	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
	    break;
      case PHFUNC_MOD_CASTLE_LX:
      case PHFUNC_MOD_MATRIX:
      case PHFUNC_MOD_FLEX:
      case PHFUNC_MOD_RFS:
	/* note: as far as known, the RFS does not (yet) support more than 4
	   sites. Anyway this is not checked here. The basic
	   assumption is: As soon as a handler provides more than 4
	   sites, the testpartsready query is available and must be
	   used. If up to 4 sites, the srqbyte query is used because I
	   assume that this query is handled more faster by the
	   handler */
        if (handlerID->noOfSites > 4)
        {
            PhFuncTaCheck(phFuncTaSend(handlerID, "testpartsready?%s",handlerID->p.eol));
            //	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            if (retVal != PHFUNC_ERR_OK) {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
        }
        else
        {
            PhFuncTaCheck(phFuncTaSend(handlerID, "srqbyte?%s", handlerID->p.eol));
            //	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            if (retVal != PHFUNC_ERR_OK) {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
            wasSRQquery = 1;
        }
        break;
      case PHFUNC_MOD_ORION:
        PhFuncTaCheck(phFuncTaSend(handlerID, "testpartsready?%s",handlerID->p.eol));
//        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
        /* Orion answer to 'testpartsready?' contains five pieces of information, M,I,S,X,Y
           M = value representing a bit pattern indicating site population.  1 = site populated, ready for test.
           I = current index count 
           S = strip ID from Dot Code Reader (DCR).  The Dot Code is found on the strip.
           Y = current row.
           X = current column.  
           For now, only the 'M' value is interesting, so we know site population.  The other values are ignored. */
        strcpy(handlerAnswer, strtok(handlerAnswer, ","));  /* Want only the value in front of the first comma. (M) */
        break;
      case PHFUNC_MOD_ECLIPSE:
        /* for Eclipse handler, it send FULLSITES? */
        PhFuncTaCheck(phFuncTaSend(handlerID, "FULLSITES?%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
        if (retVal != PHFUNC_ERR_OK)
        {
            phFuncTaRemoveStep(handlerID); // force re-send of command
            return retVal;
        }
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
    }

    /* check answer from handler, similar for all handlers.  remove
       two steps (the query and the answer) if an empty string was
       received */
    if (PHFUNC_MOD_ECLIPSE == handlerID->model)
    {
        /* eclipse handler */
        /* ensure string is in upper case */
        setStrUpper(handlerAnswer);

        if (sscanf(handlerAnswer, "FULLSITES %lx", &population) != 1)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                    "site population not received from handler: \n"
                    "received \"%s\" format expected \"FULLSITES xxxxxxxx\" \n"
                    "trying again", handlerAnswer);
            phFuncTaRemoveStep(handlerID);
            phFuncTaRemoveStep(handlerID);
            handlerID->p.oredDevicePending = 0;
            return PHFUNC_ERR_ANSWER;
        }
        tempChr = strchr(handlerAnswer, ' ')+1; 
        handlerID->p.fullsitesStrLen = strlen(tempChr);
    }
    else
    {
        /* other model */
        if (sscanf(handlerAnswer, "%lu", &population) != 1)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                    "site population not received from handler: \"%s\", trying again", handlerAnswer);
            phFuncTaRemoveStep(handlerID);
            phFuncTaRemoveStep(handlerID);
            return PHFUNC_ERR_WAITING;
        }
    }

    /* remove invalid bits */
    if (wasSRQquery)
	population = population & 0x0f;

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
    int srqPicked = 0;
    static int received = 0;
    static char notification[512];

    struct timeval pollStartTime;
    struct timeval pollStopTime;
    int timeout;
    int i;
    int partMissing = 0;

    phFuncError_t retVal = PHFUNC_ERR_OK;

    if (handlerID->p.strictPolling)
    {
        /* before any polling, check SRQ to check lot status */
        PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 1));
        if (received && (srq == 0xC0))
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
//	    PhFuncTaCheck(pollParts(handlerID));
        retVal = pollParts(handlerID);
        if (retVal != PHFUNC_ERR_OK) {
            return PHFUNC_ERR_WAITING;
        }
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
	      case PHFUNC_MOD_SUMMIT:
		if (handlerID->noOfSites > 1 && (srq & 0x01))
		{
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
			"received generic test start SRQ, need to poll");
		    /* now there may be parts, but we don't know where,
		       must poll */
//		    PhFuncTaCheck(pollParts(handlerID));
            retVal = pollParts(handlerID);
            if (retVal != PHFUNC_ERR_OK) {
                return PHFUNC_ERR_WAITING;
            }
		}
		else if (srq & 0x01) /* noOfSites == 1 */
		{
		    /* there is exactly one part given in the SRQ byte */
		    partMissing = 0;
		    handlerID->p.devicePending[0] = 1;
		    handlerID->p.oredDevicePending = 1;
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
				     "new device present at site \"%s\" (SRQ)", 
				     handlerID->siteIds[0]);
		}
		else if (srq & 0x36) /* check all special bits */
		{
		    /* some exceptional SRQ occured */
		    /* do nothing, status is analyzed later */
		    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
			"received exceptional SRQ 0x%02x:", srq);
		    if (srq & 0x02)
		    {
			PhFuncTaCheck(phFuncTaSend(handlerID, "devicenotify?%s", handlerID->p.eol));
//			PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", notification));
            retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", notification);
            if (retVal != PHFUNC_ERR_OK) {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
			phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
			    "handler SRQ status: device notification \"%s\"", notification);
			phFuncTaRemoveStep(handlerID);
			phFuncTaRemoveStep(handlerID);
			phFuncTaRemoveStep(handlerID);
			return PHFUNC_ERR_WAITING;
		    }
		    if (srq & 0x04)
		    {
			PhFuncTaCheck(phFuncTaSend(handlerID, "lotnotify?%s", handlerID->p.eol));
//			PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", notification));
            retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", notification);
            if (retVal != PHFUNC_ERR_OK) {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
			phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
			    "handler SRQ status: lot notification \"%s\"", notification);
			phFuncTaRemoveStep(handlerID);
			phFuncTaRemoveStep(handlerID);
			phFuncTaRemoveStep(handlerID);
			return PHFUNC_ERR_WAITING;
		    }
		    if (srq & 0x10)
		    {
			phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
			    "handler SRQ status: handler notification pending");
			/* will analyze status outside this call */
		    }
		    if (srq & 0x20)
		    {
			PhFuncTaCheck(phFuncTaSend(handlerID, "interfacenotify?%s", handlerID->p.eol));
//			PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", notification));
            retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", notification);
            if (retVal != PHFUNC_ERR_OK) {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
			phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
			    "handler SRQ status: interface notification \"%s\"", notification);
			phFuncTaRemoveStep(handlerID);
			phFuncTaRemoveStep(handlerID);
			phFuncTaRemoveStep(handlerID);
			return PHFUNC_ERR_WAITING;
		    }
		}		
		break;
	      case PHFUNC_MOD_CASTLE_LX:
          case PHFUNC_MOD_MATRIX:
	      case PHFUNC_MOD_RFS:
	      case PHFUNC_MOD_FLEX:
		if (handlerID->noOfSites > 4 && (srq & 0x0f))
		{
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
			"received generic test start SRQ, need to poll");
		    /* now there may be parts, but we don't know where,
		       must poll */
//		    PhFuncTaCheck(pollParts(handlerID));
            retVal = pollParts(handlerID);
            if (retVal != PHFUNC_ERR_OK) {
                return PHFUNC_ERR_WAITING;
            }
		}
		else if (srq & 0x0f)
		{
		    /* there may be parts given in the SRQ byte */
		    partMissing = 0;
		    for (i=0; i<handlerID->noOfSites; i++)
		    {
			if (srq & (1 << i))
			{
			    handlerID->p.devicePending[i] = 1;
			    handlerID->p.oredDevicePending = 1;
			    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
				"new device present at site \"%s\" (SRQ)", 
				handlerID->siteIds[i]);
			    srqPicked |= (1 << i);
			}
			else if (handlerID->p.deviceExpected[i])
			    partMissing = 1;
		    }

		    /* check whether some parts seem to be missing. In
		       case of a reprobe, we must at least receive the
		       reprobed devices. In all other cases we should
		       receive devices in all active sites. In case
		       something seems to be missing, we double check:
		       flush all remaining SRQs and than poll for the
		       current population */

		    if (partMissing)
		    {
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
			    "some expected device(s) seem(s) to be missing, polling once to be sure");
//			PhFuncTaCheck(pollParts(handlerID));
            retVal = pollParts(handlerID);
            if (retVal != PHFUNC_ERR_OK) {
                return PHFUNC_ERR_WAITING;
            }
            PhFuncTaCheck(flushSRQs(handlerID));
		    }

		    /* check whether we have received to many parts */

		    if ((srq & 0x0f) != srqPicked)
		    {
			phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
			    "The handler seems to present more devices than configured\n"
			    "The driver configuration must be changed to support more sites");
		    }
		}
		else
		{
		    /* some exceptional SRQ occured */
		    /* do nothing, status is analyzed later */
		    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
			"received exceptional SRQ 0x%02x", srq);
		}
		break;
	      case PHFUNC_MOD_ORION:
		phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
			"received generic Orion test start SRQ, need to poll");
//		PhFuncTaCheck(pollParts(handlerID));
        retVal = pollParts(handlerID);
        if (retVal != PHFUNC_ERR_OK) {
            return PHFUNC_ERR_WAITING;
        }
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
        break;
/* End of Huatek Modification */

          case PHFUNC_MOD_ECLIPSE:
            if (srq == 0x41)
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                        "received generic test start SRQ, need to poll");
                /* test start SRQ will also set lot start flag even there is no lot start signal received*/
                handlerID->p.lotStarted = 1;

                retVal = pollParts(handlerID);
                if (retVal != PHFUNC_ERR_OK) {
                    return PHFUNC_ERR_WAITING;
                }
            }
            else if (srq == 0xC0)
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
            break;
 
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
          default: break;
/* End of Huatek Modification */
	    }
	}
	else
	{
          return PHFUNC_ERR_WAITING;
	}
    }
    return retVal;
}

/*****************************************************************************
 *
 * Compare bin information between the sent binning command
 * and the received echoed binning information.
 *
 * format of each string for 16 bin (0-9 and A-H) configuration: 
 *   BINON: XXXXXXXX,XXXXXXXX,XXXXXXXX,XXXXXXXX;  (sent)
 *    ECHO: XXXXXXXX,XXXXXXXX,XXXXXXXX,XXXXXXXX;  (received)
 * X=0 to 9 and A to H; 
 * 
 * the format of each string is as follows (256 categories ):
 *   BINON: xyxyxyxyxyxyxyxy,xyxyxyxyxyxyxyxy,xyxyxyxyxyxyxyxy,xyxyxyxyxyxyxyxy; (sent)
 *   ECHO:  xyxyxyxyxyxyxyxy,xyxyxyxyxyxyxyxy,xyxyxyxyxyxyxyxy,xyxyxyxyxyxyxyxy; (received)
 * X=00 to FF (hexadecimal number)
 *
 ***************************************************************************/
static phFuncError_t checkBinningInformation(
                                            phFuncId_t handlerID     /* driver plugin ID */,
                                            const char *sent         /* sent binning command */,
                                            const char *received     /* returned binning info */
                                            )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    const char* s=sent;
    const char* r=received;
    int checkError = 0;

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

    if (handlerID->binIds)
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
            if( 16 == handlerID->p.binningCommandFormat )
            {
                int i_ret = strtol(handlerID->binIds[binNumber], (char **)NULL, 10);
                sprintf(binCode, "%x", i_ret);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "using binIds set binNumber %d to binCode \"%s\" ", 
                        binNumber, binCode);
            }
            else
            {
                int i_ret = strtol(handlerID->binIds[binNumber], (char **)NULL, 10);
                sprintf(binCode, "%02x", i_ret);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "using binIds set binNumber %d to binCode \"%s\" ", 
                        binNumber, binCode);
            }
        }
    }
    else
    {
        /* if handler bin id is not defined (default mapping), use the bin number directly */
        if( 16 == handlerID->p.binningCommandFormat )
        {
            if (binNumber < 0 || binNumber > 15)
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                        "received illegal request for bin %ld ",
                        binNumber);
                retVal=PHFUNC_ERR_BINNING;
            }
            else
            {
                sprintf(binCode, "%lx", binNumber);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "using binIds set binNumber %d to binCode \"%s\" ", 
                        binNumber, binCode);
            }
        }
        else
        {
            if (binNumber < 0 || binNumber > 255)
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                        "received illegal request for bin %ld ",
                        binNumber);
                retVal=PHFUNC_ERR_BINNING;
            }
            else
            {
                sprintf(binCode, "%02lx", binNumber);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "using binIds set binNumber %d to binCode \"%s\" ", 
                        binNumber, binCode);
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

    int i;
    char testresults[PHESTATE_MAX_SITES*4 + 16];
    static char querybins[PHESTATE_MAX_SITES*4 + 512];
    char thisbin[32];
    char *binPtr;
    char reprobe_bin[4]; /* Hardcoded 4, should be enough for '100' string.*/
    int oneQueryBin;
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    int expected=0;
/* End of Huatek Modification */
    int sendBinning = 0;
    int retryCount = 0;
    int repeatBinning = 0;
    int reprobeRequests = 0;
    long reprobeData = 0;
    char binningMsg[1024+5]; /*assuming 512 site, 1024+5 should be enough. 
                               5 is for "trs " plus a \0 character */
    char tempstr[10] = {0};
    int maxNoOfSiteInCmd = 0;

    /** 
     * CR32720
     * We are not supposed to have any SRQs in the SRQ queue at the moment.
     * So at the very beginning of the binning operation, we try to empty the
     * SRQ queue in case any duplicated, delayed or miss-sent SRQs arrived 
     * after a valid SRQ has been received (either by polling method or by 
     * the interrupt method). If these SRQs are not cleared,the tester will 
     * perfome the next round of test based on these unexpected SRQs. This will 
     * absolutely leads to incorrect test result. For example, the tested devices
     * will get re-tested since the tester performs next round of test on the basis 
     * of the duplicated SRQs which is delayed.
     */

     phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Trying to empty the SRQ queue to remove the"
                     " delayed/duplicated SRQs before sending out the binning/reprobing message...");
     PhFuncTaCheck(flushSRQs(handlerID)); 

     /* End of CR32720 */

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
	  case PHFUNC_MOD_SUMMIT:
	    /* find out whether we need to reprobe any device */
	    reprobeRequests = 0;
	    for (i=0; i<handlerID->noOfSites; i++)
	    {
		if (handlerID->activeSites[i] && 
		    (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
			oldPopulation[i] == PHESTATE_SITE_POPDEACT) &&
		    perSiteReprobe && perSiteReprobe[i])
		    reprobeRequests++;
	    }

	    /* use 'bin' and 'reprobe' command.
	     reprobe takes encoded information !!! */
	    if (reprobeRequests)
	    {
		reprobeData = 0L;
		if (handlerID->p.summitReprobeBug)
		{
		    /* the Summit SW release 1.7.3 and 1.7.4 is known
                       to not accept the encoded reprobe
                       information. Instead it accepts "reprobe
                       1". Since all devices move, this will cause the
                       devices that need real reprobe also to
                       move. However, the statistics in the SUmmit
                       handler will not match the statistics of
                       SmarTest. In single site scenarios, there is no
                       visible impact */

		    reprobeData = 1;
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
			"will send reprobe request for site \"%s\", expecting to reprobe all devices", 
			handlerID->siteIds[0]);
		}
		else
		{
		    /* this is how it should work, if Summit SW is bug free ... */
		    for (i=0; i<handlerID->noOfSites; i++)
		    {
			if (handlerID->activeSites[i] && 
			    (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
				oldPopulation[i] == PHESTATE_SITE_POPDEACT) &&
			    perSiteReprobe[i])
			{
			    reprobeData |= (1L << i);
			    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
				"will reprobe device at site \"%s\"", 
				handlerID->siteIds[i]);
			    sendBinning = 1;
			}
			else
			{
			    /* there is a no device here */
			    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
				"no device to reprobe at site \"%s\"", 
				handlerID->siteIds[i]);
			}
		    }
		}
		PhFuncTaCheck(phFuncTaSend(handlerID, "reprobe %ld%s", 
		    reprobeData, handlerID->p.eol));
	    }
	    else
	    {
                sprintf(binningMsg, "trs ");
		for (i=0; i<handlerID->noOfSites; i++)
		{
		    if (handlerID->activeSites[i] && 
			(oldPopulation[i] == PHESTATE_SITE_POPULATED ||
			    oldPopulation[i] == PHESTATE_SITE_POPDEACT))
		    {
			/* there is a device here */
			    /* bin this one */
			if (perSiteBinMap[i] < 0 || perSiteBinMap[i] > 99)
			{
				/* illegal bin code */
			    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
				"received illegal request for bin %ld at site \"%s\"",
				perSiteBinMap[i], handlerID->siteIds[i]);
			    retVal =  PHFUNC_ERR_BINNING;
                            break;
			}
			else
			{
                            /*
			    PhFuncTaCheck(phFuncTaSend(handlerID, "%c bin %ld%s", 
				(char) ('a' + i), perSiteBinMap[i], handlerID->p.eol));
                            */
			    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
				"will bin device at site \"%s\" to %ld", 
				handlerID->siteIds[i], perSiteBinMap[i]);
			    /*sendBinning = 1;*/

                if(i == 0)
                {
                    sprintf(tempstr, "%ld", perSiteBinMap[i]);
                }
                else
                {
                    sprintf(tempstr, ",%ld", perSiteBinMap[i]);
                }
                strcat(binningMsg, tempstr);
			}
		    }
		    else
		    {
			/* there is a no device here */
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "no device to bin at site \"%s\"", 
			    handlerID->siteIds[i]);
                       
                        if(i != 0)
                        {
                          sprintf(tempstr, ",");
                        }
                        strcat(binningMsg, tempstr);

		    }
		}

                if( retVal !=  PHFUNC_ERR_BINNING)
                {
                    PhFuncTaCheck(phFuncTaSend(handlerID, binningMsg));
                    sendBinning = 1;
		}
	    }
	    break;
	  case PHFUNC_MOD_CASTLE_LX:
      case PHFUNC_MOD_MATRIX:
	  case PHFUNC_MOD_RFS:
	  case PHFUNC_MOD_ORION:
	    if (handlerID->model != PHFUNC_MOD_RFS || handlerID->p.rfsUseTestresults)
	    {
		/* use 'testresults' command, bin 100 is reprobe (For Orion, bin 0 is retest.)*/
		strcpy(testresults, "testresults ");
		if (handlerID->model != PHFUNC_MOD_ORION)
		{
		    strcpy(reprobe_bin, "100");
		}
		else
		{
		    strcpy(reprobe_bin, "0");  /* For Orion, bin 0 is retest*/
		}
		for (i=0; i<handlerID->noOfSites; i++)
		{
		    if (handlerID->activeSites[i] && 
			(oldPopulation[i] == PHESTATE_SITE_POPULATED ||
			    oldPopulation[i] == PHESTATE_SITE_POPDEACT))
		    {
			/* there is a device here */
			if (perSiteReprobe && perSiteReprobe[i])
			{
			    /* reprobe this one */
			    strcat(testresults, reprobe_bin);
			    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
				"will reprobe device at site \"%s\"", 
				handlerID->siteIds[i]);
			    sendBinning = 1;
			}
			else
			{
			    /* bin this one */
			    if (perSiteBinMap[i] < 0 || perSiteBinMap[i] > 100)
			    {
				/* illegal bin code */
				phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
				    "received illegal request for bin %ld at site \"%s\"",
				    perSiteBinMap[i], handlerID->siteIds[i]);
				retVal =  PHFUNC_ERR_BINNING;
			    }
			    else
			    {
				sprintf(thisbin, "%ld", perSiteBinMap[i]);
				strcat(testresults, thisbin);
				phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
				    "will bin device at site \"%s\" to %s", 
				    handlerID->siteIds[i], thisbin);
				sendBinning = 1;
			    }
			}
		    }
		    else
		    {
			/* there is no device here */
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "no device to reprobe or bin at site \"%s\"", 
			    handlerID->siteIds[i]);
		    }
		    if (i+1 < handlerID->noOfSites)
			strcat(testresults, ",");
		}
		if (sendBinning)
		{
		    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", testresults, handlerID->p.eol));
		}
		break;
	    }
	    /* else fall through to 'bin' and 'reprobe' command (RFS in old mode) */
	  case PHFUNC_MOD_FLEX:
	    /* use 'bin' and 'reprobe' command */
	    for (i=0; i<handlerID->noOfSites; i++)
	    {
		if (handlerID->activeSites[i] && 
		    (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
			oldPopulation[i] == PHESTATE_SITE_POPDEACT))
		{
		    /* there is a device here */
		    if (perSiteReprobe && perSiteReprobe[i])
		    {
			/* reprobe this one */
			PhFuncTaCheck(phFuncTaSend(handlerID, "%c reprobe%s", 
			    (char) ('a' + i), handlerID->p.eol));
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
			    "will reprobe device at site \"%s\"", 
			    handlerID->siteIds[i]);
			sendBinning = 1;
		    }
		    else
		    {
			/* bin this one */
			if (perSiteBinMap[i] < 0 || perSiteBinMap[i] > 99)
			{
			    /* illegal bin code */
			    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
				"received illegal request for bin %ld at site \"%s\"",
				perSiteBinMap[i], handlerID->siteIds[i]);
			    retVal =  PHFUNC_ERR_BINNING;
			}
			else
			{
			    PhFuncTaCheck(phFuncTaSend(handlerID, "%c bin %ld%s", 
				(char) ('a' + i), perSiteBinMap[i], handlerID->p.eol));
			    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
				"will bin device at site \"%s\" to %ld", 
				handlerID->siteIds[i], perSiteBinMap[i]);
			    sendBinning = 1;
			}
		    }
		}
		else
		{
		    /* there is a no device here */
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			"no device to reprobe or bin at site \"%s\"", 
			handlerID->siteIds[i]);
		}
	    }
	    break;
      case PHFUNC_MOD_ECLIPSE:
        /* for eclipse, should send BINON to the handler */
        strcpy(testresults, "BINON:");

        /*
         * The site number in the BINON/BINOFF command should match the
         * site number returns from the FULLSITES command where even there
         * are less sites configed in both the handler and driver. Otherwise
         * the handler may report alarms.
         */

        if (handlerID->p.fullsitesStrLen*4 > PHESTATE_MAX_SITES)
        {
            maxNoOfSiteInCmd = PHESTATE_MAX_SITES;
        }
        else
        {
            maxNoOfSiteInCmd = handlerID->p.fullsitesStrLen*4;
        }

        for ( i=maxNoOfSiteInCmd - 1; i>=0 && i<PHESTATE_MAX_SITES && retVal==PHFUNC_ERR_OK ; --i )
        {
            if ( handlerID->activeSites[i] && 
                    (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
                     oldPopulation[i] == PHESTATE_SITE_POPDEACT) )
            {
                /* there is a device here */
                if (perSiteReprobe && perSiteReprobe[i])
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                            "eclipse hasn't supported the reprobe command");
                    sendBinning = 0;
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
                strcat(testresults, handlerID->p.binningCommandFormat == 16 ? "0" : "00");
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
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
	  default: break;
/* End of Huatek Modification */
	}

    /* for eclipse, will always verify the binning info */
    if (PHFUNC_MOD_ECLIPSE == handlerID->model && sendBinning)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                "will perform bin verification ....");
        retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", querybins);
        if (retVal != PHFUNC_ERR_OK)
        {
            phFuncTaRemoveStep(handlerID); // force re-send of command 
            return retVal;
        }

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
	else if ( handlerID->p.verifyBins && sendBinning )
	{
        /* verify binning, if necessary */
	    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
		"will perform bin verification ....");
	    PhFuncTaCheck(phFuncTaSend(handlerID, "querybins?%s", handlerID->p.eol));
	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", querybins));
	    binPtr = querybins;
	    for (i=0; i<handlerID->noOfSites; i++)
	    {
		oneQueryBin = -1;
		if (!binPtr || sscanf(binPtr, "%d", &oneQueryBin) != 1)
		{
		    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
			"did not receive bin data for site \"%s\"",
			handlerID->siteIds[i]);
		    retVal = PHFUNC_ERR_BINNING;
		}
		else
		{
		    if (handlerID->activeSites[i] && 
			(oldPopulation[i] == PHESTATE_SITE_POPULATED ||
			    oldPopulation[i] == PHESTATE_SITE_POPDEACT))
		    {
			if (perSiteReprobe && perSiteReprobe[i])
			{
			    /* it's a reprobed device */
			    switch (handlerID->model)
			    {
			      case PHFUNC_MOD_CASTLE_LX:
                case PHFUNC_MOD_MATRIX:
			      case PHFUNC_MOD_SUMMIT:
				expected = 100;
				break;
			      case PHFUNC_MOD_FLEX:
				expected = -3;
				break;	
			      case PHFUNC_MOD_RFS:
				expected = -3;
				break;
			      case PHFUNC_MOD_ORION:
				phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
					"Orion does not support reprobe.");
				break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
  			      default: break;
/* End of Huatek Modification */
			    }
			}
			else
			    expected = (int) perSiteBinMap[i];

			if (expected != oneQueryBin)
			{
			    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
				"received different bin code (%d) than expected (%d) for site \"%s\"",
				oneQueryBin, expected, handlerID->siteIds[i]);
			    retVal = PHFUNC_ERR_BINNING;
			}
		    }

		    /* move to next bin */
		    while (isgraph(*binPtr))
			binPtr++;
		    while (isspace(*binPtr))
			binPtr++;
		    if (*binPtr == '\0')
			binPtr = NULL;
		}
	    }
	    if (retVal == PHFUNC_ERR_OK)
	    {
		phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
		    ".... verification succeeded");
		PhFuncTaCheck(phFuncTaSend(handlerID, "releaseparts%s%s", 
		    handlerID->model == PHFUNC_MOD_FLEX ? "!" : "" ,
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
		"will try to send bin and reprobe data %d more time(s) before giving up",
		1 + handlerID->p.verifyMaxCount - retryCount);

	    /* we do a loop, now go back to stored interaction mark */
	    phFuncTaRemoveToMark(handlerID);	
	}

    } while (repeatBinning);

    return retVal;
}


/* send temperature setup to handler, if necessary */
static phFuncError_t reconfigureTemperature(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    struct tempSetup tempData;
    int i;
    int needsRestart = 0;
    static char recipestartOK[512];
    static char lowguardOK[512];
    static char highguardOK[512];
    static char setpointOK[512];
    static char soaktimeOK[512];
    static char recipeendOK[512];
    const char *myRecipeFile = NULL;
    phConfError_t confError;

    /* get all temperature configuration values */
    if (!phFuncTempGetConf(handlerID->myConf, handlerID->myLogger,
	&tempData))
    {
	phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
	    "temperature control configuration corrupt");
	return PHFUNC_ERR_CONFIG;
    }

    /* OK, we have it, now check what we need to do */
    if (tempData.mode == PHFUNC_TEMP_MANUAL)
    {
	phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
	    "temperature control must be setup manually at the handler");
	return retVal;
    }
    else if (tempData.mode == PHFUNC_TEMP_AMBIENT)
    {
	phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
	    "temperature control will be set to \"ambient\" by the driver");
	switch(handlerID->model)
	{
      case PHFUNC_MOD_MATRIX:
        PhFuncTaCheck(phFuncTaSend(handlerID, "tempcontrol 2%s", handlerID->p.eol));
        break;
	  case PHFUNC_MOD_SUMMIT:
	    PhFuncTaCheck(phFuncTaSend(handlerID, "tempcontrol 0%s", handlerID->p.eol));
	    break;
	  case PHFUNC_MOD_CASTLE_LX:
	    /* PhFuncTaCheck(phFuncTaSend(handlerID, "tempcontrol 0%s", handlerID->p.eol)); */
	    PhFuncTaCheck(phFuncTaSend(handlerID, "testtemp 0%s", handlerID->p.eol));
	    break;
	  case PHFUNC_MOD_FLEX:
	  case PHFUNC_MOD_RFS:
	    PhFuncTaCheck(phFuncTaSend(handlerID, "testtemp atambient%s", handlerID->p.eol));
	    localDelay_us(100000);
	    /* PhFuncTaCheck(phFuncTaSend(handlerID, "tempcontrol disable%s", handlerID->p.eol)); */
	    break;
      default: break;
	}
	return retVal;
    }

    /* tempData.mode == PHFUNC_TEMP_ACTIVE */

    /* this is the real job, define and activate all temperature
       parameters, check for correct chamber names first */
    switch(handlerID->model)
    {
	  case PHFUNC_MOD_SUMMIT:
	    for (i=0; i<tempData.chambers; i++)
	    {
		if (strcasecmp(tempData.name[i], "testsite")   != 0)
		{
		    /* illegal chamber name */
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			"the handler does not provide a temperature chamber with name \"%s\"\n"
			"please check the configuration of \"%s\"",
			tempData.name[i], PHKEY_TC_CHAMB);
		    retVal = PHFUNC_ERR_CONFIG;
		}
	    }
	    break;	    
	  case PHFUNC_MOD_CASTLE_LX:
	    for (i=0; i<tempData.chambers; i++)
	    {
		if (strcasecmp(tempData.name[i], "zone1") != 0 &&
		    strcasecmp(tempData.name[i], "zone2") != 0 &&
		    strcasecmp(tempData.name[i], "zone3") != 0 &&
		    strcasecmp(tempData.name[i], "zone4") != 0 &&
		    strcasecmp(tempData.name[i], "all")   != 0)
		{
		    /* illegal chamber name */
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			"the handler does not provide a temperature chamber with name \"%s\"\n"
			"please check the configuration of \"%s\"",
			tempData.name[i], PHKEY_TC_CHAMB);
		    retVal = PHFUNC_ERR_CONFIG;
		}
	    }
	    break;
    case PHFUNC_MOD_MATRIX:
      for (i=0; i<tempData.chambers; i++)
      {
        if((strcasecmp(tempData.name[i], "inp_chamber")  !=0) &&
           (strcasecmp(tempData.name[i], "xfr_chamber")  !=0) &&
           (strcasecmp(tempData.name[i], "inp_shuttle")  !=0) &&
           (strcasecmp(tempData.name[i], "linp_shuttle") !=0) &&
           (strcasecmp(tempData.name[i], "rinp_shuttle") !=0) &&
           (strcasecmp(tempData.name[i], "desoak")       !=0) &&
           (strcasecmp(tempData.name[i], "ldsk_shuttle") !=0) &&
           (strcasecmp(tempData.name[i], "rdsk_shuttle") !=0) &&
           (strcasecmp(tempData.name[i], "testsite")     !=0) &&
           (strncasecmp(tempData.name[i], "lts_", 4)     !=0) &&
           (strncasecmp(tempData.name[i], "rts_", 4)     !=0) &&
           (strcasecmp(tempData.name[i], "contactor")    !=0) &&
           (strcasecmp(tempData.name[i], "ts_humidity")  !=0) &&
           (strcasecmp(tempData.name[i], "")             !=0) &&
           (strcasecmp(tempData.name[i], "all")          !=0) )
        {
          /* illegal chamber name */
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
              "the handler does not provide a temperature chamber with name \"%s\"\n"
              "please check the configuration of \"%s\"",
              tempData.name[i], PHKEY_TC_CHAMB);
          retVal = PHFUNC_ERR_CONFIG;
        }
      }
      break;
	  case PHFUNC_MOD_FLEX:
	    for (i=0; i<tempData.chambers; i++)
	    {
		if (strcasecmp(tempData.name[i], "storage")      != 0 &&
		    strcasecmp(tempData.name[i], "st")           != 0 &&
		    strcasecmp(tempData.name[i], "testsite")     != 0 &&
		    strcasecmp(tempData.name[i], "ts")           != 0 &&
		    strcasecmp(tempData.name[i], "tcu")          != 0)
		{
		    /* illegal chamber name */
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			"the handler does not provide a temperature chamber with name \"%s\"\n"
			"please check the configuration of \"%s\"",
			tempData.name[i], PHKEY_TC_CHAMB);
		    retVal = PHFUNC_ERR_CONFIG;
		}
	    }	    
	    break;	
	  case PHFUNC_MOD_RFS:
	    for (i=0; i<tempData.chambers; i++)
	    {
		if (strcasecmp(tempData.name[i], "storage")      != 0 &&
		    strcasecmp(tempData.name[i], "st")           != 0 &&
		    strcasecmp(tempData.name[i], "testsite")     != 0 &&
		    strcasecmp(tempData.name[i], "ts")           != 0 &&
		    strcasecmp(tempData.name[i], "exit")         != 0 &&
		    strcasecmp(tempData.name[i], "ex")           != 0 &&
		    strcasecmp(tempData.name[i], "pickandplace") != 0 &&
		    strcasecmp(tempData.name[i], "pp")           != 0 &&
		    strcasecmp(tempData.name[i], "all")          != 0)
		{
		    /* illegal chamber name */
		    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			"the handler does not provide a temperature chamber with name \"%s\"\n"
			"please check the configuration of \"%s\"",
			tempData.name[i], PHKEY_TC_CHAMB);
		    retVal = PHFUNC_ERR_CONFIG;
		}
	    }	    
	    break;	
    default: break;
    }
    if (retVal != PHFUNC_ERR_OK)
	return retVal;

    if (handlerID->model == PHFUNC_MOD_SUMMIT)
    {
	confError = phConfConfString(handlerID->myConf, PHKEY_PL_SUMMITRECIPE,
	    0, NULL, &myRecipeFile);
	if (confError != PHCONF_ERR_OK || myRecipeFile == NULL || strlen(myRecipeFile) == 0)
	{
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
		"active temperature control on the Summit handler requires to set the name of recipe file");
	    return PHFUNC_ERR_CONFIG;
	}

	/* handler must be stopped to receive recipe, try to stop it */
	if (!handlerID->p.summitNoStopStart)
	{
	    PhFuncTaCheck(queryStatus(handlerID, 0));
	    if (!handlerID->p.stopped)
	    {
		PhFuncTaCheck(doPauseHandler(handlerID));
		if (handlerID->p.stopped)
		    needsRestart = 1;
	    }
	}

	PhFuncTaCheck(phFuncTaSend(handlerID, "recipestart %s?%s", 
	    myRecipeFile, handlerID->p.eol));
	localDelay_us(100000);
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\r\n]", recipestartOK));
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", recipestartOK);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	localDelay_us(100000);
    }

    /* chamber names seem to be OK, define the values now */
    for (i=0; i<tempData.chambers; i++)
    {
	if (tempData.control[i] == PHFUNC_TEMP_ON)
	{
	    /* setting the guardbands is handler specific */
	    switch(handlerID->model)
	    {
	      case PHFUNC_MOD_SUMMIT:
		if (tempData.lguard[i] >= 0.0)
		{
		    PhFuncTaCheck(phFuncTaSend(handlerID, "lowguard %g?%s", 
			tempData.lguard[i], handlerID->p.eol));
		    localDelay_us(100000);
//		    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\r\n]", lowguardOK));
            retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", lowguardOK);
            if (retVal != PHFUNC_ERR_OK) {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
		    localDelay_us(100000);
		}
		if (tempData.uguard[i] >= 0.0)
		{
		    PhFuncTaCheck(phFuncTaSend(handlerID, "highguard %g?%s", 
			tempData.uguard[i], handlerID->p.eol));
		    localDelay_us(100000);
//		    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\r\n]", highguardOK));
            retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", highguardOK);
            if (retVal != PHFUNC_ERR_OK) {
                phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
                return retVal;
            }
		    localDelay_us(100000);
		}
		PhFuncTaCheck(phFuncTaSend(handlerID, "setpoint %g?%s", 
		    tempData.setpoint[i], handlerID->p.eol));
		localDelay_us(100000);
//		PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\r\n]", setpointOK));
        retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", setpointOK);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
		localDelay_us(100000);
		break;
        case PHFUNC_MOD_MATRIX:
          PhFuncTaCheck(phFuncTaSend(handlerID, "setpoint %g%s", tempData.setpoint[i], handlerID->p.eol));
          if (tempData.lguard[i] >= 0.0)
          {
            PhFuncTaCheck(phFuncTaSend(handlerID, "%s lowerguardband %g%s", 
                  tempData.name[i], tempData.lguard[i], handlerID->p.eol));
          }
          if (tempData.uguard[i] >= 0.0)
          {
            PhFuncTaCheck(phFuncTaSend(handlerID, "%s upperguardband %g%s", 
                  tempData.name[i], tempData.uguard[i], handlerID->p.eol));
          }
          break;
	      case PHFUNC_MOD_CASTLE_LX:
		PhFuncTaCheck(phFuncTaSend(handlerID, "%s setpoint %g%s", 
		    tempData.name[i], tempData.setpoint[i], handlerID->p.eol));
		if (tempData.lguard[i] >= 0.0)
		{
		    PhFuncTaCheck(phFuncTaSend(handlerID, "%s lowerguardband %g%s", 
			tempData.name[i], tempData.lguard[i], handlerID->p.eol));
		}
		if (tempData.uguard[i] >= 0.0)
		{
		    PhFuncTaCheck(phFuncTaSend(handlerID, "%s upperguardband %g%s", 
			tempData.name[i], tempData.uguard[i], handlerID->p.eol));
		}
		break;
	      case PHFUNC_MOD_FLEX:
		PhFuncTaCheck(phFuncTaSend(handlerID, "%s setpoint %g%s", 
		    tempData.name[i], tempData.setpoint[i], handlerID->p.eol));
		/* warning: guardbands start at 2 degrees */
		if (tempData.lguard[i] >= 0.0)
		{
		    if (tempData.lguard[i] == 0.0)
		    {
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "not sending lowguard value to handler, value configured to 0");
		    }
		    else
		    {
			PhFuncTaCheck(phFuncTaSend(handlerID, "%s lowguard %g%s", 
			    tempData.name[i], tempData.lguard[i], handlerID->p.eol));
		    }
		}
		if (tempData.uguard[i] >= 0.0)
		{
		    if (tempData.uguard[i] == 0.0)
		    {
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "not sending highguard value to handler, value configured to 0");
		    }
		    else
		    {
			PhFuncTaCheck(phFuncTaSend(handlerID, "%s highguard %g%s", 
			    tempData.name[i], tempData.uguard[i], handlerID->p.eol));
		    }
		}
		break;
	      case PHFUNC_MOD_RFS:
		PhFuncTaCheck(phFuncTaSend(handlerID, "%s setpoint %g%s", 
		    tempData.name[i], tempData.setpoint[i], handlerID->p.eol));
		localDelay_us(100000);
		/* warning: guardbands start at 2 degrees */
		if (tempData.lguard[i] >= 0.0)
		{
		    if (tempData.lguard[i] == 0.0)
		    {
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "not sending lowerguardband value to handler, value configured to 0");
		    }
		    else
		    {
			PhFuncTaCheck(phFuncTaSend(handlerID, "%s lowerguardband %g%s", 
			    tempData.name[i], tempData.lguard[i], handlerID->p.eol));
			localDelay_us(100000);
		    }
		}
		if (tempData.uguard[i] >= 0.0)
		{
		    if (tempData.uguard[i] == 0.0)
		    {
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "not sending upperguardband value to handler, value configured to 0");
		    }
		    else
		    {
			PhFuncTaCheck(phFuncTaSend(handlerID, "%s upperguardband %g%s", 
			    tempData.name[i], tempData.uguard[i], handlerID->p.eol));
			localDelay_us(100000);
		    }
		}
		break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
              default: break;
/* End of Huatek Modification */
	    }

	    if (handlerID->model != PHFUNC_MOD_SUMMIT)
	    {
		/* switching heating and cooling on is similar for most handlers */
		if (tempData.heat[i] != PHFUNC_TEMP_UNDEF)
		{
		    PhFuncTaCheck(phFuncTaSend(handlerID, "%s heat %d%s", 
			tempData.name[i], tempData.heat[i] == PHFUNC_TEMP_ON ? 1 : 0, handlerID->p.eol));
		    if (handlerID->model == PHFUNC_MOD_RFS)
			localDelay_us(100000);
		}
		if (tempData.cool[i] != PHFUNC_TEMP_UNDEF)
		{
		    PhFuncTaCheck(phFuncTaSend(handlerID, "%s cool %d%s", 
			tempData.name[i], tempData.cool[i] == PHFUNC_TEMP_ON ? 1 : 0, handlerID->p.eol));
		    if (handlerID->model == PHFUNC_MOD_RFS)
			localDelay_us(100000);
		}
	    }
	}
    }
    
    /* define soak time, not done on a per chamber mode */
    if (tempData.chambers >= 1)
    {
	phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
	    "soaktime and desoaktime can not be defined on a per chamber mode for this handler,\n"
	    "will use configured value as defined for chamber \"%s\" (first list entry)",
	    tempData.name[0]);
	switch(handlerID->model)
	{
	  case PHFUNC_MOD_SUMMIT:
	    if (tempData.soaktime[0] >= 0)
	    {
		PhFuncTaCheck(phFuncTaSend(handlerID, "soaktime %g?%s", 
		    tempData.soaktime[0], handlerID->p.eol));
		localDelay_us(100000);
//		PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\r\n]", soaktimeOK));
        retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", soaktimeOK);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
		localDelay_us(100000);
	    }
	    break;
	  case PHFUNC_MOD_CASTLE_LX:
	    if (tempData.soaktime[0] >= 0)
	    {
		PhFuncTaCheck(phFuncTaSend(handlerID, "soaktime %g%s", 
		    tempData.soaktime[0], handlerID->p.eol));
	    }
	    if (tempData.desoaktime[0] >= 0)
	    {
		PhFuncTaCheck(phFuncTaSend(handlerID, "desoaktime %g%s", 
		    tempData.desoaktime[0], handlerID->p.eol));
	    }
	    break;
      case PHFUNC_MOD_MATRIX:
        if (tempData.soaktime[0] >= 0)
        {
          PhFuncTaCheck(phFuncTaSend(handlerID, "soaktime %g%s", 
                tempData.soaktime[0], handlerID->p.eol));
        }
        break;
	  case PHFUNC_MOD_FLEX:
	  case PHFUNC_MOD_RFS:
	    /* desoaktime can not be defined */
	    if (tempData.desoaktime[0] >= 0)
	    {
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		    "desoaktime can not be defined for this handler, ignored");
	    }
	    if (tempData.soaktime[0] >= 0)
	    {
		PhFuncTaCheck(phFuncTaSend(handlerID, "soaktime %g%s", 
		    tempData.soaktime[0], handlerID->p.eol));
	    }
	    break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
          default: break;
/* End of Huatek Modification */
	}
    }
    
    /* finally activate the temperature control */
    
    if (handlerID->model == PHFUNC_MOD_SUMMIT)
    {
	PhFuncTaCheck(phFuncTaSend(handlerID, "recipeend %s?%s", 
	    myRecipeFile, handlerID->p.eol));
	localDelay_us(100000);
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\r\n]", recipeendOK));
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", recipeendOK);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	localDelay_us(100000);
	PhFuncTaCheck(phFuncTaSend(handlerID, "recipe activategrp %s%s", 
	    myRecipeFile, handlerID->p.eol));
	/* 1 sec here to activate the recipe */
	localDelay_us(1000000);

	/* restart handler, if we stopped it */
	if (!handlerID->p.summitNoStopStart && needsRestart)
	{
	    PhFuncTaCheck(doUnpauseHandler(handlerID));
	}
    }

    switch(handlerID->model)
    {
      case PHFUNC_MOD_SUMMIT:
    case PHFUNC_MOD_MATRIX:
	PhFuncTaCheck(phFuncTaSend(handlerID, "tempcontrol 1%s", handlerID->p.eol));
	break;
      case PHFUNC_MOD_CASTLE_LX:
	PhFuncTaCheck(phFuncTaSend(handlerID, "testtemp 1%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "tempcontrol 1%s", handlerID->p.eol));
	break;
      case PHFUNC_MOD_FLEX:
	PhFuncTaCheck(phFuncTaSend(handlerID, "testtemp atsetpoint%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "tempcontrol enable%s", handlerID->p.eol));
	break;
      case PHFUNC_MOD_RFS:
	/* 1 sec delays here, hope that RFS accepts the parameters */
	localDelay_us(1000000);
	PhFuncTaCheck(phFuncTaSend(handlerID, "testtemp atsetpoint%s", handlerID->p.eol));
	localDelay_us(1000000);
	PhFuncTaCheck(phFuncTaSend(handlerID, "tempcontrol enable%s", handlerID->p.eol));
	break;
    default: break;
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
	PHKEY_BI_VERIFY, "yes", "no", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
    {
        retVal = PHFUNC_ERR_CONFIG;
    }
    else if (PHFUNC_MOD_ECLIPSE == handlerID->model)
    {
		phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
		    "eclipse will always activate bin verification mode");
		handlerID->p.verifyBins = 1;
    }
    else
    {
        switch (found)
        {
            case 1:
                /* configured "yes" */
                switch(handlerID->model)
                {
                    case PHFUNC_MOD_SUMMIT:
                        if (handlerID->p.summitNoVerify)
                        {
                            /* not available at Summit */
                            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                    "bin verification mode not possible for Summit,\n"
                                    "configuration of \"%s\", ignored", PHKEY_BI_VERIFY);
                            break;
                        }
                        /* else fall through to common section ... */
                    case PHFUNC_MOD_CASTLE_LX:
                    case PHFUNC_MOD_MATRIX:
                    case PHFUNC_MOD_FLEX:
                    case PHFUNC_MOD_RFS:
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                "will activate bin verification mode");
                        PhFuncTaCheck(phFuncTaSend(handlerID, "verifytest 1%s", handlerID->p.eol));
                        handlerID->p.verifyBins = 1;
                        /* use new testresults mode on RFS */
                        handlerID->p.rfsUseTestresults = 1;
                        break;
                    default: break;
                }
                break;
            case 2:
                /* configured "no" */
                switch(handlerID->model)
                {
                    case PHFUNC_MOD_SUMMIT:
                        if (handlerID->p.summitNoVerify)
                        {
                            /* not available at Summit */
                            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                    "bin verification mode not possible for Summit,\n"
                                    "configuration of \"%s\", ignored", PHKEY_BI_VERIFY);
                            break;
                        }
                        /* else fall through to common section ... */
                    case PHFUNC_MOD_CASTLE_LX:
                    case PHFUNC_MOD_MATRIX:
                    case PHFUNC_MOD_FLEX:
                    case PHFUNC_MOD_RFS:
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                "will deactivate bin verification mode");
                        PhFuncTaCheck(phFuncTaSend(handlerID, "verifytest 0%s", handlerID->p.eol));
                        handlerID->p.verifyBins = 0;
                        /* use new testresults mode on RFS */
                        handlerID->p.rfsUseTestresults = 1;
                        break;
                        /* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
                        /* Issue Number: 326 */
                    default: break;
                             /* End of Huatek Modification */
                }
                break;
            default:
                /* not configured, leave it like the handler is already set up */

                /* fall back to old testresults mode on RFS */
                handlerID->p.rfsUseTestresults = 0;

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
        handlerID->p.verifyMaxCount = 1; /* default one single retry */
    }

    return retVal;
}

/* check for active workarounds */
static phFuncError_t reconfigureWorkarounds(phFuncId_t handlerID)
{
    phConfError_t confError;
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phConfType_t confType = PHCONF_TYPE_LIST;
    int length = 0;
    int i;
    const char *workaround = NULL;

    /* check for bug compatibility modes */
    handlerID->p.summitReprobeBug = 0;
    handlerID->p.summitNoStopStart = 0;
    handlerID->p.summitNoVerify = 0;

    confError = phConfConfIfDef(handlerID->myConf, PHKEY_PL_SUMMITWA);
    if (confError == PHCONF_ERR_OK)
    {
	confError = phConfConfType(handlerID->myConf, PHKEY_PL_SUMMITWA,
	    0, NULL, &confType, &length);
	if (confError != PHCONF_ERR_OK || confType != PHCONF_TYPE_LIST)
	{
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
		"configuration parameter \"%s\" must be a list of strings",
		PHKEY_PL_SUMMITWA);
	    retVal = PHFUNC_ERR_CONFIG;
	}
	else
	{
	    for (i=0; i<length; i++)
	    {
		confError = phConfConfString(handlerID->myConf, PHKEY_PL_SUMMITWA, 
		    1, &i, &workaround);
		if (confError != PHCONF_ERR_OK)
		{
		    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
			"configuration parameter \"%s\" must be a list of strings",
			PHKEY_PL_SUMMITWA);
		    retVal = PHFUNC_ERR_CONFIG;
		}
		else
		{
		    if (strcasecmp(workaround, "no-multi-site-reprobe") == 0)
		    {
			handlerID->p.summitReprobeBug = 1;
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "will work around Summit's multi site reprobe bug");
		    }
		    else if (strcasecmp(workaround, "no-stop-start") == 0)
		    {
			handlerID->p.summitNoStopStart = 1;
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "will never send \"start\" or \"stop\" commands to Summit handler");
		    }
		    else if (strcasecmp(workaround, "no-verify") == 0)
		    {
			handlerID->p.summitNoVerify = 1;
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "will not activate bin verification, even if configured with \"%s\"", 
			    PHKEY_BI_VERIFY);
			
		    }
		}
	    }
	}
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
    PhFuncTaCheck(phFuncTaSend(handlerID, "identify?%s", handlerID->p.eol));
    localDelay_us(100000);
//    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", idString));
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", idString);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
    localDelay_us(100000);

    /* return result */
    return retVal;
}

static phFuncError_t getSoftwareID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **softwareIdString  /* resulting pointer to equipment ID string */
)
{
    static char whichString[512] = "";
    whichString[0] = '\0';

    phFuncError_t retVal = PHFUNC_ERR_OK;

    *softwareIdString = whichString;

    PhFuncTaCheck(phFuncTaSend(handlerID, "which?%s", handlerID->p.eol));
    localDelay_us(100000);
//    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", whichString));
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", whichString);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
    localDelay_us(100000);

    /* return result */
    return retVal;
}

static phFuncError_t verifyModel(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *idString            /* resulting pointer to equipment ID string */,
    char *whichString         /* resulting pointer to equipment ID string */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    static char gotName[512] = "";
    static char gotVersion[512] = "";
    char *verPtr = NULL;

    /* try to verify the handler model and handler SW revision */
    if (strlen(idString) > 0)
    {
	switch(handlerID->model)
	{
	  case PHFUNC_MOD_CASTLE_LX:
	    /* id string look like "Castle Handler 06 Ver 2.05f Logic\n" */
	    /* try to match the word "Castle" and find the version number,
	       if possible */
	    if (sscanf(idString, "%s %*s %*s %*s %s %*s", gotName, gotVersion) == 2)
	    {
		if (strcasecmp(gotName, "Castle") == 0)
		{
		    /* take care of the fact that no letter is in
		       the end of the version number */
		    strcat(gotVersion, " ");

		    if (sscanf(gotVersion, "%d.%d%c", 
			&handlerID->p.version1, 
			&handlerID->p.version2, 
			&handlerID->p.versionchar) == 3)
		    {
			handlerID->p.identified = 1;
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "Identified Castle handler with SW revision %d %d '%c'",
			    handlerID->p.version1, handlerID->p.version2, handlerID->p.versionchar);
		    }
		}
	    }
	    if (!handlerID->p.identified)
	    {
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		    "Could not identify Castle handler from \"identify?\" query.\n"
		    "Expected format similar to \"Castle Handler 06 Ver 2.05f Logic\\n\"\n"
		    "Received format            \"%s\"\n"
		    "Will assume to run on Castle handler anyway",
		    idString);
	    }
	    break;
      case PHFUNC_MOD_MATRIX:
        /* id string look like "Matrix Handler 06 1.2.1.0\n" */
        /* try to match the word "Matrix" and find the version number, if possible */
        if (sscanf(idString, "%s %*s %*s %s", gotName, gotVersion) == 2)
        {
          if (strcasecmp(gotName, "Matrix") == 0)
          {
            /* take care of the fact that no letter is in
               the end of the version number */
            strcat(gotVersion, " ");

            if (sscanf(gotVersion, "%d.%d.%d.%d", 
                  &handlerID->p.version1, 
                  &handlerID->p.version2, 
                  &handlerID->p.version3, 
                  &handlerID->p.version4) == 4)
            {
              handlerID->p.identified = 1;
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                  "Identified Matrix handler with SW revision %d %d %d %d",
                  handlerID->p.version1, handlerID->p.version2, handlerID->p.version3, handlerID->p.version4);
            }
          }
        }
        if (!handlerID->p.identified)
        {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
              "Could not identify Matrix handler from \"identify?\" query.\n"
              "Expected format similar to \"Matrix Handler 06 1.2.1.0\\n\"\n"
              "Received format            \"%s\"\n"
              "Will assume to run on Matrix handler anyway",
              idString);
        }
        break;
	  case PHFUNC_MOD_SUMMIT:
	    /* id string looks like "Summit 1.7.3\n" */
	    /* try to match the word "Summit" and find the version number,
	       if possible */
	    if (sscanf(idString, "%s %s", gotName, gotVersion) == 2)
	    {
		if (strcasecmp(gotName, "Summit") == 0)
		{
		    if (sscanf(gotVersion, "%d.%d.%d", 
			&handlerID->p.version1, 
			&handlerID->p.version2, 
			&handlerID->p.version3) == 3)
		    {
			handlerID->p.identified = 1;
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "Identified Summit handler with SW revision %d %d %d",
			    handlerID->p.version1, handlerID->p.version2, handlerID->p.version3);
		    }
		}
	    }
	    if (!handlerID->p.identified)
	    {
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		    "Could not identify Summit handler from \"identify?\" query.\n"
		    "Expected format similar to \"Summit 1.7.3\\n\"\n"
		    "Received format            \"%s\"\n"
		    "Will assume to run on Summit handler anyway",
		    idString);
	    }
	    break;
	  case PHFUNC_MOD_FLEX:
	    /* id string look like "Delta Design Flex Handler 6 Wed Dec 01 1999 14:31\n" */
	    /* try to match the word "Flex" and find the version number in the which? reply,
	       if possible */
	    if (sscanf(idString, "%*s %*s %s", gotName) == 1)
	    {
		if (strcasecmp(gotName, "Flex") == 0)
		{
		    /* the which string looks like "2904 15.0H 15.0H",
		     <handlerID> <master_revision>
		     <slave_revision>. The handler ID may contain
		     spaces. We check the slave revision. */
		    verPtr = strrchr(whichString, ' ');
		    if (verPtr)
		    {
			/* take care of the fact that no letter is in
                           the end of the version number */
			strcat(verPtr, " ");

			verPtr++;
			if (sscanf(verPtr, "%d.%d%c", 
			    &handlerID->p.version1, 
			    &handlerID->p.version2, 
			    &handlerID->p.versionchar) == 3)
			{
			    handlerID->p.identified = 1;
			    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
				"Identified Flex handler with SW revision %d %d '%c'",
				handlerID->p.version1, handlerID->p.version2, handlerID->p.versionchar);
			}
		    }
		}
	    }
	    if (!handlerID->p.identified)
	    {
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		    "Could not identify Flex handler from \"identify?\" and \"which?\" queries.\n"
		    "Expected format similar to \"Delta Design Flex Handler 6 Wed Dec 01 1999 14:31\\n\" and\n"
		    "                           \"<some string> 15.0H 15.0H\\n\" and\n"
		    "Received format            \"%s\" and\n"
		    "                           \"%s\" and\n"
		    "Will assume to run on Flex handler anyway",
		    idString, whichString);
	    }
	    break;
	  case PHFUNC_MOD_RFS:
	    /* id string look like "TRFS 6 V3.0g\n" */
	    /* try to match the word "TRFS" and find the version number,
	       if possible */
	    if (sscanf(idString, "%s %*s %s", gotName, gotVersion) == 2)
	    {
		if (strcasecmp(gotName, "TRFS") == 0)
		{
		    if (sscanf(gotVersion, "V%d.%d%c", 
			&handlerID->p.version1, 
			&handlerID->p.version2, 
			&handlerID->p.versionchar) == 3)
		    {
			handlerID->p.identified = 1;
			phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
			    "Identified RFS handler with SW revision %d %d %d",
			    handlerID->p.version1, handlerID->p.version2, handlerID->p.version3);
		    }
		}
	    }
	    if (!handlerID->p.identified)
	    {
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		    "Could not identify RFS handler from \"identify?\" query.\n"
		    "Expected format similar to \"TRFS 6 V3.0g\\n\"\n"
		    "Received format            \"%s\"\n"
		    "Will assume to run on RFS handler anyway",
		    idString);
	    }
	    break;
	  case PHFUNC_MOD_ORION:
	     if (sscanf(idString, "%s", gotName) == 1)
	     {
	         if (strcasecmp(gotName, "Orion") == 0)
	         {
	             handlerID->p.identified = 1;
	             phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
	                 "Identified Orion handler.");
	         }
	         if (!handlerID->p.identified)
	         {
	             phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
	                              "Could not identify Orion handler from \"identify?\" query. \n"
	                              "Expected format similar to \"Orion\"\n"
	                              "Received format    \"%s\"\n"
	                              "Will continue running the Orion handler anyway.",
	                              idString);
	         }
	     }
	  break;
      case PHFUNC_MOD_ECLIPSE:
      if (sscanf(idString, "%s", gotName) == 1)
      {
          if (strcasecmp(gotName, "Eclipse") == 0)
          {
              handlerID->p.identified = 1;
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                      "Identified Eclipse handler.");
          }
          if (!handlerID->p.identified)
          {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                      "Could not identify Eclipse handler from \"identify?\" query. \n"
                      "Expected format similar to \"Eclipse\"\n"
                      "Received format    \"%s\"\n"
                      "Will continue running the Eclipse handler anyway.",
                      idString);
          }
      }
      break;
      default: break;
	}
    }

    return retVal;
}


static phFuncError_t handlerSetup(
    phFuncId_t handlerID    /* driver plugin ID */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    static long disabledSites = 0L;
    static long sitespacing = 0L;
    static int siteDisabled[PHESTATE_MAX_SITES];
    int i;
    char handlerStripmap[64]; /* Hardcoded 64 - should be large enough for stripmap? answer.*/
    char handlerIndexPosition[32]; /* Hardcoded 32 - should be large enough for index? answer.*/
    char handlerMaterialID[64]; /* Hardcoded 64 - should be large enough for materialid? answer.*/
    static char handlerAnswer[512] = "";

/* model specific part */
    switch(handlerID->model)
    {
    case PHFUNC_MOD_CASTLE_LX:
    case PHFUNC_MOD_MATRIX:
	PhFuncTaCheck(phFuncTaSend(handlerID, "numtestsites?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.sites));
    retVal = phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.sites);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	PhFuncTaCheck(phFuncTaSend(handlerID, "set sitedisable?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%ld", &disabledSites));
    retVal = phFuncTaReceive(handlerID, 1, "%ld", &disabledSites);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	for (i=0; i<PHESTATE_MAX_SITES; i++)
	{
	    if (i<handlerID->p.sites && i<32)  /* Why 32 here? */
		handlerID->p.siteUsed[i] = (disabledSites & (1L << i)) ? 0 : 1;
	    else
		handlerID->p.siteUsed[i] = 0;
	}
	PhFuncTaCheck(phFuncTaSend(handlerID, "verifytest?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.verifyBins));
    retVal = phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.verifyBins);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	PhFuncTaCheck(phFuncTaSend(handlerID, "cmdreply 0%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "systemmode 1%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "testermode 1%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "emulationmode %d%s", handlerID->p.emulationMode, handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "errorclear%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "srqmask %s%s", handlerID->p.srqMask, handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "start%s", handlerID->p.eol));
	break;
      case PHFUNC_MOD_SUMMIT:
	PhFuncTaCheck(phFuncTaSend(handlerID, "numtestsites?%s", handlerID->p.eol));
	localDelay_us(100000);
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.sites));
    retVal = phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.sites);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	localDelay_us(100000);
	PhFuncTaCheck(phFuncTaSend(handlerID, "systemmode 1%s", handlerID->p.eol));
	localDelay_us(100000);

	if (!handlerID->p.summitNoVerify)
	{
	    PhFuncTaCheck(phFuncTaSend(handlerID, "verifytest?%s", handlerID->p.eol));
	    localDelay_us(100000);
//	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.verifyBins));
        retVal = phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.verifyBins);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
	    localDelay_us(100000);
	}

#if 0 
	/* don't remove this code !!!!!!
           functionality not yet available on Summit,
	   but may be in future Summit SW revisions. As soon as we know the
	   revision ID from Delta, we may activate this code */

	PhFuncTaCheck(phFuncTaSend(handlerID, "testsitedisable?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%ld", &disabledSites));
    retVal = phFuncTaReceive(handlerID, 1, "%ld", &disabledSites);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	for (i=0; i<PHESTATE_MAX_SITES; i++)
	{
	    if (i<handlerID->p.sites && i<32)
		handlerID->p.siteUsed[i] = (disabledSites & (1L << i)) ? 0 : 1;
	    else
		handlerID->p.siteUsed[i] = 0;
		} 

	PhFuncTaCheck(phFuncTaSend(handlerID, "cmdreply 0%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "testermode 1%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "emulationmode 2%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "errorclear%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "srqmask %s%s", handlerID->p.srqMask, handlerID->p.eol));
#endif

	if (!handlerID->p.summitNoStopStart)
	{
	    PhFuncTaCheck(phFuncTaSend(handlerID, "start%s", handlerID->p.eol));
	}

	break;
      case PHFUNC_MOD_FLEX:
	if (handlerID->noOfSites <= 4)
	{
	    PhFuncTaCheck(phFuncTaSend(handlerID, "sitespacing?%s", handlerID->p.eol));
//	    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%ld", &sitespacing));
        retVal = phFuncTaReceive(handlerID, 1, "%ld", &sitespacing);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
	    switch (sitespacing)
	    {
	      case 1:
		handlerID->p.sites = 1;
		break;
	      case 2:
	      case 3:
		handlerID->p.sites = 2;
		break;
	      case 4:
		handlerID->p.sites = 4;
		break;
	      default:
		/* answer from sitespacing not understood */
		handlerID->p.sites = sitespacing > 0 && sitespacing <= 8 ? sitespacing : 0;
		break;
	    }
	}
	else
	{
	    /* there is no query to find out about more than 4 sites */
	    handlerID->p.sites = handlerID->noOfSites;
	}



        /* CR79544: get disabled site with command "mask sitedisable?" */
        PhFuncTaCheck(phFuncTaSend(handlerID, "mask sitedisable?%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%ld", &disabledSites);
        if (retVal != PHFUNC_ERR_OK) 
        {
          phFuncTaRemoveStep(handlerID);
          return retVal;
        }
        for (i=0; i<PHESTATE_MAX_SITES; i++)
        {
          if (i<handlerID->p.sites && i<32)  
          {
            handlerID->p.siteUsed[i] = (disabledSites & (1L << i)) ? 0 : 1;
          }
          else
          {
            handlerID->p.siteUsed[i] = 0;
          }
        }

	PhFuncTaCheck(phFuncTaSend(handlerID, "verifytest?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.verifyBins));
    retVal = phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.verifyBins);
    if (retVal != PHFUNC_ERR_OK) {
       phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	PhFuncTaCheck(phFuncTaSend(handlerID, "categorysource tester%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "errorclear!%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "srqmask %s%s", handlerID->p.srqMask, handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "start!%s", handlerID->p.eol));
	break;
      case PHFUNC_MOD_RFS:
	PhFuncTaCheck(phFuncTaSend(handlerID, "conehdnum?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.sites));
    retVal = phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.sites);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	PhFuncTaCheck(phFuncTaSend(handlerID, "systemmode 1%s", handlerID->p.eol));
	PhFuncTaCheck(phFuncTaSend(handlerID, "testermode 1%s", handlerID->p.eol));

#if 0 
	/* don't remove this code !!!!!!
           functionality not yet available on Summit,
	   but may be in future RFS SW revisions. As soon as we know the
	   revision ID from Delta, we may activate this code */

	PhFuncTaCheck(phFuncTaSend(handlerID, "verifytest?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.verifyBins));
    retVal = phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.verifyBins);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
#endif

	PhFuncTaCheck(phFuncTaSend(handlerID, "srqmask %s%s", handlerID->p.srqMask, handlerID->p.eol));

	/* do not try to start the handler remotely, it will not accept it */
	/* PhFuncTaCheck(phFuncTaSend(handlerID, "start%s", handlerID->p.eol)); */
	break;
      case PHFUNC_MOD_ORION:
	PhFuncTaCheck(phFuncTaSend(handlerID, "stripmap?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerStripmap));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerStripmap);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }
	/*The stripmap query is also callable from App Model via "ph_get_id equipment"*/
        
	PhFuncTaCheck(phFuncTaSend(handlerID, "index?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerIndexPosition));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerIndexPosition);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }

	/*phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "index answer: \"%s\" (TODO - remove this)", handlerIndexPosition);
	   TODO - make this query answer available from App Model ph_get_id call. */

	PhFuncTaCheck(phFuncTaSend(handlerID, "materialid?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerMaterialID));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerMaterialID);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }

	/*phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "materialid answer: \"%s\" (TODO - remove this)", handlerMaterialID);
	   TODO - make this query answer available from App Model ph_get_id call. */

	PhFuncTaCheck(phFuncTaSend(handlerID, "numtestsites?%s", handlerID->p.eol));
//	PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.sites));
    retVal = phFuncTaReceive(handlerID, 1, "%d", &handlerID->p.sites);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }

	break;
      case PHFUNC_MOD_ECLIPSE:
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

         /* set lot quantity */
         /* TODO */

         break;

/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
    }

    /* ask for status */
    PhFuncTaCheck(queryStatus(handlerID, 1));
    
    /* flush any pending SRQs (will poll later, if we are missing some
       test start SRQ here */
    PhFuncTaCheck(flushSRQs(handlerID));

    /* end startup transactions */
    phFuncTaStop(handlerID);

    /* change equipment state */
    if (handlerID->p.stopped)
	phEstateASetPauseInfo(handlerID->myEstate, 1);
    else
	phEstateASetPauseInfo(handlerID->myEstate, 0);

    /* verify setup */
    if (handlerID->p.trusted && handlerID->p.sites != handlerID->noOfSites)
    {
	phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
	    "the number of sites (%d) reported by the handler\n"
	    "does not match the number of sites (%d) given in the driver configuration.\n"
	    "Driver operation will continue, trusting the configuration entry",
	    handlerID->p.sites, handlerID->noOfSites);
	handlerID->p.trusted = 0;
    }
    for (i=0; handlerID->p.trusted && i<handlerID->noOfSites; i++)
    {
	if ((handlerID->activeSites[i] && !handlerID->p.siteUsed[i]) ||
	    (!handlerID->activeSites[i] && handlerID->p.siteUsed[i]))
	{
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		"the site activation reported by the handler\n"
		"does not match the given driver configuration for site \"%s\".\n"
		"The handler reports \"%s\" while the configuration says \"%s\".\n"
	"Driver operation will continue, trusting the configuration entry",
		handlerID->siteIds[i],
		handlerID->p.siteUsed[i] ? "active" : "disabled",
		handlerID->activeSites[i] ? "active" : "disabled");
	}
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

    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", frQuery);

    if (retVal != PHFUNC_ERR_OK)
    {
        phFuncTaRemoveStep(handlerID); 
        return retVal;
    }

    localDelay_us(100000);

    return retVal;
}


/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int found = 0;
    phConfError_t confError;
    double dPollingInterval;
    char *machineID;
    char *softwareID;
    static int firstTime=1;
    const char* srqMask = NULL;
    long srqMaskLong = 0;
    const char *binningCommandFormat = NULL;
    char *handlerRunning = NULL;
    double dEmulationMode=0;

   /* retrieve the delta_binnng_command_format */
    handlerID->p.binningCommandFormat = 16; /* default 16-bin format*/
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_PL_DELTA_BINNING_COMMAND_FORMAT);
    if (confError == PHCONF_ERR_OK)
    {
        confError = phConfConfString(handlerID->myConf, PHKEY_PL_DELTA_BINNING_COMMAND_FORMAT,
                0, NULL, &binningCommandFormat);
        if (confError == PHCONF_ERR_OK)
        {
            if(strcmp(binningCommandFormat, "16-bin") == 0)
            {
                handlerID->p.binningCommandFormat = 16;
            }
            else if(strcmp(binningCommandFormat, "256-bin") == 0)
            {
                handlerID->p.binningCommandFormat = 256;
            }
        }
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "delta binning-Command-Format is %d-bin", handlerID->p.binningCommandFormat);
    }

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
    
    /*retrieve emulation mode*/
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_PL_DELTA_EMULATION_MODE);
    if (confError == PHCONF_ERR_OK)
    {   confError = phConfConfNumber(handlerID->myConf, PHKEY_PL_DELTA_EMULATION_MODE,
                0, NULL, &dEmulationMode);
        if (confError == PHCONF_ERR_OK)
            handlerID->p.emulationMode=dEmulationMode;
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

    /* do we want to pause the handler ? */
    handlerID->p.pauseHandler = 0;

    confError = phConfConfStrTest(&found, handlerID->myConf,
	PHKEY_OP_PAUPROB, "yes", "no", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
	retVal = PHFUNC_ERR_CONFIG;
    else
    {
	if (found == 1)
	{
	    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
		"will pause handler on SmarTest pause");
	    handlerID->p.pauseHandler = 1;
	}
    }

    /* retrieve srqmask */
    /* it set hexadecimal in configuration but Dalta handler only support decimal format */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_SU_SRQMASK);
    if (confError == PHCONF_ERR_OK)
        confError = phConfConfString(handlerID->myConf, PHKEY_SU_SRQMASK,
                                     0, NULL, &srqMask);
    if (confError == PHCONF_ERR_OK)
    {
      //covert the srq mask from hexidecimal to decimal
      sscanf(srqMask, "%lx", &srqMaskLong);
    }
    else
    {
      //covert the default srq mask to decimal
       sscanf(DEFAULT_SRQMASK, "%ld", &srqMaskLong);
    }
    snprintf(handlerID->p.srqMask, 4, "%ld", srqMaskLong);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Set SRQMASK to %s", handlerID->p.srqMask);

    if (!(srqMaskLong & 0x06))
    {
        /* if the lot start/retest lot start bit in status byte is disabled, */
        /* assume the lot is started */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Lot start bits are masked, so assume lot is already started");
        handlerID->p.lotStarted = 1;
    }

    /* perform the communication intesive parts */
    PhFuncTaCheck(reconfigureWorkarounds(handlerID));
    if ( firstTime )
    {
        PhFuncTaCheck(getMachineID(handlerID, &machineID));
        switch (handlerID->model)
        {
        case PHFUNC_MOD_CASTLE_LX:
        case PHFUNC_MOD_MATRIX:
        case PHFUNC_MOD_SUMMIT:
        case PHFUNC_MOD_FLEX:
        case PHFUNC_MOD_RFS:
            PhFuncTaCheck(getSoftwareID(handlerID, &softwareID));
            break;
        case PHFUNC_MOD_ORION:
            break;  /* Orion does not yet support getSoftwareID() (which? query).*/
        case PHFUNC_MOD_ECLIPSE:
            PhFuncTaCheck(checkHandlerRunning(handlerID, &handlerRunning));
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "FR? handler running returns \"%s\" ", handlerRunning);
            break;  /* Eclipse does not yet support getSoftwareID() (which? query).*/
        default:
            break;
        }
        PhFuncTaCheck(verifyModel(handlerID, machineID, softwareID));
        PhFuncTaCheck(handlerSetup(handlerID));
        firstTime=0;
    }
    if (PHFUNC_MOD_ECLIPSE == handlerID->model)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "eclipse does not yet support temperature configuration");
    }
    else
    {
        PhFuncTaCheck(reconfigureTemperature(handlerID));
    }
    PhFuncTaCheck(reconfigureVerification(handlerID));
    return retVal;
}


/* perform real pause the handler operation */
static phFuncError_t doPauseHandler(phFuncId_t handlerID)
{
    /* check whether the handler is already paused too */
    phFuncTaMarkStep(handlerID);
    PhFuncTaCheck(queryStatus(handlerID, 0));

    if (!handlerID->p.stopped)
    {
	/* SmarTest is paused, but the handler is not yet paused */
	phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
	    "sending stop command to handler");
	switch(handlerID->model)
	{
	  case PHFUNC_MOD_SUMMIT:
	    if (handlerID->p.summitNoStopStart)
	    {
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		    "remote stopping is not (yet) supported by Summit handler");
		return PHFUNC_ERR_OK;
	    }
	    else
	    {
		PhFuncTaCheck(phFuncTaSend(handlerID, "stop%s", handlerID->p.eol));
		break;
	    }
	  case PHFUNC_MOD_CASTLE_LX:
      case PHFUNC_MOD_MATRIX:
	    PhFuncTaCheck(phFuncTaSend(handlerID, "stop%s", handlerID->p.eol));
	    break;
	  case PHFUNC_MOD_RFS:
	    PhFuncTaCheck(phFuncTaSend(handlerID, "stop%s", handlerID->p.eol));
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		"handler was stopped by the driver and will need a manual restart later");
	    break;
	  case PHFUNC_MOD_FLEX:
	    PhFuncTaCheck(phFuncTaSend(handlerID, "stop!%s", handlerID->p.eol));
	    break;
	  case PHFUNC_MOD_ORION:
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
		"Orion does not support pause over GPIB.\n"
		"       Update config file parameter \"stop_handler_on_smartest_pause\" to \"no\".\n"
		"       No need to pause handler.");
        break;
	  case PHFUNC_MOD_ECLIPSE:
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, 
		"Eclipse does not support pause over GPIB.\n"
		"       Update config file parameter \"stop_handler_on_smartest_pause\" to \"no\".\n"
		"       No need to pause handler.");
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
          default: break;
/* End of Huatek Modification */
	}

	/* give the handler some time to accept the stop */
	sleep(1);

	/* query status again */
	PhFuncTaCheck(queryStatus(handlerID, 1));
	if (handlerID->p.stopped)
	    handlerID->p.pauseInitiatedByST = 1;
    }

    /* handler should now be stopped, otherwise return with
       WAITING and try again */
    if (!handlerID->p.stopped)
    {
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
    PhFuncTaCheck(queryStatus(handlerID, 0));

    if (handlerID->p.stopped)
    {
	if (handlerID->p.pauseInitiatedByST)
	{
	    /* SmarTest paused the handler and should unpause it now */
	    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
		"sending start command to handler");
	    switch(handlerID->model)
	    {
	      case PHFUNC_MOD_SUMMIT:
		if (handlerID->p.summitNoStopStart)
		{
		    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
			"remote starting is not (yet) supported by Summit handler");
		    return PHFUNC_ERR_OK;
		}
		else
		{
		    PhFuncTaCheck(phFuncTaSend(handlerID, "start%s", handlerID->p.eol));
		    break;
		}
	      case PHFUNC_MOD_RFS:
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		    "handler needs to be started manually now");
		sleep((unsigned int) (handlerID->heartbeatTimeout/1000000L) + 1);
		break;
	      case PHFUNC_MOD_CASTLE_LX:
        case PHFUNC_MOD_MATRIX:
		PhFuncTaCheck(phFuncTaSend(handlerID, "start%s", handlerID->p.eol));
		break;
	      case PHFUNC_MOD_FLEX:
		PhFuncTaCheck(phFuncTaSend(handlerID, "start!%s", handlerID->p.eol));
		break;
	      case PHFUNC_MOD_ORION:
          case PHFUNC_MOD_ECLIPSE:
		/* No action required for Orion - drop down to default.*/
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
  	      default: break;
/* End of Huatek Modification */
	    }

	    /* give the handler some time to accept the start */
	    sleep(1);

	    /* query status again */
	    PhFuncTaCheck(queryStatus(handlerID, 1));
	    if (!handlerID->p.stopped)
		handlerID->p.pauseInitiatedByST = 0;
	}
	else
	{
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
		"handler needs to be started manually now");
	    sleep((unsigned int) (handlerID->heartbeatTimeout/1000000L) + 1);
	}
    }

    /* handler should now be back up running, otherwise return
       with WAITING and try again */
    if (handlerID->p.stopped)
    {
	phFuncTaRemoveToMark(handlerID);	
	return PHFUNC_ERR_WAITING;
    }

    return PHFUNC_ERR_OK;
}


/*****************************************************************************
 *
 * for a certain GetStatus query, get the corresponding GPIB command
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *    all the GPIB commands are stored locally for each Delta model, this
 *    function will retrieve the corresponding command with regard to a
 *    certain query. For example, 
 *    (1)for the query "PROB_HND_CALL(ph_get_status setpoint testsite)" for Flex
 *    mode, this function will return the GPIB command:
 *        "testsite setpoint?"
 *    (2)for the query "PROB_HND_CALL(ph_get_status setpoint zone1)" for Castle
 *    mode, this function will return the GPIB command:
 *        "zone1 setpoint?"
 *    (3)for the query "PROB_HND_CALL(ph_get_status soaktime)", the return GPIB
 *    command will be:
 *        "soaktime?"
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
  /* these static array must be ordered by the first field */
  static const  phStringPair_t sFlexGpibCommands[] = {
    {PHKEY_NAME_HANDLER_STATUS_GET_SITE_MAPPING,                "sm?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_AIRTEMP,         "airtemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_COOL,            "cool?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_DESOAKTIME,      "desoaktime?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_HEAT,            "heat?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_LOWERGUARDBAND,  "lowguard?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_MASSTEMP,        "masstemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT,        "setpoint?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SOAKTIME,        "soaktime?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPCONTROL,     "tempcontrol?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE,     "temperature?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPSTATUS,      "tempstatus?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TESTTEMP,        "testtemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_UPPERGUARDBAND,  "highguard?"}
  };
  static const  phStringPair_t sCastleGpibCommands[] = {
    {PHKEY_NAME_HANDLER_STATUS_GET_SITE_MAPPING,                "sm?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_AIRTEMP,         "airtemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_COOL,            "cool?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_DESOAKTIME,      "desoaktime?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_HEAT,            "heat?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_LOWERGUARDBAND,  "lowerguardband?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_MASSTEMP,        "masstemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT,        "setpoint?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SOAKTIME,        "soaktime?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPCONTROL,     "tempcontrol?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE,     "temperature?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPSTATUS,      "tempstatus?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TESTTEMP,        "testtemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_UPPERGUARDBAND,  "upperguardband?"}
  };
  static const  phStringPair_t sMatrixGpibCommands[] = {
    {PHKEY_NAME_HANDLER_STATUS_GET_SITE_MAPPING,                "sm?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_AIRTEMP,         "airtemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_COOL,            "cool?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_DESOAKTIME,      "desoaktime?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_HEAT,            "heat?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_LOWERGUARDBAND,  "lowerguardband?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_MASSTEMP,        "masstemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT,        "setpoint?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SOAKTIME,        "soaktime?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPCONTROL,     "tempcontrol?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPERATURE,     "temperature?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TEMPSTATUS,      "tempstatus?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_TESTTEMP,        "testtemp?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_UPPERGUARDBAND,  "upperguardband?"}
  };

  static const  phStringPair_t sEclipseGpibCommands[] = {
    {PHKEY_NAME_HANDLER_STATUS_GET_BARCODE,                     "materialid?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID,                    "materialid?"}
  };


  static char buffer[MAX_STATUS_MSG] = "";
  int retVal = SUCCEED;
  const phStringPair_t *pStrPair = NULL;
  int paramSpecified = NO;
  int ignoreParam = NO;
  const char *actualName = "";

#ifdef DEBUG
  fprintf(stderr, "the token is %s\n",token);
  fprintf(stderr, "the param is %s\n",param);
#endif

  /*
   * if the parameter is specified for the token.
   * E.g: for PROB_HND_CALL(ph_get_status masstemp zone1)
   */
  if( strlen(param) > 0 ) {
    paramSpecified = YES;
    if( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_SITE_MAPPING) == 0 ) {
      /* can not specify the parameter for "site_mapping" query */
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                       "the query for \"site_mapping\" does not require any parameter!");
      retVal = FAIL;
    }
  }

  if( (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT) == 0) && (paramSpecified != YES) )
  {
    if(phConfConfIfDef(handlerID->myConf, PHKEY_NAME_HANDLER_STATUS_GET_SETPOINT_TESTZONE) == PHCONF_ERR_OK)
    {
      if(phConfConfString(handlerID->myConf, PHKEY_NAME_HANDLER_STATUS_GET_SETPOINT_TESTZONE, 0, NULL, &actualName) == PHCONF_ERR_OK)
      {
        param = actualName;
        paramSpecified = YES;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "the default chamber for temperature setpoint query is \"%s\".", param);
      }
    }
  }

  if( handlerID->model == PHFUNC_MOD_CASTLE_LX ){
    /*
     * if the parameter is specified for the token, we need to validate it.
     * E.g:
     *     for PROB_HND_CALL(ph_get_status masstemp zone1),
     *     zone1 is legal to Castle, but illegal to Flex. Flex use other chamber name.
     */
    if( paramSpecified == YES ) {  
      if( strstr(token, "temperature") != NULL ) {
        /* the parameter must be chamber name */
        if( (strcasecmp(param, "zone1")!=0) &&
            (strcasecmp(param, "zone2")!=0) &&
            (strcasecmp(param, "zone3")!=0) &&
            (strcasecmp(param, "zone4")!=0) &&
            (strcasecmp(param, "all")!=0) ){
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                           "the chamber \"%s\" is invalid name for Delta Castle handler!", param);
          retVal = FAIL;
        }
      }
    }
    if( retVal == SUCCEED ) {
      pStrPair = phBinSearchStrValueByStrKey(sCastleGpibCommands, LENGTH_OF_ARRAY(sCastleGpibCommands), token);
    }    
  } else if ( handlerID->model == PHFUNC_MOD_MATRIX ) {
    if( paramSpecified == YES ) {
      if( strstr(token, "temperature") != NULL ) {
        if( (strcasecmp(param, "inp_chamber")!=0) &&
            (strcasecmp(param, "xfr_chamber")!=0) &&
            (strcasecmp(param, "inp_shuttle")!=0) &&
            (strcasecmp(param, "linp_shuttle")!=0) &&
            (strcasecmp(param, "rinp_shuttle")!=0) &&
            (strcasecmp(param, "desoak")!=0) &&
            (strcasecmp(param, "ldsk_shuttle")!=0) &&
            (strcasecmp(param, "rdsk_shuttle")!=0) &&
            (strcasecmp(param, "testsite")!=0) &&
            (strncasecmp(param, "lts_", 4)!=0) &&
            (strncasecmp(param, "rts_", 4)!=0) &&
            (strcasecmp(param, "contactor")!=0) &&
            (strcasecmp(param, "ts_humidity")!=0) &&
            (strcasecmp(param, "")!=0) &&
            (strcasecmp(param, "all")!=0) ){
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
              "the chamber \"%s\" is invalid name for Delta Matrix handler!", param);
          retVal = FAIL;
        }
      }
    }
    if( retVal == SUCCEED ) {
      pStrPair = phBinSearchStrValueByStrKey(sMatrixGpibCommands, LENGTH_OF_ARRAY(sMatrixGpibCommands), token);
    }    
  } else if ( handlerID->model == PHFUNC_MOD_FLEX ) {
    /*
     * if the parameter is specified for the token, we need to validate it.
     * E.g:
     *     for PROB_HND_CALL(ph_get_status masstemp testsite), testsite
     *     is legal to Flex, but illegal to Castle. Flex use other chamber name, see above
     */
    if( paramSpecified == YES ) {  
      if( strstr(token, "temperature")!=NULL ) {
        /* the parameter must be chamber name */
        if( (strcasecmp(param, "storage")!=0)  &&
            (strcasecmp(param, "st")!=0)       &&
            (strcasecmp(param, "testsite")!=0) &&
            (strcasecmp(param, "ts")!=0)       &&
            (strcasecmp(param, "tcu")!=0) ) {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                           "the chamber \"%s\" is invalid name for Delta Flex handler!", param);
          retVal = FAIL;
        }
      }
    }
    if( retVal == SUCCEED ) {
      pStrPair = phBinSearchStrValueByStrKey(sFlexGpibCommands, LENGTH_OF_ARRAY(sFlexGpibCommands), token);
    }    
  } else if ( handlerID->model == PHFUNC_MOD_ECLIPSE ) {
    /*
     * for eclipse model
     */
    if( strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID) == 0 || strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_BARCODE) == 0) {
      /* does not require any parameter for "deviceid"/"barcode" query */
      ignoreParam = YES;
    }

    if( retVal == SUCCEED ) {
      pStrPair = phBinSearchStrValueByStrKey(sEclipseGpibCommands, LENGTH_OF_ARRAY(sEclipseGpibCommands), token);
    }
  } else {
    /* other Delta model,like PHFUNC_MOD_SUMMIT, PHFUNC_MOD_RFS, PHFUNC_MOD_ORION */
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "currently the driver only supports to query the status from "
                     "Delta Flex or Castle or Matrix or Eclipse Handler");
    retVal = FAIL;
  }

  if( pStrPair != NULL ) {
    strcpy(buffer,"");
    if (paramSpecified == YES && ignoreParam == NO)
    {
        /* refer to TDC-69304, the format is "parameter+command" */
        sprintf(buffer, "%s ", param);
        strcat(buffer, pStrPair->value);
    }
    else
    {
        sprintf(buffer, "%s", pStrPair->value);
    }
    *pGpibCommand = buffer;
  } else {
    /* phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                     "the query for \"%s\" is not supported yet!", token);*/
    retVal = FAIL;
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
    handlerID->p.trusted = 1;
    handlerID->p.verifyBins = 0;
    handlerID->p.verifyMaxCount = 0;

    handlerID->p.pauseHandler = 0;
    handlerID->p.pauseInitiatedByST = 0;

    handlerID->p.strictPolling = 0;
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
    handlerID->p.status = 0L;
    handlerID->p.stopped = 0;

    handlerID->p.identified = 0;
    handlerID->p.version1 = 0;
    handlerID->p.version2 = 0;
    handlerID->p.version3 = 0;
    handlerID->p.versionchar = '\0';

    handlerID->p.summitReprobeBug = 0;
    handlerID->p.summitNoStopStart = 0;
    handlerID->p.summitNoVerify = 0;

    handlerID->p.rfsUseTestresults = 0;
    handlerID->p.fullsitesStrLen = 0;
    handlerID->p.emulationMode=2;

    strcpy(handlerID->p.srqMask, DEFAULT_SRQMASK);

    /* the only reason for this distinction is, that we have
       tested the driver like this on real handlers. It may also
       work with "\n" for all handlers. But it will definately not
       work with "\r\n" for the Summit */

    if (handlerID->model == PHFUNC_MOD_CASTLE_LX)
        strcpy(handlerID->p.eol, "\r\n");
    else
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
    static char idString[512] = "";
    static char whichString[512] = "";    
    static char resultString[1024] = "";
    static char stripmap[64] = "";  /* hardcoded 64 should be large enough for stripmap? answer. (Orion)*/

    resultString[0] = '\0';

    phFuncError_t retVal = PHFUNC_ERR_OK;

    *equipIdString = resultString;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);

    /* common to all models: identify */
    PhFuncTaCheck(phFuncTaSend(handlerID, "identify?%s", handlerID->p.eol));
//    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", idString));
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", idString);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
        return retVal;
    }

    switch (handlerID->model)
    {
    case PHFUNC_MOD_CASTLE_LX:
    case PHFUNC_MOD_MATRIX:
    case PHFUNC_MOD_SUMMIT:
    case PHFUNC_MOD_FLEX:
    case PHFUNC_MOD_RFS:
        PhFuncTaCheck(phFuncTaSend(handlerID, "which?%s", handlerID->p.eol));
//        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", whichString));
        retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", whichString);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
        /* create result string */
        sprintf(resultString, "%s, %s", idString, whichString);
        break;

    case PHFUNC_MOD_ORION:
        PhFuncTaCheck(phFuncTaSend(handlerID, "stripmap?%s", handlerID->p.eol));
//        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", stripmap));
        retVal = phFuncTaReceive(handlerID, 1, "%s", stripmap);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
        /* create result string */
        sprintf(resultString, "%s. Current stripmap configuration: %s", idString, stripmap);
        break;
    case PHFUNC_MOD_ECLIPSE:
        /* for eclipse, currently we can only support the idString */
        sprintf(resultString, "%s", idString);
        break;

    default:
        break;
    }
    phFuncTaStop(handlerID);

    return retVal;
}
#endif



#ifdef STRIPINDEXID_IMPLEMENTED
/*****************************************************************************
 *
 * return the test-on-strip index identifier (for Orion handler only).
 *
 * Authors: Ryan Lessman
 *
 * Description:  The Orion test-on-strip handler may have multiple index
 *               possibilities per strip.  This function allows the current
 *               index ID to be checked via the App Model.
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStripIndexID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **indexIdString     /* resulting pointer to index ID string */
)
{
    static char indexAnswer[4];  /*hardcoded 4 should be enough for index? query answer.*/
    indexAnswer[0] = '\0';

    phFuncError_t retVal = PHFUNC_ERR_OK;
    *indexIdString = indexAnswer;
    
    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    switch (handlerID->model)
    {
    case PHFUNC_MOD_CASTLE_LX:
    case PHFUNC_MOD_MATRIX:
    case PHFUNC_MOD_SUMMIT:
    case PHFUNC_MOD_FLEX:
    case PHFUNC_MOD_RFS:
    case PHFUNC_MOD_ECLIPSE:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "The index? query is only available on Orion handler.");
        break;
    case PHFUNC_MOD_ORION:
        PhFuncTaCheck(phFuncTaSend(handlerID, "index?%s", handlerID->p.eol));
//        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", indexAnswer));
        retVal = phFuncTaReceive(handlerID, 1, "%s", indexAnswer);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
        break;
    default:
        break;
    }

    return retVal;
}
#endif



#ifdef STRIPMATERIALID_IMPLEMENTED
/*****************************************************************************
 *
 * return the test-on-strip material ID, read by DCR (for Orion handler only).
 *
 * Authors: Ryan Lessman
 *
 * Description:  The Orion test-on-strip handler can use a Dot Code Reader
 *               to read an ID string from each strip.  This function makes
 *               the materialid? string available to the App Model.
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStripMaterialID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **materialIdString     /* resulting pointer to material ID string */
)
{
    static char materialID[128];  /*hardcoded 128 should be enough for materialid? query answer.*/
    materialID[0] = '\0';
    phFuncError_t retVal = PHFUNC_ERR_OK;
    *materialIdString = materialID;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    switch (handlerID->model)
    {
    case PHFUNC_MOD_CASTLE_LX:
    case PHFUNC_MOD_MATRIX:
    case PHFUNC_MOD_SUMMIT:
    case PHFUNC_MOD_FLEX:
    case PHFUNC_MOD_RFS:
    case PHFUNC_MOD_ECLIPSE:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "The materialid? query is only available on Orion handler.");
        break;
    case PHFUNC_MOD_ORION:
        PhFuncTaCheck(phFuncTaSend(handlerID, "materialid?%s", handlerID->p.eol));
//        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", materialID));
        retVal = phFuncTaReceive(handlerID, 1, "%s", materialID);
        if (retVal != PHFUNC_ERR_OK) {
            phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
            return retVal;
        }
        break;
    default:
        break;
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

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;

    /* remember which devices we expect now */
    for (i=0; i<handlerID->noOfSites; i++)
	handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

    phFuncTaStart(handlerID);

    phFuncTaMarkStep(handlerID);
//    PhFuncTaCheck(waitForParts(handlerID));

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
	    PhFuncTaCheck(queryStatus(handlerID, 1));
    

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
    PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation, 
	NULL, perSiteBinMap));
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
    long perSiteReprobe[PHESTATE_MAX_SITES];
    long perSiteBinMap[PHESTATE_MAX_SITES];
    int i;

    /* prepare to reprobe everything */
    for (i=0; i<handlerID->noOfSites; i++)
    {
	perSiteReprobe[i] = 1L;
	perSiteBinMap[i] = 100L;
    }

    return privateBinReprobe(handlerID, perSiteReprobe, perSiteBinMap);
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
//    PhFuncTaCheck(waitForParts(handlerID));
    retVal = waitForParts(handlerID);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveToMark(handlerID);
        return retVal;
    }

    /* do we have at least one part? If not, ask for the current
       status and return with waiting */
    if (!handlerID->p.oredDevicePending)
    {
	PhFuncTaCheck(queryStatus(handlerID, 1));

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
	      switch (handlerID->model)
		{
		case PHFUNC_MOD_SUMMIT:
		  /* the Summit does not take binning while receiving
                     reprobe information. We therefore expect that
                     devices that were existent before and were not
                     reprobed still exist ! */
		  if (handlerID->p.devicePending[i] && 
		      (oldPopulation[i] == PHESTATE_SITE_EMPTY || 
		       oldPopulation[i] == PHESTATE_SITE_DEACTIVATED))
		    {
		      /* ignore any new devices in so far empty sites */
		      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
				       "new device at site \"%s\" will not be tested during reprobe action (delayed)",
				       handlerID->siteIds[i]);
		      population[i] = PHESTATE_SITE_EMPTY;
		      if (oldPopulation[i] == PHESTATE_SITE_POPDEACT ||
			  oldPopulation[i] == PHESTATE_SITE_DEACTIVATED)
			population[i] = PHESTATE_SITE_DEACTIVATED;
		    }
		  else if (! handlerID->p.devicePending[i] &&
		      (oldPopulation[i] == PHESTATE_SITE_POPDEACT ||
		      oldPopulation[i] == PHESTATE_SITE_DEACTIVATED))
		    {
		      /* expect old devices in filled sites */
		      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
				       "existing device at site \"%s\" disapeared during reprobe action, not binned!",
				       handlerID->siteIds[i]);
		      population[i] = PHESTATE_SITE_EMPTY;
		      if (oldPopulation[i] == PHESTATE_SITE_POPDEACT ||
			  oldPopulation[i] == PHESTATE_SITE_DEACTIVATED)
			population[i] = PHESTATE_SITE_DEACTIVATED;
		    }
		  else /* state of site has not changed, correct */
		    population[i] = oldPopulation[i];
		  break;
		default:
		  /* everywhere else it's different: All devices need
                     to receive a bin or reprobe data, and devices
                     that were not reprobed must be gone */
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
		  break;
		}
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
    int isPaused;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;
    
    if (handlerID->p.pauseHandler)
    {
	phEstateAGetPauseInfo(handlerID->myEstate, &isPaused);
	if (!isPaused)
	{
	    phFuncTaStart(handlerID);
	    PhFuncTaCheck(doPauseHandler(handlerID));
	    phFuncTaStop(handlerID);
	}
	if (handlerID->p.stopped)
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
    if (phFuncTaAskAbort(handlerID))
	return PHFUNC_ERR_ABORTED;
    
    phEstateAGetPauseInfo(handlerID->myEstate, &isPaused);
    if (isPaused)
    {
	phFuncTaStart(handlerID);
	PhFuncTaCheck(doUnpauseHandler(handlerID));
	phFuncTaStop(handlerID);
    }
    if (!handlerID->p.stopped)
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
    char *machineID;

    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);
    retVal = getMachineID(handlerID, &machineID);
    phFuncTaStop(handlerID);

    /* return result, there is no real check for the model here (so far) */
    if (testPassed)
        *testPassed = (retVal == PHFUNC_ERR_OK) ? 1 : 0;
    return retVal;
}
#endif


#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The private function for get status
 *
 * Authors: Jiawei Lin
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

  if ( phFuncTaAskAbort(handlerID) ) {
    return PHFUNC_ERR_ABORTED;
  }

  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                   "privateGetStatus, token = ->%s<-, param = ->%s<-", token, param);

  found = getGpibCommandForGetStatusQuery(handlerID, &gpibCommand, token, param);
  if( found == SUCCEED ) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateGetStatus, gpibCommand = ->%s<-", gpibCommand);
    PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", gpibCommand, handlerID->p.eol));
    localDelay_us(100000);
    retVal = phFuncTaReceive(handlerID, 1, "%4095[^\r\n]", response);
    if (retVal != PHFUNC_ERR_OK) {
      phFuncTaRemoveStep(handlerID);  /* force re-send of command */
      return retVal;
    }

    /* handle 2D bar code for Eclipse model*/
    if(PHFUNC_MOD_ECLIPSE == handlerID->model \
        && (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID) == 0 \
        || strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_BARCODE) == 0))
    {
      char *p = NULL;
      int i = 0;
      char barcodes[MAX_STATUS_MSG] = {0};
      int iDeviceID = atoi(param);

      strcpy(barcodes, response);
      memset(response, 0, MAX_STATUS_MSG);
      p = strtok(barcodes, ":");
      while(p)
      {
        if(++i == iDeviceID)
        {
          strcpy(response, p);
          break;
        }
        p = strtok(NULL, ":");
      }
    }
  } else {
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

  /* return result */
  return retVal;
}
#endif

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
        PhFuncTaCheck(queryStatus(handlerID, 0));
    }

    phFuncTaStop(handlerID);

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

#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot start signal from handler
 *
 * Authors: Zoyi Yu
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

    if(handlerID->model == PHFUNC_MOD_ECLIPSE) {
      if (0 == handlerID->p.lotStarted)
      {
          phFuncTaRemoveToMark(handlerID);
          return PHFUNC_ERR_WAITING;
      }
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
 * Authors: Zoyi Yu
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

