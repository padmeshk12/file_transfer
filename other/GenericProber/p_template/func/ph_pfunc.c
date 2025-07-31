/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_pfunc.c
 * CREATED  : 15 Jul 1999
 *
 * CONTENTS : Prober Driver Plugin for <prober family>
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Jul 1999, Michael Vogt, created
 *            25 Jan 1000, Michael Vogt, leveraged for probers
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
#include <assert.h>

/*--- module includes -------------------------------------------------------*/

#define _PH_PFUNC_INTERNAL_

#include "ph_tools.h"

#include "ph_tcom.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_pfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "ph_pfunc_private.h"
#include "identity.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- customization defines -------------------------------------------------*/

/* only perform dummy operations if configured with
   "plugin_simulation" and return immediately, put into comments if
   simulation mode should be prohibited */
#define PH_DUMMY_OPERATION

/* don't trust anyone, always check for a correct handles */
#define PH_HANDLE_CHECK

/* do not know how many wafers are in the input cassette */
#define ICASS_UNKNOWN

/*--- defines (don't change) ------------------------------------------------*/

#define MAX_ID_STR_LEN 512

/* check the handles, if in DEBUG mode */
#ifdef DEBUG
#define PH_HANDLE_CHECK
#endif

/* definition of the handle check */
#ifdef PH_HANDLE_CHECK
#define CheckInitialized() \
    if (!myLogger) \
        return PHPFUNC_ERR_NOT_INIT

#define CheckHandle(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHPFUNC_ERR_INVALID_HDL
#else
#define CheckInitialized()
#define CheckHandle(x)
#endif

/* definition of the dummy operation */
#ifdef PH_DUMMY_OPERATION
#define DummyOperation(p,x) \
    ((p)->simulationMode) && \
    ( \
        phLogFuncMessage(myLogger, LOG_NORMAL, \
            "simulated operation: " x), \
        sleep(getenv("PHPFUNC_FAST_SIMULATION_MODE") ? 0 : 1), \
        1 \
    )
#else
#define DummyOperation(p,x) 0
#endif


#ifdef PH_DUMMY_OPERATION
#define DummyReturnValue(p) \
    ((dummyReturnValues && \
        dummyReturnIndex < dummyReturnCount) ? \
	dummyReturnValues[dummyReturnIndex++] : \
        PHPFUNC_ERR_OK)
#else
#define DummyReturnValue(p) PHPFUNC_ERR_OK
#endif

#define RememberCall(x, c) phPFuncTaSetCall(x, c)
#define RememberSpecialCall(x, c) phPFuncTaSetSpecialCall(x, c)

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

/* Needed because the logger is only passed once during init to this
module but not during all. Is not stored in the state handle
structure, since it is then impossible to log invalid handle access to
this module */

static phLogId_t myLogger = NULL;

static phPFuncError_t *dummyReturnValues = NULL;
static int dummyReturnCount = 0;
static int dummyReturnIndex = 0;

/*--- functions -------------------------------------------------------------*/

void phPFuncSimResults(
    phPFuncError_t *returnSequence, 
    int count)
{
    dummyReturnValues = returnSequence;
    dummyReturnCount = count;
    dummyReturnIndex = 0;
}

static char *writeLongList(int number, long *bins)
{
    static char listOne[1024];
    static char listTwo[1024];
    static int useListOne = 1;
    char *destination;

    char oneBin[16];
    int i;

    destination = useListOne ? listOne : listTwo;
    useListOne = !useListOne;

    if (!bins)
	strcpy(destination, "NULL");
    else
    {
	strcpy(destination, "[");
	for (i=0; i<number && i<32; i++)
	{
	    sprintf(oneBin, "%ld", bins[i]);
	    strcat(destination, oneBin);
	    if (i+1 < number)
		strcat(destination, ", ");
	}
	if (i<number)
	    strcat(destination, "...");
	strcat(destination, "]");
    }

    return destination;    
}

static int phFuncRS232Reconfigure(struct phPFuncStruct *myself)
{
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
	"reconfigureRS232(P%p)", myself);

    phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
	"reconfigureRS232() not yet implemented");

    /* to be defined */
    return 0;
}

/*****************************************************************************
 *
 * Reconfigure the driver plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Aug 1999, Michael Vogt, generic template built
 *
 * Description: Only to be used from inside this module for dynamic
 * (re)configuration of the driver
 *
 ***************************************************************************/
