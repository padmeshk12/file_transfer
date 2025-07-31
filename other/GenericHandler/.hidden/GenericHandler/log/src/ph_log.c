/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_log.c
 * CREATED  : 14 Jun 1999
 *
 * CONTENTS : Implementation of debug log facility
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR42750
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jun 1999, Michael Vogt, created
 *            12 Dec 2007, Xiaofei Han, CR38227: block SIGIO before printing 
 *                         log messages to avoid smartest self-deadlock
 *            Oct 2008, Jiawei Lin, CR42750
 *                         support the "debug_level" value  set as "9" so as to
 *                         log the timestamp information in the message
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
#include <errno.h>
#include <signal.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"
#include "ph_log_private.h"
#include "ph_tools.h"
#include "dev/DriverDevKits.hpp"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/* don't trust anyone .... */
#define PH_HANDLE_CHECK

#ifdef DEBUG
#define PH_HANDLE_CHECK
#endif

#define COMM_HIST_FILE_PATH "/tmp/ph_comm_history"

#ifdef PH_HANDLE_CHECK
#define CheckHandle(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHLOG_ERR_INVALID_HDL
#else
#define CheckHandle(x)
#endif

#define CheckSuspend(x, y) \
    if ((x)->suspendLevel && (y)!=PHLOG_TYPE_FATAL && (y)!=PHLOG_TYPE_ERROR) \
        return PHLOG_ERR_INACTIVE

static char lastErrorMessage[PHLOG_MAX_MESSAGE_SIZE + 1];

static char savedErrorMsg[PHLOG_DFLTBUF_SIZE];

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static void lockOrUnlockSignals(int lock)
{
    sigset_t criticalSigset;
    static sigset_t restoreSigset;

    if (lock)
    {
	/* block SIGUSR2 and SIGALRM, since they are used by 
           the mhcom module. Retrieve the old state of these signal
           flags at the same time.
           CR38227: Added SIGIO here. The GIO driver uses SIGIO to capture the
           SRQs and srqHandler() is called as the signal handler. Unfortunately,
           logMessage() is  called in the srqHandler() function, in order to avoid
           the self-deadlock on the shared resources(stdout, stderr etc.), block the
           signal SIGIO before printing any log messages out.
         */
	sigemptyset(&criticalSigset);
	sigaddset(&criticalSigset, SIGUSR2);
	sigaddset(&criticalSigset, SIGALRM);
        sigaddset(&criticalSigset, SIGIO);
	sigprocmask(SIG_BLOCK, &criticalSigset, &restoreSigset);
    }
    else
    {
	/* unblock SIGUSR2 and SIGALRM, but only, if they were not
           already blocked before the lock operation was executed by
           this function. We do NOT simply restore the old signal mask
           with SIG_SETMASK, since other signals may have been
           modified in the meantime */
	sigemptyset(&criticalSigset);
	if (!sigismember(&restoreSigset, SIGUSR2))
	    sigaddset(&criticalSigset, SIGUSR2);
	if (!sigismember(&restoreSigset, SIGALRM))
	    sigaddset(&criticalSigset, SIGALRM);
        if (!sigismember(&restoreSigset, SIGIO))
            sigaddset(&criticalSigset, SIGIO);
	sigprocmask(SIG_UNBLOCK, &criticalSigset, NULL);
    }
}

/*****************************************************************************
 *
 * Create and initialize new message buffer
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jun 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_log_private.h
 *
 * Theory of operation:   **** = data, 0 = '\0', h = head, t = tail, e = eob
 *
 * If buffer is empty:
 *
 * buffer:   ________________________________________________________________
 * pointers: h
 *           t
 *           e
 *
 * After insertion of some messages:
 *
 * buffer:   *******0*****0*****00****0______________________________________
 * pointers: h
 *                                     t
 *                                     e
 *
 * After removal of some messages:
 *
 * buffer:   ______________*****00****0______________________________________
 * pointers:               h
 *                                     t
 *                                     e
 *
 * After hitting the end of the buffer:
 *
 * buffer:   ********0_____*****00****0*****************0******0*********0___
 * pointers:               h
 *                    t
 *                                                                        e
 * Maximum buffer usage:
 *
 * buffer:   ********0*****0****00****0*****************0******0***********0_
 * pointers: h
 *                                                                          t
 *                                                                          e
 * Maximum buffer usage on overwrap:
 *
 * buffer:   ********0*****0****0_****0*****************0******0***********0_
 * pointers:                      h
 *                               t
 *                                                                          e
 *
 *****************************************************************************/
static struct phLogBuffer *makeBuffer(int bufferSize)
{
    struct phLogBuffer *temp;

