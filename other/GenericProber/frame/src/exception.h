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
 *            14 Nov 2000, Chris Joyce, added phFrameHandlePluginResult()
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




/* 
 * Enumeration type for action that needs to be performed as a result of
 * a plugin call 
 */
typedef enum {
    PHFRAME_EXCEPTION_ERR_OK,
    PHFRAME_EXCEPTION_ERR_LEVEL_COMPLETE,
    PHFRAME_EXCEPTION_ERR_ERROR,
    PHFRAME_EXCEPTION_ERR_PAUSED
} exceptionActionError_t;



/*****************************************************************************
 *
 * handle plugin result
 *
 * Authors: Michael Vogt
 *          Chris Joyce
 *
 * Description:
 *
 * This function is called by the framework after any plugin call is made.
 * Its job is to interpret the return value funcError from the plugin and
 * convert it into an exceptionActionError_t and report whether the 
 * plugin call was completed and successful.
 * Exception handling routines may be called under certain circumstances:
 *
 * 1)  if PHPFUNC_ERR_TIMEOUT or PHPFUNC_ERR_WAITING result is returned by
 *     the plugin call then an internal phFrameWaiting() function is called.
 *
 * phFrameWaiting()
 * This function is called when the driver is waiting for
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
 * function may return with the proposed return values.
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
 * and/or initate more steps based on the configured value. In case of 
 * CONTINUE, the short timer is reset.
 *
 * 2)  if PHPFUNC_ERR_GPIB result is returned by
 *     the plugin call then an internal phFrameGpibProblem() function 
 *     is called.
 *
 * phFrameGpibProblem()
 * This function is called when the plug returns a communication error 
 * problem. After checking the QUIT, RESET and ABORT system flags the 
 * phEventError() handling routine is called. 
 *
 * 3)  if PHPFUNC_ERR_ANSWER returned by the plugin call then an internal 
 *     phFrameErrorAnswer() function is called.
 *
 * phFrameErrorAnswer()
 * This function is called when the plugin is unable to interpret a message 
 * received from the prober. After checking the QUIT, RESET and ABORT system 
 * flags the phEventError() handling routine is called. 
 *
 * In each of these cases the results of the exception handling routine are
 * passed back to phFrameHandlePluginResult() which interprets the results in
 * terms of the exceptionActionError_t and whether the intention of the original 
 * plugin call has now been completed or is successful.
 *
 ***************************************************************************/
void phFrameHandlePluginResult(
    struct phFrameStruct *f             /* the framework data */,
    phPFuncError_t funcError            /* the plugin result */,
    phPFuncAvailability_t p_call        /* current call to the plugin */,

    int *completed                      /* plugin call is completed or
                                           aborted */,
    int *success                        /* plugin call or exception
                                           handling was successful */,
    phTcomUserReturn_t *retVal          /* possibly changed return
                                           value, in case of completed
                                           call */,
    exceptionActionError_t *action      /* action to be performed
                                           as result of plugin call */
);


#endif /* ! _EXCEPTION_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
