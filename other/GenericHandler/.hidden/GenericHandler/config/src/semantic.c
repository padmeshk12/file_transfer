/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : semantic.c
 * CREATED  : 16 Jun 1999
 *
 * CONTENTS : Semantic actions for config and attrib parser
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 16 Jun 1999, Michael Vogt, created
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
#include "ph_conf.h"

#include "semantic.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/* concat list z of type *x to the end of list y of type
   *x. type x must contain a 'next' field of type *x. y and z
   must be variables of type *x. y may be NULL to start a new list */

#define ListConcat(x,y,z) { \
    x *__tmp_ptr; \
    if ((y) != NULL) \
    { \
        __tmp_ptr = (y); \
        while (__tmp_ptr -> next != NULL) \
            __tmp_ptr = __tmp_ptr -> next; \
        __tmp_ptr -> next = (z); \
    } \
    else \
        (y) = (z); \
}

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/
/*Begin of Huatek Modifications, Donnie Tu, 04/19/2002*/
/*Issue Number: 334*/
static int mySize = 0;
static char *myBuffer = NULL;
/*End of Huatek Modifications*/

/*--- functions -------------------------------------------------------------*/

static struct keyData *allocKeyData(void)
{
    return PhNew(struct keyData);
}

static struct listSize *allocSizeData(void)
{
    return PhNew(struct listSize);
}

static struct keyStrRng *allocKeyStrRng(void)
{
    return PhNew(struct keyStrRng);
}

static struct keyedDefinition *allocKeyedDefinition(void)
{
    return PhNew(struct keyedDefinition);
}

static struct defList *allocTree(void)
{
    return PhNew(struct defList);
}

static char *provideStaticTmpSpace(int size)
{
/*Begin of Huatek Modifications, Donnie Tu, 04/19/2002*/
/*Issue Number: 334*/
/*    static int mySize = 0;
    static char *myBuffer = NULL;*/
/*End of Huatek Modifications*/

    /* make the buffer at least 1K big */
    if (size < 1024)
	size = 1024;

    /* (re)allocate space, if bigger than expected */
    if (mySize < size)
    {
	myBuffer = (char *)realloc(myBuffer, size);
	mySize = size;
    }

    return myBuffer;
}

/*Begin of Huatek Modifications, Donnie Tu, 04/19/2002*/
/*Issue Number: 334*/
void deleteStaticTmpSpace()
{
   if(myBuffer!=NULL)
   {
      free(myBuffer);
      myBuffer=NULL;
      mySize=0;
   }

}
/*End of Huatek Modifications*/


/* produce a rough guess of how many bytes a textual representation of
   the passed data entry structure would need */
static int sizeofData(struct keyData *ptr)
{
    /* assume some bytes for formatting purposes per data entry,
       especially used for lists */
    int count = 80;
    struct keyStrRng *sptr;

    /* no data, return */
    if (!ptr)
	return count;

    /* work through lists first */
    if (ptr->next)
	count += sizeofData(ptr->next);

    /* handle single data entry */
    switch (ptr->type)
    {
      case PHCONF_KEY_TYPE_NUM_VAL:
	/* assume 32 byte for a "%g" style number print */
	count += 32;
	break;
      case PHCONF_KEY_TYPE_NUM_RNG:
	/* two numbers in style " ( %g : %g %c )  " */
	count += 32 + 32 + 12;
	break;
      case PHCONF_KEY_TYPE_STR_VAL:
	/* real string len plus quotes: "\"%s\" " */
	count += strlen(ptr->d.strVal) + 3;
	break;
      case PHCONF_KEY_TYPE_STR_RNG:
	/* style: "\"%s\" | " per string */
	sptr = ptr->d.strRng;
	while (sptr)
	{
	    count += strlen(sptr->entry);
	    count += 2 + 3;
	    sptr = sptr->next;
	}
	break;
      case PHCONF_KEY_TYPE_LIST:
	/* style: "[ ] " */
	count += 4;
	count += sizeofData(ptr->d.list.head);
	if (ptr->d.list.size)
	{
	    /* style: " %g-%g" */
	    count += 32;
	    count += 32;
	    count += 2;
	}
	break;
    }

    return count;
}

