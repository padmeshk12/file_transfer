%{
/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : parser.y
 * CREATED  : 26 May 1999
 *
 * CONTENTS : Parser definition for 
 *
 * AUTHORS  : Ulrich Frank, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 
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

/*--- module includes -------------------------------------------------------*/

#include "Xm/LabelG.h"
#include "GuiComponent.h"
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
/* Begin of Huatek Modification, Donnie Tu, 12/14/2001 */
/* Issue Number: 285 */
extern void phGuiLexInit(char *text);
/* End of Huatek Modification */

/*--- global variables ------------------------------------------------------*/

    static phGuiComponent_t *parseResult;
    static phGuiProcessId_t phGuiParserProcess;
    Dimension phGuiLabelWidth;
    
phGuiComponent_t *phGuiParseConf( phGuiProcessId_t process, char *text);
phGuiComponent_t *createComponent(char *name,char *label);
phGuiComponent_t *createGroup(phGuiComponentType_t type, char *name, char *label, phGuiComponent_t *firstChild);
void addSibling(phGuiComponent_t *member, phGuiComponent_t *sibling);
void calcLabelWidth(char *label);
static void yyerror(char *error);
%}

/*--- yacc definitions ------------------------------------------------------*/

%union {
    int                        int_v;
    float                    float_v;
    char                   *string_v;
    phGuiTextList_t      *textList_v;
    phGuiComponent_t      *guiComp_v;
}

%token NOTOKEN
%token COMMA
%token COLON
%token STAR
%token TILDE
%token BRK1_OPEN
%token BRK1_CLOSE
%token BRK2_OPEN
%token BRK2_CLOSE
%token BRK3_OPEN
%token BRK3_CLOSE
%token EXITBUTTON
%token SELECTED
%token DIRMASK
%token <int_v>     ORIENTATION
%token <int_v>     INTVAL
%token <float_v>   FLOATVAL
%token <string_v>  QSTRING
%token <guiComp_v> SIMPLEGROUP
%token <guiComp_v> RBGROUP
%token <guiComp_v> TOGGLEBUTTON
%token <guiComp_v> TBGROUP
%token <guiComp_v> PUSHBUTTON
%token <guiComp_v> TEXTFIELD
%token <guiComp_v> TEXTAREA
%token <guiComp_v> TEXT
%token <guiComp_v> MULTILINETEXT
%token <guiComp_v> OPTIONMENU
%token <guiComp_v> FILEBROWSER

/*%type <float_v> */
%type <string_v>   Filter Selection String Range ToolTip
%type <int_v>      ExitButton Columns DefaultInt ToggleButtonSelected Orientation
%type <int_v>      Enabled TypeMask
%type <textList_v> TextList ListElement ListEntry
%type <guiComp_v>  WidgetIdentifier
%type <guiComp_v>  PushButton RadioButton ToggleButton TextField TextArea OptionMenu
%type <guiComp_v>  RBGroup RBGroupMember TBGroup TBGroupMember Group SimpleGroup Glue
%type <guiComp_v>  Component Widget FileBrowser ToggleButtonInBox Text MultiLineText

%start Start

/*--- yacc grammar ----------------------------------------------------------*/

%%

Start : 
SIMPLEGROUP SimpleGroup {     
    parseResult = $2; 
}
;

Group :
Component |
Component Group { 
    addSibling($1, $2);
}
;

Component : 
Enabled Widget ToolTip { 
    $2->enabled = $1;
    $$ = $2;
    if ($3 != NULL) $2->tooltip.labelString = strdup($3);
    else $2->tooltip.labelString = NULL;
}
;

Widget :
SIMPLEGROUP   SimpleGroup   { $$ = $2; } |
RBGROUP       RBGroup       { $$ = $2; } |
TOGGLEBUTTON  ToggleButton  { $$ = $2; } |
TBGROUP       TBGroup       { $$ = $2; } |
PUSHBUTTON    PushButton    { $$ = $2; } |
TEXTFIELD     TextField     { $$ = $2; } |
TEXTAREA      TextArea      { $$ = $2; } |
TEXT          Text          { $$ = $2; } |
MULTILINETEXT MultiLineText { $$ = $2; } |
OPTIONMENU    OptionMenu    { $$ = $2; } |
FILEBROWSER   FileBrowser   { $$ = $2; } |
STAR          Glue          { $$ = $2; }
;


/*--------------------------------------------------------*/

ToolTip :
BRK3_OPEN String BRK3_CLOSE { $$ = $2; }
| /* empty */ { $$ = NULL; }
;
/*--------------------------------------------------------*/

SimpleGroup : 
COLON String COLON String BRK2_OPEN Orientation Group BRK2_CLOSE { 
    phGuiComponent_t *comp;

    comp = createGroup(PH_GUI_CONT_SIMPLE, $2, $4, $7);
    comp->core.cont.simple.orientation = (phGuiOrientation_t)$6;
    
    $$ = comp;
}
;

