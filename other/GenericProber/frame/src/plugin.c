/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : plugin.c
 * CREATED  : 14 Jul 1999
 *
 * CONTENTS : Plugin library management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR27092&CR25172
 *            Garry Xie,R&D Shanghai, CR28427
 *            Fabarca & kun xiao, R&D Shanghai, CR21589
 *            Dangln Li, R&D Shanghai, CR41221 & CR42599
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 1999, Michael Vogt, created
 *            August 2005, Jiawei Lin, CR27092&CR25172:
 *              Implement the function call from Framework to 
 *              plugin for "phPlugPFuncGetStatus", which requests
 *              the parameter/information from Prober, like WCR.
 *            February 2006, Garry Xie , CR28427
 *              Implement the function call from Framework to 
 *              plugin for "phPlugPFuncSetStatus", which set Paramers
 *              to the prober,like Alarm_Buzzer    
 *            July 2006, Fabarca & Kun Xiao, CR21589
 *              Implement the function call from Framework to plugin for 
 *              "phPlugFuncContacttest"       
 *            Nov 2008, Danglin Li, CR41221 & CR42599
 *              Implement the function call from Framework to plugin for
 *              "phPlugPFuncExecGpibCmd" and "phPlugPFuncExecGpibQuery".
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
#include <errno.h>
#include <dlfcn.h>

/*--- module includes -------------------------------------------------------*/
#include "ph_log.h"
#include "ph_pfunc.h"
#include "ph_phook.h"
#include "ph_plugin.h"
#include "ph_frame_private.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/
#define DRV_LIB_NAME  "libfunc.so"
#define HOOK_LIB_NAME "libhook.so"
#define PATH_DELIMITER '/'

/*--- typedefs --------------------------------------------------------------*/

/*--- gobal variables -------------------------------------------------------*/

static struct driverPlugin driverJump =
{
    (phPFuncAvailability_t)  0,
    (phPFuncAvailability_t)  0,
    (phPFuncAvailability_t)  0,

    (phPFuncAvailable_t *)   NULL,
    (phPFuncInit_t *)        NULL,
    (phPFuncReconfigure_t *) NULL,
    (phPFuncReset_t *)       NULL,
    (phPFuncDriverID_t *)    NULL,
    (phPFuncEquipID_t *)     NULL,
    (phPFuncLoadCass_t *)    NULL,
    (phPFuncUnloadCass_t *)  NULL,
    (phPFuncLoadWafer_t *)   NULL,
    (phPFuncUnloadWafer_t *) NULL,
    (phPFuncGetDie_t *)      NULL,
    (phPFuncBinDie_t *)      NULL,
    (phPFuncGetSubDie_t *)   NULL,
    (phPFuncBinSubDie_t *)   NULL,
    (phPFuncDieList_t *)     NULL,
    (phPFuncSubDieList_t *)  NULL,
    (phPFuncReprobe_t *)     NULL,
    (phPFuncCleanProbe_t *)  NULL,
    (phPFuncPMI_t *)         NULL,
    (phPFuncSTPaused_t *)    NULL,
    (phPFuncSTUnpaused_t *)  NULL,
    (phPFuncTestCommand_t *) NULL,
    (phPFuncDiag_t *)        NULL,
    (phPFuncStatus_t *)      NULL,
    (phPFuncUpdateState_t *) NULL,
    (phPFuncCassID_t *)      NULL,
    (phPFuncWafID_t *)       NULL,
    (phPFuncProbeID_t *)     NULL,
    (phPFuncLotID_t *)       NULL,
    (phPFuncStartLot_t *)    NULL,
    (phPFuncEndLot_t *)      NULL,
    (phPFuncCommTest_t *)    NULL,
    (phPFuncDestroy_t *)     NULL,
    (phPFuncSimResults_t *)  NULL,
    (phPFuncGetStatus_t *)   NULL,
    (phPFuncSetStatus_t *)   NULL,
    (phPFuncContacttest_t *) NULL,  /*CR21589,fabarca & kun xiao*/
    (phPFuncExecGpibCmd_t *) NULL,
    (phPFuncExecGpibQuery_t *) NULL
};

static struct hookPlugin hookJump =
{
    (phHookAvailability_t)  0,
    (phHookAvailability_t)  0,
    (phHookAvailability_t)  0,

    (phHookAvailable_t *)   NULL,
    (phHookInit_t *)        NULL,
    (phHookActionStart_t *) NULL,
    (phHookActionDone_t *)  NULL,
    (phHookProblem_t *)     NULL,
    (phHookWaiting_t *)     NULL,
    (phHookPopup_t *)       NULL
};

/*--- functions -------------------------------------------------------------*/
static void *checkFunction(phLogId_t logger, void *libHandle, const char *symbol)
{
    void *voidPtr = NULL;

    phLogFrameMessage(logger, PHLOG_TYPE_TRACE,
        "checkFunction(P%p, P%p, \"%s\")",
        logger, libHandle, symbol ? symbol : "(NULL)");

    if(symbol == NULL)
    {
        phLogFrameMessage(logger, PHLOG_TYPE_ERROR, "symbol cannot be NULL");
    }
    else
    {
        voidPtr = dlsym(libHandle, symbol);
        if(voidPtr == NULL)
        {
        phLogFrameMessage(logger, PHLOG_TYPE_ERROR,
            "could not resolve function '%s' in shared library (error: %s)", symbol, dlerror());
        }
     }

    return voidPtr;
}