    /* allocate new message buffer */
    temp = (struct phLogBuffer *) malloc(sizeof(struct phLogBuffer));
    if (temp)
    {
	/* initialize message buffer */
	temp->bufferSize = (bufferSize >= 0 ? bufferSize : PHLOG_DFLTBUF_SIZE);
	if (temp->bufferSize < PHLOG_MINBUF_SIZE)
	    temp->bufferSize = PHLOG_MINBUF_SIZE;
	temp->head = 0;
	temp->tail = 0;
	temp->eob = 0;
	temp->data = (char *) malloc(temp->bufferSize);
	if (!temp->data)
	{
	    free(temp);
	    temp = NULL;
	}
	else
	    temp->data[0] = '\0';
    }

    /* return pointer to message buffer or NULL */
    return temp;
}

/*****************************************************************************
 *
 * Append message to buffer
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log_private.h
 *
 *****************************************************************************/
static int appendMessageToBuffer(
    struct phLogBuffer *b               /* buffer to work on */,
    char *message                       /* the message to be appended */
)
{
    int space;
    int len;
    int notSent = 1;

    /* return immediately, if buffer does not exist */
    if (!b)
	return 0;

    len = strlen(message);
    
    /* return immediately, if message is to large at all */
    if (len > b->bufferSize - 2)
	return 0;

    /* try to insert message into buffer. Remove existing messages, if
       necessary */
    do
    {
	/* regular case */
	if (b->head <= b->tail)
	{
	    /* not overwrapped, available space goes from eob till end */
	    space = (b->bufferSize - b->eob) - 2;
	    if (space >= len)
	    {
		/* enough space available, copy message */
		strcpy(&(b->data[b->tail]), message);
		b->tail += (len + 1);
		b->eob = b->tail;
		notSent = 0;
	    }
	    else if (b->head > 0)
	    {
		/* there is some space at the beginning of the buffer.
		   overwrap and proceed with next if case.... */
		b->tail = 0;
	    }
	    else
	    {
		/* delete one message and go on with loop */
		removeMessageFromBuffer(b);
		continue;
	    }
	}

	/* overwrapped case */
	if (b->head > b->tail)
	{
	    /* overwrapped, available space goes from tail to head */
	    space = (b->head - b->tail) - 2;
	    if (space < len)
	    {
		/* delete one message and go on with loop */
		removeMessageFromBuffer(b);
		continue;
	    }
	    else
	    {
		strcpy(&(b->data[b->tail]), message);
		b->tail += (len + 1);
		notSent = 0;
	    }
	}
    } while (notSent);
    return 1;
}

/*****************************************************************************
 *
 * Remove message from buffer
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log_private.h
 *
 *****************************************************************************/
static char *removeMessageFromBuffer(
    struct phLogBuffer *b               /* buffer to work on */
)
{
    char *temp = NULL;

    /* return immediately, if buffer does not exist */
    if (!b)
	return NULL;

    /* only if messages are present: */
    if (b->head != b->tail)
    {
	/* start of next message */
	temp = &(b->data[b->head]);

	/* find end of the message. messages may be empty */
	while (b->data[b->head] != '\0')
	    (b->head)++;
	(b->head)++;
	
	if (b->head == b->tail)
	    /* last message was read, head reached tail, reset now */
	    b->head = b->tail = b->eob = 0;
	else if (b->head == b->eob)
	{
	    /* head reached end of used buffer, but tail is in front of 
               head. Set head to the begin of the buffer and set end of
               used buffer to current tail. end of overwrapping. */
	    b->eob = b->tail;
	    b->head = 0;
	}
    }

    /* pointer to retrieved message */
    return temp;
}

/*****************************************************************************
 *
 * Delete a buffer
 *
 * History: 16 Jun 1999, Michael Vogt, created
 *
 * Authors: Michael Vogt
 *
 *****************************************************************************/
static void deleteBuffer(
    struct phLogBuffer **b              /* buffer pointer to be deleted */
)
{
    /* return immediately, if buffer does not exist */
    if (!*b)
	return;
    
    /* free data area of buffer */
    if ((*b)->data)
    {
	free ((*b)->data);
	(*b)->data = 0;
    }

    /* free buffer type itself */
    free(*b);
    *b = NULL;
}

/*****************************************************************************
 *
 * Open any log file
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log_private.h
 *
 *****************************************************************************/
static FILE *openLogFile(const char *name, int isErrorFile)
{
    FILE *tmp;

    if (strcmp(name, "-") == 0)
	return isErrorFile ? stderr : stdout;
    else
    {
	tmp = fopen(name, "w+");
	if (!tmp)
	    fprintf(stderr, 
		"%s%s from %s: could not open log file '%s' for writing: %s\n",
		PHLOG_STR_PREFIX, PHLOG_TAG_ERROR, PHLOG_STR_LOGGER, 
		name, strerror(errno));
    }

    return tmp;
}

/*****************************************************************************
 *
 * Copy any open log file to a new opened log file
 *
 * Authors: Michael Vogt
 *
 * History: 14 Mar 2000, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log_private.h
 *
 *****************************************************************************/
