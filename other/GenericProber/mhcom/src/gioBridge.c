/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : gioBridge.c
 * CREATED  : 30 Aug 2000
 *
 * CONTENTS : GIO call layer - now using GIO
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

#include "xoc/hw/cor/gio/gio.h"

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"

#include "gioBridge.h"
#include "ph_mhcom_private.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/*--- defines ---------------------------------------------------------------*/

/* 
 * Note: for offline sessions gioId is set to NULL 
 */

#define CheckSession(x) \
{ \
    if (!(x) || ((x)->mode!=PHCOM_OFFLINE && !((x)->gioId))) \
	return PHCOM_ERR_NO_CONNECTION; \
} \


/*--- typedefs --------------------------------------------------------------*/

struct phSiclInst
{
    phComMode_t               mode;     /* selects the communication mode */
    phComIfcType_t            intfc;
    char                      *name;
    int                       isMaster; /* set for masters of GPIB sessions */
    char                      card[64];

    phLogId_t                 loggerId; /* The data logger for errors/msgs */

    unsigned long             lastDataResult;
    unsigned long             lastStatResult;
    unsigned long             lastInfoResult;
    unsigned long             lastLatchResult;

    /* online parameters, if mode == PHCOM_ONLINE or PHCOM_SIMULATION */
    IDID                      gioId;

    /* inter process communication parameters, if mode == PHCOM_IPC (to
       be added in the future */
};

struct onlineSRQMap
{
    phSiclInst_t              bridgeId; /* bridge session handle */
    phComSRQHandler_t         handler;  /* the bridge SRQ handler */
    void                      *srqPtr;  /* custom SRQ data pointer */
    struct onlineSRQMap       *next;
};

/*--- global variables ------------------------------------------------------*/

struct onlineSRQMap *onlSRQMap = NULL;

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * Get timestamp string
 *
 * Author: Michael Vogt
 *
 * Description: 
 * Returns a pointer to a string in the format like
 *
 * 1998/01/15 13:45:12.3456 
 *
 * based on the current time and local time zone
 *
 *****************************************************************************/

static char *getTimeString(int withDate)
{
    static char theTimeString[64];
    struct tm *theLocaltime;
    time_t theTime;
    struct timeval getTime;
    
    gettimeofday(&getTime, NULL);
    theTime = (time_t) getTime.tv_sec;
    theLocaltime = localtime(&theTime);
    if (withDate)
	sprintf(theTimeString, "%4d/%02d/%02d %02d:%02d:%02d.%04d",
	    theLocaltime->tm_year + 1900,
	    theLocaltime->tm_mon + 1,
	    theLocaltime->tm_mday,
	    theLocaltime->tm_hour,
	    theLocaltime->tm_min,
	    theLocaltime->tm_sec,
/* Begin of Huatek Modifications, Donnie Tu, 12/05/2001 */
/* Issue Number: 318 */
	    (int)(getTime.tv_usec/100));
/* End of Huatek Modifications */
    else
	sprintf(theTimeString, "%02d:%02d:%02d.%04d",
	    theLocaltime->tm_hour,
	    theLocaltime->tm_min,
	    theLocaltime->tm_sec,
/* Begin of Huatek Modifications, Donnie Tu, 12/05/2001 */
/* Issue Number: 318 */
	    (int)(getTime.tv_usec/100));
/* End of Huatek Modifications */

    return theTimeString;	
}

#define MAX_KEPT_BIT_STRINGS 6

static char *getBitString(unsigned long setting, char lowChr, char highChr, int count)
{
    static int pos = 0;
    static char theString[MAX_KEPT_BIT_STRINGS][33];

    pos = (pos+1) % MAX_KEPT_BIT_STRINGS;

    theString[pos][count] = '\0';
    while (count > 0)
    {
	theString[pos][--count] = (setting & 1L) ? highChr : lowChr;
	setting >>= 1;
    }

    return theString[pos];
}

