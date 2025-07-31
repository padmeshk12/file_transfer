/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_hfunc.c
 * CREATED  : 15 Jul 1999
 *
 * CONTENTS : Handler Driver Plugin for <handler family>
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR27090&CR27346
 *            Jiawei Lin, R&D Shanghai, CR38119
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Jul 1999, Michael Vogt, created
 *            09 Jun 2000, Michael Vogt, heavily changed to be more prober
 *                                       driver like
 *            Nov 2005, Jiawei Lin, CR27090&CR27346
 *              support the following query commands for Delta Flex and
 *              Castle handler:
 *                 sm?
 *                 temperature query like soaktime, lowerguard, setpoint
 *
 *            Jan 2008, Jiawei Lin, CR38119
 *              refactor the "phFuncGetStatus" and "phFuncSetStatus" to support
 *              the Mirae M660 model
 *
 *            19 Mar 2009, Roger Feng
 *              Fix the parameter parser in "phFuncSetStatus" to get multiple 
 *              parameters; trim the leading and trailing delimiters in parameters
 *
 *            30 Nov 2009, Xiaofei Han, CR33707/CR42623
 *              Add a generic parameter for the temperature related status function
 *            7 May 2015, Magco Li, CR93846
 *              add rs232 interface
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

#define _PH_HFUNC_INTERNAL_

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_tcom.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_hfunc.h"

#include "ph_keys.h"
#include "gpib_conf.h"
#include "ph_hfunc_private.h"
#include "identity.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- customization defines -------------------------------------------------*/

/* only perform dummy operations if configured with
   "plugin_simulation" and return immediately, put into comments if
   simulation mode should be prohibited */
#define PH_DUMMY_OPERATION

/* don't trust anyone, always check for a correct handles */
#define PH_HANDLE_CHECK

/*--- defines (don't change) ------------------------------------------------*/

#define MAX_ID_STR_LEN 512
#define MAX_DIAG_STR_LEN 512

/* check the handles, if in DEBUG mode */
#ifdef DEBUG
#define PH_HANDLE_CHECK
#endif

/* definition of the handle check */
#ifdef PH_HANDLE_CHECK
#define CheckInitialized() \
    if (!myLogger) \
        return PHFUNC_ERR_NOT_INIT

#define CheckHandle(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHFUNC_ERR_INVALID_HDL
#else
#define CheckInitialized()
#define CheckHandle(x)
#endif

/* definition of the dummy operation */
#ifdef PH_DUMMY_OPERATION
#define DummyOperation(p,x) \
    ((((p)->simulationMode) && \
    ( \
        phLogFuncMessage(myLogger, LOG_NORMAL, \
            "simulated operation: " x), \
        1 \
    )) || \
    (((p)->offlineMode) && \
    ( \
        phLogFuncMessage(myLogger, LOG_NORMAL, \
            "offline operation: " x), \
        1 \
    )))
#else
#define DummyOperation(p,x) 0
#endif


#ifdef PH_DUMMY_OPERATION
#define DummyReturnValue(p) \
    ((dummyReturnValues && \
        dummyReturnIndex < dummyReturnCount) ? \
	dummyReturnValues[dummyReturnIndex++] : \
        PHFUNC_ERR_OK)
#else
#define DummyReturnValue(p) PHFUNC_ERR_OK
#endif

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

/* Needed because the logger is only passed once during init to this
module but not during all. Is not stored in the state handle
structure, since it is then impossible to log invalid handle access to
this module */

static phLogId_t myLogger = NULL;

static phFuncError_t *dummyReturnValues = NULL;
static int dummyReturnCount = 0;
static int dummyReturnIndex = 0;

/*--- functions -------------------------------------------------------------*/

void phFuncSimResults(
    phFuncError_t *returnSequence, 
    int count)
{
    dummyReturnValues = returnSequence;
    dummyReturnCount = count;
    dummyReturnIndex = 0;
}

static void RememberCall(phFuncId_t driverID,  phFuncAvailability_t call)
{
    switch (driverID->interfaceType)
    {
      case PHFUNC_IF_GPIB:
      case PHFUNC_IF_LAN:
      case PHFUNC_IF_LAN_SERVER:
	phFuncTaSetCall(driverID, call);
	break;
      case PHFUNC_IF_RS232:
	break;
      default:
    break;
    }    
}

#if defined(DIAG_IMPLEMENTED) || defined(UPDATE_IMPLEMENTED) || defined(COMMTEST_IMPLEMENTED)

static void RememberSpecialCall(phFuncId_t driverID,  phFuncAvailability_t call)
{
    switch (driverID->interfaceType)
    {
      case PHFUNC_IF_GPIB:
      case PHFUNC_IF_LAN:
      case PHFUNC_IF_LAN_SERVER:
	phFuncTaSetSpecialCall(driverID, call);
	break;
      case PHFUNC_IF_RS232:
	break;
      default:
    break;
    }    
}

#endif

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