static void copyLogFile(FILE *dest, FILE *source)
{
    int c;

    if (!dest || !source)
	return;

    if (source == stdout || source == stderr)
	return;

    if (fseek(source, 0L, SEEK_SET) != 0)
	return;

    while ((c = fgetc(source)) != EOF)
	fputc(c, dest);

    fseek(source, 0L, SEEK_END);
}

static void writeMessageToFile(FILE *f, const char *message)
{
    const char *ptr;

    if (!f)
	return;

    fputs(PHLOG_STR_PREFIX, f);
    ptr = message;
    while (*ptr != '\0')
    {
	fputc((int) (*ptr), f);
	if (*ptr == '\n')
	{
	    fputs(PHLOG_STR_PREFIX, f);
	    fputs("  ", f);
	}
	ptr++;
    }
    fputc((int) '\n', f);
    fflush(f);
}


/*****************************************************************************
 *
 * Do the logging work
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log_private.h
 *
 *****************************************************************************/
static phLogError_t logMessage(
    phLogId_t loggerID        /* the validated logger ID */, 
    phLogType_t type          /* message type and verbousity */, 
    const char *caller        /* a string that identifies the caller */, 
    const char *format        /* format string as in printf(3) */, 
    va_list argp              /* variable argument list type */
)
{
    lockOrUnlockSignals(1);

    static char msgBuf[PHLOG_MAX_MESSAGE_SIZE + 1] = "";
    static char tempMsgBuf[PHLOG_MAX_MESSAGE_SIZE + 1] = "";
    static char lastMsg[PHLOG_MAX_MESSAGE_SIZE + 1] = "";
    static char finalMsgBuf[PHLOG_MAX_MESSAGE_SIZE + 1] = "";
    static char streamPrefix[15]="";

    char *ps = NULL, *pd = NULL, *ph = NULL;
    char *tempp = NULL;

    int i = 0, j = 0;

    bool issueEvent = false;
  
    FILE *lf[4];
    lf[0] = NULL;         /* by default, don't print to files */
    lf[1] = NULL;         /* by default, don't print to files */
    lf[2] = NULL;         /* by default, don't print to files */
    lf[3] = NULL;         /* by default, don't print to files */

    /* Based on the message type, start the message with a type name
       and the identification of the caller. Then check current log
       mode, to decide whether to print the message to a log file or
       to print it only to the internal buffer. */
    streamPrefix[0]= '\0';
    switch (type)
    {
      case PHLOG_TYPE_FATAL:
	if (loggerID -> modeFlags &
	    (PHLOG_MODE_FATAL|PHLOG_MODE_ERROR|PHLOG_MODE_WARN))
	{
	    lf[0] = loggerID -> firstErr;
	    lf[1] = loggerID -> secondErr;
	    lf[2] = loggerID -> firstMsg;
	    lf[3] = loggerID -> secondMsg;
	    strcat(streamPrefix,"ERROR");

	    issueEvent = true;
	}
	else
	{
	  return  PHLOG_ERR_OK;
	}
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_FATAL, caller);
	break;
      case PHLOG_TYPE_ERROR:
	if (loggerID -> modeFlags & (PHLOG_MODE_ERROR|PHLOG_MODE_WARN))
	{
	    lf[0] = loggerID -> firstErr;
	    lf[1] = loggerID -> secondErr;
	    lf[2] = loggerID -> firstMsg;
	    lf[3] = loggerID -> secondMsg;
	    strcat(streamPrefix,"ERROR");
            issueEvent = true;
	}
	else
        {
          return  PHLOG_ERR_OK;
        }
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_ERROR, caller);
	break;
      case PHLOG_TYPE_WARNING:
	if (loggerID -> modeFlags & PHLOG_MODE_WARN)
	{
	    lf[0] = loggerID -> firstErr;
	    lf[1] = loggerID -> secondErr;
	    lf[2] = loggerID -> firstMsg;
	    lf[3] = loggerID -> secondMsg;
	    strcat(streamPrefix,"WARN");
	    issueEvent = true;
	}
	else
        {
          return  PHLOG_ERR_OK;
        }
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_WARNING, caller);
	break;
      case PHLOG_TYPE_TRACE:
	if (loggerID -> modeFlags & PHLOG_MODE_TRACE)
	{
	    lf[0] = loggerID -> firstErr;
	    lf[1] = loggerID -> secondErr;
	    lf[2] = loggerID -> firstMsg;
	    lf[3] = loggerID -> secondMsg;
	    strcat(streamPrefix,"TRACE");
	    issueEvent = true;
	}
	else
        {
          return  PHLOG_ERR_OK;
        }
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_TRACE, caller);
	break;
      case PHLOG_TYPE_MESSAGE_0:
	if (loggerID -> modeFlags &
	    (PHLOG_MODE_DBG_0|PHLOG_MODE_DBG_1|PHLOG_MODE_DBG_2|PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4))
	{
	    lf[0] = loggerID -> firstMsg;
	    lf[1] = loggerID -> secondMsg;
	    strcat(streamPrefix,"INFO");
	    issueEvent = true;
	}
	else
        {
          return  PHLOG_ERR_OK;
        }
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_MESSAGE_0, caller);
	break;
      case PHLOG_TYPE_MESSAGE_1:
	if (loggerID -> modeFlags &
	    (PHLOG_MODE_DBG_1|PHLOG_MODE_DBG_2|PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4))
	{
	    lf[0] = loggerID -> firstMsg;
	    lf[1] = loggerID -> secondMsg;
	    strcat(streamPrefix,"INFO");
	    issueEvent = true;
	}
	else
        {
          return  PHLOG_ERR_OK;
        }
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_MESSAGE_1, caller);
	break;
      case PHLOG_TYPE_MESSAGE_2:
	if (loggerID -> modeFlags &
	    (PHLOG_MODE_DBG_2|PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4))
	{
	    lf[0] = loggerID -> firstMsg;
	    lf[1] = loggerID -> secondMsg;
	    strcat(streamPrefix,"INFO");
	    issueEvent = true;
	}
	else
        {
          return  PHLOG_ERR_OK;
        }
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_MESSAGE_2, caller);
	break;
      case PHLOG_TYPE_MESSAGE_3:
	if (loggerID -> modeFlags &
	    (PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4))
	{
	    lf[0] = loggerID -> firstMsg;
	    lf[1] = loggerID -> secondMsg;
	    strcat(streamPrefix,"INFO");
	    issueEvent = true;
	}
	else
        {
          return  PHLOG_ERR_OK;
        }
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_MESSAGE_3, caller);
	break;
      case PHLOG_TYPE_MESSAGE_4:
	if (loggerID -> modeFlags &
	    (PHLOG_MODE_DBG_4))
	{
	    lf[0] = loggerID -> firstMsg;
	    lf[1] = loggerID -> secondMsg;
	    strcat(streamPrefix,"INFO");
	    issueEvent = true;
	}
	else
        {
          return  PHLOG_ERR_OK;
        }
	snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s from %s: ", PHLOG_TAG_MESSAGE_4, caller);
	break;
      case PHLOG_TYPE_COMM_HIST:
        break;
    }
    /* if customized prefix text is defined, add it to the beginning of the message */
    if(strlen(loggerID->prefixText) != 0)
    {
        strncpy(tempMsgBuf, msgBuf, PHLOG_MAX_MESSAGE_SIZE);
        snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s%s", loggerID->prefixText, tempMsgBuf);
        tempMsgBuf[0] = '\0';
    }

    /*
     * if the "debug_level" set as "9", and the log message caller is not "hw interface"
     * (which aways log the timestamp information), we will ouput the timestamp information
     * CR42750, Jiawei Lin, Oct 2008
     *
     */
    if ( (loggerID->logTimestampFlag==1) && (strcasecmp(caller, "hw interface")!=0) ) {
      snprintf(msgBuf, PHLOG_MAX_MESSAGE_SIZE, "%s%s ", msgBuf, phToolsGetTimestampString());
    }

    /* complete the variable part of the message */
    tempp = &msgBuf[strlen(msgBuf)];
    vsnprintf(tempp, PHLOG_MAX_MESSAGE_SIZE-strlen(msgBuf), format, argp);

    /* exchange groups of \r and \n , if followed by a '"' */
    pd = finalMsgBuf;
    ps = msgBuf;

    do
    {
	if (*ps == '\r' || *ps == '\n')
	{
	    /* detected, look for next visible character */
	    ph = ps;
	    while (*ps == '\r' || *ps == '\n')
		ps++;
	    if (*ps == '"')
	    {
		/* yes, it's a [\r\n]+ followed by '"'
		   replace only the first one, do the rest later */
		ps = ph;
		*pd++ = '\\';
		*pd++ = (*ps == '\r') ? 'r' : 'n';
		ps++;
	    }
	    else
	    {
		/* recover and copy unchanged */
		ps = ph;
		*pd++ = *ps++;
	    }
	}
	else
	    *pd++ = *ps++;
    } while (*ps != '\0');
    *pd = '\0';

    /* delete double file destinations and stdout/stderr duplicates
       (mapped to stdout) */
    for (i=0; i<4; i++)
	for (j=i+1; j<4; j++)
	{
	    if (lf[i] == NULL || lf[j] == NULL)
		continue;
	    if (lf[i] == stdout && lf[j] == stderr)
		lf[j] = NULL;
	    if (lf[i] == stderr && lf[j] == stdout)
		lf[i] = NULL;
	    if (lf[i] == lf[j])
		lf[j] = NULL;
	}
    if(type != PHLOG_TYPE_COMM_HIST)
    {
      /* finally log the message to any open log file and to the
       * internal buffer */
      for (i=0; i<4; i++)
      {
        if (lf[i])
          writeMessageToFile(lf[i], finalMsgBuf);
      }
      appendMessageToBuffer(loggerID -> buffer, finalMsgBuf);
    }
    else
    {
      appendMessageToBuffer(loggerID->commBuffer, finalMsgBuf);
    }

    if(type == PHLOG_TYPE_FATAL || type == PHLOG_TYPE_ERROR)
    {
      strcpy(lastErrorMessage, tempp);
    }

    //issue the messages as test cell events so that
    //TC-API and TCCT console can capture these
    //messages
    if(issueEvent)
    {
      struct timeval tv;
      gettimeofday(&tv, NULL);

      /* send message to the TCCT console */
      strcat(streamPrefix,":\t[PH]");
      driver_dev_kits::TestCellEvent tcStream;
      tcStream.setEventType(driver_dev_kits::TestCellEvent::STREAM);
      std::map<std::string, std::string> streamProperties;
      streamProperties["CONTENT"] = streamPrefix + std::string(finalMsgBuf) + std::string("\n");
      tcStream.setEventProperties(streamProperties);
      tcStream.setTimeStamp(tv.tv_sec, tv.tv_usec);
      tcStream.issue();
    }

    /* save the FATAL/ERROR/WARNING messages */
    if( (type == PHLOG_TYPE_FATAL || type == PHLOG_TYPE_ERROR ))
    {
      strncat(savedErrorMsg, finalMsgBuf, PHLOG_DFLTBUF_SIZE);
      strncat(savedErrorMsg, "\n", PHLOG_DFLTBUF_SIZE);
    }

    lockOrUnlockSignals(0);

    /* return with success */
    return PHLOG_ERR_OK;
}

