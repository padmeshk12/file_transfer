/******************************************************************************
 *
 *       (c) Copyright Advantest Ltd., 2023
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 2023
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Runqiu Cao, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 2023, Runqiu Cao, created
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
#include <pthread.h>
#include <ctype.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_GuiServer.h"
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

#define RETEST_BIN_IN_HANDLER "R"
#define RETEST_BIN_IN_MSG  "A"
#define NO_BINNING_RESULT  "A"

static char handlerSiteName[MAX_SITES_LENGTH][64] = {""};
static pthread_t thread_receive_message;
static int hasRecvStartReq = 0;
static int hasCreatedThread = 0;
pthread_mutex_t lock;
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

void* receive_message(void* arg)
{
    phFuncId_t handlerID = (phFuncId_t)arg;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering %s...",__FUNCTION__);
    phComError_t comError = PHCOM_ERR_OK;
    phConfError_t confError;
    phComMode_t drvMode;
    char *commMode = NULL;
    double serverPort = 0.0;
    phComId_t comId;
    char buffer[MAX_HANDLER_ANSWER_LENGTH] = {'0'};
    int isContinue = 1;
    int length = 0;
    char *recvMes = NULL;

    /* get configuration for the communication link */
    /* today, only GPIO is supported. Future solutions will read a
       special configuration key first, to determine, which interface
       to open and how */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_MO_ONLOFF);
    if (confError == PHCONF_ERR_OK)
        confError = phConfConfString(handlerID->myConf,
            PHKEY_MO_ONLOFF, 0, NULL, (const char**)&commMode);
    if (confError != PHCONF_ERR_OK)
        commMode = NULL;

    /* further analysis of communication configuration */
    if (commMode == NULL)
        drvMode = PHCOM_ONLINE;
    else
        drvMode = strcasecmp(commMode, "on-line") == 0 ?
            PHCOM_ONLINE : PHCOM_OFFLINE;

    /* get server port by reading the field server_port in the configure file. */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_IF_LAN_PORT);
    if (confError == PHCONF_ERR_OK)
    {
        confError = phConfConfNumber(handlerID->myConf, PHKEY_IF_LAN_PORT,
                                    0, NULL, &serverPort);
        if (confError != PHCONF_ERR_OK)
        {
           phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,"must define the port number "
               "when set lan-server interface type, giving up(error %d)", (int) confError);
           return NULL;
        }
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "create LAN server socket with port \"%d\"",
        (int)serverPort);
    comError = phComLanServerCreateSocket(&(comId), drvMode, (int)serverPort, handlerID->myLogger);
    if (comError != PHCOM_ERR_OK)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL, "could not open LAN interface, "
            "giving up (error %d)", (int) comError);
        if (comId != NULL) {
            free(comId);
            comId = NULL;
        }
        return NULL;
    }

    while(1)
    {
        comError = phComLanServerWaitingForConnection(comId, handlerID->myLogger);
        if (comError == PHCOM_ERR_TIMEOUT)
        {
             //check system flag here
             long abortFlag = 0;
             phTcomGetSystemFlag(handlerID->myTcom, PHTCOM_SF_CI_ABORT, &abortFlag);
             if(abortFlag)
             {
                 phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL, "abort flag was set, "
                     "giving up on executing prober/handler driver call");
                 phComLanClose(comId);
                 if (comId != NULL) {
                     free(comId);
                     comId = NULL;
                 }
                 return NULL;
             }
             continue;
        }
        if (comError != PHCOM_ERR_OK)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL, "could not open LAN interface, giving up (error %d)", (int) comError);
            phComLanClose(comId);
            break;
        }

        while(1)
        {
            if (hasRecvStartReq)
                localDelay_us(handlerID->p.pollingInterval);
            else
            {
                comError = phComLanReceive(comId, (const char **)&recvMes, &length, 1000);
                if (comError != PHCOM_ERR_OK)
                {
                    if (comError == PHCOM_ERR_TIMEOUT)
                    {
                        localDelay_us(handlerID->p.pollingInterval);
                        continue;
                    } else if (comError == PHFUNC_ERR_LAN)
                    {
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL, "disconnect from handler.");
                        phComLanClose(comId);
                        break;
                    } else
                    {
                        break;
                    }
                }

                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "receive a message: %s.", recvMes);
                if(strncmp(recvMes,"StartRequest",12) == 0)
                {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Receive the start request message.");

                    // reply to client message "ACK StartRequest"
                     memset(buffer, 0, MAX_HANDLER_ANSWER_LENGTH);
                     strcpy(buffer, "ACK StartRequest\r\n");
                     phComLanSend(comId, buffer, strlen(buffer), 400);

                     pthread_mutex_lock(&lock);
                     hasRecvStartReq = 1;
                     pthread_mutex_unlock(&lock);
                }
                else
                {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Command %s received from handler client"
                        " isn't recognized.", recvMes);
                }
            }
        }
    }
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exiting %s...",__FUNCTION__);
    if (comId != NULL) {
        free(comId);
        comId = NULL;
    }
    return NULL;
}

