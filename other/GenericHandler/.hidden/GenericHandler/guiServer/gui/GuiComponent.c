/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : GuiComponent.c
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
 * 1) Copy this template to as many .c files as you require
 *
 * 2) Use the command 'make depend' to make visible the new
 *    source files to the makefile utility
 *
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>
#include <Xm/Text.h>
#include <Xm/ScrolledW.h>
#include <Xm/MwmUtil.h>
#include <Xm/ScrolledW.h>

/*--- module includes -------------------------------------------------------*/

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

#define CHILD_EVENT_MASK (EnterWindowMask|LeaveWindowMask)

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

extern Dimension phGuiLabelWidth;

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
static void post_tooltip(XtPointer client_data)
{
    Bool ptr_status;
    phGuiComponent_t *comp = (phGuiComponent_t *) client_data;
    Window window, root_window;
    int root_x, root_y, win_x, win_y, diff_x, diff_y, child_x, child_y;
    unsigned int mask, width, height, border_width, depth;
    Status status;

    window = None;

    ptr_status = XQueryPointer(XtDisplay(comp->w), XtWindow(comp->frame),
	&root_window, &window,
	&root_x, &root_y, &win_x, &win_y,
	&mask);

    if(ptr_status && window)
    {
	diff_x = root_x - win_x;
	diff_y = root_y - win_y;

	status = XGetGeometry(XtDisplay(comp->w), window,
	    &root_window,
	    &win_x, &win_y,
	    &width, &height,
	    &border_width, &depth);								

	if (status)
	{
	    child_x = diff_x + win_x;
	    child_y = diff_y + win_y;

	    XtVaSetValues(comp->tooltip.w,
		XmNx, (Position)(child_x + (width/2)),
		XmNy, (Position)(child_y + height + 5),
		NULL);

	    XtPopup(comp->tooltip.w, XtGrabNone);
	    comp->tooltip.on = True;
	}
    }
}

static void make_tooltip_on(phGuiComponent_t *comp)
{
    if (comp->tooltip.on) 
    {
	XtPopdown(comp->tooltip.w);
	comp->tooltip.on = False;
    }
    post_tooltip(comp);
}

static void make_tooltip_off(phGuiComponent_t *comp)
{
    if (comp->tooltip.on)
    {
	XtPopdown(comp->tooltip.w);
	comp->tooltip.on = False;
    }
}

static void child_event_handler(Widget widget, XtPointer client, XEvent *event)
{
    phGuiComponent_t *comp  = (phGuiComponent_t *) client;

    if (event->xcrossing.type == EnterNotify)
    {
	make_tooltip_on(comp);
    }
    else if (event->xcrossing.type == LeaveNotify)
    {
	make_tooltip_off(comp);
    }
}


