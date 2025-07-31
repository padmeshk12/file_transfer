/******************************************************************************
 *
 *       (c) Copyright Advantest Ltd., 2012
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_hhook.c
 * CREATED  : 01 Jul 2023
 *
 * CONTENTS : Example for hook function usage
 *
 * AUTHORS  : Zuria Zhu, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, created
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

#define _PH_HHOOK_INTERNAL_

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_hfunc.h"
#include "ph_hhook.h"

#include "ph_keys.h"
#include "ph_hhook_private.h"
#include "dev/DriverDevKits.hpp"

#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/* don't trust anyone .... */
#define PH_HANDLE_CHECK

#ifdef DEBUG
#define PH_HANDLE_CHECK
#endif

#ifdef PH_HANDLE_CHECK
#define CheckInitialized(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHHOOK_ERR_NOT_INIT
#else
#define CheckInitialized()
#endif


/* to activate some hook functions, remove some comments for the
   following defines */

/* #define ACTION_START_IMPLEMENTED */
/* #define ACTION_DONE_IMPLEMENTED */
/* #define PROBLEM_IMPLEMENTED */ 
/* #define WAITING_IMPLEMENTED */
/* #define POPUP_IMPLEMENTED */


/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

static struct phHookStruct hook =
{
    (struct phHookStruct *) NULL,
    (phFuncId_t)            NULL,
    (phComId_t)             NULL,
    (phLogId_t)             NULL,
    (phConfId_t)            NULL,
    (phStateId_t)           NULL,
    (phEstateId_t)          NULL,
    (phTcomId_t)            NULL
};

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * Check availability of hook functions
 *
 * Authors: Zuria Zhu
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, hook template created
 *
 * Description:
 * Please refer to ph_hhook.h
 *
 * Returns: The availability mask
 *
 *****************************************************************************/
phHookAvailability_t phHookAvailable(void)
{
    phHookAvailability_t result;

    /* Return whatever functions are implemented for this hook plugin
       (binary ored list of flags). Here is the full set of possible
       functions. Remove comment signs below to activate hook specific
       hook functions. In any case PHHOOK_AV_INIT is mandatory, if
       hook functions are used! */

    result = PHHOOK_AV_INIT;            /* phHookInit(3)           */

#ifdef ACTION_START_IMPLEMENTED
    result = (phHookAvailability_t)(result | PHHOOK_AV_ACTION_START);   /* phHookActionStart(3)    */
#endif

#ifdef ACTION_DONE_IMPLEMENTED
    result = (phHookAvailability_t)(result | PHHOOK_AV_ACTION_DONE);    /* phHookActionDone(3)     */
#endif

#ifdef PROBLEM_IMPLEMENTED
    result = (phHookAvailability_t)(result | PHHOOK_AV_PROBLEM);        /* phHookProblem(3)        */
#endif

#ifdef WAITING_IMPLEMENTED
    result = (phHookAvailability_t)(result | PHHOOK_AV_WAITING);        /* phHookWaiting(3)        */
#endif

#ifdef POPUP_IMPLEMENTED
    result = (phHookAvailability_t)(result | PHHOOK_AV_POPUP);          /* phHookPopup(3)          */
#endif


    return result;
}


/*****************************************************************************
 *
 * Initialize handler specific hook framework
 *
 * Authors: Zuria Zhu
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, hook template created
 *
 * Description:
 * Please refer to ph_hhook.h
 *
 * Returns: error code
 *
 *****************************************************************************/
phHookError_t phHookInit(
    phFuncId_t driver            /* the resulting driver plugin ID */,
    phComId_t communication      /* the valid communication link to the HW
			            interface used by the handler */,
    phLogId_t logger             /* the data logger to be used */,
    phConfId_t configuration     /* the configuration manager */,
    phStateId_t state            /* the overall driver state */,
    phEstateId_t estate          /* the equipment specific state */,
    phTcomId_t tester            /* the tester interface */
)
{
    /* the internal data structure is initialized. These values are
       needed for all further calls of hook functions since they
       provide the way how to communicate with other parts of the
       driver framework */

    hook.myself = &hook;
    hook.myDriver = driver;
    hook.myCom = communication;
    hook.myLogger = logger;
    hook.myConf = configuration;
    hook.myState = state;
    hook.myEstate = estate;
    hook.myTester = tester;

    /* now trace the call to this hook function */

    phLogHookMessage(hook.myLogger, PHLOG_TYPE_TRACE,
	"phHookInit(P%p, P%p, P%p, P%p, P%p, P%p)",
	driver, communication, logger, configuration, state, tester);

    /* now perform any additional internal hook initialization */

    /* .... */

    /* at the end don't forget to return the error code */

    return PHHOOK_ERR_OK;
}