/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
    phConfError_t confError;
    char *binningCommandFormat = NULL;

    /* retrieve the advantest_binnng_command_format */
    confError = phConfConfIfDef(handlerID->myConf, PHKEY_PL_SE_BINNING_COMMAND_FORMAT);
    if (confError == PHCONF_ERR_OK)
        confError = phConfConfString(handlerID->myConf, PHKEY_PL_SE_BINNING_COMMAND_FORMAT,
                                     0, NULL, (const char **)&binningCommandFormat);
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
        else
        {
            handlerID->p.binningCommandFormat = 16;
        }
    }
    else
    {
        handlerID->p.binningCommandFormat = 16; /* default 16-bin format*/
    }
    handlerID->p.verifyBins = 1;
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
    long population = 0L;
    long populationPicked = 0L;
    char* tempChr = 0;

    PhFuncTaCheck(phFuncTaSend(handlerID, "FULLSITES?%s", handlerID->p.eol));
    retVal = phFuncTaReceive(handlerID, 1, "%s", handlerAnswer);
    if(retVal != PHFUNC_ERR_OK) {
        phFuncTaRemoveStep(handlerID);
        return retVal;
    }

    if((localAnswer = strstr(handlerAnswer, "ACK OK")) != NULL)
    {
        /* ensure string is in upper case */
        setStrUpper(handlerAnswer);

        /* check answer from handler, similar for all handlers.  remove
           two steps (the query and the answer) if an empty string was
           received */
        if (sscanf(handlerAnswer, "ACK OK FULLSITES? FULLSITES=%lx", &population) != 1)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "site population not received from handler: \n"
                             "received \"%s\" format expected \"ACK OK FULLSITES? Fullsites=xxxxxxxx\" \n",
                             handlerAnswer);
            handlerID->p.oredDevicePending = 0;
            return PHFUNC_ERR_ANSWER;
        }

        tempChr = strchr(handlerAnswer, '=')+1;
        handlerID->p.fullsitesStrLen = strlen(tempChr);

        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"population:%ld site str len:%d...",
            population, handlerID->p.fullsitesStrLen);

        handlerID->p.oredDevicePending = 0;
        for(int i=0; i<handlerID->noOfSites; i++) {
            if(population & (1L << i)) {
              handlerID->p.devicePending[i] = 1;
              handlerID->p.oredDevicePending = 1;
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                               "device present at site \"%s\" (polled)",
                               handlerID->siteIds[i]);
              populationPicked |= (1L << i);
            } else {
              handlerID->p.devicePending[i] = 0;
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                               "no device at site \"%s\" (polled)",
                               handlerID->siteIds[i]);
            }
        }

        /* check whether we have received to many parts */
        if(population != populationPicked) {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                "The handler seems to present more devices than configured\n"
                "The driver configuration must be changed to support more sites");
        }
    }
    else if((localAnswer = strstr(handlerAnswer, "ACK ParameterError")) != NULL)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "For the respond of FULLSITES "
            "command, Parameter Error.\n received \"%s\"", handlerAnswer);
        return PHFUNC_ERR_ANSWER;
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "For the respond of FULLSITES "
            "command, the format is incorrect.\nreceived \"%s\"\nformat expected \"ACK OK FULLSITES? "
            "Fullsites=xxxxxxxx\"\n", handlerAnswer);
        return PHFUNC_ERR_ANSWER;
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
static phFuncError_t setupBinCode(phFuncId_t handlerID ,long binNumber ,char* binCode)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int binInt = 0;
    char tempBinCode[10] = "";

    strcpy(binCode,"");

    if (handlerID->binIds && binNumber >= 0)
    {
        /* if handler bin id is defined (hardbin/softbin mapping), get the bin code from the bin id list*/
        strcpy(tempBinCode, handlerID->binIds[binNumber]);
    }
    else
    {
        /* if handler bin id is not defined (default mapping), use the bin number directly */
        sprintf(tempBinCode, "%ld", binNumber);
    }

    /* make the bin code in the GPIB  binning command */
    if (strcmp(tempBinCode, RETEST_BIN_IN_HANDLER) == 0 || strcmp(tempBinCode, "-1") == 0)
    {
        /* retest bin */
        sprintf(binCode,"%s",RETEST_BIN_IN_MSG);
    }
    else
    {
        /* normal bins */
        if (sscanf(tempBinCode, "%d", &binInt) != 1)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                             "sscanf error, temp bin code is: %s", tempBinCode);
            return PHFUNC_ERR_BINNING;
        }
        if (binInt <= 9)
        {
            sprintf(binCode, "%d", binInt );
        }
        else
        {
            if ( (handlerID->p.binningCommandFormat == 16))
            {
                //check if the handler bin id is legal or not
                if (binInt > 16)
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                     "Invalid binning data.\n"
                                     "Could not bin to handler bin %ld. \n"
                                     "The max bin code allowed for 16-bin configuration is 16!",
                                      binInt);
                    return PHFUNC_ERR_BINNING;
                }

                /* For 16 bin configuration:
                 * Bin code in Binning Command: 0 1 2 3 4 5 6 7 8 9 A B  C  D  E  F  G  H
                 * Real bin code in handler:    0 1 2 3 4 5 6 7 8 9 R 10 11 12 13 14 15 16
                 * R means retest bin.
                 */
                sprintf(binCode, "%c", (char)(0x37 +  (binInt+1)) );
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "in order to send binning message, translate bin code from %d to \"%s\" (16-bin binning command)  ",
                                 binInt, binCode);

            }
            else
            {
                /* For 32 bin configuration
                 * Bin code in Binning Command: 0 1 2 3 4 5 6 7 8 9 A B  C  D  E  F  10 11 ... 20 21
                 * Real bin code in handler:    0 1 2 3 4 5 6 7 8 9 R 10 11 12 13 14 15 16 ... 31 32
                 * R means retest bin.
                 */
                sprintf(binCode, "%lx", (long unsigned int)(binInt+1));
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 "in order to send binning message, translate bin code from %d to \"%s\" (32-bin binning command)  ",
                                 binInt, binCode);
            }
        }
    }

    if(handlerID->binIds)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "handler_bin_ids is defined and handler_bin_ids[%ld]=\"%s\". In the GPIB command, \"%s\" is used as bin ID.",
                         binNumber, tempBinCode, binCode);
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "handler_bin_ids NOT defined, using hardbin number %ld directly. In the GPIB command, \"%s\" is used as bin ID.",
                         binNumber, binCode);
    }
    return retVal; 
}

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
    s=strchr(s,'=');
    r=strchr(r,'=');

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
static phFuncError_t binAndReprobe( phFuncId_t handlerID  /* driver plugin ID */,
                                    phEstateSiteUsage_t *oldPopulation /* current site population */,
                                    long *perSiteReprobe  /* TRUE, if a device needs reprobe*/,
                                    long *perSiteBinMap   /* valid binning data for each site where
                                                             the above reprobe flag is not set */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    char testresults[PHESTATE_MAX_SITES*2 + 512];
    char tmpCommand[256] = {0};
    char respond[128] = {0};
    static char querybins[PHESTATE_MAX_SITES*2 + 512];
    char thisbin[32];
    int i;
    int sendBinning = 0;
    static int retryCount = 0;
    int repeatBinning = 0;
    int maxNoOfSiteInCmd = 0;

    do
    {
        retVal = PHFUNC_ERR_OK;

        phFuncTaMarkStep(handlerID);

        switch (handlerID->model)
        {
        case PHFUNC_MOD_DLH:
            memset(testresults, 0, sizeof(testresults));
            strcpy(testresults, "BINON Parameter=");
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
                            if (handlerID->p.reprobeBinNumber <= 9)
                            {
                                //sprintf(tmpCommand, "%c", (char)(0x30 +  handlerID->p.reprobeBinNumber) );
                                sprintf(tmpCommand, "%d", handlerID->p.reprobeBinNumber );
                            }
                            else
                            {
                                if ( (handlerID->p.binningCommandFormat == 16))
                                {
                                    /* For 16 bin configuration:
                                     * Bin code in Binning Command: 0 1 2 3 4 5 6 7 8 9 A B  C  D  E  F  G  H
                                     * Real bin code in handler:    0 1 2 3 4 5 6 7 8 9 R 10 11 12 13 14 15 16
                                     * R means retest bin.
                                     */
                                    sprintf(tmpCommand, "%c", (char)(0x38 +  handlerID->p.reprobeBinNumber) );
                                }
                                else
                                {
                                    /* For 32 bin configuration
                                     * Bin code in Binning Command: 0 1 2 3 4 5 6 7 8 9 A B  C  D  E  F  10 11 ... 20 21
                                     * Real bin code in handler:    0 1 2 3 4 5 6 7 8 9 R 10 11 12 13 14 15 16 ... 31 32
                                     * R means retest bin.
                                     */
                                    sprintf(tmpCommand, "%lx", (long unsigned int)(handlerID->p.reprobeBinNumber+1));
                                }
                            }
                        }
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                         "will reprobe device at site \"%s\"",
                                         handlerID->siteIds[i]);
                        strcat(testresults, tmpCommand);
                        sendBinning = 1;
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
                    strcat(testresults, NO_BINNING_RESULT);
                    if ( i < handlerID->noOfSites )
                    {
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                         "no device to bin at site \"%s\"",
                                         handlerID->siteIds[i]);
                    }
                }

                if ( i != 0 )
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

        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "verify bin:%d snedBinning:%d ....", handlerID->p.verifyBins, sendBinning);
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
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", respond));
                if (strcmp(respond, "ACK OK ECHOOK") != 0)
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                   "The respond of ECHOOK is incorrect.");
            }
            else
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                 ".... verification failed");
                PhFuncTaCheck(phFuncTaSend(handlerID, "ECHONG%s",
                                           handlerID->p.eol));
                PhFuncTaCheck(phFuncTaReceive(handlerID, 1, "%s", respond));
                if (strcmp(respond, "ACK OK ECHONG") != 0)
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                   "The respond of ECHONG is incorrect.");
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

    retryCount = 0;
    return retVal;
}

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Runqiu Cao
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
    handlerID->p.verifyBins = 0;
    handlerID->p.verifyMaxCount = 0;
    handlerID->p.binningCommandFormat = 16;
    strcpy(handlerID->p.eol, "\r\n");

    pthread_mutex_init(&lock, NULL);
    /* Create a thread to receive the message from handler client.
     * And in the sub thread, the test acts as a server.*/
    if (!hasCreatedThread)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "create a sub-thread to receive start request from handler.");
        if(pthread_create(&thread_receive_message,NULL,&receive_message,handlerID) != 0)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"create thread failed");
            return PHFUNC_ERR_LAN;
        }
        else
            hasCreatedThread = 1;
    }

    /* assume, that the interface was just opened and cleared, so
     wait 1 second to give the handler time to be reset the
     interface */
    sleep(1);
