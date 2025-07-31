/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_state.c
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
 * 1) Copy this template to as many .c files as you require
 *
 * 2) Use the command 'make depend' to make visible the new
 *    source files to the makefile utility
 *
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_tcom.h"
#include "ph_state.h"

#include "ph_state_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/* don't trust anyone .... */
#define PH_HANDLE_CHECK

#ifdef DEBUG
#define PH_HANDLE_CHECK
#endif

#ifdef PH_HANDLE_CHECK
#define CheckInitialized() \
    if (!myLogger) \
        return PHSTATE_ERR_NOT_INIT

#define CheckHandle(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHSTATE_ERR_INVALID_HDL
#else
#define CheckInitialized()
#define CheckHandle(x)
#endif

/* for efficiency, state machine entries are accessed using macros */
#define DefTrans(a,b,c,d) \
    sm[a][b].newState = (c), sm[a][b].action = (d)

#define GetTransState(a,b) sm[a][b].newState

#define GetTransAction(a,b) sm[a][b].action

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

/* Needed because the logger is only passed once during init to this
module but not during all.  Is not stored in the state handle
structure, since it is then impossible to log invalid handle access to
this module */

static phLogId_t myLogger = NULL;
static phTcomId_t myTester = NULL;

/* module global state machine */

static struct levelStateTransition sm[PHSTATE_LEV__END][PHFRAME_ACT__END];


/*--- functions -------------------------------------------------------------*/

static char *getStateString(struct phStateStruct *id)
{
    static char stateString[64];

    switch (id->levelState)
    {
      case PHSTATE_LEV_START:
	sprintf(stateString, "driver not started");
	break;
      case PHSTATE_LEV_DRV_STARTED:
	sprintf(stateString, "in driver start level");
	break;
      case PHSTATE_LEV_DRV_DONE:
	sprintf(stateString, "driver level done");
	break;
      case PHSTATE_LEV_LOT_STARTED:
	sprintf(stateString, "in lot start level");
	break;
      case PHSTATE_LEV_LOT_DONE:
	sprintf(stateString, "lot level done");
	break;
      case PHSTATE_LEV_DEV_STARTED:
	sprintf(stateString, "in device start level");
	break;
      case PHSTATE_LEV_DEV_DONE:
	sprintf(stateString, "device level done");
	break;
      case PHSTATE_LEV_PAUSE_STARTED:
	sprintf(stateString, "in pause level");
	break;
      case PHSTATE_LEV_PAUSE_DONE:
	sprintf(stateString, "pause level done");
	break;
      case PHSTATE_LEV_DONTCARE:
      default:
	sprintf(stateString, "in unknown state");
	break;
    }

    return &stateString[0];
}


static phStateError_t reachDrvStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachDrvStarted(P%p, 0x%08lx)", id, (long) task);

    switch(task)
    {
      case PHSTATE_AT_CHECK:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_START:
	  case PHSTATE_LEV_DRV_DONE:
	    break;
	  default:
	    /* coming from unexpected state */
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
	break;
      case PHSTATE_AT_STEP:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_START:
	  case PHSTATE_LEV_DRV_DONE:
	    break;
	  default:
	    /* coming from unexpected state */
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching driver start level while %s",
		getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
	break;
    }

    return retVal;
}

static phStateError_t reachLotStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachLotStarted(P%p, 0x%08lx)", id, (long) task);

    switch(task)
    {
      case PHSTATE_AT_CHECK:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DRV_STARTED:
	  case PHSTATE_LEV_LOT_DONE:
	    break;
	  default:
	    /* coming from unexpected state */
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
	break;
      case PHSTATE_AT_STEP:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DRV_STARTED:
	  case PHSTATE_LEV_LOT_DONE:
	    break;
	  default:
	    /* coming from unexpected state */
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching lot start level while %s",
		getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}

	/* perform lot start state changes */
	id->lotLevelUsed = 1;

	break;
    }

    return retVal;
}

