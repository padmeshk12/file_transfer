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
#include <vector>
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
 * Status bit individual status bit used by handler to indicate how handler 
 * is functioning and any error conditions
 *
 *****************************************************************************/
class HandlerStatusBit
{
public:
    enum { No_identity = -1,
           No_position = -1 };
    HandlerStatusBit();
    explicit HandlerStatusBit(int id, bool v=false, int p=No_position, const char* n="");
    explicit HandlerStatusBit(const char* key);
    virtual ~HandlerStatusBit();

    void setHandlerStatusBit(int id, bool v=false, int p=No_position, const char* n="");
    void setBit(bool v);
    long getBitAsLong() const;
    int getId() const;
    bool getBit() const;
    const char *getName() const;
    const char *getKey() const;
    bool operator==(const HandlerStatusBit&) const;
private:
    int         identity;
    bool        value;
    int         position;
    const char* name;
    mutable char* key;
};


/*****************************************************************************
 *
 * HandlerStatus
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Collection of handler status bits
 *
 *****************************************************************************/

class HandlerEventRecorder;
class StatusObserver;

class HandlerStatus : public LogObject
{
public:
    HandlerStatus(phLogId_t l);

    /* set-up HandlerStatus HandlerStatusBit(s) */
    void setHandlerStatusList(HandlerStatusBit*);

    /* an observer class alters a HandlerStatusBit */
    bool setStatusBit(StatusObserver*, const HandlerStatusBit&);

    /* an observer class alters a HandlerStatusBit */
    bool setStatusBit(StatusObserver*, int id, bool v);

    /* retrieve HandlerStatus bit */
    const HandlerStatusBit& getHandlerStatusBit(const HandlerStatusBit&) const;

    int getNumberOfHandlerStatusBits() const;
    /* retrieve HandlerStatus bit by index */
    const HandlerStatusBit& getHandlerStatusBit(int index) const;

    /* retrieve boolean HandlerStatus bit */
    bool getStatusBit(int id) const;

    long getStatusAsLong() const;
    void logStatus() const;
    void registerObserver(StatusObserver*);
    void registerRecorder(HandlerEventRecorder*, StatusObserver*);
private:
    HandlerStatusBit& getHandlerStatusBit(const HandlerStatusBit &);
    Std::list<StatusObserver*>          observerList;
    Std::vector<HandlerStatusBit*>      statusBitList;
    HandlerEventRecorder           *pEventRecorder;
    StatusObserver                 *pRecordObserver;
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

class StatusObserver
{
public:
    virtual void statusChanged(const HandlerStatusBit&)=0;
};



#endif /* ! _STATUS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