static int phFuncRS232Reconfigure(struct phFuncStruct *myself)
{
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
	"reconfigureRS232(P%p)", myself);

    /*phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
	"reconfigureRS232() not yet implemented");*/

    /* to be defined */
    return 1;
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
static int reconfigureGlobal(struct phFuncStruct *myself)
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
    const char *sOfflineMode;
    const char *szWaitingForCommandReply = NULL;

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

    /* for handler family specific driver plugins, check the
       correctness of the new site setting here. It must match the
       real number of sites available at the handler ! */

    /* ... */

    /* GPIB command reply flag, if the flag is configured as "no", then it will execute the GPIB command without waiting for the reply, the default value is set */
    myself->waitingForCommandReply = 1;
    if (phConfConfIfDef(myself->myConf, PHKEY_MO_REPLY_OF_CMD) == PHCONF_ERR_OK)
    {
        confError = phConfConfString(myself->myConf, PHKEY_MO_REPLY_OF_CMD, 0, NULL, &szWaitingForCommandReply);
        if (confError == PHCONF_ERR_OK && strcasecmp(szWaitingForCommandReply, "no") == 0)
        {
            phLogFuncMessage(myself->myLogger, PHLOG_TYPE_TRACE, "configuration \"%s\" is set off", PHKEY_MO_REPLY_OF_CMD);
            myself->waitingForCommandReply = 0;
        }
    }
    else
    {
        phLogFuncMessage(myself->myLogger, PHLOG_TYPE_TRACE, "configuration \"%s\" couldn't be found, will set it as default value \"yes\"", PHKEY_MO_REPLY_OF_CMD);
    }

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
		    "must be a list of handler bin names",
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
    
    ifType = NULL;
    if (phConfConfString(myself->myConf, PHKEY_IF_TYPE, 0, NULL, &ifType) != 
	PHCONF_ERR_OK)
	resultValue = 0;
    
    if (strcasecmp(ifType, "gpib") == 0)
        myself->interfaceType = PHFUNC_IF_GPIB;
    else if (strcasecmp(ifType, "lan") == 0)
        myself->interfaceType = PHFUNC_IF_LAN;
    else if (strcasecmp(ifType, "lan-server") == 0)
        myself->interfaceType = PHFUNC_IF_LAN_SERVER;
    else if (strcasecmp(ifType, "rs232") == 0)
        myself->interfaceType = PHFUNC_IF_RS232;
    else
    {
	phLogFuncMessage(myself->myLogger, PHLOG_TYPE_ERROR,
	    "this driver plugin accepts the following "
	    "interface type configurations only:\n"
	    "%s: \"%s\"\n"
	    "Ensure that this parameter is set in the driver configuration",
	    PHKEY_IF_TYPE, "gpio|gpib|lan|rs232|lan-server");
	resultValue = 0;	
    }

    /* based on the interface type additional reconfigure may be necessary */

    if (resultValue) switch (myself->interfaceType)
    {
      case PHFUNC_IF_GPIB:
	/* for GPIB based handlers, reconfigure the GPIB parameters */
	if (!phFuncGpibReconfigure(myself))
	    resultValue = 0;
	break;
      case PHFUNC_IF_RS232:
	/* for RS232 based handlers, reconfigure the RS232 parameters */
	if (!phFuncRS232Reconfigure(myself))
	    resultValue = 0;
	break;
      case PHFUNC_IF_LAN:
        break;
      case PHFUNC_IF_LAN_SERVER:
        break;
      default:
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
	    myself->simulationMode = 1;
    }

    /* check whether we want to operate in offline mode */

    if (phConfConfIfDef(myself->myConf, PHKEY_MO_ONLOFF) == PHCONF_ERR_OK)
    {
	confError = phConfConfString(myself->myConf, PHKEY_MO_ONLOFF, 
	    0, NULL, &sOfflineMode);
	if (confError == PHCONF_ERR_OK && 
	    strcasecmp(sOfflineMode, "off-line") == 0)    
	    myself->offlineMode = 1;
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
phFuncAvailability_t phFuncAvailable(void)
{
    //phFuncAvailability_t retval = (phFuncAvailability_t) 0;
    unsigned int retval = 0;

#ifdef INIT_IMPLEMENTED
    retval |= PHFUNC_AV_INIT;
#endif
#ifdef RECONFIGURE_IMPLEMENTED
    retval |= PHFUNC_AV_RECONFIGURE;
#endif
#ifdef RESET_IMPLEMENTED
    retval |= PHFUNC_AV_RESET;
#endif
#ifdef DRIVERID_IMPLEMENTED
    retval |= PHFUNC_AV_DRIVERID;
#endif
#ifdef EQUIPID_IMPLEMENTED
    retval |= PHFUNC_AV_EQUIPID;
#endif
/* kaw start Oct 14 2004 */
#ifdef STRIPINDEXID_IMPLEMENTED
    retval |= PHFUNC_AV_STRIP_INDEXID;
#endif
#ifdef STRIPMATERIALID_IMPLEMENTED
    retval |= PHFUNC_AV_STRIPID;
#endif
/* kaw end */
#ifdef GETSTART_IMPLEMENTED
    retval |= PHFUNC_AV_START;
#endif
#ifdef BINDEVICE_IMPLEMENTED
    retval |= PHFUNC_AV_BIN;
#endif
#ifdef REPROBE_IMPLEMENTED
    retval |= PHFUNC_AV_REPROBE;
#endif
#ifdef BINREPROBE_IMPLEMENTED
    retval |= PHFUNC_AV_BINREPR;
#endif
#ifdef PAUSE_IMPLEMENTED
    retval |= PHFUNC_AV_PAUSE;
#endif
#ifdef UNPAUSE_IMPLEMENTED
    retval |= PHFUNC_AV_UNPAUSE;
#endif
#ifdef DIAG_IMPLEMENTED
    retval |= PHFUNC_AV_DIAG;
#endif
#ifdef STATUS_IMPLEMENTED
    retval |= PHFUNC_AV_STATUS;
#endif
#ifdef UPDATE_IMPLEMENTED
    retval |= PHFUNC_AV_UPDATE;
#endif
#ifdef COMMTEST_IMPLEMENTED
    retval |= PHFUNC_AV_COMMTEST;
#endif
#ifdef DESTROY_IMPLEMENTED
    retval |= PHFUNC_AV_DESTROY;
#endif
/* kaw start Feb 07 2005 - Mirae */
#ifdef SETSTATUS_IMPLEMENTED
    retval |= PHFUNC_AV_SET_STATUS;
#endif
#ifdef GETSTATUS_IMPLEMENTED
    retval |= PHFUNC_AV_GET_STATUS;
#endif
/* kaw end Feb 07 2005 - Mirae */
/* kaw start Feb 23 2005 - Mirae */
#ifdef LOTSTART_IMPLEMENTED
    retval |= PHFUNC_AV_LOT_START;
#endif
#ifdef LOTDONE_IMPLEMENTED
    retval |= PHFUNC_AV_LOT_DONE;
#endif
/* kaw end Feb 23 2005 - Mirae */

    return (phFuncAvailability_t)retval;
}


#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncInit(
    phFuncId_t *driverID     /* the resulting driver plugin ID */,
    phTcomId_t tester        /* the tester/tcom session to be used */,
    phComId_t communication  /* the valid communication link to the HW
			        interface used by the handler */,
    phLogId_t logger         /* the data logger to be used */,
    phConfId_t configuration /* the configuration manager */,
    phEstateId_t estate      /* the driver state */
)
{
    static int isConfigured = 0;
    static struct phFuncStruct *tmpId = NULL;

    phBinMode_t tmpBinMode = PHBIN_MODE_DEFAULT;;

    char revision[1024];
    const char *handlerFamily = "";
    const char *handlerModel = "";
    char *family;
    struct modelDef *models;
    int i;
   
    const char *testBinMode=NULL;

    /* use the logger and all other modules... */
    if (tester && communication && logger && configuration && estate)
    {
	/* if we got a valid logger, use it and trace the call
	   to this module */
	myLogger = logger;

	phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
	    "phFuncInit(P%p, P%p, P%p, P%p)", driverID, communication, 
	    myLogger, logger, configuration, estate);
    }
    else
	/* passed logger is not valid at all, give up */
	return PHFUNC_ERR_NOT_INIT;

    /* only setup everything, if this is the first call. It could be
       that the private initialization returns with a WAITING error
       and we are called again. In this case, we do not re initialize
       everything ... */
    if (!isConfigured)
    {
	/* allocate state data type */
	tmpId = PhNew(struct phFuncStruct);
	if (tmpId == 0)
	{
	    phLogFuncMessage(myLogger, PHLOG_TYPE_FATAL,
		"not enough memory during call to phFuncInit()");
	    return PHFUNC_ERR_MEMORY;
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
	tmpId->offlineMode = 0;

	/* do more initialization here.... */
	tmpId->noOfSites = 0;
	tmpId->noOfBins = 0;
	tmpId->binIds = NULL;
    tmpId->waitingForCommandReply = 1;

	phFuncTaInit(tmpId);

	/* identify myself */
	phConfConfString(configuration, PHKEY_IN_HFAM, 0, NULL, &handlerFamily);
	phConfConfString(configuration, PHKEY_IN_MODEL, 0, NULL, &handlerModel);
        phConfConfString(configuration, PHKEY_BI_MODE, 0, NULL, &testBinMode);
        /* get the binmode from phFramestruct */
        //phConfConfString(configuration, PHKEY_BI_MODE, 0, NULL, &defBinMode);
        /* end */  
	sprintf(revision, "%s", PHFUNC_IDENT " " PHFUNC_REVISION);
	#ifdef PHFUNC_LOCAL
	    strcat(revision, " (local version ");
	    strcat(revision, PHFUNC_LOCAL);
	    strcat(revision, ")");
	#endif
	phLogFuncMessage(myLogger, PHLOG_TYPE_MESSAGE_0,
	    "\n\nHandler driver identification: \"%s\" compiled at %s\n"
	    "%s: \"%s\", %s: \"%s\" (from configuration)\n", 
	    revision, PHFUNC_DATE,
	    PHKEY_IN_HFAM, handlerFamily, PHKEY_IN_MODEL, handlerModel);
        
	/* get the BinMode*/
        /* accept and store the binning mode */
        if (strcasecmp(testBinMode, "default") == 0)
	tmpBinMode = PHBIN_MODE_DEFAULT;
        else if (strcasecmp(testBinMode, "mapped-hardbins") == 0)
	tmpBinMode = PHBIN_MODE_HARDMAP;
        else if (strcasecmp(testBinMode, "mapped-softbins") == 0)
	tmpBinMode = PHBIN_MODE_SOFTMAP;
        else 
	return PHFUNC_ERR_NOT_INIT;
        tmpId->currentBinMode=tmpBinMode;

	/* check whether the configured handler model fits to this plugin */
	phFuncPredefModels(&family, &models);
	if (strcmp(family, handlerFamily) != 0)
	{
	    phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
		"The given handler family \"%s\" is not\n"
		"valid for this handler driver plugin. Expected: \"%s\"", 
		handlerFamily, family);
	    return PHFUNC_ERR_CONFIG;
	}
	i = 0;
	while (models[i].model != PHFUNC_MOD__END &&
	    strcmp(models[i].name, "*") != 0 &&
	    strcmp(models[i].name, handlerModel) != 0)
	    i++;
	if (models[i].model == PHFUNC_MOD__END)
	{
	    /* not found in supported model list */
	    phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
		"The given handler model \"%s\" is not\n"
		"supported by this handler driver plugin", handlerModel);
	    return PHFUNC_ERR_CONFIG;
	}
	tmpId->model = models[i].model;
	tmpId->cmodelName = strdup(handlerModel);
        //tmpId->currentBinMode = strdup(defBinMode);
	/* perform the configuration check and configure */
	if (!reconfigureGlobal(tmpId))
	{
	    phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
		"driver plugin could not be initialized\n"
		"due to errors in configuration");
	    return PHFUNC_ERR_CONFIG;
	}

	isConfigured = 1;
     }

	/* return the id for the initialized driver */
	*driverID = tmpId;

	RememberCall(tmpId, PHFUNC_AV_INIT);
	/* stop here, if working in simulation mode */
	if (DummyOperation(tmpId, "initialize handler driver plugin"))
	{
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
	}

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
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncReconfigure(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncReconfigure(P%p)", handlerID);
    CheckHandle(handlerID);

    /* before doing dummy stuff, ensure that we receive the changed
       configuration */
    if (!reconfigureGlobal(handlerID))
    {
	phLogFuncMessage(myLogger, PHLOG_TYPE_ERROR,
	    "driver plugin could not be fully (re)configured\n"
	    "due to errors in configuration\n"
	    "Driver plugin currently in inconsistent state !!!");
	return PHFUNC_ERR_CONFIG;
    }

    RememberCall(handlerID, PHFUNC_AV_RECONFIGURE);
    if (DummyOperation(handlerID, "reconfigure handler driver plugin"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateReconfigure(handlerID);
}
#endif /* RECONFIGURE_IMPLEMENTED */

#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the handler
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncReset(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncReset(P%p)", handlerID);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_RESET);
    if (DummyOperation(handlerID, "reset handler driver plugin"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateReset(handlerID);
}
#endif /* RESET_IMPLEMENTED */

#ifdef DRIVERID_IMPLEMENTED
/*****************************************************************************
 *
 * return the driver identification string
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncDriverID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";
    char revision[1024];
    const char *handlerFamily = "";
    const char *handlerModel = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncDriverID(P%p, P%p)", handlerID, driverIdString);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_DRIVERID);

    /* assemble the plugin ID string
       This can also be done in simulation or offline mode */
    phConfConfString(handlerID->myConf, PHKEY_IN_HFAM, 
	0, NULL, &handlerFamily);
    phConfConfString(handlerID->myConf, PHKEY_IN_MODEL, 
	0, NULL, &handlerModel);
    sprintf(revision, "%s", PHFUNC_IDENT " " PHFUNC_REVISION);
#ifdef PHFUNC_LOCAL
    strcat(revision, " (local version ");
    strcat(revision, PHFUNC_LOCAL);
    strcat(revision, ")");
#endif
    sprintf(theIDString, 
	"\"%s\" compiled at %s, "
	"%s: \"%s\", %s: \"%s\" (from configuration)", 
        revision, PHFUNC_DATE,
	PHKEY_IN_HFAM, handlerFamily, PHKEY_IN_MODEL, handlerModel);
    *driverIdString = theIDString;

    if (DummyOperation(handlerID, "get plugin ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    if (handlerID->simulationMode)
		strcat(theIDString, " (running in simulation mode)");
	    if (handlerID->offlineMode)
		strcat(theIDString, " (running in offline mode)");
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateDriverID(handlerID, driverIdString);
}
#endif /* DRIVERID_IMPLEMENTED */

#ifdef EQUIPID_IMPLEMENTED
/*****************************************************************************
 *
 * return the handler identification string
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncEquipID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncEquipID(P%p, P%p)", handlerID, equipIdString);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_EQUIPID);
    if (DummyOperation(handlerID, "get handler ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy equipment ID string");
	    if (handlerID->simulationMode)
		strcat(theIDString, " (running in simulation mode)");
	    if (handlerID->offlineMode)
		strcat(theIDString, " (running in offline mode)");
	    *equipIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateEquipID(handlerID, equipIdString);
}
#endif /* EQUIPID_IMPLEMENTED */

/* kaw start Oct 14 2004 */
#ifdef STRIPINDEXID_IMPLEMENTED
/*****************************************************************************
 *
 * return the handler identification string
 *
 * Authors: Michael Vogt, Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          14 Oct 2004, Ken Ward - added commands for Delta Orion
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncStripIndexID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncStripIndexID(P%p, P%p)", handlerID, equipIdString);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_STRIP_INDEXID);
    if (DummyOperation(handlerID, "get handler strip index ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy strip index ID string");
	    if (handlerID->simulationMode)
		strcat(theIDString, " (running in simulation mode)");
	    if (handlerID->offlineMode)
		strcat(theIDString, " (running in offline mode)");
	    *equipIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateStripIndexID(handlerID, equipIdString);
}
#endif /* STRIPINDEXID_IMPLEMENTED */


#ifdef STRIPMATERIALID_IMPLEMENTED
/*****************************************************************************
 *
 * return the handler identification string
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          14 Oct 2004, Ken Ward - added commands for Delta Orion
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncStripMaterialID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    static char theIDString[MAX_ID_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncStripMaterialID(P%p, P%p)", handlerID, equipIdString);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_STRIPID);
    if (DummyOperation(handlerID, "get handler strip material ID"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    strcpy(theIDString, "dummy strip material ID string");
	    if (handlerID->simulationMode)
		strcat(theIDString, " (running in simulation mode)");
	    if (handlerID->offlineMode)
		strcat(theIDString, " (running in offline mode)");
	    *equipIdString = theIDString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateStripMaterialID(handlerID, equipIdString);
}
#endif /* STRIPMATERIALID_IMPLEMENTED */
/* kaw end */


/* kaw start Feb 07 2005 */

#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Get status information from handler
 *
 * Authors: Michael Vogt, Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          07 Feb 2005, Ken Ward - added commands for Mirae
 *          Jan 2008, Jiawei Lin - refactor this function
 *          30 Nov 2009, Xiaofei Han - allow the user to pass a configurable
 *                                     parameter to temperature related status
 *                                     query command. CR33707/CR42623
 *
 * Description:
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncGetStatus(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *commandString      /* pointer to status to get */,
    char **responseString     /* pointer to the response string */
)
{
    static char response[MAX_STATUS_MSG] = "";
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char input[MAX_STATUS_MSG];
    char *token = NULL;
    const char *parms = NULL;
    char *answer = NULL;
    phConfError_t confError;
    char *configuredZone = NULL;
    const char *configuredZoneValue = NULL;
    char newParamString[MAX_STATUS_MSG] = "";
    char tempString[MAX_STATUS_MSG] = "";
    int aindex = 0;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
                     "phFuncGetStatus(P%p, P%p)", handlerID, commandString);
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
                     "phFuncGetStatus(P%p, %s)", handlerID, commandString);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_GET_STATUS);

    if (DummyOperation(handlerID, "get handler status"))
    {
      /* this is dummy code, only performed during regression tests */
      phFuncError_t rv;

      rv = DummyReturnValue(handlerID);
      return rv;
      /* end of dummy code */
    }

    /*phFuncTaStart(handlerID);*/
    strncpy(input, commandString, MAX_STATUS_MSG-1);
    token = strtok(input, " \t\n\r");

    if ( token )
    {
       phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
                         "phFuncGetStatus, token = ->%s<-", token);

        parms = strtok(NULL, " \t\n\r");
        if( parms == NULL ) {
          parms = "";
        }
        if( strlen(parms) > 0 ) {
          phLogFuncMessage(myLogger, PHLOG_TYPE_MESSAGE_0,
                           "phFuncGetStatus, key ->%s<-, param ->%s<-",
                           token, parms);
          strcpy(tempString, parms);
        } else {
          phLogFuncMessage(myLogger, PHLOG_TYPE_MESSAGE_0,
                           "phFuncGetStatus, key ->%s<-, No Param", token);
        }

        /*
         * CR33707/CR42623, Xiaofei Han, 12/1/2009
         * if the status query is temperature related and check if the zone specified is 
         * configured in the configure file. If so, replace the specified parameter 
         * with the configured value.
         */
        if(strstr(token, "temperature_") != NULL && strlen(tempString) != 0)
        {
          configuredZone = strtok(tempString, " \t\n\r");
          confError = phConfConfIfDef(handlerID->myConf, configuredZone);
          if (confError == PHCONF_ERR_OK)
          {
            confError = phConfConfString(handlerID->myConf, configuredZone, 0, NULL, &configuredZoneValue);
          }

          if(confError == PHCONF_ERR_OK)
          {
            phLogFuncMessage(myLogger, PHLOG_TYPE_MESSAGE_0, "phFuncGetStatus, %s is defined as ->%s<-", 
                             configuredZone, configuredZoneValue);

            /* replace the parameter with the value configured in the config file */
            strncpy(newParamString, parms, strstr(parms, configuredZone)-parms);  
            strcat(newParamString, configuredZoneValue);
            aindex = (strstr(parms, configuredZone)-parms)+strlen(configuredZone);
            if(parms[aindex] != '\0')
            {
              parms = &parms[aindex]; 
              strcat(newParamString, parms);
            }
            parms = newParamString;
          }
        }

        /*
         * for all "private.c" of plugins, they should implement the function
         * "privateGetStatus"
         *
         * the token is the key of query type, and param is its parameter;
         * the param might be empty (NULL)
         * For example:
         *    for the call PROB_HND_CALL(ph_get_status masstemp zone1),
         *    the token is "masstemp", the param is "zone1"
         */
        retVal = privateGetStatus(handlerID, token, parms, &answer);
    }
    else
    {
        retVal = PHFUNC_ERR_NA;
    }

    if(answer != NULL)
    {
      phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
                     "phFuncGetStatus, answer ->%s<-, length = %d",
                     answer, strlen(answer));
      strncpy(response, answer, MAX_STATUS_MSG-1);  // ensure no overflow
      strcat(response, "\0");  // ensure null terminated

      *responseString = response;

      phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
                       "phFuncGetStatus, response ->%s<-, length = %d",
                       *responseString, strlen(*responseString));
    }

    /*phFuncTaStop(handlerID);*/

    return retVal;
}
#endif /* GETSTATUS_IMPLEMENTED */