static phStateError_t reachDevStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachDevStarted(P%p, 0x%08lx)", id, (long) task);

    switch(task)
    {
      case PHSTATE_AT_CHECK:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DRV_STARTED:
	  case PHSTATE_LEV_LOT_STARTED:
	  case PHSTATE_LEV_DEV_DONE:
	    break;
	  default:
	    /* coming from unexpected state */
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
	break;
      case PHSTATE_AT_STEP:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DRV_STARTED:
	  case PHSTATE_LEV_LOT_STARTED:
	  case PHSTATE_LEV_DEV_DONE:
	    break;
	  default:
	    /* coming from unexpected state */
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching device start level while %s",
		getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}

	/* change skip mode from next to current */
	if (id->skipModeState == PHSTATE_SKIP_NEXT)
	    id->skipModeState = PHSTATE_SKIP_CURRENT;

	/* reset the retest and check device states to normal mode */
	id->testModeState = PHSTATE_TST_NORMAL;

	break;
    }

    return retVal;
}

static phStateError_t reachPauStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachPauStarted(P%p, 0x%08lx)", id, (long) task);

    switch(task)
    {
      case PHSTATE_AT_CHECK:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DEV_STARTED:
	    break;
	  default:
	    /* coming from unexpected state */
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
	break;
      case PHSTATE_AT_STEP:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DEV_STARTED:
	    break;
	  default:
	    /* coming from unexpected state */
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching pause level while %s",
		getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
	break;
    }

    return retVal;
}

static phStateError_t reachPauDone(struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;
    long flagCheck;
    long flagRetest;
    long flagSkip;
    phTcomError_t tError;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachPauDone(P%p, 0x%08lx)", id, (long) task);

    tError = phTcomGetSystemFlag(myTester, PHTCOM_SF_CI_RETEST, &flagRetest);
    if (tError != PHTCOM_ERR_OK)
	flagRetest = 0;
    tError = phTcomGetSystemFlag(myTester, PHTCOM_SF_CI_CHECK_DEV, &flagCheck);
    if (tError != PHTCOM_ERR_OK)
	flagCheck = 0;
    tError = phTcomGetSystemFlag(myTester, PHTCOM_SF_CI_SKIP, &flagSkip);
    if (tError != PHTCOM_ERR_OK)
	flagSkip = 0;
    
    switch(task)
    {
      case PHSTATE_AT_CHECK:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_PAUSE_STARTED:
	    break;
	  default:
	    /* coming from unexpected state */
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
	break;
      case PHSTATE_AT_STEP:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_PAUSE_STARTED:
	    break;
	  default:
	    /* coming from unexpected state */
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of pause level while %s",
		getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}

	/* check the 'check device' and the 'retest device' flags and
	   set the retest state accordingly. 'Check device' has
	   precedence above 'retest device'. But both flags don't
	   occur at the same time anyway */

	if (flagRetest)
	    id->testModeState = PHSTATE_TST_RETEST;
	else if (flagCheck)
	    id->testModeState = PHSTATE_TST_CHECK;
	
	/* check the 'retest device' and 'skip device' flags and set
	   the skip state accordingly. 'Skip device' has precedence
	   above 'retest device'. But both flags don't occur at the
	   same time anyway */

	if (flagSkip)
	{
	    if (id->skipModeState == PHSTATE_SKIP_NORMAL)
		id->skipModeState = PHSTATE_SKIP_NEXT;
	    else if (id->skipModeState == PHSTATE_SKIP_CURRENT)
		id->skipModeState = PHSTATE_SKIP_NEXT_CURRENT;
	}
	else if (flagRetest)
	{
	    if (id->skipModeState == PHSTATE_SKIP_CURRENT)
		id->skipModeState = PHSTATE_SKIP_NORMAL;
	}

	/* reset the pause flag, if not skipping, checking, or
           retesting devices */

	if (!flagSkip && !flagCheck && !flagRetest)
	    phTcomSetSystemFlag(myTester, PHTCOM_SF_CI_PAUSE, 0L);
	
	break;
    }

    return retVal;
}

static phStateError_t reachDevDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachDevDone(P%p, 0x%08lx)", id, (long) task);

    switch(task)
    {
      case PHSTATE_AT_CHECK:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DEV_STARTED:
	  case PHSTATE_LEV_PAUSE_DONE:
	    break;
	  default:
	    /* coming from unexpected state */
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
	break;
      case PHSTATE_AT_STEP:
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DEV_STARTED:
	  case PHSTATE_LEV_PAUSE_DONE:
	    break;
	  default:
	    /* coming from unexpected state */
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of device level while %s",
		getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}

	/* if we came out of the pause done actions, the skip next
           state may be set. Now we set it to the skip current state */
	if (id->skipModeState == PHSTATE_SKIP_CURRENT)
	    id->skipModeState = PHSTATE_SKIP_NORMAL;
	if (id->skipModeState == PHSTATE_SKIP_NEXT_CURRENT)
	    id->skipModeState = PHSTATE_SKIP_NEXT;

	break;
    }

    return retVal;
}

