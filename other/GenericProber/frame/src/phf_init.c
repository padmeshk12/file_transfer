/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : phf_init.c
 * CREATED  : 14 Jul 1999
 *
 * CONTENTS : Framework initialization
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 1999, Michael Vogt, created
 *            Dec 2005, Jiawei Lin, CR28409
 *              allow the user to enable/disable diagnose window by specifying
 *              the driver parameter "enable_diagnose_window"
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
#include <unistd.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_keys.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_mhcom.h"
#include "ph_pfunc.h"
#include "ph_phook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"
#include "ph_plugin.h"
#include "ph_GuiServer.h"

#include "ph_frame_private.h"
#include "reconfigure.h"
#include "phf_init.h"
#include "ph_timer.h"
#include "idrequest.h"
#include "stepcontrol.h"
#include "exception.h"
#include "dev/DriverDevKits.hpp"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define BUFF_LENGTH 1024
#define INITIAL_MSG_LOG    "/tmp/phlib_initial_msg_log"
#define INITIAL_ERROR_LOG  "/tmp/phlib_initial_error_log"

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/*Begin of Huatek Modifications, Luke Lan, 02/22/2001 */
/*Issue Number: 36   */
/*In string.h file, strdup() has been declared. So comment it*/
/*extern char *strdup(const char *str); */
/*End of Huatek Modifications */ 

static int checkServiceStatus(const char* serviceName, struct phFrameStruct *frame)
{
    char szPopenCMD[BUFF_LENGTH] = {0};
    sprintf(szPopenCMD, "pgrep %s", serviceName);

    FILE *pInfo = popen(szPopenCMD, "r");
    phLogFrameMessage(frame->logId, LOG_DEBUG, "Will execute shell command: \"%s\" to query service existence", szPopenCMD);
    if(NULL == pInfo)
    {
        phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR, "Execute shell command failed, please manually execute the shell command!");
        return -1;
    }

    char cmdResultBuff[BUFF_LENGTH] = {0};
    fgets(cmdResultBuff, BUFF_LENGTH, pInfo);

    pclose(pInfo);
    pInfo = NULL;

    if(cmdResultBuff[0] == '\0')
        return 0;
    return 1;
}

static int searchAttribFile(char *config, char *plugin, char *attrib)
{
    char *pathDelPos;
    FILE *testFP;

    /* construct the name of the attributes file out of the pathname
       of the configuration file */
    *attrib = '\0';
    strncpy(attrib, config, 1023);
    pathDelPos = strrchr(attrib, '/');
    if (pathDelPos)
    {
	pathDelPos++;
	strncpy(pathDelPos, PHLIB_S_ATTR_FILE, 
	    1023-(pathDelPos-attrib));
    }
    else
	strcpy(attrib, PHLIB_S_ATTR_FILE);

    testFP = fopen(attrib, "r");
    if (testFP)
    {
	/* found next to configuration */
	fclose(testFP);
	return 1;
    }

    /* construct the name of the attributes file out of the pathname
       of the plugin */
    *attrib = '\0';
    strncpy(attrib, plugin, 
	1023 - strlen("/config/") - strlen(PHLIB_S_ATTR_FILE));
    if (attrib[strlen(attrib)-1] != '/')
	strcat(attrib, "/");
    strcat(attrib, "config/");
    strcat(attrib, PHLIB_S_ATTR_FILE);

    testFP = fopen(attrib, "r");
    if (testFP)
    {
	/* found in plugin/config */
	fclose(testFP);
	return 1;
    }

    /* not found */
    *attrib = '\0';
    return 0;
}

/*****************************************************************************
 *
 * init driver
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, created
 *
 * Description: Initialize the driver framework. Do everything which
 * is necessary: Create handles for all sub modules, initialize these
 * modules
 *
 ***************************************************************************/
struct phFrameStruct *initDriverFramework(void)
{
    int tmpErrors = 0;
    int tmpWarnings = 0;
    phLogError_t logError;
    phConfError_t confError;
    phTcomError_t testerError;
    phStateError_t stateError;
    phEstateError_t estateError;
    phComError_t comError;
    phPlugError_t plugError;
    phPFuncError_t funcError;
    phHookError_t hookError;
    phEventError_t eventError;
    phUserDialogError_t userDialogError;