static void *checkDriverFunction(
    phLogId_t logger,
    void *libHandle,
    phPFuncAvailability_t reportedAv,
    const char *symbol,
    phPFuncAvailability_t symbolAv)
{
    void *voidPtr = NULL;

    phLogFrameMessage(logger, PHLOG_TYPE_TRACE,
        "checkDriverFunction(P%p, P%p, 0x%08lx, \"%s\", 0x%08lx)",
        logger, libHandle, (long) reportedAv, symbol ? symbol : "(NULL)",
        (long) symbolAv);

    if (symbol == NULL )
    {
        phLogFrameMessage(logger, PHLOG_TYPE_ERROR, "symbol cannot be NULL");
        return voidPtr;
    }

    voidPtr = dlsym(libHandle, symbol);

    if (!voidPtr && (reportedAv & symbolAv))
    {
        phLogFrameMessage(logger, PHLOG_TYPE_ERROR,
            "could not resolve function '%s' in shared library, although\n"
            "it was reported to exist by 'phFuncAvailable()' (error: %s)",
             symbol, dlerror());
    }

    if (voidPtr)
    {
        if (reportedAv & symbolAv)
            phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_2,
                "function '%s' found in driver plugin, will be used", symbol);
        else
            phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_2,
                "function '%s' surprisingly found in driver plugin,\n"
                "will not be used", symbol);
    }

    return voidPtr;
}

static void *checkHookFunction(
    phLogId_t logger,
    void *libHandle, phHookAvailability_t reportedAv,
    const char *symbol, phHookAvailability_t symbolAv)
{
    void *voidPtr = NULL;

    phLogFrameMessage(logger, PHLOG_TYPE_TRACE,
        "checkHookFunction(P%p, P%p, 0x%08lx, \"%s\", 0x%08lx)",
        logger, libHandle, (long) reportedAv, symbol ? symbol : "(NULL)",
        (long) symbolAv);

    if (symbol == NULL )
    {
        phLogFrameMessage(logger, PHLOG_TYPE_ERROR, "symbol cannot be NULL");
        return voidPtr;
    }

    voidPtr = dlsym(libHandle, symbol);

    if (!voidPtr && (reportedAv & symbolAv))
    {
        phLogFrameMessage(logger, PHLOG_TYPE_ERROR,
            "could not resolve function '%s' in shared library, although\n"
            "it was reported to exist by 'phHookAvailable()' (error: %s)",
            symbol, dlerror());
    }

    if (voidPtr)
    {
        if (reportedAv & symbolAv)
            phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_2,
                "function '%s' found in hook plugin, will be used", symbol);
        else
            phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_2,
                "function '%s' surprisingly found in hook plugin,\n"
                "will not be used", symbol);
    }

    return voidPtr;
}

/*****************************************************************************
 *
 * Load a driver plugin library
 *
 * Authors: Michael Vogt
 *
 * History: 14 Jul 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_plugin.h
 *
 ***************************************************************************/
