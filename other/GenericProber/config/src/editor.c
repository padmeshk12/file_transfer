/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : editor.c
 * CREATED  : 14 Jan 2000
 *
 * CONTENTS : Configuration Editor
 *
 * AUTHORS  : Ulrich Frank, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jan 2000, Ulrich Frank, created
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
#include <errno.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_conf.h"
#include "ph_GuiServer.h"

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

#define CheckHandle(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHCONF_ERR_INVALID_HDL

#define ERRORGUI                                                               \
                 "S:`errorgui`:`Error occured`{@v\n"                           \
                     "\tL::`Validation of configuration failed "               \
                         "at key: \n"                                          \
                         "%s\n\n"                                              \
                         "Do you want to...\n"                                 \
                         "REEDIT your configuration changes,\n"                \
                         "RESTART editing the configuration, or\n"             \
                         "GIVE UP changing the configuration ?`\n"             \
                     "\tS::{@h\n"                                              \
                         "\t\tp:`reedit`:`Reedit`:e\n"                         \
                         "\t\t*\n"                                             \
                         "\t\tp:`restart`:`Restart`:e\n"                       \
                         "\t\t*\n"                                             \
                         "\t\tp:`giveup`:`Give Up`:e\n"                        \
                     "\t}\n"                                                   \
                 "}\n"

#define CONFIRMGUI                                                             \
                 "S:`confirmgui`:`Comfirmation Dialog`{@v\n"                   \
                     "\tL::`Do you really want to delete key: %s?`\n"          \
                     "\tS::{@h\n"                                              \
                         "\t\tp:`ok`:`OK`:e\n"                                 \
                         "\t\t*\n"                                             \
                         "\t\tp:`cancel`:`Cancel`:e\n"                         \
                     "\t}\n"                                                   \
                 "}\n"

#define WARNGUI1                                                               \
                 "S:`warngui1`:`Warning`{@v\n"                                 \
                     "\tL::`Key %s already exists in current configuration!`\n"\
                     "\tS::{@h\n"                                                \
                         "\t\t*p:`ok`:`OK`:e*\n"                                 \
                     "\t}\n"                                                     \
                 "}\n"

#define WARNGUI2                                                               \
                 "S:`warngui2`:`Warning`{@v\n"                                 \
                     "\tL::`Key %s doesn't exists in attributes definition!`\n"\
                     "\tS::{@h\n"                                                \
                         "\t\t*p:`ok`:`OK`:e*\n"                                 \
                     "\t}\n"                                                     \
                 "}\n"

#define WARNGUI3                                                               \
                 "S:`warngui3`:`Warning`{@v\n"                                 \
                     "\tL::`Validation of new configuration failed!\n%s`\n"    \
                     "\tS::{@h\n"                                                \
                         "\t\t*p:`ok`:`OK`:e*\n"                                 \
                     "\t}\n"                                                     \
                 "}\n"

#define WARNGUI4                                                               \
                 "S:`warngui4`:`Warning`{@v\n"                                 \
                     "\tL::`Syntax error on new configuration!`\n" \
                     "\tS::{@h\n"                                                \
                         "\t\t*p:`ok`:`OK`:e*\n"                                 \
                     "\t}\n"                                                     \
                 "}\n"

#define PLEASEWAIT                                                             \
                 "S::`Merging configuration`{@v\n"                             \
                     "\tl::`Please wait while merging configuration!`\n"       \
                 "}\n"

#define ADDGUI                                                                 \
                 "S:`addgui`:`Add new key`{@v\n"                               \
                     "\tf:`config`:`Config:`:\n"                               \
                     "\tS::{@h\n"                                              \
                         "\t\tp:`ok`:`OK`:e\n"                                 \
                         "\t\t*\n"                                             \
                         "\t\tp:`cancel`:`Cancel`:e\n"                         \
                     "\t}\n"                                                   \
                 "}\n"

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

const char *flags[] = { "Fixed", NULL };
static char writeFlags[] = { 'F', 0 };

