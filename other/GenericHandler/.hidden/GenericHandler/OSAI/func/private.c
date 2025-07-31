/******************************************************************************
 *
 *       (c) Copyright Advantest Ltd., 2017
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 2017
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Adam Huang, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 2017, Adam Huang, created
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
#include <errno.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_hfunc.h"
#include "private.h"
#include "ph_keys.h"
#include "gpib_conf.h"
#include "ph_hfunc_private.h"

#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#define MAX_HANDLER_ANSWER_LENGTH 8192
#define MAX_HANDLER_MESSAGEID_LENGTH 32 
#define MAX_HANDLER_STRING_LENGTH 256
#define MAX_RECEIVED_MESSAGE_LENGTH 4096 
static int MESSAGE_ID = 0;
static char lotEndMsgID[MAX_HANDLER_MESSAGEID_LENGTH] = {0};
static const char * NEO_HANDLER_LOT_START_STRING = "SOL_COMMAND";
static const char * NO_BINNING_RESULT = "X";
static char szNeoHandlerLotStartString[MAX_HANDLER_STRING_LENGTH] = {0};
static char szNeoHandlerDutCode[PHESTATE_MAX_SITES][MAX_HANDLER_STRING_LENGTH] = {};
static char szNeoHandlerSiteInfo[PHESTATE_MAX_SITES*MAX_HANDLER_STRING_LENGTH] = {0};
static const char * LOT_START_MESSAGE       = "SOL";
static const char * LOT_DONE_MESSAGE        = "EOL";
static const char * LOT_DONE_ACK            = "EOL=OK";
static const char * LOT_ASK_START_MESSAGE   = "ASK_START";
static const char * LOT_ASK_START_ACK       = "ASK_START=OK";
static const char * TEST_SITES_INFO         = "TESTSITES";
static const char * TEST_SITES_INFO_ERROR   = "TESTSITES=ERROR";
static const char * TEST_SITES_INFO_ACK     = "TESTSITES=OK";
static const char * TEST_START_MESSAGE      = "START";
static const char * TEST_START_MESSAGE_ACK  = "START=OK";
static const char * TEST_RESULTS_ACK        = "TESTRESULTS=OK";
static const char * TEST_RESULTS_ERROR      = "TESTRESULTS=ERROR";
static const char * HANDLER_PAUSE_COMMAND   = "COMMAND=PAUSE";
static const char * HANDLER_PAUSE_ACK       = "COMMAND=OK";
static const char * HANDLER_RESUME_COMMAND  = "COMMAND=RESUME";
static const char * HANDLER_RESUME_ACK      = "COMMAND=OK";
static const char * DATANG = "";

/* this macro allows to test a not fully implemented plugin */

/* #define ALLOW_INCOMPLETE */
#define ALLOW_INCOMPLETE

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
static void getIDFromMessage(char* handlerAnswer, char* messageID, int idLength)
{
    memset(messageID, 0, idLength);
    char *remaining = strchr(handlerAnswer, '|');
    strncpy(messageID, handlerAnswer+1, strlen(handlerAnswer)-strlen(remaining)-1);
}

static int getIDFromDriver()
{
    return (++MESSAGE_ID > 9999 ? (MESSAGE_ID = 1) : MESSAGE_ID);
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
    if (select(0, NULL, NULL, NULL, &delay) == -1 && errno == EINTR)
        return 0;
    else
        return 1;
}

/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
    phConfError_t confError;
    double dPollingInterval;

    /* retrieve polling interval */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_POLLT);
    if (confError == PHCONF_ERR_OK)
    {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_POLLT, 0, NULL, &dPollingInterval);
        if (confError == PHCONF_ERR_OK)
            handlerID->p.pollingInterval = labs((long) dPollingInterval);
        else
            handlerID->p.pollingInterval = 200000L; /* default 0.2 sec */
    }

    return PHFUNC_ERR_OK;
}

