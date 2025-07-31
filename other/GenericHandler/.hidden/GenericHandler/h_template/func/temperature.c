/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : temperature.c
 * CREATED  : 29 Jun 2000
 *
 * CONTENTS : Handling of temperature configurations
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Jun 2000, Michael Vogt, created
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
#include <math.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_hfunc.h"
#include "ph_keys.h"

#include "ph_hfunc_private.h"
#include "temperature.h"
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


/* return codes:
   0: OK
   1: warned, not given
   2: error, wrong format
*/
static int getNumValues(
    phConfId_t conf           /* configuration to read values from */,
    phLogId_t logger          /* report messages here */,
    int n                     /* number of values to get */,
    const char *confKey       /* configuration entry that defines a
                                 list of numbers */,
    double *values            /* array to store the found values */
)
{
    phConfError_t confError;
    phConfType_t confType;
    double oneValue = 0.0;
    int entries;
    int i;

    confError = phConfConfIfDef(conf, confKey);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfType(conf, confKey, 0, NULL,
	    &confType, &entries);
    if (confError != PHCONF_ERR_OK)
    {
	/* configuration not given */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "configuration parameter \"%s\" not given", confKey);
	return 1;
    }

    if (confType == PHCONF_TYPE_NUM)
    {
	/* it is only one value */
	confError = phConfConfNumber(conf, confKey, 0, NULL, &oneValue);
	if (confError != PHCONF_ERR_OK)
	{
	    /* did not get the value */
	    phLogFuncMessage(logger, PHLOG_TYPE_ERROR,
		"configuration parameter \"%s\" of wrong type,\n"
		"must be a list of %d numbers or one single number", confKey, n);
	    return 2;
	}
	for (i=0; i<n; i++)
	    values[i] = oneValue;

	return 0;
    }

    else if (confType == PHCONF_TYPE_LIST && entries == n)
    {
	/* it is a list with the expected size */
	for (i=0; i<n; i++)
	{
	    confError = phConfConfNumber(conf, confKey, 1, &i, &values[i]);
	    if (confError != PHCONF_ERR_OK)
	    {
		/* did not get the value */
		phLogFuncMessage(logger, PHLOG_TYPE_ERROR,
		    "configuration parameter \"%s\" of wrong type,\n"
		    "must be a list of %d numbers", confKey, n);
		return 2;
	    }
	}

	return 0;
    }

    else
    {
	/* configuration of wrong type */
	phLogFuncMessage(logger, PHLOG_TYPE_ERROR,
	    "configuration parameter \"%s\" of wrong type,\n"
	    "must be a list of %d numbers or one single number", confKey, n);
	return 2;
    }
}

/* return codes:
   0: OK
   1: warned, not given
   2: error, wrong format
*/
static int getStringValues(
    phConfId_t conf           /* configuration to read values from */,
    phLogId_t logger          /* report messages here */,
    int n                     /* number of values to get */,
    const char *confKey       /* configuration entry that defines a
                                 list of numbers */,
    const char **values       /* array to store the found values */
)
{
    phConfError_t confError;
    phConfType_t confType;
    const char *oneString = NULL;
    int entries;
    int i;

    confError = phConfConfIfDef(conf, confKey);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfType(conf, confKey, 0, NULL,
	    &confType, &entries);
    if (confError != PHCONF_ERR_OK)
    {
	/* configuration not given */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "configuration parameter \"%s\" not given", confKey);
	return 1;
    }

    if (confType == PHCONF_TYPE_STR)
    {
	/* it is only one string */
	confError = phConfConfString(conf, confKey, 1, &i, &oneString);
	if (confError != PHCONF_ERR_OK)
	{
	    /* did not get the value */
	    phLogFuncMessage(logger, PHLOG_TYPE_ERROR,
		"configuration parameter \"%s\" of wrong type,\n"
		"must be a list of %d strings or one single string", confKey, n);
	    return 2;
	}
	for (i=0; i<n; i++)
	    values[i] = oneString;

	return 0;
    }

    if (confType == PHCONF_TYPE_LIST && entries == n)
    {
	/* it is a list with the expected size */
	for (i=0; i<n; i++)
	{
	    confError = phConfConfString(conf, confKey, 1, &i, &values[i]);
	    if (confError != PHCONF_ERR_OK)
	    {
		/* did not get the value */
		phLogFuncMessage(logger, PHLOG_TYPE_ERROR,
		    "configuration parameter \"%s\" of wrong type,\n"
		    "must be a list of %d strings or one single string", confKey, n);
		return 2;
	    }
	}

	return 0;
    }

    else
    {
	/* configuration of wrong type */
	phLogFuncMessage(logger, PHLOG_TYPE_ERROR,
	    "configuration parameter \"%s\" of wrong type,\n"
	    "must be a list of %d strings", confKey, n);
	return 2;
    }

}

