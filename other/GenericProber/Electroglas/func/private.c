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
 *            Chris Joyce
 *            Jiawei Lin, Shanghai-R&D, CR 26246
 *            Jiawei Lin, R&D Shanghai, CR27092 & CR25172
 *            Dang-lin Li, R&D Shanghai, CR31204
 *            Danglin Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Jan 2000, Michael Vogt, created
 *            27 Jan 2000, Chris Joyce, added Electoglas specific functions.
 *            July 2005, CR 26246:
 *                Assert the pointers. They should not be empty in Linux. E.g:
 *                "sscanf" will fail if its 1st parameter is empty pointer
 *            August 2005, CR27092 & CR25172:
 *              declare the "privateGetStatus". "getStatusXXX" series functions
 *              are used to retrieve the information or parameter from Prober.
 *              The information is WCR.
 *            Decement 2006, CR31204 & CR31253:
 *              add statements for handling message "TR" from the EG Prober.
 *            Nov 2008, CR41221 & CR42599
 *              Implement the "privateExecGpibCmd". this function performs to send prober setup and action command by gpib.
 *              Implement the "privateExecGpibQuery". this function performs to send prober query command by gpib.
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
#include <errno.h>

#include "ph_keys.h"
#include "gpib_conf.h"
#include "ph_pfunc_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define EOC "\r\n"

/*--- typedefs --------------------------------------------------------------*/
/*--- functions -------------------------------------------------------------*/
static phPFuncError_t getEventMsgTransaction(
    phPFuncId_t proberID      /* driver plugin ID */,
    long timeout              /* time to wait for an event */,
    int  *srqNumReceived      /* received srq */,
    char *messageReceived     /* message buffer to copy received message */
);

static phPFuncError_t waitForSrqEventThenGetMessage(
    phPFuncId_t proberID      /* driver plugin ID */,
    long timeout              /* time to wait for an event */,
    int  *srqNumReceived      /* received srq */,
    char *messageReceived     /* message buffer to copy received message */
);

static phPFuncError_t forceRun(
    phPFuncId_t proberID      /* driver plugin ID */
);

static phPFuncError_t forcePause(
    phPFuncId_t proberID      /* driver plugin ID */
);

static phPFuncError_t getDieLocationFromTS(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *testStartMsg        /* return TS msg from prober */,
    long *xDieCoord           /* get the absolute X coordinate
                                 of the current die */,
    long *yDieCoord           /* same for Y coordinate */
);

static phPFuncError_t getMultiSiteResponse(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *testStartMsg        /* return TS msg from prober */
);

static phPFuncError_t setupSitesPopulated(
    phPFuncId_t proberID      /* driver plugin ID */
);

/*****************************************************************************
 *
 * Delay communication for a given number of microseconds
 *
 ***************************************************************************/
static int localDelay_us(long microseconds)
{
    long seconds = 0;
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
    if (select(0, NULL, NULL, NULL, &delay) == -1 &&
        errno == EINTR)
        return 0;
    else
        return 1;
}

/*****************************************************************************
 *
 * Determine prober family type from model type.
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's phDrvs/sources/egSeries/ph_lib
 * 
 *   EG2001            Prober Vision Series 
 *   EG2010            Prober Vision Series 
 *   EG3001            Prober Vision Series 
 *   EG4085            Prober Vision Series 
 *   EG4060            Commander Series 
 *   EG4080            Commander Series
 *   EG4090            Commander Series 
 *   EG4200            Commander Series  4/200
 *   EG5300            Commander Series  5/300
 *   UNKNOWN_MODEL     UNKNOWN_FAMILY
 *
 ***************************************************************************/
static egProberFamilyType_t getFamilyTypeFromModelType(enum proberModel model)
{
    egProberFamilyType_t family;

    if ( model == EG2001 || 
         model == EG2010 || 
         model == EG3001 || 
         model == EG4085 )
    {
        family=EG_PROBER_VISION_SERIES;
    }
    else if ( model == EG4060 || 
              model == EG4080 || 
              model == EG4090 ||
              model == EG4200 ||
              model == EG5300 )
    {
        family=EG_COMMANDER_SERIES;
    }
    else
    {
        family=UNKNOWN_FAMILY;
    }

    return family;
}


/*****************************************************************************
 *
 * Print wafer info
 *
 * Authors: Chris Joyce
 *
 * History: 3 Feb 2000, Chris Joyce, created
 *
 * Description: Send current wafer info to the logger.
 *
 *
 ***************************************************************************/
static void logWaferInfo(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t location      /* the location to get the
                                           information for */,
    phEstateWafUsage_t usage            /* wafer usage of requested
                                           location */,
    phEstateWafType_t type              /* type of wafer */,
    int count                           /* number of wafers of type
                                           <type> loaded at <location>
                                           (may be > 1 in case of
                                           location is either
                                           PHESTATE_WAFL_INCASS or
                                           PHESTATE_WAFL_OUTCASS) */
)
{
    const char *locationStr;

    switch ( location )
    {
        case PHESTATE_WAFL_INCASS:
            locationStr="INCASS";
            break;
        case PHESTATE_WAFL_OUTCASS:
            locationStr="OUTCASS";
            break;
        case PHESTATE_WAFL_CHUCK:
            locationStr="CHUCK";
            break;
        case PHESTATE_WAFL_REFERENCE:
            locationStr="REF";
            break;
        case PHESTATE_WAFL_INSPTRAY:
            locationStr="INSP";
            break;
        default:
            locationStr="UNKNOWN";
            break;
    }

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
        "Wafer location is %s usage is %s type is %s count is %d ",
        locationStr,
        usage==PHESTATE_WAFU_LOADED ? "LOADED" : "NOT LOADED",
        type==PHESTATE_WAFT_REGULAR ? "REGULAR" : "OTHER",
        count);

    return;
}




/*****************************************************************************
 *
 * Print site info
 *
 * Authors: Chris Joyce
 *
 * History: 15 Feb 2000, Chris Joyce, created
 *
 * Description: Send current site info to the logger.
 *
 *
 ***************************************************************************/
static void logSiteBinInfo(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateSiteUsage_t *populated      /* array of populated sites */,
    long *perSiteBinMap                 /* binning information or NULL, if
                                           sub-die probing is active */,
    long *perSitePassed                 /* pass/fail information (0=fail,
                                           true=pass) on a per site basis or
                                           NULL, if sub-die probing is active */
)
{
    int i;  

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
        "site bin info perSiteBinMap and perSitePassed ");

    for (i=0; i<proberID->noOfSites; i++)
    {
        switch (populated[i])
        {
            case PHESTATE_SITE_POPULATED:
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "site %d is populated ",i+1);
                break;
            case PHESTATE_SITE_EMPTY:
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "site %d is empty ",i+1);
                break;
            case PHESTATE_SITE_DEACTIVATED:
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "site %d is deactivated ",i+1);
                break;
            case PHESTATE_SITE_POPDEACT:
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "site %d is popdeact ",i+1);
                break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
        }
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "site %d bin %ld passed %ld ",i+1,perSiteBinMap[i],perSitePassed[i]);
       
    }

    if ( proberID->binIds )
    {
#ifdef DEBUG
        for (i=0; i<proberID->noOfBins ; i++)
        {
            phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
                "bin %d is %s ",i,proberID->binIds[i]);
        }
#endif
    }
    else
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "bin ids not defined ");
    }

    return;
}


/*****************************************************************************
 *
 * Print current transaction state
 *
 * Authors: Chris Joyce
 *
 * History: 3 Feb 2000, Chris Joyce, created
 *
 * Description: Send current transaction state to the logger.
 *
 *
 ***************************************************************************/
static void updateTransactionLog(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    char tempStr[EG_MAX_DIAGNOSTIC_MSG];

    strcpy(proberID->p.egTransactionLog,"Plugin is ");
    switch ( proberID->p.pluginState )
    {
      case NORMAL_STATE:
        strcat(proberID->p.egTransactionLog,"in a normal");
        break;
      case INTO_PAUSE:
        strcat(proberID->p.egTransactionLog,"going into a paused");
        break;
      case PAUSED_STATE:
        strcat(proberID->p.egTransactionLog,"in a paused");
        break;
      case OUT_OF_PAUSE:
        strcat(proberID->p.egTransactionLog,"coming out of a paused");
        break;
    }
    strcat(proberID->p.egTransactionLog," state, ");

    if ( proberID->p.egTransactionState == EG_TRANSACTIONS_ABORTED ||
         proberID->p.egTransactionState == EG_TRANSACTIONS_HANDLED ||
         proberID->p.egTransactionState == EG_TRANSACTIONS_RESET ||
         proberID->p.egTransactionState == EG_TRANSACTIONS_ERROR )
    {
        strcat(proberID->p.egTransactionLog,"but ");
        switch ( proberID->p.egTransactionState )
        {
            case EG_TRANSACTIONS_ABORTED:
                strcat(proberID->p.egTransactionLog,
                        "transactions for current call have been aborted.");
                break;
            case EG_TRANSACTIONS_HANDLED:
                strcat(proberID->p.egTransactionLog,
                        "previous call transactions have been handled, so start new call.");
                break;
            case EG_TRANSACTIONS_RESET:
                strcat(proberID->p.egTransactionLog,
                        "transactions have been reset for this call.");
                break;
            case EG_TRANSACTIONS_ERROR:
                strcat(proberID->p.egTransactionLog,
                        "in the last call an error occured.");
                break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
        }
    }
    else
    {
        sprintf(tempStr, 
                "so far for this call %d transactions have been completed",
                (int)proberID->p.performedTransactionCount);
        strcat(proberID->p.egTransactionLog,tempStr);

        switch ( proberID->p.egTransactionState )
        {
            case EG_IDLE:
                if ( proberID->p.performedTransactionCount )
                {
                    strcat(proberID->p.egTransactionLog,
                           ", the last call completed successfully.");
                }
                else
                {
                    strcat(proberID->p.egTransactionLog,".");
                }
                break;
            case EG_SEND_MSG_WAITING_TO_SEND:
                strcat(proberID->p.egTransactionLog,
                       " trying to send message to prober.");
                break;
            case EG_GET_MSG_WAITING_SRQ:
                strcat(proberID->p.egTransactionLog,
                       " SRQ and message is expected from the prober.");
                break;
            case EG_GET_MSG_WAITING_MSG:
                strcat(proberID->p.egTransactionLog,
                       " SRQ received waiting for message from prober.");
                break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
      default: break;
/* End of Huatek Modification */
        }
    }
    strcat(proberID->p.egTransactionLog,"\n");

    return;

} 

/*****************************************************************************
 *
 * Print current transaction state
 *
 * Authors: Chris Joyce
 *
 * History: 3 Feb 2000, Chris Joyce, created
 *
 * Description: Send current transaction state to the logger.
 *
 *
 ***************************************************************************/
static void logTransactionState(
    phPFuncId_t proberID      /* driver plugin ID */
)
{

    updateTransactionLog(proberID);

    phLogFuncMessage(proberID->myLogger, 
                     LOG_DEBUG,
                     proberID->p.egTransactionLog);
}

/***************************************************************************
 *
 * Save normal state 
 *
 * Authors: Chris Joyce
 *
 * History: 3 Feb 2000, Chris Joyce, created
 *
 * Description: Normal transactions were being performed for a particulat
 * call but a message was received from the prober to say its going into
 * a paused state. Save current call's transaction state and recover
 * state when out of paused state.
 *
 ***************************************************************************/
static void saveNormalState(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "saveNormalState(P%p)", proberID);

    /* 
     * in normal transactions prober went into a paused state save current
     * transaction state.
     */
    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "save current normal transaction state ");

    logTransactionState(proberID);

    proberID->p.egNormalTransactionState=proberID->p.egTransactionState;
    proberID->p.currentNormalTransactionCall=proberID->p.currentTransactionCall;
    proberID->p.performedNormalTransactionCount=proberID->p.performedTransactionCount;
}

/***************************************************************************
 *
 * Recover normal state 
 *
 * Authors: Chris Joyce
 *
 * History: 3 Feb 2000, Chris Joyce, created
 *
 * Description: Recover normal transaction state saved in saveNormalState()
 * return
 *   TRUE   state has been recovered
 *   FALSE  no saved state to recover
 *
 ***************************************************************************/
static BOOLEAN recoverNormalState(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    BOOLEAN recoverState=FALSE;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "recoverNormalState(P%p)", proberID);

    /* 
     * go from paused transactions into normal transactions. Get saved
     * transaction state. Perform recover only once when currentTransaction
     * call is saved transaction call.
     */

    if ( proberID->p.currentNormalTransactionCall == proberID->currentCall )
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "get previous stored normal transaction state ");

        logTransactionState(proberID);

        proberID->p.egTransactionState=proberID->p.egNormalTransactionState;
        proberID->p.currentTransactionCall=proberID->p.currentNormalTransactionCall;
        proberID->p.performedTransactionCount=proberID->p.performedNormalTransactionCount;

        /* delete state in case recoverNormalState() is called again */
        proberID->p.egNormalTransactionState=EG_IDLE;
        proberID->p.currentNormalTransactionCall=(phPFuncAvailability_t) 0;
        proberID->p.performedNormalTransactionCount=0;

        logTransactionState(proberID);
        recoverState=TRUE;
    }
    else
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "no previously stored transaction state to recover");
        recoverState=FALSE;
    }

    return recoverState;
}


/*****************************************************************************
 *
 * Enter paused state
 *
 * Authors: Chris Joyce
 *
 * History: 3 Feb 2000, Chris Joyce, created
 *
 * Description: Prober is set into paused state.
 *
 *
 ***************************************************************************/
static void enterPausedState(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "enterPausedState(P%p)", proberID);

    proberID->p.egTransactionState=EG_IDLE;
    proberID->p.currentTransactionCall=(phPFuncAvailability_t) 0;
    proberID->p.performedTransactionCount=0;

    /* 
     *  set prober pause flag so framework is informed 
     */
    phEstateASetPauseInfo(proberID->myEstate,1);
    proberID->p.pluginState=PAUSED_STATE;

    logTransactionState(proberID);
}


/*****************************************************************************
 *
 * exit paused state
 *
 * Authors: Chris Joyce
 *
 * History: 3 Feb 2000, Chris Joyce, created
 *
 * Description: Exit paused state.
 *
 *
 ***************************************************************************/
static void exitPausedState(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "exitPausedState(P%p)", proberID);

    proberID->p.pluginState=OUT_OF_PAUSE;

    /* 
     *  set prober pause flag so framework is informed 
     */
    phEstateASetPauseInfo(proberID->myEstate,0);

    logTransactionState(proberID);
}


/*****************************************************************************
 *
 * Get out of paused state.
 *
 * Authors: Chris Joyce
 *
 * History: 3 Feb 2000, Chris Joyce, created
 *
 * Description: At start of normal transaction call but in a paused state.
 * See if a CO (Continue) message has been received from the prober and
 * return PHPFUNC_ERR_OK if that is true.
 *
 ***************************************************************************/
static phPFuncError_t getOutOfPausedState(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    static char messageReceivedCO[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    char command[EG_MAX_MESSAGE_SIZE];*/
/* End of Huatek Modification */

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "getOutOfPausedState(P%p)",
                     proberID);

    /* wait until prober is unpaused */
    rtnValue=getEventMsgTransaction(proberID, proberID->heartbeatTimeout, NULL, messageReceivedCO);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if ( strncmp(messageReceivedCO,"CO",2) != 0 )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "CO not received after PA to Continue probing ");
            rtnValue=PHPFUNC_ERR_ANSWER;
        }
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "got out of paused state ");


    return rtnValue;
}


