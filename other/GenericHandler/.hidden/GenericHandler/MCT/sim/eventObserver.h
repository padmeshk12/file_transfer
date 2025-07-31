/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : EventObserver.h
 * CREATED  : 5 Jan 2015
 *
 * CONTENTS : Interface header for event observer class
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 5 Jan 2015, Magco Li, created
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
#include <errno.h>
#include <errno.h>

/*--- module includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "logObject.h"
#include "status.h"
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
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
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
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
 *
 * Description:
 * Base class for event observation / recording / playback
 *
 *****************************************************************************/

class HandlerController;

template<class T> class EventRecorder : public EventObserver
{
public:
    EventRecorder(
        phLogId_t l                           /* log identifier */,
        HandlerController *pHC                /* handler controller
                                                 for receive message count */,
        Status<T> *pStatus                    /* status object */,
        StatusObserver<T> *recObs             /* observer object of status */,
        const char *fileName                  /* file to record status */);
    void recordChangedStatus(const char *bitName, bool value);
protected:
    HandlerController *pHandController;
};


/*****************************************************************************
 *
 * Event Playback
 *
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
 *
 * Description:
 * Playback of observed object
 *
 *****************************************************************************/

template<class T> class EventPlayback : public EventObserver, public StatusObserver<T>
{
public:
    EventPlayback(
        phLogId_t l                       /* log identifier */,
        Status<T> *pS                     /* convert status str to enum
                                             and notify status changes */,
        const char *fileName              /* file to read status */);
    void checkEvent(int eventCount);
    virtual void StatusChanged(T, bool b);
protected:
    Status<T> *pStatus;
private:
    void getPlaybackComponent();

    int recordStep;
    char strRecordName[MAX_MSG_LENGTH];
    T recordStatus;
    char strRecordValue[MAX_MSG_LENGTH];
    bool recordValue;
    bool end;
};






/*--- template class implementations ----------------------------------------*/

/*--- EventRecorder ---------------------------------------------------------*/

/*--- EventRecorder Constructor ---------------------------------------------*/

template<class T> EventRecorder<T>::EventRecorder(
        phLogId_t l                           /* log identifier */,
        HandlerController *pHC                /* handler controller */,
        Status<T> *pStatus                    /* status object */,
        StatusObserver<T> *recObs             /* observer object of status */,
        const char *fileName                  /* file to record status */) :
    EventObserver(l),
    pHandController(pHC)
{
    if ( strcmp(fileName, "-") == 0 )
    {
        file = stdout;
    }
    else
    {
        file = fopen(fileName, "w");
        if (!file)
        {
            throw FileException(errno);
        }
    }
    phLogSimMessage(logId, LOG_DEBUG,
                    "File successfully opened \"%s\" P%P ",
                    fileName,
                    file);

    pStatus->registerRecorder(this,recObs);
}

template<class T> void EventRecorder<T>::recordChangedStatus(const char *bitName, bool value)
{
    fprintf(file, "%3d %s %s\n",
            pHandController->getReceiveMessageCount(),
            bitName,
            (value) ? "True" : "False");
}



/*--- EventPlayback ---------------------------------------------------------*/

/*--- EventPlayback Constructor ---------------------------------------------*/

template<class T> EventPlayback<T>::EventPlayback(
        phLogId_t l                       /* log identifier */,
        Status<T> *pS                     /* convert status str to enum
                                             and notify status changes */,
        const char *fileName              /* file to read status */) :
    EventObserver(l),
    pStatus(pS)
{
    if ( strcmp(fileName, "-") == 0 )
    {
        file = stdin;
    }
    else
    {
        file = fopen(fileName, "r");
        if (!file)
        {
            throw FileException(errno);
        }
    }
    phLogSimMessage(logId, LOG_VERBOSE,
                    "File successfully opened \"%s\" ", fileName);

    pStatus->registerObserver(this);

    end=false;
    getPlaybackComponent();
}


/*--- EventRecorder public member functions ---------------------------------*/
template<class T> void EventPlayback<T>::checkEvent(int eventCount)
{
    while ( eventCount >= recordStep && !end )
    {
        if ( eventCount > recordStep )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                            "record not made use of %d %s %s current step %d ",
                            recordStep,
                            strRecordName,
                            strRecordValue,
                            eventCount);
            throw PlaybackErrorException("Record not made use of: see output log");
        }
        else
        {
            bool eventUsed=pStatus->setStatusBit(this, recordStatus, recordValue);

            if ( !eventUsed )
            {
                phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                                "record sent but not made use of %d %s %s current step %d ",
                                recordStep,
                                strRecordName,
                                strRecordValue,
                                eventCount);
                throw PlaybackErrorException("Event sent but not used: see output log");
            }
            getPlaybackComponent();
        }
    }
}

template<class T> void EventPlayback<T>::StatusChanged(T s, bool b)
{
}

/*--- EventPlayback private member functions --------------------------------*/


/*****************************************************************************
 *
 * getPlaybackComponent
 *
 * Read file expected format is  "int string string"
 * for example:
 *              0 stopped False
 *             20 alarm False
 *             25 stopped True
 *
 *****************************************************************************/

template<class T> void EventPlayback<T>::getPlaybackComponent()
{
    int rtn_fscanf;

    strcpy(strRecordName,"");
    strcpy(strRecordValue,"");

    rtn_fscanf=fscanf(file, "%d %s %s",
                     &recordStep, strRecordName, strRecordValue);

    if ( rtn_fscanf == 3 )
    {
        phLogSimMessage(logId, LOG_VERBOSE, "getPlaybackComponent() %d %s %s",
                            recordStep, strRecordName, strRecordValue );

        recordStatus=pStatus->getEnumStatus(strRecordName);

        recordValue=convertStringIntoBool(strRecordValue);
    }
    else if ( rtn_fscanf == EOF )
    {
        phLogSimMessage(logId, LOG_VERBOSE, "getPlaybackComponent() EOF detected ");
        end=true;
    }
    else
    {
        phLogSimMessage(logId, LOG_VERBOSE,
                        "getPlaybackComponent() unexpected return value from fscanf  %d ",
                        rtn_fscanf );
        throw PlaybackErrorException("File format: unexpected return value from fscanf");
    }
}


#endif /* ! _EVENT_OBS_H_ */


/*****************************************************************************
 * End of file
 *****************************************************************************/