/*****************************************************************************
 *
 * 
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
void phConfSemCatDataText(char *buffer, struct keyData *ptr, int indent)
{
    char tmpBuf[32 + 32 + 12];
    struct keyStrRng *sptr = NULL;

    while (ptr)
    {
	switch(ptr->type)
	{
	  case PHCONF_KEY_TYPE_NUM_VAL:
	    sprintf(tmpBuf, "%g", ptr->d.numVal);
	    strcat(buffer, tmpBuf);
	    break;
	  case PHCONF_KEY_TYPE_NUM_RNG:
	    sprintf(tmpBuf, "(%g:%g %c)", 
		    ptr->d.numRng.lowBound,
		    ptr->d.numRng.highBound,
		    ptr->d.numRng.precision == PHCONF_NUM_TYPE_INT ? 'i' :
		    ptr->d.numRng.precision == PHCONF_NUM_TYPE_FLOAT ? 'f' :
		    'f');
	    strcat(buffer, tmpBuf);
	    break;
	  case PHCONF_KEY_TYPE_STR_VAL:
	    if (strchr(ptr->d.strVal, '"'))
		strcpy(tmpBuf, "'");
	    else
		strcpy(tmpBuf, "\"");
	    strcat(buffer, tmpBuf);
	    strcat(buffer, ptr->d.strVal);
	    strcat(buffer, tmpBuf);
	    break;
	  case PHCONF_KEY_TYPE_STR_RNG:
	    sptr = ptr->d.strRng;
	    while (sptr)
	    {
		if (strchr(sptr->entry, '"'))
		    strcpy(tmpBuf, "'");
		else
		    strcpy(tmpBuf, "\"");
		strcat(buffer, tmpBuf);
		strcat(buffer, sptr->entry);
		strcat(buffer, tmpBuf);
		sptr = sptr->next;
		if (sptr)
		    strcat(buffer, " | ");
	    }
	    break;
	  case PHCONF_KEY_TYPE_LIST:
	    strcat(buffer, "[ ");
	    phConfSemCatDataText(buffer, ptr->d.list.head, indent+2);
	    if (ptr->d.list.size)
	    {
		strcat(buffer, " ");
		sprintf(tmpBuf, "%d", ptr->d.list.size->minCount);
		strcat(buffer, tmpBuf);
		if (ptr->d.list.size->minCount != ptr->d.list.size->maxCount)
		{
		    strcat(buffer, "-");
		    if (ptr->d.list.size->maxCount < 0)
			strcat(buffer, "?");
		    else
		    {
			sprintf(tmpBuf, "%d", ptr->d.list.size->maxCount);
			strcat(buffer, tmpBuf);
		    }
		}

	    }
	    strcat(buffer, " ]");
	    break;
	}
	ptr = ptr->next;
	if (ptr)
	    strcat(buffer, " ");
    }
}

/*****************************************************************************
 *
 * delete and free (complex) data entries
 *
 * Authors: Michael Vogt
 *
 * History: 28 Jun 1999, Michael Vogt, created
 *
 * Description: This function will (recursively) delete and free all
 * data entries associated with the passed pointer
 *
 ***************************************************************************/
