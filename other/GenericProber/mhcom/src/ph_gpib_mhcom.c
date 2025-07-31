/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_gpib_mhcom.c
 * CREATED  : 30 Aug 2000
 *
 * CONTENTS : GPIB communication layer entry point
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 30 Aug 2000, Michael Vogt, created
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
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <vector>
#include <algorithm>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_mhcom.h"

#include "xoc/hw/cor/gio/gio.h"
#include "dev/DriverDevKits.hpp"
#include "gioBridge.h"
#include "ph_gpib_mhcom.h"
#include "ph_mhcom_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/


#define PH_SESSION_CHECK

#ifdef DEBUG
#define PH_SESSION_CHECK
#endif

#ifdef PH_SESSION_CHECK
#define CheckSession(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHCOM_ERR_UNKNOWN_DEVICE
#else
#define CheckSession(x)
#endif

#define RELEASE_BUFFER(x) {\
   if ( x != NULL ) \
   {\
     free(x);\
     x = NULL;\
   }\
}

#define SRQ_BIT 64

/* delay used before sending two messages in a row, if talking to a
   simulator */
#define SIM_SEND_DELAY_MS 100
#define SIM_RECV_DELAY_MS 20
#define SIM_SRQ_DELAY_MS 20

/*--- global variables ------------------------------------------------------*/

/* The only reason, why this global variable exists is, that the alarm
   signal handler used in slave mode can not receive an arbitrary
   argument (the session ID). The drawback is, that this module can
   only handle one slave session in parallel (not too bad) */
static struct phComStruct *gGloSession = NULL;

static std::deque<int> eventFIFO(0);

struct SrqCallbackTableType
{
  unsigned char statusByte;
  AsyncSRQCallback_t callback;
  int  keepStatusByte;
  void *helperData;
  struct SrqCallbackTableType* next; 
};

static struct SrqCallbackTableType *srqCallbackTable = NULL;

/*--- function prototypes ---------------------------------------------------*/
static void srqHandler(phSiclInst_t Id, void *dataPtr);

static int localDelay_ms( int milliseconds);
static int localDelayRemaining(struct timeval *compareTime, long milliseconds);
static int mostRecentTime(struct timeval *one, struct timeval *two, 
    struct timeval *three);

/*--- functions -------------------------------------------------------------*/
/*****************************************************************************
 * handle return value
 *****************************************************************************/
static void handleReturnValue(phComError_t rtnValue)
{
    if(rtnValue == PHCOM_ERR_OK || rtnValue == PHCOM_ERR_TIMEOUT)
        return;
    driver_dev_kits::AlarmController alarmController;
    alarmController.raiseCommunicationAlarm(GPIB_ERROR_DESCRIPTION[rtnValue]);
}

/*****************************************************************************
 * Clear up the srq callback table
 *****************************************************************************/
static void freeSrqCallbackTable()
{
    if(srqCallbackTable != NULL)
    {
        struct SrqCallbackTableType *tmp_ptr = srqCallbackTable;
        while(srqCallbackTable != NULL)
        {
            srqCallbackTable = tmp_ptr->next;
            free(tmp_ptr); 
            tmp_ptr = srqCallbackTable;
        }
    }
}

/*****************************************************************************
 *
 * Get timestamp string
 *
 * Author: Michael Vogt
 *
 * Description: 
 * Returns a pointer to a string in the format like
 *
 * 1998/01/15 13:45:12.345 
 *
 * based on the current time and local time zone
 *
 *****************************************************************************/

static char *getTimeString(void)
{
    static char theTimeString[64];
    struct tm theLocaltime;
    time_t theTime;
    struct timeval getTime;
    
    gettimeofday(&getTime, NULL);
    theTime = (time_t) getTime.tv_sec;
    localtime_r(&theTime, &theLocaltime);
    sprintf(theTimeString, "%4d/%02d/%02d %02d:%02d:%02d.%04d",
	theLocaltime.tm_year + 1900,
	theLocaltime.tm_mon + 1,
	theLocaltime.tm_mday,
	theLocaltime.tm_hour,
	theLocaltime.tm_min,
	theLocaltime.tm_sec,
/* Begin of Huatek Modification, Donnie Tu, 12/05/2001 */
/* Issue Number: 318 */
	(int)(getTime.tv_usec/100));
/* End of Huatek Modification */

    return theTimeString;	
}

