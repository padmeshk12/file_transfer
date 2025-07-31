/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : reconfigure.c
 * CREATED  : 16 Jul 1999
 *
 * CONTENTS : Perform (re)configuration tasks
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *            Jiawei Lin, R&D Shanghai, CR42750
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 16 Jul 1999, Michael Vogt, created
 *            Dec 2005, Jiawei Lin, CR28409
 *              in "reconfigureFramework" function, support to read
 *              additional driver parameters "enable_diagnose_window"
 *            Oct 2008, Jiawei Lin,
 *                CR42750, support the "debug_level" parameter set as 9
 *                in which case the timestamp information will get logged
 *            May 2009, Jiawei Lin,
 *                CR45014, extend the "debug_level" to -2, -3, -4
 *                to provide feature that WARNING, ERROR, FATAL message
 *                is able to be suppressed
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
#include "ph_keys.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_mhcom.h"
#include "ph_hfunc.h"
#include "ph_hhook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"
#include "ph_tools.h"

#include "ph_frame_private.h"
#include "reconfigure.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: <your name(s)>
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/

static void saveLogFile(struct phFrameStruct *frame, char *name, int max)
{
    int i;
    char command[2048];

    if (!name || *name == '\0' || max <=0)
	return;

    for (i=max-1; i>=0; i--)
    {
	if (i>0)
	    sprintf(command, "mv -f %s_old_%d %s_old_%d > /dev/null 2>&1", 
		name, i, name, i+1);
	else
	    sprintf(command, "mv -f %s %s_old_%d > /dev/null 2>&1",
		name, name, i+1);

	phLogFrameMessage(frame->logId, PHLOG_TYPE_MESSAGE_1,
	    "trying to save old driver log file: %s", command);
	system(command);
    }
}

/*****************************************************************************
 *
 * Substitute pathnames to full path names
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, created
 *
 * Description: The passed string <path> is searched for portions of
 * the form $[^/$]*. The found portions are replaced by values
 * found in the process' environment. If the environment does not
 * define the string, an empty string isphConfConfString assumed.
 *
 ***************************************************************************/
void reconfigureCompletePath(char *complete, const char *path)
{
    char *readPtr;
    char *insertPtr;
    char *copy;
    char *tmp;
    const char *env;

    char saveChr;

    copy = strdup(path);
    readPtr = copy;
    insertPtr = complete;

    while (*readPtr != '\0')
    {
	/* copy non environment variable part */
	while (*readPtr != '$' && *readPtr != '\0')
	    *insertPtr++ = *readPtr++;

	/* copy environment variable part */
	if (*readPtr == '$')
	{
	    readPtr++;
	    tmp = readPtr;

	    /* find end of env variable name */
	    while (*readPtr != '$' && *readPtr != '/' && *readPtr != '\0')
		readPtr++;

	    /* save end marker, temporary replaced by NUL */
	    saveChr = *readPtr;
	    *readPtr = '\0';
	    env = getenv(tmp);
	    *readPtr = saveChr;
	    
	    /* copy found value */
	    if (env)
	    {
		strcpy(insertPtr, env);
		insertPtr += strlen(env);
	    }
	}
    }

    /* mark end, clean up */
    *insertPtr = '\0';
    free(copy);
}

/*****************************************************************************
 *
 * Reconfigure the driver's logger
 *
 * Authors: Michael Vogt
 *
 * History: 16 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to reconfigure.h
 *
 ****************************************************************************/
void reconfigureLogger(
    struct phFrameStruct *frame, int *resErrors, int *resWarnings)
{
    phConfError_t confError;
    phLogMode_t modeFlags;

    double dLogFileKeep;
    int logFileKeep;
    double dDebugLevel;
    int debugLevel;
    const char *driverErrLog = NULL;
    char usedDriverErrLog[1024];
    const char *driverMsgLog = NULL;
    char usedDriverMsgLog[1024];
    const char *traceMode = NULL;
    const char *prefixText = NULL;

    static char *lastDriverErrLog = NULL;
    static char *lastDriverMsgLog = NULL;

    int logTimeStampFlag = 0;

    *resWarnings = 0;
    *resErrors = 0;

