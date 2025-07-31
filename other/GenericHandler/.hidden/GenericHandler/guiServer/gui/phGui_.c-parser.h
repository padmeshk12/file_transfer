/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_PHGUI_Y_TAB_H_INCLUDED
# define YY_PHGUI_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int phGui_debug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    NOTOKEN = 258,
    COMMA = 259,
    COLON = 260,
    STAR = 261,
    TILDE = 262,
    BRK1_OPEN = 263,
    BRK1_CLOSE = 264,
    BRK2_OPEN = 265,
    BRK2_CLOSE = 266,
    BRK3_OPEN = 267,
    BRK3_CLOSE = 268,
    EXITBUTTON = 269,
    SELECTED = 270,
    DIRMASK = 271,
    ORIENTATION = 272,
    INTVAL = 273,
    FLOATVAL = 274,
    QSTRING = 275,
    SIMPLEGROUP = 276,
    RBGROUP = 277,
    TOGGLEBUTTON = 278,
    TBGROUP = 279,
    PUSHBUTTON = 280,
    TEXTFIELD = 281,
    TEXTAREA = 282,
    TEXT = 283,
    MULTILINETEXT = 284,
    OPTIONMENU = 285,
    FILEBROWSER = 286
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 71 "phGui_.c-parser.y" /* yacc.c:1909  */

    int                        int_v;
    float                    float_v;
    char                   *string_v;
    phGuiTextList_t      *textList_v;
    phGuiComponent_t      *guiComp_v;

#line 94 "y.tab.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE phGui_lval;

int phGui_parse (void);

#endif /* !YY_PHGUI_Y_TAB_H_INCLUDED  */
