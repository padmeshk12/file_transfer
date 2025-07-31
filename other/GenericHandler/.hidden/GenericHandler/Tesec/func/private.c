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
 *            09 Jun 2000, Michael Vogt, adapted from prober driver
 *            02 Aug 2000, Michael Vogt, worked in all changes based on test
 *                         at Delta Design
 *            06 Sep 2000, Michael Vogt, adapted from Delta plug-in
 *            12 Sep 2000, Chris Joyce, modify according to answers to queries
 *                         received from Multitest 
 *            20 Mar 2001, Chris Joyce, added CommTest and split init into
 *                         init + comm + config
 *             9 Apr 2001, Siegfried Appich, changed debug_level from LOG_NORMAL
 *                         to LOG_DEBUG
 *             6 Feb 2002, Chris Joyce, modified Multitest to Tesec 
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
#include "dev/DriverDevKits.hpp"

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


typedef struct siteCoordinate
{
    int isValid ;
    int x ;
    int y ;
}SiteCoordinate;
driver_dev_kits::AccessPHVariable& aphv = driver_dev_kits::AccessPHVariable::getInstance();
static SiteCoordinate perSiteCoordinate[PHESTATE_MAX_SITES];
static char ringID[64];
/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static void setStrUpper(char *s1)
{
    int i=0;

    while ( s1[i] )
    {
        s1[i]=toupper( s1[i] );
        ++i;
    }
}
/* perSiteBinMap[] stores the handler bin index for each site , we need to using binIds[] to get their bin-name respectively */
static phFuncError_t setupBinCode(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *binNumber            /* for bin number   handlerbinindex if define mapped_hardbin */,
    char *binCode             /* array to setup bin code */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    long newbinNumber=*binNumber;
    strcpy(binCode,"");
    if (handlerID->currentBinMode!=PHBIN_MODE_DEFAULT) // if  define handler bin ids and not default mode
    {

        if ( *binNumber < 0 || *binNumber >= handlerID->noOfBins )
        {
            if(*binNumber==-1)
            {
                *binNumber=0;
            }
            else
            {
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                    "invalid binning data\n"
                    "could not bin to bin index %ld \n"
                    "maximum number of bins %d set by configuration %s",
                    *binNumber, handlerID->noOfBins,PHKEY_BI_HBIDS);
                retVal=PHFUNC_ERR_BINNING;
            }
        }
        else 
        {
            if( (handlerID->model == PHFUNC_MOD_5170_IH) && handlerID->binIds)
            {
                 int i_ret = strtol(handlerID->binIds[*binNumber], (char **)NULL, 10);
                 sprintf(binCode,"%02d", i_ret);
                 phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "using binIds set binNumber %d to binCode \"%s\" ", 
                        binNumber, binCode);
            }
            else
            {
                sprintf(binCode,"%s",handlerID->binIds[ *binNumber ]);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                 "using binIds set binNumber %d to binCode \"%s\" ", 
                                 *binNumber, binCode);
            }
        }
    }
    else
    {   // if not define handler bin ids or map-binning is default, the index (equals to hardbin )  will be the sended bin
        if (*binNumber < -1 || *binNumber > 63)
        {          
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_FATAL,
                "received illegal request for bin %lx ",
                *binNumber);
            retVal=PHFUNC_ERR_BINNING;
        }
        else
        {
            if(*binNumber==-1||(*binNumber>32&&*binNumber<64))
            {
               *binNumber=0;
            }
            if(handlerID->model == PHFUNC_MOD_5170_IH)
                sprintf(binCode,"%02d",*binNumber);// binnumber is hard bin of smartest,  hard bin +1 or add anything 
            //sprintf(binCode,"%c",binNumber);// binnumber is hard bin of smartest,  hard bin +1 or add anything 
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
               "using binNumber set hard binNumber %d to handler bin \"%d\" ", 
                newbinNumber, *binNumber);            
        }
    }

    return retVal;
}

