/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_hhook.h
 * CREATED  : 27 May 1999
 *
 * CONTENTS : Interface to hook framework
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 27 May 1999, Michael Vogt, created
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

#ifndef _PH_HHOOK_H_
#define _PH_HHOOK_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_mhcom.h"
#include "ph_hfunc.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_tcom.h"
#include "ph_frame.h"
#include "ph_event.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/* Enumeration (flag) type, to define the existent functionality in
   the hook plugin: All functions should be implemented as stub
   functions. Dummy stubs must return a PHHOOK_ERR_NA error code, if
   called.

   Insert more entries below the PHHOOK_AV_DIAG line. Never change
   existing entries */
typedef enum {
    PHHOOK_AV_INIT        = 0x00000001  /* used for phHookInit(3)           */,
    PHHOOK_AV_ACTION_START= 0x00000002  /* used for phHookActionStart(3)    */,
    PHHOOK_AV_ACTION_DONE = 0x00000004  /* used for phHookActionDone(3)     */,
    PHHOOK_AV_PROBLEM     = 0x00000008  /* used for phHookProblem(3)        */,
    PHHOOK_AV_WAITING     = 0x00000010  /* used for phHookWaiting(3)        */,
    PHHOOK_AV_POPUP       = 0x00000020  /* used for phHookPopup(3)          */,

    PHHOOK_AV_MINIMUM     = 0x00000001  /* minimum functional set: init     */
} phHookAvailability_t;

typedef enum {
    PHHOOK_ERR_OK = 0      /* no error */,
    PHHOOK_ERR_NOT_INIT    /* not initialized */,
    PHHOOK_ERR_NA          /* plugin function is not available for this
			      handler type, see phHookAvailable(3) */,
} phHookError_t;

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
 * Check availability of hook functions
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * Each hook plugin must implement this function. It is specific for
 * the implemented hook plugin library. The task of this function is
 * to return a flag mask which summarizes the availability of hook
 * plugin functions. The result is a binary ORed mask of elements of
 * the phHookAvailability_t enumeration. Future plugins may increase
 * the number of provided functions but always must keep the existing
 * enumerator entries unchanged.
 *
 * Before calling a plugin function (e.g. phHookActionStart(3)), the
 * calling framework must check once, whether the plugin function is
 * implemented or not. E.g. 
 *
 * if (phHookAvailable() & PHFUNC_AV_ACTION_START) phHookActionStart(...);
 *
 * This check is usually not repeated. It only done one time at driver
 * startup.
 *
 * Note: Don't use this function for internal initialization of the
 * hook framework. The function phHookInit(3) will be called later to
 * perform these steps.
 *
 * Note: To use this function, call phPlugHookAvailable()
 *
 * Returns: The availability mask
 *
 *****************************************************************************/
typedef phHookAvailability_t (phHookAvailable_t)(void);

#ifdef _PH_HHOOK_INTERNAL_
phHookAvailable_t phHookAvailable;
#endif
phHookAvailable_t phPlugHookAvailable;


/*****************************************************************************
 *
 * Initialize handler specific hook framework
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function is mandatory for each implementation of hook
 * frameworks. It provides the only way to tell the hook functions the
 * different IDs, which are necessary to communicate with the rest of
 * the driver framework. This function is called only once during a
 * driver session. It is called just after the availability of hook
 * functions is checked with phHookAvailable(3).
 *
 * The initialization function must keep copies of the passed IDs of
 * the surounding framework. The IDs are needed to influence the
 * driver from within hook functions. (See documentation of the other
 * modules of the driver framework).
 * 
 * Returns: error code
 *
 *****************************************************************************/
typedef phHookError_t (phHookInit_t)(
    phFuncId_t driver            /* the driver plugin ID */,
    phComId_t communication      /* the valid communication link to the HW
			            interface used by the handler */,
    phLogId_t logger             /* the data logger to be used */,
    phConfId_t configuration     /* the configuration manager */,
    phStateId_t state            /* the overall driver state */,
    phEstateId_t estate          /* the equipment specific state */,
    phTcomId_t tester            /* the tester interface */
);

#ifdef _PH_HHOOK_INTERNAL_
phHookInit_t phHookInit;
#endif
phHookInit_t phPlugHookInit;


