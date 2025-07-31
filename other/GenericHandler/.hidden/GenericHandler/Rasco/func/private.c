 /******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 06 Aug 2006
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Xiaofei Han, STS R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 06 Aug 2006, Xiaofei Han, created
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
    {
      return 0;
    }
    else
    {
      return 1;
    }
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

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    static char handlerAnswer[64] = "";
    long population = 0L;
    long populationPicked = 0L;
    int i;

    switch(handlerID->model)
    {
      case PHFUNC_MOD_SO3000:
      PhFuncTaCheck(phFuncTaSend(handlerID, "SQB?%s", handlerID->p.eol));
        retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
        if (retVal != PHFUNC_ERR_OK) 
        {
          phFuncTaRemoveStep(handlerID); 
          return retVal;
        }
        /* remove the invalid bits */
        population = handlerAnswer[0] & 0x0f;
      break;

      default: 
      break;
    }

    /* check answer from handler, similar for all handlers.  remove
       two steps (the query and the answer) if an empty string was
       received */

    if (strlen(handlerAnswer) != 1 || handlerAnswer[0] < '0' || handlerAnswer[0] > '?')
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
          "site population not received from handler: \"%s\", trying again", handlerAnswer);
      phFuncTaRemoveStep(handlerID);
      phFuncTaRemoveStep(handlerID);
      return PHFUNC_ERR_WAITING;
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
           "device present at site \"%s\" (polled)", handlerID->siteIds[i]);
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

    /* ignore SRQs sent during polling if polling found devices */
    if (handlerID->p.oredDevicePending)
    {
      PhFuncTaCheck(flushSRQs(handlerID));
    }

    /* check whether we have received to many parts */    
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
    int srqPicked = 0;
    static int received = 0;
    struct timeval pollStartTime;
    struct timeval pollStopTime;
    int timeout;
    int i;
    int partMissing = 0;
   

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
          {
            timeout = 1;
          }
          else
          {
            localDelay_us(handlerID->p.pollingInterval);
          }
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
      PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, handlerID->p.oredDevicePending));

      if (received)
      {
        switch (handlerID->model)
        {
          case PHFUNC_MOD_SO3000:
            if (srq & 0x0f)
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
                {
                  partMissing = 1;
                }
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
                PhFuncTaCheck(pollParts(handlerID));
                PhFuncTaCheck(flushSRQs(handlerID));
              }

              /* check whether we have received too many parts */

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

          default: 
          break;
        }
      }
      else
      {
        /* do one poll. Maybe there is a part here, but we have not
           received an SRQ */
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
           "have not received new devices through SRQ, try to poll...");
            PhFuncTaCheck(pollParts(handlerID));
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
     *  format for the command will be --X bin YY -- for each
     *  possible site which is a maximum of 9 characters for each 
     *  site
     *  char binningCommand[PHESTATE_MAX_SITES*9 + 16];
     */
    char binningCommand[PHESTATE_MAX_SITES*9 + 16];
    char tmpCommand[256];
    int i;
    int sendBinning = 0;

    strcpy(binningCommand,"");
    
    switch(handlerID->model)
    {
      case PHFUNC_MOD_SO3000:
        /* 
         *  following answers to queries from Multitest 
         *  recommended to send all binning in one go
         */
        for (i=0; i<handlerID->noOfSites; i++)
        {
          if (handlerID->activeSites[i] && 
             (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
             oldPopulation[i] == PHESTATE_SITE_POPDEACT))
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
                sprintf(tmpCommand, "%s BIN %d ",
                   handlerID->siteIds[i], handlerID->p.reprobeBinNumber);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                      "will reprobe device at site \"%s\"", 
                      handlerID->siteIds[i]);
                strcat(binningCommand, tmpCommand);
                sendBinning = 1;
              }
            }
            else
            {
              /* 
               *  following answers to queries from Rasco 
               *  maximum bin category is 16383
               */
              if (perSiteBinMap[i] < -1 || perSiteBinMap[i] > 16383)
              {
                /* illegal bin code */
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                   "received illegal request for bin %ld at site \"%s\"",
                   perSiteBinMap[i], handlerID->siteIds[i]);
                retVal =  PHFUNC_ERR_BINNING;
              }
              else
              {
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
      default: 
      break;
    }


    if ( retVal==PHFUNC_ERR_OK && sendBinning )
    {
      PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", 
         binningCommand, handlerID->p.eol));
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
         "send command \"%s\"", 
         binningCommand);
    }

    return retVal;
}


