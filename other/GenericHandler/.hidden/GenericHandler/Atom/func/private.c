/******************************************************************************
 *
 *       (c) Copyright Agilent Technologies GmbH, Boeblingen 1999
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 1 Jul 2023
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Zuria Zhu, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 1 Jul 2023, Zuria Zhu, created
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
#include <time.h>
#include <signal.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include  "ph_GuiServer.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_hfunc.h"

#include "ph_keys.h"
//#include "gpio_conf.h"
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

#define MY_EOC "\r\n"

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

/*--- functions definitions -------------------------------------------------*/
phFuncError_t private_gpib_send(char *varCommand, phFuncId_t handlerID);
phFuncError_t private_gpib_receive(char *outReply, phFuncId_t handlerID);
phFuncError_t private_checkHandlerForError(phFuncId_t handlerID);
phFuncError_t private_askOperatorForLotEnd(phFuncId_t handlerID);

/*--- functions -------------------------------------------------------------*/

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateInit(phFuncId_t handlerID /* the prepared driver plugin ID */
)
{
  char *tmpString = NULL;
  char *tmpInterval = NULL;
  u_int tmpInteger;
  double tmpLong;

  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, "********** PH EXECUTE privateInit **********");

  // init variables
  // this is the new variable using for delay between BIN command and RFT command, unit is microsecond
  handlerID->p.pollingDelay = 5000000L;
  // this variable is used for receiving waiting_for_lot_timeout
  handlerID->p.lotTimeOutInteger = 0;
  handlerID->p.timeAskLotEnd = 5;
  //default polling interval is 1000000 microseconds
  handlerID->p.pollingInterval = 1000000L;
  strcpy(handlerID->p.gpibEndOfLine, "\r\n");

  // get value
  if (phConfGetConfDefinition(handlerID->myConf, "waiting_for_parts_timeout", &tmpString)
      != PHCONF_ERR_OK) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
        "Configuration file error - waiting_for_parts_timeout is not defined");
    return PHFUNC_ERR_CONFIG;
  }

  // verify if it's ok to write configuration param into tmpInteger
  if ((sscanf(tmpString, "%*s %i", &tmpInteger) != 1) || (tmpInteger < 5)) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
        "Configuration file error - value of waiting_for_parts_timeout is invalid (must be integer bigger or equal 5)");
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "Found: ->%s<-", tmpString);
    return PHFUNC_ERR_CONFIG;
  }

  // save and log
  handlerID->p.timeAskLotEnd = tmpInteger;
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "*** Will ask operator for lot end after waiting %i seconds", handlerID->p.timeAskLotEnd);

  /* reset polling_interval if it has been defined in configuration correctly */
  memset(tmpString, 0, strlen(tmpString));

  if(phConfGetConfDefinition(handlerID->myConf, "polling_interval", &tmpString)
      == PHCONF_ERR_OK){

    if ((sscanf(tmpString, "%*s %lf", &tmpLong) != 1) || (tmpLong < 0)) {
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
           "Configuration file error - value of polling_interval is invalid (must be long, lager than 0.)");
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "Found: ->%s<-", tmpString);
       return PHFUNC_ERR_CONFIG;
     }

    handlerID->p.pollingInterval = tmpLong;
  }

  /* reset polling_delay if it has been defined in configuration correctly */
  memset(tmpString, 0, strlen(tmpString));
  if(phConfGetConfDefinition(handlerID->myConf, "polling_delay", &tmpString)
      == PHCONF_ERR_OK){

    if ((sscanf(tmpString, "%*s %lf", &tmpLong) != 1) || (tmpLong < 0)) {
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
           "Configuration file error - value of polling_delay is invalid (must be long, equal or lager than 0.)");
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "Found: ->%s<-", tmpString);
       return PHFUNC_ERR_CONFIG;
     }

    handlerID->p.pollingDelay = tmpLong;
  }

   /* reset waiting_for_lot_timeout if it has been defined in configuration correctly */
   memset(tmpString, 0, strlen(tmpString));
   if (phConfGetConfDefinition(handlerID->myConf, "waiting_for_lot_timeout", &tmpString)
       == PHCONF_ERR_OK) {
     // verify if it's ok to write configuration param into tmpInteger
     if ((sscanf(tmpString, "%*s %i", &tmpInteger) != 1) || (tmpInteger < 0)) {
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
           "Configuration file error - value of waiting_for_lot_timeout is invalid (must be integer equal or bigger than 0)");
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "Found: ->%s<-", tmpString);
       return PHFUNC_ERR_CONFIG;
     }
     handlerID->p.lotTimeOutInteger = tmpInteger;

   }

  // done
  return PHFUNC_ERR_OK;
}
#endif

