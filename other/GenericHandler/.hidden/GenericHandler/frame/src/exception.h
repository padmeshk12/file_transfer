/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : exception.h
 * CREATED  : 15 Dec 1999
 *
 * CONTENTS : waiting and timeout management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Dec 1999, Michael Vogt, created
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

#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

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
 * wait for something
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function is called repeatedly while the driver is waiting for
 * some prober action to be completed. The task of this function is to
 * check the current waiting time against the acceptable waiting time,
 * to call hook functions, to check system flags and to eventually
 * cause abort actions on operator request.
 *
 * First the system flags are checked. In case of a QUIT ABORT or
 * RESET, the waiting will be stopped by returning with <result> set
 * to PHEVENT_RES_ABORT. In that case a message is printed and the
 * <stReturn> is set to PASS (the call was stopped from outside of the
 * driver).
 *
 * The next step is to give the waiting-event handler a chance to jump
 * in. This event handler also calls the waiting-hook function, if
 * available. Based on the return value of the event handler, this
 * funciton may return with the proposed return values.
 *
 * If the pause flag is set, but was not set as the call came in from
 * the test cell client, generate the timeout error-event now.
 *
 * If the current short timer of the framework has not yet reached the
 * value that is configured in <timeConf>, the function will now
 * return with <result> set to PHEVENT_RES_CONTINUE.
 *
 * If the timeout has been reached, a timeout error-event is generated
 * to the event handler, passing down the elapsed time the current
 * action and the configured timeout action from <actionConf>. The
 * event handler may cause the hook problem-function (if existent)
 * and/or initate more steps based on the configured value. The result
 * is passed up to the calling function. In case of CONTINUE, the
 * short timer is reset.
 *
 ***************************************************************************/
void phFrameHandlePluginResult(
    struct phFrameStruct *f             /* the framework data */,
    phFuncError_t funcError             /* the plugin result */,
    phFuncAvailability_t p_call         /* current call to the plugin */,

    int *completed                      /* plugin call is completed or
                                           aborted */,
    int *success                        /* plugin call or exception
                                           handling was successful */,
    phTcomUserReturn_t *retVal          /* possibly changed return
                                           value, in case of completed
                                           call */
    
);

#endif /* ! _EXCEPTION_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
