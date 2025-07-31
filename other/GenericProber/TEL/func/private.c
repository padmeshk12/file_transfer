/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 26 Jan 2000
 *
 * CONTENTS : Private prober specific implementation
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, CR 15358
 *            Jiawei Lin, R&D Shanghai, CR27092 & CR25172
 *            David Pei,  R&D Shanghai, CR31014       
 *            Kun Xiao, R&D Shanghai, CR21589    
 *            Danglin Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *            June 2005, CR 15358:
 *                Introduce a new Driver Config Parameter "tel_binning_method"
 *                to represent the "parity" or "binary" binning method. For P12
 *                model, both of binning methods are allowed, whereas other
 *                models like 20S, 78S, P8 only use "parity" binning method.
 *                With "binary", the capability of 256 bins are supported;
 *                "parity" method just supports up to 32 bins.
 *                Now, P-12 model is offically released to the customers
 *            August 2005, Jiawei Lin, CR27092 & CR25172
 *                declare the "privateGetStatus". "getStatusXXX" series functions
 *                are used to retrieve the information or parameter from Prober.
 *                The information is WCR.
 *            April 2006, David Pei, CR31014
 *                Define the function "convertDieLocation" to fix the
 *                negative coordinate issue when using TEL prober.
 *            July 2006, Fabarca & Kun Xiao, CR21589
 *              Define the "privateContactTest". This function performs contact test to
 *              get z_contact_height automatically.
 *              modifiy function "privateGetStatus" to support retrieving z_contact_height
 *              and planarity_window from Prober.
 *            February 2007, Kun Xiao, CR33707
 *              support retrieving more parameters from TEL prober. The parameters are:
 *              casse_status, chunck_temp, error_code, lot_number, multisite_location_number,
 *              prober_step_name, wafer_id, wafer_number and wafer_status.
 *            Nov 2008, CR41221 & CR42599
 *               Implement the "privateExecGpibCmd". this function performs to send prober setup and action command by gpib.
 *               Implement the "privateExecGpibQuery". this function performs to send prober query command by gpib.
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
#include <ctype.h>
#include <string.h>
#include <assert.h>
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 285 */
#include <ctype.h>
/* End of Huatek Modification */

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_GuiServer.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_pfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "ph_pfunc_private.h"
#include "transaction.h"

/*CR21589, Fabarca & kun xiao, 19 Jun 2006*/
#include "ci_types.h"
/*End CR21589*/

/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define MY_EOC "\r\n"
#define NO_CASS_HANDLING

/* this macro allows to test a not fully implemented plugin */

/* #define ALLOW_INCOMPLETE */
#undef ALLOW_INCOMPLETE

#ifdef ALLOW_INCOMPLETE
#define ReturnNotYetImplemented(x) \
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR, \
        "%s not yet implemented", x); \
    return PHPFUNC_ERR_OK
#else
#define ReturnNotYetImplemented(x) \
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR, \
        "%s not yet implemented", x); \
    return PHPFUNC_ERR_NA
#endif

#define ReturnOnError(x) \
    if ((x) != PHPFUNC_ERR_OK) \
        return (x)

/*Begin CR21589, Fabarca & Kun Xiao,28 Jun 2006*/
#define CONTACTTEST_MIN_STEP      1
#define CONTACTTEST_MAX_STEP     99

#define SIMULATE_DCCTEST

#ifdef SIMULATE_DCCTEST
  #define REAL_FIRST_SOME_PASS_Z_HEIGHT 210 /*real z height of first some pass for simulation*/
  #define REAL_ALL_PASS_Z_HEIGHT 270  /*real z height of all pass for simulation*/
  #define CONTACTTEST_NUM_OF_PINS 32    
#endif

#define CONTACTTEST_MAX_NUM_OF_PINS 256 /*max pin results array size*/

#define PhPFuncTaCheckStop(x) \
        { \
           phPFuncError_t taCheckVal = (x); \
           if (taCheckVal != PHPFUNC_ERR_OK && taCheckVal != PHPFUNC_ERR_Z_LIMIT) \
           {\
               phPFuncTaStop(proberID);    \
               return PHPFUNC_ERR_ABORTED;  \
           }\
        } 
/*End CR21589*/        
        
/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

/*Begin CR21589, Fabarca & Kun Xiao,28 Jun 2006*/
static int sZContactHeight = -1;
static int sPlanarityWindow= -1;
#ifdef SIMULATE_DCCTEST
  static TF_PIN_RESULT sPinResult[CONTACTTEST_NUM_OF_PINS]; /*store pin test results after executing simuExecTfparam*/
  static INT32 sState = TF_PIN_RESULTS_NOT_AVAILABLE; /*store pin results state after executing simuExecTfparam*/
#endif
/*End CR21589*/

static int contactChcekEnabled = 0;
static int ZUPSteps = 0;
static int loopCount = 100;
static int zCurrHeight = 0;
static bool lockzHeighDone = false;

/*--- functions -------------------------------------------------------------*/

phPFuncError_t handleIgnoredSRQ(phPFuncId_t proberID, int srq)   /* SRQ = ??? */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "service request ignored (SRQ 0x%02x) due to set configuration", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleAssistSRQ(phPFuncId_t proberID, int srq)
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "assistance at prober needed (SRQ 0x%02x), waiting for operation to complete", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleManualTestSRQ(phPFuncId_t proberID, int srq)
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "manual test request received (SRQ 0x%02x), ignored", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleFatalSRQ(phPFuncId_t proberID, int srq)
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_FATAL,
        "fatal situation at prober (SRQ 0x%02x), need operator interaction", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_FATAL;
}

static int checkParameter(phPFuncId_t driverID, int isOK, const char *message)
{
    if (!isOK)
        phLogFuncMessage(driverID->myLogger, PHLOG_TYPE_ERROR, message);

    return isOK;
}

static phPFuncError_t riseChuck(phPFuncId_t proberID)
{
    static int srq = 0;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    PhPFuncTaCheck(phPFuncTaSend(proberID, "Z%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 5, 0x43, 0x53, 0xC3, 0xC4, 0x62));
    switch (proberID->model)
    {
      case PHPFUNC_MOD_TEL_20S:
      case PHPFUNC_MOD_TEL_P8:
      case PHPFUNC_MOD_TEL_P12:
      case PHPFUNC_MOD_TEL_F12:

        switch (srq)
        {
          case 0x43: /* chuck rise OK */
            break;
          case 0x53: /* chuck limit reached */
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "upper chuck Z-position limit reached");
            retVal = PHPFUNC_ERR_ANSWER;
            break;
          case 0x62:
            /* prober pause detected */
            phEstateASetPauseInfo(proberID->myEstate, 1);
            phPFuncTaRemoveStep(proberID);
            retVal = PHPFUNC_ERR_WAITING;
            break;
          default:
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "unexpected SRQ received while rising chuck: 0x%02x", srq);
            retVal = PHPFUNC_ERR_ANSWER;
        }
        break;
      case PHPFUNC_MOD_TEL_78S:
        switch (srq)
        {
          case 0x43: /* chuck rise OK */
            break;
          case 0x53: /* chuck limit reached */
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "upper chuck Z-position limit reached");
            retVal = PHPFUNC_ERR_ANSWER;
            break;
          case 0xC3:
            phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
                "profiler probe on wafer (chuck rised)");
            break;
          case 0xC4:
            phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
                "profiler probe not on wafer");
            retVal = PHPFUNC_ERR_ANSWER;
            break;
          case 0x62:
            /* prober pause detected */
            phEstateASetPauseInfo(proberID->myEstate, 1);
            phPFuncTaRemoveStep(proberID);
            retVal = PHPFUNC_ERR_WAITING;
            break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
          default: break;
/* End of Huatek Modification */
        }
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
    }

    return retVal;
}

static phPFuncError_t machineNumber(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **numP              /* resulting number */
)
{
    static char mNum[4];

    mNum[0] = '\0';

    *numP = mNum;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    PhPFuncTaCheck(phPFuncTaSend(proberID, "F%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%3c", mNum));

    mNum[3] = '\0';
    return retVal;
}

static phPFuncError_t machineID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **machineIdString     /* resulting pointer to equipment ID string */
)
{
    static char mID[256] = "";
    int sendMachineID=1;
    mID[0] = '\0';
    *machineIdString = mID;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    switch (proberID->model)
    {
      case PHPFUNC_MOD_TEL_20S:
        sendMachineID=0;
        break;
      case PHPFUNC_MOD_TEL_78S:
      case PHPFUNC_MOD_TEL_P8:
      case PHPFUNC_MOD_TEL_P12:
      case PHPFUNC_MOD_TEL_F12:

        sendMachineID=1;
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
    }

    if ( sendMachineID && proberID->p.sendIDquery )
    {
        PhPFuncTaCheck(phPFuncTaSend(proberID, "?V%s", MY_EOC));
        PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%255[^\n\r]", mID));
    }
    else
    {
        strcpy(mID, "");
    }
    return retVal;
}

static phPFuncError_t verifyModel(
    phPFuncId_t proberID     /* driver plugin ID */,
    char *mID                /* resulting pointer to equipment ID string */
)
{
    int i;
    char *p;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    /* identify the prober model */
    if (!mID || strlen(mID) == 0)
    {
        if ( proberID->p.sendIDquery )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                "Impossible to receive identification string from prober.\n"
                "Can not validate the configured model type \"%s\" (ignored)",
                proberID->cmodelName);
        }
        else
        {
            phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
                "Machine ID query not sent.\n"
                "Can not validate the configured model type \"%s\" (ignored)",
                proberID->cmodelName);
        }
    }
    else
    {
        /* find third substring separated by a ',' character */
        p = strtok(mID, ",");
        for (i=0; i<2 && p; i++)
            p = strtok(NULL, ",");
        
        /* validate (or not) */
        if (p)
        {
            if (!phToolsMatch(p, proberID->cmodelName))
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "received prober model string \"%s\" does not match configured value \"%s\"\n"
                    "Please verify, that you are using the correct prober driver configuration.\n"
                    "It is necessary to restart now.", p, proberID->cmodelName);
                retVal = PHPFUNC_ERR_CONFIG;
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
                    "received prober model string: \"%s\"", p);
            }
        }
        else
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                "received identification string from prober does not match expected format:\n"
                "identification received: \"%s\"\n"
                "format expected:         \"<serial number>,<revision>,<model>,<more info>\"\n"
                "Can not validate the configured model type \"%s\" (ignored)",
                mID, proberID->cmodelName);
        }

        /* clean up strtol */
        while (p)
            p = strtok(NULL, ",");
    }

    return retVal;
}

static bool checkContactCheckStatus(phPFuncId_t proberID,char* reason)
{
    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"step into checkContactCheckStatus ");
    if(contactChcekEnabled)
    {
      contactChcekEnabled = 0;
      return true;
    }
    phPFuncError_t retVal = PHPFUNC_ERR_OK;
    if (phPFuncTaAskAbort(proberID))
    {
        strcpy(reason,"transaction control perform abort");
        return false;
    }
    int srq = 0,received = 0;
    bool isContinue = true;
    int count = 0;
    long response = -1;
    while(isContinue)
    {
        if (phPFuncTaAskAbort(proberID))
        {
            strcpy(reason,"transaction control perform abort");
            return false;
        }

        retVal = phPFuncTaTestSRQ(proberID, &received,&srq);
        ++count;
        if(retVal == PHPFUNC_ERR_OK )
        {
          if(received)
          {
            if(srq == 0x6C)
              return true;
            else
            {
              phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,"received  SRQ(0x%02x) while waiting for Contact check start signal,ignored",srq);
              phPFuncTaRemoveStep(proberID);

            }
          }
        }
        else
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,"got an unexpected GPIB error, ignore it ");
        if(count >= loopCount) /* loop for 4sx100 */
        {
          char strMsg[128] = {0};
          snprintf(strMsg, 127, "it seems that have wait SRQ for a long time, Do you want to abort or continue?");
          phGuiMsgDisplayMsgGetResponse(proberID->myLogger, 0,
              "Info", strMsg, "abort", "continue", NULL, NULL, NULL, NULL, &response);
          if(response == 2)
            isContinue = false;
          else
            count = 0;
        }
    }
    if(response == 2)
        strcpy(reason,"Operator have cancel to wait for SRQ since did not received contact check start SRQ(0x6C).");
    return false;
}

/*****************************************************************************
 *
 * convertDieLocation
 *
 * Authors: David Pei
 *
 * Description:
 *
 * Convert the die coordinate from string to int type, in case the negative
 * coordinate
 ***************************************************************************/
static phPFuncError_t convertDieLocation(const char *s, long *x, long *y)
{
  char XYLocation[20];
  int len;

  len = strlen(s);
  if ((len>=6) && (len<=10))
  {
    if ((s[0] >= '0') && (s[0] <= '9'))
    {
      strcpy(XYLocation, "+");
      strcat(XYLocation, s);
    }
    else if ((s[0] == '-') || (s[0] == '+'))
    {
      strcpy(XYLocation, s);
    }
    else
    {
      return PHPFUNC_ERR_ANSWER;
    }
    sscanf(XYLocation, "%4ld%4ld", x, y);
    return PHPFUNC_ERR_OK;
  }
  else
  {
    return PHPFUNC_ERR_ANSWER;
  }
}

static phPFuncError_t getXYLocation( phPFuncId_t proberID, char* XYLocation,long* x,long* y )
{
  PhPFuncTaCheck(phPFuncTaSend(proberID, "A%s", MY_EOC));
  PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s",XYLocation ));
  return convertDieLocation(XYLocation, x, y);

}

static phPFuncError_t getSitesInfo(phPFuncId_t proberID,int* sitesGot)
{
  phPFuncError_t retVal = PHPFUNC_ERR_OK;
  char sitesString[PHESTATE_MAX_SITES] = "";
  PhPFuncTaCheck(phPFuncTaSend(proberID, "O%s", MY_EOC));
  PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", sitesString));
  if (strlen(sitesString) != 2+(proberID->noOfSites-1)/4)
  {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
        "site information sent by prober not understood: \"%s\"\n"
        "expected %d characters for %d sites",
        sitesString, 2+(proberID->noOfSites-1)/4,
        proberID->noOfSites);
    retVal = PHPFUNC_ERR_ANSWER;
  }
  else
  {
    for (int site=0; site<proberID->noOfSites; site++)
    {
      sitesGot[site] =
        (sitesString[1+site/4] & (1 << (site%4))) ? 1:0;
    }
  }
  return retVal;

}

