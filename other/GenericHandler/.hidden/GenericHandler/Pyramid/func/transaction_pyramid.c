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
 *            6 Dec 2014, Xiaofei Han, add function phFuncTaClearGpib
 *            7 May 2015, Magco Li, add rs232 interface to phFuncTaSend() and phFuncTaReceive()
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
#include <ctype.h>

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
#include "transaction.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static void phFuncTaPush(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    struct transaction *t;

    t = PhNew(struct transaction);
    if (handlerID->ta)
    *t = *(handlerID->ta);
    else
    {
    t->step = 0;
    t->stepsDone = 0;
    t->stepMark = 0;
    t->stepsDoneMark = 0;
    t->doAbort = 0;
    t->lastCall = (phFuncAvailability_t) 0;
    t->currentCall = (phFuncAvailability_t) 0;
    t->handler = NULL;
    t->doPush = 0;
    }
    t->pop = handlerID->ta;
    handlerID->ta = t;
}

void phFuncTaInit(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    handlerID->ta = NULL;
    phFuncTaPush(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction control initialized");
}

void phFuncTaSetCall(
    phFuncId_t handlerID      /* driver plugin ID */,
    phFuncAvailability_t call
)
{
    handlerID->ta->lastCall = handlerID->ta->currentCall;
    handlerID->ta->currentCall = call;
    handlerID->ta->step = 0;
    if (handlerID->ta->lastCall != handlerID->ta->currentCall)
    {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
        "transaction control reset for new call");
    handlerID->ta->stepsDone = 0;
    }
    else
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
        "transaction control reset for repeated call");
}

void phFuncTaSetSpecialCall(
    phFuncId_t handlerID      /* driver plugin ID */,
    phFuncAvailability_t call
)
{
    /* not yet implemented.  Task: for the next TaStart - TaStop
       bracket, use a new, independent transaction stack */

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction control recognized special call");
}

void phFuncTaGetLastCall(
    phFuncId_t handlerID      /* driver plugin ID */,
    phFuncAvailability_t *call
)
{
    *call = handlerID->ta->lastCall;
}

void phFuncTaStart(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction started,step:%d,stepDone:%d", handlerID->ta->step,handlerID->ta->stepsDone);
    handlerID->ta->step = 0;
    handlerID->ta->doAbort = 0;
}

void phFuncTaStop(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction stopped");
    handlerID->ta->step = 0;
    handlerID->ta->stepsDone = 0;
    handlerID->ta->doAbort = 0;
}


int phFuncTaAskAbort(
                    phFuncId_t handlerID      /* driver plugin ID */
                    )
{
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                     "entering phFuncTaAskAbort, doAbort = %x, lastCall = %x, currentCall = %x",
                     handlerID->ta->doAbort, handlerID->ta->lastCall, handlerID->ta->currentCall );
    if ( handlerID->ta->doAbort )
    {
        handlerID->ta->doAbort = 0;
        if ( handlerID->ta->lastCall == handlerID->ta->currentCall )
        {
            phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                             "transaction control perform abort");
            return 1;
        }
        else
            return 0;
    }
    else
        return 0;
}

void phFuncTaDoAbort(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction control schedule abort");
    handlerID->ta->doAbort = 1;
}

void phFuncTaDoReset(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction control reset");
    handlerID->ta->stepsDone = 0;
    handlerID->ta->step = 0;
    handlerID->ta->doAbort = 0;
}

void phFuncTaRemoveStep(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction control removed last step");
    handlerID->ta->stepsDone--;
    handlerID->ta->step--;
}

void phFuncTaMarkStep(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    handlerID->ta->step++;
    if (handlerID->ta->stepsDone >= handlerID->ta->step)
    {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
        "NOT marking current step again, already marked");
    return;
    }

    handlerID->ta->stepsDone++;

    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction control marked current step");
    handlerID->ta->stepsDoneMark = handlerID->ta->stepsDone - 1;
    handlerID->ta->stepMark = handlerID->ta->step - 1;
}

void phFuncTaRemoveToMark(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
    "transaction control removed steps up to marked position");
    handlerID->ta->stepsDone = handlerID->ta->stepsDoneMark;
    handlerID->ta->step = handlerID->ta->stepMark;
}

