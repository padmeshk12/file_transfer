/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : idrequest.c
 * CREATED  : 24 Jan 2000
 *
 * CONTENTS : Retrieve identifications from various places
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 24 Jan 2000, Michael Vogt, created
 *            15 Jun 2000, Michael Vogt, adapted from prober driver
 *            Sep 2008, Jiawei Lin, enhance for Riesling Site Disabling feature
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

#include "ph_timer.h"
#include "ph_frame_private.h"
#include "identity.h"
#include "exception.h"
#include "idrequest.h"
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#undef LOT_ID_REQUEST_AVAILABLE

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * convert the smartest disabled sites to handler site mask
 *
 * if the input parameter "commandString" is "disabled_sites:[ 1 3 ]", and
 *  the handler_site_ids is ["A" "B" "C" "D"]
 *  the smartest_site_to_handler_site_map is [2 1 3 4]
 * after call of this function, the return value (site mask) will be
 *  "1 0 0 1", means that Site "B" and "C" is disabled
 *
 * Note:
 *  the input parameter "commandString" must take the value like
 *      "disabled_sites:[ 1 3 ]"
 *
 * Authors: Jiawei Lin, Sep 2008
 *
 ***************************************************************************/
static char *ConvertSmartestDisabledSite2SiteMask(struct phFrameStruct *f, const char *commandString)
{
  static char sSiteMask[PHESTATE_MAX_SITES+1] = "";   /* site mask string, the value like "1001" */

  /*
   * assume the input parameter "commandString" has the value "disabled_sites:[ 1 3 ]
   * the following code is to make out an array of {1,3} by parsing the command string
   */
  char * buf = (char *)malloc(strlen(commandString)+1);
  if ( buf == NULL ) {
    return NULL;
  }
  *buf = '\0';

  int start = FALSE;
  int i = 0;
  const char *p = commandString;
  long disabledSites[PHESTATE_MAX_SITES] = {0};
  while( *p != '\0' ) {
    if ( *p == '[' ) {
      start = TRUE;
      p++;
      continue;
    }
    if ( *p == ']' ) {
      buf[i] = '\0';
      break;
    }
    if ( start == TRUE ) {
      buf[i++] = *p;
    }
    p++;
  }

  char *token = NULL;
  token = strtok(buf, " ");
  i = 0;
  while (token != NULL ) {
    disabledSites[i++] = atoi(token);
    token = strtok(NULL, " ");
  }
  int length = i;
  free(buf);
  buf = NULL;

#ifdef DEBUG
  fprintf(stderr, "the length =%d\n", length);
  for(i=0;i<length;i++){
    fprintf(stderr, "disabledSites[%d]=%ld\n", i, disabledSites[i]);
  }
#endif

  /* get the current site population information  */
  phEstateSiteUsage_t *sitePopulated;
  int numOfSites = 0;
  if (phEstateAGetSiteInfo(f->estateId, &sitePopulated, &numOfSites) == PHESTATE_ERR_SITE ) {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                              "in ConvertSmartestDisabledSite2SiteMask, can't get the site info");
    return NULL;
  }

#ifdef DEBUG
  fprintf(stderr, "the numOfSites =%d\n", numOfSites);
  for(i=0;i<numOfSites;i++){
    fprintf(stderr, "sitePopulated[%d]=%ld\n", i, sitePopulated[i]);
    fprintf(stderr, "stToHandlerSiteMap[%d]=%ld\n", i, f->stToHandlerSiteMap[i]);
  }
#endif

  sSiteMask[0] = '\0';
  int found = FALSE;
  int j = 0;
  for(i=0; i<numOfSites; i++ ) {
    long smartestSite = f->stToHandlerSiteMap[i];
    found = FALSE;
    for (j=0; j<length; j++) {
      if ( disabledSites[j] == smartestSite ) {
        found = TRUE;
        break;
      }
    }
    if ( found == TRUE ) {
      sSiteMask[i] = '0';    /* mask this site */
    } else {
      sSiteMask[i] = '1';
    }
  }
  sSiteMask[numOfSites] = '\0';  /* append the EOS */

  return sSiteMask;
}

/*****************************************************************************
 *
 * convert the handler's site mask from binary to hex
 *
 * binary site mask: "0111111111011111" where the MSB is for site 1, i.e Site 1 is masked
 *   converted to
 * hex site mask: "FBFE" where the LSB is for site 1
 *
 * Authors: Jiawei Lin, Sep 2008
 *
 * History:  Fixed some bugs, Xiaofei Han, 2/25/2008,
 *
 ***************************************************************************/