    /* get the debug level and configure logger */
    if (phConfConfIfDef(frame->specificConfId, PHKEY_MO_DBGLEV) ==
	PHCONF_ERR_OK)
    {
	confError = phConfConfNumber(frame->specificConfId,
	    PHKEY_MO_DBGLEV, 0, NULL, &dDebugLevel);
	if (confError == PHCONF_ERR_OK)
	{
	    debugLevel = (int) dDebugLevel;

	    /* get current log mode and remove all debug flags */
	    phLogGetMode(frame->logId, &modeFlags);   /*the default modeFlags = PHLOG_MODE_DBG_0|PHLOG_MODE_WARN*/

	    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_0);
	    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_1);
	    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_2);
	    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_3);
	    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_DBG_4);

      /*now, the modeFlags = PHLOG_MODE_WARN */

      switch (debugLevel)
      {
          case -4:
              /* really quiet both for normal debug and exception message; every thing is not logged */
              modeFlags = PHLOG_MODE_QUIET; 
              break;
          case -3:
              modeFlags = PHLOG_MODE_FATAL;
              break;
          case -2:
              modeFlags = PHLOG_MODE_ERROR;
              break;
          case -1:
              /* not really quiet, actually modeFlags = PHLOG_MODE_WARN, only quiet for normal debug message */
              break;
          case 0:
              modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_0);
              break;
          case 1:
            modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_1);
              break;
          case 2:
            modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_2);
              break;
          case 3:
            modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_3);
              break;
          case 4:
            modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_4);
              break;
          case 9:   /*debug_level=9 means "PHLOG_MODE_DBG_4|log_time_stamp"*/
            modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_4);
              logTimeStampFlag = 1;
              break;
          default:
              /* incorrect value, do debug level 1 by default */
            modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_DBG_1);
              phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
                                "the value of debug level: %d is incorrect; the driver will adapt to use debug level 1", debugLevel);
              break;
        }

      /* replace log mode */
      phLogSetMode(frame->logId, modeFlags);

      phLogSetTimestampFlag(frame->logId, logTimeStampFlag);
	}
    }

    /* get the trace flag and configure logger */
    if (phConfConfIfDef(frame->specificConfId, PHKEY_MO_TRACE) ==
	PHCONF_ERR_OK)
    {
	confError = phConfConfString(frame->specificConfId, 
	    PHKEY_MO_TRACE, 0, NULL, &traceMode);
	if (confError == PHCONF_ERR_OK)
	{
	    /* get current log mode and remove all debug flags */
	    phLogGetMode(frame->logId, &modeFlags);
	    modeFlags = (phLogMode_t)(modeFlags & ~PHLOG_MODE_TRACE);

	    if (strcasecmp(traceMode, "yes") == 0)
		modeFlags = (phLogMode_t)(modeFlags | PHLOG_MODE_TRACE);
    
	    /* replace log mode */
	    phLogSetMode(frame->logId, modeFlags);
	}
    }


    /* set the prefix text of the log messages */

    if (phConfConfIfDef(frame->specificConfId, PHKEY_MO_PREFIXTEXT) == PHCONF_ERR_OK)
    {
        confError = phConfConfString(frame->specificConfId, 
                                     PHKEY_MO_PREFIXTEXT, 
                                     0, 
                                     NULL, 
                                     &prefixText);
        if (confError == PHCONF_ERR_OK)
        {
            phLogSetPrefixText(frame->logId, prefixText);
        }
        else
        {
            (*resWarnings)++;
        }
    }


    /* get the location of the final log files. Ignore the case, that
       log files have not been specified */

    usedDriverMsgLog[0] = '\0';
    if (phConfConfIfDef(frame->specificConfId, PHKEY_MO_MSGLOG) == 
	PHCONF_ERR_OK)
    {
	confError = phConfConfString(frame->specificConfId, 
	    PHKEY_MO_MSGLOG, 0, NULL, &driverMsgLog);
	if (confError == PHCONF_ERR_OK)
	    reconfigureCompletePath(usedDriverMsgLog, driverMsgLog);
	else
	    (*resWarnings)++;
    }
    else
	(*resWarnings)++;
    

    usedDriverErrLog[0] = '\0';
    if (phConfConfIfDef(frame->specificConfId, PHKEY_MO_ERRLOG) ==
	PHCONF_ERR_OK)
    {
	confError = phConfConfString(frame->specificConfId, 
	    PHKEY_MO_ERRLOG, 0, NULL, &driverErrLog);
	if (confError == PHCONF_ERR_OK)
	    reconfigureCompletePath(usedDriverErrLog, driverErrLog);
	else
	    (*resWarnings)++;
    }
    else
	(*resWarnings)++;

    if (*resWarnings)
	phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
	    "log files for driver not fully specified, ignored");

    /* save old versions of the log files, if necessary */
    logFileKeep = 0;
    if (phConfConfIfDef(frame->specificConfId, PHKEY_MO_LOGKEEP) == 
	PHCONF_ERR_OK)
    {
	confError = phConfConfNumber(frame->specificConfId, 
	    PHKEY_MO_LOGKEEP, 0, NULL, &dLogFileKeep);
	if (confError != PHCONF_ERR_OK)
	{
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
		"will not save existing driver log files, ignored");
	    (*resWarnings)++;
	}
	else
	    logFileKeep = (int) dLogFileKeep;
    }

    /* tell the logger to use the new file only, if the file name has
       changed or if this is the first change */
    if (!lastDriverMsgLog ||
	(driverMsgLog && strcmp(usedDriverMsgLog, lastDriverMsgLog) != 0))
    {
	saveLogFile(frame, usedDriverMsgLog, logFileKeep);
	phLogReplaceLogFiles(frame->logId, NULL, usedDriverMsgLog);
	if (lastDriverMsgLog)
	    free(lastDriverMsgLog);
	lastDriverMsgLog = strdup(usedDriverMsgLog);
    }

    if (!lastDriverErrLog ||
	(driverErrLog && strcmp(usedDriverErrLog, lastDriverErrLog) != 0))
    {
	saveLogFile(frame, usedDriverErrLog, logFileKeep);
	phLogReplaceLogFiles(frame->logId, usedDriverErrLog, NULL);
	if (lastDriverErrLog)
	    free(lastDriverErrLog);
	lastDriverErrLog = strdup(usedDriverErrLog);
    }

    return;
}

