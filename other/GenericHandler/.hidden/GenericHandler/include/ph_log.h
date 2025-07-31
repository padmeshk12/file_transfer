/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_log.h
 * CREATED  : 19 May 1999
 *
 * CONTENTS : Interface header for material handler log issues
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR42750
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 19 May 1999, Michael Vogt, created
 *            15 Jun 1999, Michael Vogt, 
 *                simulator support added, more error codes introduced, 
 *                minor changes during implementation 
 *            27 Oct 2000, Chris Joyce,
 *                added call for diagnostic messages
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

#ifndef _PH_LOG_H_
#define _PH_LOG_H_

/*--- system includes -------------------------------------------------------*/

/*Begin of Huatek Modification, Luke Lan, 02/13/2001 */
/*Issue Number: 18 */
/*In Linux, structure timeval is included in sys/time.h and unistd.h */
#include <sys/time.h>
#include <unistd.h>
/*End of Huatek Modification*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

#define PHLOG_MAX_MESSAGE_SIZE 16384 /* message to be logged must not be
					longer than this value */

/*--- typedefs --------------------------------------------------------------*/

typedef struct phLogStruct *phLogId_t;

typedef enum {
    PHLOG_ERR_OK           /* no error */,
    PHLOG_ERR_INVALID_HDL  /* the passed ID is not valid */,
    PHLOG_ERR_INACTIVE     /* logger is currently suspended, message
			      was not logged */,
    PHLOG_ERR_MEMORY       /* couldn't get memory from heap with malloc() */,
    PHLOG_ERR_FILE         /* couldn't open log file for writing */
} phLogError_t;

typedef enum {
    PHLOG_TYPE_FATAL       /* A fatal situation occured. Operation can
			      not be continued */,
    PHLOG_TYPE_ERROR       /* An error condition occured which usually
			      requires some user interaction */,
    PHLOG_TYPE_WARNING     /* A warning situation occured which can be
			      handled by the system. Operation continues 
			      with possible restrictions */,
    PHLOG_TYPE_TRACE       /* This is used to trace calls to different
			      module's interface functions */,
    PHLOG_TYPE_MESSAGE_0   /* This is a very important message which
			      is logged in debug mode 0 (normal mode)
			      or higher (see phLogSetMode(3)) */,
    PHLOG_TYPE_MESSAGE_1   /* This message is logged in debug mode 1
			      or higher (see phLogSetMode(3)) */,
    PHLOG_TYPE_MESSAGE_2   /* This message is logged in debug mode 2
			      or higher (see phLogSetMode(3)) */,
    PHLOG_TYPE_MESSAGE_3   /* This message is logged in debug mode 3
			      or higher (see phLogSetMode(3)) */,
    PHLOG_TYPE_MESSAGE_4   /* This message is logged in debug mode 4
			      or higher (see phLogSetMode(3)) */,
    PHLOG_TYPE_COMM_HIST    /* This message logs the communication messages
                              exchanged between the tester and the equipment */
} phLogType_t;

typedef enum {
    PHLOG_MODE_QUIET  = 0x00000000  /* be real quiet, do not log anything */,
    PHLOG_MODE_FATAL  = 0x00000001  /* log fatal type messages */,
    PHLOG_MODE_ERROR  = 0x00000002  /* log fatal and error type messages */,
    PHLOG_MODE_WARN   = 0x00000004  /* log fatal, error, and warning
				       type messages */,
    PHLOG_MODE_TRACE  = 0x00000100  /* log function call trace */,
    PHLOG_MODE_DBG_0  = 0x00010000  /* log mode 0 messages */,
    PHLOG_MODE_DBG_1  = 0x00020000  /* log mode 1 and 0 messages */,
    PHLOG_MODE_DBG_2  = 0x00040000  /* log mode 2, 1, and 0 messages */,
    PHLOG_MODE_DBG_3  = 0x00080000  /* log mode 3, 2, 1, and 0 messages */,
    PHLOG_MODE_DBG_4  = 0x00100000  /* log mode 4, 3, 2, 1, and 0 messages */,

    PHLOG_MODE_NORMAL = 0x00010004  /* recomended log mode: similar to
				       PHLOG_MODE_WARN | PHLOG_MODE_DBG_0 */,
    PHLOG_MODE_ALL    = 0xffffffff  /* log everything you can */
} phLogMode_t;

/*--- external variables ----------------------------------------------------*/

