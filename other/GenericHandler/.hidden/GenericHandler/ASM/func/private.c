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
 * AUTHORS  : Jax Wu, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 2019, Jax Wu, created
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
#define MAX_STRIP_ID_LENGTH 128
#define MAX_SITES_LENGTH 1024

static char handlerSiteName[MAX_SITES_LENGTH][64] = {""};
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

#define PointerCheck(x) \
    { \
        if ( (x) == NULL) \
        { \
            return PHFUNC_ERR_ANSWER; \
        } \
    } 

#define ChecksiteInfo(x) \
    { \
        if ( (x) == NULL) \
        { \
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"the length of device info should be 3,current length is 0 !!!" );\
            return PHFUNC_ERR_ANSWER; \
        } \
        if ( strlen( (x) ) != 3) \
        { \
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"the length of device info should be 3,current length is %d!",strlen( (x) ) );\
            return PHFUNC_ERR_ANSWER; \
        } \
    }
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

/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
    return PHFUNC_ERR_OK;
}

static phFuncError_t parseSiteInfo(phFuncId_t handlerID,const char* sitesInfo,int *indexMax,int* indexElementCount,int * index)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    PointerCheck(sitesInfo);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"sites info is:%s",sitesInfo);
    char tmpSitesInfo[MAX_HANDLER_ANSWER_LENGTH] = "";
    strcpy(tmpSitesInfo,sitesInfo);
    //get first site index of DUT
    char *token = strtok(tmpSitesInfo,",");
    ChecksiteInfo(token);
    char start = token[1];
    //device info will be "AA0,AA1 ... AH4,AH5" or "AI0,AI1 ... AP4,AP5" or "AQ0,AQ1 ... AX4,AX5"
    //we need to ensure what is the first character
    if('D'-start >=0 && 'D' -start <=3)
      start = 'A';
    else if('H'-start >=0 && 'H' -start <=3)
      start = 'E';
    else if('L'-start >=0 && 'L' -start <=3)
      start = 'I';
    else if('P'-start >=0 && 'P' -start <=3)
      start = 'M';
    else if('T'-start >=0 && 'T' -start <=3)
      start = 'Q';
    else if('X'-start >=0 && 'X' -start <=3)
      start = 'U';

    int i=0 ;
    handlerID->p.isNewStripOptional = 1;  
    while(token)
    {
      ChecksiteInfo(token);
      strcpy(handlerSiteName[i],token);
      index[i++] = (token[1] -start) * 6 + atoi( &token[2] ) + 1;
      token = strtok(NULL,",") ;
    }
    *indexMax = index[i-1];
    *indexElementCount = i;
    return retVal;
}
static phFuncError_t getDeviceContactInfo(phFuncId_t handlerID,const char* handlerAnswer)
{
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s...",__FUNCTION__);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Receive message is:%s",handlerAnswer);
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char tmpHandlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = "" ;
    int  index[MAX_SITES_LENGTH],indexElementCount=0,indexMax = 0,isRange = 0,index_start=0;
    memset(index,0,MAX_SITES_LENGTH);
    strncpy(tmpHandlerAnswer,handlerAnswer,MAX_HANDLER_ANSWER_LENGTH-1);
    if(strncmp(handlerAnswer,"START",5)==0 )
    {
        if( strncmp(handlerAnswer,"START RANGE",11)==0 )
        {
            //old case: the handler response is: "START RANGE X X"
            if(strlen(handlerAnswer) == 15)
            {
                 // if handler response is "START RANGE X X",the first "x" indicate start index, the second indicate end index;
                 // for example: "START RANGE 0 7 ", start index is 0, end index is 7
                 char *p = strstr(tmpHandlerAnswer,"RANGE");
                 index_start = atoi(p+6);
                 int index_end = atoi(p+8);
                 indexElementCount = indexMax = index_end +1;
                 isRange = 1;
            }
            //new case:handler response is:"START RANGE "strip_id" AA0,AA1,AA2..."
            else 
            {
               PointerCheck( strtok(tmpHandlerAnswer," ") );
               PointerCheck( strtok(NULL," ") );
               //get strip ID,if strip id is empty,this token point to the site index info
               char *token = strtok(NULL," ");
               if(strstr(token,"\""))
               {
                 // remove quote from strip Id
                 char stripId[MAX_STRIP_ID_LENGTH] = {0};
                 strcpy(stripId, token);
                 int i;
                 for(i=strlen(stripId)-1; i>=0; i--) {
                   if(stripId[i] == '\"') strcpy(&stripId[i], &stripId[i+1]);
                 }

                 strcpy(handlerID->p.stripId, stripId);
                 phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the strip id  is:%s",handlerID->p.stripId);
                 token = strtok(NULL," ");
                 retVal = parseSiteInfo(handlerID,token,&indexMax,&indexElementCount,index);
               }
               else
               {
                    //we do not know whether the response contain \" when there is no strip id entered, so add this branch to handle it!
                    retVal = parseSiteInfo(handlerID,token,&indexMax,&indexElementCount,index);

               }
            }
            
        }
        else if(strncmp(handlerAnswer,"START EXPLICIT",14) == 0)
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering START EXPLICIT ");
            strtok(tmpHandlerAnswer," ");
            strtok(NULL," ");
            char *token = strtok(NULL,",");
            int i = 0;
            while(token)
            {
              index[i] =  atoi(token) + 1 ;
              token = strtok(NULL,",");
              if(indexMax < index[i])
                indexMax= index[i++]  ;
              else
                ++i;
            } 
            indexElementCount = i;
        }
    }
    else if( strncmp(handlerAnswer,"RESTART",7) == 0 )
    {
       if(strncmp(handlerAnswer,"RESTART RANGE",13) == 0)
       {
             // if handler response is "RESTART RANGE X X",the first "x" indicate start index, the second indicate end index;
             // for example: "START RANGE 0 7 ", start index is 0, end index is 7
             char *p = strstr(tmpHandlerAnswer,"RANGE");
             index_start = atoi(p+6);
             int index_end = atoi(p+8);
             indexElementCount = indexMax = index_end +1;
             isRange = 1; 
             handlerID->p.isRetest  = 1;
       }
       else if(strncmp(handlerAnswer,"RESTART EXPLICIT",16) == 0)
       {
            strtok(tmpHandlerAnswer," ");
            strtok(NULL," ");
            char *token = strtok(NULL,",");
            int i = 0;
            while(token)
            {
              index[i] =  atoi(token) + 1 ;
              token = strtok(NULL,",");
              if(indexMax < index[i])
                indexMax= index[i++]  ;
              else
                ++i;
            } 
            indexElementCount = i;
            handlerID->p.isRetest  = 1;
       }
    }

    /* check whether we have received too many parts */
    if( indexMax > handlerID->noOfSites)
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "The handler seems to present more devices than configured\n"
          "The driver configuration must be changed to support more sites,current enableSites:%d,max support sites:%d",indexMax,handlerID->noOfSites);
      return PHFUNC_ERR_ANSWER;
    }
    //as customer maybe change the site number in production, we need to reinit the devicePending value
    int i=0;
    for ( i=0; i<handlerID->noOfSites; i++)
    {
        handlerID->p.devicePending[i] = 0;
    }

    handlerID->p.oredDevicePending = 1;
    if(isRange)
    {
      for( i=index_start; i < indexElementCount && i<MAX_SITES_LENGTH ; ++i)
      {
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"device present at site \"%s\" (polled)", handlerID->siteIds[i]);
          handlerID->p.devicePending[i] = 1;
      }
    }
    else
    {
      for(  i=0; i < indexElementCount && i<MAX_SITES_LENGTH ; ++i)
      {
          if(index[i] >= 1 )
          {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"device present at site \"%s\" (polled)", handlerID->siteIds[index[i]-1]);
            handlerID->p.devicePending[index[i]-1] = 1;
          }
      }
    }
    
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
    return retVal;
}
/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s ...",__FUNCTION__);
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {'0'};
    char* localAnswer = 0;

    PhFuncTaCheck(phFuncTaSend(handlerID, "RUN%s", handlerID->p.eol));
    PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s",handlerAnswer));
    if(strncmp(handlerAnswer,"END_OF_LOT",10) == 0)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Lot end from handler detected...");
        return PHFUNC_ERR_LOT_DONE;
    }

    if((localAnswer = strstr(handlerAnswer, "RESTART")) != NULL)
    {
      retVal = getDeviceContactInfo(handlerID, localAnswer);
    }
    else if((localAnswer = strstr(handlerAnswer, "START")) != NULL)
    {
      retVal = getDeviceContactInfo(handlerID, localAnswer);
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
    return retVal;
}

