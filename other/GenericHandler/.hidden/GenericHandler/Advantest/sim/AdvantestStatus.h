/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : AdvantestStatus.h
 * CREATED  : 29 Apri 2014
 *
 * CONTENTS : Advantest status
 *
 * AUTHORS  : Magco Li, STS-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Apri 2014, Magco Li, created
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

#ifndef _ADVANTEST_STATUS_H_
#define _ADVANTEST_STATUS_H_

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
 * Authors: Magco Li
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
 * Advantest status
 *
 * Authors: Magco Li
 *
 * Description: 
 *
 *****************************************************************************/

class EventRecorder;
class AdvantestStatusObserver;


class AdvantestStatus : public LogObject
{
public:
    enum eAdvantestStatus { stopped, alarm, errorTx, empty };

    // public member functions
    AdvantestStatus(phLogId_t l);
    bool setStatusBit(AdvantestStatusObserver*, eAdvantestStatus, bool b);
    bool getStatusBit(eAdvantestStatus);
    int getStatusAsInt();
    bool getHandlerIsReady();
    void logStatus();
    void registerObserver(AdvantestStatusObserver*);
    void registerRecorder(EventRecorder*, AdvantestStatusObserver*);
    eAdvantestStatus getEnumStatus(const char* statusName);
private:
    Std::list<AdvantestStatusObserver*> observerList;
    Std::map<eAdvantestStatus,StatusBit> StatusBitArray;
    EventRecorder                *pEventRecorder;
    AdvantestStatusObserver     *pRecordObserver;
};


enum eYStatus { y_stopped, y_alarm, y_errorTx, y_empty };


/*****************************************************************************
 *
 * Advantest status observer
 *
 * Authors: Magco Li
 *
 * Description: 
 *
 *****************************************************************************/


class AdvantestStatusObserver
{
public:
    virtual void AdvantestStatusChanged(AdvantestStatus::eAdvantestStatus, bool b)=0;
};


#endif /* ! _ADVANTEST_STATUS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
