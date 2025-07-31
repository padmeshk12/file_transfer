/******************************************************************************
 *
 *       (c) Copyright Advantest Ltd., 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 12 Mar 2015
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Magco Li, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 12 Mar 2015, Magco Li, created
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

static char equipId[PHCOM_MAX_MESSAGE_LENGTH] = "";

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

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

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    static char handlerAnswer[PHCOM_MAX_MESSAGE_LENGTH] = "";

    static char handlerAnswer_ca_enq[PHCOM_MAX_MESSAGE_LENGTH] = "";
    static char handlerAnswer_ca_ack[PHCOM_MAX_MESSAGE_LENGTH] = "";
    static char handlerAnswer_ca[PHCOM_MAX_MESSAGE_LENGTH] = "";

    static char handlerAnswer_ce_enq[PHCOM_MAX_MESSAGE_LENGTH] = "";
    static char handlerAnswer_ce_ack[PHCOM_MAX_MESSAGE_LENGTH] = "";
    static char handlerAnswer_ce[PHCOM_MAX_MESSAGE_LENGTH] = "";

    static char receiveCdEnq[PHCOM_MAX_MESSAGE_LENGTH] = "";
    static char previousCdAck[PHCOM_MAX_MESSAGE_LENGTH] = "";
    static char peripheralId[PHCOM_MAX_MESSAGE_LENGTH] = "";

    char site[PHESTATE_MAX_SITES] = "";
    char *answer = NULL;
    char *start = NULL;
    char *end = NULL;

    switch (handlerID->model)
    {
        case PHFUNC_MOD_NY20:
        case PHFUNC_MOD_NX16:
            // request handler ID only do once in the flow
            if (handlerID->p.isCdCalled == 0)
            {
                PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x05));
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", previousCdAck));

                if (previousCdAck[0] == 0x06)//0x06=ack
                {
                    // Request peripheral ID
                    PhFuncTaCheck(phFuncTaSend(handlerID, "%cCD%c", 0x02, 0x03));
                    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", receiveCdEnq));
                }
                else
                {
                    memset(previousCdAck, 0, sizeof(previousCdAck));
                    return PHFUNC_ERR_WAITING;
                }

                if (receiveCdEnq[0] == 0x05)//0x05 = enq
                {
                    PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x06));
                    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", peripheralId));
                }
                else
                {
                    memset(receiveCdEnq, 0, sizeof(receiveCdEnq));
                    return PHFUNC_ERR_WAITING;
                }

                if (strlen(peripheralId) <= 2
                    || peripheralId[0] != 0x02
                    || (peripheralId[0] == 0x02 && peripheralId[strlen(peripheralId)-1] != 0x03) // 0x02 = stx 0x03 = etx
                    )
                {

                    memset(receiveCdEnq,  0, sizeof(receiveCdEnq));
                    memset(previousCdAck, 0, sizeof(previousCdAck));
                    memset(peripheralId,  0, sizeof(peripheralId));
                    return PHFUNC_ERR_WAITING;
                }
                else
                {
                    strcpy(equipId, peripheralId);
                    handlerID->p.isCdCalled = 1;
                }
                handlerID->ta->stepsDone = 0;
            }

            // request error start
            PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x05)); // send enq
            PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer_ca_ack));

            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "Tester receive handler reply before send CA command  \"%s\" ", handlerAnswer_ca_ack);
            if (handlerAnswer_ca_ack[0] == 0x06)// 0x06 = ack
            {
                // Request errors?
                PhFuncTaCheck(phFuncTaSend(handlerID, "%cCA%c", 0x02, 0x03));
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer_ca_enq));

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "Tester receive handler reply after send CA command \"%s\" ", handlerAnswer_ca_enq);
            }
            else
            {
                memset(handlerAnswer_ca_ack, 0, sizeof(handlerAnswer_ca_ack));
                return PHFUNC_ERR_WAITING;
            }

            if (handlerAnswer_ca_enq[0] == 0x05) // 0x05 = enq
            {
                PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x06)); //send ack
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer_ca));

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "Tester receive CA response \"%s\" ", handlerAnswer_ca);
            }
            else
            {
                memset(handlerAnswer_ca_enq, 0, sizeof(handlerAnswer_ca_enq));
                return PHFUNC_ERR_WAITING;
            }

            if (strlen(handlerAnswer_ca) == 2 &&
                handlerAnswer_ca[0] == 0x02 && handlerAnswer_ca[1] == 0x03) // 0x02 = stx 0x03 = etx
            {
                //request start test begin
                PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x05)); // send enq
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer_ce_ack));

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "Tester receive handler reply before send CE command \"%s\" ", handlerAnswer_ce_enq);
            }
            else
            {
                if (handlerAnswer_ca[0] == 0x02 && handlerAnswer_ca[1] == 0x4B) //0x04B = K 0x02 = stx
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                     "Test site EMPTY and no more product available!");
                }
                else if (handlerAnswer_ca[0] == 0x02 && handlerAnswer_ca[1] == 0x4A) //0x4A = J 0x02 = stx
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                     "Device cannot be put in the desired bin!");
                }
                else if (handlerAnswer_ca[0] == 0x02 && handlerAnswer_ca[1] != 0x4A
                    && handlerAnswer_ca[1] != 0x4B && handlerAnswer_ca[1] != 0x03) //0x02 = stx 0x03 = etx
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                                     "others error!");
                }

                handlerID->ta->stepsDone = 0;
                memset(handlerAnswer_ca_enq, 0, sizeof(handlerAnswer_ca_enq));
                memset(handlerAnswer_ca_ack, 0, sizeof(handlerAnswer_ca_ack));
                memset(handlerAnswer_ca,     0, sizeof(handlerAnswer_ca));
                if (handlerID->p.caAbort == 1)
                {
                    return PHFUNC_ERR_ABORTED;
                }
                else
                {
                    return PHFUNC_ERR_WAITING;
                }
            }

            if (handlerAnswer_ce_ack[0] == 0x06) // 0x06 = ack
            {
                //send request start test command
                PhFuncTaCheck(phFuncTaSend(handlerID, "%cCE%c", 0x02, 0x03));
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer_ce_enq));

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "Tester receive handler reply after send CE command \"%s\" ", handlerAnswer_ce_enq);
            }
            else
            {
                memset(handlerAnswer_ce_ack, 0, sizeof(handlerAnswer_ce_ack));
                return PHFUNC_ERR_WAITING;
            }

            if (handlerAnswer_ce_enq[0] == 0x05) // 0x05 = enq
            {
                PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x06)); //send ack
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer_ce));

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "Tester receive CE response \"%s\" ", handlerAnswer_ce);
            }
            else
            {
                memset(handlerAnswer_ce_enq, 0, sizeof(handlerAnswer_ce_enq));
                return PHFUNC_ERR_WAITING;
            }

            //check test sites information
            if (strlen(handlerAnswer_ce) <= 2
                || handlerAnswer_ce[0] != 0x02
                || (handlerAnswer_ce[0] == 0x02 && handlerAnswer_ce[strlen(handlerAnswer_ce)-1] != 0x03) // 0x02 = stx 0x03 = etx
                )
            {
                handlerID->ta->stepsDone = 0;
                memset(handlerAnswer_ca_enq, 0, sizeof(handlerAnswer_ca_enq));
                memset(handlerAnswer_ca_ack, 0, sizeof(handlerAnswer_ca_ack));
                memset(handlerAnswer_ca,     0, sizeof(handlerAnswer_ca));
                memset(handlerAnswer_ce_enq, 0, sizeof(handlerAnswer_ce_enq));
                memset(handlerAnswer_ce_ack, 0, sizeof(handlerAnswer_ce_ack));
                memset(handlerAnswer_ce,     0, sizeof(handlerAnswer_ce));
                return PHFUNC_ERR_WAITING;
            }
            else if (strlen(handlerAnswer_ce) > (handlerID->noOfSites + 3  + 2)) //<STX>1,2,3,4<ETX> length is 4+3+2
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                                 "The handler seems to present more devices than configured\n"
                                 "The driver configuration must be changed to support more sites");
                return PHFUNC_ERR_FATAL;
            }
            else
            {
                //get test sites information
                strcpy(handlerAnswer, handlerAnswer_ce);
            }
            break;
        default:
            break;
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "poll parts devices is \"%s\" (polled)", handlerAnswer);
    /* parse the population site */
    answer = handlerAnswer;
    answer++;
    //as customer maybe change the site number in production, we need to reinit the devicePending value
    for (int i=0; i<handlerID->noOfSites; i++)
    {
        handlerID->p.devicePending[i] = 0;
    }
    do{
        start = answer;
        end = strchr(answer, ',');

        if(end != NULL)
        {
            answer = end + 1;
            end[0] = '\0';
        }

        /* get the population site */
        sscanf(start, "%100[1-9]", site);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "device present at site \"%d\"", atoi(site));

        handlerID->p.devicePending[atoi(site)-1] = 1;
        handlerID->p.oredDevicePending = 1;
    }while(end != NULL && answer[0] != '\0');

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
static phFuncError_t setupBinCode(
    phFuncId_t handlerID     /* driver plugin ID */,
    long binNumber            /* for bin number   handlerbinindex if define mapped_hardbin */,
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
            sprintf(binCode,"%s",handlerID->binIds[binNumber]);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "using binIds set binNumber %d to binCode \"%s\" ",
                             binNumber, binCode);
        }
    }
    else
    {
        if (binNumber < 0 || binNumber > 31)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                "received illegal request for bin %lx ",
                binNumber+1);
            retVal=PHFUNC_ERR_BINNING;
        }
        else
        {
            sprintf(binCode,"%lx",binNumber);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "using binNumber set binNumber %d to binCode \"%s\" ",
                             binNumber, binCode);
        }
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

    static char handlerAnswer[PHESTATE_MAX_SITES] = "";
    char binningCommand[PHESTATE_MAX_SITES] = "";
    char binningSendCommand[PHESTATE_MAX_SITES] = "";
    char tmpCommand[PHESTATE_MAX_SITES] = "";
    char thisbin[PHESTATE_MAX_SITES];
    int i = 0;
    int sendBinning = 0;
    int tmpbin = 0;

    phFuncTaMarkStep(handlerID);

    switch (handlerID->model)
    {
        case PHFUNC_MOD_NY20:
        case PHFUNC_MOD_NX16:
          PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x05));
          PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer));
          if (handlerAnswer[0] == 0x06)//0x06=ack
          {
              sprintf(binningCommand,"%c%s", 0x02, "BA");

              for(i=0; i<handlerID->noOfSites; i++)
              {
                  if(handlerID->activeSites[i] &&
                     (oldPopulation[i]==PHESTATE_SITE_POPULATED||
                      oldPopulation[i]==PHESTATE_SITE_POPDEACT))
                  {
                      if(perSiteReprobe&&perSiteReprobe[i])
                      {
                          if(!handlerID->p.reprobeBinDefined)
                          {
                              phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_ERROR,
                                            "Site \"%s\" needs reprobe while reprobeBinDefined is not set, please check key values of \"%s\" and \"%s\"",
                              handlerID->siteIds[i], PHKEY_RP_AMODE, PHKEY_RP_BIN);
                              return PHFUNC_ERR_BINNING;
                          }
                          else
                          {
                              /* call Reprobe*/
                              sprintf(tmpCommand,"%s,0,%d;",handlerID->siteIds[i],handlerID->p.reprobeBinNumber);
                              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                        "Site \"%s\" needs reprobe",
                                        handlerID->siteIds[i]);
                              strcat(binningCommand,tmpCommand);
                          }
                      }
                      else  /* do not need reprobe*/
                      {
                          retVal=setupBinCode(handlerID, perSiteBinMap[i], thisbin);
                          tmpbin=atoi(thisbin);
                          if(tmpbin>31 || tmpbin <0)
                          {
                              phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_FATAL,
                              "receive illegal request for bin %ld at site \"%s\"",
                              perSiteBinMap[i],handlerID->siteIds[i]);
                              return PHFUNC_ERR_BINNING;
                          }
                          if(retVal==PHFUNC_ERR_OK)
                          {
                              if(thisbin[0]!='\0')
                              {
                                  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                    "will bin device at site \"%s\" to %s",
                                    handlerID->siteIds[i], thisbin);
                                    sprintf(tmpCommand,"%s,0,%s;",handlerID->siteIds[i],thisbin);
                              }
                              else
                              {
                                  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                    "will bin device at site \"%s\" to %d",
                                    handlerID->siteIds[i], perSiteBinMap[i]);
                                    sprintf(tmpCommand,"%s,0,%ld;",handlerID->siteIds[i],perSiteBinMap[i]);
                              }
                              strcat(binningCommand, tmpCommand);
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
                  else  /* no device at site i*/
                  {
                      phLogFuncMessage(handlerID->myLogger,LOG_DEBUG,
                        "no device to reprobe or bin at site \"%s\"",
                        handlerID->siteIds[i]);
                  }
              }
          }
          else
          {
              return PHFUNC_ERR_WAITING;
          }
        break;
     default:
        break;
    }
    if ( retVal==PHFUNC_ERR_OK && sendBinning==1)
    {
        strncpy(binningSendCommand,binningCommand, strlen(binningCommand) - 1);
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s%c",
                                   binningSendCommand, 0x03));
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "send command \"%s\"",
                         binningSendCommand);
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

    /* Ismeca now only supports polling since it's based on rs232 */
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
                         "Ismeca doesn't support interrupt mode, the interrupt setting is ignored");
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

    /* retrieve ca command abort setting */
    confError = phConfConfStrTest(&found, handlerID->myConf,
        PHKEY_NAME_ISMECA_HANDLER_CA_ABORT, "yes", "no", NULL);
    if(confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
    {
        retVal = PHFUNC_ERR_CONFIG;
    }
    else
    {
        if(found == 1)
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                            "the ca commmand abort is set to yes");
            handlerID->p.caAbort = 1;
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
 * Authors: Magco Li
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
    handlerID->p.caAbort = 0;
    handlerID->p.isCdCalled = 0;

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
 * Authors: Magco Li
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
 * Authors: Magco Li
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
 * Authors: Magco Li
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
 * Authors: Magco Li
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateEquipID(phFuncId_t handlerID /* driver plugin ID */,
                             char **equipIdString /* resulting pointer to equipment ID string */)
{

    phFuncError_t retVal = PHFUNC_ERR_OK;

    if (strcmp(equipId, "") != 0)
    {
        *equipIdString = equipId;
    }
    else
    {
        static char cdAck[PHCOM_MAX_MESSAGE_LENGTH] = "";
        static char cdEnq[PHCOM_MAX_MESSAGE_LENGTH] = "";
        static char cdId[PHCOM_MAX_MESSAGE_LENGTH] = "";
        cdId[0] = '\0';
        *equipIdString = cdId;

        PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x05));
        PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", cdAck));
        if (cdAck[0] == 0x06)//0x06=ack
        {
            // Request peripheral ID
            PhFuncTaCheck(phFuncTaSend(handlerID, "%cCD%c", 0x02, 0x03));
            PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", cdEnq));
        }
        else
        {
            memset(cdAck, 0, sizeof(cdAck));
            return PHFUNC_ERR_WAITING;
        }

        if (cdEnq[0] == 0x05)//0x05 = enq
        {
            PhFuncTaCheck(phFuncTaSend(handlerID, "%c", 0x06));
            PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%511[^\n\r]", cdId));
        }
        else
        {
            memset(cdEnq, 0, sizeof(cdEnq));
            return PHFUNC_ERR_WAITING;
        }

        if (strlen(cdId) < 2
            || (strlen(cdId) == 2 && cdId[0] == 0x02)
            || cdId[0] != 0x02
            || (cdId[0] == 0x02 && cdId[strlen(cdId)-1] != 0x03) // 0x02 = stx 0x03 = etx
            )
        {
            handlerID->ta->stepsDone = 0;
            memset(cdEnq, 0, sizeof(cdEnq));
            memset(cdAck, 0, sizeof(cdAck));
            memset(cdId,  0, sizeof(cdId));
            return PHFUNC_ERR_WAITING;
        }

        handlerID->ta->stepsDone = 0;
    }

    return retVal;
}
#endif

#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next device
 *
 * Authors: Magco Li
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateGetStart(phFuncId_t handlerID /* driver plugin ID */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

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
 * Authors: Magco Li
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
 * Authors: Magco Li
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
 * Authors: Magco Li
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
 * Authors: Magco Li
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
 * Authors: Magco Li
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
 * Authors: Magco Li
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDiag(phFuncId_t handlerID /* driver plugin ID */,
                          char **diag /* pointer to handlers diagnostic output */)
{
    ReturnNotYetImplemented("privateDiag");
}
#endif

#ifdef STATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Magco Li
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
 * Authors: Magco Li
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
 * Authors: Magco Li
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
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
 * Authors: Magco Li
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
 * Authors: Magco Li
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

#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The stub function for set status
 *
 * Authors: Magco Li
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
    ReturnNotYetImplemented("privateSetStatus");
}
#endif

/*****************************************************************************
 * End of file
 *****************************************************************************/