/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s...",__FUNCTION__);
    int i =0;
    while(1)
    {
        if(i==5)
          return PHFUNC_ERR_WAITING;
        ++i;
        PhFuncTaCheck(pollParts(handlerID));
        if(!handlerID->p.oredDevicePending)
          localDelay_us(handlerID->p.pollingInterval);
        else
          break;
    }
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
    return PHFUNC_ERR_OK;
}
static phFuncError_t setupBinCode(phFuncId_t handlerID ,long binNumber ,char* thisBin)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
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
            return PHFUNC_ERR_BINNING;
        }
        int i_ret = strtol(handlerID->binIds[binNumber], (char **)NULL, 10);
        sprintf(thisBin,"%d", i_ret);
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"using default bin map,handler bi id is:%d", binNumber);
        sprintf(thisBin,"%ld",binNumber);
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
    char thisBin[64] = "",tmpHandlerAnswer[128] = "";
    char testResult[MAX_HANDLER_ANSWER_LENGTH] = ""; 
    int sendBinning = 0,i = 0;
    strcpy(testResult,"RESULT ");
    switch(handlerID->model)
    {
        case PHFUNC_MOD_TFS:
            for( i=0;i<handlerID->noOfSites;++i)
            {
                if(handlerID->activeSites[i] && (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
                   oldPopulation[i] == PHESTATE_SITE_POPDEACT) )
                {
                  retVal = setupBinCode(handlerID,perSiteBinMap[i],thisBin);
                  if ( retVal==PHFUNC_ERR_OK )
                  {
                    if(handlerID->p.isNewStripOptional)
                    {
                      strcat(testResult,handlerSiteName[i]);
                      strcat(testResult,",");
                    }
                    strcat(testResult, thisBin);
                    if(handlerID->p.isNewStripOptional)
                      strcat(testResult,",");
                    else
                      strcat(testResult," ");
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "will bin device at site \"%s\" to %s", 
                        handlerID->siteIds[i], thisBin);
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
            break;
        default:
            break;
    }
    handlerID->p.isNewStripOptional = 0;
    if(sendBinning)
    {
       PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", testResult,handlerID->p.eol));
       PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s",tmpHandlerAnswer));
    }

    return retVal;
}

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Jax Wu
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
    }

    handlerID->p.strictPolling = 1; //always polling since it's acted as a client roll
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
    handlerID->p.status = 0L;
    handlerID->p.paused = 0;
    handlerID->p.isNewStripOptional = 0;
    handlerID->p.isRetest = 0;
    handlerID->p.deviceGot = 0;
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
 * Authors: Jax Wu
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
 * Authors: Jax Wu
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