char* displayCurrentZheight(phPFuncId_t proberID)
{
  static char handlerAnswer[1024] = "";
  memset(handlerAnswer,'\0',sizeof(handlerAnswer));
  // unify the z height for all type of prober
  sprintf(handlerAnswer, "i%d", zCurrHeight);

  return handlerAnswer;
}

void itoS(int site,char *result)
{
    int num = site;
    int i=0;
    char arr[100] = "";
    for(i=0;num>0;++i)
    {
        int tmp = num%10;
        char a = tmp + '0';
        result[i] = a;
        num = num/10;
    }
    num = i/2;
    for(int j=0;j<num;++j,--i)
    {
        char tmp = result[j];
        result[j] = result[i-1];
        result[i-1] = tmp;
    }
    return ;
}

static phPFuncError_t getEnableSites(phPFuncId_t proberID,char* response)
{
  phPFuncError_t retVal = PHPFUNC_ERR_OK;
  int sitesGot[PHESTATE_MAX_SITES];
  double stSite = 0;
  retVal = getSitesInfo(proberID,sitesGot);
  if(retVal != PHPFUNC_ERR_OK)
    return retVal;
  for (int site=0; site<proberID->noOfSites; site++)
  {
    if (proberID->activeSites[site] == 1)
    {
      if(sitesGot[site])
      {
        if (PHCONF_ERR_OK == phConfConfIfDef(proberID->myConf, PHKEY_SI_STHSMAP))
        {
          phConfConfNumber(proberID->myConf,PHKEY_SI_STHSMAP,1,&site,&stSite);
          char result[100] = "";
          itoS(stSite,result);
          //phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,"handler site:%s(sm site: %d),result:%s",proberID->siteIds[site],(int)stSite,result);
          strcat(response,result);
        }
      }
    }
  }
  return retVal;
}

static phPFuncError_t lockChuck( phPFuncId_t proberID )
{
     phPFuncError_t retVal = PHPFUNC_ERR_OK;
     int srq = 0;
     phPFuncTaStart(proberID);
     PhPFuncTaCheck(phPFuncTaSend(proberID,"z%s",MY_EOC) )
     phPFuncTaGetSRQ(proberID, &srq, 1, 0x59);
     if(srq != 0x59 )
        retVal = PHPFUNC_ERR_ANSWER;
     phPFuncTaStop(proberID);
     lockzHeighDone = true;
     return retVal;
}

static int parseXYCoordinateFromPara(int *x,int *y,  char* para)
{
    if(para)
    {
        char *p = strchr(para,'x');
        if(p)
        {
            char xArr[20] = "",yArr[20] = "";
            strcpy(yArr,p+1);
            strncpy(xArr,para,strlen(para)-strlen(yArr)-1);
            *x = strtol(xArr,NULL,10);
            *y = strtol(yArr,NULL,10);
            return 0;
        }
        else
            return -1;
    }
    else
        return -1;
}

static int parseZParameter(int *incremental,int *zMaxHeight, char* para)
{
    char *token = strtok(para,",");
    if(token)
        *incremental = strtol(token,NULL,10);
    else
        return -1;
    token = strtok(NULL,",");
    if(token)
        *zMaxHeight = strtol(token,NULL,10);
    else
        return -1;
    return 0;
}

static phPFuncError_t waitLotStarted(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    static int srq1 = 0;
    static int srq2 = 0;
    int lotStarted = 0;

    phEstateAGetLotInfo(proberID->myEstate, &lotStarted);
    if (lotStarted)
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG, 
            "prober has already send out the lot start SRQ");
        return PHPFUNC_ERR_OK;
    }

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG, "waiting for lot start");

    do
    {
        PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq1, 8, 0x46, 0x4a, 0x52, 0x60, 0x70, 0x4c, 0x48, 0x62));
        if (srq1 == 0x52) /* 'N' command */
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "received SRQ 0x%02x while waiting for lot start.\n"
                "Please disable wafer name receive at prober", srq1);
            phPFuncTaRemoveStep(proberID);
        }
        if (srq1 == 0x70) /* 'd' command */
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "received SRQ 0x%02x while waiting for lot start.\n"
                "Please disable cassette map at prober", srq1);
            phPFuncTaRemoveStep(proberID);
        }
        if (srq1 == 0x60) /* 'l' command */
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "received SRQ 0x%02x while waiting for lot start.\n"
                "Please disable random wafer testing at prober", srq1);
            phPFuncTaRemoveStep(proberID);
        }
        if (srq1 == 0x4c) /* alignment */
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "received SRQ 0x%02x while waiting for lot start.\n"
                "Alignment error at prober", srq1);
            phPFuncTaRemoveStep(proberID);
        }
        if (srq1 == 0x48) /* previous lot end */
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "received SRQ 0x%02x while waiting for lot start.\n"
                "Lot end received while waiting for lot start, ignored", srq1);
            phPFuncTaRemoveStep(proberID);
        }
        if (srq1 == 0x62) /* paused */
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "received SRQ 0x%02x while waiting for lot start.\n"
                "Prober paused, ignored", srq1);
            phPFuncTaRemoveStep(proberID);
        }
        if(srq1 == 0x6C) /* AUTOZ start */
        {
            contactChcekEnabled = 1;
            phLogFuncMessage(proberID->myLogger, LOG_NORMAL,"receive autoz start SRQ(0x6C) in waitLotStarted function, still waiting lot start SRQ");
            phPFuncTaRemoveStep(proberID);
        }

    } while (srq1 == 0x52 || srq1 == 0x60 || srq1 == 0x70 || srq1 == 0x4c || srq1 == 0x48 || srq1 == 0x62 || srq1 == 0x6C);

    if (srq1 == 0x4a)
    {
        PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq2, 1, 0x46));
    }

    return PHPFUNC_ERR_OK;
}


/*****************************************************************************
 *
 * analysis the request from the Prober for "privateGetStatus"
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 *  The request from Prober might be encoded. We should use this
 *  function to parse it. For example, for "wafer_units' request, the response
 *  is:
 *     "ab", where
 *         a=1: wafer size is in mm
 *         b=0: die is in inch
 *         b=1: die is in metric, here is um
 *
 *     So,we need to parse it
 *
 *Parameter:
 *  commandString (I). The command string
 *  encodedResult (I). It is the orinigal encoded response from Prober
 *  pResult (O). Output of the parsed result
 *
 *Return:
 *  FAIL or SUCCEED
 *
 ***************************************************************************/
static int analysisResultOfGetStatus(phPFuncId_t proberID, const char * commandString, const char *encodedResult, char *pResult)
{
  static const char *waferUnits[] = {"inch", "um"};
  static const char *posX[] = {"", "Left", "Right", "Right", "Left"};
  static const char *posY[] = {"", "Down", "Down", "Up", "Up"};
  static const char *cassStatus[] = {"NO cassette", "Ready to test", "Testing under way",
                                     "Testing finished", "Rejected wafer cassette"};

  int retVal = SUCCEED;
  const char *p = encodedResult;


  if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_SIZE) == 0 ) {
    sprintf(pResult, "%s%s", p, "mm");
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_UNITS) == 0 ) {
    int u1 = 0;
    int u2 = 0;
    if( (sscanf(p,"%1d%1d",&u1,&u2)==2) && (u2>=0) && (u2<=1) ) {
      strcpy(pResult, waferUnits[u2]);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_POS_X) == 0 ) {
    /* the response from Prober is "N", where N=0,1,2,3 */
    if( ('1'<=(*p)) && ((*p)<='4') ) {
      strcpy(pResult, posX[(*p)-'0']);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_POS_Y) == 0 ) {
    if( ('1'<=(*p)) && ((*p)<='4') ) {
      strcpy(pResult, posY[(*p)-'0']);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CENTER_X) == 0 ) {
    int x = 0;
    int y = 0;
    if( sscanf(p,"X%dY%d",&x,&y) == 2 ) {
      sprintf(pResult, "%03d", x);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CENTER_Y) == 0 ) {
    int x = 0;
    int y = 0;
    if( sscanf(p,"X%dY%d",&x,&y) == 2 ) {
      sprintf(pResult, "%03d", y);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_DIE_HEIGHT) == 0 ) {
    char x[6] = "";
    char y[6] = "";
    if( sscanf(p,"X%5sY%5s",x,y) == 2 ) {
      sprintf(pResult, "%s", y);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_DIE_WIDTH) == 0 ) {
    char x[6] = "";
    char y[6] = "";
    if( sscanf(p,"X%5sY%5s",x,y) == 2 ) {
      sprintf(pResult, "%s", x);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CASS_STATUS) == 0 ) {
    int slotNum = 0;
    int numOfNotFinished = 0;
    int idCass1 = 0;
    int idCass2 = 0;

    if( (p != NULL) && (sscanf(p,"%2d%2d%1d%1d",&slotNum,&numOfNotFinished,&idCass1,&idCass2)==4) ){
      if( (idCass1>=0) && (idCass1 <= 4) && (idCass2>=0) && (idCass2<=4) ) {
        sprintf(pResult, "%s%02d%02d%d%d%s", "<", slotNum, numOfNotFinished, idCass1, idCass2, ">");
        sprintf(pResult, "%s%s:%d; %s:%d; %s:%s; %s:%s", pResult,
               "slot number of Wafer on chuck", slotNum,
               "count of Wafers remained", numOfNotFinished,
               "status of Cassette 1", cassStatus[idCass1],
               "status of Cassette 2", cassStatus[idCass2]);
      } else {
          retVal = FAIL;
      }
    } else {
        retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_LOT_NUMBER) == 0 ) {
    char lotNumber[32] = "";

    if((p != NULL) && sscanf(&p[3], "%20c", lotNumber) == 1)
    {
      lotNumber[20] = '\0';
      strcpy(pResult, lotNumber);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_MULTISITE_LOCATION_NUMBER) == 0 ) {
    int location;

    if((p != NULL) && sscanf(&p[64], "%2d", &location) == 1)
    {
      sprintf(pResult, "%d", location);
    } else {
      retVal = FAIL;
    }
  }
  else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CHUCK_TEMP) == 0 ) {
    static char currentTemp[8] = "";
    static char setTemp[8] = "";
    static char buffer_set_temp [MAX_STATUS_MSG_LEN] = "";

    /** Begin of temperature string re-format:
      * Change the temperature string to something like: <nnnnmmmm>
      * in which nnnn reprents the current temp and mmmm represents
      * the setting temp. If the setting temp is not avaliable, just
      * set mmmm to 9999.
      */
    /* Begin of setting temp reading */
    PhPFuncTaCheck(phPFuncTaSend(proberID, "f2%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", buffer_set_temp));

    if(sscanf(buffer_set_temp, "%5c", setTemp) == 1)
    {
      if(strchr(setTemp, '.') != NULL) /* using the hot chuck */
      {
        sprintf(setTemp, "%04.0lf", 10*strtod(setTemp, NULL));
      } else {
        sprintf(setTemp, "%04ld", strtol(setTemp, NULL, 10));
      }
    } else {
        strcpy(setTemp, "9999");
    } /* End of setting temp reading */

    if((p != NULL) && sscanf(p, "%5c", currentTemp) == 1)
    { 
      if(strchr(currentTemp, '.') != NULL) /* using the hot chuck */
      {
         sprintf(pResult, "%s<%04.0lf%s>", "current temperature:", 10*strtod(currentTemp, NULL), setTemp);
      } else {
         sprintf(pResult, "%s<%04ld%s>", "current temperature:", strtol(currentTemp, NULL, 10), setTemp); /* using hot and cold chuck*/
      }
    } else {
      retVal = FAIL;
    }
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_ERROR_CODE) == 0 ) {
    char errorCode[8] = "";
    if ( (p!=NULL) && (sscanf(p, "%4c", errorCode)==1) ){
      sprintf(pResult, "%s", errorCode);
    } else {
      retVal = FAIL;
    }
  }
  else if ( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_NUMBER) == 0 ) {
    if ( p!=NULL ){
       sprintf(pResult, "<%03ld>", strtol(p, NULL, 10));
    }
  
  }
  else {
    /*
    * default handle of WAFER_ID, CHUCK_TEMPERATURE
    */
    strcpy(pResult, p);
  }


  return retVal;
}