/* send temperature setup to handler, if necessary */
static phFuncError_t reconfigureTemperature(phFuncId_t handlerID)
{    
    phFuncError_t retVal = PHFUNC_ERR_OK;
    struct tempSetup tempData;

    /* get all temperature configuration values */
    if (!phFuncTempGetConf(handlerID->myConf, handlerID->myLogger,&tempData))
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
        case PHFUNC_MOD_SO3000:
          PhFuncTaCheck(phFuncTaSend(handlerID, ":TEMP:SETPOINT OFF%s", handlerID->p.eol));
        break;
        default: 
        break;
      }
      return retVal;
    }

    /* tempData.mode == PHFUNC_TEMP_ACTIVE */

    /* this is the real job, define and activate all temperature
       parameters, check for correct chamber names first.currently
       the Rasco SO3000 handler doesn't care about the chambers when
       setting temperature, but in case the following code will be
       used someday later, we just have the code segment commented out. */
    /*switch(handlerID->model)                                                           */
    /*{                                                                                  */
    /*  case PHFUNC_MOD_SO3000:                                                          */
    /*  for (i=0; i<tempData.chambers; i++)                                              */
    /*  {                                                                                */
    /*    if (strcasecmp(tempData.name[i], "80")   != 0)                                 */
    /*    {                                                                              */
    /*      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,                      */
    /*         "the handler does not provide a temperature chamber with name \"%s\"\n"   */
    /*         "please check the configuration of \"%s\"",                               */
    /*         tempData.name[i], PHKEY_TC_CHAMB);                                        */
    /*      retVal = PHFUNC_ERR_CONFIG;                                                  */
    /*    }                                                                              */
    /*  }                                                                                */
    /*  break;                                                                           */
    /*  default:                                                                         */
    /*  break;                                                                           */ 
    /*}                                                                                  */

    /* if (retVal != PHFUNC_ERR_OK)                                                      */
    /*{                                                                                  */
    /*  return retVal;                                                                   */
    /*}                                                                                  */

    /* chamber names seem to be OK, define the values now */
    /*for (i=0; i<tempData.chambers; i++)                                                */
    /*{                                                                                  */
    /*  if (tempData.control[i] == PHFUNC_TEMP_ON)                                       */
    /*  {                                                                                */
    /*    switch(handlerID->model)                                                       */
    /*    {                                                                              */
    /*      case PHFUNC_MOD_SO3000:                                                      */
    /*        PhFuncTaCheck(phFuncTaSend(handlerID, ":TEMP:SETPOINT %s,%d%s",            */
    /*        tempData.name[i], (int) tempData.setpoint[i], handlerID->p.eol));          */
    /*      break;                                                                       */
    /*      default:                                                                     */
    /*      break;                                                                       */
    /*    }                                                                              */
    /*  }                                                                                */
    /*}                                                                                  */


    PhFuncTaCheck(phFuncTaSend(handlerID, ":TEMP:SETPOINT %+d%s", (int) tempData.setpoint[0], handlerID->p.eol));
    
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

    *machineIdString = idString;

    phFuncError_t retVal = PHFUNC_ERR_OK;

    /* common to all models: identify */
    PhFuncTaCheck(phFuncTaSend(handlerID, "ID?%s", handlerID->p.eol));
    localDelay_us(100000);
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", idString);
    if (retVal != PHFUNC_ERR_OK) 
    {
      phFuncTaRemoveStep(handlerID); // force re-send of command 
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
    double dReprobeBinNumber;
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
    {
      confError = phConfConfNumber(handlerID->myConf, PHKEY_FC_POLLT, 
          0, NULL, &dPollingInterval);
    }

    if (confError == PHCONF_ERR_OK)
    {
      handlerID->p.pollingInterval = labs((long) dPollingInterval);
    }
    else
    {
      handlerID->p.pollingInterval = 200000L; /* default 0.2 sec */
    }

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

    PhFuncTaCheck(reconfigureTemperature(handlerID));

    return retVal;
}