/*****************************************************************************
 *
 * Open the underlaying GIO session
 *
 * Author: Jeff Morton / Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

static phComError_t openSiclSession(
    struct phComStruct *session
)
{
    phComError_t rtnValue=PHCOM_ERR_OK;
    char tmpString[PHCOM_MAX_MESSAGE_LENGTH+1] = "";

    sprintf(tmpString, "%s,%d", session->gpib->device,session->gpib->port);

    if ((session->gioId = phComIOpen(tmpString, session->mode, 
	    PHCOM_IFC_GPIB, session->loggerId)) == 0)
    {
        return PHCOM_ERR_GPIB_INVALID_INST; 
    }

    if ((rtnValue = phComITimeout(session->gioId, 5000)) != PHCOM_ERR_OK)
    {
        phComIClose(session->gioId);
        return rtnValue;
    }

    if ((rtnValue = phComIClear(session->gioId)) != PHCOM_ERR_OK) 
    {
        phComIClose(session->gioId);
        return rtnValue;
    }

    if ((rtnValue = phComIgpibAtnCtl(session->gioId, 0)) != PHCOM_ERR_OK)
    {
        phComIClose(session->gioId);
        return rtnValue;
    }

    if ((rtnValue = phComIOnSRQ(session->gioId, srqHandler, session)) != PHCOM_ERR_OK)
    {
        phComIClose(session->gioId);
        return rtnValue;
    }

    if ((rtnValue = phComITermChr(session->gioId, '\n')) != PHCOM_ERR_OK)
    {
	phComIClose(session->gioId);
	return rtnValue;
    }

    /* ... session is now open and working */
    return rtnValue;
}

/*****************************************************************************
 *
 * Close the underlaying GIO session
 *
 * Author: Jeff Morton / Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

static phComError_t closeSiclSession(
    struct phComStruct *session
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;

    rtnValue = phComIClose(session->gioId);
    if (rtnValue == PHCOM_ERR_OK)
	session->gioId = NULL;

    return rtnValue;
}

/*****************************************************************************
 *
 * Open a GPIB Session
 *
 * Author: Jeff Morton / Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComGpibOpen(
    phComId_t *newSession,
    phComMode_t mode,                    
    const char *device,                    
    int port,
    phLogId_t logger,
    phComGpibEomAction_t act
)
{
    int count;

    phComError_t rtnValue=PHCOM_ERR_OK;
    struct phComStruct *tmpId;

    phLogComMessage(logger, PHLOG_TYPE_TRACE,
	"phComGpibOpen(P%p, %d, \"%s\", %d, P%p)",
	newSession, (int) mode, device ? device : "(NULL)", 
	port, logger);

    /* don't allow interrupts during this phase */
    phComIIntrOff(mode);

    /* set session ID to NULL in case anything goes wrong */
    *newSession = NULL;
    gGloSession = NULL;

    /* allocate new communication data type */
    tmpId = PhNew(struct phComStruct);
    if (tmpId == 0)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL,
	    "not enough memory during call to phComGpibOpen()");
	phComIIntrOn(mode);
        return PHCOM_ERR_MEMORY;
    }
    tmpId -> gpib = PhNew(struct phComGpibStruct);
    if (tmpId -> gpib == 0)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL,
	    "not enough memory during call to phComGpibOpen()");
	phComIIntrOn(mode);
        RELEASE_BUFFER(tmpId);
        return PHCOM_ERR_MEMORY;
    }
    tmpId -> lan = NULL;

    /* initialize with default values */
    tmpId -> myself = NULL;
    tmpId -> mode = mode;
    tmpId -> gioId = NULL;
    tmpId -> loggerId = logger;
    tmpId -> intfc = PHCOM_IFC_GPIB;

    /* initialize GPIP specific data */
    if (device)
    {
      strncpy(tmpId -> gpib -> device, device, 63);
      tmpId -> gpib -> device[63] = '\0';
    }
    tmpId -> gpib -> port = port;
    tmpId -> gpib -> act = act;

    tmpId -> gpib -> gSrqLaunched = 0; 

    /* in off-line mode validate the handle and return, all done */
    if (mode == PHCOM_OFFLINE) {
	tmpId->myself = tmpId;
	*newSession  = tmpId;
	phComIIntrOn(mode);        
	return rtnValue;
    }

    /* initialize some delays and timestamps */
    if (mode == PHCOM_SIMULATION || 
	getenv("PHCOM_PROBER_SIMULATION") || 
	getenv("PHCOM_SIMULATION"))
    {
	tmpId->mode =  PHCOM_ONLINE;
	tmpId->gpib->gSimulatorInUse = 1;
	gettimeofday(&tmpId->gpib->gLastSentTime, NULL);
	tmpId->gpib->gLastRecvTime = tmpId->gpib->gLastSentTime;
	tmpId->gpib->gLastSRQTime = tmpId->gpib->gLastSentTime;
	phLogComMessage(logger, PHLOG_TYPE_WARNING, 
	    "phComGpibOpen():     expecting simulator in use (delayed timing)");
    } 
    else
	tmpId->gpib->gSimulatorInUse = 0;

    /* initialize Event FIFO */
    eventFIFO.clear();


    /* open the GIO session to the interface */
    rtnValue = openSiclSession(tmpId);
    if (rtnValue != PHCOM_ERR_OK)
    {
	phComIIntrOn(mode);
        RELEASE_BUFFER(tmpId->gpib);
        RELEASE_BUFFER(tmpId);
	return rtnValue;
    }

    /* validate the handle */
    tmpId->myself = tmpId;
    *newSession  = tmpId;
    gGloSession = tmpId;    /* needed by alarmHandler() */

    phLogComMessage(logger, LOG_VERBOSE, 
	"phComGpibOpen():     successful");

    phComIIntrOn(mode);
    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComGpibOpen
 *****************************************************************************/