/*****************************************************************************
 *
 * Initialize an error and message logger
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogInit(
    phLogId_t *loggerID       /* the resulting logger ID */,
    phLogMode_t modeFlags     /* the verbosity flags */,
    const char *firstErrFile  /* file name to log error, warning, and
				 trace messages in, "-" refers to stderr, 
				 or NULL or "" */,
    const char *secondErrFile /* another file name to log error, warning, and
				 trace messages in, "-" refers to stderr, 
				 or NULL or "" */,
    const char *firstMsgFile  /* file name to log general (debug)
				 messages in, "-" refers to stdout, 
				 or NULL or "" */,
    const char *secondMsgFile /* another file name to log general (debug)
				 messages in, "-" refers to stdout, 
				 or NULL or "" */,
    int bufferSize            /* size of internal buffer to be used
				 for message logging. 0 disables
				 internal buffer, negative numbers
				 cause a default value to be used */
)
{
    struct phLogStruct *tmpId;
    phLogError_t error = PHLOG_ERR_OK;

    savedErrorMsg[0] = '\0';

    /* allocate new logger data type */
    tmpId = (struct phLogStruct *) malloc(sizeof(struct phLogStruct));
    if (tmpId == 0)
    {
	fprintf(stderr, 
		"%s%s from %s: not enough memory during call to phLogInit()\n",
		PHLOG_STR_PREFIX, PHLOG_TAG_FATAL, PHLOG_STR_LOGGER);
	return PHLOG_ERR_MEMORY;
    }

    /* initialize with default values */
    tmpId -> myself = tmpId;
    tmpId -> modeFlags = modeFlags;
    tmpId -> suspendLevel = 0;
    tmpId -> prefixText[0] = '\0';

    /*
     *the default behaviour is to not log the timestamp
     *CR42750, Jiawei Lin, Oct 2008
     */
    tmpId -> logTimestampFlag = 0;

    /* open log files */
    tmpId -> firstErr = NULL;
    tmpId -> secondErr = NULL;
    tmpId -> firstMsg = NULL;
    tmpId -> secondMsg = NULL;

    if (firstErrFile && *firstErrFile != '\0')
    {
	tmpId -> firstErr = openLogFile(firstErrFile, 1);
	if (!tmpId -> firstErr)
	    error = PHLOG_ERR_FILE;
    }
    if (secondErrFile && *secondErrFile != '\0')
    {
	tmpId -> secondErr = openLogFile(secondErrFile, 1);
	if (!tmpId -> secondErr)
	    error = PHLOG_ERR_FILE;
    }
    if (firstMsgFile && *firstMsgFile != '\0')
    {
	tmpId -> firstMsg = openLogFile(firstMsgFile, 0);
	if (!tmpId -> firstMsg)
	    error = PHLOG_ERR_FILE;
    }
    if (secondMsgFile && *secondMsgFile != '\0')
    {
	tmpId -> secondMsg = openLogFile(secondMsgFile, 0);
	if (!tmpId -> secondMsg)
	    error = PHLOG_ERR_FILE;
    }

    /* ensure, that at least one message file and one error file exists */
    if (!tmpId -> firstErr && !tmpId -> secondErr)
	tmpId -> firstErr = stderr;

    if (!tmpId -> firstMsg && !tmpId -> secondMsg)
	tmpId -> firstMsg = stdout;

    /* initialize necessary buffers */
    if (bufferSize != 0)
    {
	tmpId -> buffer = makeBuffer(bufferSize);
	if (!tmpId -> buffer) {
	    free(tmpId);
	    return PHLOG_ERR_MEMORY;
	}
    }
    else
	tmpId -> buffer = NULL;

    /* initialize the buffer which saves the communication
     * history between the tester and the ph equipment */
    tmpId->commBuffer = makeBuffer(PHLOG_DFLTBUF_SIZE);
    if(!tmpId->commBuffer) {
      free(tmpId);
      return PHLOG_ERR_MEMORY;
    }

    /* return */
    *loggerID = tmpId;
    return error;
}

