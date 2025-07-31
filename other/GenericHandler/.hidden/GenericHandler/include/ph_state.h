/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_state.h
 * CREATED  : 28 May 1999
 *
 * CONTENTS : driver state control
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 28 May 1999, Michael Vogt, created
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

#ifndef _PH_STATE_H_
#define _PH_STATE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_frame.h"
#include "ph_tcom.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif

typedef struct phStateStruct *phStateId_t;

typedef enum {
    PHSTATE_ERR_OK = 0                /* no error */,
    PHSTATE_ERR_NOT_INIT              /* the state controller was not
					 initialized successfully */,
    PHSTATE_ERR_INVALID_HDL           /* the passed ID is not valid */,
    PHSTATE_ERR_MEMORY                /* couldn't get memory from heap
					 with malloc() */,
    PHSTATE_ERR_ILLTRANS              /* an illegal level state
					 transition was requested and
					 performed */,
    PHSTATE_ERR_IGNORED               /* a transition was ignored
					 since it was already effective. It 
					 was no illegal transition though */

} phStateError_t;

typedef enum {
    PHSTATE_LEV_START = 0             /* default starting state */,
    PHSTATE_LEV_DRV_STARTED           /* driver start was called,
					 driver is set up and fully 
					 initialized */,
    PHSTATE_LEV_DRV_DONE              /* driver was finished (end of
					 testprogram), may be restarted */,
    PHSTATE_LEV_LOT_STARTED           /* new lot started */,
    PHSTATE_LEV_LOT_DONE              /* lot finished */,
    PHSTATE_LEV_DEV_STARTED           /* device started, ready for test */,
    PHSTATE_LEV_DEV_DONE              /* device finished, devices are
					 binned unless retesting or 
					 checking */,
    PHSTATE_LEV_PAUSE_STARTED         /* pause mode started */,
    PHSTATE_LEV_PAUSE_DONE            /* pause mode completed, checked
					 for changed system flags */,
    PHSTATE_LEV_DONTCARE              /* internal value, will not
					 occur at interface level of this 
					 module */,

    PHSTATE_LEV__END                  /* end of enum list marker, don't change  */
} phStateLevel_t;

typedef enum {
    PHSTATE_DRV_NORMAL                /* normal operation mode */,
    PHSTATE_DRV_SST                   /* single step mode, prompt user
					 before each handler driver call */,
    PHSTATE_DRV_HAND                  /* hand test mode, handler is
					 diconnected, prompt user to insert 
					 parts, bin parts, ... */,
    PHSTATE_DRV_SST_HAND              /* combination of single step
					 and hand test mode */
} phStateDrvMode_t;

typedef enum {
    PHSTATE_SKIP_NORMAL               /* normal operation mode */,
    PHSTATE_SKIP_NEXT                 /* next device will be skipped
					 for test by SmarTest */,
    PHSTATE_SKIP_CURRENT              /* the current deveice was
					 skipped for test by SmarTest */,
    PHSTATE_SKIP_NEXT_CURRENT         /* the current device was
					 skipped and the next device will be 
					 skipped for test by SmarTest */
} phStateSkipMode_t;

typedef enum {
    PHSTATE_TST_NORMAL                /* normal operation mode */,
    PHSTATE_TST_RETEST                /* current device will be
					 retested, handler must not exchange 
					 devices */,
    PHSTATE_TST_CHECK                 /* current device will be
					 checked, handler must not exchange 
					 devices */
} phStateTesterMode_t;

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
 * Driver state control initialization
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function initializaes the state controller. It must be called
 * before any subsequent call to this module. The ID of the state
 * cotroller is returned in <stateID> and must be passed to any other
 * call into this module.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateInit(
    phStateId_t *stateID              /* the resulting driver state ID */,
    phTcomId_t testerID               /* access to the tester */,
    phLogId_t loggerID                /* logger to be used */
);

/*****************************************************************************
 *
 * Destroy a Driver state controller
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This function destroys a state controller. The stateID will become
 * invalid.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateDestroy(
    phStateId_t stateID               /* the state ID */
);

/*Begin of Huatek Modifications, Donnie Tu, 04/24/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free a Driver state controller
 *
 * Authors: Donnie Tu
 *
 * Description:
 * This function destroys and frees a state controller.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateFree(
    phStateId_t *stateID               /* the state ID */
);
/*End of Huatek Modifications*/

/*****************************************************************************
 *
 * Get current level state
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * This funciton returns the current level state of the state
 * controller. Level states are changed by calls to
 * phStateChangeLevel(3). They can not be set directly. Level states
 * changes are the result of incoming driver calls from the
 * test cell client.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetLevel(
    phStateId_t stateID               /* driver state ID */,
    phStateLevel_t *state             /* the current driver level state */
);

