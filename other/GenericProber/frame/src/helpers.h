/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : helpers.h
 * CREATED  : 14 Dec 1999
 *
 * CONTENTS : Gobal help functions for the framework
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Dec 1999, Michael Vogt, created
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

#ifndef _HELPERS_H_
#define _HELPERS_H_

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

int phFrameCheckAbort(
    struct phFrameStruct **f
);

int phFrameCheckInit(
    struct phFrameStruct **f
);

int phFramePanic(
    struct phFrameStruct *frame, 
    const char *message
);

int phFramePerformEntrySteps(
    struct phFrameStruct *frame,
    phFrameAction_t call,
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

void phFramePerformExitSteps(
    struct phFrameStruct *frame,
    phFrameAction_t call,
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */, 
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are: 
                              CI_CALL_PASS (function completed successfully),
                              CI_CALL_ERROR (function call failed), 
                              CI_CALL_BREAKD (break) */
);

void phFrameDescribeStateError(
    struct phFrameStruct *frame,
    phStateError_t error, 
    phFrameAction_t action
);

#endif /* ! _HELPERS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