/*--- external function -----------------------------------------------------*/

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
 * Initialize an error and message logger
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function creates a new logging facility used by other modules
 * of the driver framework. Logging is divided into error logging and
 * message logging. The logger can manage up to two files for error
 * message output and up to two files for general (and debug) message
 * output. If more than one file is used for one message type,
 * messages are logged to both target files. This can be used to log
 * error messages to stderr and to a distinct file in parallel. If all
 * message file of one group are set to NULL, stderr or stdout is used
 * by default.
 *
 * For convenience the following special rules apply: In general,
 * messages indicated as fatal, error or warnings are send to the
 * defined error output as well as to the defined message
 * output. Therefore the message output always contain sthe complete
 * log. In case a message is send to stderr and stdout according to
 * the above rule, it is only send to stdout to avoid duplicate
 * messages on the console or in the UI report window.
 *
 * The mode flags define, what kind of messages should be logged to
 * files at all. Internally, the logger tries to log everything it
 * sees, but only the activated message types (through the <modeFlags>
 * parameter) are really logged to files. In case of fatal situations,
 * the internal log buffers can be dumped to trace back the error
 * situation. For efficiency reasons, the internal (hidden) logging
 * can be shut off on request.
 *
 * Returns: error code. A PHLOG_ERR_FILE error is gracefully ignored,
 * i.e. the returned logger ID is valid but will only log to stdout or
 * stderr respectively.
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
);

/*****************************************************************************
 *
 * Replace the secondary log files
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function changes the current secondary log files. It is
 * intended to be used after the log file destinations have been read
 * from a configuration setting. The main problem is, that the
 * configuration already needs th elogger, so th elogger can not be
 * initialized after the configuration manager.
 *
 * Returns: error code
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
);
    
/*****************************************************************************
 *
 * Retrieve the current log mode
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * The current log mode is returned in <modeFlags>
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogGetMode(
    phLogId_t loggerID        /* the logger ID */,
    phLogMode_t *modeFlags    /* the current verbosity flags */
);

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
);

/*****************************************************************************
 *
 * Set the prefix text for the message
 *
 * Authors: Xiaofei Han
 *
 * Description:
 * This function modified the prefix text which is added to the beginning of
 * each driver message.
 * 
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogSetPrefixText(
    phLogId_t loggerID        /* the logger ID */,
    const char *prefixText    /* the prefix text */
);

/*****************************************************************************
 *
 * Set the log mode flags
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function changes the current mode flags (the verbosity) of the
 * logger. Flags may be combined by binary logical OR combination of
 * several of the defined mode flags. PHLOG_MODE_NORMAL is the
 * recommended setting.
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogSetMode(
    phLogId_t loggerID        /* the logger ID */,
    phLogMode_t modeFlags     /* the new verbosity flags */
);


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
);


/*****************************************************************************
 *
 * Suspend the logger
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function allows to suspend the current loging session. To
 * suspend means, all message types except error and fatal messages
 * are discarded. They are not even logged to the internal hidden
 * buffers. Suspension of logging can be used in timing critical parts
 * of the driver.
 *
 * The counterpart of this function is phLogSetActive(3) to resume
 * the logging according to the mode flags. This pair of functions
 * implements a nesting facility, i.e. the logger must be activated as
 * often as it is deactivated to finally become active. This can be
 * used to suspend and resume a logger within different nesting levels
 * of the driver architecture.
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogSetInactive(
    phLogId_t loggerID        /* the logger ID */
);

/*****************************************************************************
 *
 * Resume the logger
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function allows to resume the current loging session after
 * suspension with phLogSetInactive(3).
 *
 * The counterpart of this function is phLogSetInactive(3) to resume
 * the logging according to the mode flags. This pair of functions
 * implements a nesting facility, i.e. the logger must be activated as
 * often as it is deactivated to finally become active. This can be
 * used to suspend and resume a logger within different nesting levels
 * of the driver architecture.
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogSetActive(
    phLogId_t loggerID        /* the logger ID */
);

/*****************************************************************************
 *
 * Dump internal buffers
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * If the logger is not suspended and the mode flags specify that a
 * specific message should not be logged, the message is written to an
 * internal buffer area instead. This is usefull for fatal error
 * analysis. In fatal situations, the internal buffers may be dumped
 * to the error log files, even if they include messages which are not
 * logged during normal (non fatal) operation. The dump verbosity can
 * be selected by the <dumpModeFlags> parameter. The recommended
 * setting is PHLOG_MODE_ALL.
 *
 * Warning: The internal buffers are restricted in size (defined at
 * compile time). The internal buffers are re-used during
 * operation. If messages are dumped, only the most recent messages
 * are available.
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogDump(
    phLogId_t loggerID        /* the logger ID */,
    phLogMode_t dumpModeFlags /* the verbosity flags for dumping */
);