/*****************************************************************************
 *
 * Reconfigure the driver's state controller
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to reconfigure.h
 *
 ****************************************************************************/
void reconfigureStateController(
    struct phFrameStruct *frame, int *resErrors, int *resWarnings)
{
    phConfError_t confError;
    const char *handtestFlag = NULL;
    const char *singlestepFlag = NULL;
    int doHandtest = 0;
    int doSinglestep = 0;
    phStateDrvMode_t newMode;

    static phStateDrvMode_t lastMode = PHSTATE_DRV_NORMAL;

    *resWarnings = 0;
    *resErrors = 0;

    /* (Re)configure the hand test/single step mode */

    confError = phConfConfIfDef(frame->specificConfId, PHKEY_OP_HAND);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfString(frame->specificConfId, 
	    PHKEY_OP_HAND, 0, NULL, &handtestFlag);
    if (confError != PHCONF_ERR_OK)
    {
	(*resWarnings)++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
	    "hand test mode not specified, ignored");
    }
    else
    {
	if (strcasecmp(handtestFlag, "on") == 0)
	    doHandtest = 1;
    }

    confError = phConfConfIfDef(frame->specificConfId, PHKEY_OP_SST);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfString(frame->specificConfId, 
	    PHKEY_OP_SST, 0, NULL, &singlestepFlag);
    if (confError != PHCONF_ERR_OK)
    {
	(*resWarnings)++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
	    "single step mode not specified, ignored");
    }
    else
    {
	if (strcasecmp(singlestepFlag, "on") == 0)
	    doSinglestep = 1;
    }

    if (!doHandtest)
    {
	if (!doSinglestep)
	    newMode = PHSTATE_DRV_NORMAL;
	else
	    newMode = PHSTATE_DRV_SST;
    }
    else
    {
	if (!doSinglestep)
	    newMode = PHSTATE_DRV_HAND;
	else
	    newMode = PHSTATE_DRV_SST_HAND;
    }

    /* only change the driver mode, if it has changed in the
       configuration, not if it is different from the current
       mode.... */
    if (newMode != lastMode)
    {
	phStateSetDrvMode(frame->stateId, newMode);
	lastMode = newMode;
    }

    return;
}

/*****************************************************************************
 *
 * Reconfigure the driver's site management
 *
 * Authors: Michael Vogt
 *
 * History: 22 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to reconfigure.h
 *
 ****************************************************************************/
void reconfigureSiteManagement(
    struct phFrameStruct *frame, int *resErrors, int *resWarnings)
{
    phConfError_t confError;
    phConfType_t defType;
    int listLength;
    int i, j;
    int found;
    double stSite;
    double maskFlag;

