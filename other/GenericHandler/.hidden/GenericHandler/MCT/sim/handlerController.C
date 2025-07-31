/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : handlerController.C
 * CREATED  : 5 Jan 2015
 *
 * CONTENTS : Handler controller class definitions
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 5 Jan 2015, Magco Li, created
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


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "viewController.h"
#include "eventObserver.h"
#include "gpibComm.h"
#include "simHelper.h"

/*--- defines ---------------------------------------------------------------*/
/*--- typedefs --------------------------------------------------------------*/
/*--- class definitions -----------------------------------------------------*/



/*--- class implementations -------------------------------------------------*/


/*--- HandlerController -----------------------------------------------------*/

/*--- public member functions -----------------------------------------------*/

/*
 * Handler controller setup with simplest GUI
 */
HandlerController::HandlerController(HandlerControllerSetup *setup,
                                     ViewControllerSetup    *vcSetup) :
    LogObject(setup->debugLevel),
    timeout(setup->timeout),
    defaultEoc(setup->eoc),
    done(0),
    queryError(setup->queryError),
    receiveMessageCount(0),
    replyQueryCount(0)
{
    try {
        phLogSimMessage(logId, LOG_VERBOSE,"Create view controller");

        pViewController = new ViewController(logId,this,vcSetup);

        pHandler = new Handler(logId,setup->handler);

        pGpibComm = new GpibCommunication(logId, setup->gpibComm);

        pViewController->showView();

    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
        throw;
    }
}


/*
 * Protected Constructor for use of derived classes of HandlerController
 * which must setup their own GUI
 */
HandlerController::HandlerController(HandlerControllerSetup *setup) :
    LogObject(setup->debugLevel),
    timeout(setup->timeout),
    defaultEoc(setup->eoc),
    done(0),
    queryError(setup->queryError),
    receiveMessageCount(0),
    replyQueryCount(0)
{
    try {
        pHandler = new Handler(logId,setup->handler);
        pGpibComm = new GpibCommunication(logId, setup->gpibComm);
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
        throw;
    }
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
    if ( pHandler )
    {
        delete pHandler;
    }
}


void HandlerController::switchOn()
{
    while (!done)
    {
        pViewController->checkEvent(1);
    }
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
        ++receiveMessageCount;
        strcpy(messageBuffer, message);
        removeEoc(messageBuffer);
        pViewController->setReceiveMsgField(messageBuffer);
        handleMessage(messageBuffer);
        break;
      case GpibCommunication::comm_timeout:
        if ( to >= timeout )
        {
            strcpy(messageBuffer, "");
            pViewController->setGpibStatus(ViewController::timedOut);
            pViewController->setReceiveMsgField(messageBuffer);
        }
        break;
      case GpibCommunication::comm_error:
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
        phLogSimMessage(logId, LOG_DEBUG, "SRQ send 0x%02x", int(x));
        pViewController->setSendSrqField(x);
        break;
      case GpibCommunication::comm_timeout:
        pViewController->setGpibStatus(ViewController::timedOut);
        break;
      case GpibCommunication::comm_error:
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


const char *HandlerController::getModelName() const
{
    return "Handler";
}

/*--- protected member functions -----------------------------------------------*/

void HandlerController::handleMessage(char *)
{

}

void HandlerController::quit()
{
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

    if ( queryError && (replyQueryCount % 7 == 0) )
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

    do {
        commErr = trySendSRQ(x);
        if ( commErr != GpibCommunication::comm_ok )
        {
            sleep(1);
        }
    } while( commErr != GpibCommunication::comm_ok );
}

/*--- private member functions -------------------------------------------------*/

void HandlerController::sendMessageEoc(const char* msg, const char* eoc)
{
    GpibCommunication::commError commErr;

    do {
        commErr = trySendMsg(msg,eoc);
        if ( commErr != GpibCommunication::comm_ok )
        {
            sleep(1);
        }
    } while( commErr != GpibCommunication::comm_ok );
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
        pViewController->setGpibStatus(ViewController::timedOut);
        break;
      case GpibCommunication::comm_error:
        pViewController->setGpibStatus(ViewController::error);
        break;
    }
    return commErr;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
