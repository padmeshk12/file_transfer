/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_diag.c
 * CREATED  : 27 Sep 2000
 *
 * CONTENTS : gpib diagnostics session for interactively sending and
 *            receiving messages and events.
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 27 Sep 2000, Chris Joyce, taken diagnostics/gpibTest/sim.c as
 *                                      starting point.
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_GuiServer.h"
#include "ph_diag.h"
#include "ph_diag_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define END_OF_COMMAND "\r\n"

#define DIAG_SESSION_CHECK

#ifdef DIAG_SESSION_CHECK
#define CheckSession(x) \
    if (!(x) || (x)->myself != (x)) \
        return DIAG_ERR_INVALID_HDL
#else
#define CheckSession(x)
#endif


/*--- global variables ------------------------------------------------------*/

/*
 * ATTENTION! the values of the array r_rec_msg_array and r_rec_srq_array 
 * must follow the order of those defined in ph_diag_private.h file 
 * recMsgMode_t recSrqMode_t.
 */

static const char *r_rec_msg_array[] = { "r_rec_msg_button",
                                         "r_rec_msg_afterSRQ",
                                         "r_rec_msg_afterSend",
                                         "r_rec_msg_always",
                                         NULL };

static const char *r_rec_srq_array[] = { "r_rec_srq_button",
                                         "r_rec_srq_afterSend",
                                         "r_rec_srq_always",
                                         NULL };

static const char *gpibDiagGuiDesc = \
    "S:`diag_test`:`GPIB Communication Test` " \
        "{@v " \
            "S:``:``{@v " \
                "\t S::{@h  f:`f_send`:`Message`:    p:`p_send`:`Send`[`Try to send a message`] } \n" \
                "\t S::{@h  f:`f_answ`:`Answer`:     p:`p_rec_msg`:`Receive`[`Try to receive an answer`] } \n" \
                "\t S::{@h  f:`f_srq`:`SRQ`:         p:`p_rec_srq`:`Receive`[`Try to receive an SRQ`] } \n" \
                "\t S::{@h  f:`f_status`:`STATUS`:                           } \n" \
                   "} "  \
                     "\t S:``:`Send Message Set-up`{@v " \
                         " o:`o_send_msg_timeout`:`Timeout` " \
                                        "  (`1 second`      " \
                                        "   `3 seconds`     " \
                                        "   `10 seconds`    " \
                                        "   `60 seconds`):%d[`How long to wait for trying to send a message`] " \
                     " } \n" \
                     "\t S:``:`Receive Answer Set-up`{@v " \
                         " R::{  " \
                         "   `r_rec_msg_button`    :`receive answer only on <Receive> button press` " \
                         "   `r_rec_msg_afterSRQ`  :`automatically after receiving an SRQ` " \
                         "   `r_rec_msg_afterSend` :`automatically after sending a message` " \
                         "   `r_rec_msg_always`    :`receive answer continuously`  " \
                         " }:%d:1  \n" \
                         "\t o:`o_rec_msg_timeout`:`Timeout`  " \
                         "                 (`1 second`  " \
                         "                  `3 seconds` " \
                         "                  `10 seconds`  " \
                         "                  `60 seconds`):%d[`How long to wait for trying to receiving an answer`] " \
                     "} "   \
                     "\t S::`Receive SRQ Set-up`{@v  " \
                     "    R::{ " \
                     "        `r_rec_srq_button`    :`receive SRQ only on <Receive> button press`  " \
                     "        `r_rec_srq_afterSend` :`automatically after sending a message`      " \
                     "        `r_rec_srq_always`    :`receive SRQ continuously`    " \
                     "    }:%d:1  \n" \
                     "\t  o:`o_rec_srq_timeout`:`Timeout`   " \
                     "                     (`1 second`      " \
                     "                      `3 seconds`     " \
                     "                      `10 seconds`    " \
                     "                      `60 seconds`):%d[`How long to wait to try to receive an SRQ`] " \
                     "} \n" \
                     "\t S::{@h " \
                     "    o:`o_debug_level`:`Set Debug Level`  " \
                     "                     (`debug -1`        " \
                     "                      `debug 0`           " \
                     "                      `debug 1`           " \
                     "                      `debug 2`           " \
                     "                      `debug 3`           " \
                     "                      `debug 4`):%d[`Set debug level for UI report window`] " \
                     "} \n " \
          "  S::{@h  *p:`p_close`:`%s`[`%s`]  } " \
    "} ";


/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * Convert timeout GPIB Diagnostic GUI position
 *
 *****************************************************************************/
