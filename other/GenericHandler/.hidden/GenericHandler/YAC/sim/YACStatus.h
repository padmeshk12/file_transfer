/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : YACStatus.h
 * CREATED  : 29 Dec 2006
 *
 * CONTENTS : YAC status
 *
 * AUTHORS  : Kun Xiao, STS R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Dec 2006, Kun Xiao, created
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

#ifndef _YAC_STATUS_H_
#define _YAC_STATUS_H_

/*--- system includes -------------------------------------------------------*/
#include <map>
#include <list>

/*--- module includes -------------------------------------------------------*/

#include "logObject.h"
#include "simHelper.h"
#include "status.h"

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
 * Status type Conversion Exception 
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Base class from which all simulator exceptions may be derived.
 *
 *****************************************************************************/

class StatusStringToEnumConversionException : public Exception
{
public:
    StatusStringToEnumConversionException(const char *errStr);
    virtual void printMessage();
private:
    Std::string convertStringError;
};



/*****************************************************************************
 *
 * YAC status
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/

class EventRecorder;
class YACStatusObserver;


class YACStatus : public LogObject
{
public:
    enum eYACStatus { stopped, alarm, errorTx, empty };

    // public member functions
    YACStatus(phLogId_t l);
    bool setStatusBit(YACStatusObserver*, eYACStatus, bool b);
    bool getStatusBit(eYACStatus);
    int getStatusAsInt();
    bool getHandlerIsReady();
    void logStatus();
    void registerObserver(YACStatusObserver*);
    void registerRecorder(EventRecorder*, YACStatusObserver*);
    eYACStatus getEnumStatus(const char* statusName);
private:
    Std::list<YACStatusObserver*> observerList;
    Std::map<eYACStatus,StatusBit> StatusBitArray;
    EventRecorder                *pEventRecorder;
    YACStatusObserver     *pRecordObserver;
};


//enum eYStatus { y_stopped, y_alarm, y_errorTx, y_empty };

/*****************************************************************************
 *
 * YAC status observer
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/


class YACStatusObserver
{
public:
    virtual void YACStatusChanged(YACStatus::eYACStatus, bool b)=0;
};

#endif /* ! _YAC_STATUS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
