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
 *          : Jeff Morton, STE-ADC
 *          : Jiawei Lin, R&D Shanghai, CR27092 & CR25172
 *          : David Pei, R&D Shanghai, CR27997
 *          : Garry Xie,  R&D Shanghai, CR28427
 *          : Kun Xiao, R&D Shanghai, CR21589
 *          : Jiawei Lin, R&D Shanghai, CR36888
 *          : Jiawei Lin, R&D Shanghai, CR41379
 *          : Danglin Li, R&D Shanghai, CR42470
 *          : Danglin Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *          : 22 Feb 2000, Jeff Morton modifications
 *          : 19 Jun 2000, Michael Vogt, handle more SRQs (CIDB 12963, 12988)
 *          : 11 Jan 2001, Michael Vogt, CR 2895 (sequence back, forced lot end)
 *          : August 2005, Jiawei Lin, CR27092 & CR25172
 *              Implement the "privateGetStatus" function. The "getStatusXXX"
 *              series of functions are used to retrieve the information/parameter
 *              from Prober, like WCR, Lot Number, Chuck temperature, etc.
 *          : February 2006, David Pei, CR27997
 *              Ignore the waiting for STB100 after binning die, in function "privateBinDie"
 *              The driver should function correctly whether there is a STB 100 or not
 *          : February 2006, Garry Xie , CR28427
 *              modifiy function "privateGetStatus" to retrieve information from Prober,
 *              like MULTIPLI_LOCATION_NUMBER,ERROR_CODE,WAFER_STATUS
 *              Implement the "privateSetStatus"function which belong to the "ph_set_status"
 *              serial functions that are used to set parameters to Prober, like
 *              Alarm_Buzzer.
 *          : July 2006, Fabarca & Kun Xiao, CR21589
 *              Define the "privateContactTest". This function performs contact test to
 *              get z_contact_height automatically.
 *              modifiy function "privateGetStatus" to support retrieving z_contact_height
 *              and planarity_window from Prober.
 *          : Jan 2008, Jiawei Lin, CR36888
 *              Add to support the SRQ 0x42 (Chuck down).
 *              Background: In some case like the wafer cleanning at the begining of productiont test,
 *              the prober's chuck might still be at down position and replied with the SRQ 0x42. The driver
 *              must be able to handle this.
 *          : July 2008, Jiawei Lin, CR41379
 *              Enhance to support more site number. the original driver only supports up to 4 site in parallel.
 *              With this enhancement, the driver now supports 8 site, 16 site and even above 
 *          : Nov 2008, CR41221 & CR42599
 *               Implement the "privateExecGpibCmd". this function performs to send prober setup and action command by gpib.
 *               Implement the "privateExecGpibQuery". this function performs to send prober query command by gpib.
 *          : January 2009, Danglin Li, CR42470
 *              Add the SRQ 0x43 and 0x5b as lot start signal for sepeical case. Normally, driver wait for
 *              SRQ 0x46 to start lot. But below situation need 0x43 and 0x5b as lot start singal:
 *              Operator stop prober and abort test program when testing die,
 *              and make changes to test program, then restart prober from die level
 *              and start test program, the SRQ 0x43 or 0x5b will issue as start signal.
 *
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

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_pfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "ph_pfunc_private.h"
#include "transaction.h"

/*CR21589, Kun Xiao, 19 Jun 2006*/
#include  "ph_tools.h"
#include  "ph_GuiServer.h"
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

static int contactChcekEnabled = 0;
static int ZUPSteps = 0;
/*End CR21589*/

/*--- functions -------------------------------------------------------------*/

phPFuncError_t handleIgnoredSRQ(phPFuncId_t proberID, int srq)   /* SRQ = ??? */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
	"Service request is ignored (SRQ 0x%02x) due to set configuration", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleErrorSRQ(phPFuncId_t proberID, int srq)     /* SRQ = 76 = 0x4c */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
	"Error is reported at prober (SRQ 0x%02x). Need operator assistance.", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleWaferCountSRQ(phPFuncId_t proberID, int srq)  /* SRQ = 80 = 0x50 */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "Wafer Count is received (SRQ 0x%02x). Wafer is unloaded. ", srq);
    proberID->p.waferCountReceived = 1;
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleWarningErrorSRQ(phPFuncId_t proberID, int srq)   /* SRQ = 87 = 0x57 */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
	"Warning Error is received (SRQ 0x%02x). Need operator assistance.", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleCassetteLoadedSRQ(phPFuncId_t proberID, int srq)   /* SRQ = 112 = 0x70 */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "Cassette Load is received (SRQ 0x%02x). Ignored", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleGPIBInitialDoneSRQ(phPFuncId_t proberID, int srq)   /* SRQ = 64 = 0x40 */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "GPIB Initial Done is received (SRQ 0x%02x). Ignored", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleWaferUnloadingDoneSRQ(phPFuncId_t proberID, int srq)   /* SRQ = 71 = 0x47 */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "Wafer Unloading Done is received (SRQ 0x%02x). Ignored", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleProberInitialDoneSRQ(phPFuncId_t proberID, int srq)   /* SRQ = 75 = 0x4b */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "Prober Initial Done is received (SRQ 0x%02x). Ignored", srq);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleStopSRQ(phPFuncId_t proberID, int srq)   /* SRQ = 90 = 0x5a */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "Probing stop is detected (SRQ 0x%02x)", srq);
    phEstateASetPauseInfo(proberID->myEstate, 1);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}

phPFuncError_t handleStartSRQ(phPFuncId_t proberID, int srq)   /* SRQ = 91 = 0x5b */
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
        "Probing restart is detected (SRQ 0x%02x)", srq);
    phEstateASetPauseInfo(proberID->myEstate, 0);
    phPFuncTaRemoveStep(proberID);
    return PHPFUNC_ERR_WAITING;
}


static int coordDigitNumber = 3;
static int minCoord = -99;
static int maxCoord = 999;

/* flush any unexpected SRQs */
static phPFuncError_t flushSRQs(phPFuncId_t proberID)
{
    phPFuncError_t retVal = PHPFUNC_ERR_OK;
    int received = 0;
    int srq = 0;

    do
    {
	    PhPFuncTaCheck(phPFuncTaTestSRQ(proberID, &received, &srq));
	    phPFuncTaRemoveStep(proberID);
    } while (received);

    return retVal;
}


/* read the xy coordinates from the reply string of Q query*/
static phPFuncError_t readXY( phPFuncId_t proberID,
                              const char *coordString,
                              long *x,
                              long *y)
{
  phPFuncError_t retVal = PHPFUNC_ERR_OK;
  const char *xloc = NULL, *yloc = NULL;
  static int firstCall = 1;
  if(coordString == NULL || x == NULL || y == NULL)
  {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_FATAL,
        "Driver error. Invalid input parameters to readXY() function.");
    retVal = PHPFUNC_ERR_ANSWER;
  }
  else
  {
    /* find the pos of the xy coordinates */
    xloc = strstr(coordString,"X");
    yloc = strstr(coordString,"QY");
    if(xloc == NULL || yloc == NULL )
    {
      return PHPFUNC_ERR_ANSWER;
    }

    if(sscanf(xloc+1, "%ld", x) != 1 || sscanf(yloc+2, "%ld", y) != 1)
    {
      retVal = PHPFUNC_ERR_ANSWER;
    }
  }

  if(retVal == PHPFUNC_ERR_ANSWER)
  {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_FATAL,
        "Invalid XY coordinates returned from the prober %s", coordString);
  }
  else
  {
    /* we also leverage this function to get the xy digit number
     * configuration on the prober side */
    coordDigitNumber = strlen(xloc+1);
    if(coordDigitNumber == 3)
    {
      minCoord = -99;
      maxCoord = 511;
    }
    else if(coordDigitNumber == 4)
    {
      minCoord = -999;
      maxCoord = 9999;
    }
    else
    {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "Length of XY coordinate value is %d digits; expect 3 or 4",
                coordDigitNumber);
      retVal = PHPFUNC_ERR_ANSWER;
    }

    if(firstCall && (retVal == PHPFUNC_ERR_OK))
    {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_1,
          "Length of XY coordinate is %d digits; range of coordinate value is [%d, %d]",
          coordDigitNumber,
          minCoord,
          maxCoord);
      firstCall = 0;
    }
  }

  return retVal;
}


static int checkParameter(phPFuncId_t driverID, int isOK, const char *message)
{
    if (!isOK)
	phLogFuncMessage(driverID->myLogger, PHLOG_TYPE_ERROR, message);

    return isOK;
}