static int convertTimeoutToOptionMenuPosition(int timeout)
{
    int position=1;

    switch ( timeout )
    {
      case 1:
        position=1;
        break;
      case 3:
        position=2;
        break;
      case 10:
        position=3;
        break;
      case 60:
        position=4;
        break;
    }
    
    return position;
} 

/*****************************************************************************
 *
 * Convert debug GPIB Diagnostic GUI position
 *
 *****************************************************************************/
static int convertDebugToOptionMenuPosition(int debug)
{
    /* debug     position */
    /*   -1         1     */ 
    /*    0         2     */ 
    /*    1         3     */ 
    /*    2         4     */ 
    /*    3         5     */ 
    /*    4         6     */ 

    return debug+2;
} 

/*****************************************************************************
 *
 * Remove EOC characters from end of string
 *
 *****************************************************************************/
static void removeEoc(char *string)
{
    int lastChar;
    int eocChar=TRUE;

    while ( eocChar )
    {
       lastChar=strlen(string)-1;

       if ( lastChar >= 0 )
       {
           eocChar=string[lastChar]=='\n' || string[lastChar]=='\r';
       }
       else
       {
           eocChar=FALSE;
       }

       if ( eocChar )
       {
           string[lastChar]='\0';
       }

    }
}


/*****************************************************************************
 *
 * Get Debug Level from Log Mode
 *
 *****************************************************************************/
static int getDebugFromLogMode(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
    int debug;
    phLogMode_t modeFlags;

    phLogGetMode(diagID->loggerId,&modeFlags);

    if ( modeFlags & PHLOG_MODE_DBG_0 )
    {
        debug=0;
    }
    else if ( modeFlags & PHLOG_MODE_DBG_1 )
    {
        debug=1;
    }
    else if ( modeFlags & PHLOG_MODE_DBG_2 )
    {
        debug=2;
    }
    else if ( modeFlags & PHLOG_MODE_DBG_3 )
    {
        debug=3;
    }
    else if ( modeFlags & PHLOG_MODE_DBG_4 )
    {
        debug=4;
    }
    else
    {
        debug=-1;
    }
   
    return debug;
}


/*****************************************************************************
 *
 * Set Log Mode from Debug Level 
 *
 *****************************************************************************/
static void setLogModeDebugLevel(
    phDiagId_t diagID             /* gpib diagnostic ID */,
    int        debugLevel         /* new debug level    */
)
{
    phLogMode_t modeFlags;

    /* get current log mode and remove all debug flags */
    phLogGetMode(diagID->loggerId, &modeFlags);
    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_0);
    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_1);
    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_2);
    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_3);
    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_4);

    switch (debugLevel)
    {
      case -1:
        /* quiet */
        break;
      case 0:
        modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_0);
        break;
      case 1:
        modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_1);
        break;
      case 2:
        modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_2);
        break;
      case 3:
        modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_3);
        break;
      case 4:
        modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_4);
        break;
      default:
        /* incorrect value, do debug level 1 by default */
        modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_1);
        break;
    }

    /* replace log mode */
    phLogSetMode(diagID->loggerId, modeFlags);
}

/*****************************************************************************
 *
 * Check GUI error and convert into a diagnostic error.
 *
 *****************************************************************************/