static phFuncError_t handleSitesInfo(phFuncId_t handlerID, char * handlerAnswer)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char siteInfo[MAX_HANDLER_ANSWER_LENGTH] = {0};
    char messageID[MAX_HANDLER_MESSAGEID_LENGTH] = {0};
    int siteCount = 0;

    //receive the TEST (site setup) message
    if (sscanf(handlerAnswer, "%*[^=]=%[^\x03]", siteInfo) != 1)
    {
        //incorrect site setup command format
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                "Incorrect site population received from handler: \n"
                "received \"%s\" format expected \"\x02\x34|TESTSITES=X_+1_002+...\x03\" \n"
                "trying again", handlerAnswer);
        phFuncTaRemoveStep(handlerID); //mark
        phFuncTaRemoveStep(handlerID); //receive
        handlerID->p.oredDevicePending = 0;
        retVal = PHFUNC_ERR_WAITING;
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_1, "begin to parse site info message %s", siteInfo);
        strncpy(szNeoHandlerSiteInfo, siteInfo, PHESTATE_MAX_SITES*MAX_HANDLER_STRING_LENGTH-1);
        char * posToParse = siteInfo;
        
        while(posToParse != NULL)
        {
            if(*posToParse == 'X')
            {
                //there is no device
                handlerID->p.devicePending[siteCount] = 0;
                //skip X_
                posToParse += 2;
                char * posOfPlus = strchr(posToParse, '+'); //find the next site info
                if(posOfPlus == NULL)                       //will be end of site info
                    posToParse = NULL;
                else
                    posToParse = posOfPlus+1;                      //skip '+' to point next site info

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "no device at site \"%s\" (polled)", handlerID->siteIds[siteCount]);
                siteCount++;
            }
            else
            {
                handlerID->p.devicePending[siteCount] = 1;
                handlerID->p.oredDevicePending = 1;
                //skip 1_
                posToParse += 2;
                char * posOfPlus = strchr(posToParse, '+'); //find the next site info
                if(posOfPlus == NULL)                       //will be end of site info
                {
                    strncpy(szNeoHandlerDutCode[siteCount], posToParse, MAX_HANDLER_ANSWER_LENGTH-1); 
                    posToParse = NULL;
                }
                else
                {
                    strncpy(szNeoHandlerDutCode[siteCount], posToParse, posOfPlus - posToParse);
                    posToParse = posOfPlus+1;                      //skip '+' to point next site info
                }

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "device present at site \"%s\" (polled) with dut code \"%s\" ", handlerID->siteIds[siteCount], szNeoHandlerDutCode[siteCount]);
                siteCount++;
            }
        }
        //succefully parsed site info
        getIDFromMessage(handlerAnswer, messageID, MAX_HANDLER_MESSAGEID_LENGTH-1);
        phFuncTaSend(handlerID, "\x02%s|%s\x03", messageID, TEST_SITES_INFO_ACK);
    }

    if (siteCount > handlerID->noOfSites)
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                "The handler seems to present more devices than configured\n"
                "The driver configuration must be changed to support more sites");
    return retVal;
}

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID, int * completedInvocation)
{
    static char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    char messageID[MAX_HANDLER_MESSAGEID_LENGTH] = {0};
    char receivedMessage[MAX_RECEIVED_MESSAGE_LENGTH] = {0};

    if(*completedInvocation == 1)
    {
        memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
        *completedInvocation = 0;
    }

    switch (handlerID->model)
    {
        case PHFUNC_MOD_NEO:
            do{
                memset(receivedMessage, 0, MAX_RECEIVED_MESSAGE_LENGTH);
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", receivedMessage));
                if(strchr(receivedMessage, '\x03') == NULL)
                    phFuncTaRemoveStep(handlerID);
                strncat(handlerAnswer, receivedMessage, MAX_RECEIVED_MESSAGE_LENGTH);
            } while(strchr(handlerAnswer, '\x03') == NULL);
            
            if(strstr(handlerAnswer, TEST_SITES_INFO) != NULL)
                return handleSitesInfo(handlerID, handlerAnswer);
            else if(strstr(handlerAnswer, LOT_DONE_MESSAGE) != NULL)
            {
                phTcomSetSystemFlag(handlerID->myTcom, PHTCOM_SF_CI_LEVEL_END, 1L);
                getIDFromMessage(handlerAnswer, lotEndMsgID, MAX_HANDLER_MESSAGEID_LENGTH-1);
                return PHFUNC_ERR_LOT_DONE;
            }
            else
            {
                getIDFromMessage(handlerAnswer, messageID, MAX_HANDLER_MESSAGEID_LENGTH-1);
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "unexpected command \"%s\" received from handler, will abort test", handlerAnswer);
                phFuncTaSend(handlerID, "\x02%s|%s\x03", messageID, TEST_SITES_INFO_ERROR);
                return PHFUNC_ERR_ABORTED;
            }
            break;
        default:
            break;
    }

    return PHFUNC_ERR_OK;
}

