/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_hfunc_private.h
 * CREATED  : 15 Jul 1999
 *
 * CONTENTS : Private definitions for driver plugin
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Jul 1999, Michael Vogt, created
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

#ifndef _PH_HFUNC_PRIVATE_H_
#define _PH_HFUNC_PRIVATE_H_

/*--- system includes -------------------------------------------------------*/

/*--- module includes -------------------------------------------------------*/

#include "gpib_conf.h"
#include "transaction.h"
#include "driver_defaults.h"
#include "private.h"
#include "ph_tcom.h"

/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL  PHLOG_TYPE_MESSAGE_1
#define LOG_DEBUG   PHLOG_TYPE_MESSAGE_2
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_3

/*--- typedefs --------------------------------------------------------------*/

enum proberInterface
{
    PHPFUNC_IF_GPIB,
    PHPFUNC_IF_RS232
};

struct rs232Struct{
    /* to be defined */
    int dummy;
};


/* do not change this struct. All plugin specific enhancements should
   go into the struct pluginPrivate definition from private.h. This
   struct is part of the struct phPFuncStruct below (see component <p> */

struct phPFuncStruct
{
    struct phPFuncStruct     *myself;
    phTcomId_t               myTcom;
    phLogId_t                myLogger;
    phConfId_t               myConf;
    phComId_t                myCom;
    phEstateId_t             myEstate;

    int                      simulationMode;

    enum proberModel         model;
    char                     *cmodelName; /* configured model name */

    long                     heartbeatTimeout;

    int                      noOfSites;
    int                      activeSites[PHESTATE_MAX_SITES];
    const char               *siteIds[PHESTATE_MAX_SITES];
    
    int                      noOfBins;
    const char               **binIds;

    struct transaction       *ta;

    enum proberInterface     interfaceType;
    union
    {
	struct gpibStruct    gpib;
	struct rs232Struct   rs232;
    }                        u;

    struct pluginPrivate     p;     /* definition see private.h */
};

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

#endif /* ! _PH_HFUNC_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