static phFuncError_t  parseSiteInfoFor3270(phFuncId_t handlerID,char* handlerAnswer)
{
     /* ensure string is in upper case */
    setStrUpper(handlerAnswer);
    char tmppopulation[256]={0};    
    /* check answer from handler, similar for all handlers.  remove
       two steps (the query and the answer) if an empty string was
       received */
    if (sscanf(handlerAnswer, "D%s", tmppopulation) != 1)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "site population not received from handler: \n"
                         "received \"%s\" format expected \"Dxxxxxxxx\" \n",
                         handlerAnswer);
        handlerID->p.oredDevicePending = 0;
        return PHFUNC_ERR_ANSWER;
     }
     // find the value of population
     phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                      "device present at site \"%s\" (polled)", 
                      tmppopulation);
      /* Estate set*/
     char *start = NULL,*end = NULL;
     start=tmppopulation;
     while(((*start)&0xBf) == 0){
	start++;
	}
     end=tmppopulation;
     while(*end!='\0'){
       end++;
     }
     end--;
     int i=0,j=0;
     int partmissing=0;
     char tmpchar = 0;
     handlerID->p.oredDevicePending = 0;
     while(end>start)
     {
       tmpchar=0xBf & (*end);
       for(j=0;j<4;j++)
       {
         if(tmpchar & (1L<<j))
         {
             handlerID->p.devicePending[i*4+j] = 1;
             handlerID->p.oredDevicePending = 1;
             phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                              "device present at site \"%s\" (polled), pending=%d", 
                              handlerID->siteIds[i*4+j],handlerID->p.devicePending[i*4+j]);
         }
         else
         {
             handlerID->p.devicePending[i*4+j] = 0;
             if(handlerID->p.deviceExpected[i*4+j])
             {
                 partmissing=1;
             }
             phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                              "no device at site \"%s\" (polled)", 
                              handlerID->siteIds[i*4+j]);
         }
       }
       end--;          
       i++;
     }
     tmpchar=0xBf & (*end);
     if(tmpchar>=8)
       handlerID->p.current_test_sites=4+i*4;
     else if(tmpchar>=4)
       handlerID->p.current_test_sites=3+i*4;
     else if(tmpchar>=2)
       handlerID->p.current_test_sites=2+i*4;
     else if(tmpchar>=1) 
       handlerID->p.current_test_sites=1+i*4;
     else
       handlerID->p.current_test_sites=0+i*4;

     for(j=0;j<handlerID->p.current_test_sites-i*4;j++)
     {
       if(tmpchar & (1L<<j))
       {
           handlerID->p.devicePending[i*4+j] = 1;
           handlerID->p.oredDevicePending = 1;
           phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                            "device present at site \"%s\" (polled)", 
                            handlerID->siteIds[i*4+j]);
       }
       else
       {
           handlerID->p.devicePending[i*4+j] = 0;
           phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                            "no device at site \"%s\" (polled)", 
                            handlerID->siteIds[i*4+j]);
       }
     }

     // if current sites dismatch the number in configure, gives the warning
     if(handlerID->p.current_test_sites<handlerID->noOfSites)
     {
         phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_WARNING,
                          "The number of DUT is less than that in configure file");
     }
     if(partmissing)
     {
         phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                          "some expected device(s) seem(s) to be missing");
     }
     // end of Estate set
     phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                      "At this time the number of sites to be tested is \"%d\" ", 
                      handlerID->p.current_test_sites);
     /* check whether we have received too many parts */   
     if (handlerID->p.current_test_sites > handlerID->noOfSites)
     {
         phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                          "The handler seems to present more devices than configured\n"
                          "The driver configuration must be changed to support more sites");
     }
     return PHFUNC_ERR_OK;
}

