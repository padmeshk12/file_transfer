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
 *            03 Dec 1999, Michael Vogt, rework according to prober 
 *                         driver needs
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

#define GetSysFlag(a,b,c) \
    c = phTcomGetSystemFlag(myTester, b, &a); \
    if (c != PHTCOM_ERR_OK) a = 0

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
      case PHSTATE_LEV_CASS_STARTED:
	sprintf(stateString, "in cassette start level");
	break;
      case PHSTATE_LEV_CASS_DONE:
	sprintf(stateString, "cassette level done");
	break;
      case PHSTATE_LEV_WAF_STARTED:
	sprintf(stateString, "in wafer start level");
	break;
      case PHSTATE_LEV_WAF_DONE:
	sprintf(stateString, "wafer level done");
	break;
      case PHSTATE_LEV_DIE_STARTED:
	sprintf(stateString, "in die start level");
	break;
      case PHSTATE_LEV_DIE_DONE:
	sprintf(stateString, "die level done");
	break;
      case PHSTATE_LEV_SUBDIE_STARTED:
	sprintf(stateString, "in sub-die start level");
	break;
      case PHSTATE_LEV_SUBDIE_DONE:
	sprintf(stateString, "sub-die level done");
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

/*---------------------------------------------------------------------------*/

static phStateError_t reachDrvStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachDrvStarted(P%p, 0x%08lx)", id, (long) task);

    switch(id->levelState)
    {
      case PHSTATE_LEV_START:
      case PHSTATE_LEV_DRV_DONE:
	break;
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching driver start level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform driver start state changes */

 	/* reset the used lot level flag for new driver run */
	id->lotLevelUsed = 0;
	id->cassLevelUsed = 0;
	id->subdieLevelUsed = 0;
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachLotStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachLotStarted(P%p, 0x%08lx)", id, (long) task);

    switch(id->levelState)
    {
      case PHSTATE_LEV_DRV_STARTED:
      case PHSTATE_LEV_LOT_DONE:
	break;
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching lot start level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform lot start state changes */
	id->lotLevelUsed = 1;
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachCassStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachCassStarted(P%p, 0x%08lx)", id, (long) task);

    switch(id->levelState)
    {
      case PHSTATE_LEV_DRV_STARTED:
	if (id->lotLevelUsed)
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "Possible error in test cell client:\n"
		    "reaching cassette start level while %s",
		    getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	}
	break;
      case PHSTATE_LEV_LOT_STARTED:
      case PHSTATE_LEV_CASS_DONE:
	break;
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching cassette start level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform cassette start state changes */
	id->cassLevelUsed = 1;
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachWafStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachWafStarted(P%p, 0x%08lx)", id, (long) task);

    switch(id->levelState)
    {
      case PHSTATE_LEV_DRV_STARTED:
	if (id->lotLevelUsed || id->cassLevelUsed)
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "Possible error in test cell client:\n"
		    "reaching wafer start level while %s",
		    getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	}
	break;
      case PHSTATE_LEV_LOT_STARTED:
	if (id->cassLevelUsed)
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "Possible error in test cell client:\n"
		    "reaching wafer start level while %s",
		    getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	}
	break;
      case PHSTATE_LEV_CASS_STARTED:
      case PHSTATE_LEV_WAF_DONE:
	break;
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching wafer start level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform wafer start state changes */
	id->retestRequest = 0;
	id->referenceRequest = 0;
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachDieStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachDieStarted(P%p, 0x%08lx)", id, (long) task);

    switch(id->levelState)
    {
      case PHSTATE_LEV_WAF_STARTED:
      case PHSTATE_LEV_DIE_DONE:
	break;
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching die start level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform die start state changes */
	if (!id->subdieLevelUsed)
	{
	    /* Note: at first call of this function it is not clear
               whether a subdie level is going to be used or
               not. However, during the first call, the skip mode is
               never set to SKIP_NEXT nor the tester mode has been
               changed from NORMAL */

	    /* change skip mode from next to current */
	    if (id->skipModeState == PHSTATE_SKIP_NEXT)
		id->skipModeState = PHSTATE_SKIP_CURRENT;

	    /* reset the retest and check device states to normal mode */
	    id->testModeState = PHSTATE_TST_NORMAL;
	}
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachSubDieStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachSubDieStarted(P%p, 0x%08lx)", id, (long) task);

    switch(id->levelState)
    {
      case PHSTATE_LEV_DIE_STARTED:
      case PHSTATE_LEV_SUBDIE_DONE:
	break;
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching sub-die start level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform sub-die start state changes */
	id->subdieLevelUsed = 1;
	
	/* change skip mode from next to current */
	if (id->skipModeState == PHSTATE_SKIP_NEXT)
	    id->skipModeState = PHSTATE_SKIP_CURRENT;
	
	/* reset the retest and check device states to normal mode */
	id->testModeState = PHSTATE_TST_NORMAL;
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachPauStarted(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachPauStarted(P%p, 0x%08lx)", id, (long) task);

    switch(id->levelState)
    {
      case PHSTATE_LEV_SUBDIE_STARTED:
	break;
      case PHSTATE_LEV_DIE_STARTED:
	if (!id->subdieLevelUsed)
	    break;
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Exceptionally reaching pause level while %s.\n"
		"Prober pause and unpause actions will not be performed",
		getStateString(id));
	retVal = PHSTATE_ERR_IGNORED;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform pause start state changes */
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachPauDone(struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    long flagCheck;
    long flagRetest;
    long flagSkip;
    long flagRetWaf;
    long flagReference;
    phTcomError_t tError;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachPauDone(P%p, 0x%08lx)", id, (long) task);

    switch(id->levelState)
    {
      case PHSTATE_LEV_PAUSE_STARTED:
	break;
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Exceptionally reaching end of pause level while %s.\n"
		"Prober pause and unpause actions will not be performed",
		getStateString(id));
	retVal = PHSTATE_ERR_IGNORED;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform pause done state changes */

	GetSysFlag(flagRetest, PHTCOM_SF_CI_RETEST, tError);
	GetSysFlag(flagCheck, PHTCOM_SF_CI_CHECK_DEV, tError);
	GetSysFlag(flagSkip, PHTCOM_SF_CI_SKIP, tError);
	GetSysFlag(flagRetWaf, PHTCOM_SF_CI_RETEST_W, tError);
	GetSysFlag(flagReference, PHTCOM_SF_CI_REFERENCE, tError);
    
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

	/* check the reference wafer flag */
	if (flagReference)
	    id->referenceRequest = 1;

	/* check for a wafer retest request */
	if (flagRetWaf)
	    id->retestRequest = 1;

	/* reset the pause flag, if not skipping, checking, or
	   retesting devices */

	if (!flagSkip && !flagCheck && !flagRetest)
	    phTcomSetSystemFlag(myTester, PHTCOM_SF_CI_PAUSE, 0L);
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachSubDieDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachSubDieDone(P%p, 0x%08lx)", id, (long) task);

    /* look at previous state */
    switch(id->levelState)
    {
      case PHSTATE_LEV_SUBDIE_STARTED:
	break;
      case PHSTATE_LEV_PAUSE_DONE:
	if (id->subdieLevelUsed)
	    break;
	/* reaching sub-die done after pause but sub-die start was
	   not used */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of sub-die level but sub-die level was never started");
	retVal = PHSTATE_ERR_ILLTRANS;
	break;		
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of sub-die level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform sub-die done state changes */

	/* if we came out of the pause done actions, the skip next
           state may be set. Now we set it to the skip current state */
	if (id->skipModeState == PHSTATE_SKIP_CURRENT)
	    id->skipModeState = PHSTATE_SKIP_NORMAL;
	if (id->skipModeState == PHSTATE_SKIP_NEXT_CURRENT)
	    id->skipModeState = PHSTATE_SKIP_NEXT;

    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachDieDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachDieDone(P%p, 0x%08lx)", id, (long) task);

    /* look at previous state */
    switch(id->levelState)
    {
      case PHSTATE_LEV_SUBDIE_DONE:
      case PHSTATE_LEV_DIE_STARTED:
	break;
      case PHSTATE_LEV_PAUSE_DONE:
	if (id->subdieLevelUsed)
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "Possible error in test cell client:\n"
		    "reaching end of die level while %s",
		    getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	}
	break;
      case PHSTATE_LEV_SUBDIE_STARTED:
	if (id->expectLevelEnd || id->expectAbort)
	    break;
	else
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "assuming quit of testprogram");
	    break;
	}
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of die level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform die done state changes */

	if (!id->subdieLevelUsed)
	{
	    /* if we came out of the pause done actions, the skip next
	       state may be set. Now we set it to the skip current state */
	    if (id->skipModeState == PHSTATE_SKIP_CURRENT)
		id->skipModeState = PHSTATE_SKIP_NORMAL;
	    if (id->skipModeState == PHSTATE_SKIP_NEXT_CURRENT)
		id->skipModeState = PHSTATE_SKIP_NEXT;
	}
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachWafDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachWafDone(P%p, 0x%08lx)", id, (long) task);

    /* look at previous state */
    switch(id->levelState)
    {
      case PHSTATE_LEV_DIE_DONE:
      case PHSTATE_LEV_WAF_STARTED:
	break;
      case PHSTATE_LEV_PAUSE_DONE:
	if (!id->referenceRequest && !id->retestRequest)
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "Possible error in test cell client:\n"
		    "reaching end of wafer level while %s",
		    getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	}
	break;
      case PHSTATE_LEV_DIE_STARTED:
	if (id->expectLevelEnd || id->expectAbort)
	    break;
	else
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "assuming quit of testprogram");
	    break;
	}
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of wafer level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform wafer done state changes */
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachCassDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachCassDone(P%p, 0x%08lx)", id, (long) task);

    /* look at previous state */
    switch(id->levelState)
    {
      case PHSTATE_LEV_WAF_DONE:
      case PHSTATE_LEV_CASS_STARTED:
	break;
      case PHSTATE_LEV_WAF_STARTED:
	if (id->expectLevelEnd || id->expectAbort)
	    break;
	else
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "assuming quit of testprogram");
	    break;
	}
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of cassette level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (!id->cassLevelUsed)
    {
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of cassette level but cassette level was not started");
	retVal = PHSTATE_ERR_ILLTRANS;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform cassette done state changes */
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachLotDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachLotDone(P%p, 0x%08lx)", id, (long) task);

    /* look at previous state */
    switch(id->levelState)
    {
      case PHSTATE_LEV_CASS_DONE:
      case PHSTATE_LEV_LOT_STARTED:
	break;		
      case PHSTATE_LEV_WAF_DONE:
      case PHSTATE_LEV_WAF_STARTED:
	if (id->expectLevelEnd  || id->expectAbort|| 
	    id->levelState == PHSTATE_LEV_WAF_DONE)
	{
	    if (id->cassLevelUsed)
	    {
		if (task == PHSTATE_AT_STEP)
		    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
			"Possible error in test cell client:\n"
			"reaching end of lot level while %s.\n"
			"End of cassette level seems to be missing",
			getStateString(id));
		retVal = PHSTATE_ERR_ILLTRANS;
	    }
	}
	else if (!id->cassLevelUsed)
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "assuming quit of testprogram");
	}
	else
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "Possible error in test cell client:\n"
		    "reaching end of lot level while %s",
		    getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	}
	break;
      case PHSTATE_LEV_CASS_STARTED:
	if (id->expectLevelEnd || id->expectAbort)
	    break;
	else
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "assuming quit of testprogram");
	    break;
	}
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of lot level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }

    if (!id->lotLevelUsed)
    {
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of lot level but lot level was not started");
	retVal = PHSTATE_ERR_ILLTRANS;
    }

    if (task == PHSTATE_AT_STEP)
    {
	/* perform lot done state changes */
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

static phStateError_t reachDrvDone(
    struct phStateStruct *id, enum actionTask_t task)
{
    phStateError_t retVal = PHSTATE_ERR_OK;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"reachLotDone(P%p, 0x%08lx)", id, (long) task);

    /* look at previous state */
    switch(id->levelState)
    {
      case PHSTATE_LEV_LOT_DONE:
      case PHSTATE_LEV_DRV_STARTED:
	break;		
      case PHSTATE_LEV_WAF_DONE:
      case PHSTATE_LEV_WAF_STARTED:
	if (id->expectLevelEnd  || id->expectAbort
	    || id->levelState == PHSTATE_LEV_WAF_DONE)
	{
	    if (id->lotLevelUsed || id->cassLevelUsed)
	    {
		if (task == PHSTATE_AT_STEP)
		    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
			"Possible error in test cell client:\n"
			"reaching end of driver level while %s.\n"
			"End of lot and/or cassette level seem to be missing",
			getStateString(id));
		retVal = PHSTATE_ERR_ILLTRANS;
	    }
	}
	else if (!id->lotLevelUsed && !id->cassLevelUsed)
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "assuming quit of testprogram");
	}
	else
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "Possible error in test cell client:\n"
		    "reaching end of driver level while %s",
		    getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	}
	break;
      case PHSTATE_LEV_CASS_DONE:
      case PHSTATE_LEV_CASS_STARTED:
	if (id->expectLevelEnd  || id->expectAbort
	    || id->levelState == PHSTATE_LEV_CASS_DONE)
	{
	    if (id->lotLevelUsed)
	    {
		if (task == PHSTATE_AT_STEP)
		    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
			"Possible error in test cell client:\n"
			"reaching end of driver level while %s.\n"
			"End of lot level seems to be missing",
			getStateString(id));
		retVal = PHSTATE_ERR_ILLTRANS;
	    }
	}
	else if (!id->lotLevelUsed)
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "assuming quit of testprogram");
	}
	else
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "Possible error in test cell client:\n"
		    "reaching end of driver level while %s",
		    getStateString(id));
	    retVal = PHSTATE_ERR_ILLTRANS;
	}
	break;
      case PHSTATE_LEV_LOT_STARTED:
	if (id->expectLevelEnd || id->expectAbort)
	    break;
	else
	{
	    if (task == PHSTATE_AT_STEP)
		phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		    "assuming quit of testprogram");
	    break;
	}
      default:
	/* coming from unexpected state */
	if (task == PHSTATE_AT_STEP)
	    phLogStateMessage(myLogger, PHLOG_TYPE_WARNING,
		"Possible error in test cell client:\n"
		"reaching end of driver level while %s",
		getStateString(id));
	retVal = PHSTATE_ERR_ILLTRANS;
	break;
    }


    if (task == PHSTATE_AT_STEP)
    {
	/* perform driver done state changes */
    }

    return retVal;
}