/*****************************************************************************
 *
 * Driver action start hook
 *
 * Authors: Zuria Zhu
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, hook template created
 *
 * Description:
 * Please refer to ph_hhook.h
 *
 * Returns: error code
 *
 *****************************************************************************/
phHookError_t phHookActionStart(
    phFrameAction_t call          /* the identification of an incoming
				     call from the test cell client */,
    phEventResult_t *result       /* the result of this hook call */,
    char *parm_list_input         /* the parameters passed over from
				     the test cell client */,
    int parmcount                 /* the parameter count */,
    char *comment_out             /* the comment to be send back to the
				     test cell client */,
    int *comlen                   /* the length of the resulting
				     comment */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHHOOK_RES_ABORT by this hook
				     function */
)
{
    /* do checks and trace the call first */
    CheckInitialized(&hook);
    phLogHookMessage(hook.myLogger, PHLOG_TYPE_TRACE, 
	"phHookActionStart(0x%08lx, P%p, \"%s\", %d, \"%s\", %d, P%p)",
	(long) call, result, 
	parm_list_input ? parm_list_input : "(NULL)", parmcount, 
	comment_out ? comment_out : "(NULL)", comlen, 
	stReturn);

#ifdef ACTION_START_IMPLEMENTED

    /* Here is a sample implementation which prints out a message on
       each incoming ph_device_done call (before the framework starts
       to bin the devices) */

    switch (call)
    {
      case PHFRAME_ACT_DEV_START:

	phLogHookMessage(hook.myLogger, PHLOG_TYPE_MESSAGE_0,
	    "start of ph_device_start recognized by hook function");

	/* set the result value:
	   the framework must continue with the ph_device_start actions */
	*result = PHEVENT_RES_CONTINUE;
	break;

      case PHFRAME_ACT_DEV_DONE:

	phLogHookMessage(hook.myLogger, PHLOG_TYPE_MESSAGE_0,
	    "start of ph_device_done recognized by hook function");

	/* set the result value:
	   the framework must continue with the ph_device_done actions */
	*result = PHEVENT_RES_CONTINUE;
	break;

      default:

	/* set the result value:
	   the hook function has done nothing */
	*result = PHEVENT_RES_VOID;
	break;
    }

    return PHHOOK_ERR_OK;

#else

    /* not implemented */
    return PHHOOK_ERR_NA;

#endif
}


/*****************************************************************************
 *
 * Driver action done hook
 *
 * Authors: Zuria Zhu
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, hook template created
 *
 * Description:
 * Please refer to ph_hhook.h
 *
 * Returns: error code
 *
 *****************************************************************************/
phHookError_t phHookActionDone(
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
)
{
    /* some local variables */

#ifdef ACTION_DONE_IMPLEMENTED
    phEstateSiteUsage_t *sitePopulation;
    int entries;
    int i;
    static long deviceCount = 0L;
#endif

    /* do checks and trace the call first */
    CheckInitialized(&hook);
    phLogHookMessage(hook.myLogger, PHLOG_TYPE_TRACE, 
	"phHookActionDone(0x%08lx, \"%s\", %d, \"%s\", %d, P%p)",
	(long) call,
	parm_list_input ? parm_list_input : "(NULL)", parmcount, 
	comment_out ? comment_out : "(NULL)", comlen, 
	stReturn);

#ifdef ACTION_DONE_IMPLEMENTED

    /* Here is a sample implementation which prints out a message after
       each performed ph_device_start call */

    switch (call)
    {
      case PHFRAME_ACT_DEV_START:

	/* get the handler's site population status and count all devices */

	if (phEstateAGetSiteInfo(hook.myEstate, &sitePopulation, &entries) ==
	    PHESTATE_ERR_OK)
	{
	    for (i=0; i<entries; i++)
		if (sitePopulation[i] == PHESTATE_SITE_POPULATED)
		    deviceCount++;
	}

	phLogHookMessage(hook.myLogger, PHLOG_TYPE_MESSAGE_0,
	    "end of ph_device_start recognized by hook function, total: %ld",
	    deviceCount);
	break;

      case PHFRAME_ACT_DEV_DONE:

	phLogHookMessage(hook.myLogger, PHLOG_TYPE_MESSAGE_0,
	    "end of ph_device_done recognized by hook function");
	break;

      default:
	/* nothing to do */
	break;
    }

    return PHHOOK_ERR_OK;

#else

    /* not implemented */
    return PHHOOK_ERR_NA;

#endif
}


/*****************************************************************************
 *
 * Problem hook
 *
 * Authors: Zuria Zhu
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, hook template created
 *
 * Description:
 * Please refer to ph_hhook.h
 *
 * Returns: error code
 *
 *****************************************************************************/