static phPFuncError_t raiseChuck(phPFuncId_t proberID)
{
    static int srq = 0;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    PhPFuncTaCheck(phPFuncTaSend(proberID, "Z%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x43));

    return retVal;
}

static phPFuncError_t machineID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **machineIdString     /* resulting pointer to equipment ID string */
)
{
    static char mID[256] = "";

    mID[0] = '\0';

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    *machineIdString = mID;

    PhPFuncTaCheck(phPFuncTaSend(proberID, "B%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", mID));

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
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
	    "Impossible to receive identification string from prober.\n"
	    "Can not validate the configured model type \"%s\" (ignored)",
	    proberID->cmodelName);
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
        retVal = phPFuncTaTestSRQ(proberID, &received,&srq);
        ++count;
        if(retVal == PHPFUNC_ERR_OK )
        {
          if(received)
          {
#if 0
            if(srq == 0x46)
            {
              phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,"receive SRQ(0x46) in auto z process .");
              strcpy(reason,"receive SRQ(0x46) before receive 0x69,please check contact check settting at prober side");
              //return false; as tsk prober in US can not send 0x69 , it can only send 0x46, therefor, we use 0x46 as AUTOZ start SRQ, it need to change back when officaly release
              return false;
            }
#endif
            if(srq == 0x69)
              return true;
            else
            {
              phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,"received  SRQ(0x%02x) while waiting for Contact check start signal,ignored",srq);
              phPFuncTaRemoveStep(proberID);

            }
          }
        }
        else
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,"got a unexcepted GPIB error,ignore it ");
        if(count>=200)
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
        strcpy(reason,"Never received Contact check start SRQ(0x69),opeartor have cancel to wait for SRQ,please check if enable contact check feature at prober side");
    return false;
        
}
static phPFuncError_t waitLotStarted(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    int waiting = 1;
    static int srq1 = 0;
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
        long response = -1;
        /* wait for ANY SRQ by defining 0 specific SRQs to wait for :-) */
        PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq1, 0));
        /* SRQ was received, check for lot start SRQ */
        /* Normally, driver wait for SRQ 0x46 to start lot. But the situation below
           need SRQ 0x43 or 0x5b as lot start singal:
           Operator stop prober and abort test program when test device,
           and make changes to test program, then restart prober from die level
           and start test program, the SRQ 0x43 or 0x5b will issue as start signal.
           Pease refer to CR42470 for detail. */
        if (srq1 == 0x43 || srq1 == 0x5b)
        {
            if (proberID->p.unexpectedSRQResp == 0)
            {
                char strMsg[128] = {0};
                snprintf(strMsg, 127, "Receive SRQ 0x%02x while waiting for lot start, Do you want to abort or continue?", srq1);
                phGuiMsgDisplayMsgGetResponse(proberID->myLogger, 0,
                  "Info", strMsg, "abort", "continue", NULL, NULL, NULL, NULL, &response);
            }
            else
            {
                response = proberID->p.unexpectedSRQResp;
            }
        }

        /* If receive SQR 0x46, lot start; if receive SRQ (0x43 or 0x5b) and continue, lot start. */
        if ( (srq1 == 0x46) || ((srq1 == 0x43 || srq1 == 0x5b) && response == 3) )
        {
            waiting = 0;
        }
        /* If receive SRQ (0x43 or 0x5b) and abort, test program abort. */
        else if ( (srq1 == 0x43 || srq1 == 0x5b) && response == 2 )
        {
            return PHPFUNC_ERR_ABORTED;
        }
        else
        {
            if(0x69 == srq1)
            {
                contactChcekEnabled = 1;
                phLogFuncMessage(proberID->myLogger, LOG_NORMAL,"receive auto z start SRQ(0x69) in waitLotStarted function, still waiting lot start SRQ");
            }
            else
                phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
              "SRQ flushed (SRQ 0x%02x), ignored", srq1);
            waiting = 1;
            phPFuncTaRemoveStep(proberID);
          
        }
    } while (waiting);

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
 *  function to parse it. For example, for "wafer_size' request, the response
 *  is:
 *     "urN", where N=0,1,2,3 respectively means "mm","inch","um","mil"
 *  
 *  After parsing, the result will be "mm", "inch" or other
 *
 *  For some query, like Wafer_number, cassette status, we also include
 *  the original (raw) data from Prober. For example, if the answer is 
 *  "x012420" for query cass_status, then the output will be:
 *     "<012420>slot number of Wafer on chuck:1; count of Wafers remaining:24;
 *              status of Cassette 1: Testing under way; 
 *              status of Cassette 2: NO cassette"
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
static int analysisResultOfGetStatus(const char * commandString, const char *encodedResult, char *pResult)
{
  static const char *waferUnits[] = {"mm", "inch", "um", "mil"};
  static const char *posX[] = {"Right", "Left"};
  static const char *posY[] = {"Up", "Down"};
  static const char *errType[] = {"System error", "Error state", "Operation call", "Warning condition", "Information"};
  static const char *cassStatus[] = {"NO cassette", "Ready to test", "Testing under way",
                                     "Testing finished", "Rejected wafer cassette"};
  /* static char *waferStatus[] = {"NO wafer", "Testing not done", "Testing finished", "Testing under way"}; */
  
  int retVal = SUCCEED;
  const char *p = encodedResult;

  if(p == NULL) {
    return FAIL;
  }
  if( strstr(p, "ur") != NULL ) {   /* the query is about Data ID */
    p++;p++;  /* step the beginning part "ur" */
    if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_UNITS) == 0 ) {
      /* the response from Prober is "urN", where N=0,1,2,3 */
      if( (p!=NULL) && (('0'<=(*p)) && ((*p)<='3')) ) {
        strcpy(pResult, waferUnits[(*p)-'0']);
      } else {
        retVal = FAIL;
      }
    } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_POS_X) == 0 ) {
      /* the response from Prober is "urN", where N=0,1 */
      if( (p!=NULL) && (('0'<=(*p)) && ((*p)<='1')) ) {
        strcpy(pResult, posX[(*p)-'0']);
      } else {
        retVal = FAIL;
      }
    } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_POS_Y) == 0 ) {
      if( (p!=NULL) && (('0'<=(*p)) && ((*p)<='1')) ) {
        strcpy(pResult, posY[(*p)-'0']);
      } else {
        retVal = FAIL;
      }
    } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_FLAT) == 0 ) {
      /* the response from Prober is like "ur180deg" */
      int deg = 0;
      if( (p!=NULL) && (sscanf(p,"%ddeg",&deg)==1) ) {
        sprintf(pResult, "%d", deg);
      } else {
        retVal = FAIL;
      } 
    } else {
      strcpy(pResult, p);
    } 
  } else {              /* the query for other commands, we may also include the raw data*/
    if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_NUMBER) == 0 ) {
      int slotNumber = 0;
      int cassNumber = 0;
      if( (p!=NULL) && (sscanf(p,"X%2d%1d", &slotNumber, &cassNumber)==2) ){
        sprintf(pResult, "%s%02d%1d%s", "<", slotNumber, cassNumber, ">");
        sprintf(pResult, "%s%s:%d; %s:%d", pResult, 
                "slot number", slotNumber, "cassette number", cassNumber);
      } else {
        retVal = FAIL;
      }
    } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CHUCK_TEMP) == 0 ) {
      int settingTemp = 0;
      int currentTemp = 0;
      /*Begin CR92296, Adam Huang, 24 Nov 2014*/
      /*if( (p!=NULL) && (sscanf(p, "f%4d%4d", &settingTemp, &currentTemp) == 2) ){*/
        /*sprintf(pResult, "%s%04d%04d%s", "<", settingTemp, currentTemp, ">");*/
        /*sprintf(pResult, "%s%s:%.1f; %s:%.1f", pResult, "setting temperature",*/
                /*settingTemp*0.1, "current temperature", currentTemp*0.1);*/
      /*} else {*/
        /*retVal = FAIL;*/
      /*}*/
      if( (p!=NULL) && (sscanf(p, "f%4d%4d", &currentTemp, &settingTemp) == 2) )
      {
        sprintf(pResult, "%s%04d%04d%s", "<", currentTemp, settingTemp, ">");
        sprintf(pResult, "%s%s:%.1f; %s:%.1f", pResult, "current temperature",
                currentTemp*0.1, "setting temperature", settingTemp*0.1);
      }
      else if ('f' == *p)
      {
        sprintf(pResult, "%s", "<>current temperature:<>; setting temperature:<>");
      }
      else
      {
          retVal = FAIL;
      }
      /*End CR92296*/
    } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CASS_STATUS) == 0 ) {
      int slotNum = 0;
      int numOfNotFinished = 0;
      int idCass1 = 0;
      int idCass2 = 0;
      if( (p!=NULL) && (sscanf(p,"x%2d%2d%1d%1d",&slotNum,&numOfNotFinished,&idCass1,&idCass2)==4) ){
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
    } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_MULTISITE_LOCATION_NUMBER) == 0 ) {
      /* the response from Prober is like "H02", "H04", "HA0" */
      /* fix CR-153832 */
      char mulLocNum[4] = "";
      if( (p!=NULL) && phToolsMatch(p,"H[0-9a-zA-Z]{2}") ) {
        sscanf(p, "H%2s", mulLocNum);
        sprintf(pResult, "%s", mulLocNum);
      } else {
        retVal = FAIL;
      }
    } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_ERROR_CODE) == 0 ) {
      /* response from Prober is like "EE0001", "ES0002" but when normal state , response is "E" */
      char errRec[] = "SEOWI";
      if ( (p!=NULL) && (phToolsMatch(p,"E")) ) {
        sprintf(pResult, "%s%s", "<", ">");
        sprintf(pResult, "%s%s: %s; %s: %s", pResult, "error type","NA", "error code", "NA");
      }else if( (p!=NULL) && phToolsMatch(p,"E[SEOWI][0-9]{4}") ){
        p++;                                                             /* step the beginnig part "E" */
        sprintf(pResult, "%s%s%s", "<", p, ">");
        int pos = strchr(errRec, *p) - errRec;                           /* get the  position in errType[] */ 
        sprintf(pResult, "%s%s: %s; %s: %s", pResult, "error type", errType[pos], "error code", p+1);
      } else {
        retVal = FAIL;
      }
    } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_STATUS) == 0 ) {
      /*
       * the response from Prober is like :"w12......22222222222.00.......00000000000"
       *                                      <-- totally 25 -->  <-- totoally 25 -->
       *
       * the first "1" means the status of Cassette 1, the first "0" means the status of Cassette 2
       * for each Cassette, they are totoally 25 characters, each character represents one Wafer's status
       */
      if( (p!=NULL) && (phToolsMatch(p,"w[0-4][0-3]{25}\\.[0-4][0-3]{25}")) ){ 
        const char *statusRec = "01234";
        char *token = NULL;        
        char sTemp[128] = "";

        p++;
        strncpy(sTemp, p, sizeof(sTemp) - 1);
        sprintf(pResult, "<%s>", sTemp);
                
        token = strtok(sTemp, ".");
        if(token != NULL)
        {
          int pos = strchr(statusRec, *token) - statusRec;
          sprintf(pResult, "%s%s: %s%s: %s", pResult, "Cassette 1", cassStatus[pos], ", wafer status", token+1);
          token = strtok(NULL, ".");
          if(token != NULL)
          {
            pos = strchr(statusRec, *token) - statusRec;
            sprintf(pResult, "%s%s: %s%s: %s", pResult, "; Cassette 2", cassStatus[pos], ", wafer status", token+1);
          } else {
            retVal = FAIL;
          }
        } else {
          retVal = FAIL;
        } 
      } else {
        retVal = FAIL;
      } 
    }

    /*Begin CR91531, Adam Huang, 24 Nov 2014*/
    else if(strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CARD_CONTACT_COUNT) == 0)
    {
        long lcardContactCount = 0;
        if((p!=NULL) && (sscanf(p, "kc%ld", &lcardContactCount) == 1))
        {
            sprintf(pResult, "%s:%ld", "card contact count", lcardContactCount);
        }
        else
        {
            retVal = FAIL;
        }
    }
    else if(strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_GROSS_VALUE_REQUEST) == 0)
    {
        int igrossValue = 0;
        if(p!=NULL)
        {
            if(sscanf(p, "y%4d", &igrossValue) == 1)
            {
                sprintf(pResult, "%s:%.1f%%", "gross value", igrossValue*0.1);
            }
            else
            {
                sprintf(pResult, "%s", "NET is specified instead of yield");
            }
        }
    }
    else if(strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_DEVICE_PARAMETER) == 0)
    {
        char szbuf[MAX_STATUS_MSG_LEN] = {0};
        if(p!=NULL && (strlen(p) == 199))
        {
            p++;p++;
            //12 byte wafer name
            char szwaferName[13] = {0};
            strncpy(szwaferName, p, 12);
            sprintf(szbuf, "<%s:%s>", "wafer name", szwaferName);
            p+=12;
            //1 byte wafer size
            char chwaferSize = *p;
            if ('C' == chwaferSize)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "wafer size", "12 inch");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "wafer size", "8 inch");
            }
            p+=1;
            //3 byte ORI.FLA.Angle
            char szoriFlatAngle[4] = {0};
            strncpy(szoriFlatAngle, p, 3);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "orientation flat angle", szoriFlatAngle);
            p+=3;
            //10 byte X Axis & Y Axis
            char szxAxis[6] = {0};
            char szyAxis[6] = {0};
            strncpy(szxAxis, p, 5);
            p+=5;
            strncpy(szyAxis, p, 5);
            p+=5;
            sprintf(szbuf, "%s<%s:%s>", szbuf, "x axis", szxAxis);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "y axis", szyAxis);
            //22 byte not used
            p+=22;
            //4 byte prober size
            char szproberSize[5] = {0};
            strncpy(szproberSize, p, 4);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "prober size", szproberSize);
            p+=4;
            //1 byte target sense
            char chtargetSense = *p;
            if ('1' == chtargetSense)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "target sense", "is applied");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "wafer size", "is not applied");
            }
            p+=1;
            //12 byte not used
            p+=12;
            //6 byte standard chip x & y
            char szxPosition[4] = {0};
            char szyPosition[4] = {0};
            strncpy(szxPosition, p, 3);
            p+=3;
            strncpy(szyPosition, p, 3);
            p+=3;
            sprintf(szbuf, "%s<%s:%s>", szbuf, "target position x", szxPosition);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "target position y", szyPosition);
            //10 byte not used
            p+=10;
            //1 byte prober area select
            char chproberAreaSelect = *p;
            if ('Y' == chproberAreaSelect)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "prober area select", "5 point data in");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "prober area select", "wafer margin");
            }
            p+=1;
            //3 byte edge correct
            char szedgeCorrect[4] = {0};
            strncpy(szedgeCorrect, p, 3);
            sprintf(szbuf, "%s<%s:%s%%>", szbuf, "edge correct", szedgeCorrect);
            p+=3;
            //1 byte sample prober
            char chsamplePorber = *p;
            if ('1' == chsamplePorber)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "sampling test", "is applied");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "sampling test", "is not applied");
            }
            p+=1;
            //48 byte sample step 1-8 x & y
            char szxSampleStep[4] = {0};
            char szySampleStep[4] = {0};
            int itemp = 1;
            for(itemp=1; itemp<=8; itemp++)
            {
                strncpy(szxSampleStep, p, 3);
                p+=3;
                strncpy(szySampleStep, p, 3);
                p+=3;
                sprintf(szbuf, "%s<%s%d:%s>", szbuf, "sampling test die x", itemp, szxSampleStep);
                sprintf(szbuf, "%s<%s%d:%s>", szbuf, "sampling test die y", itemp, szySampleStep);
            }
            //5 byte pass net
            char szpassNet[6] = {0};
            strncpy(szpassNet, p, 5);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "pass die count limit at the yield check", szpassNet);
            p+=5;
            //3 byte not used
            p+=3;
            //1 byte align axis
            char chalianAxis = *p;
            if ('X' == chalianAxis)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "align axis is", "X");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "align axis is", "Y");
            }
            p+=1;
            //1 byte auto focus
            char chautoFocus = *p;
            if ('1' == chautoFocus)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "auto focus", "is applied");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "auto focus", "is not applied");
            }
            p+=1;
            //2 byte x & y align size
            char chxAlignSize = *p;
            sprintf(szbuf, "%s<%s:%c>", szbuf, "x alian size is", chxAlignSize);
            p++;
            char chyAlignSize = *p;
            sprintf(szbuf, "%s<%s:%c>", szbuf, "y alian size is", chyAlignSize);
            p++;
            //2 byte x & y monitor chip
            char chxMonitorChip = *p;
            sprintf(szbuf, "%s<%s:%c>", szbuf, "monitor chip x is", chxMonitorChip);
            p++;
            char chyMonitorChip = *p;
            sprintf(szbuf, "%s<%s:%c>", szbuf, "monitor chip x is", chyMonitorChip);
            p++;
            //8 byte not used
            p+=8;
            //1 byte multi chip
            char chmultiChip = *p;
            sprintf(szbuf, "%s<%s:%c>", szbuf, "multi chip is", chmultiChip);
            p++;
            //2 byte multi location
            char szmultiLocation[3] = {0};
            strncpy(szmultiLocation, p, 2);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "multi location is", szmultiLocation);
            p+=2;
            //1 byte unload stop
            char chunloadStop = *p;
            if ('Y' == chunloadStop)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "unload stop", "is done");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "unload stop", "is not done");
            }
            p+=1;
            //2 byte stop counter
            char szstopCounter[3] = {0};
            strncpy(szstopCounter, p, 2);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "stop counter is", szstopCounter);
            p+=2;
            //1 byte test reject cassette
            char chtestRejectCassette= *p;
            if ('0' == chtestRejectCassette)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "test reject cassette", "LOAD CST");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "test reject cassette", "REJECT CST");
            }
            p+=1;
            //1 byte align reject cassette
            char chalignRejectCassette= *p;
            if ('0' == chalignRejectCassette)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "align reject cassette", "LOAD CST");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "align reject cassette", "REJECT CST");
            }
            p+=1;
            //1 byte continuous fail process
            char chcontinuousFailProcess= *p;
            sprintf(szbuf, "%s<%s:%c>", szbuf, "continuous FAIL post-process", chcontinuousFailProcess);
            p++;
            //1 byte continuous process
            char chcontinuousProcess= *p;
            if ('0' == chcontinuousProcess)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "continuous process", "STOP");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "continuous process", "wafer unloading and next wafer loading");
            }
            p+=1;
            //3 byte continuous preset
            char szcontinuousPreset[4] = {0};
            strncpy(szcontinuousPreset, p, 3);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "continuous preset is", szcontinuousPreset);
            p+=3;
            //2 byte skip chip row
            char szskipChipRow[3] = {0};
            strncpy(szskipChipRow, p, 2);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "skip chip row is", szskipChipRow);
            p+=2;
            //1 byte check back count
            char chcheckBackCount= *p;
            sprintf(szbuf, "%s<%s:%c>", szbuf, "check back count", chcheckBackCount);
            p++;
            //1 byte reject wafer count
            char chrejectWaferCount = *p;
            sprintf(szbuf, "%s<%s:%c>", szbuf, "reject wafer count", chrejectWaferCount);
            p++;
            //1 byte polish after check back
            char chpolishAfter = *p;
            if ('Y' == chpolishAfter)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "needle cleaning with the continuous FAIL check back is", "applied");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "needle cleaning with the continuous FAIL check back is", "not applied");
            }
            p+=1;
            //1 byte polish after check back
            char chpolish = *p;
            if ('Y' == chpolish)
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "needle cleaning is", "applied");
            }
            else
            {
                sprintf(szbuf, "%s<%s:%s>", szbuf, "needle cleaning is", "not applied");
            }
            p+=1;
            //2 byte z count
            char szzCount[3] = {0};
            strncpy(szzCount, p, 2);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "Z count is", szzCount);
            p+=2;
            //4 byte chip count
            char szchipCount[5] = {0};
            strncpy(szchipCount, p, 4);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "chip count is", szchipCount);
            p+=4;
            //2 byte wafer count
            char szwaferCount[3] = {0};
            strncpy(szwaferCount, p, 2);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "wafer count is", szwaferCount);
            p+=2;
            //3 byte over drive
            char szoverDrive[4] = {0};
            strncpy(szoverDrive, p, 3);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "over drive is", szoverDrive);
            p+=3;
            //4 byte total count
            char sztotalCount[5] = {0};
            strncpy(sztotalCount, p, 4);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "total count is", sztotalCount);
            p+=4;
            //3 byte hot chuck
            char szhotChuck[4] = {0};
            strncpy(szhotChuck, p, 3);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "hot chuck is", szhotChuck);
            p+=3;
            //4 byte preset address x & y
            char szpresetAddressX[3] = {0};
            strncpy(szpresetAddressX, p, 2);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "preset address x is", szpresetAddressX);
            p+=2;
            char szpresetAddressY[3] = {0};
            strncpy(szpresetAddressY, p, 2);
            sprintf(szbuf, "%s<%s:%s>", szbuf, "preset address y is", szpresetAddressY);
            strncpy(pResult, szbuf, MAX_STATUS_MSG_LEN-1);
        }
        else
        {
            retVal = FAIL;
        }
    }
    /*End CR91531*/

    else {
      p++;    /* step the command itself in the case of single-character command */
      strcpy(pResult, p);
    }
  }

  return retVal;
}