void phFuncTaAddSRQHandler(
    phFuncId_t handlerID      /* driver plugin ID */,
    int srq                   /* srq to handle */,
    phFuncTaSRQHandlerFoo_t *foo /* the handler function to use */
)
{
    if(handlerID->interfaceType == PHFUNC_IF_LAN)
      return;

    struct srqHandler *newHandler;

    newHandler = PhNew(struct srqHandler);
    newHandler->srq = srq;
    newHandler->foo = foo;
    newHandler->next = handlerID->ta->handler;
    handlerID->ta->handler = newHandler;
}

/*****************************************************************************
 *
 * Perform a message send step
 *
 * Authors: Michael Vogt
 *
 * History: 16 Mar 2015, Magco Li , add function phComRs232Send()
 *
 * Description:
 * please refer to transaction.h
 *
 * Returns: error code
 *
 ***************************************************************************/

static void copyMsg( char* buf, const char* src,int len)
{
    int i =0;
    while(i<len)
    {
      buf[i] = src[i];
      ++i;
    }
}

static void copyMsgForReceive( char* buf, const char* src,int len)
{
    int i =0;
    int j = 3;
    while(i<len)
    {
      buf[i] = src[j++];
      ++i;
    }
}
static void printMsg(phFuncId_t handlerID ,char* data,int len)
{
     phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"step in printMsg,len:%d\n",len);
    for(int i=0;i<len;++i)
       phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"%d,",data[i]&&0xff);
    
}
phFuncError_t phFuncTaSend(
    phFuncId_t handlerID     /* driver plugin ID */,
    const char *format             /* format string like in printf */,
    ...                      /* variable argument list */
)
{
    va_list argp;
    phComError_t comError = PHCOM_ERR_OK;
    char command[PHESTATE_MAX_SITES * 16] = {0};
    int lenForLan = 0;

    if (PHCONF_ERR_OK == phConfConfIfDef(handlerID->myConf, PHKEY_NAME_IS_SECS))
    {
        int  found = 0;
        phConfError_t confError = phConfConfStrTest(&found, handlerID->myConf, PHKEY_NAME_IS_SECS, "yes", "no", NULL);
        if(  confError == PHCONF_ERR_OK)
        {
             /* "yes" has been found in configuration file */
            if( found == 1 )
            {
                
                va_start(argp,format);
                lenForLan = va_arg(argp,int);
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"message type:secs,the length of sending msg is:%d",lenForLan);
                char* p = va_arg(argp, char*);
                copyMsg(command,p,lenForLan);
                va_end(argp);

            }
        }
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"message type:not secs");
        va_start(argp, format);
        vsprintf(command, format, argp);
        va_end(argp);
    }

    handlerID->ta->step++;
    if (handlerID->ta->stepsDone >= handlerID->ta->step)
    {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
        "NOT sending message, was already sent, steps:%d,stepsDone:%d", handlerID->ta->step,handlerID->ta->stepsDone);
    return PHFUNC_ERR_HAVE_DEAL;
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
	"trying to send message to handler, timeout = %ld", handlerID->heartbeatTimeout);
    if(handlerID->interfaceType == PHFUNC_IF_GPIB)
    {
      /* send command over the GPIB link */
      comError = phComGpibSend(handlerID->myCom, command, strlen(command),handlerID->heartbeatTimeout);
    }
    else if(handlerID->interfaceType == PHFUNC_IF_LAN || handlerID->interfaceType == PHFUNC_IF_LAN_SERVER)
    {
      /* send command over the LAN link */
      comError = phComLanSend(handlerID->myCom, command, lenForLan, handlerID->heartbeatTimeout);
    }
    if(handlerID->interfaceType == PHFUNC_IF_RS232)
    {
      /* send command over the RS232 link */
      comError = phComRs232Send(handlerID->myCom, command, strlen(command),handlerID->heartbeatTimeout);
    }

    switch (comError)
    {
      case PHCOM_ERR_OK:
    handlerID->ta->stepsDone++;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
        "message sent");
    return PHFUNC_ERR_OK;
      case PHCOM_ERR_TIMEOUT:
	phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
	    "sending message \"%s\" to handler timed out", command);
	return PHFUNC_ERR_TIMEOUT;
      default:
        if(handlerID->interfaceType == PHFUNC_IF_GPIB)
        {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                           "unexpected GPIB error");
          return PHFUNC_ERR_GPIB;
        }
        else if(handlerID->interfaceType == PHFUNC_IF_LAN || handlerID->interfaceType == PHFUNC_IF_LAN_SERVER)
        {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                           "unexpected LAN error");
          return PHFUNC_ERR_LAN;
        }
        else if(handlerID->interfaceType == PHFUNC_IF_RS232)
        {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                           "unexpected RS232 error, transaction is reset");
          phFuncTaStop(handlerID);
          return PHFUNC_ERR_RS232;
        }
    }

    return PHFUNC_ERR_OK;
}