//    pthread_mutex_destroy(&lock);

    return retVal;
}
#endif


#ifdef RECONFIGURE_IMPLEMENTED
/*****************************************************************************
 *
 * reconfigure driver plugin
 *
 * Authors: Runqiu Cao
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
 * Authors: Runqiu Cao
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
 * Authors: Runqiu Cao
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
 * Authors: Runqiu Cao
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
 * Authors: Runqiu Cao
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

    int loopCount = 0;
    while(!hasRecvStartReq)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "waiting for the message StartRequest.");
        localDelay_us(handlerID->p.pollingInterval);
        ++loopCount;
        if (loopCount > 1000)
        {
            long response = 0;
            char strMsg[128] = {0};
            snprintf(strMsg, 127, "it seems that have wait the message StartRequest for a long time, "
                "Do you want to abort or continue?");
            phGuiMsgDisplayMsgGetResponse(handlerID->myLogger, 0, "Info", strMsg, "abort",
                "continue", NULL, NULL, NULL, NULL, &response);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"response from the Dialog: %d", response);
            if (response == 3)
                loopCount = 0;
            else
                return PHFUNC_ERR_ABORTED;
        }
    }

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
 * Authors: Runqiu Cao
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

    pthread_mutex_lock(&lock);
    hasRecvStartReq = 0;
    pthread_mutex_unlock(&lock);

    return PHFUNC_ERR_OK;
}
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Runqiu Cao
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
 * Authors: Runqiu Cao
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
 * Authors: Runqiu Cao
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
    ReturnNotYetImplemented("privateCommTest");
}
#endif



#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Runqiu Cao
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
 * Authors: Runqiu Cao
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
 * Authors: Runqiu Cao
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

/*****************************************************************************
 *
 * for a certain SetStatus, get the corresponding GPIB command
 *
 * Authors: Runqiu Cao
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
    /* these static array must be ordered by the first field */
    static const  phStringPair_t sGpibCommands[] =
    {
        {PHKEY_NAME_HANDLER_STATUS_SET_ACM_CLEAR,"ACMCLEAR?"},
        {PHKEY_NAME_HANDLER_STATUS_SET_ACM_RETEST_CLEAR,"ACMRETESTCLEAR?"},
        {PHKEY_NAME_HANDLER_STATUS_SET_CHANGE_PAGE,"CHANGEPAGE"},
        {PHKEY_NAME_HANDLER_STATUS_SET_GPIB_TIMEOUT,"GPIBTIMEOUT"},
        {PHKEY_NAME_HANDLER_STATUS_SET_INPUT_QTY,"INPUTQTY"},
        {PHKEY_NAME_HANDLER_STATUS_SET_LOTCLEAR,"LOTCLEAR?"},
        {PHKEY_NAME_HANDLER_STATUS_SET_LOT_END,"LOTEND"},
        {PHKEY_NAME_HANDLER_STATUS_SET_LOT_RETEST_CLEAR,"LOTRETESTCLEAR?"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGENABLE,"PRETRGENABLE"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGINI,"PRETRGINI"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTTIME,"PRETRGOUTTIME"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTVALUE,"PRETRGOUTVALUE"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETMETHOD,"PRETRGRETMETHOD"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETVALUE,"PRETRGRETVALUE"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGSIGTYPE,"PRETRGSIGTYPE"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGTESTEND,"PRETRGTESTEND"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PRETRGWAITTIME,"PRETRGWAITTIME"},
        {PHKEY_NAME_HANDLER_STATUS_SET_PROFILE, "PROFILE" },
        {PHKEY_NAME_HANDLER_STATUS_SET_SITE_PROFILE, "SITEPROFILE:" },
        {PHKEY_NAME_HANDLER_STATUS_SET_SOFT_RESET,"SOFTRESET"},
        {PHKEY_NAME_HANDLER_STATUS_SET_SOUND_OFF,"SOUNDOFF"},
        {PHKEY_NAME_HANDLER_STATUS_SET_START, "START"},
        {PHKEY_NAME_HANDLER_STATUS_SET_STOP,"STOP"},
        {PHKEY_NAME_HANDLER_STATUS_SET_SYNCTESTER,"SYNCTESTER"},
        {PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGCYCLE,"TEMPLOGCYCLE"},
        {PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGLENGTH,"TEMPLOGLENGTH"},
        {PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSEND,"TEMPLOGSEND"},
        {PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSTART,"TEMPLOGSTART"},
        {PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSTOP,"TEMPLOGSTOP"},
    };
    static char buffer[MAX_STATUS_MSG] = "";
    int retVal = SUCCEED;
    const phStringPair_t *pStrPair = NULL;
    int paramSpecified = NO;
    int ignoreParam = NO;

    if (strlen(param) > 0 )
    {
        paramSpecified = YES;
        if (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGCYCLE) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGLENGTH) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGENABLE) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGSIGTYPE) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGWAITTIME) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTTIME) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTVALUE) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETMETHOD) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETVALUE) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_CHANGE_PAGE) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_GPIB_TIMEOUT) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_PROFILE) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_SITE_PROFILE) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_LOT_END) != 0 &&
            strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_SET_INPUT_QTY) != 0 )
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

