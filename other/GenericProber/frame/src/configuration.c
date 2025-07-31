/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : configuration.c
 * CREATED  : 20 Jan 2000
 *
 * CONTENTS : Configuration actions interface
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 20 Jan 2000, Michael Vogt, created
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

#include "ci_types.h"

#include "ph_tools.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_mhcom.h"
#include "ph_pfunc.h"
#include "ph_phook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"
#include "ph_keys.h"

#include "ph_timer.h"
#include "ph_frame_private.h"
#include "helpers.h"
#include "configuration.h"
#include "exception.h"

#include "reconfigure.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/
#define MAX_SITE_INFO_LENGTH 3072*10  /* support 1024 sites */
#define MAX_SITE_SETTING_STRING 3072*10*6

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static int doReconfigure(struct phFrameStruct *f)
{
    phPFuncError_t funcError;
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;
    int success = 0;
    int completed = 0;

    while (!success && !completed)
    {
        funcError = phPlugPFuncReconfigure(f->funcId);

        /* analyze result from reconfigure and take action */
        phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_RECONFIGURE,
                                  &completed, &success, &retVal, &action);
    }

    if ( success && 
         action==PHFRAME_EXCEPTION_ERR_ERROR && 
         retVal!=PHTCOM_CI_CALL_PASS )
    {
        success=0;
    }

    if (!success)
    {
	if (!phFramePanic(f, 
	    "problems during reconfiguration (see log),\n"
	    "may be recovered by additional reconfiguration"))
	    success = 1;	
    }

    return success;
}


static int mergeAndReconfigure(
    struct phFrameStruct *f, 
    phConfId_t tempConfId)
{
    int goOn = 1;
    phConfError_t confError;
    const char *key;
    char *oneDef;
    int errors;
    int warnings;
    int allErrors;
    int allWarnings;

    /* merge and reconfigure */
    /* log the changes */
    if (goOn)
    {
        phLogFrameMessage(f->logId, LOG_NORMAL, "Logging the configuration change request....");
        phLogFrameMessage(f->logId, LOG_NORMAL, "");

        confError = phConfConfFirstKey(tempConfId, &key);
        while (confError == PHCONF_ERR_OK)
        {
            confError = phConfGetConfDefinition(tempConfId, key, &oneDef);
            phLogFrameMessage(f->logId, LOG_NORMAL, "%s", oneDef);
            confError = phConfConfNextKey(tempConfId, &key);
        }
        phLogFrameMessage(f->logId, LOG_NORMAL, "");
        phLogFrameMessage(f->logId, LOG_NORMAL, "End of configuration change log");
    }

    /* do the merge */
    if (goOn)
    {
        /* in the phConfMerge, calling phConfSemDeleteDefinition to destory defination tree */
        confError = phConfMerge(f->specificConfId, tempConfId, PHCONF_FLAG_DONTCARE);
        free(tempConfId);
        tempConfId=NULL;
        goOn = (confError == PHCONF_ERR_OK);
        if (!goOn)
            phLogFrameMessage(f->logId, LOG_NORMAL, "configuration not changed");
    }

    /* reconfigure internal settings */
    if (goOn)
    {
        allErrors = 0;
        allWarnings = 0;
        reconfigureLogger(f, &errors, &warnings);
        allErrors += errors;
        allWarnings += warnings;
        reconfigureStateController(f, &errors, &warnings);
        allErrors += errors;
        allWarnings += warnings;
        reconfigureSiteManagement(f, &errors, &warnings);
        allErrors += errors;
        allWarnings += warnings;
        reconfigureBinManagement(f, &errors, &warnings);
        allErrors += errors;
        allWarnings += warnings;
        reconfigureNeedleCleaning(f, &errors, &warnings);
        allErrors += errors;
        allWarnings += warnings;
        if (allErrors)
            if (phFramePanic(f, "errors during reconfiguration"))
                return 0;
        if (allWarnings)
            phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "warnings during reconfiguration");	    
        if ((f->funcAvail & PHPFUNC_AV_RECONFIGURE))
            if (!doReconfigure(f))
                return 0;
    }

    return 1;
}