/*****************************************************************************
*
* analysis the request from the Prober for "privateSetStatus"
*
* Authors: Garryxie
*
* Description:
*   after set the Prober,the Prober will return a long value
*   this function analysis the return value,eg:
*   when set Alarm buzzer,the return value from the Prober:
*     (1) for command "alarm_buzzer" 0x65--sucess, 0x4c--fail
*     (2) for command "double_z_up enable/disable" 0x59--sucess, 0x4e--fail
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
static int analysisResultOfSetStatus(const char * commandString, const int encodedResult, char *pResult)
{
  int retVal = SUCCEED;
  const int setResult = encodedResult;

  if( setResult != 0 ){
    if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_ALARM_BUZZER) == 0 ){
      /* set alarm buzzer ,return 0x65--sucess or 0x4c--fail */
      if ( setResult == 0x65 ){
        sprintf(pResult, "%s %s", PHKEY_NAME_PROBER_STATUS_ALARM_BUZZER, "successfully");
      }else if( setResult == 0x4c){
        sprintf(pResult, "%s %s", PHKEY_NAME_PROBER_STATUS_ALARM_BUZZER, "failed");
      }else {
        retVal = FAIL;
      }      
    } else {
    retVal = FAIL;
    }
  }else {
    retVal = FAIL;
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
    proberID->p.sendTimeout = proberID->heartbeatTimeout;
    proberID->p.receiveTimeout = proberID->heartbeatTimeout;
    proberID->p.chuckUnderFirstDie = 0;
    proberID->p.b_is_waferid = 0;
    proberID->p.max_bins = 64;
    proberID->p.unexpectedSRQResp = 0;
    proberID->p.waferLoadedImmediatelyAfterBinning = 0;
    proberID->p.waferCountReceived = 0;

    /* perform private reconfiguration first, if this is not a retry call */
    if (!isConfigured)
    {
        phPFuncTaAddSRQHandler(proberID, 0x4c, handleErrorSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x50, handleWaferCountSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x57, handleWarningErrorSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x70, handleCassetteLoadedSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x5a, handleStopSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x5b, handleStartSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x40, handleGPIBInitialDoneSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x47, handleWaferUnloadingDoneSRQ);
        phPFuncTaAddSRQHandler(proberID, 0x4b, handleProberInitialDoneSRQ);

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

    /* take over the heartbeat timeout for the mhcom layer */
    proberID->p.sendTimeout = proberID->heartbeatTimeout;
    proberID->p.receiveTimeout = proberID->heartbeatTimeout;

    /* check which command should be used for wafer ID or needle cleaning */
    confError = phConfConfStrTest(&found, proberID->myConf,
	PHKEY_PL_TSKWBCMD, "W", "b", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
	retVal = PHPFUNC_ERR_CONFIG;
    else
	switch (found)
	{
	  case 0:
	    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
		"configuration \"%s\" not given, assuming \"W\"",
		PHKEY_PL_TSKWBCMD);
	    /* fall into case 1 */
	  case 1:
	    proberID->p.b_is_waferid = 0;
	    break;
	  case 2:
	    proberID->p.b_is_waferid = 1;
	    break;
	}

    /* check if first character of Wafer ID should be removed to
       compensate for a firmware bug on some TSK probers. */
    confError = phConfConfStrTest(&found, proberID->myConf, 
    PHKEY_PL_TSKWAFIDCHAR, "no", "yes", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
    retVal = PHPFUNC_ERR_CONFIG;
    else
    switch (found)
    {
    case 0:
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
        "configuration \"%s\" not given, assuming \"no\"",
        PHKEY_PL_TSKWAFIDCHAR);
        /* fall into case 1 */
    case 1:
        proberID->p.waferid_rm_first_char = 0;
        break;
    case 2:
        proberID->p.waferid_rm_first_char = 1;
        break;
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

    /* Get max number of bins, either 64 or 256 */
    confError = phConfConfStrTest(&found, proberID->myConf, PHKEY_GB_MAX_BINS, "64", "256", "10000", "1000000", NULL);
    if ( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
    {
        /* out of range or bad format */
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "configuration value of \"%s\" must be either 64, 256, 10000 or 1000000\n",
                         PHKEY_GB_MAX_BINS);
        retVal = PHPFUNC_ERR_CONFIG;
    }
    else
    {
        switch ( found )
        {
            case 1:  // 64 bins
                proberID->p.max_bins = 64;
                break;
            case 2:  // 256 bins
                proberID->p.max_bins = 256;
                break;
            case 3:  // 9999 bins
                proberID->p.max_bins = 10000;
                break;
            case 4: //999999 bins
                proberID->p.max_bins = 1000000;
                break;
            default:  // 64 bins
                proberID->p.max_bins = 64;
                break;

        }
    }


    /* how to do when received unexpected SRQ during waiting for lot start */
    confError = phConfConfStrTest(&found, proberID->myConf,
                                  PHKEY_OP_UNEXPSRQC, "dialog", "abort", "continue", NULL);
    if(confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
    {
        retVal = PHPFUNC_ERR_CONFIG;
    }
    else
    {
        switch ( found )
        {
            case 0:
            case 1:
                proberID->p.unexpectedSRQResp = 0;
                break;
            case 2:
                proberID->p.unexpectedSRQResp = 2;
                break;
            case 3:
                proberID->p.unexpectedSRQResp = 3;
                break;
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

    char *mID;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
	return PHPFUNC_ERR_ABORTED;

    phPFuncTaStart(proberID);
    PhPFuncTaCheck(machineID(proberID, &mID));
    phPFuncTaStop(proberID);

    sprintf(equipId, "reported: %s configured: %s",
        mID, proberID->cmodelName);
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



char* displayCurrentZheight(phPFuncId_t proberID)
{
  
  static char handlerAnswer[1024] = "";
  memset(handlerAnswer,'\0',sizeof(handlerAnswer));
  phPFuncError_t ret = PHPFUNC_ERR_OK;
  ret = phPFuncTaSend(proberID, "i24%s", MY_EOC);
  if(ret != PHPFUNC_ERR_OK)
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING," send command \"i24\"failed ");
  else
  {
    ret = phPFuncTaReceive(proberID, 1, "%s", handlerAnswer);
    if(ret != PHPFUNC_ERR_OK)
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,"receive response of \"i24\" failed ");
    else
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,"current z height is:%s",handlerAnswer);
  }
  return handlerAnswer;
}
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
    static long xc = 0;
    static long yc = 0;
    static int srq = 0;
    static int srq2 = 0;
    static long firstX = 0;
    static long firstY = 0;
    static int firstIsDefined = 0;
    static char xyCoordinates[100];
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static int received = 0;*/
/* End of Huatek Modification */

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
                "wafer source \"%s\" is empty, wafer not loaded", "input cassette");
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
            if(coordDigitNumber == 3)
            {
              PhPFuncTaCheck(phPFuncTaSend(proberID,
                  "JY%03ldX%03ld%s", firstY, firstX, MY_EOC));
            }
            else
            {
              PhPFuncTaCheck(phPFuncTaSend(proberID,
                  "JY%04ldX%04ld%s", firstY, firstX, MY_EOC));
            }
            PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 3, 0x42, 0x43, 0x64));
	    switch (srq)
	    {
	      case 0x42:       /* 66 = auto indexing complete Z-down */
		break;
	      case 0x43:       /* 67 = auto indexing complete Z-up */
		break;
	      case 0x64:       /* 100 - test complete */
		break;
	    }
	    if (srq == 0x64)
	    {
		PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq2, 2, 0x42, 0x43));
		switch (srq2)
		{
		  case 0x42:       /* 66 = auto indexing complete Z-down */
		    break;
		  case 0x43:       /* 67 = auto indexing complete Z-up */
		    break;
		}
	    }
        }
    }
    else
    {
	/* regular operation, load wafer was already done by last unload */
    }
    
    /* common operations: move to reference position, 
       ask for this position and rise the chuck */
    PhPFuncTaCheck(raiseChuck(proberID));
    //displayCurrentZheight(proberID);
    PhPFuncTaCheck(phPFuncTaSend(proberID, "Q%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", xyCoordinates));

    retVal = readXY(proberID, xyCoordinates, &xc, &yc);
    if (retVal != PHPFUNC_ERR_OK) {
        return retVal;
    }

    phPFuncTaStop(proberID);
    
    proberID->p.chuckUnderFirstDie = 1;

    phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
	"wafer loaded and ready to test\n"
	"current die position:   [ %ld, %ld ]",
	xc, yc);
    
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

    return retVal;
}
#endif

