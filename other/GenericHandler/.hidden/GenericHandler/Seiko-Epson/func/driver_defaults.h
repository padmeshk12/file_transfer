/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : driver_defaults.h
 * CREATED  : 06 Aug 1999
 *
 * CONTENTS : Defaults and use of Configuration
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 02 Jan 2001, Michael Vogt
 *                         Chris Joyce, modified 
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

#ifndef _DRIVER_DEFAULTS_H_
#define _DRIVER_DEFAULTS_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "gpib_conf.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

enum handlerModel
{
    /* list of known handlers, not all of them are supported but this
       is defined in driver_defaults.c !!! */
    PHFUNC_MOD_NS5000           /* HM/NS Series handler */,
    PHFUNC_MOD_NS6000           /* HM/NS Series handler */,
    PHFUNC_MOD_NS7000           /* HM/NS Series handler */,
    PHFUNC_MOD_HONTECH          /* HonTech Handler. 
                                 * Added for CR-89175. 
                                 * HonTech handler is compatiable with
                                 * the Seiko-Epson handler, so in order to
                                 * save more room for the already crowded
                                 * handler driver family, we will just
                                 * re-use the Seiko-Epson driver and add
                                 * extra HonTech feature in this driver */,
    PHFUNC_MOD_NS6040           /* HM/NS Series handler */,
    PHFUNC_MOD_NS8040           /* HM/NS Series handler */,
    PHFUNC_MOD_CHROMA           /* Chroma 3180 Handler */,
    PHFUNC_MOD_NS8000           /* HM/NS Series handler*/,
    
    /* delete next entry, if not applicable */
    /* PHFUNC_MOD_GENERIC, */
    /* don't change next line ! */
    PHFUNC_MOD__END
};

struct modelDef
{
    enum handlerModel model;
    const char *name;
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
 * Get supported handler models
 *
 * Authors: Michael Vogt
 *
 * Description: This function returns a pointer to an array of valid
 * handler models supported by this plugin. It is only used for plugin
 * internal things and checked at plugin initialization.
 *
 * To define a handler specific plugin, just edit the file
 * driver_defaults.c to determine the above described selection.
 * 
 ***************************************************************************/

void phFuncPredefModels(
    char **family                       /* the accepted family name */,
    struct modelDef **models            /* the list of accepted models */
);

#endif /* ! _DRIVER_DEFAULTS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
