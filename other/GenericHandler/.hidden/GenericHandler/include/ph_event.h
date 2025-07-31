/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_event.h
 * CREATED  : 31 May 1999
 *
 * CONTENTS : Eventhandler for driver framework
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 31 May 1999, Michael Vogt, created
 *            Dec 2005, Jiawei Lin, CR28409
 *              enhance "phEventError" to disable the popup window if the
 *              user dislike it; add one new parameter "displayWindowForOperator"
 *              for "phEventError" function
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

#ifndef _PH_EVENT_H_
#define _PH_EVENT_H_

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

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif

typedef struct phEventStruct *phEventId_t;

typedef enum {
    PHEVENT_ERR_OK = 0              /* no error */,
    PHEVENT_ERR_INVALID_HDL         /* the passed ID is not valid */,
    PHEVENT_ERR_MEMORY              /* couldn't get memory from heap
                                       with malloc() */,
    PHEVENT_ERR_GUI                 /* an error occured in one of
                                       gui server calls */,
    PHEVENT_ERR_COMM                /* an error occured in one of
                                       communication calls */,
    PHEVENT_ERR_COMM_INIT_TIMEOUT  /* a timeout error occured during 
                                       initialization of communication */,
    PHEVENT_ERR_CONF                /* an error occured in one of
                                       configuration calls */,
    PHEVENT_ERR_DIAG                /* an error occured in one of
                                       diagnostic calls */,
    PHEVENT_ERR_PLUGIN              /* an error occured in a plugin
                                       call */,
    PHEVENT_ERR_INTERNAL            /* this is a bug */
} phEventError_t;

typedef enum {
    PHEVENT_RES_VOID        /* event has done nothing which could
			       disturb the operation of the driver. No
			       external actions are necessary */,
    PHEVENT_RES_CONTINUE    /* event handler has done something
			       (e.g. prompted a message, etc.), but it
			       is necessary to continue the action
			       which caused calling of this event
			       (e.g. applicable while waiting for
			       parts, if problem was solved inside of
			       event) */,
    PHEVENT_RES_HANDLED     /* event handler has handled the problem,
			       e.g. prompted a message to the operator */,
    PHEVENT_RES_ABORT       /* abort action which caused calling of
			       this event. Return controll to SmarTest
			       immediately. Return with provided return
			       value */
} phEventResult_t;

typedef enum {
    PHEVENT_CAUSE_WP_TIMEOUT /* timeout during waiting for part */,
    PHEVENT_CAUSE_IF_PROBLEM /* HW interface does not work */,
    PHEVENT_CAUSE_ANSWER_PROBLEM /* plugin has not understood
                                    answer received from handler */
} phEventCause_t;

typedef enum {
    PHEVENT_OPACTION_CONTINUE /* continue to wait */,
    PHEVENT_OPACTION_ASK      /* ask operator what to do */,
    PHEVENT_OPACTION_SKIP     /* skip the current operation */
} phEventOpAction_t;

typedef union {

    struct {
	phFrameAction_t am_call;       /* current app model call */
	phFuncAvailability_t p_call;   /* current call to the plugin */
	long elapsedTimeSec;           /* total elapsed waiting time */
	phEventOpAction_t opAction;    /* proposed operator action */
    } to;                              /* valid, for timeout errors */

    struct {
	phFrameAction_t am_call;       /* current app model call */
	phFuncAvailability_t p_call;   /* current call to the plugin */
    } intf;                            /* valid, for interface errors */

#if 0
    long elapsedTimeSec      /* valid, for timeout errors */;
    phComError_t ifError     /* valid, if an interface problem occured */;
#endif

} phEventDataUnion_t;

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
 * Initialize driver event handler
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function initializes the driver event handler. During
 * initialization a possible hook plugin is loaded. The event handler
 * is the place where user interactions take place. Besides the
 * logger, this should be the only place within the driver framework
 * to produce user readable output.
 *
 * Since the event handler implements general error recovery
 * strategies (independent from the used driver plugin), the error
 * recovery is kept general and can not cover handler specific
 * functionality. The event handler will become active on general
 * communication problems to the handler, on timeouts, for single
 * stepping or hand test mode, etc. The operator will be asked
 * questions to recover from the problem/error. All communication will
 * be logged with the logger.
 * 
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventInit(
    phEventId_t *eventID         /* result event handler ID */,
    phFuncId_t driver            /* the driver plugin ID */,
    phComId_t communication      /* the valid communication link to the HW
			            interface used by the handler */,
    phLogId_t logger             /* the data logger to be used */,
    phConfId_t configuration     /* the configuration manager */,
    phStateId_t state            /* the overall driver state */,
    phEstateId_t estate          /* the equipment specific state */,
    phTcomId_t tester            /* the tester communication */
);


