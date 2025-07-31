/******************************************************************************
 *
 *       (c) Copyright Advantest, 2014
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 16 Dec 2014
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 16 Dec 2014, Magco Li , created
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

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_hfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "ph_hfunc_private.h"


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

#define LEN_OF_DEVICE_ID_PER_SITE  50
#define LEN_OF_STRIP_ID   200

char stripID[LEN_OF_STRIP_ID];
char deviceIDs[PHESTATE_MAX_SITES][LEN_OF_DEVICE_ID_PER_SITE];
char deviceIDStringFromHandler[PHESTATE_MAX_SITES*LEN_OF_DEVICE_ID_PER_SITE];

struct SingleSiteXY
{
  char x[LEN_OF_DEVICE_ID_PER_SITE];
  char y[LEN_OF_DEVICE_ID_PER_SITE];
};

struct  SingleSiteXY coordinates[PHESTATE_MAX_SITES];

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/*
 * A:1 , B:2 ~Z:26
 * AA=26*1(A)+1(A)=27
 * AB=26*1(A)+2(B)=28
 * BC=26*2(B)+3(C)=55
 * ZZ=26*26+26=...
 */
int computeX(char *x_coordinate)
{
  int i = 0;
  int rtnValue = 0;

  if(strlen(x_coordinate) > 2)
  {
      return -1;
  }
  
  if(x_coordinate == NULL || x_coordinate[0] == '\0')
  {
      return 0;
  }

  rtnValue = (strlen(x_coordinate) == 1)?
             (toupper(x_coordinate[0]) - 'A' + 1):
             26* (toupper(x_coordinate[0]) - 'A' + 1);

  for(i = 1; i<strlen(x_coordinate) ;i++)
  {
      rtnValue = rtnValue + toupper(x_coordinate[i]) - 'A' + 1;
  }

  return rtnValue;
}

