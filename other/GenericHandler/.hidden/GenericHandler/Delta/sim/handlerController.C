/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : handlerController.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Handler controller class definitions
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Xiaofei Feng, R&D Shanghai, CR25072
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 25 Jan 2001, Chris Joyce , created
 *            Dec 2005, Xiaofei Feng, CR25072
 *              Modified HandlerController::createView() to fix segmentation fault.
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
#include <cstring>

/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "handler.h"
#include "viewController.h"
#include "eventObserver.h"
#include "gpibComm.h"
#include "simHelper.h"
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

/*--- struct implementations ------------------------------------------------*/

HandlerControllerSetup::HandlerControllerSetup()
{
    eoc                              =   "\n";
    debugLevel                       =   0;
    timeout                          =   5000;
    queryError                       =   false;
    verifyTest                       =   false;
    multipleSrqs                     =   false;
    recordFile                       =   NULL;
    playbackFile                     =   NULL;
    noGui                            =   false;
    corruptMessage                   =   false;
}


/*--- class implementations -------------------------------------------------*/


/*--- HandlerController -----------------------------------------------------*/

/*--- HandlerController constructor -----------------------------------------*/

HandlerController::HandlerController(HandlerControllerSetup *setup) :
    LogObject(setup->debugLevel), 
    pViewController(0),
    pHEventRecorder(0),
    pHEventPlayback(0),
    timeout(setup->timeout), 
    defaultEoc(setup->eoc), 
    verifyTest(setup->verifyTest),
    multipleSrqs(setup->multipleSrqs),
    operatorPause(false),
    done(0),
    corruptMessage(setup->corruptMessage),
    replyQueryCount(0), 
    queryError(setup->queryError),
    receiveMessageCount(0),
    noGuiDisplay(setup->noGui),
    recordFile(setup->recordFile),    
    playbackFile(setup->playbackFile)
{
    try {
        pGpibComm = new GpibCommunication(logId, setup->gpibComm);

        pHandler = new Handler(logId,setup->handler);

        pHstatus = new HandlerStatus(logId);

        pHstatus->registerObserver(this);
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
        throw;
    }
    srqMask=0xFF;
    length=0;
}


HandlerController::~HandlerController()
{
    phLogSimMessage(logId, LOG_VERBOSE,
                    "HandlerController destructor called: delete %P %P ",
                    pViewController,
                    pGpibComm);
    if ( pViewController )
    {
        delete pViewController;
    }
    if ( pGpibComm )
    {
        delete pGpibComm;
    }
    if ( pHEventRecorder )
    {
        delete pHEventRecorder;
    }
    if ( pHEventPlayback )
    {
        delete pHEventPlayback;
    }
    if( pHandler )
    {
        delete pHandler;
    }
    if ( pHstatus )
    {
        delete pHstatus;
    }
}

/*--- public member functions -----------------------------------------------*/

void HandlerController::switchOn(deltaModel model)
{
    phLogSimMessage(logId, LOG_VERBOSE,"HandlerController::switchOn()");

    tryReceiveMsg(1000);

    pViewController->checkEvent(0);
}

GpibCommunication::commError HandlerController::tryReceiveMsg() 
{
    return tryReceiveMsg(timeout);
}

GpibCommunication::commError HandlerController::tryReceiveMsg(int to) 
{
    GpibCommunication::commError commErr;
    char messageBuffer[MAX_MSG_LENGTH];
    const char *message;

    phLogSimMessage(logId, LOG_VERBOSE,"HandlerController::tryReceiveMsg() %s",getTimeString());

    if ( to >= timeout )
    {
        pViewController->setReceiveMsgField("");
        pViewController->setGpibStatus(ViewController::waitingMsg);
    }
    commErr=pGpibComm->receive(&message, &length, 1000L*to);

    if ( to >= timeout )
    {
        pViewController->setGpibStatus(ViewController::okay);
    }

    switch ( commErr )
    {
      case GpibCommunication::comm_ok:
        phLogSimMessage(logId, LOG_VERBOSE,"HandlerController::tryReceiveMsg() comm_ok %s",getTimeString());
        ++receiveMessageCount;
        strcpy(messageBuffer, message);
        removeEoc(messageBuffer);
        pViewController->setReceiveMsgField(messageBuffer);
        handleMessage(messageBuffer);
        break;
      case GpibCommunication::comm_timeout:
        phLogSimMessage(logId, LOG_VERBOSE,"HandlerController::tryReceiveMsg() comm_timeout %s",getTimeString());
        if ( to >= timeout )
        {
            strcpy(messageBuffer, "");
            pViewController->setGpibStatus(ViewController::timedOut);
            pViewController->setReceiveMsgField(messageBuffer);
        }
        break;
      case GpibCommunication::comm_error:
        phLogSimMessage(logId, LOG_VERBOSE,"HandlerController::tryReceiveMsg() comm_error");
        if ( to >= timeout )
        {
            strcpy(messageBuffer, "");
            pViewController->setGpibStatus(ViewController::error);
            pViewController->setReceiveMsgField(messageBuffer);
        }
        break;
    }

    return commErr;
}


