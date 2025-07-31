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
 *            Jiawei Lin, R&D Shanghai, CR 27092 & CR 25172
 *            Garry Xie,R&D Shanghai, CR28427
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 24 Jan 2000, Michael Vogt, created
 *            Aug 2005, CR27092 & CR 25172
 *              Implement the function "phFrameGetStatus".
 *              It could be used to request parameter/information
 *              from Prober. Thus more datalog are possible, like WCR(Wafer 
 *              Configuration Record),Wafer_ID, Wafer_Number, Cassette Status 
 *              and Chuck Temperature.
 *            February 2006, Garry Xie, CR28427
 *              Implement the function "phFrameSetStatus".
 *              It could be used to set parameter/information
 *              to Prober. like Alarm_buzzer.
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
#include "ph_pfunc.h"
#include "ph_phook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"

#include "ph_timer.h"
#include "ph_frame_private.h"
#include "identity.h"
#include "exception.h"
#include "idrequest.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

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
    phPFuncError_t funcError;
    phPFuncAvailability_t pluginCall;
/* Begin of Huatek Modification, Donnie Tu, 12/10/2001 */
/* Issue Number: 315 */
/*    phEventResult_t exceptionResult = PHEVENT_RES_VOID;*/
/*    phTcomUserReturn_t exceptionReturn = PHTCOM_CI_CALL_PASS;*/
/* End of Huatek Modification */

    char definition[CI_MAX_COMMENT_LEN];
    char result[CI_MAX_COMMENT_LEN];
    char revision[CI_MAX_COMMENT_LEN];
    char *idString;
    char *token;
    int tmpUsed = 0;

    exceptionActionError_t action;
    int success;
    int completed;

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
	}

	funcError = PHPFUNC_ERR_OK;
        action = PHFRAME_EXCEPTION_ERR_OK;
        success = 0;
        completed = 0;
	pluginCall = (phPFuncAvailability_t) 0;
	phFrameStartTimer(f->totalTimer);    
	phFrameStartTimer(f->shortTimer);    

        while (!success && !completed)
        {
	    /* test for plugin SW ID */
	    if (strcasecmp(token, PHKEY_NAME_ID_PLUGIN) == 0)
	    {
		keyFound = 1;
		pluginCall = PHPFUNC_AV_DRIVERID;
		if ((f->funcAvail & PHPFUNC_AV_DRIVERID) &&
		    (funcError = phPlugPFuncDriverID(f->funcId, &idString)) == PHPFUNC_ERR_OK)
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

	    /* test for prober HW ID */
	    else if (strcasecmp(token, PHKEY_NAME_ID_EQUIPMENT) == 0)
	    {
		keyFound = 1;
		pluginCall = PHPFUNC_AV_EQUIPID;
		if ((f->funcAvail & PHPFUNC_AV_EQUIPID) &&
		    (funcError = phPlugPFuncEquipID(f->funcId, &idString)) == PHPFUNC_ERR_OK)
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

	    /* test for cassette ID */
	    else if (strcasecmp(token, PHKEY_NAME_ID_CASS) == 0)
	    {
		keyFound = 1;
		pluginCall = PHPFUNC_AV_CASSID;
		if ((f->funcAvail & PHPFUNC_AV_CASSID) &&
		    (funcError = phPlugPFuncCassID(f->funcId, &idString)) == PHPFUNC_ERR_OK)
		{
		    sprintf(definition, "%s ID: %s", PHKEY_NAME_ID_CASS, idString);
		    sprintf(result, "%s", idString);
		}
		else
		{
		    sprintf(definition, "%s ID: not available", PHKEY_NAME_ID_CASS);
		    sprintf(result, "%s_id_not_available", PHKEY_NAME_ID_CASS);
		}
	    }

	    /* test for wafer ID */
	    else if (strcasecmp(token, PHKEY_NAME_ID_WAFER) == 0)
	    {
		keyFound = 1;
		pluginCall = PHPFUNC_AV_WAFID;
		if ((f->funcAvail & PHPFUNC_AV_WAFID) &&
		    (funcError = phPlugPFuncWafID(f->funcId, &idString)) == PHPFUNC_ERR_OK)
		{
		    sprintf(definition, "%s ID: %s", PHKEY_NAME_ID_WAFER, idString);
		    sprintf(result, "%s", idString);
		}
		else
		{
		    sprintf(definition, "%s ID: not available", PHKEY_NAME_ID_WAFER);
		    sprintf(result, "%s_id_not_available", PHKEY_NAME_ID_WAFER);
		}
	    }

	    /* test for probe card ID */
	    else if (strcasecmp(token, PHKEY_NAME_ID_PROBE) == 0)
	    {
		keyFound = 1;
		pluginCall = PHPFUNC_AV_PROBEID;
		if ((f->funcAvail & PHPFUNC_AV_PROBEID) &&
		    (funcError = phPlugPFuncProbeID(f->funcId, &idString)) == PHPFUNC_ERR_OK)
		{
		    sprintf(definition, "%s ID: %s", PHKEY_NAME_ID_PROBE, idString);
		    sprintf(result, "%s", idString);
		}
		else
		{
		    sprintf(definition, "%s ID: not available", PHKEY_NAME_ID_PROBE);
		    sprintf(result, "%s_id_not_available", PHKEY_NAME_ID_PROBE);
		}
	    }

	    /* test for lot ID */
	    else if (strcasecmp(token, PHKEY_NAME_ID_LOT) == 0)
	    {
		keyFound = 1;
		pluginCall = PHPFUNC_AV_LOTID;
		if ((f->funcAvail & PHPFUNC_AV_LOTID) &&
		    (funcError = phPlugPFuncLotID(f->funcId, &idString)) == PHPFUNC_ERR_OK)
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

	    if (!keyFound)
            {
		completed = 1;
            }
	    else
	    {
                /* analyze result from pluginCall and take action */
                phFrameHandlePluginResult(f, funcError, pluginCall,
                                          &completed, &success, &retVal, &action);
	    }
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
 * retrieve specific status/parameter values from Prober.
 *
 * Authors: Jiawei Lin
 *
 * Description:
 *    It is one of "getStatus" series functions and performs the necessary
 *    actions in Framework side.
 *
 * Input/Output:
 *    Input parameter "commandString" is the key name for information request.
 *    Output the result of request to "outputString".
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
  static char response[MAX_STATUS_MSG_LEN] = "";
  char *responsePtr = NULL;
  phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
  phPFuncError_t funcError = PHPFUNC_ERR_OK;
  int success = NO;
  int completed = NO;
  exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "entering phFrameGetStatus");

  strcpy(response, "");
  if ( f->funcAvail & PHPFUNC_AV_GETSTATUS ) {
    while( (completed==NO) && (success==NO) ) {
      phLogFrameMessage(f->logId, LOG_DEBUG,
                        "about to call phPlugFuncGetStatus, commandString: ->%s<-", commandString);
      if ( ( funcError = phPlugPFuncGetStatus(f->funcId, commandString, &responsePtr)) == PHPFUNC_ERR_OK ){
        phLogFrameMessage(f->logId, LOG_NORMAL,
                          "phPlugFuncGetStatus \"%s\" returned: \"%s\"", commandString, responsePtr);
      } else {
        phLogFrameMessage(f->logId, LOG_DEBUG,
                          "phPlugFuncGetStatus \"%s\" returned error: %d", commandString, funcError);
      }

      /* analyze result from pluginCall and take action */
      phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_GETSTATUS, &completed, &success, &retVal, &action);
    } 
  
    if(responsePtr != NULL)
    {
      if (strlen(responsePtr) < MAX_STATUS_MSG_LEN) {
        strcpy(response, responsePtr);
      } else {
        strncpy(response, responsePtr, MAX_STATUS_MSG_LEN-1);
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "too long response string from Plugin\n"
                          "it will be truncated to maximum length = %d",
                          MAX_STATUS_MSG_LEN-1);
      }
    }
    else
    {
      strcpy(response, "");
    }
  } else {
    /*
    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                      "function 'phFrameGetStatus' not available, f->funcAvail = %x, PHPFUNC_AV_GETSTATUS = %x",
                      f->funcAvail, PHPFUNC_AV_GETSTATUS);
    retVal = PHTCOM_CI_CALL_ERROR;
    */

    /* CR42623/CR33707 Xiaofei Han, make the behavior of ph_get_status with other driver functions */
    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                      "ph_get_status is not available for the current prober driver");
    strcpy(response, "ph_get_status_FUNCTION_NOT_AVAILABLE");
  }

  /* bring out the response string to the caller of this function*/
  *responseString = response;
  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
                    "exiting phFrameGetStatus, responseString = ->%s<-", *responseString);
  return retVal;

}


