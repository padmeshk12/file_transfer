/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : YACHandlerController.C
 * CREATED  : 29 Dec 2006
 *
 * CONTENTS : YAC handler controller implementation
 *
 * AUTHORS  : Kun Xiao, STS R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Dec 2006, Kun Xiao , created
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

#include "YACHandlerController.h"
#include "handlerController.h"
#include "handler.h"
#include "YACStatus.h"
#include "YACViewController.h"
#include "eventObserver.h"
#include "simHelper.h"

#include "ph_tools.h"

/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/



/*--- class implementations -------------------------------------------------*/

/*--- SEHandlerController ---------------------------------------------------*/


YACHandlerController::YACHandlerController(YACHandlerControllerSetup *setup,
                                         ViewControllerSetup    *VCsetup) :
    HandlerController(setup),
    model(setup->model),
    verifyTest(setup->verifyTest),
    multipleSrqs(setup->multipleSrqs)
{
    try {

        pYACStatus = new YACStatus(logId);
        pYACStatus->registerObserver(this);
        pYACStatus->setStatusBit(this, YACStatus::stopped, true);

        pYACViewController = new YACViewController(logId,this,pYACStatus,VCsetup);
        pYACViewController->showView();
        pViewController = pYACViewController;

//        Status<eYStatus> yStatus(logId);
//        yStatus.setStatusList( 
//            new HandlerStatusBit<eYStatus>(y_stopped, "stopped", false, 0) );
//        yStatus.setStatusList( 
//            new HandlerStatusBit<eYStatus>(y_alarm,   "alarm",   false, 0) );
//        yStatus.setStatusList( 
//            new HandlerStatusBit<eYStatus>(y_errorTx, "errorTx", false, 0) );
//        yStatus.setStatusList( 
//            new HandlerStatusBit<eYStatus>(y_empty,   "empty",   false, 0) );

        if ( setup->recordFile )
        {
            pEventRecorder = new EventRecorder(logId,
                                               this,
                                               pYACStatus,
                                               pYACViewController,
                                               setup->recordFile);
        }

        if ( setup->playbackFile )
        {
            pEventPlayback = new EventPlayback(logId,
                                               pYACStatus,
                                               setup->playbackFile);
        }
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
        throw;
    }
    srqMask=0xFF;

    pYACViewController->setGpibStatus(ViewController::okay);
}

YACHandlerController::~YACHandlerController()
{
}

unsigned char YACHandlerController::getSrqByte()
{
    unsigned char srqByte = 0;

    pYACStatus->logStatus();

    switch ( model )
    {
      case HYAC8086:
                    // bit 0 Demand of start
        srqByte = (pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0 ;

                    // bit 1 (don't use) Demand of manual start

                    // bit 2 Alarm set on handler
        srqByte|= (1<<2) * pYACStatus->getStatusBit(YACStatus::alarm);

                    // bit 3 Error in transmission
        srqByte|= (1<<3) * pYACStatus->getStatusBit(YACStatus::errorTx);

                    // bit 4 Receive result of test

                    // bit 5 don't use

                    // bit 7 Spare
        break;
      default:
        break;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte before mask %d",int(srqByte));

                 // Mask out bits not needed
    srqByte&=srqMask;

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte with mask %d",int(srqByte));

    return srqByte;
}

void YACHandlerController::switchOn() 
{
    while (!done)
    {
        if ( pEventPlayback )
        {
            pEventPlayback->checkEvent( getReceiveMessageCount() );
        }

        pYACViewController->checkEvent(0);

        tryReceiveMsg(1000);

        if ( pHandler->sendTestStartSignal() )
        {
            pYACStatus->setStatusBit(this, YACStatus::stopped, false);

            unsigned char srqSequence=getSrqByte();

                         // bit 6 SRQ asserted
            srqSequence|= (1<<6);

            if ( multipleSrqs )
            {
                if ( 0x01 & srqSequence )
                {
                    sendSRQ(srqSequence);
                    sendSRQ(srqSequence);
                    sendSRQ(srqSequence);
                    sendSRQ(srqSequence);
                }
            }
            else
            {
                sendSRQ(srqSequence);
            }
        }

        if ( pHandler->allTestsComplete() && pYACStatus->getStatusBit(YACStatus::empty)==false )
        {
            pHandler->logHandlerStatistics();

            pYACStatus->setStatusBit(this, YACStatus::stopped, true);
            pYACStatus->setStatusBit(this, YACStatus::empty, true);
        }
    }
}

void YACHandlerController::YACStatusChanged(YACStatus::eYACStatus ebit, bool b)
{
    switch (ebit)
    {
      case YACStatus::stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "YACHandlerController stop detected new value %s",
                        (b) ? "True" : "False");
        if ( b )
        {
            phLogSimMessage(logId, LOG_DEBUG, "YACHandler stopped");
            pHandler->stop();
        }
        else
        {
            phLogSimMessage(logId, LOG_DEBUG, "YACHandler started");
            pHandler->start();
        }
        break;
      case YACStatus::alarm:
        phLogSimMessage(logId, LOG_VERBOSE, "YACHandlerController alarm detected new value %s",
                        (b) ? "True" : "False");
        break;
      case YACStatus::errorTx:
        phLogSimMessage(logId, LOG_VERBOSE, "YACHandlerController errorTx detected new value %s",
                        (b) ? "True" : "False");
        break;
      case YACStatus::empty:
        phLogSimMessage(logId, LOG_VERBOSE, "YACHandlerController empty detected new value %s",
                        (b) ? "True" : "False");
        if ( b )
        {
            pHandler->stop();
        }
        else
        {
            pHandler->reset();
        }
        break;
    }
}

YACHandlerController::eYACModel YACHandlerController::getModelType() const
{
    return model;
}

/*--- YACHandlerController protected and public functions --------------------*/


void YACHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_DEBUG, "Received message \"%s\"",message);

    setStrlower(message);

    if (phToolsMatch(message, "fullsites\\?"))
        serviceFullsites(message);
    else if (phToolsMatch(message, "binon:.*"))
        serviceBinon(message);
    else if (phToolsMatch(message, "echook"))
        serviceEchoOk(message);
    else if (phToolsMatch(message, "echong"))
        serviceEchoNg(message);
    else if (phToolsMatch(message, "fr\\?"))
        serviceStatus(message);
    else if (phToolsMatch(message, "srqmask .*"))
        serviceSrqmask(message);
    else
    {
        pYACViewController->setGpibStatus(ViewController::notUnderstood);
    }
}