static phFuncError_t  parseSiteInfoFor5170(phFuncId_t handlerID,char* handlerAnswer)
{
    char tmpSiteInfo[1024] = "";
    if (sscanf(handlerAnswer, "D%s", tmpSiteInfo) != 1)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                         "site population not received from handler: \n"
                         "received \"%s\" format expected \"Dxxxxxxxxx\" \n",
                         handlerAnswer);
        handlerID->p.oredDevicePending = 0;
        return PHFUNC_ERR_ANSWER;
    }

    char* token = tmpSiteInfo;
    int  isExceedConfig = 0;
    while( (token=strtok(token,",")) != NULL)
    {
        char tmp[4] = {0};
        strncpy(tmp,token,3);
        int siteNum = atoi(tmp);
        handlerID->p.devicePending[siteNum-1] = 1;
        handlerID->p.oredDevicePending = 1;
        if(siteNum > handlerID->noOfSites)
        {
            isExceedConfig = 1;
            break;
        }
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"device present at site \"%s\" (polled)", handlerID->siteIds[siteNum-1]);
        SiteCoordinate aCoordinate; 
        strncpy(tmp,token+3,3);
        aCoordinate.x = atoi(tmp);
        strncpy(tmp,token+6,3);
        aCoordinate.y = atoi(tmp);
        aCoordinate.isValid = 1;
        perSiteCoordinate[siteNum-1] = aCoordinate;
        phTcomSetDiePosXYOfSite(handlerID->myTcom,siteNum , aCoordinate.x, aCoordinate.y);
        token = NULL;
    }
    if(isExceedConfig)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
              "The handler seems to present more devices than configured\n"
              "The driver configuration must be changed to support more sites");
        return PHFUNC_ERR_ANSWER;
    }
    for (int i=0; i<handlerID->noOfSites; i++)
    {
        if(perSiteCoordinate[i].isValid)
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "device present at site \"%s\" (polled)", handlerID->siteIds[i]);
        else
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"no device at site \"%s\" (polled)", handlerID->siteIds[i]);
    }
    return PHFUNC_ERR_OK;
}
/* end of finding bin-name*/
/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;    
    char handlerAnswer[1024] = {0};    
    int i=0;

    switch(handlerID->model)
    {
        case PHFUNC_MOD_3270_IH:
        case PHFUNC_MOD_5170_IH:
          for(i=0;i<handlerID->noOfSites;i++)
          {
              handlerID->p.devicePending[i]=0;
          }
          PhFuncTaCheck(phFuncTaSend(handlerID, "D%s", handlerID->p.eol));
          retVal = phFuncTaReceive(handlerID, 1, "%1023[^\n\r]", handlerAnswer);
          if (retVal != PHFUNC_ERR_OK) 
          {
              phFuncTaRemoveStep(handlerID); // force re-send of command - kaw
              return retVal;
          }

          if(handlerID->model == PHFUNC_MOD_3270_IH)
            retVal = parseSiteInfoFor3270(handlerID,handlerAnswer);
          if(handlerID->model == PHFUNC_MOD_5170_IH)  
            retVal = parseSiteInfoFor5170(handlerID,handlerAnswer);
          break;
     
        default: break;     
    }

    
    return retVal;
}
static void updateRingID(char* ringID,int len)
{
   if(len>1 && ringID)
   {
       char realRingID[512] = "";
       strncpy(realRingID,ringID+1,len-1);
       aphv.setPHVariable("RINGID",realRingID); 
       strcpy(ringID,realRingID);
   }
   
}
/* end of poll */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
    int srq = 0;
    int srqPicked = 0;
    static int received = 0;
    /* Begin of Huatek Modification, Chris Wang, 06/14/2002 */
    /* Issue Number: 315 */
    /* End of Huatek Modification */
    int i;
    int partMissing = 0;
    char handlerAnswer[512] = {0};    
    phFuncError_t retVal = PHFUNC_ERR_OK;
    /* try to receive a start SRQ */
    PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 
      handlerID->p.oredDevicePending));
    if (received)
    {
      switch (handlerID->model)
        {
          case PHFUNC_MOD_3270_IH:
            if(srq & 0x40)
              {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                  "Test Request has been sent from handler, need to poll ");
                PhFuncTaCheck(pollParts(handlerID));
              }
                break;
          case PHFUNC_MOD_9588_IH:
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
                    else
                    { 
                        handlerID->p.devicePending[i] = 0;
                        if (handlerID->p.deviceExpected[i])
                            partMissing = 1;
                    }
                }

               /* 
                    check whether some parts seem to be missing. In
                    case of a reprobe, we must at least receive the
                    reprobed devices. In all other cases we should
                    receive devices in all active sites.
                */
 
                if (partMissing)
                {
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "some expected device(s) seem(s) to be missing");
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
                /* some empty SRQ occured this my be as expected when:           */
                /* The Power switch of the Handler is turned ON.                 */
                /* The Handler has received a DC(Device Clear) signal            */
                /* as documented in Specification for 9588-IH                    */
                /* tester interface (Rev.2 2000/8/19) page 5/11                  */
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                 "received SRQ 0x%02x", srq);
            }
            break;
          case PHFUNC_MOD_5170_IH:
            switch(srq)
            {
                case 0x41:
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received start of ring test SRQ(0x41)");
                handlerID->p.isStripEnd = 0;
                aphv.setPHVariable("isStripEnd","false"); 
                PhFuncTaCheck(phFuncTaSend(handlerID, "I%s", handlerID->p.eol));
                retVal = phFuncTaReceive(handlerID, 1, "%511[^\n\r]", handlerAnswer);
                if (retVal != PHFUNC_ERR_OK) 
                {
                    phFuncTaRemoveStep(handlerID); // force re-send of command 
                    return retVal;
                }
                updateRingID(handlerAnswer,sizeof(handlerAnswer));
                break;
                case 0x40:
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received test request SRQ(0x40),need to poll");
                PhFuncTaCheck(pollParts(handlerID));
                break;
                case 0x43:
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received end of ring test SRQ(0x43)");
                handlerID->p.isStripEnd = 1;
                aphv.setPHVariable("isStripEnd","true"); 
                return PHFUNC_ERR_OK;
                case 0x44:
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received end of lot SRQ(0x44)");
                aphv.setPHVariable("isLotEnd","true");
                return PHFUNC_ERR_LOT_DONE;
                default:
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received unknown SRQ(%d)",srq);
                return PHFUNC_ERR_ANSWER;

            }
            break;  
          default:
            break; 
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

    char binningCommand[PHESTATE_MAX_SITES + 16];
    char tmpCommand[16];
    int i;
    int sendBinning = 0;
    char thisbin[16];
    int tmpbin;

    switch(handlerID->model)
    {
        case PHFUNC_MOD_3270_IH:
            sprintf(binningCommand,"S%c",(char)(0x40+handlerID->p.current_test_sites));
            //sprintf(binningCommand,"S%c",(char)(0x40+handlerID->noOfSites));
            /* The perSiteBinMap[] comes from f->deviceBin[] , and the array is valued in function prepareBinOneSite() in device.c
               For example, when site=0, its smartest site is stored at stToHandlerSiteMap[site] .
               I think the index i here coming from  0 - (noOfSites-1)  refers to  the handler site, so the perSiteBinMap also
               refers to handler bin.
            */
            for(i=handlerID->p.current_test_sites-1;i>-1&&retVal==PHFUNC_ERR_OK;i--)  
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
                        else{
                            /* call Reprobe*/
                            sprintf(tmpCommand,"%c",handlerID->p.reprobeBinNumber);// This bin num means reprobe
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                      "Site \"%s\" needs reprobe",
                                      handlerID->siteIds[i]);  // sirteIds is a char pointer pointed to a string(id name) with maximum lenght
                            strcat(binningCommand,tmpCommand);
                        }
                            
                    }
                    else  /* do not need reprobe*/
                        {
                       /* sprintf(tmpCommand,"%c",(char)(0x40+i));
                          strcat(binningCommand, tmpCommand);
                          sendBinning = 1;*/                     
                        retVal=setupBinCode(handlerID, &perSiteBinMap[i], thisbin);
                        tmpbin=atoi(thisbin);
                        if(tmpbin>63 || tmpbin <0)
                        {
                          phLogFuncMessage(handlerID->myLogger,PHLOG_TYPE_FATAL,
                            "receive illegal request for bin %ld at site \"%s\"",
                            perSiteBinMap[i],handlerID->siteIds[i]);
                            return PHFUNC_ERR_BINNING;
                        } 
                        if(retVal==PHFUNC_ERR_OK)
                        {
                          //phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                          //      "perSiteBinMap[%d] is %ld", 
                          //      i,perSiteBinMap[i]);
                          if(*thisbin!='\0')
                          {
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                              "will bin device at site \"%s\" to %s", 
                              handlerID->siteIds[i], thisbin);
                            sprintf(tmpCommand,"%c",(char)(0x40+tmpbin));
                          }
                          else
                          {
                            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                              "will bin device at site \"%s\" to %d", 
                              handlerID->siteIds[i], perSiteBinMap[i]);
                            sprintf(tmpCommand,"%c",(char)(0x40+perSiteBinMap[i]));
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
                  strcat(binningCommand,"@");
                }
            }
            strcat(binningCommand,"Q");
            break;
      case PHFUNC_MOD_9588_IH:

        /* add Condition to binning command       */
        /* page 7/11 of Specification for 9588-IH */
        /*                                        */
        /* Single Site = 'A' = 41H                */
        /*   Dual Site = 'B' = 42H                */
        /* Triple Site = 'C' = 43H                */
        /*   Quad Site = 'D' = 44H                */
        /*                                        */
        /* use fact that '@' = 40H                */
        /* add number to sites                    */
        /*                                        */
        sendBinning=1;
        sprintf(binningCommand, "S%c", (char)( 0x40 + handlerID->noOfSites ));

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
                        sprintf(tmpCommand, "%c", (char)(0x40 +  handlerID->p.reprobeBinNumber) );
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                           "will reprobe device at site \"%s\"", 
                        handlerID->siteIds[i]);
                        strcat(binningCommand, tmpCommand);
                    }
                }
                else
                {
                    /* 
                     *  maximum bin category is 15
                     */
                    if (perSiteBinMap[i] < 0 || perSiteBinMap[i] > 15)
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
                        sprintf(tmpCommand, "%c", (char)(0x40 +  perSiteBinMap[i]) );
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                                         "will bin device at site \"%s\" to %ld", 
                                         handlerID->siteIds[i], perSiteBinMap[i]);
                        strcat(binningCommand, tmpCommand);
                    }
                }
             }
             else
             {
                 /* there is no device here fill space with 0x40 '@' */

                 phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                                  "no device to reprobe or bin at site \"%s\"", 
                                  handlerID->siteIds[i]);
                strcat(binningCommand, "@");
             }
        }
        /* add end of binning character */
        strcat(binningCommand, "Q");
        break;
      case PHFUNC_MOD_5170_IH:
        binningCommand[0] = 'S';
        for(int i=handlerID->noOfSites-1; i>=0;--i )
        {
            if(handlerID->activeSites[i] && (oldPopulation[i]==PHESTATE_SITE_POPULATED||
               oldPopulation[i]==PHESTATE_SITE_POPDEACT)) 
            {
                retVal=setupBinCode(handlerID, &perSiteBinMap[i], thisbin);
                if(retVal == PHFUNC_ERR_OK)
                {
                    int num = strtol(handlerID->siteIds[i],NULL,10); 
                    char siteId[24] = "";
                    sprintf(siteId,"%03d",num);
                    strcat(binningCommand,siteId);
                    strcat(binningCommand,thisbin);
                    if(i>0)
                      strcat(binningCommand,",");
                    else
                      strcat(binningCommand,"Q");
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"will bin device at site \"%s\" to %s",siteId,thisbin);
                    sendBinning = 1;
                }
            }
            else
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"no device at site \"%s\"",handlerID->siteIds[i]);

        }
        if(binningCommand[strlen(binningCommand)-1] == ',')
            binningCommand[strlen(binningCommand)-1] = 'Q';
      default:
        break;
    }
    if ( retVal==PHFUNC_ERR_OK && sendBinning==1)
    {
        PhFuncTaCheck(phFuncTaSend(handlerID, "%s%s", 
            binningCommand, handlerID->p.eol));
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                         "send command \"%s\"", 
                         binningCommand);
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
/* Begin of Huatek Modification, Chris Wang, 06/14/2002 */
/* Issue Number: 315 */
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int found = 0;
    phConfError_t confError;
    double dReprobeBinNumber;
    static int firstTime=1;