/*****************************************************************************
 *
 * Replace the secondary log files
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogReplaceLogFiles(
    phLogId_t loggerID        /* the logger ID */,
    char *secondErrFile       /* another file name to log error, warning, and
				 trace messages in, "-" refers to stderr, 
				 or NULL or "" */,
    char *secondMsgFile       /* another file name to log general (debug)
				 messages in, "-" refers to stdout, 
				 or NULL or "" */
)
{
    phLogError_t error = PHLOG_ERR_OK;
    FILE *newFile;

    CheckHandle(loggerID);

    lockOrUnlockSignals(1);

    if (secondErrFile && *secondErrFile != '\0')
    {
	newFile = openLogFile(secondErrFile, 1);
	if (! newFile)
	    error = PHLOG_ERR_FILE;
	else
	{
	    copyLogFile(newFile, loggerID -> secondErr);

	    if (loggerID -> secondErr && loggerID -> secondErr != stderr)
		fclose(loggerID -> secondErr);

	    loggerID -> secondErr = newFile;
	}
    }
    if (secondMsgFile && *secondMsgFile != '\0')
    {
	newFile = openLogFile(secondMsgFile, 0);
	if (! newFile)
	    error = PHLOG_ERR_FILE;
	else
	{
	    copyLogFile(newFile, loggerID -> secondMsg);

	    if (loggerID -> secondMsg && loggerID -> secondMsg != stdout)
		fclose(loggerID -> secondMsg);

	    loggerID -> secondMsg = newFile;
	}
    }

    lockOrUnlockSignals(0);

    return error;
}