static int askOperatorStartEditor(
    struct phFrameStruct *f
)
{
    long response;
    phEventResult_t result;
    phTcomUserReturn_t stReturn;

    do
    {
	response = 8;
	phEventPopup(
	    f->eventId, &result, &stReturn, 0, "menu",
	    "Interactive reconfiguration procedure\n"
	    "Do you want to...\n"
	    "CHANGE the current driver configuration or\n"
	    "CONTINUE with normal operation ?",
	    "Change", "Continue", "", "", "", "", &response, NULL);
	if (result == PHEVENT_RES_ABORT)
	    /* this is not a correct handling but better than nothing here */
	    return 0;

    } while (response != 2 && response != 3);

    return (response == 2);
}



static int interactiveReconfigure(
    struct phFrameStruct *f
)
{
    int goOn;
    int tryAgain;
    int rework;
    int valid;
    int failed;
    long response;
    phConfError_t confError;
    phConfId_t tempConfId;
    phEventResult_t result;
    phTcomUserReturn_t stReturn;

    char editFileName[] = "/tmp/tempfile_XXXXXX";

    char command[1024];

    /* pseudo code:
     *
     * ask operator and give advise
     * log
     * possible early return
     * do
     *     write configuration to file
     *     log
     *     do 
     *         log
     *         start terminal with default editor
     *         read in temporary new configuration
     *         validate temporary configuration
     *         log
     *         if (errors)
     *             ask whether to give up, rework or to start again
     *             log
     *         endif
     *     while (rework)
     *     delete temporary file
     * while (start again)
     * if (no errors)
     *     ask whether to merge in the new configuration
     *     log
     *     if (yes)
     *         perform merge
     *         log new configuration
     *         reconfigure internal settings
     *     endif
     * else
     *     log
     * endif
     */

    /* ask, if the operator really wants to change the configuration */
    goOn = askOperatorStartEditor(f);

    /* repeat providing an editor with the unchanged configuration */
    tryAgain = 1;
    valid = 0;
    failed = 0;
    tempConfId = NULL;
    while (goOn && tryAgain)
    {
	/* provide the configuration file */
        (void)mkstemp(editFileName);
	confError = phConfWriteConfFile(f->specificConfId,
	    editFileName, (phConfFlag_t)(PHCONF_FLAG_DONTCARE & ~PHCONF_FLAG_FIXED));
	if (confError != PHCONF_ERR_OK)
	{
	    goOn = 0;
	    break;
	}

	do
	{
	    /* provide editor */
	    sprintf(command, "xterm -e %s %s",
		getenv("EDITOR") ? getenv("EDITOR") : "vi",
		editFileName);
	    phLogFrameMessage(f->logId, LOG_DEBUG,
		"starting editor for interactive reconfiguration:\n%s",
		command);
	    system(command);
	    
	    /* read in and validate the file */
	    confError = phConfReadConfFile(&tempConfId, editFileName);
	    valid = (confError == PHCONF_ERR_OK);

	    if (valid)
	    {
		confError = phConfValidate(tempConfId, f->attrId);
		valid = (confError == PHCONF_ERR_OK);
	    }

	    rework = 0;
	    tryAgain = 0;
	    failed = 0;
	    if (!valid)
	    {
		do
		{
		    response = 8;
		    phEventPopup(f->eventId, &result, &stReturn, 0, "menu",
			"Validation of changed configuration failed.\n"
			"Do you want to...\n"
			"REEDIT your configuration changes,\n"
			"RESTART editing the configuration, or\n"
			"GIVE UP changing the configuration ?",
			"", "", "", "Reedit", "Restart", "Give up", 
			&response, NULL);

		    if (result == PHEVENT_RES_ABORT) {
                        if (tempConfId)
                        {
                            phConfDestroyConf(tempConfId);
                            free(tempConfId);
                            tempConfId = NULL;		    
                        }
			/* this is not a correct handling but better
                           than nothing here */
                        return 1;
                    }
		} while (response < 5 || response > 7);
		switch (response)
		{
		  case 5:
		    rework = 1; break;
		  case 6:
		    tryAgain = 1; break;
		  case 7:
		    goOn = 0; break;
		}

		failed = 1;

		/* destroy the invalid temp config, if existent */
		
	    }
            if (tempConfId)
            {
                phConfDestroyConf(tempConfId);
                free(tempConfId);
                tempConfId = NULL;		    
            }
	} while (rework && goOn);

	/* delete configuration file */
	sprintf(command, "rm %s", editFileName);
	system(command);
    }

    if (valid)
    {
	/* last chance to cancel the configuration changes */
	do
	{
	    response = 8;
	    phEventPopup(f->eventId, &result, &stReturn, 0, "menu",
		"Configuration changes seem to be OK\n"
		"Do you want to...\n"
		"ACCEPT your configuration changes or\n"
		"CANCEL your configuration changes ?",
		"", "", "", "", "Accept", "Cancel", &response, NULL);

	    if (result == PHEVENT_RES_ABORT)
		/* this is not a correct handling but better
		   than nothing here */
		return 1;
	} while (response < 6 || response > 7);
	goOn = (response == 6);
	if (!goOn)
	{
	    phConfDestroyConf(tempConfId);
            free(tempConfId);
	    tempConfId = NULL;		    
	}
    }
    else if (failed)
    {
	/* final invalidity message */
	do
	{
	    response = 2;
	    phEventPopup(f->eventId, &result, &stReturn, 0, "menu",
		"Your configuration changes are invalid\n"
		"You have to...\n"
		"CONTINUE and restart the reconfiguration process",
		"Continue", "", "", "", "", "", &response, NULL);

	    if (result == PHEVENT_RES_ABORT)
		/* this is not a correct handling but better
		   than nothing here */
		return 1;
	} while (response != 2);
    }

    if (valid && goOn)
	/* do the merge */
        /* mergeAndReconfigure will destroy the definition tree. do not call phConfDestroyConf */
	return mergeAndReconfigure(f, tempConfId);
    else
	return 1;
}

