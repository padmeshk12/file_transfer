/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SEstatus.h
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

#ifndef _SE_STATUS_H_
#define _SE_STATUS_H_

/*--- system includes -------------------------------------------------------*/
#include <map>
#include <list>

/*--- module includes -------------------------------------------------------*/

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
    virtual ~StatusStringToEnumConversionException(){};
private:
    Std::string convertStringError;
};



/*****************************************************************************
 *
 * Seiko-Epson status
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/

class EventRecorder;
class SeikoEpsonStatusObserver;


class SeikoEpsonStatus : public LogObject
{
public:
    enum eSEStatus { stopped, alarm, errorTx, empty };

    // public member functions
    SeikoEpsonStatus(phLogId_t l);
    bool setStatusBit(SeikoEpsonStatusObserver*, eSEStatus, bool b);
    bool getStatusBit(eSEStatus);
    int getStatusAsInt();
    bool getHandlerIsReady();
    void logStatus();
    void registerObserver(SeikoEpsonStatusObserver*);
    void registerRecorder(EventRecorder*, SeikoEpsonStatusObserver*);
    eSEStatus getEnumStatus(const char* statusName);
private:
    Std::list<SeikoEpsonStatusObserver*> observerList;
    Std::map<eSEStatus,StatusBit> StatusBitArray;
    EventRecorder                *pEventRecorder;
    SeikoEpsonStatusObserver     *pRecordObserver;
};


enum eYStatus { y_stopped, y_alarm, y_errorTx, y_empty };


/*****************************************************************************
 *
 * Seiko-Epson status observer
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/


class SeikoEpsonStatusObserver
{
public:
    virtual void SEStatusChanged(SeikoEpsonStatus::eSEStatus, bool b)=0;
};


#endif /* ! _SE_STATUS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
