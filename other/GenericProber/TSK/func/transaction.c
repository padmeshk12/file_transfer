/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : transaction.c
 * CREATED  : 31 Jan 2000
 *
 * CONTENTS : Basic GPIB transactions
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 31 Jan 2000, Michael Vogt, created
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
#include <stdarg.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_pfunc.h"

#include "gpib_conf.h"
#include "ph_pfunc_private.h"
#include "transaction.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static void phPFuncTaPush(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    struct transaction *t;

    t = PhNew(struct transaction);
    if (proberID->ta)
	*t = *(proberID->ta);
    else
    {
	t->step = 0;
	t->stepsDone = 0;
	t->doAbort = 0;
	t->lastCall = (phPFuncAvailability_t) 0;
	t->currentCall = (phPFuncAvailability_t) 0;
	t->handler = NULL;
	t->doPush = 0;
    }
    t->pop = proberID->ta;
    proberID->ta = t;
}

void phPFuncTaInit(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    proberID->ta = NULL;
    phPFuncTaPush(proberID);
    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	"transaction control initialized");
}

void phPFuncTaSetCall(
    phPFuncId_t proberID      /* driver plugin ID */,
    phPFuncAvailability_t call
)
{
    proberID->ta->lastCall = proberID->ta->currentCall;
    proberID->ta->currentCall = call;
    proberID->ta->step = 0;
    if (proberID->ta->lastCall != proberID->ta->currentCall)
    {
	phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	    "transaction control reset for new call");
	proberID->ta->stepsDone = 0;
    }
    else
	phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	    "transaction control reset for repeated call");	
}

void phPFuncTaSetSpecialCall(
    phPFuncId_t proberID      /* driver plugin ID */,
    phPFuncAvailability_t call
)
{
    /* not yet implemented.  Task: for the next TaStart - TaStop
       bracket, use a new, independent transaction stack */

    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	"transaction control recognized special call");
}

void phPFuncTaGetLastCall(
    phPFuncId_t proberID      /* driver plugin ID */,
    phPFuncAvailability_t *call
)
{
    *call = proberID->ta->lastCall;
}

void phPFuncTaStart(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	"transaction started");
    proberID->ta->step = 0;
    proberID->ta->doAbort = 0;
}

void phPFuncTaStop(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	"transaction stopped");
    proberID->ta->step = 0;
    proberID->ta->stepsDone = 0;
    proberID->ta->doAbort = 0;
}


int phPFuncTaAskAbort(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    if (proberID->ta->doAbort)
    {
	proberID->ta->doAbort = 0;
	if (proberID->ta->lastCall == proberID->ta->currentCall)
	{
	    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
		"transaction control perform abort");
	    return 1;
	}
	else
	    return 0;
    }
    else
	return 0;
}

void phPFuncTaDoAbort(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	"transaction control schedule abort");
    proberID->ta->doAbort = 1;
}

void phPFuncTaDoReset(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	"transaction control reset");
    proberID->ta->stepsDone = 0;
    proberID->ta->step = 0;
    proberID->ta->doAbort = 0;
}

void phPFuncTaRemoveStep(
    phPFuncId_t proberID      /* driver plugin ID */
)
{
    phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	"transaction control removed last step");
    proberID->ta->stepsDone--;
    proberID->ta->step--;
}

void phPFuncTaAddSRQHandler(
    phPFuncId_t proberID      /* driver plugin ID */,
    int srq                   /* srq to handle */,
    phPFuncTaSRQHandlerFoo_t *foo /* the handler function to use */
)
{
    struct srqHandler *newHandler;

    newHandler = PhNew(struct srqHandler);
    newHandler->srq = srq;
    newHandler->foo = foo;
    newHandler->next = proberID->ta->handler;
    proberID->ta->handler = newHandler;
}