static  phDiagError_t checkGuiError(
    phDiagId_t diagID             /* gpib diagnostic ID */,
    phGuiError_t guiError         /* status from gui call */
)
{
    phDiagError_t rtnValue;

    switch (guiError)
    {
      case PH_GUI_ERR_OK:
        /* no error */
        rtnValue=DIAG_ERR_OK;
	break;
      case PH_GUI_ERR_TIMEOUT:
#if DEBUG
	phLogDiagMessage(diagID->loggerId, LOG_VERBOSE,
	    "received timeout error from gui server");
#endif
        rtnValue=DIAG_ERR_GUI_TIMEOUT;
	break;
      default:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "received error code %d from gui server",
            (int) guiError);
        rtnValue=DIAG_ERR_GUI;
        break;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Check Communication error and convert into a diagnostic error.
 *
 *****************************************************************************/

static  phDiagError_t checkComError(
    phDiagId_t diagID             /* gpib diagnostic ID */,
    phComError_t comError         /* status from comm call */
)
{
    phDiagError_t rtnValue;

    switch (comError)
    {
      case PHCOM_ERR_OK:
        /* no error */
        rtnValue=DIAG_ERR_OK;
	break;
      case PHCOM_ERR_TIMEOUT:
	phLogDiagMessage(diagID->loggerId, LOG_VERBOSE,
	    "received timeout error from communication layer");
        rtnValue=DIAG_ERR_COMM_TIMEOUT;
	break;
      case PHCOM_ERR_UNKNOWN_DEVICE:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "interface device is unknown to communication layer");
        rtnValue=DIAG_ERR_COMM;
        break;
      case PHCOM_ERR_SYNTAX:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "communication layer syntax error");
        rtnValue=DIAG_ERR_COMM;
        break;
      case PHCOM_ERR_VERSION:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "communication layer version error");
        rtnValue=DIAG_ERR_COMM;
        break;
      case PHCOM_ERR_NO_CONNECTION:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "communication layer connection error");
        rtnValue=DIAG_ERR_COMM;
        break;
      case PHCOM_ERR_GPIB_INVALID_INST:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "communication layer unable to initialize gpib");
        rtnValue=DIAG_ERR_COMM;
        break;
      case PHCOM_ERR_GPIB_ICLEAR_FAIL:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "communication layer gpib call iclear() failed");
        rtnValue=DIAG_ERR_COMM;
        break;
      case PHCOM_ERR_GPIB_INCOMPLETE_SEND:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "message sent over GPIB was sent incomplete");
        rtnValue=DIAG_ERR_COMM;
        break;
      default:
        phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
            "received error code %d from communication layer",
            (int) comError);
        rtnValue=DIAG_ERR_COMM;
        break;
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Return Diagnostic module status string
 *
 *****************************************************************************/

static const char *getStatusString(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/07/2001 */
/* Issue Number: 274 */
    const char *statusStr=NULL;   /* string of current status */ 
/* End of Huatek Modification  */

    switch ( diagID->status )
    {
      case OK:
        statusStr = "OK";
        break;
      case WAITING_FOR_SRQ:
        statusStr = "Waiting to receive SRQ ...";
        break;
      case WAITING_FOR_MSG:
        statusStr = "Waiting to receive message ...";
        break;
      case TRYING_TO_SEND:
        statusStr = "Trying to send message ...";
        break;
      case TIMEOUT:
        statusStr = "TIMED OUT";
        break;
      case ERROR:
        statusStr = "error, see log";
        break;
    }

    return statusStr;
}




/*****************************************************************************
 *
 * Display Diagnostic module status 
 *
 *****************************************************************************/

static phDiagError_t displayStatus(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;
    char status[DIAG_MAX_MESSAGE_LENGTH];

    strcpy(status,getStatusString(diagID));
    guiError = phGuiChangeValue(diagID->guiId, "f_status", status);
    rtnValue=checkGuiError( diagID, guiError );
   
    return rtnValue;
}

static phDiagError_t setAndDisplayStatus(
    phDiagId_t diagID             /* gpib diagnostic ID */,
    gpibDiagnosticStatus_t s      /* current status     */
)
{
    diagID->status=s;
    return displayStatus(diagID);
}





/*****************************************************************************
 *
 * Get GUI int data
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce
 *
 * Description:
 * Gets GUI data string for componentName and converts to integer according
 * to format.
 *
 *****************************************************************************/
static phDiagError_t getGuiDataInt(
    phDiagId_t diagID             /* gpib diagnostic ID */,
    const char *componentName     /* name of the component */,
    const char *format            /* format of returned string */,
    int *value                    /* int value to set */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;

    int timeout_getGuiData=1; 
    char response[DIAG_MAX_MESSAGE_LENGTH];
    char cmptName[DIAG_MAX_MESSAGE_LENGTH];

    strcpy(cmptName,componentName);

    guiError = phGuiGetData(diagID->guiId, cmptName, response, timeout_getGuiData);
    rtnValue=checkGuiError(diagID,guiError);

    if ( rtnValue == DIAG_ERR_OK )
    {
        if ( sscanf(response,format,value) != 1 )
        {
            phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
                "unable to interpret gui server return value \"%s\" with sscanf \n"
                "format expected %s ",
                response,format);
        }
        else
        {
#if DEBUG
            phLogDiagMessage(diagID->loggerId, LOG_VERBOSE,
                "interpretted component %s returned value \"%s\" as %d \n",
                componentName,response,*value);
#endif
        }
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Get GUI BOOL data
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce
 *
 * Description:
 * Gets GUI data string for componentName and converts to integer according
 * to format.
 *
 *****************************************************************************/
static phDiagError_t getGuiDataBool(
    phDiagId_t diagID             /* gpib diagnostic ID */,
    const char *componentName     /* name of the component */,
    int *value                    /* int value to set */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;

    int timeout_getGuiData=1; 
    char response[DIAG_MAX_MESSAGE_LENGTH];
    char cmptName[DIAG_MAX_MESSAGE_LENGTH];

    strcpy(cmptName,componentName);

    guiError = phGuiGetData(diagID->guiId, cmptName, response, timeout_getGuiData);
    rtnValue=checkGuiError(diagID,guiError);

    if ( rtnValue == DIAG_ERR_OK )
    {
        if ( strcasecmp(response, "true") == 0)
        {
            *value=TRUE;
#if DEBUG
            phLogDiagMessage(diagID->loggerId, LOG_VERBOSE,
                "interpretted component %s returned value \"%s\" as %d \n",
                componentName,response,*value);
#endif
        }
        else if( strcasecmp(response, "false") == 0)
        {
            *value=FALSE;
#if DEBUG
            phLogDiagMessage(diagID->loggerId, LOG_VERBOSE,
                "interpretted component %s returned value \"%s\" as %d \n",
                componentName,response,*value);
#endif
        }
        else
        {
            phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
                "unable to interpret gui server return value \"%s\" \n"
                "format expected true | false ",
                response);
        }
    }

    return rtnValue;
}