int validateChangedValues(
    phGuiProcessId_t gui,
    phConfId_t configId,
    phAttrId_t attributesID,  /* attributes to validate against */
    phConfFlag_t selection,
    char **err
    );

int validateValue(
    char *data,
    struct keyedDefinition *entry, 
    struct keyedDefinition *attrEntry,
    phAttrId_t attributesID,  /* attributes to validate against */
    char **err
    );

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description: 
 *
 *****************************************************************************/
static void copyAndEnlargeString(char **dest, const char *src) {
    int destSize, srcSize;

    if (*dest != NULL) destSize = strlen(*dest);
    else destSize = 0;
    srcSize = strlen(src);

    *dest = (char *) realloc(*dest, destSize + srcSize + 1);
    if (*dest == 0) {
	perror("Error in copyAndEnlargeString");
	exit(1);
    }
    sprintf((*dest + destSize), src); 
}


static void logMergeRequest(
    phLogId_t logger, 
    phConfId_t confId
)
{
    phConfError_t confError;
    const char *key = NULL;
    char *oneDef = NULL;

    confError = phConfConfFirstKey(confId, &key);
    while (confError == PHCONF_ERR_OK)
    {
	confError = phConfGetConfDefinition(
	    confId, key, &oneDef);
	/* exceptionally log to lowest debug level ! */
	phLogConfMessage(logger, PHLOG_TYPE_MESSAGE_0, "%s", oneDef);
	confError = phConfConfNextKey(confId, &key);
    }
}

/*****************************************************************************
 *
 * Create the gui description for the configuration editor
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description: 
 *
 *****************************************************************************/
static void createControlPanel(
    char **descr,
    int enhanced
)
{
    copyAndEnlargeString(descr, "\tS::``{@h\n"); 
    copyAndEnlargeString(descr, "\t\tp:`ok`:`Apply`:e\n"); 
    if (enhanced) copyAndEnlargeString(descr, "\t\t*p:`add`:`Add`:e\n");
    copyAndEnlargeString(descr, "\t\t*p:`cancel`:`Cancel`:e\n");
    copyAndEnlargeString(descr, "\t}\n");

}

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description: 
 *
 *****************************************************************************/
static void createDeletePanel(
    int entries,
    char **descr
)
{
    int i;
    char data[160];

    i = 0;
    copyAndEnlargeString(descr, "\t\tS::{@v\n"); 
    while (i < entries) {
	copyAndEnlargeString(descr, "\t\t\tS::``{@h\n"); 
	sprintf(data, "\t\t\t\tp:`delete%d`:`Delete`:e", i);
	copyAndEnlargeString(descr, data);
	copyAndEnlargeString(descr, "\t\t\t}\n");
	i ++;
    }
    copyAndEnlargeString(descr, "\t\t}\n");
}

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description: 
 *
 *****************************************************************************/
static void createFlagPanel(
    phConfId_t configId,
    phAttrId_t attributesID,   /* attributes to validate against */
    phConfFlag_t selection,
    char **descr
)
{
    struct keyedDefinition *entry, *attrEntry;
    int i = 0, j = 0;
    char data[160];

    entry = configId->conf->first;

    copyAndEnlargeString(descr, "\t\tS::{@v\n");
    while (entry) {
	attrEntry = phConfSemSearchDefinition(attributesID->attr, entry->key);
	if (!attrEntry) {
	    entry = entry->next;
	    continue;
	}
	if (!validateDefinition(entry->data, attrEntry->data))  {
	    entry = entry->next;
	    continue;
	}

	if (((~entry->flagMask & PHCONF_FLAG_DONTCARE) 
	    | (entry->flagMask & selection)) == PHCONF_FLAG_DONTCARE) {

	    i = 0;
	    copyAndEnlargeString(descr, "\t\t\tT::``{\n");
	    while (flags[i] != NULL) {
		if ((entry->flagMask >> i) & 1) {
		    sprintf(data, "\t\t\t\t`%s%d`:`%s`:s\n", flags[i], j, flags[i]);
		}
		else {
		    sprintf(data, "\t\t\t\t`%s%d`:`%s`\n", flags[i], j, flags[i]);
		}
		copyAndEnlargeString(descr, data);
		i++;
	    }
	    j++;
	    copyAndEnlargeString(descr, "\t\t\t}:1\n");
	}
	entry = entry->next;
    }
    copyAndEnlargeString(descr, "\t\t}\n");
}

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description: 
 *
 *****************************************************************************/