static void deleteData(struct keyData *old)
{
    struct keyStrRng *tmp;
    struct keyData *next;

    /* always assume, that we are using lists, concatenated over the
       next pointer. delete all elements in the list */
    while(old)
    {
	/* delete specific data entry */
	switch (old->type)
	{
	  case PHCONF_KEY_TYPE_NUM_VAL:
	  case PHCONF_KEY_TYPE_NUM_RNG:
	    break;
	  case PHCONF_KEY_TYPE_STR_VAL:
	    if (old->d.strVal)
		free(old->d.strVal);
	    break;
	  case PHCONF_KEY_TYPE_STR_RNG:
	    /* the data entry is another list of elements of different
               type. walk through the list and delete all string entries */
	    while (old->d.strRng)
	    {
		tmp = old->d.strRng;
		old->d.strRng = tmp->next;
		if (tmp->entry)
		    free(tmp->entry);
		free(tmp);
	    }
	    break;
	  case PHCONF_KEY_TYPE_LIST:
	    /* the data entry starts a new list. recursively delete
               that list, which may consist of entries of any type */
	    deleteData(old->d.list.head);
/* Begin of Huatek Modification, Only Fang, 04/20/2002 */
/* Issue Number: 334 */
            if (old->d.list.size)
               free(old->d.list.size);
/* End of Huatek Modification */
	    break;
	}
	
	/* now, delete the current data entry structure but keep the
           pointer to the next element, if we are inside a list */
	next = old->next;
	free(old);
	old = next;
    }
}

/*****************************************************************************
 *
 * delete and free internal definition trees
 *
 * Authors: Michael Vogt
 *
 * History: 29 Jun 1999, Michael Vogt, created
 *
 * Description: This function will (recursively) delete and free all
 * definition entries associated with the passed tree pointer
 *
 ***************************************************************************/
static void deleteDefinition(struct keyedDefinition *tree)
{
    /* return immediately, if nothing more to delete */
    if (!tree)
	return;

    /* delete right and left subtree first */
    deleteDefinition(tree->left);
    deleteDefinition(tree->right);

    /* now delete the root of the tree */
    if (tree->hint)
	free(tree->hint);

    if (tree->key)
	free(tree->key);

    deleteData(tree->data);
/* Begin of Huatek Modification, Only Fang, 04/20/2002 */
/* Issue Number: 334 */
    free(tree);
/* End of Huatek Modification */
}

/*****************************************************************************
 *
 * walk through tree, looking for a specific entry
 *
 * Authors: Michael Vogt
 *
 * History: 28 Jun 1999, Michael Vogt, created
 *
 * Description: This function will (recursively) search an entry in
 * the passed tree pointer. It returns either a pointer to a pointer
 * to the found entry (if exists) or a pointer to a NULL pointer,
 * representing the place where a new entry with the given key should
 * be inserted into the tree. 
 *
 * This may work either, if the passed tree pointer is a pointer to
 * the NULL pointer (pointer to the empty tree). However, in the
 * calling function this case is handled first to be more intuitive.
 *
 ***************************************************************************/
static struct keyedDefinition **lookupTree(
    struct keyedDefinition **tree, 
    const char *key
)
{
    int comparison;

    /* searched entry is not inside the tree, return with a pointer to
       this NULL pointer, as a point where to enter the new entry into
       the tree */
    if (! *tree)
	return tree;

    /* compare current entry and either go on searching left or
       right. Return with a pointer to the current entry, if the key
       was found */
    comparison = strcmp((*tree)->key, key);
    if (comparison < 0)
	return lookupTree(&((*tree)->left), key);
    else if (comparison > 0)
	return lookupTree(&((*tree)->right), key);
    else
	return tree;
}

/*****************************************************************************
 *
 * Search a definition within a definition tree
 *
 * Authors: Michael Vogt
 *
 * History: 28 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct keyedDefinition *phConfSemSearchDefinition(
    struct defList         *tree       /* existing definition tree */, 
    const char             *key        /* entry key to search for */
)
{
    struct keyedDefinition **found;

    /* if the tree is empty, return immediately */
    if (!tree || !tree->tree || !key)
	return NULL;

    /* lookup the tree */
    found = lookupTree(&(tree->tree), key);

    /* return either pointer to found definition entry or NULL */
    return *found;
}