phPlugError_t phPlugLoadDriver(
    phLogId_t            logger         /* report errors here */,
    char                 *path          /* directory path were to look
					   for driver library plugin */,
    phPFuncAvailability_t *available    /* returns the set of
					   available driver plugin functions */
)
{
    char libname[1024]="";
    int libnameLength;
    char  * message_s = NULL ;
    static void *libHandle = NULL;

    phPlugError_t retVal = PHPLUG_ERR_OK;

    phLogFrameMessage(logger, PHLOG_TYPE_TRACE,
	"phPlugLoadDriver(P%p, \"%s\", P%p)",
	logger, path ? path : "(NULL)", available);

    /* first check, if a library was already loaded and is going to be
       replaced: */
    if (libHandle != NULL)
    {
        dlclose(libHandle);
        libHandle = NULL;

	driverJump.found =       (phPFuncAvailability_t)  0;
	driverJump.reported =    (phPFuncAvailability_t)  0;
	driverJump.used =        (phPFuncAvailability_t)  0;

	driverJump.available =   (phPFuncAvailable_t *)   NULL;
	driverJump.init =        (phPFuncInit_t *)        NULL;
	driverJump.reconfigure = (phPFuncReconfigure_t *) NULL;
	driverJump.reset =       (phPFuncReset_t *)       NULL;
	driverJump.driverID =    (phPFuncDriverID_t *)    NULL;
	driverJump.equipID =     (phPFuncEquipID_t *)     NULL;
	driverJump.loadCass =    (phPFuncLoadCass_t *)    NULL;
	driverJump.unloadCass =  (phPFuncUnloadCass_t *)  NULL;
	driverJump.loadWafer =   (phPFuncLoadWafer_t *)   NULL;
	driverJump.unloadWafer = (phPFuncUnloadWafer_t *) NULL;
	driverJump.getDie =      (phPFuncGetDie_t *)      NULL;
	driverJump.binDie =      (phPFuncBinDie_t *)      NULL;
	driverJump.getSubDie =   (phPFuncGetSubDie_t *)   NULL;
	driverJump.binSubDie =   (phPFuncBinSubDie_t *)   NULL;
	driverJump.dieList =     (phPFuncDieList_t *)     NULL;
	driverJump.subDieList =  (phPFuncSubDieList_t *)  NULL;
	driverJump.reprobe =     (phPFuncReprobe_t *)     NULL;
	driverJump.cleanProbe =  (phPFuncCleanProbe_t *)  NULL;
	driverJump.PMI =         (phPFuncPMI_t *)         NULL;
	driverJump.STPaused =    (phPFuncSTPaused_t *)    NULL;
	driverJump.STUnpaused =  (phPFuncSTUnpaused_t *)  NULL;
	driverJump.testCommand = (phPFuncTestCommand_t *) NULL;
	driverJump.diag =        (phPFuncDiag_t *)        NULL;
	driverJump.status =      (phPFuncStatus_t *)      NULL;
	driverJump.update =      (phPFuncUpdateState_t *) NULL;
	driverJump.cassID =      (phPFuncCassID_t *)      NULL;
	driverJump.wafID =       (phPFuncWafID_t *)       NULL;
	driverJump.probeID =     (phPFuncProbeID_t *)     NULL;
	driverJump.lotID =       (phPFuncLotID_t *)       NULL;
	driverJump.startLot =    (phPFuncStartLot_t *)    NULL;
	driverJump.endLot =      (phPFuncEndLot_t *)      NULL;
	driverJump.commTest =    (phPFuncCommTest_t *)    NULL;
	driverJump.destroy =     (phPFuncDestroy_t *)     NULL;
	driverJump.simResults =  (phPFuncSimResults_t *)  NULL;
        driverJump.getStatus =   (phPFuncGetStatus_t *)   NULL;
        driverJump.setStatus =   (phPFuncSetStatus_t *)   NULL;
        driverJump.contacttest = (phPFuncContacttest_t *) NULL; /*CR21589,fabarca & Kun Xiao, 16 Jun 2006*/
        driverJump.execGpibCmd = (phPFuncExecGpibCmd_t *) NULL;
        driverJump.execGpibQuery = (phPFuncExecGpibQuery_t *) NULL;
    }
    *available = (phPFuncAvailability_t)  0;

    /* prepare loading of a new library */
    if(path!=NULL){
      strncpy(libname, path, sizeof(libname) - 1);
    }
    libnameLength = strlen(libname);

    /* cat a '/', if missing but only if not an empty relative path */
    if (libnameLength > 0 && libname[libnameLength-1] != PATH_DELIMITER)
    {
	libname[libnameLength+1] = '\0';
	libname[libnameLength] = PATH_DELIMITER;
    }

    /* cat the library name */
    strcat(libname, "lib/");
    strcat(libname, DRV_LIB_NAME);

    /* now load the library */
    phLogFrameMessage(logger, LOG_DEBUG,
	"trying to load driver plugin %s ...", libname);

    libHandle = dlopen(libname, RTLD_NOW|RTLD_GLOBAL);
    if (libHandle == NULL)
    {
        /* an error occured, library could not be loaded. This is a
           fatal situation during driver initialization.... */
         phLogFrameMessage(logger, PHLOG_TYPE_FATAL,
                           "File: '%s'\n'%s'",
                           libname,
                           dlerror());
         return PHPLUG_ERR_LOAD;
    }


    /* else: the library was loaded, check consistency */

    /* first try to call the 'availability' report function */
    driverJump.available = (phPFuncAvailable_t *) 
	checkFunction(logger, libHandle, "phPFuncAvailable");
    if (! driverJump.available)
	driverJump.reported = (phPFuncAvailability_t) 0;	
    else
	driverJump.reported = (*(driverJump.available))();

    /* now try to find the interface functions of the plugin */
    driverJump.found = (phPFuncAvailability_t) 0;	

    driverJump.init =        (phPFuncInit_t *)
        checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncInit", PHPFUNC_AV_INIT);
    if (driverJump.init)
	driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_INIT);

    driverJump.reconfigure = (phPFuncReconfigure_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncReconfigure", PHPFUNC_AV_RECONFIGURE);
    if (driverJump.reconfigure)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_RECONFIGURE);

    driverJump.reset =       (phPFuncReset_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncReset", PHPFUNC_AV_RESET);
    if (driverJump.reset)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_RESET);

    driverJump.driverID =    (phPFuncDriverID_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncDriverID", PHPFUNC_AV_DRIVERID);
    if (driverJump.driverID)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_DRIVERID);

    driverJump.equipID =     (phPFuncEquipID_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncEquipID", PHPFUNC_AV_EQUIPID);
    if (driverJump.equipID)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_EQUIPID);

    driverJump.loadCass =    (phPFuncLoadCass_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncLoadCass", PHPFUNC_AV_LDCASS);
    if (driverJump.loadCass)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_LDCASS);

    driverJump.unloadCass =  (phPFuncUnloadCass_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncUnloadCass", PHPFUNC_AV_UNLCASS);
    if (driverJump.unloadCass)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_UNLCASS);

    driverJump.loadWafer =   (phPFuncLoadWafer_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncLoadWafer", PHPFUNC_AV_LDWAFER);
    if (driverJump.loadWafer)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_LDWAFER);

    driverJump.unloadWafer = (phPFuncUnloadWafer_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncUnloadWafer", PHPFUNC_AV_UNLWAFER);
    if (driverJump.unloadWafer)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_UNLWAFER);

    driverJump.getDie =      (phPFuncGetDie_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncGetDie", PHPFUNC_AV_GETDIE);
    if (driverJump.getDie)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_GETDIE);

    driverJump.binDie =      (phPFuncBinDie_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncBinDie", PHPFUNC_AV_BINDIE);
    if (driverJump.binDie)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_BINDIE);

    driverJump.getSubDie =   (phPFuncGetSubDie_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncGetSubDie", PHPFUNC_AV_GETSUBDIE);
    if (driverJump.getSubDie)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_GETSUBDIE);

    driverJump.binSubDie =   (phPFuncBinSubDie_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncBinSubDie", PHPFUNC_AV_BINSUBDIE);
    if (driverJump.binSubDie)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_BINSUBDIE);

    driverJump.dieList =     (phPFuncDieList_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncDieList", PHPFUNC_AV_DIELIST);
    if (driverJump.dieList)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_DIELIST);

    driverJump.subDieList =  (phPFuncSubDieList_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncSubDieList", PHPFUNC_AV_SUBDIELIST);
    if (driverJump.subDieList)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_SUBDIELIST);

    driverJump.reprobe =     (phPFuncReprobe_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncReprobe", PHPFUNC_AV_REPROBE);
    if (driverJump.reprobe)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_REPROBE);

    driverJump.cleanProbe =  (phPFuncCleanProbe_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncCleanProbe", PHPFUNC_AV_CLEAN);
    if (driverJump.cleanProbe)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_CLEAN);

    driverJump.PMI =         (phPFuncPMI_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncPMI", PHPFUNC_AV_PMI);
    if (driverJump.PMI)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_PMI);

    driverJump.STPaused =    (phPFuncSTPaused_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncSTPaused", PHPFUNC_AV_PAUSE);
    if (driverJump.STPaused)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_PAUSE);

    driverJump.STUnpaused =  (phPFuncSTUnpaused_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncSTUnpaused", PHPFUNC_AV_UNPAUSE);
    if (driverJump.STUnpaused)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_UNPAUSE);

    driverJump.testCommand = (phPFuncTestCommand_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncTestCommand", PHPFUNC_AV_TEST);
    if (driverJump.testCommand)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_TEST);

    driverJump.diag =        (phPFuncDiag_t *)
        checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncDiag", PHPFUNC_AV_DIAG);
    if (driverJump.diag)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_DIAG);

    driverJump.status =      (phPFuncStatus_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncStatus", PHPFUNC_AV_STATUS);
    if (driverJump.status)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_STATUS);

    driverJump.update =      (phPFuncUpdateState_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncUpdateState", PHPFUNC_AV_UPDATE);
    if (driverJump.update)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_UPDATE);

    driverJump.cassID =     (phPFuncCassID_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncCassID", PHPFUNC_AV_CASSID);
    if (driverJump.cassID)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_CASSID);

    driverJump.wafID =     (phPFuncWafID_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncWafID", PHPFUNC_AV_WAFID);
    if (driverJump.wafID)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_WAFID);

    driverJump.probeID =     (phPFuncProbeID_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncProbeID", PHPFUNC_AV_PROBEID);
    if (driverJump.probeID)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_PROBEID);

    driverJump.lotID =     (phPFuncLotID_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncLotID", PHPFUNC_AV_LOTID);
    if (driverJump.lotID)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_LOTID);

    driverJump.startLot =    (phPFuncStartLot_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncStartLot", PHPFUNC_AV_STARTLOT);
    if (driverJump.startLot)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_STARTLOT);

    driverJump.endLot =  (phPFuncEndLot_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncEndLot", PHPFUNC_AV_ENDLOT);
    if (driverJump.endLot)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_ENDLOT);

    driverJump.commTest = (phPFuncCommTest_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
	    "phPFuncCommTest", PHPFUNC_AV_COMMTEST);
    if (driverJump.commTest)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_COMMTEST);

    driverJump.getStatus = (phPFuncGetStatus_t *) checkDriverFunction(
      logger, libHandle, driverJump.reported, "phPFuncGetStatus", PHPFUNC_AV_GETSTATUS);
    if( driverJump.getStatus ) {
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_GETSTATUS);
    }

    driverJump.setStatus = (phPFuncSetStatus_t *) checkDriverFunction(
      logger, libHandle, driverJump.reported, "phPFuncSetStatus", PHPFUNC_AV_SETSTATUS);
    if( driverJump.setStatus ) {
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_SETSTATUS);
    }

    driverJump.destroy =     (phPFuncDestroy_t *)
	checkDriverFunction(logger, libHandle, driverJump.reported,
                        "phPFuncDestroy", PHPFUNC_AV_DESTROY);
    if (driverJump.destroy)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_DESTROY);

    driverJump.simResults =     (phPFuncSimResults_t *)
	checkDriverFunction(logger, libHandle, (phPFuncAvailability_t)0,
                        "phPFuncSimResults", (phPFuncAvailability_t)0);

    /*CR21589, fabarca & Kun Xiao, 16 Jun 2006*/
    driverJump.contacttest = (phPFuncContacttest_t *)
    checkDriverFunction(logger, libHandle, driverJump.reported,
                        "phPFuncContacttest", PHPFUNC_AV_CONTACTTEST);
    if (driverJump.contacttest)
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_CONTACTTEST);
    /*End CR21589*/

    driverJump.execGpibCmd = (phPFuncExecGpibCmd_t *) checkDriverFunction(
      logger, libHandle, driverJump.reported, "phPFuncExecGpibCmd", PHPFUNC_AV_EXECGPIBCMD);
    if( driverJump.execGpibCmd ) {
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_EXECGPIBCMD);
    }

    driverJump.execGpibQuery = (phPFuncExecGpibQuery_t *) checkDriverFunction(
      logger, libHandle, driverJump.reported, "phPFuncExecGpibQuery", PHPFUNC_AV_EXECGPIBQUERY);
    if( driverJump.execGpibQuery ) {
      driverJump.found = (phPFuncAvailability_t)(driverJump.found | PHPFUNC_AV_EXECGPIBQUERY);
    }

    driverJump.used = (phPFuncAvailability_t)(driverJump.found & driverJump.reported);

    /* log final report message */
    if (driverJump.found == driverJump.reported)
	phLogFrameMessage(logger, LOG_DEBUG, 
                      "driver plugin loaded");
    else if (driverJump.used != (driverJump.used & driverJump.reported))
    {
	phLogFrameMessage(logger, PHLOG_TYPE_ERROR, 
                      "driver plugin loaded with restrictions");
	retVal = PHPLUG_ERR_DIFFER;
    }

    /* return result, only report functions that were found and were
       expected to be found */
    *available = driverJump.used;
    
    return retVal;
}

