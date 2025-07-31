/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : semantic.h
 * CREATED  : 26 May 1999
 *
 * CONTENTS : Common semantic actions for configurations and attributes
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 May 1999, Michael Vogt, created
 *
 * Instructions:
 *
 * 1) Copy this template to as many .h files as you require
 *
 * 2) Use the command 'make depend' to make the new
 *    source files visible to the makefile utility
 *
 * 3) To support automatic man page (documentation) generation, follow the
 *    instructions given for the function template below.
 * 
 * 4) Put private functions and other definitions into separate header
 * files. (divide interface and private definitions)
 *
 *****************************************************************************/

#ifndef _SEMANTIC_H_
#define _SEMANTIC_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#ifdef __C2MAN__
#include "ph_conf.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

enum keyType
{
    PHCONF_KEY_TYPE_NUM_VAL            /* data entry is a number */,
    PHCONF_KEY_TYPE_NUM_RNG            /* data entry is a number range */,
    PHCONF_KEY_TYPE_STR_VAL            /* data entry is a string */,
    PHCONF_KEY_TYPE_STR_RNG            /* data entry is a choice of strings */,
    PHCONF_KEY_TYPE_LIST               /* data entry is a list of entries */
};

enum numType
{
    PHCONF_NUM_TYPE_INT                /* number is of integral type */,
    PHCONF_NUM_TYPE_FLOAT              /* number is of floating point type */
};

struct keyNumRng
{
    double                 lowBound;   /* minimum value */
    double                 highBound;  /* maximum value */
    enum numType           precision;  /* the number's precision */
};

struct keyStrRng
{
    char                   *entry;     /* this string choice */
    struct keyStrRng       *next;      /* next string choice */
};

struct listSize
{
    int                    minCount;   /* minimum number of entries, */
    int                    maxCount;   /* minimum number of entries, 
					  -1 refers to no upper limit */
};

struct keyList
{
    struct keyData         *head;      /* head of a data list */
    struct listSize        *size;      /* size of lists for attributes */
};

struct keyData 
{
    enum keyType           type;       /* type of this key data entry */
    union
    {
	double             numVal;     /* one specific number */
	struct keyNumRng   numRng;     /* range of number values */
	char               *strVal;    /* one specific string */
	struct keyStrRng   *strRng;    /* choice of strings */
	struct keyList     list;       /* a data list */
    }                      d;          /* data part of this key data */

    struct keyData         *next;      /* concat entries within a list */
};

struct keyedDefinition 
{
    char                   *hint;      /* the optional descriptive hint */
    char                   *key;       /* the key string (without colon) */
    phConfFlag_t           flagMask;   /* binary ORed flags */
    struct keyData         *data;      /* data definition */
    struct keyedDefinition *left;      /* search tree, smaller entries */
    struct keyedDefinition *right;     /* search tree, bigger entries */
    struct keyedDefinition *next;      /* linear list, ordered as read in */
};

struct defList
{
    struct keyedDefinition *tree;       /* entry point into search tree */
    struct keyedDefinition *first;      /* first entry (in time) */
    struct keyedDefinition *last;       /* last entry (in time) */
};

/*--- external variables ----------------------------------------------------*/

/*--- external function -----------------------------------------------------*/

/*****************************************************************************
 * To allow a consistent interface definition and documentation, the
 * documentation is automatically extracted from the comment section
 * of the below function declarations. All text within the comment
 * region just in front of a function header will be used for
 * documentation. Additional text, which should not be visible in the
 * documentation (like this text), must be put in a separate comment
 * region, ahead of the documentation comment and separated by at
 * least one blank line.
 *
 * To fill the documentation with life, please fill in the angle
 * bracketed text portions (<>) below. Each line ending with a colon
 * (:) or each line starting with a single word followed by a colon is
 * treated as the beginning of a separate section in the generated
 * documentation man page. Besides blank lines, there is no additional
 * format information for the resulting man page. Don't expect
 * formated text (like tables) to appear in the man page similar as it
 * looks in this header file.
 *
 * Function parameters should be commented immediately after the type
 * specification of the parameter but befor the closing bracket or
 * dividing comma characters.
 *
 * To use the automatic documentation feature, c2man must be installed
 * on your system for man page generation.
 *****************************************************************************/

/*****************************************************************************
 *
 *
 * Authors: Michael Vogt
 *
 * Description: 
 * 
 * Returns: 
 *
 ***************************************************************************/
void phConfSemCatDataText(
    char *buffer,
    struct keyData *ptr,
    int indent
    );

/*****************************************************************************
 *
 * Search a definition within a definition tree
 *
 * Authors: Michael Vogt
 *
 * Description: 
 * 
 * The passed tree is searched for an enry with the given key. If
 * found, a pointer to the entry is returned. Otherwise NULL is
 * returned. The passed tree may be NULL;
 *
 * Returns: NULL or pointer to found entry
 *
 ***************************************************************************/
struct keyedDefinition *phConfSemSearchDefinition(
    struct defList         *tree       /* existing definition tree */, 
    const char             *key        /* entry key to search for */
);