#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize prober specific plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateInit(
    phPFuncId_t proberID      /* the prepared driver plugin ID */
)
{
    static int isConfigured = 0;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int i;*/
/*    int count;*/
/* End of Huatek Modification */

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    /* reset some private variables */
    proberID->p.stIsMaster = 0;
    proberID->p.pauseProber = 0;
    proberID->p.reversedBinning = 0;
    proberID->p.asciiOCommand = 0;
    proberID->p.dontBinDies = 0;
    proberID->p.sendIDquery  = 0;
    proberID->p.sendWafAbort = 0;
    proberID->p.max_bins = 32;
    proberID->p.binningMethod = PARITY_BINNING_METHOD;
    proberID->p.pauseInitiatedByProber = 0;
    proberID->p.pauseInitiatedByST = 0;    
    proberID->p.dieProbed = 0;

    /* perform private initialization first, if this is not a retry call */
    if (!isConfigured)
    {
        phPFuncTaAddSRQHandler(proberID, 0x4e, handleAssistSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x68, handleManualTestSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x50, handleFatalSRQ);
        phPFuncTaAddSRQHandler(proberID, 0xff, handleFatalSRQ);
        isConfigured = 1;
    }

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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateReconfigure(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    phConfError_t confError;
    phConfType_t confType;
    int listSize = 0;
    double dSRQVal = 0.0;
    int i;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;
    int found;
    char *mID = NULL;

    /* check out who is controlling the prober */
    confError = phConfConfStrTest(&found, proberID->myConf,
        PHKEY_PR_STEPMODE, "smartest", "prober", "learnlist", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
        retVal = PHPFUNC_ERR_CONFIG;
    else
        switch (found)
        {
          case 0:
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                "configuration \"%s\" not given, assuming \"smartest\"",
                PHKEY_PR_STEPMODE);
            /* fall into case 1 */
          case 1:
            proberID->p.stIsMaster = 1; 
            break;
          case 2:
            proberID->p.stIsMaster = 0; 
            break;
          case 3:
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "configuration value \"learnlist\" not supported for \"%s\"",
                PHKEY_PR_STEPMODE);
            retVal = PHPFUNC_ERR_CONFIG;
            break;
        }

    /* do we want to pause the prober ? */
    proberID->p.pauseProber = 0;
    confError = phConfConfStrTest(&found, proberID->myConf,
        PHKEY_OP_PAUPROB, "yes", "no", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
        retVal = PHPFUNC_ERR_CONFIG;
    else
    {
        if (found == 1)
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "will pause prober on SmarTest pause");
            proberID->p.pauseProber = 1;
        }
    }

    /* do we need to use reverse binning */
    proberID->p.reversedBinning = 0;
    confError = phConfConfStrTest(&found, proberID->myConf,
        PHKEY_PL_TELREVBIN, "yes", "no", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
        retVal = PHPFUNC_ERR_CONFIG;
    else
    {
        if (found == 1)
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "will use reversed bin data sequence");
            proberID->p.reversedBinning = 1;
        }
    }

    /* do we need to accept ASCII style prober information */
    proberID->p.asciiOCommand = 0;
    confError = phConfConfStrTest(&found, proberID->myConf,
        PHKEY_PL_TELOCMD, "ascii", "encoded", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
        retVal = PHPFUNC_ERR_CONFIG;
    else
    {
        if (found == 1)
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "will receive ASCII style on probe information");
            proberID->p.asciiOCommand = 1;
            if (proberID->noOfSites > 8)
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "number of sites exceeds maximum of 8 for configuration "
                    "\"%s\" being set to \"ascii\"", PHKEY_PL_TELOCMD);
                retVal = PHPFUNC_ERR_CONFIG;
            }
        }
    }

    /* can we send the ?V query - machine ID */
    proberID->p.sendIDquery = 1;  /* 
                                   * by default sent machine ID query for 
                                   * model types for which this is 
                                   * believed to be legal (78S and P8)
                                   */
    confError = phConfConfIfDef(proberID->myConf, PHKEY_PL_TELIDQRY);
    if (confError == PHCONF_ERR_OK)
    {
        /* send ID query has been defined */

        confError = phConfConfStrTest(&found, proberID->myConf,
            PHKEY_PL_TELIDQRY, "no", "yes", NULL);
        if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
            retVal = PHPFUNC_ERR_CONFIG;
        else
        {
            if (found == 1)
            {
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "will not send \"?V\" query machine ID");
                proberID->p.sendIDquery = 0;
            }
        }
    }

    /* can we send the VF command - unload wafer abort test */
    proberID->p.sendWafAbort = 1; /* 
                                   * by default sent unload wafer
                                   * abort test for all model types 
                                   */
    confError = phConfConfIfDef(proberID->myConf, PHKEY_PL_TELWAFABT);
    if (confError == PHCONF_ERR_OK)
    {
        /* send VF command has been defined */

        confError = phConfConfStrTest(&found, proberID->myConf,
            PHKEY_PL_TELWAFABT, "no", "yes", NULL);
        if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
            retVal = PHPFUNC_ERR_CONFIG;
        else
        {
            if (found == 1)
            {
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "will not send \"VF\" command unload wafer abort test");
                proberID->p.sendWafAbort = 0;
            }
        }
    }

    /* do we want to send bin data at all */
    proberID->p.dontBinDies = 0;
    confError = phConfConfStrTest(&found, proberID->myConf,
        PHKEY_BI_DONTBIN, "yes", "no", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
        retVal = PHPFUNC_ERR_CONFIG;
    else
    {
        if (found == 1)
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "will never send bin data to prober");
            proberID->p.dontBinDies = 1;
        }
    }

    /* ignore some explicitely defined SRQs */
    confError = phConfConfIfDef(proberID->myConf, PHKEY_GB_IGNSRQ);
    if (confError == PHCONF_ERR_OK)
    {
        /* there is something defined to be ignored... */
        confError = phConfConfType(proberID->myConf, PHKEY_GB_IGNSRQ,
            0, NULL, &confType, &listSize);
        if (confType != PHCONF_TYPE_LIST)
        {
            /* ... but in a wrong format */
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "configuration value of \"%s\" must be a list of integers\n"
                "in the range of 0 to 255",
                PHKEY_GB_IGNSRQ);
            retVal = PHPFUNC_ERR_CONFIG;
        }
        else
        {
            /* ... in a correct format */
            for (i=0; i<listSize; i++)
            {
                confError = phConfConfNumber(proberID->myConf, PHKEY_GB_IGNSRQ,
                    1, &i, &dSRQVal);
                if (confError == PHCONF_ERR_OK && 
                    dSRQVal >= 0 && dSRQVal <= 255)
                {
                    /* yup, here is one: ignore it! */
                    phPFuncTaAddSRQHandler(proberID, (int) dSRQVal, handleIgnoredSRQ);
                    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                        "SRQ 0x%02x from prober will be ignored, if received",
                        (int) dSRQVal);
                }
                else
                {
                    /* out of range or bad format */
                    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                        "configuration value of \"%s\" must be a list of integers\n"
                        "in the range of 0 to 255",
                        PHKEY_GB_IGNSRQ);
                    retVal = PHPFUNC_ERR_CONFIG;
                }
            }
        }
    }

  /*
  * read the Driver Configuration Parameter: tel_binning_method, only valid in P12/P8 model 
  * CR 15358
  */
  confError = phConfConfIfDef(proberID->myConf, PHKEY_PL_TELBINNINGMETHOD);
  if(confError == PHCONF_ERR_OK) {
    /* there is something defined for "tel_binning_method" Driver Configuration Parameter */
    switch(proberID->model) {
      case PHPFUNC_MOD_TEL_20S:
      case PHPFUNC_MOD_TEL_78S:  /* for 20S, 78S, it is just ignored and print a warning */
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                         "configuration parameter \"%s\" is only meaningful to TEL P8/P12/F12 model\n"
                         "20S/78S model always use \"parity\" binning method",
                         PHKEY_PL_TELBINNINGMETHOD);
        break;
      case PHPFUNC_MOD_TEL_P8:  /* "tel_binning_method" is only valid for P8/P12/F12 model */
      case PHPFUNC_MOD_TEL_P12:
      case PHPFUNC_MOD_TEL_F12:

        confError = phConfConfStrTest(&found, proberID->myConf, 
                                      PHKEY_PL_TELBINNINGMETHOD, "parity", "binary", NULL);
        if( (confError != PHCONF_ERR_UNDEF) && (confError != PHCONF_ERR_OK) ) {
          /* out of range or bad format */
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                           "configuration value of \"%s\" must be either \"parity\" or \"binary\"\n",
                           PHKEY_PL_TELBINNINGMETHOD);
          retVal = PHPFUNC_ERR_CONFIG;
        } else {
          switch( found ) {
            case 1:   /* "parity" */
              proberID->p.binningMethod = PARITY_BINNING_METHOD;
              break;
            case 2:   /* "binary" */
              proberID->p.binningMethod = BINARY_BINNING_METHOD;
              break;
            default:
              phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_FATAL, "internal error at line: %d, "
                               "file: %s",__LINE__, __FILE__);
              break;
          }
        }
        break;
      default:
        break;
    }
  }

  /* 10000 bin only enabled in binary binning method */
  if(proberID->p.binningMethod == BINARY_BINNING_METHOD)
  {
      /* Get max number of bins, either 256 or 10000*/
      confError = phConfConfStrTest(&found, proberID->myConf, PHKEY_GB_MAX_BINS, "256", "10000", "1000000", NULL);

      if ( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
      {
          /* out of range or bad format */
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR, "configuration value of \"%s\" must be either 256 , 10000 or 1000000\n", PHKEY_GB_MAX_BINS);
          retVal = PHPFUNC_ERR_CONFIG;
      }
      else
      {
          switch ( found )
          {
              case 1:  // 256 bins
                  proberID->p.max_bins = 256;
                  break;
              case 2:  // 10000 bins
                  proberID->p.max_bins = 10000;
                  break;
              case 3:  // 1000000 bins
                  proberID->p.max_bins = 1000000;
                  break;
              default: /* for default value in binnary binning method is 256 */
                  proberID->p.max_bins = 256;
                  break;
          }
      }
  }

    /* z command behavior,if the flag is configured as "no", then it will not execute the GPIB "z" command when getting die or loading wafer, the default value is "yes" */
    proberID->p.sendZCmd = 1;
    confError = phConfConfStrTest(&found, proberID->myConf, PHKEY_PL_TELSENDZ, "yes", "no", NULL);
    if( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
    {
        retVal = PHPFUNC_ERR_CONFIG;
    }
    else
    {
        if( found == 2 )
        {
            proberID->p.sendZCmd = 0;
        }
    }

    /* only if steps above have succeeded, communicate to the prober */
    if (retVal == PHPFUNC_ERR_OK)
    {
        /* start identification process */
        phPFuncTaStart(proberID);
        PhPFuncTaCheck(machineID(proberID, &mID));
        phPFuncTaStop(proberID);
        
        /* identify the prober model */
        retVal = verifyModel(proberID, mID);
    }

    /* return final result */
    return retVal;
}
#endif


#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateReset(
    phPFuncId_t proberID     /* driver plugin ID */
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDriverID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
)
{
    /* the resulting Id String is already composed by the code in
       ph_pfunc.c. We don't need to do anything more here and just
       return with OK */

    return PHPFUNC_ERR_OK;
}
#endif



#ifdef EQUIPID_IMPLEMENTED
/*****************************************************************************
 *
 * return the prober identification string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateEquipID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    static char equipId[256] = "";
    equipId[0] = '\0';
    *equipIdString = equipId;

    char *mNum;
    char *mID;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    phPFuncTaStart(proberID);
    PhPFuncTaCheck(machineNumber(proberID, &mNum));
    PhPFuncTaCheck(machineID(proberID, &mID));
    phPFuncTaStop(proberID);

    sprintf(equipId, "reported: %s number: %s configured: %s", 
        mID, mNum, proberID->cmodelName);
    return retVal;
}
#endif



#ifdef LDCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Load an input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLoadCass(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif



#ifdef UNLCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Unload an input cassette
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateUnloadCass(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
}
#endif

#ifdef LDWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Load a wafer to the chuck
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLoadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t source        /* where to load the wafer
                                           from. PHESTATE_WAFL_OUTCASS
                                           is not a valid option! */
)
{
    static long xr = 0;
    static long yr = 0;
    static long xc = 0;
    static long yc = 0;
    static int srq = 0;
    static int firstX = 0;
    static int firstY = 0;
    static int firstIsDefined = 0;
    static char XYLocation[20];


    phEstateCassUsage_t cassUsage;
    phEstateWafUsage_t usage;
    phEstateWafType_t type;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    /* check parameters first. This plugin does not support handling
       of reference wafers, so the only valid source is the
       input cassette or the chuck, in case the wafer is going to be
       retested */
    if (source != PHESTATE_WAFL_INCASS && source != PHESTATE_WAFL_CHUCK)
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "illegal source \"%s\" to load a wafer. Not supported.",
            source == PHESTATE_WAFL_OUTCASS ? "output cassette" :
            source == PHESTATE_WAFL_REFERENCE ? "reference position" :
            source == PHESTATE_WAFL_INSPTRAY ? "inspection tray" :
            "unknown");
        return PHPFUNC_ERR_PARAM;
    }

    /* check input source */
    phEstateAGetICassInfo(proberID->myEstate, &cassUsage);
    if ((cassUsage == PHESTATE_CASS_LOADED_WAFER_COUNT &&
        source == PHESTATE_WAFL_INCASS) ||
        source != PHESTATE_WAFL_INCASS)
    {
        phEstateAGetWafInfo(proberID->myEstate, source, &usage, &type, NULL);
        if (usage == PHESTATE_WAFU_NOTLOADED && source == PHESTATE_WAFL_INCASS)
        {
            phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
                "wafer source \"%s\" is empty, wafer not loaded",
                source == PHESTATE_WAFL_INCASS ? "input cassette" :
                source == PHESTATE_WAFL_CHUCK ? "chuck" :
                "unknown");
            return PHPFUNC_ERR_PAT_DONE;
        }
    }

    phPFuncTaStart(proberID);

    /* source specific actions */
    if (source == PHESTATE_WAFL_CHUCK)
    {
        /* retest wafer case, don't load the wafer, but go back to the first die */
        if (firstIsDefined)
        {
            PhPFuncTaCheck(phPFuncTaSend(proberID, 
                "b%03d%03d%s", firstX, firstY, MY_EOC));
            PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 4, 0x47, 0x4f, 0x4b, 0x62));
            if (srq == 0x62)
            {
                /* prober pause detected */
                phEstateASetPauseInfo(proberID->myEstate, 1);
                phPFuncTaRemoveStep(proberID);
                return PHPFUNC_ERR_WAITING;
            }
            else
                phEstateASetPauseInfo(proberID->myEstate, 0);
        }
    }
    else
    {
        /* regular operation, load wafer was already done by last unload */
    }

    /* common operations: move to reference position, 
       ask for this position and rise the chuck */

    // move to reference position after Auto-Z when prober is master
    if ((!proberID->p.stIsMaster) && lockzHeighDone)
    {
      PhPFuncTaCheck(phPFuncTaSend(proberID, "B%s", MY_EOC));
      PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 2, 0x46, 0x4b));
      lockzHeighDone = false;
    }

    PhPFuncTaCheck(phPFuncTaSend(proberID, "a%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", XYLocation));

    retVal = convertDieLocation(XYLocation, &xr, &yr);
    if(retVal != PHPFUNC_ERR_OK)
    {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
              "Incorrect XY coordinate value received from the prober: %s", XYLocation);
      return retVal;
    }
    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE, "Convert result x:%ld y:%ld", xr, yr);
    
    PhPFuncTaCheck(phPFuncTaSend(proberID, "A%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", XYLocation));

    retVal = convertDieLocation(XYLocation, &xc, &yc);
    if(retVal != PHPFUNC_ERR_OK)
    {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
              "Incorrect XY coordinate value received from the prober: %s", XYLocation);
      return retVal;
    }
    
    if( 1 == proberID->p.sendZCmd )
    {
        PhPFuncTaCheck(riseChuck(proberID));
    }
    phPFuncTaStop(proberID);
    
    phLogFuncMessage(proberID->myLogger, LOG_NORMAL, "wafer loaded and ready to test\n"
      "reference or initial die position: [ %ld, %ld ]\n"
      "current die position:   [ %ld, %ld ]", xr, yr, xc, yc);
    
    /* store first position for a wafer retest */
    firstX = xc;
    firstY = yc;
    firstIsDefined = 1;
    
    /* update equipment specific state, move wafer from source to chuck */
    if ((cassUsage == PHESTATE_CASS_LOADED_WAFER_COUNT &&
        source == PHESTATE_WAFL_INCASS) ||
        source != PHESTATE_WAFL_INCASS)
    {
        phEstateASetWafInfo(proberID->myEstate, 
            source, PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
    }
    else
    {
        /* we are loading from an input cassette with unknown
           number of wafers */
        type = PHESTATE_WAFT_REGULAR;
    }
    phEstateASetWafInfo(proberID->myEstate, 
        PHESTATE_WAFL_CHUCK, PHESTATE_WAFU_LOADED, type);

    /* by default not implemented */
    return retVal;
}
#endif