/*****************************************************************************
 *
 * Perform a message send step
 *
 * Authors: Michael Vogt
 *
 * Description:
 * please refer to transaction.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phPFuncError_t phPFuncTaSend(
    phPFuncId_t proberID     /* driver plugin ID */,
    const char *format       /* format string like in printf */,
    ...                      /* variable argument list */
)
{
    va_list argp;
    phComError_t comError;
    char command[20480] = "";

    va_start(argp, format);
    vsprintf(command, format, argp);
    va_end(argp);
    
    proberID->ta->step++;
    if (proberID->ta->stepsDone >= proberID->ta->step)
    {
	phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	    "NOT sending message, was already sent: \"%s\"", command);
	return PHPFUNC_ERR_OK;
    }

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
	"trying to send message to prober: \"%s\"", command);
    comError = phComGpibSend(proberID->myCom, command, strlen(command),
	proberID->p.sendTimeout);

    switch (comError)
    {
      case PHCOM_ERR_OK:
	proberID->ta->stepsDone++;
	phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
	    "message sent");
	return PHPFUNC_ERR_OK;
      case PHCOM_ERR_TIMEOUT:
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
	    "sending message \"%s\" to prober timed out", command);
	return PHPFUNC_ERR_TIMEOUT;
      default:
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
	    "unexpected GPIB error");
	return PHPFUNC_ERR_GPIB;
    }
}

/*****************************************************************************
 *
 * Perform a message receive step
 *
 * Authors: Michael Vogt
 *
 * Description:
 * please refer to transaction.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phPFuncError_t phPFuncTaReceive(
    phPFuncId_t proberID     /* driver plugin ID */,
    int nargs                /* number of arguments in the variable part */,
    const char *format       /* format string like in scanff */,
    ...                      /* variable argument list */
)
{
    va_list argp;
    phComError_t comError;
    const char *answer;
    int length;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    proberID->ta->step++;
    if (proberID->ta->stepsDone >= proberID->ta->step)
    {
	phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	    "NOT trying to receive message from prober, already received");
	return PHPFUNC_ERR_OK;
    }

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
	"trying to receive message from prober");
    comError = phComGpibReceive(proberID->myCom, &answer, &length, 
	proberID->p.receiveTimeout);

    switch (comError)
    {
      case PHCOM_ERR_OK:
	phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
	    "received from prober: \"%s\"", answer);

	va_start(argp, format);
	if (vsscanf(answer, format, argp) != nargs)
	{
	    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
		"message from prober not understood\n"
		"expected format \"%s\", got answer \"%s\"\n",
		format, answer);
	    retVal = PHPFUNC_ERR_ANSWER;
	}
	va_end(argp);
	proberID->ta->stepsDone++;
	break;
      case PHCOM_ERR_TIMEOUT:
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
	    "receiving message from prober timed out");
	retVal = PHPFUNC_ERR_WAITING;
	break;
      default:
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
	    "unexpected GPIB error");
	retVal = PHPFUNC_ERR_GPIB;
	break;
    }

    return retVal;
}

/*****************************************************************************
 *
 * Perform a SRQ receive step
 *
 * Authors: Michael Vogt
 *
 * Description:
 * please refer to transaction.h
 *
 * Returns: error code
 *
 ***************************************************************************/
int callSRQHandler(phPFuncId_t proberID, int srq, phPFuncError_t *retVal)
{
    struct srqHandler *current;
    int called = 0;

    current = proberID->ta->handler;
    while (current && !called)
    {
	if (srq == current->srq)
	{
	    *retVal = (current->foo)(proberID, srq);
	    called = 1;
	}
	current = current->next;
    }

    return called;
}