#ifdef STRIPSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for strip start signal from handler
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStripStart(
    phFuncId_t handlerID     /* driver plugin ID */,
    int timeoutFlag 
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateStripStart, timeoutFlag = %s", timeoutFlag ? "TRUE" : "FALSE");
    retVal = privateGetStart(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit privateStripStart... ");
    return retVal;
}
#endif

#ifdef STRIPDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for strip end signal from handler
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateStripDone(
    phFuncId_t handlerID     /* driver plugin ID */,
    int timeoutFlag 
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateStripDone, timeoutFlag = %s", timeoutFlag ? "TRUE" : "FALSE");
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit privateStripDone... ");
    return retVal;
}
#endif

#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next device
 *
 * Authors: Jax Wu
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
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s...",__FUNCTION__);
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;
    if(handlerID->p.deviceGot)
    {
         phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"device has got ");
         return PHFUNC_ERR_OK;
    }
      
    phFuncTaStart(handlerID);
    phFuncTaMarkStep(handlerID);
    waitForParts(handlerID);
 

    /* do we have at least one part? If not, ask for the current status and return with waiting */
    if (!handlerID->p.oredDevicePending)
    {
        /* during the next call everything up to here should be repeated */
        phFuncTaRemoveToMark(handlerID);
        return PHFUNC_ERR_WAITING;
    }
    phFuncTaStop(handlerID);
    /* we have received devices for test. Change the equipment specific state now */
    int i = 0;
    handlerID->p.oredDevicePending = 0;
    for ( i = 0; i < handlerID->noOfSites; i++)
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
    }
    handlerID->p.deviceGot = 1;
    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
    return PHFUNC_ERR_OK;
}
#endif