#ifdef RECONFIGURE_IMPLEMENTED
/*****************************************************************************
 *
 * reconfigure driver plugin
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 **************************************************handlerID->p.gpibEndOfLine*************************/
phFuncError_t privateReconfigure(phFuncId_t handlerID /* driver plugin ID */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateReconfigure **********");
  ReturnNotYetImplemented("privateReconfigure");
}
#endif

#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the handler
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReset(phFuncId_t handlerID /* driver plugin ID */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateReset **********");
  ReturnNotYetImplemented("privateReset");
}
#endif

#ifdef DRIVERID_IMPLEMENTED
/*****************************************************************************
 *
 * return the driver identification string
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDriverID(phFuncId_t handlerID /* driver plugin ID */, char **driverIdString /* resulting pointer to driver ID string */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateDriverID **********");

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
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateEquipID(phFuncId_t handlerID /* driver plugin ID */, char **equipIdString /* resulting pointer to equipment ID string */
)
{
  static char tmpEquipmentID[48] = "DB Design ATOM handler";

  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateEquipID **********");

  *equipIdString = tmpEquipmentID;
  return PHFUNC_ERR_OK;
}
#endif

#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * lot start
 *
 * Authors: Zuria Zhu
 * 2023/Jul/01
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
	phFuncError_t tmpRet;

 	// logging
	phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,"********** PH EXECUTE privateLotStart **********");
	
	// ask handler for error
	if ( (tmpRet=private_checkHandlerForError(handlerID))!=PHFUNC_ERR_OK )
		return tmpRet;
		
	// ok	
	return PHFUNC_ERR_OK;
}
#endif

#ifdef LOTDONE_IMPLEMENTED
/*****************************************************************************
 *
 * lot done
 *
 * Authors: Zuria Zhu
 * 2023/Jul/01
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
 	// logging
	phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,"********** PH EXECUTE privateLotDone **********");
	/* abort in case of unsuccessful retry */
        if ( phFuncTaAskAbort(handlerID) ) return PHFUNC_ERR_ABORTED;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "exiting privateLotDone, retVal = %d\n", retVal);
        return retVal;
}
#endif

/* limit query frequency */
static int localDelay_us(long microseconds)
{
  long seconds;
  struct timeval delay;

  /* us must be converted to seconds and microseconds ** for use by select(2) */
  if(microseconds >= 1000000L) {
    seconds = microseconds / 1000000L;
    microseconds = microseconds % 1000000L;
  } else {
    seconds = 0;
  }

  delay.tv_sec = seconds;
  delay.tv_usec = microseconds;

  // set the mask of all the signals which would interrupt the select function
  sigset_t mask, oldmask;
  sigemptyset(&mask);
  sigemptyset(&oldmask);
  sigfillset(&mask);
  sigprocmask(SIG_BLOCK,&mask,&oldmask);

  //critical code, this is a time out waiting task
  int result=select(0, NULL, NULL, NULL, &delay);

  //Restore the previous mask
  sigprocmask(SIG_SETMASK, &oldmask, NULL);

  return result;
}

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  char tmpResponse[1024] = "";
  phFuncError_t tmpRet;
  phEstateSiteUsage_t population[PHESTATE_MAX_SITES];

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering %s ...", __FUNCTION__);
  PhFuncTaCheck(phFuncTaSend(handlerID, "RFT?%s", handlerID->p.eol));
  PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", tmpResponse));

  // got data - ready to test ? FOLLOWING the manual, 0 is ready for test and 1 is not!
  if (strcmp(tmpResponse, "0") == 0) {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "ready to test");
    population[0] = PHESTATE_SITE_EMPTY;

    /* correct pending devices information, overwrite all (trust the
     last handler query) */
    handlerID->p.oredDevicePending = 0;
    handlerID->p.devicePending[0] = 1;
    handlerID->p.oredDevicePending = 1;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
        "device present at site \"%s\" (polled)", "1");

    return PHFUNC_ERR_OK;
  }

  // not ready to test
  else if (strcmp(tmpResponse, "1") == 0) {
    handlerID->p.devicePending[0] = 0;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
        "no device at site \"%s\" (polled)",
        "1");

  }

  // unexpected reply
  else {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
        "Received unexpected reply for GPIB query RFT? - %s", tmpResponse);
    return PHFUNC_ERR_FATAL;
  }

  return retVal;
}

