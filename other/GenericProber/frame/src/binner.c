/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : binner.c
 * CREATED  : 23 Jul 1999
 *
 * CONTENTS : Bin Management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 23 Jul 1999, Michael Vogt, created
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
#include "binner.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/
#define RELEASE_BUFFER(x) {\
   if ( x != NULL ) \
   {\
     free(x);\
     x = NULL;\
   }\
}

/*--- typedefs --------------------------------------------------------------*/

struct stBinSorter
{
    int                 used;
    int                 nextUse;
    long               *handlerBinIndexField;
};

struct binMapEntry
{
    long long           code;
    struct stBinSorter *sorter;
    struct binMapEntry *next;
};

struct binningMap
{
    struct binMapEntry *mapList;
    int numberOfHandlerBins;
};


struct binActMapEntry
{
    long long           code;
    phBinActType_t      action;
    struct binActMapEntry *next;
};

struct binningActMap
{
    struct binActMapEntry *mapList;
};

/*--- functions -------------------------------------------------------------*/

static struct binMapEntry **searchCode(phBinMapId_t handle, long long code)
{
    struct binMapEntry **current;

    current = &(handle->mapList);
    while (*current && (*current)->code != code)
	current = &((*current)->next);

    return current;
}

static struct binActMapEntry **searchActCode(phBinActMapId_t handle, long long code)
{
    struct binActMapEntry **current;

    current = &(handle->mapList);
    while (*current && (*current)->code != code)
	current = &((*current)->next);

    return current;
}

/*****************************************************************************
 *
 * Reset/Initialize the bin mapping feature
 *
 * Authors: Michael Vogt
 *
 * Description: Prepares entering of a new smartest to handler
 * mapping, frees an existing mapping
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int resetBinMapping(phBinMapId_t *handle, int num)
{
    struct binMapEntry *help;

    if (*handle)
    {
	while ((*handle)->mapList)
	{
	    /* clean up */
	    free((*handle)->mapList->sorter->handlerBinIndexField);
	    free((*handle)->mapList->sorter);
	    help = (*handle)->mapList;
	    (*handle)->mapList = (*handle)->mapList->next;
	    free(help);
	}
    }
    
    *handle = PhNew(struct binningMap);
    if (! (*handle))
	return 1;

    (*handle)->mapList = NULL;
    (*handle)->numberOfHandlerBins = num;

    return 0;
}

/*****************************************************************************
 *
 * Reset/Initialize the bin mapping feature
 *
 * Authors: Michael Vogt
 *
 * Description: Prepares entering of a new smartest bin to action
 * mapping, frees an existing mapping
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int resetBinActMapping(phBinActMapId_t *handle)
{
    struct binActMapEntry *help;

    if (*handle)
    {
	while ((*handle)->mapList)
	{
	    /* clean up */
	    help = (*handle)->mapList;
	    (*handle)->mapList = (*handle)->mapList->next;
	    free(help);
	}
    }
    
    *handle = PhNew(struct binningActMap);
    if (! (*handle))
	return 1;

    (*handle)->mapList = NULL;

    return 0;
}

/*****************************************************************************
 *
 * Enter one entry into the map
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * If the <stCode> has not already got an entry, a new binMapEntry is
 * allocated and a new stBinSorter entry is allocated, based on the
 * <numberOf HandlerBins> given during initialization. The new
 * stBinSorter is initialized to { 0 , 0 , ... }. The new binMapEntry
 * is initialized to { <stCode>, ..., ... } and put to the top of the
 * internal list.
 *
 * If the entry exists, <handlerBinIndex> is entered at the <used>
 * position and <used> is incremented, checking that used doesn't hit
 * the <numberOf HandlerBins> border before doing this.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int enterBinMap(phBinMapId_t handle, long long stCode, long handlerBinIndex)
{
    struct binMapEntry *current;

    /* search Code */
    current = *searchCode(handle, stCode);

    if (!current)
    {
	/* not found, allocate new SmarTest code entry */
	current = PhNew(struct binMapEntry);
	if (!current)
	    return 1;
	current->code = stCode;

	/* allocate a new sorter for this SmarTest Bin */
	current->sorter = PhNew(struct stBinSorter);
	if (!current->sorter) {
            RELEASE_BUFFER(current);
	    return 1;
        }
	current->sorter->used = 0;
	current->sorter->nextUse = 0;
	current->sorter->handlerBinIndexField = PhNNew(long, handle->numberOfHandlerBins);
	if (!current->sorter->handlerBinIndexField) {
            RELEASE_BUFFER(current->sorter);
            RELEASE_BUFFER(current);
	    return 1;
        }

	/* put it to the top of sorter lists */
	current->next = handle->mapList;
	handle->mapList = current;
    }

    /* if the maximum number of handler bins was used for this sorter,
       return with error code */
    if (current->sorter->used == handle->numberOfHandlerBins) {
        RELEASE_BUFFER(current->sorter->handlerBinIndexField);
        RELEASE_BUFFER(current->sorter);
        RELEASE_BUFFER(current);
	return 2;
    }

    /* enter handler bin into sorter, increment used entries */
    current->sorter->handlerBinIndexField[current->sorter->used] = 
	handlerBinIndex;
    current->sorter->used++;

    return 0;    
}