/*****************************************************************************
 *
 * Set the prefix text for the message
 *
 * Authors: Xiaofei Han
 *
 * History: 6 Nov 2013, created
 *
 * Description:
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogSetPrefixText(
    phLogId_t loggerID        /* the logger ID */,
    const char *prefixText    /* the prefix text */
)
{
    CheckHandle(loggerID);

    if(prefixText != NULL)
    {
        strncpy(loggerID->prefixText, prefixText, PHLOG_MAX_LEN_PREFIX);
        loggerID->prefixText[PHLOG_MAX_LEN_PREFIX] = '\0';
    }

    return PHLOG_ERR_OK;
}

    
/*****************************************************************************
 *
 * Retrieve the current log mode
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogGetMode(
    phLogId_t loggerID        /* the logger ID */,
    phLogMode_t *modeFlags    /* the current verbosity flags */
)
{
    CheckHandle(loggerID);

    *modeFlags = loggerID->modeFlags;
    return PHLOG_ERR_OK;
}

/*****************************************************************************
 *
 * Get the log timestamp flag
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * Returns: error code
 *
 *****************************************************************************/
 phLogError_t phLogGetTimestampFlag(
    phLogId_t loggerID,
    int *logTimeStampFlag
)
{
    CheckHandle(loggerID);

    *logTimeStampFlag = loggerID->logTimestampFlag;
    return PHLOG_ERR_OK;
}

/*****************************************************************************
 *
 * Set the log mode flags
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogSetMode(
    phLogId_t loggerID        /* the logger ID */,
    phLogMode_t modeFlags     /* the new verbosity flags */
)
{
    CheckHandle(loggerID);

    loggerID->modeFlags = modeFlags;
    return PHLOG_ERR_OK;
}

/*****************************************************************************
 *
 * Set the log timestamp flag
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogSetTimestampFlag(
    phLogId_t loggerID,
    int logTimeStampFlag
)
{
    CheckHandle(loggerID);

    loggerID->logTimestampFlag = logTimeStampFlag;
    return PHLOG_ERR_OK;
}

/*****************************************************************************
 *
 * Suspend the logger
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogSetInactive(
    phLogId_t loggerID        /* the logger ID */
)
{
    CheckHandle(loggerID);

    loggerID->suspendLevel++;
    return PHLOG_ERR_OK;
}

/*****************************************************************************
 *
 * Resume the logger
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogSetActive(
    phLogId_t loggerID        /* the logger ID */
)
{
    CheckHandle(loggerID);

    loggerID->suspendLevel--;
    if (loggerID->suspendLevel < 0)
	loggerID->suspendLevel = 0;

    return PHLOG_ERR_OK;
}

