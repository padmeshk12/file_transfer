/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : Miraestatus.h
 * CREATED  : 18 Jan 2001
 *
 * CONTENTS : Mirae status
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

#ifndef _MIRAE_STATUS_H_
#define _MIRAE_STATUS_H_

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
    virtual ~StatusStringToEnumConversionException();
    virtual void printMessage();
private:
    Std::string convertStringError;
};



/*****************************************************************************
 *
 * Mirae status
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/

class EventRecorder;
class MiraeStatusObserver;


class MiraeStatus : public LogObject
{
public:
    // Bits 0 to 15 of STATUS? response:
    enum eMiraeStatus { waiting_tester_response, stopped, high_alarm, low_alarm, temp_alarm, temp_stable, out_of_temp, wait_for_soak, handler_empty, input_empty, reserved_C2, reserved_C3, controlling_remote, reserved_D1, reserved_D2, reserved_D3 };

    // public member functions
    MiraeStatus(phLogId_t l);
    virtual ~MiraeStatus();
    bool setStatusBit(MiraeStatusObserver*, eMiraeStatus, bool b);
    bool getStatusBit(eMiraeStatus);
    int getStatusAsInt();
    bool getHandlerIsReady();
    void logStatus();
    void registerObserver(MiraeStatusObserver*);
    void registerRecorder(EventRecorder*, MiraeStatusObserver*);
    eMiraeStatus getEnumStatus(const char* statusName);
private:
    Std::list<MiraeStatusObserver*> observerList;
    Std::map<eMiraeStatus,StatusBit> StatusBitArray;
    EventRecorder                *pEventRecorder;
    MiraeStatusObserver     *pRecordObserver;
};



/*****************************************************************************
 *
 * Mirae status observer
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/


class MiraeStatusObserver
{
public:
    virtual void MiraeStatusChanged(MiraeStatus::eMiraeStatus, bool b)=0;
};


#endif /* ! _MIRAE_STATUS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