/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID, int *completedInvocation)
{
    struct timeval pollStartTime;
    struct timeval pollStopTime;
    int timeout;

    /* apply strict polling loop, ask for site population */
    gettimeofday(&pollStartTime, NULL );

    timeout = 0;
    localDelay_us(handlerID->p.pollingInterval);
    do
    {
        PhFuncTaCheck(pollParts(handlerID, completedInvocation));
        if (!handlerID->p.oredDevicePending)
        {
            gettimeofday(&pollStopTime, NULL );
            if ((pollStopTime.tv_sec - pollStartTime.tv_sec) * 1000000L
                    + (pollStopTime.tv_usec - pollStartTime.tv_usec)
                    > handlerID->heartbeatTimeout)
                timeout = 1;
            else
                localDelay_us(handlerID->p.pollingInterval);
        }
    } while (!handlerID->p.oredDevicePending && !timeout);

    return PHFUNC_ERR_OK;
}

static phFuncError_t setupBinCode(phFuncId_t handlerID /* driver plugin ID */,
        long binNumber       /* for bin number */,
        char *binCode        /* array to setup bin code */)
{
    if (handlerID->binIds)
    {
        /* if handler bin id is defined (hardbin/softbin mapping), get the bin code from the bin id list*/
        strcpy(binCode, handlerID->binIds[binNumber]);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                "handler_bin_ids is defined and handler_bin_ids[%ld]=\"%s\". " \
                "In the RESULT command, \"%s\" is used as bin ID.",
                binNumber, binCode, binCode);
    }
    else
    {
        /* if handler bin id is not defined (default mapping), use the bin number directly */
        sprintf(binCode, "%ld", binNumber);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                "handler_bin_ids NOT defined, using hardbin number %ld directly. " \
                "In the RESULT command, \"%s\" is used as bin ID.",
                binNumber, binCode);
    }

    return PHFUNC_ERR_OK;
}

