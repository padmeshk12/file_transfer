/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : gpib_conf.h
 * CREATED  : 10 Dec 1999
 *
 * CONTENTS : Configuration of GPIB specific parts
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 10 Dec 1999, Michael Vogt, created
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

#ifndef _GPIB_CONF_H_
#define _GPIB_CONF_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

struct gpibStruct{
    int                     dummy;     /* DUMMY GPIB KEY */
};

struct gpibFlagsStruct{
    int dummyDeflt;     /* DUMMY GPIB KEY */
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
 * Reconfigure plugin specific GPIB definitions
 *
 * Authors: Michael Vogt
 *
 * Description: On initialization and when the configuration has
 * changed, the configuration data must be read and the driver plugin
 * must be reconfigured. This function performs all necessary steps
 * for GPIB specific parts of the driver.
 * 
 * Notes: The function will print any error messages and warnings
 *
 * Returns: 0 in fatal situations, 1 on success
 *
 ***************************************************************************/

int phFuncGpibReconfigure(
    struct phPFuncStruct *myself         /* the handle of the plugin */
);

#endif /* ! _GPIB_CONF_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
