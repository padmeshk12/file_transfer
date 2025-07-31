/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_log_private.h
 * CREATED  : 14 Jun 1999
 *
 * CONTENTS : Private definitions for debug log facility
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR42750
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jun 1999, Michael Vogt, created
 *            Oct 2008, Jiawei Lin, CR42750
 *                support the "debug_level" value "9", to log the
 *                timestamp information
 *
 * Instructions:
 *
 * 1) Copy this template to as many .h files as you require
 *
 * 2) Use the command 'make depend' to make the new
 *    source files visible to the makefile utility
 *
 * 3) To support automatic man page (documentation) generation, follow the
 *    instructions given for the function template below.
 * 
 * 4) Put private functions and other definitions into separate header
 * files. (divide interface and private definitions)
 *
 *****************************************************************************/

#ifndef _PH_LOG_PRIVATE_H_
#define _PH_LOG_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/

#ifdef __C2MAN__
#include <stdio.h>
#include <stdarg.h>
#include "ph_log.h"
#endif

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

#define PHLOG_DFLTBUF_SIZE (1<<18) /* default internal buffers to hold 256K */
#define PHLOG_MINBUF_SIZE  (1<<6)  /* minimal internal buffers to hold 64 */

/* user visible messages will be composed like
   PHLOG_STR_PREFIX PHLOG_TAG_? from PHLOG_STR_? : <message> */

#define PHLOG_STR_PREFIX      "PH lib: "

/* the following definitions must not contain <space> characters. this
   is a message identification, also used for identifying message
   types that are logged to the internal buffer only */
#define PHLOG_TAG_FATAL       "fatal"
#define PHLOG_TAG_ERROR       "error"
#define PHLOG_TAG_WARNING     "warning"
#define PHLOG_TAG_TRACE       "trace"
#define PHLOG_TAG_MESSAGE_0   "message"
#define PHLOG_TAG_MESSAGE_1   "debug-1"
#define PHLOG_TAG_MESSAGE_2   "debug-2"
#define PHLOG_TAG_MESSAGE_3   "debug-3"
#define PHLOG_TAG_MESSAGE_4   "debug-4"

/* the following definitions describe the calling part of the PH framework */
#define PHLOG_STR_LOGGER      "logger"
#define PHLOG_STR_FRAME       "framework"
#define PHLOG_STR_CONF        "configuration manager"
#define PHLOG_STR_FUNC        "driver plugin"
#define PHLOG_STR_COM         "hw interface"
#define PHLOG_STR_TESTER      "tcom interface"
#define PHLOG_STR_STATE       "state control"
#define PHLOG_STR_EVENT       "event handler"
#define PHLOG_STR_HOOK        "hook control"
#define PHLOG_STR_SIM         "simulator"
#define PHLOG_STR_GUI         "GUI server"
#define PHLOG_STR_DIAG        "diagnostics"

#define PHLOG_MAX_LEN_PREFIX   100


/*--- typedefs --------------------------------------------------------------*/

struct phLogBuffer
{
    int bufferSize;                     /* size of buffer space */
    char *data;                         /* buffer area, used as ring
					   buffer. messages within the
					   buffer are '\0' terminated. */
    int head;                           /* valid messages within the
					   buffer start here */
    int eob;                            /* end of buffer, if tail < head */
    int tail;                           /* new messages for for the
					   buffer go here */
};

struct phLogStruct
{
    struct phLogStruct *myself;
    phLogMode_t         modeFlags;
    int                 logTimestampFlag;
    FILE               *firstErr;
    FILE               *secondErr;
    FILE               *firstMsg;
    FILE               *secondMsg;
    int                 suspendLevel;
    struct phLogBuffer *buffer;
    struct phLogBuffer *commBuffer;
    char               prefixText[PHLOG_MAX_LEN_PREFIX+1];
};

/*--- external variables ----------------------------------------------------*/

/*--- internal function -----------------------------------------------------*/