/*****************************************************************************
 *
 * Add a definition to a (possibly empty) definition tree
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct defList *phConfSemAddDefinition(
    struct defList         *tree       /* existing definition tree or NULL */, 
    struct keyedDefinition *entry      /* definition entry to add to tree */
)
{
    struct keyedDefinition **found;
    struct keyedDefinition **walk;

    /* if tree is empty so far, return the only new entry as the new
       tree (with only one element) */
    if (!entry)
	return tree;

    if (!tree || !(tree->tree))
    {
	if (!tree)
	    tree = allocTree();
	if (tree)
	{
	    tree->tree = entry;
	    tree->first = entry;
	    tree->last = entry;
	}
	return tree;
    }

    /* lookup a place, where the new entry is already stored or should
       be stored */
    found = lookupTree(&(tree->tree), entry->key);

    if (*found)
    {
	/* entry already exists, cut the existing entry out of the
           tree, replace it by the new one and delete the old entry */

	/* walk through linear list to find predecessor */
	walk = &(tree->first);
	while (*walk != *found)
	    walk = &((*walk)->next);

	/* cut found entry out of linear list structure */
	entry->next = (*found)->next;
	(*found)->next = NULL;
	if (tree->last == *found)
	    tree->last = entry;
	if (tree->first == *found)
	    tree->first = entry;
	*walk = entry;

	/* cut found entry out of tree structure */
	entry->left = (*found)->left;
	(*found)->left = NULL;
	entry->right = (*found)->right;
	(*found)->right = NULL;

	/* replace in tree structure, and delete old entry */
	deleteDefinition(*found);
	*found = entry;
    }
    else
    {
	/* entry does not exist so far, found points to the place
           where to enter the new entry into the tree structure */
	(*found) = entry;
	tree->last->next = entry;
	tree->last = entry;
	entry->next = NULL;
    }

    /* return the modified tree. */
    return tree;
}

/*****************************************************************************
 *
 * Create a definition
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct keyedDefinition *phConfSemMakeDefinition(
    char                   *hint       /* the optional descriptive hint */,
    char                   *key        /* the key string (without colon) */,
    phConfFlag_t           flagMask    /* binary ORed flags */,
    struct keyData         *data       /* data definition */
)
{
    struct keyedDefinition *newDefinition;

    /* allocate a definition entry. Initialize the entry or return
       immediately with NULL, if memory is not available */
    newDefinition = allocKeyedDefinition();
    if (newDefinition)
    {
	newDefinition->hint = hint;
	newDefinition->key = key;
	newDefinition->flagMask = flagMask;
	newDefinition->data = data;
	newDefinition->left = NULL;
	newDefinition->right = NULL;
	newDefinition->next = NULL;
    }

    /* return the new definition entry or NULL */
    return newDefinition;
}

/*****************************************************************************
 *
 * Delete a definition list
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
void phConfSemDeleteDefinition(
    struct defList *def       /* the definition(tree) to be deleted */
)
{
    /* return immediately, if nothing more to delete */
    if (!def)
	return;

    deleteDefinition(def->tree);
    free(def);
}