phLogError_t phLogDumpCommHistory(
    phLogId_t loggerID        /* the logger ID */
);

/*****************************************************************************
 *
 * Log messages from the driver framework
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. The message
 * type is defined by the parameter <type>. The messages itself is
 * defined by a format string and data parameters as used in
 * printf(3). The format is not restricted, but should follow some
 * basic rules:
 *
 * Messages should NOT include the type of the message. The type is
 * determined by the <type> flag and automatically logged.
 *
 * Messages should NOT include the component name of the driver
 * (e.g. framework, configuration, hook, etc.). The component name is
 * derived by the name of this log function. Don't use this log
 * function for other driver components as for the one defined in this
 * description and the Note section.
 *
 * Error and Warning messages will be visible to the user. Please use
 * clear forumlations and don't refer to internal driver parameters.
 *
 * Trace messages are used for debugging and are read by the
 * developer. They should print the called function name with all
 * parameters in a C like syntax. E.g. for this function, the format
 * and variable arguments portion would look like:
 *
 * "phLogFrameMessage(P%p, 0x%08lx, \\"%s\\", ...)", loggerID,
 * (long) type, format 
 *
 * Trace messages should not include any tabing. The recommended format
 * to print the arguments of a function is the following:
 *
 * pointers of any kind: P%p
 *
 * strings or character pointers: \\"%s\\" 
 * 
 * If a NULL is passed as
 * character pointer, "(NULL)" should be printed, e.g. like:
 * phLogFrameMessage(PHLOG_TYPE_TRACE, "foo(\\"%s\\")", ptr ? ptr : "(NULL)")
 *
 * characters: '%c'
 *
 * floating point numbers: %g
 *
 * normal integers: %d
 *
 * long integers: %ld
 *
 * enumeration types: 0x%08lx (enumerators must be cast to (long) in the
 * argument list
 *
 * Note: This function should only be called by the driver framework.
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogFrameMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the configuration manager
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the configuration manager
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogConfMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the driver plugin
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the driver plugin
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogFuncMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the communication layer
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the communication
 * layer (link to the real material handler)
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogComMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the tester interface
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the tester interface
 * (link to CPI and TPI)
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogTesterMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the driver state controller
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the driver state controller
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogStateMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the event handler
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the
 *
 * Returns: error code event handler
 *
 *****************************************************************************/
phLogError_t phLogEventMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the hook framework
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the hook framework and
 * hook functions
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogHookMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the simulator
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the hook framework and
 * hook functions
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogSimMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */, 
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from the GUI server
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the hook framework and
 * hook functions
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogGuiMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */,
    ...                       /* variable argument list */
);

/*****************************************************************************
 *
 * Log messages from diagnostics
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 * This function performs the actual logging of messages. For a
 * detailed description, please refer to phLogFrameMessage.
 *
 * Note: This function should only be called by the diagnostics module
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogDiagMessage(
    phLogId_t loggerID        /* the logger ID */,
    phLogType_t type          /* message type and verbousity */,
    const char *format        /* format string as in printf(3) */,
    ...                       /* variable argument list */
);

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
char* phLogGetLastErrorMessage();

/*****************************************************************************
 *
 * Destroy a logger
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * This function destroys the passed logger.
 *
 * Returns: error code
 *
 *****************************************************************************/
phLogError_t phLogDestroy(
        phLogId_t loggerID    /* the logger ID */
);

/*Begin of Huatek Modifications, Donnie Tu, 04/18/2002*/
/*Issue Number: 334*/

/*****************************************************************************
 *
 * free the logger struct malloced by phLogInit
 *
 * Authors: Donnie Tu
 *
 * Description:
 * 
 * This function free the logger struct.
 *
 * Note: Please insure there no chance to use it 
 *
 * Returns: none
 *
 *****************************************************************************/
void phLogFree(
        phLogId_t *loggerID    /* the logger ID */
);
/*End of Huatek Modifications*/

char *phLogGetSavedErrorMsg();

void phLogClearSavedErrorMsg();

/*End of Huatek Modifications*/

#endif /* ! _PH_LOG_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