static int localDelay_us(long microseconds)
{
    long seconds;
    struct timeval delay;

    /* us must be converted to seconds and microseconds ** for use by select(2) */
    if(microseconds >= 1000000L)
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
    if(select(0, NULL, NULL, NULL, &delay) == -1 &&
       errno == EINTR)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/* perSiteBinMap[] stores the handler bin index for each site , we need to using binIds[] to get their bin-name respectively */
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
            sprintf(binCode,"%s",handlerID->binIds[ binNumber ]);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "using binIds set binNumber %d to binCode \"%s\" ",
                             binNumber, binCode);
        }
    }
    else
    {
        if (binNumber < 0 || binNumber > 99999)
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
/* end of finding bin-name*/

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    static char response[MAX_STATUS_MSG] = "";
    char answer[MAX_STATUS_MSG] = "";

    int i = 0;
    int siteCount = 0;
    int i_pos = 0;

    char deviceIDString[PHESTATE_MAX_SITES*LEN_OF_DEVICE_ID_PER_SITE] = "";
    char temp1[MAX_STATUS_MSG] = "";
    char temp2[MAX_STATUS_MSG] = "";
    char *start = NULL;
    char *end = NULL;
    char *tempDeviceIDString = NULL;

    switch(handlerID->model)
    {
        case PHFUNC_MOD_FH_1200:
          for(i=0;i<handlerID->noOfSites;i++)
          {
              handlerID->p.devicePending[i]=0;
          }

          PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%4096[^\r\n]", response));
          PhFuncTaCheck(phFuncTaClearGpib(handlerID));
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                           "Received message \"%s\" (polled)",
                           response);
          break;

        default:
          break;
    }

    strcpy(answer, response);

    if(strcmp(answer, "OK") == 0)
    {
        return PHFUNC_ERR_OK;
    }

    if(strcmp(answer, "Set PanelComplete") == 0)
    {
        if(1 == handlerID->p.stripCalled)
        {
            /*when we received SetPanelComplete in strip start process, postpone to privateGetStart*/
            handlerID->p.stripSetPCCalled = 1;
        }
        else
        {
            /* will set the level end flag even if the privateStripStart is not be called */
            phTcomSetSystemFlag(handlerID->myTcom, PHTCOM_SF_CI_LEVEL_END, 1L);
        }
        handlerID->p.stripDone = 1;
        phFuncTaRemoveToMark(handlerID);
        phFuncTaStop(handlerID);
        return PHFUNC_ERR_OK;
    }

    /* get the strip id and device id from the start test message */
    if(sscanf(answer, "%s%s%s%s", temp1, temp2, stripID, deviceIDString) != 4 ||
       strcmp(temp1, "Set") !=0 ||
       strcmp(temp2, "StartTest") != 0)
    {
        phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_WARNING,
                         "Unrecognized message from handler \"%s\"; ignored ", answer);
        phFuncTaRemoveStep(handlerID);
        phFuncTaRemoveStep(handlerID);
        phFuncTaRemoveStep(handlerID);
        return PHFUNC_ERR_WAITING;
    }

    phTrimLeadingTrailingDelimiter(stripID, "\"");
    strcpy(deviceIDStringFromHandler, deviceIDString);

    phLogFuncMessage(handlerID->myLogger,LOG_DEBUG,
                     "Got Strip ID: %s; Device IDs: %s ", stripID, deviceIDString);

    /* parse the device ID string */
    tempDeviceIDString = deviceIDString;
    do
    {
        start = tempDeviceIDString;
        end = strchr(tempDeviceIDString, ',');

        if(end != NULL)
        {
            tempDeviceIDString = end + 1;
            end[0] = '\0';
        }

        if(start == end)
        {
            phLogFuncMessage(handlerID->myLogger,LOG_DEBUG,
                             "No device present at site %d",
                             siteCount+1);
            handlerID->p.devicePending[siteCount] = 0;
            siteCount++;
            continue;
        }

        /* save the device id for site */
        strncpy(deviceIDs[siteCount], start, LEN_OF_DEVICE_ID_PER_SITE-1);

        /* save the x/y coordiante */
        sscanf(start, "%100[A-Za-z]", coordinates[siteCount].x);
        sscanf(start+strlen(coordinates[siteCount].x), "%100[0-9]", coordinates[siteCount].y);

        handlerID->p.devicePending[siteCount] = 1;
        handlerID->p.oredDevicePending = 1;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "device present at site \"%d\", coordinates = (%s,%s)",
                         siteCount+1,
                         coordinates[siteCount].x,
                         coordinates[siteCount].y);

        i_pos = computeX(coordinates[siteCount].x);
        if (-1 == i_pos)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "Received invalid coordinates, please check the handler status.");
            retVal = PHFUNC_ERR_FATAL;
        }

        phTcomSetDiePosXYOfSite(handlerID->myTcom, siteCount+1, i_pos, atoi(coordinates[siteCount].y));

        siteCount++;

    } while(end != NULL && tempDeviceIDString[0] != '\0');

    handlerID->p.deviceAvailable = siteCount;
    /* if current sites dismatch the number in configure, gives the warning */
    if(siteCount<handlerID->noOfSites)
    {
         phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_WARNING,
                          "The number of DUT is less than that in configure file");
    }

    /* check whether we have received too many parts */
    if (siteCount > handlerID->noOfSites)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "The handler seems to present more devices than configured\n"
                         "The driver configuration must be changed to support more sites");
        retVal = PHFUNC_ERR_FATAL;
    }

    return retVal;
}
/* end of poll */

static phFuncError_t waitForParts(phFuncId_t handlerID)
{
    static int srq = 0;
    static int received = 0;

    phFuncError_t retVal = PHFUNC_ERR_OK;
    /* try to receive a start SRQ */
    PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq,
        handlerID->p.oredDevicePending));
    if (received)
    {
        switch (handlerID->model)
          {
            case PHFUNC_MOD_FH_1200:
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                "Test Request has been sent from handler, need to poll ");
              PhFuncTaCheck(pollParts(handlerID));
              break;
            default:
              break;
         }
    }

    return retVal;
}