/*****************************************************************************/
/* FUNCTION: phComIOpen()                                                    */
/* DESCRIPTION:                                                              */
/*            iopen gio/simulator                                           */
/*****************************************************************************/
phSiclInst_t phComIOpen(
    const char *addr          /* the name of the interface */,
    phComMode_t mode          /* on/off line, simulation, etc. */,
    phComIfcType_t intfc      /* GPIB */,
    phLogId_t loggerId        /* here we log messages and errors */
)
{
    struct phSiclInst *tmpId = NULL;

    /* create new ID struct */
    tmpId = PhNew(struct phSiclInst);
    if (!tmpId)
    {
        phLogComMessage(loggerId, PHLOG_TYPE_FATAL,
	    "not enough memory during call to phComIOpen()");
        return NULL;
    }

    /* initialize */
    tmpId->mode = mode;
    tmpId->intfc = intfc;
    tmpId->name = strdup(addr);
    tmpId->isMaster = (intfc != PHCOM_IFC_GPIB) ? 0 :
	(strstr(addr, "cmdr") ? 0 : 1);
    if (intfc == PHCOM_IFC_GPIB)
    {
	if (mode == PHCOM_OFFLINE)
	    if (tmpId->isMaster)
		strcpy(tmpId->card, "OFF-LINE GPIB MASTER");
	    else
		strcpy(tmpId->card, "OFF-LINE GPIB SLAVE ");
	else
	    if (tmpId->isMaster)
		strcpy(tmpId->card, "GPIB MASTER");
	    else
		strcpy(tmpId->card, "GPIB SLAVE ");	    
    }
    
    tmpId->loggerId = loggerId;
    tmpId->lastDataResult = ~0UL;
    tmpId->lastStatResult = ~0UL;
    tmpId->lastInfoResult = ~0UL;
    tmpId->lastLatchResult = ~0UL;

    switch (mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(loggerId, LOG_VERBOSE, "iopen()");
#endif
	tmpId->gioId = gio_open(addr);
        if (tmpId->gioId == 0)   /* 0 returned if bad */
        {
	    if (intfc == PHCOM_IFC_GPIB)
		phLogComMessage(loggerId, PHLOG_TYPE_ERROR,
		    "unable to establish gpib gio session for interface \"%s\"", addr);
	    free(tmpId);
            tmpId = NULL;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(loggerId, LOG_VERBOSE,
	    "offline iopen()");
#endif
	tmpId->gioId = 0;
	break;
    }

    /* pretty print the action */
    if (tmpId)
    {
	if (intfc == PHCOM_IFC_GPIB)
	{
	    phLogComMessage(loggerId, LOG_DEBUG,
		"%s Timestamp                Communication", tmpId->card);	    
	    phLogComMessage(loggerId, LOG_DEBUG,
		"%s", tmpId->card);
	    phLogComMessage(loggerId, LOG_DEBUG,
		"%s %s opened %s", tmpId->card,	getTimeString(1), addr);
	}
    }

    return tmpId;
}

/*****************************************************************************/
/* FUNCTION: phComIClose()                                                   */
/* DESCRIPTION:                                                              */
/*            iclose gio/simulator                                          */
/*****************************************************************************/
phComError_t phComIClose(
    phSiclInst_t session      /* The session ID of an open session. */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
	phLogComMessage(session->loggerId, LOG_VERBOSE, "iclose()"); 
#endif
        if ((gioRtnVal = gio_close(session->gioId)))
        {
            phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR,
		"iclose() failed: %s", gio_get_error_str(0, gioRtnVal));
            rtnValue = PHCOM_ERR_GPIB_ICLOSE_FAIL;
        }
	session->gioId = 0;
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline iclose()");
#endif
	break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
	if (session->intfc == PHCOM_IFC_GPIB)
	{
	    phLogComMessage(session->loggerId, LOG_DEBUG, 
		"%s %s closed", session->card,
		getTimeString(1));	    
	}
    }

    return rtnValue;
}


