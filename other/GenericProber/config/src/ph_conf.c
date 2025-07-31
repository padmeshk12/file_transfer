/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_conf.c
 * CREATED  : 29 Jun 1999
 *
 * CONTENTS : Implementation of Configuration Management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Jun 1999, Michael Vogt, created
 *            19 Jul 1999, Michael Vogt, added phConfConfIfDef
 *            20 Jul 1999, Michael Vogt, added configuration key iterator
 *            22 Jul 1999, Michael Vogt, added retrieval of config types
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
#include <stdarg.h>
#include <time.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_conf.h"

#include "semantic.h"
#include "config.h"
#include "attrib.h"
#include "ph_conf_private.h"
#include "editor.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define PHCONF_INDEX_STR_LEN 256

/* don't trust anyone .... */
#define PH_HANDLE_CHECK

#ifdef DEBUG
#define PH_HANDLE_CHECK
#endif

#ifdef PH_HANDLE_CHECK
#define CheckInitialized() \
    if (!myLogger) \
        return PHCONF_ERR_NOT_INIT

#define CheckHandle(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHCONF_ERR_INVALID_HDL
#else
#define CheckInitialized()
#define CheckHandle(x)
#endif

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

/* the only global variable used. Needed because the logger is only
   passed once during init to this module but not during all
   subsequent operations that create a new configuration */
static phLogId_t myLogger = NULL;

/*Begin of Huatek Modifications, Donnie Tu, 04/26/2002*/
/*Issue Number: 334*/
    static char **ptrArray = NULL;
    static int ptrArraySize = 0;
/*End of Huatek Modifications*/

/*--- functions -------------------------------------------------------------*/
static char *writeFieldIndex(int dimension, int *indexField)
{
    static char indexStr[PHCONF_INDEX_STR_LEN];
    int i, j;

    i=0; j=0;
    indexStr[i++] = '[';
    while (j<dimension && i<PHCONF_INDEX_STR_LEN-32)
    {
	i += sprintf(&indexStr[i], "%d", indexField[j]);
	j++;
	if (j<dimension)
	    indexStr[i++] = ',';
    }
    if (i>=PHCONF_INDEX_STR_LEN-32)
    {
	indexStr[i++] = '.';
	indexStr[i++] = '.';
	indexStr[i++] = '.';
    }
    indexStr[i++] = ']';
    indexStr[i] = '\0';

    return &indexStr[0];
}

/*****************************************************************************
 *
 * read a file into memory
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf_private.h
 *
 *****************************************************************************/
static phConfError_t readFile(char **buffer, const char *fileName, int isAttribfile)
{
    FILE *inFile;
    long int length;
    long int readIn;
    int e1;

    /* read the file into a temporary *buffer */
    inFile = fopen(fileName, "r");
    if (!inFile)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR,
	    "could not open %s file \"%s\" for reading", 
	    isAttribfile ? "attributes" : "configuration",
	    fileName);
	return PHCONF_ERR_FILE;	
    }

    /* determine length of the file and rewind to the beginning */
    e1 = fseek(inFile, 0L, SEEK_END);
    length = ftell(inFile);
    rewind(inFile);
    if (e1 != 0 || length <= 0)
    {
	fclose(inFile);
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR,
	    "could not read %s file \"%s\", empty?", 
	    isAttribfile ? "attributes" : "configuration",
	    fileName);
	return PHCONF_ERR_FILE;	
    }

    /* read in the file into internal *buffer and close file */
    *buffer = PhNNew(char, length+1);
    if (! *buffer)
    {
	fclose(inFile);
	phLogConfMessage(myLogger, PHLOG_TYPE_FATAL,
	    "not enough memory to read in %s file",
	    isAttribfile ? "attributes" : "configuration");
	return PHCONF_ERR_MEMORY;
    }
    memset(*buffer, '\0', length+1);

    readIn = fread(*buffer, 1, length, inFile);
    fclose(inFile);

    if (readIn != length)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR,
	    "could not read %s file \"%s\", got %ld bytes instead of %ld",
	    isAttribfile ? "attributes" : "configuration",
            fileName, readIn, length);
	return PHCONF_ERR_FILE;		
    }

    /* add the end of string marker and return with success */
    (*buffer)[readIn] = '\0';	
    return PHCONF_ERR_OK;
}

/*****************************************************************************
 *
 * validate a single configuration entry
 *
 * Authors: Michael Vogt
 *
 * History: 1 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf_private.h
 *
 *****************************************************************************/
int validateDefinition(
    struct keyData *confData,
    struct keyData *attrData)
{
    struct keyStrRng *strChoice;
    int tmpRetVal;
    struct keyData *currentConf;
    int listsize;

