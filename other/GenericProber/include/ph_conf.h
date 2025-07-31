/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_conf.h
 * CREATED  : 21 May 1999
 *
 * CONTENTS : Handler Configuration Management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 21 May 1999, Michael Vogt, created
 *            19 Jul 1999, Michael Vogt, added phConfConfIfDef
 *            20 Jul 1999, Michael Vogt, added configuration key iterator
 *            22 Jul 1999, Michael Vogt, added retrieval of config types
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

#ifndef _PH_CONF_H_
#define _PH_CONF_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

//#ifdef __cplusplus
//extern "C" {
//#endif


/* Handle for configurations */
typedef struct phConfConfStruct *phConfId_t;
typedef struct phConfAttrStruct *phAttrId_t;

typedef enum {
    PHCONF_ERR_OK = 0      /* no error */,
    PHCONF_ERR_NOT_INIT    /* initialization was not done with a valid
			      message logger ID so far */,
    PHCONF_ERR_INVALID_HDL /* the passed ID is not valid */,
    PHCONF_ERR_MEMORY      /* couldn't get memory from heap with malloc() */,
    PHCONF_ERR_PARSE       /* parse errors in configuration or
			      attributes file */,
    PHCONF_ERR_UNDEF       /* definition for the given key does not exist */,
    PHCONF_ERR_TYPE        /* the accessed entry is of different type
			      then requested */,
    PHCONF_ERR_INDEX       /* one of the index entries for the list
			      retrieval is out of range (one of the
			      lists has less entries than specified,
			      or the entry is not even a list) */,
    PHCONF_ERR_FILE        /* the specified file name can not be
			      accessed or the file permissions don't allow 
			      the access */,
    PHCONF_ERR_VALIDATION  /* the configuration is not valid according
			      to the attributes definition */,
    PHCONF_ERR_MERGE       /* a configuration entry was not merged due
			      to set flag values */,
    PHCONF_ERR_INTERNAL    /* an internal error in the data structures occured.
			      this is a BUG ! */
} phConfError_t;

/* Attention for the following definition: don't use the upper most
   bit, since it is removed during conversions to type (long) */

typedef enum {
    PHCONF_TYPE_NUM       = 0x00000001 /* number type definitions */, 
    PHCONF_TYPE_STR       = 0x00000002 /* string type definitions */,
    PHCONF_TYPE_LIST      = 0x00000004 /* an array (a list) */,

    PHCONF_TYPE_INUM_RNG  = 0x00010000 /* integral number range */,
    PHCONF_TYPE_FNUM_RNG  = 0x00020000 /* floating point number range */,
    PHCONF_TYPE_STR_CHC   = 0x00040000 /* choice of strings */,

    PHCONF_TYPE_ALL       = 0x7fffffff /* apply on all data types */
} phConfType_t;

typedef enum {
    PHCONF_FLAG_EMPTY     = 0x00000000 /* only apply, if no flags are set */,
    PHCONF_FLAG_FIXED     = 0x00000001 /* apply on definitions with
					  'F' flag set */,
    PHCONF_FLAG_DONTCARE  = 0x7fffffff /* apply on all flag type definitions */
} phConfFlag_t;

typedef enum { 
    PHCONF_PANEL_FLAGS    = 0x00000001, /* show the flags panel         */
    PHCONF_PANEL_DELETE   = 0x00000002, /* show the delete panel        */
    PHCONF_PANEL_EDIT     = 0x00000004  /* show the enhanced edit panel */
} phPanelFlag_t;

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
 * Initialize the configuration manager
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function must be called before using any other part of the
 * configuration manager. The internal data structures are
 * initialized. The passed data logger will be used to log all further
 * actions and possible error situations.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfInit(
    phLogId_t logger         /* the data logger to be used */
);

/*****************************************************************************
 *
 * Read configuration from file
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * A complete configuration setup is read from the passed
 * configuration file. During read, the definitions are checked
 * against syntactical errors. Error messages are reported with the
 * help of the data logger defined with phConfInit(3). If an error
 * occurs, no valid configuration ID is passed back to the calling
 * function.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfReadConfFile(
    phConfId_t *configID     /* the resulting configuration ID */,
    const char *fileName     /* path to configuration file */
);

