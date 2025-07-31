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
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 1999, Michael Vogt, created
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
#include "ph_hfunc.h"
#include "ph_hhook.h"
#include "ph_plugin.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/
#define DRV_LIB_NAME "libfunc.so"
#define HOOK_LIB_NAME "libhook.so"

#define PATH_DELIMITER '/'

/*--- typedefs --------------------------------------------------------------*/

/*--- gobal variables -------------------------------------------------------*/

static struct driverPlugin driverJump =
{
    (phFuncAvailability_t)      0,
    (phFuncAvailability_t)      0,
    (phFuncAvailability_t)      0,

    (phFuncAvailable_t *)       NULL,
    (phFuncInit_t *)            NULL,
    (phFuncReconfigure_t *)     NULL,
    (phFuncReset_t *)           NULL,
    (phFuncDriverID_t *)        NULL,
    (phFuncEquipID_t *)         NULL,
    (phFuncGetStart_t *)        NULL,
    (phFuncBinDevice_t *)       NULL,
    (phFuncReprobe_t *)         NULL,
    (phFuncSendCommand_t *)     NULL,
    (phFuncSendQuery_t *)       NULL,
    (phFuncDiag_t *)            NULL,
    (phFuncBinReprobe_t *)      NULL,
    (phFuncSTPaused_t *)        NULL,
    (phFuncSTUnpaused_t *)      NULL,
    (phFuncStatus_t *)          NULL,
    (phFuncUpdateState_t *)     NULL,
    (phFuncCommTest_t *)        NULL,
    (phFuncDestroy_t *)         NULL,
    (phFuncStripIndexID_t *)    NULL,
    (phFuncStripMaterialID_t *) NULL,
    (phFuncGetStatus_t *)       NULL,
    (phFuncSetStatus_t *)       NULL,
    (phFuncLotStart_t *)        NULL,
    (phFuncLotDone_t *)         NULL,
    (phFuncStripStart_t *)      NULL,
    (phFuncStripDone_t *)       NULL,
    (phFuncExecGpibCmd_t *) NULL,
    (phFuncExecGpibQuery_t *) NULL,
    (phFuncGetSrqStatusByte_t *) NULL
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
                              "could not resolve function '%s' in shared library (error: )",
                              symbol ? symbol : "(NULL)", dlerror()); 
        }
     }

    return voidPtr;
}