    int tmpNumSites;
    int tmpStToHandlerSiteMap[PHESTATE_MAX_SITES];
    phEstateSiteUsage_t *currentPopulation;
    int entries;
    phEstateSiteUsage_t tmpPopulated[PHESTATE_MAX_SITES];
    const char *siteName;

    *resWarnings = 0;
    *resErrors = 0;

    /* (re)configure the site mapping and the active site mask */

    /* get the list of handler site names. This is a 'must' field. It
       determines the number of sites at the handler side */
    listLength = 0;
    confError = phConfConfType(frame->specificConfId,
	PHKEY_SI_HSIDS, 0, NULL, &defType, &listLength);
    if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST ||
	listLength == 0 || listLength > PHESTATE_MAX_SITES)
    {
	(*resErrors)++;
	if (confError == PHCONF_ERR_OK)
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		"configuration '%s' of wrong type, must be a list of site names",
		PHKEY_SI_HSIDS);
	return;
    }
    else
	tmpNumSites = listLength;

    /* get current site population, don't loose already tested devices */

    if (phEstateAGetSiteInfo(frame->estateId, &currentPopulation, &entries) ==
	PHESTATE_ERR_SITE)
    {
	(*resErrors)++;
	return;
    }

    /* prepare the mask field, since the following is optional */
    for (i=0; i<tmpNumSites; i++)
	tmpPopulated[i] = i < entries ? 
	    currentPopulation[i] : PHESTATE_SITE_EMPTY;
    
    /* if, the site mask is defined, read in the field, check that
       size is equal to size of handler site names, fill in masked
       sites */
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_SI_HSMASK);
    if (confError == PHCONF_ERR_OK)
    {
	listLength = 0;
	confError = phConfConfType(frame->specificConfId,
	    PHKEY_SI_HSMASK, 0, NULL, &defType, &listLength);
	if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST ||
	    listLength != tmpNumSites)
	{
	    (*resErrors)++;
	    if (confError == PHCONF_ERR_OK)
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "configuration '%s' of wrong type,\n"
		    "must be a list of numbers with equal size as '%s'\n"
            "if you want to change the size of '%s', please set correct '%s' first!",
		    PHKEY_SI_HSMASK, PHKEY_SI_HSIDS, PHKEY_SI_HSMASK, PHKEY_SI_HSIDS);
	    return;
	}

	for (i=0; i<tmpNumSites; i++)
	{
	    confError = phConfConfNumber(frame->specificConfId,
		PHKEY_SI_HSMASK, 1, &i, &maskFlag);
	    if (confError != PHCONF_ERR_OK)
		(*resErrors)++;
	    else
	    {
		if (! (int) maskFlag)
		{
		    /* set a site to be deactivated */
		    if (tmpPopulated[i] == PHESTATE_SITE_POPULATED ||
			tmpPopulated[i] == PHESTATE_SITE_POPDEACT)
		    {
			phConfConfString(frame->specificConfId,
			     PHKEY_SI_HSIDS, 1, &i, &siteName);
			phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
			    "handler site \"%s\" is currently populated.\n"
			    "Will deactivate it after device was binned",
			    siteName, PHKEY_SI_HSMASK);
			tmpPopulated[i] = PHESTATE_SITE_POPDEACT;
			(*resWarnings)++;
		    }
		    else
			tmpPopulated[i] = PHESTATE_SITE_DEACTIVATED;
		}
		else
		{
		    /* (re)activate a site */
		    if (tmpPopulated[i] == PHESTATE_SITE_DEACTIVATED)
			tmpPopulated[i] = PHESTATE_SITE_EMPTY;
		    if (tmpPopulated[i] == PHESTATE_SITE_POPDEACT)
			tmpPopulated[i] = PHESTATE_SITE_POPULATED;
		}
	    }
	}

	if (*resErrors)
	    return;
    }

    /* prepare the SmarTest to handler site mapping. The array starts
       with index 0, as usual, but the first entry of this array
       refers to site number 1 !!! */
    for (i=0; i<tmpNumSites; i++)
	tmpStToHandlerSiteMap[i] = i+1;

    /* if, the site mapping is defined, read in the field, check that
       size is equal to size of handler site names, fill in mapping */
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_SI_STHSMAP);
    if (confError == PHCONF_ERR_OK)
    {
	listLength = 0;
	confError = phConfConfType(frame->specificConfId,
	    PHKEY_SI_STHSMAP, 0, NULL, &defType, &listLength);
	if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST ||
	    listLength != tmpNumSites)
	{
	    (*resErrors)++;
	    if (confError == PHCONF_ERR_OK)
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "configuration '%s' of wrong type,\n"
		    "must be a list of numbers with equal size as '%s'\n"
            "if you want to change the size of '%s', please set correct '%s' first!",
		    PHKEY_SI_STHSMAP, PHKEY_SI_HSIDS, PHKEY_SI_STHSMAP, PHKEY_SI_HSIDS);
	    return;
	}
	
	for (i=0; i<tmpNumSites; i++)
	{
	    confError = phConfConfNumber(frame->specificConfId,
		PHKEY_SI_STHSMAP, 1, &i, &stSite);
	    if (confError != PHCONF_ERR_OK)
		(*resErrors)++;
	    else 
		tmpStToHandlerSiteMap[i] = (int) stSite;
	}

        