/*****************************************************************************
 *
 * Set transaction variables
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description:  Set transaction state variables at start of transactions.
 * Check egTransactionState to see if variables should be reset and if
 * transactions have been started whether or not they finished correctly.
 *
 ***************************************************************************/
static phPFuncError_t setTransactionVariables(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    static char messageBuffer[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    proberID->p.currentTransactionCount=0;

    /* first check to see if transactions have been aborted, handled or 
       reset by event handler */
    if ( proberID->p.egTransactionState == EG_TRANSACTIONS_ABORTED ||
         proberID->p.egTransactionState == EG_TRANSACTIONS_HANDLED ||
         proberID->p.egTransactionState == EG_TRANSACTIONS_RESET ||
         proberID->p.egTransactionState == EG_TRANSACTIONS_ERROR )
    {
        if ( proberID->p.egTransactionState == EG_TRANSACTIONS_ABORTED )
        {
            rtnValue=PHPFUNC_ERR_ABORTED;
        }
        proberID->p.egTransactionState=EG_IDLE;
        proberID->p.currentTransactionCall=proberID->currentCall;
        proberID->p.performedTransactionCount=0;
    }
    else
    {
        if ( proberID->p.currentTransactionCall != proberID->currentCall )
        {
            proberID->p.currentTransactionCall=proberID->currentCall;

            proberID->p.performedTransactionCount=0;

            if ( proberID->p.egTransactionState == EG_SEND_MSG_WAITING_TO_SEND ||
                 proberID->p.egTransactionState == EG_GET_MSG_WAITING_SRQ ||
                 proberID->p.egTransactionState == EG_GET_MSG_WAITING_MSG )
            {
                phLogFuncMessage(proberID->myLogger, 
                                 LOG_DEBUG,
                                 "previous call aborted in middle of transaction try to get reply and ignore");
                waitForSrqEventThenGetMessage(proberID,proberID->heartbeatTimeout,NULL,messageBuffer);
            }
            proberID->p.egTransactionState=EG_IDLE;
        }
    }

    strcpy(proberID->p.egDiagnosticString,"");
  
    return rtnValue;
}



/*****************************************************************************
 *
 * Initialize transactions for given phPFuncAvailability_t
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: To be called at the start of every phPFuncAvailability_t
 * call which perform normal transactions.
 *
 *
 ***************************************************************************/
static phPFuncError_t initializeNormalTransactions(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    static char messageBuffer[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,"initialize normal transactions ...");

    logTransactionState(proberID);

    rtnValue=setTransactionVariables(proberID);

    if ( rtnValue==PHPFUNC_ERR_OK && proberID->p.pluginState==PAUSED_STATE )
    {
        rtnValue=getOutOfPausedState(proberID);

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"out of paused state");

            /* set plugin state to OUT_OF_PAUSE */
            exitPausedState(proberID);
        }
    }

    if ( rtnValue==PHPFUNC_ERR_OK && proberID->p.pluginState==OUT_OF_PAUSE )
    {
        if ( recoverNormalState(proberID) == FALSE )
        {
            /* 
             * If just come out of pause but previous normal state was not
             * recovered then get the TA signal and set to a normal state.
             */
            rtnValue=waitForSrqEventThenGetMessage(proberID,proberID->heartbeatTimeout,NULL,messageBuffer);
  
            if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageBuffer,"TA",2)!=0) )            
            {                                                                             
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,                     
                        "TA not received after PA CO MC continue ");                                            
                rtnValue=PHPFUNC_ERR_ANSWER;                                                            
            }                                                                                        
      
 
            /* if get the TA message, get the dir position */
            if(rtnValue==PHPFUNC_ERR_OK && strncmp(messageBuffer,"TA",2)==0)
            {
               phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "Test Start signal received \"%s\"",messageBuffer);

                proberID->p.store_die_loc_valid = FALSE;
                if ( proberID->p.queryDiePosition==FALSE )
                {
                    if ( getDieLocationFromTS(proberID,
                                              messageBuffer,
                                              &proberID->p.store_dieX,
                                              &proberID->p.store_dieY) == PHPFUNC_ERR_OK )
                    {
                        proberID->p.store_die_loc_valid = TRUE;
                    }
                }

                if ( proberID->noOfSites != 1 )
                {
                    rtnValue=getMultiSiteResponse(proberID,messageBuffer);
                }
                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    rtnValue=setupSitesPopulated(proberID);
                }
             }


            if ( rtnValue==PHPFUNC_ERR_OK )            
            {
                proberID->p.pluginState=NORMAL_STATE;
                proberID->p.currentTransactionCall=(phPFuncAvailability_t) 0;
                proberID->p.egTransactionState=EG_IDLE;
            }
        }
        if ( rtnValue==PHPFUNC_ERR_OK )            
        { 
            rtnValue=setTransactionVariables(proberID);
        }
    } 

    return rtnValue;
}

/*****************************************************************************
 *
 * Complete transactions for given phPFuncAvailability_t
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: To be called at the end of every phPFuncAvailability_t
 * which will performs normal transactions.
 *
 *
 ***************************************************************************/
static phPFuncError_t completeNormalTransactions(
    phPFuncId_t proberID      /* driver plugin ID */,
    phPFuncError_t rtnValue   /* return value from phPFuncAvailability_t call */
)
{
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,"complete transactions ...");

    if ( rtnValue == PHPFUNC_ERR_OK || rtnValue == PHPFUNC_ERR_PAT_DONE || rtnValue == PHPFUNC_ERR_ABORTED )
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"transactions all completed");
        proberID->p.currentTransactionCall=(phPFuncAvailability_t) 0;
        proberID->p.performedTransactionCount=0;
    }
    else
    {
        if ( rtnValue == PHPFUNC_ERR_WAITING )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"transactions not completed: waiting for reply");
        }
        else if ( rtnValue == PHPFUNC_ERR_TIMEOUT )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"transactions not completed: waiting to send");
        }
        else
        {
            phLogFuncMessage(proberID->myLogger,
                             LOG_DEBUG,
                             "transactions not completed: error occured (%d)",
                             rtnValue);
            proberID->p.egTransactionState=EG_TRANSACTIONS_ERROR;
        }
    }

    if ( proberID->p.pluginState == INTO_PAUSE )
    {
        saveNormalState(proberID);
        enterPausedState(proberID);
    }
    else if ( proberID->p.pluginState == OUT_OF_PAUSE )
    {
        proberID->p.pluginState=NORMAL_STATE;
    }

#if 0
    /*                                                                     */
    /* added during tests in attempt to detect test case failures where    */
    /* EG prober simulator is occasionally failing to send compete message */
    /*                                                                     */
    if ( rtnValue == PHPFUNC_ERR_ANSWER )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,                     
                "PHPFUNC_ERR_ANSWER detected: go into a forever loop ");                                            
        for (;;)
        {

        }
    }
#endif

    return rtnValue;
}




/*****************************************************************************
 *
 * Initialize paused transactions for given phPFuncAvailability_t
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: To be called at the start of every phPFuncAvailability_t
 * which will perform transactions.
 *
 *
 ***************************************************************************/
static phPFuncError_t initializePausedTransactions(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    static char messageBuffer[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,"initialize paused transactions ...");

    logTransactionState(proberID);

    rtnValue=setTransactionVariables(proberID);

    if ( proberID->p.pluginState == OUT_OF_PAUSE )
    { 
        /* 
         * If just come out of pause then get the TA signal and set to a normal state.
         */
         rtnValue=waitForSrqEventThenGetMessage(proberID,proberID->heartbeatTimeout,NULL,messageBuffer);
  
        if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageBuffer,"TA",2)!=0) )            
        {                                                                             
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,                     
                    "TA not received after PA CO MC continue ");                                            
            rtnValue=PHPFUNC_ERR_ANSWER;                                                            
        }                                                                                        

        if ( rtnValue==PHPFUNC_ERR_OK )            
        {
            proberID->p.pluginState=NORMAL_STATE;
            proberID->p.currentTransactionCall=(phPFuncAvailability_t) 0;
            proberID->p.egTransactionState=EG_IDLE;
        }
    }

    if ( proberID->p.pluginState == NORMAL_STATE )
    {
        rtnValue=forcePause(proberID);

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"into paused state");

            enterPausedState(proberID);

            rtnValue=setTransactionVariables(proberID);
        }
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Complete paused transactions for given phPFuncAvailability_t
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: To be called at the end of every phPFuncAvailability_t
 * which will perform transactions.
 *
 *
 ***************************************************************************/
static phPFuncError_t completePausedTransactions(
    phPFuncId_t proberID      /* driver plugin ID */,
    phPFuncError_t rtnValue   /* return value from phPFuncAvailability_t call */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static char messageBuffer[EG_MAX_MESSAGE_SIZE];*/
/* End of Huatek Modification */

    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,"complete paused transactions ...");

    if ( rtnValue == PHPFUNC_ERR_OK || rtnValue == PHPFUNC_ERR_PAT_DONE || rtnValue == PHPFUNC_ERR_ABORTED )
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"transactions all completed");
        proberID->p.currentTransactionCall=(phPFuncAvailability_t) 0;
        proberID->p.performedTransactionCount=0;
    }
    else
    {
        if ( rtnValue == PHPFUNC_ERR_WAITING )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"transactions not completed: waiting for reply");
        }
        else if ( rtnValue == PHPFUNC_ERR_TIMEOUT )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,"transactions not completed: waiting to send");
        }
        else
        {
            phLogFuncMessage(proberID->myLogger,
                             LOG_DEBUG,
                             "transactions not completed: error occured (%d)",
                             rtnValue);
            proberID->p.egTransactionState=EG_TRANSACTIONS_ERROR;
        }
    }

    if ( proberID->p.pluginState == OUT_OF_PAUSE && rtnValue == PHPFUNC_ERR_OK )
    {
        exitPausedState(proberID);
    }
    else if ( proberID->p.pluginState == INTO_PAUSE && rtnValue == PHPFUNC_ERR_OK )
    {
        proberID->p.pluginState=PAUSED_STATE;
    }

    return rtnValue;
}



/*****************************************************************************
 *
 * Initialize transactions for given phPFuncAvailability_t
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: To be called at the start of every phPFuncAvailability_t
 * which will perform either normal or paused transactions.
 *
 *
 ***************************************************************************/
static phPFuncError_t initializeTransactions(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,"initialize transactions ...");

    switch (proberID->p.pluginState)
    {
      case NORMAL_STATE:
      case OUT_OF_PAUSE:
        rtnValue=initializeNormalTransactions(proberID);
        break;
      case PAUSED_STATE:
      case INTO_PAUSE:
        rtnValue=initializePausedTransactions(proberID);
        break;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Complete transactions for given phPFuncAvailability_t
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: To be called at the end of every phPFuncAvailability_t
 * which has performed either normal or paused transactions.
 *
 *
 ***************************************************************************/
static phPFuncError_t completeTransactions(
    phPFuncId_t proberID      /* driver plugin ID */,
    phPFuncError_t rtnValue   /* return value from phPFuncAvailability_t call */
)
{
    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,"complete transactions ...");

    switch (proberID->p.pluginState)
    {
      case NORMAL_STATE:
      case OUT_OF_PAUSE:
        rtnValue=completeNormalTransactions(proberID,rtnValue);
        break;
      case PAUSED_STATE:
      case INTO_PAUSE:
        rtnValue=completePausedTransactions(proberID,rtnValue);
        break;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Get message.
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Receive a message from mhcom layer and put it into
 * messageReceived.
 *
 ***************************************************************************/
static phPFuncError_t getMessage(
    phPFuncId_t proberID      /* driver plugin ID */,
    char  *messageReceived
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phComError_t comError;      
    const char *message;         /* received message, may contain '\0'
                                    characters. */
    int length;                  /* length of the received message */

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "getMessage(P%p P%p)", proberID,messageReceived);

    comError = phComGpibReceive(proberID->myCom,
                                &message,
                                &length, 
                                proberID->heartbeatTimeout);

    if ( comError != PHCOM_ERR_OK )
    {
        if ( comError == PHCOM_ERR_TIMEOUT )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                "unexpected gpib timeout received from communication layer");
            rtnValue=PHPFUNC_ERR_TIMEOUT;
        }
        else
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "unexpected error code (%d) received from communication layer",
                (int) comError);
            rtnValue=PHPFUNC_ERR_GPIB;
        }
    }
    else
    { 
        /* Received a message */
        if ( length >= EG_MAX_MESSAGE_SIZE )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "unexpectedly large message size (%d) received from communication layer",
                length);
            rtnValue=PHPFUNC_ERR_GPIB;
        }
        else
        {           
            strcpy(messageReceived,message);
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "message received \"%s\"",
                messageReceived);
            rtnValue=PHPFUNC_ERR_OK;
        }
    }
    return rtnValue;
}


/*****************************************************************************
 *
 * Send message
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Send a message to the mhcom layer.
 *
 ***************************************************************************/
static phPFuncError_t sendMessage(
    phPFuncId_t proberID      /* driver plugin ID */,
    const char *message       /* command or query to send */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phComError_t comError;
    char terminatedMessage[EG_MAX_MESSAGE_SIZE];
    
    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "sendMessage(P%p, \"%s\")",
                     proberID, message);

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
        "sending message \"%s\" timeout %ld ",
        message,
        proberID->heartbeatTimeout);

    /* add correct ending to message */
    sprintf(terminatedMessage,"%s\r\n",message); 

    comError = phComGpibSend(proberID->myCom,
                         terminatedMessage,
                         strlen(terminatedMessage), 
                         proberID->heartbeatTimeout);

    if ( comError == PHCOM_ERR_OK )
    {
        rtnValue=PHPFUNC_ERR_OK;
    }
    else if ( comError == PHCOM_ERR_TIMEOUT )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
            "basic transaction failure sending \"%s\" unexpected gpib timeout received from communication layer",
            message);
        rtnValue=PHPFUNC_ERR_TIMEOUT;
    }
    else
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "basic transaction failure sending \"%s\" unexpected error code (%d) received from communication layer",
            message,
            (int) comError);
        rtnValue=PHPFUNC_ERR_GPIB;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Wait for SRQ event then get any message.
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * Jeff/phDvrs/sources/egSeries/ph_lib/driver.c
 * Wait for SRQ event then read any message from prober.
 *
 ***************************************************************************/
static phPFuncError_t waitForSrqEventThenGetMessage(
    phPFuncId_t proberID      /* driver plugin ID */,
    long timeout              /* time to wait for an event */,
    int  *srqNumReceived      /* received srq */,
    char *messageReceived     /* message buffer to copy received message */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phComError_t comError;      /* mhcom error received */
    phComGpibEvent_t *event;    /* current pending event or NULL */
    int pending;                /* number of additional pending events in
                                   the FIFO buffer */
    BOOLEAN receivedSRQ=FALSE;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "waitForSrqEventGetMessage(P%p, %ld)",
                     proberID,
                     timeout);

    /* clear messageReceived string */
    if(srqNumReceived != NULL)
    {
         *srqNumReceived = 0;
    }
    strcpy(messageReceived,"");

    do {
        comError = phComGpibGetEvent(proberID->myCom,
                                     &event,
                                     &pending, 
                                     timeout);

        if ( comError == PHCOM_ERR_OK )
        {
            if ( event == NULL ) 
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "unexpected null event received from communication layer");
                rtnValue=PHPFUNC_ERR_GPIB;
            }
            else if ( event->type != PHCOM_GPIB_ET_SRQ )
            { 
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "event received but not an SRQ ignore %d",
                    event->type);
                rtnValue=PHPFUNC_ERR_OK;
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "SRQ received %d",
                    event->type);
                if(srqNumReceived != NULL)
                {
                     *srqNumReceived = event->d.srq_byte;
                }
                receivedSRQ=TRUE;
            }
        }
        else
        {
            if (comError != PHCOM_ERR_TIMEOUT )
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "unexpected error code (%d) received from communication layer",
                    (int) comError);
                rtnValue=PHPFUNC_ERR_GPIB;
            }
            else
                /* then waiting for event has timed out: return waiting */
                rtnValue=PHPFUNC_ERR_WAITING;
        }
    } while ( rtnValue==PHPFUNC_ERR_OK && !receivedSRQ);

  
    if ( rtnValue==PHPFUNC_ERR_OK && receivedSRQ==TRUE )
    {
        /* set transaction state */
        rtnValue=getMessage(proberID,messageReceived);
    }

    return rtnValue;
}