/****************************************************************************/

static void setWaferUnloaded(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t destination   /* where to unload the wafer */
)
{
    phEstateWafUsage_t usage;
    phEstateWafType_t type;

    /* update equipment specific state, move wafer from chuck to
       destination */
    phEstateAGetWafInfo(proberID->myEstate, 
	PHESTATE_WAFL_CHUCK, &usage, &type, NULL);
    phEstateASetWafInfo(proberID->myEstate, 
	PHESTATE_WAFL_CHUCK, PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
    phEstateASetWafInfo(proberID->myEstate, 
	destination, PHESTATE_WAFU_LOADED, type);
}

/****************************************************************************/

static void setLotEnd(
    phPFuncId_t proberID                /* driver plugin ID */
)
{
    phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_NOTLOADED);
    phEstateASetLotInfo(proberID->myEstate, 0);
}


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
 *           L   ----->         
 *               <-----   0x46, 0x48
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
    static int srq2 = 0;
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
    else if (proberID->p.waferLoadedImmediatelyAfterBinning == 1)
    {
      /* already recieved 0x46 after the M command, do nothing */
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_4,
                       "Wafer is already loaded after last binning. Will not send the \"L\" command.");
   
      /* reset the flag */
      proberID->p.waferLoadedImmediatelyAfterBinning = 0;
    }
    else
    {
        if (proberID->p.stIsMaster)  /* Smartest is Master */
        {      
	    phPFuncTaStart(proberID);
	    PhPFuncTaCheck(phPFuncTaSend(proberID, "L%s", MY_EOC));
	    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 3, 0x51, 0x42, 0x43));
	    switch (srq)
	    {
	      case 0x51: /* 81 = wafer end */
		break;
	      case 0x42:
	      case 0x43:    /* possible pending index complete being
                               here, if a wafer unload is enforced */
		phPFuncTaRemoveStep(proberID);
		return PHPFUNC_ERR_WAITING;		
	    }

	    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq2, 3, 0x46, 0x52, 0x5e));
	    switch (srq2)
	    {
	      case 0x46:      /* 70 = wafer unload/load OK */
		break;
	      case 0x52:      /* 82 = cassette end signal from prober */
		retVal = PHPFUNC_ERR_PAT_DONE;
		break;
	      case 0x5e:      /* 94 = lot done signal from prober */
		retVal = PHPFUNC_ERR_PAT_DONE;
		break;
	    }

	    phPFuncTaStop(proberID);
	}
	else   /* prober is master */
	{
	    phPFuncTaStart(proberID);
	    PhPFuncTaCheck(phPFuncTaSend(proberID, "L%s", MY_EOC));
	    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 6, 0x46, 0x52, 0x5e, 0x51, 0x42, 0x43));
	    switch (srq)
	    {
	      case 0x46:      /* 70 = wafer unload/load OK */
		break;
	      case 0x52:      /* 82 = cassette end signal from prober */
		retVal = PHPFUNC_ERR_PAT_DONE;
		break;
	      case 0x5e:      /* 94 = lot done signal from prober */
		retVal = PHPFUNC_ERR_PAT_DONE;
		break;
	      case 0x42:
	      case 0x43:    /* possible pending index complete being
                               here, if a wafer unload is enforced */
		phPFuncTaRemoveStep(proberID);
		return PHPFUNC_ERR_WAITING;		
	    }

	    phPFuncTaStop(proberID);
	}
    }

    setWaferUnloaded(proberID, destination);

    /* remove input cassette and empty output cassette */
    if (retVal == PHPFUNC_ERR_PAT_DONE)
    {
	setLotEnd(proberID);
    }

    return retVal;
}
#endif