    switch (confData->type)
    {
      case PHCONF_KEY_TYPE_NUM_VAL:
	/* compare a given numeric value to its valid range, e.g.
	   42   matches        (0:100 i)
	   42   matches        (0:100 f)
	   42.2 does not match (0:100 i)
	   42.2 matches        (0:100 f)
	   */
	if (attrData->type == PHCONF_KEY_TYPE_NUM_RNG)
	{
	    /* the attribute really expects a numeric value here */
	    if (confData->d.numVal >= attrData->d.numRng.lowBound &&
		confData->d.numVal <= attrData->d.numRng.highBound)
	    {
		if (attrData->d.numRng.precision == PHCONF_NUM_TYPE_INT &&
		    confData->d.numVal != (double) ((int) confData->d.numVal))
		    return 0;
		else
		    return 1;
	    }
	    else
		return 0;
	}
	else
	    /* the attribute expects something else */
	    return 0;
      case PHCONF_KEY_TYPE_STR_VAL:
	/* compare a given string to its valid choices, e.g.
	   "A"     matches "A" | "B" | "C"
	   "abcde" matches "*"
	   */
	if (attrData->type == PHCONF_KEY_TYPE_STR_RNG)
	{
	    /* the attribute really expects a string here, check all
               entries of the string choice list, the special string
               "*" means don't care */
	    strChoice = attrData->d.strRng;
	    while (strChoice)
	    {
		if (strcmp(strChoice->entry, confData->d.strVal) == 0 ||
		    strcmp(strChoice->entry, "*") == 0)
		    return 1;
		strChoice = strChoice->next;
	    }
	    /* string was not matched */
	    return 0;
	}
	else
	    /* the attribute expects something else */
	    return 0;
      case PHCONF_KEY_TYPE_LIST:
	/* compare a list. 
	   comparing a list means, that each element of the list must match the
	   single entry prototype of the attributes list, e.g.
	   [ 1 2 3 ]               matches [ (0:10 f) ]
	   [ [ "X" ] [ "Y" "Z" ] ] matches [ [ "A" | "B" | ... | "Z" ] ]
	   [ 1 2 3 "X" ]           can not be matched at all with the current 
	                           concept of attributes definition.
	   */
	if (attrData->type == PHCONF_KEY_TYPE_LIST)
	{
	    /* the attribute really expects a list here. Validate each
               entry of the configuration definition against the
               single entry of the attribute list */
	    tmpRetVal = 1;
	    currentConf = confData->d.list.head;
	    listsize = 0;
	    while (tmpRetVal && currentConf)
	    {
		tmpRetVal *= validateDefinition(currentConf, attrData->d.list.head);
		currentConf = currentConf->next;
		listsize++;
	    }
	    if (attrData->d.list.size)
	    {
		if (listsize < attrData->d.list.size->minCount)
		    tmpRetVal = 0;
		if (attrData->d.list.size->maxCount >= 0 &&
		    listsize > attrData->d.list.size->maxCount)
		    tmpRetVal = 0;
	    }
	    return tmpRetVal;
	}
	else
	    /* the attribute expects something else */
	    return 0;
      case PHCONF_KEY_TYPE_NUM_RNG:
      case PHCONF_KEY_TYPE_STR_RNG:
      default:
	/* does not exist for configuration entries */
	return 0;
    }
}

/*****************************************************************************
 *
 * access a list entry
 *
 * Authors: Michael Vogt
 *
 * History: 1 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf_private.h
 *
 *****************************************************************************/
static struct keyData *walkThroughLists(
    struct keyData *data, 
    int dim, 
    int *field)
{
    int count;

    if (dim < 0)
	return NULL;

    while (dim > 0 && data)
    {
	if (*field < 0)
	    return NULL;

	if (data->type != PHCONF_KEY_TYPE_LIST)
	    return NULL;

	data = data->d.list.head;
	count = *field;
	while (data && count>0)
	{
	    data = data->next;
	    count--;
	}
	dim--;
	field++;
    }
    
    return data;
}

/*****************************************************************************
 *
 * Initialize the configuration manager
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfInit(
    phLogId_t logger         /* the data logger to be used */
)
{
    if (logger)
    {
	/* if we got a valid logger, use it and trace the first call
           to this module */
	myLogger = logger;
	phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	    "phConfInit(P%p)", logger);
	return PHCONF_ERR_OK;
    }
    else
	/* passed logger is not valid at all, give up */
	return PHCONF_ERR_NOT_INIT;
}

/*****************************************************************************
 *
 * Read configuration from file
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfReadConfFile(
    phConfId_t *configID     /* the resulting configuration ID */,
    const char *fileName     /* path to configuration file */
)
{
    struct phConfConfStruct *tmpId = NULL;
    phConfError_t retVal = PHCONF_ERR_OK;
    char *buffer = NULL;
    
    if(*configID != NULL)
    {
      phConfDestroyConf(*configID);
      free(*configID);
      *configID = NULL;
    }

    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfReadConfFile(P%p, \"%s\")",
	configID, fileName ? fileName : "(NULL)");

    tmpId = PhNew(struct phConfConfStruct);
    if (tmpId == NULL)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_FATAL,
	    "not enough memory during call to phConfReadConfFile()");
	return PHCONF_ERR_MEMORY;
    }

    /* initialize the new handle */
    tmpId->myself = tmpId;
    tmpId->current = NULL;

    if(fileName == NULL)
    {
        free(tmpId);
        *configID = PhNew(struct phConfConfStruct);
        return PHCONF_ERR_FILE;
    }

    /* read the file into a temporary buffer */
    retVal = readFile(&buffer, fileName, 0);
    if (retVal != PHCONF_ERR_OK)
    {
	free(tmpId);
        if(buffer != NULL) free(buffer);
        *configID = PhNew(struct phConfConfStruct);
        return retVal;
    }

    /* do the main work, read in the configuration */
    phLogConfMessage(myLogger, LOG_NORMAL,
	"reading configuration file \"%s\"", fileName);
    tmpId->conf = phConfConfParse(buffer, myLogger);

    /* clean up */
    free(buffer);
    buffer = NULL;

    if (!tmpId->conf)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "could not parse the given configuration");
        free(tmpId);
	retVal = PHCONF_ERR_PARSE;	
        *configID = PhNew(struct phConfConfStruct);
    }
    else
    {
	*configID = tmpId;
    }

    return retVal;
}