/* End of Huatek Modification */

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

/* Begin of Huatek Modification, Chris Wang, 06/14/2002 */
/* Issue Number: 315 */
/* End of Huatek Modification */

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

    strcpy(handlerID->p.eol, "\r\n");
    handlerID->p.isStripEnd = 0;
    aphv.setPHVariable("RINGID", "0");


    
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
    /* by default not implemented */
    ReturnNotYetImplemented("privateEquipID");
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
    for (i=0; i<handlerID->noOfSites; i++){
        handlerID->p.deviceExpected[i] = handlerID->activeSites[i];
        perSiteCoordinate[i].isValid = 0;
    }

    phFuncTaStart(handlerID);

    phFuncTaMarkStep(handlerID);
    PhFuncTaCheck(waitForParts(handlerID));
	
    if(handlerID->p.isStripEnd)
    {
        phTcomSetSystemFlag(handlerID->myTcom, PHTCOM_SF_CI_LEVEL_END, 1L);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"strip end SRQ receied, set LEVEL END flag to break current LEVEL");
        phFuncTaStop(handlerID);
        return PHFUNC_ERR_LOT_DONE;
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
    *diag = phLogGetLastErrorMessage();
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


#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The private function for getting status
 *
 * Authors: Jax wu
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

    if( (0 == strcasecmp(token, "x_coordinate")) || (0 == strcasecmp(token, "y_coordinate"))  )
    {
      if( (PHFUNC_MOD_5170_IH == handlerID->model) )
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
            static char coordinate[16] = "";
            if( 0 == strcasecmp(token, "x_coordinate") )
                sprintf(coordinate,"%d", perSiteCoordinate[iSiteNum-1].x);
            else if( 0 == strcasecmp(token, "y_coordinate") )
                sprintf(coordinate,"%d", perSiteCoordinate[iSiteNum-1].y);
            *responseString = coordinate;
          }
        }
      }
      else
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"the current mode:%s not support coordinate",handlerID->model);

    }
    else
     phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"privateGetStatus, the token = ->%s< is not supported yet", token); 

    /* ensure null terminated */
    response[strlen(response)] = '\0';

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "privateGetStatus, answer ->%s<-, length = %d",
                     response, strlen(response));

    phFuncTaStop(handlerID);

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