static phStateError_t reachLotDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;
    long flagAbort;
    phTcomError_t tError;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachLotDone(P%p, 0x%08lx)", id, (long) task);

    /* get current system flags */
    tError = phTcomGetSystemFlag(myTester, PHTCOM_SF_CI_ABORT, &flagAbort);
    if (tError != PHTCOM_ERR_OK)
	flagAbort = 0;

    /* look at previous state */
    switch(id->levelState)
    {
      case PHSTATE_LEV_DEV_DONE:
      case PHSTATE_LEV_LOT_STARTED:
	/* this is the correct path... */
	break;		
      case PHSTATE_LEV_DEV_STARTED:
	/* directly coming from device start, this is only valid,
	   if the ABORT flag is set, otherwise it looks like a
	   missuse in the test cell client */
	if (flagAbort)
	    break;
      default:
	/* coming from unexpected state */
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    /* now check whether to print a warning */
    if (retVal != PHSTATE_ERR_OK)
    {
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of lot level while %s",
		getStateString(id));
    }
    else if (! id->lotLevelUsed)
    {
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of lot level but lot level was not started",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
    }
	
    return retVal;
}

static phStateError_t reachDrvDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;
    long flagAbort;
    phTcomError_t tError;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachDrvDone(P%p, 0x%08lx)", id, (long) task);

    /* get current system flags */
    tError = phTcomGetSystemFlag(myTester, PHTCOM_SF_CI_ABORT, &flagAbort);
    if (tError != PHTCOM_ERR_OK)
	flagAbort = 0;

    if (id->lotLevelUsed)
	switch(id->levelState)
	{
	  case PHSTATE_LEV_LOT_DONE:
	    /* this is the correct path */
	    break;
	  case PHSTATE_LEV_LOT_STARTED:
	    if (flagAbort)
		/* ok during ABORT, otherwise fall through to default */
		break;
	  default:
	    /* coming from unexpected state */
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}
    else /* lot level not used */
	switch(id->levelState)
	{
	  case PHSTATE_LEV_DEV_DONE:
	  case PHSTATE_LEV_DRV_STARTED:
	  case PHSTATE_LEV_LOT_DONE:
	    /* this is the correct path */
	    /* Coming from lot done here is only partly correct, since
               the lot in used flag is not set. But this problem was
               reported before while reaching the lot done level */
	    break;			    
	  case PHSTATE_LEV_DEV_STARTED:
	    if (flagAbort)
		/* ok during ABORT, otherwise fall through to default */
		break;
	  default:
	    /* coming from unexpected state */
	    retVal = PHSTATE_ERR_ILLTRANS;
	    break;
	}

    /* now check whether to print a warning */
    if (retVal != PHSTATE_ERR_OK && task == PHSTATE_AT_STEP)
	phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
	    "Possible error in test cell client:\n"
	    "reaching end of driver level while %s",
	    getStateString(id));

    if (task == PHSTATE_AT_STEP)
	/* reset the used lot level flag for next driver run */
	id->lotLevelUsed = 0;

    return retVal;
}