/*****************************************************************************
 *
 * Get first definition key (Iterator)
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Retrieves the name of the key for the first configuration definition
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfConfFirstKey(
    phConfId_t configID      /* the configuration ID */,
    const char **key         /* the key of the first definition entry */
);

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
);

/*****************************************************************************
 *
 * Write configuration to file
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Write a (portion of a) configuration to a file. An important role
 * plays the selection parameter. It is used to define, which types of
 * definitions should be written to the file. E.g. it could be of
 * interest to hide the fixed definitions, because they should not be
 * changeable anyway.
 *
 * Example to write all but the fixed definitions:
 *
 * phConfWriteConfFile(.., .., PHCONF_FLAG_DONTCARE & ~PHCONF_FLAG_FIXED)
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfWriteConfFile(
    phConfId_t configID      /* the configuration ID */,
    const char *fileName     /* path to configuration file */,
    phConfFlag_t selection   /* selection of definitions, which should
				be written to file */
);

/*****************************************************************************
 *
 * Write definition to string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Write a definition to a string. 
 * Example to write a specific definition to a string...
 *
 * phConfWriteConfFile(..., "temperature", &def)
 *
 * ... may result in a string like: def = "temperature: 42"
 *
 * Warning: Do not free or manipulate any data portion returned by the
 * function. data remains valid until next call to the configuration
 * module.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfGetConfDefinition(
    phConfId_t configID      /* the configuration ID */,
    const char *key          /* the key to write the definition for */,
    char **result            /* pointer to resulting definition string */
);

/*****************************************************************************
 *
 * Write definition to string (value only)
 *
 *
 * Description:
 *
 * Write a definition value to a string.
 * Example to write a specific definition to a string...
 *
 * phConfWriteConfFile(..., "temperature", &def)
 *
 * ... may result in a string like: def = "42"
 *
 * Warning: Do not free or manipulate any data portion returned by the
 * function. data remains valid until next call to the configuration
 * module.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfGetConfDefinitionValue(
    phConfId_t configID      /* the configuration ID */,
    const char *key          /* the key to write the definition for */,
    char **result            /* pointer to resulting definition string */
);

/*****************************************************************************
 *
 * Read configuration from string
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Similar to phConfReadConfFile(3), but reads configuration from a
 * '\0' terminated string. This function may be used by UIs for
 * interactive configuration.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfSetConfDefinition(
    phConfId_t *configID     /* the resulting configuration ID */,
    char *confString         /* string with configuration data */
);

/*****************************************************************************
 *
 * Read attributes from file
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * A complete attributes setup is read from the passed
 * attributes file. During read, the definitions are checked
 * against syntactical errors. Error messages are reported with the
 * help of the data logger defined with phConfInit(3). If an error
 * occurs, no valid attributes ID is passed back to the calling
 * function.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfReadAttrFile(
    phAttrId_t *attributesID /* the resulting attributes ID */,
    const char *fileName     /* path to configuration file */
);

/*****************************************************************************
 *
 * Validate a configuration
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Each definition included in the passed configuration is validated
 * against the passed attributes. The configuration may contain less
 * definitions then the attributes. Each definition within the
 * configuration must have a counterpart in the attributes
 * definitions. For numerical values, the range of the value is
 * checked. For string values, the possible strings are checked
 * against all defined attribute options. For array values, all
 * entries of the array are checked against the attributes.
 *
 * Note: A configuration should always be validated before use or merge.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfValidate(
    phConfId_t configID      /* configuration to validate */,
    phAttrId_t attributesID  /* attributes to validate against */
);

/*****************************************************************************
 *
 * Merge two configurations into one
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * All definitions found in the source configuration are merged into
 * the target configuration. For internal efficiency reasons, the
 * source configuration becomes invalid after the merge (as if
 * destroyed with phConfDestroy(3)).
 *
 * During merge, the key of each definition is checked. If a source
 * key already exists in the target configuration, the corresponding
 * definition is replaced. If the key does not exist so far, it is
 * created for the target configuration.
 *
 * If definitions are not merged due to the defined selection, a
 * message is logged and an error code is produced. The target
 * configuration will only contain changes for the selected definition
 * types.
 *
 * Always ensure, that only valid configurations are used for merge
 * (see phConfValidate(3)).
 *
 * Returns: error code.
 *
 *****************************************************************************/

phConfError_t phConfMerge(
    phConfId_t targetConfID  /* the configuration to merge into */,
    phConfId_t sourceConfID  /* the configuration to be merged */, 
    phConfFlag_t selection   /* selection of definitions to merge */
);