/*****************************************************************************/
/* FUNCTION: phComIClear()                                                   */
/* DESCRIPTION:                                                              */
/*            iclear gio/simulator                                          */
/*****************************************************************************/
phComError_t phComIClear(
    phSiclInst_t session      /* The session ID of an open session. */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
	phLogComMessage(session->loggerId, LOG_VERBOSE, "iclear()"); 
#endif
        if ((gioRtnVal = gio_clear(session->gioId)))
        {
            phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR,
		"iclear() failed: %s", gio_get_error_str(session->gioId, gioRtnVal));
	    if (session->intfc == PHCOM_IFC_GPIB)
		rtnValue = PHCOM_ERR_GPIB_ICLEAR_FAIL;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline iclear()");
#endif
	break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
	if (session->intfc == PHCOM_IFC_GPIB)
	{
	    phLogComMessage(session->loggerId, LOG_DEBUG, 
		"%s %s cleared", session->card,
		getTimeString(1));	    
	}
    }

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComILock()                                                    */
/* DESCRIPTION:                                                              */
/*            ilock gio/simulator call                                      */
/*****************************************************************************/
phComError_t phComILock(
    phSiclInst_t session      /* The session ID of an open session. */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "ilock()");
#endif

        /* lock session */ 
        if ((gioRtnVal = gio_lock(session->gioId)))
	{
            phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR, 
		"ilock() failed: %s", gio_get_error_str(session->gioId, gioRtnVal));
            rtnValue = PHCOM_ERR_GPIB_ILOCK_FAIL;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline ilock()");
#endif
	break;
    }

    return rtnValue;
}


/*****************************************************************************/
/* FUNCTION: phComIUnlock()                                                  */
/* DESCRIPTION:                                                              */
/*            iunlock gio/simulator call                                    */
/*****************************************************************************/
phComError_t phComIUnlock(
    phSiclInst_t session      /* The session ID of an open session. */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "iunlock()");
#endif

        /* unlock session */ 
        if ((gioRtnVal = gio_unlock(session->gioId)))
	{
            phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR,
		"iunlock() failed: %s", gio_get_error_str(session->gioId, gioRtnVal));
            rtnValue = PHCOM_ERR_GPIB_IUNLOCK_FAIL;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline iunlock()");
#endif
	break;
    }

    return rtnValue;
}


/*****************************************************************************/
/* FUNCTION: phComIIntrOn()                                                  */
/* DESCRIPTION:                                                              */
/*            iintron gio/simulator call                                    */
/*****************************************************************************/
phComError_t phComIIntrOn(
    phComMode_t mode          /* the communication mode */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    switch (mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
        /* enable interrupts */ 
        if ((gioRtnVal = gio_resume_events()))
	{
            rtnValue = PHCOM_ERR_UNKNOWN;
        }
	break;
      case PHCOM_OFFLINE:
	break;
    }

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComIIntrOff()                                                 */
/* DESCRIPTION:                                                              */
/*            iintroff gio/simulator call                                   */
/*****************************************************************************/
phComError_t phComIIntrOff(
    phComMode_t mode          /* the communication mode */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    switch (mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
        /* disable interrupts */ 
        if ((gioRtnVal = gio_suspend_events()))
	{
            rtnValue = PHCOM_ERR_UNKNOWN;
        }
	break;
      case PHCOM_OFFLINE:
	break;
    }

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComIOnSRQ()                                                   */
/* DESCRIPTION:                                                              */
/*            ionsrq gio/simulator call                                     */
/*****************************************************************************/
static void onlineSrqHandler(IDID Id)
{
    /* online GIO SRQ handler */

    struct onlineSRQMap *current = onlSRQMap;
    int handled = 0;

    while (current && !handled)
    {
	if (current->bridgeId->gioId == Id)
	{
	    /* handler found, call it */
	    (current->handler)(current->bridgeId, current->srqPtr);
	    handled = 1;
	}
	else
	    current = current->next;
    }

    if (!handled)
    {
	/* exceptional case */
	fprintf(stderr, "gioBridge: a SRQ occured for an unknown GIO session\n");
    }
}

phComError_t phComIOnSRQ(
    phSiclInst_t session      /* The session ID of an open session. */,
    phComSRQHandler_t handler /* the SRQ handler function */,
    void *ptr                 /* customizable pointer to some
                                 arbitrary memory location, will be
                                 delivered during SRQ to the handler
                                 function as last argument */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;
    struct onlineSRQMap *tmpEntry = NULL;

    CheckSession(session);

    /* this function must be protected */
    phComIIntrOff(session->mode);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "ionsrq()");
#endif

	/* create new SRQ map entry */
	tmpEntry = PhNew(struct onlineSRQMap);
	if (!tmpEntry)
	    rtnValue = PHCOM_ERR_MEMORY;
	else
	{
	    tmpEntry->bridgeId = session;
	    tmpEntry->handler = handler;
	    tmpEntry->srqPtr = ptr;
	    tmpEntry->next = onlSRQMap;

	    /* create interrupt handler */ 
	    if ((gioRtnVal = gio_set_SRQ_handler(session->gioId, onlineSrqHandler)))
	    {
		phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR, 
		    "ionsrq() failed: %s", gio_get_error_str(session->gioId, gioRtnVal));
		rtnValue = PHCOM_ERR_GPIB_IONSRQ_FAIL;
		free (tmpEntry);
	    }
	    else
	    {
		onlSRQMap = tmpEntry;
	    }
	}
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline ionsrq()");
#endif
	break;
    }

    /* end of protection */
    phComIIntrOn(session->mode);

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComITimeout()                                                 */
/* DESCRIPTION:                                                              */
/*            itimeout gio/simulator call                                   */
/*****************************************************************************/
phComError_t phComITimeout(
    phSiclInst_t session      /* The session ID of an open session. */,
    long tval                 /* timout in milli seconds */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "itimeout(%ld)", tval);