/*****************************************************************************
 *
 *                         TRANSACTION CALLS START
 *
 ***************************************************************************/

/*****************************************************************************
 *
 * Send message transaction
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Message is sent
 *
 ***************************************************************************/
static phPFuncError_t sendMsgTransaction(
    phPFuncId_t proberID      /* driver plugin ID */,
    const char *message       /* command or query to send */
)
{
    BOOLEAN performTransaction=TRUE;
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    static char messageBuffer[EG_MAX_MESSAGE_SIZE];
    
    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "sendMsgTransaction(P%p, \"%s\")",
                     proberID, message);

    proberID->p.currentTransactionCount++;

    if ( proberID->p.performedTransactionCount >= proberID->p.currentTransactionCount )
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "message \"%s\" has already been sent ",
            message);

        performTransaction=FALSE;
    }
    else
    {
        if ( proberID->p.egTransactionState == EG_IDLE || 
             proberID->p.egTransactionState == EG_SEND_MSG_WAITING_TO_SEND )
        {
            /* 
             *  Just in case there is a pending message waiting to be
             *  read. Clear buffer and ignore message.
             */
            rtnValue=waitForSrqEventThenGetMessage(proberID,0L,NULL,messageBuffer);

            switch ( rtnValue )
            {
                case PHPFUNC_ERR_OK:
                    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                        "\n\nmessage received \"%s\"  while trying to send message \"%s\" message ignored \n\n\n ",
                        messageBuffer,
                        message); 
                    break;
                case PHPFUNC_ERR_TIMEOUT:
                    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                        "srq received without message while trying to send message \"%s\" srq ignored ",
                        message); 
                    break;
                default:
                    if ( rtnValue != PHPFUNC_ERR_WAITING )
                    {
                        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                            "unexpected return value (%d) from buffer clear before sending message \"%s\" ",
                            rtnValue,
                            message);
                    }
                    rtnValue=PHPFUNC_ERR_OK;
            }
            if ( proberID->p.egTransactionState == EG_IDLE )
            {
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "try to send message \"%s\"",message);
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "still trying to send message \"%s\"",message);
            }
        }
        else
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "in wrong transaction state cannot recover ");
                rtnValue=PHPFUNC_ERR_ANSWER;
        }
    }

    if ( rtnValue==PHPFUNC_ERR_OK && performTransaction == TRUE )
    {
        proberID->p.egTransactionState=EG_SEND_MSG_WAITING_TO_SEND;

        rtnValue=sendMessage(proberID,message);

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            proberID->p.egTransactionState=EG_IDLE;
            proberID->p.performedTransactionCount++;
        }
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Wait for SRQ event then get any message.
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * Jeff/phDvrs/sources/egSeries/ph_lib/driver.c
 * Wait for SRQ event then read any message from prober.
 *
 ***************************************************************************/
static phPFuncError_t getEventMsgTransaction(
    phPFuncId_t proberID      /* driver plugin ID */,
    long timeout              /* time to wait for an event */,
    int  *srqNumReceived      /* received srq */,
    char *messageReceived /* message buffer to copy received message */
)
{
    BOOLEAN performTransaction=TRUE;
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "getEventMsgTransaction(P%p, %ld)",
                     proberID,
                     timeout);

    proberID->p.currentTransactionCount++;

    if ( proberID->p.performedTransactionCount >= proberID->p.currentTransactionCount )
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "getEventMsgTransaction has already been performed");
        performTransaction=FALSE;
    }
    else
    {
        if ( proberID->p.egTransactionState == EG_IDLE )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "now wait for SRQ ");
        }
        else if ( proberID->p.egTransactionState == EG_GET_MSG_WAITING_SRQ )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "get event and message still waiting for SRQ ");
        }
        else if ( proberID->p.egTransactionState == EG_GET_MSG_WAITING_MSG )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "get event and message still waiting for message ");
        }
        else
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "in wrong transaction state cannot recover ");
                rtnValue=PHPFUNC_ERR_ANSWER;
        }
    }

    if ( rtnValue == PHPFUNC_ERR_OK && performTransaction == TRUE )
    {

        if ( proberID->p.egTransactionState == EG_IDLE ||
             proberID->p.egTransactionState == EG_GET_MSG_WAITING_SRQ )
        {
            proberID->p.egTransactionState=EG_GET_MSG_WAITING_SRQ;
            rtnValue=waitForSrqEventThenGetMessage(proberID,timeout,srqNumReceived,messageReceived);
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "call function waitForSrqEventThenGetMessage to get message ");
        }
        else
        {
            rtnValue=getMessage(proberID, messageReceived);
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "call function getMessage to get message ");
        }

        switch ( rtnValue )
        {
            case PHPFUNC_ERR_OK:
                proberID->p.egTransactionState=EG_IDLE;
                proberID->p.performedTransactionCount++;
                break;
            case PHPFUNC_ERR_WAITING:
                /* 
                 *  if return value is waiting but timeout is 0L then this was 
                 *  only performed to clear buffer so OK can be returned
                 */
                if ( timeout == 0L )
                { 
                    proberID->p.egTransactionState=EG_IDLE;
                    proberID->p.performedTransactionCount++;
                    rtnValue=PHPFUNC_ERR_OK;
                }
                break;
            case PHPFUNC_ERR_TIMEOUT:
                proberID->p.egTransactionState=EG_GET_MSG_WAITING_MSG;
                break;
            default: break;
        }
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Basic transaction
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Electoglas prober is set-up to rely on a basic transaction 
 * for operation where a command or query is sent to the prober and an SRQ
 * is expected as a reply with a message.
 *
 ***************************************************************************/
static phPFuncError_t basicTransaction(
    phPFuncId_t proberID      /* driver plugin ID */,
    const char *message       /* command or query to send */,
    int  *srqNumReceived      /* received srq */,
    char *messageReceived     /* message buffer to copy received message */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    rtnValue=sendMsgTransaction(proberID,message);

    if ( rtnValue==PHPFUNC_ERR_OK )
    {
        rtnValue=getEventMsgTransaction(proberID,proberID->heartbeatTimeout,srqNumReceived,messageReceived);
    }

    return rtnValue;
}

/*****************************************************************************
 *
 *                       TRANSACTION CALLS END
 *
 ***************************************************************************/


/*****************************************************************************
 *
 * Identify Prober Type 
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Identify Electroglas family and model type from ID string.
 *
 ***************************************************************************/
static phPFuncError_t identifyFamilyModelType(
    phPFuncId_t proberID              /* driver plugin ID */,
    char *id_string                   /* equipment id string */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;


    /* copy id_string and remove EOC part of returned equipmentID string */
    strncpy(proberID->p.equipmentIDStr, id_string, sizeof(proberID->p.equipmentIDStr) - 1);
    if (strlen(proberID->p.equipmentIDStr) >= strlen(EOC) &&
        strcmp(&proberID->p.equipmentIDStr[strlen(proberID->p.equipmentIDStr) - strlen(EOC)], EOC) == 0)
        proberID->p.equipmentIDStr[strlen(proberID->p.equipmentIDStr) - strlen(EOC)] = '\0';

    if (strncmp(id_string,"2001",4) == 0) {
        proberID->p.proberModel = EG2001;
    }
    else if (strncmp(id_string,"2010",4) == 0) {
        proberID->p.proberModel = EG2010;
    }
    else if (strncmp(id_string,"3001",4) == 0) {
        proberID->p.proberModel = EG3001;
    }
    else if (strncmp(id_string,"4085",4) == 0) {
        proberID->p.proberModel = EG4085;
    }
    else if (strncmp(id_string,"4060",4) == 0) {
        proberID->p.proberModel = EG4060;
    }
    else if (strncmp(id_string,"4080",4) == 0) {
        proberID->p.proberModel = EG4080;
    }
    else if (strncmp(id_string,"4090",4) == 0) {
        proberID->p.proberModel = EG4090;
    }
    else if (strncmp(id_string,"4/200",5) == 0) {
        proberID->p.proberModel = EG4200;
    }
    else if (strncmp(id_string,"5/300",5) == 0) {
        proberID->p.proberModel = EG5300;
    }
    else {
        proberID->p.proberModel = UNKNOWN_MODEL;
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "prober model \"%s\" not supported \n",
            id_string);
        rtnValue = PHPFUNC_ERR_MODEL;
    }

    proberID->p.proberFamily = getFamilyTypeFromModelType(proberID->p.proberModel);

    return rtnValue;
}


/*****************************************************************************
 *
 * Compare identified model type with configuration model type.
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Compare model type as identified by "ID" command with model
 * type as defined in configuration file.
 *
 ***************************************************************************/
static phPFuncError_t compareIdentifiedModelToConfigurationModel(
    phPFuncId_t proberID              /* driver plugin ID */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    char *family;
    struct modelDef *models;
    int i;

    if ( proberID->model != PHPFUNC_MOD_GENERIC &&
         proberID->model != proberID->p.proberModel )
    {

        /* get prober model name */
        phFuncPredefModels(&family, &models);
        i = 0;
        while (models[i].model != PHPFUNC_MOD__END &&
               models[i].model != proberID->model )
        {
            i++;
        }

        /* not found in supported model list */
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "The model type defined in the configuration file \"%s\" is different\n"
            "to the identified model \"%s\" ", models[i].name, proberID->p.equipmentIDStr);
        rtnValue=PHPFUNC_ERR_CONFIG;
    }

    if ( proberID->p.stepMode == STEPMODE_LEARNLIST &&
         proberID->p.proberFamily == EG_COMMANDER_SERIES )
    {
        if ( !(*(proberID->p.recipeFileName)) ) 
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "no recipe file name given in configuration \"%s\"", PHKEY_PL_EGRECIPE);
            rtnValue=PHPFUNC_ERR_CONFIG;
        }
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Prober move die 
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * egSeries/ph_lib/driver.c proberMoveDie()
 *
 ***************************************************************************/
static phPFuncError_t proberMoveDie(
    phPFuncId_t proberID      /* driver plugin ID */,
    long dieX                 /* this is the absolute X
                                 coordinate of the current die
                                 request */,
    long dieY                 /* same for Y coordinate */
)
{
    static char messageReceived_ZD[EG_MAX_MESSAGE_SIZE];
    static char messageReceived_MOX[EG_MAX_MESSAGE_SIZE];
    static char messageReceived_ZU[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    char command[EG_MAX_MESSAGE_SIZE];

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "proberMoveDie(P%p, %ld, %ld)",
                     proberID, dieX, dieY);

    /* move Z Down */
    rtnValue=basicTransaction(proberID,"ZD",NULL,messageReceived_ZD);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        sprintf(command,"MOX%03ldY%03ld",dieX,dieY);

        /* Move Absolute in Die Steps */
        rtnValue=basicTransaction(proberID,command,NULL,messageReceived_MOX);
    }

    if ( rtnValue == PHPFUNC_ERR_OK && proberID->p.performSubdieProbing == FALSE )
    {
        /* move Z Up */
        rtnValue=basicTransaction(proberID,"ZU",NULL,messageReceived_ZU);
    }
    return rtnValue;
}



/*****************************************************************************
 *
 * Prober move subdie absolute in Machine Steps
 *
 * Authors: Chris Joyce
 *
 * History: 8 Feb 2000, Chris Joyce, created
 *
 * Description: Move the forcer specified distance relative to the micro
 * origin.
 *
 ***************************************************************************/
static phPFuncError_t proberMoveSubDie(
    phPFuncId_t proberID      /* driver plugin ID */,
    long subdieX              /* this is the relative X
                                 coordinate of the current sub die
                                 request */,
    long subdieY              /* same for X coordinate */
)
{
    static char messageReceived_ZD[EG_MAX_MESSAGE_SIZE];
    static char messageReceived_FM[EG_MAX_MESSAGE_SIZE];
    static char messageReceived_ZU[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    char command[EG_MAX_MESSAGE_SIZE];

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "proberMoveSubDie(P%p, %ld, %ld)",
                     proberID, subdieX, subdieY);

    /* move Z Down */
    rtnValue=basicTransaction(proberID,"ZD",NULL,messageReceived_ZD);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        sprintf(command,"FMX%03ldY%03ld",subdieX,subdieY);

        /* Move Absolute in Die Steps */
        rtnValue=basicTransaction(proberID,command,NULL,messageReceived_FM);
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        /* move Z Up */
        rtnValue=basicTransaction(proberID,"ZU",NULL,messageReceived_ZU);
    }
    return rtnValue;
}


/*****************************************************************************
 *
 * Prober reprobe die 
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * egSeries/ph_lib/driver.c proberReprobeDie()
 *
 ***************************************************************************/
static phPFuncError_t proberReprobeDie(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    static char messageReceived_ZD[EG_MAX_MESSAGE_SIZE];
    static char messageReceived_ZU[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "proberReprobeDie(P%p)",
                     proberID);

    /* move Z Down */
    rtnValue=basicTransaction(proberID,"ZD",NULL,messageReceived_ZD);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        /* move Z Up */
        rtnValue=basicTransaction(proberID,"ZU",NULL,messageReceived_ZU);
    }
    return rtnValue;
}


/*****************************************************************************
 *
 * Prober query die location 
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * egSeries/ph_lib/driver.c proberGetDieLocation()
 *
 ***************************************************************************/
static phPFuncError_t proberQueryDieLocation(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *dieX                /* get the absolute X coordinate 
                                 of the current die */,
    long *dieY                /* same for Y coordinate */
)
{
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    long xDieCoord=0;
    long yDieCoord=0;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "proberQueryDieLocation(P%p, P%p, P%p)",
                     proberID, dieX, dieY);


    /* request actual XY position of die */
    rtnValue=basicTransaction(proberID,"?P",NULL,messageReceived);

    if ( proberID->p.pluginState == OUT_OF_PAUSE )
    {
        rtnValue=basicTransaction(proberID,"?P",NULL,messageReceived);
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if ( strncmp(messageReceived,"PA",2) == 0 )
        {
            proberID->p.pluginState=INTO_PAUSE;
// kaw            rtnValue=PHPFUNC_ERR_WAITING;
        }
        else
        {
            if (sscanf(messageReceived,"X%ldY%ld",&xDieCoord,&yDieCoord) != 2) 
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "unable to interpret returned message \"%s\" with sscanf,\n"
                "format expected XnYn",
                messageReceived);
                rtnValue=PHPFUNC_ERR_ANSWER; 
            }
            else 
            { 
                *dieX = xDieCoord;
                *dieY = yDieCoord;
            } 
        }
    }
    return rtnValue;
}


/*****************************************************************************
 *
 * Prober get die location
 *
 * Authors: Chris Joyce
 *
 * History: 13 Mar 2001, Chris Joyce, created
 *
 ***************************************************************************/
