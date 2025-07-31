/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : stepcontrol.h
 * CREATED  : 12 Jan 2000
 *
 * CONTENTS : Control of die and subdie stepping pattern
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 12 Jan 2000, Michael Vogt, created
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

#ifndef _STEPCONTROL_H_
#define _STEPCONTROL_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

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
 * Initialize the step control
 *
 * Authors: Michael Vogt
 *
 * Description: This function reads some configuration values to learn
 * about hwich mode the driver is working in (master, slave or
 * learnlist), to read the wafer description file (if necessary), to
 * read a sub-die stepping pattern from the configuration, if
 * necessary, and to initialize its internal data structures for the
 * die on wafer representation.
 *
 ***************************************************************************/
void phFrameStepControlInit(
    struct phFrameStruct *f             /* the framework data */
);

void phFrameStepResetPattern(
    phStepPattern_t *sp
);

int phFrameStepDoStep(
    phStepPattern_t *sp,
    struct phFrameStruct *f             /* the framework data */    
);

int phFrameStepSnoopStep(
    phStepPattern_t *sp,
    struct phFrameStruct *f             /* the framework data */    
);



#endif /* ! _STEPCONTROL_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