static char * ConvertBinSiteMaskToHex(const char *binSiteMask)
{
  static char sHexSiteMask[64] = "";

  int numOfSites = strlen(binSiteMask);
  int site = numOfSites - 1;
  int nibbleValue = 0;
  int val = 0;
  char nibbleString[64] = "";

  sHexSiteMask[0] = '\0';
  while( site >= 0 ) {
    val = binSiteMask[site] - '0';
    nibbleValue += (val<<site);
    site--;
  }

  sprintf(nibbleString, "%X", nibbleValue);
  strcpy(sHexSiteMask, nibbleString);

  return sHexSiteMask;
}

/*****************************************************************************
 *
 * retrieve a specific ID value
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Performs the necessary.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameGetID(
    struct phFrameStruct *f             /* the framework data */,
    const char *idRequestString,
    const char **outputString
)
{
    static char tmpComment[CI_MAX_COMMENT_LEN];

    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    phFuncError_t funcError;
    phFuncAvailability_t pluginCall;
/*    phEventResult_t exceptionResult = PHEVENT_RES_VOID;*/
/*    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;*/

    char definition[CI_MAX_COMMENT_LEN];
    char result[CI_MAX_COMMENT_LEN];
    char revision[CI_MAX_COMMENT_LEN];
    char *idString;
    char *token;
    int tmpUsed = 0;
    int success = 0;
    int completed = 0;
    int keyFound;

    tmpComment[0] = '\0';

    /* -fwritablestrings not supported with GCC version >= 4 */
    char *myIdRequestString = (char*)malloc(strlen(idRequestString)+1);
    if (myIdRequestString != NULL)
        strcpy(myIdRequestString, idRequestString);
    token = strtok(myIdRequestString, " \t\n");

    while (token)
    {
	definition[0] = '\0';
	strcpy(result, "not_available");
	success = 0;
	completed = 0;
	keyFound = 0;

	/* test for driver SW ID */
	if (strcasecmp(token, PHKEY_NAME_ID_DRIVER) == 0)
	{
	    sprintf(revision, "%s", PHFRAME_IDENT " " PHFRAME_REVISION);

#ifdef PHFRAME_LOCAL
	    strcat(revision, " (local version ");
	    strcat(revision, PHFRAME_LOCAL);
	    strcat(revision, ")");
#endif

	    sprintf(definition,"%s ID: \"%s\" compiled at %s", 
		PHKEY_NAME_ID_DRIVER, revision, PHFRAME_DATE);
	    sprintf(result,"\"%s\" compiled at %s", 
		revision, PHFRAME_DATE);
	    keyFound = 1;
	    success = 1;
	    completed = 1;
	}

	funcError = PHFUNC_ERR_OK;
	pluginCall = (phFuncAvailability_t) 0;
	phFrameStartTimer(f->totalTimer);    
	phFrameStartTimer(f->shortTimer);    
	while (!success && !completed)
	{
	    /* test for plugin SW ID */
	    if (strcasecmp(token, PHKEY_NAME_ID_PLUGIN) == 0)
	    {
		keyFound = 1;
		pluginCall = PHFUNC_AV_DRIVERID;
		if ((f->funcAvail & PHFUNC_AV_DRIVERID) &&
		    (funcError = phPlugFuncDriverID(f->funcId, &idString)) == PHFUNC_ERR_OK)
		{
		    sprintf(definition, "%s ID: %s", PHKEY_NAME_ID_PLUGIN, idString);
		    sprintf(result, "%s", idString);
		}
		else
		{
		    sprintf(definition, "%s ID: not available", PHKEY_NAME_ID_PLUGIN);
		    sprintf(result, "%s_id_not_available", PHKEY_NAME_ID_PLUGIN);
		}
	    }


	    /* test for handler HW ID */
	    else if (strcasecmp(token, PHKEY_NAME_ID_EQUIPMENT) == 0)
	    {
		keyFound = 1;
		pluginCall = PHFUNC_AV_EQUIPID;
		if ((f->funcAvail & PHFUNC_AV_EQUIPID) &&
		    (funcError = phPlugFuncEquipID(f->funcId, &idString)) == PHFUNC_ERR_OK)
		{
		    sprintf(definition, "%s ID: %s", PHKEY_NAME_ID_EQUIPMENT, idString);
		    sprintf(result, "%s", idString);
		}
		else
		{
		    sprintf(definition, "%s ID: not available", PHKEY_NAME_ID_EQUIPMENT);
		    sprintf(result, "%s_id_not_available", PHKEY_NAME_ID_EQUIPMENT);
		}
	    }
	    
        /* test for handler INDEX ID - added Oct 14 2004 KAW */
	    else if (strcasecmp(token, PHKEY_NAME_ID_STRIP_INDEX_ID) == 0)
	    {
		keyFound = 1;
		pluginCall = PHFUNC_AV_STRIP_INDEXID;
		if ((f->funcAvail & PHFUNC_AV_STRIP_INDEXID) &&
		    (funcError = phPlugFuncStripIndexID(f->funcId, &idString)) == PHFUNC_ERR_OK)
		{
		    sprintf(definition, "%s ID: %s", PHKEY_NAME_ID_STRIP_INDEX_ID, idString);
		    sprintf(result, "%s", idString);
		}
		else
		{
		    sprintf(definition, "%s ID: not available", PHKEY_NAME_ID_STRIP_INDEX_ID);
		    sprintf(result, "%s_id_not_available", PHKEY_NAME_ID_STRIP_INDEX_ID);
		}
	    }

        /* test for handler MATERIAL ID - added Oct 14 2004 KAW */
	    else if (strcasecmp(token, PHKEY_NAME_ID_STRIP_ID) == 0)
	    {
		keyFound = 1;
		pluginCall = PHFUNC_AV_STRIPID;
		if ((f->funcAvail & PHFUNC_AV_STRIPID) &&
		    (funcError = phPlugFuncStripMaterialID(f->funcId, &idString)) == PHFUNC_ERR_OK)
		{
		    sprintf(definition, "%s ID: %s", PHKEY_NAME_ID_STRIP_ID, idString);
		    sprintf(result, "%s", idString);
		}
		else
		{
		    sprintf(definition, "%s ID: not available", PHKEY_NAME_ID_STRIP_ID);
		    sprintf(result, "%s_id_not_available", PHKEY_NAME_ID_STRIP_ID);
		}
	    }



#ifdef LOT_ID_REQUEST_AVAILABLE
	    /* test for lot ID */
	    else if (strcasecmp(token, PHKEY_NAME_ID_LOT) == 0)
	    {
		keyFound = 1;
		pluginCall = PHFUNC_AV_LOTID;
		if ((f->funcAvail & PHFUNC_AV_LOTID) &&
		    (funcError = phPlugFuncLotID(f->funcId, &idString)) == PHFUNC_ERR_OK)
		{
		    sprintf(definition, "%s ID: %s", PHKEY_NAME_ID_LOT, idString);
		    sprintf(result, "%s", idString);
		}
		else
		{
		    sprintf(definition, "%s ID: not available", PHKEY_NAME_ID_LOT);
		    sprintf(result, "%s_id_not_available", PHKEY_NAME_ID_LOT);
		}
	    }
#endif

	    if (keyFound)
		phFrameHandlePluginResult(f, funcError, pluginCall, 
		    &completed, &success, &retVal);
	    else
		completed = 1;
	}

	/* ID request not understood */
	if (!keyFound)
	{
	    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
		"identification for key \"%s\" is not defined", token);
	}

	/* step to next, before printing current */
	token = strtok(NULL, " \t\n");

	phLogFrameMessage(f->logId, LOG_NORMAL,
	    "ID request: \"%s\"", definition);
	if (strlen(result) >= (CI_MAX_COMMENT_LEN - tmpUsed) - 2)
	    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
		"ID string to long, can not be returned");
	else
	{
	    strcat(tmpComment, result);
	    tmpUsed += strlen(result);
	    if (token)
	    {
		strcat(tmpComment, ", ");
		tmpUsed += 2;
	    }
	}
    }

    if (myIdRequestString != NULL)
       free(myIdRequestString);

    *outputString = &tmpComment[0];
    return retVal;
}