/*****************************************************************************
 *
 * Driver action event
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The main purpose of this event entry is to pass the driver call to
 * subsequent hook layer, if existent. See phEventActionHook(3) for
 * more details.
 *
 * Single Step Mode:
 * Another main purpose is to provide the single step mechanism (ask
 * the operator for confirmation of the next step, if single step mode
 * is active):
 * In single step mode a popup dialog box will appear that asks the operator
 * to choose between one of the following options:
 *
 * QUIT - Set quit flag, PHEVENT_RES_ABORT, PHTCOM_CI_CALL_PASS
 *       (double confirmation, eventually repeat these questions in a loop)
 *
 * STEP - continue in step mode
 *       -> PHEVENT_RES_CONTINUE
 *
 * DUMP - (write driver's current trace for analysis)
 *
 * CONTINUE - break out of single step mode: set mode to either 
 *            normal mode or hand test mode.
 *       -> PHEVENT_RES_CONTINUE
 *
 * The <result> of this event call may be one of PHEVENT_RES_CONTINUE,
 * or PHEVENT_RES_ABORT. In case of PHEVENT_RES_ABORT the <stReturn>
 * value must be set to a correct value.
 *
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventAction(
    phEventId_t eventID           /* event handler ID */,    
    phFrameAction_t call          /* the identification of an incoming
				     call from the test cell client */,
    phEventResult_t *result       /* the result of this event call */,
    char *parm_list_input         /* the parameters passed over from
				     the test cell client */,
    int parmcount                 /* the parameter count */,
    char *comment_out             /* the comment to be send back to the
				     test cell client */,
    int *comlen                   /* the length of the resulting
				     comment */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHEVENT_RES_ABORT by this event
				     function */
);

/*****************************************************************************
 *
 * Driver action done event
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function is called at any time, the driver has completely
 * handled a call from the test cell client.  The main purpose of
 * this event entry is to pass the driver call to subsequent hook
 * layer, if existent. See phEventActionHook(3) for more details.
 *
 * No further actions will take place after calling this event
 * function.  The event function will receive the incoming parameters
 * from the test cell client as well as the already set and valid
 * result values from the drivers actions.
 *
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventDoneAction(
    phEventId_t eventID           /* event handler ID */,    
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
				     set by the driver */
);

/*****************************************************************************
 *
 * Error event
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This event is called during error conditions. Based on the passed
 * <reason>, one of the fields of the additional <data> union may
 * provide additional information (like elapsed time or subsequent
 * error codes). In general the <result> of this event call should be
 * one of PHEVENT_RES_CONTINUE, PHEVENT_RES_HANDLED, or
 * PHEVENT_RES_ABORT. The valid selection depends on the event
 * reason. In case of PHEVENT_RES_ABORT always the <stReturn> value
 * must be set to a correct value, the driver state must be valid and
 * necessary SmarTest flags must be set.
 *
 * Timeout:
 *
 * In case of a PHEVENT_CAUSE_WP_TIMEOUT, the event handler performs the
 * following actions:
 *
 * A popup dialog box will appear if either the pause flag is set or the 
 * configuration 'waiting_for_parts_timeout_action' is defined and set 
 * to "operator-help". Otherwise this function will return with the result
 * PHEVENT_RES_CONTINUE, indicating, that the calling function should continue
 * waiting for parts.
 * The popup dialog asks the operator for one of the three following 
 * options:
 *
 * QUIT - (no parts are there and the testprogram should be terminated)
 *       -> Set quit flag, PHEVENT_RES_ABORT, PHTCOM_CI_CALL_PASS
 *       (double confirmation, eventually repeat these questions in a loop)
 *
 * START - (parts are already inserted, test start signal was lost)
 *       -> fill in site state in state controller, PHEVENT_RES_HANDLED
 *
 * DUMP - (write driver's current trace for analysis)
 *
 * CONTINUE - (no parts are here so far but the handler will insert some soon)
 *       -> PHEVENT_RES_CONTINUE
 *
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventError(
    phEventId_t eventID           /* event handler ID */,    
    phEventCause_t reason         /* the reason, why this event is called */,
    phEventDataUnion_t *data      /* pointer to additional data,
				     associated with the given
				     <reason>, or NULL, if no
				     additional data is available */,
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHEVENT_RES_ABORT by this event
				     function */,
    int displayWindowForOperator  /* introduced by CR28409 */
);