    const char *envConfig = NULL;
    const char *driverPlugin = NULL;
    const char *driverConf = NULL;
    char usedDriverPlugin[1024];
    char usedDriverConf[1024];
    char usedDriverAttr[1024];

    const char *commMode = NULL;
    phComMode_t drvMode;
    phComGpibEomAction_t eomAction = PHCOM_GPIB_EOMA_NONE;
    const char *interfaceName = NULL;
    const char *interfaceType = NULL;
    double busyThreshold = 0.0;
    long drvBusyDelay;
    double pollingInterval = 0.0;
    long drvPollTime;
    double gpibPort = 0.0;
    int drvGpibPort;

    phTcomMode_t testerMode;
    const char *testerSimulation;

    const char *idString;

    struct phFrameStruct *frame;
/* Begin of Huatek Modification, Donnie Tu, 12/10/2001 */
/* Issue Number: 315 */
/*    long pauseFlag;*/
/*    long quitFlag;*/
/* End of Huatek Modification */
    long abortFlag;

    int success;
    int completed;
    phTcomUserReturn_t retPlugin;
    exceptionActionError_t action;

    int selection = 0;

    /* allocate frame struct space */
    frame = PhNew(struct phFrameStruct);
    if (!frame)
    {
	fprintf(stderr, "PH lib: FATAL! "
	    "Could not allocate memory for driver initialization");
	return NULL;
    }

    /* be sure to reset all at the beginning */
    frame->isInitialized = 0;
    frame->initTried = 1;
    frame->errors = 0;
    frame->warnings = 0;

    frame->currentAMCall = PHFRAME_ACT_DRV_START;

    frame->logId = NULL;
    frame->globalConfId = NULL;
    frame->specificConfId = NULL;
    frame->attrId = NULL;
    frame->testerId = NULL;
    frame->stateId = NULL;
    frame->estateId = NULL;
    frame->comId = NULL;
    frame->funcId = NULL;
    frame->eventId = NULL;

    frame->funcAvail = (phPFuncAvailability_t) 0;
    frame->hookAvail = (phHookAvailability_t) 0;

    frame->numSites = 0;
    /* frame->stToProberSiteMap[PHESTATE_MAX_SITES] = ... */

    frame->binMode =  PHBIN_MODE_DEFAULT;
    frame->binPartialMode =  0;
    frame->binMapHdl = NULL;
    frame->dontTrustBinning = 0;

    frame->binActUsed = PHBIN_ACT_NONE;
    frame->binActMode = PHBIN_ACTMODE_HARDBINS;
    frame->binActMapHdl = NULL;

    frame->cleaningRatePerDie = 0;
    frame->cleaningRatePerWafer = 0;
    frame->cleaningDieCount = 0;
    frame->cleaningWaferCount = 0;

    frame->isSubdieProbing = 0;
    /* frame->perSiteDieOffset[PHESTATE_MAX_SITES] = ... */

    frame->cassetteLevelUsed = 0;
    frame->lotLevelUsed = 0;

/*
    frame->stDieLocIsByIndex = 0;
    frame->probDieLocIsByIndex = 0;
    frame->dieSize.x = 0.0;
    frame->dieSize.y = 0.0;
    frame->subdieSize.x = 0.0;
    frame->subdieSize.y = 0.0;
    frame->dieLocOffset.x = 0.0;
    frame->dieLocOffset.y = 0.0;
    frame->subdieLocOffset.x = 0.0;
    frame->subdieLocOffset.y = 0.0;
*/

    frame->d_sp.stepMode = PHFRAME_STEPMD_EXPLICIT;
    frame->d_sp.count = 0;
    frame->d_sp.pos = 0;
    frame->d_sp.xStepList = NULL;
    frame->d_sp.yStepList = NULL;
    frame->d_sp.dieMap = NULL;
    frame->d_sp.mark = PHFRAME_STMARK_OUT;
    frame->d_sp.next = NULL;

