/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_diag.h
 * CREATED  : 25 Oct 2000
 *
 * CONTENTS : Interface header for diagnostics tool
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Ulrich Frank,
 *            Chris Joyce
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 25 Oct 2000, Michael Vogt,
 *                         Ulrich Frank,
 *                         Chris Joyce, created
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

#ifndef _PH_DIAG_H_
#define _PH_DIAG_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/
#include "ph_log.h"
#include "ph_mhcom.h"

/*--- defines ---------------------------------------------------------------*/


/*--- typedefs --------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif

/* Handle for open diagnostic session */
typedef struct phDiagStruct *phDiagId_t;

typedef enum {
    DIAG_ERR_OK = 0        /* no error */,
    DIAG_ERR_INVALID_HDL   /* the passed ID is not valid */,
    DIAG_ERR_MEMORY        /* couldn't get memory from heap
                                 with malloc() */,
    DIAG_ERR_COMM_TIMEOUT  /* received a timeout from a
                              communication call 
                              (can occur if for example
                               no event occurred ) */,
    DIAG_ERR_GUI_TIMEOUT   /* received a timeout from the
                              GUI server */,
    DIAG_ERR_GUI           /* an error occured in one of 
                              gui server calls */,
    DIAG_ERR_COMM          /* an error occured in one of 
                              communication calls */
} phDiagError_t;


typedef enum {
    STAND_ALONE            /* Diagnostic session is a stand alone program */,
    DIAGNOSTIC_SUITE       /* Diagnostic session forms part of a wider
                              diagnostic suite of tests that can be performed */
} phDiagSessionType;


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
 * Initialize diagnostic handler
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
);


/*****************************************************************************
 *
 * Create GPIB Diagnostics Master session.
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Create a GPIB Master diagnostics GUI sesssion.  This tool allow the user
 * to specify when events and messages can be received and sent.
 * The phDiagSessionType must be specified indicating that this is either
 * a standalone session of a suite of diagnostic tests.
 *
 * Returns: error code
 *
 *****************************************************************************/
phDiagError_t gpibDiagnosticsMaster(
    phDiagId_t diagID           /* gpib diagnostic ID */,
    phDiagSessionType session   /* kind of session */
);

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
void phDiagnosticsFree(phDiagId_t *diagID);
/*End of Huatek Modifications*/

//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_DIAG_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/