phPFuncError_t phPFuncTaGetSRQ(
    phPFuncId_t proberID     /* driver plugin ID */,
    int *srq                 /* received SRQ byte */,
    int nargs                /* number of arguments in the variable
                                part */,
    ...                      /* variable argument list, must be a list
                                of (int) values, representing accepted
                                SRQs */
)
{
    va_list argp;
    phComError_t comError;
    phComGpibEvent_t *event;
    int pending;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;
    int done;
    int candidate;
    char expected[2048];
    int snargs;

    snargs = nargs;
    proberID->ta->step++;
    if (proberID->ta->stepsDone >= proberID->ta->step)
    {
	phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	    "NOT trying to receive SRQ from prober, already received");
	return PHPFUNC_ERR_OK;
    }

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
	"trying to receive SRQ from prober");
    comError = phComGpibGetEvent(proberID->myCom, &event, &pending, 
	proberID->p.receiveTimeout);

    switch (comError)
    {
      case PHCOM_ERR_OK:
	if (event)
	{
	    proberID->ta->stepsDone++;
	    switch (event->type)
	    {
	      case PHCOM_GPIB_ET_SRQ:
		*srq = event->d.srq_byte;
		phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
		    "received from prober: SRQ 0x%02x", *srq);
		done = 0;
		if (!done && snargs>0)
		{
		    va_start(argp, nargs);
		    expected[0] = '\0';
		    while (!done && nargs > 0)
		    {
			candidate = va_arg(argp, int);
			if (candidate == *srq)
			    done = 1;
			else
			{
			    sprintf(&expected[strlen(expected)], 
				"0x%02x ", candidate);
			}
			nargs--;
		    }
		    va_end(argp);
		}
		/* give the calling function a chance to handle the
                   SRQ before the SRQ handlers try to handle it */
		if (!done)
		    done = callSRQHandler(proberID, *srq, &retVal);
		if (!done && snargs>0)
		{
		    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
			"SRQ from prober not understood, "
			"got 0x%02x, expected one of [ %s]\n", *srq, expected);
		    retVal = PHPFUNC_ERR_ANSWER;
		}
		break;
	      default:
		phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
		    "event from prober not understood");
		retVal = PHPFUNC_ERR_ANSWER;
		break;
	    }
	}
	else
	    retVal = PHPFUNC_ERR_WAITING;
	break;
      case PHCOM_ERR_TIMEOUT:
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
	    "receiving SRQ from prober timed out");
	retVal = PHPFUNC_ERR_WAITING;
	break;
      default:
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
	    "unexpected GPIB error");
	retVal = PHPFUNC_ERR_GPIB;
	break;
    }

    return retVal;
}

/*****************************************************************************
 *
 * Test for another SRQ
 *
 * Authors: Michael Vogt
 *
 * Description:
 * please refer to transaction.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phPFuncError_t phPFuncTaTestSRQ(
    phPFuncId_t proberID     /* driver plugin ID */,
    int *received            /* set, if another srq was received */,
    int *srq                 /* received SRQ byte */
)
{
    phComError_t comError;
    phComGpibEvent_t *event;
    int pending;
    phPFuncError_t retVal = PHPFUNC_ERR_OK;

    proberID->ta->step++;
    if (proberID->ta->stepsDone >= proberID->ta->step)
    {
	phLogFuncMessage(proberID->myLogger, LOG_VERBOSE,
	    "NOT trying to receive SRQ from prober, already received");
	return PHPFUNC_ERR_OK;
    }

    *received = 0;

    phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
	"trying to receive SRQ from prober");
    comError = phComGpibGetEvent(proberID->myCom, &event, &pending, 
	proberID->p.receiveTimeout);

    switch (comError)
    {
      case PHCOM_ERR_OK:
	if (event)
	{
	    proberID->ta->stepsDone++;
	    switch (event->type)
	    {
	      case PHCOM_GPIB_ET_SRQ:
		*received = 1;
		*srq = event->d.srq_byte;
		phLogFuncMessage(proberID->myLogger, LOG_DEBUG,
		    "received from prober: SRQ 0x%02x", *srq);
		callSRQHandler(proberID, *srq, &retVal);
		break;
	      default:
		phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
		    "event from prober not understood");
		retVal = PHPFUNC_ERR_ANSWER;
		break;
	    }
	}
	else
	{
	    /* no additional SRQ received within timeout */
	    phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
		"no additional SRQ received from prober");
	    retVal = PHPFUNC_ERR_OK;
	}
	break;
      case PHCOM_ERR_TIMEOUT:
	/* no additional SRQ received within timeout */
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_WARNING,
	    "no additional SRQ received from prober");
	retVal = PHPFUNC_ERR_OK;
	break;
      default:
	phLogFuncMessage(proberID->myLogger, PHLOG_TYPE_ERROR,
	    "unexpected GPIB error");
	retVal = PHPFUNC_ERR_GPIB;
	break;
    }

    return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