/* create and send bin and reprobe information information */
static phFuncError_t binAndReprobe( phFuncId_t handlerID  /* driver plugin ID */,
                                    phEstateSiteUsage_t *oldPopulation /* current site population */,
                                    long *perSiteReprobe  /* TRUE, if a device needs reprobe*/,
                                    long *perSiteBinMap   /* valid binning data for each site where
                                                             the above reprobe flag is not set */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char testresults[PHESTATE_MAX_SITES * 2 + 512] = "";
    static char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0}; 
    char thisbin[32] = {0};
    int siteCount = 0;
    memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);

    phFuncTaMarkStep(handlerID);
    switch (handlerID->model)
    {
        case PHFUNC_MOD_NEO:
            snprintf(testresults, strlen(testresults)-1, "\x02%d|TESTRESULTS=", getIDFromDriver());
            for (siteCount = 0;
                 siteCount < handlerID->noOfSites && siteCount < PHESTATE_MAX_SITES && retVal == PHFUNC_ERR_OK;
                 siteCount++)
            {
                if (handlerID->activeSites[siteCount] && (oldPopulation[siteCount] == PHESTATE_SITE_POPULATED || oldPopulation[siteCount] == PHESTATE_SITE_POPDEACT))
                {
                    /* there is a device here */
                    setupBinCode(handlerID, perSiteBinMap[siteCount], thisbin);
                    strcat(testresults, thisbin);
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "will bin device at site \"%s\" to %s", handlerID->siteIds[siteCount], thisbin);
                }
                else
                {
                    /* there's no device here */
                    strcat(testresults, NO_BINNING_RESULT);
                    if (siteCount < handlerID->noOfSites)
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "no device to bin at site \"%s\", use %s as bin ID", handlerID->siteIds[siteCount], NO_BINNING_RESULT);
                }

                if (siteCount != handlerID->noOfSites-1)
                {
                    strcat(testresults, "_");
                    strcat(testresults, szNeoHandlerDutCode[siteCount]);
                    strcat(testresults, "+");
                }
                else
                {
                    strcat(testresults, "_");
                    strcat(testresults, szNeoHandlerDutCode[siteCount]);
                }
            }
            //end flag
            strcat(testresults, "\x03");

            PhFuncTaCheck(phFuncTaSend(handlerID, "%s", testresults));
            PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));

            if(strstr(handlerAnswer, TEST_RESULTS_ACK) == NULL)
            {
                //sometimes we will receive the echo bin information and ack in one message  
                //in that case we will fall into infinite timeout issue since timeout will clear the handlerAnswer
                //which means there will not be second ack
                char receivedMessage[MAX_RECEIVED_MESSAGE_LENGTH] = {0};
                do{
                    memset(receivedMessage, 0, MAX_RECEIVED_MESSAGE_LENGTH);
                    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", receivedMessage));
                    if(strchr(receivedMessage, '\x03') == NULL)
                        phFuncTaRemoveStep(handlerID);
                    strncat(handlerAnswer, receivedMessage, MAX_RECEIVED_MESSAGE_LENGTH);
                } while(strchr(handlerAnswer, '\x03') == NULL);

                if(strstr(handlerAnswer, TEST_RESULTS_ACK) == NULL)
                {
                    //since there will consecutive reply from handler
                    //we may received TEST_RESULTS_ACK in previous invocation
                    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
                }

                if(strstr(handlerAnswer, TEST_RESULTS_ACK) == NULL || strstr(handlerAnswer, TEST_RESULTS_ERROR) != NULL)
                {
                    retVal = PHFUNC_ERR_ABORTED;
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                            "Expecting \"%s\" but the response \"%s\" is received for the RESULT command, try again",
                            TEST_RESULTS_ACK, handlerAnswer);
                }
            }
            break;
        default:
            break;
    }

    return retVal;
}

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Adam Huang
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
    int i = 0;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;

    /* do some really initial settings */
    handlerID->p.sites = handlerID->noOfSites;
    for (i = 0; i < handlerID->p.sites; i++)
    {
        handlerID->p.siteUsed[i] = handlerID->activeSites[i];
        handlerID->p.devicePending[i] = 0;
        handlerID->p.deviceExpected[i] = 0;
    }

    MESSAGE_ID = 0;
    handlerID->p.strictPolling = 1; //always polling since it's acted as a server roll
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
    handlerID->p.status = 0L;
    handlerID->p.paused = 0;
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
 * Authors: Adam Huang
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
 * Authors: Adam Huang
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
 * Authors: Adam Huang
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
 * Authors: Adam Huang
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
 * Authors: Adam Huang
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
    char messageID[MAX_HANDLER_MESSAGEID_LENGTH] = {0};
    char handlerAnswer[512] = "";
    int i = 0;
    static int completedInvocation = 0;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];

    /*init the dut code*/
    memset(szNeoHandlerDutCode, 0, PHESTATE_MAX_SITES*MAX_HANDLER_STRING_LENGTH);
    memset(szNeoHandlerSiteInfo, 0, PHESTATE_MAX_SITES*MAX_HANDLER_STRING_LENGTH);

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;

    /* remember which devices we expect now */
    for (i = 0; i < handlerID->noOfSites; i++)
        handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

    phFuncTaStart(handlerID);

    phFuncTaMarkStep(handlerID);

    PhFuncTaCheck(waitForParts(handlerID, &completedInvocation));

    /* do we have at least one part? If not, ask for the current status and return with waiting */
    if (!handlerID->p.oredDevicePending)
    {
        /* during the next call everything up to here should be repeated */
        phFuncTaRemoveToMark(handlerID);
        return PHFUNC_ERR_WAITING;
    }

    /* Send TEST RUN to the handler */
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_4, "Ready for test flow execution, waiting for the START signal form the handler");
    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
    if(strstr(handlerAnswer, TEST_START_MESSAGE) == NULL)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "unexpected answer from handler: \"%s\".", handlerAnswer);
        return PHFUNC_ERR_ANSWER;
    }
    getIDFromMessage(handlerAnswer, messageID, MAX_HANDLER_MESSAGEID_LENGTH-1);

    PhFuncTaCheck(phFuncTaSend(handlerID, "\x02%s|%s\x03", messageID, TEST_START_MESSAGE_ACK));

    phFuncTaStop(handlerID);

    /* we have received devices for test. Change the equipment specific state now */
    handlerID->p.oredDevicePending = 0;
    for (i = 0; i < handlerID->noOfSites; i++)
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
            if (handlerID->p.devicePending[i]) /* something is wrong here, we got a device for an inactive site */
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "received device for deactivated site \"%s\"",handlerID->siteIds[i]);
            population[i] = PHESTATE_SITE_DEACTIVATED;
        }
        handlerID->p.oredDevicePending |= handlerID->p.devicePending[i];
    }
    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);

    completedInvocation = 1;
    return PHFUNC_ERR_OK;
}
#endif