#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested device
 *
 * Authors: Jax Wu
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
    
    handlerID->p.deviceGot = 0;
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
 * Authors: Jax Wu
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
 * Authors: Jax Wu
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


#ifdef COMMTEST_IMPLEMENTED
/*****************************************************************************
 *
 * Communication test
 *
 * Authors: Jax Wu
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
 * Authors: Jax Wu
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
 * Authors: Jax Wu
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
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateLotStart, %s", timeoutFlag ? "TRUE" : "FALSE");

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Exiting privateLotStart, retVal = %d", retVal);
    return retVal;
}
#endif

#ifdef LOTDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot end signal from handler
 *
 * Authors: Jax Wu
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

    return retVal;
}
#endif

static void getAllCoordinates(char *response)
{
	int i=0,j=0;
	for(;i<24;++i)
	{
		j+=sprintf(response+j,"%s,",handlerSiteName[i]);
	}
	return ;
}

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
	if(strcasecmp(token,"stripId") == 0)
		strcpy(handlerAnswer,handlerID->p.stripId);
	else if(strcasecmp(token,"xy_coordinate") == 0 )
	{
		if(param == NULL || param[0] == '\0' )
		{
			getAllCoordinates(handlerAnswer);
		}
		else
		{
			int index = atoi(param);
			if(index == 0)
			{
				phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_WARNING,"site index is beginning with 1,change index 0 to 1");
				index = 1;
			}
			strcpy(handlerAnswer,handlerSiteName[index-1]);
		}
	}
	else if(strcasecmp(token,"x_coordinate_int") == 0 )
	{
		if(param != NULL && param[0] != '\0' )
		{
			int index = atoi(param);
			if(index == 0)
			{
				phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_WARNING,"site index is beginning with 1,change index 0 to 1");
				index = 1;
			}
			strncpy(handlerAnswer,handlerSiteName[index-1]+2,1);
		}
	}
	else if(strcasecmp(token,"y_coordinate") == 0 )
	{
		if(param != NULL && param[0] != '\0' )
		{
			int index = atoi(param);
			if(index == 0)
			{
				phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_WARNING,"site index is beginning with 1,change index 0 to 1");
				index = 1;
			}
			strncpy(handlerAnswer,handlerSiteName[index-1],2);
		}
	}

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE, "privateGetStatus, answer ->%s<-, length = %d", handlerAnswer, strlen(handlerAnswer));

    return retVal;
}
#endif

/*****************************************************************************
 * End of file
 *****************************************************************************/