static int guiBasedInteractiveReconfigure(
    struct phFrameStruct *f
)
{
    phConfError_t confError;
    int errors;
    int warnings;
    int allErrors;
    int allWarnings;
    int goOn;

    /* ask, if the operator really wants to change the configuration */
    goOn = askOperatorStartEditor(f);
    if (!goOn)
	return 1;

    phLogFrameMessage(f->logId, LOG_NORMAL, 
        "Logging interactive configuration modifications ....\n");

    confError = phConfEditConfDef(f->specificConfId, f->attrId, 
	(phConfFlag_t)(PHCONF_FLAG_DONTCARE & ~PHCONF_FLAG_FIXED), (phPanelFlag_t) 0, NULL);

    phLogFrameMessage(f->logId, LOG_NORMAL, "");
    phLogFrameMessage(f->logId, LOG_NORMAL, 
	"End of interactive configuration modifications");

    if (confError == PHCONF_ERR_OK)
    {
	allErrors = 0;
	allWarnings = 0;
	reconfigureLogger(f, &errors, &warnings);
	allErrors += errors;
	allWarnings += warnings;
	reconfigureStateController(f, &errors, &warnings);
	allErrors += errors;
	allWarnings += warnings;
	reconfigureSiteManagement(f, &errors, &warnings);
	allErrors += errors;
	allWarnings += warnings;
	reconfigureBinManagement(f, &errors, &warnings);
	allErrors += errors;
	allWarnings += warnings;
	reconfigureNeedleCleaning(f, &errors, &warnings);
	allErrors += errors;
	allWarnings += warnings;
	if (allErrors)
	{ 
	    if (phFramePanic(f, "errors during reconfiguration"))
		return 0;
	}
	if (allWarnings)
	{ 
	    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
		"warnings during reconfiguration");	    
	}
	if ((f->funcAvail & PHPFUNC_AV_RECONFIGURE))
	{
	    if (!doReconfigure(f))
		return 0;
	}
    }
    return 1;
}

