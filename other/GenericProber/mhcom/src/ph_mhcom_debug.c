/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_mhcom_debug.c
 * CREATED  : Nov 1999
 *
 * CONTENTS : Debug of material handler communication layer
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jun 1999, Chris Joyce, created
 *
 * Instructions:
 *
 * 1) Copy this template to as many .c files as you require
 *
 * 2) Use the command 'make depend' to make visible the new
 *    source files to the makefile utility
 *
 *****************************************************************************/

#ifdef DEBUG

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

/*--- module includes -------------------------------------------------------*/


#include "ph_mhcom.h"

#include "gioBridge.h"
#include "ph_mhcom_private.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */


/*--- debug functions -------------------------------------------------------*/

static unsigned int lineNumber=1;


/*****************************************************************************
 * printDataInBinary
 *****************************************************************************/
static void printDataInBinary(unsigned long dataValue, int numOfPositions )
{
    int position=0;
    unsigned int bit;
    unsigned int mask;

    for ( position=numOfPositions-1 ; position >= 0 ; position-- )
    {

        mask = 1 << position;

        bit = dataValue & mask;

        printf("%d",
               bit ? 1 : 0 );
        if ( position % 4 == 0 )
        {
            printf(" ");
        }
    }
}


/*****************************************************************************
 * print60622InputLines
 *****************************************************************************/
static void print60622InputLines(unsigned long dataInput,
                                 unsigned long statusLines,
                                 unsigned long eirLine
)
{
    printf("dmonitor %3d) ",lineNumber);
    printf("0x%04X ",dataInput);
    printDataInBinary(dataInput, 16 );
    printf("  ");
    printDataInBinary(statusLines, 2 );
    printf("  ");
    printDataInBinary(eirLine, 1 );
}


/*****************************************************************************
 * print60622InputLines
 *****************************************************************************/
static void monitor60622InputLines(unsigned long dataInput,
                                   unsigned long statusLines,
                                   unsigned long eirLine
)
{
    static unsigned long PreviousDataInput;
    static unsigned long PreviousStatusLines;
    static unsigned long PreviousEirLine;

    if ( PreviousDataInput != dataInput ||
         PreviousStatusLines != statusLines ||
         PreviousEirLine != eirLine ||
         lineNumber == 1 )
    {
        PreviousDataInput=dataInput;
        PreviousStatusLines=statusLines;
        PreviousEirLine=eirLine;

        print60622InputLines(dataInput,statusLines,eirLine);
        printf("\n");
        lineNumber++;
    }
    fflush(stdout);
}


/*****************************************************************************
 * print61622InputLines
 *****************************************************************************/
static void print61622InputLines(unsigned long dataInput,
                                 unsigned long statusLines,
                                 unsigned long eirLine,
                                 unsigned long latches,
                                 unsigned long set_latches,
                                 unsigned long polarity,
                                 unsigned long pullups,
                                 unsigned long filter,
                                 unsigned long filterTime
)
{
    print60622InputLines(dataInput,statusLines,eirLine);

    printf(" L 0x%04X ",latches);
    printf(" SL 0x%04X ",set_latches);
    printf(" P 0x%04X ",polarity);
    printf(" PU %d ",(int)pullups);
    printf(" F 0x%04X ",filter);
    printf(" FT %d ",(int)filterTime);
}


/*****************************************************************************
 * print61622InputLines
 *****************************************************************************/
static void monitor61622InputLines(unsigned long dataInput,
                                   unsigned long statusLines,
                                   unsigned long eirLine,
                                   unsigned long latches,
                                   unsigned long set_latches,
                                   unsigned long polarity,
                                   unsigned long pullups,
                                   unsigned long filter,
                                   unsigned long filterTime
)
{
    static unsigned long PreviousDataInput;
    static unsigned long PreviousStatusLines;
    static unsigned long PreviousEirLine;
    static unsigned long PreviousLatches;
    static unsigned long PreviousSetLatches;
    static unsigned long PreviousPolarity;
    static unsigned long PreviousPullups;
    static unsigned long PreviousFilter;
    static unsigned long PreviousFilterTime;

    if ( PreviousDataInput != dataInput ||
         PreviousStatusLines != statusLines ||
         PreviousEirLine != eirLine ||
         PreviousLatches != latches ||
         PreviousSetLatches != set_latches ||
         PreviousPolarity != polarity ||
         PreviousPullups != pullups ||
         PreviousFilter != filter ||
         PreviousFilterTime != filterTime ||
         lineNumber == 1 )
    {

        PreviousDataInput=dataInput;
        PreviousStatusLines=statusLines;
        PreviousEirLine=eirLine;
        PreviousLatches=latches;
        PreviousSetLatches=set_latches;
        PreviousPolarity=polarity;
        PreviousPullups=pullups;
        PreviousFilter=filter;
        PreviousFilterTime=filterTime;

        print61622InputLines(dataInput,
                             statusLines,
                             eirLine,
                             latches,
                             set_latches,
                             polarity,
                             pullups,
                             filter,
                             filterTime);
        printf("\n");
        lineNumber++;
    }
    fflush(stdout);
}

#endif /* defined DEBUG */