/*****************************************************************************
 *
 * Waiting for parts event
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This event is called periodically during waiting for parts insertion
 * by the handler. The periodic of these calls is defined in the
 * handler configuration (usually within the range of 2 to 5
 * seconds). The event is not to handle timeouts, while
 * waiting for parts. If a final timeout occurs, the error event must be
 * called.
 *
 * The <result> of this event call is one of be one of
 * PHEVENT_RES_VOID, PHEVENT_RES_CONTINUE, PHEVENT_RES_HANDLED, or
 * PHEVENT_RES_ABORT. In case of PHEVENT_RES_ABORT the <stReturn>
 * value is set to a correct value. In case of PHEVENT_RES_HANDLED
 * devices are now inserted and the driver state was set accordingly.
 *
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventWaiting(
    phEventId_t eventID           /* event handler ID */,    
    long elapsedTimeSec           /* elapsed time while waiting for
				     this part */,
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */
);

/*****************************************************************************
 *
 * Message popup event
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This event is called at any time, a popup message will be presented
 * to the operator. The message (defined with <msg>, <b2s>, ...,
 * <b7s>, <response>) is of similar format like in the CPI call
 * MsgDisplayMsgGetResponse(). If the parameter <input> is not NULL,
 * the message also queries for an answer string (like in CPI call
 * MsgDisplayMsgGetStringResponse().
 *
 * The main purpose of this event is to pass the arguments to the
 * corresponding hook function (if existent) or to prompt the message
 * to the operator. 
 *
 * In case the hook function does NOT exist, depending on the <input>
 * parameter, either phTcomMsgDisplayMsgGetResponse() or
 * phTcomMsgDisplayMsgGetStringResponse() will be called. The <result>
 * function than will be set to PHEVENT_RES_HANDLED.
 *
 * In case the hook function is called and returned without error, the
 * <result> value of the hook function is checked. If set to
 * PHEVENT_RES_HANDLED or PHHOOK_RES_ABORT, this function will return
 * immediately, bypassing all parameters returned from the hook
 * function. In case the hook function results with other values this
 * function prompts the message to the operator as described
 * above.
 *
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventPopup(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHHOOK_RES_ABORT by underlying hook
				     function */,
    int  needQuitAndCont          /* we really need the default QUIT
				     and CONTINUE buttons to be
				     displayed */,
    const char *title                   /* title of dialog box */,
    const char *msg                     /* the message to be displayed */,
    const char *b2s                     /* additional button with text */,
    const char *b3s                     /* additional button with text */,
    const char *b4s                     /* additional button with text */,
    const char *b5s                     /* additional button with text */,
    const char *b6s                     /* additional button with text */,
    const char *b7s                     /* additional button with text */,
    long *response                      /* response (button pressed), ranging
				     from 1 ("quit") over 2 to 7 (b2s
				     to b7s) up to 8 ("continue") */,
    char *input                   /* pointer to area to fill in reply
				     string from operator or NULL, if no 
				     reply is expected */
);