#ifdef UNLWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Unload a wafer from the chuck
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 *
 * explicit unload command, no matter who controlls the stepping
 *
 *      driver            prober
 *
 *      U        ----->         
 *               <-----   0x46, 0x48
 *               <-----   0x4e (if configured at prober)
 *
 ***************************************************************************/
phPFuncError_t privateUnloadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t destination   /* where to unload the wafer
                                           to. PHESTATE_WAFL_INCASS is
                                           not valid option! */
)
{
    static int srq = 0;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static int srq2 = 0;*/
/*    static int received = 0;*/
/* End of Huatek Modification */

    phEstateWafUsage_t usage;
    phEstateWafType_t type;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    /* check parameters first. This plugin does not support handling
       of reference wafers, so the only valid destination is the
       output cassette or the chuck, in case the wafer is going to be
       retested */
    if (destination != PHESTATE_WAFL_OUTCASS &&
        destination != PHESTATE_WAFL_CHUCK)
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "illegal destination \"%s\" to unload a wafer. Not supported.",
            destination == PHESTATE_WAFL_INCASS ? "input cassette" :
            destination == PHESTATE_WAFL_REFERENCE ? "reference position" :
            destination == PHESTATE_WAFL_INSPTRAY ? "inspection tray" :
            "unknown");
        return PHPFUNC_ERR_PARAM;
    }

    if (destination == PHESTATE_WAFL_CHUCK)
    {
        /* retest wafer case, don't unload the wafer */
        
    }
    else
    {
        /* regular operation, unload wafer */
        phPFuncTaStart(proberID);
        if ( proberID->p.dieProbed && proberID->p.sendWafAbort )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "we want to enforce an abort of the current lot");
            PhPFuncTaCheck(phPFuncTaSend(proberID, "VF%s", MY_EOC));
            PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 2, 0x48, 0x62));
            if (srq == 0x62)
            {
                phPFuncTaRemoveStep(proberID);
                return PHPFUNC_ERR_WAITING;
            }
        }
        else
        {
            PhPFuncTaCheck(phPFuncTaSend(proberID, "U%s", MY_EOC));
            PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 3, 0x46, 0x48, 0x62));
            if (srq == 0x62)
            {
                phPFuncTaRemoveStep(proberID);
                return PHPFUNC_ERR_WAITING;
            }
        }
        phPFuncTaStop(proberID);

        switch (srq)
        {
          case 0x46: /* wafer unload OK, next wafer loaded */
            break;
          case 0x48: /* wafer unload OK, and lot end */
            phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_NOTLOADED);
            phEstateASetLotInfo(proberID->myEstate, 0);
            retVal = PHPFUNC_ERR_PAT_DONE;
            break;
        }
    }

    /* update equipment specific state, move wafer from chuck to
       destination */
    phEstateAGetWafInfo(proberID->myEstate, 
        PHESTATE_WAFL_CHUCK, &usage, &type, NULL);
    phEstateASetWafInfo(proberID->myEstate, 
        PHESTATE_WAFL_CHUCK, PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
    phEstateASetWafInfo(proberID->myEstate, 
        destination, PHESTATE_WAFU_LOADED, type);

    /* by default not implemented */
    return retVal;
}
#endif



#ifdef GETDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 *
 * 
 * automatic mode (prober is master)
 *
 *      driver            prober
 *
 *      A        ----->         
 *               <-----   xxxyyy
 *
 *      Z        ----->                           (may be avoided ?)
 *               <-----   0x43 (0x53, 0xC3)
 *
 *
 * step mode (prober is slave)
 *
 *      bxxxyyy  ----->   
 *               <-----   0x47 (0x4F 0x4b)        Z up is implicit
 *
 *
 * if multisite probing only:
 *
 *      O        ----->
 *               <-----   abcd0000                site info 1 or 0
 *                                                for model TEL_1000
 *
 *      O        ----->                           bit coded
 *               <-----   [@A-O]+                 for P8 and TEL_1200
 *
 *
 ***************************************************************************/
phPFuncError_t privateGetDie(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *dieX                /* in explicit probing mode
                                 (stepping_controlled_by is set to
                                 "smartest"), this is the absolute X
                                 coordinate of the current die
                                 request, as sent to the prober. In
                                 autoindex mode
                                 (stepping_controlled_by is set to
                                 "prober" or "learnlist"), the
                                 coordinate is returned in this
                                 field. Any necessary transformations
                                 (index to microns, coordinate
                                 systems, etc.) must be performed by
                                 the calling framework */,
    long *dieY                /* same for Y coordinate */
)
{
    static long x = 0;
    static long y = 0;
    static int sitesGot[PHESTATE_MAX_SITES];
    static char sitesString[PHESTATE_MAX_SITES];
    static int srq = 0;
    static char XYLocation[20];


    phPFuncError_t retVal = PHPFUNC_ERR_OK;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    int site;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    /* check parameters first */
    if (proberID->p.stIsMaster)
    {
        if (!checkParameter(proberID,
            *dieX >= -999 && *dieX <= 999 &&
            *dieY >= -999 && *dieY <= 999,
            "requested die position is out of valid range")
            )
            return PHPFUNC_ERR_PARAM;
    }

    /* perform communication steps */
    phPFuncTaStart(proberID);
    if (proberID->p.stIsMaster)
    {
        char format[20] = {0};
        if(*dieX >= 0 && *dieY >= 0)
          strcpy(format, "b%03d%03d%s");
        else if(*dieX < 0 && *dieY >= 0)
          strcpy(format, "b%04d%03d%s");
        else if(*dieX >= 0 && *dieY < 0)
          strcpy(format, "b%03d%04d%s");
        else if(*dieX < 0 && *dieY < 0)
          strcpy(format, "b%04d%04d%s");

        PhPFuncTaCheck(phPFuncTaSend(proberID, 
            format, *dieX, *dieY, MY_EOC));

        PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 4, 0x47, 0x4f, 0x4b, 0x62));
        switch (srq)
        {
          case 0x47:
            /* indexed to die position */
            phEstateASetPauseInfo(proberID->myEstate, 0);
            break;
          case 0x4f:
            /* position outside of wafer, keep site flags
               unchanged, write warning message and return OK */
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                "requested die position [%ld, %ld] is outside of wafer "
                "(SRQ 0x%02x)", *dieX, *dieY, (int) srq);
            phEstateASetPauseInfo(proberID->myEstate, 0);
            break;
          case 0x4b:
            /* position of wafer and end of probing (78S and P8 only) */
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "probing aborted by prober (SRQ 0x%02x)", (int) srq);
            retVal = PHPFUNC_ERR_PAT_DONE;
            break;
          case 0x62:
            /* prober pause detected */
            phEstateASetPauseInfo(proberID->myEstate, 1);
            phPFuncTaRemoveStep(proberID);
            retVal = PHPFUNC_ERR_WAITING;
            break;
        }
    }

    /* early return on error */
    if (retVal != PHPFUNC_ERR_OK)
    {
        return retVal;
    }

    PhPFuncTaCheck(phPFuncTaSend(proberID, "A%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", XYLocation));

    retVal = convertDieLocation(XYLocation, &x, &y);
    if(retVal != PHPFUNC_ERR_OK)
    {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
              "Incorrect XY coordinate value received from the prober: %s", XYLocation);
      return retVal;
    }

    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE, "Convert %s to x:%ld y:%ld", XYLocation, x, y);

    /* verify reached position */
    if (proberID->p.stIsMaster)
    {
        if (x != *dieX || y != *dieY)
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "reached die position [%ld, %ld] differs from requested position [%ld, %ld].\n"
                "SmarTest's wafermap does not seem to match the real wafer dimensions.\n"
                "Please correct wafermap and restart test.",
                x, y, *dieX, *dieY);
            return PHPFUNC_ERR_ANSWER;
        }
    }

    if( 1 == proberID->p.sendZCmd )
    {
        PhPFuncTaCheck(riseChuck(proberID));
    }

    if (proberID->noOfSites == 1)
    {
        sitesGot[0] = (proberID->p.stIsMaster && srq == 0x4f) ? 0 : 1;
    }
    else
    {
        PhPFuncTaCheck(phPFuncTaSend(proberID, "O%s", MY_EOC));
        switch (proberID->model)
        {
          case PHPFUNC_MOD_TEL_20S:
            if (proberID->p.asciiOCommand)
            {
              PhPFuncTaCheck(phPFuncTaReceive(proberID, 8,
                    "%1d%1d%1d%1d%1d%1d%1d%1d",
                    &sitesGot[0], &sitesGot[1], &sitesGot[2], &sitesGot[3], 
                    &sitesGot[4], &sitesGot[5], &sitesGot[6], &sitesGot[7]));
        break;
            }
            /* else fall through to most common case */
    case PHPFUNC_MOD_TEL_P8:
    case PHPFUNC_MOD_TEL_P12:
    case PHPFUNC_MOD_TEL_F12:
    case PHPFUNC_MOD_TEL_78S:
            PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", sitesString));
            if (strlen(sitesString) != 1+(proberID->noOfSites-1)/4)
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "site information sent by prober not understood: \"%s\"\n"
                    "expected %d characters for %d sites",
                    sitesString, 1+(proberID->noOfSites-1)/4, 
                    proberID->noOfSites);
                retVal = PHPFUNC_ERR_ANSWER;
            }
            else
                for (site=0; site<proberID->noOfSites; site++)
                    sitesGot[site] = 
                        (sitesString[site/4] & (1 << (site%4))) ? 1:0;
            break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
          default: break;
/* End of Huatek Modification */
        }
    }
    phPFuncTaStop(proberID);
                
    /* early return on error */
    if (retVal != PHPFUNC_ERR_OK)
        return retVal;

    proberID->p.dieProbed = 1;

    /* all done, set site population */
    for (site=0; site<proberID->noOfSites; site++)
    {
        if (proberID->activeSites[site] == 1)
        {
            population[site] = sitesGot[site] ? 
                PHESTATE_SITE_POPULATED : PHESTATE_SITE_EMPTY;
        }
        else
            population[site] = PHESTATE_SITE_DEACTIVATED;
    }
    phEstateASetSiteInfo(proberID->myEstate, population, proberID->noOfSites);

    if (!proberID->p.stIsMaster)
    {
        *dieX = x;
        *dieY = y;
    }
    return PHPFUNC_ERR_OK;
}
#endif