static int reconfigureGlobal(struct phPFuncStruct *myself)
{
    int resultValue = 1;
    phEstateError_t estateError;
    phConfError_t confError;
    phEstateSiteUsage_t *population;
    int numSites;
    int i;
    int listLength;
    phConfType_t defType;
    static char emptyString[] = "";
    const char *ifType;
    double dHeartbeatTimeout;
    const char *sSimulationMode;

    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
	"reconfigureGlobal(P%p)", myself);

    /* get number of sites and active site mask from state controller,
       get the names of the sites from the configuration for future
       fast access. Note: The state controller is always reconfigured
       first before this function is called */
    estateError =phEstateAGetSiteInfo(myself->myEstate, &population,&numSites);
    if (estateError == PHESTATE_ERR_OK)
    {
	myself->noOfSites = numSites;
	for (i=0; i<numSites; i++)
	{
	    switch (population[i])
	    {
	      case PHESTATE_SITE_POPULATED:
	      case PHESTATE_SITE_EMPTY:
	      case PHESTATE_SITE_POPREPROBED:
		myself->activeSites[i] = 1;
		break;
	      case PHESTATE_SITE_DEACTIVATED:
		myself->activeSites[i] = 0;		
		break;
	      case PHESTATE_SITE_POPDEACT:
		myself->activeSites[i] = -1;		
		break;
	    }
	    confError = phConfConfString(myself->myConf, PHKEY_SI_HSIDS,
		1, &i, &(myself->siteIds[i]));
	    if (confError != PHCONF_ERR_OK)
		myself->siteIds[i] = emptyString;
	}
    }

    /* for prober family specific driver plugins, check the
       correctness of the new site setting here. It must match the
       real number of sites available at the prober ! */

    /* ... */


    /* if the number of bins is known and the bin IDs are given the
       appropriate fields should be initialized for fast access */

    myself->noOfBins = 0;
    if (phConfConfIfDef(myself->myConf, PHKEY_BI_HBIDS) == PHCONF_ERR_OK)
    {
	listLength = 0;
	confError = phConfConfType(myself->myConf, PHKEY_BI_HBIDS,
	    0, NULL, &defType, &listLength);
	if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST ||
	    listLength == 0)
	{
	    if (confError == PHCONF_ERR_OK)
		phLogFuncMessage(myself->myLogger, PHLOG_TYPE_ERROR,
		    "configuration \"%s\" of wrong type,\n"
		    "must be a list of prober bin names",
		    PHKEY_BI_HBIDS);
	    resultValue = 0;
	}
	else
	{
	    myself->noOfBins = listLength;
	    if (myself->binIds)
		free(myself->binIds);
	    myself->binIds = PhNNew(const char *, listLength);
	    for (i=0; i<listLength; i++)
	    {
		phConfConfString(myself->myConf, PHKEY_BI_HBIDS, 
		    1, &i, &(myself->binIds[i]));
	    }
	}
    }

    /* the central heartbeat timeout must be get from the
       configuration. It determines the maximum wait times while
       waiting for parts until a TIMEOUT error is returned */

    confError = phConfConfIfDef(myself->myConf, PHKEY_FC_HEARTBEAT);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfNumber(myself->myConf, PHKEY_FC_HEARTBEAT, 
	    0, NULL, &dHeartbeatTimeout);
    if (confError == PHCONF_ERR_OK)    
	myself-> heartbeatTimeout = (long) (dHeartbeatTimeout * 1000.0);
    else
    {
	myself-> heartbeatTimeout = 5000000L;
    }
    
    /* the plugin must also check, whether it works together with the
       correct interface type. */

    ifType = NULL;
    if (phConfConfString(myself->myConf, PHKEY_IF_TYPE, 0, NULL, &ifType) != 
	PHCONF_ERR_OK)
	resultValue = 0;
    
    if (strcasecmp(ifType, "gpib") == 0)
	myself->interfaceType = PHPFUNC_IF_GPIB;
    /* else if (strcasecmp(ifType, "rs232") == 0)
	myself->interfaceType = PHPFUNC_IF_RS232; */
    else
    {
	phLogFuncMessage(myself->myLogger, PHLOG_TYPE_ERROR,
	    "this driver plugin accepts the following "
	    "interface type configuration only:\n"
	    "%s: \"%s\"\n"
	    "Ensure that this parameter is set in the driver configuration",
	    PHKEY_IF_TYPE, "gpib");
	resultValue = 0;	
    }

    /* based on the interface type additional reconfigure may be necessary */

    if (resultValue) switch (myself->interfaceType)
    {
      case PHPFUNC_IF_GPIB:
	/* for GPIB based probers, reconfigure the GPIB parameters */
	if (!phFuncGpibReconfigure(myself))
	    resultValue = 0;
	break;
      case PHPFUNC_IF_RS232:
	/* for RS232 based probers, reconfigure the RS232 parameters */
	if (!phFuncRS232Reconfigure(myself))
	    resultValue = 0;
    break;
      default:
	/* not supported */
	phLogFuncMessage(myself->myLogger, PHLOG_TYPE_ERROR,
	    "the requested interface type is not supported");
	resultValue = 0;
	break;
    }

    /* now that everything is configured, see whether we want to fall
       back into simulation mode */

    if (phConfConfIfDef(myself->myConf, PHKEY_MO_PLUGSIM) == PHCONF_ERR_OK)
    {
	confError = phConfConfString(myself->myConf, PHKEY_MO_PLUGSIM, 
	    0, NULL, &sSimulationMode);
	if (confError == PHCONF_ERR_OK && 
	    strcasecmp(sSimulationMode, "yes") == 0)    
	    myself-> simulationMode = 1;
    }

    return resultValue;
}