static phPFuncError_t getXYLocation( phPFuncId_t proberID, char* XYLocation,long* x,long* y )
{
  PhPFuncTaCheck(phPFuncTaSend(proberID, "Q%s", MY_EOC));
  PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s",XYLocation ));
  return readXY(proberID, XYLocation, x, y);

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
        phConfError_t confError = phConfConfIfDef(proberID->myConf, PHKEY_SI_STHSMAP);
        if (confError == PHCONF_ERR_OK)
        {
          confError = phConfConfNumber(proberID->myConf, PHKEY_SI_STHSMAP, 1, &site, &stSite);
          if(confError == PHCONF_ERR_OK){
            char result[100] = "";
            itoS(stSite, result);
            //phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,"handler site:%s(sm site: %d),result:%s",proberID->siteIds[site],(int)stSite,result);
            strcat(response, result);
          }
        }
      }
    }
  }
  return retVal;
}

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
 *      Q        ----->         
 *               <-----   QYyyyXyyy
 *
 *               <-----   SRQ/0x43      auto index to next die
 *
 *
 * step mode (prober is slave)
 *
 *    JYyyyXxxx  ----->   
 *               <-----   0x43          Z up is implicit
 *
 *
 * if multisite probing only:
 *
 *      O        ----->         
 *               <-----   O[@ABD]*      site info 1 or 0
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
    static char xyCoordinates[100] = "";
    static int sitesGot[PHESTATE_MAX_SITES];
    static char sitesString[PHESTATE_MAX_SITES];
    static int srq = 0;
    static int srq2 = 0;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    int site;

    if (phPFuncTaAskAbort(proberID))
	return PHPFUNC_ERR_ABORTED;

    phPFuncTaStart(proberID);

    if (proberID->p.stIsMaster)  /* Smartest is Master */
    {
	if (!checkParameter(proberID,
	    *dieX >= minCoord && *dieX <= maxCoord && *dieY >= minCoord && *dieY <= maxCoord,
	    "requested die position is out of valid range")
	    )
	    return PHPFUNC_ERR_PARAM;

        if(coordDigitNumber == 3)
        {
          PhPFuncTaCheck(phPFuncTaSend(proberID,"JY%03ldX%03ld%s",*dieY,*dieX,MY_EOC));
        }
        else
        {
          PhPFuncTaCheck(phPFuncTaSend(proberID,"JY%04ldX%04ld%s",*dieY,*dieX,MY_EOC));
        }
	PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 5, 0x42, 0x43, 0x4a, 0x51, 0x64));
	switch (srq)
	{
	  case 0x42:       /* 66 = auto indexing complete Z-down */
	       break;
	  case 0x43:       /* 67 = auto indexing complete Z-up */
	       break;
	  case 0x4a:       /* 74 = outside of probing area */
	       break;
	  case 0x51:
	       /* position outside of wafer, keep site flags
	       unchanged, write warning message and return OK */
	       phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                  "requested die position is outside of wafer "
                  "(SRQ 0x%02x)", (int) srq);
	       break;
	  case 0x64:       /* 100 - test complete */
	       break;
	}
        if (srq == 0x64)
        {
	   PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq2, 2, 0x42, 0x43));
	   switch (srq2)
	   {
              case 0x42:       /* 66 = auto indexing complete Z-down */
	           break;
              case 0x43:       /* 67 = auto indexing complete Z-up */
	           break;
	   }
        }

	if (srq == 0x42 || (srq == 0x64 && srq2 == 0x42))
	{
	     PhPFuncTaCheck(raiseChuck(proberID));
	}
    }
    else  /* Prober is Master */
    {

	/* the following is removed temporarily, we do now receive
           this SRQ during binDie. This may be changed back in the
           future. Don't remove this as possible dead code !!!!!!! */
#if 0
	if (!proberID->p.chuckUnderFirstDie)
	{
	   /* don't need to send Z command after first die of wafer,
	      wait for SRQ 0x43 to start test or handle any exceptional SRQs*/
	   PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 5, 0x42, 0x43, 0x46, 0x52, 0x5e));
	   switch (srq)
	   {
	     case 0x42: /* auto indexed and auto chuck down completed */
	       break;
	     case 0x43: /* auto indexed and auto chuck up completed */
	       break;
	     case 0x46: /* sequence back situation, the wafer test has
                           been restarted */
	       phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
		   "'sequence back' situation detected while waiting for next die (SRQ 0x%02x)",
		   (int) srq);
	       /* The framework will schedule the wafer to be retested */
	       retVal = PHPFUNC_ERR_PAT_REPEAT;
	       break;
	     case 0x52:  /* 82 = cassette end signal from prober */
	       retVal = PHPFUNC_ERR_PAT_DONE;
	       break;
	     case 0x5e:  /* 94 = lot done signal from prober */
	       retVal = PHPFUNC_ERR_PAT_DONE;
	       break;
	   }

	   if (srq == 0x42)
	   {
	       PhPFuncTaCheck(raiseChuck(proberID));
	   }
	}
#endif

	/* else: a new wafer was loaded, and there is no SRQ to receive here */

	/* now receive current die coordinates, if contacted */
	if (retVal == PHPFUNC_ERR_OK)
	{
        retVal = getXYLocation(proberID,xyCoordinates,&x,&y);
	    if (retVal != PHPFUNC_ERR_OK) {
	        return retVal;
	    }
	}
    }


    if (proberID->noOfSites == 1)
	sitesGot[0] = 1;
    else if (retVal == PHPFUNC_ERR_OK)
    {
      retVal = getSitesInfo(proberID,sitesGot);
    }
		
    phPFuncTaStop(proberID);

    /* early return on error or end of wafer */
    if (retVal != PHPFUNC_ERR_OK)
    {
    	return retVal;
    }

    /* now we expect an SRQ for every next die */
    proberID->p.chuckUnderFirstDie = 0;

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

    return retVal;;
}
#endif



#ifdef BINDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested die
 *
 * Authors: Michael Vogt
 *          Ken Ward
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 *
 * Modified 13April2005 to allow either 64 or 256 as max number of bins (CR 23211) - kaw
 *
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

    /* Critical SRQ handling */
    int foundCriticalSRQ = -1;
    static int criticalSRQs[] = {0x43};


    int binCode[4*(1+(PHESTATE_MAX_SITES-1)/4)];
    char binCodeStr[10];
    char pfCode[1+(PHESTATE_MAX_SITES-1)/4];
    char command[4*(1+(PHESTATE_MAX_SITES-1)/4)*9+4];
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    long theBin=0;
    int passed=0;