#ifdef BINDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateBinDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL, if
                                sub-die probing is active */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis or
                                NULL, if sub-die probing is active */
)
{
    static int srq1 = 0;
    static int srq2 = 0;

    char binCode[4*(1+(PHESTATE_MAX_SITES-1)/4)][9] = {0};
    char pfCode[1+(PHESTATE_MAX_SITES-1)/4] = {0};
    char command[4*(1+(PHESTATE_MAX_SITES-1)/4)*9+4] = {0};
    long theBin;
    int passed;
    int empty;
    phEstateSiteUsage_t *population;
    phEstateSiteUsage_t newPopulation[PHESTATE_MAX_SITES];
    int entries;
    int site;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    if (!perSiteBinMap)
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING, 
            "no binning data available, not sending anything to the prober");
        return PHPFUNC_ERR_OK;
    }

    if (!proberID->p.dieProbed)
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING, 
            "no die currently probed, due to wafer abort");
        return PHPFUNC_ERR_OK;        
    }

    if (!proberID->p.dontBinDies)
    {
        /* prepare empty binning string */
        for (site = 0; site < proberID->noOfSites; site++)
        {
            switch (proberID->model)
            {
                case PHPFUNC_MOD_TEL_20S:
                    strcpy(binCode[site], "00000000");
                    if (site%4 == 0)
                        pfCode[site/4] = '0';
                    break;
                case PHPFUNC_MOD_TEL_P8:
                case PHPFUNC_MOD_TEL_P12:
                case PHPFUNC_MOD_TEL_F12:
                case PHPFUNC_MOD_TEL_78S:
                    strcpy(binCode[site], "@@@@@@@@");
                    if (site%4 == 0)
                        pfCode[site/4] = '@';
                    break;
                default: break;
            }
        }

        /* compose the binning information string */
        phEstateAGetSiteInfo(proberID->myEstate, &population, &entries);
        for (site = 0; site < proberID->noOfSites; site++)
        {
            /* get bin data or empty status */
            theBin = perSiteBinMap[site];
            passed = perSitePassed[site];
            empty = 0;

            switch (population[site])
            {
                case PHESTATE_SITE_POPULATED:
                    newPopulation[site] = PHESTATE_SITE_EMPTY;
                    break;
                case PHESTATE_SITE_POPDEACT:
                    newPopulation[site] = PHESTATE_SITE_DEACTIVATED;
                    break;
                case PHESTATE_SITE_EMPTY:
                case PHESTATE_SITE_DEACTIVATED:
                case PHESTATE_SITE_POPREPROBED:
                    empty = 1;
                    break;
            }

            if (theBin == -1)
                empty = 1;

            /* max_bins will be 32 in parity binning method, and will be configurable in binary method */
            if(!empty && (theBin < 0 || theBin >= proberID->p.max_bins))
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                        "invalid binning data\n"
                        "could not bin to bin index %ld at prober site \"%s\"",
                        theBin, proberID->siteIds[site]);
                return PHPFUNC_ERR_BINNING;
            }

            /* for binnary binning method, it takes the 5 bytes to represent binning number, last 3 bytes is still take the value of 0x01000000 = @ */
            /* e.g. 9999 = 0x270F = 0x01000010 0x01000111 0x01000000 0x01001111 ...... = O@GB@@@@ */
            /* e.g. 999999 = 0xF423F = 0x01001111 0x01000100 0x01000010 0x01000011 0x01001111 ...... = OCBDO@@@ */  
            if(!empty)
            {
                if(proberID->p.reversedBinning)
                {
                    if( proberID->p.binningMethod == PARITY_BINNING_METHOD )
                        binCode[proberID->noOfSites - site - 1][theBin>>2] |= (char) (1 << theBin%4);
                    else
                    {
                        /* binary binning-method */
                        binCode[proberID->noOfSites - site - 1][0] |= (char) (theBin&0x0F);
                        binCode[proberID->noOfSites - site - 1][1] |= (char) ((theBin>>4)&0x0F);
                        binCode[proberID->noOfSites - site - 1][2] |= (char) ((theBin>>8)&0x0F);
                        binCode[proberID->noOfSites - site - 1][3] |= (char) ((theBin>>12)&0x0F);
                        binCode[proberID->noOfSites - site - 1][4] |= (char) ((theBin>>16)&0x0F);
                    }

                    if(!passed)
                        pfCode[(proberID->noOfSites - site - 1)/4] |= (1 << (proberID->noOfSites - site - 1)%4);
                }
                else
                {
                    if( proberID->p.binningMethod == PARITY_BINNING_METHOD )
                        binCode[site][theBin>>2] |= (char) (1 << theBin%4);
                    else
                    {
                        /* binary binning-method */
                        binCode[site][0] |= (char) (theBin&0x0F);
                        binCode[site][1] |= (char) ((theBin>>4)&0x0F);
                        binCode[site][2] |= (char) ((theBin>>8)&0x0F);
                        binCode[site][3] |= (char) ((theBin>>12)&0x0F);
                        binCode[site][4] |= (char) ((theBin>>16)&0x0F);
                    }

                    if(!passed)
                        pfCode[site/4] |= (1 << site%4);
                }
            }
        }

        /* compose complete bin command */
        strcpy(command, "C");
        for (site = 0; site < proberID->noOfSites; site++)
        {
            if (site%4 == 0)
                strncat(command, &pfCode[site/4], 1);
            strcat(command, binCode[site]);
        }
        strcat(command, MY_EOC);
    }

    /* send the binning to the prober, wait for ackowledge and step */
    phPFuncTaStart(proberID);
    if (!proberID->p.dontBinDies)
    {
        /* only bin, if not configured not to bin */
        PhPFuncTaCheck(phPFuncTaSend(proberID, command));
        PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq1, 2, 0x59, 0x4d));
        if (srq1 == 0x4d)
        {
            /* handle continous fails, the prober is paused, need to wait
               for an unpause */
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                "prober has reported a continuous fail problem");
            phPFuncTaRemoveStep(proberID);
            return PHPFUNC_ERR_WAITING;
        }
    }

    if (! proberID->p.stIsMaster)
    {
        /* changed at TEL, recommendation: always send Q to perform
           the auto step, never use r. This would be difficult anyway
           in case of multi site probing */
        PhPFuncTaCheck(phPFuncTaSend(proberID, "Q%s", MY_EOC));
        do
        {
            PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq2, 4, 0x47, 0x4b, 0x48, 0x62));
            if (srq2 == 0x62)
            {
                /* prober pause detected */
                phEstateASetPauseInfo(proberID->myEstate, 1);
                phPFuncTaRemoveStep(proberID);
                return PHPFUNC_ERR_WAITING;
            }
            else
                phEstateASetPauseInfo(proberID->myEstate, 0);
        } while (srq2 == 0x62);
    }
    else
        retVal = PHPFUNC_ERR_OK;
    phPFuncTaStop(proberID);


    /* correct status */
    if (proberID->p.stIsMaster)
      // Fix CR-167079, "VF" is used to end wafer and prober recipe
      // It is controlled by operator when Prober or Smartest is master.
      proberID->p.dieProbed = 1;
    else
    {
        switch (srq2)
        {
          case 0x47:
            /* next die probed */
            proberID->p.dieProbed = 1;
            break;
          case 0x4b:
            /* wafer end */
            proberID->p.dieProbed = 0;
            retVal = PHPFUNC_ERR_PAT_DONE;
            break;
          case 0x48:
            /* this is an unexpected cassette end here. We deal with this
               problem by sending a pattern done here and also doing
               this on the upcoming unload wafer call */
            proberID->p.dieProbed = 0;
            phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_NOTLOADED);
            phEstateASetLotInfo(proberID->myEstate, 0);
            retVal = PHPFUNC_ERR_PAT_DONE;
            break;
        }
    }

    /* by default not implemented */
    return retVal;
}
#endif



#ifdef GETSUBDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next sub-die
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateGetSubDie(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *subdieX             /* in explicit probing mode
                                 (stepping_controlled_by is set to
                                 "smartest"), this is the relative X
                                 coordinate of the current sub die
                                 request, as sent to the prober. In
                                 autoindex mode
                                 (stepping_controlled_by is set to
                                 "prober" or "learnlist"), the
                                 coordinate is returned in this
                                 field. Any necessary transformations
                                 (index to microns, coordinate
                                 systems) must be performed by the
                                 calling framework */,
    long *subdieY             /* same for Y coordinate */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateGetSubDie");
}
#endif



#ifdef BINSUBDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested sub-dice
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateBinSubDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL if working
                                with sub die level */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateBinSubDie");
}
#endif



#ifdef DIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDieList(
    phPFuncId_t proberID      /* driver plugin ID */,
    int count                 /* number of entries in <dieX> and <dieY> */,
    long *dieX                /* this is the list of absolute X
                                 coordinates of the die learnlist as
                                 to be sent to the prober. Any
                                 necessary transformations (index to
                                 microns, coordinate systems, etc.)
                                 must be performed by the calling
                                 framework */,
    long *dieY                 /* same for Y coordinate */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateDieList");
}
#endif



#ifdef SUBDIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send sub-die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSubDieList(
    phPFuncId_t proberID      /* driver plugin ID */,
    int count                 /* number of entries in <dieX> and <dieY> */,
    long *subdieX             /* this is the list of relative X
                                 coordinates of the sub-die learnlist as
                                 to be sent to the prober. Any
                                 necessary transformations (index to
                                 microns, coordinate systems, etc.)
                                 must be performed by the calling
                                 framework */,
    long *subdieY             /* same for Y coordinate */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateSubDieList");
}
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe dice
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 *
 *      driver            prober
 *
 *      D        ----->         
 *               <-----   0x44
 *
 *      Z        ----->
 *               <-----   0x43 (0x53, 0xC3)
 *
 ***************************************************************************/
phPFuncError_t privateReprobe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    static int srq = 0;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    /* perform communication steps */
    phPFuncTaStart(proberID);
    PhPFuncTaCheck(phPFuncTaSend(proberID, "D%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 2, 0x44, 0x62));
    if (srq == 0x62)
    {
        phPFuncTaRemoveStep(proberID);
        return PHPFUNC_ERR_WAITING;        
    }
    PhPFuncTaCheck(riseChuck(proberID));
    phPFuncTaStop(proberID);

    /* by default not implemented */
    return retVal;
}
#endif



#ifdef CLEAN_IMPLEMENTED
/*****************************************************************************
 *
 * Clean the probe needles
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCleanProbe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    static int srq = 0;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    /* perform communication steps */
    phPFuncTaStart(proberID);
    PhPFuncTaCheck(phPFuncTaSend(proberID, "p%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 2, 0x59, 0x62));
    if (srq == 0x62)
    {
        phPFuncTaRemoveStep(proberID);
        return PHPFUNC_ERR_WAITING;        
    }
    phPFuncTaStop(proberID);

    /* by default not implemented */
    return retVal;
}
#endif



#ifdef PMI_IMPLEMENTED
/*****************************************************************************
 *
 * Perform a probe mark inspection
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privatePMI(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privatePMI");
}
#endif



#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSTPaused(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    int isPaused;
    phPFuncTaStart(proberID);
    if (proberID->p.pauseProber)
    {
        phEstateAGetPauseInfo(proberID->myEstate, &isPaused);
        if (!isPaused)
        {
            /* SmarTest is paused, but the prober is not yet paused */
            PhPFuncTaCheck(phPFuncTaSend(proberID, "e%s", MY_EOC));
            proberID->p.pauseInitiatedByST = 1;
            phEstateASetPauseInfo(proberID->myEstate, 1);
        }
        else
            proberID->p.pauseInitiatedByProber = 1;
    }
    phPFuncTaStop(proberID);
    return PHPFUNC_ERR_OK;
}
#endif



#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSTUnpaused(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    static int srq;
    int isPaused;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    phPFuncTaStart(proberID);
    phEstateAGetPauseInfo(proberID->myEstate, &isPaused);
    if (isPaused)
    {
        if (proberID->p.pauseInitiatedByST)
        {
            /* SmarTest is unpaused, we need to wait until the prober resumes */
            PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 2, 0x59, 0x4b));
            proberID->p.pauseInitiatedByST = 0;
            phEstateASetPauseInfo(proberID->myEstate, 0);
            if (srq == 0x4b)
            {
                /* end of wafer received after pause */
                proberID->p.dieProbed = 0;
                retVal = PHPFUNC_ERR_PAT_DONE;
            }
        }
        if (proberID->p.pauseInitiatedByProber)
        {
            proberID->p.pauseInitiatedByProber = 0;
        }
    }
    phPFuncTaStop(proberID);
    return retVal;
}
#endif



#ifdef TEST_IMPLEMENTED
/*****************************************************************************
 *
 * send a command string to the prober
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateTestCommand(
    phPFuncId_t proberID     /* driver plugin ID */,
    char *command            /* command string */
)
{
    phPFuncTaStart(proberID);
    phPFuncTaStop(proberID);
    /* by default not implemented */
    ReturnNotYetImplemented("privateTestCommand");
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDiag(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **diag              /* pointer to probers diagnostic output */
)
{
  *diag = phLogGetLastErrorMessage();
  return PHPFUNC_ERR_OK;
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateStatus(
    phPFuncId_t proberID                /* driver plugin ID */,
    phPFuncStatRequest_t action         /* the current status action
                                           to perform */,
    phPFuncAvailability_t *lastCall     /* the last call to the
                                           plugin, not counting calls
                                           to this function */
)
{
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    /* don't change state counters */
    /* phPFuncTaStart(proberID); */
    switch (action)
    {
      case PHPFUNC_SR_QUERY:
        /* we can handle this query globaly */
        if (lastCall)
            phPFuncTaGetLastCall(proberID, lastCall);
        break;
      case PHPFUNC_SR_RESET:
        /* The incomplete function should reset and retried as soon as
           it is called again. In case last incomplete function is
           called again it should now start from the beginning */
        phPFuncTaDoReset(proberID);
        break;
      case PHPFUNC_SR_HANDLED:
        /* in case last incomplete function is called again it should
           now start from the beginning */
        phPFuncTaDoReset(proberID);
        break;
      case PHPFUNC_SR_ABORT:
        /* the incomplete function should abort as soon as it is
           called again */
        phPFuncTaDoAbort(proberID);
        break;
    }

    /* don't change state counters */
    /* phPFuncTaStop(proberID); */
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateUpdateState(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    phPFuncTaStart(proberID);
    phPFuncTaStop(proberID);
    /* by default not implemented */
    ReturnNotYetImplemented("privateUpdateState");
}
#endif



#ifdef CASSID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current cassette ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCassID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **cassIdString      /* resulting pointer to cassette ID string */
)
{
    static char cassId[256] = "A Cassette";

    /* by default not implemented */
    *cassIdString = cassId;
    ReturnNotYetImplemented("privateCassID");
}
#endif



#ifdef WAFID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current wafer ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateWafID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **wafIdString      /* resulting pointer to wafer ID string */
)
{
    static char wafSetup[512] = "";
    static char wafId[256] = "";
    wafId[0] = '\0';
    *wafIdString = wafId;
    int len;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    phPFuncTaStart(proberID);
    switch (proberID->model)
    {
      case PHPFUNC_MOD_TEL_20S:
        PhPFuncTaCheck(phPFuncTaSend(proberID, "W%s", MY_EOC));
        PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%511[^\n\r]", wafSetup));
    break;
      case PHPFUNC_MOD_TEL_78S:
      case PHPFUNC_MOD_TEL_P8:
      case PHPFUNC_MOD_TEL_P12:
      case PHPFUNC_MOD_TEL_F12:
        PhPFuncTaCheck(phPFuncTaSend(proberID, "n%s", MY_EOC));
        PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%20c", wafSetup));
        wafSetup[20] = '\0';
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
     default: break;
/* End of Huatek Modification */
    }
    phPFuncTaStop(proberID);
    
    switch (proberID->model)
    {
      case PHPFUNC_MOD_TEL_20S:
        if (strlen(wafSetup) > 25 || strlen(wafSetup) == 0)
        {
            phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
                "could not receive wafer identification with command \"W\"\n"
                "Please check prober setup parameters and dip switches\n");
            strcpy(wafId, "ID not available");
        }
        else
            strcpy(wafId, wafSetup);
        break;
      case PHPFUNC_MOD_TEL_78S:
      case PHPFUNC_MOD_TEL_P8:
      case PHPFUNC_MOD_TEL_P12:
      case PHPFUNC_MOD_TEL_F12:
        /* copy wafer ID */
        wafId[0] = '\0';
        strncpy(wafId, wafSetup, 20);
        wafId[20] = '\0';
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
    }

    /* remove trailing white space */
    len = strlen(wafId);
    while (len > 0 && isspace(wafId[len-1]))
    {
        wafId[len-1] = '\0';
        len--;
    }
    return retVal;
}
#endif



#ifdef PROBEID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current probe card ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateProbeID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **probeIdString     /* resulting pointer to probe card ID string */
)
{
    static char probeId[256] = "A Probe Card";

    /* by default not implemented */
    *probeIdString = probeId;
    ReturnNotYetImplemented("privateProbeID");
}
#endif



