/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : EventObserver.h
 * CREATED  : 31 Jan 2001
 *
 * CONTENTS : Interface header for event observer class
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

#ifndef _EVENT_OBS_H_
#define _EVENT_OBS_H_

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>


/*--- module includes -------------------------------------------------------*/

#include "logObject.h"
#include "YACStatus.h"
#include "YACStatus.h"
#include "handlerController.h"

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
 * FileException
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Error occurred while trying to read file
 *
 *****************************************************************************/

class FileException : public Exception
{
public:
    FileException(int errNum);
    virtual void printMessage();
private:
    int errorNumber;
};


/*****************************************************************************
 *
 * Playback error exception
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Some sort of error has occurred while trying to playback events.
 *
 *****************************************************************************/

class PlaybackErrorException : public Exception
{
public:
    PlaybackErrorException(const char *msg);
    virtual void printMessage();
private:
    Std::string message;
};



/*****************************************************************************
 *
 * Event Observer base class
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Base class for event observation / recording / playback
 *
 *****************************************************************************/


class EventObserver : public LogObject
{
public:
    EventObserver(
        phLogId_t l                           /* log identifier */);
protected:
    FILE *file;
};


/*****************************************************************************
 *
 * Event Recorder
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Base class for event observation / recording / playback
 *
 *****************************************************************************/

class HandlerController;

class EventRecorder : public EventObserver
{
public:
    EventRecorder(
        phLogId_t l                           /* log identifier */,
        HandlerController *pHC                /* handler controller 
                                                 for receive message count */, 
        YACStatus *pYACStatus           /* status object */,
        YACStatusObserver* recObs      /* observer object of status */,
        const char *fileName                  /* file to record status */);
    void recordChangedStatus(const char *bitName, bool value);
protected:
    HandlerController *pHandController;
};


/*****************************************************************************
 *
 * Event Playback
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Playback of observed object
 *
 *****************************************************************************/

class EventPlayback : public EventObserver, public YACStatusObserver
{
public:
    EventPlayback(
        phLogId_t l                       /* log identifier */,
        YACStatus *pYACS            /* convert status str to enum 
                                             and notify status changes */,
        const char *fileName              /* file to read status */);
    void checkEvent(int eventCount);
    virtual void YACStatusChanged(YACStatus::eYACStatus, bool b);
protected:
    YACStatus *pYACStatus;
private:
    void getPlaybackComponent();

    int recordStep;
    char strRecordName[MAX_MSG_LENGTH];
    YACStatus::eYACStatus recordStatus;
    char strRecordValue[MAX_MSG_LENGTH];
    bool recordValue;
    bool end;
};


#endif /* ! _EVENT_OBS_H_ */


/*****************************************************************************
 * End of file
 *****************************************************************************/