#if 0
        /* disable the site number checking for 1-tester-multiple-handler control*/
        /* CR-82345, 2013-10-17*/
	for (j=1; j<=tmpNumSites; j++)
	{
	    found = 0;
	    for (i=0; i<tmpNumSites; i++)
	    {
		if (tmpStToHandlerSiteMap[i] == j)
		    found = 1;
	    }
	    if (!found)
	    {
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "configuration '%s' of wrong type, "
		    "SmarTest site number %d not found",
		    PHKEY_SI_STHSMAP, j);
		(*resErrors)++;
	    }
	}
#endif

	if (*resErrors)
	    return;
    }

    /* now we have read everything and checked for consistency, apply
       the configuration to the internal state: */
    if (! *resErrors)
    {
	if (phEstateASetSiteInfo(frame->estateId, tmpPopulated, tmpNumSites) ==
	    PHESTATE_ERR_SITE)
	{
	    (*resErrors)++;
	    return;
	}
	frame->numSites = tmpNumSites;
	for (i=0; i<tmpNumSites; i++)
	    frame->stToHandlerSiteMap[i] = tmpStToHandlerSiteMap[i];
    }

    return;
}	

/*****************************************************************************
 *
 * Reconfigure the driver's bin management
 *
 * Authors: Michael Vogt
 *
 * History: 23 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to reconfigure.h
 *
 ****************************************************************************/
void reconfigureBinManagement(
    struct phFrameStruct *frame, int *resErrors, int *resWarnings)
{
    phConfError_t confError;
    phConfType_t defType;

    phBinMapId_t tmpBinMap = NULL;
    phBinMode_t tmpBinMode = PHBIN_MODE_DEFAULT;
    const char *binMapping;
    const char *binString;
    int numHandlerBins = 0;
    int idsGiven = 0;
    int listLength = 0;
    int retestBinsNumber = 0;
    int retestBinsDefined = 0;
    int hardmapGiven = 0;
    int softmapGiven = 0;
    int i, j;

    int indexField[2];
    double dBinIndex = 0.0;
    int binnerRet;

    *resWarnings = 0;
    *resErrors = 0;

    /* debug*/
    /*char *model_now=NULL;
    phConfConfString(frame->specificConfId, PHKEY_IN_MODEL,
	0, NULL, &model_now);*/

    /* retrieve the binning mode */
    confError = phConfConfString(frame->specificConfId, PHKEY_BI_MODE,
	0, NULL, &binMapping);

    if (confError != PHCONF_ERR_OK)
    {
	(*resErrors)++;
  	goto ERROR_END;
    }

