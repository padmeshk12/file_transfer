/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : sparsematrice.h
 * CREATED  : 11 Apr 2000
 *
 * CONTENTS : Implementation of arbitrary sparse matrices
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 11 Apr 2000, Michael Vogt, created
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

#ifndef _SPARSEMATRICE_H_
#define _SPARSEMATRICE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

typedef struct sparseMatrice *phMatrice_t;

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
 * <give a SINGLE LINE function description here>
 *
 * Authors: <your name(s)>
 *
 * Description:
 * <Give the detailed function description here.>
 * <It may cover multiple lines.>
 *
 * <Blank lines, like the one just ahead, may be used to separate paragraphs>
 *
 * Input parameter format: 
 * <Describe the format of the input parameter string here>
 *
 * Output result format:
 * <Describe the format of the output parameter string here>
 * 
 * Warnings: <delete this, if not used>
 *
 * Notes: <delete this, if not used>
 *
 * Returns: <the function's return value, delete this if void>
 *
 ***************************************************************************/
phMatrice_t initMatrice(
    int minx                            /* minimum x value, may be negative */,
    int miny                            /* minimum y value, may be negative */,
    int maxx                            /* maximum x value, may be negative */,
    int maxy                            /* maximum y value, may be negative */,
    long expectedSize                   /* expected number of elements
                                           to be stored in the
                                           matice. This value in
                                           conjunction with the
                                           minimum and maximum values
                                           defines the internal
                                           implementation algorithm to
                                           be as space efficient as
                                           possible */,
    size_t elementSize                  /* size of one matrice element */,
    void *defaultElement                /* default empty element, used
                                           for dynamic matrix growth */
);

void *getElementRef(
    phMatrice_t m                       /* matrice to operate on */,
    int x                               /* coordinate */,
    int y                               /* coordinate */
);

#endif /* ! _SPARSEMATRICE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