static phPFuncError_t proberGetDieLocation(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *dieX                /* get the absolute X coordinate
                                 of the current die */,
    long *dieY                /* same for Y coordinate */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    if ( proberID->p.store_die_loc_valid )
    {
        /* use stored die location from last TS message */

        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
             "refering to last received die location %ld %ld",
             proberID->p.store_dieX, proberID->p.store_dieY);

        *dieX = proberID->p.store_dieX;
        *dieY = proberID->p.store_dieY;
    }
    else
    {
        rtnValue=proberQueryDieLocation(proberID,dieX,dieY);
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Prober get subdie location 
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 *
 ***************************************************************************/
static phPFuncError_t proberGetSubDieLocation(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *subdieX             /* get the absolute X coordinate 
                                 of the current die */,
    long *subdieY             /* same for Y coordinate */
)
{
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    long xSubDieCoord=0;
    long ySubDieCoord=0;
    int  siteNumber;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "proberGetSubDieLocation(P%p, P%p, P%p)",
                     proberID, subdieX, subdieY);

    /* 
     *  COMMANDER_SERIES prober does not support 
     *  ?F query micro site position.
     */

    /* request micro coordinates */
    rtnValue=basicTransaction(proberID,"?F",NULL,messageReceived);

    if ( proberID->p.pluginState == OUT_OF_PAUSE )
    {
        rtnValue=basicTransaction(proberID,"?F",NULL,messageReceived);
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if ( strncmp(messageReceived,"PA",2) == 0 )
        {
            proberID->p.pluginState=INTO_PAUSE;
// kaw            rtnValue=PHPFUNC_ERR_WAITING;
        }
        else
        {
            if (sscanf(messageReceived,"X%ldY%ldS%d",&xSubDieCoord,&ySubDieCoord,&siteNumber) != 3) 
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "unable to interpret returned message \"%s\" with sscanf,\n"
                "format expected XnYnSd", 
                messageReceived);
                rtnValue=PHPFUNC_ERR_ANSWER; 
            }
            else
            {
                *subdieX = xSubDieCoord;
                *subdieY = ySubDieCoord;
            } 
        }
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Get state variables
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * egSeries/ph_lib/driver.c getStateVars()
 * Gets the actual state of the prober         
 *
 ******************************************************************************/
static phPFuncError_t getStateVariables(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "getStateVariables(P%p)",
                     proberID);

/*    before calling determineState (getStateVariables) a check should  */
/*    be made to see if a CO (Continue) has been made so following call */
/*    is not necessary.                                                 */
/*    taken from getStateVars(): to clear EG4085 buffer                 */
/*    rtnValue=getEventMsgTransaction(proberID,0L,NULL,messageReceived);     */

    /* Request state variables */
    rtnValue=basicTransaction(proberID,"?R",NULL,messageReceived);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if (strncmp(messageReceived,"C",1))     /* waiting for CXSXFX string */
        {
            rtnValue=getEventMsgTransaction(proberID,proberID->heartbeatTimeout,NULL,messageReceived);
        }

        /* In many cases the EG4085 returns either a "TA" or "PA" instead             */
        /* of the status string "CXSXFXRXWXPXEXAX", reading the status                */
        /* twice averts that problem.                                                 */
        if (strncmp(messageReceived,"C",1))   /* try second time */
        {            
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                             "..invalid status string, keep looking");

            rtnValue=getEventMsgTransaction(proberID,proberID->heartbeatTimeout,NULL,messageReceived);
        }
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if (sscanf(messageReceived,
                   "C%dS%dF%dR%dW%dP%dE%dA%d",
                   &(proberID->p.run_code),
                   &(proberID->p.run_subcode),
                   &(proberID->p.first_die_set),
                   &(proberID->p.reference_stored),
                   &(proberID->p.wafer_on_chuck),
                   &(proberID->p.wafer_profiled),
                   &(proberID->p.wafer_edge_profiled),
                   &(proberID->p.wafer_autoaligned) ) != 8) 
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "unable to interpret ?R query returned message \"%s\" with sscanf,\n"
                "format expected CnSnFbRbWbPbEbAb", 
                messageReceived);
                rtnValue=PHPFUNC_ERR_ANSWER;
        }
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Determine state
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * egSeries/ph_lib/driver.c determineState()
 *   From the state of the prober this determines if it is in the half pause
 *   or full pause condition.  This is critical for correct operation of the 
 *   Pause/Continue function.                                         
 *
 ***************************************************************************/
static phPFuncError_t determineState(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "determineState(P%p)",
                     proberID);

    rtnValue=getStateVariables(proberID);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if ( proberID->p.run_code == 0 )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                             "prober state is IDLE");
            proberID->p.proberState=EG_STATE_IDLE;
        }
        else if ( proberID->p.run_code > 0 )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                             "prober state is RUNNING");
            proberID->p.proberState=EG_STATE_RUNNING;
        }
        else if ( proberID->p.run_code < 0 )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                             "prober state is PAUSED");
            proberID->p.proberState=EG_STATE_PAUSED;
        }
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Force pause
 *
 * Authors: Chris Joyce
 *
 * History: 21 Jan 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * egSeries/ph_lib/driver.c forcePause()
 * IMPORTANT  The "PA" pause prober command toggles the pause state of the
 *   prober on and off. This code checks the prober state using"?R" and then
 *   uses "PA" to put the prober in the correct state.
 *
 ***************************************************************************/
static phPFuncError_t forcePause(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    static char messageBuffer[EG_MAX_MESSAGE_SIZE];
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "forcePause(P%p)",
                     proberID);
    /* 
     *  Make sure there are no pending messages to read.
     */
    rtnValue=getEventMsgTransaction(proberID,0L,NULL,messageBuffer); 

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        rtnValue=determineState(proberID);
    }

    if ( rtnValue == PHPFUNC_ERR_OK && proberID->p.proberState == EG_STATE_RUNNING )
    {
        /* Pause/Continue Probing */
        rtnValue=basicTransaction(proberID,"PA",NULL,messageReceived);

        if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceived,"MC",2)!=0) )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "prober doesn't respond to PA - Pause/Continue probing ");
            rtnValue=PHPFUNC_ERR_ANSWER;
        }
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Force run
 *
 * Authors: Chris Joyce
 *
 * History: 4 Feb 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * egSeries/ph_lib/driver.c forcePause()
 * IMPORTANT  The "PA" pause prober command toggles the pause state of the
 *   prober on and off. This code checks the prober state using"?R" and then
 *   uses "PA" to put the prober in the correct state.
 *
 ***************************************************************************/
static phPFuncError_t forceRun(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    static char messageReceivedGetCO[EG_MAX_MESSAGE_SIZE];
    static char messageReceivedGetMC[EG_MAX_MESSAGE_SIZE];
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static char messageReceivedGetTA[EG_MAX_MESSAGE_SIZE];*/
/* End of Huatek Modification */
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "forceRun(P%p)",
                     proberID);
    /* 
     *  First check to see if a CO (Continue) has been issued from the
     *  prober. If one has then assume prober has already been
     *  restarted.
     */

    rtnValue=getEventMsgTransaction(proberID,0L,NULL,messageReceived);

    if ( strncmp(messageReceived,"CO",2) != 0 )
    {
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue=determineState(proberID);
        }

        if ( rtnValue == PHPFUNC_ERR_OK && proberID->p.proberState == EG_STATE_IDLE )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "prober in IDLE state, can't start up");
        }
        else if ( rtnValue == PHPFUNC_ERR_OK && proberID->p.proberState == EG_STATE_PAUSED )
        {
            /* Pause/Continue Probing */
            rtnValue=basicTransaction(proberID,"PA",NULL,messageReceivedGetCO);

            if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceivedGetCO,"CO",2)!=0) )
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "CO not received after PA to Continue probing ");
                rtnValue=PHPFUNC_ERR_ANSWER;
            }

           /* 
            *  Electroglas onsite bug fixes 
            *  Forcing a run should not determine state but see if
            *  returned message is a CO - Continue
            *  Then assume run has been called during probing
            *  and expect to receive a TA
            */

            /* PVS doesn't send MC */
            if ( rtnValue==PHPFUNC_ERR_OK && proberID->p.proberFamily == EG_COMMANDER_SERIES )
            {
                rtnValue=getEventMsgTransaction(proberID,proberID->heartbeatTimeout,NULL,messageReceivedGetMC);

                if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceivedGetMC,"MC",2)!=0) ) 
                {
                    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "MC not received after PA CO continue ");
                    rtnValue=PHPFUNC_ERR_ANSWER;
                }
            }
        }
    }
    return rtnValue;
}



/*****************************************************************************
 *
 * Prober close
 *
 * Authors: Chris Joyce
 *
 * History: 22 Feb 2000, Chris Joyce, created
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * egSeries/ph_lib/driver.c proberClose()
 *
 ***************************************************************************/
static phPFuncError_t proberClose(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static char messageReceived[EG_MAX_MESSAGE_SIZE];*/
/* End of Huatek Modification */
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* trace this call */
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
                     "proberClose(P%p)",
                     proberID);

    return rtnValue;
}


/*****************************************************************************
 *
 * Setup bin code
 *
 * Authors: Chris Joyce
 *
 * History: 4 Feb 2000, Chris Joyce, created
 *
 * Description: 
 *   For given bin information setup correct binCode.
 *
 ***************************************************************************/
static phPFuncError_t setupBinCode(
    phPFuncId_t proberID      /* driver plugin ID */,
    BOOLEAN sitePopulated     /* setup bin code for populated/unpopulated site */,
    long passed               /* for site which has passed */,
    long binNumber            /* for bin number */,
    char *binCode             /* array to setup bin code */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* setup binCode with correct number for this site */
    strcpy(binCode,""); 

    /* Electroglas onsite bug fixes */
    /* don't send 0 bin code on PASS */
    /* take out not populated site bin */

    /* temporary bug fix binNumber sent may be -1 so set this to 0 */

    if ( binNumber == -1 )
        binNumber=0;

    if ( sitePopulated == FALSE )
    {
        sprintf(binCode,"%s","255");  /* site not populated but send binning anyway */
    }
    else if (  proberID->binIds )
    {
        if ( binNumber < 0 || binNumber >= proberID->noOfBins )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "invalid binning data\n"
                "could not bin to bin index %ld \n"
                "maximum number of bins %d set by configuration %s",
                binNumber, proberID->noOfBins,PHKEY_BI_HBIDS);
            rtnValue=PHPFUNC_ERR_BINNING;
        }
        else
        {
            sprintf(binCode,"%s",proberID->binIds[ binNumber ]);
        }
    }
    else
    {
        if (binNumber < 0 || binNumber > 255)
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "invalid binning data\n"
                "could not bin to bin index %ld ",
                binNumber);
            rtnValue=PHPFUNC_ERR_BINNING;
        }
        else
        {
            sprintf(binCode,"%ld",binNumber);
        }
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Get multi-site site response
 *
 * Authors: Chris Joyce
 *
 * History: 4 Feb 2000, Chris Joyce, created
 *
 * Description: 
 *   Get multi-site site reponse from the TS - test start message
 *
 ***************************************************************************/
static phPFuncError_t getMultiSiteResponse(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *testStartMsg        /* return TS msg from prober */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    int srCounter;
    char *tsMsg=testStartMsg;

    /* When Multi-Die probing is enabled format of TF or TS signal
       may be TF,d or TFXnYn,d                   2-32  dice in array
              TF,d,d or TFXnYn,d,d              33-64  dice in array
              TF,d,d,d or TFXnYn,d,d,d          65-96  dice in array
              TF,d,d,d,d or TFXnYn,d,d,d,d      97-128 dice in array
       or     TS,d or TSXnYn,d                   2-32  dice in array
              TS,d,d or TSXnYn,d,d              33-64  dice in array
              TS,d,d,d or TSXnYn,d,d,d          65-96  dice in array
              TS,d,d,d,d or TSXnYn,d,d,d,d      97-128 dice in array
       not interested in coordinates if
       they have been included so sscanf string from first ',' */

    for ( srCounter=0 ; srCounter< proberID->p.sitesResponseArraySize && rtnValue==PHPFUNC_ERR_OK ; srCounter++ )
    {
        tsMsg=strchr(tsMsg,',');

        /* CR 26246 */
        if( tsMsg == NULL ) {
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                           "unable to find ',' in TS or TF message '%s'\n"
                           "for on wafer code, format expected 'TF,d', 'TF,d,d', 'TS,d' or 'TS,d,d'", 
                           testStartMsg);
          rtnValue = PHPFUNC_ERR_ANSWER;
        } else if (sscanf(tsMsg,",%lu",&(proberID->p.sitesResponse[srCounter] )) != 1){
          phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                           "unable to interpret returned test start message '%s' with sscanf \n"
                           "for on wafer code, format expected 'TF,d', 'TF,d,d', 'TS,d' or 'TS,d,d'", 
                           testStartMsg);
          rtnValue=PHPFUNC_ERR_ANSWER;
        } else {
          phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                           "sites response for %d site is %lu ",
                           srCounter+1,proberID->p.sitesResponse[srCounter]);
        }

        if(tsMsg != NULL) tsMsg++; 
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Setup sitesPopulated array from sitesResponse array
 *
 * Authors: Chris Joyce
 *
 * History: 4 Feb 2000, Chris Joyce, created
 *
 * Description: 
 * The sitesResponse array should have been setup from the TS signal.
 * From the information in the sitesReponse array for each binary bit
 * setup the sitePopulation.
 *
 ***************************************************************************/
static phPFuncError_t setupSitesPopulatedFromSitesResponse(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    int srCounter;
    unsigned long mask;
    int bitCounter;
    int site=0;

    for ( srCounter=0 ; srCounter< proberID->p.sitesResponseArraySize && rtnValue==PHPFUNC_ERR_OK ; srCounter++ )
    {
        mask = 1;

        for ( bitCounter=0 ; bitCounter<32 && site<proberID->noOfSites ; bitCounter++, site++ )
        {
            if ( mask & proberID->p.sitesResponse[srCounter] )
            {
                proberID->p.sitesPopulated[site]=TRUE;
            }
            else
            {
                proberID->p.sitesPopulated[site]=FALSE;
            }
            phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
                "sitesPopulated for site %d is %s ",
                site+1,
                proberID->p.sitesPopulated[site]==TRUE ? "TRUE" : "FALSE" );
            mask<<=1;
        }
    }

    if ( site != proberID->noOfSites )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "unable to convert site response data for all sites \n"
            "only able to interpret %d sites out of %d ", 
            site,
            proberID->noOfSites);
        rtnValue=PHPFUNC_ERR_ANSWER;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Setup sitesPopulated array
 *
 * Authors: Chris Joyce
 *
 * History: 4 Feb 2000, Chris Joyce, created
 *
 * Description: 
 * If there is only one site then set site 0 TRUE otherwise set set sites
 * populated from sitesResponse.
 *
 ***************************************************************************/
static phPFuncError_t setupSitesPopulated(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    
    if ( proberID->noOfSites ==  1 )
    {
        proberID->p.sitesPopulated[0]=TRUE;
    }
    else
    {
        rtnValue=setupSitesPopulatedFromSitesResponse(proberID);
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Get micro probing site number from test start message.
 *
 * Authors: Chris Joyce
 *
 * History: 4 Feb 2000, Chris Joyce, created
 *
 * Description: 
 *   Get microprobing site number 'n' from the test start message TSSn.
 *
 ***************************************************************************/
static phPFuncError_t getMicroProbingSiteNumber(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *testStartMsg        /* return TS msg from prober */,
    int *siteNumber           /* micro site number */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    char *tsMsg;

    /* 
     *  When Micro-Die probing is enabled format of TF or TM signal
     *  may be TFSd or TMSd sscanf string from last 'S' 
     */

    tsMsg=strrchr(testStartMsg,'S');

    /* CR 26246 */
    if( tsMsg == NULL ) {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                       "unable to find 'S' in TM or TF message '%s'\n"
                       "for microsite number, format expected 'TFSn' or 'TMSn' ", 
                       testStartMsg);
      rtnValue = PHPFUNC_ERR_ANSWER;
    } else if (sscanf(tsMsg,"S%d",siteNumber) != 1){
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                       "unable to interpret returned test start message '%s' with sscanf \n"
                       "for microsite number, format expected 'TFSn' or 'TMSn' ", 
                       testStartMsg);
      rtnValue = PHPFUNC_ERR_ANSWER;
    } else {
      phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                       "microsite site number is %d ",
                       *siteNumber);
    }

    return rtnValue;
}