/*****************************************************************************
 *
 * Perform a message receive step
 *
 * Authors: Michael Vogt
 *
 * History: 16 Mar 2015, Magco Li , add function phComRs232Receive()
 *
 * Description:
 * please refer to transaction.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phFuncError_t phFuncTaReceive(
                             phFuncId_t handlerID     /* driver plugin ID */,
                             int nargs                /* number of arguments in the variable part */,
                             const char *format       /* format string like in scanff */,
                             ...                      /* variable argument list */
                             )
{
    va_list argp;
    phComError_t comError = PHCOM_ERR_OK;
    const char *answer = NULL;
    int length = 0;
    phFuncError_t retVal = PHFUNC_ERR_OK;
    const char *ptr = 0;
    char *strArg = 0;
    int i = 0;


    handlerID->ta->step++;
    if ( handlerID->ta->stepsDone >= handlerID->ta->step )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "NOT trying to receive message from handler, already received,step:%d,stepDone:%d",handlerID->ta->step,handlerID->ta->stepsDone);
        return PHFUNC_ERR_OK;
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                     "trying to receive message from handler, timeout = %ld",
                     handlerID->heartbeatTimeout);
    if(handlerID->interfaceType == PHFUNC_IF_GPIB)
    {
      /* receive message over the GPIB link */
      comError = phComGpibReceive(handlerID->myCom, &answer, &length,handlerID->heartbeatTimeout);
    }
    else if(handlerID->interfaceType == PHFUNC_IF_LAN || handlerID->interfaceType == PHFUNC_IF_LAN_SERVER)
    {
      /* receive message over the LAN link */
      comError = phComLanReceive(handlerID->myCom, &answer, &length,handlerID->heartbeatTimeout);
    }
    else if(handlerID->interfaceType == PHFUNC_IF_RS232)
    {
      /* receive message over the RS232 link */
      comError = phComRs232Receive(handlerID->myCom, &answer, &length,handlerID->heartbeatTimeout);
    }


    switch ( comError )
    {
        case PHCOM_ERR_OK:
            handlerID->ta->stepsDone++;

            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                             "received from handler: \"%s\"", answer);


            if (PHCONF_ERR_OK == phConfConfIfDef(handlerID->myConf, PHKEY_NAME_IS_SECS))
            {
                int  found = 0;
                phConfError_t confError = phConfConfStrTest(&found, handlerID->myConf, PHKEY_NAME_IS_SECS, "yes", "no", NULL);
                if(  confError == PHCONF_ERR_OK )
                {
                    /* "yes" has been found in configuration file */
                    if( found == 1 )
                    {
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the receive msg type is:secs");
                        int len = answer[3];
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the length of receive msg  is:%d",len+1);
                        va_start(argp,format);
                        char* p = va_arg(argp, char*);
                        copyMsgForReceive(p,answer,len+1);
                        va_end(argp);

                    }
                }
            }
            else
            {
              /* exception: scanf can not parse empty strings, so, if the
                 format parameter is set to "%s", and if the received
                 message is either empty or only consists of non visible
                 characters, we will not use scanf but send back an empty
                 string instead */

              if ( strcmp(format, "%s") == 0 )
              {
                ptr = answer;
                if ( ptr != NULL )
                {
                  while ( *ptr != '\0' && !isgraph(*ptr) ) ptr++;
                  if ( *ptr == '\0' )
                  {
                    /* this is the exception, return empty string */
                    va_start(argp, format);
                    strArg = va_arg(argp, char *);
                    *strArg = '\0';
                    va_end(argp);
                    // simulate time-out so framework will try again
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
                        "received blank message from handler \"%s\", timing out", answer);
                    //                    retVal = PHFUNC_ERR_ANSWER;
                    retVal = PHFUNC_ERR_WAITING;
                    break;
                  }
                }
              }

              if(strcmp(format,"%s")==0)
              {
                va_start(argp,format);
                char *p = va_arg(argp,char *);
                strcpy(p,answer);
                while(*p != '\0')
                {
                  if( (*p=='\r') || (*p=='\n'))
                    *p = '\0';
                  ++p;
                }
              }
              else{
                /* this is the regular case, use scanf, to parse the string */

                va_start(argp, format);
                i = vsscanf(answer, format, argp);
                if ( i != nargs )
                {
                  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                      "message from handler not understood\n"
                      "expected format \"%s\", got answer \"%s\"\n",
                      format, answer);
                  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                      "vsscanf returned %d, expected %d\n",
                      i, nargs);
                  retVal = PHFUNC_ERR_ANSWER;
                }
                va_end(argp);
              }

            }

            break;
        case PHCOM_ERR_TIMEOUT:
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
                             "receiving message from handler timed out");
            retVal = PHFUNC_ERR_WAITING;
            break;
        default:
            if(handlerID->interfaceType == PHFUNC_IF_GPIB)
            {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                               "unexpected GPIB error");
              retVal = PHFUNC_ERR_GPIB;
            }
            else if(handlerID->interfaceType == PHFUNC_IF_LAN || handlerID->interfaceType == PHFUNC_IF_LAN_SERVER)
            {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                               "unexpected LAN error");
              retVal = PHFUNC_ERR_LAN;
            }
            else if(handlerID->interfaceType == PHFUNC_IF_RS232)
            {
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                               "unexpected RS232 error, transaction is reset");
              phFuncTaStop(handlerID);
              retVal = PHFUNC_ERR_RS232;
            }
            break;
    }

    return retVal;
}