phComError_t phComGpibOpen(phComId_t *newSession, phComMode_t mode, const char *device, int port, phLogId_t logger, phComGpibEomAction_t act)
{
    phComError_t rtnValue = _phComGpibOpen(newSession, mode, device, port, logger, act);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Reopen a GPIB Session
 *
 * Author: Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComGpibReopen(
    phComId_t session,
    char *device,                    
    int port
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;

    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
	"phComGpibReopen(P%p, \"%s\", %d)",
	session, device ? device : "(NULL)", port);

    /* don't allow interrupts during this phase */
    phComIIntrOff(session->mode);

    /* apply possibly changed device information */
    if (device && strlen(device) > 0)
    {
	strncpy(session->gpib->device, device, 63);
	session->gpib->device[63] = '\0';
    }
    if (port >= 0)
	session->gpib->port = port;

    if (session->mode == PHCOM_OFFLINE) {
	phComIIntrOn(session->mode);
	return rtnValue;
    }

    /* close the current and open the new session */
    rtnValue = closeSiclSession(session);
    if (rtnValue != PHCOM_ERR_OK && rtnValue != PHCOM_ERR_NO_CONNECTION)
    {
	phComIIntrOn(session->mode);
	return rtnValue;
    }

    /* give the HW interface some time to recover */
    sleep(1);

    /* open a new session */
    if ((rtnValue = openSiclSession(session)) != PHCOM_ERR_OK)
    {
	phComIIntrOn(session->mode);
	return rtnValue;
    }
    
    /* all done */
    phComIIntrOn(session->mode);
    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComGpibReOpen
 *****************************************************************************/
phComError_t phComGpibReOpen(phComId_t session, char *device, int port)
{
    phComError_t rtnValue = _phComGpibReopen(session, device, port);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Send a string over the GPIB bus  
 *
 * Authors:  Jeff Morton / Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComGpibSend(
    phComId_t session, 
    char *message, 
    int length, 
    long timeout 
)
{
    unsigned long act_msg;
    unsigned long timeout_ms = timeout/1000;
    int timedOut = 0;
    phComError_t rtnValue=PHCOM_ERR_OK;
    struct timeval entryTime;

    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
	"phComGpibSend(P%p, \"%s\", %d, %ld)",
	session, message ? message : "(NULL)", length, timeout);

    if (message == NULL || session->mode == PHCOM_OFFLINE) {
	return rtnValue;
    }

    if ((rtnValue = phComITimeout(session->gioId, timeout_ms))
        != PHCOM_ERR_OK)
        return rtnValue;

    phComIIntrOff(session->mode);
    if (session->gpib->gSimulatorInUse) {
       switch (mostRecentTime(&session->gpib->gLastSentTime, 
          &session->gpib->gLastRecvTime, &session->gpib->gLastSRQTime))
       {
            case 1:
                localDelayRemaining(&session->gpib->gLastSentTime, 100); break;
            case 2:
                localDelayRemaining(&session->gpib->gLastRecvTime, 20); break;
            case 3:
		localDelayRemaining(&session->gpib->gLastSRQTime, 20); break;
       }
    }
    if ((rtnValue = phComIWrite(session->gioId, message, strlen(message), 1, &act_msg)) != PHCOM_ERR_OK)
    {
       phComIIntrOn(session->mode);
       return rtnValue;
    }

    if (session->gpib->gSimulatorInUse) {
        gettimeofday(&session->gpib->gLastSentTime, NULL);
    }

    /* check whether it is necessary to perform a special end of
       message action. This is a workaround for a bug with GIO
       and/or the GPIB hardware: Th eproblem ist that the EOI line
       remains asserted after the last byte of a message has been
       send. This may cause some instruments (seen at a EG4080
       wafer prober with EG station controller) to hang up or
       create long delays on the bus. Unfortunately the EOI line
       can not be released directly. We need to perform a
       different operation which releases the EOI line as a side
       effect */
    switch(session->gpib->act)
    {
        case PHCOM_GPIB_EOMA_TOGGLEATN:
        /* assert and release the ATN line.  Acording to the IEEE
           488 standard this is allowed. The side effect of this
           operation is that the EOI line is released while the
           ATN line is asserted, because EOI && ATN have a special
           meaning. GIO or the GPIB hardware knows that and
           releases the EOI line */
            phLogComMessage(session->loggerId, LOG_DEBUG,
	           "toggling ATN line to release the EOI line");
            phComIgpibAtnCtl(session->gioId, 1);
            phComIgpibAtnCtl(session->gioId, 0);
            break;
	case PHCOM_GPIB_EOMA_SPOLL:
	/* perform a single serial poll. This is OK for the bus
           and the instrument that is polled. Of corse we are not
           allowed to loose the serial byte, if it contains
           valuable information. This is why the SRQ handler
           function is called. It takes care about the byte and
           puts into the FIFO, if it is a valid byte */
	   phLogComMessage(session->loggerId, LOG_DEBUG,
	            "performing one serial poll to release the EOI line");
	   srqHandler(session->gioId, session);
	   break;
         case PHCOM_GPIB_EOMA_NONE:
           default:
	     /* do nothing */
	   break;
     }

     phComIIntrOn(session->mode);

     return rtnValue;
}

/*****************************************************************************
 * wrapper of phComGpibSend
 *****************************************************************************/
phComError_t phComGpibSend(phComId_t session, char *message, int length, long timeout)
{
    phComError_t rtnValue = _phComGpibSend(session, message, length, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Receive a string over the GPIB bus  
 *
 * Authors:  Jeff Morton / Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComGpibReceive( 
    phComId_t session, 
    const char **message,
    int *length,
    long timeout 
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/05/2001 */
/* Issue Number: 315 */
    unsigned long timeout_ms = timeout/1000;
/* End of Huatek Modification */
    int reason;
    unsigned long read_nbr_bytes;
    int timedOut = 0;

    static char tmpString[PHCOM_MAX_MESSAGE_LENGTH+1] = "";
    static char receivedMessage[PHCOM_MAX_MESSAGE_LENGTH+1] = "";
    phComError_t rtnValue=PHCOM_ERR_OK;
    struct timeval entryTime;

    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
	"phComGpibReceive(P%p, P%p, P%p, %ld)",
	session, message, length, timeout);

    if (session->mode == PHCOM_OFFLINE) {
	return rtnValue;
    }

    if ((rtnValue = phComITimeout(session->gioId, timeout_ms))
        != PHCOM_ERR_OK)
        return rtnValue;

    phComIIntrOff(session->mode);
    if (session->gpib->gSimulatorInUse) {
        switch (mostRecentTime(&session->gpib->gLastSentTime, 
                &session->gpib->gLastRecvTime, &session->gpib->gLastSRQTime))
        {
            case 1:
                 localDelayRemaining(&session->gpib->gLastSentTime, 20); break;
            case 2:
                 localDelayRemaining(&session->gpib->gLastRecvTime, 20); break;
            case 3:
                 localDelayRemaining(&session->gpib->gLastSRQTime, 20); break;
        }
    }
    if ((rtnValue = phComIRead(session->gioId, tmpString, 
        sizeof(tmpString), &reason, &read_nbr_bytes)) != PHCOM_ERR_OK)
    {
        phComIIntrOn(session->mode);
        return rtnValue;
    } 
    else 
    {
        tmpString[read_nbr_bytes] = '\0';
        *length = read_nbr_bytes;
        *message = tmpString;
    }
    if (session->gpib->gSimulatorInUse) {
       gettimeofday(&session->gpib->gLastRecvTime, NULL);
    }
    phComIIntrOn(session->mode);

    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComGpibReceive
 *****************************************************************************/
phComError_t phComGpibReceive(phComId_t session, const char **message, int *length, long timeout)
{
    phComError_t rtnValue = _phComGpibReceive(session, message, length, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Clear the GPIB interface
 *
 * Authors: Xiaofei Han
 *
 * 16 Dec 2014, Xiaofei Han , created
 *
 * Description:
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComGpibClear(phComId_t session)
{
  phComError_t rtnValue=PHCOM_ERR_OK;

  rtnValue = phComIClear(session->gioId);

  return rtnValue;
}

/*****************************************************************************
 * wrapper of phComGpibClear
 *****************************************************************************/
phComError_t phComGpibClear(phComId_t session)
{
    phComError_t rtnValue = _phComGpibClear(session);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Query for an Event over the GPIB bus  
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComGpibGetEvent(
    phComId_t session,
    phComGpibEvent_t **event,
    int *pending,
    long timeout                
)
{
    unsigned long timeout_ms;
    phComError_t rtnValue=PHCOM_ERR_OK;
    static phComGpibEvent_t event2;
    *event = &event2;

    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
	"phComGpibGetEvent(P%p, P%p, P%p, %ld)",
	session, event, pending, timeout);

    if (session->mode == PHCOM_OFFLINE)
	return rtnValue;

    phComIIntrOff(session->mode); 

    timeout_ms = timeout/1000;   /* convert from microseconds to milliseconds*/
    if ((rtnValue = phComITimeout(session->gioId, timeout_ms)) 
	!= PHCOM_ERR_OK)
    {
	phComIIntrOn(session->mode); 
	return rtnValue;
    }

    event2.type = PHCOM_GPIB_ET_SRQ;

    if (eventFIFO.empty())
    {
        /* Nothing in FIFO yet */
        if (timeout_ms == 0) 
        {
            phLogComMessage(session->loggerId, LOG_VERBOSE, 
                            "phComGpibGetEvent(): no SRQ in FIFO, immediate return");
            phComIIntrOn(session->mode); 
            return PHCOM_ERR_TIMEOUT; 
        }
        phLogComMessage(session->loggerId, LOG_VERBOSE,
                        "phComGpibGetEvent(): waiting for SRQ for %ld ms",
                        timeout_ms); 

        if ((rtnValue = phComIWaitHdlr(session->gioId, timeout_ms)) != PHCOM_ERR_OK)
        {
            /* problem with iwaithdlr(), log message then leave  */
            *pending = 0;
            *event = NULL;   /* send back a NULL to calling function */
            phLogComMessage(session->loggerId, LOG_VERBOSE,
                            "phComGpibGetEvent(): no SRQ was received within %ld ms", 
                            timeout_ms);
            phComIIntrOn(session->mode); 
            return rtnValue;
        }
    }  

    /* check whether iwaithdlr succeeded but there was still no SRQ
       received. This seems to be a GIO bug: sometimes it happens,
       that the srqHandler is called, but no SRQ comes in. In this
       case, nothing is put into the FIFO. Try to work around this
       by returning a TIMEOUT. Maybe the correct SRQ comes later
       during a second interrupt !? */
    if (eventFIFO.empty())
    {
        phLogComMessage(session->loggerId, PHLOG_TYPE_WARNING,
                        "phComGpibGetEvent(): no SRQ was received but waiting did not timeout", 
                        timeout_ms);
        phComIIntrOn(session->mode); 
        return PHCOM_ERR_TIMEOUT; 	   
    }    

    /* something should be in FIFO at this point */
    event2.d.srq_byte = (unsigned char)(eventFIFO.front());
    eventFIFO.pop_front();

    phLogComMessage(session->loggerId, LOG_DEBUG,
                    "%sGPIB %s SRQ recv <--  0x%02x (%d) (from FIFO)", 
                    "" /* room for different modes */, 
                    getTimeString(), 
                    (int)(event2.d.srq_byte), (int)(event2.d.srq_byte));

    phComIIntrOn(session->mode);
    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComGpibGetEvent
 *****************************************************************************/
phComError_t phComGpibGetEvent(phComId_t session, phComGpibEvent_t **event, int *pending, long timeout)
{
    phComError_t rtnValue = _phComGpibGetEvent(session, event, pending, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Query for expected Event over the GPIB bus. If SRQ arrives but it's not
 * in the expected list, this function will simply keep waiting.
 *
 * Authors: Xiaofei Han
 *
 * Description:
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/
phComError_t _phComGpibGetExpectedEvents(
    phComId_t session,
    phComGpibEvent_t &event,
    std::vector<int> expected,
    long timeout
)
{
    unsigned long timeout_ms;
    phComError_t rtnValue=PHCOM_ERR_OK;
    int i = 0;

    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
                    "phComGpibGetEvent(P%p, P%p, P%p, %ld)",
                    session, event, timeout);

    if (session->mode == PHCOM_OFFLINE)
    {
      return rtnValue;
    }

    phComIIntrOff(session->mode);

    timeout_ms = timeout/1000;   /* convert from microseconds to milliseconds*/
    if ((rtnValue = phComITimeout(session->gioId, timeout_ms)) != PHCOM_ERR_OK)
    {
        phComIIntrOn(session->mode);
        return rtnValue;
    }

    event.type = PHCOM_GPIB_ET_SRQ;

    if (eventFIFO.empty())
    {
        /* Nothing in FIFO yet */
        if (timeout_ms == 0)
        {
            phLogComMessage(session->loggerId, LOG_VERBOSE,
                            "phComGpibGetEvent(): no SRQ in FIFO, immediate return");
            phComIIntrOn(session->mode);
            return PHCOM_ERR_OK;
        }
        phLogComMessage(session->loggerId, LOG_VERBOSE,
                        "phComGpibGetEvent(): waiting for SRQ for %ld ms",
                        timeout_ms);

        if ((rtnValue = phComIWaitHdlr(session->gioId, timeout_ms)) != PHCOM_ERR_OK)
        {
            /* problem with iwaithdlr(), log message then leave  */
            phLogComMessage(session->loggerId, LOG_VERBOSE,
                            "phComGpibGetEvent(): no SRQ was received within %ld ms",
                            timeout_ms);
            phComIIntrOn(session->mode);
            return rtnValue;
        }
    }

    /* check whether iwaithdlr succeeded but there was still no SRQ
       received. This seems to be a GIO bug: sometimes it happens,
       that the srqHandler is called, but no SRQ comes in. In this
       case, nothing is put into the FIFO. Try to work around this
       by returning a TIMEOUT. Maybe the correct SRQ comes later
       during a second interrupt !? */
    if (eventFIFO.empty())
    {
        phLogComMessage(session->loggerId, PHLOG_TYPE_WARNING,
                        "phComGpibGetEvent(): no SRQ was received but waiting did not timeout",
                        timeout_ms);
        phComIIntrOn(session->mode);
        return PHCOM_ERR_TIMEOUT;
    }

    /* something should be in FIFO at this point */
    if(expected.empty())
    {
      event.d.srq_byte = (unsigned char)(eventFIFO.front());
      eventFIFO.pop_front();

      phLogComMessage(session->loggerId, LOG_DEBUG,
                      "The expected SRQ list is empty, just return 0x%02x (%d)",
                      (int)(event.d.srq_byte), (int)(event.d.srq_byte));

      phLogComMessage(session->loggerId, LOG_DEBUG,
                          "GPIB  %s SRQ recv <--  0x%02x (%d) (from FIFO)",
                          getTimeString(),
                          (int)(event.d.srq_byte), (int)(event.d.srq_byte));
      phComIIntrOn(session->mode);
      return PHCOM_ERR_OK;
    }

    // we will check if the received SRQ is expected
    for(unsigned int i = 0; i < expected.size(); i++)
    {
      phLogComMessage(session->loggerId, LOG_DEBUG,
                      "%sGPIB: check if SRQ 0x%02x (%d) in the FIFO",
                      getTimeString(),
                      expected[i], expected[i]);
      for(std::deque<int>::iterator iter = eventFIFO.begin(); iter != eventFIFO.end(); iter++)
      {
        phLogComMessage(session->loggerId, LOG_DEBUG,
                        "%sGPIB: SRQ to check is %d, current SRQ in the FIFO is %d",
                        getTimeString(),
                        expected[i], *iter);

        if(expected[i] == *iter)
        {
          event.d.srq_byte = (unsigned char)(*iter);
          eventFIFO.erase(iter);
          phLogComMessage(session->loggerId, LOG_DEBUG,
                          "%sGPIB: Got expected SRQ 0x%02x (%d)",
                          getTimeString(),
                          (int)(event.d.srq_byte), (int)(event.d.srq_byte));
          phComIIntrOn(session->mode);
          return PHCOM_ERR_OK;
        }
      }
    }

    //we don't find expected in the FIFO when we reach this point
    rtnValue = PHCOM_ERR_TIMEOUT;

    phComIIntrOn(session->mode);
    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComGpibGetExpectedEvents
 *****************************************************************************/
phComError_t phComGpibGetExpectedEvents(phComId_t session, phComGpibEvent_t &event, std::vector<int> expected, long timeout)
{
    phComError_t rtnValue = _phComGpibGetExpectedEvents(session, event, expected, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Check if any SRQ in the input SRQ list can be found in the FIFO. This function
 * doesn't remove any SRQs from the FIFO.
 *
 * Returns: error code
 *
 ***************************************************************************/
phComError_t _phComGpibCheckSRQs(
    phComId_t session,
    const int srqs[],
    int srqCount,
    int *foundSRQ)
{
  int i = 0, j = 0;
  *foundSRQ = -1;
  phComError_t rtnValue=PHCOM_ERR_OK;

  CheckSession(session);

  if (session->mode == PHCOM_OFFLINE)
      return rtnValue;

  phComIIntrOff(session->mode);

  for(i = 0; i < eventFIFO.size(); i++)
  {
    phLogComMessage(session->loggerId, LOG_DEBUG,
            "Found SRQ 0x%02x in FIFO", eventFIFO[i]);

    for(j = 0; j < srqCount; j++)
    {
      if( srqs[j] == eventFIFO[i])
      {
        phLogComMessage(session->loggerId, LOG_DEBUG,
                "Matched SRQ 0x%02x in FIFO", srqs[j]);
        *foundSRQ = srqs[j];
        phComIIntrOn(session->mode);
        return rtnValue;
      }
    }
  }

  phComIIntrOn(session->mode);
  return rtnValue;
}


/*****************************************************************************
 * wrapper of phComGpibCheckSRQs
 *****************************************************************************/
phComError_t phComGpibCheckSRQs(phComId_t session, const int srqs[], int srqCount, int *foundSRQ)
{
    phComError_t rtnValue = _phComGpibCheckSRQs(session, srqs, srqCount, foundSRQ);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Send an Event over the GPIB bus (Slave Mode Only)
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t phComGpibSendEvent(
    phComId_t session,
    phComGpibEvent_t *event,
    long timeout                
)
{
  phComError_t rtnValue = PHCOM_ERR_OK;
  return rtnValue;
}


/*****************************************************************************
 *
 * Close a GPIB session 
 *
 * Authors: Jeff Morton / Michael Vogt
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComGpibClose(
    phComId_t session
)
{
    phComError_t rtnValue=PHCOM_ERR_OK;
    phComMode_t mode;

    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
	"phComGpibClose(P%p)", session);

    /* don't allow interrupts */
    mode = session->mode;
    phComIIntrOff(mode);

    //free the SRQ callback table
    freeSrqCallbackTable();

    /* don't free the memory: In case the session ID is abused
       from outside this module, we do prevent segmentation
       faults. This is not a big memory leak, since sessions are
       usually only opened once */

    if (session->mode == PHCOM_OFFLINE) {
        /* make phComId invalid */
        session -> myself = NULL;
        /* free(session); */

	phComIIntrOn(mode);
	return rtnValue;
    }

    if ((rtnValue = closeSiclSession(session)) == PHCOM_ERR_OK)
    { 
        /* make phComId invalid */
        session -> myself = NULL;
        /* free(session); */
    }

    phComIIntrOn(mode);
    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComGpibClose
 *****************************************************************************/
phComError_t phComGpibClose(phComId_t session)
{
    phComError_t rtnValue = _phComGpibClose(session);
    handleReturnValue(rtnValue);
    return rtnValue;
}


phComError_t _phComGpibRegisterSRQCallback(
    phComId_t session, 
    unsigned char statusByte,
    AsyncSRQCallback_t callback,
    int keepStatusByte,
    void *helperData
)
{
    struct SrqCallbackTableType *tmp_ptr = NULL;
    tmp_ptr = PhNew(struct SrqCallbackTableType);
    if(tmp_ptr != NULL)
    {
        tmp_ptr->statusByte = statusByte;
        tmp_ptr->callback = callback;
        tmp_ptr->next = srqCallbackTable; 
        tmp_ptr->keepStatusByte = keepStatusByte;
        tmp_ptr->helperData = helperData;
        srqCallbackTable = tmp_ptr;
    }
    else
    {
        return PHCOM_ERR_MEMORY;
    }

    return PHCOM_ERR_OK;
}

/*****************************************************************************
 * wrapper of phComGpibRegisterSRQCallback
 *****************************************************************************/
phComError_t phComGpibRegisterSRQCallback(phComId_t session, unsigned char statusByte, AsyncSRQCallback_t callback, int keepStatusByte, void* helperData)
{
    phComError_t rtnValue = _phComGpibRegisterSRQCallback(session, statusByte, callback, keepStatusByte, helperData);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * SRQ Event Handler   
 *
 * Authors: Jeff Morton 
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

static void srqHandler(phSiclInst_t Id, void *dataPtr)
{
    unsigned char stb;  
    struct phComStruct *session = (struct phComStruct *) dataPtr;
    struct SrqCallbackTableType *tmp_ptr = srqCallbackTable;
    int keepStatusByte = 1;

    phComIIntrOff(session->mode);

    if (session->gpib->gSimulatorInUse) {
	/* slow down if using GPIB card as simulator (nac) */
	switch (mostRecentTime(&session->gpib->gLastSentTime, 
	    &session->gpib->gLastRecvTime, &session->gpib->gLastSRQTime))
	{
	  case 1:
	    localDelayRemaining(&session->gpib->gLastSentTime, 2); break;
	  case 2:
	    localDelayRemaining(&session->gpib->gLastRecvTime, 2); break;
	  case 3:
	    localDelayRemaining(&session->gpib->gLastSRQTime, 20); break;
	}
    }

    /* perform serial poll */
    if (phComIReadStb(Id, &stb) != PHCOM_ERR_OK) {   
	if (session->gpib->gSimulatorInUse) {
	    gettimeofday(&session->gpib->gLastSRQTime, NULL);
	}
	phComIIntrOn(session->mode);
	return;
    }

    /* 
     * Loop through the callback table to see if callback function is defined
     * for the current status byte 
     */
    while(tmp_ptr != NULL)
    {
        if(tmp_ptr->statusByte == stb)
        {
            (tmp_ptr->callback)(stb, tmp_ptr->helperData);
            keepStatusByte = tmp_ptr->keepStatusByte;
            break;
        }
        else
        {
            tmp_ptr = tmp_ptr->next;
        }
    }

    if(keepStatusByte == 1)
    { //no callback is defined, just put the SRQ in the FIFO
        if (stb & SRQ_BIT) {
          eventFIFO.push_back((int)stb);
        } else {
	    phLogComMessage(session->loggerId, LOG_DEBUG,
	        "srqHandler():        serial poll delivered empty status byte (ignored)");  
        }
    }

    /* release ATN line, this is a workaround of a GIO bug */
    phComIgpibAtnCtl(Id, 0);

    if (session->gpib->gSimulatorInUse) {
	gettimeofday(&session->gpib->gLastSRQTime, NULL);
    }

    phComIIntrOn(session->mode);
    return;
}

/*****************************************************************************
 * Local Delay mechanism 
 * Authors: Michael Vogt
 * Description: 
 *   Delays execution until the compare time plus the given milliseconds 
 *   is reached
 *****************************************************************************/

static int mostRecentTime(
    struct timeval *one, struct timeval *two, struct timeval *three)
{
    int recent = 1;
    struct timeval selected = *one;

    if (two->tv_sec > selected.tv_sec ||
	(two->tv_sec == selected.tv_sec && two->tv_usec > selected.tv_usec))
    {
	recent = 2;
	selected = *two;
    }

    if (three->tv_sec > selected.tv_sec ||
	(three->tv_sec == selected.tv_sec && three->tv_usec >selected.tv_usec))
    {
	recent = 3;
	selected = *three;
    }

    return recent;
}

static int localDelayRemaining(struct timeval *compareTime, long milliseconds)
{
    struct timeval now;
/* Begin of Huatek Modification, Donnie Tu, 12/05/2001 */
/* Issue Number: 315 */
/*    struct timeval delay;*/
/* End of Huatek Modification */
    long millisecondsGone;

    gettimeofday(&now, NULL);
    millisecondsGone = (now.tv_sec - compareTime->tv_sec) * 1000L;
    millisecondsGone += (now.tv_usec - compareTime->tv_usec) / 1000L;

    if (millisecondsGone > milliseconds)
	return 1;
    else
	return localDelay_ms((int) (milliseconds - millisecondsGone));
}

/*****************************************************************************
 * Local Delay mechanism 
 * Authors: Jeff Morton 
 * Description: 
 *   Delays program execution by the specified amount in milliseconds.        
 *****************************************************************************/

static int localDelay_ms( int milliseconds)
{
    int seconds;
    int microseconds;
    struct timeval delay;
 
    /* ms must be converted to seconds and microseconds ** for use by select(2)*/
    if (milliseconds > 999) {
	microseconds = milliseconds % 1000;
	seconds = (milliseconds - microseconds) / 1000;
	microseconds *= 1000;
    } else {
	seconds = 0;
	microseconds = milliseconds * 1000;
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
 * End of file
 *****************************************************************************/