/*****************************************************************************
 *
 * Check availability of plugin functions
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phPFuncAvailability_t phPFuncAvailable(void)
{
    phPFuncAvailability_t retval = (phPFuncAvailability_t) 0;

#ifdef INIT_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_INIT);
#endif
#ifdef RECONFIGURE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_RECONFIGURE);
#endif
#ifdef RESET_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_RESET);
#endif
#ifdef DRIVERID_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_DRIVERID);
#endif
#ifdef EQUIPID_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_EQUIPID);
#endif
#ifdef LDCASS_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_LDCASS);
#endif
#ifdef UNLCASS_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_UNLCASS);
#endif
#ifdef LDWAFER_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_LDWAFER);
#endif
#ifdef UNLWAFER_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_UNLWAFER);
#endif
#ifdef GETDIE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_GETDIE);
#endif
#ifdef BINDIE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_BINDIE);
#endif
#ifdef GETSUBDIE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_GETSUBDIE);
#endif
#ifdef BINSUBDIE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_BINSUBDIE);
#endif
#ifdef DIELIST_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_DIELIST);
#endif
#ifdef SUBDIELIST_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_SUBDIELIST);
#endif
#ifdef REPROBE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_REPROBE);
#endif
#ifdef CLEAN_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_CLEAN);
#endif
#ifdef PMI_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_PMI);
#endif
#ifdef PAUSE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_PAUSE);
#endif
#ifdef UNPAUSE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_UNPAUSE);
#endif
#ifdef TEST_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_TEST);
#endif
#ifdef DIAG_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_DIAG);
#endif
#ifdef STATUS_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_STATUS);
#endif
#ifdef UPDATE_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_UPDATE);
#endif
#ifdef CASSID_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_CASSID);
#endif
#ifdef WAFID_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_WAFID);
#endif
#ifdef PROBEID_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_PROBEID);
#endif
#ifdef LOTID_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_LOTID);
#endif
#ifdef STARTLOT_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_STARTLOT);
#endif
#ifdef ENDLOT_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_ENDLOT);
#endif
#ifdef COMMTEST_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_COMMTEST);
#endif
#ifdef DESTROY_IMPLEMENTED
    retval = (phPFuncAvailability_t)(retval | PHPFUNC_AV_DESTROY);
#endif

    return retval;
}

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize prober specific plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncInit(
    phPFuncId_t *driverID     /* the resulting driver plugin ID */,
    phTcomId_t  tester        /* the tester/tcom session to be used */,
    phComId_t communication   /* the valid communication link to the HW
			         interface used by the prober */,
    phLogId_t logger          /* the data logger to be used */,
    phConfId_t configuration  /* the configuration manager */,
    phEstateId_t estate       /* the prober state */
)
{
    static int isConfigured = 0;
    static struct phPFuncStruct *tmpId = NULL;

    char revision[1024];
    const char *proberFamily = "";
    const char *proberModel = "";
    char *family;
    struct modelDef *models;
    int i;

    /* only setup everything, if this is the first call. It could be
       that the private initialization returns with a WAITING error
       and we are called again. In this case, we do not re initialize
       everything ... */
    if (!isConfigured)
    {
	/* use the logger and all other modules... */
	if (tester && communication && logger && configuration && estate)
	{
	    /* if we got a valid logger, use it and trace the first call
	       to this module */
	    myLogger = logger;

	    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
		"phFuncInit(P%p, P%p, P%p, P%p)", driverID, communication, 
		myLogger, logger, configuration, estate);
	}
	else
	    /* passed logger is not valid at all, give up */
	    return PHPFUNC_ERR_NOT_INIT;

	/* allocate state data type */
	tmpId = PhNew(struct phPFuncStruct);
	if (tmpId == 0)
	{
	    phLogFuncMessage(myLogger, PHLOG_TYPE_FATAL,
		"not enough memory during call to phFuncInit()");
	    return PHPFUNC_ERR_MEMORY;
	}

	/* initialize the new handle */
	tmpId->myself = tmpId;
        tmpId->myTcom = tester;
	tmpId->myLogger = logger;
	tmpId->myConf = configuration;
	tmpId->myCom = communication;
	tmpId->myEstate = estate;

	/* variables used for simulation mode only, to allow for regression
	   tests only. Becomes active, if configuration "plugin_simulation" is
	   set to "yes" during inititalization or reconfiguration of plugin */

	tmpId->simulationMode = 0;

	/* do more initialization here.... */
	tmpId->noOfSites = 0;
	tmpId->noOfBins = 0;
	tmpId->binIds = NULL;

	phPFuncTaInit(tmpId);

	/* identify myself */
	phConfConfString(configuration, PHKEY_IN_HFAM, 0, NULL, &proberFamily);
	phConfConfString(configuration, PHKEY_IN_MODEL, 0, NULL, &proberModel);
	sprintf(revision, "%s", PHPFUNC_IDENT " " PHPFUNC_REVISION);
#ifdef PHPFUNC_LOCAL
	    strcat(revision, " (local version ");
	    strcat(revision, PHPFUNC_LOCAL);
	    strcat(revision, ")");
#endif
	phLogFuncMessage(myLogger, PHLOG_TYPE_MESSAGE_0,
	    "\n\nProber driver identification: \"%s\" compiled at %s\n"
	    "%s: \"%s\", %s: \"%s\" (from configuration)\n", 
	    revision, PHPFUNC_DATE,
	    PHKEY_IN_HFAM, proberFamily, PHKEY_IN_MODEL, proberModel);

	/* check whether the configured prober model fits to this plugin */
	phFuncPredefModels(&family, &models);
	if (strcmp(family, proberFamily) != 0)
	{
	    phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
		"The given prober family \"%s\" is not\n"
		"valid for this prober driver plugin. Expected: \"%s\"", 
		proberFamily, family);
	    return PHPFUNC_ERR_CONFIG;
	}
	i = 0;
	while (models[i].model != PHPFUNC_MOD__END &&
	    strcmp(models[i].name, "*") != 0 &&
	    !phToolsMatch(proberModel, models[i].name))
	    i++;
	if (models[i].model == PHPFUNC_MOD__END)
	{
	    /* not found in supported model list */
	    phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
		"The given prober model \"%s\" is not\n"
		"supported by this prober driver plugin", proberModel);
	    return PHPFUNC_ERR_CONFIG;
	}
	tmpId->model = models[i].model;
	tmpId->cmodelName = strdup(proberModel);

	/* perform the configuration check and configure */
	if (!reconfigureGlobal(tmpId))
	{
	    phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
		"driver plugin could not be initialized\n"
		"due to errors in configuration");
	    return PHPFUNC_ERR_CONFIG;
	}

	isConfigured = 1;
    }

    /* return the id for the initialized driver */
    *driverID = tmpId;

    RememberCall(tmpId, PHPFUNC_AV_INIT);
    /* stop here, if working in simulation mode */
    if (DummyOperation(tmpId, "initialize prober driver plugin"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    /* define a reference wafer */
	    phEstateASetWafInfo(tmpId->myEstate, PHESTATE_WAFL_REFERENCE,
		PHESTATE_WAFU_LOADED, PHESTATE_WAFT_REFERENCE);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section (privateInit should include
       privateReconfiguration() */
    return privateInit(tmpId);
}
#endif /* INIT_IMPLEMENTED */


#ifdef RECONFIGURE_IMPLEMENTED
/*****************************************************************************
 *
 * reconfigure driver plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncReconfigure(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncReconfigure(P%p)", proberID);
    CheckHandle(proberID);

    /* before doing dummy stuff, ensure that we receive the changed
       configuration */
    if (!reconfigureGlobal(proberID))
    {
	phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
	    "driver plugin could not be fully (re)configured\n"
	    "due to errors in configuration\n"
	    "Driver plugin currently in inconsistent state !!!");
	return PHPFUNC_ERR_CONFIG;
    }

    RememberCall(proberID, PHPFUNC_AV_RECONFIGURE);
    if (DummyOperation(proberID, "reconfigure prober driver plugin"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }
    
    /* complete call in private section */
    return privateReconfigure(proberID);
}
#endif /* RECONFIGURE_IMPLEMENTED */


#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the prober
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncReset(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncReset(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_RESET);
    if (DummyOperation(proberID, "reset prober driver plugin"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateReset(proberID);
}
#endif /* RESET_IMPLEMENTED */


#ifdef DRIVERID_IMPLEMENTED
/*****************************************************************************
 *
 * return the driver identification string
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncDriverID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";
    char revision[1024];
    const char *proberFamily;
    const char *proberModel;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncDriverID(P%p, P%p)", proberID, driverIdString);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_DRIVERID);
    if (DummyOperation(proberID, "get plugin ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy plugin ID string");
	    *driverIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* assemble the plugin ID string */
    phConfConfString(proberID->myConf, PHKEY_IN_HFAM, 
        0, NULL, &proberFamily);
    phConfConfString(proberID->myConf, PHKEY_IN_MODEL, 
        0, NULL, &proberModel);
    sprintf(revision, "%s", PHPFUNC_IDENT " " PHPFUNC_REVISION);
#ifdef PHPFUNC_LOCAL
    strcat(revision, " (local version ");
    strcat(revision, PHPFUNC_LOCAL);
    strcat(revision, ")");
#endif
    sprintf(theIDString, 
        "\"%s\" compiled at %s, "
        "%s: \"%s\", %s: \"%s\" (from configuration)", 
        revision, PHPFUNC_DATE,
        PHKEY_IN_HFAM, proberFamily, PHKEY_IN_MODEL, proberModel);
    *driverIdString = theIDString;

    /* complete call in private section */
    return privateDriverID(proberID, driverIdString);
}
#endif /* DRIVERID_IMPLEMENTED */


#ifdef EQUIPID_IMPLEMENTED
/*****************************************************************************
 *
 * return the prober identification string
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncEquipID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncEquipID(P%p)", proberID, equipIdString);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_EQUIPID);
    if (DummyOperation(proberID, "get prober ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy equipment ID string");
	    *equipIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateEquipID(proberID, equipIdString);
}
#endif /* EQUIPID_IMPLEMENTED */


#ifdef LDCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Load an input cassette
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncLoadCass(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncLoadCass(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_LDCASS);
    if (DummyOperation(proberID, "load (input) cassette"))
    {
	/* this is dummy code, only performed during regression tests */
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
	/*int count;*/
	/*int i;*/
/* End of Huatek Modification */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
#ifdef ICASS_UNKNOWN
	    /* load an input cassette with unknown number of wafers */
	    phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_LOADED);
#else
	    /* put max. 3 wafers into the input cassette */
	    phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_LOADED);
	    phEstateAGetWafInfo(proberID->myEstate, PHESTATE_WAFL_INCASS,
		NULL, NULL, &count);
	    for (i=count; i<3; i++)
		phEstateASetWafInfo(proberID->myEstate, PHESTATE_WAFL_INCASS,
		    PHESTATE_WAFU_LOADED, PHESTATE_WAFT_REGULAR);
#endif
	}
	else if (rv == PHPFUNC_ERR_PAT_DONE)
	{
	    /* unload input cassette */
	    phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_NOTLOADED);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateLoadCass(proberID);
}
#endif /* LDCASS_IMPLEMENTED */