/*****************************************************************************
 *
 * Get X and Y position from test start message.
 *
 * Authors: Chris Joyce
 *
 * History: 20 Mar 2000, Chris Joyce, created
 *
 * Description: 
 *   Get X and Y coordinate from the test start message TSXnYn
 *
 ***************************************************************************/
static phPFuncError_t getDieLocationFromTS(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *testStartMsg        /* return TS msg from prober */,
    long *xDieCoord           /* get the absolute X coordinate 
                                 of the current die */,
    long *yDieCoord           /* same for Y coordinate */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    char *posMsg;

    *xDieCoord=0L;
    *yDieCoord=0L;

    /* 
     *  When sending of current XY die coordinates is enabled
     *  TS signal takes for TSXnYn of TFXnYn
     *  sscanf string from first 'X'  
     */

    posMsg=strrchr(testStartMsg,'X');

    /* CR 26246 */
    if( posMsg == NULL ) {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                       "unable to find 'X' in TS or TF message '%s'\n"
                       "for XY position, format not of the 'TFXnYn' or 'TSXnYn' form ", 
                       testStartMsg);
      rtnValue = PHPFUNC_ERR_ANSWER;
    } else if (sscanf(posMsg,"X%ldY%ld",xDieCoord,yDieCoord) != 2){
      phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                       "unable to interpret returned test start message '%s' with sscanf \n"
                       "for XY position, format not of the 'TFXnYn' or 'TSXnYn' form ", 
                       testStartMsg);
      rtnValue = PHPFUNC_ERR_ANSWER;
    } else {
      phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                       "XY position determined X = %ld Y = %ld ",
                       *xDieCoord, *yDieCoord);
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Perform configuration file configuration.
 *
 * Authors: Chris Joyce
 *
 * History: 3 Nov 2000, Chris Joyce, created
 *
 * Description: 
 *   Set plugin configuration according to configuration file values.
 *
 ***************************************************************************/
static phPFuncError_t setConfigurationFromConfFile(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phConfError_t confError;
    int found = 0;
    const char *confRecipeFileName = NULL;
    int length;
    int srCounter;
    
    /* find out who is going to control the stepping */
    phConfConfStrTest(&found, proberID->myConf, PHKEY_PR_STEPMODE,
        "smartest", "prober", "learnlist", NULL);
    switch (found)
    {
      case 0:
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "configuration \"%s\" not given, assuming \"smartest\"",
            PHKEY_PR_STEPMODE);
        /* fall through into case 1 */
      case 1:
        proberID->p.stepMode=STEPMODE_EXPLICIT;
        break;
      case 2:
        proberID->p.stepMode=STEPMODE_AUTO;
        break;
      case 3:
        proberID->p.stepMode=STEPMODE_LEARNLIST;
        break;
      default:
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "unknown configuration value for \"%s\"", PHKEY_PR_STEPMODE);
        rtnValue=PHPFUNC_ERR_CONFIG;
    }

    /* Determine prober family type from configuration file */
    proberID->p.configProberFamily = getFamilyTypeFromModelType(proberID->model);

    /* find out whether to stop the prober when smartest is paused */
    phConfConfStrTest(&found, proberID->myConf, PHKEY_OP_PAUPROB,
        "yes", "no", NULL);
    if (found == 1)
    {
        proberID->p.stopProberOnSmartestPause = TRUE;
    }
    else 
    {
        proberID->p.stopProberOnSmartestPause = FALSE;
    }

    /* find out whether we do sub-die probing */
    phConfConfStrTest(&found, proberID->myConf, PHKEY_PR_SUBDIE,
        "no", "yes", NULL);
    if (found == 2)
    {
        proberID->p.performSubdieProbing = TRUE;
    }
    else 
    {
        proberID->p.performSubdieProbing = FALSE;
    }

    /* 
     * find out whether or not to bin bad dies for single 
     * sites when smartest is in control.
     * in any other cases ignore and give warning message
     */
    proberID->p.inkBadDies = FALSE;
    confError = phConfConfIfDef(proberID->myConf, PHKEY_BI_INKBAD);
    if ( confError == PHCONF_ERR_OK )
    {
        if ( proberID->p.stepMode==STEPMODE_EXPLICIT )
        {
            phConfConfStrTest(&found, proberID->myConf, PHKEY_BI_INKBAD,
                "no", "yes", NULL);
            if (found == 2)
            {
                proberID->p.inkBadDies = TRUE;
            }
            else 
            {
                proberID->p.inkBadDies = FALSE;
            }

            if ( proberID->noOfSites>1 && proberID->p.inkBadDies==TRUE )
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "configuration parameter \"%s\" not supported for stepping controlled by \"smartest\" for multidie probing", 
                PHKEY_BI_INKBAD);
                rtnValue=PHPFUNC_ERR_CONFIG;
            }
        }
        else
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "configuration parameter \"%s\" not supported for stepping controlled by \"prober\" or \"learnlist\"", 
            PHKEY_BI_INKBAD);
            rtnValue=PHPFUNC_ERR_CONFIG;
        }
    }

    /* get the recipe file if its been defined */
    strcpy(proberID->p.recipeFileName,"");
    confError = phConfConfIfDef(proberID->myConf, PHKEY_PL_EGRECIPE);
    if (confError == PHCONF_ERR_OK)
    {
        confError = phConfConfString(proberID->myConf,
            PHKEY_PL_EGRECIPE, 0, NULL, &confRecipeFileName);
        if (confError == PHCONF_ERR_OK)
        {
            length=strlen(confRecipeFileName);

            /* Received a message */
            if ( length >= EG_MAX_MESSAGE_SIZE )
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "unexpectedly large recipe file name \"%s\" ",
                    confRecipeFileName);
                rtnValue=PHPFUNC_ERR_CONFIG;
            }
        }
    }

    /* 
     *  A recipe file name only makes sense if the step pattern
     *  is learnlist and the model type is one of the Electroglas 
     *  Commander Series
     */ 
    if ( proberID->p.stepMode == STEPMODE_LEARNLIST &&
         ( proberID->p.configProberFamily == EG_COMMANDER_SERIES ||
           proberID->model == PHPFUNC_MOD_GENERIC ))

    {
        if ( !(confRecipeFileName) )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "no recipe file name given in configuration \"%s\"", PHKEY_PL_EGRECIPE);
            rtnValue=PHPFUNC_ERR_CONFIG;
        }
        else
        {
            strncpy(proberID->p.recipeFileName, confRecipeFileName,
          sizeof(proberID->p.recipeFileName) - 1);
        }
    }
    else
    {
        if ( proberID->p.stepMode != STEPMODE_LEARNLIST && confRecipeFileName )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
            "stepping pattern is not learnlist: recipe file \"%s\" ignored.",
            confRecipeFileName);
        }
        if ( proberID->p.configProberFamily == EG_PROBER_VISION_SERIES && confRecipeFileName )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
            "Electroglas Prober Vision Series do not need a recipe file: \"%s\" ignored.",
            confRecipeFileName);
        }
        if ( proberID->model == PHPFUNC_MOD_GENERIC && confRecipeFileName )
        {
            strcpy(proberID->p.recipeFileName,confRecipeFileName);
        }
    }

    /* find out whether or not to send the SM15 commmand */
    phConfConfStrTest(&found, proberID->myConf, PHKEY_PL_EGSENDSETUPCMD,
        "yes", "no", NULL);
    if (found == 2)
    {
        proberID->p.sendSetupCommand = FALSE;
    }
    else 
    {
        proberID->p.sendSetupCommand = TRUE;
    }

    /* find out whether or not to send the ?P commmand */
    phConfConfStrTest(&found, proberID->myConf, PHKEY_PL_EGQRYDIEPOS,
        "yes", "no", NULL);
    if (found == 2)
    {
        proberID->p.queryDiePosition = FALSE;
    }
    else 
    {
        proberID->p.queryDiePosition = TRUE;
    }

    /* set up number of sites array */
    if (proberID->noOfSites > 1)
    {
        proberID->p.sitesResponseArraySize=(proberID->noOfSites-1)/32 + 1;

        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "Site response array size is %d ",proberID->p.sitesResponseArraySize);

        /* initialize site response array */
        for ( srCounter=0 ; srCounter< proberID->p.sitesResponseArraySize ; srCounter++ )
        {
            proberID->p.sitesResponse[srCounter] = 0xffffffff;
        }
    }

    /* set up sites populated array */
    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        rtnValue=setupSitesPopulated(proberID);
    }

    /* 
     *  Check configuration for illegal combinations
     *  and warning messages for current implimentation:
     * 
     *  stepping controlled by smartest STEPMODE_EXPLICIT and multisite
     *  binning 
     *  (Not defined how to ink for multisite binning with the IK command)
     *
     *  stepping controlled by prober or learnlist cannot have
     *  multisite binning and subdie probing together
     *  (Instead of TC for multiple sites there is an ET command
     *   Multiple die test complete but cannot find one for 
     *   the equivalent for the FC command (Micro test complete))
     * 
     *  cannot have subdie probing and prober in control
     *  for subdies a learnlist must be sent which is only 
     *  appropriate for learnlist mode.
     *
     */

    if ( proberID->p.stepMode == STEPMODE_EXPLICIT &&
         proberID->noOfSites > 1 )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
            "for %s \"smartest\" multisite inking is not supported for this plugin",
            PHKEY_PR_STEPMODE);
    }

    if ( ( proberID->p.stepMode == STEPMODE_AUTO ||
           proberID->p.stepMode == STEPMODE_LEARNLIST ) &&
           proberID->noOfSites > 1 &&
           proberID->p.performSubdieProbing == TRUE )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "for %s \"prober\" or \"learnlist\" \n"
            "multisite binning and subdie probing cannot be performed together for this plugin",
            PHKEY_PR_STEPMODE);
        rtnValue=PHPFUNC_ERR_CONFIG;
    }

    if ( proberID->p.stepMode == STEPMODE_AUTO &&
         proberID->p.performSubdieProbing == TRUE )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
            "for %s \"prober\" micro lists cannot be set-up. \nThis is currently only supported  with \"learnlist\" mode ",
            PHKEY_PR_STEPMODE);
    }

    /* check environment variable to see if prober is simulated */
    if ( getenv("PHCOM_PROBER_SIMULATION") )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
            " privateReconfigure():      Simulator In Use (GPIB card as nac)");
        proberID->p.proberSimulation=TRUE;
    }

    return rtnValue;
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
 *     "SM1Un", where 
 *         n=0: die is in Enlish, i.e. 0.1 mil
 *         n=1: die is in metric, i.e. um (micro)
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
static int analysisResultOfGetStatus(const char * commandString, const char *encodedResult, char *pResult)
{
  static const char *waferUnits[] = {"mil", "um"};
  static const char *posX[] = {"", "Left", "Right", "Right", "Left"};
  static const char *posY[] = {"", "Up", "Up", "Down", "Down"};
  
  int retVal = SUCCEED;
  const char *p = encodedResult;

  
  if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_SIZE) == 0 ) {
    int size = 0;
    if( sscanf(p,"SP4D%d",&size) == 1 ){
      sprintf(pResult, "%d%s", size, "mm");
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_UNITS) == 0 ) {
    int unit = 0;
    if( (sscanf(p,"SM1U%1d",&unit)==1) && (unit>=0) && (unit<=1) ) {
      strcpy(pResult, waferUnits[unit]);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_POS_X) == 0 ) {
    int quad = 0;
    if( (sscanf(p,"SM11Q%1d",&quad)==1) && (1<=quad) && (quad<=4) ) {
      strcpy(pResult, posX[quad]);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_POS_Y) == 0 ) {
    int quad = 0;
    if( (sscanf(p,"SM11Q%1d",&quad)==1) && (1<=quad) && (quad<=4) ) {
      strcpy(pResult, posY[quad]);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CENTER_X) == 0 ) {
    int x = 0;
    int y = 0;
    int dummy1 = 0;
    int dummy2 = 0;
    int dummy3 = 0;
    if( sscanf(p,"IX%dY%dX%dY%dD%d",&dummy1,&dummy2,&x,&y,&dummy3) == 5 ) {
      sprintf(pResult, "%.1f%s", x*0.1, "mil");
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_CENTER_Y) == 0 ) {
    int x = 0;
    int y = 0;
    int dummy1 = 0;
    int dummy2 = 0;
    int dummy3 = 0;
    if( sscanf(p,"IX%dY%dX%dY%dD%d",&dummy1,&dummy2,&x,&y,&dummy3) == 5 ) {
      sprintf(pResult, "%.1f%s", y*0.1, "mil");
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_DIE_HEIGHT) == 0 ) {
    char x[16] = "";
    char y[16] = "";
    if( sscanf(p,"SP29X%[0-9.]Y%[0-9.]",x,y) == 2 ) {
      sprintf(pResult, "%s", y);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_DIE_WIDTH) == 0 ) {
    char x[16] = "";
    char y[16] = "";
    if( sscanf(p,"SP29X%[0-9.]Y%[0-9.]",x,y) == 2 ) {
      sprintf(pResult, "%s", x);
    } else {
      retVal = FAIL;
    }
  } else if( strcasecmp(commandString, PHKEY_NAME_PROBER_STATUS_WAFER_FLAT) == 0 ) {
    int flat = 0;
    if( sscanf(p,"SM3F%d",&flat) == 1 ) {
      sprintf(pResult, "%d", flat);
    } else {
      retVal = FAIL;
    }
  } else {
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
 *          Chris Joyce
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
    /* initialize private variables */
    proberID->p.egTransactionState=EG_IDLE;
    proberID->p.currentTransactionCall=(phPFuncAvailability_t) 0;
    proberID->p.performedTransactionCount=0;
    proberID->p.currentTransactionCount=0;

    proberID->p.egNormalTransactionState=EG_IDLE;
    proberID->p.currentNormalTransactionCall=(phPFuncAvailability_t) 0;
    proberID->p.performedNormalTransactionCount=0;

    proberID->p.dieListCount=0;
    proberID->p.dieListX=NULL;
    proberID->p.dieListY=NULL;
    proberID->p.subdieListCount=0;
    proberID->p.subdieListX=NULL;
    proberID->p.subdieListY=NULL;
    proberID->p.proberState=EG_STATE_UNKNOWN;
    proberID->p.firstWafer=TRUE;
    proberID->p.stopProberOnSmartestPause=FALSE;
    proberID->p.sendSetupCommand=TRUE;
    proberID->p.queryDiePosition=TRUE;
    proberID->p.performSubdieProbing = FALSE;
    proberID->p.probingComplete = FALSE;
    proberID->p.waferDone = FALSE;
    proberID->p.firstMicroDieSiteNumber=0;
    proberID->p.currentMicroDieSiteNumber=0;
    proberID->p.pluginState=NORMAL_STATE;
    proberID->p.proberSimulation=FALSE;

    strcpy(proberID->p.equipmentIDStr,"");
    strcpy(proberID->p.recipeFileName,"");
    strcpy(proberID->p.egTransactionLog,"");
    strcpy(proberID->p.egDiagnosticString,"");

    proberID->p.store_dieX          = 0L;
    proberID->p.store_dieY          = 0L;
    proberID->p.store_die_loc_valid = FALSE;

    return PHPFUNC_ERR_OK;
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
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    static BOOLEAN firstTime = TRUE;
    static char messageReceived_ID[EG_MAX_MESSAGE_SIZE];
    static char messageReceived_SM15[EG_MAX_MESSAGE_SIZE];
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static char messageReceived_WM[EG_MAX_MESSAGE_SIZE];*/
/* End of Huatek Modification */

    rtnValue = setConfigurationFromConfFile(proberID);

    if ( rtnValue == PHPFUNC_ERR_OK && firstTime==TRUE )
    {
        rtnValue=initializeNormalTransactions(proberID);

        /* get prober's ID */
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue=basicTransaction(proberID,"ID",NULL,messageReceived_ID);
        }

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue = identifyFamilyModelType(proberID, messageReceived_ID);
        }

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue = compareIdentifiedModelToConfigurationModel(proberID);
        }

        /* send setup command string */
        if ( rtnValue == PHPFUNC_ERR_OK && proberID->p.sendSetupCommand )
        {
             /*
              * Electroglas onsite bug fixes
              * No need for alarm calls
              * remove number 9 ALARM MESSAGES
              */
            /*                                       1234567890123  */
            rtnValue=basicTransaction(proberID,"SM15M1111101100010",NULL,messageReceived_SM15);
        }