#endif

        /* set timeout */ 
        if ((gioRtnVal = gio_set_timeout(session->gioId, tval)))
	{
            phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR,
		"itimeout(%ld) failed: %s", tval, gio_get_error_str(session->gioId, gioRtnVal));
	    if (session->intfc == PHCOM_IFC_GPIB)
		rtnValue = PHCOM_ERR_GPIB_ITIMEOUT_FAIL;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline itimeout()");
#endif
	break;
    }

    return rtnValue;
}


/*****************************************************************************/
/* FUNCTION: phComIWaitHdlr()                                                */
/* DESCRIPTION:                                                              */
/*            iwaithdlr gio/simulator call                                  */
/*****************************************************************************/
phComError_t phComIWaitHdlr(
    phSiclInst_t session      /* The session ID of an open session. */,
    long tval                 /* timout in milli seconds */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "iwaithdlr(%ld)", tval);
#endif

        /* set timeout and wait */ 
        if ((gioRtnVal = gio_wait_event(tval)))
	{
            phLogComMessage(session->loggerId, 
		gioRtnVal == GIO_ERR_IO_TIMEOUT ? LOG_DEBUG : PHLOG_TYPE_ERROR,
		"iwaithdlr(%ld) failed: %s", tval, gio_get_error_str(session->gioId, gioRtnVal));
	    rtnValue = (gioRtnVal == GIO_ERR_IO_TIMEOUT) ? 
		PHCOM_ERR_TIMEOUT : PHCOM_ERR_UNKNOWN;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline iwaithdlr()");
#endif
	break;
    }

    return rtnValue;
}


/*****************************************************************************/
/* FUNCTION: phComITermChr()                                                 */
/* DESCRIPTION:                                                              */
/*            itermchr gio/simulator call                                   */
/*****************************************************************************/
phComError_t phComITermChr(
    phSiclInst_t session      /* The session ID of an open session. */,
    int tchr                  /* termination character */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "itermchr(%d)", tchr);
#endif

        /* set termination character */ 
        if ((gioRtnVal = gio_set_termchr(session->gioId, tchr)))
	{
            phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR,
		"itermchr(%d) failed: %s", tchr, gio_get_error_str(session->gioId, gioRtnVal));
	    if (session->intfc == PHCOM_IFC_GPIB)
		rtnValue = PHCOM_ERR_GPIB_ITERMCHR_FAIL;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline itermchr()");