#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested device
 *
 * Authors: Adam Huang
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
    phEstateSiteUsage_t *oldPopulation;
    int entries;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    int i;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
    {
        return PHFUNC_ERR_ABORTED;
    }

    /* get current site population */
    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

    phFuncTaStart(handlerID);
    PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation, NULL, perSiteBinMap));
    phFuncTaStop(handlerID);

    /* modify site population, everything went fine, otherwise we would not reach this point */
    for (i = 0; i < handlerID->noOfSites; i++)
    {
        if (handlerID->activeSites[i] && (oldPopulation[i] == PHESTATE_SITE_POPULATED || oldPopulation[i] == PHESTATE_SITE_POPDEACT))
            population[i] = oldPopulation[i] == PHESTATE_SITE_POPULATED ? PHESTATE_SITE_EMPTY : PHESTATE_SITE_DEACTIVATED;
        else
            population[i] = oldPopulation[i];
    }

    /* change site population */
    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);

    return PHFUNC_ERR_OK;
}
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Adam Huang
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
 * Authors: Adam Huang
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
 * Authors: Adam Huang
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
    char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};

    phFuncTaMarkStep(handlerID);

    if (!handlerID->p.paused)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Sending pause command to handler...");

        PhFuncTaCheck(phFuncTaSend(handlerID, "\x02%d|%s\x03", getIDFromDriver(), HANDLER_PAUSE_COMMAND));
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
        memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s", handlerAnswer));

        if (strstr(handlerAnswer, HANDLER_PAUSE_ACK) != NULL)
        {
            handlerID->p.paused = 1;
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Handler is paused...");
        }
        else
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "Failed to pause the handler...");
            phFuncTaRemoveToMark(handlerID);
            return PHFUNC_ERR_WAITING;
        }
    }

    return PHFUNC_ERR_OK;
}
#endif



#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to handler plugin
 *
 * Authors: Adam Huang
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
    char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};

    phFuncTaMarkStep(handlerID);

    if (handlerID->p.paused)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Sending continue command to handler...");

        PhFuncTaCheck(phFuncTaSend(handlerID, "\x02%d|%s\x03", getIDFromDriver(), HANDLER_RESUME_COMMAND));
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
        memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s", handlerAnswer));

        if (strstr(handlerAnswer, HANDLER_RESUME_ACK) != NULL)
        {
            handlerID->p.paused = 0;
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Handler continues...");
        }
        else
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "Failed to continue the handler...");
            phFuncTaRemoveToMark(handlerID);
            return PHFUNC_ERR_WAITING;
        }
    }

    return PHFUNC_ERR_OK;
}
#endif