#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot start signal from handler
 *
 * Authors: Jax wu
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
    int received = 0;
    int srq = 0;
    switch(handlerID->model)
    {
        case PHFUNC_MOD_5170_IH:
           PhFuncTaCheck(phFuncTaTestSRQ(handlerID, &received, &srq, 0));
           if (received && (srq == 0x42))
           {
             retVal = PHFUNC_ERR_OK;
             aphv.setPHVariable("isLotEnd","false");
           }
           else
             retVal =  PHFUNC_ERR_WAITING; // if don't receive the SRQ or SRQ not equal to lot start(0x42),just return wait and repeat all again.
           break;
        default:
           break;
    }
    
    if(retVal == PHFUNC_ERR_OK)
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Load lot successfully,exit privateLotStart");
    else
    {
        phFuncTaRemoveToMark(handlerID);
        return retVal;
    }
    phFuncTaStop(handlerID);
    return retVal;
    
 
}
#endif

#ifdef STRIPMATERIALID_IMPLEMENTED
/*****************************************************************************
 *
 * return the test-on-strip material ID.
 *
 * Authors: Jax Wu
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
    *materialIdString = ringID;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                         "privateStripMaterialID  strip ID  = %s\n", *materialIdString);
    return retVal;

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
    /* by default not implemented */
    ReturnNotYetImplemented("privateCommTest");
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


/*****************************************************************************
 * End of file
 *****************************************************************************/