#endif
	break;
    }

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComIWrite()                                                   */
/* DESCRIPTION:                                                              */
/*            iwrite gio/simulator call                                     */
/*****************************************************************************/
phComError_t phComIWrite(
    phSiclInst_t session      /* The session ID of an open session. */,
    char *buf                 /* buffer to write from */,
    unsigned long datalen     /* number of bytes to write */,
    int endi                  /* send end indicator, if set */,
    unsigned long *actual     /* number of written bytes */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, 
	    "iwrite(\"%s\", %ld, %d)", buf, datalen, endi);
#endif

        /* write string to GIO interface */ 
        if ((gioRtnVal = gio_write(session->gioId, buf, datalen, endi, (size_t*)actual)))
	{
            phLogComMessage(session->loggerId, 
		gioRtnVal == GIO_ERR_IO_TIMEOUT ? LOG_DEBUG : PHLOG_TYPE_ERROR,
		"iwrite(\"%s\", %ld, %d) failed: %s", buf, datalen, endi, gio_get_error_str(session->gioId, gioRtnVal));
	    rtnValue = (gioRtnVal == GIO_ERR_IO_TIMEOUT) ? 
		PHCOM_ERR_TIMEOUT : PHCOM_ERR_UNKNOWN;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline iwrite()");
#endif
	*actual = datalen;
	break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
	if (session->intfc == PHCOM_IFC_GPIB)
	{
	    if (*actual == datalen)
		phLogComMessage(session->loggerId, LOG_DEBUG, 
		    "%s %s MSG send %s \"%s\"", session->card,
		    getTimeString(1), session->isMaster ? "--> " : " <--", buf);
	    else
		phLogComMessage(session->loggerId, LOG_DEBUG, 
		    "%s %s MSG send %s \"%s\" INCOMPLETE, %d bytes", session->card,
		    getTimeString(1), session->isMaster ? "--> " : " <--", buf, *actual);

	    /* save the history */
	    phLogComMessage(session->loggerId, PHLOG_TYPE_COMM_HIST,
	        "(%s) T ---> E: \"%s\"", getTimeString(1), buf);
	}
    }

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComIRead()                                                    */
/* DESCRIPTION:                                                              */
/*            iread gio/simulator call                                      */
/*****************************************************************************/
phComError_t phComIRead(
    phSiclInst_t session      /* The session ID of an open session. */,
    char *buf                 /* buffer to read to */,
    unsigned long bufsize     /* size of buffer */,
    int *reason               /* returned reason for end of reading */,
    unsigned long *actual     /* returned number of read bytes */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, 
	    "iread(P%p, %ld)", buf, bufsize);
#endif

        /* read string from GIO interface */
	buf[0] = '\0';
        if ((gioRtnVal = gio_read(session->gioId, buf, bufsize, reason, (size_t*)actual)))
	{
            phLogComMessage(session->loggerId, 
		gioRtnVal == GIO_ERR_IO_TIMEOUT ? LOG_DEBUG : PHLOG_TYPE_ERROR,
		"iread(-> \"%s\", %ld, -> %d, -> %ld) failed: %s", 
		buf, bufsize, *reason, *actual, gio_get_error_str(session->gioId, gioRtnVal));
	    rtnValue = (gioRtnVal == GIO_ERR_IO_TIMEOUT) ? 
		PHCOM_ERR_TIMEOUT : PHCOM_ERR_UNKNOWN;
        }
	else
	    buf[*actual] = '\0';
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "offline iread()");
#endif
	if (bufsize > 1 && buf)
	{
	    strcpy(buf, "\n");
	    *actual = 1;
	}
	else
	    *actual = 0;
	*reason = GIO_REASON_END;
	break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
	if (session->intfc == PHCOM_IFC_GPIB)
	{
	    phLogComMessage(session->loggerId, LOG_DEBUG, 
		"%s %s MSG recv %s \"%s\"", session->card,
		getTimeString(1), session->isMaster ? "<-- " : " -->", buf);

            /* save the history */
            phLogComMessage(session->loggerId, PHLOG_TYPE_COMM_HIST,
                "(%s) T <--- E: \"%s\"",  getTimeString(1), buf);
	}
    }

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComISetStb()                                                  */
/* DESCRIPTION:                                                              */
/*            isetstb gio/simulator                                         */
/*****************************************************************************/
phComError_t phComISetStb(
    phSiclInst_t session        /* The session ID of an open GPIB session. */,
    unsigned char stb           /* status byte to set */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;
    char timestamp[80];

    CheckSession(session);

    /* get the timestamp now, before the SRQ we are going to launch is
       handled... */
    strncpy(timestamp, getTimeString(1), sizeof(timestamp));
    timestamp[sizeof(timestamp)-1] = '\0';

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "isetstb(%d)", (int) stb);
#endif

        /* set status byte */ 
        if ((gioRtnVal = gio_set_status_byte(session->gioId, stb)))
	{
            phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR,
		"isetstb(%d) failed: %s", (int) stb, gio_get_error_str(session->gioId, gioRtnVal));
            rtnValue = PHCOM_ERR_UNKNOWN;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, 
	    "offline isetstb()");