static char *replaceUnderlines(char *text) {
    int i = 0, len = strlen(text);
    char *newText = strdup(text);
    char *ptr = newText;

    while (ptr ++ != NULL && i++ < len) {
	if (*ptr == '_') *ptr = ' ';
    }
    return newText;
}
 
static void createStandardEditor(
    phConfId_t configId,
    phAttrId_t attributesID,   /* attributes to validate against */
    phConfFlag_t selection,    /* selection of definitions, which should
				  be shown in gui */
    phPanelFlag_t editorFlags, /* selection of definitions, which editor-panels
				  should be shown in gui */
    int *entries,
    char **descr
)
{
    struct keyedDefinition *entry, *attrEntry;
    struct keyStrRng *range;
    int defString, i;
    char data[1600], *tmp;

    entry = configId->conf->first;
    copyAndEnlargeString(descr, "\t\tS::{@v\n");

    (*entries) = 0;

    while (entry)
    {
	attrEntry = phConfSemSearchDefinition(attributesID->attr, entry->key);
	if (!attrEntry) {
	    entry = entry->next;
	    continue;
	}
	if (!validateDefinition(entry->data, attrEntry->data))  {
	    entry = entry->next;
	    continue;
	}

	if (((~entry->flagMask & PHCONF_FLAG_DONTCARE) 
	    | (entry->flagMask & selection)) == PHCONF_FLAG_DONTCARE) {

	    copyAndEnlargeString(descr, "\t\t\tS::``{@h\n");

	    if (!(editorFlags & PHCONF_PANEL_FLAGS) && 
		(entry->flagMask & PHCONF_FLAG_FIXED)) 
		copyAndEnlargeString(descr, "~");

	    switch (attrEntry->data->type) {
	      case PHCONF_KEY_TYPE_NUM_RNG: /* data entry is a number range      */
		if (attrEntry->data->d.numRng.precision == PHCONF_NUM_TYPE_INT) {
		    sprintf(data, "\t\t\t\tf:`%s`:`%s (%g-%g i)`:`%g`\n",
			attrEntry->key, 
			tmp = replaceUnderlines(attrEntry->key), 
			attrEntry->data->d.numRng.lowBound,
			attrEntry->data->d.numRng.highBound,
			entry->data->d.numVal);
		    copyAndEnlargeString(descr, data);
		    free(tmp);
		}
		else {
		    sprintf(data, "\t\t\t\tf:`%s`:`%s (%g-%g f)`:`%g`",
			attrEntry->key, 
			tmp = replaceUnderlines(attrEntry->key),
			attrEntry->data->d.numRng.lowBound,
			attrEntry->data->d.numRng.highBound,
			entry->data->d.numVal);
		    copyAndEnlargeString(descr, data);
		    free(tmp);
		}
		if (entry->hint != NULL && entry->hint[0] != 0) {
		    copyAndEnlargeString(descr, "[`");
		    copyAndEnlargeString(descr, entry->hint);
		    copyAndEnlargeString(descr, "`]\n");
		}
		else copyAndEnlargeString(descr, "\n");
		break;
	      case PHCONF_KEY_TYPE_STR_RNG: /* data entry is a choice of strings */
		range = attrEntry->data->d.strRng;
		if (strcmp(range->entry, "*") == 0) {
		    sprintf(data, "\t\t\t\tf:`%s`:`%s (*)`:`", 
			entry->key, 
			tmp = replaceUnderlines(entry->key));
		    copyAndEnlargeString(descr, data);
		    free(tmp);
		    data[0] = 0;
		    phConfSemCatDataText(data, entry->data, 0);    
		    copyAndEnlargeString(descr, data);
		    copyAndEnlargeString(descr, "`");
		}
		else {
		    sprintf(data, "\t\t\t\to:`%s`:`%s`(\n", entry->key, tmp = replaceUnderlines(entry->key));
		    copyAndEnlargeString(descr, data);
		    free(tmp);
		    defString = i = 1;
		    while (range) {
			sprintf(data, "\t\t\t\t`%s`\n", range->entry); 		
			copyAndEnlargeString(descr, data);
			if (strcmp(range->entry, entry->data->d.strVal) == 0) defString = i;
			i++;
			range = range->next;
		    }
		    sprintf(data, "\t\t\t\t):%d", defString); 
		    copyAndEnlargeString(descr, data);
		}
		if (entry->hint != NULL && entry->hint[0] != 0) {
		    copyAndEnlargeString(descr, "[`");
		    copyAndEnlargeString(descr, entry->hint);
		    copyAndEnlargeString(descr, "`]\n");
		}
		else copyAndEnlargeString(descr, "\n");
		break;
	      case PHCONF_KEY_TYPE_LIST:    /* data entry is a list of entries   */
		sprintf(data, "\t\t\t\tf:`%s`:`%s`:`", entry->key, tmp = replaceUnderlines(entry->key));
		copyAndEnlargeString(descr, data);
		free(tmp);
		data[0] = 0;
		phConfSemCatDataText(data, entry->data, 0);    
		copyAndEnlargeString(descr, data);
		if (entry->hint != NULL && entry->hint[0] != 0) {
		    copyAndEnlargeString(descr, "`[`");
		    copyAndEnlargeString(descr, entry->hint);
		    copyAndEnlargeString(descr, "`]\n");
		}
		else copyAndEnlargeString(descr, "`\n");
		break;
	      default:
		break;
	    }

	    copyAndEnlargeString(descr, "\t\t\t}\n");
	    (*entries) ++;
	}
	entry = entry->next;
    }

    copyAndEnlargeString(descr, "\t\t}\n"); 
}