/*****************************************************************************
 *
 * Get attribute's type and nesting count
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Attribute definitions may be nested lists (of lists (of lists
 * (...))). This function returns the number of nesting levels. E.g. a
 * value of 1 is returned in <nesting>, if it's a simple one
 * dimensional list. 0 is returned, if it is no list at all.
 *
 * Attributes may be either number ranges, string choices or a
 * possible nested list of these two types. Different types may not be
 * mixed within one attribute definition. This function returns the
 * data type of the inner most level (of a possibly nested list
 * structure) of an attribute definition in <theType>. 
 *
 * Note: <theType> can be one of PHCONF_TYPE_INUM_RNG,
 * PHCONF_TYPE_FNUM_RNG, PHCONF_TYPE_STR_CHC only.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfAttrType(
    phAttrId_t attributesID  /* attributes ID */,
    char *key                /* the key of the definition to look for */,
    phConfType_t *theType    /* the type is returned here */,
    int *nesting             /* the deepest nesting for this attribute
				definition */
);

/*****************************************************************************
 *
 * Get an attribute's number range 
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * If the given key describes a number range or a (nested) list of
 * number ranges, this function returns the bounds of the number
 * range.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfAttrNumRange(
    phAttrId_t attributesID  /* attributes ID */,
    char *key                /* the key of the definition to look for */,
    double *lowerBound       /* the resulting lower bound of the
				number definition */,
    double *upperBound       /* the resulting upper bound of the
				number definition */
);

/*****************************************************************************
 *
 * Get an attribute's string choice
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * If the given key describes a string choice or a (nested) list of
 * string choices, this function returns a NULL terminated array of
 * string pointers, describing the choices.
 *
 * Warning: Do not free or manipulate any data portion returned by the
 * function. data remains valid until next call to the configuration
 * module.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfAttrStrChoice(
    phAttrId_t attributesID  /* attributes ID */,
    char *key                /* the key of the definition to look for */,
    char ***choices          /* resulting pointer to array of string
				pointers, NULL terminated list */
);


/*****************************************************************************
 *
 * Check availability of a specific key
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * If the given key is defined, the function returns without error,
 * otherwise it silently reports an PHCONF_ERR_UNDEF error code. This
 * may be used to check for keys that are optional, without producing
 * error messages.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfConfIfDef(
    phConfId_t configID      /* configuration ID */,
    const char *key          /* the key of the definition to look for */
);

/*****************************************************************************
 *
 * Get a configuration's number entry
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function retrieves a number entry assoziated with a passed
 * configuration key. The key may represent a list (or a list of
 * lists, etc). To specify the entry position within a list, the
 * parameters <dimension> and <indexField> must be used. <dimension>
 * must match the dimension (the nesting) of the specific list entry.
 * The <indexField> parameter allows the navigation through the list
 * structure. It denotes the index numbers within the nested
 * lists. The <indexField> must point to an integer array, which has
 * at least <dimension> entries. If <dimension> is set to 0 (for
 * definitions without list structure), the <indexField> may be
 * NULL. See the examples sections for more information.
 *
 * Examples:
 *
 * Assuming two configuration entries with the keys "key1" and "key2"
 * like the following
 *
 * { ... } key1: 42
 *
 * { ... } key2: [ 15 1 98 [ 17 9 65 ] [ 19 4 71 [ 0 ] ] ]
 *
 * The following parameters must be used to retrieve specific entries
 *
 * key = "key1", dimension = 0, indexField = [ ]       -> 42
 *
 * key = "key2", dimension = 1, indexField = [ 0 ]     -> 15
 *
 * key = "key2", dimension = 1, indexField = [ 1 ]     -> 1
 *
 * key = "key2", dimension = 1, indexField = [ 2 ]     -> 98
 *
 * key = "key2", dimension = 2, indexField = [ 3 0 ]   -> 17
 *
 * key = "key2", dimension = 2, indexField = [ 3 1 ]   -> 9
 *
 * key = "key2", dimension = 2, indexField = [ 3 2 ]   -> 65
 *
 * key = "key2", dimension = 2, indexField = [ 4 0 ]   -> 19
 *
 * key = "key2", dimension = 2, indexField = [ 4 1 ]   -> 4
 *
 * key = "key2", dimension = 2, indexField = [ 4 2 ]   -> 71
 *
 * key = "key2", dimension = 3, indexField = [ 4 3 0 ] -> 0
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfConfNumber(
    phConfId_t configID      /* configuration ID */,
    const char *key          /* the key of the definition to look for */,
    int dimension            /* the requested nesting and the array
				size of <indexField> */,
    int *indexField          /* index array to access nested list entries */,
    double *value            /* the resulting numeric value */
);