/*****************************************************************************
 *
 * Get first definition key (Iterator)
 *
 * Authors: Michael Vogt
 *
 * Description:
 * Please refer to ph_conf.h
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfConfFirstKey(
    phConfId_t configID      /* the configuration ID */,
    const char **key         /* the key of the first definition entry */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfConfFirstKey(P%p, P%p)",	configID, key);
    CheckHandle(configID);
    
    if (!key)
	return PHCONF_ERR_UNDEF;

    configID->current = configID->conf->first;
    if (configID->current)
    {
	*key = configID->current->key;
	return PHCONF_ERR_OK;
    }
    else
    {
	*key = NULL;
	return PHCONF_ERR_UNDEF;    
    }
}

/*****************************************************************************
 *
 * Get next definition key (Iterator)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Retrieves the name of the key for the next configuration
 * definition. The end of the entry list is reached if *key is set to
 * NULL.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfConfNextKey(
    phConfId_t configID      /* the configuration ID */,
    const char **key         /* the key of the next definition entry */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfConfNextKey(P%p, P%p)", configID, key);
    CheckHandle(configID);
    
    if (!key)
	return PHCONF_ERR_UNDEF;

    *key = NULL;
    if (configID->current)
    {
	configID->current = configID->current->next;
	if (configID->current)
	    *key = configID->current->key;
    }

    return *key ? PHCONF_ERR_OK : PHCONF_ERR_UNDEF;    
}

/*****************************************************************************
 *
 * Write configuration to file
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfWriteConfFile(
    phConfId_t configID      /* the configuration ID */,
    const char *fileName     /* path to configuration file */,
    phConfFlag_t selection   /* selection of definitions, which should
				be written to file */
)
{
    phConfError_t retVal = PHCONF_ERR_OK;
    struct keyedDefinition *entry = NULL;
    FILE *outFile;
    time_t timeNow;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfWriteConfFile(P%p, \"%s\", 0x%08lx)",
	configID, fileName ? fileName : "(NULL)", 
	(long) selection);
    CheckHandle(configID);

    if(fileName == NULL)
    {
        phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, "File name is null");
        return PHCONF_ERR_FILE;
    }
    
    /* open output file for writing */
    outFile = fopen(fileName, "w");
    if (!outFile)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR,
	    "can't open file \"%s\" for writing", fileName);
	return PHCONF_ERR_FILE;
    }

    phLogConfMessage(myLogger, LOG_NORMAL,
	"writing configuration to file \"%s\"", fileName);

    /* write a header */
    fprintf(outFile, "# Current configuration settings of material handler driver\n");
    timeNow = time(NULL);
    fprintf(outFile, "# retrieved at: %s", ctime(&timeNow));
    fprintf(outFile, "# \n");
    fprintf(outFile, "# Definition types not included: ");
    if (! (selection & PHCONF_FLAG_FIXED))
	fprintf(outFile, "fixed-definitions ");
    /* if ( ... ) */
    if (selection == PHCONF_FLAG_DONTCARE)
	fprintf(outFile, "(no filters active)");
    fprintf(outFile, "\n");
    fprintf(outFile, "# \n");
    fprintf(outFile, "# Please refer to the configuration hints given in '{ }' sections below\n");
    fprintf(outFile, "# \n");
    fprintf(outFile, "# Change definitions with care !\n");
    fprintf(outFile, "\n");

    /* walk through all definitions and write texts to file */
    entry = configID->conf->first;
    while (entry)
    {
	/* binary bit operation on all flags:
	   print only, if the following is true for each flag position:
	   flag is not set or flag is set together with selection */
	if (((~entry->flagMask & PHCONF_FLAG_DONTCARE) 
	    | (entry->flagMask & selection)) == PHCONF_FLAG_DONTCARE)
	{
	    if (entry->hint)
		fprintf(outFile, "{ %s }\n", entry->hint);
	    fprintf(outFile, "%s\n\n", phConfSemWriteDef(entry));
	}

	entry = entry->next;
    }

    /* write a footer */
    fprintf(outFile, "# end of configuration settings\n");
    
    /* clean up */
    fclose(outFile);

    return retVal;
}

/*****************************************************************************
 *
 * Write definition to string
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfGetConfDefinition(
    phConfId_t configID      /* the configuration ID */,
    const char *key          /* the key to write the definition for */,
    char **result            /* pointer to resulting definition string */
)
{
    phConfError_t retVal = PHCONF_ERR_OK;
    struct keyedDefinition *entry = NULL;
    static char empty[1];

    empty[0] = '\0';
    *result = empty;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfGetConfDefinition(P%p, \"%s\", P%p)",
	configID, key ? key : "(NULL)", result);
    CheckHandle(configID);

    /* search the configuration for the given key */
    entry = phConfSemSearchDefinition(configID->conf, key);
    if (!entry)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR,
	    "configuration for key \"%s\" does not exist", key);
	return PHCONF_ERR_UNDEF;
    }

    /* retrieve the textual definition and return */
    *result = phConfSemWriteDef(entry);
    return retVal;
}

