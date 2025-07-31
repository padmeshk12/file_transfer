/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : MCTHandlerController.C
 * CREATED  : 5 Jan 2015
 *
 * CONTENTS : MCT handler controller implementation
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

#include "MCTHandlerController.h"
#include "handlerController.h"
#include "handler.h"
#include "HandlerViewController.h"
#include "eventObserver.h"
#include "simHelper.h"


#include "ph_tools.h"

using namespace std;

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/



/*--- class implementations -------------------------------------------------*/

/*--- MCTHandlerController ------------------------------------------------*/


MCTHandlerController::MCTHandlerController(
        MCTHandlerControllerSetup   *setup       /* configurable setup values */,
        ViewControllerSetup           *VCsetup     /* view controller */) :
    HandlerController(setup),
    model(setup->model),
    testedDevice(0)
{
    try {

        pStatus = new Status<eMctStatus>(logId);
        pStatus->setStatusList(new HandlerStatusBit<eMctStatus>(stopped,"stopped",false,1,true) );
        pStatus->setStatusList(new HandlerStatusBit<eMctStatus>(empty,"empty",false,1,true) );
        pStatus->registerObserver(this);
        pStatus->selfTest();

        pHViewController = new HandlerViewController<eMctStatus>(logId,this,pStatus,VCsetup);
        pHViewController->showView();
        pViewController=pHViewController;

        if ( setup->recordFile )
        {
            pEventRecorder = new EventRecorder<eMctStatus>(logId,
                                               this,
                                               pStatus,
                                               pHViewController,
                                               setup->recordFile);
        }
        else
        {
            pEventRecorder = NULL;
        }

        if ( setup->playbackFile )
        {
            pEventPlayback = new EventPlayback<eMctStatus>(logId,
                                               pStatus,
                                               setup->playbackFile);
        }
        else
        {
            pEventPlayback = NULL;
        }
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
        throw;
    }

    pHViewController->setGpibStatus(ViewController::okay);
    
    memset(newPanel, 0, MAX_MSG_LENGTH);
}

MCTHandlerController::~MCTHandlerController()
{
}