#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * Set status information in handler
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          07 Feb 2005, Ken Ward - added commands for Mirae
 *          30 Nov 2009, Xiaofei Han - allow user pass configured parameter
 *                                     to the temperature related status query
 *                                     CR33707/CR42623
 *
 * Description:
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncSetStatus(
                             phFuncId_t handlerID     /* driver plugin ID */,
                             char *commandString       /* pointer to status to set */
                             )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    char input[MAX_STATUS_MSG] = "";
    char localParam[MAX_STATUS_MSG] = "";
    char *token = NULL;
    char *parms = NULL;    
    phConfError_t confError;
    char *configuredZone = NULL;
    const char *configuredZoneValue = NULL;
    char newParamString[MAX_STATUS_MSG] = "";
    char tempString[MAX_STATUS_MSG] = "";
    int aindex = 0;


    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                     "phFuncSetStatus(P%p, P%p)", handlerID, commandString);
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_MESSAGE_3,
                     "phFuncSetStatus(P%p, %s)", handlerID, commandString);

    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_SET_STATUS);
    if ( DummyOperation(handlerID, "set handler status") )
    {
        /* this is dummy code, only performed during regression tests */
        phFuncError_t rv;

        rv = DummyReturnValue(handlerID);
        return rv;
        /* end of dummy code */
    }

    /* complete call in private section */

    phFuncTaStart(handlerID);

    if ( strlen ( commandString) <= MAX_STATUS_MSG ) {
      strcpy (input, commandString);
    }
    token = strtok(input, " \t\n\r");

    if ( token )
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE,
                      "phFuncSetStatus, token = ->%s<-", token);

      // get the params 
      strncpy(localParam, commandString + strlen(token), MAX_STATUS_MSG-1);
      localParam[MAX_STATUS_MSG-1] = '\0';

      phTrimLeadingTrailingDelimiter(localParam, " \t\r\n");
      parms = localParam;

      if( strlen(parms) > 0 ) {
        phLogFuncMessage(myLogger, PHLOG_TYPE_MESSAGE_0,
                        "phFuncSetStatus, key ->%s<-, param ->%s<-",
                        token, parms);
        strcpy(tempString, parms);
      } else {
        phLogFuncMessage(myLogger, PHLOG_TYPE_MESSAGE_0,
                        "phFuncSetStatus, key ->%s<-, No Param", token);
      }

      /*
       * CR33707/CR42623, Xiaofei Han, 12/1/2009
       * if the status query is temperature related, check if the parameter is configured in
       * the plugin configuration file, if so, replace the parameter with the value defined
       * in the configuration file.
       */
      if(strstr(token, "temperature_") != NULL && strlen(tempString) > 0)
      {
        configuredZone = strtok(tempString, " \t\n\r");
        confError = phConfConfIfDef(handlerID->myConf, configuredZone);
        if (confError == PHCONF_ERR_OK)
        {
          confError = phConfConfString(handlerID->myConf, configuredZone, 0, NULL, &configuredZoneValue);
        }

        if(confError == PHCONF_ERR_OK)
        {
          phLogFuncMessage(myLogger, PHLOG_TYPE_MESSAGE_0, "phFuncGetStatus, %s is defined as ->%s<-", 
                                     configuredZone, configuredZoneValue);

          /* replace parameter with the value defined in the config file */
          strncpy(newParamString, parms, strstr(parms, configuredZone)-parms);
          strcat(newParamString, configuredZoneValue);
          aindex = (strstr(parms, configuredZone)-parms)+strlen(configuredZone);
          if(parms[aindex] != '\0')
          {
            parms = &parms[aindex];
            strcat(newParamString, parms);
          }
          parms = newParamString;
        }
      }

      /*
      * for all "private.c" of plugins, they should implement the function
      * "privateSetStatus"
      *
      * the token is the key of query type, and param is its parameter;
      *
      * the param might be empty (NULL) (but in very seldom case)
      * For example:
      *    for the call PROB_HND_CALL(ph_set_status key),
      *    the token is "masstemp", the param is "zone1"
      * the common case is:
      *    PROB_HND_CALL(ph_set_status key value)
      */
      retVal = privateSetStatus(handlerID, token, parms);
    }
    else
    {
      retVal = PHFUNC_ERR_NA;
    }

    phFuncTaStop(handlerID);

    return retVal;
}
#endif /* SETSTATUS_IMPLEMENTED */