static void *checkDriverFunction(
    phLogId_t logger, 
    void *libHandle, 
    phFuncAvailability_t reportedAv, 
    const char *symbol,
    phFuncAvailability_t symbolAv)
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
    phFuncAvailability_t *available     /* returns the set of
					   available driver plugin functions */
)
{
    char libname[1024];
    int libnameLength;

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

	driverJump.found =       (phFuncAvailability_t)  0;
	driverJump.reported =    (phFuncAvailability_t)  0;
	driverJump.used =        (phFuncAvailability_t)  0;
	driverJump.available =   (phFuncAvailable_t *)   NULL;
	driverJump.init =        (phFuncInit_t *)        NULL;
	driverJump.reconfigure = (phFuncReconfigure_t *) NULL;
	driverJump.reset =       (phFuncReset_t *)       NULL;
	driverJump.driverID =    (phFuncDriverID_t *)    NULL;
	driverJump.equipID =     (phFuncEquipID_t *)     NULL;
	driverJump.getStart =    (phFuncGetStart_t *)    NULL;
	driverJump.binDevice =   (phFuncBinDevice_t *)   NULL;
	driverJump.reprobe =     (phFuncReprobe_t *)     NULL;
	driverJump.sendCommand = (phFuncSendCommand_t *) NULL;
	driverJump.sendQuery =   (phFuncSendQuery_t *)   NULL;
	driverJump.diag =        (phFuncDiag_t *)        NULL;
	driverJump.binReprobe =  (phFuncBinReprobe_t *)  NULL;
	driverJump.STPaused =    (phFuncSTPaused_t *)    NULL;
	driverJump.STUnpaused =  (phFuncSTUnpaused_t *)  NULL;
	driverJump.status =      (phFuncStatus_t *)      NULL;
	driverJump.update =      (phFuncUpdateState_t *) NULL;
	driverJump.commTest =    (phFuncCommTest_t *)    NULL;
	driverJump.destroy =     (phFuncDestroy_t *)     NULL;
	driverJump.stripID =     (phFuncStripIndexID_t *)     NULL;
	driverJump.materialID =  (phFuncStripMaterialID_t *)     NULL;
    driverJump.getStatus =   (phFuncGetStatus_t *)       NULL;
    driverJump.setStatus =   (phFuncSetStatus_t *)       NULL;
    driverJump.lotStart =    (phFuncLotStart_t *)       NULL;
    driverJump.lotDone =     (phFuncLotDone_t *)       NULL;
    driverJump.stripStart =    (phFuncStripStart_t *)       NULL;
    driverJump.stripDone =    (phFuncStripDone_t *)       NULL;
    driverJump.execGpibCmd = (phFuncExecGpibCmd_t *) NULL;
    driverJump.execGpibQuery = (phFuncExecGpibQuery_t *) NULL;
    driverJump.getSrqStatusByte = (phFuncGetSrqStatusByte_t *) NULL;
    }
    *available = (phFuncAvailability_t)  0;

    /* prepare loading of a new library */
    strcpy(libname, path);
    libnameLength = strlen(libname);

    /* cat a '/', if missing but only if not an empty relative path */
    if (libnameLength > 0 && libname[libnameLength-1] != PATH_DELIMITER)
    {
	libname[libnameLength+1] = '\0';
	libname[libnameLength] = PATH_DELIMITER;
    }

    char temp[1024] = {0};
    strcpy(temp, libname);
    char *ptr = NULL;
    if ((ptr = strrchr(temp, '/')) != NULL) {
        *ptr = '\0';
    } else {
        phLogFrameMessage(logger, PHLOG_TYPE_FATAL,
            "The path '%s' is not available ", libname);
        return PHPLUG_ERR_LOAD;
    }
    if ((ptr = strrchr(temp, '/')) != NULL) {
        ptr+=1;
    } else {
        ptr = temp;
    }

    if (strcasecmp(ptr, "DLH") == 0) {
        ptr = getenv("DRIVER_IS_VERIFIED_WITH_REAL_HANDLER");
        if (ptr == NULL || strcasecmp(ptr, "TRUE") != 0) {
            phLogFrameMessage(logger, PHLOG_TYPE_FATAL,
                "It is not supported since this driver '%s' has not been verified with real "
                "handler", libname);
            return PHPLUG_ERR_LOAD;
        }
    }

    /* cat the library name */
    strcat(libname, "lib/");
    strcat(libname, DRV_LIB_NAME);

    /* now load the library */
    phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_0,
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
    driverJump.available = (phFuncAvailable_t *) 
	checkFunction(logger, libHandle, "phFuncAvailable");
    if (! driverJump.available)
	driverJump.reported = (phFuncAvailability_t) 0;	
    else
	driverJump.reported = (*(driverJump.available))();

    /* now try to find the interface functions of the plugin */
    driverJump.found = (phFuncAvailability_t) 0;	

    driverJump.init = (phFuncInit_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncInit", PHFUNC_AV_INIT);
    if (driverJump.init)
	driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_INIT);

    driverJump.reconfigure = (phFuncReconfigure_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncReconfigure", PHFUNC_AV_RECONFIGURE);
    if (driverJump.reconfigure)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_RECONFIGURE);

    driverJump.reset = (phFuncReset_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncReset", PHFUNC_AV_RESET);
    if (driverJump.reset)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_RESET);

    driverJump.driverID = (phFuncDriverID_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncDriverID", PHFUNC_AV_DRIVERID);
    if (driverJump.driverID)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_DRIVERID);

    driverJump.equipID = (phFuncEquipID_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncEquipID", PHFUNC_AV_EQUIPID);
    if (driverJump.equipID)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_EQUIPID);

    driverJump.getStart = (phFuncGetStart_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncGetStart", PHFUNC_AV_START);
    if (driverJump.getStart)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_START);

    driverJump.binDevice = (phFuncBinDevice_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncBinDevice", PHFUNC_AV_BIN);
    if (driverJump.binDevice)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_BIN);

    driverJump.reprobe = (phFuncReprobe_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncReprobe", PHFUNC_AV_REPROBE);
    if (driverJump.reprobe)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_REPROBE);

    driverJump.sendCommand = (phFuncSendCommand_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncSendCommand", PHFUNC_AV_COMMAND);
    if (driverJump.sendCommand)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_COMMAND);

    driverJump.sendQuery = (phFuncSendQuery_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncSendQuery", PHFUNC_AV_QUERY);
    if (driverJump.sendQuery)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_QUERY);

    driverJump.diag = (phFuncDiag_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncDiag", PHFUNC_AV_DIAG);
    if (driverJump.diag)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_DIAG);

    driverJump.binReprobe = (phFuncBinReprobe_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncBinReprobe", PHFUNC_AV_BINREPR);
    if (driverJump.binReprobe)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_BINREPR);

    driverJump.STPaused = (phFuncSTPaused_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncSTPaused", PHFUNC_AV_PAUSE);
    if (driverJump.STPaused)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_PAUSE);

    driverJump.STUnpaused = (phFuncSTUnpaused_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncSTUnpaused", PHFUNC_AV_UNPAUSE);
    if (driverJump.STUnpaused)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_UNPAUSE);

    driverJump.status = (phFuncStatus_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncStatus", PHFUNC_AV_STATUS);
    if (driverJump.status)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_STATUS);

    driverJump.update = (phFuncUpdateState_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncUpdateState", PHFUNC_AV_UPDATE);
    if (driverJump.update)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_UPDATE);

    driverJump.commTest = (phFuncCommTest_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncCommTest", PHFUNC_AV_COMMTEST);
    if (driverJump.commTest)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_COMMTEST);

    driverJump.destroy = (phFuncDestroy_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncDestroy", PHFUNC_AV_DESTROY);
    if (driverJump.destroy)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_DESTROY);

    /* kaw add Oct 14 2004 */
    driverJump.stripID = (phFuncStripIndexID_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncStripIndexID", PHFUNC_AV_STRIP_INDEXID);
    if (driverJump.stripID)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_STRIP_INDEXID);

    driverJump.materialID = (phFuncStripMaterialID_t *) 
	checkDriverFunction(logger, libHandle, driverJump.reported, 
	"phFuncStripMaterialID", PHFUNC_AV_STRIPID);
    if (driverJump.materialID)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_STRIPID);
    /* kaw end */

    /* kaw add Feb 07 2005 */
    driverJump.getStatus = (phFuncGetStatus_t *) 
    checkDriverFunction(logger, libHandle, driverJump.reported, 
    "phFuncGetStatus", PHFUNC_AV_GET_STATUS);
    if (driverJump.getStatus)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_GET_STATUS);

    driverJump.setStatus = (phFuncSetStatus_t *)
    checkDriverFunction(logger, libHandle, driverJump.reported, 
    "phFuncSetStatus", PHFUNC_AV_SET_STATUS);
    if (driverJump.setStatus)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_SET_STATUS);
    /* kaw end Feb 07 2005 */

    /* kaw add Feb 23 2005 */
    driverJump.lotStart = (phFuncLotStart_t *) 
    checkDriverFunction(logger, libHandle, driverJump.reported, 
    "phFuncLotStart", PHFUNC_AV_LOT_START);
    if (driverJump.lotStart)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_LOT_START);

    driverJump.lotDone = (phFuncLotDone_t *) 
    checkDriverFunction(logger, libHandle, driverJump.reported, 
    "phFuncLotDone", PHFUNC_AV_LOT_DONE);
    if (driverJump.lotDone)
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_LOT_DONE);
    /* kaw end Feb 23 2005 */

    /* magco li add Oct 30 2014 */
    driverJump.stripStart = (phFuncStripStart_t *)
    checkDriverFunction(logger, libHandle, driverJump.reported,
    "phFuncStripStart", PHFUNC_AV_STRIP_START);
    if (driverJump.stripStart)
    driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_STRIP_START);

    driverJump.stripDone = (phFuncStripDone_t *)
    checkDriverFunction(logger, libHandle, driverJump.reported,
    "phFuncStripDone", PHFUNC_AV_STRIP_DONE);
    if (driverJump.stripDone)
    driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_STRIP_DONE);
    /* magco li end Oct 30 2014 */

    driverJump.execGpibCmd = (phFuncExecGpibCmd_t *) checkDriverFunction(
      logger, libHandle, driverJump.reported, "phFuncExecGpibCmd", PHFUNC_AV_EXECGPIBCMD);
    if( driverJump.execGpibCmd ) {
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_EXECGPIBCMD);
    }

    driverJump.execGpibQuery = (phFuncExecGpibQuery_t *) checkDriverFunction(
      logger, libHandle, driverJump.reported, "phFuncExecGpibQuery", PHFUNC_AV_EXECGPIBQUERY);
    if( driverJump.execGpibQuery ) {
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_EXECGPIBQUERY);
    }

    driverJump.getSrqStatusByte = (phFuncGetSrqStatusByte_t *) checkDriverFunction(
      logger, libHandle, driverJump.reported, "phFuncGetSrqStatusByte", PHFUNC_AV_GETSRQSTATUSBYTE);
    if( driverJump.getSrqStatusByte ) {
      driverJump.found = (phFuncAvailability_t)(driverJump.found | PHFUNC_AV_GETSRQSTATUSBYTE);
    }

    
    driverJump.used = (phFuncAvailability_t)(driverJump.found & driverJump.reported);

    /* log final report message */
    if (driverJump.found == driverJump.reported)
	phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_0, 
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
    FILE *testFile;

    static void *libHandle = NULL;

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
    strcpy(libname, path);
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
	phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_0,
	    "hook plugin does not exist (ignored)");
	return PHPLUG_ERR_NEXIST;
    }
    fclose(testFile);
    testFile = NULL;

    /* now load the library */
    phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_0,
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
	phLogFrameMessage(logger, PHLOG_TYPE_MESSAGE_0, 
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

phFuncAvailability_t phPlugFuncAvailable(void)
{
    if (driverJump.available)
	return (*(driverJump.available))();
    else
	return (phFuncAvailability_t) 0;
}

phFuncError_t phPlugFuncInit(
    phFuncId_t *driverID     /* the resulting driver plugin ID */,
    phTcomId_t tester        /* the tester/tcom session to be used */,
    phComId_t communication  /* the valid communication link to the HW
			        interface used by the handler */,
    phLogId_t logger         /* the data logger to be used */,
    phConfId_t configuration /* the configuration manager */,
    phEstateId_t estate      /* the driver state */
)
{
    if (driverJump.init)
	return (*(driverJump.init))(
	    driverID, tester, communication, logger, configuration, estate);
    else
	return PHFUNC_ERR_NA;
}


phFuncError_t phPlugFuncReconfigure(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    if (driverJump.reconfigure)
	return (*(driverJump.reconfigure))(
	    handlerID);
    else
	return PHFUNC_ERR_NA;
}


phFuncError_t phPlugFuncReset(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    if (driverJump.reset)
	return (*(driverJump.reset))(
	    handlerID);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncDriverID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **driverIdString    /* resulting pointer to driver ID string */
)
{
    if (driverJump.driverID)
	return (*(driverJump.driverID))(
	    handlerID, driverIdString);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncEquipID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    if (driverJump.equipID)
	return (*(driverJump.equipID))(
	    handlerID, equipIdString);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncStripIndexID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    if (driverJump.stripID)
	return (*(driverJump.stripID))(
	    handlerID, equipIdString);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncStripMaterialID(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **equipIdString     /* resulting pointer to equipment ID string */
)
{
    if (driverJump.materialID)
	return (*(driverJump.materialID))(
	    handlerID, equipIdString);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncGetStart(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    if (driverJump.getStart)
	return (*(driverJump.getStart))(
	    handlerID);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncBinDevice(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteBinMap       /* */
)
{
    if (driverJump.binDevice)
	return (*(driverJump.binDevice))(
	    handlerID, perSiteBinMap);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncReprobe(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    if (driverJump.reprobe)
	return (*(driverJump.reprobe))(
	    handlerID);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncSendCommand(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *command            /* command string */
)
{
    if (driverJump.sendCommand)
	return (*(driverJump.sendCommand))(
	    handlerID, command);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncSendQuery(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *query              /* query string */,
    char **answer            /* pointer to answer string */
)
{
    if (driverJump.sendQuery)
	return (*(driverJump.sendQuery))(
	    handlerID, query, answer);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncDiag(
    phFuncId_t handlerID     /* driver plugin ID */,
    char **diag              /* pointer to handlers diagnostic output */
)
{
    if (driverJump.diag)
	return (*(driverJump.diag))(
	    handlerID, diag);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncBinReprobe(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteReprobe     /* TRUE, if a device needs reprobe*/,
    long *perSiteBinMap      /* valid binning data for each site where
                                the above reprobe flag is not set */
)
{
    if (driverJump.binReprobe)
	return (*(driverJump.binReprobe))(
	    handlerID, perSiteReprobe, perSiteBinMap);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncSTPaused(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    if (driverJump.STPaused)
	return (*(driverJump.STPaused))(
	    handlerID);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncSTUnpaused(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    if (driverJump.STUnpaused)
	return (*(driverJump.STUnpaused))(
	    handlerID);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncStatus(
    phFuncId_t handlerID     /* driver plugin ID */,
    phFuncStatRequest_t action          /* the current status action
                                           to perform */,
    phFuncAvailability_t *lastCall      /* the last call to the
                                           plugin, not counting calls
                                           to this function */
)
{
    if (driverJump.status)
	return (*(driverJump.status))(
	    handlerID, action, lastCall);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncUpdateState(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    if (driverJump.update)
	return (*(driverJump.update))(
	    handlerID);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncCommTest(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *testPassed          /* test result */
)
{
    if (driverJump.commTest)
	return (*(driverJump.commTest))(
	    handlerID, testPassed);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncDestroy(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    if (driverJump.destroy)
	return (*(driverJump.destroy))(
	    handlerID);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncGetStatus(
    phFuncId_t handlerID     /* driver plugin ID */,
    char *commandString      /* pointer to string containing status to Get */
                             /* and parameters                             */,
    char **responseString    /* pointer to the response string */
)
{
    if (driverJump.getStatus)
	return (*(driverJump.getStatus))(
	    handlerID, commandString, responseString);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncSetStatus(
    phFuncId_t handlerID,    /* driver plugin ID */
    char *commandString      /* pointer to string containing status to Get */
                             /* and parameters                             */
)
{
    if (driverJump.setStatus)
	return (*(driverJump.setStatus))(handlerID, commandString);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncLotStart(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm                /* if == NOTIMEOUT, then will not return until gets an SRQ */
)
{
    if (driverJump.lotStart)
    return (*(driverJump.lotStart))(handlerID, parm);
    else
	return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncLotDone(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm                
)
{
    if (driverJump.lotDone)
    return (*(driverJump.lotDone))(handlerID, parm);
    else
	return PHFUNC_ERR_NA;
}

/*magco li start Oct 24 2014 */
phFuncError_t phPlugFuncStripStart(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm                /* if == NOTIMEOUT, then will not return until gets an SRQ */
)
{
    if (driverJump.stripStart)
    return (*(driverJump.stripStart))(handlerID, parm);
    else
        return PHFUNC_ERR_NA;
}

phFuncError_t phPlugFuncStripDone(
    phFuncId_t handlerID,     /* driver plugin ID */
    char *parm
)
{
    if (driverJump.stripDone)
    return (*(driverJump.stripDone))(handlerID, parm);
    else
        return PHFUNC_ERR_NA;
}
/*magco li end Oct 24 2014 */

phFuncError_t phPlugFuncExecGpibCmd(
    phFuncId_t handlerID      /* driver plugin ID */,
    char *commandString       /* key name */,
    char **responseString     /* output of response string */
    )
{
  if( driverJump.execGpibCmd != NULL ) {
    return (*(driverJump.execGpibCmd))(
                                     handlerID,
                                     commandString,
                                     responseString
                                     );
  } else {
    return PHFUNC_ERR_NA;
  }
}

phFuncError_t phPlugFuncExecGpibQuery(
    phFuncId_t handlerID      /* driver plugin ID */,
    char *commandString       /* key name */,
    char **responseString     /* output of response string */
    )
{
  if( driverJump.execGpibQuery != NULL ) {
    return (*(driverJump.execGpibQuery))(
                                     handlerID,
                                     commandString,
                                     responseString
                                     );
  } else {
    return PHFUNC_ERR_NA;
  }
}


phFuncError_t phPlugFuncGetSrqStatusByte(
    phFuncId_t handlerID      /* driver plugin ID */,
    char *commandString       /* key name */,
    char **responseString     /* output of response string */
    )
{
  if( driverJump.getSrqStatusByte != NULL ) {
    return (*(driverJump.getSrqStatusByte))(
                                     handlerID,
                                     commandString,
                                     responseString
                                     );
  } else {
    return PHFUNC_ERR_NA;
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
    if (hookJump.init)
	return (*(hookJump.init))(
	    driver, communication, logger, configuration, state, estate, tester);
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