/* wait for strip start from handler */
static phFuncError_t waitForStripStart(phFuncId_t handlerID)
{
    int i = 0;
    phFuncError_t retVal = PHFUNC_ERR_OK;

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "Entering waitForStripStart\n");
    switch ( handlerID->model )
    {
        case PHFUNC_MOD_FH_1200:
            /* abort in case of unsuccessful retry */
            if (phFuncTaAskAbort(handlerID))
               return PHFUNC_ERR_ABORTED;

            /* remember which devices we expect now */
            for (i=0; i<handlerID->noOfSites; i++){
               handlerID->p.deviceExpected[i] = handlerID->activeSites[i];
            }

            phFuncTaStart(handlerID);
            phFuncTaMarkStep(handlerID);
            PhFuncTaCheck(waitForParts(handlerID));

            /* do we have at least one part? If not, ask for the current
              status and return with waiting */
            if (!handlerID->p.oredDevicePending)
            {
                /* reset stripDone flag and stop ta return ERR_OK to exit level */
                if (handlerID->p.stripDone == 1)
                {
                    handlerID->p.stripDone = 0;
                    phFuncTaStop(handlerID);
                    return PHFUNC_ERR_OK;
                }

                /* during the next call everything up to here should be
                   repeated */
                phFuncTaStop(handlerID);
                phFuncTaRemoveToMark(handlerID);

                return PHFUNC_ERR_WAITING;
            }

            phFuncTaStop(handlerID);
            phEstateASetPauseInfo(handlerID->myEstate, 0);
            break;
        default:
            break;
    }
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "Exiting waitForStripStart, retVal = %d\n", retVal);
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

    char binningCommand[PHESTATE_MAX_SITES * 16] = "";
    char binningSendCommand[PHESTATE_MAX_SITES * 16] = "";
    char tmpCommand[PHESTATE_MAX_SITES] = "";
    char thisbin[PHESTATE_MAX_SITES] = "";
    int i = 0;
    int sendBinning = 0;
    int tmpbin = 0;

    switch(handlerID->model)
    {
        case PHFUNC_MOD_FH_1200:
            sprintf(binningCommand,"Set Bin \"%s\" ", stripID);

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
                            sprintf(tmpCommand,"%s,%d,",deviceIDs[i],handlerID->p.reprobeBinNumber);
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
                        if(tmpbin>99999 || tmpbin <0)
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
                                  sprintf(tmpCommand,"%s,%s,",deviceIDs[i],thisbin);
                            }
                            else
                            {
                                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                  "will bin device at site \"%s\" to %d",
                                  handlerID->siteIds[i], perSiteBinMap[i]);
                                  sprintf(tmpCommand,"%s,%ld,",deviceIDs[i],perSiteBinMap[i]);
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
            break;
      default:
        break;
    }
    if ( retVal==PHFUNC_ERR_OK && sendBinning==1)
    {
        strncpy(binningSendCommand,binningCommand, strlen(binningCommand) - 1);
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s",
                      binningSendCommand, handlerID->p.eol));
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "send command \"%s\"",
                         binningSendCommand);

        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Waiting for 30ms before clear the GPIB interface");
        localDelay_us(30000);
        PhFuncTaCheck(phFuncTaClearGpib(handlerID));

    }

    return retVal;
}


static phFuncError_t handlerSetup(
    phFuncId_t handlerID    /* driver plugin ID */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    return retVal;
}


/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int found = 0;
    phConfError_t confError;
    double dReprobeBinNumber;
    static int firstTime=1;

    /*
     * get reprobe bin number
     * following answers to queries from Multitest it appears the
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
            "configuration \"%s\" set: will send devices to bin %d upon receiving a reprobe",
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
    if ( (found==2 || found==3)  && !handlerID->p.reprobeBinDefined )
    {
        /* frame work may send reprobe command but a reprobe bin has not been defined */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
            "a reprobe mode has been defined without its corresponding bin number\n"
            "please check the configuration values of \"%s\" and \"%s\"",
            PHKEY_RP_AMODE, PHKEY_RP_BIN);
        retVal = PHFUNC_ERR_CONFIG;
    }

    /* perform the communication intesive parts */
    if ( firstTime )
    {
        PhFuncTaCheck(handlerSetup(handlerID));

        firstTime=0;
    }

    return retVal;
}

