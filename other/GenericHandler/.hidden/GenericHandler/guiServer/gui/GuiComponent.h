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

#ifndef _GUICOMPONENT_H_
#define _GUICOMPONENT_H_

/*--- system includes -------------------------------------------------------*/

#include <Xm/Xm.h> 
#include "GuiProcess.h"

/*--- defines ---------------------------------------------------------------*/

#define PH_GUI_PB_WIDTH    100
#define PH_GUI_PB_HEIGHT    30
#define PH_GUI_TB_WIDTH    100
#define PH_GUI_TB_HEIGHT    30
#define PH_GUI_TF_WIDTH    240
#define PH_GUI_TF_HEIGHT    30
#define PH_GUI_TA_HEIGHT   120
#define PH_GUI_TA_WIDTH    240
#define PH_GUI_LABEL_HEIGHT 30
#define PH_GUI_OM_WIDTH    120
#define PH_GUI_OM_HEIGHT    28

#define PH_GUI_MAX_OM_COMPS 40

/*--- typedefs --------------------------------------------------------------*/

typedef enum { 
    PH_GUI_CONT_TOP     /* Toplevel Container */,
    PH_GUI_CONT_SIMPLE  /* Simple Container   */,
    PH_GUI_CONT_RG      /* RadioButtonGroup   */,
    PH_GUI_CONT_TG      /* ToggleButtonGroup  */,
    PH_GUI_COMP_PB      /* PushButton         */,
    PH_GUI_COMP_TB      /* ToggleButton       */,
    PH_GUI_COMP_RB      /* RadioButton        */,
    PH_GUI_COMP_TF      /* TextField          */,
    PH_GUI_COMP_TA      /* TextArea           */,
    PH_GUI_COMP_TL      /* Simple Text Label  */,
    PH_GUI_COMP_ML      /* MultiLine Text     */,
    PH_GUI_COMP_OM      /* OptionMenu         */,
    PH_GUI_COMP_FB      /* FileBrowser        */,
    PH_GUI_COMP_GL      /* Glue               */
} phGuiComponentType_t;

typedef enum {
    PH_GUI_HORIZONTAL = XmHORIZONTAL,
    PH_GUI_VERTICAL = XmVERTICAL
} phGuiOrientation_t;

typedef struct phGuiTextList {
    Widget w;
    char *entry;
    struct phGuiTextList *next;
} phGuiTextList_t;


typedef struct {
    Widget w;
    Widget label;
    char *labelString;
    Boolean on;
} phToolTip_t;


typedef struct {
    phGuiOrientation_t orientation;
} phGuiSimpleContCore_t;

typedef struct {
    phGuiOrientation_t orientation;
    int selected;
    int columns;
} phGuiRBContCore_t;

typedef struct {
    phGuiOrientation_t orientation;
    int columns;
} phGuiTBContCore_t;

typedef union {
    phGuiSimpleContCore_t simple;
    phGuiRBContCore_t rb;
    phGuiTBContCore_t tb;
} phGuiContainerCore_t;


typedef struct {
    int exitButton;
} phGuiPushButton_t;

typedef struct {
    Boolean selected;
} phGuiToggleButton_t;

typedef struct {
    int selected;    
} phGuiRadioButton_t;

typedef struct {
    char *text;
    char *range;
} phGuiTextField_t;

typedef struct {
    char *text;
} phGuiTextArea_t;

typedef struct {
    char *text;
} phGuiMultiLineText_t;

typedef struct {
    phGuiTextList_t *textList;
    int selected;    
} phGuiOptionMenu_t;

typedef struct {
    Widget browser;
    char *filter;
    char *selection;
    XtArgVal typeMask;
} phGuiFileBrowser_t;

typedef union {
    phGuiPushButton_t    pb;
    phGuiToggleButton_t  tb;
    phGuiRadioButton_t   rb;
    phGuiTextField_t     tf;
    phGuiTextArea_t      ta;
    phGuiMultiLineText_t ml;
    phGuiOptionMenu_t    om;
    phGuiFileBrowser_t   fb;
} phGuiComponentCore_t;


typedef struct phGuiComponentStruct {
    int nr;       /* component nr in the current branch */
    Widget frame;
    Widget labelw;
    Widget w;
    Boolean enabled;
    char *name;
    char *label;
    phToolTip_t tooltip;
    struct phGuiProcessStruct *process;
    struct phGuiComponentStruct *prevSibling;
    struct phGuiComponentStruct *nextSibling;
    struct phGuiComponentStruct *parent;
    struct phGuiComponentStruct *firstChild;
    struct phGuiComponentStruct *lastChild;

    phGuiComponentType_t type;
    union {
        phGuiComponentCore_t comp;
        phGuiContainerCore_t cont;
    } core;
} phGuiComponent_t;

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

phGuiComponent_t *getComponent(
    phGuiComponent_t *comp,
    char *name
    );

char *getComponentData(
    phGuiProcessId_t process,
    phGuiComponent_t *component
    );

void setComponentData(
    phGuiProcessId_t process,
    phGuiComponent_t *component,
    char *data
    );

void simulateAction(
    phGuiProcessId_t process, 
    phGuiComponent_t *component
    );

void resizeComponent(phGuiComponent_t *comp);

void freeDialog(phGuiComponent_t *component);

void setupDialog(phGuiProcessId_t process);

#endif /* ! _GUIPROCESS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