/*****************************************************************************
 *
 * Dump internal buffers
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogDump(
    phLogId_t loggerID        /* the logger ID */,
    phLogMode_t dumpModeFlags /* the verbosity flags for dumping */
)
{
    const char *oneMessage;

    CheckHandle(loggerID);

    lockOrUnlockSignals(1);

    if (loggerID -> firstErr)
	writeMessageToFile(loggerID -> firstErr,
	    "dumping internal message trace ...\n");

    if (loggerID -> secondErr)
	writeMessageToFile(loggerID -> secondErr,
	    "dumping internal message trace ...\n");

    while ((oneMessage = removeMessageFromBuffer(loggerID -> buffer)))
    {
	if (
	    (strncmp(oneMessage, PHLOG_TAG_FATAL, strlen(PHLOG_TAG_FATAL)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_FATAL|PHLOG_MODE_ERROR|PHLOG_MODE_WARN)) ||

	    (strncmp(oneMessage, PHLOG_TAG_ERROR, strlen(PHLOG_TAG_ERROR)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_ERROR|PHLOG_MODE_WARN)) ||

	    (strncmp(oneMessage, PHLOG_TAG_WARNING, strlen(PHLOG_TAG_WARNING)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_WARN)) ||

	    (strncmp(oneMessage, PHLOG_TAG_TRACE, strlen(PHLOG_TAG_TRACE)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_TRACE)) ||

	    (strncmp(oneMessage, PHLOG_TAG_MESSAGE_0, strlen(PHLOG_TAG_MESSAGE_0)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_DBG_0|PHLOG_MODE_DBG_1|PHLOG_MODE_DBG_2|PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4)) ||

	    (strncmp(oneMessage, PHLOG_TAG_MESSAGE_1, strlen(PHLOG_TAG_MESSAGE_1)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_DBG_1|PHLOG_MODE_DBG_2|PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4)) ||

	    (strncmp(oneMessage, PHLOG_TAG_MESSAGE_2, strlen(PHLOG_TAG_MESSAGE_2)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_DBG_2|PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4)) ||

	    (strncmp(oneMessage, PHLOG_TAG_MESSAGE_3, strlen(PHLOG_TAG_MESSAGE_3)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_DBG_3|PHLOG_MODE_DBG_4)) ||

	    (strncmp(oneMessage, PHLOG_TAG_MESSAGE_4, strlen(PHLOG_TAG_MESSAGE_4)) == 0 &&
	     dumpModeFlags & (PHLOG_MODE_DBG_4))
	   )
	{
	    if (loggerID -> firstErr)
		writeMessageToFile(loggerID -> firstErr, oneMessage);

	    if (loggerID -> secondErr)
		writeMessageToFile(loggerID -> secondErr, oneMessage);
	}
    }

    if (loggerID -> firstErr)
	writeMessageToFile(loggerID -> firstErr, 
	    "... end of dumping internal message trace\n");

    if (loggerID -> secondErr)
	writeMessageToFile(loggerID -> secondErr, 
	    "... end of dumping internal message trace\n");

    lockOrUnlockSignals(0);

    return PHLOG_ERR_OK;
}

static void writeHistDumpHeader(FILE *fileptr)
{
  static char beginMsg[] = "Begin dumping the communication history (last 256KB)";
  static char blockDelim[] = "******************************************************";

  if(fileptr == NULL)
    return;

  writeMessageToFile(fileptr, blockDelim);
  writeMessageToFile(fileptr, beginMsg);
  writeMessageToFile(fileptr, blockDelim);
}

static void writeHistDumpTailer(FILE *fileptr)
{
  static char endMsg[] = "End dumping the communication history to file: "COMM_HIST_FILE_PATH;
  static char blockDelim[] = "******************************************************";

  if(fileptr == NULL)
    return;

  writeMessageToFile(fileptr, blockDelim);
  writeMessageToFile(fileptr, endMsg);
  writeMessageToFile(fileptr, blockDelim);
}


static void writeHistDumpError(FILE *fileptr)
{
  static char errMsg[] = "Failed to open the communication history file: "COMM_HIST_FILE_PATH;
  static char blockDelim[] = "******************************************************";

  if(fileptr == NULL)
    return;

  writeMessageToFile(fileptr, blockDelim);
  writeMessageToFile(fileptr, errMsg);
  writeMessageToFile(fileptr, blockDelim);
}

phLogError_t phLogDumpCommHistory(
    phLogId_t loggerID        /* the logger ID */
)
{
  static int dumped = 0;
  char *oneMessage;
  FILE *commHistoryFile = NULL;

  if(dumped)
    return PHLOG_ERR_OK;

  CheckHandle(loggerID);

  lockOrUnlockSignals(1);

  writeHistDumpHeader(loggerID->firstErr);
  writeHistDumpHeader(loggerID->secondErr);

  commHistoryFile = fopen(COMM_HIST_FILE_PATH, "a+w");

  if(commHistoryFile == NULL)
  {
    writeHistDumpError(loggerID->firstErr);
    writeHistDumpError(loggerID->secondErr);
    return PHLOG_ERR_OK;
  }

  writeHistDumpHeader(commHistoryFile);
  writeMessageToFile(commHistoryFile, "T = Tester; E = PH Equipment");
  while ((oneMessage = removeMessageFromBuffer(loggerID -> commBuffer)))
  {
    writeMessageToFile(commHistoryFile, oneMessage);
  }
  writeHistDumpTailer(commHistoryFile);

  fclose(commHistoryFile);
  dumped = 1;

  writeHistDumpTailer(loggerID->firstErr);
  writeHistDumpTailer(loggerID->secondErr);

  lockOrUnlockSignals(0);

  return PHLOG_ERR_OK;
}