#ifdef EQUIPID_IMPLEMENTED
/*****************************************************************************
 *
 * return the handler identification string
 *
 * Authors: Runqiu Cao
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
    {
        return PHFUNC_ERR_ABORTED;
    }

    phFuncTaStart(handlerID);
    PhFuncTaCheck(phFuncTaSend(handlerID, "VERSION?%s", handlerID->p.eol));
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

    return retVal;
}
#endif

/*****************************************************************************
 *
 * The stub function for set status
 *
 * Authors: Runqiu Cao
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
    static char response[MAX_STATUS_MSG] = "";
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char *gpibCommand = NULL;
    int found = FAIL;

    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "privateSetStatus, token = ->%s<-, param = ->%s<-", token, param);
    phFuncTaStart(handlerID);
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
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_0,
                        "privateSetStatus, %s returned: %s",
                        token, response);
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

static void getAllCoordinates(char *response)
{
	int i=0,j=0;
	for(;i<24;++i)
	{
		j+=sprintf(response+j,"%s,",handlerSiteName[i]);
	}
	return ;
}

/*****************************************************************************
 *
 * for a certain GetStatus query, get the corresponding GPIB command
 *
 * Authors: Runqiu Cao
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
    static const  phStringPair_t sGpibCommands[] =
    {
        /*Begin CR92686, Adam Huang, 31 Jan 2015*/
        {PHKEY_NAME_HANDLER_STATUS_GET_ACM_CONTACT, "ACMCONTACT?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_ACM_ERROR,"ACMERROR?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_ACM_JAM,"ACMJAM?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_ACM_RETRY,"ACMRETRY?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_ACM_TIME,"ACMTIME?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_ACM_TOTAL,"ACMTOTAL?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_CURRENT_ALM,"CURRENTALM?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_HANDLER_NUMBER,"HANDLERNUM?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_LDPOCKET,"LDPOCKET?"},  /*DLT enhance */
        {PHKEY_NAME_HANDLER_STATUS_GET_LDTRAY,"LDTRAY?"},      /*DLT enhance */
        {PHKEY_NAME_HANDLER_STATUS_GET_LOT_ERROR,"LOTERROR?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_LOT_JAM,"LOTJAM?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_LOT_RETRY,"LOTRETRY?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_LOT_TIME,"LOTTIME?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_LOT_TOTAL,"LOTTOTAL?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_MEASTEMP,"MEASTEMP?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_SETTEMP,"SETTEMP?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGCYCLE,"TEMPLOGCYCLE?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGLENGTH,"TEMPLOGLENGTH?"},
        {PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGSTS,"TEMPLOGSTS"},
        /*End*/
    };
    static char buffer[MAX_STATUS_MSG] = "";
    int retVal = SUCCEED;
    const phStringPair_t *pStrPair = NULL;
    int paramSpecified = NO;
    int ignoreParam = NO;

    if (strlen(param) > 0)
    {
        paramSpecified = YES;
        if (strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID) != 0)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                    "the status query does not require any parameters!\n"
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