/*****************************************************************************
 *
 * retrieve specific configuration status values
 *
 * Authors: Ken Ward
 *
 * Description:
 * Performs the necessary.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameGetStatus(
                                   struct phFrameStruct *f             /* the framework data */,
                                   char *commandString,
                                   char **responseString
                                   )
{
    /*Begin CR92686, Adam Huang, 05 MAR 2015*/
    int success = 0;
    int completed = 0;
    static char response[CI_MAX_COMMENT_LEN];
    char *responsePtr = NULL;
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    phFuncError_t funcError;
    phFuncAvailability_t pluginCall = PHFUNC_AV_GET_STATUS;

    phLogFrameMessage(f->logId, LOG_DEBUG, "entering phFrameGetStatus");
    if ( f->funcAvail & PHFUNC_AV_GET_STATUS )
    {
        while(!success && !completed)
        {
            phLogFrameMessage(f->logId, LOG_DEBUG, "about to call phPlugFuncGetStatus, commandString: ->%s<-", commandString);
            if (( funcError = phPlugFuncGetStatus(f->funcId, commandString, &responsePtr)) == PHFUNC_ERR_OK )
            {
                phLogFrameMessage(f->logId, LOG_NORMAL, "phPlugFuncGetStatus \"%s\" returned: \"%s\"", commandString, responsePtr);
            }
            else
            {
                phLogFrameMessage(f->logId, LOG_DEBUG, "phPlugFuncGetStatus \"%s\" returned error: %d", commandString, funcError);
                /*retVal = PHTCOM_CI_CALL_ERROR;*/
            }
            phFrameHandlePluginResult(f, funcError, pluginCall, &completed, &success, &retVal);
        }

        if (responsePtr != NULL && strlen(responsePtr) <= CI_MAX_COMMENT_LEN)
        {
            strcpy(response, responsePtr);
        }
        else
        {
            strcpy(response, "");
        }
    }
    else
    {
        /*
        phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
        "function not available, f->funcAvail = %x, PHFUNC_AV_GET_STATUS = %x",
        f->funcAvail, PHFUNC_AV_GET_STATUS);
        retVal = PHTCOM_CI_CALL_ERROR; 
        */

        /* CR42623/CR33707 Xiaofei Han, make the behavior of ph_get_status with other driver functions */
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "ph_get_status is not available for the current handler driver");
        strcpy(response, "ph_get_status_FUNCTION_NOT_AVAILABLE");
    }

    *responseString = response;
    phLogFrameMessage(f->logId, LOG_DEBUG, "exiting phFrameGetStatus, response = ->%s<-", response);

    return retVal;
    /*End CR92686*/
}