/*****************************************************************************
 *
 * Write definition to string (value only)
 *
 * Description:
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfGetConfDefinitionValue(
    phConfId_t configID      /* the configuration ID */,
    const char *key          /* the key to write the definition for */,
    char **result            /* pointer to resulting definition string */
)
{
    phConfError_t retVal = PHCONF_ERR_OK;
    struct keyedDefinition *entry = NULL;
    static char defValue[10240];

    defValue[0] = '\0';
    *result = defValue;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE,
        "phConfGetConfDefinitionValue(P%p, \"%s\", P%p)",
        configID, key ? key : "(NULL)", result);
    CheckHandle(configID);

    /* search the configuration for the given key */
    entry = phConfSemSearchDefinition(configID->conf, key);
#if 0
    if (!entry)
    {
        phLogConfMessage(myLogger, PHLOG_TYPE_ERROR,
            "configuration for key \"%s\" does not exist", key);
        return PHCONF_ERR_UNDEF;
    }
#endif
    if(entry)
    {
      /* retrieve the textual definition and return */
      phConfSemCatDataText(defValue, entry->data, 40);
    }

    return retVal;
}

/*****************************************************************************
 *
 * Read configuration from string
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfSetConfDefinition(
    phConfId_t *configID     /* the resulting configuration ID */,
    char *confString         /* string with configuration data */
)
{
    struct phConfConfStruct *tmpId;
    phConfError_t retVal = PHCONF_ERR_OK;
    
    if(*configID != NULL)
    {
      phConfDestroyConf(*configID);
      free(*configID);
      *configID = NULL;
    }

    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfSetConfDefinition(P%p, \"%s\")",
	configID, confString ? confString : "(NULL)");

    tmpId = PhNew(struct phConfConfStruct);
    if (tmpId == NULL)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_FATAL,
	    "not enough memory during call to phConfSetConfDefinition()");
	return PHCONF_ERR_MEMORY;
    }

    /* initialize the new handle */
    tmpId->myself = tmpId;
    tmpId->current = NULL;

    /* do the main work, read in the configuration */
    tmpId->conf = phConfConfParse(confString, myLogger);

    if (!tmpId->conf)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "could not parse the given configuration");
	retVal = PHCONF_ERR_PARSE;	
        free(tmpId);
        *configID = PhNew(struct phConfConfStruct);
    }
    else
    {
	*configID = tmpId;
    }

    return retVal;
}

/*****************************************************************************
 *
 * Read attributes from file
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfReadAttrFile(
    phAttrId_t *attributesID /* the resulting attributes ID */,
    const char *fileName     /* path to configuration file */
)
{
    struct phConfAttrStruct *tmpId = NULL;
    phConfError_t retVal = PHCONF_ERR_OK;
    char *buffer = NULL;
    
    if(*attributesID != NULL)
    {
      phConfDestroyAttr(*attributesID);
      free(*attributesID);
      *attributesID = NULL;
    }

    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfReadAttrFile(P%p, \"%s\")",
	attributesID, fileName ? fileName : "(NULL)");

    tmpId = PhNew(struct phConfAttrStruct);
    if (tmpId == NULL)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_FATAL,
	    "not enough memory during call to phConfReadAttrFile()");
	return PHCONF_ERR_MEMORY;
    }

    /* initialize the new handle */
    tmpId->myself = tmpId;
    tmpId->current = NULL;

    if(fileName == NULL)
    {
        free(tmpId);
	*attributesID = PhNew(struct phConfAttrStruct);
        return PHCONF_ERR_FILE;
    }

    /* read the file into a temporary buffer */
    retVal = readFile(&buffer, fileName, 1);
    if (retVal != PHCONF_ERR_OK)
    {
	free(tmpId);
        if (buffer != NULL) free(buffer);
	*attributesID = PhNew(struct phConfAttrStruct);
        return retVal;
    }

    /* do the main work, read in the configuration */
    phLogConfMessage(myLogger, LOG_NORMAL,
	"reading attributes file \"%s\"", fileName);
    tmpId->attr = phConfAttrParse(buffer, myLogger);

    /* clean up */
    free(buffer);
    buffer = NULL;

    if (!tmpId->attr)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "could not parse the given attributes");
        free(tmpId);
	retVal = PHCONF_ERR_PARSE;	
	*attributesID = PhNew(struct phConfAttrStruct);
    }
    else
    {
	*attributesID = tmpId;
    }

    return retVal;
}