/* kaw end Feb 07 2005 */

/* kaw start Feb 23 2005 */
#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot start signal
 *
 * Authors: Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          23 Feb 2005, Ken Ward added function
 *
 * Description:
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncLotStart(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm                /* if == NOTIMEOUT, then will not return until gets an SRQ */
)
{
    int timeoutFlag = 1;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncLotStart(P%p, %s)", handlerID, parm);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_LOT_START);
    if (DummyOperation(handlerID, "get lot start"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	return rv;
	/* end of dummy code */
    }

    if ( strlen ( parm ) > 0 )
    {
        if (strcasecmp (parm, "NOTIMEOUT") == 0) timeoutFlag = 0;
    }

    /* complete call in private section */
    return privateLotStart(handlerID, timeoutFlag);
}
#endif /* LOTSTART_IMPLEMENTED */

#ifdef LOTDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot start signal
 *
 * Authors: Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          23 Feb 2005, Ken Ward added function
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncLotDone(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm
)
{
    int timeoutFlag = 1;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncLotDone(P%p, %s)", handlerID, parm);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_LOT_DONE);
    if (DummyOperation(handlerID, "get lot done"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateLotDone(handlerID, timeoutFlag);
}
#endif /* LOTDONE_IMPLEMENTED */

/* kaw emd Feb 23 2005 */

/* magco li start Oct 24 2014 */
#ifdef STRIPSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for strip start signal
 *
 * Authors: magco li
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          24 Oct 2014, Magco li added function
 *
 * Description:
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncStripStart(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm                /* if == NOTIMEOUT, then will not return until gets an SRQ */
)
{
    int timeoutFlag = 1;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
        "phFuncStripStart(P%p, %s)", handlerID, parm);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_STRIP_START);
    if (DummyOperation(handlerID, "get strip start"))
    {
        /* this is dummy code, only performed during regression tests */
        phFuncError_t rv;

        rv = DummyReturnValue(handlerID);
        return rv;
        /* end of dummy code */
    }

    if ( strlen ( parm ) > 0 )
    {
        if (strcasecmp (parm, "NOTIMEOUT") == 0) timeoutFlag = 0;
    }

    /* complete call in private section */
    return privateStripStart(handlerID, timeoutFlag);
}
#endif /* STRIPSTART_IMPLEMENTED */

#ifdef STRIPDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for strip end signal
 *
 * Authors: Ken Ward
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *          24 Oct 2014, magco li added function
 *
 * Description:
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncStripDone(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm
)
{
    int timeoutFlag = 1;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
        "phFuncStripDone(P%p, %s)", handlerID, parm);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_STRIP_DONE);
    if (DummyOperation(handlerID, "get strip done"))
    {
        /* this is dummy code, only performed during regression tests */
        phFuncError_t rv;

        rv = DummyReturnValue(handlerID);
        return rv;
        /* end of dummy code */
    }

    /* complete call in private section */
    return privateStripDone(handlerID, timeoutFlag);
}
#endif /* STRIPDONE_IMPLEMENTED */
/* magco li end Oct 24 2014 */