/*****************************************************************************
 *
 * Apply an interactive reconfiguration session
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary configuration actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameInteractiveConfiguration(
    struct phFrameStruct *f             /* the framework data */
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    if (getenv("USE_OLD_CONFIGURATION_EDITOR"))
    {
	/* editor based reconfiguration */
	retVal = interactiveReconfigure(f) ? 
	    PHTCOM_CI_CALL_PASS : PHTCOM_CI_CALL_ERROR;
    }
    else
    {
	retVal = guiBasedInteractiveReconfigure(f) ?
	    PHTCOM_CI_CALL_PASS : PHTCOM_CI_CALL_ERROR;
    }

    return retVal;
}


/*****************************************************************************
 *
 * Apply a specific configuration setting
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary configuration actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameSetConfiguration(
    struct phFrameStruct *f             /* the framework data */,
    char *inputString
)
{
    char defaultSiteSetting[MAX_SITE_SETTING_STRING] = {0};
    char defaultSiteMask[MAX_SITE_INFO_LENGTH] = {0};
    char defaultSiteMapping[MAX_SITE_INFO_LENGTH] = {0};
    char defaultDieOffset[MAX_SITE_INFO_LENGTH*3] = {0};

    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    phConfError_t confError;
    phConfType_t defType;
    phConfId_t tempConfId = NULL;
    phEstateSiteUsage_t *currentPopulation = NULL;
    int newSiteNumbers = 0;
    int currentSiteNumbers = 0;
    int goOn = 1;

    if(NULL != strstr(inputString, "prober_site_ids")) //we received site number change request
    {
        //generate configuration struct
        if ((confError = phConfSetConfDefinition(&tempConfId, inputString)) != PHCONF_ERR_OK)
            return PHTCOM_CI_CALL_ERROR;

        //read the site numbers will be set
        if ((confError = phConfConfType(tempConfId, PHKEY_SI_HSIDS, 0, NULL, &defType, &newSiteNumbers)) != PHCONF_ERR_OK)
            return PHTCOM_CI_CALL_ERROR;

        //read current site number
        if (phEstateAGetSiteInfo(f->estateId, &currentPopulation, &currentSiteNumbers) == PHESTATE_ERR_SITE)
            return PHTCOM_CI_CALL_ERROR;

        if(currentSiteNumbers == newSiteNumbers) //without extension when the number is the same as existent configuration
            confError = phConfSetConfDefinition(&tempConfId, inputString);
        else if (currentSiteNumbers > newSiteNumbers) //abort if we received unexpected site number
        {
            phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                    "number of sites may not be reduced during driver runtime.\n"
                    "Use %s to deactivate sites during runtime\n"
                    "or restart driver all over with new site definition",
                    PHKEY_SI_HSMASK);
            return PHTCOM_CI_CALL_ERROR;
        }
        else //extend the site configuration
        {
            strncpy(defaultSiteMask, "prober_site_mask: [", MAX_SITE_INFO_LENGTH-1);
            strncpy(defaultSiteMapping, "smartest_site_to_prober_site_map: [", MAX_SITE_INFO_LENGTH-1);
            strncpy(defaultDieOffset, "multi_site_die_offsets: [", MAX_SITE_INFO_LENGTH*3-1);
            int iLengthMask = 0, iLengthMapping = 0, iLengthOffset = 0;

            for(int i=0; i<newSiteNumbers; i++)
            {
                //construct the site configuration
                iLengthMask = strlen(defaultSiteMask);
                iLengthMapping = strlen(defaultSiteMapping);
                iLengthOffset = strlen(defaultDieOffset);
                snprintf(defaultSiteMask+iLengthMask, MAX_SITE_INFO_LENGTH-iLengthMask, " %d", 1);
                snprintf(defaultSiteMapping+iLengthMapping, MAX_SITE_INFO_LENGTH-iLengthMapping, " %d", i+1);
                snprintf(defaultDieOffset+iLengthOffset, MAX_SITE_INFO_LENGTH*3-iLengthOffset, " [%d %d]", i, i);
            }

            //add terminated characters
            strcat(defaultSiteMask, " ]");
            strcat(defaultSiteMapping, " ]");
            strcat(defaultDieOffset, " ]");

            //final construction
            snprintf(defaultSiteSetting, MAX_SITE_SETTING_STRING, "%s\n%s\n%s\n%s", inputString, defaultSiteMask, defaultSiteMapping, defaultDieOffset);

            phLogFrameMessage(f->logId, LOG_NORMAL, "Number of sites will be extend from %d to %d, please also change the site mask, site mapping, and die offsets for adaption, otherwise, all of these parameters will be fill out with default value:\n%s", currentSiteNumbers, newSiteNumbers, defaultSiteSetting);
            confError = phConfSetConfDefinition(&tempConfId, defaultSiteSetting);
        }
    }
    else
        /* parse the configuration change request */
        confError = phConfSetConfDefinition(&tempConfId, inputString);

    goOn = (confError == PHCONF_ERR_OK);
    
    /* validate the changes */
    if (goOn)
    {
        confError = phConfValidate(tempConfId, f->attrId);
        if (confError != PHCONF_ERR_OK)
        {
            goOn = 0;
            phLogFrameMessage(f->logId, LOG_NORMAL, "configuration not changed");
        }
    }

    /* log the changes */
    if (goOn)
    {
        /* mergeAndReconfigure will destroy the definition tree. do not call phConfDestroyConf */
        mergeAndReconfigure(f, tempConfId);
    }
    else
    {
        phConfDestroyConf(tempConfId);
        free(tempConfId);
        tempConfId=NULL;
        retVal = PHTCOM_CI_CALL_ERROR;
    }

    return retVal;
}



