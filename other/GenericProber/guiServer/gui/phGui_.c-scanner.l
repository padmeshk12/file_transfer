%{
/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : lexer.l
 * CREATED  : 26 May 1999
 *
 * CONTENTS : Scanner definition for 
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
#include <string.h>
#include <locale.h>

/*--- module includes -------------------------------------------------------*/

#include "GuiComponent.h"
#include "phGui_.c-parser.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define yylval phGui_lval

#undef YY_INPUT
#define YY_INPUT(buffer, result, maxsize) \
  { \
     int c = phGuiLexInput(); \
     result = (c == '\0') ? YY_NULL : (buffer[0] = c, 1); \
  }

#undef output
#define output(c)   phGuiLexOutput(c)

#undef YYLMAX
#define YYLMAX 4000

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/
static int phGuiLexInput(void);
static void phGuiLexOutput(int c);

/*--- global variables ------------------------------------------------------*/

static char *phGuiLexPointer = NULL;
static int phGuiLexPos = 0;

/*--- lex definitions -------------------------------------------------------*/

%}
 
digit           [0-9]

sign            [+-]
letter          [A-Za-z]
inqstring       [^`]
inrstring       [^\^]
exp             [Ee][-+]?[0-9]+
white           [ \t]
eol             [\n]

%%

{white} |
{eol} {
}

({sign})?({digit})+({exp})? {
    yylval.int_v = atol((char *) yytext);

    return(INTVAL);
}

({sign})?({digit})+({exp})?              |
({sign})?({digit})+"."({digit})*({exp})? {
    yylval.float_v = atof((char *) yytext);

    return(FLOATVAL);
}

\`{inqstring}*\` {
    /* copy the string and get rid of the quotes */

    yylval.string_v = strdup((char *) yytext + 1);
    yylval.string_v[strlen(yylval.string_v) - 1] = 0;

    return(QSTRING);
}

\^{inrstring}*\^ {
    /* copy the string and get rid of the "^" */

    yylval.string_v = strdup((char *) yytext + 1);
    yylval.string_v[strlen(yylval.string_v) - 1] = 0;

    return(QSTRING);
}

"," {
    return(COMMA);
}


":" {
    return(COLON);
}


"*" {
    return(STAR);
}


"~" {
    return(TILDE);
}


"(" {
    return(BRK1_OPEN);
}


")" {
    return(BRK1_CLOSE);
}


"{" {
    return(BRK2_OPEN);
}


"}" {
    return(BRK2_CLOSE);
}


"[" {
    return(BRK3_OPEN);
}


"]" {
    return(BRK3_CLOSE);
}


@h | 
@v {
    yylval.int_v = yytext[1] == 'h'? PH_GUI_HORIZONTAL : PH_GUI_VERTICAL;
    return(ORIENTATION);
}


S {
    return (SIMPLEGROUP);
}


R {
    return (RBGROUP);
}


t {
    return (TOGGLEBUTTON);
}


T {
    return (TBGROUP);
}


l {
    return (TEXT);
}


L {
    return (MULTILINETEXT);
}


p {
    return (PUSHBUTTON);
}


f {
    return (TEXTFIELD);
}


a {
    return (TEXTAREA);
}


o {
    return (OPTIONMENU);
}


b {
    return (FILEBROWSER);
}


e {
    return (EXITBUTTON);
}


s {
    return (SELECTED);
}


D {
    return (DIRMASK);
}

. {
    return (NOTOKEN);
}

%%

/*--- lex scanner related functions -----------------------------------------*/
/*
main () {
    int token;

    while(token = yylex()) {
	switch (token) {
	  case NOTOKEN:
	    printf("Error in %s\n", tokenval);
	    break;
	  case INTVAL:
	    printf("(INTVAL) %s\n", tokenval);
	    break;
	  case FLOATVAL:
	    printf("(FLOATVAL) %s\n", tokenval);
	    break;    
	  case QSTRING:
	    printf("(QSTRING) %s\n", tokenval);
	    break;    
	  case ORIENTATION:
	    printf("(ORIENTATION)%s\n", tokenval);
	    break;    
	  case PUSHBUTTON:
	    printf("(PUSHBUTTON)%s\n", tokenval);
	    break;    
	  case RADIOBUTTON:
	    printf("(RADIOBUTTON)%s\n", tokenval);
	    break;    
	  case RBGROUP:
	    printf("(RBGROUP)%s\n", tokenval);
	    break;    
	  case TOGGLEBUTTON:
	    printf("(TOGGLEBUTTON)%s\n", tokenval);
	    break;    
	  case TBGROUP:
	    printf("(TBGROUP)%s\n", tokenval);
	    break;    
	  case TEXTFIELD:
	    printf("(TEXTFIELD)%s\n", tokenval);
	    break;    
	  case TEXTAREA:
	    printf("(TEXTAREA)%s\n", tokenval);
	    break;    
	  case OPTIONMENU:
	    printf("(OPTIONMENU)%s\n", tokenval);
	    break;    
	  case FILEBROWSER:
	    printf("(FILEBROWSER)%s\n", tokenval);
	    break;    
	  case COMMA:
	    printf("(COMMA)%s\n", tokenval);
	    break;    
	  case STAR:
	    printf("(STAR)%s\n", tokenval);
	    break;    
	  case BRK1_OPEN:
	    printf("(BRK1_OPEN)%s\n", tokenval);
	    break;    
	  case BRK1_CLOSE:
	    printf("(BRK1_CLOSE)%s\n", tokenval);
	    break;    
	  case BRK2_OPEN:
	    printf("(BRK2_OPEN)%s\n", tokenval);
	    break;    
	  case BRK2_CLOSE:
	    printf("(BRK2_CLOSE)%s\n", tokenval);
	    break;    
	  case BRK3_OPEN:
	    printf("(BRK3_OPEN)%s\n", tokenval);
	    break;    
	  case BRK3_CLOSE:
	    printf("(BRK3_CLOSE)%s\n", tokenval);
	    break;    
	  case EXITBUTTON:
	    printf("(EXITBUTTON)%s\n", tokenval);
	    break;    
	  case SELECTED:
	    printf("(SELECTED)%s\n", tokenval);
	    break;    
	}
    }
}
*/

/*****************************************************************************
 *
 * Initialize the lexer with new buffer of input data
 *
 * Authors: Michael Vogt
 *
 * History: 26 May 1999, Michael Vogt, created
 *
 * Description: 
 * please refer to scanner.h
 *
 ***************************************************************************/
void phGuiLexInit(char *text)
{
#ifndef FLEX_SCANNER
    setlocale(LC_ALL, yylocale);
#endif

    phGuiLexPointer = text;
}
/*****************************************************************************
 *
 * Get next scan character
 *
 * Authors: Michael Vogt
 *
 * History: 26 May 1999, Michael Vogt, created
 *
 * Description: 
 * Scanner calls this function to get the next character out of the text
 * buffer. Line and column count needs to be adjusted.
 *
 ***************************************************************************/
static int phGuiLexInput(void)
{
/*
    if (*phGuiLexPointer == '\n')
    {
        phGuiLexLine++;
        phGuiLexLastLineCount = phGuiLexPos;
        phGuiLexPos=0;
    }
    else */
        phGuiLexPos++;

    return ((int) *phGuiLexPointer++);
}

/*****************************************************************************
 *
 * Put character
 *
 * Authors: Michael Vogt
 *
 * History: 26 May 1999, Michael Vogt, created
 *
 * Description: 
 * Scanner will call this function to output characters
 * that can not be scanned by the scanner's definition. For the
 * current application, this would be a fault. Currently, this
 * characters are gracefully ignored.
 *
 ***************************************************************************/
static void phGuiLexOutput(int c)
{
    /* do nothing. Don't write any output characters. This function
       should never be called! */

    /* putc(c, yyout); */
}

/*****************************************************************************
 *
 * Authors: Ulrich Frank
 *
 * History: 30 November 1999, Ulrich Frank, created
 *
 * Description: 
 *
 ***************************************************************************/
int yywrap(void)
{
    return(1);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