void initTooltip(phGuiComponent_t *comp) {
    Arg temp_args[10];
    int i = 0;
    XmString str;

    if (comp->tooltip.labelString != NULL) {
	XtSetArg(temp_args[i], XmNallowShellResize, True); i++;
	XtSetArg(temp_args[i], XmNborderWidth, 1); i++;

	XtDisplay(comp->w);

	XtSetArg(temp_args[i], XmNborderColor, comp->process->color[1]); i++;
	XtSetArg(temp_args[i], XmNbackground, comp->process->color[0]); i++;
	
	comp->tooltip.w = XtCreatePopupShell("Notifier", overrideShellWidgetClass,
	    comp->w, temp_args, i);

/*
  if(tip_font_list)
  XtSetArg(temp_args[i], XmNfontList, tip_font_list); i++;
*/
	str = XmStringCreateLtoR(comp->tooltip.labelString, XmFONTLIST_DEFAULT_TAG);
	comp->tooltip.label = XtVaCreateManagedWidget(
	    "NotifierLabel",
	    xmLabelWidgetClass, 
	    comp->tooltip.w,
	    XmNlabelString, str,
	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNbackground, comp->process->color[0],
	    XmNforeground, comp->process->color[1],
	    XmNfontList, comp->process->fontList,
	    NULL);

	XmStringFree(str);

	XtAddEventHandler(comp->w, CHILD_EVENT_MASK, False,
	    (XtEventHandler) child_event_handler,
	    (XtPointer) comp);
    }
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateTopLevelContainer(phGuiComponent_t *comp) {
    if (comp != NULL && comp->frame != NULL) {
	comp->labelw = XtVaCreateManagedWidget(
	    "Scroller", 
	    xmScrolledWindowWidgetClass,
	    comp->frame,
	    XmNscrollingPolicy, XmAUTOMATIC,
	    XmNshadowThickness, 0,
	    XmNspacing, 0,
	    XmNscrolledWindowMarginWidth, 0,
	    XmNscrolledWindowMarginHeight, 0,
	    XmNfontList, comp->process->fontList,
	    NULL);

	/*CreateTopLevelContainer */
	comp->w = 
	    XtVaCreateManagedWidget(
	    "ScrollerContainer", 
	    xmRowColumnWidgetClass,
	    comp->labelw, 
	    XmNpacking, XmPACK_TIGHT,
	    XmNorientation, comp->core.cont.simple.orientation,
	    XmNnumColumns, 1,
	    XmNfontList, comp->process->fontList,
	    NULL);
    }
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateSimpleContainer(phGuiComponent_t *comp) {
    Widget frame;
    XmString str;

    if (comp->label != NULL) {
	comp->frame = XtVaCreateManagedWidget(
	    "SimpleContainerFrame",
	    xmFrameWidgetClass,
	    comp->parent->w, 
	    XmNshadowType, XmSHADOW_ETCHED_IN,
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNfontList, comp->process->fontList,
	    NULL);

	if (comp->label[0] != 0 && 
	    comp->parent->type != PH_GUI_CONT_TOP) {
	    str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
	    comp->labelw = XtVaCreateManagedWidget(
		"FrameLabel",
		xmLabelGadgetClass, comp->frame,
		XmNchildType, XmFRAME_TITLE_CHILD,
		XmNchildVerticalAlignment, XmALIGNMENT_CENTER,
		XmNlabelString, str,
		XmNfontList, comp->process->fontList,
		NULL);
	    XmStringFree(str);
	}

	frame = comp->frame;
    }
    else frame = comp->parent->w;

    comp->w = 
	XtVaCreateManagedWidget(
	    "SimpleContainer", 
	    xmRowColumnWidgetClass,
	    frame, 
	    XmNpacking, XmPACK_TIGHT,
	    XmNorientation, comp->core.cont.simple.orientation,
	    XmNnumColumns, 1,
	    XmNfontList, comp->process->fontList,
	    NULL);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateRBContainer(phGuiComponent_t *comp) {
    XmString str;
    Widget frame;

    if (comp->label != NULL) {
	comp->frame = XtVaCreateManagedWidget(
	    "RBContainerFrame",
	    xmFrameWidgetClass,
	    comp->parent->w, 
	    XmNshadowType, XmSHADOW_ETCHED_IN,
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNfontList, comp->process->fontList,
	    NULL);
	if (comp->label[0] != 0) {
	    str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
	    comp->labelw = XtVaCreateManagedWidget(
		"FrameLabel",
		xmLabelGadgetClass, comp->frame,
		XmNchildType, XmFRAME_TITLE_CHILD,
		XmNchildVerticalAlignment, XmALIGNMENT_CENTER,
		XmNlabelString, str,
		XmNfontList, comp->process->fontList,
		NULL);
	    XmStringFree(str);
	}
	frame = comp->frame;
    }
    else frame = comp->parent->w;

    comp->w = 
	XtVaCreateManagedWidget(
	    "RBContainer", 
	    xmRowColumnWidgetClass,
	    frame, 
	    XmNpacking, XmPACK_COLUMN,
	    XmNorientation, XmVERTICAL,
	    XmNnumColumns, comp->core.cont.rb.columns,
	    XmNradioBehavior, True,
	    XmNfontList, comp->process->fontList,
	    NULL);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateTBContainer(phGuiComponent_t *comp) {
    Widget frame;
    XmString str;

    if (comp->label != NULL) {
	comp->frame = XtVaCreateManagedWidget(
	    "TBContainerFrame",
	    xmFrameWidgetClass,
	    comp->parent->w, 
	    XmNshadowType, XmSHADOW_ETCHED_IN,
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNfontList, comp->process->fontList,
	    NULL);

	if (comp->label[0] != 0) {
	    str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
	    comp->labelw = XtVaCreateManagedWidget(
		"FrameLabel",
		xmLabelGadgetClass, comp->frame,
		XmNchildType, XmFRAME_TITLE_CHILD,
		XmNchildVerticalAlignment, XmALIGNMENT_CENTER,
		XmNlabelString, str,
		XmNfontList, comp->process->fontList,
		NULL);
	    XmStringFree(str);
	}
	frame = comp->frame;
    }
    else frame = comp->parent->w;

    comp->w = 
	XtVaCreateManagedWidget(
	    "TBContainer", 
	    xmRowColumnWidgetClass,
	    frame, 
	    XmNpacking, XmPACK_COLUMN,
	    XmNorientation, XmVERTICAL,
	    XmNnumColumns, comp->core.cont.tb.columns,
	    XmNradioBehavior, False,
	    XmNfontList, comp->process->fontList,
	    NULL);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreatePushButton(phGuiComponent_t *comp) {
    XmString str;

    comp->frame = XtVaCreateManagedWidget(
	"Frame", 
	xmFormWidgetClass,
	comp->parent->w, 
	XmNfontList, comp->process->fontList,
	NULL);

    str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
    comp->w = 
	XtVaCreateManagedWidget(
	    "Button", 
	    xmPushButtonWidgetClass,
	    comp->frame,
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNwidth,  PH_GUI_PB_WIDTH,
	    XmNheight, PH_GUI_PB_HEIGHT,
	    XmNresizeWidth, False,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
    XmStringFree(str);
    XtAddCallback(comp->w, XmNactivateCallback, PushButtonCallback, comp);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateToggleButton(phGuiComponent_t *comp) {
    Widget frame;
    XmString str;

    if (comp->parent->type != PH_GUI_CONT_TG) {
	frame = comp->frame = XtVaCreateManagedWidget(
	    "Frame", 
	    xmFormWidgetClass,
	    comp->parent->w, 
	    XmNfontList, comp->process->fontList,
	    NULL);
    }
    else {
	frame = comp->parent->w;
    }

    str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
    comp->w = 
	XtVaCreateManagedWidget(
	    "Button",
	    xmToggleButtonWidgetClass,
	    frame, 
	    XmNset, comp->core.comp.tb.selected,
	    XmNmarginHeight, 3,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
    XmStringFree(str);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateRadioButton(phGuiComponent_t *comp) {
    int selected;
    XmString str;

    selected = comp->parent->core.cont.rb.selected;

    str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
    comp->w = 
	XtVaCreateManagedWidget(
	    "Button",
	    xmToggleButtonWidgetClass,
	    comp->parent->w, 
	    XmNset, selected == comp->nr,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
    XmStringFree(str);

    if (selected == comp->nr) {
	XtVaSetValues(
	    comp->parent->w, 
	    XmNmenuHistory, comp->w,
	    NULL
	    );
    }
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateTextField(phGuiComponent_t *comp) {
    Dimension x = 0;
    XmString str;

    comp->frame = XtVaCreateManagedWidget(
	NULL, 
	xmFormWidgetClass,
	comp->parent->w, 
	XmNfontList, comp->process->fontList,
	NULL);

    if (comp->label != NULL) {
	str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
	x = phGuiLabelWidth + 10;
	comp->labelw = XtVaCreateManagedWidget(
	    "Label",
	    xmLabelGadgetClass, comp->frame,
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNheight, PH_GUI_TF_HEIGHT,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
	XmStringFree(str);
    }

    comp->w = 
	XtVaCreateManagedWidget(
	    "Text", 
	    xmTextFieldWidgetClass,
	    comp->frame, 
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNx,  x,
	    XmNwidth,  PH_GUI_TF_WIDTH,
	    XmNheight, PH_GUI_TF_HEIGHT,
	    XmNvalue, comp->core.comp.tf.text,
	    XmNfontList, comp->process->fontList,
	    NULL);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateTextArea(phGuiComponent_t *comp) {
    Widget scroller;
    Dimension y = 0;
    XmString str;

    comp->frame = XtVaCreateManagedWidget(
	"Frame", 
	xmFormWidgetClass,
	comp->parent->w,
	XmNbottomAttachment, XmATTACH_FORM,
	XmNtopAttachment, XmATTACH_FORM,
	XmNleftAttachment, XmATTACH_FORM,
	XmNrightAttachment, XmATTACH_FORM,
	XmNfontList, comp->process->fontList,
	NULL);

    if (comp->label != NULL) {
	str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
	y = PH_GUI_LABEL_HEIGHT;
	comp->labelw = XtVaCreateManagedWidget(
	    "Label",
	    xmLabelGadgetClass,
	    comp->frame,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNheight, PH_GUI_LABEL_HEIGHT,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
	XmStringFree(str);
    }

    scroller = XtVaCreateManagedWidget(
	"Scroller", 
	xmScrolledWindowWidgetClass,
	comp->frame,
	XmNheight, PH_GUI_TA_HEIGHT,
	XmNtopWidget, comp->labelw,
	XmNtopAttachment, XmATTACH_WIDGET,
        XmNleftAttachment, XmATTACH_WIDGET,
        XmNrightAttachment, XmATTACH_WIDGET,
	XmNbottomAttachment, XmATTACH_FORM,
	XmNfontList, comp->process->fontList,
	NULL);

    comp->w = 
	XtVaCreateManagedWidget(
	    "Text", 
	    xmTextWidgetClass,
	    scroller, 
	    XmNvalue, comp->core.comp.tf.text,
	    XmNeditMode, XmMULTI_LINE_EDIT,       
	    XmNfontList, comp->process->fontList,
	    NULL);



}


/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateText(phGuiComponent_t *comp) {
    XmString str;

    comp->frame = XtVaCreateManagedWidget(
	"Frame", 
	xmFormWidgetClass,
	comp->parent->w, 
	NULL);

    str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
    comp->w = 
	XtVaCreateManagedWidget(
	    "Label",
	    xmLabelGadgetClass, comp->frame,
	    XmNalignment, XmALIGNMENT_BEGINNING,
	    XmNwidth, phGuiLabelWidth,
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNheight, PH_GUI_TF_HEIGHT,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
    XmStringFree(str);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateMultiLineText(phGuiComponent_t *comp) {
    XmString str;

    comp->frame = XtVaCreateManagedWidget(
	"Frame", 
	xmRowColumnWidgetClass,
	comp->parent->w, 
	XmNpacking, XmPACK_COLUMN,
	XmNorientation, XmVERTICAL,
	XmNnumColumns, 1,
	XmNfontList, comp->process->fontList,
	NULL);

    str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
    comp->w = XtVaCreateManagedWidget(
	"Label", 
	xmLabelGadgetClass,
	comp->frame, 
	XmNmarginWidth, 5,
	XmNmarginHeight, 4,
	XmNalignment, XmALIGNMENT_BEGINNING,
	XmNlabelString, str,
	XmNfontList, comp->process->fontList,
	NULL);
    XmStringFree(str);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateOptionMenu(phGuiComponent_t *comp) {
    Widget pane;
    phGuiTextList_t *dummy;
    int argc = 0;
    int nr = 0;
    int x = 0;
    int dx = 0, dy = 0;
    Dimension width, height;
    XmString str;

    comp->frame = XtVaCreateManagedWidget(
	"Frame", 
	xmFormWidgetClass,
	comp->parent->w, 
	XmNmarginWidth, 0,
	XmNmarginHeight, 0,
	XmNfontList, comp->process->fontList,
	NULL);

    if (comp->label != NULL) {
	str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
	x = phGuiLabelWidth + 10;
	comp->labelw = XtVaCreateManagedWidget(
	    "Label",
	    xmLabelGadgetClass, comp->frame,
	    XmNmarginWidth, 5,
	    XmNmarginTop, 5,
	    XmNmarginBottom, 3,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
	XmStringFree(str);
    }

    comp->w = 
	XmCreateOptionMenu(comp->frame, "Button", NULL, 0);
    pane = XmCreatePulldownMenu(comp->frame, "Frame", NULL, 0);
    XtVaSetValues(
	comp->w, 
	XmNsubMenuId, pane, 
	XmNx, x,
	XmNmarginWidth, 0,
	XmNmarginHeight, 0,
	NULL);
    XtManageChild(comp->w);

    dummy = comp->core.comp.om.textList;

    do {
	dummy->w = XtVaCreateManagedWidget(
            dummy->entry, 
	    xmPushButtonGadgetClass,
	    pane,
	    XmNmarginWidth, 0,
	    XmNmarginHeight, 0,
	    XmNfontList, comp->process->fontList,
	    NULL);
/* Begin of Huatek Modification, Donnie Tu, 04/20/2001 */
/* Issue Number: 200 */
	if(nr==0)
	{
	XtVaGetValues(dummy->w, XmNwidth, &width, XmNheight, &height, NULL);
	dx = PH_GUI_OM_WIDTH - 3 - width; if (dx < 3) dx = 3;
	dy = PH_GUI_OM_HEIGHT - 3 - height; if (dy < 3) dy = 3;
	}
	XtVaSetValues(dummy->w, 
	    XmNmarginLeft, 3, 
	    XmNmarginRight, dx, 
	    XmNmarginTop, 3, 
	    XmNmarginBottom, dy, 
	    NULL);
/* End of Huatek Modification */
	if (comp->core.comp.om.selected == nr++) {
	    XtVaSetValues(
		comp->w, 
		XmNmenuHistory, dummy->w,
		NULL); 
	}
	dummy = dummy->next;
    } while (dummy != NULL && argc < PH_GUI_MAX_OM_COMPS);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateFileBrowser(phGuiComponent_t *comp) {
    Dimension x = 0;
    Widget button;
    XmString str;

    comp->frame = XtVaCreateManagedWidget(
	NULL, 
	xmFormWidgetClass,
	comp->parent->w, 
	XmNfontList, comp->process->fontList,
	NULL);

    if (comp->label != NULL) {
	str = XmStringCreateLtoR(comp->label, XmFONTLIST_DEFAULT_TAG);
	x = phGuiLabelWidth + 10;
	comp->labelw = XtVaCreateManagedWidget(
	    "Label",
	    xmLabelGadgetClass, comp->frame,
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNheight, PH_GUI_TF_HEIGHT,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
	XmStringFree(str);
    }

    comp->w = 
	XtVaCreateManagedWidget(
	    "Text", 
	    xmTextFieldWidgetClass,
	    comp->frame, 
	    XmNmarginWidth, 5,
	    XmNmarginHeight, 4,
	    XmNx,  x,
	    XmNwidth,  PH_GUI_TF_WIDTH,
	    XmNheight, PH_GUI_TF_HEIGHT,
	    XmNvalue, comp->core.comp.fb.selection,
	    XmNfontList, comp->process->fontList,
	    NULL);

    str = XmStringCreateLtoR("Browse...", XmFONTLIST_DEFAULT_TAG);
    button = XtVaCreateManagedWidget(
	 "Button", 
	 xmPushButtonWidgetClass,
	 comp->frame, 
	 XmNmarginWidth, 5,
	 XmNmarginHeight, 4,
	 XmNx,  x + PH_GUI_TF_WIDTH + 10,
	 XmNwidth,  PH_GUI_PB_WIDTH,
	 XmNheight, PH_GUI_PB_HEIGHT,
	 XmNresizeWidth, False,
	 XmNlabelString, str,
	 XmNfontList, comp->process->fontList,
	 NULL);
     XmStringFree(str);
     
     XtAddCallback(button, XmNactivateCallback, FileBrowserCallback, comp);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void CreateGlue(phGuiComponent_t *comp) {
    XmString str;

    comp->frame = XtVaCreateManagedWidget(
	"Frame", 
	xmFormWidgetClass,
	comp->parent->w, 
	XmNmarginWidth, 0,
	XmNmarginHeight, 0,
	XmNfontList, comp->process->fontList,
	NULL);

    str = XmStringCreateLtoR("", XmFONTLIST_DEFAULT_TAG);
    comp->w = XtVaCreateManagedWidget(
	"Label", 
	xmLabelGadgetClass,
	comp->frame, 
	XmNmarginWidth, 0,
	XmNmarginHeight, 0,
	XmNlabelString, str,
	XmNfontList, comp->process->fontList,
	NULL);
    XmStringFree(str);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
phGuiComponent_t *getComponent(phGuiComponent_t *comp, char *name) {
    phGuiComponent_t *result;

    if (comp == NULL) {
	return NULL;
    }

    if ((result = getComponent(comp->firstChild, name)) != NULL) return result;
    if ((result = getComponent(comp->nextSibling, name)) != NULL) return result;

    if (comp != NULL && comp->name != NULL &&
	strcmp(name, comp->name) == 0) return comp;
    else return NULL;
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void setComponentData(phGuiProcessId_t process, phGuiComponent_t *comp, char *data) {
    Widget w;
    XmString str;
    phGuiTextList_t *dummy;

    switch (comp->type) {
      case PH_GUI_COMP_PB:
	str = XmStringCreateLtoR(data, XmFONTLIST_DEFAULT_TAG);
	XtVaSetValues(
	    comp->w,
	    XmNlabelString, str,
	    XmNfontList, comp->process->fontList,
	    NULL);
	XtVaSetValues(
	    comp->w,
	    XmNwidth,  PH_GUI_PB_WIDTH,
	    XmNheight, PH_GUI_PB_HEIGHT,
	    NULL);
	XmStringFree(str);
	break;
      case PH_GUI_COMP_TB:
	XtVaSetValues(comp->w, XmNset, strcasecmp(data, "True") == 0, NULL);
	break;
      case PH_GUI_COMP_RB:
	XtVaGetValues(comp->parent->w, XmNmenuHistory, &w, NULL);
	XtVaSetValues(w, XmNset, False, NULL);
	XtVaSetValues(comp->w, XmNset, strcasecmp(data, "True") == 0, NULL);
	if (strcasecmp(data, "True") == 0) {
	    XtVaSetValues(
		comp->parent->w, 
		XmNmenuHistory, comp->w,
		NULL
		);
	}

	break;
      case PH_GUI_COMP_TF:
	XtVaSetValues(comp->w, XmNvalue, data, NULL);
	break;
      case PH_GUI_COMP_TA:
	XtVaSetValues(comp->w, XmNvalue, data, NULL);
	break;
      case PH_GUI_COMP_OM:
        /* BUG FIX Option menu set component date not done */
        dummy = comp->core.comp.om.textList;
        do {
           if ( strcasecmp(data, XtName(dummy->w)) == 0 ) {
	        XtVaSetValues(
		    comp->w, 
		    XmNmenuHistory, dummy->w,
		    NULL); 
	    }
	    dummy = dummy->next;
        } while (dummy != NULL);
	break;
      case PH_GUI_COMP_FB:
	XtVaSetValues(comp->w, XmNvalue, data, NULL);
	break;
      default:
	break;
    }
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
char *getComponentData(phGuiProcessId_t process, phGuiComponent_t *comp) {
    Boolean b;
    String s;
    Widget w;
    char lbuffer[32];

    switch (comp->type) {
      case PH_GUI_COMP_PB:
	break;
      case PH_GUI_COMP_TB:
	XtVaGetValues(comp->w, XmNset, &b, NULL);
	sprintf(lbuffer, "%s", b?"True":"False");
	return strdup(lbuffer);
      case PH_GUI_COMP_RB:
	XtVaGetValues(comp->w, XmNset, &b, NULL);
	sprintf(lbuffer, "%s", b?"True":"False");
	return strdup(lbuffer);
      case PH_GUI_COMP_TF:
	XtVaGetValues(comp->w, XmNvalue, &s, NULL);
	return strdup(s);
      case PH_GUI_COMP_TA:
	XtVaGetValues(comp->w, XmNvalue, &s, NULL);
	return strdup(s);
      case PH_GUI_COMP_OM:
	XtVaGetValues(comp->w, XmNmenuHistory, &w, NULL);
	return strdup(XtName(w));
      case PH_GUI_COMP_FB:
	XtVaGetValues(comp->w, XmNvalue, &s, NULL);
	return strdup(s);
      default:
	break;
    }

    return NULL;
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void freeDialog(phGuiComponent_t *comp) {

    if (comp == NULL) {
	return;
    }

    freeDialog(comp->firstChild);
    freeDialog(comp->nextSibling);

    if (comp->tooltip.labelString != NULL) {
	free(comp->tooltip.labelString);
	XtDestroyWidget(comp->tooltip.label);
	XtDestroyWidget(comp->tooltip.w);
    }

    XtDestroyWidget(comp->w);

    switch (comp->type) {
      case PH_GUI_CONT_TOP:
	free(comp->label);
	free(comp->name);
        XtDestroyWidget(comp->labelw);
	return;
      case PH_GUI_CONT_SIMPLE:
	if (comp->label != NULL) {
	    if (comp->label[0] != 0 &&
		comp->parent->type != PH_GUI_CONT_TOP) {
		XtDestroyWidget(comp->labelw);
	    }
	    XtDestroyWidget(comp->frame);
	}
	XtDestroyWidget(comp->w);
	break;
      case PH_GUI_CONT_RG:
      case PH_GUI_CONT_TG:
	if (comp->label != NULL) {
	    XtDestroyWidget(comp->frame);
	    if (comp->label[0] != 0) {
		XtDestroyWidget(comp->labelw);
	    }
	}
	break;
      case PH_GUI_COMP_TB:
	if (comp->parent->type != PH_GUI_CONT_TG) {
	    XtDestroyWidget(comp->frame);
	}
	break;
      case PH_GUI_COMP_RB:
	break;
      case PH_GUI_COMP_TL:
	XtDestroyWidget(comp->frame);
	break;
      case PH_GUI_COMP_ML:
	XtDestroyWidget(comp->frame);
	break;
      case PH_GUI_COMP_TF:
	free(comp->core.comp.tf.text);
	XtDestroyWidget(comp->frame);
	break;
      case PH_GUI_COMP_TA:
	free(comp->core.comp.ta.text);
	XtDestroyWidget(comp->frame);
	break;
      case PH_GUI_COMP_PB:
      case PH_GUI_COMP_OM:
	XtDestroyWidget(comp->frame);
	break;
      case PH_GUI_COMP_FB:
	XtDestroyWidget(comp->frame);
	break;
      case PH_GUI_COMP_GL:
	XtDestroyWidget(comp->frame);
	break;
      default:
	break;
    }
    free(comp->name);
    free(comp->label);
    free(comp);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
/*
void printSize(phGuiProcessId_t process, phGuiComponent_t *comp) {
    Dimension w, h;

    if (comp == NULL) {
	return;
    }

    XtVaGetValues(
	comp->w,
	XmNheight, &h,
	XmNwidth, &w,
	NULL);

    switch (comp->type) {
      case PH_GUI_CONT_TOP:
	break;
      case PH_GUI_CONT_SIMPLE:
	break;
      case PH_GUI_CONT_RG:
	break;
      case PH_GUI_CONT_TG:
	break;
      case PH_GUI_COMP_PB:
	printf("PushButton:   %ld %ld\n", w, h);
	break;
      case PH_GUI_COMP_TB:
	printf("ToggleButton: %ld %ld\n", w, h);
	break;
      case PH_GUI_COMP_RB:
	printf("RadioButton:  %ld %ld\n", w, h);
	break;
      case PH_GUI_COMP_TF:
	printf("TextField:    %ld %ld\n", w, h);
	break;
      case PH_GUI_COMP_TA:
	printf("TextArea:     %ld %ld\n", w, h);
	break;
      case PH_GUI_COMP_OM:
	printf("OptionMenu:   %ld %ld\n", w, h);
	break;
      case PH_GUI_COMP_FB:
	printf("FileBrowser:  %ld %ld\n", w, h);
	break;
      default:
	printf("%s\n", comp->name);
	break;
    }

    printSize(process, comp->firstChild);
    printSize(process, comp->nextSibling);
}
*/
/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void setupComponent(phGuiProcessId_t process, phGuiComponent_t *comp) {

    if (comp == NULL) {
	return;
    }

    comp->process = process;

    switch (comp->type) {
      case PH_GUI_CONT_TOP:
	CreateTopLevelContainer(comp);
	break;
      case PH_GUI_CONT_SIMPLE:
	CreateSimpleContainer(comp);
	break;
      case PH_GUI_CONT_RG:
	CreateRBContainer(comp);
	break;
      case PH_GUI_CONT_TG:
	CreateTBContainer(comp);
	break;
      case PH_GUI_COMP_PB:
	CreatePushButton(comp);
	break;
      case PH_GUI_COMP_TB:
	CreateToggleButton(comp);
	break;
      case PH_GUI_COMP_RB:
	CreateRadioButton(comp);
	break;
      case PH_GUI_COMP_TF:
	CreateTextField(comp);
	break;
      case PH_GUI_COMP_TA:
	CreateTextArea(comp);
	break;
      case PH_GUI_COMP_TL:
	CreateText(comp);
	break;
      case PH_GUI_COMP_ML:
	CreateMultiLineText(comp);
	break;
      case PH_GUI_COMP_OM:
	CreateOptionMenu(comp);
	break;
      case PH_GUI_COMP_FB:
	CreateFileBrowser(comp);
	break;
      case PH_GUI_COMP_GL:
	CreateGlue(comp);
	break;
      default:
/*	printf("%s\n", comp->name);*/
	break;
    }

    initTooltip(comp);

    setupComponent(process, comp->firstChild);
    setupComponent(process, comp->nextSibling);

    if (comp->frame != NULL) 
	XtSetSensitive(comp->frame, comp->enabled);
    else XtSetSensitive(comp->w, comp->enabled);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void simulateAction(phGuiProcessId_t process, phGuiComponent_t *comp) {
    XmPushButtonCallbackStruct cbs;

    cbs.reason = XmCR_ACTIVATE;
    cbs.event = NULL;
    cbs.click_count = 1;

    switch (comp->type) {
      case PH_GUI_COMP_PB:
	PushButtonCallback(comp->w, comp, (XtPointer *) &cbs);
	break;
      default:
	break;
    }
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void resizeComponent(phGuiComponent_t *comp) {
    Dimension w, h, dw, dh, sw = 0, sh = 0, sumw = 0, sumh = 0, parw, parh, neww, newh;
    int glueCounter = 1;
    int glueNr = 1;
    Widget horScrollBar, verScrollBar;
    phGuiComponent_t *dummy;    

    if (comp == NULL) {
	return;
    }

    resizeComponent(comp->firstChild);
    resizeComponent(comp->nextSibling);

    switch (comp->type) {
      case PH_GUI_CONT_TOP: 
	if (comp->firstChild->label != NULL) {
	    XtVaGetValues(    
		comp->firstChild->frame,
		XmNwidth, &w,
		XmNheight, &h,
		NULL);
	}
	else {
	    XtVaGetValues(    
		comp->firstChild->w, 
		XmNwidth, &w,
		XmNheight, &h,
		NULL);
	}

	XtVaGetValues(    
	    comp->labelw, 
	    XmNhorizontalScrollBar, &horScrollBar,
	    XmNverticalScrollBar, &verScrollBar,
	    NULL);

	if (horScrollBar != NULL) {
	    XtVaGetValues(    
		horScrollBar,
		XmNheight, &sh,
		NULL);
	}

	if (verScrollBar != NULL) {
	    XtVaGetValues(    
		verScrollBar,
		XmNwidth, &sw,
		NULL);
	}


	dw = DisplayWidth(XtDisplay(comp->frame), 0);
	dh = DisplayHeight(XtDisplay(comp->frame), 0);

	w += 6;
	h += 6;

	if (w >= dw - 100) h += sh + 2;
	else if (h >= dh - 100) w += sw + 2;

	if (w >= dw - 100) w = dw - 100;
	if (h >= dh - 100) h = dh - 100;	

	XtVaSetValues(    
	    comp->frame, 
	    XmNwidth, w,
	    XmNheight, h,
	    XmNx, (Dimension) ((dw - w) / 2),
	    XmNy, (Dimension) ((dh - h) / 2),
/*	    XmNx, ((dw - w) / 2) < 0? 0 : (dw - w) / 2,
	    XmNy, ((dh - h) / 2) < 0? 0 : (dh - h) / 2,*/
	    NULL);
	break;

      case PH_GUI_COMP_GL:
	XtVaGetValues(comp->parent->w, XmNwidth, &parw, XmNheight, &parh, NULL);
	XtVaGetValues(comp->parent->w, XmNmarginWidth, &dw, XmNmarginHeight, &dh, NULL);
	parw-=2*dw; parh-=2*dh; 

	dummy = comp->parent->firstChild;
	do {
	    if (dummy != comp && dummy->type != PH_GUI_COMP_GL) {
		if (dummy->frame != NULL) {
		    XtVaGetValues(dummy->frame, XmNwidth, &w, XmNheight, &h, NULL);
		    XtVaGetValues(dummy->frame, XmNmarginWidth, &dw, XmNmarginHeight, &dh, NULL);
		}
		else {
		    XtVaGetValues(dummy->w, XmNwidth, &w, XmNheight, &h, NULL);
		    XtVaGetValues(dummy->w, XmNmarginWidth, &dw, XmNmarginHeight, &dh, NULL);
		}
		sumw += w + dw; sumh += h + dh;
	    }
	    else if (dummy != comp && dummy->type == PH_GUI_COMP_GL) {
		glueCounter++;
	    }
	    else {
		glueNr = glueCounter;
	    }
	    dummy = dummy->nextSibling;
	} while (dummy != NULL);

	neww = (parw - sumw) / glueCounter - 3;
	newh = (parh - sumh) / glueCounter - 3;

	if (glueNr == glueCounter) {
	    neww += (parw - sumw) % glueCounter;
	    newh += (parh - sumh) % glueCounter;
	}

	if (comp->parent->core.cont.simple.orientation == PH_GUI_HORIZONTAL) {
	    XtVaSetValues(comp->w, XmNwidth, neww, NULL);
	}
	else {
	    XtVaSetValues(comp->w, XmNheight, newh, NULL);
	}
	break;

      default:
	break;
    }
}
/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void setupDialog(phGuiProcessId_t process) {
    XtVaSetValues(
	process->toplevel->frame, 
	XmNheight, 0,
	XmNwidth, 0,
	NULL);

    setupComponent(process, process->toplevel);

    XtVaSetValues(
        process->toplevel->frame, 
        XmNmwmDecorations, ~MWM_DECOR_MINIMIZE & ~MWM_DECOR_TITLE & ~MWM_DECOR_BORDER,
        XmNtitle, process->toplevel->label,
	XmNiconName, process->toplevel->label,
        NULL);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