unsigned char MCTHandlerController::getSrqByte()
{
    unsigned char srqByte = 0;

    switch ( model )
    {
      case FH1200:
        srqByte = (pHandler->getSitePopulationAsLong()) ? 0x43:0x00;
        break;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte %d",int(srqByte));

    return srqByte;
}

void MCTHandlerController::switchOn()
{
    while (!done)
    {
        if ( pEventPlayback )
        {
            pEventPlayback->checkEvent( getReceiveMessageCount() );
        }

        pHViewController->checkEvent(1);

        tryReceiveMsg(1000);

        switch(model){
            case FH1200:
                if ( pHandler->sendTestStartSignal() )
                {
                    if ( pHandler->testsNotStarted() )

                    {
                        pStatus->setStatusBit(this, stopped, false);
                    }

                    unsigned char   srqSequence=getSrqByte();

                    // bit 6 SRQ asserted
                    srqSequence |= (1<<6);
                    phLogSimMessage(logId, LOG_DEBUG, "Send Srq is \"%lx\"",srqSequence);

                    testedDevice = pHandler->getTest();
                    sendSRQ(srqSequence);
                    serviceStartSignal();
                }
                break;
        }

        if ( pHandler->allTestsComplete() && pStatus->getStatusBit(empty)==false )
        {
            pHandler->logHandlerStatistics();

            pStatus->setStatusBit(this, stopped, true);
            pStatus->setStatusBit(this, empty, true);
        }
    }
}

void MCTHandlerController::StatusChanged(eMctStatus ebit, bool b)
{
    switch (ebit)
    {
      case stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "MCTHandlerController stop detected new value %s",
                        (b) ? "True" : "False");
        if ( b )
        {
            pHandler->stop();
        }
        else
        {
            pHandler->start();
        }
        break;
      case empty:
        phLogSimMessage(logId, LOG_VERBOSE, "MCTHandlerController empty detected new value %s",
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

MCTHandlerController::eMctModel MCTHandlerController::getModelType() const
{
    return model;
}

const char *MCTHandlerController::getModelName() const
{
    const char *name;

    switch ( model )
    {
      case FH1200:
        name="MCT FH1200 GPIB Handler";
        break;
    }

    return name;
}


/*--- MCTHandlerController protected and public functions --------------------*/


void MCTHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_NORMAL, "Received message \"%s\"",message);

    if (phToolsMatch(message, "Set Bin.*"))
    {
        phLogSimMessage(logId, LOG_NORMAL, "testedDevice = %d", testedDevice);
        serviceSortBinSignal(message);
#if 0
        if (testedDevice == 4)
        {
            sendSRQ(0x43);
            replyToQuery("Set PanelComplete");
        }
        if (testedDevice == 8)
        {
            sendSRQ(0x43);
            replyToQuery("Set PanelComplete");
        }
#endif
    }
    else if (phToolsMatch(message, "Get HandlerId.*"))
    {
        replyToQuery("Set HandlerId \"PH/1 1234 Ver.1.0.07/15/99");
    }
    else if (phToolsMatch(message, "GetTemperatureStatus.*"))
    {
        sendSRQ(0x43);
        replyToQuery("SC_TRITEMP Soak1Base,Enable,OFF,25.7,50,4,10,Soak1Cover,Enable,OFF,25.7,50,4,10,TestChunck, Enable,OFF,27.1,50,0,2,AirGeater,Disable,OFF,-4.8,50,30,100");
    }
    else if (phToolsMatch(message, "GetHATSTemperatureRaw.*"))
    {
        sendSRQ(0x43);
        replyToQuery("SC_TRITEMP TemperatureRaw");
    }
    else if (phToolsMatch(message, "GetHATSTemperatureCal.*"))
    {
        sendSRQ(0x43);
        replyToQuery("SC_TRITEMP TemperatureCal");
    }
    else if (phToolsMatch(message, "GetHATSTemperatureTop.*"))
    {
        sendSRQ(0x43);
        replyToQuery("SC_TRITEMP HATS-001 85.01,85.03,84.97");
    }
    else if (phToolsMatch(message, "GetHATSDutyCycle.*"))
    {
        sendSRQ(0x43);
        replyToQuery("SC_TRITEMP DutyCycle");
    }
    else if (phToolsMatch(message, "GetHATSSiteData.*"))
    {
        sendSRQ(0x43);
        replyToQuery("SC_TRITEMP HATS-001 L1,85.01,M1,85.00,L2,79.99,M2,80.05");
    }
    else if (phToolsMatch(message, "StartTemperatureUpdate.*"))
    {
        sendSRQ(0x43);
        replyToQuery("SC_TRITEMP Soak1Base,Able,OFF,52.7,50,4,10,Soak1Cover,Enable,OFF,25.7,50,4,10,TestChunck, Enable,OFF,27.1,50,0,2,AirGeater,Disable,OFF,-4.8,50,30,100");
    }
    else
    {
        pHViewController->setGpibStatus(ViewController::notUnderstood);
    }
}

void MCTHandlerController::serviceStartSignal()
{
    char messageBuffer[MAX_MSG_LENGTH]="";
    char devicesReady[MAX_MSG_LENGTH]="";

    pHandler->getSitePopulationAsLong_MCTFH1200(devicesReady);

    strcat(messageBuffer,"Set StartTest ");

    phLogSimMessage(logId, LOG_DEBUG, "pHandler->getTest() \"%d\"",testedDevice);

    if (testedDevice == 4)
    {
        strcpy(newPanel,"\"1234\" ");
    }
    else if (testedDevice == 8)
    {
        strcpy(newPanel, "\"5678\" ");
    }
    else
    {
        strcpy(newPanel, "\"9090\" ");
    }
    strcat(messageBuffer,newPanel);

    strcat(messageBuffer,devicesReady);

    replyToQuery(messageBuffer);

}

void MCTHandlerController::serviceSortBinSignal(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    //
    // Sort Signal Command expected format
    // Set Bin x,y,x,y,x,y,\r\n
    //         ^ ^
    //         | |___ bin num
    //         |____ test site
    //
    //

    const char *posMsg=msg;
    int num_of_sites=pHandler->getNumOfSites();
    int bin[1024]={0};
    int i,site;
    bool isBin = true;
    /* go past first Set Bin in message */

    switch(model){
        case FH1200:
        char *message = strtok((char*)posMsg, "\"");
        if (message)
        {
            message = strtok(NULL,"\"");
            i = 0;
            site = 0;
            while(i< num_of_sites)
            {
                if (pHandler->getSitePopulated(i))
                {
                    while(message)
                    {
                       message = strtok(NULL, ",");
                       if(!isBin)//bin num
                       {
                          bin[site] = atoi(message);
                          isBin = true;
                          break;
                        }
                        else //device id
                        {
                           isBin = false;
                        }
                    }
                }
                i++;
                site++;
            }
        }
        for(i=0;i<num_of_sites;i++)
        {
             if (pHandler->getSitePopulated(i))
             {
                 pHandler->sendDeviceToBin(i,bin[i]);
             }
        }
        break;
    }
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