    /* accept and store the binning mode */
    if (strcasecmp(binMapping, "default") == 0)
	tmpBinMode = PHBIN_MODE_DEFAULT;
    else if (strcasecmp(binMapping, "mapped-hardbins") == 0)
	tmpBinMode = PHBIN_MODE_HARDMAP;
    else if (strcasecmp(binMapping, "mapped-softbins") == 0)
	tmpBinMode = PHBIN_MODE_SOFTMAP;
    else
    {

        /*phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
	    " value '%s' ",
	    model_now);*/
	phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
	    " 2222222222 value '%s' ",
	    binMapping);
	phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
	    "11111111111 unknown value '%s' for definition '%s'",
	    binMapping, PHKEY_BI_MODE);
	(*resErrors)++;
  	goto ERROR_END;
    }

    /* retrieve the number of handler bins (from the ID list) */
    idsGiven = 0;
    numHandlerBins = 0;
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_BI_HBIDS);
    if (confError == PHCONF_ERR_OK)
    {
	listLength = 0;
	confError = phConfConfType(frame->specificConfId, PHKEY_BI_HBIDS,
	    0, NULL, &defType, &listLength);
	if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST ||
	    listLength == 0)
	{
	    if (confError == PHCONF_ERR_OK)
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "configuration '%s' of wrong type,\n"
		    "must be a list of handler bin names",
		    PHKEY_BI_HBIDS);
	    (*resErrors)++;
  	    goto ERROR_END;
	}
	numHandlerBins = listLength;
	idsGiven = listLength;
    }

    /* handler bins must be given, if some mapping takes place */
    if (!idsGiven && 
	(tmpBinMode == PHBIN_MODE_HARDMAP || tmpBinMode == PHBIN_MODE_SOFTMAP))
    {
	phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
	    "configuration '%s' must be given, if '%s' is set to '%s'",
	    PHKEY_BI_HBIDS, PHKEY_BI_MODE, binMapping);
	(*resErrors)++;
  	goto ERROR_END;
    }

    /* look out for retest bins. If given, it must work together with
       the handler bin IDs array (index range) or is used directly, if
       handler bin Ids are not defined */
    retestBinsDefined = 0;
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_BI_HRTBINS);
    if (confError == PHCONF_ERR_OK)
    {
	listLength = 0;
	confError = phConfConfType(frame->specificConfId, PHKEY_BI_HRTBINS,
	    0, NULL, &defType, &listLength);
	if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST)
	{
	    if (confError == PHCONF_ERR_OK)
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "configuration '%s' of wrong type,\n"
		    "must be a list of handler bins",
		    PHKEY_BI_HRTBINS);
	    (*resErrors)++;
  	    goto ERROR_END;
	}

	for (i=0; i<listLength; i++)
	{
	    confError = phConfConfNumber(frame->specificConfId, 
		PHKEY_BI_HRTBINS, 1, &i, &dBinIndex);
	    if (confError != PHCONF_ERR_OK)
	    {
		(*resErrors)++;
  	        goto ERROR_END;
	    }

	    if (idsGiven)
	    {
		if (dBinIndex < 0 || dBinIndex >= numHandlerBins)
		{
		    phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
			"handler bin %d in configuration '%s'\n"
			"out of range for given configuration '%s'",
			(int) dBinIndex, PHKEY_BI_HRTBINS, PHKEY_BI_HBIDS);
		    (*resErrors)++;
  	            goto ERROR_END;
		}
	    }
	    else
	    {
		if ((int) dBinIndex >= numHandlerBins)
		    numHandlerBins = 1 + (int) dBinIndex;
	    }
	}
	if (listLength == 0)
	    numHandlerBins++;

	retestBinsDefined = 1;
	retestBinsNumber = listLength;
    }

    /* allocate a bin map, if either bin IDs or retest bins were defined */
    if (numHandlerBins != 0)
    {
	binnerRet = resetBinMapping(&tmpBinMap, numHandlerBins);
	if (binnerRet != 0)
	{
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		"error %d from bin management during reset", binnerRet);
	    (*resErrors)++;
  	    goto ERROR_END;
	}
    }

    /* if retest bins were defined, enter them into the bin map */
    if (retestBinsDefined)
    {
	for (i=0; i<retestBinsNumber; i++)
	{
	    confError = phConfConfNumber(frame->specificConfId, 
		PHKEY_BI_HRTBINS, 1, &i, &dBinIndex);
            if (confError != PHCONF_ERR_OK)
            {
                (*resErrors)++;
                goto ERROR_END;
	    }

            if (tmpBinMap == NULL)
            {
                phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR, "Bin map does not exist");
                (*resErrors)++;
                goto ERROR_END;
            }
	    binnerRet = enterBinMap(tmpBinMap, -1LL, (long) dBinIndex);
	    if (binnerRet != 0)
	    {
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "error %d from bin management during enter", binnerRet);
		(*resErrors)++;
  	    	goto ERROR_END;
	    }
	}	
	if (retestBinsNumber == 0)
	{
	    binnerRet = enterBinMap(tmpBinMap, -1LL, -1L);
	    if (binnerRet != 0)
	    {
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "error %d from bin management during enter", binnerRet);
		(*resErrors)++;
  	        goto ERROR_END;
	    }	    
	}
    }

    /* if hardbin mapping is active, enter the hardbins into the map */
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_BI_HARDMAP);
    if (confError == PHCONF_ERR_OK)
    {
	if (tmpBinMode != PHBIN_MODE_HARDMAP)
	{
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
		"configuration '%s' ignored, since '%s' is set to '%s'",
		PHKEY_BI_HARDMAP, PHKEY_BI_MODE, binMapping);
	}
	else
	{
	    hardmapGiven = 0;
	    confError = phConfConfType(frame->specificConfId, PHKEY_BI_HARDMAP,
		0, NULL, &defType, &hardmapGiven);
	    if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST ||
		hardmapGiven != idsGiven)
	    {
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "configuration '%s' of wrong type,\n"
		    "must be a list of %d lists of SmarTest hardbins,\n"
		    "following configuration '%s'",
		    PHKEY_BI_HARDMAP, idsGiven, PHKEY_BI_HBIDS);
		(*resErrors)++;
  	    	goto ERROR_END;
	    }
	    for (i=0; i<hardmapGiven; i++)
	    {
		confError = phConfConfType(frame->specificConfId, 
		    PHKEY_BI_HARDMAP, 1, &i, &defType, &listLength);
		if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST)
		{
		    phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
			"configuration '%s' of wrong type,\n"
			"must be a list of lists of SmarTest hardbins",
			PHKEY_BI_HARDMAP);
		    (*resErrors)++;
  	    	    goto ERROR_END;
		}
		for (j=0; j<listLength; j++)
		{
		    indexField[0] = i;
		    indexField[1] = j;
		    confError = phConfConfNumber(frame->specificConfId, 
		    PHKEY_BI_HARDMAP, 2, indexField, &dBinIndex);
		    if (confError != PHCONF_ERR_OK)
		    {
			(*resErrors)++;
  	    		goto ERROR_END;
		    }

		    binnerRet = enterBinMap(tmpBinMap, 
			(long long) dBinIndex, (long) i);
		    if (binnerRet != 0)
		    {
			phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
			    "error %d from bin management during enter", 
			    binnerRet);
			(*resErrors)++;
  	    		goto ERROR_END;
		    }
		}
	    }
	}
    }

    /* if softbin mapping is active, enter the softbins into the map */
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_BI_SOFTMAP);
    if (confError == PHCONF_ERR_OK)
    {
	if (tmpBinMode != PHBIN_MODE_SOFTMAP)
	{
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
		"configuration '%s' ignored, since '%s' is set to '%s'",
		PHKEY_BI_SOFTMAP, PHKEY_BI_MODE, binMapping);
	}
	else
	{
	    softmapGiven = 0;
	    confError = phConfConfType(frame->specificConfId, PHKEY_BI_SOFTMAP,
		0, NULL, &defType, &softmapGiven);
	    if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST ||
		softmapGiven != idsGiven)
	    {
		phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		    "configuration '%s' of wrong type,\n"
		    "must be a list of %d lists of SmarTest hardbins,\n"
		    "following configuration '%s'",
		    PHKEY_BI_SOFTMAP, idsGiven, PHKEY_BI_HBIDS);
		(*resErrors)++;
  	    	goto ERROR_END;
	    }
	    for (i=0; i<softmapGiven; i++)
	    {
		confError = phConfConfType(frame->specificConfId, 
		    PHKEY_BI_SOFTMAP, 1, &i, &defType, &listLength);
		if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST)
		{
		    phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
			"configuration '%s' of wrong type,\n"
			"must be a list of lists of SmarTest softbins",
			PHKEY_BI_SOFTMAP);
		    (*resErrors)++;
  	            goto ERROR_END;
		}
		for (j=0; j<listLength; j++)
		{
		    indexField[0] = i;
		    indexField[1] = j;
		    confError = phConfConfString(frame->specificConfId, 
		    PHKEY_BI_SOFTMAP, 2, indexField, &binString);
		    if (confError != PHCONF_ERR_OK)
		    {
			(*resErrors)++;
  	            	goto ERROR_END;
		    }

		    if (strlen(binString) > (BIN_CODE_LENGTH - 1))
		    {
			phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
			    "entries in configuration '%s' are restricted\n"
			    "to %d characters", 
			    PHKEY_BI_SOFTMAP, (BIN_CODE_LENGTH - 1));
			(*resErrors)++;
  	            	goto ERROR_END;
		    }
                    long long binIndex = 0LL;
                    strncpy((char *)(&binIndex), binString, BIN_CODE_LENGTH);
		    binnerRet = enterBinMap(tmpBinMap, binIndex, (long) i);
		    if (binnerRet != 0)
		    {
			phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
			    "error %d from bin management during enter", 
			    binnerRet);
			(*resErrors)++;
  	            	goto ERROR_END;
		    }
		}
	    }
	}
    }

    /* report errors, if important mapping information is missing */
    if (tmpBinMode == PHBIN_MODE_HARDMAP && !hardmapGiven)
    {
	phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
	    "configuration '%s' must be given for '%s': %s'",
	    PHKEY_BI_HARDMAP, PHKEY_BI_MODE, binMapping);
	(*resErrors)++;
  	goto ERROR_END;
    }

    /* report errors, if important mapping information is missing */
    if (tmpBinMode == PHBIN_MODE_SOFTMAP && !softmapGiven)
    {
	phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
	    "configuration '%s' must be given for '%s': %s'",
	    PHKEY_BI_SOFTMAP, PHKEY_BI_MODE, binMapping);
	(*resErrors)++;
  	goto ERROR_END;
    }