/*****************************************************************************
 *
 * Load a hook plugin library
 *
 * Authors: Michael Vogt
 *
 * History: 24 Aug 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_plugin.h
 *
 ***************************************************************************/
phPlugError_t phPlugLoadHook(
    phLogId_t            logger         /* report errors here */,
    char                 *path          /* directory path were to look
					   for driver library plugin */,
    phHookAvailability_t *available     /* returns the set of
					   available hook plugin functions */
)
{
    char libname[1024];
    int libnameLength;
    char  * message_s = NULL ;
    static void *libHandle = NULL;
    FILE *testFile;

    phPlugError_t retVal = PHPLUG_ERR_OK;

    phLogFrameMessage(logger, PHLOG_TYPE_TRACE,
	"phPlugLoadHook(P%p, \"%s\", P%p)",
	logger, path ? path : "(NULL)", available);

    /* first check, if a library was already loaded and is going to be
       replaced: */
    if (libHandle != NULL)
    {
        dlclose(libHandle);
        libHandle = NULL;

	hookJump.found  =        (phHookAvailability_t)  0;
	hookJump.reported  =     (phHookAvailability_t)  0;
	hookJump.used  =         (phHookAvailability_t)  0;

	hookJump.available  =    (phHookAvailable_t *)   NULL;
	hookJump.init  =         (phHookInit_t *)        NULL;
	hookJump.actionStart  =  (phHookActionStart_t *) NULL;
	hookJump.actionDone  =   (phHookActionDone_t *)  NULL;
	hookJump.problem  =      (phHookProblem_t *)     NULL;
	hookJump.waiting  =      (phHookWaiting_t *)     NULL;
	hookJump.popup  =        (phHookPopup_t *)       NULL;
    }
    *available = (phHookAvailability_t)  0;

    if(path == NULL)
    {
        phLogFrameMessage(logger, PHLOG_TYPE_FATAL, "Library path is NULL");
        return PHPLUG_ERR_LOAD;
    }

    /* prepare loading of a new library */
    strncpy(libname, path, sizeof(libname) - 1);
    libnameLength = strlen(libname);

    /* cat a '/', if missing but only if not an empty relative path */
    if (libnameLength > 0 && libname[libnameLength-1] != PATH_DELIMITER)
    {
	libname[libnameLength+1] = '\0';
	libname[libnameLength] = PATH_DELIMITER;
    }

    /* cat the library name */
    strcat(libname, "lib/");
    strcat(libname, HOOK_LIB_NAME);

    /* this is a hook library and is completely optional, so first
       check whether it exists and exit, if not */
    testFile = fopen(libname, "r");
    if (!testFile)
    {
	/* hook library does not exist, print a message and return */
	phLogFrameMessage(logger, LOG_DEBUG,
	    "hook plugin does not exist (ignored)");
	return PHPLUG_ERR_NEXIST;
    }
    fclose(testFile);
    testFile = NULL;

    /* now load the library */
    phLogFrameMessage(logger, LOG_DEBUG,
	"trying to load hook plugin %s ...", libname);
    libHandle = dlopen(libname, RTLD_NOW|RTLD_GLOBAL);
    if (libHandle == NULL)
    {
        /* an error occured, library could not be loaded. This is a
           fatal situation during driver initialization.... */
         phLogFrameMessage(logger, PHLOG_TYPE_FATAL,
                "File: '%s'\n'%s'",
                libname,
                dlerror());

        return PHPLUG_ERR_LOAD;
    }

    /* else: the library was loaded, check consistency */

    /* first try to call the 'availability' report function */
    hookJump.available = (phHookAvailable_t *) 
	checkFunction(logger, libHandle, "phHookAvailable");
    if (! hookJump.available)
	hookJump.reported = (phHookAvailability_t) 0;	
    else
	hookJump.reported = (*(hookJump.available))();

    /* now try to find the interface functions of the plugin */
    hookJump.found = (phHookAvailability_t) 0;	

    hookJump.init = (phHookInit_t *) 
	checkHookFunction(logger, libHandle, hookJump.reported, 
	"phHookInit", PHHOOK_AV_INIT);
    if (hookJump.init)
	hookJump.found = (phHookAvailability_t)(hookJump.found | PHHOOK_AV_INIT);

    hookJump.actionStart = (phHookActionStart_t *) 
	checkHookFunction(logger, libHandle, hookJump.reported, 
	"phHookActionStart", PHHOOK_AV_ACTION_START);
    if (hookJump.actionStart)
      hookJump.found = (phHookAvailability_t)(hookJump.found | PHHOOK_AV_ACTION_START);

    hookJump.actionDone = (phHookActionDone_t *) 
	checkHookFunction(logger, libHandle, hookJump.reported, 
	"phHookActionDone", PHHOOK_AV_ACTION_DONE);
    if (hookJump.actionDone)
      hookJump.found = (phHookAvailability_t)(hookJump.found | PHHOOK_AV_ACTION_DONE);

    hookJump.problem = (phHookProblem_t *) 
	checkHookFunction(logger, libHandle, hookJump.reported, 
	"phHookProblem", PHHOOK_AV_PROBLEM);
    if (hookJump.problem)
      hookJump.found = (phHookAvailability_t)(hookJump.found | PHHOOK_AV_PROBLEM);

    hookJump.waiting = (phHookWaiting_t *) 
	checkHookFunction(logger, libHandle, hookJump.reported, 
	"phHookWaiting", PHHOOK_AV_WAITING);
    if (hookJump.waiting)
      hookJump.found = (phHookAvailability_t)(hookJump.found | PHHOOK_AV_WAITING);

    hookJump.popup = (phHookPopup_t *) 
	checkHookFunction(logger, libHandle, hookJump.reported, 
	"phHookPopup", PHHOOK_AV_POPUP);
    if (hookJump.popup)
      hookJump.found = (phHookAvailability_t)(hookJump.found | PHHOOK_AV_POPUP);

    hookJump.used = (phHookAvailability_t)(hookJump.found & hookJump.reported);

    /* log final report message */
    if (hookJump.found == hookJump.reported)
	phLogFrameMessage(logger, LOG_DEBUG, 
	    "hook plugin loaded");
    else if (hookJump.used != (hookJump.used & hookJump.reported))
    {
	phLogFrameMessage(logger, PHLOG_TYPE_ERROR, 
	    "hook plugin loaded with restrictions");
	retVal = PHPLUG_ERR_DIFFER;
    }

    /* return result, only report functions that were found and were
       expected to be found */
    *available = hookJump.used;
    
    return retVal;
}