/*****************************************************************************
 *
 * Delete a definition entry
 *
 * Authors: Ulrich Frank
 *
 * History: 17 Jan 2000, Ulrich Frank, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
void phConfSemDeleteDefinitionKey(
    struct defList *tree,
    struct keyedDefinition *def /* the definition to be deleted */
)
{
    struct keyedDefinition *walk, *t, *c, *pre;

    /* return immediately, if nothing to delete */
    if (!def || !tree || !tree->tree) return;

    if (!lookupTree(&(tree->tree), def->key)) return;

    /* walk through linear list to find predecessor */
    walk = tree->first;
    while (walk && walk != def && walk->next != def)
	walk = walk->next;

    if (walk == NULL) return;

    walk->next = def->next;
    /* cut found entry out of linear list structure */
    if (tree->last == def) tree->last = walk;
    if (tree->first == def) tree->first = def->next;

    def->next = NULL;

    /* cut found entry out of tree structure */
    pre = tree->tree; walk = pre;
    while (walk && def != walk) {
	pre = walk;
	walk = strcmp(walk->key, def->key) < 0? walk->left : walk->right;
    }
    t = walk;
    if (t == NULL) return;

    if (!t->left) {
	if (t->right) walk = walk->right;
	else walk = NULL;
    }
    else if (!t->left->right) {
	walk = walk->left;
	walk->right = t->right;
    }
    else {
	c = walk->left;
	while (c->right->right) c = c->right;
	walk = c->right;
	c->right = walk->left;
	walk->right = t->right;
	walk->left = t->left;
    }

    if (tree->tree == def) {
	tree->tree = walk;
	if (walk == NULL) tree->first = tree->last = NULL;
    }
    else {
	if (walk == NULL) c = def;
	else c = walk;

	if (strcmp(pre->key, c->key) < 0) pre->left = walk;
	else if (strcmp(pre->key, c->key) > 0) pre->right = walk;
    }

    t->left = t->right = NULL;

    deleteDefinition(t);
}

/*****************************************************************************
 *
 * Create a default list size definition'
 *
 * Authors: Michael Vogt
 *
 * History: 01 Dec 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct listSize *phConfSemDefaultSize(void)
{
    struct listSize         *size;

    /* allocate a data entry.
       Initialize the entry or return
       immediately with NULL, if memory is not available */
    size = allocSizeData();
    if (size)
    {
	size->minCount = 0;
	size->maxCount = -1;
    }

    return size;    
}

/*****************************************************************************
 *
 * Create an empty data list
 *
 * Authors: Michael Vogt
 *
 * History: 13 Aug 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct keyData *phConfSemEmptyList(void)
{
    struct keyData         *listData;

    /* allocate a data entry.
       Initialize the entry or return
       immediately with NULL, if memory is not available */
    listData = allocKeyData();
    if (listData)
    {
	listData->type = PHCONF_KEY_TYPE_LIST;
	listData->d.list.head = NULL;
	listData->d.list.size = NULL;
	listData->next = NULL;
    }

    return listData;    
}

/*****************************************************************************
 *
 * Add a data entry to a data list
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct keyData *phConfSemAddData(
    struct keyData         *listData   /* existing data list or NULL */,
    struct keyData         *entry      /* data entry to add to list */
)
{
    /* allocate a data entry, if not already existent. 
       Initialize the entry or return
       immediately with NULL, if memory is not available */
    if (!listData)
    {
	listData = allocKeyData();
	if (listData)
	{
	    listData->type = PHCONF_KEY_TYPE_LIST;
	    listData->d.list.head = NULL;
	    listData->d.list.size = NULL;
	    listData->next = NULL;
	}
    }

    /* put new entry into existing list */
    if (listData)
	ListConcat(struct keyData, listData->d.list.head, entry);

    return listData;
}

/*****************************************************************************
 *
 * Create a data entry for a numerical value
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct keyData *phConfSemMakeNumVal(
    double                 value       /* the number value */
)
{
    struct keyData *newData;

    /* allocate a data entry. Initialize the entry or return
       immediately with NULL, if memory is not available */
    newData = allocKeyData();
    if (newData)
    {
	newData->type = PHCONF_KEY_TYPE_NUM_VAL;
	newData->d.numVal = value;
	newData->next = NULL;
    }

    /* return the new data entry  or NULL */
    return newData;
}

/*****************************************************************************
 *
 * Create a data entry for a numerical range definition
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct keyData *phConfSemMakeNumChoice(
    double                 lowBound    /* minimum value */,
    double                 highBound   /* maximum value */,
    enum numType           precision   /* the number's precision */
)
{
    struct keyData *newData;

    /* allocate a data entry. Initialize the entry or return
       immediately with NULL, if memory is not available */
    newData = allocKeyData();
    if (newData)
    {
	newData->type = PHCONF_KEY_TYPE_NUM_RNG;
	newData->d.numRng.lowBound = lowBound;
	newData->d.numRng.highBound = highBound;
	newData->d.numRng.precision = precision;
	newData->next = NULL;
    }

    /* return the new data entry  or NULL */
    return newData;
}

