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
 * CONTENTS : Parser definition for configuration files
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

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"
#include "ph_conf.h"

#include "semantic.h"
#include "scanner.h"
#include "config.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#ifdef DEBUG
#define YYDEBUG 1
#endif

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

#ifdef YYDEBUG
extern int phConfConfYydebug;
#endif

static phLogId_t myLogger = NULL;
static struct defList *parseResult = NULL;

static void yyerror(const char *error);
static int yylex(void);
%}

/*--- yacc definitions ------------------------------------------------------*/

/* ATTENTION: Since the same scanner is used for this grammar
   (config.y) and the attributes grammar (attrib.y), the following
   union definition and the token definitions must be equal in both
   files config.y and attrib.y: */

%union
{
    double                 num_val;
    char                   *str_val;
    struct keyStrRng       *strCh_val;
    enum numType           prec_val;
    phConfFlag_t           flag_val;
    struct keyNumRng       *numRg_val;
    struct keyData         *data_val;
    struct keyedDefinition *def_val;
    struct defList         *tree_val;
    struct listSize        size_val;
}

%token <str_val>           HSTRING QSTRING KSTRING 
%token <num_val>           NUMBER
%token <size_val>          LSIZE

%type <tree_val>           configuration
%type <def_val>            definition
%type <data_val>           data array datalist simpledata
%type <flag_val>           flags flaglist oneflag
%type <str_val>            hint key quotedstring

/*--- yacc grammar ----------------------------------------------------------*/

%%

configuration: 
definition 
{ $$ = (parseResult = phConfSemAddDefinition(NULL, $1)); }
| configuration definition
{ $$ = (parseResult = phConfSemAddDefinition($1, $2)); }

definition: 
hint key flags data
{ $$ = phConfSemMakeDefinition($1, $2, $3, $4); }

hint: 
HSTRING 
{ $$ = $1; }
| /* empty */
{ $$ = NULL; }

key: 
KSTRING
{ $$ = $1; }

flags: 
'<' flaglist '>' 
{ $$ = $2; }
| /* empty */
{ $$ = PHCONF_FLAG_EMPTY; }

data: 
array 
{ $$ = $1; }
| simpledata
{ $$ = $1; }

array: 
'[' datalist ']'
{ $$ = $2; }
| '[' ']'
{ $$ = phConfSemEmptyList(); }

datalist: 
data 
{ $$ = phConfSemAddData(NULL, $1); }
| datalist data
{ $$ = phConfSemAddData($1, $2); }

simpledata: 
NUMBER 
{ $$ = phConfSemMakeNumVal($1); }
| quotedstring
{ $$ = phConfSemMakeStrVal($1); }

quotedstring: 
QSTRING
{ $$ = $1; }

flaglist: 
oneflag 
{ $$ = $1; }
| flaglist oneflag
/* Begin of Huatek Modification, Donnie Tu, 2001/11/26*/
/* Issue Number: 305 */
{ $$ =  phConfFlag_t($1 | $2); }
/* End of Huatek Modification */

oneflag: 
'F'
{ $$ = PHCONF_FLAG_FIXED; }

%%

/*--- yacc parser related functions -----------------------------------------*/

/* this declaration can not be put to the very beginning, since
   YYSTYPE is not known there... */
extern YYSTYPE phConfLexYylval;         /* global variable for
					   communication with the scanner */

/*****************************************************************************
 *
 * Report a parse error
 *
 * Authors: Michael Vogt
 *
 * History: 17 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Send the parse error to the logger.
 *
 * Note: the name of this function will be replaced by a CPP
 * definition created by yacc
 *
 ***************************************************************************/
static void yyerror(const char *error)
{
    int line, pos;
    phConfLexPosition(&line, &pos);

    phLogConfMessage(myLogger, PHLOG_TYPE_ERROR, 
		     "configuration error near line %d, position %d: %s\n", 
		     line, pos, error);
}

/*****************************************************************************
 *
 * Parsers call to lex
 *
 * Authors: Michael Vogt
 *
 * History: 17 Jun 1999, Michael Vogt, created
 *
 * Description: We don't call the lexer directly, since a common lexer
 * is used for two different parsers. The lexers main function has a
 * different name!
 *
 * Note: the name of this function will be replaced by a CPP
 * definition created by yacc
 *
 ***************************************************************************/
static int yylex(void)
{
    int result;
   
    result = phConfLexYylex();
    yylval = phConfLexYylval;

    return result;
}

/*****************************************************************************
 *
 * Parse a configuration string
 *
 * Authors: Michael Vogt
 *
 * History: 17 Jun 1999, Michael Vogt, created
 *
 * Description: 
 * Please refer to config.h
 *
 ***************************************************************************/
struct defList *phConfConfParse(
    char *text                          /* configuration text to be parsed */, 
    phLogId_t logger                    /* diagnostic output goes here */
)
{
    int parseError;

#ifdef DEBUG
    if (getenv("DEBUG_CONF_PARSER"))
	phConfConfYydebug = 1;
    else
	phConfConfYydebug = 0;
#endif

    myLogger = logger;
    parseResult = NULL;
    phConfLexInit(text);
    parseError = yyparse();
    myLogger = NULL;

    if (parseError)
    {
	phConfSemDeleteDefinition(parseResult);
	parseResult = NULL;
    }

    return parseResult;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