/*****************************************************************************
 *
 * Validate a configuration
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfValidate(
    phConfId_t configID      /* configuration to validate */,
    phAttrId_t attributesID  /* attributes to validate against */
)
{
    struct keyedDefinition *confEntry;
    struct keyedDefinition *attrEntry;
    phConfError_t retVal = PHCONF_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, "phConfValidate(P%p, P%p)",
	configID, attributesID);
    CheckHandle(configID);
    CheckHandle(attributesID);

    confEntry = configID->conf->first;
    while (confEntry)
    {
	attrEntry = phConfSemSearchDefinition(attributesID->attr,
	    confEntry->key);
	if (!attrEntry)
	{
	    phLogConfMessage(myLogger, PHLOG_TYPE_WARNING,
	        "Customized config param %s found in the config file (not defined in the attribute file).",
		confEntry->key);
	    retVal = PHCONF_ERR_OK;
	}
	else
	{
	    if (!validateDefinition(confEntry->data, attrEntry->data))
	    {
		char *tmpStr;
		
		tmpStr = strdup(phConfSemWriteDef(confEntry));
		phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
		    "The following configuration is not valid,\n"
		    "since the given value is out of the valid range\n" 
		    "Given Configuration:\n"
		    "%s\n"
		    "Valid range:\n"
		    "%s",
		    tmpStr, 
		    phConfSemWriteDef(attrEntry));
		free(tmpStr);

        /* directly return error to avoid next entry overwirte the value */
		return PHCONF_ERR_VALIDATION;
	    }
	}

	confEntry = confEntry->next;
    }

    /* return the validation result */
    return retVal;
}

/*****************************************************************************
 *
 * Merge two configurations into one
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfMerge(
    phConfId_t targetConfID  /* the configuration to merge into */,
    phConfId_t sourceConfID  /* the configuration to be merged */, 
    phConfFlag_t selection   /* selection of definitions to merge */
)
{
    struct keyedDefinition *sourceEntry;
    struct keyedDefinition *nextSourceEntry;
    struct keyedDefinition *targetEntry;
    phConfError_t retVal = PHCONF_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfMerge(P%p, P%p, 0x%08lx)", targetConfID, sourceConfID, 
	(long) selection);
    CheckHandle(targetConfID);
    CheckHandle(sourceConfID);

    sourceEntry = sourceConfID->conf->first;
    while (sourceEntry)
    {
	/* cut out the current source entry of the source configuration,
	   be destructive to the source tree, it's no longer used.... */
	nextSourceEntry = sourceEntry->next;
	sourceEntry->left = NULL;
	sourceEntry->right = NULL;
	sourceEntry->next = NULL;
	
	/* look, whether there is already a target definition with this key */
	targetEntry = phConfSemSearchDefinition(targetConfID->conf,
	    sourceEntry->key);
	
	if (targetEntry)
	{
	    /* target already exists, check flags before merging,
               print error message, if not merged. 
	       binary bit operation on all flags:
	       print only, if the following is true for each flag position:
	       flag is not set or flag is set together with selection */
	    if (((~targetEntry->flagMask & PHCONF_FLAG_DONTCARE) 
		| (targetEntry->flagMask & selection)) == PHCONF_FLAG_DONTCARE)
	    {
		targetConfID->conf = phConfSemAddDefinition(
		    targetConfID->conf, sourceEntry);
	    }
	    else
	    {
		char *tmpStr;
		
		tmpStr = strdup(phConfSemWriteDef(sourceEntry));
		phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
		    "A configuration entry was not merged\n"
		    "due to the flag setting of the target definition\n"
		    "Source definition:\n"
		    "%s\n"
		    "Target definition:\n"
		    "%s", 
		    tmpStr,
		    phConfSemWriteDef(targetEntry));
		free(tmpStr);
/* Begin of Huatek Modification, Only Fang, 04/20/2002 */
/* Issue Number: 334 */
                free(sourceEntry->data);
                free(sourceEntry);
/* End of Huatek Modification */

		retVal = PHCONF_ERR_MERGE;
	    }
	}
	else
	    /* target does not exist so far */
	    targetConfID->conf = phConfSemAddDefinition(
		targetConfID->conf, sourceEntry);
	
	/* go on with next source entry */
	sourceEntry = nextSourceEntry;
    }

    /* make the source configuration invalid */
    sourceConfID->conf->tree = NULL;
    sourceConfID->conf->first = NULL;
    sourceConfID->conf->last = NULL;
    phConfSemDeleteDefinition(sourceConfID->conf);
    sourceConfID->conf = NULL;
    sourceConfID->myself = NULL;

    /* return with correct error value */
    return retVal;
}

/*****************************************************************************
 *
 * Get attribute's type and nesting count
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfAttrType(
    phAttrId_t attributesID  /* attributes ID */,
    char *key                /* the key of the definition to look for */,
    phConfType_t *theType    /* the type is returned here */,
    int *nesting             /* the deepest nesting for this attribute
				definition */
)
{
    struct keyedDefinition *entry;
    struct keyData *data;
    int count;
    phConfError_t retVal = PHCONF_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfAttrType(P%p, \"%s\", P%p, P%p)", 
	attributesID, key ? key : "(NULL)", theType, nesting);
    CheckHandle(attributesID);

    /* look for the definition with the given key */
    entry = phConfSemSearchDefinition(attributesID->attr, key);
    if (!entry)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "An attribute definition for the key \"%s\" does not exist",
	    key);
	return PHCONF_ERR_UNDEF;
    }
    
    /* assume a nested list, count through all nesting levels */
    count = 0;
    data = entry->data;
    while (data->type == PHCONF_KEY_TYPE_LIST)
    {
	data = data->d.list.head;
	count++;
    }

    /* the innermost entry was found, determine type */
    switch (data->type)
    {
      case PHCONF_KEY_TYPE_NUM_RNG:
	/* a number, now look for the precission */
	if (data->d.numRng.precision == PHCONF_NUM_TYPE_INT)
	    *theType = PHCONF_TYPE_INUM_RNG;
	else
	    *theType = PHCONF_TYPE_FNUM_RNG;
	*nesting = count;
	break;
      case PHCONF_KEY_TYPE_STR_RNG:
	/* a string choice */
	*theType = PHCONF_TYPE_STR_CHC;
	*nesting = count;
	break;
      case PHCONF_KEY_TYPE_NUM_VAL:
      case PHCONF_KEY_TYPE_STR_VAL:
      case PHCONF_KEY_TYPE_LIST:
	/* this is definately a bug */
	retVal = PHCONF_ERR_INTERNAL;
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "An attributes entry is of unknown type:\n%s",
	    phConfSemWriteDef(entry));
	break;
    }

    /* return with the correct value */
    return retVal;
}