#ifdef LOTID_IMPLEMENTED
/*****************************************************************************
 *
 * return the current lot ID
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateLotID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **lotIdString       /* resulting pointer to lot ID string */
)
{
    static char lotSetup[512] = "";
    static char lotId[256] = "";
    lotId[0] = '\0';
    *lotIdString = lotId;
    int len;
    int lotStarted;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    phEstateAGetLotInfo(proberID->myEstate, &lotStarted);
    if (!lotStarted)
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING, 
            "can not retrieve lot ID as long as lot is not started");
        strcpy(lotId, "lot_id_not_available");
        *lotIdString = lotId;
        return PHPFUNC_ERR_OK;
    }

    phPFuncTaStart(proberID);
    switch (proberID->model)
    {
      case PHPFUNC_MOD_TEL_20S:
        PhPFuncTaCheck(phPFuncTaSend(proberID, "g%s", MY_EOC));
        PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%41c", lotSetup));
        lotSetup[41] = '\0';
        break;
      case PHPFUNC_MOD_TEL_78S:
      case PHPFUNC_MOD_TEL_P8:
      case PHPFUNC_MOD_TEL_P12:
      case PHPFUNC_MOD_TEL_F12:
        PhPFuncTaCheck(phPFuncTaSend(proberID, "g%s", MY_EOC));
        PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%23c", lotSetup));
        lotSetup[23] = '\0';
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
    }
    phPFuncTaStop(proberID);
    
    switch (proberID->model)
    {
      case PHPFUNC_MOD_TEL_20S:
        /* copy portion of lot name */
        lotId[0] = '\0';
        strncpy(lotId, &lotSetup[3], 18);
        lotId[18] = '\0';
        break;
      case PHPFUNC_MOD_TEL_78S:
      case PHPFUNC_MOD_TEL_P8:
      case PHPFUNC_MOD_TEL_P12:
      case PHPFUNC_MOD_TEL_F12:
        /* copy portion of lot name */
        lotId[0] = '\0';
        strncpy(lotId, &lotSetup[3], 20);
        lotId[20] = '\0';
        break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
     default: break;
/* End of Huatek Modification */
    }

    /* remove trailing white space */
    len = strlen(lotId);
    while (len > 0 && isspace(lotId[len-1]))
    {
        lotId[len-1] = '\0';
        len--;
    }
    return retVal;
}
#endif



#ifdef STARTLOT_IMPLEMENTED
/*****************************************************************************
 *
 * Start a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateStartLot(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    phPFuncError_t retVal = PHPFUNC_ERR_OK;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int i;*/
/*    int count;*/
/* End of Huatek Modification */

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    phPFuncTaStart(proberID);
    PhPFuncTaCheck(waitLotStarted(proberID));
    phPFuncTaStop(proberID);

    /* don't do this withing TaStart and TaStop, because it may change
       the control flow */
    phEstateASetLotInfo(proberID->myEstate, 1);

    return retVal;
}
#endif



#ifdef ENDLOT_IMPLEMENTED
/*****************************************************************************
 *
 * End a lot
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateEndLot(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    phPFuncError_t retVal = PHPFUNC_ERR_OK;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int i; */
/*    int count; */
/* End of Huatek Modification */

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    phEstateASetLotInfo(proberID->myEstate, 0);

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
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateCommTest(
    phPFuncId_t proberID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
)
{
    char *mNum;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
        return PHPFUNC_ERR_ABORTED;

    phPFuncTaStart(proberID);
    retVal = machineNumber(proberID, &mNum);
    phPFuncTaStop(proberID);

    /* return result, there is no real check for the model here (so far) */
    if (testPassed)
        *testPassed = (retVal == PHPFUNC_ERR_OK) ? 1 : 0;
    return retVal;
}
#endif


#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Get Information/Parameter/Status from Prober
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *   Implements the following information requested by CR27092&CR25172:
 *    (1)STDF WCR,
 *    (2)Wafer Number, Lot Number, Chuck Temperatue, Name of
 *       Prober setup, Cassette Status.
 *   Implements the following information requested by CR21589
 *    (1)z_contact_height, planarity_window
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateGetStatus(
        phPFuncId_t proberID        /* driver plugin ID */,
        char *commandString   /* key name to get parameter/information */,
        char **responseString       /* output of response string */

)
{
  static const phStringPair_t sGpibCommands[] = {
    {PHKEY_NAME_PROBER_STATUS_CASS_STATUS,                "S"},

    {PHKEY_NAME_PROBER_STATUS_CENTER_X,                   "CD"},
    {PHKEY_NAME_PROBER_STATUS_CENTER_Y,                   "CD"},

    {PHKEY_NAME_PROBER_STATUS_CHUCK_TEMP,                 "f1"}, /* chuck current temperature */

    {PHKEY_NAME_PROBER_STATUS_DIE_HEIGHT,                 "DD"},
    {PHKEY_NAME_PROBER_STATUS_DIE_WIDTH,                  "DD"},

    {PHKEY_NAME_PROBER_STATUS_ERROR_CODE,                 "s"},
    {PHKEY_NAME_PROBER_STATUS_LOT_NUMBER,                 "g"},
    {PHKEY_NAME_PROBER_STATUS_MULTISITE_LOCATION_NUMBER,  "G"},

    {PHKEY_NAME_PROBER_STATUS_POS_X,                      "WD"},
    {PHKEY_NAME_PROBER_STATUS_POS_Y,                      "WD"},

    /*{PHKEY_NAME_PROBER_STATUS_PROBER_SETUP_NAME,          "?"},*/

    {PHKEY_NAME_PROBER_STATUS_WAFER_FLAT,                 "WF"},

    {PHKEY_NAME_PROBER_STATUS_WAFER_ID,                   "n"},  /*so far not implemented, but we have privateWafId*/

    {PHKEY_NAME_PROBER_STATUS_WAFER_NUMBER,               "w"},  /* uncertain! */

    {PHKEY_NAME_PROBER_STATUS_WAFER_SIZE,                 "WS"},

    /*{PHKEY_NAME_PROBER_STATUS_WAFER_STATUS,               "?"},*/

    {PHKEY_NAME_PROBER_STATUS_WAFER_UNITS,                "WU"}
  };

  static const phStringPair_t sFormat[] = {
    {PHKEY_NAME_PROBER_STATUS_CASS_STATUS,                "%s"},

    {PHKEY_NAME_PROBER_STATUS_CENTER_X,                   "%s"},
    {PHKEY_NAME_PROBER_STATUS_CENTER_Y,                   "%s"},

    {PHKEY_NAME_PROBER_STATUS_CHUCK_TEMP,                 "%s"},

    {PHKEY_NAME_PROBER_STATUS_DIE_HEIGHT,                 "%s"},
    {PHKEY_NAME_PROBER_STATUS_DIE_WIDTH,                  "%s"},

    {PHKEY_NAME_PROBER_STATUS_ERROR_CODE,                 "%s"},
    {PHKEY_NAME_PROBER_STATUS_LOT_NUMBER,                 "%s"},
    {PHKEY_NAME_PROBER_STATUS_MULTISITE_LOCATION_NUMBER,  "%198c"},

    {PHKEY_NAME_PROBER_STATUS_POS_X,                      "%s"},
    {PHKEY_NAME_PROBER_STATUS_POS_Y,                      "%s"},

    {PHKEY_NAME_PROBER_STATUS_PROBER_SETUP_NAME,          "%s"},

    {PHKEY_NAME_PROBER_STATUS_WAFER_FLAT,                 "%s"},

    {PHKEY_NAME_PROBER_STATUS_WAFER_ID,                   "%s"},

    {PHKEY_NAME_PROBER_STATUS_WAFER_NUMBER,               "%s"},

    {PHKEY_NAME_PROBER_STATUS_WAFER_SIZE,                 "%s"},

    {PHKEY_NAME_PROBER_STATUS_WAFER_STATUS,               "%s"},

    {PHKEY_NAME_PROBER_STATUS_WAFER_UNITS,                "%s"}
  };

  static char response[MAX_STATUS_MSG_LEN] = "";
  static char buffer[MAX_STATUS_MSG_LEN] = "";
  char result[MAX_STATUS_MSG_LEN] = "";
  phPFuncError_t retVal = PHPFUNC_ERR_OK;
  char *token = NULL;
  const phStringPair_t *pStrPair = NULL;
  const phStringPair_t *pFormatStrPair = NULL;
  int usedLen = 0;
  response[0] = '\0';
  *responseString = response;


  token = strtok(commandString, " \t\n\r");

  if( token == NULL ) {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING, 
                     "No token found in the input parameter of PROB_HND_CALL: 'ph_get_status'");
    return PHPFUNC_ERR_NA;
  }

  if (phPFuncTaAskAbort(proberID))
    return PHPFUNC_ERR_ABORTED;

  phPFuncTaStart(proberID);

  /* 
  * the user may query several parameter one time in "ph_get_status", e.g.
  *     *wafer_size_and_wafer_flat = PROB_HND_CALL(ph_get_stauts wafer_size wafer_flat);
  */
  strcpy(response, "");
  while ( token != NULL ) {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "phFuncGetStatus, token = ->%s<-", token);

    if( strcasecmp(token, "getenablesites") == 0)
      getEnableSites(proberID, result);
    else if(strcasecmp(token,"isContactCheckEnabled") == 0)
    {
       char reason[128] = "";
       if(checkContactCheckStatus(proberID, reason))
         strcpy(result, "yes.");
       else
       {
         phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING, "checkContactCheckStatus return false");
         sprintf(result, "no. reason:%s", reason);
       }
    }
    else if( strcasecmp(token, "AUTOZ_GetPreZUPSteps") == 0 )
    {
      phLogFuncMessage(proberID->myLogger, LOG_DEBUG, "the Z UP times of previous die is:%d",ZUPSteps);
      char times[32] = "";
      itoS(ZUPSteps, times);
      strcpy(result, times);
      // reset ZUPSteps
      ZUPSteps = 0;
    }
    else if( strcasecmp(token,"AUTOZ_GetCurrentZHeight") == 0 )
    {
      char* buf =  displayCurrentZheight(proberID);
      phLogFuncMessage(proberID->myLogger, LOG_DEBUG, "in getStatus, z current height:%s",buf);
      strcpy(result, buf);
    }
    else
    {
      pStrPair = phBinSearchStrValueByStrKey(sGpibCommands, LENGTH_OF_ARRAY(sGpibCommands), token);
      if( pStrPair != NULL ) {
        PhPFuncTaCheck(phPFuncTaSend(proberID, "%s%s", pStrPair->value, MY_EOC));
        strcpy(buffer, "");
        pFormatStrPair = phBinSearchStrValueByStrKey(sFormat, LENGTH_OF_ARRAY(sFormat), token);
        if ( pFormatStrPair != NULL )
        {
          PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, pFormatStrPair->value, buffer));
        } else
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                           "The search value can not be found by the search token");
        }

        /*
        * here, anything is OK, the query was sent out and the response was received.
        * We need further handle the response, for example:
        *    buffer = "1", when we request the "pos_x" (positive direction of X)
        *  after analysis,
        *    result = "Right->Left"
        */
        if ( analysisResultOfGetStatus(proberID, pStrPair->key, buffer, result) != SUCCEED ){
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                           "The response = ->%s<- from the Prober cannot be correctly parsed\n"
                           "This could be an internal error from either Prober or Driver", buffer);
          /* but we still keep the result though it is not parsed correctly*/
          strcpy(result,buffer);
        }

        /* until here, the "result" contains the parsed response from Prober */
      } else {
        if (strcmp(token, PHKEY_NAME_Z_CONTACT_HEIGHT) == 0)
        {
          if (sZContactHeight == -1)
          {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                             "z_contact_height not exist, ph_contact_test should be called beforehand");
          }
          sprintf(result, "%d", sZContactHeight);
        } else if (strcmp(token, PHKEY_NAME_PLANARITY_WINDOW) == 0)
        {
          if (sPlanarityWindow == -1)
          {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                             "planarity_window not exist, ph_contact_test should be called beforehand");
          }
          sprintf(result, "%d", sPlanarityWindow);
        }
        else
        {
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                           "The key \"%s\" is not available, or may not be supported yet", token);
          sprintf(result, "%s_KEY_NOT_AVAILABLE", token);
        }
      }
    }

    /* step to next token */
    token = strtok(NULL, " \t\n\r");

    if( (strlen(result)+usedLen+2) >= MAX_STATUS_MSG_LEN ) {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                       "the response string is too long; information query just stop");
      break;
    } else {
      strcat(response, result);
      usedLen += strlen(result);
      if( token != NULL ) {
        strcat(response, ", ");
        usedLen += 2;
      }
    }

    strcpy(result, "");
  } /* end of while ( token ) */

  /* ensure null terminated */
  response[strlen(response)] = '\0';

  phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE, 
                   "privateGetStatus, answer ->%s<-, length = %d",
                   response, strlen(response));
  
  phPFuncTaStop(proberID);
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
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateDestroy(
      phPFuncId_t proberID                 /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateDestroy");
}
#endif

#ifdef SIMULATE_DCCTEST
/*****************************************************************************
 *
 * simulate ExecTfParam
 *
 * Authors: Kun Xiao Shangai
 *
 * Description:
 * This function simualtes ExecTfParam procedure in test mode, so we only 
 * set the measure result(FAIL/PASS) per pin, the measure value is ignored. 
 * We do not simulate the test function exectued by ExecTfParam, and it is 
 * always supposed to be executed successfully.

 ***************************************************************************/