/*--- calling driver plugin functions ---------------------------------------*/

phPFuncAvailability_t phPlugPFuncAvailable(void)
{
    if (driverJump.available)
	return (*(driverJump.available))();
    else
	return (phPFuncAvailability_t) 0;
}

phPFuncError_t phPlugPFuncInit(
    phPFuncId_t *driverID     /* the resulting driver plugin ID */,
    phTcomId_t tester         /* the tester/tcom session to be used */,
    phComId_t communication   /* the valid communication link to the HW
			         interface used by the prober */,
    phLogId_t logger          /* the data logger to be used */,
    phConfId_t configuration  /* the configuration manager */,
    phEstateId_t estate       /* the prober state */
)
{
    if (driverJump.init)
	return (*(driverJump.init))(
	    driverID, tester, communication, logger, configuration, estate);
    else
	return PHPFUNC_ERR_NA;
}

phPFuncError_t phPlugPFuncReconfigure(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    if (driverJump.reconfigure)
	return (*(driverJump.reconfigure))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncReset(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    if (driverJump.reset)
	return (*(driverJump.reset))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncDriverID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
)
{
    if (driverJump.driverID)
	return (*(driverJump.driverID))(
	    proberID, driverIdString);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncEquipID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    if (driverJump.equipID)
	return (*(driverJump.equipID))(
	    proberID, equipIdString);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncLoadCass(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    if (driverJump.loadCass)
	return (*(driverJump.loadCass))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncUnloadCass(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    if (driverJump.unloadCass)
	return (*(driverJump.unloadCass))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncLoadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t source        /* where to load the wafer
                                           from. PHESTATE_WAFL_OUTCASS
                                           is not a valid option! */
)
{
    if (driverJump.loadWafer)
	return (*(driverJump.loadWafer))(
	    proberID, source);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncUnloadWafer(
    phPFuncId_t proberID                /* driver plugin ID */,
    phEstateWafLocation_t destination   /* where to unload the wafer
                                           to. PHESTATE_WAFL_INCASS is
                                           not valid option! */
)
{
    if (driverJump.unloadWafer)
	return (*(driverJump.unloadWafer))(
	    proberID, destination);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncGetDie(
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
    if (driverJump.getDie)
	return (*(driverJump.getDie))(
	    proberID, dieX, dieY);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncBinDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information or NULL, if
                                sub-die probing is active */,
    long *perSitePassed      /* pass/fail information (0=fail,
                                true=pass) on a per site basis or
                                NULL, if sub-die probing is active */
)
{
    if (driverJump.binDie)
	return (*(driverJump.binDie))(
	    proberID, perSiteBinMap, perSitePassed);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncGetSubDie(
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
    if (driverJump.getSubDie)
	return (*(driverJump.getSubDie))(
	    proberID, subdieX, subdieY);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncBinSubDie(
    phPFuncId_t proberID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information */,
    long *perSitePassed
)
{
    if (driverJump.binSubDie)
	return (*(driverJump.binSubDie))(
	    proberID, perSiteBinMap, perSitePassed);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncDieList(
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
    if (driverJump.dieList)
	return (*(driverJump.dieList))(
	    proberID, count, dieX, dieY);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncSubDieList(
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
    if (driverJump.subDieList)
	return (*(driverJump.subDieList))(
	    proberID, count, subdieX, subdieY);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncReprobe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    if (driverJump.reprobe)
	return (*(driverJump.reprobe))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncCleanProbe(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    if (driverJump.cleanProbe)
	return (*(driverJump.cleanProbe))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncPMI(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    if (driverJump.PMI)
	return (*(driverJump.PMI))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncSTPaused(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    if (driverJump.STPaused)
	return (*(driverJump.STPaused))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncSTUnpaused(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    if (driverJump.STUnpaused)
	return (*(driverJump.STUnpaused))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncTestCommand(
    phPFuncId_t proberID     /* driver plugin ID */,
    char *command            /* command string */
)
{
    if (driverJump.testCommand)
	return (*(driverJump.testCommand))(
	    proberID, command);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncDiag(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **diag              /* pointer to probers diagnostic output */
)
{
    if (driverJump.diag)
	return (*(driverJump.diag))(
	    proberID, diag);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncStatus(
    phPFuncId_t proberID                /* driver plugin ID */,
    phPFuncStatRequest_t action         /* the current status action
                                           to perform */,
    phPFuncAvailability_t *lastCall     /* the last call to the
                                           plugin, not counting calls
                                           to this function */
)
{
    if (driverJump.status)
	return (*(driverJump.status))(
	    proberID, action, lastCall);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncUpdateState(
    phPFuncId_t proberID                /* driver plugin ID */
)
{
    if (driverJump.update)
	return (*(driverJump.update))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncCassID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **cassIdString      /* resulting pointer to cassette ID string */
)
{
    if (driverJump.cassID)
	return (*(driverJump.cassID))(
	    proberID, cassIdString);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncWafID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **wafIdString       /* resulting pointer to wafer ID string */
)
{
    if (driverJump.wafID)
	return (*(driverJump.wafID))(
	    proberID, wafIdString);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncProbeID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **probeIdString     /* resulting pointer to probe card ID string */
)
{
    if (driverJump.probeID)
	return (*(driverJump.probeID))(
	    proberID, probeIdString);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncLotID(
    phPFuncId_t proberID     /* driver plugin ID */,
    char **lotIdString       /* resulting pointer to lot ID string */
)
{
    if (driverJump.lotID)
	return (*(driverJump.lotID))(
	    proberID, lotIdString);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncStartLot(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    if (driverJump.startLot)
	return (*(driverJump.startLot))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncEndLot(
    phPFuncId_t proberID       /* driver plugin ID */
)
{
    if (driverJump.endLot)
	return (*(driverJump.endLot))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


phPFuncError_t phPlugPFuncCommTest(
    phPFuncId_t proberID     /* driver plugin ID */,
    int *testPassed          /* test result */
)
{
    if (driverJump.commTest)
	return (*(driverJump.commTest))(
	    proberID, testPassed);
    else
	return PHPFUNC_ERR_NA;
}

phPFuncError_t phPlugPFuncGetStatus(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *commandString       /* key name */,
    char **responseString     /* output of response string */
    )
{
  if( driverJump.getStatus != NULL ) {
    return (*(driverJump.getStatus))(
                                     proberID,
                                     commandString,
                                     responseString
                                     );
  } else {
    return PHPFUNC_ERR_NA;
  }
}

phPFuncError_t phPlugPFuncSetStatus(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *commandString       /* key name */,
    char **responseString     /* output of response string */
    )
{
  if( driverJump.setStatus != NULL ) {
    return (*(driverJump.setStatus))(
                                     proberID,
                                     commandString,
                                     responseString
                                     );
  } else {
    return PHPFUNC_ERR_NA;
  }
}

phPFuncError_t phPlugPFuncDestroy(
    phPFuncId_t proberID     /* driver plugin ID */
)
{
    if (driverJump.destroy)
	return (*(driverJump.destroy))(
	    proberID);
    else
	return PHPFUNC_ERR_NA;
}


void phPlugPFuncSimResults(
    phPFuncError_t *dummyReturnValues,
    int dummyReturnCount
)
{
    if (driverJump.simResults)
	(*(driverJump.simResults))(
	    dummyReturnValues, dummyReturnCount);
}

/*CR21589, fabarca & kun xiao 16 Jun 2006*/
phPFuncError_t phPlugPFuncContacttest(
    phPFuncId_t proberID                        /* driver plugin ID */,
    phContacttestSetup_t  contacttestSetupID    /* resulting pointer to data structure */
)
{
    if (driverJump.contacttest)
    {
        return (*(driverJump.contacttest))(
            proberID, contacttestSetupID);
    }
    else
    {
        return PHPFUNC_ERR_NA;
    }
}

phPFuncError_t phPlugPFuncExecGpibCmd(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *commandString       /* key name */,
    char **responseString     /* output of response string */
    )
{
  if( driverJump.execGpibCmd != NULL ) {
    return (*(driverJump.execGpibCmd))(
                                     proberID,
                                     commandString,
                                     responseString
                                     );
  } else {
    return PHPFUNC_ERR_NA;
  }
}

phPFuncError_t phPlugPFuncExecGpibQuery(
    phPFuncId_t proberID      /* driver plugin ID */,
    char *commandString       /* key name */,
    char **responseString     /* output of response string */
    )
{
  if( driverJump.execGpibQuery != NULL ) {
    return (*(driverJump.execGpibQuery))(
                                     proberID,
                                     commandString,
                                     responseString
                                     );
  } else {
    return PHPFUNC_ERR_NA;
  }
}

/*--- calling hook plugin functions -----------------------------------------*/

phHookAvailability_t phPlugHookAvailable(void)
{
    if (hookJump.available)
	return (*(hookJump.available))();
    else
	return (phHookAvailability_t) 0;
}

phHookError_t phPlugHookInit(
    phPFuncId_t driver            /* the resulting driver plugin ID */,
    phComId_t communication      /* the valid communication link to the HW
			            interface used by the handler */,
    phLogId_t logger             /* the data logger to be used */,
    phConfId_t configuration     /* the configuration manager */,
    phStateId_t state            /* the overall driver state */,
    phEstateId_t estate          /* the equipment state */,
    phTcomId_t tester            /* the tester interface */
)
{
    if (hookJump.init)
	return (*(hookJump.init))(
	    driver, communication, logger, configuration, state, estate, 
	    tester);
    else
	return PHHOOK_ERR_NA;
}

phHookError_t phPlugHookActionStart(
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
    if (hookJump.actionStart)
	return (*(hookJump.actionStart))(
	    call, result, parm_list_input, parmcount, 
	    comment_out, comlen, stReturn);
    else
	return PHHOOK_ERR_NA;
}

phHookError_t  phPlugHookActionDone(
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
    if (hookJump.actionDone)
	return (*(hookJump.actionDone))(
	    call, parm_list_input, parmcount, comment_out, comlen, stReturn);
    else
	return PHHOOK_ERR_NA;
}

phHookError_t     phPlugHookProblem(
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
    if (hookJump.problem)
	return (*(hookJump.problem))(
	    reason, data, result, stReturn);
    else
	return PHHOOK_ERR_NA;
}

phHookError_t     phPlugHookWaiting(
    long elapsedTimeSec           /* elapsed time while waiting for
				     this part */,
    phEventResult_t *result       /* the result of this hook call */,
    phTcomUserReturn_t *stReturn  /* proposed SmarTest return value, if
				     <result> is set to
				     PHHOOK_RES_ABORT by this hook
				     function */
)
{
    if (hookJump.waiting)
	return (*(hookJump.waiting))(
	    elapsedTimeSec, result, stReturn);
    else
	return PHHOOK_ERR_NA;
}

phHookError_t       phPlugHookPopup(
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
    if (hookJump.popup)
	return (*(hookJump.popup))(
	    result, stReturn, needQuitAndCont,
	    msg, b2s, b3s, b4s, b5s, b6s, b7s, 
	    response, input);
    else
	return PHHOOK_ERR_NA;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