/* End of Huatek Modification */
    int empty;
    phEstateSiteUsage_t *population;
    phEstateSiteUsage_t newPopulation[PHESTATE_MAX_SITES];
    int entries;
    int site;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if ( phPFuncTaAskAbort(proberID) )
        return PHPFUNC_ERR_ABORTED;

    if ( !perSiteBinMap )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING, 
                         "no binning data available, not sending anything to the prober");
        return PHPFUNC_ERR_OK;
    }

    /* compose the binning information string */
    phEstateAGetSiteInfo(proberID->myEstate, &population, &entries);

    for ( site = 0; site < 4*(1+(proberID->noOfSites-1)/4); site++ )
    {
        /* get bin data or empty status */
        if ( site < proberID->noOfSites )
        {
            theBin = perSiteBinMap[site];
            passed = perSitePassed[site];
            empty = 0;

            switch ( population[site] )
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

            if ( (!empty) && (theBin < 0 || theBin >= proberID->p.max_bins) )
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                                 "invalid binning data\n"
                                 "could not bin to bin index %ld at prober site \"%s\"",
                                 theBin, proberID->siteIds[site]);
                return PHPFUNC_ERR_BINNING;
            }
        }
        else
        {
            empty = 1;
        }

        /* initialize binCode and pfCode */
        binCode[site] = 0;
        if ( site%4 == 0 )
            pfCode[site/4] = 64;

        /* fill in data into binning string */
        if ( !empty )
        {
            binCode[site] = (int) theBin;
            if ( !passed )
                pfCode[site/4] += (1 << (site%4));  //CR41379, fix to support 8 site number and above 
        }

    }

    /* can not send binning data to prober in multi site scenarios
       when SmarTest is controlling the stepping: reason: it is not
       clear how to handle dies, that have been touched twice and
       receive a binning code -1. The above algorithm sets reprobed
       dies to bin 0, but this is a test quality issue. There does not
       seem to be an empty bin available for the TSK, so better send
       no binning data at all. This must be changed in the future ! */
    if ( proberID->p.stIsMaster && proberID->noOfSites > 1 )
    {
        return PHPFUNC_ERR_OK;
    }


    /* compose complete bin command */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_3,
                     "Begin building M command, proberID->p.max_bins = %d", proberID->p.max_bins);

    strcpy(command, "M");

    for ( site = 0; site < proberID->noOfSites; site++ )
    {
        /* start each series of 4 bins with pass/fail character */
        if ( site%4 == 0 )
        {
            strncat(command, &pfCode[site/4], 1);
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_3,
                             "Building M command, pfCode[site/4] = 0x%2X", ((char *)(&pfCode[site/4]))[0]);
        }

        /* for 256 bin and 9999 bin, it takes the format like 0x01000000 0x01000000 0x01000000, 
           the last 6 bit used to represent the bin number */
        /* e.g. 9999 = 0x270F = 0x01000010 0x01011100 0x01001111 = B/O */
        /* for 999999 bin mode, the bin code will take 8 bytes to represent the bin number, and first character will represent  
           the last 4 bit of the bin number and so on , last 3 bytes is ignored */
        /* e.g. 999998 = 0xF423E = 0x01001110(E) 0x01000011(3) 0x01000010(2) 0x01000100(4) 0x01001111(F) 
           0x01000000(@) 0x01000000(@) 0x01000000(@) = NCBDO@@@ */

        if ( proberID->p.max_bins == 64 )
            sprintf(binCodeStr, "%c", (0x40|binCode[site]));
        else if (proberID->p.max_bins == 256) 	// need two character codes per bin when max_bins is 256
            sprintf(binCodeStr, "%c%c", (0x40|((binCode[site]&0xc0)>>6)), (0x40|(binCode[site]&0x3f)));
        else if (proberID->p.max_bins == 10000)
            sprintf(binCodeStr, "%c%c%c", 0x40|((binCode[site]>>12)&0x03), 0x40|((binCode[site]>>6)&0x3f), 0x40|binCode[site]&0x3f);
        else if(proberID->p.max_bins == 1000000)
          sprintf(binCodeStr, "%c%c%c%c%c@@@",(0x40|(binCode[site]&0x0f)),(0x40|((binCode[site]>>4)&0x0f)),
                                              (0x40|((binCode[site]>>8)&0x0f)),(0x40|((binCode[site]>>12)&0x0f)),
                                              (0x40|((binCode[site]>>16)&0x0f)));
        else
        {
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR, 
          "The max bin number is out of range, it should be either 64, 256, 10000 or 1000000");
          return PHPFUNC_ERR_BINNING;
        }
          
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_3,
        "Building M command, site = %d, binCode[site] = 0x%2X, binCodeStr ->%s<-", site, binCode[site], binCodeStr);
        strcat(command, binCodeStr);
    }
    strcat(command, MY_EOC);

    /* send the binning to the prober, wait for ackowledge and step */
    phPFuncTaStart(proberID);

    /* critical SRQ check before binning. Sometime the operator may
     * mistakenly press the MANUAL START button on the prober screen in
     * the middle of testflow execution, which generates
     * unexpected SRQs like SOT(0x43) which can lead to wafer shift if
     * the binning message is sent out
     * NOTE: don't check for repated transaction call*/
    if(proberID->ta->currentCall != proberID->ta->lastCall)
    {
      phComGpibCheckSRQs(proberID->myCom, criticalSRQs,
          sizeof(criticalSRQs)/sizeof(int), &foundCriticalSRQ);
      if(foundCriticalSRQ != -1)
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_FATAL,
            "Before sending the binning message, received STB(0x%02x). " \
            "Quit the test program immediately!", foundCriticalSRQ);
        return PHPFUNC_ERR_ANSWER;
      }

      /* for the first call of this function, also clear the
       * flags before sending M command */
      proberID->p.waferCountReceived = 0;
      proberID->p.waferLoadedImmediatelyAfterBinning = 0;
    }

    PhPFuncTaCheck(phPFuncTaSend(proberID, command));

    /* waiting for the SRQ */
    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID,&srq1,8,0x42,0x43,0x45,0x46,0x51,0x64,0x52,0x5e));

    switch ( srq1 )
    {
        case 0x42:         /* 66 = auto indexing complete, z-down */
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                            "after binning, receive the SRQ 0x42 (chuck is down) without SRQ 0x64 (test complete); please check the Prober status");

            /* anyway, we still raise the chuck in order to continue */
            retVal = raiseChuck(proberID);
            if ( retVal != PHPFUNC_ERR_OK ) {
              return retVal;
            }
            break;
        case 0x43:         /* 67 = auto indexing complete Z-up -   */
            break;
        case 0x45:         /* 69 = marking done -  smartest is master only */
            break;
        case 0x46:         /* 70 = wafer loading done  */
          if(proberID->p.waferCountReceived == 0)
          {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                "Received wafer loading done (SRQ 0x%02x) after binning command \"M\". \n"
                "Sequence Back/Re-alignment (manual operation) case. Will not send the \"L\" command.",
                0x46);
          }
          else
          {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                "Received wafer count (SRQ 0x%02x) and wafer loading done (SRQ 0x%02x) after binning command \"M\". \n"
                "Manual Unload/Next Wafer (manual operation) case. Will not send the \"L\" command.",
                0x50, 0x46);
            proberID->p.waferCountReceived = 0;
          }
          proberID->p.waferLoadedImmediatelyAfterBinning = 1;
          retVal = PHPFUNC_ERR_PAT_DONE;
            break;
        case 0x64:         /* 100 = test done -  prober is master only */
            break;
        case 0x51:         /* 81 = wafer done, "L" needs to be send */
            retVal = PHPFUNC_ERR_PAT_DONE;
            break;
        case 0x52:         /* 82 = cassette done, added to be prepared here */
        case 0x5e:         /* 94 = lot done, added to be prepared here */
            /* we can directly modify the global states here, since it is
                   the last action during this call before return */
            setWaferUnloaded(proberID, PHESTATE_WAFL_OUTCASS);
            setLotEnd(proberID);
            retVal = PHPFUNC_ERR_PAT_DONE;
            break;
        default:
            break;
    }

    /* After receiving "marking/testing done", check the subsequent SRQs  */
    if ( !proberID->p.stIsMaster && (srq1 == 0x45 || srq1 == 0x64) )
    {
        PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq2, 5, 0x42, 0x43, 0x46, 0x52, 0x5e));
        switch ( srq2 )
        {
            case 0x42:       /* 66 = auto indexing complete, but Z-down */
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_4,
                                 "Chuck down detected (SRQ 0x42)");
                retVal = raiseChuck(proberID);
                if ( retVal != PHPFUNC_ERR_OK ) {
                  return retVal;
                }
                break;
            case 0x43:       /* 67 = auto indexing complete */
                break;
            case 0x46:       /* 70 = wafer loading done */
              if(proberID->p.waferCountReceived == 0)
              {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                    "Received wafer loading done (SRQ 0x%02x) after binning command \"M\". \n"
                    "Sequence Back/Re-alignment (manual operation) case. Will not send the \"L\" command.",
                    0x46);
              }
              else
              {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                    "Received wafer count (SRQ 0x%02x) and wafer loading done (SRQ 0x%02x) after binning command \"M\". \n"
                    "Manual Unload/Next Wafer (manual operation) case. Will not send the \"L\" command.",
                    0x50, 0x46);
                proberID->p.waferCountReceived = 0;
              }
              proberID->p.waferLoadedImmediatelyAfterBinning = 1;
              retVal = PHPFUNC_ERR_PAT_DONE;
              break;
            case 0x52:  /* cassette end */
            case 0x5e:  /* lot end */      
                setWaferUnloaded(proberID, PHESTATE_WAFL_OUTCASS);
                setLotEnd(proberID);
                retVal = PHPFUNC_ERR_PAT_DONE;
                break;
            default:
                break;
        }
    }

    phPFuncTaStop(proberID);
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
    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x44));
    PhPFuncTaCheck(raiseChuck(proberID));
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
    char cmd;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
	return PHPFUNC_ERR_ABORTED;

    cmd = proberID->p.b_is_waferid ? 'W' : 'b';

    /* perform communication steps */
    phPFuncTaStart(proberID);
    PhPFuncTaCheck(phPFuncTaSend(proberID, "%c%s", cmd, MY_EOC));
    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x59));
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
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
      "inside STPaused, finally ");
     
    return retVal;

    /* by default not implemented */
    /* ReturnNotYetImplemented("privateSTPaused"); */
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
    /* by default not implemented */
    return PHPFUNC_ERR_OK;
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
    switch (action)
    {
      case PHPFUNC_SR_QUERY:
	/* we can handle this query globaly */
	if (lastCall)
	    phPFuncTaGetLastCall(proberID, lastCall);
	return PHPFUNC_ERR_OK;
      case PHPFUNC_SR_RESET:
	/* The incomplete function should reset and retried as soon as
           it is called again. In case last incomplete function is
           called again it should now start from the beginning */
	phPFuncTaDoReset(proberID);
	return PHPFUNC_ERR_OK;
      case PHPFUNC_SR_HANDLED:
	/* in case last incomplete function is called again it should
           now start from the beginning */
	phPFuncTaDoReset(proberID);
	return PHPFUNC_ERR_OK;
      case PHPFUNC_SR_ABORT:
	/* the incomplete function should abort as soon as it is
           called again */
	phPFuncTaDoAbort(proberID);
	return PHPFUNC_ERR_OK;
    }

    /* by default not implemented */
    return PHPFUNC_ERR_OK;
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
    char cmd;

    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
	return PHPFUNC_ERR_ABORTED;

    cmd = proberID->p.b_is_waferid ? 'b' : 'W';

    phPFuncTaStart(proberID);
    PhPFuncTaCheck(phPFuncTaSend(proberID, "%c%s", cmd, MY_EOC)); 
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%511[^\n\r]", wafSetup));
    phPFuncTaStop(proberID);
    
    if (strlen(wafSetup) > 25 || strlen(wafSetup) == 0)
    {
       phLogFuncMessage(proberID->myLogger, LOG_NORMAL,
          "could not receive wafer identification with command \"%c\"\n"
	  "Please check prober setup parameters and configuration of '%s'\n", 
	   cmd, PHKEY_PL_TSKWBCMD);
       strcpy(wafId, "ID not available");
    }
    else
    {
       strcpy(wafId, wafSetup);
    }

    if (proberID->p.waferid_rm_first_char)
        *wafIdString = wafId+1; /* skip the first character */
    else
        *wafIdString = wafId;

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
    static char lotID[24];
    lotID[0] = '\0';
    *lotIdString = lotID;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
    return PHPFUNC_ERR_ABORTED;

    PhPFuncTaCheck(phPFuncTaSend(proberID, "V%s", MY_EOC));
    PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", lotID));


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
/*    int i;*/
/*    int count;*/
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
    char *mID;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    if (phPFuncTaAskAbort(proberID))
	return PHPFUNC_ERR_ABORTED;

    phPFuncTaStart(proberID);
    retVal = machineID(proberID, &mID);
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
 *   Implements the following information request by CR27092&CR25172:
 *    (1)STDF WCR, 
 *    (2)Wafer Number, Lot Number, Chuck Temperatue, Name of 
 *       Prober setup, Cassette Status.
 *  Implements the following information requested by CR21589
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
  /*Begin CR91531, Adam Huang, 24 Nov 2014*/
  static const phStringPair_t sGpibCommands[] = {
  {PHKEY_NAME_PROBER_STATUS_CARD_CONTACT_COUNT,         "kc"},/*CR91531*/
  {PHKEY_NAME_PROBER_STATUS_CASS_STATUS,                "x"},
  {PHKEY_NAME_PROBER_STATUS_CENTER_X,                   "ur10238"},
  {PHKEY_NAME_PROBER_STATUS_CENTER_Y,                   "ur10239"},
  {PHKEY_NAME_PROBER_STATUS_CHUCK_TEMP,                 "f"},
  {PHKEY_NAME_PROBER_STATUS_DEVICE_PARAMETER,           "ku"},/*CR91531*/
  {PHKEY_NAME_PROBER_STATUS_DIE_HEIGHT,                 "ur0005"},
  {PHKEY_NAME_PROBER_STATUS_DIE_WIDTH,                  "ur0004"},
  {PHKEY_NAME_PROBER_STATUS_ERROR_CODE,                 "E"},
  {PHKEY_NAME_PROBER_STATUS_GROSS_VALUE_REQUEST,        "Y"},/*CR91531*/
  {PHKEY_NAME_PROBER_STATUS_LOT_NUMBER,                 "V"},
  {PHKEY_NAME_PROBER_STATUS_MULTISITE_LOCATION_NUMBER,  "H"},
  {PHKEY_NAME_PROBER_STATUS_POS_X,                      "ur1224"},
  {PHKEY_NAME_PROBER_STATUS_POS_Y,                      "ur1225"},
  {PHKEY_NAME_PROBER_STATUS_PROBER_SETUP_NAME,          "ur0001"},
  {PHKEY_NAME_PROBER_STATUS_WAFER_FLAT,                 "ur0008"},
  {PHKEY_NAME_PROBER_STATUS_WAFER_ID,                   "b"},
  {PHKEY_NAME_PROBER_STATUS_WAFER_NUMBER,               "X"},
  {PHKEY_NAME_PROBER_STATUS_WAFER_SIZE,                 "ur0002"},
  {PHKEY_NAME_PROBER_STATUS_WAFER_STATUS,               "w"},
  {PHKEY_NAME_PROBER_STATUS_WAFER_UNITS,                "ur0708"},
 };
 /*End CR91531*/

  static char response[MAX_STATUS_MSG_LEN] = "";
  static char buffer[MAX_STATUS_MSG_LEN] = "";
  char result[MAX_STATUS_MSG_LEN] = "";
  phPFuncError_t retVal = PHPFUNC_ERR_OK;
  char *token = NULL;
  const phStringPair_t *pStrPair = NULL;
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
  * wafer_size_and_wafer_flat = PROB_HND_CALL(ph_get_stauts wafer_size wafer_flat);
  */
  strcpy(response, "");
  if(strcasecmp(token,"getenablesites") == 0)
    getEnableSites(proberID,response);
  else if(strcasecmp(token,"isContactCheckEnabled") == 0)
  {
     char reason[128] = "";
     if(checkContactCheckStatus(proberID,reason))
       strcpy(response,"yes.");
     else
     {
       phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,"checkContactCheckStatus return false");
       strcpy(response,"no. reason:");
       strcat(response,reason);
     }
  }
  else if( strcasecmp(token,"AUTOZ_GetPreZUPSteps") == 0 )
  {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"the Z UP times of previous die is:%d",ZUPSteps);
        char times[32] = "";
        itoS(ZUPSteps,times);
        strcpy(response,times);
        // reset ZUPSteps
        ZUPSteps = 0;
  }
  else if( strcasecmp(token,"AUTOZ_GetCurrentZHeight") == 0 )
  {
        char* buf =  displayCurrentZheight(proberID);
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"in getStatus, z current height:%s",buf);
        strcpy(response,buf);
  }
  else{
  while ( token != NULL ) {      
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE, 
                     "phFuncGetStatus, token = ->%s<-", token);        

    pStrPair = phBinSearchStrValueByStrKey(sGpibCommands, LENGTH_OF_ARRAY(sGpibCommands), token);
    if( pStrPair != NULL ) {      
      PhPFuncTaCheck(phPFuncTaSend(proberID, "%s%s", pStrPair->value, MY_EOC));
      strcpy(buffer, "");
      PhPFuncTaCheck(phPFuncTaReceive(proberID, 1, "%s", buffer));

      /*
      * here, anything is OK, the query was sent out and the response was received.
      * We need further handle the response, for example:
      *    buffer = "ur0", when we request the "wafer unit"
      *  after analysis,
      *    result = "mm"   
      */
      if ( analysisResultOfGetStatus(pStrPair->key, buffer, result) != SUCCEED ){
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                         "The response = ->%s<- from the Prober cannot be correctly parsed\n" 
                         "This could be an internal error from either Prober or Driver", buffer);
        /* but we still keep the result though it is not parsed correctly*/
        strcpy(result, buffer);
      }

      /* until here, the "result" contains the parsed response from Prober */
    } 
    else 
    {
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
  } // end of else(not getenablesites)
  /* ensure null terminated */
  response[strlen(response)] = '\0';

  phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE, 
                   "privateGetStatus, answer ->%s<-, length = %ld",
                   response, strlen(response));

  phPFuncTaStop(proberID);

  return retVal;
}
#endif