GpibCommunication::commError HandlerController::trySendSRQ(unsigned char x) 
{
    GpibCommunication::commError commErr;
    phComGpibEvent_t sendEvent;

    sendEvent.type = PHCOM_GPIB_ET_SRQ;
    sendEvent.d.srq_byte = x;

    pViewController->setGpibStatus(ViewController::tryingSendSrq);

    commErr=pGpibComm->sendEvent(&sendEvent, 1000L*timeout);

    pViewController->setGpibStatus(ViewController::okay);

    switch ( commErr )
    {
      case GpibCommunication::comm_ok:
        phLogSimMessage(logId, LOG_DEBUG, "SRQ(0x%02x) has been sent", int(x));
        pViewController->setSendSrqField("");
        break;
      case GpibCommunication::comm_timeout:
        phLogSimMessage(logId, LOG_DEBUG, "SRQ timeout 0x%02x", int(x));
        pViewController->setGpibStatus(ViewController::timedOut);
        break;
      case GpibCommunication::comm_error:
        phLogSimMessage(logId, LOG_DEBUG, "SRQ error 0x%02x", int(x));
        pViewController->setGpibStatus(ViewController::error);
        break;
    }
    return commErr;
}

GpibCommunication::commError HandlerController::trySendMsg(const char* msg)
{
    return trySendMsg(msg, defaultEoc);
}

int HandlerController::getReceiveMessageCount() const
{
    return receiveMessageCount;
}

const char *HandlerController::getName() const
{
    return "Generic Handler";
}


/*--- protected member functions -----------------------------------------------*/
void HandlerController::createView() 
{
    try {
        pViewController = new  ViewController(logId,
                                              this,
                                              pHstatus,
                                              noGuiDisplay);
        pViewController->showView();

        if ( ( recordFile != NULL ) && ( *recordFile ) )
        {
            pHEventRecorder = new HandlerEventRecorder(logId,
                                                       this,
                                                       pHstatus,
                                                       pViewController,
                                                       recordFile);
        }

        if ( ( playbackFile != NULL ) && ( *playbackFile ) )
        {
            pHEventPlayback = new HandlerEventPlayback(logId,
                                                       pHstatus,
                                                       playbackFile);
        }
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
        throw;
    }
    pViewController->setGpibStatus(ViewController::okay);
}

void HandlerController::checkData()
{
    phLogSimMessage(logId, LOG_VERBOSE, "HC::checkData()");

    pViewController->checkData(0);

    if ( pHEventPlayback )
    {
        pHEventPlayback->checkEvent( getReceiveMessageCount() );
    }
}


void HandlerController::handleMessage(char *message) 
{
    implHandleMessage(message);
}

void HandlerController::implHandleMessage(char *message) 
{
    phLogSimMessage(logId, LOG_DEBUG, "HC::handleMessage \"%s\" not understood",
                    message);
    pViewController->setGpibStatus(ViewController::notUnderstood);
}

void HandlerController::quit() 
{
    phLogSimMessage(logId, LOG_VERBOSE, "HC::quit() all done");
    done=1;
}

void HandlerController::dump() 
{
    phLogDump(logId, PHLOG_MODE_ALL);
}


void HandlerController::sendMessage(const char *message)
{
    sendMessageEoc(message, defaultEoc);
}

void HandlerController::replyToQuery(const char *message)
{
    ++replyQueryCount;

    phLogSimMessage(logId, LOG_VERBOSE, 
                    "reply to query \"%s\" query count %d corrupt every 7th query %s ",
                    message,
                    replyQueryCount,
                    (queryError) ? "True" : "False");

    if ( queryError && (replyQueryCount % 14 == 0) )
    {
        phLogSimMessage(logId, LOG_DEBUG, "corrupt query reply from %s to %s ",
                        message," <no response, will timeout>");

//        sendMessageEoc(" ", "\n");
    }
    else if ( queryError && (replyQueryCount % 7 == 0) )
    {
        phLogSimMessage(logId, LOG_DEBUG, "corrupt query reply from %s to %s ",
                        message," \\n");

        sendMessageEoc(" ", "\n");
    }
    else
    {
        sendMessage(message);
    }
}


void HandlerController::sendSRQ(unsigned char x)
{
    GpibCommunication::commError commErr;
    int tries = 0;
    phLogSimMessage(logId, LOG_NORMAL, "try to send srq 0x%x", x);

    do {
        ++tries;
        commErr = trySendSRQ(x);
        if ( commErr != GpibCommunication::comm_ok )
        {
            sleep(1);
        }
    } while( (commErr != GpibCommunication::comm_ok) && (tries < 2) );
}

/*--- private member functions -------------------------------------------------*/

void HandlerController::sendMessageEoc(const char* msg, const char* eoc)
{
    GpibCommunication::commError commErr;
    int tries = 0;

    do {
        ++tries;
        commErr = trySendMsg(msg,eoc);
        if ( commErr != GpibCommunication::comm_ok )
        {
            sleep(1);
        }
    } while( (commErr != GpibCommunication::comm_ok) && (tries < 2) );
}


GpibCommunication::commError HandlerController::trySendMsg(const char* msg, const char* eoc)
{
    GpibCommunication::commError commErr;
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE,
                    "trying to send message '%s'", msg);

    strcpy(messageBuffer,msg);
    strcat(messageBuffer,eoc);

    pViewController->setGpibStatus(ViewController::tryingSendMsg);

    commErr=pGpibComm->send(messageBuffer, strlen(messageBuffer), 1000L*timeout);

    pViewController->setGpibStatus(ViewController::okay);

    switch ( commErr )
    {
      case GpibCommunication::comm_ok:
        phLogSimMessage(logId, LOG_DEBUG, "message send '%s'", msg);
        pViewController->setSendMsgField("");
        break;
      case GpibCommunication::comm_timeout:
        phLogSimMessage(logId, LOG_VERBOSE, "trying to send '%s' timeout", msg);
        pViewController->setGpibStatus(ViewController::timedOut);
        break;
      case GpibCommunication::comm_error:
        phLogSimMessage(logId, LOG_VERBOSE, "trying to send '%s' comm error", msg);
        pViewController->setGpibStatus(ViewController::error);
        break;
    }
    return commErr;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