static phStateError_t serveDrvMode(
    struct phStateStruct *id, enum actionTask_t task)
{
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"serveDrvMode(P%p, 0x%08lx)", id, (long) task);

    switch(task)
    {
      case PHSTATE_AT_CHECK:
	switch (id->currentAction)
	{
	  case PHFRAME_ACT_HANDTEST_START:
	    return (
		id->drvModeState == PHSTATE_DRV_NORMAL ||
		id->drvModeState == PHSTATE_DRV_SST) ?
		PHSTATE_ERR_OK : PHSTATE_ERR_IGNORED;
	  case PHFRAME_ACT_HANDTEST_STOP:
	    return (
		id->drvModeState == PHSTATE_DRV_HAND ||
		id->drvModeState == PHSTATE_DRV_SST_HAND) ?
		PHSTATE_ERR_OK : PHSTATE_ERR_IGNORED;
	  case PHFRAME_ACT_STEPMODE_START:
	    return (
		id->drvModeState == PHSTATE_DRV_NORMAL ||
		id->drvModeState == PHSTATE_DRV_HAND) ?
		PHSTATE_ERR_OK : PHSTATE_ERR_IGNORED;
	  case PHFRAME_ACT_STEPMODE_STOP:
	    return (
		id->drvModeState == PHSTATE_DRV_SST ||
		id->drvModeState == PHSTATE_DRV_SST_HAND) ?
		PHSTATE_ERR_OK : PHSTATE_ERR_IGNORED;
	  default:
	    break;
	}
	break;
      case PHSTATE_AT_STEP:
	switch(id->currentAction)
	{
	  case PHFRAME_ACT_HANDTEST_START:
	    switch (id->drvModeState)
	    {
	      case PHSTATE_DRV_NORMAL:
		id->drvModeState = PHSTATE_DRV_HAND;
		return PHSTATE_ERR_OK;
	      case PHSTATE_DRV_SST:
		id->drvModeState = PHSTATE_DRV_SST_HAND;
		return PHSTATE_ERR_OK;
	      case PHSTATE_DRV_HAND:
	      case PHSTATE_DRV_SST_HAND:
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "already in hand test mode, ignored");
		return  PHSTATE_ERR_IGNORED;
	    }
	    break;
	  case PHFRAME_ACT_HANDTEST_STOP:
	    switch (id->drvModeState)
	    {
	      case PHSTATE_DRV_HAND:
		id->drvModeState = PHSTATE_DRV_NORMAL;
		return PHSTATE_ERR_OK;
	      case PHSTATE_DRV_SST_HAND:
		id->drvModeState = PHSTATE_DRV_SST;
		return PHSTATE_ERR_OK;
	      case PHSTATE_DRV_NORMAL:
	      case PHSTATE_DRV_SST:
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "not in hand test mode, ignored");
		return  PHSTATE_ERR_IGNORED;
	    }
	    break;
	  case PHFRAME_ACT_STEPMODE_START:
	    switch (id->drvModeState)
	    {
	      case PHSTATE_DRV_NORMAL:
		id->drvModeState = PHSTATE_DRV_SST;
		return PHSTATE_ERR_OK;
	      case PHSTATE_DRV_HAND:
		id->drvModeState = PHSTATE_DRV_SST_HAND;
		return PHSTATE_ERR_OK;
	      case PHSTATE_DRV_SST:
	      case PHSTATE_DRV_SST_HAND:
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "already in single step mode, ignored");
		return  PHSTATE_ERR_IGNORED;
	    }
	    break;
	  case PHFRAME_ACT_STEPMODE_STOP:
	    switch (id->drvModeState)
	    {
	      case PHSTATE_DRV_SST:
		id->drvModeState = PHSTATE_DRV_NORMAL;
		return PHSTATE_ERR_OK;
	      case PHSTATE_DRV_SST_HAND:
		id->drvModeState = PHSTATE_DRV_HAND;
		return PHSTATE_ERR_OK;
	      case PHSTATE_DRV_NORMAL:
	      case PHSTATE_DRV_HAND:
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "not in single step mode, ignored");
		return  PHSTATE_ERR_IGNORED;
	    }
	    break;
	  default: 
	    break;
	}
	break;
    }

    return PHSTATE_ERR_OK;
}


static phStateError_t  performChangeLevel(
    struct phStateStruct *stateID, phStateLevel_t newState, 
    actionPtr_t stateAction, enum actionTask_t type)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"performChangeLevel(P%p, 0x%08lx, P%p, 0x%08lx)", 
	stateID, newState, stateAction, type);

/* disable the check for immediate repeated same function call.
 * changed for multiple equipments control driver since in a multiple
 * equipment environment, the same function may be called repeatedly 
 */
#if 0
    if (newState != PHSTATE_LEV_DONTCARE && 
	stateID->levelState == newState)
    {
	if (type != PHSTATE_AT_CHECK)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"already %s, ignored",
		getStateString(stateID));
	return PHSTATE_ERR_IGNORED;
    }
#endif

    if (stateAction)
	retVal = (*stateAction)(stateID, type);

    if (type != PHSTATE_AT_CHECK && newState != PHSTATE_LEV_DONTCARE)
    {
	stateID->levelState = newState;
	phLogStateMessage(myLogger, PHLOG_TYPE_MESSAGE_2,
	    "changed state to '%s'", getStateString(stateID));
    }

    return retVal;
}