/*--------------------------------------------------------*/

RBGroup :
COLON String COLON String BRK2_OPEN RBGroupMember BRK2_CLOSE DefaultInt Columns { 
    phGuiComponent_t *comp;

    comp = createGroup(PH_GUI_CONT_RG, $2, $4, $6); 
    comp->core.cont.rb.selected = $8;
    comp->core.cont.rb.columns = $9;

    $$ = comp; 
}
;

RBGroupMember :
RadioButton |
RadioButton RBGroupMember {
    addSibling($1, $2);
}
;

RadioButton :
Enabled QSTRING COLON QSTRING { 
    phGuiComponent_t *comp;

    comp = createComponent($2, $4);
    comp->type = PH_GUI_COMP_RB;
    comp->enabled = $1;

    $$ = comp;
}
;

/*--------------------------------------------------------*/

TBGroup :
COLON String COLON String BRK2_OPEN TBGroupMember BRK2_CLOSE Columns { 
    phGuiComponent_t *comp;

    comp = createGroup(PH_GUI_CONT_TG, $2, $4, $6); 
    comp->core.cont.tb.columns = $8;

    $$ = comp;
}
;

TBGroupMember :
ToggleButtonInBox |
ToggleButtonInBox TBGroupMember {
    addSibling($1, $2);
}

;

ToggleButton :
WidgetIdentifier ToggleButtonSelected { 
    phGuiComponent_t *comp;

    comp = $1;
    comp->type = PH_GUI_COMP_TB;
    comp->core.comp.tb.selected = $2;

    $$ = comp;
}
;

ToggleButtonInBox :
Enabled QSTRING COLON QSTRING ToggleButtonSelected { 
    phGuiComponent_t *comp;

    comp = createComponent($2, $4);
    comp->enabled = $1;
    comp->type = PH_GUI_COMP_TB;
    comp->core.comp.tb.selected = $5;

    $$ = comp;
}
;

ToggleButtonSelected :
COLON SELECTED { $$ = True; } 
| /* empty */ { $$ = False; }
;

/*--------------------------------------------------------*/

PushButton :
WidgetIdentifier ExitButton
{
    phGuiComponent_t *comp;

    comp = $1;
    comp->type = PH_GUI_COMP_PB;
    comp->core.comp.pb.exitButton = $2;

    $$ = comp;
}
;

/*--------------------------------------------------------*/

TextField :
WidgetIdentifier COLON String Range {
    phGuiComponent_t *comp;

    comp = $1;
    comp->type = PH_GUI_COMP_TF;
    comp->core.comp.tf.text = $3;
    comp->core.comp.tf.range = $4;

    calcLabelWidth(comp->label);

    $$ = comp;
}
;

Range :
/*
FLOATVAL COMMA FLOATVAL |
INTVAL   COMMA INTVAL
*/
COLON String { $$ = $2; }
| /* empty */ { $$ = NULL; }
;

/*--------------------------------------------------------*/

TextArea :
WidgetIdentifier COLON QSTRING {
    phGuiComponent_t *comp;

    comp = $1;
    comp = $1;
    comp->type = PH_GUI_COMP_TA;
    comp->core.comp.ta.text = $3;
    
    $$ = comp;
}
;

/*--------------------------------------------------------*/

Text :
WidgetIdentifier {
    phGuiComponent_t *comp;

    comp = $1;
    comp->type = PH_GUI_COMP_TL;
    
    $$ = comp;
}
;

/*--------------------------------------------------------*/

MultiLineText :
WidgetIdentifier {
    phGuiComponent_t *comp;

    comp = $1;
    comp->type = PH_GUI_COMP_ML;
    
    $$ = comp;
}
;

/*--------------------------------------------------------*/

OptionMenu :
WidgetIdentifier TextList COLON INTVAL {
    phGuiComponent_t *comp;

    comp = $1;
    comp->type = PH_GUI_COMP_OM;
    comp->core.comp.om.textList = $2;
    comp->core.comp.om.selected = $4 - 1;

    calcLabelWidth(comp->label);

    $$ = comp;
}
;

TextList :
BRK1_OPEN ListElement BRK1_CLOSE { $$ = $2; }
;

ListElement :
ListEntry |
ListEntry ListElement { $1->next = $2; }
;

ListEntry :
QSTRING {
    phGuiTextList_t *textList;
    
    textList = (phGuiTextList_t *) malloc(sizeof(phGuiTextList_t));
    textList->entry = $1;
    textList->next = NULL;

    calcLabelWidth($1);

    $$ = textList;
}
;

/*--------------------------------------------------------*/

FileBrowser :
WidgetIdentifier Filter Selection TypeMask {
    phGuiComponent_t *comp;
    comp = $1;
    comp->type = PH_GUI_COMP_FB;
    comp->core.comp.fb.filter = $2;
    comp->core.comp.fb.selection = $3;
    comp->core.comp.fb.typeMask = $4? XmFILE_DIRECTORY : XmFILE_REGULAR;

    calcLabelWidth(comp->label);

    $$ = comp;
}
;