#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
*
* Set Status to Prober
*
* Authors: Garryxie
*
* Description:
*   Implement the commands as below that is reuqested by CR28427 to set the prober :
*     alarm buzzer, enable double z up, disble double z up
*
* Prober specific implementation of the corresponding plugin function
* as defined in ph_pfunc.h
*
* 2006/1/9 modified by garryxie to satisfy the CR28427
***************************************************************************/
phPFuncError_t privateSetStatus(
      phPFuncId_t proberID                            /* driver plugin ID */,
      char *commandString                             /* key name to get parameter/information */,
      char **responseString                           /* output of response string */
  
)
{
  static const phStringPair_t sGpibCommands[] = {
    {PHKEY_NAME_PROBER_STATUS_ALARM_BUZZER, "em"}
  };
  
  static char response[MAX_STATUS_MSG_LEN] = "";
  char bkCommandString[MAX_STATUS_MSG_LEN] = "";        /* 
                                                        * an array used as a backup of the commandString, which is used in this function to remain the content
                                                        * of commandString
                                                        */
  static int stbRet = 0;                                       /*SRQ return value*/
  char result[MAX_STATUS_MSG_LEN] = "";
  phPFuncError_t retVal = PHPFUNC_ERR_OK;
  char *token = NULL;
  const phStringPair_t *pStrPair = NULL;
  int usedLen = 0;
  char sParam[128] = "";                              /*parameter*/
  BOOLEAN commandValid = TRUE ;                       /* whether the command is a valid one */                      
  response[0] = '\0';
  *responseString = response;

  strcpy(sParam, "");  
  strncpy(bkCommandString, commandString, MAX_STATUS_MSG_LEN-1);
  
  /* 
  * The parameter has two format (1)PlainText xxxxx (2) " xx  xxx " can include ' '              
  * we must pretreat the commandstring, when encounter the '\"' ,here we replace            
  * the ' ' with 0x80, and then replace the '\"' with ' '. so we can have a PlainText
  * for example:
  *           "abcdefg   hijk  lmn"
  * will be transfered to abcdefg$$$$hijk$$$lmn( here, input $ to replace 0x80  )                 
  */
  char *p = NULL;                                 
  p = strpbrk(bkCommandString, "\"");
  if( p != NULL ){
    phReplaceCharsInStrWithACertainChar(p, " ", 0x80);
    phReplaceCharsInStrWithACertainChar(p, "\"", ' ');
  }
  
  token = strtok(bkCommandString, " \t\n\r");
  
  if( token == NULL ) {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING, 
                     "No token found in the input parameter of PROB_HND_CALL: 'ph_set_status'");
    return PHPFUNC_ERR_NA;
  }
  
  if (phPFuncTaAskAbort(proberID)) {
    return PHPFUNC_ERR_ABORTED;
  }
  
  phPFuncTaStart(proberID);
  
  strcpy(response, "");
  if ( token != NULL ) {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE, "phFuncGetStatus, token = ->%s<-", token);
  
    pStrPair = phBinSearchStrValueByStrKey(sGpibCommands, LENGTH_OF_ARRAY(sGpibCommands), token);
    if( pStrPair != NULL ) {
      if( strcasecmp(token, PHKEY_NAME_PROBER_STATUS_ALARM_BUZZER) == 0){
        token = strtok(NULL, " \t\n\r");
        if(token != NULL){
          /*
          * for the alarm_buzzer command, there is a message whose length should be 20 followed, 
          * when the message's length is shorter than 20, we fill it with ' ' to expand its length to 20. 
          * when its length is larger than 20 ,we intercept its first 20 characters
          */
          snprintf(sParam, 21, "%s", token);
          strcat(sParam, "                   ");
          sParam[20] = '\0';
           
          char sTemp[2];                                               
          sTemp[0] = 0x80;
          sTemp[1] = '\0';
          /*replace 0x80 in sParm with ' ', because we pretreated the bkCommandString above */
          phReplaceCharsInStrWithACertainChar(sParam, sTemp, ' ');
        } else {
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR, 
                           "in 'ph_set_status', the 'alarm_buzzer' must be followed by the alarm message");
          commandValid = FALSE;
        }        
      }   

      if (commandValid == TRUE) {
        PhPFuncTaCheck(phPFuncTaSend(proberID, "%s%s%s", pStrPair->value, sParam, MY_EOC));
        PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &stbRet, 0));
    
        if ( analysisResultOfSetStatus(pStrPair->key, stbRet, result) != SUCCEED ){
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                           "The response = ->%d<- from the Prober cannot be correctly parsed\n"
                           "This could be an internal error from either Prober or Driver", stbRet);
          /* but we still keep the result though it is not parsed correctly*/
          sprintf(result, "%d", stbRet);
        } else {
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_1, 
                           "after analysisResultOfSetStatus(), the returned result is %s", result);
        }      
            
        if( (strlen(result)+usedLen+2) >= MAX_STATUS_MSG_LEN ) {
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                           "the response string is too long; information query just stop");
        } else {
          strcat(response, result);
        }
      }    
    } else {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                       "The key \"%s\" is not available, or may not be supported yet", token);
      sprintf(response, "%s_KEY_NOT_AVAILABLE", token);
    }
  } /* end of if ( token != NULL ) */
  
  /* ensure null terminated */
  response[strlen(response)] = '\0';
  
  phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE, 
                   "privateGetStatus, answer ->%s<-, length = %ld",
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
    phPFuncId_t proberID     /* driver plugin ID */
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
 * Description: move chuck to the required die position (x, y)
 *
 ***************************************************************************/
