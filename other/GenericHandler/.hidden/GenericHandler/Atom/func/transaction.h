/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : transaction.h
 * CREATED  : 01 Jul 2023
 *
 * CONTENTS : Basic GPIB transactions
 *
 * AUTHORS  : Zuria Zhu, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, created
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

#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

#define PhFuncTaCheck(x) \
    { \
        phFuncError_t __phPFunTaRetVal = (x); \
        if (__phPFunTaRetVal != PHFUNC_ERR_OK) \
        { \
            return __phPFunTaRetVal; \
        } \
    } 

/*--- typedefs --------------------------------------------------------------*/

typedef phFuncError_t (phFuncTaSRQHandlerFoo_t)(phFuncId_t handlerID, int srq);

struct srqHandler
{
    int srq;
    phFuncTaSRQHandlerFoo_t *foo;
    struct srqHandler *next;
};

struct transaction
{
    phFuncAvailability_t    lastCall;
    phFuncAvailability_t    currentCall;
    int step;                               /* current step counter */
    int stepsDone;                          /* number of basic transaction
					       steps already performed for
					       the current plugin call */
    int stepMark;
    int stepsDoneMark;
    int                      doAbort;
    struct srqHandler        *handler;
    int                      doPush;
    struct transaction       *pop;
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

void phFuncTaInit(
    phFuncId_t handlerID      /* driver plugin ID */
);

void phFuncTaSetCall(
    phFuncId_t handlerID      /* driver plugin ID */,
    phFuncAvailability_t call
);

void phFuncTaSetSpecialCall(
    phFuncId_t handlerID      /* driver plugin ID */,
    phFuncAvailability_t call
);

void phFuncTaGetLastCall(
    phFuncId_t handlerID      /* driver plugin ID */,
    phFuncAvailability_t *call
);

void phFuncTaStart(
    phFuncId_t handlerID      /* driver plugin ID */
);

void phFuncTaStop(
    phFuncId_t handlerID      /* driver plugin ID */
);

int phFuncTaAskAbort(
    phFuncId_t handlerID      /* driver plugin ID */
);

void phFuncTaDoAbort(
    phFuncId_t handlerID      /* driver plugin ID */
);

void phFuncTaDoReset(
    phFuncId_t handlerID      /* driver plugin ID */
);

void phFuncTaRemoveStep(
    phFuncId_t handlerID      /* driver plugin ID */
);

void phFuncTaMarkStep(
    phFuncId_t handlerID      /* driver plugin ID */
);

void phFuncTaRemoveToMark(
    phFuncId_t handlerID      /* driver plugin ID */
);

void phFuncTaAddSRQHandler(
    phFuncId_t handlerID      /* driver plugin ID */,
    int srq                   /* srq to handle */,
    phFuncTaSRQHandlerFoo_t *foo /* the handler function to use */
);

/*****************************************************************************
 *
 * Perform a message send step
 *
 * Authors: Zuria Zhu
 *
 * Description: This function tries to send the given message over the
 * existing GPIB connection under the following conditions: The
 * handlerID data struture contains fields that log the current and the
 * last call to the plugin layer. It also contains a counter of
 * transactions steps performed since call of the plugin function and
 * a counter of already successfully performed transactions within a
 * necessary sequence. If the current call to this function was not
 * yet successfully completed, it is performed now. Otherwise the
 * function returns without any action. In case of problems or errors,
 * the return value will be set accordingly.
 * In case of success, or, if no action was performed, the function
 * returns with PHFUNC_ERR_OK
 *
 * Warnings: The mechanism only works, if a plugin function always
 * executes the same sequence of basic transactions until the pugin
 * function finally succeeds or a new plugin function is called.
 *
 * Notes: This function is to be used in conjunction with
 * phFuncTaSend(3), phFuncTaReceive(3), phFuncTaGetSRQ(3), and the
 * macros PhFuncTaCheck(3)
 *
 * Returns: error code
 *
 ***************************************************************************/
phFuncError_t phFuncTaSend(
    phFuncId_t handlerID     /* driver plugin ID */,
    const char *format       /* format string like in printf */,
    ...                      /* variable argument list */
);

/*****************************************************************************
 *
 * Perform a message receive step
 *
 * Authors: Zuria Zhu
 *
 * Description: This function tries to receive message over the
 * existing GPIB connection and scan the given values under the
 * following conditions: The handlerID data struture contains fields
 * that log the current and the last call to the plugin layer. It also
 * contains a counter of transactions steps performed since call of
 * the plugin function and a counter of already successfully performed
 * transactions within a necessary sequence. If the current call to
 * this function was not yet successfully completed, it is performed
 * now. Otherwise the function returns without any action. In case of
 * problems or errors, the return value will be set accordingly.  In
 * case of success, or, if no action was performed, the function
 * returns with PHFUNC_ERR_OK
 *
 * Warnings: The mechanism only works, if a plugin function always
 * executes the same sequence of basic transactions until the pugin
 * function finally succeeds or a new plugin function is called.
 *
 * Notes: This function is to be used in conjunction with
 * phFuncTaSend(3), phFuncTaReceive(3), phFuncTaGetSRQ(3), and the
 * macros PhFuncTaCheck(3)
 *
 * Returns: error code
 *
 ***************************************************************************/
phFuncError_t phFuncTaReceive(
    phFuncId_t handlerID     /* driver plugin ID */,
    int nargs                /* number of arguments in the variable part */,
    const char *format       /* format string like in scanff */,
    ...                      /* variable argument list */
);

/*****************************************************************************
 *
 * Clear the GPIB interface
 *
 * Authors: Zuria Zhu
 *
 * Description: This function clears the GPIB interface,
 * In case of problems or errors, the return value with PHFUNC_ERR_WAITING.
 * In case of success, or, if no action was performed, the function
 * returns with PHFUNC_ERR_OK
 *
 *
 * Returns: error code
 *
 ***************************************************************************/
phFuncError_t phFuncTaClearGpib(
    phFuncId_t handlerID      /* driver plugin ID */
);

/*****************************************************************************
 *
 * Perform a SRQ receive step
 *
 * Authors: Zuria Zhu
 *
 * Description: This function tries to receive an SRQ over the
 * existing GPIB connection and compares it with the given list of
 * accepted SRQs under the following conditions: The handlerID data
 * struture contains fields that log the current and the last call to
 * the plugin layer. It also contains a counter of transactions steps
 * performed since call of the plugin function and a counter of
 * already successfully performed transactions within a necessary
 * sequence. If the current call to this function was not yet
 * successfully completed, it is performed now. Otherwise the function
 * returns without any action. In case of problems or errors, the
 * return value will be set accordingly.  In case of success, or, if
 * no action was performed, the function returns with PHFUNC_ERR_OK
 *
 * Warnings: The mechanism only works, if a plugin function always
 * executes the same sequence of basic transactions until the pugin
 * function finally succeeds or a new plugin function is called.
 *
 * Notes: This function is to be used in conjunction with
 * phFuncTaSend(3), phFuncTaReceive(3), phFuncTaGetSRQ(3), and the
 * macros PhFuncTaCheck(3)
 *
 * Returns: error code
 *
 ***************************************************************************/
phFuncError_t phFuncTaGetSRQ(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *srq                 /* received SRQ byte */,
    int nargs                /* number of arguments in the variable
                                part */,
    ...                      /* variable argument list, must be a list
                                of (int) values, representing accepted
                                SRQs */
);

/*****************************************************************************
 *
 * Test for a SRQ receive
 *
 * Authors: Zuria Zhu
 *
 * Description: This function tries to receive an SRQ over the
 * existing GPIB connection and compares it with the given list of
 * accepted SRQs under the following conditions: The handlerID data
 * struture contains fields that log the current and the last call to
 * the plugin layer. It also contains a counter of transactions steps
 * performed since call of the plugin function and a counter of
 * already successfully performed transactions within a necessary
 * sequence. If the current call to this function was not yet
 * successfully completed, it is performed now. Otherwise the function
 * returns without any action. In case of problems or errors, the
 * return value will be set accordingly.  In case of success, or, if
 * no action was performed, the function returns with PHFUNC_ERR_OK
 *
 * Warnings: The mechanism only works, if a plugin function always
 * executes the same sequence of basic transactions until the pugin
 * function finally succeeds or a new plugin function is called.
 *
 * Notes: Compared to phFuncTaGetSRQ() this function returns with
 * PHFUNC_ERR_OK, even, if no SRQ was received within the internal
 * defined timeout. This mat be used to check for any more pending
 * SRQs from the handler or in cases where it is not known whether the
 * handler sends an SRQ or not.
 *
 * Notes: This function is to be used in conjunction with
 * phFuncTaSend(3), phFuncTaReceive(3), phFuncTaGetSRQ(3), and the
 * macros PhFuncTaCheck(3)
 *
 * Warning: This function may delay the driver if called and no SRQ is
 * pending. It must only be used, if there is really a chance to
 * receive an SRQ or within operations that are slow by nature
 * (e.g. wafer handling)
 *
 * Returns: error code
 *
 ***************************************************************************/
phFuncError_t phFuncTaTestSRQ(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *received            /* set, if another srq was received */,
    int *srq                 /* received SRQ byte */,
    int fast                 /* fast return, if set. Otherwise a
                                timeout is applied, if no SRQ is
                                pending */
);

/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 285 */
void phFuncTaMarkStep(
    phFuncId_t handlerID      /* driver plugin ID */
);
void phFuncTaRemoveToMark(
    phFuncId_t handlerID      /* driver plugin ID */
);
/* End of Huatek Modification */


#endif /* ! _TRANSACTION_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