/*****************************************************************************
 *
 * Create the gui description for the configuration editor
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description: 
 *
 *****************************************************************************/
static void createDialog(
    phConfId_t configId,
    phAttrId_t attributesID, /* attributes to validate against */
    phConfFlag_t selection,  /* selection of definitions, which should
				be shown in gui */
    phPanelFlag_t panelflags,
    char **descr
) 
{
    int entries = 0;
    char *tmpDescr = NULL;

    (*descr) = NULL;

    copyAndEnlargeString(descr, "S:`editDialog`:`Edit Configuration`{@v\n");
    if (configId->conf->first) {
	copyAndEnlargeString(&tmpDescr, "\tS::{@h\n");
	createStandardEditor(configId, attributesID, selection, (phPanelFlag_t)(panelflags & ~PHCONF_PANEL_EDIT), &entries, &tmpDescr);
	if (panelflags & PHCONF_PANEL_FLAGS) createFlagPanel(configId, attributesID, selection, &tmpDescr);
	if (panelflags & PHCONF_PANEL_DELETE) createDeletePanel(entries, &tmpDescr);
	copyAndEnlargeString(&tmpDescr, "\t}\n");
    }
    if (entries) copyAndEnlargeString(descr, tmpDescr);
    free(tmpDescr);
    createControlPanel(descr, panelflags & PHCONF_PANEL_EDIT);
    copyAndEnlargeString(descr, "}\n");
}

/*****************************************************************************
 *
 * write back changed configuration
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description:
 *
 *****************************************************************************/
int isqstring(char *text) {
    int i = 0, l;

    l = strlen(text);
    if (l > 1) {
      if (text[0] == '"') {
        while (++i < l-1) if (text[i] == '"') return 0;
        if (text[i] == '"') return 1;
      }
      else if (text[0] == '\'') {
        while (++i < l-1) if (text[i] == '\'') return 0;
        if (text[i] == '\'') return 1;
      }
    }
    return 0;
}

