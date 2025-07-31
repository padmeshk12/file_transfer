/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_tcom_private.h
 * CREATED  : 22 Jul 1999
 *
 * CONTENTS : Private definitions for tcom module
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 22 Jul 1999, Chris Joyce, created
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

#ifndef _PH_TCOM_PRIVATE_H_
#define _PH_TCOM_PRIVATE_H_

#if defined(__cplusplus)
   extern "C" {
#endif


/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/
#include "ph_tools.h"
/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL  PHLOG_TYPE_MESSAGE_2
#define LOG_DEBUG   PHLOG_TYPE_MESSAGE_3
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_4

#define MAX_FLAG_STRING_LEN 64
#define SIMULATED_SITES 4096
#define SIMULATED_MAX_PATH_LEN (1024 + 2 + 1)

/*--- typedefs --------------------------------------------------------------*/


struct phTcomStruct
{
    struct phTcomStruct *myself;           /* self reference for validity
                                              check */
    phTcomMode_t mode;                      /* the communication mode (for future
                                              improvements, currently only
                                              PHTCOM_MODE_ONLINE is valid) */
    phLogId_t loggerId;                    /* ID of the logger to use for this 
                                              session */

    int createGetTestLog;                  /* save flag actions, if set */
    int createSetTestLog;                  /* save flag actions, if set */

    int quitAlreadySet;                /* save the flag value set by the driver */
    int abortAlreadySet;               /* save the flag value set by the driver */

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

#if defined(__cplusplus)
}
#endif


#endif /* ! _PH_MHCOM_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