#ifdef UNLCASS_IMPLEMENTED
/*****************************************************************************
 *
 * Unload an input cassette
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncUnloadCass(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncUnloadCass(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_UNLCASS);
    if (DummyOperation(proberID, "unload (input) cassette"))
    {
	/* this is dummy code, only performed during regression tests */
	int count;
	int i;
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK || rv == PHPFUNC_ERR_PAT_DONE)
	{
	    /* remove input cassette and empty output cassette */
	    phEstateASetICassInfo(proberID->myEstate, PHESTATE_CASS_NOTLOADED);
	    phEstateAGetWafInfo(proberID->myEstate, PHESTATE_WAFL_OUTCASS,
		NULL, NULL, &count);
	    for (i=0; i<count; i++)
		phEstateASetWafInfo(proberID->myEstate, PHESTATE_WAFL_OUTCASS,
		    PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateUnloadCass(proberID);
}
#endif /* UNLCASS_IMPLEMENTED */


#ifdef LDWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Load a wafer to the chuck
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncLoadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t source        /* where to load the wafer
                                           from. PHESTATE_WAFL_OUTCASS
                                           is not a valid option! */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncLoadWafer(P%p, 0x%08lx)", proberID, (long) source);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_LDWAFER);
    if (DummyOperation(proberID, "load wafer"))
    {
	/* this is dummy code, only performed during regression tests */
	phEstateCassUsage_t cassUsage;
	phEstateWafUsage_t usage;
	phEstateWafType_t type;
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    phEstateAGetICassInfo(proberID->myEstate, &cassUsage);
	    if ((cassUsage == PHESTATE_CASS_LOADED_WAFER_COUNT &&
		source == PHESTATE_WAFL_INCASS) ||
		source != PHESTATE_WAFL_INCASS)
	    {
		/* we are loading from an input cassette with known
                   number of wafers or from another location */
		phEstateAGetWafInfo(proberID->myEstate, 
		    source, &usage, &type, NULL);
		if (usage == PHESTATE_WAFU_NOTLOADED &&
		    source == PHESTATE_WAFL_INCASS)
		    return PHPFUNC_ERR_PAT_DONE;
		phEstateASetWafInfo(proberID->myEstate, 
		    source, PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
	    }
	    else
	    {
		/* we are loading from an input cassette with unknown
                   number of wafers */
		type = PHESTATE_WAFT_REGULAR;
	    }
	    phEstateASetWafInfo(proberID->myEstate, 
		PHESTATE_WAFL_CHUCK, PHESTATE_WAFU_LOADED, type);
	}
	else if (rv == PHPFUNC_ERR_PAT_DONE)
	{
	    /* ensure that no wafer is loaded */
	    phEstateASetWafInfo(proberID->myEstate, PHESTATE_WAFL_CHUCK, 
		PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateLoadWafer(proberID, source);
}
#endif /* LDWAFER_IMPLEMENTED */


#ifdef UNLWAFER_IMPLEMENTED
/*****************************************************************************
 *
 * Unload a wafer from the chuck
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncUnloadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t destination   /* where to unload the wafer
                                           to. PHESTATE_WAFL_INCASS is
                                           not valid option! */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncUnloadWafer(P%p, 0x%08lx)", proberID, (long) destination);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_UNLWAFER);
    if (DummyOperation(proberID, "unload wafer"))
    {
	/* this is dummy code, only performed during regression tests */
	phEstateWafUsage_t usage;
	phEstateWafType_t type;
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK || PHPFUNC_ERR_PAT_DONE)
	{
	    phEstateAGetWafInfo(proberID->myEstate, PHESTATE_WAFL_CHUCK, 
		&usage, &type, NULL);
	    phEstateASetWafInfo(proberID->myEstate, PHESTATE_WAFL_CHUCK, 
		PHESTATE_WAFU_NOTLOADED, PHESTATE_WAFT_UNDEFINED);
	    phEstateASetWafInfo(proberID->myEstate, destination, 
		PHESTATE_WAFU_LOADED, type);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateUnloadWafer(proberID, destination);
}
#endif /* UNLWAFER_IMPLEMENTED */


#ifdef GETDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next die
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncGetDie(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *dieX                /* in explicit probing mode
                                 (stepping_controlled_by is set to
                                 "smartest"), this is the absolute X
                                 coordinate of the current die
                                 request, as sent to the prober. In
                                 autoindex mode
                                 (stepping_controlled_by is set to
                                 "prober" or "learnlist"), the
                                 coordinate is returned in this
                                 field. Any necessary transformations
                                 (index to microns, coordinate
                                 systems, etc.) must be performed by
                                 the calling framework */,
    long *dieY                /* same for Y coordinate */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncGetDie(P%p, P%p -> %ld, P%p -> %ld)", 
	proberID, dieX, dieX ? *dieX : 0L, dieY, dieY ? *dieY : 0L);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_GETDIE);
    if (DummyOperation(proberID, "get die(s)"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;
	phEstateSiteUsage_t *oldPopulation;
	int entries;
	phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
	int i;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    phEstateAGetSiteInfo(proberID->myEstate, &oldPopulation, &entries);
	    for (i=0; i<proberID->noOfSites; i++)
	    {
		if (oldPopulation[i] == PHESTATE_SITE_EMPTY)
		    population[i] = PHESTATE_SITE_POPULATED;
		else
		    population[i] = oldPopulation[i];
	    }
	    phEstateASetSiteInfo(proberID->myEstate, population, proberID->noOfSites);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    if(dieX != NULL && dieY != NULL)
    {
      return privateGetDie(proberID, dieX, dieY);
    }
    else
    {
      return PHPFUNC_ERR_FATAL;
    }
}
#endif /* GETDIE_IMPLEMENTED */


#ifdef BINDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested die
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncBinDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL, if
                                sub-die probing is active */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis or
                                NULL, if sub-die probing is active */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncBinDie(P%p, %s, %s)", proberID, 
	writeLongList(proberID->noOfSites, perSiteBinMap),
	writeLongList(proberID->noOfSites, perSitePassed));
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_BINDIE);
    if (DummyOperation(proberID, "bin die(s)"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;
	phEstateSiteUsage_t *oldPopulation;
	int entries;
	phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
	int i;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    phEstateAGetSiteInfo(proberID->myEstate, &oldPopulation, &entries);
	    for (i=0; i<proberID->noOfSites; i++)
	    {
		switch (oldPopulation[i])
		{
		  case PHESTATE_SITE_POPULATED:
		  case PHESTATE_SITE_EMPTY:
		  case PHESTATE_SITE_POPREPROBED:
		    population[i] = PHESTATE_SITE_EMPTY;
		    break;
		  case PHESTATE_SITE_DEACTIVATED:
		  case PHESTATE_SITE_POPDEACT:
		    population[i] = PHESTATE_SITE_DEACTIVATED;
		    break;
		}
	    }
	    phEstateASetSiteInfo(proberID->myEstate, population, proberID->noOfSites);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateBinDie(proberID, perSiteBinMap, perSitePassed);
}
#endif /* BINDIE_IMPLEMENTED */


#ifdef GETSUBDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next sub-die
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncGetSubDie(
    phPFuncId_t proberID      /* driver plugin ID */,
    long *subdieX             /* in explicit probing mode
                                 (stepping_controlled_by is set to
                                 "smartest"), this is the relative X
                                 coordinate of the current sub die
                                 request, as sent to the prober. In
                                 autoindex mode
                                 (stepping_controlled_by is set to
                                 "prober" or "learnlist"), the
                                 coordinate is returned in this
                                 field. Any necessary transformations
                                 (index to microns, coordinate
                                 systems) must be performed by the
                                 calling framework */,
    long *subdieY             /* same for Y coordinate */
)
{
     /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncGetSubDie(P%p, P%p -> %ld, P%p -> %ld)", proberID, 
	subdieX, subdieX ? *subdieX : 0L, subdieY, subdieY ? *subdieY : 0L);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_GETSUBDIE);
    if (DummyOperation(proberID, "get sub-die(s)"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;
	phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
	int i;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    for (i=0; i<proberID->noOfSites; i++)
		population[i] = PHESTATE_SITE_POPULATED;
	    phEstateASetSiteInfo(proberID->myEstate, population, proberID->noOfSites);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    if(subdieX != NULL && subdieY != NULL)
    {
      return privateGetSubDie(proberID, subdieX, subdieY);
    }
    else
    {
      return PHPFUNC_ERR_FATAL;
    }
}
#endif /* GETSUBDIE_IMPLEMENTED */


#ifdef BINSUBDIE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested sub-dice
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncBinSubDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncBinSubDie(P%p, %s, %s)", proberID, 
	writeLongList(proberID->noOfSites, perSiteBinMap),
	writeLongList(proberID->noOfSites, perSitePassed));
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_BINSUBDIE);
    if (DummyOperation(proberID, "bin sub-die(s)"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;
	phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
	int i;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    for (i=0; i<proberID->noOfSites; i++)
		population[i] = PHESTATE_SITE_EMPTY;
	    phEstateASetSiteInfo(proberID->myEstate, population, proberID->noOfSites);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateBinSubDie(proberID, perSiteBinMap, perSitePassed);
}
#endif /* BINSUBDIE_IMPLEMENTED */


#ifdef DIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncDieList(
    phPFuncId_t proberID      /* driver plugin ID */,
    int count                 /* number of entries in <dieX> and <dieY> */,
    long *dieX                /* this is the list of absolute X
                                 coordinates of the die learnlist as
                                 to be sent to the prober. Any
                                 necessary transformations (index to
                                 microns, coordinate systems, etc.)
                                 must be performed by the calling
                                 framework */,
    long *dieY                 /* same for Y coordinate */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncDieList(P%p, %d, %s, %s)", proberID, count, 
	writeLongList(count, dieX), writeLongList(count, dieY));
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_DIELIST);
    if (DummyOperation(proberID, "define die learnlist"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateDieList(proberID, count, dieX, dieY);
}
#endif /* DIELIST_IMPLEMENTED */


#ifdef SUBDIELIST_IMPLEMENTED
/*****************************************************************************
 *
 * Send sub-die learnlist to prober
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncSubDieList(
    phPFuncId_t proberID      /* driver plugin ID */,
    int count                 /* number of entries in <dieX> and <dieY> */,
    long *subdieX             /* this is the list of relative X
                                 coordinates of the sub-die learnlist as
                                 to be sent to the prober. Any
                                 necessary transformations (index to
                                 microns, coordinate systems, etc.)
                                 must be performed by the calling
                                 framework */,
    long *subdieY             /* same for Y coordinate */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncSubDieList(P%p, %d, %s, %s)", proberID, count,
	writeLongList(count, subdieX), writeLongList(count, subdieY));
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_SUBDIELIST);
    if (DummyOperation(proberID, "define sub-die learnlist"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateSubDieList(proberID, count, subdieX, subdieY);
}
#endif /* SUBDIELIST_IMPLEMENTED */


#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe dice
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncReprobe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncReprobe(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_REPROBE);
    if (DummyOperation(proberID, "reprobe"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateReprobe(proberID);
}
#endif /* REPROBE_IMPLEMENTED */


#ifdef CLEAN_IMPLEMENTED
/*****************************************************************************
 *
 * Clean the probe needles
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncCleanProbe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncCleanProbe(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_CLEAN);
    if (DummyOperation(proberID, "clean probe"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateCleanProbe(proberID);
}
#endif /* CLEAN_IMPLEMENTED */


#ifdef PMI_IMPLEMENTED
/*****************************************************************************
 *
 * Perform a probe mark inspection
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncPMI(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncPMI(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_PMI);
    if (DummyOperation(proberID, "perform PMI"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privatePMI(proberID);
}
#endif /* PMI_IMPLEMENTED */


#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncSTPaused(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncSTPaused(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_PAUSE);
    if (DummyOperation(proberID, "notify SmarTest pause started"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    phEstateASetPauseInfo(proberID->myEstate, 1);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateSTPaused(proberID);
}
#endif /* PAUSE_IMPLEMENTED */


#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to prober plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncSTUnpaused(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncSTUnpaused(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_UNPAUSE);
    if (DummyOperation(proberID, "notify SmarTest pause done"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    phEstateASetPauseInfo(proberID->myEstate, 0);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateSTUnpaused(proberID);
}
#endif /* UNPAUSE_IMPLEMENTED */


#ifdef TEST_IMPLEMENTED
/*****************************************************************************
 *
 * send a command string to the prober
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncTestCommand(
    phPFuncId_t proberID     /* driver plugin ID */,
    char *command            /* command string */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncTestCommand(P%p, \"%s\")", proberID, 
	command ? command : "(NULL)");
    CheckHandle(proberID);
    RememberSpecialCall(proberID, PHPFUNC_AV_TEST);
    if (DummyOperation(proberID, "test command"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateTestCommand(proberID, command);
}
#endif /* TEST_IMPLEMENTED */


#ifdef DIAG_IMPLEMENTED
/*****************************************************************************
 *
 * retrieve diagnostic information
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncDiag(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **diag              /* pointer to probers diagnostic output */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncDiag(P%p, P%p)", proberID, diag);
    CheckHandle(proberID);
    RememberSpecialCall(proberID, PHPFUNC_AV_DIAG);
    if (DummyOperation(proberID, "get diagnostics"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateDiag(proberID, diag);
}
#endif /* DIAG_IMPLEMENTED */


#ifdef STATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncStatus(
    phPFuncId_t proberID                /* driver plugin ID */,
    phPFuncStatRequest_t action         /* the current status action
                                           to perform */,
    phPFuncAvailability_t *lastCall     /* the last call to the
                                           plugin, not counting calls
                                           to this function */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncStatus(P%p, 0x%08lx, P%p)", proberID, (long) action, lastCall);
    CheckHandle(proberID);
    /* don't remember call to the status function */
    /* RememberSpecialCall(proberID, PHPFUNC_AV_STATUS); */

    if (DummyOperation(proberID, "get/set driver plugin status"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateStatus(proberID, action, lastCall);
}
#endif /* STATUS_IMPLEMENTED */


#ifdef UPDATE_IMPLEMENTED
/*****************************************************************************
 *
 * Current status of plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncUpdateState(
    phPFuncId_t proberID                /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncUpdateState(P%p)", proberID);
    CheckHandle(proberID);
    RememberSpecialCall(proberID, PHPFUNC_AV_UPDATE);
    if (DummyOperation(proberID, "update driver plugin and equipment state"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateUpdateState(proberID);
}
#endif /* UPDATE_IMPLEMENTED */


#ifdef CASSID_IMPLEMENTED
/*****************************************************************************
 *
 * return the cassette identification string
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncCassID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **cassIdString      /* resulting pointer to cassette ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncCassID(P%p)", proberID, cassIdString);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_CASSID);
    if (DummyOperation(proberID, "get cassette ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy cassette ID string");
	    *cassIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateCassID(proberID, cassIdString);
}
#endif /* CASSID_IMPLEMENTED */


#ifdef WAFID_IMPLEMENTED
/*****************************************************************************
 *
 * return the wafer identification string
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncWafID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **wafIdString       /* resulting pointer to wafer ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncWafID(P%p)", proberID, wafIdString);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_WAFID);
    if (DummyOperation(proberID, "get wafer ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy wafer ID string");
	    *wafIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateWafID(proberID, wafIdString);
}
#endif /* WAFID_IMPLEMENTED */


#ifdef PROBEID_IMPLEMENTED
/*****************************************************************************
 *
 * return the probe card identification string
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncProbeID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **probeIdString     /* resulting pointer to probe ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncProbeID(P%p)", proberID, probeIdString);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_PROBEID);
    if (DummyOperation(proberID, "get probe card ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy probe card ID string");
	    *probeIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateProbeID(proberID, probeIdString);
}
#endif /* PROBEID_IMPLEMENTED */


#ifdef LOTID_IMPLEMENTED
/*****************************************************************************
 *
 * return the lot identification string
 *
 * Authors: Michael Vogt
 *
 * History: 31 March 2000, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncLotID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **lotIdString       /* resulting pointer to lot ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncLotID(P%p)", proberID, lotIdString);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_LOTID);
    if (DummyOperation(proberID, "get lot card ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy lot ID string");
	    *lotIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateLotID(proberID, lotIdString);
}
#endif /* LOTID_IMPLEMENTED */


#ifdef STARTLOT_IMPLEMENTED
/*****************************************************************************
 *
 * Start a lot
 *
 * Authors: Michael Vogt
 *
 * History: 31 March 2000, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncStartLot(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncStartLot(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_STARTLOT);
    if (DummyOperation(proberID, "start lot"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	    /* start a lot */
	    phEstateASetLotInfo(proberID->myEstate, 1);
	}
	else if (rv == PHPFUNC_ERR_PAT_DONE)
	{
	    /* ensure that lot is stopped */
	    phEstateASetLotInfo(proberID->myEstate, 0);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateStartLot(proberID);
}
#endif /* STARTLOT_IMPLEMENTED */


#ifdef ENDLOT_IMPLEMENTED
/*****************************************************************************
 *
 * End a lot
 *
 * Authors: Michael Vogt
 *
 * History: 31 March 2000, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncEndLot(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncEndLot(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_ENDLOT);
    if (DummyOperation(proberID, "end lot"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK || PHPFUNC_ERR_PAT_DONE)
	{
	    /* remove input cassette and empty output cassette */
	    phEstateASetLotInfo(proberID->myEstate, 0);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateEndLot(proberID);
}
#endif /* ENDLOT_IMPLEMENTED */


#ifdef COMMTEST_IMPLEMENTED
/*****************************************************************************
 *
 * Communication Test 
 *
 * Authors: Michael Vogt
 *
 * History: 31 Oct 2000, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncCommTest(
    phPFuncId_t proberID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncCommTest(P%p, P%p)", proberID, testPassed);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_COMMTEST);
    if (DummyOperation(proberID, "communication test"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK || PHPFUNC_ERR_PAT_DONE)
	{
	    /* assume passed */
	    if (testPassed)
		*testPassed = 1;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateCommTest(proberID, testPassed);
}
#endif /* COMMTEST_IMPLEMENTED */


#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_pfunc.h
 *
 ***************************************************************************/
phPFuncError_t phPFuncDestroy(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phPFuncDestroy(P%p)", proberID);
    CheckHandle(proberID);
    RememberCall(proberID, PHPFUNC_AV_DESTROY);
    if (DummyOperation(proberID, "destroy prober driver plugin"))
    {
	/* this is dummy code, only performed during regression tests */
	phPFuncError_t rv;

	rv = DummyReturnValue(proberID);
	if (rv == PHPFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateDestroy(proberID);
}
#endif /* DESTROY_IMPLEMENTED */

/*****************************************************************************
 * End of file
 *****************************************************************************/