/*****************************************************************************
 *
 * Get an attribute's number range 
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfAttrNumRange(
    phAttrId_t attributesID  /* attributes ID */,
    char *key                /* the key of the definition to look for */,
    double *lowerBound       /* the resulting lower bound of the
				number definition */,
    double *upperBound       /* the resulting upper bound of the
				number definition */
)
{
    struct keyedDefinition *entry;
    struct keyData *data;
    phConfError_t retVal = PHCONF_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfAttrNumRange(P%p, \"%s\", P%p, P%p)", 
	attributesID, key ? key : "(NULL)", lowerBound, upperBound);
    CheckHandle(attributesID);

    /* look for the definition with the given key */
    entry = phConfSemSearchDefinition(attributesID->attr, key);
    if (!entry)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "An attribute definition for the key \"%s\" does not exist",
	    key);
	return PHCONF_ERR_UNDEF;
    }
    
    /* assume a nested list, count through all nesting levels */
    data = entry->data;
    while (data->type == PHCONF_KEY_TYPE_LIST)
	data = data->d.list.head;

    /* the innermost entry was found, retrieve the lower and upper bound */
    if (data->type == PHCONF_KEY_TYPE_NUM_RNG)
    {
	*lowerBound = data->d.numRng.lowBound;
	*upperBound = data->d.numRng.highBound;
    }
    else
    {
	/* this is a type mismatch, this entry is not a number range... */
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "The attribute entry does not define a number range:\n%s",
	    phConfSemWriteDef(entry));
	retVal = PHCONF_ERR_TYPE;
    }

    /* return with the correct value */
    return retVal;
}

/*****************************************************************************
 *
 * Get an attribute's string choice
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfAttrStrChoice(
    phAttrId_t attributesID  /* attributes ID */,
    char *key                /* the key of the definition to look for */,
    char ***choices          /* resulting pointer to array of string
				pointers, NULL terminated list */
)
{
/*Begin of Huatek Modifications, Donnie Tu, 04/26/2002*/
/*Issue Number: 334*/

/*    static char **ptrArray = NULL;*/
    static char *empty[1];
/*    int ptrArraySize = 0;*/
/*End of Huatek Modifications*/
    int count;

    struct keyedDefinition *entry;
    struct keyData *data;
    struct keyStrRng *strEntry;
    phConfError_t retVal = PHCONF_ERR_OK;

    empty[0] = NULL;
    *choices = empty;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfAttrStrChoice(P%p, \"%s\", P%p)", 
	attributesID, key ? key : "(NULL)", choices);
    CheckHandle(attributesID);

    /* look for the definition with the given key */
    entry = phConfSemSearchDefinition(attributesID->attr, key);
    if (!entry)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "An attribute definition for the key \"%s\" does not exist",
	    key);
	return PHCONF_ERR_UNDEF;
    }
    
    /* assume a nested list, count through all nesting levels */
    data = entry->data;
    while (data->type == PHCONF_KEY_TYPE_LIST)
	data = data->d.list.head;

    /* the innermost entry was found, retrieve the string choice */
    if (data->type == PHCONF_KEY_TYPE_STR_RNG)
    {
	/* first count existing entries and consider one spare entry
           for the final NULL entry */
	strEntry = data->d.strRng;
	count = 1;
	while (strEntry)
	{
	    strEntry = strEntry->next;
	    count++;
	}

	/* allocate a minimum size of 64 entries to hold the string pointers */
	if (count < 64)
	    count = 64;
	if (ptrArraySize < count)
	{
	    if (ptrArray)
		free(ptrArray);

	    ptrArray = PhNNew(char *, count);
	    ptrArraySize = count;
	}

	/* copy the string pointers of the string choice into the
           pointer array */
	strEntry = data->d.strRng;
	count = 0;
	while (strEntry)
	{
	    ptrArray[count] = strEntry->entry;
	    strEntry = strEntry->next;
	    count++;
	}
	
	/* mark the end of the list */
	ptrArray[count] = NULL;

	/* return the result */
	*choices = ptrArray;
    }
    else
    {
	/* this is a type mismatch, this entry is not a string choice... */
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "The attribute entry does not define a string choice:\n%s",
	    phConfSemWriteDef(entry));
	retVal = PHCONF_ERR_TYPE;
    }

    /* return with the correct value */
    return retVal;
}