/*****************************************************************************
 *
 * set specific status/parameter values to Prober.
 *
 * Authors: Garry xie
 *
 * Description:
 *    It is one of "setStatus" series functions and performs the necessary
 *    actions in Framework side.
 *
 * Input/Output:
 *    Input parameter "commandString" is the key name for information request.
 *    Output the result of request to "outputString".
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameSetStatus(
                                    struct phFrameStruct *f             /* the framework data */,
                                    char *commandString,
                                    char **responseString
)
{
  static char response[MAX_STATUS_MSG_LEN] = "";
  char *responsePtr = NULL;
  phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
  phPFuncError_t funcError = PHPFUNC_ERR_OK;
  int success = NO;
  int completed = NO;
  exceptionActionError_t action = PHFRAME_EXCEPTION_ERR_OK;

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "entering phFrameSetStatus");

  strcpy(response, "");
  if ( f->funcAvail & PHPFUNC_AV_SETSTATUS ) {
    while( (completed==NO) && (success==NO) ) {
      phLogFrameMessage(f->logId, LOG_DEBUG,
                        "about to call phPlugFuncSetStatus, commandString: ->%s<-", commandString);
      if ( ( funcError = phPlugPFuncSetStatus(f->funcId, commandString, &responsePtr)) == PHPFUNC_ERR_OK ){
        phLogFrameMessage(f->logId, LOG_NORMAL,
                          "phPlugFuncSetStatus \"%s\" returned: \"%s\"", commandString, responsePtr);
      } else {
        phLogFrameMessage(f->logId, LOG_DEBUG,
                          "phPlugFuncSetStatus \"%s\" returned error: %d", commandString, funcError);
      }

      /* analyze result from pluginCall and take action */
      phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_SETSTATUS, &completed, &success, &retVal, &action);
    } 
  
    if(responsePtr != NULL)
    {
      if (strlen(responsePtr) < MAX_STATUS_MSG_LEN) {
        strcpy(response, responsePtr);
      } else {
        strncpy(response, responsePtr, MAX_STATUS_MSG_LEN-1);
        phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "too long response string from Plugin\n"
                          "it will be truncated to maximum length = %d",
                          MAX_STATUS_MSG_LEN-1);
      }
    }
    else
    {
      strcpy(response, "");
    }
  } else {
    /* 
    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                      "function 'phFrameSetStatus' not available, f->funcAvail = %llx, PHPFUNC_AV_SETSTATUS = %llx",
                      f->funcAvail, PHPFUNC_AV_SETSTATUS);
    retVal = PHTCOM_CI_CALL_ERROR;
    */
    /* CR42623/CR33707 Xiaofei Han, make the behavior of ph_set_status with other driver functions */
    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                      "ph_set_status is not available for the current prober driver");
    strcpy(response, "ph_set_status_FUNCTION_NOT_AVAILABLE");
  }

  /* bring out the response string to the caller of this function*/
  *responseString = response;
  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
                    "exiting phFrameSetStatus, responseString = ->%s<-", *responseString);
  return retVal;

}
/*****************************************************************************
 * End of file
 *****************************************************************************/