/*****************************************************************************
 *
 * Change level state
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * On each incoming driver call from the test cell client, this
 * function must be called, passing the code for the current driver
 * call. The state controller performs internal actions to reach the
 * new state. Most often these actions are void but in some cases,
 * supbsequent states of the controller are changed. The state
 * controller internally checks flags from SmarTest, e.g. to check the
 * SKIP flag after pause actions have been executed.
 *
 * If a state transition is illegal, the state is tried to be changed
 * anyway (assuming errors in the appplication model). Also an error
 * code is returned.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateChangeLevel(
    phStateId_t stateID               /* driver state ID */,
    phFrameAction_t action            /* driver action to cause state
					 changes */
);

/*****************************************************************************
 *
 * Check level state change
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * On each incoming driver call from the test cell client, this
 * function should be called, before any driver actions are
 * performed. This function does not change any internal states like
 * phStateChangeLevel(3), but only returns an error code, if the
 * proposed state transition is not valid in the current
 * situation. The function may be used by hook functions or within
 * user intefaces to prevent the driver to reach illegal conditions.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateCheckLevelChange(
    phStateId_t stateID               /* driver state ID */,
    phFrameAction_t action            /* driver action to be tested
					 for current driver level state */
);

/*****************************************************************************
 *
 * Get driver mode
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The current driver mode is returned. Driver modes distinguish
 * between mormal operation, single step operation or hand test mode
 * operations. The state controller only keeps track of these
 * states. It does not directly influence the real communication. It
 * is the task of the driver framework and the driver plugin (and of
 * the hook functions) to call this function to find out about the
 * current driver mode.
 *
 * The driver mode is set implicetly by calls to phStateChangeLevel(3).
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetDrvMode(
    phStateId_t stateID               /* driver state ID */,
    phStateDrvMode_t *state           /* current driver mode */
);

/*****************************************************************************
 *
 * Set driver mode
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The current driver mode can be set (see also
 * phStateGetDrvMode(3)). Usually this function is not called
 * directly, since the driver modes are set implicetly by calls to
 * phStateChangeLevel(3). This function may be used to bring the
 * driver into a special state for debug purposes or at startup. Also
 * user interfaces may use this function to change the mode.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateSetDrvMode(
    phStateId_t stateID               /* driver state ID */,
    phStateDrvMode_t state            /* new forced driver mode */
);

/*****************************************************************************
 *
 * Get skip mode
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The current skip mode is returned. Skip modes distinguish between
 * normal operation, skipping the next part, skipping the current
 * part, or combinations of this. During skipping (operator pressed
 * SKIP DEVICE within a pause dialog of the test cell client) devices
 * are not tested. Therefore the driver must bin these untested
 * devices into retest bins of the handler. The skip mode is changed
 * inplicetly at end of pause actions. The SKIP flag of SmarTest is
 * read to achieve this.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetSkipMode(
    phStateId_t stateID               /* driver state ID */,
    phStateSkipMode_t *state          /* current device skipping mode */
);

/*****************************************************************************
 *
 * Set skip mode
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The current skip mode can be set (see also
 * phStateGetSkipMode(3)). Usually this function is not called
 * directly, since the skip modes are set implicetly by calls to
 * phStateChangeLevel(3). This function may be used to bring the
 * driver into a special state for debug purposes or at startup. Also
 * user interfaces may use this function to change the mode.
 *
 * Warning: Handle with care, since reseting the skip mode may cause
 * device binning into wrong bins!
 *
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateSetSkipMode(
    phStateId_t stateID               /* driver state ID */,
    phStateSkipMode_t state           /* new forced device skipping mode */
);

/*****************************************************************************
 *
 * Get tester mode
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The current tester mode is returned. Tester modes distinguish
 * between normal operation, retesting parts or checking parts.
 * During retesting or checking (operator pressed RETEST DEVICE or
 * CHECK DEVICE within a pause dialog of the test cell client)
 * devices should not be exchanged during device done and device start
 * actions.  The tester mode is changed inplicetly at end of pause
 * actions. The RETEST and CHECK flags of SmarTest are read to achieve
 * this.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetTesterMode(
    phStateId_t stateID               /* driver state ID */,
    phStateTesterMode_t *state        /* current test mode */
);

/*****************************************************************************
 *
 * Set tester mode
 *
 * Authors: Michael Vogt
 *
 * Description:
 * 
 * The current tester mode can be set (see also
 * phStateGetTesterMode(3)). Usually this function is not called
 * directly, since the tester modes are set implicetly by calls to
 * phStateChangeLevel(3). This function may be used to bring the
 * driver into a special state for debug purposes or at startup. Also
 * user interfaces may use this function to change the mode.
 *
 * Warning: Handle with care, since changing the tester mode may cause
 * device changes even if not wanted, or vica versa.
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateSetTesterMode(
    phStateId_t stateID               /* driver state ID */,
    phStateTesterMode_t state         /* new forced test mode */
);

//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_STATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