int validateValue(
    char *data,
    struct keyedDefinition *entry, 
    struct keyedDefinition *attrEntry,
    phAttrId_t attributesID,  /* attributes to validate against */
    char **err
    )
{
    double numVal;
    char data2[1024];
    char errtext[1024];
    char *ptr;
    struct keyStrRng *range;
    phConfId_t tempConfId = NULL;
    int result = 1, valid;

    switch (entry->data->type)
    {
      case PHCONF_KEY_TYPE_NUM_VAL:
        numVal = strtod(data, &ptr);

        if ((numVal == 0 && (data == ptr || errno == ERANGE)) ||
            (attrEntry->data->d.numRng.precision == PHCONF_NUM_TYPE_INT && numVal != (double) ((int) numVal)) ||
            (ptr != NULL && *ptr != 0)) {
          sprintf(errtext, 
                  "%s: Value has an illegal type. Expected '%s'\n", entry->key,
                  attrEntry->data->d.numRng.precision == PHCONF_NUM_TYPE_INT? 
                  "Integer" : "Float");
          copyAndEnlargeString(err, errtext);
          result = 0;
          break;
        }

        /* the attribute really expects a numeric value here */
        else if (numVal < attrEntry->data->d.numRng.lowBound ||
                 numVal > attrEntry->data->d.numRng.highBound ||
                 numVal == +HUGE_VAL || 
                 numVal == -HUGE_VAL)
        {
          sprintf(errtext, 
                  "%s: Value is out of Range. Expected [%g, %g]\n", 
                  entry->key,
                  attrEntry->data->d.numRng.lowBound,
                  attrEntry->data->d.numRng.highBound);
          copyAndEnlargeString(err, errtext);
          result = 0;
        }
        break;
      case PHCONF_KEY_TYPE_STR_VAL:
        range = attrEntry->data->d.strRng;
        valid = 0;
        while (range) {
          if (strcmp(range->entry, data) == 0 ||
              (strcmp(range->entry, "*") == 0 && isqstring(data))) {
            valid = 1;
          }
          range = range->next;
        }
        if (!valid) {
          sprintf(errtext, "%s: Value is out of range!\n", entry->key);
          copyAndEnlargeString(err, errtext);
          result = 0;
        }
        break;
      case PHCONF_KEY_TYPE_LIST:
        sprintf(data2, "%s:%s", entry->key, data);
        if (phConfSetConfDefinition(&tempConfId, data2) != PHCONF_ERR_OK || 
            phConfValidate(tempConfId, attributesID) != PHCONF_ERR_OK) {
          sprintf(errtext, "%s: Syntax error!", entry->key);
          copyAndEnlargeString(err, errtext);          
          result = 0;
        }
        if (tempConfId != NULL) {
          phConfDestroyConf(tempConfId);
          free(tempConfId);
          tempConfId = NULL;
        }
        break;
      default:
        break;
    }
    return result;
}

int validateChangedValues(
    phGuiProcessId_t gui,
    phConfId_t configId,
    phAttrId_t attributesID,  /* attributes to validate against */
    phConfFlag_t selection,
    char **err
)
{
    struct keyedDefinition *entry, *attrEntry;
    char data[2048];
    int result = 1;

    entry = configId->conf->first;

    while (entry)
    {
	attrEntry = phConfSemSearchDefinition(attributesID->attr, entry->key);
	if (!attrEntry) {
	    entry = entry->next;
	    continue;
	}

	if (((~entry->flagMask & PHCONF_FLAG_DONTCARE) 
	    | (entry->flagMask & selection)) == PHCONF_FLAG_DONTCARE) {

	    phGuiGetData(gui, entry->key, data, 3);

	    result &= validateValue(data, entry, attrEntry, attributesID, err);
	}
	entry = entry->next;
    }
    return result;
}

/*****************************************************************************
 *
 * write back changed configuration
 *
 * Authors: Ulrich Frank
 *
 * History: 04.01.2000, Ulrich Frank, created
 *
 * Description:
 *
 *****************************************************************************/