#if 0
        /* On site tests send "WM" so send XY coordinates with TS */
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue=basicTransaction(proberID,"WM1",NULL,messageReceived_WM);
        }
#endif

        rtnValue=completeNormalTransactions(proberID,rtnValue);

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            firstTime=FALSE;
        }
    }

    return rtnValue;
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
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR, 
                     "privateReset not yet implemented");

    return PHPFUNC_ERR_NA;
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


    *equipIdString=proberID->p.equipmentIDStr;
    return PHPFUNC_ERR_OK;
}
#endif



#ifdef LDCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Load an input cassette
 *
 * Authors: Michael Vogt
 *          Chris Joyce
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
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
/*    int count;*/
/*    phEstateWafUsage_t usage;*/
/*    phEstateWafType_t type;*/
/* End of Huatek Modification */

    rtnValue=initializeNormalTransactions(proberID);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        /* ensure input cassette is loaded: counting not used */
        phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_LOADED);
    }

    return completeNormalTransactions(proberID,rtnValue);
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
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    int count;
    int i;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    phEstateWafUsage_t usage;*/
/*    phEstateWafType_t type;*/
/* End of Huatek Modification */

    rtnValue=initializeNormalTransactions(proberID);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if ( proberID->p.probingComplete == TRUE )
        {
            rtnValue=proberClose(proberID);
            if ( rtnValue == PHPFUNC_ERR_OK )
            {
                rtnValue=PHPFUNC_ERR_PAT_DONE;
            }
        }
        else
        {
            /* remove input cassette and empty output cassette */
            phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_NOTLOADED);
            phEstateAGetWafInfo(proberID->myEstate, PHESTATE_WAFL_OUTCASS,
                NULL, NULL, &count);
            for (i=0; i<count; i++)
                phEstateASetWafInfo(proberID->myEstate, PHESTATE_WAFL_OUTCASS,
                    PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
        }
    }

    return completeNormalTransactions(proberID,rtnValue);
}
#endif



#ifdef LDWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Load a wafer to the chuck
 *
 * Authors: Michael Vogt
 *          Chris Joyce
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
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    BOOLEAN correctMessage=FALSE;
    BOOLEAN sendHWCommand=TRUE;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    phEstateWafUsage_t usage;*/
/*    phEstateWafType_t type;*/
/*    int count;*/
/* End of Huatek Modification */

    rtnValue=initializeNormalTransactions(proberID);

    if ( rtnValue == PHPFUNC_ERR_ABORTED )
    {
        /* 
           load wafer has been aborted by operator:
           There are no more wafers to load so close
           operations
         */
        proberID->p.probingComplete = TRUE;
    }

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

    /*
     *  If stepping is controlled by prober STEPMODE_AUTO or
     *  through a learnlist STEPMODE_LEARNLIST 
     *      Check to see if wafer is to be re-tested (source is chuck) 
     *          AP - abort probing                                         
     *          BA - begin auto probe                                 
     *      wait for autoprobe signal
     *  if stepping is controlled by smartest STEPMODE_EXPLICIT
     *  and source is in cassette
     *      send "HW" signal
     */

    if ( proberID->p.stepMode == STEPMODE_AUTO || proberID->p.stepMode == STEPMODE_LEARNLIST )
    {
        if ( source == PHESTATE_WAFL_CHUCK )
        {
            /* 
             *  Make sure there are no pending messages to read.
             */
            rtnValue=getEventMsgTransaction(proberID,0L,NULL,messageReceived);

            if ( proberID->p.proberFamily == EG_PROBER_VISION_SERIES )
            {
                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    rtnValue=basicTransaction(proberID,"AP",NULL,messageReceived); 
                }
                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    rtnValue=sendMsgTransaction(proberID,"BA"); 
                }
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "unable to retest wafer: please pause prober and retest ");
            }
        }

        while( rtnValue == PHPFUNC_ERR_OK && correctMessage == FALSE )
        {
            if ( proberID->p.firstWafer==TRUE && source==PHESTATE_WAFL_INCASS )
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
                    "PRESS PROBE BUTTON ON PROBER");
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
                    "WAITING FOR AUTOPROBE TO COMPLETE");
            }
    
            rtnValue = getEventMsgTransaction(proberID,proberID->heartbeatTimeout,NULL,messageReceived);

            if ( (strncmp(messageReceived,"TF",2) == 0) || 
                 (strncmp(messageReceived,"TS",2) == 0) ||
                 (strncmp(messageReceived,"TR",2) == 0) ||
                 (strncmp(messageReceived,"TM",2) == 0) )
            {
                correctMessage=TRUE;

                proberID->p.store_die_loc_valid = FALSE;
                if ( rtnValue==PHPFUNC_ERR_OK && proberID->p.queryDiePosition==FALSE )
                {
                    if ( getDieLocationFromTS(proberID, 
                                              messageReceived, 
                                              &proberID->p.store_dieX, 
                                              &proberID->p.store_dieY) == PHPFUNC_ERR_OK )
                    {
                        proberID->p.store_die_loc_valid = TRUE;
                    }
                }

                if ( proberID->noOfSites != 1 )
                {
                    rtnValue=getMultiSiteResponse(proberID,messageReceived);
                }
                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    rtnValue=setupSitesPopulated(proberID);
                }
                if ( proberID->p.performSubdieProbing == TRUE )
                {
                    rtnValue=getMicroProbingSiteNumber(proberID, 
                                                       messageReceived,
                                                       &(proberID->p.currentMicroDieSiteNumber));

                    proberID->p.firstMicroDieSiteNumber = proberID->p.currentMicroDieSiteNumber;
                }
            }
        } 
    }
    else /* STEPMODE_EXPLICIT (smartest) */
    {
        if ( rtnValue==PHPFUNC_ERR_OK && source==PHESTATE_WAFL_INCASS )
        {
           /* 
            *  Off site Electroglas tests 22/3/2000
            *  If its a Prober Vision Series Prober then don't
            *  just send a HW since a wafer is needed to do profiling
            *  If this is the first time this routine is called then
            *  check status with ?R then if there is
            *  a wafer on the chuck 
            *  and System is Idle
            *  and System is waiting for commands
            *  and First Die is set
            *  and wafer is profiled
            *  and wafer is auto aligned
            *  then test this wafer
            *  other wise send HW
            */

            if ( proberID->p.firstWafer==TRUE && proberID->p.proberFamily == EG_PROBER_VISION_SERIES )
            {
                rtnValue=determineState(proberID);

                if ( rtnValue==PHPFUNC_ERR_OK && proberID->p.wafer_on_chuck == 1 )
                {
                    if ( proberID->p.run_code            == 0 &&
                         proberID->p.run_subcode         == 0 &&
                         proberID->p.first_die_set       == 1 &&
                         proberID->p.wafer_profiled      == 1 &&
                         proberID->p.wafer_autoaligned   == 1 )
                    {
                        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
                            "Wafer already on chuck start probing on this wafer...  ");
                        sendHWCommand=FALSE;
                    }
                    else
                    {
                        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
                            "Wafer on chuck not set-up ");
                        strcpy(proberID->p.egDiagnosticString,
                               "Wafer on chuck not set-up "); 
                        rtnValue=PHPFUNC_ERR_ANSWER;
                    }
                }
            }
 
            if ( rtnValue==PHPFUNC_ERR_OK && sendHWCommand == TRUE )
            { 
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "Perform HW transaction");
    
                /* send "HW" Handle Wafer */
                rtnValue=basicTransaction(proberID,"HW",NULL,messageReceived);

                if ( strncmp(messageReceived,"MF",2) == 0 )
                {
                    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
                    "Handle wafer (HW) command failed: All wafers tested? ");

                    rtnValue=PHPFUNC_ERR_ANSWER;
    
                    strcpy(proberID->p.egDiagnosticString,
                           "Handle wafer (HW) command failed. "); 

                    /* 
                     * delay things by heartbeatTimeout to give prober
                     * time to react before this call is repeated by
                     * frame work.
                     */
                    localDelay_us(proberID->heartbeatTimeout);

                }
            } 
        }
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if ( (strncmp(messageReceived,"TS",2) == 0) && correctMessage==TRUE )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
                        "Test Start \"TS\" received but not \"TF\" signal accepted will continue");
        }

        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "Perform wafer load on abstract model ");

        phEstateASetWafInfo(proberID->myEstate,
            PHESTATE_WAFL_CHUCK, PHESTATE_WAFU_LOADED, PHESTATE_WAFT_REGULAR);

        /* first wafer in cassette now being tested */
        proberID->p.firstWafer=FALSE;
        /* set first die on wafer variable */
        proberID->p.firstDie=TRUE;
    }

    return completeNormalTransactions(proberID,rtnValue);
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
 ***************************************************************************/
phPFuncError_t privateUnloadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t destination   /* where to unload the wafer
                                           to. PHESTATE_WAFL_INCASS is
                                           not valid option! */
)
{
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phEstateWafUsage_t usage;
    phEstateWafType_t type;

    rtnValue=initializeNormalTransactions(proberID);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        /* get info on wafer on chuck */
        phEstateAGetWafInfo(proberID->myEstate,
            PHESTATE_WAFL_CHUCK, &usage, &type, NULL);

        logWaferInfo(proberID,destination,usage,type,0);

        /* remove wafer from chuck */
        phEstateASetWafInfo(proberID->myEstate, PHESTATE_WAFL_CHUCK,
            PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
        /* put wafer on destination */
        phEstateASetWafInfo(proberID->myEstate, destination,
            PHESTATE_WAFU_LOADED, type);
    }

    return completeNormalTransactions(proberID,rtnValue);
}
#endif



#ifdef GETDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next die
 *
 * Authors: Michael Vogt
 *          Chris Joyce
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
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

/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    static char messageReceived[EG_MAX_MESSAGE_SIZE];*/
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phEstateSiteUsage_t *oldPopulation;
    int entries;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    int site;