/*****************************************************************************
 *
 * Add a definition to a (possibly empty) definition tree
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The <entry> is inserted into the search tree <tree>. Sort criteria
 * is the key field within the <entry> structure. If an entry with an
 * equal key already exists, it is replaced by the new entry. The
 * replaced entry will be destroyed (no external references should
 * point to tree entries in general).
 *
 * To start a new tree, the <tree> parameter may be NULL.
 *
 * Returns: pointer to modified tree (possibly new root)
 *
 ***************************************************************************/
struct defList *phConfSemAddDefinition(
    struct defList         *tree       /* existing definition tree or NULL */, 
    struct keyedDefinition *entry      /* definition entry to add to tree */
);

/*****************************************************************************
 *
 * Create a definition
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Based on the parameters of this function, a new definition is
 * created. The passed parameters are reused, i.e. no memory
 * allocation or memory free operations are performed on the
 * parameters. The calling function should not keep references to the
 * parameter data.
 *
 * Returns: pointer to new created definition structure
 *
 ***************************************************************************/
struct keyedDefinition *phConfSemMakeDefinition(
    char                   *hint       /* the optional descriptive hint */,
    char                   *key        /* the key string (without colon) */,
    phConfFlag_t           flagMask    /* binary ORed flags */,
    struct keyData         *data       /* data definition */
);

/*****************************************************************************
 *
 * Delete a definition set
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The passed definition is deleted and the associated memory is freed.
 *
 ***************************************************************************/
void phConfSemDeleteDefinition(
    struct defList *def                /* the definition(tree) to be deleted */
);

/*****************************************************************************
 *
 * Delete a definition entry
 *
 * Authors: Ulrich Frank
 *
 * History: 17 Jan 2000, Ulrich Frank, created
 *
 * Description: 
 *
 * The passed definition entry is deleted and the associated memory is freed.
 *
 ***************************************************************************/
void phConfSemDeleteDefinitionKey(
    struct defList *tree,
    struct keyedDefinition *def /* the definition to be deleted */
    );

/*****************************************************************************
 *
 * Create a default list size definition
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Create an empty data list size definition
 *
 * Returns: pointer to empty size definition
 *
 ***************************************************************************/
struct listSize *phConfSemDefaultSize(void);

/*****************************************************************************
 *
 * Create an empty data list
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Create an empty data list
 *
 * Returns: pointer to empty data entry list
 *
 ***************************************************************************/
struct keyData *phConfSemEmptyList(void);

/*****************************************************************************
 *
 * Add a data entry to a data list
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * The passed data <entry> is added to the end of the passed data
 * entry <list>. The <list> parameter may be NULL. This can be used to
 * create new list out of the passed data <entry>.
 *
 * Returns: pointer to modified data entry list
 *
 ***************************************************************************/
struct keyData *phConfSemAddData(
    struct keyData         *listData   /* existing data list or NULL */,
    struct keyData         *entry      /* data entry to add to list */
);

/*****************************************************************************
 *
 * Create a data entry for a numerical value
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * A new data entry is created. The new entry holds a numerical
 * value. The value is set to the passed <value> parameter.
 *
 * Returns: pointer to new data entry
 *
 ***************************************************************************/
struct keyData *phConfSemMakeNumVal(
    double                 value       /* the number value */
);

/*****************************************************************************
 *
 * Create a data entry for a numerical range definition
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * A new data entry is created. The new entry holds a numerical
 * range definition. The range is set according to the passed parameters.
 *
 * Returns: pointer to new data entry
 *
 ***************************************************************************/
struct keyData *phConfSemMakeNumChoice(
    double                 lowBound    /* minimum value */,
    double                 highBound   /* maximum value */,
    enum numType           precision   /* the number's precision */
);

/*****************************************************************************
 *
 * Create a data entry for a string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * A new data entry is created. The new entry holds a string value.
 * The string is set according to the passed <value>. The string is
 * not copied but re-used. Do not externally free(3) the storage area
 * of the string.
 *
 * Returns: pointer to new data entry or NULL if memory is not available
 *
 ***************************************************************************/
struct keyData *phConfSemMakeStrVal(
    char                   *value      /* the string value */
);

/*****************************************************************************
 *
 * Create or expand data entries for a choice of strings
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * A new <entry> is added to a choice of strings. The <choice> may be
 * NULL, which causes a new choice to be created. The string <entry>
 * is not copied but re-used. Do not externally free(3) the storage
 * area of the string.
 *
 * Returns: pointer to new data entry or NULL if memory is not available
 *
 ***************************************************************************/
struct keyData *phConfSemMakeStrChoice(
    struct keyData         *choice     /* existing string choice or NULL */,
    char                   *entry      /* string entry to add to choice */
);

/*****************************************************************************
 *
 * Write a definition to a string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * As a counterpart to the parser, this function creates a string that
 * represents the passed definition entry. The result is written to an
 * internal string area. It may be copied but not modified or freed.
 *
 * Returns: pointer to the internal definition string
 *
 ***************************************************************************/
char *phConfSemWriteDef(
    struct keyedDefinition *entry       /* definition entry to write */
);

#endif /* ! _SEMANTIC_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
