/******************************************************************************
 *
 *       (c) Copyright Advantest, 2014
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_hhook_private.h
 * CREATED  : 16 Dec 2014
 *
 * CONTENTS : Example for hook function usage
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 16 Dec 2014, Magco Li , created
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

#ifndef _PH_HHOOK_PRIVATE_H_
#define _PH_HHOOK_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/* To keep everything clean, the hook layer should store all hook
   global information withing one single structure */

struct phHookStruct
{
    struct phHookStruct      *myself;     /* pointer to this structure */

    phFuncId_t               myDriver;    /* the driver plugin */
    phComId_t                myCom;       /* the hardware interface */
    phLogId_t                myLogger;    /* the message logger */
    phConfId_t               myConf;      /* the driver configuration */
    phStateId_t              myState;     /* the internal driver state */
    phEstateId_t             myEstate;    /* the equipment specific state */
    phTcomId_t               myTester;    /* the interface to SmarTest */
};

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
 * <give a SINGLE LINE function description here>
 *
 * Authors: <your name(s)>
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * <Blank lines, like the one just ahead, may be used to separate paragraphs>
 *
 * Input parameter format:
 * <Describe the format of the input parameter string here>
 *
 * Output result format:
 * <Describe the format of the output parameter string here>
 *
 * Warnings: <delete this, if not used>
 *
 * Notes: <delete this, if not used>
 *
 * Returns: <the function's return value, delete this if void>
 *
 ***************************************************************************/

void a_function(
    char  *parm_list_input /* input parameter string pointer, see
                              format definition */,
    int    parmcount       /* No. of tokens in *parm_list_input */,
    char  *comment_out     /* output/result buffer, see format definition */,
    int   *comlen          /* length (in bytes) of output data */,
    int   *state_out       /* return code of the function call. Valid
                              Return Codes are:
                              CI_CALL_PASS (function completed successfully),
                              CI_TEST_PASS (test passed),
                              CI_TEST_FAIL (test failed),
                              CI_CALL_ERROR (function call failed),
                              CI_CALL_BREAKD (break) */
);

#endif /* ! _PH_HHOOK_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
