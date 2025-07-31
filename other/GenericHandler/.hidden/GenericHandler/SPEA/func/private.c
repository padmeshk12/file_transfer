/******************************************************************************
 *
 *       (c) Copyright Advantest Ltd., 2014
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 1 Sept 2014
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Xiaofei Han, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 1 Sept 2014, Xiaofei Han, created
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
#include "private.h"
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

#define NO_BINNING_RESULT "0"
/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static void setStrUpper(char *s1)
{
    int i = 0;

    while (s1[i])
    {
        s1[i] = toupper(s1[i]);
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
    if (select(0, NULL, NULL, NULL, &delay) == -1 && errno == EINTR)
        return 0;
    else
        return 1;
}
static phFuncError_t handleQueryAnswer(phFuncId_t handlerID, char* queryAnswer)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char siteInfo[2048] = "";
    char *siteToken = NULL;
    int siteCount = 0;

    switch (handlerID->model)
    {
        case PHFUNC_MOD_H3570:

            /* ensure string is in upper case */
            setStrUpper(queryAnswer);

            if (!strcasecmp(queryAnswer, "NO ACTION") || !strcasecmp(queryAnswer, "BUSY"))
            {
                //receive the NO ACTION or BUSY response, remove send/receive 2 steps
                //so that we can continue querying
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_4,
                        "%s received from the handler. Continue querying. \n",
                        queryAnswer);
                phFuncTaRemoveStep(handlerID);
                phFuncTaRemoveStep(handlerID);
                handlerID->p.oredDevicePending = 0;
                retVal = PHFUNC_ERR_WAITING;
            }
            else if (!strcasecmp(queryAnswer, "START_LOT"))
            {
                //received the START_LOT message
                if (handlerID->p.lotStarted == 0)
                {
                    handlerID->p.lotStarted = 1;
                }
                else
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                            "Repeated START_LOT (no END_LOT yet) is received from the handler. Continue querying.\n");
                }
                phFuncTaRemoveStep(handlerID);
                phFuncTaRemoveStep(handlerID);
                handlerID->p.oredDevicePending = 0;
                retVal = PHFUNC_ERR_WAITING;
            }
            else if (!strcasecmp(queryAnswer, "END_LOT"))
            {
                //received the END_LOT message
                if (handlerID->p.lotStarted == 1)
                {
                    handlerID->p.lotStarted = 0;
                }
                else
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                            "Lot is not started but END_LOT is received from the handler.\n");
                }

                //end the lot
                handlerID->p.oredDevicePending = 0;
                retVal = PHFUNC_ERR_LOT_DONE;
            }
            else if (strstr(queryAnswer, "LOT_INFO ") != NULL )
            {
                //received the LOT_INFO
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_4,
                        "%s received from the handler. Continue querying. \n",
                        queryAnswer);
                phFuncTaRemoveStep(handlerID);
                phFuncTaRemoveStep(handlerID);
                handlerID->p.oredDevicePending = 0;
                retVal = PHFUNC_ERR_WAITING;
            }
            else if (strstr(queryAnswer, "TEST ") != NULL )
            {
                //receive the TEST (site setup) message
                if (sscanf(queryAnswer, "TEST %s", siteInfo) != 1)
                {
                    //incorrect site setup command format
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                            "Incorrect site population received from handler: \n"
                                    "received \"%s\" format expected \"TEST x,x,x,x,x,x,x,x\" \n"
                                    "trying again", queryAnswer);
                    phFuncTaRemoveStep(handlerID);
                    phFuncTaRemoveStep(handlerID);
                    handlerID->p.oredDevicePending = 0;
                    retVal = PHFUNC_ERR_WAITING;
                }
                else
                {
                    //the TEST command is in format: TEST 1,1,1,1,0,0,0,1,1,...
                    handlerID->p.oredDevicePending = 0;
                    siteCount = 0;
                    siteToken = strtok(siteInfo, ",");
                    while (siteToken != NULL )
                    {
                        if (atoi(siteToken) == 1)
                        {
                            handlerID->p.devicePending[siteCount] = 1;
                            handlerID->p.oredDevicePending = 1;
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                    "device present at site \"%s\" (polled)",
                                    handlerID->siteIds[siteCount]);
                            siteCount++;
                        }
                        else
                        {
                            handlerID->p.devicePending[siteCount] = 0;
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                    "no device at site \"%s\" (polled)",
                                    handlerID->siteIds[siteCount]);
                            siteCount++;
                        }

                        siteToken = strtok(NULL, ",");
                    }

                    if (siteCount > handlerID->noOfSites)
                    {
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                         "The handler seems to present more devices than configured\n"
                                         "The driver configuration must be changed to support more sites");
                    }
                }
            }
            else
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                        "Unknown answer returned from handler: %s, will send QUERY again",
                        queryAnswer);
                phFuncTaRemoveStep(handlerID);
                phFuncTaRemoveStep(handlerID);
                handlerID->p.oredDevicePending = 0;
                retVal = PHFUNC_ERR_WAITING;
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

    static char handlerAnswer[2048] = "";

    switch (handlerID->model)
    {
        case PHFUNC_MOD_H3570:

            PhFuncTaCheck(phFuncTaSend(handlerID, "QUERY%s", handlerID->p.eol));
            PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer));
            return(handleQueryAnswer(handlerID, handlerAnswer));
            break;
        default:
            break;
    }

    return retVal;
}