#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for test start signal
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncGetStart(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncGetStart(P%p)", handlerID);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_START);
    if (DummyOperation(handlerID, "get devices"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;
	phEstateSiteUsage_t *oldPopulation;
	int entries;
	phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
	int i;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);
	    for (i=0; i<handlerID->noOfSites; i++)
	    {
		if (oldPopulation[i] == PHESTATE_SITE_EMPTY)
		    population[i] = PHESTATE_SITE_POPULATED;
		else
		    population[i] = oldPopulation[i];
	    }
	    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateGetStart(handlerID);
}
#endif /* GETSTART_IMPLEMENTED */

#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested devices
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncBinDevice(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteBinMap       /* */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    CheckHandle(handlerID);
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncBinDevice(P%p, %s)", handlerID, 
	writeLongList(handlerID->noOfSites, perSiteBinMap));
    RememberCall(handlerID, PHFUNC_AV_BIN);
    if (DummyOperation(handlerID, "bin devices"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;
	phEstateSiteUsage_t *oldPopulation;
	int entries;
	phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
	int i;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);
	    for (i=0; i<handlerID->noOfSites; i++)
	    {
		switch (oldPopulation[i])
		{
		  case PHESTATE_SITE_POPULATED:
		  case PHESTATE_SITE_EMPTY:
		    population[i] = PHESTATE_SITE_EMPTY;
		    break;
		  case PHESTATE_SITE_DEACTIVATED:
		  case PHESTATE_SITE_POPDEACT:
		    population[i] = PHESTATE_SITE_DEACTIVATED;
		    break;
		}
	    }
	    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateBinDevice(handlerID, perSiteBinMap);
}
#endif /* BINDEVICE_IMPLEMENTED */

#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncReprobe(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncReprobe(P%p)", handlerID);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_REPROBE);
    if (DummyOperation(handlerID, "reprobe all"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateReprobe(handlerID);
}
#endif /* REPROBE_IMPLEMENTED */


#ifdef BINREPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncBinReprobe(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteReprobe     /* TRUE, if a device needs reprobe*/,
    long *perSiteBinMap      /* valid binning data for each site where
                                the above reprobe flag is not set */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncBinReprobe(P%p, %s, %s)", handlerID,
	writeLongList(handlerID->noOfSites, perSiteReprobe),
	writeLongList(handlerID->noOfSites, perSiteBinMap));
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_REPROBE);
    if (DummyOperation(handlerID, "bin or reprobe"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;
	phEstateSiteUsage_t *oldPopulation;
	int entries;
	phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
	int i;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);
	    for (i=0; i<handlerID->noOfSites; i++)
	    {
		if (!perSiteReprobe[i])
		    switch (oldPopulation[i])
		    {
		      case PHESTATE_SITE_POPULATED:
		      case PHESTATE_SITE_EMPTY:
			population[i] = PHESTATE_SITE_EMPTY;
			break;
		      case PHESTATE_SITE_DEACTIVATED:
		      case PHESTATE_SITE_POPDEACT:
			population[i] = PHESTATE_SITE_DEACTIVATED;
			break;
		    }
		else
		    population[i] = oldPopulation[i];
	    }
	    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateBinReprobe(handlerID, perSiteReprobe, perSiteBinMap);
}
#endif /* BINREPROBE_IMPLEMENTED */