/*****************************************************************************
 *
 * Get a configuration's string entry
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function retrieves a 'string' entry assoziated with a passed
 * configuration key. The key may represent a list (or a list of
 * lists, etc). To specify the entry position within a list, the
 * parameters <dimension> and <indexField> must be used. <dimension>
 * must match the dimension (the nesting) of the specific list entry.
 * The <indexField> parameter allows the navigation through the list
 * structure. It denotes the index numbers within the nested
 * lists. The <indexField> must point to an integer array, which has
 * at least <dimension> entries. If <dimension> is set to 0 (for
 * definitions without list structure), the <indexField> may be
 * NULL. See the examples sections of phConfConfNumber(3) for more
 * information.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfConfString(
    phConfId_t configID      /* configuration ID */,
    const char *key          /* the key of the definition to look for */,
    int dimension            /* the requested nesting and the array
				size of <indexField> */,
    int *indexField          /* index array to access nested list entries */,
    const char **value       /* pointer to resulting string value */
);

/*****************************************************************************
 *
 * Get a configuration's type
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function retrieves the type of an entry assoziated with a passed
 * configuration key. The key may represent a list (or a list of
 * lists, etc). To specify the entry position within a list, the
 * parameters <dimension> and <indexField> must be used. <dimension>
 * must match the dimension (the nesting) of the specific list entry.
 * The <indexField> parameter allows the navigation through the list
 * structure. It denotes the index numbers within the nested
 * lists. The <indexField> must point to an integer array, which has
 * at least <dimension> entries. If <dimension> is set to 0 (for
 * definitions without list structure), the <indexField> may be
 * NULL. See the examples sections of phConfConfNumber(3) for more
 * information.
 *
 * Returns: error code
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
);

/*****************************************************************************
 *
 * Test a string configuration value
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * This function checks a simple string configuration value (no lists
 * allowed) against a set of possible values. The comparison is done
 * case independent. This function is very usefull to check the
 * existence of a configuration value and the value itself in a very
 * easy way. E.g. to find out whether the configuration key "online"
 * was set to either "yes" or "no", use:
 *
 * error = phConfConfStrTest(&found, confId, "yes", "no", NULL);
 *
 * In this example a configured value of "yes" would set found to 1,
 * "no" would set found to 2, any other (unspecified value) would set
 * found to 3. In case the key "online" is not configured, or in any
 * other error condition, found is set to 0 and the correct error code
 * is returned (e.g. PHCONF_ERR_UNDEF if "online" is not configured,
 * or PHCONF_ERR_TYPE if "online" is not a simple string value).
 *
 * Very Important Note:
 * Don't forget to end the list of variable arguments with NULL
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
);

    
/*****************************************************************************
 *
 * Destroy the passed configuration.
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Destroy the passed configuration.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfDestroyConf(
    phConfId_t configID      /* configuration ID */
);

/*****************************************************************************
 *
 * Destroy the passed attributes.
 *
 * Authors: Michael Vogt
 *
 * Description:
 *
 * Destroy the passed attributes.
 *
 * Returns: error code
 *
 *****************************************************************************/

phConfError_t phConfDestroyAttr(
    phAttrId_t attributesID  /* attributes ID */
);

/*****************************************************************************
 *
 * Edit a configuration
 *
 * Authors: Ulrich Frank
 *
 * Description:
 *
 * Returns: pointer to the internal definition string
 *
 ***************************************************************************/
phConfError_t phConfEditConfDef(
    phConfId_t id,
    phAttrId_t attributesID, /* attributes to validate against */
    phConfFlag_t selection,  /* selection of definitions, which should
				be shown in gui */
    phPanelFlag_t panelflags,
    int *response            /* 1 if ok is pressed, 0 if cancel */
    );

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
void phConfFreeStatic(void); 
/*End of Huatek Modifications*/

//#ifdef __cplusplus
//}
//#endif

#endif /* ! _PH_CONF_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