/*****************************************************************************
 *
 * Driver action start hook
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * If available, this function is called at any time, the application
 * model places a call to the handler driver. No other actions
 * (besides loging of the incoming call) takes place before this hook
 * function is called. The hook may take over the complete control of
 * the incoming test cell client request. However, this is not
 * recommended! The function should mainly be used to notify the hook
 * framework about incoming calls, not to take over control.
 *
 * The hook function should set the <result> parameter to either
 * PHEVENT_RES_VOID or PHEVENT_RES_CONTINUE, to signal acceptance of
 * the test cell client's call, or PHEVENT_RES_HANDLED, if the hook
 * has taken over control over the incoming call and has performed all
 * necessary actions. In case of PHEVENT_RES_ABORT, the hook function
 * signals that the driver call should be aborted immediately (control
 * gets back to the test cell client). In this case the hook function
 * must set <comment_out>, <comlen> and <stReturn> to correct
 * values. In all other cases, these values are ignored.
 *
 * Note: When the test cell client calls the driver framework for the
 * first time (ph_driver_start call), first the whole driver framework
 * and hook framework is initialized, before this function is called
 * to notify the incoming applicatin model call. In all other cases,
 * this hook is called immediately at the beginning of the incoming
 * call, just after loging the call.
 *
 * Returns: error code
 *
 *****************************************************************************/
typedef phHookError_t (phHookActionStart_t)(
    phFrameAction_t call          /* the identification of an incoming
				     call from the test cell client */,
    phEventResult_t *result       /* the result of this hook call,
				     must be set by the hook function */,
    char *parm_list_input         /* the parameters passed over from
				     the test cell client */,
    int parmcount                 /* the parameter count */,
    char *comment_out             /* the comment to be send back to
				     the test cell client in case of
				     PHEVENT_RES_ABORT */,
    int *comlen                   /* the length of the resulting
				     comment in case of
				     PHEVENT_RES_ABORT */,
    phTcomUserReturn_t *stReturn  /* SmarTest return value, if
				     <result> is set to
				     PHEVENT_RES_ABORT by this hook
				     function */
);

#ifdef _PH_HHOOK_INTERNAL_
phHookActionStart_t phHookActionStart;
#endif
phHookActionStart_t phPlugHookActionStart;


/*****************************************************************************
 *
 * Driver action done hook
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * If available, this function is called at any time, the driver has
 * completely handled a call from the test cell client.  No further
 * actions will take place after calling this hook function.  The hook
 * function will receive the incoming parameters from the application
 * model as well as the already set and valid result values from the
 * drivers actions.  The hook function may overrule these values
 * However, this is not recommended! The function should mainly be
 * used to notify the hook framework about performed calls (e.g. to
 * implement internal data loging, etc.), and not to take over
 * control.
 *
 * Returns: error code
 *
 *****************************************************************************/
typedef phHookError_t (phHookActionDone_t)(
    phFrameAction_t call          /* the identification of an incoming
				     call from the test cell client */,
    char *parm_list_input         /* the parameters passed over from
				     the test cell client */,
    int parmcount                 /* the parameter count */,
    char *comment_out             /* the comment to be send back to
				     the test cell client, as already
				     set by the driver */,
    int *comlen                   /* the length of the resulting
				     comment */,
    phTcomUserReturn_t *stReturn  /* SmarTest return value as already
				     set by the driver framework */
);

#ifdef _PH_HHOOK_INTERNAL_
phHookActionDone_t phHookActionDone;
#endif
phHookActionDone_t phPlugHookActionDone;


/*****************************************************************************
 *
 * Problem hook
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This hook is called during error conditions. Based on the passed
 * <reason>, one of the fields of the additional <data> union
 * provides additional information (like elapsed time or subsequent
 * error codes). The <result> of this hook call should be one of
 * PHEVENT_RES_VOID, PHEVENT_RES_CONTINUE, PHEVENT_RES_HANDLED, or
 * PHEVENT_RES_ABORT. In case of PHEVENT_RES_ABORT the <stReturn>
 * value must be set to a correct value.
 *
 * Returns: error code
 *
 *****************************************************************************/