/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
  int timeout=0;
  struct timeval pollStartTime;
  struct timeval pollStopTime;
  phFuncError_t retVal = PHFUNC_ERR_OK;
  gettimeofday(&pollStartTime, NULL);
  /*  because Atom handler has no response after binning command,
      if there's not a few seconds pause, the next polling command will be immediately sent and get a wrong reply,
      said there's a device ready for test detected.
      But actually during this time the handler is in the process of binning the current device, haven't had time to load next device yet.*/
  localDelay_us(handlerID->p.pollingDelay);
  do {
        PhFuncTaCheck(pollParts(handlerID));
        if(!handlerID->p.oredDevicePending) {
          gettimeofday(&pollStopTime, NULL);
          if((pollStopTime.tv_sec - pollStartTime.tv_sec)*1000000L +
             (pollStopTime.tv_usec - pollStartTime.tv_usec) >
             handlerID->p.lotTimeOutInteger*1000000L)
            timeout = 1;
          else
            localDelay_us(handlerID->p.pollingInterval);
        }
      } while(!handlerID->p.oredDevicePending && !timeout);

  if(!handlerID->p.oredDevicePending){
    long response = -1;
    char strMsg[128] = { 0 };
    snprintf(strMsg, 127,
        "It seems that after many repeated queries, still no device ready for test detected. Do you want to abort or continue?");
    phGuiMsgDisplayMsgGetResponse(handlerID->myLogger, 0, "Info", strMsg, "abort", "continue",
        NULL, NULL, NULL, NULL, &response);
    if (response == 2) {                      //it seems abort
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
          "Never received Contact check start SRQ, operator have been canceled to wait for SRQ, please check if enable contact check feature at handler side");
      return PHFUNC_ERR_LOT_DONE;
    }
  }
  return retVal;
}

#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next device
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateGetStart(phFuncId_t handlerID /* driver plugin ID */
)
{

  phFuncError_t retVal = PHFUNC_ERR_OK;
  int i = 0;
  phEstateSiteUsage_t population[PHESTATE_MAX_SITES];

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateGetStart\n");

  /* abort in case of unsuccessful retry */
  if (phFuncTaAskAbort(handlerID))
    return PHFUNC_ERR_ABORTED;

  /* remember which devices we expect now */
  for (i = 0; i < handlerID->noOfSites; i++)
    handlerID->p.deviceExpected[i] = handlerID->activeSites[i];

  phFuncTaStart(handlerID);

  phFuncTaMarkStep(handlerID);
  PhFuncTaCheck(waitForParts(handlerID));

  /* do we have at least one part? If not, ask for the current
   status and return with waiting */
  if (!handlerID->p.oredDevicePending) {
    /* during the next call everything up to here should be
     repeated */
    phFuncTaRemoveToMark(handlerID);

    return PHFUNC_ERR_WAITING;
  }

  phFuncTaStop(handlerID);

  /* we have received devices for test. Change the equipment
   specific state now */
  handlerID->p.oredDevicePending = 0;
  for (i = 0; i < handlerID->noOfSites; i++) {
    if (handlerID->activeSites[i] == 1) {
      if (handlerID->p.devicePending[i]) {
        handlerID->p.devicePending[i] = 0;
        population[i] = PHESTATE_SITE_POPULATED;
      } else
        population[i] = PHESTATE_SITE_EMPTY;
    } else {
      if (handlerID->p.devicePending[i]) {
        /* something is wrong here, we got a device for an
         inactive site */
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
            "received device for deactivated site \"%s\"", handlerID->siteIds[i]);
      }
      population[i] = PHESTATE_SITE_DEACTIVATED;
    }
    handlerID->p.oredDevicePending |= handlerID->p.devicePending[i];
  }
  phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);

  return retVal;
}
#endif

/* create and send bin information */
static phFuncError_t binDevice(phFuncId_t handlerID /* driver plugin ID */,
    phEstateSiteUsage_t *oldPopulation /* current site population */, long *perSiteBinMap /* valid binning data for each site */
    )
{
  phFuncError_t retVal = PHFUNC_ERR_OK;

  char tmpString[64] = "BIN";

  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateBinDevice **********");

  // fail or pass ? - only 1 device
  if(handlerID->binIds){/* if handler bin id is defined (hardbin/softbin mapping). perSiteBinMap is the index of bin id list*/
    if(perSiteBinMap[0]==0){
      strcat(tmpString, "1");
    }else{
      strcat(tmpString, "2");
    }
  }else{/* if handler bin id is not defined (default mapping), use the perSiteBinMap directly */
    if (perSiteBinMap[0] == 1){
      strcat(tmpString, "1");
    }else{
      strcat(tmpString, "2");
    }
  }

  // send
  PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", tmpString, handlerID->p.eol));

  return PHFUNC_ERR_OK;

}