ERROR_END:
    if (! *resErrors)
    {
	if( frame-> binMapHdl)
	   freeBinMapping(&(frame->binMapHdl));
	frame->binMapHdl = tmpBinMap;
	frame->binMode = tmpBinMode;
    }
    else
    {
	if(tmpBinMap)
   	   freeBinMapping(&tmpBinMap);
    }

    return;
}

/*****************************************************************************
 *
 * Reconfigure the driver's framework
 *
 * Authors: Michael Vogt
 *
 * History: 06 May 2000, Michael Vogt, created
 *
 * Description: 
 * Please refer to reconfigure.h
 *
 ****************************************************************************/
void reconfigureFramework(
    struct phFrameStruct *frame         /* the framework */,
    int *resErrors                      /* resulting number of errors */,
    int *resWarnings                    /* resulting number of warnings */
)
{
    int found = 0;

    /* what is the mode for automatic reprobe actions ? */
    phConfConfStrTest(&found, frame->specificConfId, PHKEY_RP_AMODE, 
	"off", "all", "per-site", NULL);
    switch (found)
    {
      case 1:
	frame->autoReprobe = PHFRAME_RPMODE_OFF;
	break;
      case 2:
	frame->autoReprobe = PHFRAME_RPMODE_ALL;
	if (!(frame->funcAvail & PHFUNC_AV_REPROBE))
	{
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		"ph_reprobe() is not available for the current handler driver", PHKEY_RP_AMODE);
	    (*resWarnings)++;
	}
	break;
      case 3:
	frame->autoReprobe = PHFRAME_RPMODE_PERSITE;
	if (!(frame->funcAvail & PHFUNC_AV_BINREPR))
	{
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
		"ph_reprobe() on a per site basis (configuration parameter \"%s\") is not available for the current handler driver", PHKEY_RP_AMODE);
	    (*resErrors)++;
	}
	break;
      default:
	/* not given, don't care, use default value */
	frame->autoReprobe = PHFRAME_RPMODE_ALL;
	break;	
    }


  /* read the driver configuration parameter: "enable_diagnose_window" */
  if(phConfConfIfDef(frame->specificConfId, "enable_diagnose_window") == PHCONF_ERR_OK) {
    found = 0;
    phConfError_t confError = phConfConfStrTest(&found, frame->specificConfId, 
                                 "enable_diagnose_window", "yes", "no", NULL);

    if( (confError != PHCONF_ERR_UNDEF) && (confError != PHCONF_ERR_OK) ) {
      /* out of range or bad format */
      phLogFuncMessage(frame->logId, PHLOG_TYPE_ERROR,
                       "configuration value of \"%s\" must be either \"yes\" or \"no\"\n",
                       "enable_diagnose_window");
      (*resWarnings)++;
    } else {
      switch( found ) {
        case 1:   /* "yes" */
          frame->enableDiagnoseWindow = YES;
          break;
        case 2:   /* "no" */
          frame->enableDiagnoseWindow = NO;
          break;
        default:
          phLogFuncMessage(frame->logId, PHLOG_TYPE_FATAL, "internal error at line: %d, "
                           "function: %s, file: %s",__LINE__, __FUNCTION__,__FILE__);
          (*resErrors)++;
          break;
      }
    }
  }

    return;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