    frame->sd_sp.stepMode = PHFRAME_STEPMD_EXPLICIT;
    frame->sd_sp.count = 0;
    frame->sd_sp.pos = 0;
    frame->sd_sp.xStepList = NULL;
    frame->sd_sp.yStepList = NULL;
    frame->sd_sp.dieMap = NULL;
    frame->sd_sp.mark = PHFRAME_STMARK_OUT;
    frame->sd_sp.next = NULL;

    frame->totalTimer = NULL;
    frame->shortTimer = NULL;

    frame->initialPauseFlag = 0L;
    frame->initialQuitFlag = 0L;

    frame->createTestLog = 0;
    frame->testLogOut = NULL;

    /* be default we enable the popup window in case of error, CR 28409 */
    frame->enableDiagnoseWindow = YES;  

    /* initialize timer elements for later usage */
    frame->totalTimer = phFrameGetNewTimerElement();    
    frame->shortTimer = phFrameGetNewTimerElement();

    /* first initialize the logger. It is used by every other
       module. Since no log files are defined so far, two dedicated
       files are used in /tmp */

#ifdef DEBUG
    logError = phLogInit(&frame->logId, PHLOG_MODE_ALL, 
	"-", INITIAL_ERROR_LOG, "-", INITIAL_MSG_LOG, -1);
#else
    logError = phLogInit(&frame->logId, (phLogMode_t)(PHLOG_MODE_NORMAL|PHLOG_MODE_DBG_1),
	"-", INITIAL_ERROR_LOG, "-", INITIAL_MSG_LOG, -1);
#endif

    if (logError != PHLOG_ERR_OK && logError != PHLOG_ERR_FILE)
    {
        frame->errors++;
        fprintf(stderr, "PH lib: FATAL! Could not initialize logger (error %d)\n", (int) logError);
        return frame;
    }

    if(1 == checkServiceStatus("firewalld", frame))
        phLogFuncMessage(frame->logId, PHLOG_TYPE_WARNING, "Detected firewall service which may impact on the SRQ handling, please stop the service by following commands: \"systemctl stop firewalld.service\"");

    if(0 == checkServiceStatus("rpcbind", frame))
    {
        phLogFuncMessage(frame->logId, PHLOG_TYPE_WARNING, "Didn't detect rpcbind service, may break PH communication, please start the service by following commands: \"systemctl start rpcbind.service\"");
        driver_dev_kits::AlarmController ac;
        ac.raiseCommunicationAlarm("Indispensable rpcbind service is missing");
    }

