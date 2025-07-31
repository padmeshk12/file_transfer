 /******************************************************************************
 *
 *       (c) Copyright Advantest 2015
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
 *            Ken Ward, Bitsoft Systems, Inc, Mirae revision
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

#ifdef NOTDEFINEDFORMIRAE
/* return codes:
   0: OK
   1: warned, not given
   2: error, wrong format
*/
static int getStringValues(
    phConfId_t conf           /* configuration to read values from */,
    phLogId_t logger          /* report messages here */,
    int n                     /* number of values to get */,
    char *confKey             /* configuration entry that defines a
                                 list of numbers */,
    char **values             /* array to store the found values */
)
{
    phConfError_t confError;
    phConfType_t confType;
    char *oneString = NULL;
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
#endif // NOTDEFINEDFORMIRAE

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
    double values[PHFUNC_TEMP_MAX_CHAMBERS];
    int iValue;
    double dValue;

    double convertMult = 1.0;
    double convertAdd = 0.0;
    char handlerUnit[64] = "";

    /* check for control mode */
    confError = phConfConfStrTest(&found, conf,
                                  PHKEY_TC_CONTROL, 
                                  "off", "hot", "ambient", "cca", NULL);
    if ( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
        return 0;
    else
    {
        --found;
        switch ( found )
        {
            case PHFUNC_TEMP_STOP:
            case PHFUNC_TEMP_HOT:
            case PHFUNC_TEMP_AMBIENT:
            case PHFUNC_TEMP_CCA:
                data->mode = (enum tempControl)found;
                break;
            default:
                data->mode = PHFUNC_TEMP_STOP;
                break;      
        }
    }

    if (data->mode == PHFUNC_TEMP_STOP) 
    {
        /* early return with success, if not activated */
        phLogFuncMessage(logger, LOG_DEBUG,
                         "\"%s\" set to be \"stop\", will ignore all remaining temperature parameters",
                         PHKEY_TC_CONTROL);
        return 1;
    }

    /* check for temperature unit */
    confError = phConfConfStrTest(&found, conf,
                                  PHKEY_TC_CUNIT, "celsius", "fahrenheit", "kelvin", NULL);
    if ( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
        return 0;
    if ( found == 0 ) found = 1; // default to celsius
    switch ( found )
    {
        case 1:
            phLogFuncMessage(logger, LOG_DEBUG,
                             "configured temperature values are treated as given in \"celsius\"");
            break;
        case 2:
            phLogFuncMessage(logger, LOG_DEBUG,
                             "configured temperature values are treated as given in \"fahrenheit\"");
            break;
        case 3:
            phLogFuncMessage(logger, LOG_DEBUG,
                             "configured temperature values are treated as given in \"kelvin\"");
            break;  
    }

    /* Mirae MR5800 supports only Celsius */
    found2 = 1;
    strcpy(handlerUnit, "celsius");
    /*
    confError = phConfConfStrTest(&found2, conf,
                                  PHKEY_TC_HUNIT, "celsius", "fahrenheit", "kelvin", NULL);
    if ( confError != PHCONF_ERR_UNDEF && confError != PHCONF_ERR_OK )
        return 0;
    if ( found2 == 0 ) found2 = 1; // default to celsius
    switch ( found2 )
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
    */

    /* the handler value h is calculated from the given value g by:
       h = convertMult * g + convertAdd (linear transformation) */
    switch ( found )
    {
        case 1:
            switch ( found2 )
            {
                case 1: /* C*m + a -> C */
                    convertMult = 1.0;
                    convertAdd = 0.0;
                    break;
#if 0
                case 2: /* C*m + a -> F */
                    convertMult = 9.0/5.0;
                    convertAdd = 32;
                    break;
                case 3: /* C*m + a -> K */
                    convertMult = 1.0;
                    convertAdd = 273.15;
                    break;
#endif
            }
            break;      
        case 2:
            switch ( found2 )
            {
                case 1: /* F*m + a -> C */
                    convertMult = 5.0/9.0;
                    convertAdd = - 32 * 5.0/9.0;
                    break;
#if 0
                case 2: /* F*m + a -> F */
                    convertMult = 1.0;
                    convertAdd = 0.0;
                    break;
                case 3: /* F*m + a -> K */
                    convertMult = 5.0/9.0;
                    convertAdd = 273.15 - 32 * 5.0/9.0;
                    break;
#endif
            }
            break;
        case 3:
            switch ( found2 )
            {
                case 1: /* K*m + a -> C */
                    convertMult = 1.0;
                    convertAdd = -273.15;
                    break;
#if 0
                case 2: /* K*m + a -> F */
                    convertMult = 9.0/5.0;
                    convertAdd = 32 - 273.15 * 9.0/5.0;
                    break;
                case 3: /* K*m + a -> K */
                    convertMult = 1.0;
                    convertAdd = 0.0;
                    break;
#endif
            }
            break;
    }


    /* Mirae has only one chamber controllable from the driver */
    data->chambers = 1;

    /* if we come here, we know how many chambers there are and we
       have the names */

    /* today, all chambers are treated the same, we can not switch
       some of them off. Only the named one are used anyway. In the
       future they may be other configurations that allow to choose
       the activation of each chamber individually */

    /* get setpoint of chamber temperature */
    switch ( getNumValues(conf, logger, data->chambers, PHKEY_TC_SET, values) )
    {
        case 0:
            data->setpoint[0] = values[0]*convertMult + convertAdd;
            phLogFuncMessage(logger, LOG_DEBUG,
                             "temperature value %g of \"%s\" converted to %g %s",
                             values[0], PHKEY_TC_SET, data->setpoint[0], handlerUnit);
            break;
        case 1:
            /* temperature control activated but no values defined */
            phLogFuncMessage(logger, PHLOG_TYPE_ERROR,
                             "\"%s\" set to be \"hot\" or \"cca\" but no setpoints given with \"%s\"",
                             PHKEY_TC_CONTROL,  PHKEY_TC_SET);
            retVal = 0;
            break;
        case 2:
            retVal = 0;
            break;
    }

    /* get upper guard of chamber temperature */
    /* Mirae MR5800 will use this as the band value */
    iValue = -1;
    confError = phConfConfIfDef(conf, PHKEY_TC_UGUARD);
    if ( confError == PHCONF_ERR_OK )
    {
        confError = phConfConfNumber(conf, PHKEY_TC_UGUARD, 
                                     0, NULL, &dValue);
        if ( (confError == PHCONF_ERR_OK) )
        {
            iValue = (int)dValue; /* convert */
        }
        if ((iValue < 1) || (iValue > 10)) // ?? CHECK RANGE - KAW
        {
            // error: invalid value
            phLogFuncMessage(logger, PHLOG_TYPE_WARNING,
                             "Value for temp_upper_guard_band must be between 1 and 10.\n,"
                             "SETBAND command will not be sent.");
        }
        else 
        {
            phLogFuncMessage(logger, LOG_DEBUG,
                             "value %g of \"%s\" converted to SETBAND %d",
                             dValue, PHKEY_TC_UGUARD, iValue);
        }
    }
    data->uguard[0] = iValue;


    /* get lower guard of chamber temperature */
    /* Not used by Mirae MR58000 */

    /* get soak time */
    iValue = -1;
    confError = phConfConfIfDef(conf, PHKEY_TC_SOAK);
    if ( confError == PHCONF_ERR_OK )
    {
        confError = phConfConfNumber(conf, PHKEY_TC_SOAK, 
                                     0, NULL, &dValue);
        if ( (confError == PHCONF_ERR_OK) )
        {
            iValue = (int)dValue; /* convert */
        }
        if ((iValue < 0) || (iValue > 999)) // ?? CHECK RANGE - KAW
        {
            // error: invalid value
            phLogFuncMessage(logger, PHLOG_TYPE_WARNING,
                             "Value for temp_soaktime must be between 0 and 999.\n,"
                             "SETSOAK command will not be sent.");
        }
        else 
        {
            phLogFuncMessage(logger, LOG_DEBUG,
                             "time value %g of \"%s\" converted to %d seconds",
                             dValue, PHKEY_TC_SOAK, iValue);
        }
    }
    data->soaktime[0] = iValue;

    /* get desoak time */
    /* not used on Mirae MR5800 */

    /* get heat flags */
    /* not used on Mirae MR5800 */

    /* get cool flags */
    /* not used on Mirae MR5800 */

    return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