static void simuExecTfParam(
       char *param                       /* test function parameters */, 
       FLOAT64 result[TF_MAX_NR_RSLTS]   /* array that holds the test results */, 
       INT32 *funcState                  /* return code telling whether the testfunction
                                            has executed successfully or not*/, 
       int zheight                       /* current z height */
)
{
  int i;


  /*test function passed*/
  *funcState = 0; 

  /* pin results are always available */
  sState = TF_PIN_RESULTS_AVAILABLE;

  /* set test results of pins */
  if(zheight<REAL_FIRST_SOME_PASS_Z_HEIGHT)
  { /* current z height is lower than real first_some_pass point */

    for (i = 0; i < CONTACTTEST_NUM_OF_PINS; i++)
    {
      sPinResult[i].meas[0].result = 1; /*all pins failed*/    
    }
  }
  else if(zheight<REAL_ALL_PASS_Z_HEIGHT)
  { /*
     * current z height is between real first_some_pass point and 
     * real all_pass point 
    */

    int numOfPassPins;
    if ((REAL_ALL_PASS_Z_HEIGHT - REAL_FIRST_SOME_PASS_Z_HEIGHT) != 0) 
    {
      numOfPassPins = CONTACTTEST_NUM_OF_PINS * (zheight - REAL_FIRST_SOME_PASS_Z_HEIGHT)/(REAL_ALL_PASS_Z_HEIGHT - REAL_FIRST_SOME_PASS_Z_HEIGHT);
      if (numOfPassPins == 0)
      {
        numOfPassPins = 1; /*at least one pin passed*/    
      }
    }
    else
    {
      numOfPassPins = CONTACTTEST_NUM_OF_PINS;
    }

    for (i = 0; i < numOfPassPins; i++)
    {
      sPinResult[i].meas[0].result = 0; /*numOfPassPins pins passed test*/
    }
    for (i = numOfPassPins; i < CONTACTTEST_NUM_OF_PINS; i++)
    {
      sPinResult[i].meas[0].result = 1; /*remnant pins failed*/  
    }
  }
  else 
  { /*current z height exceeds real all_pass point*/
 
    for (i = 0; i < CONTACTTEST_NUM_OF_PINS; i++)
    {
      sPinResult[i].meas[0].result = 0; /*all pins passed test*/ 
    }
  }   
}
#endif

#ifdef SIMULATE_DCCTEST
/*****************************************************************************
 *
 * simulate GetPinResults
 *
 * Authors: Kun Xiao Shanghai
 * Description:
 * This function gets the pin results after excecution of 
 * simuExecTfParam
 *
 ***************************************************************************/
static void simuGetPinResults (
       TF_PIN_RESULT pin_result[]  /* array that holds the individual 
                                           result of each pin */, 
       INT32 *nr_of_pins           /* upon entering this function "nr_of_pins" should 
                                           hold the dimensioned size of the "pin_result" array,
                                           upon returning "nr_of_pins" means the number of available
                                           pins in the "pin_result" array */, 
       INT32 *pinstate             /* pin result state */
)
{
  int i;


  if (*nr_of_pins <= CONTACTTEST_NUM_OF_PINS)
  {
    *pinstate = TF_PIN_RESULTS_ARRAY_TOO_SMALL;
    *nr_of_pins = 0;
    return;
  }

  *pinstate = sState;
  *nr_of_pins = CONTACTTEST_NUM_OF_PINS;

  for (i=0; i<CONTACTTEST_NUM_OF_PINS; i++)
  {
    pin_result[i] = sPinResult[i];  
  }
}
#endif

/*****************************************************************************
 *
 * phDCContinuityTest
 *
 * Authors: Fabrizio Arca - EDO, Kun Xiao Shanghai
 *
 * Description: simulation of performing continuity test
 *
 ***************************************************************************/
#ifdef SIMULATE_DCCTEST
static phDCContinuityTestResult_t simuPhDCContinuityTest(
       phPFuncId_t proberID                      /* driver plugin ID */,     
       phContacttestSetup_t contacttestSetupId   /* contact test ID */,
       int zheight                               /* current z height */      
)
{

  INT32 tf_state, pinCount = CONTACTTEST_MAX_NUM_OF_PINS;
  FLOAT64 tf_result[TF_MAX_NR_RSLTS];
  TF_PIN_RESULT pinResults[CONTACTTEST_MAX_NUM_OF_PINS];
  int i = 0, iPin = 0, numPass = 0, numPins = 0;
  char pinGroupName[32];
  double forceCurrent;
  double lowLimit;
  double highLimit;
  char parameters[100];
  BULinkedItem *p = NULL;
  phTestCondition_t testCondition = NULL;

    
  p = contacttestSetupId->digitalPingroup->first;
  while (p != NULL) 
  {
    testCondition = (phTestCondition_t)(p->data);
    strcpy(pinGroupName, testCondition->pinGroupName);
    forceCurrent = testCondition->forceCurrent;
    lowLimit = testCondition->lowLimit;
    highLimit = testCondition->highLimit;

    sprintf(parameters, "continuity;%s;0;[micro]A;%f;mV;%f;mV;%f;;ms;1;0;Continuity($P)", 
            pinGroupName, forceCurrent, lowLimit, highLimit);   

    for (i = 0; i < proberID->noOfSites; i++) 
    {
      /* need to focus specific site to execute test function? */
      simuExecTfParam(parameters, tf_result, &tf_state, zheight);
      simuGetPinResults(pinResults, &pinCount, &tf_state);
      numPins = numPins + pinCount;

      switch(tf_state)
      {
        case TF_PIN_RESULTS_NOT_AVAILABLE:
          //ReportMessage(UI_MESSAGE,"No pin results available");
          break;

        case TF_PIN_RESULTS_AVAILABLE:
        case TF_PIN_RESULTS_AND_VAL_AVAILABLE:
          for (iPin=0; iPin < pinCount; iPin++) 
          {
             if (pinResults[iPin].meas[0].result == 0)
             {
               numPass++;
             }
          }
          break;

        default:
          break;
      } 
    } /* end for i < proberID->noOfSites */  
   
    p = p->next;
  } /* end while p!=NULL */ 

  if(numPass == 0)
  {
    return ALL_FAIL;
  } else if (numPass == numPins)
  {
    return ALL_PASS;
  }
  else
  {
    return SOME_PASS;
  }
  
}
#endif

#if 0
/* comment for 'defined but not used' */
/*****************************************************************************
 *
 * phDCContinuityTest
 *
 * Authors: Fabrizio Arca - EDO, Kun Xiao Shanghai
 *
 * Description: perform continuity test
 *
 ***************************************************************************/
static phDCContinuityTestResult_t phDCContinuityTest(
       phPFuncId_t proberID                      /* driver plugin ID */,   
       phContacttestSetup_t contacttestSetupId   /* contact test ID */
)
{

  INT32 tf_state, pinCount = CONTACTTEST_MAX_NUM_OF_PINS;
  FLOAT64 tf_result[TF_MAX_NR_RSLTS];
  TF_PIN_RESULT pinResults[CONTACTTEST_MAX_NUM_OF_PINS];
  int i = 0, iPin = 0, numPass = 0, numPins = 0;
  char pinGroupName[32];
  double forceCurrent;
  double lowLimit;
  double highLimit;
  char parameters[100];
  BULinkedItem *p = NULL;
  phTestCondition_t testCondition = NULL;


  p = contacttestSetupId->digitalPingroup->first;
  while (p != NULL) 
  {
    testCondition = (phTestCondition_t)(p->data);
    strcpy(pinGroupName, testCondition->pinGroupName);
    forceCurrent = testCondition->forceCurrent;
    lowLimit = testCondition->lowLimit;
    highLimit = testCondition->highLimit;

    sprintf(parameters, "continuity;%s;0;[micro]A;%f;mV;%f;mV;%f;;ms;1;0;Continuity($P)", 
            pinGroupName, forceCurrent, lowLimit, highLimit);      

    for (i = 0; i < proberID->noOfSites; i++) 
    {
      /* need to focus specific site to execute test function? */
      ExecTfParam(parameters, tf_result, &tf_state);
      GetPinResults(pinResults, &pinCount, &tf_state);
      numPins = numPins + pinCount;

      switch(tf_state)
      {
        case TF_PIN_RESULTS_NOT_AVAILABLE:
          ReportMessage(UI_MESSAGE,"No pin results available");
          break;

        case TF_PIN_RESULTS_AVAILABLE:
        case TF_PIN_RESULTS_AND_VAL_AVAILABLE:
          for (iPin=0; iPin < pinCount; iPin++) 
          {
             if (pinResults[iPin].meas[0].result == 0)
             {
               numPass++;
             }
          }
          break;

        default:
          break;
      } 
    } /* end for i < proberID->noOfSites */  
   
    p = p->next;
  } /* end while p!=NULL */ 

  if(numPass == 0)
  {
    return ALL_FAIL;
  } else if (numPass == numPins)
  {
    return ALL_PASS;
  }
  else
  {
    return SOME_PASS;
  }  
}
#endif

/*****************************************************************************
 *
 * indexDie
 *
 * Authors: Fabrizio Arca - EDO, Kun Xiao Shangai
 *
 * Description: move chuck to the required die position
 *
 ***************************************************************************/
static phPFuncError_t indexDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    int x,
    int y
)
{
  static int xcur, ycur;
  static int srq = 0;
  phPFuncError_t retVal = PHPFUNC_ERR_OK;


  if (phPFuncTaAskAbort(proberID))
  {
    return PHPFUNC_ERR_ABORTED;
  }

  phPFuncTaStart(proberID);  
  PhPFuncTaCheck(phPFuncTaSend(proberID,
                               "b%03d%03d%s", x, y, MY_EOC));
  
  /* answer from prober */
  PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 3, 0x47, 0x4f, 0x4b));
  phPFuncTaStop(proberID);
  
  switch (srq)
        {
          case 0x47:
            /* indexed to die position */
            break;
          case 0x4f:
            /* 
             * position outside of wafer, keep site flags
             * unchanged, write warning message and return OK 
            */
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                             "requested die position [%d, %d] is outside of wafer "
                             "(SRQ 0x%02x)", x, y, (int) srq);
            phEstateASetPauseInfo(proberID->myEstate, 0);
            return PHPFUNC_ERR_CONFIG;
            break;
          case 0x4b:
            /* position of wafer and end of probing (78S and P8 only) */
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                             "probing aborted by prober (SRQ 0x%02x)", (int) srq);
            retVal = PHPFUNC_ERR_PARAM;
            return retVal;
            break;
        }

  /* request current die position */
  phPFuncTaStart(proberID);
  PhPFuncTaCheck(phPFuncTaSend(proberID,
                               "A%s", MY_EOC));

  /* answer from prober */
  PhPFuncTaCheck(phPFuncTaReceive(proberID, 2, "%3d%3d", &xcur, &ycur));
  phPFuncTaStop(proberID);

  /* check current die position are the same as requested */
  if ((x != xcur)||(y != ycur))
  {
     phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                      "requested die position not achieved");
     retVal = PHPFUNC_ERR_ANSWER;
  }
  
  return retVal;
}


/*****************************************************************************
 *
 * moveChuck
 *
 * Authors: Fabrizio Arca - EDO, Kun Xiao Shangai
 *
 * Description: move chuck up and down
 *
 ***************************************************************************/
static phPFuncError_t moveChuck(
    phPFuncId_t proberID       /* driver plugin ID */,
    phDirection_t dir                    /* movement direction */ ,
    int incremental            /* step size */,
    int *zHeight             /* current z-height */,
    int zHeightMax           /* max z-height */,
    int zHeightMin           /* min z-height */
)
{
  static int srq = 0;
  phPFuncError_t retVal = PHPFUNC_ERR_OK;


  if (phPFuncTaAskAbort(proberID))
  {
    return PHPFUNC_ERR_ABORTED;
  }

  if (dir == UP || dir ==DOWN) 
  {
    if ((incremental < CONTACTTEST_MIN_STEP) || (incremental > CONTACTTEST_MAX_STEP))
    {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                       "incremental value not in the range of [%d, %d]",CONTACTTEST_MAX_STEP, CONTACTTEST_MIN_STEP);
      return PHPFUNC_ERR_CONFIG;
    }
  }

  phPFuncTaStart(proberID);
  switch( dir ) 
  {
    case BASELINE:   /* move to baseline*/

      PhPFuncTaCheck(phPFuncTaSend(proberID,
                                   "D%s", MY_EOC));

      /* answer from prober */
      PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x44));
      if (srq != 0x44)
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "prober cannot reach home position");
        retVal = PHPFUNC_ERR_ANSWER;
      }
      else
      {
        *zHeight = 0;
      }
      break; 

    case UP:   /* move UP */

      if ((*zHeight + incremental) > zHeightMax)
      {
         phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,"current z-height(%d) + %d will exceed maximum z-heigt(%d), chuck move up not performed",*zHeight,incremental,zHeightMax);
         retVal = PHPFUNC_ERR_Z_LIMIT;
         break;
      }

      PhPFuncTaCheck(phPFuncTaSend(proberID,
                                   "Z+%02d%s", incremental, MY_EOC));
        
      /* answer from prober */
      PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 2, 0x43, 0x53));
      if (srq == 0x53)
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "Z Limit ERROR");
        retVal = PHPFUNC_ERR_Z_LIMIT;
      } 
      else if (srq == 0x43)
      {
        *zHeight = (*zHeight) + incremental;
      }
      else
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "prober cannot execute Z step");
        retVal = PHPFUNC_ERR_ANSWER;
      }
      break;

    case DOWN:   /* move DOWN */

      if ((*zHeight - incremental) < zHeightMin) 
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "z-height will be below minimum z-height, chuck move down not performed!");
        return PHPFUNC_ERR_Z_LIMIT;
      }
      else
      {
        *zHeight = (*zHeight) - incremental;
      }

      PhPFuncTaCheck(phPFuncTaSend(proberID,
                                   "Z-%02d%s", incremental, MY_EOC));

      /* answer from prober */
      PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 2, 0x43, 0x53));     
      if (srq == 0x53)
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "Z Limit ERROR");
        retVal = PHPFUNC_ERR_Z_LIMIT;
      }   
      else if(srq != 0x43)
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "prober cannot execute Z step");
        retVal = PHPFUNC_ERR_ANSWER;
      }     
      break;

    default:
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                       "Error movement direction! ");
      retVal = PHPFUNC_ERR_PARAM;
  }
  phPFuncTaStop(proberID);

  return retVal;
}


/*****************************************************************************
 *
 * setContactPosition
 *
 * Authors: Fabrizio Arca - EDO, Kun Xiao Shangai
 *
 * Description: set the current z height as the contact position
 *
 ***************************************************************************/
static phPFuncError_t setContactPosition(
    phPFuncId_t proberID     /* driver plugin ID */)

