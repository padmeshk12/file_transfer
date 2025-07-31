/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_conf_private.h
 * CREATED  : 29 Jun 1999
 *
 * CONTENTS : Implementation of Configuration Management
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Jun 1999, Michael Vogt, created
 *            20 Jul 1999, Michael Vogt, added configuration key iterator
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

#ifndef _PH_CONF_PRIVATE_H_
#define _PH_CONF_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#ifdef __C2MAN__
#include "ph_log.h"
#include "ph_conf.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_2

/*--- typedefs --------------------------------------------------------------*/

struct phConfConfStruct
{
    struct phConfConfStruct *myself;
    struct defList *conf;
    struct keyedDefinition *current;
};

struct phConfAttrStruct
{
    struct phConfAttrStruct *myself;
    struct defList *attr;
    struct keyedDefinition *current;
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
/* Begin of Huatek Modificationi, Donnie Tu, 12/07/2001 */
/* Issue Number: 324 */
/* delete the declare for function readFile */

/* End of Huatek Modification */
/*****************************************************************************
 *
 * validate a single configuration entry
 *
 * Authors: Michael Vogt
 *
 * Description: A single configuration data entry is validated against
 * a single attributes entry. Both entries may be lists (or nested
 * lists) according to the syntactic restrictions that attribute entry
 * lists only contain one element.
 *
 * Returns: 1 if configuration data is valid, 0 else
 *
 *****************************************************************************/
int validateDefinition(
    struct keyData *confData,
    struct keyData *attrData
);

/*****************************************************************************
 *
 * access a list entry
 *
 * Authors: Michael Vogt
 *
 * Description: This function retrieves the pointer to an entry of a
 * nested list structure according to the definition of
 * phConfConfNumber(3)
 *
 * See Also: phConfConfNumber(3)
 *
 * Returns: the found entry or NULL, if list structure does not fit
 * the index values.
 *
 *****************************************************************************/
/* Begin of Huatek Modificationi, Donnie Tu, 12/07/2001 */
/* Issue Number: 324 */
/*static struct keyData *walkThroughLists(
*    struct keyData *data, 
*    int dim, 
*    int *field
);*/
/* End of Huatek Modification */

#endif /* ! _PH_CONF_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
