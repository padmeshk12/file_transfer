/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : GuiCallbacks.h
 * CREATED  : 06 Dec 1999
 *
 * CONTENTS : generic callbackss for the gui components
 *
 * AUTHORS  : Ulrich Frank, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 06 Dec 1999, Ulrich Frank, created
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

#ifndef _GUICALLBACKS_H_
#define _GUICALLBACKS_H_

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
 * <give a SINGLE LINE function description here>
 *
 * Authors:  Ulrich Frank, SSTD-R&D
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * <Blank lines, like the one just ahead, may be used to separate paragraphs>
 *
 * Input parameter format: 
 * <Describe the format of the input parameter string here>
 *
 * Output result format:
 * <Describe the format of the output parameter string here>
 * 
 * Warnings: <delete this, if not used>
 *
 * Notes: <delete this, if not used>
 *
 * Returns: <the function's return value, delete this if void>
 *
 ***************************************************************************/
/* Begin of Huatek Modification, Amy He, 03/04/2005 */
/* CR Number: 16324 */
#define PHLIB_ROOT_PATH  "/tmp"
/* End of Huatek Modification */

void PushButtonCallback(Widget w, XtPointer clientData, XtPointer callData);
void ToggleButtonCallback(Widget w, XtPointer clientData, XtPointer callData);
void FileBrowserCallback(Widget w, XtPointer clientData, XtPointer callData);
void FBokCallback(Widget w, XtPointer clientData, XtPointer callData);
void FBcancelCallback(Widget w, XtPointer clientData, XtPointer callData);

#endif /* ! _GUICALLBACKS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