/*****************************************************************************
 *
 * Driver state control initialization
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateInit(
    phStateId_t *stateID              /* the resulting driver state ID */,
    phTcomId_t tester                 /* access to the tester */,
    phLogId_t logger                  /* logger to be used */
)
{
    struct phStateStruct *tmpId = NULL;
    phFrameAction_t frameActIndex;
    phStateLevel_t stateLevIndex;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int i;*/
/* End of Huatek Modification */

    /* use the logger */
    if (logger && tester)
    {
	/* if we got a valid logger, use it and trace the first call
           to this module */
	myLogger = logger;
	myTester = tester;
	phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	    "phStateInit(P%p, P%p)", stateID, myLogger);
    }
    else
	/* passed logger is not valid at all, give up */
	return PHSTATE_ERR_NOT_INIT;

    /* allocate state data type */
    tmpId = PhNew(struct phStateStruct);
    if (tmpId == 0)
    {
	phLogStateMessage(myLogger, PHLOG_TYPE_FATAL,
	    "not enough memory during call to phStateInit()");
	return PHSTATE_ERR_MEMORY;
    }

    /* initialize the new handle */
    tmpId->myself = tmpId;
    tmpId->currentAction = PHFRAME_ACT__END;
    tmpId->levelState = PHSTATE_LEV_START;
    tmpId->drvModeState = PHSTATE_DRV_NORMAL;
    tmpId->testModeState = PHSTATE_TST_NORMAL;
    tmpId->skipModeState = PHSTATE_SKIP_NORMAL;
    tmpId->lotLevelUsed = 0;

    /* initialize the level default state transition machine */
    for (frameActIndex = PHFRAME_ACT_DRV_START; 
	 frameActIndex < PHFRAME_ACT__END; frameActIndex = (phFrameAction_t)(frameActIndex + 1))
	for (stateLevIndex = PHSTATE_LEV_START; 
	     stateLevIndex < PHSTATE_LEV__END; stateLevIndex = (phStateLevel_t)(stateLevIndex + 1))
	    DefTrans(stateLevIndex, frameActIndex, PHSTATE_LEV_DONTCARE, NULL);
	
    /* these are the regular state transitions during normal operation */
    DefTrans(PHSTATE_LEV_START,         PHFRAME_ACT_DRV_START,   PHSTATE_LEV_DRV_STARTED,   reachDrvStarted);

    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_LOT_START,   PHSTATE_LEV_LOT_STARTED,   reachLotStarted);
    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_DEV_START,   PHSTATE_LEV_DEV_STARTED,   reachDevStarted);
    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_DRV_START,   PHSTATE_LEV_DRV_STARTED,   NULL           );

    DefTrans(PHSTATE_LEV_DRV_DONE,      PHFRAME_ACT_DRV_START,   PHSTATE_LEV_DRV_STARTED,   reachDrvStarted);
    DefTrans(PHSTATE_LEV_DRV_DONE,      PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      NULL           );

    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_DEV_START,   PHSTATE_LEV_DEV_STARTED,   reachDevStarted);
    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_LOT_START,   PHSTATE_LEV_LOT_STARTED,   NULL           );

    DefTrans(PHSTATE_LEV_LOT_DONE,      PHFRAME_ACT_LOT_START,   PHSTATE_LEV_LOT_STARTED,   reachLotStarted);
    DefTrans(PHSTATE_LEV_LOT_DONE,      PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_LOT_DONE,      PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      NULL           );

    DefTrans(PHSTATE_LEV_DEV_STARTED,   PHFRAME_ACT_PAUSE_START, PHSTATE_LEV_PAUSE_STARTED, reachPauStarted);
    DefTrans(PHSTATE_LEV_DEV_STARTED,   PHFRAME_ACT_DEV_DONE,    PHSTATE_LEV_DEV_DONE,      reachDevDone   );
    DefTrans(PHSTATE_LEV_DEV_STARTED,   PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_DEV_STARTED,   PHFRAME_ACT_DEV_START,   PHSTATE_LEV_DEV_STARTED,   NULL           );

    DefTrans(PHSTATE_LEV_DEV_DONE,      PHFRAME_ACT_DEV_START,   PHSTATE_LEV_DEV_STARTED,   reachDevStarted);
    DefTrans(PHSTATE_LEV_DEV_DONE,      PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_DEV_DONE,      PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_DEV_DONE,      PHFRAME_ACT_DEV_DONE,    PHSTATE_LEV_DEV_DONE,      NULL           );

    DefTrans(PHSTATE_LEV_PAUSE_STARTED, PHFRAME_ACT_PAUSE_DONE,  PHSTATE_LEV_PAUSE_DONE,    reachPauDone   );
    DefTrans(PHSTATE_LEV_PAUSE_STARTED, PHFRAME_ACT_PAUSE_START, PHSTATE_LEV_PAUSE_STARTED, NULL           );

    DefTrans(PHSTATE_LEV_PAUSE_DONE,    PHFRAME_ACT_DEV_DONE,    PHSTATE_LEV_DEV_DONE,      reachDevDone   );
    DefTrans(PHSTATE_LEV_PAUSE_DONE,    PHFRAME_ACT_PAUSE_DONE,  PHSTATE_LEV_PAUSE_DONE,    NULL           );

    /* these are exceptional state transitions, only occuring, if the
       test cell client is not set up correctly or if something else
       is screwed up */
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_DRV_START,   PHSTATE_LEV_DRV_STARTED,   reachDrvStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_LOT_START,   PHSTATE_LEV_LOT_STARTED,   reachLotStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_DEV_START,   PHSTATE_LEV_DEV_STARTED,   reachDevStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_PAUSE_START, PHSTATE_LEV_PAUSE_STARTED, reachPauStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_PAUSE_DONE,  PHSTATE_LEV_PAUSE_DONE,    reachPauDone   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_DEV_DONE,    PHSTATE_LEV_DEV_DONE,      reachDevDone   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );

    /* these are regular transitions for requests, that don't change
       the level state but may cause any other state changes */
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_HANDTEST_START, PHSTATE_LEV_DONTCARE,   serveDrvMode   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_HANDTEST_STOP,  PHSTATE_LEV_DONTCARE,   serveDrvMode   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_STEPMODE_START, PHSTATE_LEV_DONTCARE,   serveDrvMode   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_STEPMODE_STOP,  PHSTATE_LEV_DONTCARE,   serveDrvMode   );

    /* return state controller ID */
    *stateID = tmpId;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Destroy a Driver state controller
 *
 * Authors: Michael Vogt
 *
 * History: 09 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateDestroy(
    phStateId_t stateID               /* the state ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateDestroy(P%p)", stateID);
    CheckHandle(stateID);
    
    /* destroy the handle only, no memory needs to be freed */
    
    stateID->myself = NULL;

    return PHSTATE_ERR_OK;
}