/*****************************************************************************
 *
 * Clear the GPIB interface
 *
 * Authors: Xiaofei Han
 *
 * History: 16 Dec 2014, Xiaofei Han , created
 *
 * Description:
 * please refer to transaction.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phFuncError_t phFuncTaClearGpib( phFuncId_t handlerID )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phComError_t comError = PHCOM_ERR_OK;

    handlerID->ta->step++;
    if ( handlerID->ta->stepsDone >= handlerID->ta->step )
    {
        phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
                         "NOT trying to clear device, already cleared");
        return PHFUNC_ERR_OK;
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Ready to clear the GPIB interface");
    comError = phComGpibClear(handlerID->myCom);
    if(comError == PHCOM_ERR_OK)
    {
        handlerID->ta->stepsDone++;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Successfully clear GPIB interface");
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "Failed to clear GPIB interface, retry...");
        retVal = PHFUNC_ERR_WAITING;
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
int callSRQHandler(phFuncId_t handlerID, int srq, phFuncError_t *retVal)
{
    struct srqHandler *current;
    int called = 0;

    if(handlerID->interfaceType == PHFUNC_IF_LAN)
      return 0;

    current = handlerID->ta->handler;
    while (current && !called)
    {
    if (srq == current->srq)
    {
        *retVal = (current->foo)(handlerID, srq);
        called = 1;
    }
    current = current->next;
    }

    return called;
}


phFuncError_t phFuncTaGetSRQ(
    phFuncId_t handlerID     /* driver plugin ID */,
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
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int done;
    int candidate;
    char expected[PHESTATE_MAX_SITES * 16];
    int snargs;

    if(handlerID->interfaceType == PHFUNC_IF_LAN)
      return PHFUNC_ERR_OK;

    snargs = nargs;
    handlerID->ta->step++;
    if (handlerID->ta->stepsDone >= handlerID->ta->step)
    {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
        "NOT trying to receive SRQ from handler, already received");
    return PHFUNC_ERR_OK;
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
    "trying to receive SRQ from handler, timeout = %l",
        handlerID->heartbeatTimeout);
    comError = phComGpibGetEvent(handlerID->myCom, &event, &pending,
    handlerID->heartbeatTimeout);

    switch (comError)
    {
      case PHCOM_ERR_OK:
    if (event)
    {
        handlerID->ta->stepsDone++;
        switch (event->type)
        {
          case PHCOM_GPIB_ET_SRQ:
        *srq = event->d.srq_byte;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
            "received from handler: SRQ 0x%02x", *srq);
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
            done = callSRQHandler(handlerID, *srq, &retVal);
        if (!done && snargs>0)
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
            "SRQ from handler not understood, "
            "got 0x%02x, expected one of [ %s]\n",
            *srq, expected);
            retVal = PHFUNC_ERR_ANSWER;
        }
        break;
          default:
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
            "event from handler not understood");
        retVal = PHFUNC_ERR_ANSWER;
        break;
        }
    }
    else
        retVal = PHFUNC_ERR_WAITING;
    break;
      case PHCOM_ERR_TIMEOUT:
	phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
        "receiving SRQ from handler timed out");
        retVal = PHFUNC_ERR_WAITING;
    break;
      default:
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
        "unexpected GPIB error");
    retVal = PHFUNC_ERR_GPIB;
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
phFuncError_t phFuncTaTestSRQ(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *received            /* set, if another srq was received */,
    int *srq                 /* received SRQ byte */,
    int fast                 /* fast return, if set. Otherwise a
                                timeout is applied, if no SRQ is
                                pending */
)
{
    phComError_t comError;
    phComGpibEvent_t *event;
    int pending;
    phFuncError_t retVal = PHFUNC_ERR_OK;

    if(handlerID->interfaceType == PHFUNC_IF_LAN)
      return PHFUNC_ERR_OK;

    handlerID->ta->step++;
    if (handlerID->ta->stepsDone >= handlerID->ta->step)
    {
    phLogFuncMessage(handlerID->myLogger, LOG_VERBOSE,
        "NOT trying to receive SRQ from handler, already received");
    return PHFUNC_ERR_OK;
    }

    *received = 0;

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
    "calling phComGpibGetEvent to receive SRQ, timeout = %d, fast = %d",
         handlerID->heartbeatTimeout, fast);

    comError = phComGpibGetEvent(handlerID->myCom, &event, &pending,
    fast ? 0L : handlerID->heartbeatTimeout);

    switch (comError)
    {
      case PHCOM_ERR_OK:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
    "after phComGpibGetEvent, returned = PHCOM_ERR_OK, event = %d, pending = %d",
        event, pending);
    if (event)
    {
        handlerID->ta->stepsDone++;
        switch (event->type)
        {
          case PHCOM_GPIB_ET_SRQ:
        *received = 1;
        *srq = event->d.srq_byte;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
            "received from handler: SRQ 0x%02x", *srq);
        callSRQHandler(handlerID, *srq, &retVal);
        break;
          default:
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
            "event from handler not understood");
        retVal = PHFUNC_ERR_ANSWER;
        break;
        }
    }
    else
    {
        /* no additional SRQ received within timeout */
	    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
        "no additional SRQ received from handler");
        handlerID->ta->stepsDone++;
        retVal = PHFUNC_ERR_OK;
    }
    break;
      case PHCOM_ERR_TIMEOUT:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
    "after phComGpibGetEvent, returned = PHCOM_ERR_TIMEOUT, event = %d, pending = %d",
        event, pending);
    /* no additional SRQ received within timeout */
	phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,
        "no additional SRQ received from handler");
    handlerID->ta->stepsDone++;
    retVal = PHFUNC_ERR_OK;
    break;
      default:
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,
    "after phComGpibGetEvent, returned = %d, event = %d, pending = %d",
        comError, event, pending);
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
        "unexpected GPIB error");
    retVal = PHFUNC_ERR_GPIB;
    break;
    }

    return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