#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested device
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateBinDevice(phFuncId_t handlerID /* driver plugin ID */, long *perSiteBinMap /* binning information */
)
{

  phFuncError_t retVal = PHFUNC_ERR_OK;

  phEstateSiteUsage_t *oldPopulation;
  int entries;
  phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
  int i = 0;

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
  for (i = 0; i < handlerID->noOfSites; i++) {
    if (handlerID->activeSites[i]
        && (oldPopulation[i] == PHESTATE_SITE_POPULATED
            || oldPopulation[i] == PHESTATE_SITE_POPDEACT)) {
      population[i] =
          oldPopulation[i] == PHESTATE_SITE_POPULATED ? PHESTATE_SITE_EMPTY :
              PHESTATE_SITE_DEACTIVATED;
    } else
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
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReprobe(phFuncId_t handlerID /* driver plugin ID */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateReprobe **********");
  ReturnNotYetImplemented("privateReprobe");
}
#endif

#ifdef BINREPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateBinReprobe(phFuncId_t handlerID /* driver plugin ID */,
    long *perSiteReprobe /* TRUE, if a device needs reprobe*/, long *perSiteBinMap /* valid binning data for each site where
     the above reprobe flag is not set */
    )
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateBinReprobe **********");
  ReturnNotYetImplemented("privateBinReprobe");
}
#endif

#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to handler plugin
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSTPaused(phFuncId_t handlerID /* driver plugin ID */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateSTPaused **********");
  ReturnNotYetImplemented("privateSTPaused");
}
#endif

#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to handler plugin
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateSTUnpaused(phFuncId_t handlerID /* driver plugin ID */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateSTUnpaused **********");
  ReturnNotYetImplemented("privateSTUnpaused");
}
#endif

#ifdef DIAG_IMPLEMENTED
/*****************************************************************************
 *
 * retrieve diagnostic information
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDiag(phFuncId_t handlerID /* driver plugin ID */, char **diag /* pointer to handlers diagnostic output */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, "********** PH EXECUTE privateDiag **********");

  *diag = phLogGetLastErrorMessage();
  return PHFUNC_ERR_OK;
}
#endif

#ifdef STATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStatus(phFuncId_t handlerID /* driver plugin ID */, phFuncStatRequest_t action /* the current status action
 to perform */, phFuncAvailability_t *lastCall /* the last call to the
 plugin, not counting calls
 to this function */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateStatus **********");
  ReturnNotYetImplemented("privateStatus");
}
#endif

#ifdef UPDATE_IMPLEMENTED
/*****************************************************************************
 *
 * update the equipment state
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateUpdateState(phFuncId_t handlerID /* driver plugin ID */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateUpdateState **********");
  ReturnNotYetImplemented("privateUpdateState");
}
#endif

#ifdef COMMTEST_IMPLEMENTED
/*****************************************************************************
 *
 * Communication test
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 *
 *
 * Functionality implemented
 *   
 *   Authors: Zuria Zhu
 *   Date:    2023/Jul/01
 * 
 ***************************************************************************/
phFuncError_t privateCommTest(phFuncId_t handlerID /* driver plugin ID */, int *testPassed /* whether the communication test has passed
 or failed        */
)
{
  char tmpResponse[1024] = "";
  phFuncError_t tmpRet;

  // default failed
  *testPassed = 0;

  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateCommTest **********");
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "#################################################################");
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "# Advantest driver for DB Design ATOM handler - Rev. 0.90 Beta");
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "#################################################################");
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, "Verify handler type now");

  // send
  if ((tmpRet = private_gpib_send("IDN?", handlerID)) != PHFUNC_ERR_OK) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
        "Could not verify handler type (send GPIB command failed)");
    return tmpRet;
  }

  // receive
  if ((tmpRet = private_gpib_receive(tmpResponse, handlerID)) != PHFUNC_ERR_OK) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
        "Could not verify handler type (receive GPIB reply failed)");
    return tmpRet;
  }

  // handler ok?
  if (strstr(tmpResponse, "DB Design") == NULL) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "This handler is not supported - %s",
        tmpResponse);
    return PHFUNC_ERR_MODEL;
  }

  // ok
  *testPassed = 1;
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, "Handler found - %s", tmpResponse);
  return PHFUNC_ERR_OK;
}
#endif

#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Zuria Zhu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDestroy(phFuncId_t handlerID /* driver plugin ID */
)
{
  // logging
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,
      "********** PH EXECUTE privateDestroy **********");
  return PHFUNC_ERR_OK;
}
#endif