static void storeChangedValues(
    phGuiProcessId_t gui,
    phConfId_t configId,
    phAttrId_t attributesID,  /* attributes to validate against */
    phConfFlag_t selection,
    phPanelFlag_t panelflags,
    phLogId_t logger
)
{
    struct keyedDefinition *entry, *attrEntry;
    phConfId_t tempConfId = NULL;
    char data[2048];
    char data2[2048];
    char flagString[80];
    char tmp[160];
    char tmp2[160];
    char *data3;
    int i, j, k;

    data3 = NULL;

    entry = configId->conf->first;
    j = 0;

    while (entry)
    {
      attrEntry = phConfSemSearchDefinition(attributesID->attr, entry->key);
      if (!attrEntry) {
        entry = entry->next;
        continue;
      }

      if (((~entry->flagMask & PHCONF_FLAG_DONTCARE) 
           | (entry->flagMask & selection)) == PHCONF_FLAG_DONTCARE) {

        flagString[0] = '<';
        k = 1;
        i = 0;
        if (panelflags & PHCONF_PANEL_FLAGS) {
          while (flags[i] != NULL) {
            strncpy(tmp, flags[i], sizeof(tmp));
            tmp[sizeof(tmp)-1] = '\0';
            sprintf(&tmp[strlen(tmp)], "%ld", (long) j);
            phGuiGetData(gui, tmp, tmp2, 3);
            if (strcasecmp(tmp2, "True") == 0) flagString[k++] = writeFlags[i];
            i++;
          }
          flagString[k] = '>';
          flagString[k+1] = 0;
        }
        if (k == 1) flagString[0] = 0;

        switch (entry->data->type)
        {
          case PHCONF_KEY_TYPE_NUM_VAL:
            phGuiGetData(gui, entry->key, data, 3);
            sprintf(data2, "{%s}%s:%s%s", entry->hint, entry->key, flagString, data);
            copyAndEnlargeString(&data3, data2);
            break;
          case PHCONF_KEY_TYPE_STR_VAL:
            phGuiGetData(gui, entry->key, data, 3);
            if (strcmp(attrEntry->data->d.strRng->entry, "*") != 0) {
              sprintf(data2, "{%s}%s:%s\"%s\"", entry->hint, entry->key, flagString, data);
            }
            else sprintf(data2, "{%s}%s:%s%s", entry->hint, entry->key, flagString, data);
            copyAndEnlargeString(&data3, data2);
            break;
          case PHCONF_KEY_TYPE_LIST:
            phGuiGetData(gui, entry->key, data, 3);
            sprintf(data2, "{%s}%s:%s%s", entry->hint, entry->key, flagString, data);
            copyAndEnlargeString(&data3, data2);
            break;
          default:
            break;
        }
        j++;
      }
      entry = entry->next;
    }
    phConfSetConfDefinition(&tempConfId, data3);
    logMergeRequest(logger, tempConfId);
    phConfMerge(configId, tempConfId, PHCONF_FLAG_DONTCARE);
    phConfDestroyConf(tempConfId);
    free(tempConfId);
}

