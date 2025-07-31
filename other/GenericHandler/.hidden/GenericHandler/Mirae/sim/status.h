/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : status.h
 * CREATED  : 18 Jan 2001
 *
 * CONTENTS : status
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce, created
 *            25 March 2008, Xiaofei Han, move the class implementation to a new file 
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

#ifndef _STATUS_H_
#define _STATUS_H_

/*--- system includes -------------------------------------------------------*/
#include <map>
#include <list>
#include <string>

/*--- module includes -------------------------------------------------------*/

#include "logObject.h"
#include "simHelper.h"

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
 * Handler status bit
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Status bit used by handler to indicate how handler is functioning and
 * any error conditions
 *
 *****************************************************************************/
template<class T> class HandlerStatusBit
{
public:
    enum { No_position = -1 };
    HandlerStatusBit();
    HandlerStatusBit(T e);
    HandlerStatusBit(T e, const char* n, bool v, int p);
    void setHandlerStatusBit(T e, const char* n, bool b, int p);
    void setBit(bool b);
    T getEnum() const; 
    const char *getName() const;
    bool getBit() const;
    int getBitAsInt() const;
private:
    T           enumValue;
    const char* name;
    bool        value;
    int         position;
};


/*****************************************************************************
 *
 * Status
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Collection of status bits
 *
 *****************************************************************************/

class EventRecorder;
template<class T> class StatusObserver;

template<class T> class Status : public LogObject
{
public:
    // public member functions
    Status(phLogId_t l);
    void setStatusList(HandlerStatusBit<T>*);
    bool setStatusBit(StatusObserver<T>*, T, bool b);
    bool getStatusBit(T);
    int getStatusAsInt();
    void logStatus();
    void registerObserver(StatusObserver<T>*);
    void registerRecorder(EventRecorder*, StatusObserver<T>*);
    T getEnumStatus(const char* statusName);
private:
    HandlerStatusBit<T>& getHandlerStatusBit(T e);

    Std::list<StatusObserver<T>*>          observerList;
    Std::list<HandlerStatusBit<T>*>        statusBitList;
    EventRecorder                     *pEventRecorder;
    StatusObserver<T>                 *pRecordObserver;
};


/*****************************************************************************
 *
 * Status observer
 *
 * Authors: Chris Joyce
 *
 * Description: 
 *
 *****************************************************************************/


template<class T> class StatusObserver
{
public:
    virtual void StatusChanged(T, bool b)=0;
};




#endif /* ! _STATUS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