/*****************************************************************************
 *
 * Log messages from the driver framework
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogFrameMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_FRAME, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the configuration manager
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogConfMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_CONF, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the driver plugin
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogFuncMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_FUNC, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the communication layer
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogComMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_COM, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the tester interface
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogTesterMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_TESTER, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the driver state controller
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogStateMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_STATE, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the event handler
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogEventMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_EVENT, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the hook framework
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogHookMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_HOOK, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the simulator
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogSimMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_SIM, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the GUI server
 *
 * Authors: Michael Vogt
 *
 * History: 23 Nov 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogGuiMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_GUI, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Log messages from the GUI server
 *
 * Authors: Michael Vogt
 *
 * History: 23 Nov 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogDiagMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */,
    ...                       /* variable argument list */
)
{
    phLogError_t tmpRetVal;
    va_list argp;
    
    CheckHandle(loggerID);
    CheckSuspend(loggerID, type);
    
    /* call the common internal log function to do the actual work */
    va_start(argp, format);
    tmpRetVal = logMessage(loggerID, type, PHLOG_STR_DIAG, format, argp);
    va_end(argp);

    return tmpRetVal;
}

/*****************************************************************************
 *
 * Query and return the pointer to the last error message
 *
 * Description:
 *
 * This function returns the pointer to the last error message. This function
 * will be used by each plugins' privateDiag() function.
 *
 * Returns: the pointer to the last error message
 *
 *****************************************************************************/
char* phLogGetLastErrorMessage()
{
  return lastErrorMessage;
}


/*****************************************************************************
 *
 * Destroy a logger
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_log.h
 *
 *****************************************************************************/
phLogError_t phLogDestroy(
        phLogId_t loggerID    /* the logger ID */
)
{
    CheckHandle(loggerID);
    
    savedErrorMsg[0] = '\0';

    /* close all log files, besides stdout and stderr */
    if (loggerID -> firstErr != NULL && loggerID -> firstErr != stderr)
    {
	fclose(loggerID -> firstErr);
        loggerID -> firstErr = NULL;
    }
    if (loggerID -> secondErr != NULL && loggerID -> secondErr != stderr)
    {
	fclose(loggerID -> secondErr);
        loggerID -> secondErr = NULL;
    }
    if (loggerID -> firstMsg != NULL && loggerID -> firstMsg != stdout)
    {
	fclose(loggerID -> firstMsg);
        loggerID -> firstMsg = NULL;
    }
    if (loggerID -> secondMsg != NULL && loggerID -> secondMsg != stdout)
    {
	fclose(loggerID -> secondMsg);
        loggerID -> secondMsg = NULL;
    }

    /* free the associated buffer */
    deleteBuffer(&(loggerID -> buffer));
    deleteBuffer(&(loggerID -> commBuffer));

    /* make loggerID invalid */
    loggerID -> myself = NULL;

    /* don't free the logger struct, to have a chance to detect
       later illegal usage */

    return PHLOG_ERR_OK;
}

/*Begin of Huatek Modifications, Donnie Tu, 04/18/2002*/
/*Issue Number: 334*/

/*****************************************************************************
 *
 * free the logger struct malloced by phLogInit
 *
 * Authors: Donnie Tu
 *
 * History: 18 Apr 2002, Donnie Tu, Created
 *
 * Description:
 * Please refer to ph_log.h
 *
 *****************************************************************************/
void phLogFree(
        phLogId_t *loggerID    /* the logger ID */
)
{
   savedErrorMsg[0] = '\0';

   if( loggerID!=NULL && *loggerID!=NULL)
   {
      if((*loggerID)->buffer)
      {
	if ((*loggerID)->buffer->data)
	   free((*loggerID)->buffer->data);
	free((*loggerID)->buffer);
      }

      if((*loggerID)->commBuffer)
      {
        if ((*loggerID)->commBuffer->data)
           free((*loggerID)->commBuffer->data);
        free((*loggerID)->commBuffer);
      }

      free(*loggerID);
      *loggerID = NULL;
   }
}
/*End of Huatek Modifications*/


char *phLogGetSavedErrorMsg()
{
  return savedErrorMsg;
}

void phLogClearSavedErrorMsg()
{
  savedErrorMsg[0] = '\0';
}



/*****************************************************************************
 * End of file
 *****************************************************************************/
