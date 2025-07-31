/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : reconfigure.h
 * CREATED  : 16 Jul 1999
 *
 * CONTENTS : Perform (re)configuration tasks
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 16 Jul 1999, Michael Vogt, created
 *            Dec 2005, Jiawei Lin, CR28409
 *              add "reconfigureFramework" function which supports to read
 *              additional driver parameters which are defined in the structure
 *              "phFrameStruct"
 *              
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

#ifndef _RECONFIGURE_H_
#define _RECONFIGURE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

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
 * Substitute pathnames to full path names
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, created
 *
 * Description: The passed string <path> is searched for portions of
 * the form $[^/$]*. The found portions are replaced by values
 * found in the process' environment. If the environment does not
 * define the string, an empty string is assumed.
 *
 ***************************************************************************/
void reconfigureCompletePath(char *complete, const char *path);

/*****************************************************************************
 *
 * Reconfigure the driver's logger
 *
 * Authors: Michael Vogt
 *
 * Description: This function reads the logger specific fields out of
 * the current specific configuration and reconfigures the logger tool
 * accordingly. These parts are the message log file, the error log
 * file, the number of old log files to keep, the debug level, and the
 * trace mode.
 *
 * Returns: The number of warnings, that occured during this operation
 *
 ***************************************************************************/
void reconfigureLogger(
    struct phFrameStruct *frame         /* the framework */,
    int *resErrors                      /* resulting number of errors */,
    int *resWarnings                    /* resulting number of warnings */
);

/*****************************************************************************
 *
 * Reconfigure the driver's state controller
 *
 * Authors: Michael Vogt
 *
 * Description: This function reads the state controller specific
 * fields out of the current specific configuration and reconfigures
 * the module accordingly. These parts are the hand test and single
 * step modes.
 *
 * Returns: The number of warnings, that occured during this operation
 *
 ***************************************************************************/
void reconfigureStateController(
    struct phFrameStruct *frame         /* the framework */,
    int *resErrors                      /* resulting number of errors */,
    int *resWarnings                    /* resulting number of warnings */
);


/*****************************************************************************
 *
 * Reconfigure the driver's site management
 *
 * Authors: Michael Vogt
 *
 * Description: This function reads the site specific fields out of
 * the current specific configuration and reconfigures the state
 * module and parts of the <frame> structure accordingly. These parts
 * are: the handler site IDs, the site mask, and the SmarTest to
 * handler site map.
 *
 * In case of an error or inconsitent configuration, the internal
 * state of the driver is NOT changed, error messages are logged and
 * the number of errors is returned. During driver initialization,
 * this should lead to an abort situation.
 *
 * Returns: The number of errors, that occured during this operation
 *
 ***************************************************************************/
void reconfigureSiteManagement(
    struct phFrameStruct *frame         /* the framework */,
    int *resErrors                      /* resulting number of errors */,
    int *resWarnings                    /* resulting number of warnings */
);


/*****************************************************************************
 *
 * Reconfigure the driver's bin management
 *
 * Authors: Michael Vogt
 *
 * Description: This function reads the bin specific fields out of the
 * current specific configuration and reconfigures the state module
 * and parts of the <frame> structure accordingly. These parts are the
 * bin mapping method, the handler retest bin, the handler bin IDs,
 * the SmarTest hardbin to handler map, and the SmarTest softbin to
 * handler map.
 *
 * In case of an error or inconsitent configuration, the internal
 * state of the driver is NOT changed, error messages are logged and
 * the number of errors is returned. During driver initialization,
 * this should lead to an abort situation.
 *
 * Returns: The number of errors, that occured during this operation
 *
 ***************************************************************************/
void reconfigureBinManagement(
    struct phFrameStruct *frame         /* the framework */,
    int *resErrors                      /* resulting number of errors */,
    int *resWarnings                    /* resulting number of warnings */
);


/*****************************************************************************
 *
 * Reconfigure the driver's needle cleaning mechanism
 *
 * Authors: Michael Vogt
 *
 * Description: This function reads the bin specific fields out of the
 * current specific configuration and reconfigures the cleaning part
 * of the <frame> structure accordingly. 
 *
 * In case of an error or inconsitent configuration, the internal
 * state of the driver is NOT changed, error messages are logged and
 * the number of errors is returned. During driver initialization,
 * this should lead to an abort situation.
 *
 * Returns: The number of errors, that occured during this operation
 *
 ***************************************************************************/
void reconfigureNeedleCleaning(
    struct phFrameStruct *frame         /* the framework */,
    int *resErrors                      /* resulting number of errors */,
    int *resWarnings                    /* resulting number of warnings */
);


/*****************************************************************************
 *
 * Reconfigure the framework
 *
 * Authors: Jiawei Lin
 *
 * Description: This function reads the driver parameters out of the
 * current specific configuration and reconfigures "framwork" structure 
 *
 * In case of an error or inconsitent configuration, the internal
 * state of the driver is NOT changed, error messages are logged and
 * the number of errors is returned. During driver initialization,
 * this should lead to an abort situation.
 *
 * Returns: The number of errors, that occured during this operation
 *
 ***************************************************************************/
void reconfigureFramework(
    struct phFrameStruct *frame         /* the framework */,
    int *resErrors                      /* resulting number of errors */,
    int *resWarnings                    /* resulting number of warnings */
);

#endif /* ! _RECONFIGURE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
