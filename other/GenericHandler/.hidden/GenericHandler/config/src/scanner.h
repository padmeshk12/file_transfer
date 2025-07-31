/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : scanner.h
 * CREATED  : 26 May 1999
 *
 * CONTENTS : Interface to lex scanner
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 May 1999, Michael Vogt, created
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

#ifndef _SCANNER_H_
#define _SCANNER_H_

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
 * Initialize the lexer with new buffer of input data
 *
 * Authors: Michael Vogt
 *
 * Description: 
 * Configurations and attributes may be parsed again and
 * again during process lifetime. The inout will not come from a file
 * but from regular C strings that already reside in memory. This
 * function resets the scanner to start scanning the provided text
 * buffer.
 *
 ***************************************************************************/
void phConfLexInit(
    const char *text                          /* text buffer to scan next */
);

/*****************************************************************************
 *
 * Report current scan position
 *
 * Authors: Michael Vogt
 *
 * Description: 
 * For error description, the parser which calls the
 * scanner needs to know the current scan position in the current text
 * buffer. For a new text buffer after phConfLexInit(3) the line count
 * starts with 1 and the column count starts with 0;
 *
 ***************************************************************************/
void phConfLexPosition(
    int *line                           /* returns current line position */,
    int *pos                            /* returns current column position */
);


/*****************************************************************************
 *
 * Do the scanning work
 *
 * Authors: lex
 *
 * Description: 
 * This is yylex
 *
 ***************************************************************************/
int phConfLexYylex(void);

#endif /* ! _SCANNER_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
