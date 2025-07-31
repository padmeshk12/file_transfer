/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : GuiCallbacks.c
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
#include <Xm/Xm.h>
#include <Xm/FileSB.h>
/* Begin of Huatek Modification, Amy He, 03/04/2005 */
/* CR Number:16324 */
#include <unistd.h>
/* End of Huatek Modification */

/*--- module includes -------------------------------------------------------*/

#include "GuiProcess.h"
#include "GuiComponent.h"
#include "GuiCallbacks.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/
extern void sendEvent(
    phGuiProcessId_t process, 
    char *widgetName, 
    int eventType
);

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: 06 Dec 1999 - initial revision
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void PushButtonCallback(Widget w, XtPointer clientData, XtPointer callData) {
    phGuiComponent_t *comp = (phGuiComponent_t *) clientData;

    if (comp->core.comp.pb.exitButton) {
	sendEvent(comp->process, comp->name, PH_GUI_EXITBUTTON_EVENT);
    }
    else {
	sendEvent(comp->process, comp->name, PH_GUI_PUSHBUTTON_EVENT);
    }
    /* Begin of Huatek Modification, Tony Liu, 02/01/2005 */
    /* CR Number:16328 */
    if (comp->tooltip.on)
    {
       XtPopdown(comp->tooltip.w);
       comp->tooltip.on = False;
    }
    /* End of Huatek Modification */
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: 06 Dec 1999 - initial revision
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
/*
void ToggleButtonCallback(Widget w, XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *cbs =
	(XmToggleButtonCallbackStruct *) callData;
}
*/

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: 06 Dec 1999 - initial revision
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void FBokCallback(Widget w, XtPointer clientData, XtPointer callData) {
    XmFileSelectionBoxCallbackStruct *cbs = 
	(XmFileSelectionBoxCallbackStruct *) callData;
    phGuiComponent_t *comp = (phGuiComponent_t *) clientData;
    char *filename;
    
    XmStringGetLtoR(cbs->value, XmFONTLIST_DEFAULT_TAG, &filename);
    XtVaSetValues(comp->w, XmNvalue, filename, NULL);
    XtFree(filename);
    XtDestroyWidget(comp->core.comp.fb.browser);
    XtSetSensitive(comp->process->toplevel->w, True);
}

/*****************************************************************************
 
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: 06 Dec 1999 - initial revision
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void FBcancelCallback(Widget w, XtPointer clientData, XtPointer callData) {
    phGuiComponent_t *comp = (phGuiComponent_t *) clientData;

    XtUnrealizeWidget(comp->core.comp.fb.browser);
    XtSetSensitive(comp->process->toplevel->w, True);
}

void FBunmapCallback(Widget w, XtPointer clientData, XtPointer callData) {
    phGuiComponent_t *comp = (phGuiComponent_t *) clientData;

    XtSetSensitive(comp->process->toplevel->w, True);
}


/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: 06 Dec 1999 - initial revision
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void FileBrowserCallback(Widget w, XtPointer clientData, XtPointer callData) {
    phGuiComponent_t *comp = (phGuiComponent_t *) clientData;
    Widget tmp;
    char *tmpString;
    char *text;
    char selection[1024];
    XmString pattern;
    Arg al[10];
    int ac = 0;

    XtSetArg(al[ac], XmNfileTypeMask, comp->core.comp.fb.typeMask); ac++;
    XtSetArg(al[ac], XmNfontList, comp->process->fontList); ac++;
    /* Begin of Huatek Modification, Benny Wang, 05/01/2005 */
    /* CR Number:16323 */
    XtSetArg(al[ac], XmNtitle,comp->label); ac++;
    /* End of Huatek Modification */
    comp->core.comp.fb.browser = XmCreateFileSelectionDialog(
	comp->process->toplevel->frame, comp->label, al, ac);

    XtVaGetValues(comp->w, XmNvalue, &text, NULL);    
    tmpString = strrchr(text, '/');
    if (tmpString != NULL) {
	*(tmpString+1) = 0;
	strcpy(selection, text);
    /* Begin of Huatek Modification, Amy He, 03/04/2005 */
    /* CR Number:16324 */
        if(access(selection,0)==-1)
           strcpy(selection,PHLIB_ROOT_PATH);
        strcat(selection, comp->core.comp.fb.filter);
    }
    else {
        strcpy(selection,PHLIB_ROOT_PATH);
        strcat(selection, comp->core.comp.fb.filter);
    }
    /* End of Huatek Modification */

    pattern = XmStringCreateLtoR(selection, XmFONTLIST_DEFAULT_TAG);
    XmFileSelectionDoSearch(comp->core.comp.fb.browser, pattern);
    XmStringFree(pattern);

    XtVaGetValues(comp->w, XmNvalue, &text, NULL);    
    tmp = XmFileSelectionBoxGetChild(comp->core.comp.fb.browser, XmDIALOG_TEXT);
    XtVaSetValues(tmp, XmNvalue, text, NULL);

    XtAddCallback(comp->core.comp.fb.browser, XmNokCallback, FBokCallback, comp);
    XtAddCallback(comp->core.comp.fb.browser, XmNcancelCallback, FBcancelCallback, comp);
    XtAddCallback(comp->core.comp.fb.browser, XmNunmapCallback, FBunmapCallback, comp);

    XtSetSensitive(comp->process->toplevel->w, False);
    XtManageChild(comp->core.comp.fb.browser);

    XtFree(text);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