/*****************************************************************************
 *
 * Wait for test start signal (handtest)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * In hand test mode, this function is called instead of the driver
 * plugin function phFuncGetStart(3) (see ph_hfunc.h). The purpose of
 * this function is to ask the operator to play the role of the
 * handler: to insert parts into the DUT board sockets. This function
 * must behave exactly the same in terms of postconditions:
 *
 * QUIT - The testprogram will be stopped (double confirmation) -> Set
 * quit flag, PHEVENT_RES_ABORT, PHTCOM_CI_CALL_PASS
 *
 * CONTINUE - The operator has inserted the parts -> the site
 * states has been modified in the state controller and PHFUNC_ERR_OK
 * is returned in <funcError>, PHEVENT_RES_HANDLED
 *
 * TIMEOUT - The operator wants to simulate a timeout error ->
 * PHFUNC_ERR_TIMEOUT is returned in <funcError>,
 * PHEVENT_RES_CONTINUE
 * 
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventHandtestGetStart(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    phFuncError_t *funcError      /* the simulated get start result */
);

/*****************************************************************************
 *
 * Bin tested devices (handtest)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * In hand test mode, this function is called instead of the driver
 * plugin function phFuncBinDevice(3) (see ph_hfunc.h). The purpose 
 * of this function is to ask the operator to play the role of the
 * handler: to remove parts from the DUT board sockets and bin
 * them. This function will display the binning information for each 
 * of the populated sites. After the operator presses CONTINUE the 
 * populated sites will be set to empty.
 * 
 * This function will behave exactly the same as phFuncBinDevice(3)
 * in terms of postconditions:
 *
 * QUIT - The testprogram will be stopped (double confirmation) -> Set
 * quit flag, PHEVENT_RES_ABORT, PHTCOM_CI_CALL_PASS
 *
 * CONTINUE - Set populated sites to empty. PHFUNC_ERR_OK must
 * be returned in <funcError>, PHEVENT_RES_HANDLED
 *
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventHandtestBinDevice(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    long *perSiteBinMap           /* bin map */
);

/*****************************************************************************
 *
 * reprobe devices (handtest)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * In hand test mode, this function is called instead of the driver
 * plugin function phFuncReprobe(3) (see ph_hfunc.h). The purpose of
 * this function is to ask the operator to play the role of the
 * handler: to remove and reinsert parts into the DUT board
 * sockets. This function must behave exactly the same in terms of
 * postconditions:
 *
 * QUIT - The testprogram will be stopped (double confirmation) -> Set
 * quit flag, PHEVENT_RES_ABORT, PHTCOM_CI_CALL_PASS
 *
 * CONTINUE - The operator has reinserted the parts ->
 * PHFUNC_ERR_OK must be returned in <funcError>, PHEVENT_RES_HANDLED
 *
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventHandtestReprobe(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    phFuncError_t *funcError      /* the simulated reprobe result */
);

/*****************************************************************************
 *
 * Bin or reprobe tested devices (handtest)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * In hand test mode, this function is called instead of the driver
 * plugin function phFuncBinReprobe(3) (see ph_hfunc.h). The purpose
 * of this function is to ask the operator to play the role of the
 * handler: to remove parts from the DUT board sockets and bin them or
 * to reprobe some parts. This function will display the binning and
 * reprobe information for each of the populated sites. After the
 * operator presses CONTINUE the populated sites which do not require
 * reprobe will be set to empty as during binning.
 * 
 * This function will behave exactly the same as phFuncBinDevice(3)
 * in terms of postconditions:
 *
 * QUIT - The testprogram will be stopped (double confirmation) -> Set
 * quit flag, PHEVENT_RES_ABORT, PHTCOM_CI_CALL_PASS
 *
 * CONTINUE - Set populated sites that do not require reprobe to
 * empty. PHFUNC_ERR_OK must be returned in <funcError>,
 * PHEVENT_RES_HANDLED
 *
 * Returns: error code
 *
 *****************************************************************************/
phEventError_t phEventHandtestBinReprobe(
    phEventId_t eventID           /* event handler ID */,        
    phEventResult_t *result       /* the result of this event call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     phEvent_RES_ABORT by this event
				     function */,
    long *perSiteReprobe          /* reprobe flags */,
    long *perSiteBinMap           /* bin map */,
    phFuncError_t *funcError      /* the simulated reprobe result */
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
 * This function free the driver event handler. 
 *
 * Returns: none
 *
 *****************************************************************************/
void phEventFree(phEventId_t *eventID);   
/*End of Huatek Modifications*/

//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_EVENT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