/*****************************************************************************
 * To allow a consistent interface definition and documentation, the
 * documentation is automatically extracted from the comment section
 * of the below function declarations. All text within the comment
 * region just in front of a function header will be used for
 * documentation. Additional text, which should not be visible in the
 * documentation (like this text), must be put in a separate comment
 * region, ahead of the documentation comment and separated by at
 * least one blank line.
 *
 * To fill the documentation with life, please fill in the angle
 * bracketed text portions (<>) below. Each line ending with a colon
 * (:) or each line starting with a single word followed by a colon is
 * treated as the beginning of a separate section in the generated
 * documentation man page. Besides blank lines, there is no additional
 * format information for the resulting man page. Don't expect
 * formated text (like tables) to appear in the man page similar as it
 * looks in this header file.
 *
 * Function parameters should be commented immediately after the type
 * specification of the parameter but befor the closing bracket or
 * dividing comma characters.
 *
 * To use the automatic documentation feature, c2man must be installed
 * on your system for man page generation.
 *****************************************************************************/

/*****************************************************************************
 *
 * Create and initialize new message buffer
 *
 * Authors: Michael Vogt
 *
 * Returns: pointer to buffer on success, NULL if no more memory
 *
 *****************************************************************************/
static struct phLogBuffer *makeBuffer(
    int bufferSize                      /* defines the size of the buffer */
);

/*****************************************************************************
 *
 * Append message to buffer
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * The provided message is added to the passed ring buffer. If the
 * message is bigger than the ring buffer itself, 0 is returned,
 * otherwise 1 is returned. To add a message to the buffer, previous
 * stored messages may be overwritten.
 *
 * Returns: 1 on success, else 0
 *
 *****************************************************************************/
static int appendMessageToBuffer(
    struct phLogBuffer *b               /* buffer to work on */,
    char *message                       /* the message to be appended */
);

/*****************************************************************************
 *
 * Remove message from buffer
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * The next (unremoved) message is removed from the buffer. The
 * function returns a pointer to the removed messages. The data of
 * this message must be copied or printed before any further call to
 * removeMessageFromBuffer(3) or
 * appendMessageToBuffer(3). Otherwise the message contents may
 * become corrupt.
 *
 * Returns: pointer to removed message or NULL if buffer is empty.
 *
 *****************************************************************************/
static char *removeMessageFromBuffer(
    struct phLogBuffer *b               /* buffer to work on */
);

/*****************************************************************************
 *
 * Delete a buffer
 *
 * Authors: Michael Vogt
 *
 *****************************************************************************/
static void deleteBuffer(
    struct phLogBuffer **b              /* buffer pointer to be deleted */
);

/*****************************************************************************
 *
 * Open any log file
 *
 * Authors: Michael Vogt
 *
 * Description: 
 * Open the file with the given name for writing and
 * return the file pointer. In case of problems to open the file, an
 * error message is printed and NULL is returned. In case of "-" given
 * for the filename, either stderr or stdout is returned, depending on
 * the <isErrorFile> parameter.
 *
 * Returns: file pointer or NULL on failure
 *
 *****************************************************************************/
static FILE *openLogFile(
    const char *name                    /* name of the file to open,
					   "-" means stderr or stdout */,
    int isErrorFile                     /* open error file, if !0 */
);

/*****************************************************************************
 *
 * Do the logging work
 *
 * Authors: Michael Vogt
 *
 * Description: 
 * This function performs the work as described for
 * phLogFrameMessage(3). The only difference is, that this function
 * accepts a variable argument list type as defined in stdarg.h. Also
 * the identification of the caller is passed to this function.
 *
 * Returns: error code
 *
 *****************************************************************************/
static phLogError_t logMessage(
    phLogId_t loggerID        /* the validated logger ID */, 
    phLogType_t type          /* message type and verbousity */, 
    const char *caller        /* a string that identifies the caller */, 
    const char *format        /* format string as in printf(3) */, 
    va_list argp              /* variable argument list type */
);

#endif /* ! _PH_LOG_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