/*****************************************************************************
 *
 * set specific configuration status values
 *
 * Authors: Ken Ward
 *
 * Description:
 * Performs the necessary.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameSetStatus(
                                   struct phFrameStruct *f             /* the framework data */,
                                   char *commandString
                                   )
{
    phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
    phFuncError_t funcError;
    phFuncAvailability_t pluginCall;

    int completed = 0;
    int success = 0;

    static const char *sDisabledSitesToken = "disabled_sites:";
    char *pCommandString = commandString;
    char newCommandString[128] = "";

    pluginCall = PHFUNC_AV_SET_STATUS;
    if ( f->funcAvail & PHFUNC_AV_SET_STATUS )
    {
        /*
         * enhance for Riesling
         * the AMP will call "PROB_HND_CALL(ph_set_status disabled_sites:[ 1 3 ])" when the smartest's Site 1 and Site 3
         * are disabled, due to the hardware failure. that's, the original input param "commandString" takes the value
         * "disabled_sites:[ 1 3 ]"
         *
         * the new "command string" will be like "contactsel FFEF"
         * Jiawei Lin, Sep 2008
         */
        if ( strstr(commandString, sDisabledSitesToken) != NULL ) {
          phLogFrameMessage(f->logId, LOG_DEBUG, "%s", commandString);
          strcpy(newCommandString, "contactsel ");
          /* convert the disabled sites to handler's site mask (in Hex) */
          char *binSiteMask = ConvertSmartestDisabledSite2SiteMask(f, commandString);
          phLogFrameMessage(f->logId, LOG_DEBUG, "the handler new Site Mask in Binary is %s (MSB is Site 1)", binSiteMask);
          char *hexSiteMask = ConvertBinSiteMaskToHex(binSiteMask);
          phLogFrameMessage(f->logId, LOG_DEBUG, "the handler new Site Mask in Hex is %s (LSB is Site 1)", hexSiteMask);
          strcat(newCommandString, hexSiteMask);

          pCommandString = newCommandString;
        }

        /*repeats until the handler reply*/
        while(!success && !completed)
        {
            phLogFrameMessage(f->logId, LOG_DEBUG, "about to call phPlugFuncSetStatus, commandString: ->%s<-", commandString);
            if (( funcError = phPlugFuncSetStatus(f->funcId, pCommandString)) == PHFUNC_ERR_OK )
            {
                phLogFrameMessage(f->logId, LOG_NORMAL, "SET_STATUS: \"%s\"", pCommandString);
            }
            else
            {
                phLogFrameMessage(f->logId, LOG_DEBUG, "SET_STATUS \"%s\" returned error: %d", pCommandString, funcError);
            }
            phFrameHandlePluginResult(f, funcError, pluginCall, &completed, &success, &retVal);
        }
    }
    else
    {
        /* CR42623/CR33707 Xiaofei Han, make the behavior of ph_set_status with other driver functions */
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                          "ph_set_status is not available for the current handler driver");
        /* retVal = PHTCOM_CI_CALL_ERROR;*/
    }

    return retVal;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