#ifdef PAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest pause to handler plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncSTPaused(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncSTPaused(P%p)", handlerID);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_PAUSE);
    if (DummyOperation(handlerID, "notify SmarTest pause started"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    phEstateASetPauseInfo(handlerID->myEstate, 1);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateSTPaused(handlerID);
}
#endif /* PAUSE_IMPLEMENTED */


#ifdef UNPAUSE_IMPLEMENTED
/*****************************************************************************
 *
 * Notify SmarTest un-pause to handler plugin
 *
 * Authors: Michael Vogt
 *
 * History: 10 Dec 1999, Michael Vogt, generic template built
 *
 * Description:
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncSTUnpaused(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncSTUnpaused(P%p)", handlerID);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_UNPAUSE);
    if (DummyOperation(handlerID, "notify SmarTest pause done"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    phEstateASetPauseInfo(handlerID->myEstate, 0);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateSTUnpaused(handlerID);
}
#endif /* UNPAUSE_IMPLEMENTED */


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
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncDiag(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **diag              /* pointer to handlers diagnostic output */
)
{
    static char theDiagString[MAX_DIAG_STR_LEN+1] = "";

    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncDiag(P%p, P%p)", handlerID, diag);
    CheckHandle(handlerID);
    RememberSpecialCall(handlerID, PHFUNC_AV_DIAG);
    if (DummyOperation(handlerID, "get diagnostics"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    strcpy(theDiagString, "driver plugin diagnostics:\n");
	    if (handlerID->simulationMode)
		strcat(theDiagString, "running in simulation mode\n");
	    if (handlerID->offlineMode)
		strcat(theDiagString, "running in offline mode\n");
	    *diag = theDiagString;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateDiag(handlerID, diag);
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
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncStatus(
    phFuncId_t handlerID                /* driver plugin ID */,
    phFuncStatRequest_t action         /* the current status action
                                           to perform */,
    phFuncAvailability_t *lastCall     /* the last call to the
                                           plugin, not counting calls
                                           to this function */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncStatus(P%p, 0x%08lx, P%p)", handlerID, (long) action, lastCall);
    CheckHandle(handlerID);
    /* don't remember call to the status function */
    /* RememberSpecialCall(handlerID, PHFUNC_AV_STATUS); */

    if (DummyOperation(handlerID, "get/set driver plugin status"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	    if (action == PHFUNC_SR_QUERY)
		phFuncTaGetLastCall(handlerID, lastCall);
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateStatus(handlerID, action, lastCall);
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
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncUpdateState(
    phFuncId_t handlerID                /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncUpdateState(P%p)", handlerID);
    CheckHandle(handlerID);
    RememberSpecialCall(handlerID, PHFUNC_AV_UPDATE);
    if (DummyOperation(handlerID, "update driver plugin and equipment state"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateUpdateState(handlerID);
}
#endif /* UPDATE_IMPLEMENTED */

#ifdef COMMTEST_IMPLEMENTED
/*****************************************************************************
 *
 * Communication Test 
 *
 * Authors: Michael Vogt
 *
 * History: 13 Feb 2001, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncCommTest(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phFuncCommTest(P%p, P%p)", handlerID, testPassed);
    CheckHandle(handlerID);
    RememberSpecialCall(handlerID, PHFUNC_AV_COMMTEST);
    if (DummyOperation(handlerID, "communication test"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK )
	{
	    /* assume passed */
	    if (testPassed)
		*testPassed = 1;
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateCommTest(handlerID, testPassed);
}
#endif /* COMMTEST_IMPLEMENTED */


#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, generic template built
 *
 * Description: 
 * Please refer to ph_hfunc.h
 *
 ***************************************************************************/
phFuncError_t phFuncDestroy(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogFuncMessage(myLogger, PHLOG_TYPE_TRACE,
	"phFuncDestroy(P%p)", handlerID);
    CheckHandle(handlerID);
    RememberCall(handlerID, PHFUNC_AV_DESTROY);
    if (DummyOperation(handlerID, "destroy handler driver plugin"))
    {
	/* this is dummy code, only performed during regression tests */
	phFuncError_t rv;

	rv = DummyReturnValue(handlerID);
	if (rv == PHFUNC_ERR_OK)
	{
	}
	return rv;
	/* end of dummy code */
    }

    /* complete call in private section */
    return privateDestroy(handlerID);
}
#endif /* DESTROY_IMPLEMENTED */

/*****************************************************************************
 * End of file
 *****************************************************************************/