/*    int paused = 0;*/
/* End of Huatek Modification */

    rtnValue=initializeNormalTransactions(proberID);

    /* if cannot initialize transactions return now */
    if ( rtnValue != PHPFUNC_ERR_OK )
    {
        return completeNormalTransactions(proberID,rtnValue);
    } 

    /*
     *  if stepping is controlled by prober STEPMODE_AUTO then
     *      if this is the first die on the wafer
     *          send "MF" Move to First (Preset) Position command to
     *          prober
     *          send "ZU" Move Z Up to raise the chuck
     *      get die location
     *
     *  if stepping is controlled by a learnlist STEPMODE_LEARNLIST then
     *      if this is the first die on the wafer
     *          move die to first X Y position on learn list
     *      get die location
     *
     *  if stepping is controlled by smartest STEPMODE_EXPLICIT
     *      move prober die to X Y
     */

    if ( proberID->p.stepMode == STEPMODE_AUTO )
    {
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue=proberGetDieLocation(proberID,dieX, dieY);
        }
    }
    else if ( proberID->p.stepMode == STEPMODE_LEARNLIST )
    {
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue=proberGetDieLocation(proberID,dieX, dieY);
        }
    }
    else /* STEPMODE_EXPLICIT (smartest) */
    {
        /* move to die position X Y */
        rtnValue=proberMoveDie( proberID, *dieX , *dieY );
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        /* remember current die positions */
        proberID->p.currentDieX=*dieX;
        proberID->p.currentDieY=*dieY;
        proberID->p.firstDie=FALSE;
        proberID->p.firstSubDie=TRUE;

        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "Perform get die on abstract model ");

        phEstateAGetSiteInfo(proberID->myEstate, &oldPopulation, &entries);
        for (site=0; site<proberID->noOfSites ; site++)
        {
            if (proberID->activeSites[site] == 1)
            {
                if ( proberID->p.sitesPopulated[site] == TRUE )
                { 
                    population[site] = PHESTATE_SITE_POPULATED;
                }
                else 
                { 
                    population[site] = PHESTATE_SITE_EMPTY;
                }
            }
            else
            {
                population[site] = PHESTATE_SITE_DEACTIVATED;
            }
        }
        phEstateASetSiteInfo(proberID->myEstate, population, proberID->noOfSites);
    }

    return completeNormalTransactions(proberID,rtnValue);
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
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phEstateSiteUsage_t *population;
    int entries;
    phEstateSiteUsage_t newPopulation[PHESTATE_MAX_SITES];
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    BOOLEAN sitePopulated=FALSE;
/* End of Huatek Modification */
    int site;
    char command[EG_MAX_MESSAGE_SIZE];
    char binCode[10];
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int paused;*/
/* End of Huatek Modification */

    rtnValue=initializeNormalTransactions(proberID);

    /* if cannot initialize transactions return now */
    if ( rtnValue != PHPFUNC_ERR_OK )
    {
        return completeNormalTransactions(proberID,rtnValue);
    } 

    if ( proberID->p.performSubdieProbing == TRUE &&
         proberID->p.waferDone == TRUE )
    {
        proberID->p.waferDone=FALSE;
        rtnValue=PHPFUNC_ERR_PAT_DONE;
    }

    /* check perSiteBinMap and perSitePassed */
    if ( !perSiteBinMap || !perSitePassed )
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "perSiteBinMap P%p or perSitePassed P%p NULL pointer ",perSiteBinMap,perSitePassed);
        return completeNormalTransactions(proberID,rtnValue);
    }

    /* 
     *  if stepping is controlled by smartest STEPMODE_EXPLICIT
     *      setup IK bin code
     *  check for a MC "Message complete" command
     *
     *  if stepping is controlled by prober STEPMODE_AUTO OR
     *     stepping is controlled by a learnlist STEPMODE_LEARNLIST then 
     *      setup TC single site or
     *            EC multi-site bin code
     *
     *  if a PC "Step Pattern Complete" message has been returned
     *      return to frame work pattern done.
     *  else if a TS "Test Start"
     *            TM "Test Micro Site"
     *            TF "Test First Die"
     *      that's okay return
     *  else if a TA "Test again" continue after pause
     *      if Ptpo has been perfomed this wafer then
     *          then send message again
     *
     *  else if a PA "Pause Continue"
     *      then inform frame work a pause has been received
     *
     *  else if a ER "error"  
     *      then this is a problem need to halt
     *          force pause
     *
     */

    phEstateAGetSiteInfo(proberID->myEstate, &population, &entries);

    logSiteBinInfo(proberID,population,perSiteBinMap,perSitePassed);

    if ( proberID->p.stepMode == STEPMODE_EXPLICIT)
    {
  if (proberID->noOfSites==1 && proberID->p.inkBadDies==TRUE )
  {
      /* When stepping is controlled by smartest only single
         site is supported */
      site=0;

      strcpy(command,"IK");
      rtnValue=setupBinCode(proberID,
    TRUE,
    perSitePassed[site],
    perSiteBinMap[site],
    binCode);
      strcat(command,binCode);

      switch (population[site])
      {
        case PHESTATE_SITE_POPULATED:
        case PHESTATE_SITE_EMPTY:
    newPopulation[site] = PHESTATE_SITE_EMPTY;
    break;
        case PHESTATE_SITE_POPDEACT:
        case PHESTATE_SITE_DEACTIVATED:
    newPopulation[site] = PHESTATE_SITE_DEACTIVATED;
    break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
          default: break;
/* End of Huatek Modification */
      }

      /* send "IKc" command Ink Device c = bincode */
      if ( rtnValue == PHPFUNC_ERR_OK )
      {
    rtnValue=basicTransaction(proberID,command,NULL,messageReceived);
      }

            if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceived,"MC",2)!=0) )
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "Ink Device (IKc) inking command failed");
                rtnValue=PHPFUNC_ERR_ANSWER;
      }
  }
    } 
    else /* STEPMODE_AUTO || STEPMODE_LEARNLIST */
    {

        /* make binning string */
        if ( proberID->noOfSites == 1 )
        {
            strcpy(command,"TC");
        }
        else
        {
            strcpy(command,"ET");
        }

        for (site = 0; site < proberID->noOfSites ; site++)
        {
            switch (population[site])
            {
              case PHESTATE_SITE_POPULATED:
                newPopulation[site] = PHESTATE_SITE_EMPTY;
                sitePopulated=TRUE;
                break;
              case PHESTATE_SITE_POPDEACT:
                newPopulation[site] = PHESTATE_SITE_DEACTIVATED;
                sitePopulated=TRUE;
                break;
              case PHESTATE_SITE_EMPTY:
              case PHESTATE_SITE_DEACTIVATED:
                newPopulation[site] = population[site];
                sitePopulated=FALSE;
                break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
        default: break;
/* End of Huatek Modification */
            }

            rtnValue=setupBinCode(proberID,
                                  sitePopulated,
                                  perSitePassed[site],
                                  perSiteBinMap[site],
                                  binCode);

            strcat(command,binCode);
            if ( site+1 < proberID->noOfSites  )
            {
                strcat(command,",");
            }
        }

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue=basicTransaction(proberID,command,NULL,messageReceived);
        }

        if ( proberID->p.pluginState == OUT_OF_PAUSE )
        {
            rtnValue=getEventMsgTransaction(proberID,proberID->heartbeatTimeout,NULL,messageReceived);
        }

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            if ( strncmp(messageReceived,"PC",2) == 0 )
            {
                /* step patt complete, point next wafer */
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "Received pattern complete ");
                rtnValue=PHPFUNC_ERR_PAT_DONE;
            }
            else if ( (strncmp(messageReceived,"TS",2) == 0 ) ||
                      (strncmp(messageReceived,"TR",2) == 0 ) ||
                      (strncmp(messageReceived,"TM",2) == 0 ) ||
                      (strncmp(messageReceived,"TA",2) == 0 ) ||
                      (strncmp(messageReceived,"TF",2) == 0 ) )
            {
                /* TS - Test Start      */                
                /* TR - Test cycle or retest */                
                /* TM - Test Micro Site */
                /* TF - Test First Die  */
                /* TA - Continue after pause */

                /* 
                 *  In Jeff Morton's code if PTPO was performed
                 *  for this wafer then the TCn command should
                 *  be resent and the answer checked
                 */

                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "Test Start signal received \"%s\"",messageReceived);

                proberID->p.store_die_loc_valid = FALSE;
                if ( rtnValue==PHPFUNC_ERR_OK && proberID->p.queryDiePosition==FALSE )
                {
                    if ( getDieLocationFromTS(proberID, 
                                              messageReceived, 
                                              &proberID->p.store_dieX, 
                                              &proberID->p.store_dieY) == PHPFUNC_ERR_OK )
                    {
                        proberID->p.store_die_loc_valid = TRUE;
                    }
                }

                if ( proberID->noOfSites != 1 )
                {
                    rtnValue=getMultiSiteResponse(proberID,messageReceived);
                }
                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    rtnValue=setupSitesPopulated(proberID);
                }
            }
            else if ( strncmp(messageReceived,"PA",2) == 0 )
            {
                proberID->p.pluginState=INTO_PAUSE;
// kaw                rtnValue=PHPFUNC_ERR_WAITING;
            }
            else if ( strncmp(messageReceived,"ER",2) == 0 )
            {
                /* 
                 * Problem need to halt
                 */
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "Received \"%s\" message try to pause prober",messageReceived);
                rtnValue=forcePause(proberID);
                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    proberID->p.pluginState=INTO_PAUSE;
                    phEstateASetPauseInfo(proberID->myEstate,1);
// kaw                    rtnValue=PHPFUNC_ERR_WAITING;
                }
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "Reply from TCn command \"%s\" not understood",
                    messageReceived);
                rtnValue=PHPFUNC_ERR_ANSWER;
            }
        }
    }

    if ( (rtnValue == PHPFUNC_ERR_OK || rtnValue == PHPFUNC_ERR_PAT_DONE) && (proberID->p.pluginState != INTO_PAUSE))  // kaw
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "Perform bin die on abstract model ");

        phEstateASetSiteInfo(proberID->myEstate, newPopulation, proberID->noOfSites);
    }

    return completeNormalTransactions(proberID,rtnValue);
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
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    int i;

    rtnValue=initializeNormalTransactions(proberID);

    /* if cannot initialize transactions return now */
    if ( rtnValue != PHPFUNC_ERR_OK )
    {
        return completeNormalTransactions(proberID,rtnValue);
    } 

    /* 
     *  if stepping is controlled by prober STEPMODE_AUTO OR
     *     by a learnlist STEPMODE_LEARNLIST then 
     *        get die location
     *
     *  if stepping is controlled by smartest STEPMODE_EXPLICIT
     *        move prober subdie X Y
     */

    if ( proberID->p.stepMode == STEPMODE_AUTO || proberID->p.stepMode == STEPMODE_LEARNLIST )
    {
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            if ( proberID->p.stepMode == STEPMODE_LEARNLIST )
            {
                if ( proberID->p.currentMicroDieSiteNumber < 1 || 
                     proberID->p.currentMicroDieSiteNumber > proberID->p.subdieListCount )
                {
                    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                        "unable to determine micro site coordinates using current site number %d "
                        "expected range 1-%d",
                        proberID->p.currentMicroDieSiteNumber,
                        proberID->p.subdieListCount);
                }
                else
                {
                    *subdieX=(proberID->p.subdieListX)[proberID->p.currentMicroDieSiteNumber-1];
                    *subdieY=(proberID->p.subdieListY)[proberID->p.currentMicroDieSiteNumber-1];
                }
            }
            else /* STEPMODE_AUTO */
            {
                if ( proberID->p.configProberFamily == EG_PROBER_VISION_SERIES )
                {
                    rtnValue=proberGetSubDieLocation(proberID,subdieX, subdieY);
                }
                else
                {
                    /*
                     * Unable to determine subdie location for Commander Series
                     * of probers since the ?F request is not supported.
                     */
                    *subdieX=PHPFUNC_DIEPOS_NA;
                    *subdieY=PHPFUNC_DIEPOS_NA;
                }
            }
        }
    }
    else /* STEPMODE_EXPLICIT (smartest) */
    {
        /* move to die position X Y */
        rtnValue=proberMoveSubDie( proberID, *subdieX , *subdieY );
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        proberID->p.firstSubDie=FALSE;

        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "Perform get subdie on abstract model ");

        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "current die position is X= %ld Y= %ld ",proberID->p.currentDieX,proberID->p.currentDieY); 

        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "current microdie position  is X= %ld Y= %ld ",
            *subdieX,
            *subdieY); 

        for (i=0; i<proberID->noOfSites; i++)
            population[i] = PHESTATE_SITE_POPULATED;
        phEstateASetSiteInfo(proberID->myEstate, population, proberID->noOfSites);
    }

    return completeNormalTransactions(proberID,rtnValue);
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
    long *perSiteBinMap      /* binning information */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis */
)
{

    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    phEstateSiteUsage_t *population;
    int entries;
    phEstateSiteUsage_t newPopulation[PHESTATE_MAX_SITES];
    int site;
    char command[EG_MAX_MESSAGE_SIZE];
    char binCode[10];

    rtnValue=initializeNormalTransactions(proberID);

    /* if cannot initialize transactions return now */
    if ( rtnValue != PHPFUNC_ERR_OK )
    {
        return completeNormalTransactions(proberID,rtnValue);
    } 

    /* check perSiteBinMap and perSitePassed */
    if ( !perSiteBinMap || !perSitePassed )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
            "perSiteBinMap P%p or perSitePassed P%p NULL pointer ",perSiteBinMap,perSitePassed);
        return completeNormalTransactions(proberID,rtnValue);
    }

    phEstateAGetSiteInfo(proberID->myEstate, &population, &entries);

    logSiteBinInfo(proberID,population,perSiteBinMap,perSitePassed);

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
        "current die position is X= %ld Y= %ld ",proberID->p.currentDieX,proberID->p.currentDieY); 

    /* 
     *  if stepping is controlled by smartest STEPMODE_EXPLICIT
     *      setup IK bin code
     *  check for a MC "Message complete" command
     *
     *  if stepping is controlled by prober STEPMODE_AUTO OR
     *     stepping is controlled by a learnlist STEPMODE_LEARNLIST then 
     *      setup FC bin code
     *
     *  if a PC "Step Pattern Complete" message has been returned "
     *      return to frame work pattern done.
     *  else if microdieCounter is lastMicroDie
     *      return pattern complete
     *  else if a TS "Test Start"
     *            TM "Test Micro Site"
     *            TF "Test First Die"
     *      that's okay return
     *  else if a TA "Test again" continue after pause
     *      if Ptpo has been perfomed this wafer then
     *          then send message again
     *
     *  else if a PA "Pause Continue"
     *      then inform frame work a pause has been received
     *
     *  else if a ER "error"  
     *      then this is a problem need to halt
     *          force pause
     *
     */

    /* For subdie binning only single site is supported */
    site=0;

    if ( proberID->p.stepMode == STEPMODE_EXPLICIT )
    {
        strcpy(command,"IK");
        rtnValue=setupBinCode(proberID,
                              TRUE,
                              perSitePassed[site],
                              perSiteBinMap[site],
                              binCode);
        strcat(command,binCode);

        switch (population[site])
        {
          case PHESTATE_SITE_POPULATED:
          case PHESTATE_SITE_EMPTY:
            newPopulation[site] = PHESTATE_SITE_EMPTY;
            break;
          case PHESTATE_SITE_POPDEACT:
          case PHESTATE_SITE_DEACTIVATED:
            newPopulation[site] = PHESTATE_SITE_DEACTIVATED;
            break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
    default: break;
/* End of Huatek Modification */
        }

        /* send "IKc" command Ink Device c = bincode */
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue=basicTransaction(proberID,command,NULL,messageReceived);
        }

        if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceived,"MC",2)!=0) )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "Ink Device (IKc) inking command failed");
            rtnValue=PHPFUNC_ERR_ANSWER;
        }
    } 
    else /* STEPMODE_AUTO || STEPMODE_LEARNLIST */
    {

        strcpy(command,"FC");
        rtnValue=setupBinCode(proberID,
                              TRUE,
                              perSitePassed[site],
                              perSiteBinMap[site],
                              binCode);
        strcat(command,binCode);

        switch (population[site])
        {
          case PHESTATE_SITE_POPULATED:
          case PHESTATE_SITE_EMPTY:
            newPopulation[site] = PHESTATE_SITE_EMPTY;
            break;
          case PHESTATE_SITE_POPDEACT:
          case PHESTATE_SITE_DEACTIVATED:
            newPopulation[site] = PHESTATE_SITE_DEACTIVATED;
            break;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 326 */
    default: break;
/* End of Huatek Modification */
        }

        /* send "FCn" command Micro Test Complete n 0 to 9 : ; < = > ? */
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            rtnValue=basicTransaction(proberID,command,NULL,messageReceived);
        }

        if ( proberID->p.pluginState == OUT_OF_PAUSE )
        {
            rtnValue=getEventMsgTransaction(proberID,proberID->heartbeatTimeout,NULL,messageReceived);
        }

        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            if ( strncmp(messageReceived,"PC",2) == 0 )
            {
                /* step patt complete, point next wafer */
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "Send pattern complete ");
                proberID->p.waferDone=TRUE;
                rtnValue=PHPFUNC_ERR_PAT_DONE;
            }
            else if ( (strncmp(messageReceived,"TS",2) == 0 ) ||
                      (strncmp(messageReceived,"TR",2) == 0 ) ||
                      (strncmp(messageReceived,"TM",2) == 0 ) ||
                      (strncmp(messageReceived,"TA",2) == 0 ) ||
                      (strncmp(messageReceived,"TF",2) == 0 ) )
            {
                /* TS - Test Start      */
                /* TR - Test cycle or retest */                
                /* TM - Test Micro Site */
                /* TF - Test First Die  */
                /* TA - Continue after pause */

                /* 
                 *  In Jeff Morton's code if PTPO was performed
                 *  for this wafer then the TCn command should
                 *  be resent and the answer checked
                 */

                proberID->p.store_die_loc_valid = FALSE;
                if ( rtnValue==PHPFUNC_ERR_OK && proberID->p.queryDiePosition==FALSE )
                {
                    if ( getDieLocationFromTS(proberID, 
                                              messageReceived, 
                                              &proberID->p.store_dieX, 
                                              &proberID->p.store_dieY) == PHPFUNC_ERR_OK )
                    {
                        proberID->p.store_die_loc_valid = TRUE;
                    }
                }

                if ( proberID->p.performSubdieProbing == TRUE )
                {
                    rtnValue=getMicroProbingSiteNumber(proberID, 
                                                       messageReceived,
                                                       &(proberID->p.currentMicroDieSiteNumber));
                }

                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "Test Start signal received \"%s\"",messageReceived);
            }
            else if ( strncmp(messageReceived,"PA",2) == 0 )
            {
                /* 
                 *  PA - Pause / Continue
                 */
                proberID->p.pluginState=INTO_PAUSE;
// kaw                rtnValue=PHPFUNC_ERR_WAITING;
            }
            else if ( strncmp(messageReceived,"ER",2) == 0 )
            {
                /* 
                 * Problem need to halt
                 */
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "Received \"%s\" message try to pause prober",messageReceived);
                rtnValue=forcePause(proberID);
                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    proberID->p.pluginState=INTO_PAUSE;
                    phEstateASetPauseInfo(proberID->myEstate,1);