#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The stub function for get status
 *
 * Authors: Runqiu Cao
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
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "privateGetStatus, token = ->%s<-, param = ->%s<-", commandString, paramString);

    phFuncError_t retVal = PHFUNC_ERR_OK;
    static char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    handlerAnswer[0] = '\0';
    *responseString = handlerAnswer;

    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    const char *token = commandString;
    const char *param = paramString;
    int found = FAIL;
    char *gpibCommand = NULL;
    found = getGpibCommandForGetStatusQuery(handlerID, &gpibCommand, token, param);
    phFuncTaStart(handlerID);
    if ( found == SUCCEED )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateGetStatus, gpibCommand = ->%s<-", gpibCommand);
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", gpibCommand, handlerID->p.eol));
        localDelay_us(100000);
        retVal = phFuncTaReceive(handlerID, 1, "%4095[^\r\n]", handlerAnswer);
        if (retVal != PHFUNC_ERR_OK)
        {
            phFuncTaRemoveStep(handlerID);
            return retVal;
        } 
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_0,
                        "privateGetStatus, %s returned: %s",
                        token, handlerAnswer);
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                         "The key \"%s\" is not available, or may not be supported yet", token);
        sprintf(handlerAnswer, "%s_KEY_NOT_AVAILABLE", token);
    }

    /* ensure null terminated */
    handlerAnswer[strlen(handlerAnswer)] = '\0';
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "privateGetStatus, answer ->%s<-, length = %d",
                     handlerAnswer, strlen(handlerAnswer));
    phFuncTaStop(handlerID);
    return retVal;
}
#endif

/*****************************************************************************
 * End of file
 *****************************************************************************/