#ifdef DIAG_IMPLEMENTED
/*****************************************************************************
 *
 * retrieve diagnostic information
 *
 * Authors: Adam Huang
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
 * Authors: Adam Huang
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
}
#endif



#ifdef UPDATE_IMPLEMENTED
/*****************************************************************************
 *
 * update the equipment state
 *
 * Authors: Adam Huang
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
 * Authors: Adam Huang
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phFuncError_t privateCommTest(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateCommTest");
}
#endif



#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Adam Huang
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
    char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    char handlerMsg[MAX_HANDLER_ANSWER_LENGTH] = {0};
    char messageID[MAX_HANDLER_MESSAGEID_LENGTH] = {0};
    static int receivedFlag = 0;     //to avoid repeated comparison of received msg

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateLotStart, %s", timeoutFlag ? "TRUE" : "FALSE");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);

    //warting for start lot command
    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
    if(receivedFlag == 0)
    {
        if(strstr(handlerAnswer, LOT_START_MESSAGE) == NULL)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "unexpected answer from handler: \"%s\".", handlerAnswer);
            return PHFUNC_ERR_ANSWER;
        }
        getIDFromMessage(handlerAnswer, messageID, MAX_HANDLER_MESSAGEID_LENGTH-1);
    }
    receivedFlag = 1;

    //sending the lot information string
    snprintf(handlerMsg, MAX_HANDLER_ANSWER_LENGTH-1, "\x02%s|%s\x03", messageID, szNeoHandlerLotStartString);
    PhFuncTaCheck(phFuncTaSend(handlerID, "%s", handlerMsg));

    //warting for ask start command
    memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
    if(strstr(handlerAnswer, LOT_ASK_START_MESSAGE) == NULL)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "unexpected answer from handler: \"%s\".", handlerAnswer);
        return PHFUNC_ERR_ANSWER;
    }

    //sending the response to handler
    getIDFromMessage(handlerAnswer, messageID, MAX_HANDLER_MESSAGEID_LENGTH-1);
    PhFuncTaCheck(phFuncTaSend(handlerID, "\x02%s|%s\x03", messageID, LOT_ASK_START_ACK));

    phFuncTaStop(handlerID);
    receivedFlag = 0;

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
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    char handlerMsg[MAX_HANDLER_ANSWER_LENGTH] = {0};
    char messageID[MAX_HANDLER_MESSAGEID_LENGTH] = {0};
    memset(szNeoHandlerLotStartString, 0, MAX_HANDLER_STRING_LENGTH);

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateLotDone");

    phFuncTaStart(handlerID);
    if(strncmp(lotEndMsgID, "", MAX_HANDLER_MESSAGEID_LENGTH) == 0)
    {
        /* abort in case of unsuccessful retry */
        if ( phFuncTaAskAbort(handlerID) )
            return PHFUNC_ERR_ABORTED;

        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
        if(strstr(handlerAnswer, LOT_DONE_MESSAGE) == NULL)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "unexpected answer from handler: \"%s\".", handlerAnswer);
            return PHFUNC_ERR_ANSWER;
        }

        getIDFromMessage(handlerAnswer, messageID, MAX_HANDLER_MESSAGEID_LENGTH-1);
        snprintf(handlerMsg, MAX_HANDLER_ANSWER_LENGTH-1, "\x02%s|%s\x03", messageID, LOT_DONE_ACK); 
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s", handlerMsg));

        phTcomSetSystemFlag(handlerID->myTcom, PHTCOM_SF_CI_LEVEL_END, 1L);
    }
    else
    {
        snprintf(handlerMsg, MAX_HANDLER_ANSWER_LENGTH-1, "\x02%s|%s\x03", lotEndMsgID, LOT_DONE_ACK); 
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s", handlerMsg));
    }
    phFuncTaStop(handlerID);

    memset(lotEndMsgID, 0, MAX_HANDLER_MESSAGEID_LENGTH);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Exiting privateLotDone, retVal = %d", retVal);
    return retVal;
}
#endif

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
                                                             set the information from Handler */
                              const char *param           /* the parameter for command string */
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE, "privateSetStatus, token = ->%s<-, param = ->%s<-", token, param);

    if (strcasecmp(token, NEO_HANDLER_LOT_START_STRING) == 0)
        strncpy(szNeoHandlerLotStartString, param, MAX_HANDLER_STRING_LENGTH-1);
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "The key \"%s\" is not available, or may not be supported yet", token);
        retVal = PHFUNC_ERR_ANSWER;
    }

    return retVal;
}
#endif