// kaw                    rtnValue=PHPFUNC_ERR_WAITING;
                }
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                    "Reply from TCn command \"%s\" not understood",
                    messageReceived);
                rtnValue=PHPFUNC_ERR_ANSWER;
            }
        }
    }

    if ( (rtnValue == PHPFUNC_ERR_OK || rtnValue == PHPFUNC_ERR_PAT_DONE ) && (proberID->p.pluginState != INTO_PAUSE)) //kaw

    {
        if ( rtnValue == PHPFUNC_ERR_OK &&
             proberID->p.stepMode != STEPMODE_EXPLICIT &&
             proberID->p.currentMicroDieSiteNumber == proberID->p.firstMicroDieSiteNumber )
        {
            phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                "End of pattern detected by looking at site number current and first are %d ",
                proberID->p.currentMicroDieSiteNumber); 
            rtnValue=PHPFUNC_ERR_PAT_DONE;
        }

        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "Perform bin subdie on abstract model ");

        phEstateASetSiteInfo(proberID->myEstate, newPopulation, proberID->noOfSites);
    }

    return completeNormalTransactions(proberID,rtnValue);
}
#endif


#ifdef DIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send die learnlist to prober
 *
 * Authors: Michael Vogt
 *          Chris Joyce
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
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    int i;
    char command[EG_MAX_MESSAGE_SIZE];

    rtnValue=initializeNormalTransactions(proberID);

    /* if cannot initialize transactions return now */
    if ( rtnValue != PHPFUNC_ERR_OK )
    {
        return completeNormalTransactions(proberID,rtnValue);
    } 

    /* save die list parameters */
    proberID->p.dieListCount=count;
    proberID->p.dieListX=dieX;
    proberID->p.dieListY=dieY;

    for ( i=0 ; i< count ; i++ )
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "die list X= %ld Y= %ld ",
            dieX[i],
            dieY[i]);
    }


    /* 
     *  clear existing learn list
     *  if the prober family is Prober Vision Series then
     *      send an "RS" command Clear Learn List
     *  if the prober family is Commander Series
     *      send a $LSCL+recipe file name command
     *  FOR each entry in the die list
     *      IF prober family is ProberVision Series
     *          "ADXnYn" add die to learn list 
     *      ELSE
     *          $LSAD+recipe file+XnYn 
     *      END IF
     */

    if ( proberID->p.proberFamily == EG_PROBER_VISION_SERIES )
    {
        /* clear learn list send "RS" Clear Learn List */ 
        rtnValue=basicTransaction(proberID,"RS",NULL,messageReceived);
    }
    else /* COMMANDER SERIES */
    {
        /* clear learn list send $LSCL+recipe file name 
           deletes all dice from specified list */ 

        sprintf(command,"$LSCL\"%s\"",proberID->p.recipeFileName);
        rtnValue=basicTransaction(proberID,command,NULL,messageReceived);
    }

    if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceived,"MC",2)!=0) )
    {
        if ( proberID->p.proberFamily == EG_PROBER_VISION_SERIES )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "clear learn list command failed ");
        }
        else
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "clear learn list command failed: check recipe file name \"%s\" has been set up.",
                proberID->p.recipeFileName);
        }

        rtnValue=PHPFUNC_ERR_ANSWER;
    }


    for ( i=0 ; i< count && rtnValue==PHPFUNC_ERR_OK  ; i++ )
    {
        if ( proberID->p.proberFamily == EG_PROBER_VISION_SERIES )
        {
            /* send command ADXnYn add die to learn list */ 
            sprintf(command,"ADX%ldY%ld",dieX[i],dieY[i]);
            rtnValue=basicTransaction(proberID,command,NULL,messageReceived);
        }
        else /* COMMANDER SERIES */
        {
            /* send command $LSAD+recipe file+XnYn 
               add dice to learn list */
            sprintf(command,"$LSAD\"%s\"X%ldY%ld",proberID->p.recipeFileName,dieX[i],dieY[i]);
            rtnValue=basicTransaction(proberID,command,NULL,messageReceived);
        }

        if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceived,"MC",2)!=0) )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "add die to learn list failed ");
            rtnValue=PHPFUNC_ERR_ANSWER;
        }
    }

    /* 
     *  Following command and comment is written in Jeff Morton's 
     *  phDrvs/sources/egSeries/ph_lib/driver.c downloadStepPattern()
     *  can't find this command in manual 
     *  set learn list operation - need to be changed to row 
     */
    if ( proberID->p.proberFamily == EG_COMMANDER_SERIES )
    {
        if ( rtnValue == PHPFUNC_ERR_OK )
        {
            /* Select probe mode SM4Pn n==4- Learn */
            rtnValue=basicTransaction(proberID,"SM4P4",NULL,messageReceived);
        }
    }
    return completeNormalTransactions(proberID,rtnValue);
}
#endif



#ifdef SUBDIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send sub-die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * Description: Taken from Jeff Morton's Electoglas driver software
 * Jeff/phDvrs/sources/egSeries/ph_lib/driver.c downloadMicroPattern()
 *
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
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    int i;
    char command[EG_MAX_MESSAGE_SIZE];

    rtnValue=initializeNormalTransactions(proberID);

    /* if cannot initialize transactions return now */
    if ( rtnValue != PHPFUNC_ERR_OK )
    {
        return completeNormalTransactions(proberID,rtnValue);
    } 

    /* save subdie list parameters */
    proberID->p.subdieListCount=count;
    proberID->p.subdieListX=subdieX;
    proberID->p.subdieListY=subdieY;

    for ( i=0 ; i< count ; i++ )
    {
        phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
            "subdie list X= %ld Y= %ld ",
            subdieX[i],
            subdieY[i]);
    }

    /* 
     *  send SM35B1 - SM35Bn enable microprobing n=0 disable n=1 enable
     *  send RF     - Clear Micro List
     *
     *  FOR each entry in the subdie list
     *      "FAXxYySn" Add site to micro list
     *                 xy = +/- 32767 XY coordinates
     *                 n  = 1-126 Microsite number
     *      Units 0.1 mil/1 micron
     */

    /* enable microprobing */
    rtnValue=basicTransaction(proberID,"SM35B1",NULL,messageReceived);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        /* clear micro list */
        rtnValue=basicTransaction(proberID,"RF",NULL,messageReceived);

        if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceived,"MC",2)!=0) )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "clear micro list \"RF\" command failed ");
            rtnValue=PHPFUNC_ERR_ANSWER;
        }
    }

    for ( i=0 ; i< count && rtnValue==PHPFUNC_ERR_OK  ; i++ )
    {
        /* send command FAXxYySn add site to micro list */ 
        sprintf(command,"FAX%ldY%ldS%d",subdieX[i],subdieY[i],i+1);
        rtnValue=basicTransaction(proberID,command,NULL,messageReceived);

        if ( rtnValue==PHPFUNC_ERR_OK && (strncmp(messageReceived,"MC",2)!=0) )
        {
            phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                "add site to micro list failed ");
            rtnValue=PHPFUNC_ERR_ANSWER;
        }
    }

    return completeNormalTransactions(proberID,rtnValue);
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
 ***************************************************************************/
phPFuncError_t privateReprobe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    static char messageBuffer[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    rtnValue=initializeTransactions(proberID);

    /* if cannot initialize transactions return now */
    if ( rtnValue != PHPFUNC_ERR_OK )
    {
        return completeNormalTransactions(proberID,rtnValue);
    } 

    /* 
     *  Make sure there are no pending messages to read.
     */
    rtnValue=getEventMsgTransaction(proberID,0L,NULL,messageBuffer);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        rtnValue=proberReprobeDie(proberID);
    }

    return completeTransactions(proberID,rtnValue);
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
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    rtnValue=initializeTransactions(proberID);

    /* if cannot initialize transactions return now */
    if ( rtnValue != PHPFUNC_ERR_OK )
    {
        return completeNormalTransactions(proberID,rtnValue);
    } 

    /* send CP command clean probe tips */
    rtnValue=basicTransaction(proberID,"CP",NULL,messageReceived);

    return completeTransactions(proberID,rtnValue);
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
    return PHPFUNC_ERR_NA;
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
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    int paused;
    /* 
     *  Doesn't make sense to pause a prober when smartest is
     *  in control.
     */
    if ( proberID->p.stopProberOnSmartestPause == TRUE )
    {
        /* check to see if already paused */
        phEstateAGetPauseInfo(proberID->myEstate,&paused);

        if ( !paused )
        {
            if ( proberID->p.stepMode == STEPMODE_EXPLICIT )
            {
                /* 
                 *  Doesn't make sense to pause a prober when smartest is
                 *  in control.
                 */
                phEstateASetPauseInfo(proberID->myEstate,1);
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "Prober not yet paused try to force a pause ");

                rtnValue=initializePausedTransactions(proberID);

                completePausedTransactions(proberID,rtnValue);
            }
        }
    }

    return rtnValue;
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
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
    int paused;

    /* 
     *  Doesn't make sense to pause a prober when smartest is
     *  in control.
     */
    if ( proberID->p.stopProberOnSmartestPause == TRUE )
    {
        /* check to see if already paused */
        phEstateAGetPauseInfo(proberID->myEstate,&paused);

        if ( paused )
        {
            if ( proberID->p.stepMode == STEPMODE_EXPLICIT )
            {
                /* 
                 *  Doesn't make sense to pause a prober when smartest is
                 *  in control.
                 */
                phEstateASetPauseInfo(proberID->myEstate,0);
            }
            else
            {
                phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
                    "Prober is paused try to force run ");

                rtnValue=initializePausedTransactions(proberID);
    
                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    rtnValue=forceRun(proberID);
                }

                if ( rtnValue == PHPFUNC_ERR_OK )
                {
                    proberID->p.pluginState=OUT_OF_PAUSE;
                }

                completePausedTransactions(proberID,rtnValue);
            }
        }
    }

    return rtnValue;
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
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    rtnValue=initializeTransactions(proberID);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        rtnValue=basicTransaction(proberID,command,NULL,messageReceived);
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
            "Test command \"%s\" sent, answer returned \"%s\" ",
            command,
            messageReceived);

            sprintf(proberID->p.egDiagnosticString,
                    "Test command \"%s\" sent, answer returned \"%s\" ",
                    command,
                    messageReceived);
    }

    return completeTransactions(proberID,rtnValue);
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
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    if ( *proberID->p.egDiagnosticString )
    {
        *diag=proberID->p.egDiagnosticString;
    }
    else
    {
        updateTransactionLog(proberID);
        *diag=proberID->p.egTransactionLog;
    }

    return rtnValue;
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
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    const char *actionString=0;
/* End of Huatek Modification */

    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE,
        "privateStatus(P%p, 0x%08lx, P%p)", proberID, (long) action, lastCall);

    switch (action)
    {
      case PHPFUNC_SR_QUERY:
        actionString="QUERY";
        break;
      case PHPFUNC_SR_RESET:
        actionString="RESET";
        proberID->p.egTransactionState=EG_TRANSACTIONS_RESET;
        break;
      case PHPFUNC_SR_HANDLED:
        actionString="HANDLED";
        proberID->p.egTransactionState=EG_TRANSACTIONS_HANDLED;
        break;
      case PHPFUNC_SR_ABORT:
        actionString="ABORT";
        proberID->p.egTransactionState=EG_TRANSACTIONS_ABORTED;
        break;
    }

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
        "Status plugin is informed %s ",
        actionString);

    return rtnValue;
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
    return PHPFUNC_ERR_NA;
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
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
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
    static char egWaferId[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    rtnValue=initializeNormalTransactions(proberID);

    /* send "?W0" Request wafer ID */
    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        rtnValue=basicTransaction(proberID,"?W0",NULL,egWaferId);
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        if (strlen(egWaferId) >= strlen(EOC) &&
            strcmp(&egWaferId[strlen(egWaferId) - strlen(EOC)], EOC) == 0)
                egWaferId[strlen(egWaferId) - strlen(EOC)] = '\0';
        *wafIdString = egWaferId;
    }

    return completeNormalTransactions(proberID,rtnValue);
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
    /* by default not implemented */
    return PHPFUNC_ERR_NA;
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
    static char messageReceived[EG_MAX_MESSAGE_SIZE];
    phPFuncError_t rtnValue=PHPFUNC_ERR_OK;

    /* Don't keep sending ID */
    /* proberID->p.egTransactionState=EG_TRANSACTIONS_RESET; */

    rtnValue=initializeTransactions(proberID);

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        rtnValue=basicTransaction(proberID,"ID",NULL,messageReceived);
    }

    if ( rtnValue == PHPFUNC_ERR_OK )
    {
        *testPassed=TRUE;
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
            "Communication test: PASSED \n"
            "ID request sent answer returned \"%s\" ",
            messageReceived);

        sprintf(proberID->p.egDiagnosticString,
                "Commmunication test: ID request send, answer returned \"%s\"",
                messageReceived);
    }
    else
    {
        *testPassed=FALSE;
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_MESSAGE_0,
            "Communication test: FAILED \n");

            sprintf(proberID->p.egDiagnosticString,
                    "Communication test: error occurred\n");
    }
    return completeTransactions(proberID,rtnValue);
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
    {PHKEY_NAME_PROBER_STATUS_CENTER_X,     "?I"},
    {PHKEY_NAME_PROBER_STATUS_CENTER_Y,     "?I"},
    {PHKEY_NAME_PROBER_STATUS_DIE_HEIGHT,   "?SP29"},
    {PHKEY_NAME_PROBER_STATUS_DIE_WIDTH,    "?SP29"},
    {PHKEY_NAME_PROBER_STATUS_POS_X,        "?SM11"},
    {PHKEY_NAME_PROBER_STATUS_POS_Y,        "?SM11"},
    {PHKEY_NAME_PROBER_STATUS_WAFER_FLAT,   "?SM3"},
    {PHKEY_NAME_PROBER_STATUS_WAFER_SIZE,   "?SP4"},
    {PHKEY_NAME_PROBER_STATUS_WAFER_UNITS,  "?SM1"}
  };

  static char response[MAX_STATUS_MSG_LEN] = "";
  char buffer[MAX_STATUS_MSG_LEN] = "";
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

  retVal = initializeTransactions(proberID);
  if( retVal != PHPFUNC_ERR_OK ) {
    return completeTransactions(proberID, retVal);
  }

  /* 
  * the user may query several parameter one time in "ph_get_status", e.g.
  *     *wafer_size_and_wafer_flat = PROB_HND_CALL(ph_get_stauts wafer_size wafer_flat);
  */
  strcpy(response, "");
  while ( token != NULL ) {
    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_TRACE, 
                     "phFuncGetStatus, token = ->%s<-", token);

    pStrPair = phBinSearchStrValueByStrKey(sGpibCommands, LENGTH_OF_ARRAY(sGpibCommands), token);
    if( pStrPair != NULL ) {
      strcpy(buffer, "");
      retVal = basicTransaction(proberID, pStrPair->value, NULL, buffer);
      if (retVal != PHPFUNC_ERR_OK) {
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                         "basicTransaction return FAILURE for query ->%s<-", pStrPair->value); 
        return completeTransactions(proberID,retVal);
      }

      /*
      * here, anything is OK, the query was sent out and the response was received.
      * We need further handle the response, for example:
      *    buffer = "SM11Q1", when we request the "pos_x" (positive direction of X)
      *  after analysis,
      *    result = "Right->Left"   
      */
      if ( analysisResultOfGetStatus(pStrPair->key, buffer, result) != SUCCEED ){
        phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
                         "The response = ->%s<- from the Prober cannot be correctly parsed\n" 
                         "This could be an internal error from either Prober or Driver", buffer);
        /* but we still keep the result though it is not parsed correctly*/
        strcpy(result,buffer);
      }

      /* until here, the "result" contains the parsed response from Prober */
    } else {
      phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
                       "The key \"%s\" is not available, or may not be supported yet", token);
      sprintf(result, "%s_KEY_NOT_AVAILABLE", token);
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

  return completeTransactions(proberID, retVal);
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
    return PHPFUNC_ERR_NA;
}
#endif


/*****************************************************************************
 * End of file
 *****************************************************************************/