phHookError_t phHookProblem(
    phEventCause_t reason         /* the reason, why this hook is called */,
    phEventDataUnion_t *data      /* pointer to additional data,
				     associated with the given
				     <reason>, or NULL, if no
				     additional data is available */,
    phEventResult_t *result       /* the result of this hook call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHHOOK_RES_ABORT by this hook
				     function */
)
{
    /* do checks and trace the call first */
    CheckInitialized(&hook);
    phLogHookMessage(hook.myLogger, PHLOG_TYPE_TRACE, 
	"phHookProblem(0x%08lx, P%p, P%p, P%p)",
	(long) reason, data, result, stReturn);

#ifdef PROBLEM_IMPLEMENTED

    /* Here is a sample implementation which prints out the total
       waiting time on timeout problems */

    switch (reason)
    {
      case PHEVENT_CAUSE_WP_TIMEOUT:

	if (data)
	    phLogHookMessage(hook.myLogger, PHLOG_TYPE_MESSAGE_0, 
		"hook layer recognized timeout after %ld seconds",
	        data->to.elapsedTimeSec);

	/* set the result value: 
	   the framework must continue to handle
	   the timeout situation. In this case the event handler will
	   now popup the timeout message. In case, the hook function
	   has solved the timeout problem (parts are inserted and the
	   state controller has been notified), the result should be
	   PHEVENT_RES_HANDLED */
	*result = PHEVENT_RES_CONTINUE;
	break;

      default:

	/* set the result value:
	   the hook function has done nothing */
	*result = PHEVENT_RES_VOID;
	break;
    }

    return PHHOOK_ERR_OK;

#else

    /* not implemented */
    return PHHOOK_ERR_NA;

#endif
}


/*****************************************************************************
 *
 * Waiting for parts hook
 *
 * Authors: Zuria Zhu
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, hook template created
 *
 * Description:
 * Please refer to ph_hhook.h
 *
 * Returns: error code
 *
 *****************************************************************************/
phHookError_t phHookWaiting(
    long elapsedTimeSec           /* elapsed time while waiting for
				     this part */,
    phEventResult_t *result       /* the result of this hook call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHHOOK_RES_ABORT by this hook
				     function */
)
{
#ifdef WAITING_IMPLEMENTED
    long flag;
#endif

    /* do checks and trace the call first */
    CheckInitialized(&hook);
    phLogHookMessage(hook.myLogger, PHLOG_TYPE_TRACE, 
	"phHookWaiting(%ld, P%p, P%p)", elapsedTimeSec, result, stReturn);

#ifdef WAITING_IMPLEMENTED

    /* Here is a sample implementation which reads SmarTest's PAUSE
       flag and prints a message, if the flag is set */

    if (phTcomGetSystemFlag(hook.myTester, PHTCOM_SF_CI_PAUSE, &flag) ==
	PHTCOM_ERR_OK)
    {
	if (flag)
	{
	    phLogHookMessage(hook.myLogger, PHLOG_TYPE_MESSAGE_0,
	    "hook function recognized pause flag set while waiting for parts");
	}
    }

    /* set the result value:
       the framework must continue to wait for parts */
    *result = PHEVENT_RES_CONTINUE;

    return PHHOOK_ERR_OK;
    
#else

    /* not implemented */
    return PHHOOK_ERR_NA;
#endif
}


/*****************************************************************************
 *
 * Message popup hook
 *
 * Authors: Zuria Zhu
 *
 * HISTORY  : 01 Jul 2023, Zuria Zhu, hook template created
 *
 * Description:
 * Please refer to ph_hhook.h
 *
 * Returns: error code
 *
 *****************************************************************************/
phHookError_t phHookPopup(
    phEventResult_t *result       /* the result of this hook call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHHOOK_RES_ABORT by this hook
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
)
{
    /* do checks and trace the call first */
    CheckInitialized(&hook);
    phLogHookMessage(hook.myLogger, PHLOG_TYPE_TRACE, 
	"phHookPopup(P%p, P%p, %d, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", P%p, P%p)",
	result, stReturn, needQuitAndCont,
	msg ? msg : "(NULL)",
	b2s ? b2s : "(NULL)",
	b3s ? b3s : "(NULL)",
	b4s ? b4s : "(NULL)",
	b5s ? b5s : "(NULL)",
	b6s ? b6s : "(NULL)",
	b7s ? b7s : "(NULL)",
	response, input);

#ifdef POPUP_IMPLEMENTED

    /* Here is a sample implementation which ...
       ... does nothing */

    /* ... */

    /* set the result value:
       the framework must continue to present the popup */
    *result = PHEVENT_RES_VOID;

    return PHHOOK_ERR_OK;
    
#else

    /* not implemented */
    return PHHOOK_ERR_NA;
#endif
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
