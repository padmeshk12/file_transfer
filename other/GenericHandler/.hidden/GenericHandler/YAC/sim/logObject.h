/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : logObject.h
 * CREATED  : 19 May 1999
 *
 * CONTENTS : Interface header for log object base class
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce, created
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

#ifndef _LOGOBJECT_H_
#define _LOGOBJECT_H_

/*--- system includes -------------------------------------------------------*/

#include <string>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_log.h"

/*--- defines ---------------------------------------------------------------*/


#define LOG_NORMAL         PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG          PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE        PHLOG_TYPE_MESSAGE_2



/*--- typedefs --------------------------------------------------------------*/


/*--- class definition  -----------------------------------------------------*/

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
 * Log Object base class
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Object with the ability to send messages to log files.
 *
 *****************************************************************************/

class LogObject
{
public:
    LogObject(phLogId_t l);
    LogObject(int debugLevel);
protected:
    phLogId_t        logId;
private:
    phLogMode_t getLogMode(int debugLevel);
};


/*****************************************************************************
 *
 * Exception base class
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Base class from which all simulator exceptions may be derived.
 *
 *****************************************************************************/

class Exception
{
public:
    virtual void printMessage()=0;
};


#endif /* ! _SIM_LOGOBJECT_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