/*****************************************************************************
 *
 * for a certain GetStatus query, get x and/or y coordinate of device on strip
 *
 * Authors: Magco Li
 *
 * History: 6 Jan 2015, Magco Li , created
 *
 * Description:
 *    the x/y coordinate is from device id, device id consists of letter and digital,
 *    the x coordinate is consists of only letter, the y coordinate is consist of
 *    only digital.
 *
 * Return:
 *    function return SUCCEED if everything is OK, FAIL if error;
 *
 ***************************************************************************/
static int getValueForGetStatusQuery(
    phFuncId_t handlerID,
    char **pValue,
    const char *token,
    const char *param
)
{
    int i_pos = 0;
    int retVal = SUCCEED;
    static char statusValue[PHESTATE_MAX_SITES*LEN_OF_DEVICE_ID_PER_SITE] = "";

#ifdef DEBUG
    fprintf(stderr, "the token is %s\n",token);
    fprintf(stderr, "the param is %s\n",param);
#endif

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "The token \"%s\" the param is  \"%s\"", token, param);
    //x and y coordinate
    if (atoi(param) <= PHESTATE_MAX_SITES)
    {
        if (strcmp(token, PHKEY_NAME_HANDLER_STATUS_GET_X_COORDINATE) == 0)
        {
            strcpy(statusValue, coordinates[atoi(param) - 1].x);
        }
        else if(strcmp(token, PHKEY_NAME_HANDLER_STATUS_GET_X_COORDINATE_INT) == 0)
        {
            i_pos = computeX(coordinates[atoi(param) - 1].x);
            if (-1 == i_pos)
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "Received invalid coordinates, please check the handler status.");
                retVal = FAIL;
            }
            sprintf(statusValue, "%d\n", i_pos);
        }
        else if(strcmp(token, PHKEY_NAME_HANDLER_STATUS_GET_XY_COORDINATES) == 0)
        {
            strcpy(statusValue, deviceIDStringFromHandler);
        }
        else if(strcmp(token,PHKEY_NAME_HANDLER_STATUS_GET_Y_COORDINATE) == 0)
        {
            strcpy(statusValue, coordinates[atoi(param) - 1].y);
        }
        else
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                             "The key \"%s\" is not available, or may not be supported yet", token);
            strcpy(statusValue,"");
            retVal = FAIL;
        }
    }
    else
    {
        strcpy(statusValue,"");
        retVal = FAIL;
    }
    *pValue = statusValue;

    return retVal;
}

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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

    handlerID->p.oredDevicePending = 0;
    handlerID->p.stopped = 0;
    handlerID->p.stripCalled = 0;
    handlerID->p.stripSetPCCalled = 0;
    handlerID->p.stripDone = 0;
    handlerID->p.deviceAvailable = 0;

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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    int i;

    if (handlerID->p.deviceAvailable == 0)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                           "enter privateGetStart");
        switch ( handlerID->model )
        {
            case PHFUNC_MOD_FH_1200:
                /* set level end by receiving SetPanelComplete in strip start process */
                if(1 == handlerID->p.stripSetPCCalled)
                {
                    phTcomSetSystemFlag(handlerID->myTcom, PHTCOM_SF_CI_LEVEL_END, 1L);
                    handlerID->p.stripSetPCCalled = 0;
                    return PHFUNC_ERR_OK;
                }

                /* abort in case of unsuccessful retry */
                if (phFuncTaAskAbort(handlerID))
                    return PHFUNC_ERR_ABORTED;

                /* remember which devices we expect now */
                for (i=0; i<handlerID->noOfSites; i++){
                    handlerID->p.deviceExpected[i] = handlerID->activeSites[i];
                }

                phFuncTaStart(handlerID);
                phFuncTaMarkStep(handlerID);
                PhFuncTaCheck(waitForParts(handlerID));

                /* do we have at least one part? If not, ask for the current
                 status and return with waiting */
                if (!handlerID->p.oredDevicePending)
                {
                    /* reset stripDone flag and stop ta return ERR_OK to exit level */
                    if (handlerID->p.stripDone == 1)
                    {
                        handlerID->p.stripDone = 0;
                        phFuncTaStop(handlerID);
                        return PHFUNC_ERR_OK;
                    }

                    /* during the next call everything up to here should be
                       repeated */
                    phFuncTaRemoveToMark(handlerID);
                    phFuncTaStop(handlerID);
                    return PHFUNC_ERR_WAITING;
                }

                phFuncTaStop(handlerID);
                phEstateASetPauseInfo(handlerID->myEstate, 0);
                break;
            default:
                break;
        }
    }

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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
    for(i=0; i<handlerID->noOfSites; i++)
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

    for( i= 0; i < PHESTATE_MAX_SITES; i++)
    {
        strcpy(coordinates[i].x, "");
        strcpy(coordinates[i].y, "");
        strcpy(deviceIDs[i], "");
        strcpy(deviceIDStringFromHandler, "");
    }
    handlerID->p.deviceAvailable = 0;
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
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
                switch (handlerID->model)
                {
                    default:
                        /*
                     * following answers to queries from Multitest it appears
                     * the original assumption is correct:
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
    /* by default not implemented */
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
 * History: 16 Dec 2014, Magco Li , created
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
 * The stub function for get status
 *
 * Authors: Xiaofei Han
 *
 * History: 16 Dec 2014, Xiaofei Han , created
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
    phFuncError_t retVal = PHFUNC_ERR_OK;

    const char *token = commandString;
    const char *param = paramString;
    static char *rtnString = NULL;
    static char response[MAX_STATUS_MSG] = "";
    response[0] = '\0';
    *responseString = response;
    int found = FAIL;

    if ( phFuncTaAskAbort(handlerID) )
    {
        return PHFUNC_ERR_ABORTED;
    }

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "privateGetStatus, token = ->%s<-, param = ->%s<-", token, param);

    if( (atoi(param)<= 0 && strcmp(token, PHKEY_NAME_HANDLER_STATUS_GET_XY_COORDINATES) != 0)
        || atoi(param) > PHESTATE_MAX_SITES)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                         "Illegal site number specified: %d", atoi(param));
        return PHFUNC_ERR_FATAL;
    }
    else
    {
        found = getValueForGetStatusQuery(handlerID, &rtnString, token, param);
        if( found == SUCCEED )
        {
            *responseString = rtnString;
        }
        else
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                             "The key \"%s\" is not available, or may not be supported yet", token);
            sprintf(response, "%s_KEY_NOT_AVAILABLE", token);
        }
    }

    return retVal;
}
#endif