#endif
	break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
	if (session->intfc == PHCOM_IFC_GPIB)
	{
	    phLogComMessage(session->loggerId, LOG_DEBUG, 
		"%s %s SRQ set  %s 0x%02x (%d)", session->card,
		timestamp, session->isMaster ? "--> " : " <--", 
		(int) stb, (int) stb);
	}
    }

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComIReadStb()                                                 */
/* DESCRIPTION:                                                              */
/*            ireadstb gio/simulator                                        */
/*****************************************************************************/
phComError_t phComIReadStb(
    phSiclInst_t session        /* The session ID of an open GPIB session. */,
    unsigned char *stb          /* status byte to be returned */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "ireadstb()");
#endif

        /* set status byte */ 
        if ((gioRtnVal = gio_read_status_byte(session->gioId, stb)))
	{
            phLogComMessage(session->loggerId, 
		gioRtnVal == GIO_ERR_IO_TIMEOUT ? LOG_DEBUG : PHLOG_TYPE_ERROR,
		"ireadstb() failed: %s", gio_get_error_str(session->gioId, gioRtnVal));
            rtnValue = (gioRtnVal == GIO_ERR_IO_TIMEOUT) ? 
		PHCOM_ERR_TIMEOUT : PHCOM_ERR_UNKNOWN;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, 
	    "offline ireadstb()");
#endif
	*stb = 0;
	break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
	if (session->intfc == PHCOM_IFC_GPIB)
	{
	    phLogComMessage(session->loggerId, LOG_DEBUG, 
		"%s %s SRQ recv %s 0x%02x (%d)%s", session->card,
		getTimeString(1), session->isMaster ? "<-- " : " -->", 
		(int) *stb, (int) *stb, (*stb & 0x40) ? " (to FIFO)" : "");

            /* save the history */
	    phLogComMessage(session->loggerId, PHLOG_TYPE_COMM_HIST,
	                    "(%s) T <--- E: 0x%02x", getTimeString(1), *stb);
	}
    }

    return rtnValue;
}

/*****************************************************************************/
/* FUNCTION: phComIgpibAtnCtl()                                              */
/* DESCRIPTION:                                                              */
/*            igpibatnctl gio/simulator                                     */
/*****************************************************************************/
phComError_t phComIgpibAtnCtl(
    phSiclInst_t session        /* The session ID of an open GPIB session. */,
    int atnval                  /* assert ATN line, if set                 */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    GIO_ERROR gioRtnVal = GIO_ERR_NO_ERROR;

    CheckSession(session);

    switch (session->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "igpibatnctl(%d)", atnval);
#endif

        /* assert or release ATN line */ 
        if ((gioRtnVal = gio_gpib_ATN_control(gio_get_interface(session->gioId), atnval)))
	{
            phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR,
		"igpibatnctl() failed: %s", gio_get_error_str(session->gioId, gioRtnVal));
            rtnValue = PHCOM_ERR_GPIB_IGPIBATNCTL_FAIL;
        }
	break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, 
	    "offline igpibatnctl()");
#endif
	break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
	phLogComMessage(session->loggerId, LOG_DEBUG, 
	    "%s %s ATN %s", session->card,
	    getTimeString(1), atnval ? "asserted" : "released");
    }

    return rtnValue;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
