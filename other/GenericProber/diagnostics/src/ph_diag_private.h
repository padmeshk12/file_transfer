/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_diag_private.h
 * CREATED  : 27 Sep 2000
 *
 * CONTENTS : Diagnostics send message module
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 27 Jul 1999, Chris Joyce, created
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

#ifndef _PH_DIAG_PRIVATE_H_
#define _PH_DIAG_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/
/*--- module includes -------------------------------------------------------*/

#include "ph_diag.h"


/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_2

#define DIAG_MAX_MESSAGE_LENGTH 2048
#define DIAG_TEXT_AREA_LENGTH 256


/*--- typedefs --------------------------------------------------------------*/


/* Enumeration type for the different kinds of gpib diagnostic status */
typedef enum {
    OK                  /* reading GUI events */,
    WAITING_FOR_SRQ     /* waiting for an event */,
    WAITING_FOR_MSG     /* trying to receive message */,
    TRYING_TO_SEND      /* trying to send message */,
    TIMEOUT             /* timeout has occured */,
    ERROR               /* some kind of error has occurred */,
} gpibDiagnosticStatus_t;


/* 
 * ATTENTION! the values of the enumerated type recMsgMode_t, recSrqMode_t 
 * must follow the order of those defined in the ph_diag.c file 
 * r_rec_msg_array, r_rec_srq_array so the right values are returned in 
 * the getGuiData calls.
 */

typedef enum {
    REC_MSG_MODE_BUTTON = 0,
    REC_MSG_MODE_AFTER_SRQ,
    REC_MSG_MODE_AFTER_SEND,
    REC_MSG_MODE_ALWAYS,
} recMsgMode_t;

typedef enum {
    REC_SRQ_MODE_BUTTON = 0,
    REC_SRQ_MODE_AFTER_SEND,
    REC_SRQ_MODE_ALWAYS,
} recSrqMode_t;


struct phDiagStruct {
    struct phDiagStruct *myself;                /* self reference for validity
                                                   check */
    phComId_t communicationId;                  /* the valid communication link to
                                                   the HW interface used by the
                                                   handler */
    phLogId_t loggerId;                         /* the data logger to be used */

    phGuiProcessId_t guiId;                     /* send message gui */

    gpibDiagnosticStatus_t status;              /* status of diagnostics */

    /* send message parameters */
    int sendMsgTimeout;

    /* receive message parameters */
    recMsgMode_t recMsgMode;
    int recMsgModeChanged;                      /* receive message mode has changed 
                                                   because of GUI event */
    int recMsgTimeout;

    /* receive SRQ parameters */
    recSrqMode_t recSrqMode;
    int recSrqModeChanged;                      /* receive SRQ mode has changed 
                                                   because of GUI event */
    int recSRQTimeout;

    /* Debug parameters */
    int org_debug;
    int debug;

    int quit;
    int getSRQ;
    int getMSG;
    int sendMsgGuiRequest;
    int getSrqGuiRequest;
    int getMsgGuiRequest;

    char textLog[ DIAG_TEXT_AREA_LENGTH + 1 ];
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







/* Begin of Huatek Modification, Donnie Tu, 12/07/2001 */
/* Issue Number: 322 */
#endif /* ! _PH_DIAG_PRIVATE_H_ */
/* End of Huatek Modification */

/*****************************************************************************
 * End of file
 *****************************************************************************/