/*****************************************************************************
 *
 * for a certain GetStatus query, get the corresponding GPIB command
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *    all the actual GPIB commands are stored locally, this function retrieves the 
 *    command corresponding to the "token". For example, 
 *    for the query "PROB_HND_CALL(ph_get_status site_mapping)", this function
 *    returns the GPIB command:
 *        ":SITES:MAPPING?"                     
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
  static const  phStringPair_t sGpibCommands[] = {
    {PHKEY_NAME_HANDLER_STATUS_GET_SITE_MAPPING,                ":SITES:MAPPING?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT,        ":TEMP:SETPOINT?"},
    {PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SOAKTIME,        ":TEMP:SOAKTIME?"},
  };
  static char buffer[MAX_STATUS_MSG] = "";
  int retVal = SUCCEED;
  const phStringPair_t *pStrPair = NULL;
  int paramSpecified = NO;
  int ignoreParam = NO;

  if (strlen(param) > 0) 
  {
    paramSpecified = YES;
    if (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_SITE_MAPPING) == 0 ||
        strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SETPOINT) == 0  ||
        strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_TEMPERATURE_SOAKTIME) == 0) 
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                       "the status query \"temperature_setpoint\",\"temperature_soaktime\" and \"site_mapping\" does not require any parameters!\n"
                       "Ignore the parameter %s.", 
                       param);
      ignoreParam = YES;
      retVal = SUCCEED;
    }
  }

  if (retVal == SUCCEED) 
  {
    pStrPair = phBinSearchStrValueByStrKey(sGpibCommands, LENGTH_OF_ARRAY(sGpibCommands), token);
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

    handlerID->p.strictPolling = 0;
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
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
 * Authors: Xiaofei Han
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
 * Authors: Xiaofei Han
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
 * Authors: Xiaofei Han
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
    static char resultString[1024] = "";
    idString[0] = '\0';
    *equipIdString = idString;

    phFuncError_t retVal = PHFUNC_ERR_OK;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
    return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);

    /* common to all models: identify */
    PhFuncTaCheck(phFuncTaSend(handlerID, "ID?%s", handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", idString);
    if (retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID); // force re-send of command
        return retVal;
    }

    phFuncTaStop(handlerID);

   /* create result string */
    sprintf(resultString, "%s", idString);

    /* strip of white space at the end of the string */
    while (strlen(resultString) > 0 && isspace(resultString[strlen(resultString)-1]))
    resultString[strlen(resultString)-1] = '\0';
    return retVal;
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
phFuncError_t privateGetStart(
    phFuncId_t handlerID      /* driver plugin ID */
)
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
    for (i=0; i<handlerID->noOfSites; i++)
    {
      handlerID->p.deviceExpected[i] = handlerID->activeSites[i];
    }

    phFuncTaStart(handlerID);

    phFuncTaMarkStep(handlerID);
    retVal = waitForParts(handlerID);
    if (retVal != PHFUNC_ERR_OK) 
    {
      phFuncTaRemoveToMark(handlerID);
      return retVal;
    }

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
        {
          population[i] = PHESTATE_SITE_EMPTY;
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

    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);

    return retVal;
}
#endif



#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested device
 *
 * Authors: Xiaofei han
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
    {
      return PHFUNC_ERR_ABORTED;
    }

    /* get current site population */
    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

    phFuncTaStart(handlerID);
    PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation,NULL, perSiteBinMap));
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
      {
        population[i] = oldPopulation[i];	
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
 * Authors: Xiaofei Han
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
      perSiteBinMap[i] = handlerID->p.reprobeBinNumber;
    }

    return privateBinReprobe(handlerID, perSiteReprobe, perSiteBinMap);
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
    {
      return PHFUNC_ERR_ABORTED;
    }
    
    /* remember which devices we expect now */
    for (i=0; i<handlerID->noOfSites; i++)
    {
      handlerID->p.deviceExpected[i] = 
         (handlerID->activeSites[i] && perSiteReprobe[i]);
    }

    /* get current site population */
    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

    phFuncTaStart(handlerID);

    PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation, perSiteReprobe, perSiteBinMap));

    /* wait for reprobed parts */
    phFuncTaMarkStep(handlerID);
    retVal = waitForParts(handlerID);
    if (retVal != PHFUNC_ERR_OK) 
    {
      phFuncTaRemoveToMark(handlerID);
      return retVal;
    }

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
            {
              population[i] = oldPopulation[i];
            }
          }
        }
        else /* ! perSiteReprobe[i] */
        {
          /* All devices need to receive a bin or reprobe data, and devices
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
          {
            population[i] = PHESTATE_SITE_DEACTIVATED;
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
 * Authors: Xiaofei Han
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
 * Authors: Xiaofei Han
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
 * Authors: Xiaofei Han
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
 * Authors: Xiaofei Han
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
 * Authors: Xiaofei Han 
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
 * Authors: Xiaofei Han
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

  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
     "privateGetStatus, token = ->%s<-, param = ->%s<-", token, param);

  found = getGpibCommandForGetStatusQuery(handlerID, &gpibCommand, token, param);
  if( found == SUCCEED ) 
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