/*****************************************************************************
 *
 * Clear fields
 *
 * Authors: Chris Joyce
 *
 * Description:
 *
 *
 *****************************************************************************/
static phDiagError_t clearSrqField(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;

    guiError = phGuiChangeValue(diagID->guiId, "f_srq", "");
    rtnValue = checkGuiError(diagID,guiError);

    return rtnValue;
}

static phDiagError_t clearAnswField(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;

    guiError = phGuiChangeValue(diagID->guiId, "f_answ", "");
    rtnValue = checkGuiError(diagID,guiError);

    return rtnValue;
}




/*****************************************************************************
 *
 * Get and display communication Event
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce, taken from diagnostics/gpibTest/sim.c
 *
 * Description:
 *
 * Keeps calling phComGpibGetEvent until either an error occurs or no more
 * events pending.
 *
 * SRQs found are dispayed on GUI
 *
 *****************************************************************************/
static phDiagError_t getCommEvent(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;
    phComError_t comError;

    phComGpibEvent_t *comEvent;
    int pending=1;
    char srq[DIAG_MAX_MESSAGE_LENGTH];

    setAndDisplayStatus(diagID,WAITING_FOR_SRQ);


    comError = phComGpibGetEvent(diagID->communicationId, 
                                 &comEvent, 
                                 &pending, 
                                 diagID->recSRQTimeout * 1000000L );
    rtnValue = checkComError( diagID, comError );

    switch ( rtnValue )
    {
      case DIAG_ERR_OK:
        if ( comEvent == NULL )
        {
             phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_ERROR,
                               "unexpected null event received from communication layer");
             setAndDisplayStatus(diagID,ERROR);
        }
        else if ( comEvent->type != PHCOM_GPIB_ET_SRQ )
        {
            phLogDiagMessage(diagID->loggerId, LOG_DEBUG,
                "event received but not an SRQ ignore %d",
                comEvent->type);
             setAndDisplayStatus(diagID,ERROR);
        }
        else
        {
            sprintf(srq, "SRQ received 0x%02x ", (int) comEvent->d.srq_byte);
            guiError = phGuiChangeValue(diagID->guiId, "f_srq", srq);
            rtnValue = checkGuiError(diagID,guiError);
            phLogDiagMessage(diagID->loggerId, LOG_NORMAL,
                              "SRQ received 0x%02x", comEvent->d.srq_byte);
            setAndDisplayStatus(diagID,OK);
        }
        break;
      case DIAG_ERR_COMM_TIMEOUT:
#ifdef DEBUG
        phLogDiagMessage(diagID->loggerId, LOG_VERBOSE,
                          "time out received while trying to receive event ");
#endif
        setAndDisplayStatus(diagID,TIMEOUT);
        break;
      default:
        setAndDisplayStatus(diagID,ERROR);
        break;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Get and display communication Message
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce, taken from diagnostics/gpibTest/sim.c
 *
 * Description:
 *
 * Keeps calling phComGpibGetEvent until either an error occurs or no more
 * events pending.
 *
 *****************************************************************************/
static phDiagError_t getCommMessage(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/07/2001 */
/* Issue Number: 315 */
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;
    phComError_t comError;

    const char *message = "";
    int length = 0;
    char request[DIAG_MAX_MESSAGE_LENGTH];
/*    char requestLog[DIAG_MAX_MESSAGE_LENGTH];*/
/* End of Huatek Modification */
    setAndDisplayStatus(diagID, WAITING_FOR_MSG);

    comError = phComGpibReceive(diagID->communicationId, 
                                 &message, 
                                 &length, 
                                 diagID->recMsgTimeout * 1000000L );
    rtnValue = checkComError( diagID, comError );

    switch ( rtnValue )
    {
      case DIAG_ERR_OK:
        strcpy(request, message);
        removeEoc(request);
        guiError = phGuiChangeValue(diagID->guiId, "f_answ", request);
        rtnValue = checkGuiError( diagID, guiError );
        if ( rtnValue == DIAG_ERR_OK && *request )
        {
            phLogDiagMessage(diagID->loggerId, LOG_NORMAL,
                              "message received \"%s\"", request);
        }
        setAndDisplayStatus(diagID, OK);
        break;
      case DIAG_ERR_COMM_TIMEOUT:
#ifdef DEBUG
        phLogDiagMessage(diagID->loggerId, LOG_VERBOSE,
                   "received timeout error while waiting for message");
#endif
        setAndDisplayStatus(diagID, TIMEOUT);
        break;
      default:
        setAndDisplayStatus(diagID, ERROR);
        break;
    }

    return rtnValue;
}


/*****************************************************************************
 *
 * Send displayed communication Message
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce, taken from diagnostics/gpibTest/sim.c
 *
 * Description:
 *
 * Keeps calling phComGpibGetEvent until either an error occurs or no more
 * events pending.
 *
 *****************************************************************************/
static phDiagError_t sendCommMessage(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/07/2001 */
/* Issue Number: 315 */
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;
    phComError_t comError;

    char request[DIAG_MAX_MESSAGE_LENGTH];
/*    char requestLog[DIAG_MAX_MESSAGE_LENGTH];*/
/* End of Huatek Modification */
    guiError = phGuiGetData(diagID->guiId, "f_send", request, 5);
    rtnValue = checkGuiError( diagID, guiError );

    if ( rtnValue == DIAG_ERR_OK )
    {
        phLogDiagMessage(diagID->loggerId, LOG_NORMAL,
            "trying to send message \"%s\"", request);
        strcat(request, END_OF_COMMAND);

        setAndDisplayStatus(diagID, TRYING_TO_SEND);
        comError = phComGpibSend(diagID->communicationId, 
                                 request, 
                                 strlen(request), 
                                 diagID->sendMsgTimeout * 1000000L );
        rtnValue = checkComError( diagID, comError );

        switch ( rtnValue )
        {
          case DIAG_ERR_OK:
            guiError = phGuiChangeValue(diagID->guiId, "f_send", "");
            checkGuiError(diagID,guiError);
            setAndDisplayStatus(diagID, OK);
            break;
          case DIAG_ERR_COMM_TIMEOUT:
#ifdef DEBUG
            phLogDiagMessage(diagID->loggerId, LOG_VERBOSE,
                              "received timeout error while trying to send message ");
#endif
            setAndDisplayStatus(diagID, TIMEOUT);
            break;
          default:
            setAndDisplayStatus(diagID, ERROR);
            break;
        }
    }
    return rtnValue;
}


/*****************************************************************************
 *
 * Get gui event
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce, taken from diagnostics/gpibTest/sim.c
 *
 * Description:
 *
 * Keeps calling phComGpibGetEvent until either an error occurs or no more
 * events pending.
 *
 *****************************************************************************/
static phDiagError_t getGuiEventAndData(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/07/2001 */
/* Issue Number: 315 */
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;

    phGuiEvent_t guiEvent;
    int timeout_getGuiEvent=1; 
/*    char response[DIAG_MAX_MESSAGE_LENGTH];*/

    int boolValue;
    int arrayCounter;
    recSrqMode_t previousRecSrqMode;
    recMsgMode_t previousRecMsgMode;
/* End of Huatek Modification */

    /* get all gui events on event queue */
    while ( rtnValue == DIAG_ERR_OK && diagID->quit==FALSE )
    {
        guiError = phGuiGetEvent(diagID->guiId, &guiEvent, timeout_getGuiEvent);
        rtnValue=checkGuiError(diagID,guiError);

        if ( rtnValue == DIAG_ERR_OK )
        {
            timeout_getGuiEvent=0; 

            switch (guiEvent.type)
            {
              case PH_GUI_NO_EVENT:
    	        break;
              case PH_GUI_NO_ERROR:
                break;
              case PH_GUI_COMTEST_RECEIVED:
                break;
              case PH_GUI_IDENTIFY_EVENT:
                break;
              case PH_GUI_ERROR_EVENT:
                break;
              case PH_GUI_PUSHBUTTON_EVENT:
              case PH_GUI_EXITBUTTON_EVENT:
                if (strcmp(guiEvent.name, "p_close") == 0)
                {
                    diagID->quit=TRUE; 
                }
                else if(strcmp(guiEvent.name, "p_send") == 0)
                {
                    rtnValue=clearAnswField(diagID);
                    if ( rtnValue == DIAG_ERR_OK )
                    {
                        rtnValue=clearSrqField(diagID);
                    }
                    diagID->sendMsgGuiRequest=TRUE;
                }
                else if(strcmp(guiEvent.name, "p_rec_msg") == 0)
                {
                    rtnValue=clearAnswField(diagID);
                    diagID->getMsgGuiRequest=TRUE;
                }
                else if(strcmp(guiEvent.name, "p_rec_srq") == 0)
                {
                    rtnValue=clearSrqField(diagID);
                    diagID->getSrqGuiRequest=TRUE;
                }
            }
        }
    } 

    /* get send message set-up data: sendMsgTimeout */
    if ( rtnValue == DIAG_ERR_OK  || rtnValue == DIAG_ERR_GUI_TIMEOUT )
    {
        rtnValue=getGuiDataInt(diagID,"o_send_msg_timeout","%d",&diagID->sendMsgTimeout);
    }

    /* get receive message set-up data: recMsgMode */
    rtnValue=DIAG_ERR_OK;
    boolValue=FALSE;
    arrayCounter=0;
    previousRecMsgMode=diagID->recMsgMode;
    while ( rtnValue == DIAG_ERR_OK &&
            r_rec_msg_array[arrayCounter] &&
            boolValue==FALSE )
    {
        rtnValue=getGuiDataBool(diagID,r_rec_msg_array[arrayCounter],&boolValue);

        if ( rtnValue==DIAG_ERR_OK && boolValue )
        {
            diagID->recMsgMode=(recMsgMode_t)arrayCounter;
        }
        ++arrayCounter;
    }
    diagID->recMsgModeChanged=(previousRecMsgMode!=diagID->recMsgMode);

    /* get receive message set-up data: recMsgTimeout */
    if ( rtnValue == DIAG_ERR_OK  || rtnValue == DIAG_ERR_GUI_TIMEOUT )
    {
        rtnValue=getGuiDataInt(diagID,"o_rec_msg_timeout","%d",&diagID->recMsgTimeout);
    }

    /* get receive srq set-up data: recSRQTimeout */
    rtnValue=DIAG_ERR_OK;
    boolValue=FALSE;
    arrayCounter=0;
    previousRecSrqMode=diagID->recSrqMode;
    while ( rtnValue == DIAG_ERR_OK &&
            r_rec_srq_array[arrayCounter] &&
            boolValue==FALSE )
    {
        rtnValue=getGuiDataBool(diagID,r_rec_srq_array[arrayCounter],&boolValue);

        if ( rtnValue==DIAG_ERR_OK && boolValue )
        {
            diagID->recSrqMode=(recSrqMode_t)arrayCounter;
        }
        ++arrayCounter;
    }
    diagID->recSrqModeChanged=(previousRecSrqMode!=diagID->recSrqMode);


    if ( rtnValue == DIAG_ERR_OK  || rtnValue == DIAG_ERR_GUI_TIMEOUT )
    {
        rtnValue=getGuiDataInt(diagID,"o_rec_srq_timeout","%d",&diagID->recSRQTimeout);
    }

    /* get debug level */
    if ( rtnValue == DIAG_ERR_OK  || rtnValue == DIAG_ERR_GUI_TIMEOUT )
    {
        rtnValue=getGuiDataInt(diagID,"o_debug_level","debug %d",&diagID->debug);
        setLogModeDebugLevel(diagID,diagID->debug);
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Initialize diagnostic variables
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce, taken from diagnostics/gpibTest/sim.c
 *
 * Description:
 *
 *
 *****************************************************************************/
static phDiagError_t diagInitialize(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;

    diagID->status=OK;
    diagID->quit=FALSE;
    strcpy(diagID->textLog,"");

    diagID->sendMsgGuiRequest=FALSE;
    diagID->getSrqGuiRequest=FALSE;
    diagID->getMsgGuiRequest=FALSE;
    diagID->getMSG=FALSE;
    diagID->getSRQ=FALSE;

    return rtnValue;
}


/*****************************************************************************
 *
 * Communication Loop
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce, taken from diagnostics/gpibTest/sim.c
 *
 * Description:
 *
 * Loops continuously until all requests have been answered.
 *
 *****************************************************************************/
static phDiagError_t communicationLoop(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;

    if ( diagID->sendMsgGuiRequest )
    {
        rtnValue=sendCommMessage(diagID);
        diagID->sendMsgGuiRequest=FALSE;

        if ( rtnValue==DIAG_ERR_OK )
        {
            if ( diagID->recSrqMode==REC_SRQ_MODE_AFTER_SEND )
            {
                diagID->getSRQ=TRUE;
            }
            if ( diagID->recMsgMode==REC_MSG_MODE_AFTER_SEND )
            {
                diagID->getMSG=TRUE;
            }
        }
    }

    if ( diagID->getSRQ || diagID->getSrqGuiRequest )
    {
        rtnValue=getCommEvent(diagID);
        diagID->getSrqGuiRequest=FALSE;
        if ( rtnValue==DIAG_ERR_OK )
        { 
            diagID->getSRQ=FALSE;
        }
        if ( (rtnValue==DIAG_ERR_OK && diagID->recMsgMode==REC_MSG_MODE_AFTER_SRQ) )
        {
            diagID->getMSG=TRUE;
        }
    }

    if ( diagID->getMsgGuiRequest || diagID->getMSG )
    {
        rtnValue=getCommMessage(diagID);

        diagID->getMsgGuiRequest=FALSE;

        if ( rtnValue==DIAG_ERR_OK )
        { 
            diagID->getMSG=FALSE;
        }
    }

    return DIAG_ERR_OK;
}



/*****************************************************************************
 *
 * Do work
 *
 * Authors: Chris Joyce
 *
 * History: 29 Sep 2000, Chris Joyce, taken from diagnostics/gpibTest/sim.c
 *
 * Description:
 *
 * Keeps calling phComGpibGetEvent until either an error occurs or no more
 * events pending.
 *
 *****************************************************************************/
static phDiagError_t doWork(
    phDiagId_t diagID             /* gpib diagnostic ID */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/07/2001 */
/* Issue Number: 315 */
    phDiagError_t rtnValue=DIAG_ERR_OK;
/*    recSrqMode_t previousRecSrqMode;*/
/*    recMsgMode_t previousRecMsgMode;*/
/* End of Huatek Modification */
    rtnValue=diagInitialize(diagID);
    setAndDisplayStatus(diagID, OK);

    while ( !diagID->quit && rtnValue==DIAG_ERR_OK )
    {
        rtnValue=getGuiEventAndData(diagID);
        if ( rtnValue==DIAG_ERR_GUI_TIMEOUT )
        {
            rtnValue=DIAG_ERR_OK;
        }

        switch ( diagID->recMsgMode )
        {
          case REC_MSG_MODE_BUTTON:
            diagID->getMSG=FALSE;
            break;
          case REC_MSG_MODE_AFTER_SRQ:
          case REC_MSG_MODE_AFTER_SEND:
            if ( diagID->recMsgModeChanged )
            {
                diagID->getMSG=FALSE;
            }
            break;
          case REC_MSG_MODE_ALWAYS:
            diagID->getMSG=TRUE;
            break;
        }

        switch ( diagID->recSrqMode )
        {
          case REC_SRQ_MODE_BUTTON:
            diagID->getSRQ=FALSE;
            break;
          case REC_SRQ_MODE_AFTER_SEND:
            if ( diagID->recSrqModeChanged )
            {
                diagID->getSRQ=FALSE;
            }
            break;
          case REC_SRQ_MODE_ALWAYS:
            diagID->getSRQ=TRUE;
            break;
        }

        if ( !diagID->quit && rtnValue==DIAG_ERR_OK )
        {
            rtnValue=communicationLoop(diagID);
        }
    }

#ifdef RESTORE_ORG_DEBUG_LEVEL
    setLogModeDebugLevel(diagID,diagID->org_debug);
#endif

    phGuiDestroyDialog(diagID->guiId);

    return rtnValue;
}



/*****************************************************************************
 *
 * Initialize driver event handler
 *
 * Authors: Chris Joyce
 *
 * Description:
 *
 * This function initializes the gpib diagnostics handler.
 *
 * Returns: error code
 *
 *****************************************************************************/
phDiagError_t phDiagnosticsInit(
    phDiagId_t *diagID           /* result gpib diagnostic ID */,
    phComId_t communication      /* the valid communication link to the HW
                                    interface used by the handler */,
    phLogId_t logger             /* the data logger to be used */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;
    struct phDiagStruct *tmpId;

    /* set diagID to NULL in case anything goes wrong */
    *diagID = NULL;

    phLogDiagMessage(logger,
                      PHLOG_TYPE_TRACE,
                      "gpibDiagInit(P%p, P%p, P%p)",
                      diagID,
                      communication,
                      logger);

    /* allocate new event data type */
    tmpId = PhNew(struct phDiagStruct);
    if (tmpId == 0)
    {
        phLogDiagMessage(logger,
                          PHLOG_TYPE_FATAL,
                          "not enough memory during call to gpibDiagInit()");
        return DIAG_ERR_MEMORY;
    }

    /* if have not returned yet everything is okay */
    tmpId -> myself = tmpId;
    tmpId -> communicationId = communication;
    tmpId -> loggerId = logger;
    tmpId -> guiId = NULL;

    /* set default msg SRQ parameters */
    /* send message parameters */
    tmpId->sendMsgTimeout=1;
    /* receive message parameters */
    tmpId->recMsgMode=REC_MSG_MODE_BUTTON;
    tmpId->recMsgTimeout=1;
    /* receive SRQ parameters */
    tmpId->recSrqMode=REC_SRQ_MODE_BUTTON;
    tmpId->recSRQTimeout=1;

    *diagID  = tmpId;
    return rtnValue;
}


/*****************************************************************************
 *
 * Send Message
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Diagnostic send message.
 *
 * No further actions will take place after calling this event
 * function.  The event function will receive the incoming parameters
 * from the test cell client as well as the already set and valid
 *
 *
 *
 * Returns: error code
 *
 *****************************************************************************/
phDiagError_t gpibDiagnosticsMaster(
    phDiagId_t diagID           /* gpib diagnostic ID */,
    phDiagSessionType session   /* kind of session */
)
{
    phDiagError_t rtnValue=DIAG_ERR_OK;
    phGuiError_t guiError;
    char *descr;
    const char *exitButton;
    const char *exitToolTip;

    CheckSession(diagID);

    phLogDiagMessage(diagID->loggerId, PHLOG_TYPE_TRACE,
        "gpibDiagnosticsMaster(P%p %s)",
        diagID,
        (session==STAND_ALONE) ? "STAND-ALONE" : "DIAG-SUITE" );

    descr = (char *) malloc(strlen(gpibDiagGuiDesc) + 256);

    if (descr == 0) {
        rtnValue=DIAG_ERR_MEMORY;
    }

    if ( rtnValue == DIAG_ERR_OK )
    {
        /* get and save debug mode */
        diagID->debug=getDebugFromLogMode(diagID);
        diagID->org_debug=diagID->debug;

        if ( session == STAND_ALONE )
        {
            exitButton="Quit";
            exitToolTip="Terminate session";
        }
        else
        {
            exitButton="Close";
            exitToolTip="Return to previous window";
        }

        sprintf(descr, gpibDiagGuiDesc, 
                       convertTimeoutToOptionMenuPosition(diagID->sendMsgTimeout),
                       diagID->recMsgMode + 1,
                       convertTimeoutToOptionMenuPosition(diagID->recMsgTimeout),
                       diagID->recSrqMode + 1,
                       convertTimeoutToOptionMenuPosition(diagID->recSRQTimeout),
                       convertDebugToOptionMenuPosition(diagID->debug), 
                       exitButton,
                       exitToolTip);

        guiError=phGuiCreateUserDefDialog(&diagID->guiId, diagID->loggerId, PH_GUI_COMM_ASYNCHRON, 
                                          descr);
        rtnValue=checkGuiError( diagID,guiError);
    }

    if ( rtnValue == DIAG_ERR_OK )
    {
        guiError=phGuiShowDialog(diagID->guiId, 0);
        rtnValue=checkGuiError( diagID, guiError );
    }

    if ( rtnValue==DIAG_ERR_OK )
    {
        rtnValue=doWork(diagID);
    }

    if ( descr != NULL )
    {
        free(descr);
        descr = NULL;
    }

    return rtnValue;
}

/*Begin of Huatek Modifications, Donnie Tu, 04/23/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free driver event handler
 *
 * Authors: Donnie Tu
 *
 * Description:
 *
 * This function free the gpib diagnostics handler.
 *
 * Returns: none
 *
 *****************************************************************************/

void phDiagnosticsFree(phDiagId_t *diagID)
{
   if(*diagID)
   {
      free(*diagID);
      *diagID=NULL;
   }
}
/*End of Huatek Modifications*/

/*****************************************************************************
 * End of file
 *****************************************************************************/