/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
    struct timeval pollStartTime;
    struct timeval pollStopTime;
    int timeout;

    phFuncError_t retVal = PHFUNC_ERR_OK;

    if (handlerID->p.strictPolling)
    {
        /* apply strict polling loop, ask for site population */
        gettimeofday(&pollStartTime, NULL );

        timeout = 0;
        localDelay_us(handlerID->p.pollingInterval);
        do
        {
            PhFuncTaCheck(pollParts(handlerID));
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
static phFuncError_t setupBinCode(phFuncId_t handlerID /* driver plugin ID */,
                                  long binNumber       /* for bin number */,
                                  char *binCode        /* array to setup bin code */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

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

    return retVal;
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
    char tmpCommand[256] = "";
    static char response[256] = "";
    char thisbin[32];
    int siteCount = 0;
    int sendBinning = 0;

    phFuncTaMarkStep(handlerID);

    switch (handlerID->model)
    {
        case PHFUNC_MOD_H3570:

            strcpy(testresults, "RESULT ");

            for (siteCount = 0;
                 siteCount < handlerID->noOfSites && siteCount < PHESTATE_MAX_SITES && retVal == PHFUNC_ERR_OK;
                 siteCount++)
            {
                if (handlerID->activeSites[siteCount] &&
                    (oldPopulation[siteCount] == PHESTATE_SITE_POPULATED || oldPopulation[siteCount] == PHESTATE_SITE_POPDEACT))
                {
                    /* there is a device here */
                    if (perSiteReprobe && perSiteReprobe[siteCount])
                    {
                        if (!handlerID->p.reprobeBinDefined)
                        {
                            /* no reprobe bin number defined */
                            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                                             "received reprobe command at site \"%s\" but no reprobe bin number defined \n"
                                             "please check the configuration values of \"%s\" and \"%s\"",
                                             handlerID->siteIds[siteCount],
                                             PHKEY_RP_AMODE, PHKEY_RP_BIN);
                            retVal = PHFUNC_ERR_BINNING;
                        }
                        else
                        {
                            /* reprobe this one */
                            sprintf(tmpCommand, "%d", handlerID->p.reprobeBinNumber);
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                             "will reprobe device at site \"%s\"",
                                             handlerID->siteIds[siteCount]);
                            strcat(testresults, tmpCommand);
                            sendBinning = 1;
                        }
                    }
                    else
                    {
                        retVal = setupBinCode(handlerID, perSiteBinMap[siteCount], thisbin);
                        if (retVal == PHFUNC_ERR_OK)
                        {
                            strcat(testresults, thisbin);
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                             "will bin device at site \"%s\" to %s",
                                             handlerID->siteIds[siteCount], thisbin);
                            sendBinning = 1;
                        }
                        else
                        {
                            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                             "unable to send binning at site \"%s\"",
                                             handlerID->siteIds[siteCount]);
                            sendBinning = 0;
                        }
                    }
                }
                else
                {
                    /* there's no device here */
                    strcat(testresults, NO_BINNING_RESULT);
                    if (siteCount < handlerID->noOfSites)
                    {
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                         "no device to bin at site \"%s\", use %s as bin ID",
                                         handlerID->siteIds[siteCount], NO_BINNING_RESULT);
                    }
                }

                if (siteCount != handlerID->noOfSites-1)
                {
                    strcat(testresults, ",");
                }
            }

            if (sendBinning)
            {
                PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", testresults, handlerID->p.eol));
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", response));

                if(strcasecmp(response, "ACK"))
                {
                    retVal = PHFUNC_ERR_WAITING;
                    phFuncTaRemoveStep(handlerID);
                    phFuncTaRemoveStep(handlerID);
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                                     "Expecting ACK but the response \"%s\" is received for the RESULT command, try again",
                                     response);
                }
            }
            break;
        default:
            break;
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
    double dReprobeBinNumber;

    /* SPEA now only supports polling since it's based on LAN */
    phConfConfStrTest(&found, handlerID->myConf, PHKEY_FC_WFPMODE, "polling", "interrupt", NULL );
    if (found == 1)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "activated strict polling mode");
        handlerID->p.strictPolling = 1;
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "SPEA doesn't support interrupt mode, the interrupt setting is ignored");
    }

    /* retrieve polling interval */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_FC_POLLT);
    if (confError == PHCONF_ERR_OK)
    {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_POLLT, 0, NULL, &dPollingInterval);
        if (confError == PHCONF_ERR_OK)
        {
            handlerID->p.pollingInterval = labs((long) dPollingInterval);
        }
        else
        {
            handlerID->p.pollingInterval = 200000L; /* default 0.2 sec */
        }
    }

     /* get reprobe bin number */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_RP_BIN);
    if (confError == PHCONF_ERR_OK)
    {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_RP_BIN, 0, NULL, &dReprobeBinNumber);

        if (confError == PHCONF_ERR_OK)
        {
            handlerID->p.reprobeBinDefined = 1;
            handlerID->p.reprobeBinNumber = abs((int) dReprobeBinNumber);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "configuration \"%s\" set: will send BIN %d command upon receiving a reprobe",
                             PHKEY_RP_BIN, handlerID->p.reprobeBinNumber);
        }
        else
        {
            handlerID->p.reprobeBinDefined = 0;
            handlerID->p.reprobeBinNumber = 0;
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "configuration \"%s\" not set: ignore reprobe command",
                             PHKEY_RP_BIN);
        }
    }

    phConfConfStrTest(&found, handlerID->myConf, PHKEY_RP_AMODE, "off", "all", "per-site", NULL );
    if ((found == 2 || found == 3) && !handlerID->p.reprobeBinDefined)
    {
        /* frame work may send reprobe command but a reprobe bin has not been defined */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "a reprobe mode has been defined without its corresponding bin number\n"
                         "please check the configuration values of \"%s\" and \"%s\"",
                         PHKEY_RP_AMODE, PHKEY_RP_BIN);
        retVal = PHFUNC_ERR_CONFIG;
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
phFuncError_t privateInit(phFuncId_t handlerID /* the prepared driver plugin ID */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int i;

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

    handlerID->p.strictPolling = 0;
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
    handlerID->p.status = 0L;
    handlerID->p.paused = 0;

    handlerID->p.lotStarted = 0;

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
 * Authors: Xiaofei Han
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReconfigure(phFuncId_t handlerID  /* driver plugin ID */)
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
 ******************r*********************************************************/
phFuncError_t privateReset(phFuncId_t handlerID /* driver plugin ID */)
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
phFuncError_t privateDriverID(phFuncId_t handlerID /* driver plugin ID */,
                              char **driverIdString /* resulting pointer to driver ID string */)
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
phFuncError_t privateEquipID(phFuncId_t handlerID /* driver plugin ID */,
                             char **equipIdString /* resulting pointer to equipment ID string */)
{
    return PHFUNC_ERR_OK;
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
phFuncError_t privateGetStart(phFuncId_t handlerID /* driver plugin ID */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char handlerAnswer[512] = "";
    int i;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
    {
        return PHFUNC_ERR_ABORTED;
    }

    /* remember which devices we expect now */
    for (i = 0; i < handlerID->noOfSites; i++)
        handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

    phFuncTaStart(handlerID);

    phFuncTaMarkStep(handlerID);

    PhFuncTaCheck(waitForParts(handlerID));

    /* do we have at least one part? If not, ask for the current
     status and return with waiting */
    if (!handlerID->p.oredDevicePending)
    {
        /* during the next call everything up to here should be repeated */
        phFuncTaRemoveToMark(handlerID);
        return PHFUNC_ERR_WAITING;
    }

    /* Send TEST RUN to the handler */
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_4,
                     "Ready for test flow execution, send TEST RUN to the handler");
    PhFuncTaCheck(phFuncTaSend(handlerID, "TEST RUN%s", handlerID->p.eol));
    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer));

    if(strcasecmp(handlerAnswer, "ACK"))
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "received unexpected response \"%s\", try to send again.",
                         handlerAnswer);
        phFuncTaRemoveStep(handlerID);
        phFuncTaRemoveStep(handlerID);
        return PHFUNC_ERR_WAITING;
    }

    phFuncTaStop(handlerID);

    /* we have received devices for test. Change the equipment
     specific state now */
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
phFuncError_t privateBinDevice(phFuncId_t handlerID /* driver plugin ID */,
                               long *perSiteBinMap /* binning information */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

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

    /* modify site population, everything went fine, otherwise we
     would not reach this point */
    for (i = 0; i < handlerID->noOfSites; i++)
    {
        if (handlerID->activeSites[i]
                && (oldPopulation[i] == PHESTATE_SITE_POPULATED
                        || oldPopulation[i] == PHESTATE_SITE_POPDEACT))
        {
            population[i] =
                    oldPopulation[i] == PHESTATE_SITE_POPULATED ?
                            PHESTATE_SITE_EMPTY : PHESTATE_SITE_DEACTIVATED;
        }
        else
            population[i] = oldPopulation[i];
    }

    /* change site population */
    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);

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
phFuncError_t privateReprobe(phFuncId_t handlerID /* driver plugin ID */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    long perSiteReprobe[PHESTATE_MAX_SITES];
    long perSiteBinMap[PHESTATE_MAX_SITES];
    int i = 0;

    if (!handlerID->p.reprobeBinDefined)
    {
        /* no reprobe bin number defined */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                         "received reprobe command at site \"%s\" but no reprobe bin number defined \n"
                         "please check the configuration values of \"%s\" and \"%s\"",
                         handlerID->siteIds[i], PHKEY_RP_AMODE, PHKEY_RP_BIN);
        retVal = PHFUNC_ERR_BINNING;
    }
    else
    {
        /* prepare to reprobe everything */
        for (i = 0; i < handlerID->noOfSites; i++)
        {
            perSiteReprobe[i] = 1L;
            perSiteBinMap[i] = handlerID->p.reprobeBinNumber;
        }
        retVal = privateBinReprobe(handlerID, perSiteReprobe, perSiteBinMap);
    }

    return retVal;
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
phFuncError_t privateBinReprobe(phFuncId_t handlerID /* driver plugin ID */,
                                long *perSiteReprobe /* TRUE, if a device needs reprobe*/,
                                long *perSiteBinMap /* valid binning data for each site where
                                                       the above reprobe flag is not set */)
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
    for (i = 0; i < handlerID->noOfSites; i++)
        handlerID->p.deviceExpected[i] = (handlerID->activeSites[i] && perSiteReprobe[i]);

    /* get current site population */
    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

    phFuncTaStart(handlerID);

    PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation, perSiteReprobe, perSiteBinMap));

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
    for (i = 0; i < handlerID->noOfSites; i++)
    {
        if (handlerID->activeSites[i] == 1)
        {
            if (perSiteReprobe[i])
            {
                if (handlerID->p.devicePending[i])
                {
                    if (oldPopulation[i] == PHESTATE_SITE_POPULATED || oldPopulation[i] == PHESTATE_SITE_POPDEACT)
                    {
                        handlerID->p.devicePending[i] = 0;
                    }
                    else
                    {
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                         "new device at site \"%s\" will not be tested during reprobe action (delayed)",
                                         handlerID->siteIds[i]);
                    }
                    population[i] = oldPopulation[i];
                }
                else
                {
                    if (oldPopulation[i] == PHESTATE_SITE_POPULATED || oldPopulation[i] == PHESTATE_SITE_POPDEACT)
                    {
                        /* something is wrong here, we expected a reprobed device */
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                         "did not receive device that was scheduled for reprobe at site \"%s\"",
                                         handlerID->siteIds[i]);
                        population[i] = PHESTATE_SITE_EMPTY;
                    }
                    else
                    {
                        population[i] = oldPopulation[i];
                    }
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
                if (oldPopulation[i] == PHESTATE_SITE_POPDEACT || oldPopulation[i] == PHESTATE_SITE_DEACTIVATED)
                {
                    population[i] = PHESTATE_SITE_DEACTIVATED;
                }
            }
        }
        else
        {
            if (handlerID->p.devicePending[i])
            {
                /* something is wrong here, we got a device for an inactive site */
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
phFuncError_t privateSTPaused(phFuncId_t handlerID /* driver plugin ID */)
{
    ReturnNotYetImplemented("privateSTPaused");
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
phFuncError_t privateSTUnpaused(phFuncId_t handlerID /* driver plugin ID */)
{
    ReturnNotYetImplemented("privateSTUnpaused");
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
phFuncError_t privateDiag(phFuncId_t handlerID /* driver plugin ID */,
                          char **diag /* pointer to handlers diagnostic output */)
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
phFuncError_t privateStatus(phFuncId_t handlerID /* driver plugin ID */,
                            phFuncStatRequest_t action /* the current status action to perform */,
                            phFuncAvailability_t *lastCall /* the last call to the plugin, not
                                                            counting calls to this function */)
{
    ReturnNotYetImplemented("privateStatus");
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
phFuncError_t privateUpdateState(phFuncId_t handlerID /* driver plugin ID */)
{
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
phFuncError_t privateCommTest(phFuncId_t handlerID /* driver plugin ID */,
                              int *testPassed /* whether the communication
                                                 test has passed or failed */)
{
    ReturnNotYetImplemented("privateCommTest");
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
phFuncError_t privateDestroy(phFuncId_t handlerID /* driver plugin ID */)
{
    ReturnNotYetImplemented("privateDestroy");
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
phFuncError_t privateGetStatus(phFuncId_t handlerID, /* driver plugin ID */
                               const char *commandString, /* the string of command, i.e.
                                                             the key to get the information from Handler */
                               const char *paramString, /* the parameter for command string */
                               char **responseString /* output of the response string */)
{
    ReturnNotYetImplemented("privateGetStatus");
}
#endif

/*****************************************************************************
 *
 * for a certain SetStatus, get the corresponding command
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
static int getCommandForSetStatusQuery(phFuncId_t handlerID,
                                       char **pGpibCommand,
                                       const char *token,
                                       const char *param)
{
    /* these static array must be ordered by the first field */
    static const  phStringPair_t sCommands[] =
    {
        {PHKEY_NAME_HANDLER_STATUS_SET_SABON,"SOCKET_AIR_BLOW_ON"},
        {PHKEY_NAME_HANDLER_STATUS_SET_SABOFF,"SOCKET_AIR_BLOW_OFF"}
    };
    static char buffer[MAX_STATUS_MSG] = "";
    int retVal = SUCCEED;
    const phStringPair_t *pStrPair = NULL;
    int paramSpecified = NO;
    int ignoreParam = NO;

    if (strlen(param) > 0 )
    {
        paramSpecified = YES;
        if (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_SABON) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_SABOFF) != 0 )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                            "the status setting does not require any parameters!\n"
                            "Ignore the parameter %s.",
                            param);
            ignoreParam = YES;
            retVal = SUCCEED;
        }
    }

    if (retVal == SUCCEED)
    {
        pStrPair = phBinSearchStrValueByStrKey(sCommands, LENGTH_OF_ARRAY(sCommands), token);
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
phFuncError_t privateSetStatus(phFuncId_t handlerID, /* driver plugin ID */
                               const char *token, /* the string of command, i.e.
                                                     the key to get the information from Handler */
                               const char *param /* the parameter for command string */)
{
    static char response[MAX_STATUS_MSG] = "";
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char *command = NULL;
    int found = FAIL;

    if ( phFuncTaAskAbort(handlerID) )
    {
        return PHFUNC_ERR_ABORTED;
    }

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "privateSetStatus, token = ->%s<-, param = ->%s<-", token, param);

    found = getCommandForSetStatusQuery(handlerID, &command, token, param);
    if ( found == SUCCEED )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "privateSetStatus, gpibCommand = ->%s<-", command);
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", command, handlerID->p.eol));
        localDelay_us(100000);
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%4095[^\r\n]", response));
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "The key \"%s\" is not available, or may not be supported yet", token);
        sprintf(response, "%s_KEY_NOT_AVAILABLE", token);
    }


    /* ensure null terminated */
    response[strlen(response)] = '\0';

    if(strcmp(response, "ACK"))
    {
        /* command is not executed successfully */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "The handler didn't reply \"ACK\" ");
        retVal = PHFUNC_ERR_ANSWER;
    }

    phFuncTaStop(handlerID);

    return retVal;
}
#endif

/*****************************************************************************
 * End of file
 *****************************************************************************/