/*****************************************************************************
 *
 * Create a data entry for a string
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct keyData *phConfSemMakeStrVal(
    char                   *value      /* the string value */
)
{
    struct keyData *newData;

    /* allocate a data entry. Initialize the entry or return
       immediately with NULL, if memory is not available */
    newData = allocKeyData();
    if (newData)
    {
	newData->type = PHCONF_KEY_TYPE_STR_VAL;
	newData->d.strVal = value;
	newData->next = NULL;
    }

    /* return the new data entry  or NULL */
    return newData;
}

/*****************************************************************************
 *
 * Create or expand data entries for a choice of strings
 *
 * Authors: Michael Vogt
 *
 * History: 21 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to sematic.h
 *
 ***************************************************************************/
struct keyData *phConfSemMakeStrChoice(
    struct keyData         *choice     /* existing string choice or NULL */,
    char                   *entry      /* string entry to add to choice */
)
{
    struct keyStrRng *newRngEntry;

    /* allocate a new string entry for a string choice data
       type. Initialize the entry or return immediately with NULL, if
       memory is not available */
    newRngEntry = allocKeyStrRng();
    if (newRngEntry)
    {
	newRngEntry->entry = entry;
	newRngEntry->next = NULL;
    }
    else
	return NULL;

    /* if this is a new string choice, a data structure must be allocated */
    if (!choice)
    {
	/* allocate a data entry. Initialize the entry or return
           immediately with NULL, if memory is not available */
	choice = allocKeyData();
	if (choice)
	{
	    choice->type = PHCONF_KEY_TYPE_STR_RNG;
	    choice->d.strRng = NULL;
	    choice->next = NULL;
	}
	else
        {
            if (newRngEntry != NULL)
            {
                free(newRngEntry);
                newRngEntry = NULL;
            }
            return NULL;
        }
    }

    /* append the new string entry to the (possible empty) list of
       string entries of the data entry */
    ListConcat(struct keyStrRng, choice->d.strRng, newRngEntry);

    /* return the modified data entry */
    return choice;
}

/*****************************************************************************
 *
 * Write a definition to a string
 *
 * Authors: Michael Vogt
 *
 * History: 28 Jun 1999, Michael Vogt, created
 *
 * Description:
 * Please refer to sematic.h
 *
 ***************************************************************************/
char *phConfSemWriteDef(
    struct keyedDefinition *entry       /* definition entry to write */
)
{
    int size;
    int allocSize;
    char *buffer;

    /* pure data portion */
    size = sizeofData(entry->data);

    /* key and key formating space: */
    size += strlen(entry->key) + 80;

    /* flags space, built for the future */
    size += 80;

    /* round up space to next power of 2 */
    allocSize = 1;
    while (size > 0)
    {
	allocSize <<= 1;
	size >>= 1;
    }

    /* finally get the space */
    buffer = provideStaticTmpSpace(allocSize);

    /* now write the definition: */

    /* the key... */
    sprintf(buffer, "%s: ", entry->key);

    /* ... some flags ... */
    if (entry->flagMask != PHCONF_FLAG_EMPTY)
    {
	strcat(buffer, "< ");

	if (entry->flagMask & PHCONF_FLAG_FIXED)
	    strcat(buffer, "F ");

	strcat(buffer, "> ");
    }

    /* ... fill up to column 40 or start another line ... */
    if (strlen(buffer) <= 40)
	while (strlen(buffer) < 40)
	    strcat(buffer, " ");
    else
	strcat(buffer, "\n                                        ");

    /* ... and the data definition */
    phConfSemCatDataText(buffer, entry->data, 40);

    return buffer;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
