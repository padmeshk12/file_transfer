%{
/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : config.y
 * CREATED  : 26 May 1999
 *
 * CONTENTS : Scanner definition for configuration and attribute files
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 May 1999, Michael Vogt, created
 *            16 Jun 1999, Michael Vogt, finalyze first version
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
#include <locale.h>
#include <ctype.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_conf.h"

#include "semantic.h"
#include "scanner.h"
#include "phConfConfYy.c-parser.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#undef yylex
#define yylex phConfLexYylex

#undef yylval
#define yylval phConfLexYylval

#undef YYLMAX
#define YYLMAX 4000

#undef YY_INPUT
#define YY_INPUT(buffer, result, maxsize) \
  { \
     int c = phConfLexInput(); \
     result = (c == '\0') ? YY_NULL : (buffer[0] = c, 1); \
  }

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/
static int phConfLexInput(void);
/*Begin of Huatek Modifications, Luke Lan, 02/20/2001 */
/*Issue Number:  36                                   */
/* This function has declared in string.h file, so comment it here */
/* extern char *strdup(const char *);  */
/*End of Huatek Modifications.     */

/*--- static global variables functions -------------------------------------*/

static const char *phConfLexPointer = NULL;
static int phConfLexLine = 1;
static int phConfLexPos = 0;
static int phConfLexLastLineCount = 0;

YYSTYPE phConfLexYylval;

/*--- lex definitions -------------------------------------------------------*/

%}

%option noyywrap

digit           [0-9]
sign            [+-]
letter          [A-Za-z]
inqstring       [^'\n]
indqstring      [^"\n]
inhstring       [^}]
inkstring       [^"'}: \t\n]
exp             [Ee][-+]?[0-9]+
white           [ \t\n]
free            [^\n]

%%

({sign})?({digit})+({exp})?              |
({sign})?({digit})+"."({digit})*({exp})? |
({sign})?({digit})*"."({digit})+({exp})? {
    /* get the float number out of the text portion */
    phConfLexYylval.num_val = atof((char *) yytext);

    return(NUMBER);
}

\?                                       |
\?\-\?                                   |
\?\-({digit})+                           |
({digit})+\-\?                           |
({digit})+\-({digit})+                   {
    char *first;
    char *second;
    char *minus;

    /* get list size information out of text portion */
    if (strcmp((char *) yytext, "?") == 0)
    {
	phConfLexYylval.size_val.minCount = 0;
	phConfLexYylval.size_val.maxCount = -1;
    }
    else
    {
	minus = strchr((char *) yytext, (int) '-');
	first = (char *) yytext;
	second = minus+1;
	*minus = '\0';

	if (strcmp(first, "?") == 0)
	    phConfLexYylval.size_val.minCount = 0;
	else
	    phConfLexYylval.size_val.minCount = atoi(first);

	if (strcmp(second, "?") == 0)
	    phConfLexYylval.size_val.maxCount = -1;
	else
	    phConfLexYylval.size_val.maxCount = atoi(second);

	*minus = '-';
    }

    return(LSIZE);
}

\"{indqstring}*\" {
    /* copy the string and get rid of the double quotes */
    phConfLexYylval.str_val = strdup((char *) yytext+1);
    phConfLexYylval.str_val[strlen(phConfLexYylval.str_val)-1] = '\0';

    return(QSTRING);
}

\'{inqstring}*\' {
    /* copy the string and get rid of the double quotes */
    phConfLexYylval.str_val = strdup((char *) yytext+1);
    phConfLexYylval.str_val[strlen(phConfLexYylval.str_val)-1] = '\0';

    return(QSTRING);
}

\{{inhstring}*\} {
    /* begin CR 9160 (line above!): The hint string may also be emtpy now */

    /* A hint string may pass over several lines and may start and end
       with additional white space characters. Delete the surrounding
       braces, and all white space next to the braces. Copy the string. */

    char *start;
    char *end;

    /* find first non white character after opening brace */
    start = (char *) yytext + 1;
    while (isspace(*start)) 
        start++;

    /* copy */
    phConfLexYylval.str_val = strdup(start);

    /* delete all white space character before closing brace */
    end = &phConfLexYylval.str_val[strlen(phConfLexYylval.str_val)] - 1;
    *end = '\0';
    while (end > phConfLexYylval.str_val && isspace(*(end-1))) 
    /* end CR 9160 */
    { 
        end--; 
        *end = '\0'; 
    }

    return(HSTRING);
}

{letter}{inkstring}+{white}*\: {
    /* A key string starts with a letter and ends with colon. There
       may be additional white spaces before the colon. Delete all
       white space next to the colon. Copy the string. */

    char *end;

    /* copy */
    phConfLexYylval.str_val = strdup((char *) yytext);

    /* delete all white space character before colon */
    end = &phConfLexYylval.str_val[strlen(phConfLexYylval.str_val)] - 1;
    *end = '\0';
    while (isspace(*(end-1))) 
    { 
        end--; 
        *end = '\0'; 
    }

    return(KSTRING);
}

{letter}                                { return(toupper(yytext[0])); }

\#{free}*                               |
{white}*                                ;

.                                       { return(yytext[0]); }

%%

/*--- lex scanner related functions -----------------------------------------*/
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
static int phConfLexInput(void)
{
    if (*phConfLexPointer == '\n')
    {
	phConfLexLine++;
	phConfLexLastLineCount = phConfLexPos;
	phConfLexPos=0;
    }
    else if (*phConfLexPointer == '\t')
    {
	do
	    phConfLexPos++;
	while (phConfLexPos % 8 != 0);
    }
    else
	phConfLexPos++;

    return ((int) *phConfLexPointer++);
}

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
void phConfLexInit(const char *text)
{
#ifndef FLEX_SCANNER
    setlocale(LC_ALL, yylocale);
#endif
    phConfLexLine = 1;
    phConfLexPos = 0;
    phConfLexLastLineCount = 0;
    phConfLexPointer = text;
}

/*****************************************************************************
 *
 * Report current scan position
 *
 * Authors: Michael Vogt
 *
 * History: 26 May 1999, Michael Vogt, created
 *
 * Description: 
 * please refer to scanner.h
 *
 ***************************************************************************/
void phConfLexPosition(int *line, int *pos)
{
    *line = phConfLexLine;
    *pos = phConfLexPos;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
