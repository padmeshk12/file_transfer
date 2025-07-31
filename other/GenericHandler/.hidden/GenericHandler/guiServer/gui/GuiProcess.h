/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : GuiProcess.h
 * CREATED  : 26 Oct 1999
 *
 * CONTENTS : GUI functions
 *
 * AUTHORS  : Ulrich Frank, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Oct 1999, Ulrich Frank, created
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

#ifndef _GUIPROCESS_H_
#define _GUIPROCESS_H_

/*--- system includes -------------------------------------------------------*/

#include <Xm/Xm.h>
#include <stdio.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_GuiServer.h"

/*--- defines ---------------------------------------------------------------*/

#define PH_HANDLE_CHECK

#ifdef DEBUG
#define PH_HANDLE_CHECK
#endif

#ifdef PH_HANDLE_CHECK
#define CheckHandle(x) \
    if (!(x) || (x)->myself != (x)) \
		return PH_GUI_ERR_INVALIDE_HDL
#else
#define CheckHandle(x)
#endif

#define PH_GUI_NAME_LENGTH  40
#define PH_GUI_LABEL_LENGTH 40

/*--- typedefs --------------------------------------------------------------*/

typedef enum {
    PH_GUIP_ERR_OK             /* no error			      	*/,
    PH_GUIP_ERR_UNKNOWN        /* unknown error				*/,
} phGuiProcessError_t;

typedef enum { 
    showDialog = 0, 
    hideDialog, 
    disposeDialog, 
    enableComponent, 
    disableComponent, 
    configure, 
    requestData,
    requestEvent,
    changeData,
    simAction,
    comTest,
    identifyGui 
} phGuiRequest_t;

/*--- external function -----------------------------------------------------*/

struct simListStruct {
    struct simListStruct *next;
    char *name;
};

struct phGuiProcessStruct
{
    phLogId_t logger;
    phGuiSequenceMode_t mode;
    struct phGuiProcessStruct *myself;
    struct phGuiComponentStruct *toplevel;
    XtAppContext context;
    pid_t guipid;
    FILE *requestFp, *eventFp, *dataFp;
    int requestPipe[2], eventPipe[2], dataPipe[2];
    int semid;
    Boolean visible;
    Boolean initialized;
    struct simListStruct *simList;
    XmFontList fontList;
    unsigned long color[2];
/*    XFontStruct* font_info[4];*/
};

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

#endif /* ! _GUIPROCESS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