typedef phHookError_t (phHookProblem_t)(
    phEventCause_t reason         /* the reason, why this hook is called */,
    phEventDataUnion_t *data      /* pointer to additional data,
				     associated with the given
				     <reason>, or NULL, if no
				     additional data is available */,
    phEventResult_t *result       /* the result of this hook call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHEVENT_RES_ABORT by this hook
				     function */
);

#ifdef _PH_HHOOK_INTERNAL_
phHookProblem_t phHookProblem;
#endif
phHookProblem_t phPlugHookProblem;


/*****************************************************************************
 *
 * Waiting for parts hook
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This hook is called periodically during waiting for parts insertion
 * by the handler. The period of these calls is defined in the handler
 * configuration (flag_check_interval, usually set within the range of 2
 * to 5 seconds). This hook should NOT be used to handle timeouts,
 * while waiting for parts. If a final timeout occurs, the problem hook
 * will be called.
 *
 * The <result> of this hook call must be one of PHEVENT_RES_VOID,
 * PHEVENT_RES_CONTINUE, PHEVENT_RES_HANDLED, or PHEVENT_RES_ABORT. In case
 * of PHEVENT_RES_ABORT the <stReturn> value must be set to a correct
 * value.
 *
 * Returns: error code
 *
 *****************************************************************************/
typedef phHookError_t (phHookWaiting_t)(
    long elapsedTimeSec           /* elapsed time while waiting for
				     this (group of) device(s) */,
    phEventResult_t *result       /* the result of this hook call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHEVENT_RES_ABORT by this hook
				     function */
);

#ifdef _PH_HHOOK_INTERNAL_
phHookWaiting_t phHookWaiting;
#endif
phHookWaiting_t phPlugHookWaiting;


/*****************************************************************************
 *
 * Message popup hook
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This hook is called at any time, a popup message will be presented
 * to the operator. The message (defined with <msg>, <b2s>, ...,
 * <b7s>, <response>) is of similar format like in the CPI call
 * MsgDisplayMsgGetResponse(). If the parameter <input> is not NULL,
 * the message also queries for an answer string (like in CPI call
 * MsgDisplayMsgGetStringResponse().
 *
 * The hook function may decide to present the message to the
 * operator. In this case, it must handle all passed parameters
 * correctly. If the message if presented by the hook function, the
 * hook function must return the <result> PHEVENT_RES_HANDLED and set
 * <response> and <input> field (if not NULL). Otherwise the calling
 * driver framework will present the message to the operator on its
 * own. In case of PHEVENT_RES_ABORT, the message is also not prompted
 * to the operator from outside this hook function. Then also the
 * <stReturn> value must be set correctly.
 *
 * In case the <needQuitAndCont> is set to 0, the QUIT and CONTINUE
 * buttons as known from the usual user dialog boxes are not really
 * needed. It then would be very good not to display them. However,
 * the <response> value returned must not be adapted to a lower count
 * of visible buttons. A <response> of 2 must always refer to <b2s>
 * and so on.
 *
 * The <result> of this hook function must be one of PHEVENT_RES_VOID,
 * PHEVENT_RES_CONTINUE, PHEVENT_RES_HANDLED, or PHEVENT_RES_ABORT.
 *
 * Returns: error code
 *
 *****************************************************************************/
typedef phHookError_t (phHookPopup_t)(
    phEventResult_t *result       /* the result of this hook call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHEVENT_RES_ABORT by this hook
				     function */,
    int  needQuitAndCont          /* we really need the default QUIT
				     and CONTINUE buttons to be
				     displayed */,
    const char *msg               /* the message to be displayed */,
    const char *b2s               /* additional button with text */,
    const char *b3s               /* additional button with text */,
    const char *b4s               /* additional button with text */,
    const char *b5s               /* additional button with text */,
    const char *b6s               /* additional button with text */,
    const char *b7s               /* additional button with text */,
    long *response                /* response (button pressed), ranging
				     from 1 ("quit") over 2 to 7 (b2s
				     to b7s) up to 8 ("continue") */,
    char *input                   /* pointer to area to fill in reply
				     string from operator or NULL, if no 
				     reply is expected */
);

#ifdef _PH_HHOOK_INTERNAL_
phHookPopup_t phHookPopup;
#endif
phHookPopup_t phPlugHookPopup;

#ifdef __cplusplus
}
#endif

#endif /* ! _PH_HHOOK_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