/*---------------------------------------------------------------------------*/

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

/*---------------------------------------------------------------------------*/

static phStateError_t  performChangeLevel(
    struct phStateStruct *stateID, phStateLevel_t newState, 
    actionPtr_t stateAction, enum actionTask_t type)
{
    phStateError_t retVal = PHSTATE_ERR_OK;
    long leFlag = 0L;
    long abFlag = 0L;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE,
	"performChangeLevel(P%p, 0x%08x, P%p, 0x%08x)",
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

    if(phTcomGetSystemFlag(myTester, PHTCOM_SF_CI_ABORT, &abFlag)
	== PHTCOM_ERR_OK && abFlag)
    {
	/* expect do go one level up now, abort situation */
	stateID->expectAbort = 1;
    }
    else
	stateID->expectAbort = 0;

    if (stateAction)
	retVal = (*stateAction)(stateID, type);

    if (type != PHSTATE_AT_CHECK && 
	newState != PHSTATE_LEV_DONTCARE &&
	retVal != PHSTATE_ERR_IGNORED)
    {
	stateID->levelState = newState;
	phLogStateMessage(myLogger, LOG_NORMAL,
	    "changed state to '%s'", getStateString(stateID));

	if(phTcomGetSystemFlag(myTester, PHTCOM_SF_CI_LEVEL_END, &leFlag)
	    == PHTCOM_ERR_OK && leFlag)
	{
	    /* on next level change, we expect do go one level up, similar
	       to the abort situation */
	    stateID->expectLevelEnd = 1;
	}
	else
	    stateID->expectLevelEnd = 0;
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

    /* use the logger */
    if (logger && tester)
    {
	/* if we got a valid logger, use it and trace the first call
           to this module */
	myLogger = logger;
	myTester = tester;
	phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	    "phStateInit(P%p, P%p, P%p)", stateID, myTester, myLogger);
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
    tmpId->cassLevelUsed = 0;
    tmpId->subdieLevelUsed = 0;
    tmpId->expectLevelEnd = 0;
    tmpId->expectAbort = 0;
/*
    tmpId->currentWafer = PHESTATE_WAFT_UNDEFINED;
    tmpId->nextWafer = PHESTATE_WAFT_REGULAR;
*/
    tmpId->retestRequest = 0;
    tmpId->referenceRequest = 0;

    /* initialize the level default state transition machine */
    for (frameActIndex = PHFRAME_ACT_DRV_START; 
	 frameActIndex < PHFRAME_ACT__END; frameActIndex = (phFrameAction_t)(frameActIndex + 1))
	for (stateLevIndex = PHSTATE_LEV_START; 
	     stateLevIndex < PHSTATE_LEV__END; stateLevIndex = (phStateLevel_t)(stateLevIndex + 1))
	    DefTrans(stateLevIndex, frameActIndex, PHSTATE_LEV_DONTCARE, NULL);
	
    /* these are the regular state transitions during normal operation */
    DefTrans(PHSTATE_LEV_START,         PHFRAME_ACT_DRV_START,   PHSTATE_LEV_DRV_STARTED,   reachDrvStarted);

    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_LOT_START,   PHSTATE_LEV_LOT_STARTED,   reachLotStarted);
    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_CASS_START,  PHSTATE_LEV_CASS_STARTED,  reachCassStarted);
    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_WAF_START,   PHSTATE_LEV_WAF_STARTED,   reachWafStarted);
    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_DRV_STARTED,   PHFRAME_ACT_DRV_START,   PHSTATE_LEV_DRV_STARTED,   NULL           );

    DefTrans(PHSTATE_LEV_DRV_DONE,      PHFRAME_ACT_DRV_START,   PHSTATE_LEV_DRV_STARTED,   reachDrvStarted);
    DefTrans(PHSTATE_LEV_DRV_DONE,      PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      NULL           );

    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_CASS_START,  PHSTATE_LEV_CASS_STARTED,  reachCassStarted);
    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_WAF_START,   PHSTATE_LEV_WAF_STARTED,   reachWafStarted);
    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_LOT_STARTED,   PHFRAME_ACT_LOT_START,   PHSTATE_LEV_LOT_STARTED,   NULL           );

    DefTrans(PHSTATE_LEV_LOT_DONE,      PHFRAME_ACT_LOT_START,   PHSTATE_LEV_LOT_STARTED,   reachLotStarted);
    DefTrans(PHSTATE_LEV_LOT_DONE,      PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_LOT_DONE,      PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      NULL           );

    DefTrans(PHSTATE_LEV_CASS_STARTED,  PHFRAME_ACT_WAF_START,   PHSTATE_LEV_WAF_STARTED,   reachWafStarted);
    DefTrans(PHSTATE_LEV_CASS_STARTED,  PHFRAME_ACT_CASS_DONE,   PHSTATE_LEV_CASS_DONE,     reachCassDone   );
    DefTrans(PHSTATE_LEV_CASS_STARTED,  PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_CASS_STARTED,  PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_CASS_STARTED,  PHFRAME_ACT_CASS_START,  PHSTATE_LEV_CASS_STARTED,  NULL           );

    DefTrans(PHSTATE_LEV_CASS_DONE,     PHFRAME_ACT_CASS_START,  PHSTATE_LEV_CASS_STARTED,  reachCassStarted);
    DefTrans(PHSTATE_LEV_CASS_DONE,     PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_CASS_DONE,     PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_CASS_DONE,     PHFRAME_ACT_CASS_DONE,   PHSTATE_LEV_CASS_DONE,     NULL           );

    DefTrans(PHSTATE_LEV_WAF_STARTED,   PHFRAME_ACT_DIE_START,   PHSTATE_LEV_DIE_STARTED,   reachDieStarted);
    DefTrans(PHSTATE_LEV_WAF_STARTED,   PHFRAME_ACT_WAF_DONE,    PHSTATE_LEV_WAF_DONE,      reachWafDone   );
    DefTrans(PHSTATE_LEV_WAF_STARTED,   PHFRAME_ACT_CASS_DONE,   PHSTATE_LEV_CASS_DONE,     reachCassDone  );
    DefTrans(PHSTATE_LEV_WAF_STARTED,   PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_WAF_STARTED,   PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_WAF_STARTED,   PHFRAME_ACT_WAF_START,   PHSTATE_LEV_WAF_STARTED,   NULL           );

    DefTrans(PHSTATE_LEV_WAF_DONE,      PHFRAME_ACT_WAF_START,   PHSTATE_LEV_WAF_STARTED,   reachWafStarted);
    DefTrans(PHSTATE_LEV_WAF_DONE,      PHFRAME_ACT_CASS_DONE,   PHSTATE_LEV_CASS_DONE,     reachCassDone  );
    DefTrans(PHSTATE_LEV_WAF_DONE,      PHFRAME_ACT_LOT_DONE,    PHSTATE_LEV_LOT_DONE,      reachLotDone   );
    DefTrans(PHSTATE_LEV_WAF_DONE,      PHFRAME_ACT_DRV_DONE,    PHSTATE_LEV_DRV_DONE,      reachDrvDone   );
    DefTrans(PHSTATE_LEV_WAF_DONE,      PHFRAME_ACT_WAF_DONE,    PHSTATE_LEV_WAF_DONE,      NULL           );

    DefTrans(PHSTATE_LEV_DIE_STARTED,   PHFRAME_ACT_SUBDIE_START,PHSTATE_LEV_SUBDIE_STARTED,reachSubDieStarted);
    DefTrans(PHSTATE_LEV_DIE_STARTED,   PHFRAME_ACT_PAUSE_START, PHSTATE_LEV_PAUSE_STARTED, reachPauStarted);
    DefTrans(PHSTATE_LEV_DIE_STARTED,   PHFRAME_ACT_DIE_DONE,    PHSTATE_LEV_DIE_DONE,      reachDieDone   );
    DefTrans(PHSTATE_LEV_DIE_STARTED,   PHFRAME_ACT_WAF_DONE,    PHSTATE_LEV_WAF_DONE,      reachWafDone   );
    DefTrans(PHSTATE_LEV_DIE_STARTED,   PHFRAME_ACT_DIE_START,   PHSTATE_LEV_DIE_STARTED,   NULL           );

    DefTrans(PHSTATE_LEV_DIE_DONE,      PHFRAME_ACT_DIE_START,   PHSTATE_LEV_DIE_STARTED,   reachDieStarted);
    DefTrans(PHSTATE_LEV_DIE_DONE,      PHFRAME_ACT_WAF_DONE,    PHSTATE_LEV_WAF_DONE,      reachWafDone   );
    DefTrans(PHSTATE_LEV_DIE_DONE,      PHFRAME_ACT_DIE_DONE,    PHSTATE_LEV_DIE_DONE,      NULL           );

    DefTrans(PHSTATE_LEV_SUBDIE_STARTED,PHFRAME_ACT_PAUSE_START, PHSTATE_LEV_PAUSE_STARTED, reachPauStarted);
    DefTrans(PHSTATE_LEV_SUBDIE_STARTED,PHFRAME_ACT_SUBDIE_DONE, PHSTATE_LEV_SUBDIE_DONE,   reachSubDieDone);
    DefTrans(PHSTATE_LEV_SUBDIE_STARTED,PHFRAME_ACT_DIE_DONE,    PHSTATE_LEV_DIE_DONE,      reachDieDone   );
    DefTrans(PHSTATE_LEV_SUBDIE_STARTED,PHFRAME_ACT_SUBDIE_START,PHSTATE_LEV_SUBDIE_STARTED,NULL           );

    DefTrans(PHSTATE_LEV_SUBDIE_DONE,   PHFRAME_ACT_SUBDIE_START,PHSTATE_LEV_SUBDIE_STARTED,reachSubDieStarted);
    DefTrans(PHSTATE_LEV_SUBDIE_DONE,   PHFRAME_ACT_DIE_DONE,    PHSTATE_LEV_DIE_DONE,      reachDieDone   );
    DefTrans(PHSTATE_LEV_SUBDIE_DONE,   PHFRAME_ACT_SUBDIE_DONE, PHSTATE_LEV_SUBDIE_DONE,   NULL           );

    DefTrans(PHSTATE_LEV_PAUSE_STARTED, PHFRAME_ACT_PAUSE_DONE,  PHSTATE_LEV_PAUSE_DONE,    reachPauDone   );
    DefTrans(PHSTATE_LEV_PAUSE_STARTED, PHFRAME_ACT_PAUSE_START, PHSTATE_LEV_PAUSE_STARTED, NULL           );

    DefTrans(PHSTATE_LEV_PAUSE_DONE,    PHFRAME_ACT_SUBDIE_DONE, PHSTATE_LEV_SUBDIE_DONE,   reachSubDieDone);
    DefTrans(PHSTATE_LEV_PAUSE_DONE,    PHFRAME_ACT_DIE_DONE,    PHSTATE_LEV_DIE_DONE,      reachDieDone   );
    DefTrans(PHSTATE_LEV_PAUSE_DONE,    PHFRAME_ACT_WAF_DONE,    PHSTATE_LEV_WAF_DONE,      reachWafDone   );
    DefTrans(PHSTATE_LEV_PAUSE_DONE,    PHFRAME_ACT_PAUSE_DONE,  PHSTATE_LEV_PAUSE_DONE,    NULL           );

    /* these are exceptional state transitions, only occuring, if the
       test cell client is not set up correctly or if something else
       is screwed up */
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_DRV_START,   PHSTATE_LEV_DRV_STARTED,   reachDrvStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_LOT_START,   PHSTATE_LEV_LOT_STARTED,   reachLotStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_CASS_START,  PHSTATE_LEV_CASS_STARTED,  reachCassStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_WAF_START,   PHSTATE_LEV_WAF_STARTED,   reachWafStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_DIE_START,   PHSTATE_LEV_DIE_STARTED,   reachDieStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_SUBDIE_START,PHSTATE_LEV_SUBDIE_STARTED,reachSubDieStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_PAUSE_START, PHSTATE_LEV_PAUSE_STARTED, reachPauStarted);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_PAUSE_DONE,  PHSTATE_LEV_PAUSE_DONE,    reachPauDone   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_SUBDIE_DONE, PHSTATE_LEV_SUBDIE_DONE,   reachSubDieDone);
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_DIE_DONE,    PHSTATE_LEV_DIE_DONE,      reachDieDone   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_WAF_DONE,    PHSTATE_LEV_WAF_DONE,      reachWafDone   );
    DefTrans(PHSTATE_LEV_DONTCARE,      PHFRAME_ACT_CASS_DONE,   PHSTATE_LEV_CASS_DONE,     reachCassDone  );
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
    CheckHandle(stateID);
    
    /* return the requested value */
    *state = stateID -> drvModeState;
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateGetDrvMode(P%p, P%p -> 0x%08lx)", 
	stateID, state, (long) *state);

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
	"phStateSetDrvMode(P%p, 0x%08lx)", stateID, (long) state);
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
    CheckHandle(stateID);
    
    /* return the requested value */
    *state = stateID -> skipModeState;

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateGetSkipMode(P%p, P%p, 0x%08lx)", 
	stateID, state, (long) *state);

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
	"phStateSetSkipMode(P%p, 0x%08lx)", stateID, (long) state);
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
    CheckHandle(stateID);
    
    /* return the requested value */
    *state = stateID -> testModeState;
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateGetTesterMode(P%p, P%p -> 0x%08lx)", 
	stateID, state, (long) *state);

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
	"phStateSetTesterMode(P%p, 0x%08lx)", stateID, (long) state);
    CheckHandle(stateID);
    
    /* return the requested value */
    stateID -> testModeState = state;

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Get wafer mode
 *
 * Authors: Michael Vogt
 *
 * History: 03 Dec 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 * 
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateGetWaferMode(
    phStateId_t stateID               /* driver state ID */,
    phStateWaferRequest_t *request    /* current wafer request, as of
                                         last pause end actions */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    CheckHandle(stateID);
    
    /* return the requested value */
    if (stateID -> retestRequest)
    {
	if (stateID -> referenceRequest)
	    *request = PHSTATE_WR_RET_REF;
	else
	    *request = PHSTATE_WR_RETEST;
    }
    else
    {
	if (stateID -> referenceRequest)
	    *request = PHSTATE_WR_REFERENCE;
	else
	    *request = PHSTATE_WR_NORMAL;	
    }

    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateGetWaferMode(P%p, P%p -> 0x%08lx)", 
	stateID, request, (long ) *request);

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 *
 * Set wafer mode
 *
 * Authors: Michael Vogt
 *
 * History: 03 Dec 1999, Michael Vogt, created
 *
 * Description:
 * please refer to ph_state.h
 *
 * Returns: error code
 *
 ***************************************************************************/
phStateError_t phStateSetWaferMode(
    phStateId_t stateID               /* driver state ID */,
    phStateWaferRequest_t request     /* new wafer request, as in
                                         after last pause end actions */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogStateMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phStateSetWaferMode(P%p, 0x%08lx)", 
	stateID, (long) request);
    CheckHandle(stateID);
    
    /* set the requested value */
    switch (request)
    {
      case PHSTATE_WR_NORMAL:
	stateID -> referenceRequest = 0;
	stateID -> retestRequest = 0;
	break;
      case PHSTATE_WR_RETEST:
	stateID -> referenceRequest = 0;
	stateID -> retestRequest = 1;
	break;
      case PHSTATE_WR_REFERENCE:
	stateID -> referenceRequest = 1;
	stateID -> retestRequest = 0;
	break;
      case PHSTATE_WR_RET_REF:
	stateID -> referenceRequest = 1;
	stateID -> retestRequest = 1;
	break;
    }

    return PHSTATE_ERR_OK;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
