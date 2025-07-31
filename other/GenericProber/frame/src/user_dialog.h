/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : user_dialog.h
 * CREATED  :  9 May 2001
 *
 * CONTENTS : User Dialog management 
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  :  9 May 2001, Chris Joyce, created
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

#ifndef _USER_DIALOG_H_
#define _USER_DIALOG_H_

/*--- system includes -------------------------------------------------------*/

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_tcom.h"
#include "ph_event.h"

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/


/*--- typedefs --------------------------------------------------------------*/

typedef struct userDialog *phUserDialogId_t;

typedef enum {
    PHUSERDIALOG_ERR_OK = 0        /* no error */,
    PHUSERDIALOG_ERR_INVALID_HDL   /* the passed ID is not valid */,
    PHUSERDIALOG_ERR_MEMORY        /* couldn't get memory from heap
                                      with malloc() */,
    PHUSERDIALOG_ERR_GUI           /* an error occured in one of
                                      gui server calls */,
    PHUSERDIALOG_ERR_CONF          /* an error occured in one of
                                      configuration calls */,
    PHUSERDIALOG_ERR_INTERNAL      /* this is a bug */
} phUserDialogError_t;

typedef enum {
    ud_freq_unknown,
    ud_freq_never,
    ud_freq_once,
    ud_freq_repeatedly
} phUserDialogFreq_t;



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
 * Initialize user dialog internal variables
 *
 * Authors: Chris Joyce
 *
 * Description: Called once during init for each defined phUserDialogId_t
 *
 * Returns: error code
 *
 ***************************************************************************/
phUserDialogError_t initUserDialog(
    phUserDialogId_t *handle     /* result user dialog ID */,
    phLogId_t logger             /* the data logger to be used */,
    phConfId_t configuration     /* the configuration manager */,
    phTcomId_t tester            /* the tester communication */,
    phEventId_t event            /* event handler */,
    const char* c                /* config key value */,
    const char* default_title    /* default dialog title */,
    const char* default_msg      /* default dialog message */,
    phUserDialogFreq_t exf       /* expected frequency of Dialog */
);

/*****************************************************************************
 *
 * Display User Dialog
 *
 * Authors: Chris Joyce
 *
 * Description:
 *
 * Description: Check internal values and display user dialog when
 *              appropriate.
 *
 * Returns: error code
 *
 ***************************************************************************/
phUserDialogError_t displayUserDialog(
    phUserDialogId_t handle             /* user dialog handle */
);

/*Begin of Huatek Modifications, Donnie Tu, 04/24/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free user dialog internal variables
 *
 * Authors: Donnie Tu
 *
 * Description:
 *
 * Returns: None
 *
 ***************************************************************************/
void freeUserDialog( phUserDialogId_t *handle);
/*End of Huatek Modification*/



#endif /* ! _USER_DIALOG_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