    /* now initialize the configuration manager. */
    confError = phConfInit(frame->logId);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not initialize configuration (error %d)", (int) confError);
        return frame;
    }

    /* read in the global configuration file. It determines, were to find
       the driver plugin and were to read the detailed configuration */
    envConfig = getenv(PHLIB_ENV_GCONF);
    if (envConfig && strlen(envConfig) != 0)
    {
	phLogFrameMessage(frame->logId, LOG_NORMAL, 
	    "looking for global configuration file \"%s\"\n"
	    "(environment variable %s is set)", 
	    envConfig, PHLIB_ENV_GCONF);
	confError = phConfReadConfFile(&frame->globalConfId, 
	    envConfig);	
    }
    if (!envConfig || strlen(envConfig) == 0)
    {
	phLogFrameMessage(frame->logId, LOG_NORMAL, 
	    "looking for global configuration file \"%s\"", 
	    PHLIB_CONF_PATH "/" PHLIB_CONF_DIR "/" PHLIB_GB_CONF_FILE);
	confError = phConfReadConfFile(&frame->globalConfId, 
	    PHLIB_CONF_PATH "/" PHLIB_CONF_DIR "/" PHLIB_GB_CONF_FILE);
    }
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not read global configuration (error %d)", (int) confError);
        return frame;
    }

    /* from the global configuration, get the name of the driver
       specific configuration */
    confError = phConfConfString(frame->globalConfId, 
	PHKEY_G_PLUGIN, 0, NULL, &driverPlugin);
    if (confError != PHCONF_ERR_OK)
	frame->errors++;
    confError = phConfConfString(frame->globalConfId, 
	PHKEY_G_CONFIG, 0, NULL, &driverConf);
    if (confError != PHCONF_ERR_OK)
	frame->errors++;
    if (frame->errors>0)
    {
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "basic configuration settings could not be read.\n");
        return frame;	
    }
    reconfigureCompletePath(usedDriverPlugin, driverPlugin);
    reconfigureCompletePath(usedDriverConf, driverConf);

    /* read in the specific configuration file. */
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"looking for specific configuration file \"%s\"", usedDriverConf);

    confError = phConfReadConfFile(&frame->specificConfId, usedDriverConf);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not read specific configuration (error %d)", 
                (int) confError);
        return frame;
    }

    /* construct the name of the attributes file out of the pathname
       of the configuration file */
    if (!searchAttribFile(usedDriverConf, usedDriverPlugin, usedDriverAttr))
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not locate attributes file \"%s\"\n"
                "(searched next to file %s/config and \n"
                "in directory %s)", 
                PHLIB_S_ATTR_FILE, usedDriverConf, usedDriverPlugin);
        return frame;	
    }
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"looking for specific attributes file \"%s\"", usedDriverAttr);

    /* read the attributes file */
    confError = phConfReadAttrFile(&frame->attrId, usedDriverAttr);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not read attributes file (error %d)", 
                (int) confError);
        return frame;
    }
    
    /* check configuration against attributes */
    confError = phConfValidate(frame->specificConfId, frame->attrId);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "The prober specific configuration is not valid, "
                "giving up (error %d)", 
                (int) confError);
        return frame;
    }

    /* Now first get the location of the final log files before doing
       anything else. Ignore the case, that log files have not been
       specified, but print out a warning */
    reconfigureLogger(frame, &tmpErrors, &tmpWarnings);
    frame->errors += tmpErrors;
    frame->warnings += tmpWarnings;

    /* Now we have a valid and working configuration and also a
       working log mechanism. Now all remaining modules are
       initialized and handles are passed around */

    /* identify myself, now where the logger logs.... */
    phFrameGetID(frame, PHKEY_NAME_ID_DRIVER, &idString);
    /*
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"\n\n%s\n", idString);
    */

    /* initialize the tester communication */
    testerMode = PHTCOM_MODE_ONLINE;
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_MO_STSIM);
    if (confError == PHCONF_ERR_OK)
    {
	confError = phConfConfString(frame->specificConfId, PHKEY_MO_STSIM,
	    0, NULL, &testerSimulation);
	if (confError == PHCONF_ERR_OK && 
	    strcasecmp(testerSimulation, "yes") == 0)
	    testerMode = PHTCOM_MODE_SIMULATED;
    }
    testerError = phTcomInit(&(frame->testerId), frame->logId, testerMode);
    if (testerError != PHTCOM_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize link to tester, giving up (error %d)", 
	    (int) testerError);
	return frame;
    }

    /* before doing anything else, check the abort flag. This may be a
       retry or restart of the driver after a fatal situation, we
       don't want to go on then.... */
    abortFlag = 0;
    phTcomGetSystemFlag(frame->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if (abortFlag)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "abort flag was set, giving up on executing prober/handler driver call");
	return frame;
    }
    

    /* initialize the state controller */
    stateError = phStateInit(&(frame->stateId),
	frame->testerId, frame->logId);
    if (stateError != PHSTATE_ERR_OK)
	frame->errors++;
    
    /* initialize the equipment specific state controller */
    estateError = phEstateInit(&(frame->estateId), frame->logId);
    if (estateError != PHESTATE_ERR_OK)
	frame->errors++;
    
    /* set some parameters for the state controller */
    reconfigureStateController(frame, &tmpErrors, &tmpWarnings);
    frame->errors += tmpErrors;
    frame->warnings += tmpWarnings;
    if (frame->errors)
    {
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize state controller, giving up");
	return frame;
    }
    
    reconfigureSiteManagement(frame, &tmpErrors, &tmpWarnings);
    frame->errors += tmpErrors;
    frame->warnings += tmpWarnings;
    if (frame->errors)
    {
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not initialize site management, giving up");
        return frame;
    }

    /* initialize the die and sub-die step control of the framework */
    phFrameStepControlInit(frame);
    if (frame->errors)
    {
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize the stepping pattern, giving up");
	return frame;
    }

    /* get configuration for the communication link */

    confError = phConfConfString(frame->specificConfId, 
	PHKEY_IF_NAME, 0, NULL, &interfaceName);
    if (confError != PHCONF_ERR_OK)
	frame->errors++;

    confError = phConfConfIfDef(frame->specificConfId, PHKEY_MO_ONLOFF);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfString(frame->specificConfId, 
	    PHKEY_MO_ONLOFF, 0, NULL, &commMode);
    if (confError != PHCONF_ERR_OK)
	commMode = NULL;

    /* further analysis of communication configuration */
    if (commMode == NULL)
	drvMode = PHCOM_ONLINE;
    else
	drvMode = strcasecmp(commMode, "on-line") == 0 ?
	    PHCOM_ONLINE : PHCOM_OFFLINE;
    

    /* get configuration to find out what kind of interface we are working on */

    confError = phConfConfString(frame->specificConfId, 
        PHKEY_IF_TYPE, 0, NULL, &interfaceType);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not get interface type from comfiguration, giving up");
        return frame;
    }

    if (strcasecmp(interfaceType, "gpib") == 0)
    {
	confError = phConfConfNumber(frame->specificConfId, 
	    PHKEY_IF_PORT, 0, NULL, &gpibPort);
	if (confError != PHCONF_ERR_OK)
	    frame->errors++;
	drvGpibPort = (int) gpibPort;

	confError = phConfConfStrTest(&selection, 
	    frame->specificConfId, PHKEY_GB_EOMACTION,
	    "none", "toggle-ATN", "serial-poll", NULL);
	if (confError != PHCONF_ERR_OK && confError != PHCONF_ERR_UNDEF)
	    frame->errors++;

	switch (selection)
	{
	  case 2: /* toggle-ATN */
	    eomAction = PHCOM_GPIB_EOMA_TOGGLEATN;
	    break;
	  case 3: /* serial-poll */
	    eomAction = PHCOM_GPIB_EOMA_SPOLL;
	    break;
	  case 0: /* no given */
	  case 1: /* "none" selected */
	  default: /* something else given */
	    eomAction = PHCOM_GPIB_EOMA_NONE;
	    break;
	}	    

	if (frame->errors)
	{
	    /* are there any errors, also during above analysis? */
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
		"configuration for communication interface incomplete or wrong");
	    return frame;
	}
    
	do
	{
	    phLogFrameMessage(frame->logId, LOG_DEBUG, 
		"initializing GPIB interface \"%s\" for port %d", interfaceName, drvGpibPort);
	    comError = phComGpibOpen(&(frame->comId), drvMode, interfaceName, drvGpibPort, frame->logId, eomAction);
	    if (comError != PHCOM_ERR_OK)
	    {
		char response[128];

		switch (comError)
		{
		  case PHCOM_ERR_GPIB_ICLEAR_FAIL:
		    /* there seems to be no listener on the GPIB bus */
		    frame->warnings++;
		    phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
			"no prober seems to be connected to the GPIB interface");
		    response[0] = '\0';
		    phGuiShowOptionDialog(frame->logId, PH_GUI_ERROR_ABORT_RETRY,
			"Prober driver error:\n"
			"\n"
			"Starting up the GPIB communication timed out.\n"
			"The reason could be that no prober is connected\n"
			"to the GPIB interface.\n"
			"\n"
			"Please ensure that the prober is correctly connected\n"
			"and press RETRY.\n"
			"\n"
			"Otherwise press ABORT to quit the testprogram.\n",
			response);
		    if (strcasecmp(response, "retry") == 0)
			break;
		    /* else fall into default case ... */
		  default:
		    frame->errors++;
		    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
			"could not open GPIB interface, giving up (error %d)", 
			(int) comError);
		    return frame;
		}
	    }
	} while (comError != PHCOM_ERR_OK);
    }
    else
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "unknown interface type \"%s\", giving up", interfaceType);
	return frame;
    }

    /* activate driver plugin */
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"trying to load driver plugin \"%s\"", usedDriverPlugin);
    plugError = phPlugLoadDriver(frame->logId, usedDriverPlugin, 
	&frame->funcAvail);
    if (plugError != PHPLUG_ERR_OK && plugError != PHPLUG_ERR_DIFFER)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not load prober specific driver plugin, "
	    "giving up (error %d)", 
	    (int) plugError);
	return frame;
    }
    if ((frame->funcAvail & PHPFUNC_AV_MINIMUM) != PHPFUNC_AV_MINIMUM)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "Minimum functional requirements for driver plugin not met, "
	    "giving up");
	return frame;
    }
    if (frame->isSubdieProbing && (
	!(frame->funcAvail & PHPFUNC_AV_GETSUBDIE) ||
	!(frame->funcAvail & PHPFUNC_AV_BINSUBDIE)))
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "Subdie handling is configured but not provided\n"
	    "by the driver plugin, giving up");
	return frame;
    }
    
    
    /* initialize prober specific driver */
    phLogFrameMessage(frame->logId, LOG_NORMAL,
	"waiting for prober to initialize");
    phFrameStartTimer(frame->totalTimer);
    phFrameStartTimer(frame->shortTimer);

    action = PHFRAME_EXCEPTION_ERR_OK;
    success = 0;
    completed = 0;
    retPlugin = PHTCOM_CI_CALL_PASS;

    while (!success && !completed)
    {
	funcError = phPlugPFuncInit(&frame->funcId, frame->testerId, frame->comId, frame->logId, 
	    frame->specificConfId, frame->estateId);

        /* analyze result from initialization and take action */
        phFrameHandlePluginResult(frame, funcError, PHPFUNC_AV_INIT,
                                  &completed, &success, &retPlugin, &action);

        if (!success && !completed)
        {
            /* go on waiting, but sleep before, give the GPIB bus
               time to recover */
            sleep(5);
        }
    }

    if ( action == PHFRAME_EXCEPTION_ERR_ERROR )
    {
        frame->errors++;
    }