static void searchKeyByNr(
    int nr, 
    phConfId_t configId,
    phAttrId_t attributesID,
    phConfFlag_t selection,
    struct keyedDefinition **key
)
{
    struct keyedDefinition *entry, *attrEntry;
    int i = 0;

    entry = configId->conf->first;

    while (entry) {
	attrEntry = phConfSemSearchDefinition(attributesID->attr, entry->key);
	if (!attrEntry) {
	    entry = entry->next;
	    continue;
	}

	if (((~entry->flagMask & PHCONF_FLAG_DONTCARE) 
	    | (entry->flagMask & selection)) == PHCONF_FLAG_DONTCARE) {

	    if (i == nr) {
		*key = entry;
		return;
	    }
	    i ++;
	}
	entry = entry->next;
    }
    key = NULL;
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
 *****************************************************************************/
static phConfError_t addNewDefinition(
    phLogId_t logger,
    phAttrId_t attributesID,
    phConfId_t targetConfId,
    int *changed
) {
    int done = 0;
    char button[40];
    char data[1000], otherDescr[500];
    struct phConfConfStruct *confId = NULL;
    struct keyedDefinition *attrEntry;
    phGuiProcessId_t otherGui = NULL, warnGui = NULL;
    char *err;

    *changed = 0;
    
    while (!done) {
      button[0] = 0;
      otherGui = NULL;
      
      phGuiCreateUserDefDialog(&otherGui, logger, PH_GUI_COMM_SYNCHRON, ADDGUI);
      phGuiShowDialogGetResponse(otherGui, 1, button);
      if (strcmp((const char *) button, "ok") == 0) {
        phGuiGetData(otherGui, "config", data, 3);
        warnGui = NULL;
        confId = NULL;
        if (phConfSetConfDefinition(&confId, data) != PHCONF_ERR_OK) {
          phGuiDisableComponent(otherGui, "addgui");
          phGuiCreateUserDefDialog(&warnGui, logger, PH_GUI_COMM_SYNCHRON, WARNGUI4);
          phGuiShowDialogGetResponse(warnGui, 1, button);
          phGuiDestroyDialog(warnGui);
          warnGui = NULL;
          phGuiEnableComponent(otherGui, "addgui");
        } else {
          if ((attrEntry = phConfSemSearchDefinition(attributesID->attr, confId->conf->first->key))) {
            if (!phConfSemSearchDefinition(targetConfId->conf, confId->conf->first->key)) {
              err = NULL;
              data[0] = 0;
              phConfSemCatDataText(data, confId->conf->first->data, 0);
              if (validateValue(data, confId->conf->first, attrEntry, attributesID, &err)) {
                logMergeRequest(logger, confId);
                phConfMerge(targetConfId, confId, PHCONF_FLAG_DONTCARE);
                *changed = 1;
                done = 1;
              } else {
                phGuiDisableComponent(otherGui, "addgui");
                sprintf(otherDescr, WARNGUI3, err);
                phGuiCreateUserDefDialog(&warnGui, logger, PH_GUI_COMM_SYNCHRON, otherDescr);
                phGuiShowDialogGetResponse(warnGui, 1, button);
                phGuiDestroyDialog(warnGui);
                warnGui = NULL;
                phGuiEnableComponent(otherGui, "addgui");
              }
              free(err);
            } else {
              phGuiDisableComponent(otherGui, "addgui");
              sprintf(otherDescr, WARNGUI1, confId->conf->first->key);
              phGuiCreateUserDefDialog(&warnGui, logger, PH_GUI_COMM_SYNCHRON, otherDescr);
              phGuiShowDialogGetResponse(warnGui, 1, button);
              phGuiDestroyDialog(warnGui);
              warnGui = NULL;
              //free (confId);    //should not use "free" here, but "phConfDestroyConf"              
              phGuiEnableComponent(otherGui, "addgui");
            }
          } else {
            phGuiDisableComponent(otherGui, "addgui");
            sprintf(otherDescr, WARNGUI2, confId->conf->first->key);
            phGuiCreateUserDefDialog(&warnGui, logger, PH_GUI_COMM_SYNCHRON, otherDescr);
            phGuiShowDialogGetResponse(warnGui, 1, button);
            phGuiDestroyDialog(warnGui);
            warnGui = NULL;
            //free (confId);     //should not use "free" here, but "phConfDestroyConf"
            phGuiEnableComponent(otherGui, "addgui");
          }
        }
        if (confId != NULL) {
          phConfDestroyConf(confId);
          free(confId);
          confId = NULL;
        }
      } else {
        done = 1;
      }

      phGuiDestroyDialog(otherGui);
    }
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
 *****************************************************************************/
phConfError_t createAndHandleEditor(
    phLogId_t logger,
    phConfId_t configId,
    phAttrId_t attributesID,  /* attributes to validate against */
    phConfFlag_t selection,   /* selection of definitions, which should
				be shown in gui */
    phPanelFlag_t panelflags,
    int *response             /* 1 if ok is pressed, 0 if cancel */
) 
{
    struct keyedDefinition *deleteVar = NULL;
    phGuiProcessId_t gui = NULL, otherGui = NULL;
    phConfError_t error = PHCONF_ERR_OK;
    char button[40], otherDescr[2048], *descr;
    char *err;
    int done = 0, nr = 0, changed;

    CheckHandle(configId);
    CheckHandle(attributesID);

    createDialog(configId, attributesID, selection, panelflags, &descr);
    phGuiCreateUserDefDialog(&gui, logger, PH_GUI_COMM_SYNCHRON, descr);

    while (!done) {
	button[0] = 0;
	if (phGuiShowDialogGetResponse(gui, 0, button) != PH_GUI_ERR_OK) {
	    error =  PHCONF_ERR_INTERNAL;
	    done = 1;
	    break;
	}
	else {
	    if (strcmp((const char *) button, "ok") == 0) {
		phGuiCreateUserDefDialog(&otherGui, logger, PH_GUI_COMM_ASYNCHRON, PLEASEWAIT);
		phGuiShowDialog(otherGui, 0);

		done = 1;
		phGuiDisableComponent(gui, "editDialog");
		err = NULL;
		if (validateChangedValues(gui, configId, attributesID, selection, &err)) {
		    storeChangedValues(gui, configId, attributesID, selection, panelflags, logger);
		}
		else {
		    sprintf(otherDescr, ERRORGUI, err);

				phGuiDestroyDialog(otherGui);
				otherGui = NULL;
		    phGuiCreateUserDefDialog(&otherGui, logger, PH_GUI_COMM_SYNCHRON, otherDescr);

				phGuiShowDialogGetResponse(otherGui, 1, button);
		    
		    done = 0;
		    if (strcmp((const char *) button, "restart") == 0) {
			phGuiConfigure(gui, descr);
		    }
		    else if (strcmp((const char *) button, "reedit") == 0) {
		    }
		    else {
			done = 1;
		    }
		}
		free(err);
		err = NULL;
		phGuiEnableComponent(gui, "editDialog");
		phGuiDestroyDialog(otherGui);
		otherGui = NULL;
	    }
	    else if (strcmp(button, "add") == 0) {
		phGuiDisableComponent(gui, "editDialog");
		addNewDefinition(logger, attributesID, configId, &changed);
		if (changed) {
		    createDialog(configId, attributesID, selection, panelflags, &descr);
		    phGuiConfigure(gui, descr);
		}
		
		phGuiEnableComponent(gui, "editDialog");
	    }
	    else if ((const char *) strstr(button, "delete") == button) {
		phGuiDisableComponent(gui, "editDialog");
		/* confirm dialog */
		nr = atol(button + strlen("delete"));
		button[0] = 0;
		searchKeyByNr(nr, configId, attributesID, selection, &deleteVar);
		sprintf(otherDescr, CONFIRMGUI, deleteVar->key);

		/* find out selected key */
		phGuiCreateUserDefDialog(&otherGui, logger, PH_GUI_COMM_SYNCHRON, otherDescr);
		phGuiShowDialogGetResponse(otherGui, 1, button);
		phGuiDestroyDialog(otherGui);
		otherGui = NULL;
		if (strcmp((const char *) button, "ok") == 0) {
		    phConfSemDeleteDefinitionKey(configId->conf, deleteVar);
		    free(descr);
		    createDialog(configId, attributesID, selection, panelflags, &descr);
		    phGuiConfigure(gui, descr);
		}
		phGuiEnableComponent(gui, "editDialog");
		done = 0;
	    }
	    else if (strcmp(button, "cancel") == 0) {
		done = 1;
	    }
	}
    }
    if (response != NULL) *response = strcmp((const char *) button, "ok")==0? 1:0;
    free(descr);
    phGuiDestroyDialog(gui);

    return error;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
