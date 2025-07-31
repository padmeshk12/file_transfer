/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : EventObserver.C
 * CREATED  : 29 Apri 2014
 *
 * CONTENTS : Log and Timer definitions
 *
 * AUTHORS  : Magco Li, STS-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Apri 2014, Magco Li , created
 *
 * Instructions:
 *
 * 1) Copy this template to as many .c files as you require
 *
 * 2) Use the command 'make depend' to make visible the new
 *    source files to the makefile utility
 *
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#if __GNUC__ >= 3
#include <iostream>
#endif

#include <errno.h>


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "AdvantestStatus.h"
#include "eventObserver.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/

/*--- class implementations -------------------------------------------------*/


/*--- FileException ---------------------------------------------------------*/
FileException::FileException(int errNum) :
    errorNumber(errNum)
{
}

void FileException::printMessage()
{
    Std::cerr << "Error occured in accessing file: ";

    switch (errorNumber)
    {
      default:
        Std::cerr << "errno " << errorNumber;
        break;
    }

}

/*--- PlaybackErrorException ------------------------------------------------*/
PlaybackErrorException::PlaybackErrorException(const char *msg) :
    message(msg)
{
}

void PlaybackErrorException::printMessage()
{
    Std::cerr << "Playback error: " << message;
}


/*--- EventObserver ---------------------------------------------------------*/

EventObserver::EventObserver(phLogId_t l  /* log identifier */) :
    LogObject(l)
{
}


/*--- EventRecorder ---------------------------------------------------------*/

EventRecorder::EventRecorder(
        phLogId_t l                           /* log identifier */,
        HandlerController *pHC                /* handler controller */,
        AdvantestStatus *pAdvantestStatus           /* status object */,
        AdvantestStatusObserver* recObs      /* observer object of status */,
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

    pAdvantestStatus->registerRecorder(this,recObs);
}

void EventRecorder::recordChangedStatus(const char *bitName, bool value)
{
    fprintf(file, "%3d %s %s\n",
            pHandController->getReceiveMessageCount(),
            bitName,
            (value) ? "True" : "False");
}


/*****************************************************************************
 *
 * Event Playback
 *
 * Authors: Magco Li
 *
 * Description:
 * Playback of observed object
 *
 *****************************************************************************/

/*--- EventRecorder constructor ---------------------------------------------*/

EventPlayback::EventPlayback(
        phLogId_t l                       /* log identifier */,
        AdvantestStatus *pATs            /* convert status str to enum
                                             and notify status changes */,
        const char *fileName              /* file to read status */) :
    EventObserver(l),
    pAdvantestStatus(pATs)
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

    pAdvantestStatus->registerObserver(this);

    end=false;
    getPlaybackComponent();
}

/*--- EventRecorder public member functions ---------------------------------*/
void EventPlayback::checkEvent(int eventCount)
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
            bool eventUsed=pAdvantestStatus->setStatusBit(this, recordStatus, recordValue);
  
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

void EventPlayback::AdvantestStatusChanged(AdvantestStatus::eAdvantestStatus, bool b)
{
}

/*--- EventPlayback private member functions --------------------------------*/

/*****************************************************************************
 *
 * getPlaybackComponent
 *
 * Authors: Magco Li
 *
 * Description:
 *
 * Read file expected format is  "int string string"
 * for example:
 *              0 stopped False
 *             20 alarm False
 *             25 stopped True
 *
 *****************************************************************************/

void EventPlayback::getPlaybackComponent()
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
            
        recordStatus=pAdvantestStatus->getEnumStatus(strRecordName);

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

/*****************************************************************************
 * End of file
 *****************************************************************************/
