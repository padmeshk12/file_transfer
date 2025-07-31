/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : gpibComm.h
 * CREATED  : 18 Jan 2001
 *
 * CONTENTS : Seiko-Epson status
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

#ifndef _GPIB_COMM_
#define _GPIB_COMM_

/*--- system includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "ph_mhcom.h"

/*--- module includes -------------------------------------------------------*/

#include "logObject.h"

/*--- defines ---------------------------------------------------------------*/



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
 * Simulator Gpib communication class exceptions
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/

class CommErrorException : public Exception
{
public:
    CommErrorException(phComError_t e);
    virtual void printMessage();
protected:
    phComError_t comError;
};
 



/*****************************************************************************
 *
 * Simulator Gpib communication class
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/

struct GpibCommSetup;

class GpibCommunication : public LogObject
{
public:
    // Communication error 
    enum commError { comm_ok,
                     comm_timeout,
                     comm_error };
    
    GpibCommunication(phLogId_t l, const GpibCommSetup&);
    ~GpibCommunication();

    commError sendEvent(phComGpibEvent_t *event, long timeout);
    commError send(char *h_string, int length, long timeout);
    commError receive(const char **message, int *length, long timeout);

private:
    commError convertPhCommErrorIntoCommError(phComError_t e);
    phComId_t        comId;
};


struct GpibCommSetup
{
    char* hpibDevice;
};



#endif /* ! _GPIB_COMM_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