/*Begin of Huatek Modifications, Donnie Tu, 04/24/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free a Driver state controller
 *
 * Authors: Donnie Tu
 *
 * History: 24 Apr 2002,Donnie Tu, created
 *
 * Description:
 * please refer to ph_state.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateFree(
    phStateId_t *stateID               /* the state ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
        "phStateDestroy(P%p)", *stateID);
    CheckHandle(*stateID);
 
    free(*stateID);
    *stateID = NULL;
 
    return PHSTATE_ERR_OK;
}
/*End of Huatek Modifications*/

/*****************************************************************************
 *
 * Get current level state
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetLevel(
    phStateId_t stateID               /* driver state ID */,
    phStateLevel_t *state             /* the current driver level state */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateGetLevel(P%p, P%p)", stateID, state);
    CheckHandle(stateID);
    
    /* return the requested value */
    *state = stateID -> levelState;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Change level state
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateChangeLevel(
    phStateId_t stateID               /* driver state ID */,
    phFrameAction_t action            /* driver action to cause state
					 changes */
)
{
    phStateLevel_t newState;
    actionPtr_t stateAction;
    
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateChangeLevel(P%p, 0x%08lx)",
	stateID, (long) action);
    CheckHandle(stateID);
    
    /* we are running the following algorithm: If for the passed
       action and the current level state a new level state or an action
       function is defined, these are applied. Otherwise, if neither of them
       is defined (PHSTATE_LEV_DONTCARE, NULL), the defined actions in the
       level state dontcare column of the state machine are applied (if
       defined at all). If non of the above is defined, it is assumed that
       this is a valid transition anyway, which doesn't cause any internal
       state changes */

    stateID->currentAction = action;

    /* transition depends on current state ? */
    newState = GetTransState(stateID->levelState, action);
    stateAction = GetTransAction(stateID->levelState, action);
    
    if (newState != PHSTATE_LEV_DONTCARE || stateAction != NULL)
	return performChangeLevel(stateID, newState, stateAction, 
	    PHSTATE_AT_STEP);

    /* transition does not depend on current state ? */
    newState = GetTransState(PHSTATE_LEV_DONTCARE, action);
    stateAction = GetTransAction(PHSTATE_LEV_DONTCARE, action);
    
    if (newState != PHSTATE_LEV_DONTCARE || stateAction != NULL)
	return performChangeLevel(stateID, newState, stateAction, 
	    PHSTATE_AT_STEP);

    /* transition was not found, does not change state */
    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Check level state change
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateCheckLevelChange(
    phStateId_t stateID               /* driver state ID */,
    phFrameAction_t action            /* driver action to be tested
					 for current driver level state */
)
{
    phStateLevel_t newState;
    actionPtr_t stateAction;
    
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateCheckLevelChange(P%p, 0x%08lx)",
	stateID, (long) action);
    CheckHandle(stateID);
    
    /* The same state changes as in phStateChangeLevel() are
       requested, however, no action takes place, only the transition
       is checked for validity */

    stateID->currentAction = action;

    /* transition depends on current state ? */
    newState = GetTransState(stateID->levelState, action);
    stateAction = GetTransAction(stateID->levelState, action);
    
    if (newState != PHSTATE_LEV_DONTCARE || stateAction != NULL)
	return performChangeLevel(stateID, newState, stateAction, 
	    PHSTATE_AT_CHECK);

    /* transition does not depend on current state ? */
    newState = GetTransState(PHSTATE_LEV_DONTCARE, action);
    stateAction = GetTransAction(PHSTATE_LEV_DONTCARE, action);
    
    if (newState != PHSTATE_LEV_DONTCARE || stateAction != NULL)
	return performChangeLevel(stateID, newState, stateAction, 
	    PHSTATE_AT_CHECK);

    /* transition was not found, does not change state */
    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Get driver mode
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetDrvMode(
    phStateId_t stateID               /* driver state ID */,
    phStateDrvMode_t *state           /* current driver mode */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateGetDrvMode(P%p, P%p)", stateID, state);
    CheckHandle(stateID);
    
    /* return the requested value */
    *state = stateID -> drvModeState;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Set driver mode
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateSetDrvMode(
    phStateId_t stateID               /* driver state ID */,
    phStateDrvMode_t state            /* new forced driver mode */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateSetDrvMode(P%p, 0x%08lx)", stateID, state);
    CheckHandle(stateID);
    
    /* return the requested value */
    stateID -> drvModeState = state;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Get skip mode
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetSkipMode(
    phStateId_t stateID               /* driver state ID */,
    phStateSkipMode_t *state          /* current device skipping mode */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateGetSkipMode(P%p, P%p)", stateID, state);
    CheckHandle(stateID);
    
    /* return the requested value */
    *state = stateID -> skipModeState;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Set skip mode
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateSetSkipMode(
    phStateId_t stateID               /* driver state ID */,
    phStateSkipMode_t state           /* new forced device skipping mode */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateSetSkipMode(P%p, 0x%08lx)", stateID, state);
    CheckHandle(stateID);
    
    /* return the requested value */
    stateID -> skipModeState = state;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Get tester mode
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetTesterMode(
    phStateId_t stateID               /* driver state ID */,
    phStateTesterMode_t *state        /* current test mode */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateGetTesterMode(P%p, P%p)", stateID, state);
    CheckHandle(stateID);
    
    /* return the requested value */
    *state = stateID -> testModeState;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Set tester mode
 *
 * Authors: Michael Vogt
 *
 * History: 07 Jul 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateSetTesterMode(
    phStateId_t stateID               /* driver state ID */,
    phStateTesterMode_t state         /* new forced test mode */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateSetTesterMode(P%p, 0x%08lx)", stateID, state);
    CheckHandle(stateID);
    
    /* return the requested value */
    stateID -> testModeState = state;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
