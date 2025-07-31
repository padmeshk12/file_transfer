/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : sparsematrice.c
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

#include "ph_tools.h"
#include "sparsematrice.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/
#define RELEASE_BUFFER(x) {\
   if ( x != NULL ) \
   {\
     free(x);\
     x = NULL;\
   }\
}

/*--- typedefs --------------------------------------------------------------*/

enum matriceAlg { M_ARRAY, M_LISTS };

struct matriceArray
{
    char **allRows;
};

struct matriceListsElement
{
    int index;
    void *data;
    struct matriceListsElement *next;
};

struct matriceLists
{
    struct matriceListsElement *rows;
};

struct sparseMatrice
{
    /* common data */
    int minx;
    int miny;
    int maxx;
    int maxy;
    size_t elemSize;
    void *defaultElement;

    /* algorithm specific data */
    enum matriceAlg algorithm;
    union
    {
	struct matriceArray array;
	struct matriceLists lists;
    } a;
};

/*--- functions -------------------------------------------------------------*/

static int initArrayMatrice(struct sparseMatrice *m)
{
    int rows;
    int cols;
    int i, j;

    rows = m->maxy - m->miny + 1;
    cols = m->maxx - m->minx + 1;

    m->a.array.allRows = (char **) malloc(rows * sizeof(char *));
    if (!m->a.array.allRows)
	return 0;

    for (i=0; i<rows; i++)
    {
	m->a.array.allRows[i] = (char *) malloc(cols * m->elemSize);
	if (!m->a.array.allRows[i])
	    return 0;
	for (j=0; j<cols; j++)
	{
	    memcpy(&(m->a.array.allRows[i][j*m->elemSize]), m->defaultElement, m->elemSize);
	}
    }

    return 1;
}

static void *getArrayMatrice(struct sparseMatrice *m, int x, int y)
{
    return (void *) &(m->a.array.allRows[y-m->miny][m->elemSize*(x-m->minx)]);
}

/*---------------------------------------------------------------------------*/

static int initListsMatrice(struct sparseMatrice *m)
{
    m->a.lists.rows = NULL;
    return 1;
}

static void *getListsMatrice(struct sparseMatrice *m, int x, int y)
{
    struct matriceListsElement **row;
    struct matriceListsElement **col;

    struct matriceListsElement *tmp;
    
    row = &(m->a.lists.rows);
    
    while (*row && (*row)->index < y)
	row = &((*row)->next);

    if (!(*row) || (*row)->index != y)
    {
	/* make a new row */
	tmp = PhNew(struct matriceListsElement);
	tmp->index = y;
	tmp->data = NULL;
	tmp->next = *row;
	*row = tmp;
    }

    col = (struct matriceListsElement **) &((*row)->data);

    while (*col && (*col)->index < x)
	col = &((*col)->next);

    if (!(*col) || (*col)->index != x)
    {
	/* make a new col */
	tmp = PhNew(struct matriceListsElement);
	tmp->index = x;
	tmp->data = NULL;
	tmp->next = *col;
	*col = tmp;
    }

    if (!(*col)->data)
    {
	/* create the data entry */
	(*col)->data = malloc(m->elemSize);
	memcpy((*col)->data, m->defaultElement, m->elemSize);
    }

    return (*col)->data;
}


/*---------------------------------------------------------------------------*/

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
)
{
    struct sparseMatrice *tm;

    tm = PhNew(struct sparseMatrice);
    if (!tm)
	return NULL;

    tm->minx = minx;
    tm->maxx = maxx;
    tm->miny = miny;
    tm->maxy = maxy;
    tm->elemSize = elementSize;
    tm->defaultElement = malloc(elementSize);
    memcpy(tm->defaultElement, defaultElement, elementSize);

    /* select best known algorithm */
    tm->algorithm = M_ARRAY;

    /* use lists, if it is not used more than 10% of the matrix */
    if ((long)(maxx - minx) * (long)(maxy - miny) > 10L * expectedSize)
	tm->algorithm = M_LISTS;

    /* initialize algorithm dependent part */
    switch (tm->algorithm)
    {
      case M_ARRAY:
	if (!initArrayMatrice(tm)) {
            RELEASE_BUFFER(tm->defaultElement);
            RELEASE_BUFFER(tm);
	    return NULL;
        }
	break;
      case M_LISTS:
	if (!initListsMatrice(tm)) {
            RELEASE_BUFFER(tm->defaultElement);
            RELEASE_BUFFER(tm);
	    return NULL;
        }
	break;
    }

    return tm;
}

void *getElementRef(
    phMatrice_t m                       /* matrice to operate on */,
    int x                               /* coordinate */,
    int y                               /* coordinate */
)
{
    void *location = NULL;

    /* check bounds */
    if (!m || x<m->minx || x>m->maxx || y<m->miny || y>m->maxy)
	return NULL;

    /* algorithm dependent retrieval of storage location */
    switch (m->algorithm)
    {
      case M_ARRAY:
	location = getArrayMatrice(m, x, y);
	break;
      case M_LISTS:
	location = getListsMatrice(m, x, y);
	break;
    }

    return location;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
