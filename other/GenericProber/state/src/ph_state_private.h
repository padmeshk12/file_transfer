/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_state_private.h
 * CREATED  : 07 Jul 1999
 *
 * CONTENTS : equipment handler state controller
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 07 Jul 1999, Michael Vogt, created
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

#ifndef _PH_STATE_PRIVATE_H_
#define _PH_STATE_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#ifdef __C2MAN__
#include "ph_log.h"
#include "ph_state.h"
#include "ph_frame.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL PHLOG_TYPE_MESSAGE_2
#define LOG_DEBUG PHLOG_TYPE_MESSAGE_3
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_4

/*--- typedefs --------------------------------------------------------------*/

enum actionTask_t { 
    PHSTATE_AT_CHECK                    /* check only, whether a
					   transition is valid at the 
					   current time */,
    PHSTATE_AT_STEP                     /* do the transition
					   step. Report any errors, but try 
					   to reach the requested level 
					   anyway */
};

struct phStateStruct
{
    struct phStateStruct *myself;       /* self reference for validity
					   check */
    phFrameAction_t currentAction;      /* the current ph_ frame action */
    phStateLevel_t levelState;          /* the current driver level */
    phStateDrvMode_t drvModeState;      /* the current driver mode */
    phStateSkipMode_t skipModeState;    /* the current skip mode */
    phStateTesterMode_t testModeState;  /* the current retest mode */
    int lotLevelUsed;                   /* flag to indicate, that the
					   lot level is used */
    int cassLevelUsed;                  /* flag to indicate, that the
					   lot level is used */
    int subdieLevelUsed;                /* flag to indicate, that the
					   lot level is used */
    int expectLevelEnd;                 /* set internally, if a LEVEL
                                           END codition was seen */
    int expectAbort;                    /* set internally, if a LEVEL
                                           END codition was seen */
    /* phEstateWafType_t currentWafer; */    /* current wafer type assumed on
                                           the chuck */
    /* phEstateWafType_t nextWafer; */       /* next wafer type assumed on
                                           the chuck */
    int retestRequest;                  /* set, if a retest request is
                                           recognized by pause done
                                           and reset on next wafer
                                           start action */
    int referenceRequest;               /* set, if a reference request is
                                           recognized by pause done
                                           and reset on next wafer
                                           start action */
};

typedef phStateError_t (*actionPtr_t)(
    struct phStateStruct *id, enum actionTask_t task);

struct levelStateTransition
{
    phStateLevel_t newState;
    actionPtr_t action;
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
 * reach driver started level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachDrvStarted(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach lot started level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachLotStarted(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach cassette started level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachCassStarted(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach wafer started level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachWafStarted(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach die started level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachDieStarted(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach sub-die started level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachSubDieStarted(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach pause started level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachPauStarted(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach pause done level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachPauDone(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach sub-die done level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachSubDieDone(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach die done level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachDieDone(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach wafer done level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachWafDone(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach cassette done level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachCassDone(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach lot done level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachLotDone(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * reach driver done level
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to reach
 * the given level state. These steps basically change subsequent
 * state controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t reachDrvDone(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * change driver mode
 *
 * Authors: Michael Vogt
 *
 * Description: This function performs all necessary steps to switch
 * between hand test and normal mode and between single step and
 * normal mode. These steps basically change subsequent state
 * controls, check the consistency and produce an error code, if
 * applicable.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t serveDrvMode(struct phStateStruct *id, 
    enum actionTask_t task);

/*****************************************************************************
 *
 * change the driver level
 *
 * Authors: Michael Vogt
 *
 * Description: This function calls one of the above reach level state
 * functions and actually performs the change in level for the main
 * state controller. Also ignored level changes are handled here.
 *
 * Returns: error code to be passed back to module caller
 *
 *****************************************************************************/
static phStateError_t  performChangeLevel(
    struct phStateStruct *stateID, phStateLevel_t newState, 
    actionPtr_t stateAction, enum actionTask_t type);

#endif /* ! _PH_STATE_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