/*****************************************************************************
 *
 * Enter one entry into the map
 *
 * Authors: Michael Vogt
 *
 * Description: 
 *
 * The given <action> is added to the given bin Code <stCode>
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int enterBinActMap(phBinActMapId_t handle, long long stCode, phBinActType_t action)
{
    struct binActMapEntry *current;

    /* search Code */
    current = *searchActCode(handle, stCode);

    if (!current)
    {
	/* not found, allocate new SmarTest code entry with no action */
	current = PhNew(struct binActMapEntry);
	if (!current)
	    return 1;
	current->code = stCode;
	current->action = PHBIN_ACT_NONE;

	/* put it to the top of sorter lists */
	current->next = handle->mapList;
	handle->mapList = current;
    }

    /* add the requested action */
    current->action = (phBinActType_t)(current->action | action);

    return 0;    
}

/*****************************************************************************
 *
 * Map one SmarTest bin to one handler bin
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The <stCode> is searched in the binMapEntry List and put to tht top
 * of the list. In the associated stBinSorter, the entry at <nextUse>
 * is retrieved and returned in <handlerBinIndex>. <nextUse> is
 * incremented or reset to 0.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int mapBin(phBinMapId_t handle, long long stCode, long *handlerBinIndex)
{
    struct binMapEntry **pCurrent;
    struct binMapEntry *help;
    struct stBinSorter *sorter;

    /* search code */
    pCurrent = searchCode(handle, stCode);
    if (! *pCurrent)
	return 2;

    /* retrieve next handler bin to use from this sorter, increment
       sorter position */
    sorter = (*pCurrent)->sorter;
    *handlerBinIndex = sorter->handlerBinIndexField[sorter->nextUse];
    (sorter->nextUse)++;
    if (sorter->nextUse >= sorter->used)
	sorter->nextUse = 0;

    /* put entry for this SmarTest Bin to the top of the cache list */
    if (pCurrent != &(handle->mapList))
    {
	help = *pCurrent;
	*pCurrent = help->next;
	 help->next = handle->mapList;
	 handle->mapList = help;
    }

    return 0;
}

/*****************************************************************************
 *
 * Map one SmarTest bin to an action combination
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The <stCode> is searched in the bin action list and returned.
 *
 * Returns: 0 on success, or error number >= 1
 *
 ***************************************************************************/
int mapAct(phBinActMapId_t handle, long long stCode, phBinActType_t *action)
{
    struct binActMapEntry **pCurrent;
    struct binActMapEntry *help;
    int tmpReturn = 0;

    /* search code */
    pCurrent = searchActCode(handle, stCode);
    if (! *pCurrent)
    {
	/* this bin does not have a defined action but it is likely to
           be used quite often, so create an empty entry in the action
           map, put it to the top of the list and return an NONE
           action */

	tmpReturn = enterBinActMap(handle, stCode, PHBIN_ACT_NONE);
	if (tmpReturn)
	    return tmpReturn;

	*action = PHBIN_ACT_NONE;
	return 0;
    }

    /* get the related action */
    *action = (*pCurrent)->action;

    /* put entry for this SmarTest Bin to the top of the cache list */
    if (pCurrent != &(handle->mapList))
    {
	help = *pCurrent;
	*pCurrent = help->next;
	 help->next = handle->mapList;
	 handle->mapList = help;
    }

    return 0;
}

/*Begin of Huatek Modifications, Donnie Tu, 04/23/2002*/
/*Issue Number: 334*/
/*****************************************************************************
 *
 * Free the bin mapping feature
 *
 * Authors: Donnie Tu
 *
 * Description: frees an existing mapping
 *
 * Returns: none
 *
 ***************************************************************************/
 
void freeBinMapping( phBinMapId_t *handle)
{
struct binMapEntry *help;

    if (*handle)
    {
        while ((*handle)->mapList)
        {
            /* clean up */
            free((*handle)->mapList->sorter->handlerBinIndexField);
            free((*handle)->mapList->sorter);
            help = (*handle)->mapList;
            (*handle)->mapList = (*handle)->mapList->next;
            free(help);
        }
        free(*handle);
        *handle=NULL;
    }
}
/*End of Huatek Modifications*/


/*Begin of kaw Modifications 11/15/2004*/
/*Issue Number: 15686*/
/*****************************************************************************
 *
 * Free the bin Act mapping feature
 *
 * Authors: Ken Ward
 *
 * Description: frees act map 
 *
 * Returns: none
 *
 ***************************************************************************/
 
void freeActBinMapping( phBinActMapId_t *handle)
{
    struct binActMapEntry *help;

    if (*handle) {
        while ((*handle)->mapList) {
            /* clean up */
            help = (*handle)->mapList;
            (*handle)->mapList = (*handle)->mapList->next;
            free(help);
        }
        free(*handle);
        *handle=NULL;
    }
}
/*End of kaw Modifications*/

/*****************************************************************************
 * End of file
 *****************************************************************************/
