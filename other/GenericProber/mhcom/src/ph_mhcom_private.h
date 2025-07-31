/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_mhcom_private.h
 * CREATED  : 17 Jun 1999
 *
 * CONTENTS : Private definitions for mhcom module
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jun 1999, Chris Joyce, created
 *            7 May 2015, Magco Li, add code to support RS232 communication
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

#ifndef _PH_MHCOM_PRIVATE_H_
#define _PH_MHCOM_PRIVATE_H_

#ifdef __C2MAN__
#include "ph_mhcom.h"
#include "ph_gpib_mhcom.h"
#include "ph_lan_mhcom.h"
#include "ph_rs232_mhcom.h"
#include "gioBridge.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL  PHLOG_TYPE_MESSAGE_2
#define LOG_DEBUG   PHLOG_TYPE_MESSAGE_3
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_4

/*--- typedefs --------------------------------------------------------------*/

struct phComStruct
{
    struct phComStruct *myself;
    phComMode_t mode;                   /* mhcom mode online/offline */
    phSiclInst_t gioId;                /* ID from gioBridge iopen */
    phLogId_t loggerId;                 /* The data logger for errors/msgs */

    phComIfcType_t intfc;               /* selects interface, so that
                                           one of the following
                                           structures is valid: */

    struct phComGpibStruct *gpib;       /* pointer to all GPIB specific data */
    struct phComLanStruct  *lan;        /* pointer to all LAN specific data */
    struct phComRs232Struct *rs232;     /* pointer to all RS232 specific data */
};


#endif /* ! _PH_MHCOM_PRIVATE_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