/*****************************************************************************
 *
 * retrieve a specific configuration value
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary configuration actions.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameGetConfiguration(
    struct phFrameStruct *f             /* the framework data */,
    char *confRequestString,
    char **outputString
)
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;

    static char tmpComment[CI_MAX_COMMENT_LEN];
    const char *token;
    int tmpUsed = 0;
    char *definition;
    phConfId_t foundConf;

    tmpComment[0] = '\0';
    token = strtok(confRequestString, " \t\n");
    while (token)
    {
	/* look in both, the global and the specific configuration */
	foundConf = NULL;
	if (phConfConfIfDef(f->globalConfId, token) == PHCONF_ERR_OK)
	    foundConf = f->globalConfId;
	else if (phConfConfIfDef(f->specificConfId, token) == PHCONF_ERR_OK)
	    foundConf = f->specificConfId;

	if (!foundConf)
	{
#if 0
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"configuration for key \"%s\" is not defined", token);
#endif
	}
	else
	{
	    definition = NULL;
	    if (phConfGetConfDefinitionValue(foundConf, token, &definition)
		== PHCONF_ERR_OK && definition != NULL)
	    {
		phLogFrameMessage(f->logId, LOG_NORMAL,
		"configuration request: \"%s\"", definition);
		if (strlen(definition) >= (CI_MAX_COMMENT_LEN - tmpUsed))
		    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
			"configuration string to long, can not be returned");
		else
		{
		    strcat(tmpComment, definition);
		    tmpUsed += strlen(definition);
		}
	    }
	}

	/* step to next, before printing newline */
	token = strtok(NULL, " \t\n");

	if (token && foundConf && tmpUsed + 1 < CI_MAX_COMMENT_LEN)
	{
	    strcat(tmpComment, "\n");
	    tmpUsed += 1;
	}
    }

    *outputString = &tmpComment[0];
    return retVal;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