#ifdef STRIPSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for strip start signal from handler
 *
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStripStart(
                              phFuncId_t handlerID,      /* driver plugin ID */
                              int timeoutFlag
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "Entering privateStripStart, timeoutFlag = %s\n", timeoutFlag ? "TRUE" : "FALSE");

    /* abort in case of unsuccessful retry */
    if ( phFuncTaAskAbort(handlerID) ) return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);

    /* strip start called flag is set */
    handlerID->p.stripCalled = 1;
    PhFuncTaCheck(waitForStripStart(handlerID));
    handlerID->p.stripCalled = 0;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "privateStripStart complete\n");

    phFuncTaStop(handlerID);

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "exiting privateStripStart, retVal = %d\n", retVal);
    return retVal;

}
#endif

#ifdef STRIPDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Cleanup after strip end
 *
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStripDone(
                              phFuncId_t handlerID,      /* driver plugin ID */
                              int timeoutFlag
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                          "enter privateStripDone");
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "current strip %s is ended\n", stripID);
    return retVal;
}
#endif

#ifdef STRIPMATERIALID_IMPLEMENTED
/*****************************************************************************
 *
 * return the test-on-strip material ID.
 *
 * Authors: Magco Li
 *
 * History: 16 Dec 2014, Magco Li , created
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
    phFuncError_t retVal = PHFUNC_ERR_OK;
    *materialIdString = stripID;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "privateStripMaterialID  materialId  = %s\n", *materialIdString);
    return retVal;

}
#endif
/*****************************************************************************
 * End of file
 *****************************************************************************/