#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The stub function for get status
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
    static char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    const char *token = commandString;
    const char *param = paramString;
    memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
    phFuncError_t retVal = PHFUNC_ERR_OK;
    *responseString = handlerAnswer;

    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE, "privateGetStatus, token = ->%s<-, param = ->%s<-", token, param);

    if (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID) == 0 || strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_BARCODE) == 0)
    {
        if (0 == strlen(param))
            strncpy(handlerAnswer, szNeoHandlerSiteInfo, MAX_HANDLER_ANSWER_LENGTH-1);
        else
        {
            int iDeviceID = atoi(param);
            if(iDeviceID <= 0 || *(szNeoHandlerDutCode[iDeviceID-1]) == '\0')
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "The specific Deivce ID is Invalid, will return empty string!");
                strncpy(handlerAnswer, DATANG, strlen(DATANG));
            }
            else
                strncpy(handlerAnswer, szNeoHandlerDutCode[iDeviceID-1], MAX_HANDLER_STRING_LENGTH-1);
        }
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "The key \"%s\" is not available, or may not be supported yet", token);
        sprintf(handlerAnswer, "%s_KEY_NOT_AVAILABLE", token);
        retVal = PHFUNC_ERR_ANSWER;
    }

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE, "privateGetStatus, answer ->%s<-, length = %d", handlerAnswer, strlen(handlerAnswer));

    return retVal;
}
#endif

#ifdef EXECGPIBCMD_IMPLEMENTED
/*****************************************************************************
 *
 * Send handler setup and action command by GPIB
 *
 * Authors:Xiaofei Han
 *
 * Description:
 *
 * This fucntion is used to send handler setup and action command to setup handler
 * initial parameters and control handler.
 *
 * handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateExecGpibCmd(phFuncId_t handlerID /* driver plugin ID */,
                                 char *commandString /* key name to get parameter/information */,
                                 char **responseString /* output of response string */)
{
    static char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
    phFuncError_t retVal = PHFUNC_ERR_OK;
    *responseString = handlerAnswer;

    phFuncTaStart(handlerID);
    /* execute command */
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateExecGpibCmd, command = ->%s<-", commandString);
    retVal = phFuncTaSend(handlerID, "\x02%d|%s\x03", getIDFromDriver(), commandString);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateExecGpibCmd sent ->%s<-, result is %d", commandString, retVal);
    if(handlerID->waitingForCommandReply== 1)
    {
        /* echo command */
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);

        /* receive command result */
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "call phFuncTaReceive in privateExecGpibCmd, return %d", retVal);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateExecGpibCmd, answer ->%s<-, length = %d", handlerAnswer, strlen(handlerAnswer));
        if(strstr(commandString, "STATUSH") != NULL || strstr(commandString, "QUERY") != NULL)
            retVal = phFuncTaSend(handlerID, "%s", handlerAnswer);
        else
        {
            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            retVal = phFuncTaSend(handlerID, "%s", handlerAnswer);
            retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
            retVal = phFuncTaSend(handlerID, "%s", handlerAnswer);
        }
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateExecGpibCmd will not wait answer");
    }

    phFuncTaStop(handlerID);  
    return retVal;
}
#endif

#ifdef EXECGPIBQUERY_IMPLEMENTED
/*****************************************************************************
 *
 * Send handler query command by GPIB
 *
 * Authors:Xiaofei Han 
 *
 * Description:
 *
 * This fucntion is used to send handler query command to get handler parameters
 * during handler driver runtime.
 *
 * handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateExecGpibQuery(phFuncId_t handlerID /* driver plugin ID */,
                                   char *commandString /* key name to get parameter/information */,
                                   char **responseString /* output of response string */)
{
    static char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0}; 
    memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
    *responseString = handlerAnswer;
    phFuncError_t retVal = PHFUNC_ERR_OK;

    phFuncTaStart(handlerID);

    /* execute command */
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateExecGpibQuery, command = ->%s<-", commandString);
    PhFuncTaCheck(phFuncTaSend(handlerID, "\x02%d|%s\x03", getIDFromDriver(), commandString));
    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
    memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", handlerAnswer));
    PhFuncTaCheck(phFuncTaSend(handlerID, "%s", handlerAnswer));

    phFuncTaStop(handlerID);

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateExecGpibQuery, answer ->%s<-, length = %d", handlerAnswer, strlen(handlerAnswer));
    return retVal;
}
#endif
/*****************************************************************************
 * End of file
 *****************************************************************************/