/*****************************************************************************
 *
 * Check availability of a specific key
 *
 * Authors: Michael Vogt
 *
 * History: 19 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfConfIfDef(
    phConfId_t configID      /* configuration ID */,
    const char *key          /* the key of the definition to look for */
)
{
    struct keyedDefinition *entry;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfConfIfDef(P%p, \"%s\")", 
	configID, key ? key : "(NULL)");
    CheckHandle(configID);

    /* look for the definition with the given key */
    entry = phConfSemSearchDefinition(configID->conf, key);

    /* return the success.... */
    return entry ? PHCONF_ERR_OK : PHCONF_ERR_UNDEF;
}

/*****************************************************************************
 *
 * Get a configuration's number entry
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfConfNumber(
    phConfId_t configID      /* configuration ID */,
    const char *key          /* the key of the definition to look for */,
    int dimension            /* the requested nesting and the array
				size of <indexField> */,
    int *indexField          /* index array to access nested list entries */,
    double *value            /* the resulting numeric value */
)
{
    struct keyedDefinition *entry;
    struct keyData *data;
    phConfError_t retVal = PHCONF_ERR_OK;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfConfNumber(P%p, \"%s\", %d, %s, P%p)", 
	configID, key ? key : "(NULL)", dimension, 
	writeFieldIndex(dimension, indexField), value);
    CheckHandle(configID);

    /* look for the definition with the given key */
    entry = phConfSemSearchDefinition(configID->conf, key);
    if (!entry)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "A configuration definition for the key \"%s\" does not exist",
	    key);
	return PHCONF_ERR_UNDEF;
    }
    
    /* assume a nested list, find the correct entry according to the index */
    data = walkThroughLists(entry->data, dimension, indexField);
    if (!data)
    {
	/* the list index does not fit */
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "The list index does not fit the configuration definition\n%s",
	    phConfSemWriteDef(entry));
	return PHCONF_ERR_INDEX;
    }

    /* the entry was found, retrieve the numeric value */
    if (data->type == PHCONF_KEY_TYPE_NUM_VAL)
	*value = data->d.numVal;
    else
    {
	/* this is a type mismatch, this entry is not a number ... */
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "The configuration entry does not define a number value:\n%s",
	    phConfSemWriteDef(entry));
	retVal = PHCONF_ERR_TYPE;
    }

    /* return with the correct value */
    return retVal;
}

/*****************************************************************************
 *
 * Get a configuration's string entry
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfConfString(
    phConfId_t configID      /* configuration ID */,
    const char *key          /* the key of the definition to look for */,
    int dimension            /* the requested nesting and the array
				size of <indexField> */,
    int *indexField          /* index array to access nested list entries */,
    const char **value       /* pointer to resulting string value */
)
{
    struct keyedDefinition *entry;
    struct keyData *data;
    phConfError_t retVal = PHCONF_ERR_OK;
    static char empty[1];

    empty[0] = '\0';
    *value = empty;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfConfString(P%p, \"%s\", %d, %s, P%p)", 
	configID, key ? key : "(NULL)", dimension, 
	writeFieldIndex(dimension, indexField), value);
    CheckHandle(configID);

    /* look for the definition with the given key */
    entry = phConfSemSearchDefinition(configID->conf, key);
    if (!entry)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "A configuration definition for the key \"%s\" does not exist",
	    key);
	return PHCONF_ERR_UNDEF;
    }
    
    /* assume a nested list, find the correct entry according to the index */
    data = walkThroughLists(entry->data, dimension, indexField);
    if (!data)
    {
	/* the list index does not fit */
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "The list index does not fit the configuration definition\n%s",
	    phConfSemWriteDef(entry));
	return PHCONF_ERR_INDEX;
    }

    /* the entry was found, retrieve the numeric value */
    if (data->type == PHCONF_KEY_TYPE_STR_VAL)
	*value = data->d.strVal;
    else
    {
	/* this is a type mismatch, this entry is not a string ... */
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "The configuration entry does not define a string entry:\n%s",
	    phConfSemWriteDef(entry));
	retVal = PHCONF_ERR_TYPE;
    }

    /* return with the correct value */
    return retVal;
}

/*****************************************************************************
 *
 * Get a configuration's type
 *
 * Authors: Michael Vogt
 *
 * History: 22 Jul 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfConfType(
    phConfId_t configID      /* configuration ID */,
    const char *key          /* the key of the definition to look for */,
    int dimension            /* the requested nesting and the array
				size of <indexField> */,
    int *indexField          /* index array to access nested list entries */,
    phConfType_t *type       /* the type of the addressed element */,
    int *size                /* if type is a PHCONF_TYPE_LIST, the
				size of the list is returned */
)
{
    struct keyedDefinition *entry;
    struct keyData *data;
    phConfError_t retVal = PHCONF_ERR_OK;
    int count;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfConfType(P%p, \"%s\", %d, %s, P%p, P%p)", 
	configID, key ? key : "(NULL)", dimension, 
	writeFieldIndex(dimension, indexField), type, size);
    CheckHandle(configID);

    /* look for the definition with the given key */
    entry = phConfSemSearchDefinition(configID->conf, key);
    if (!entry)
    {
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "A configuration definition for the key \"%s\" does not exist",
	    key);
	return PHCONF_ERR_UNDEF;
    }
    
    /* assume a nested list, find the correct entry according to the index */
    data = walkThroughLists(entry->data, dimension, indexField);
    if (!data)
    {
	/* the list index does not fit */
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "The list index does not fit the configuration definition\n%s",
	    phConfSemWriteDef(entry));
	return PHCONF_ERR_INDEX;
    }

    /* the entry was found, check the type */
    switch (data->type)
    {
      case PHCONF_KEY_TYPE_NUM_VAL:
	*type = PHCONF_TYPE_NUM;
	break;
      case PHCONF_KEY_TYPE_STR_VAL:
	*type = PHCONF_TYPE_STR;
	break;
      case PHCONF_KEY_TYPE_LIST:
	*type = PHCONF_TYPE_LIST;
	count = 0;
	data = data->d.list.head;
	while (data)
	{
	    count++;
	    data = data->next;
	}
	*size = count;
	break;
      default:
	retVal = PHCONF_ERR_INTERNAL;
	break;
    }

    return retVal;
}