void YACHandlerController::serviceStatus(const char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    sprintf(messageBuffer, "FR%d", int(!pYACStatus->getStatusBit(YACStatus::stopped)) );

    pYACStatus->logStatus();

    replyToQuery(messageBuffer);
 
    return;
}

void YACHandlerController::serviceFullsites(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    unsigned long devicesReady;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    devicesReady=pHandler->getSitePopulationAsLong();

    sprintf(messageBuffer, "FULLSITES %.8lx", devicesReady );

    replyToQuery(messageBuffer);

    return;
}

void YACHandlerController::serviceBinon(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    //
    // get binon information
    // expected format
    // BINON: xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx;
    //
    int site=31;
    int bin;
    const char *posMsg=msg;

    /* find first colon */
    posMsg=strchr(posMsg,':');

    while ( posMsg && *posMsg && site>=0 )
    {
        posMsg=findHexadecimalDigit(posMsg);

        if ( isxdigit(*posMsg) )
        {
            if ( site < pHandler->getNumOfSites() )
            {
                bin=convertHexadecimalDigitIntoInt(*posMsg);

                if ( bin != 0 )
                {
                    if ( verifyTest )
                    {
                        pHandler->setBinData(site,bin);
                    }
                    else
                    {
                        pHandler->sendDeviceToBin(site,bin);
                    }
                }
            }
            --site;
            ++posMsg;
        }
    }


    if ( site>=0 )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                        "unable to interpret BINON data: \"%s\" after position at site %d ",
                        msg,site+1);
    }
    else
    {
        if ( verifyTest )
        {
            serviceEcho();
        }
    }
}


void YACHandlerController::serviceEcho()
{
    char messageBuffer[MAX_MSG_LENGTH];
    char tmp[64];
    int site = 0;
    int bin = 0;

    phLogSimMessage(logId, LOG_VERBOSE, "serviceEcho()");

    //
    // echo bin information
    // format
    // ECHO:xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx;
    //
    strcpy(messageBuffer,"ECHO:");
    for ( site=31 ; site>=0  ; --site)
    {
        if ( site < pHandler->getNumOfSites() )
        {
            bin=pHandler->getBinData(site);

            if (bin == siteNotYetAssignedBinNumber)
            {
                switch ( model )
                {
                  case HYAC8086:
                    bin=0;
                    break;
                  default:
                    break;
                }
            }
        }
        else
        {
            switch ( model )
            {
              case HYAC8086:
                bin=0;
                break;
              default:
                break;
            }
        }
        sprintf(tmp, "%x", bin);
        strcat(messageBuffer,tmp);

        if ( site == 0 )
        {
            strcat(messageBuffer,";");
        }
        else if ( site % 8 == 0 )
        {
            strcat(messageBuffer,",");
        }
    }
    sendMessage(messageBuffer);

    return;
}

void YACHandlerController::serviceEchoOk(const char *msg)
{
    int s;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    for ( s=0 ; s < pHandler->getNumOfSites() ; ++s)
    {
        pHandler->releaseDevice(s);
    }
    pHandler->resetVerifyCount();

    return;
}

void YACHandlerController::serviceEchoNg(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    pHandler->binDataVerified();

    if ( pHandler->isStopped() )
    {
        pYACStatus->setStatusBit(this, YACStatus::stopped, true);
    }
    return;
}


void YACHandlerController::serviceSrqmask(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /*
     * Mask byte for SRQ generation.
     */

    if ( sscanf(msg,"srqmask %d",&srqMask) != 1 )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"srqmask n\"",msg);
    }

    return;
}




/*****************************************************************************
 * End of file
 *****************************************************************************/