#if 0
	if (funcError == PHPFUNC_ERR_WAITING)
	{
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
		"prober not yet initialized, still waiting, set pause, quit, or abort flag to abort");
	    sleep(5);
	    pauseFlag = 0;
	    phTcomGetSystemFlag(frame->testerId, PHTCOM_SF_CI_PAUSE, &pauseFlag);
	    quitFlag = 0;
	    phTcomGetSystemFlag(frame->testerId, PHTCOM_SF_CI_QUIT, &quitFlag);
	    abortFlag = 0;
	    phTcomGetSystemFlag(frame->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
	    if (pauseFlag || quitFlag || abortFlag)
		funcError = PHPFUNC_ERR_ABORTED;
	    else
		phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
		"trying again....");
	}
	if (funcError != PHPFUNC_ERR_OK && funcError != PHPFUNC_ERR_WAITING)
	{
	    frame->errors++;
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
		"could not initialize prober specific driver plugin, "
		"giving up (error %d)", 
		(int) funcError);
	    return frame;
	}
#endif

    if (frame->errors)
	return frame;

    /* activate hook plugin */
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"trying to load hook plugin \"%s\"", usedDriverPlugin);
    plugError = phPlugLoadHook(frame->logId, usedDriverPlugin, 
	&frame->hookAvail);
    if (plugError != PHPLUG_ERR_OK && 
	plugError != PHPLUG_ERR_DIFFER &&
	plugError != PHPLUG_ERR_NEXIST)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not load prober specific hook plugin, "
	    "giving up (error %d)", 
	    (int) plugError);
	return frame;
    }

    /* only go on with hooks if existent */
    if (plugError == PHPLUG_ERR_OK || plugError == PHPLUG_ERR_DIFFER)
    {
	if ((frame->hookAvail & PHHOOK_AV_MINIMUM) != PHHOOK_AV_MINIMUM)
	{
	    frame->errors++;
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
		"Minimum functional requirements for hook plugin not met, "
		"giving up");
	    return frame;
	}
    
	/* initialize prober specific hooks */
	hookError = phPlugHookInit(frame->funcId, frame->comId, frame->logId, 
	    frame->specificConfId, frame->stateId, frame->estateId, 
	    frame->testerId);
	if (hookError != PHHOOK_ERR_OK)
	{
	    frame->errors++;
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
		"could not initialize prober specific hook plugin, "
		"giving up (error %d)", 
		(int) hookError);
	    return frame;
	}
    }

    /* initialize the event handler */
    eventError = phEventInit(&frame->eventId, frame->funcId, frame->comId, 
	frame->logId, frame->specificConfId, frame->stateId, frame->estateId, 
	frame->testerId);
    if (eventError != PHEVENT_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize event handler, "
	    "giving up (error %d)", 
	    (int) eventError);
	return frame;
    }

    /* initialize [ Communication Start ] User Dialog */
    userDialogError = initUserDialog(&frame->dialog_comm_start,
                                     frame->logId,
                                     frame->specificConfId, 
                                     frame->testerId,
                                     frame->eventId,
                                     PHKEY_OP_DIAGCOMMST,
                                     "Communication Start",
                                     "Please press Continue as soon as \n"
                                     "the prober is ready to start\n"
                                     "communication with the tester.\n"
                                     "Press Quit to end the testprogram.",
                                     ud_freq_once);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize user dialog, "
	    "giving up (error %d)", 
	    (int) userDialogError);
	return frame;
    }

    /* initialize [ Configuration Start ] User Dialog */
    userDialogError = initUserDialog(&frame->dialog_config_start,
                                     frame->logId,
                                     frame->specificConfId, 
                                     frame->testerId,
                                     frame->eventId,
                                     PHKEY_OP_DIAGCONFST,
                                     "Configuration Start",
                                     "Please press Continue as soon as \n"
                                     "the prober is ready to \n"
                                     "receive setup information.\n"
                                     "Press Quit to end the testprogram.",
                                     ud_freq_once);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize user dialog, "
	    "giving up (error %d)", 
	    (int) userDialogError);
	return frame;
    }

    /* initialize [ Lot Start ] User Dialog */
    userDialogError = initUserDialog(&frame->dialog_lot_start,
                                     frame->logId,
                                     frame->specificConfId, 
                                     frame->testerId,
                                     frame->eventId,
                                     PHKEY_OP_DIAGLOTST,
                                     "Lot Start",
                                     "Please press Continue as soon as \n"
                                     "the prober is ready to \n"
                                     "provide the first wafer.\n"
                                     "Press Quit to end the testprogram.",
                                     ud_freq_unknown);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize user dialog, "
	    "giving up (error %d)", 
	    (int) userDialogError);
	return frame;
    }

    /* initialize [ Wafer Start ] User Dialog */
    userDialogError = initUserDialog(&frame->dialog_wafer_start,
                                     frame->logId,
                                     frame->specificConfId, 
                                     frame->testerId,
                                     frame->eventId,
                                     PHKEY_OP_DIAGWAFST,
                                     "Wafer Start",
                                     "Please press Continue as soon as \n"
                                     "the prober is ready to test \n"
                                     "the next wafer.\n"
                                     "Press Quit to end the testprogram.",
                                     ud_freq_unknown);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize user dialog, "
	    "giving up (error %d)", 
	    (int) userDialogError);
	return frame;
    }

    /* find out whether we want to generate an output file that can be
       used for automatic driver test */
    if (getenv(TEST_LOG_ENV_VAR))
    {
	frame->testLogOut = fopen(getenv(TEST_LOG_ENV_VAR), "w");
	if ( frame->testLogOut &&
	     fclose(frame->testLogOut)== 0 )
        {   
            frame->testLogOut = fopen(getenv(TEST_LOG_ENV_VAR), "a");
        }
	if ( frame->testLogOut )
	    frame->createTestLog = 1;
    }

    /* all done, puhhh....... */
    frame->isInitialized = 1;
    phLogFrameMessage(frame->logId, LOG_NORMAL,
	"Prober driver framework initialized\n");

    return frame;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