static phPFuncError_t indexDie(
    phPFuncId_t proberID          /* driver plugin ID */,
    int x,
    int y
)
{
  /*int eightSites;*/
  /*int xcur, ycur;*/
  /*char onWafer, onWafer1;*/
  static int srq = 0;
  phPFuncError_t retVal = PHPFUNC_ERR_OK;


  if (phPFuncTaAskAbort(proberID))
  {
    return PHPFUNC_ERR_ABORTED;
  }

  phPFuncTaStart(proberID);
  if(coordDigitNumber == 3)
  {
    PhPFuncTaCheck(phPFuncTaSend(proberID,"JY%03dX%03d%s", y, x, MY_EOC));
  }
  else
  {
    PhPFuncTaCheck(phPFuncTaSend(proberID,"JY%04dX%04d%s", y, x, MY_EOC));
  }

  /* answer from prober */
  PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 5, 0x42, 0x43, 0x4a, 0x51, 0x64));
  while (srq == 0x64)
  {
    PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 5, 0x42, 0x43, 0x4a, 0x51, 0x64));
  }


  switch (srq)
  {
    case 0x42:
       /* indexed to die position, chuck down */
       break;
    case 0x43:
       /* indexed to die position, chuck up */
       // when in autoz mode, prober will send 0x43 and 0x69, receive 0x69. 
       phPFuncTaGetSRQ(proberID, &srq, 1, 0x69);
       break;
    case 0x4a:
       /* 
        * position outside of probing area, keep site flags
        * unchanged, write warning message and return OK 
       */
       phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                        "requested die position [%d, %d] is outside of probing area "
                        "(SRQ 0x%02x)", x, y, (int) srq);
       phEstateASetPauseInfo(proberID->myEstate, 0);
       phPFuncTaStop(proberID);
       return PHPFUNC_ERR_PARAM;
       break;
    case 0x51:
       phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                        "wrong requested die position - wafer end (SRQ 0x%02x)", (int) srq);
       retVal = PHPFUNC_ERR_PARAM;
       phPFuncTaStop(proberID);

       return retVal;
       break;
    default:
       break;
  }
  phPFuncTaStop(proberID);


  /* request current die position - valid only during probing */
  #if 0
  /* 
   * TSK simulator dose not support "JY%3dX%3d\r\n" command corretly now,
   * so sending "Q\r\n" command the driver will receive wrong die position
   * for the present.
  */
  phPFuncTaStart(proberID);
  PhPFuncTaCheck(raiseChuck(proberID));
  PhPFuncTaCheck(phPFuncTaSend(proberID,
                               "Q%s", MY_EOC));

  /* answer from prober */
  retVal = phPFuncTaReceive(proberID, 2, "QY%3dX%3d", &xcur, &ycur);
  if (retVal != PHPFUNC_ERR_OK) 
  {
    phPFuncTaRemoveStep(proberID); // force re-send of command - kaw
    return retVal;
  }
  phPFuncTaStop(proberID);

  /* check current die position are the same as requested */
  if((x != xcur)||(y != ycur))
  {
     phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                      "requested die position not achieved");
     retVal = PHPFUNC_ERR_ANSWER;
     return retVal;
  }
  #endif

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
    phPFuncId_t proberID     /* driver plugin ID */,
    phDirection_t dir          /* movement direction */,
    int incremental          /* z incremental size */,
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
      return PHPFUNC_ERR_PARAM;
    }
  }

  phPFuncTaStart(proberID);
  switch( dir ) 
  {
    case BASELINE:   /* move to baseline, waiting command from ACCRETECH*/

      PhPFuncTaCheck(phPFuncTaSend(proberID,
                                  "D%s", MY_EOC));

      /* answer from prober */
      PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x44));

      if(srq != 0x44){
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
      PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x5c));
      if(srq != 0x5c)
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "prober cannot execute Z step");
        retVal = PHPFUNC_ERR_ANSWER;
      }
      else
        *zHeight = (*zHeight) + incremental;
      ++ZUPSteps;
      break;

    case DOWN:   /* move DOWN */

      if ((*zHeight - incremental) < zHeightMin) 
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "z-height will be below minimum z-height, chuck move down not performed!,current z height:%d,incremental:%d,zHeightMin:%d",*zHeight,incremental,zHeightMin);
        retVal = PHPFUNC_ERR_Z_LIMIT;
        break;
      }
      
      PhPFuncTaCheck(phPFuncTaSend(proberID,
                                   "Z-%02d%s", incremental, MY_EOC));
      /* answer from prober */
      PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x5c));
      if (srq != 0x5c)
      {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                         "prober cannot execute Z step");
        retVal = PHPFUNC_ERR_ANSWER;
      }
      else
        *zHeight = (*zHeight) - incremental;
      break;

    default:
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                       "Error movement direction! ");
      retVal = PHPFUNC_ERR_PARAM;
  }
  
  phPFuncTaStop(proberID);

  return retVal;
}

static phPFuncError_t lockChuck( phPFuncId_t proberID )
{
     phPFuncError_t retVal = PHPFUNC_ERR_OK;
     int srq = 0;
     phPFuncTaStart(proberID);
     PhPFuncTaCheck(phPFuncTaSend(proberID,"z%s",MY_EOC) )
     phPFuncTaGetSRQ(proberID, &srq, 1, 0x74);
     if(srq != 0x74 )
        retVal = PHPFUNC_ERR_ANSWER; 
     phPFuncTaStop(proberID);
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
            strncpy(yArr,p+1,sizeof(yArr)-1);
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
#ifdef SETSTATUSFORAUTOZ_IMPLEMENTED
/*****************************************************************************
 *
 * Set parameter to the prober
 *
 * Authors: Jax Wu
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
    static int zCurrHeight = 0;
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
       retVal = indexDie(proberID,x,y);
       //in case customer do AUTO Z in each wafer, reset the total Z UP steps  and total Z UP steps.
       ZUPSteps = 0;
     }
     else if(strcmp(commandString,"AUTOZ_indexDie") == 0 )
     {
        if(parseXYCoordinateFromPara(&x,&y,para) == -1)
            return PHPFUNC_ERR_ANSWER;
        retVal = indexDie(proberID,x,y);
        // when prober move prober card to a position, it will move chuck down firstly, thus, reset the current z heiht
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
       retVal = indexDie(proberID,orginalX,orginalY);
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
 *
 * setContactPosition
 *
 * Authors: Fabrizio Arca - EDO, Kun Xiao Shangai
 *
 * Description: set the current z height as the contact position
 *
 ***************************************************************************/
static phPFuncError_t setContactPosition(
    phPFuncId_t proberID     /* driver plugin ID */
)
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
  PhPFuncTaCheck(phPFuncTaGetSRQ(proberID, &srq, 1, 0x74));
  phPFuncTaStop(proberID);

  if (srq != 0x74)
  {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                     "prober cannot set contact position Z");
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

/*****************************************************************************
 * End of file
 *****************************************************************************/