#ifdef EXECGPIBCMD_IMPLEMENTED
/*****************************************************************************
 *
 * execute GPIB command
 *
 * Functionality implemented
 *   
 *   Authors: Zuria Zhu
 *   Date:    2023/Jul/01
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateExecGpibCmd(
        phFuncId_t handlerID        /* driver plugin ID */,
        char *commandString         /* key name to get parameter/information */,
        char **responseString       /* output of response string */

)
{
  	       phFuncError_t tmpRet;
	static char          tmpResponse[16];
	
	// logging
	phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,"********** PH EXECUTE privateExecGpibCmd **********");
	phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,"Send command: %s",commandString);
	
	// send
	if ( (tmpRet=private_gpib_send(commandString,handlerID))!=PHFUNC_ERR_OK )
	{
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"Send GPIB command failed");
		return tmpRet;
	}	
		
	// just for safty	
	strcpy(tmpResponse,"");
	*responseString=tmpResponse;
	return PHFUNC_ERR_OK;
}
#endif

#ifdef EXECGPIBQUERY_IMPLEMENTED
/*****************************************************************************
 *
 * execute GPIB query
 *
 * Functionality implemented
 *   
 *   Authors: Zuria Zhu
 *   Date:    2023/Jul/01
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateExecGpibQuery(
    phFuncId_t handlerID        /* driver plugin ID */,
    char *commandString         /* key name to get parameter/information */,
    char **responseString       /* output of response string */

)
{
	static char          tmpResponse[4096];
  	       phFuncError_t tmpRet;

	// logging
	phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,"********** PH EXECUTE privateExecGpibQuery **********");
	phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,"Send query: %s",commandString);
	
	// send
	if ( (tmpRet=private_gpib_send(commandString,handlerID))!=PHFUNC_ERR_OK )
	{
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"Send GPIB query failed");
		return tmpRet;
	}	
		
	// receive	
	if ( (tmpRet = private_gpib_receive(tmpResponse,handlerID))!=PHFUNC_ERR_OK )
	{
		phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"Receive reply of GPIB query failed");
		return tmpRet;
	}	

	// log reply
	phLogFuncMessage(handlerID->myLogger, LOG_NORMAL,"Reply from handler: %s",tmpResponse);

	// set reply buffer
	*responseString=tmpResponse;
	return PHFUNC_ERR_OK;
}
#endif

/*****************************************************************************
 *
 * Send GPIB command to handler
 *
 * Functionality implemented
 *   
 *   Authors: Zuria Zhu
 *   Date:    2023/Jul/01
 *
 ***************************************************************************/
phFuncError_t private_gpib_send(char *varCommand, phFuncId_t handlerID)
{
  phFuncError_t tmpRet;

  // log
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, "GPIB send: %s", varCommand);

  // send
  if ((tmpRet = phFuncTaSend(handlerID, "%s%s", varCommand, handlerID->p.gpibEndOfLine))
      != PHFUNC_ERR_OK) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "GPIB send failed");
    return tmpRet;
  }
  return PHFUNC_ERR_OK;
}

/*****************************************************************************
 *
 * Wait for GPIB reply from handler
 *
 * Functionality implemented
 *   
 *   Authors: Zuria Zhu
 *   Date:    2023/Jul/01
 *
 ***************************************************************************/
phFuncError_t private_gpib_receive(char *outReply, phFuncId_t handlerID)
{
  // log
  phLogFuncMessage(handlerID->myLogger, LOG_NORMAL, "GPIB wait reply");

  // receive
  if (phFuncTaReceive(handlerID, 1, "%511[^\r\n]", outReply) != PHFUNC_ERR_OK) {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "GPIB wait reply failed");
    return PHFUNC_ERR_GPIB;
  }
  return PHFUNC_ERR_OK;
}

/*****************************************************************************
 *
 * Ask handler - error occured?
 *
 * Functionality implemented
 *   
 *   Authors: Zuria Zhu
 *   Date:    2023/Jul/01
 *
 ***************************************************************************/
phFuncError_t private_checkHandlerForError(phFuncId_t handlerID)
{
  char tmpReply[1024];
  phFuncError_t tmpRet;

  // send
  if ((tmpRet = private_gpib_send("ERROR?", handlerID)) != PHFUNC_ERR_OK)
    return tmpRet;

  // send
  if ((tmpRet = private_gpib_receive(tmpReply, handlerID)) != PHFUNC_ERR_OK)
    return tmpRet;

  // check reply
  if (strncmp(tmpReply, "No Error", 8) == 0)
    return PHFUNC_ERR_OK;

  // log error
  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "Handler reported error: %s", tmpReply);
  return PHFUNC_ERR_FATAL;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