/*****************************************************************************
 *
 * Get temperature defintions from configuration
 *
 * Authors: Michael Vogt
 *
 * Description: 
 * Retrieve all temperature parameters from the given
 * configuration and fill up a data struct. Temperature parameters are
 * converted to celsius by default.
 *
 * Returns: 1 if OK, 0 if the configuration was corrupt
 *
 ***************************************************************************/

int phFuncTempGetConf(
    phConfId_t conf           /* configuration to read values from */,
    phLogId_t logger          /* report messages here */,
    struct tempSetup *data    /* data struct to be filled by this function */
)
{
    int retVal = 1;
    int found = 0;
    int found2 = 0;
    phConfError_t confError;
    phConfType_t confType;
    int entries;
    const char *name;
    double values[PHFUNC_TEMP_MAX_CHAMBERS];
    const char *stringValues[PHFUNC_TEMP_MAX_CHAMBERS];
    int i;

    double convertMult = 1.0;
    double convertAdd = 0.0;
    char handlerUnit[64] = "";

    /* check for control mode */
    confError = phConfConfStrTest(&found, conf,
	PHKEY_TC_CONTROL, "manual", "ambient", "active", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
	return 0;
    else
    {
	switch (found)
	{
	  case 2:
	    data->mode = PHFUNC_TEMP_AMBIENT;
	    break;
	  case 3:
	    data->mode = PHFUNC_TEMP_ACTIVE;
	    break;
	  case 1:
	  default:
	    data->mode = PHFUNC_TEMP_MANUAL;
	    break;	    
	}
    }

    if (data->mode != PHFUNC_TEMP_ACTIVE)
    {
	/* early return with success, if not activated */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "\"%s\" not set to be \"active\", will ignore all remaining temperature parameters",
	    PHKEY_TC_CONTROL);
	return 1;
    }

    /* check for temperature unit */
    confError = phConfConfStrTest(&found, conf,
	PHKEY_TC_CUNIT, "celsius", "fahrenheit", "kelvin", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
	return 0;
    if (found == 0)
	found = 1;
    switch (found)
    {
      case 1:
	phLogFuncMessage(logger, LOG_DEBUG,
	    "configured temperature values are treated to be given in \"celsius\"");
	break;
      case 2:
	phLogFuncMessage(logger, LOG_DEBUG,
	    "configured temperature values are treated to be given in \"fahrenheit\"");
	break;
      case 3:
	phLogFuncMessage(logger, LOG_DEBUG,
	    "configured temperature values are treated to be given in \"kelvin\"");
	break;	
    }

    confError = phConfConfStrTest(&found2, conf,
	PHKEY_TC_HUNIT, "celsius", "fahrenheit", "kelvin", NULL);
    if (confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK)
	return 0;
    if (found2 == 0)
	found2 = 1;
    switch (found2)
    {
      case 1:
	phLogFuncMessage(logger, LOG_DEBUG,
	    "handler temperature values are setup in \"celsius\"");
	strcpy(handlerUnit, "celsius");
	break;
      case 2:
	phLogFuncMessage(logger, LOG_DEBUG,
	    "handler temperature values are setup in \"fahrenheit\"");
	strcpy(handlerUnit, "fahrenheit");
	break;
      case 3:
	phLogFuncMessage(logger, LOG_DEBUG,
	    "handler temperature values are setup in \"kelvin\"");
	strcpy(handlerUnit, "kelvin");
	break;	
    }


    /* the handler value h is calculated from the given value g by:
       h = convertMult * g + convertAdd (linear transformation) */
    switch (found)
    {
      case 1:
	switch (found2)
	{
	  case 1: /* C*m + a -> C */
	    convertMult = 1.0;
	    convertAdd = 0.0;
	    break;
	  case 2: /* C*m + a -> F */
	    convertMult = 9.0/5.0;
	    convertAdd = 32;
	    break;
	  case 3: /* C*m + a -> K */
	    convertMult = 1.0;
	    convertAdd = 273.15;
	    break;
	}
	break;	    
      case 2:
	switch (found2)
	{
	  case 1: /* F*m + a -> C */
	    convertMult = 5.0/9.0;
	    convertAdd = - 32 * 5.0/9.0;
	    break;
	  case 2: /* F*m + a -> F */
	    convertMult = 1.0;
	    convertAdd = 0.0;
	    break;
	  case 3: /* F*m + a -> K */
	    convertMult = 5.0/9.0;
	    convertAdd = 273.15 - 32 * 5.0/9.0;
	    break;
	}
	break;
      case 3:
	switch (found2)
	{
	  case 1: /* K*m + a -> C */
	    convertMult = 1.0;
	    convertAdd = -273.15;
	    break;
	  case 2: /* K*m + a -> F */
	    convertMult = 9.0/5.0;
	    convertAdd = 32 - 273.15 * 9.0/5.0;
	    break;
	  case 3: /* K*m + a -> K */
	    convertMult = 1.0;
	    convertAdd = 0.0;
	    break;
	}
	break;
    }

    /* check for the chamber names and number of chambers */
    confError = phConfConfIfDef(conf, PHKEY_TC_CHAMB);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfType(conf, PHKEY_TC_CHAMB, 0, NULL,
	    &confType, &entries);
    if (confError != PHCONF_ERR_OK || confType != PHCONF_TYPE_LIST || entries <= 0)
    {
	/* illegal or non existing definition of chamber names */
	return 0;
    }
    else
    {
	if (entries > PHFUNC_TEMP_MAX_CHAMBERS)
	{
	    /* to many chambers defined */
	    return 0;
	}
	data->chambers = entries;
	for (i=0; i<data->chambers; i++)
	{
	    confError = phConfConfString(conf, PHKEY_TC_CHAMB, 1, &i, &name);
	    if (confError == PHCONF_ERR_OK)
		data->name[i] = strdup(name);
	    else
		return 0;
	}
    }

    /* if we come here, we know how many chambers there are and we
       have the names */

    /* today, all chambers are treated the same, we can not switch
       some of them off. Only the named one are used anyway. In the
       future they may be other configurations that allow to choose
       the activation of each chamber individually */
    for (i=0; i<data->chambers; i++)
	data->control[i] = PHFUNC_TEMP_ON;
    
    /* get setpoint of chamber temperature */
    switch (getNumValues(conf, logger, data->chambers, PHKEY_TC_SET, values))
    {
      case 0:
	for (i=0; i<data->chambers; i++)
	{
	    data->setpoint[i] = values[i]*convertMult + convertAdd;
	    phLogFuncMessage(logger, LOG_DEBUG,
		"temperature value %d of \"%s\" converted to %g %s",
		i, PHKEY_TC_SET, data->setpoint[i], handlerUnit);
	}
	break;
      case 1:
	/* temperature control activated but no values defined */
	phLogFuncMessage(logger, PHLOG_TYPE_ERROR,
	    "\"%s\" set to be \"active\" but no setpoints given with \"%s\"",
	    PHKEY_TC_CONTROL,  PHKEY_TC_SET);
	retVal = 0;
	break;
      case 2:
	retVal = 0;
	break;
    }

    /* get upper guard of chamber temperature */
    switch (getNumValues(conf, logger, data->chambers, PHKEY_TC_UGUARD, values))
    {
      case 0:
	for (i=0; i<data->chambers; i++)
	{
	    data->uguard[i] = fabs(values[i])*convertMult;
	    phLogFuncMessage(logger, LOG_DEBUG,
		"relative temperature value %d of \"%s\" converted to %g %s",
		i, PHKEY_TC_UGUARD, data->uguard[i], handlerUnit);
	}
	break;
      case 1:
	/* temperature control activated but no values defined */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "\"%s\" set to be \"active\" but no upper guard limits given with \"%s\",\n"
	    "assuming upper guard is not active",
	    PHKEY_TC_CONTROL,  PHKEY_TC_UGUARD);
	for (i=0; i<data->chambers; i++)
	    data->uguard[i] = -1.0;
	break;
      case 2:
	retVal = 0;
	break;
    }

    /* get lower guard of chamber temperature */
    switch (getNumValues(conf, logger, data->chambers, PHKEY_TC_LGUARD, values))
    {
      case 0:
	for (i=0; i<data->chambers; i++)
	{
	    data->lguard[i] = fabs(values[i])*convertMult;
	    phLogFuncMessage(logger, LOG_DEBUG,
		"relative temperature value %d of \"%s\" converted to %g %s",
		i, PHKEY_TC_LGUARD, data->lguard[i], handlerUnit);
	}
	break;
      case 1:
	/* temperature control activated but no values defined */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "\"%s\" set to be \"active\" but no lower guard limits given with \"%s\",\n"
	    "assuming lower guard is not active",
	    PHKEY_TC_CONTROL,  PHKEY_TC_LGUARD);
	for (i=0; i<data->chambers; i++)
	    data->lguard[i] = -1.0;
	break;
      case 2:
	retVal = 0;
	break;
    }

    /* get soak time */
    switch (getNumValues(conf, logger, data->chambers, PHKEY_TC_SOAK, values))
    {
      case 0:
	for (i=0; i<data->chambers; i++)
	{
	    data->soaktime[i] = fabs(values[i]);
	    phLogFuncMessage(logger, LOG_DEBUG,
		"time value %d of \"%s\" converted to %g seconds",
		i, PHKEY_TC_SOAK, data->soaktime[i]);
	}
	break;
      case 1:
	/* temperature control activated but no values defined */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "\"%s\" set to be \"active\" but no soak time given with \"%s\",\n"
	    "assuming soak time is not active",
	    PHKEY_TC_CONTROL,  PHKEY_TC_SOAK);
	for (i=0; i<data->chambers; i++)
	    data->soaktime[i] = -1.0;
	break;
      case 2:
	retVal = 0;
	break;
    }

    /* get desoak time */
    switch (getNumValues(conf, logger, data->chambers, PHKEY_TC_DESOAK, values))
    {
      case 0:
	for (i=0; i<data->chambers; i++)
	{
	    data->desoaktime[i] = fabs(values[i]);
	    phLogFuncMessage(logger, LOG_DEBUG,
		"time value %d of \"%s\" converted to %g seconds",
		i, PHKEY_TC_DESOAK, data->desoaktime[i]);
	}
	break;
      case 1:
	/* temperature control activated but no values defined */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "\"%s\" set to be \"active\" but no desoak time given with \"%s\",\n"
	    "assuming desoak time is not active",
	    PHKEY_TC_CONTROL,  PHKEY_TC_DESOAK);
	for (i=0; i<data->chambers; i++)
	    data->desoaktime[i] = -1.0;
	break;
      case 2:
	retVal = 0;
	break;
    }

    /* get heat flags */
    switch (getStringValues(conf, logger, data->chambers, PHKEY_TC_HEAT, stringValues))
    {
      case 0:
	for (i=0; i<data->chambers; i++)
	{
	    if (strcasecmp(stringValues[i], "yes") == 0)
	    {
		data->heat[i] = PHFUNC_TEMP_ON;
		phLogFuncMessage(logger, LOG_DEBUG,
		    "switch %d of \"%s\" converted to \"active\"",
		    i, PHKEY_TC_HEAT);
	    }
	    else if (strcasecmp(stringValues[i], "no") == 0)
	    {
		data->heat[i] = PHFUNC_TEMP_OFF;
		phLogFuncMessage(logger, LOG_DEBUG,
		    "switch %d of \"%s\" converted to \"inactive\"",
		    i, PHKEY_TC_HEAT);
	    }
	    else
	    {
		data->heat[i] = PHFUNC_TEMP_UNDEF;
		phLogFuncMessage(logger, LOG_DEBUG,
		    "switch %d of \"%s\" converted to \"manual\"",
		    i, PHKEY_TC_HEAT);
	    }
	}
	break;
      case 1:
	/* temperature control activated but no values defined */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "\"%s\" set to be \"active\" but heating is not activated with \"%s\",\n"
	    "assuming heating is not active",
	    PHKEY_TC_CONTROL,  PHKEY_TC_HEAT);
	for (i=0; i<data->chambers; i++)
	    data->heat[i] = PHFUNC_TEMP_UNDEF;
	break;
      case 2:
	retVal = 0;
	break;
    }

    /* get cool flags */
    switch (getStringValues(conf, logger, data->chambers, PHKEY_TC_COOL, stringValues))
    {
      case 0:
	for (i=0; i<data->chambers; i++)
	{
	    if (strcasecmp(stringValues[i], "yes") == 0)
	    {
		data->cool[i] = PHFUNC_TEMP_ON;
		phLogFuncMessage(logger, LOG_DEBUG,
		    "switch %d of \"%s\" converted to \"active\"",
		    i, PHKEY_TC_COOL);
	    }
	    else if (strcasecmp(stringValues[i], "no") == 0)
	    {
		data->cool[i] = PHFUNC_TEMP_OFF;
		phLogFuncMessage(logger, LOG_DEBUG,
		    "switch %d of \"%s\" converted to \"inactive\"",
		    i, PHKEY_TC_COOL);
	    }
	    else
	    {
		data->cool[i] = PHFUNC_TEMP_UNDEF;
		phLogFuncMessage(logger, LOG_DEBUG,
		    "switch %d of \"%s\" converted to \"manual\"",
		    i, PHKEY_TC_COOL);
	    }
	}
	break;
      case 1:
	/* temperature control activated but no values defined */
	phLogFuncMessage(logger, LOG_DEBUG,
	    "\"%s\" set to be \"active\" but cooling is not activated with \"%s\",\n"
	    "assuming cooling is not active",
	    PHKEY_TC_CONTROL,  PHKEY_TC_COOL);
	for (i=0; i<data->chambers; i++)
	    data->cool[i] = PHFUNC_TEMP_UNDEF;
	break;
      case 2:
	retVal = 0;
	break;
    }

    return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