Filter :
COLON QSTRING { $$ = $2; } |
COLON { $$ = NULL; }
;

Selection :
COLON QSTRING { $$ = $2; } |
COLON { $$ = NULL; }
;

TypeMask :
COLON DIRMASK { $$ = True;  } |
/* empty */   { $$ = False; }
;

/*--------------------------------------------------------*/

Glue :
/* empty */
{
    phGuiComponent_t *comp;
    comp = createComponent(NULL, NULL);
    comp->type = PH_GUI_COMP_GL;

    $$ = comp;
}
;

/*--------------------------------------------------------*/

WidgetIdentifier :
COLON String COLON String { 
    $$ = createComponent($2, $4);
}
;

String :
QSTRING
| /* empty */ {
/*
    char *text;

    text = (char *) malloc(sizeof(char));
    text[0] = 0;

    $$ = text;
*/
    $$ = NULL;
}
;

Enabled :
TILDE { $$ = False; }
| /* empty */ { $$ = True; }
;

Orientation :
ORIENTATION
| /* empty */ { $$ = PH_GUI_HORIZONTAL; }
;

ExitButton :
COLON EXITBUTTON { $$ = True; }
| 
/* empty */ { $$ = False; }
;

DefaultInt :
COLON INTVAL { $$ = $2; }
;

Columns :
COLON INTVAL { $$ = $2; }
;

%%

/*--- yacc parser related functions -----------------------------------------*/

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 1 Dec 1999, Ulrich Frank, created
 *
 * Description: 
 *
 ***************************************************************************/
phGuiComponent_t *phGuiParseConf(
    phGuiProcessId_t process,
    char *text                          /* attributes text to be parsed */
)
{
    int parseError;

    phGuiParserProcess = process;

    parseResult = NULL;
    phGuiLexInit(text);
    parseError = yyparse();

    return parseError? NULL : parseResult; 
}

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 1 Dec 1999, Ulrich Frank, created
 *
 * Description: 
 *
 ***************************************************************************/
phGuiComponent_t *createComponent(
    char *name,
    char *label
) {
    phGuiComponent_t *comp;

    comp = (phGuiComponent_t *) malloc(sizeof(phGuiComponent_t));
/*Begin of Huatek Modification, Donnie Tu, 03/26/2002*/
/*Issue Number: 274 */
   memset(comp,0,sizeof(phGuiComponent_t));
/*End of Huatek Modification*/
    comp->w = comp->labelw = comp->frame = NULL;
    comp->name  = name; 
    comp->label = label; 
    comp->prevSibling = comp->nextSibling = 
	comp->firstChild = comp->lastChild = NULL;
    comp->tooltip.on = False;
    comp->tooltip.labelString = NULL;

    return comp;
}

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 1 Dec 1999, Ulrich Frank, created
 *
 * Description: 
 *
 ***************************************************************************/
phGuiComponent_t *createGroup(
    phGuiComponentType_t type,
    char *name,
    char *label,
    phGuiComponent_t *firstChild
) {
    phGuiComponent_t *group;
    phGuiComponent_t *dummy;
    int nr;

    group = createComponent(name, label);
    group->type = type;
    group->enabled = True;

    nr = 0;
    dummy = group->firstChild = firstChild;
    if (dummy != NULL) {
	dummy->nr = ++nr;
	dummy->parent = group;
	while (dummy->nextSibling != NULL) {
	    dummy = dummy->nextSibling;
	    dummy->parent = group;
	    dummy->nr = ++nr;
	}
    }
    group->lastChild = dummy;

    return group;
}

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 1 Dec 1999, Ulrich Frank, created
 *
 * Description: 
 *
 ***************************************************************************/
void addSibling(phGuiComponent_t *member, phGuiComponent_t *sibling) {
    member->nextSibling = sibling;
    sibling->prevSibling = member;
}


/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 1 Dec 1999, Ulrich Frank, created
 *
 * Description: 
 *
 ***************************************************************************/
void calcLabelWidth(char *label) {
    XmString string;
    Dimension width;

/*
    widget = XtVaCreateWidget(
	    label,
	    xmLabelGadgetClass, 
	    phGuiParserProcess->toplevel->frame, 
	    NULL);
    XtVaGetValues(widget, XmNwidth, &width, NULL);
*/
    string = XmStringCreateLtoR(label, XmFONTLIST_DEFAULT_TAG);
    width = XmStringWidth(phGuiParserProcess->fontList, string);
    XmStringFree(string);    

    phGuiLabelWidth = phGuiLabelWidth < width? width : phGuiLabelWidth;
}


/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 1 Dec 1999, Ulrich Frank, created
 *
 * Description: 
 *
 ***************************************************************************/
static void yyerror(char *error)
{
/*    printf("error in parsing\n");*/
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