/*****************************************************************************
 *
 * Test a string configuration value
 *
 * Authors: Michael Vogt
 *
 * History: 13 Jan 2000, Michael Vogt, created
 *
 * Description:
 * Please refer to ph_conf.h
 *
 *****************************************************************************/

phConfError_t phConfConfStrTest(
    int *found               /* the position of the string within the
                                variable argument list below that is
                                equal to the configured value. If a
                                string was matched, a number > 0 is
                                returned. If a string could not be
                                matched, 0 is returned here and the
                                error code is set accordingly */,
    phConfId_t configID      /* configuration ID */,
    const char *key          /* the key of the definition to look for */,
    ...                      /* an arbitrary number of char pointers
                                ending with NULL */
)
{
    va_list ap;
    struct keyedDefinition *entry;
    struct keyData *data;
    phConfError_t retVal = PHCONF_ERR_OK;
    char *current;

    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, 
	"phConfConfStrTest(P%p, P%p, \"%s\", ...)", found, 
	configID, key ? key : "(NULL)");
    CheckHandle(configID);

    /* look for the definition with the given key */
    entry = phConfSemSearchDefinition(configID->conf, key);
    if (!entry)
    {
	/* does not exist, all done */
	*found = 0;
	return PHCONF_ERR_UNDEF;
    }
    
    /* assume no list! */
    data = entry->data;

    /* the entry was found, retrieve the string value */
    if (data->type == PHCONF_KEY_TYPE_STR_VAL)
    {
	va_start(ap, key);
	current = va_arg(ap, char *);
	*found = 1;
	while (current && strcasecmp(current, data->d.strVal) != 0)
	{
	    (*found)++;
	    current = va_arg(ap, char *);
	}
	va_end(ap);
    }
    else
    {
	/* this is a type mismatch, this entry is not a string ... */
	phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
	    "The configuration entry does not define a string entry:\n%s",
	    phConfSemWriteDef(entry));
	*found = 0;
	retVal = PHCONF_ERR_TYPE;
    }

    /* return with the correct value */
    return retVal;
}

/*****************************************************************************
 *
 * Destroy the passed configuration.
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfDestroyConf(
    phConfId_t configID      /* configuration ID */
)
{
    /* do checks and trace the call first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, "phConfDestroyConf(P%p)", 
	configID);
    CheckHandle(configID);

    /* destroy the data */
    phConfSemDeleteDefinition(configID->conf);
    configID->conf = NULL;
    configID->myself = NULL;

    return PHCONF_ERR_OK;
}

/*****************************************************************************
 *
 * Destroy the passed attributes.
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to ph_conf.h
 *
 *****************************************************************************/
phConfError_t phConfDestroyAttr(
    phAttrId_t attributesID  /* attributes ID */
)
{
    /* do checks and trace the cDonnie Tuall first */
    CheckInitialized();
    phLogConfMessage(myLogger, PHLOG_TYPE_TRACE, "phConfDestroyAttr(P%p)", 
	attributesID);
    CheckHandle(attributesID);

    /* destroy the data */
    phConfSemDeleteDefinition(attributesID->attr);
    attributesID->attr = NULL;
    attributesID->myself = NULL;

    return PHCONF_ERR_OK;
}

/*****************************************************************************
 *
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description:
 *
 ***************************************************************************/
phConfError_t phConfEditConfDef(
    phConfId_t configId,
    phAttrId_t attributesID, /* attributes to validate against */
    phConfFlag_t selection,  /* selection of definitions, which should
				be shown in gui */
    phPanelFlag_t panelflags,
    int *response            /* 1 if ok is pressed, 0 if cancel */
)
{
    CheckInitialized();

    return createAndHandleEditor(
	myLogger, configId, attributesID, selection, panelflags, response);
}

/*Begin of Huatek Modifications, Donnie Tu, 05/5/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 *
 * Authors: Donnie Tu
 *
 * History: 05.05.2002, Donnie Tu, created
 *
 * Description:
 *
 ***************************************************************************/
void phConfFreeStatic(void)
{
   if(ptrArray!=NULL)
   {
	free(ptrArray);
	ptrArray=NULL;
	ptrArraySize=0;
   }
}
/*End of Huatek Modifications*/


/*****************************************************************************
 * End of file
 *****************************************************************************/