{
  static int srq = 0;
  phPFuncError_t retVal = PHPFUNC_ERR_OK;


  if (phPFuncTaAskAbort(proberID))
  {
    return PHPFUNC_ERR_ABORTED;
  }

  phPFuncTaStart(proberID);
  PhPFuncTaCheck(phPFuncTaSend(proberID,
                               "z%s", MY_EOC));

  /* answer from prober */
  PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x59));
  phPFuncTaStop(proberID);
  
  if(srq != 0x59)
  {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                     "prober cannot set contact positionZ");
    retVal = PHPFUNC_ERR_ANSWER;
    return retVal;
  }

  return PHPFUNC_ERR_OK;
}

#ifdef CONTACTTEST_IMPLEMENTED  
/*****************************************************************************
 *
 * contact test
 *
 * Authors: Fabrizio Arca - EDO, Kun Xiao Shangai
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateContacttest(
    phPFuncId_t proberID                     /* driver plugin ID */,
    phContacttestSetup_t contactestSetupId   /* contact test ID */
)
{
  int xDie, yDie, largeStep, smallStep, safetyWindow, zHeightMax;
  int zHeightOfFirstSomePass, zHeightOfAllPass, zCurrentHeight;
  phDCContinuityTestResult_t checkResult = ALL_FAIL, preCheckResult = ALL_FAIL;
  phContactPointSelection_t contactPointSelection;
  phPFuncError_t retVal = PHPFUNC_ERR_OK; 
  int i;


  largeStep = contactestSetupId->largeStep;
  smallStep = contactestSetupId->smallStep;
  safetyWindow = contactestSetupId->safetyWindow; 
  zHeightMax = contactestSetupId->zLimit;
  contactPointSelection = contactestSetupId->contactPointSelection;

  for (i = 0; i < PH_MAX_NUM_COORD; i++) 
  {
    zHeightOfFirstSomePass = -1, zHeightOfAllPass = -1;
    checkResult = ALL_FAIL, preCheckResult = ALL_FAIL;
    /* Move chuck at the baseline and reinitialize */
    retVal = moveChuck(proberID, BASELINE, 0, &zCurrentHeight, zHeightMax, 0);
    

    /* index selected die */
    xDie = contactestSetupId->dieCoordinate[i].x;
    yDie = contactestSetupId->dieCoordinate[i].y;

    phLogFuncMessage(proberID->myLogger,PHLOG_TYPE_MESSAGE_4, "Begin to index die (x: %d, y:%d)", xDie,yDie);
    PhPFuncTaCheckStop(retVal = indexDie(proberID, xDie, yDie)); 
    
    while ((checkResult == ALL_FAIL) && (retVal != PHPFUNC_ERR_Z_LIMIT)) 
    {
      /* raise chuck by large step */
      PhPFuncTaCheckStop(retVal = moveChuck(proberID, UP, largeStep, &zCurrentHeight, zHeightMax, 0)); 

      /* perform continuity Test */
      #ifdef SIMULATE_DCCTEST
        checkResult = simuPhDCContinuityTest (proberID, contactestSetupId, zCurrentHeight);
      #else
        checkResult = phDCContinuityTest (proberID, contactestSetupId);
      #endif

      if (retVal == PHPFUNC_ERR_Z_LIMIT)
      {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                         "Z-height exceeded Z-Limit");
      }
    }  /* exit while  */

    while (((checkResult == SOME_PASS) || (checkResult == ALL_PASS)) && (retVal != PHPFUNC_ERR_Z_LIMIT))
    {
      /* Lower chuck by small step */
      PhPFuncTaCheckStop(retVal = moveChuck(proberID, DOWN, smallStep, &zCurrentHeight, zHeightMax, 0)); 

      /*record pre phDCContinuityTest result*/
      preCheckResult = checkResult;

      /* perform continuity Test */
      #ifdef SIMULATE_DCCTEST
        checkResult = simuPhDCContinuityTest (proberID, contactestSetupId, zCurrentHeight);
      #else
        checkResult = phDCContinuityTest (proberID, contactestSetupId);
      #endif
    }  /* exit while --> means ALL_FAIL at current z height or Z-Limit*/

    /* raise chuck by one small step and save FIRST_SOME_PASS point */
    if (retVal != PHPFUNC_ERR_Z_LIMIT)
    {
      PhPFuncTaCheckStop(retVal = moveChuck(proberID, UP, smallStep, &zCurrentHeight, zHeightMax, 0)); 
      
      /* consider that first_some_pass height is the same as all_pass height*/
      checkResult = preCheckResult;
      if (checkResult == ALL_PASS)
      {
        zHeightOfAllPass = zCurrentHeight;
      }

      /* 
       * record Z height as FIRST_SOME_PASS and set Z height == first_some_pass 
       * if required by user in configuration 
      */
      zHeightOfFirstSomePass = zCurrentHeight; 
      if (contactPointSelection == SELECTION_FIRST_SOME_PASS)
      {
        PhPFuncTaCheckStop(setContactPosition(proberID)); 
      }
    } /*end if(retVal != PHPFUNC_ERR_Z_LIMIT)*/

    /* raise chuck by small steps til ALL_PASS */
    while ((checkResult == SOME_PASS) && (retVal != PHPFUNC_ERR_Z_LIMIT))
    {
      /* raise chuck of small step  */
      PhPFuncTaCheckStop(retVal = moveChuck(proberID, UP, smallStep, &zCurrentHeight, zHeightMax, 0)); 

      /* perform continuity Test */
      #ifdef SIMULATE_DCCTEST
        checkResult = simuPhDCContinuityTest (proberID, contactestSetupId, zCurrentHeight);
      #else
        checkResult = phDCContinuityTest (proberID, contactestSetupId);
      #endif

      /* Check results - Zlimit error */
      if (retVal == PHPFUNC_ERR_Z_LIMIT)
      {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                         "Z height exceeded Z-Limit");
      }

      if (checkResult == ALL_PASS)
      {
        zHeightOfAllPass = zCurrentHeight;
        break;
      }
    }  /* end while - means checkResult == ALL_PASS or Z-Limit */

    if ((checkResult == ALL_PASS)&& ((zHeightOfAllPass - zHeightOfFirstSomePass) <= safetyWindow)) 
    {
        break;
    }    /* force exit loop for i < PH_MAX_NUM_COORD */

    /* otherwise next loop i++ */

  }  /* end for  i < PH_MAX_NUM_COORD */

  if (retVal == PHPFUNC_ERR_Z_LIMIT)
  {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_FATAL,
                     "z limit error, Contact Test Failed");
    return PHPFUNC_ERR_Z_LIMIT;
  }

  if ((zHeightOfAllPass - zHeightOfFirstSomePass) > safetyWindow)
  {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_FATAL,
                     "planarity window size: %d exceeds safetywindow size: %d, "
                     "Contact Test Failed", (zHeightOfAllPass - zHeightOfFirstSomePass), safetyWindow);
    return PHPFUNC_ERR_CONFIG;
  }
    
  /* Contact test finished successfully set prober at Z contact Height */
  if ((checkResult == ALL_PASS) && (contactPointSelection == SELECTION_ALL_PASS))
  {
    PhPFuncTaCheckStop(setContactPosition(proberID));
  }

  /* save contatct test results */
  if (contactPointSelection == SELECTION_FIRST_SOME_PASS)
  {
    sZContactHeight = zHeightOfFirstSomePass;
  }
  else if (contactPointSelection == SELECTION_ALL_PASS)
  {
    sZContactHeight = zHeightOfAllPass;
  }
  sPlanarityWindow = zHeightOfAllPass - zHeightOfFirstSomePass;
  phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
                   "Z contactHeight is: %d, planarityWindow is: %d", sZContactHeight,sPlanarityWindow); 

  return retVal;
}
#endif

#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
*
* Set Status to Prober
*
* Authors: Zoyi Yu
*
* Description:
*   set some commands.
*
* Prober specific implementation of the corresponding plugin function
* as defined in ph_pfunc.h
*
***************************************************************************/
phPFuncError_t privateSetStatus(
      phPFuncId_t proberID                            /* driver plugin ID */,
      char *commandString                             /* key name to get parameter/information */,
      char **responseString                           /* output of response string */

)
{
  phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                   "privateSetStatus not yet implemented");

  return PHPFUNC_ERR_NA;
}
#endif


#ifdef SETSTATUSFORAUTOZ_IMPLEMENTED
/*****************************************************************************
 *
 * Set parameter to the prober
 *
 * Authors: Zoyi Yu
 *
 * Description:
 *   set some commands for autoz
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phPFuncError_t privateSetStatusForAutoZ(
    phPFuncId_t proberID        /* driver plugin ID */,
    const char *commandString   /* the string of command */,
    const char *paramString     /* the parameter for command string*/,
    char **responseString       /* output of response string */

)
{
    phPFuncError_t retVal = PHPFUNC_ERR_OK;
    static char response[MAX_STATUS_MSG_LEN] = "";
    const char *token =  commandString;
    char para[MAX_STATUS_MSG_LEN]  =  "";
    strncpy(para,paramString,MAX_STATUS_MSG_LEN-1);
    static int x=0,y=0;
    static long orginalX =0;
    static long orginalY = 0;
    static int incremental = 0, zMaxHeight=0;
    static int dieSteps =  0,ZUPMaxSteps = 0;
    *responseString = response;
     phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                     "privateSetStatusForAutoZ, token = ->%s<-, param = ->%s<-", token, para);
     if(strcmp(commandString,"AUTOZ_indexDiePrimary") == 0)
     {
       // in first index,we need to move prober point
       retVal = moveChuck(proberID,BASELINE,0,&zCurrHeight,0,0);
       if(parseXYCoordinateFromPara(&x,&y,para) == -1)
         return PHPFUNC_ERR_ANSWER;
       char xyLocation[64] = "";
       phPFuncTaStart(proberID);
       retVal = getXYLocation(proberID,xyLocation, &orginalX, &orginalY);
        phPFuncTaStop(proberID);
       if(retVal != PHPFUNC_ERR_OK)
       {
         phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
             "Incorrect XY coordinate value received from the prober: %s", xyLocation);
         return retVal;
       }

       // fix CR-164756 index die timeout since temperature control of chuck
       // retry 60 times waiting 60x4s
       for(int iCount=0; iCount<60; iCount++) {
         retVal = indexDie(proberID,x,y);
         if (retVal != PHPFUNC_ERR_WAITING)
         {
           break;
         }
       }

       //in case customer do AUTO Z in each wafer, reset the total Z UP steps  and total Z UP steps.
       ZUPSteps = 0;
     }
     else if(strcmp(commandString,"AUTOZ_indexDie") == 0 )
     {
        if(parseXYCoordinateFromPara(&x,&y,para) == -1)
            return PHPFUNC_ERR_ANSWER;
        // when TEL prober move prober card to a position, it will keep position before drive, so need move chuck down firstly
        int srq = 0;
        phPFuncTaStart(proberID);
        PhPFuncTaCheck(phPFuncTaSend(proberID, "D%s", MY_EOC));
        PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x44));
        if (srq != 0x44)
        {
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                           "prober cannot reach home position");
          return PHPFUNC_ERR_ANSWER;
        }
        phPFuncTaStop(proberID);

        // fix CR-164756 index die timeout since temperature control of chuck
        // retry 60 times waiting 60x4s
        for(int iCount=0; iCount<60; iCount++) {
          retVal = indexDie(proberID,x,y);
          if (retVal != PHPFUNC_ERR_WAITING)
          {
            break;
          }
        }

        zCurrHeight = 0;
     }
     else if(strcmp(commandString,"AUTOZ_Z+") == 0 )
     {
       phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"receive AUTOZ_Z+ request");
       if(parseZParameter(&incremental,&zMaxHeight,para) == -1)
         return PHPFUNC_ERR_ANSWER;

       retVal = moveChuck(proberID,UP,incremental,&zCurrHeight,zMaxHeight,0);
       phPFuncTaStart(proberID);
       displayCurrentZheight(proberID);
       phPFuncTaStop(proberID);
     }
     else if(strcmp(commandString,"AUTOZ_Z-") == 0 )
     {
       phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"receive AUTOZ_Z- request");
       if(parseZParameter(&incremental,&zMaxHeight,para) == -1)
         return PHPFUNC_ERR_ANSWER;

       retVal = moveChuck(proberID,DOWN,incremental,&zCurrHeight,zMaxHeight,0);
       phPFuncTaStart(proberID);
       displayCurrentZheight(proberID);
       phPFuncTaStop(proberID);
     }
     else if(strcmp(commandString,"AUTOZ_lockZHeight") == 0 )
     {
        retVal = lockChuck(proberID);
        phPFuncTaStart(proberID);
        displayCurrentZheight(proberID);
        phPFuncTaStop(proberID);
     }
     else if(strcmp(commandString,"AUTOZ_moveOrginalPosition") == 0 )
     {
       if (!proberID->p.stIsMaster)
       {
         int srq = 0;
         PhPFuncTaCheck(phPFuncTaSend(proberID, "B%s", MY_EOC));
         PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 2, 0x46, 0x4b));
       }
       zCurrHeight = 0;
     }
     else if(strcmp(commandString,"AUTOZ_Pass_or_Fail") == 0 )
     {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"Auto-Z at %dx%d with Z-step %d:",x,y,ZUPSteps);
        int size = para[0] -48;
        for (int site=1; site<=size; site++)
          phLogFuncMessage(proberID->myLogger, LOG_DEBUG," site%d:%s",site,(para[site] == 50?"Pass":"Fail"));
     }



     if(retVal == PHPFUNC_ERR_OK)
     {
         strncpy(response,commandString,64);
         strcat(response,"  execute succeed!");
     }
     else
     {
        strncpy(response,commandString,64);
        strcat(response,"  execute failed!");
        char reason[128] = "";
        if(PHPFUNC_ERR_PARAM == retVal)
        {
            sprintf(reason," reason:%s outside of prober area",para);
            strcat(response,reason);
        }
        else if(PHPFUNC_ERR_Z_LIMIT == retVal)
        {
          sprintf(reason," reason: the current height will reach:%d, it over maximum z-height:%d",zCurrHeight+incremental,zMaxHeight);
          strcat(response,reason);
        }
        else if(PHPFUNC_ERR_ANSWER == retVal)
        {
          sprintf(reason," reason: prober cannot execute Z step command or reach home position");
          strcat(response,reason);
        }

        retVal = PHPFUNC_ERR_OK;
     }
     return retVal;


}
#endif


/*****************************************************************************
 * End of file
 *****************************************************************************/
