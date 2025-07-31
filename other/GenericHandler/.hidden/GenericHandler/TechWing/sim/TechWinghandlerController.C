/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : TechWinghandlerController.C
 * CREATED  : 10 Jan 2005
 *
 * CONTENTS : TechWing handler controller implementation
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, TechWing revision(CR29015)
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 25 Jan 2001, Chris Joyce , created
 *            March 2006, Jiawei Lin, CR29015
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

#include "TechWinghandlerController.h"
#include "handlerController.h"
#include "handler.h"
#include "TechWingstatus.h"
#include "TechWingviewController.h"
#include "eventObserver.h"
#include "simHelper.h"

#include "ph_tools.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/



/*--- class implementations -------------------------------------------------*/

/*--- TechWingHandlerController ---------------------------------------------------*/


TechWingHandlerController::TechWingHandlerController(TechWingHandlerControllerSetup *setup,
                                         ViewControllerSetup    *VCsetup) :
    HandlerController(setup),
    model(setup->model),
    verifyTest(setup->verifyTest),
    multipleSrqs(setup->multipleSrqs),
    numOfSites(setup->handler.numOfSites)
{
    try {
        
        pTechWingstatus = new TechWingStatus(logId);
        pTechWingstatus->registerObserver(this);
        //pTechWingstatus->setStatusBit(this, TechWingStatus::stopped, true);

        pTechWingstatus->setStatusBit(this, TechWingStatus::temp_stable, true);

        pTechWingViewController = new TechWingViewController(logId,this,pTechWingstatus,VCsetup);
        pTechWingViewController->showView();
        pViewController=pTechWingViewController;

        if ( setup->recordFile )
        {
            pEventRecorder = new EventRecorder(logId,
                                               this,
                                               pTechWingstatus,
                                               pTechWingViewController,
                                               setup->recordFile);
        }

        if ( setup->playbackFile )
        {
            pEventPlayback = new EventPlayback(logId,
                                               pTechWingstatus,
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

    pTechWingViewController->setGpibStatus(ViewController::okay);
}

TechWingHandlerController::~TechWingHandlerController()
{
}

unsigned char TechWingHandlerController::getSrqByte()
{
    unsigned char srqByte = 0;

    pTechWingstatus->logStatus();

    switch ( model )
    {
    case TW2XX:
    case TW3XX:
        phLogSimMessage(logId, LOG_VERBOSE,
                        "\nEntering TechWingHandlerController::getSrqByte(): \n"
                        "    SitePopulation = %ld \n"
                        "    controlling_remote = %x \n"
                        "    input_empty = %x \n"
                        "    handler_empty = %x \n"
                        "    wait_for_soak = %x \n"
                        "    out_of_temp = %x \n"
                        "    temp_stable = %x \n"
                        "    temp_alarm = %x \n"
                        "    low_alarm = %x \n"
                        "    high_alarm = %x \n"
                        "    stopped = %x \n"
                        "    waiting_tester_response = %x \n",
                        pHandler->getSitePopulationAsLong(),
                        pTechWingstatus->getStatusBit(TechWingStatus::controlling_remote),
                        pTechWingstatus->getStatusBit(TechWingStatus::input_empty),
                        pTechWingstatus->getStatusBit(TechWingStatus::handler_empty),
                        pTechWingstatus->getStatusBit(TechWingStatus::wait_for_soak),
                        pTechWingstatus->getStatusBit(TechWingStatus::out_of_temp),
                        pTechWingstatus->getStatusBit(TechWingStatus::temp_stable),
                        pTechWingstatus->getStatusBit(TechWingStatus::temp_alarm),
                        pTechWingstatus->getStatusBit(TechWingStatus::low_alarm),
                        pTechWingstatus->getStatusBit(TechWingStatus::high_alarm),
                        pTechWingstatus->getStatusBit(TechWingStatus::stopped),
                        pTechWingstatus->getStatusBit(TechWingStatus::waiting_tester_response)
                       );
        if (((pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::controlling_remote)      == 0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::input_empty)             == 0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::handler_empty)           == 0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::wait_for_soak)           == 0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::out_of_temp)             == 0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::temp_stable)             == 1) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::temp_alarm)              == 0) &&
// don't care?   (pTechWingstatus->getStatusBit(TechWingStatus::low_alarm) == 0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::high_alarm)              == 0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::stopped)                 == 0)
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::waiting_tester_response) == 0) &&
            )
        {
            srqByte = 0x41;
        }
        else if (
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::controlling_remote)      == 0) &&
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::input_empty)             == 0) &&
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::handler_empty)           == 0) &&
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::wait_for_soak)           == 0) &&
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::out_of_temp)             == 0) &&
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::temp_stable)             == 1) &&
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::temp_alarm)              == 0) &&
            (pTechWingstatus->getStatusBit(TechWingStatus::low_alarm)               == 1) )
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::high_alarm)              == 0) &&
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::stopped)                 == 1) &&
// don't care?            (pTechWingstatus->getStatusBit(TechWingStatus::waiting_tester_response) == 0))
        {
            srqByte = 0x3d; // handler jammed
        }
        else {
            srqByte = 0x00;  // ??
        }
        break;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte before mask %d",int(srqByte));

                 // Mask out bits not needed
    srqByte&=srqMask;

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte with mask %d",int(srqByte));

    return srqByte;
}

void TechWingHandlerController::switchOn() 
{
    bool sendLotStart = true;

    while (!done)
    {
        if ( pEventPlayback )
        {
            pEventPlayback->checkEvent( getReceiveMessageCount() );
        }
        
        phLogSimMessage(logId, LOG_VERBOSE, "TechWingHandlerController::switchOn() calling checkEvent(0)");
        pTechWingViewController->checkEvent(0);
        
        phLogSimMessage(logId, LOG_VERBOSE, "TechWingHandlerController::switchOn() calling tryReceiveMsg(1000)");
        tryReceiveMsg(0);

      
        if ( pHandler->sendTestStartSignal() )
        {
          phLogSimMessage(logId, LOG_DEBUG, "TechWingHandlerController sendTestStartSignal == TRUE");

          pTechWingstatus->setStatusBit(this, TechWingStatus::stopped, false);

          if ( sendLotStart )
          {
            sendSRQ(0x46); /* lot start SRQ */
            sendLotStart = false;
            sleep(2);
          }
          unsigned char srqSequence=getSrqByte();

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

        if ( pHandler->allTestsComplete() && pTechWingstatus->getStatusBit(TechWingStatus::handler_empty)==false )
        {
            pHandler->logHandlerStatistics();

            pTechWingstatus->setStatusBit(this, TechWingStatus::stopped, true);
            pTechWingstatus->setStatusBit(this, TechWingStatus::handler_empty, true);
            sendSRQ(0x48);  /* send lot done */
        }
    }
    phLogSimMessage(logId, LOG_DEBUG, "TechWingHandlerController::switchOn() Exiting");
}

void TechWingHandlerController::TechWingStatusChanged(TechWingStatus::eTechWingStatus ebit, bool b)
{
    switch ( ebit )
    {
        case TechWingStatus::stopped:
            phLogSimMessage(logId, LOG_VERBOSE, "TechWingHandlerController stop detected new value %s",
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
        case TechWingStatus::handler_empty:
            phLogSimMessage(logId, LOG_VERBOSE, "TechWingHandlerController empty detected new value %s",
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
        default:
            break;
    }
}

TechWingHandlerController::eTechWingModel TechWingHandlerController::getModelType() const
{
    return model;
}

/*--- TechWingHandlerController protected and public functions --------------------*/


void TechWingHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_DEBUG, "Received message \"%s\"",message);

    setStrlower(message);

    if (phToolsMatch(message, "fullsites\\?"))
        serviceFullsites(message);
    else if (phToolsMatch(message, "fr\\?"))
        serviceStatus(message);
    else if (phToolsMatch(message, "binon:.*"))
        serviceBinon(message);
    else if (phToolsMatch(message, "echook"))
        serviceEchoOk(message);
    else if (phToolsMatch(message, "echong"))
        serviceEchoNg(message);
//    else if (phToolsMatch(message, "srqmask .*"))
//        serviceSrqmask(message);
    else if (phToolsMatch(message, "testset .*"))
        serviceTestset(message);
    else if (phToolsMatch(message, "runmode .*"))
        serviceRunmode(message);
    else if (phToolsMatch(message, "settemp .*"))
        serviceSettemp(message);
    else if (phToolsMatch(message, "setband .*"))
        serviceSetband(message);
    else if (phToolsMatch(message, "setsoak .*"))
        serviceSetsoak(message);
    else if (phToolsMatch(message, "tempctrl .*"))
        serviceTempctrl(message);
    else if (phToolsMatch(message, "sitesel .*") || phToolsMatch(message, "contactsel .*"))
        serviceSitesel(message);
    else if (phToolsMatch(message, "setname .*"))
        serviceSetname(message);
    else if (phToolsMatch(message, "sitemap .*"))
        serviceSitemap(message);
    else if (phToolsMatch(message, "setbin .*"))
        serviceSetbin(message);
    else if (phToolsMatch(message, "loader"))
        serviceLoader(message);
    else if (phToolsMatch(message, "start"))
        serviceStart(message);
    else if (phToolsMatch(message, "stop"))
        serviceStop(message);
    else if (phToolsMatch(message, "plunger .*"))
        servicePlunger(message);
    else if (phToolsMatch(message, "version\\?"))
        serviceVersion(message);
    else if (phToolsMatch(message, "name\\?"))
        serviceNameQuery(message);
    else if (phToolsMatch(message, "testset\\?"))
        serviceTestsetQuery(message);
    else if (phToolsMatch(message, "settemp\\?"))
        serviceSetTempQuery(message);
    else if (phToolsMatch(message, "meastemp .*\\?"))
        serviceMeasTempQuery(message);
    else if (phToolsMatch(message, "status\\?"))
        serviceStatusQuery(message);
    else if (phToolsMatch(message, "jam\\?"))
        serviceJamQuery(message);
    else if (phToolsMatch(message, "jamcode\\?"))
        serviceJamcodeQuery(message);
    else if (phToolsMatch(message, "jamque\\?"))
        serviceJamqueQuery(message);
    else if (phToolsMatch(message, "jamcount\\?"))
        serviceJamCount(message);
    else if (phToolsMatch(message, "setlamp\\?"))
        serviceSetlampQuery(message);
    else if (phToolsMatch(message, "testkind\\?"))
        serviceTestKind(message);
    else
    {
        pTechWingViewController->setGpibStatus(ViewController::notUnderstood);
    }
}


void TechWingHandlerController::serviceStatus(const char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    bool condition;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    condition = ((pTechWingstatus->getStatusBit(TechWingStatus::stopped)) ||
                 (pTechWingstatus->getStatusBit(TechWingStatus::low_alarm)) ||
                 (pTechWingstatus->getStatusBit(TechWingStatus::high_alarm)) ||
                 (pTechWingstatus->getStatusBit(TechWingStatus::temp_alarm)));
    sprintf(messageBuffer, "FR %d", int(!condition));

    pTechWingstatus->logStatus();

    replyToQuery(messageBuffer);
 
    return;
}

void TechWingHandlerController::serviceFullsites(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    unsigned long long devicesReady;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    devicesReady=pHandler->getSitePopulationAsLong();

    sprintf(messageBuffer, "FULLSITES %.16llx", devicesReady );

    replyToQuery(messageBuffer);

    return;
}

void TechWingHandlerController::serviceBinon(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    //
    // get binon information
    // expected format
    // BINON:xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx;
    //
    //int site=31;
    int site= numOfSites*4 -1;
    int bin;
    const char *posMsg=msg;

phLogSimMessage(logId, LOG_DEBUG, "entering serviceBinon, message = ->%s<-\n",msg);

    /* find first space */
    posMsg=strchr(posMsg,':');

    while ( posMsg && *posMsg && site>=0 )
    {
        posMsg=findHexadecimalDigit(posMsg);

        if ( isxdigit(*posMsg) )
        {
            if ( site < pHandler->getNumOfSites() )
            {
                bin=convertHexadecimalDigitIntoInt(*posMsg);
phLogSimMessage(logId, LOG_DEBUG, "In serviceBinon, site = %d, bin = %x\n",site, bin);

                if ((*posMsg != 'A') && (*posMsg != 'a')) 
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
            char arr[MAX_MSG_LENGTH] = "";
            strcpy(arr,msg);
            serviceEcho(arr);
        }
    }
    phLogSimMessage(logId, LOG_DEBUG, "exiting serviceBinon, message = ->%s<-\n",msg);
}


void TechWingHandlerController::serviceEcho(char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    char tmp[64];
    int site;
    int bin = 0;

    phLogSimMessage(logId, LOG_VERBOSE, "serviceEcho()");

    //
    // echo bin information
    // format
    // ECHO xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx;
    //
    strcpy(messageBuffer,"ECHO:");
    
    char* p  = strstr(msg,":");
    strcat(messageBuffer,p+1);
    sendMessage(messageBuffer);

    return;
}

void TechWingHandlerController::serviceEchoOk(const char *msg)
{
    int s;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\", numSites = ",msg, pHandler->getNumOfSites());

    for ( s=0 ; s < pHandler->getNumOfSites() ; ++s)
    {
        pHandler->releaseDevice(s);
    }
    pHandler->resetVerifyCount();

    return;
}

void TechWingHandlerController::serviceEchoNg(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    pHandler->binDataVerified();

    if ( pHandler->isStopped() )
    {
        pTechWingstatus->setStatusBit(this, TechWingStatus::stopped, true);
    }
    return;
}


void TechWingHandlerController::serviceSrqmask(const char *msg)
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


void TechWingHandlerController::serviceTestset(const char *msg)
{

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??
    
    // sprintf(messageBuffer, "",  );

    sendMessage("SettingOK");

    return;
}


void TechWingHandlerController::serviceRunmode(const char *msg)
{

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("SettingOK");

    return;
}



void TechWingHandlerController::serviceSettemp(const char *msg)
{

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("SettingOK");

    return;
}



void TechWingHandlerController::serviceSetband(const char *msg)
{

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("SettingOK");

    return;
}



void TechWingHandlerController::serviceSetsoak(const char *msg)
{

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("SettingOK");

    return;
}



void TechWingHandlerController::serviceTempctrl(const char *msg)
{

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("SettingOK");

    return;
}



void TechWingHandlerController::serviceSitesel(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";
    int retVal = 0;
    char command[MAX_MSG_LENGTH] = "";
    long long mask = 0;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    
    retVal = sscanf(msg, "%s %lx", command, (long unsigned*)&mask);
    if (retVal == 2)
    {
        // prepare results;
        pHandler->setSitesEnabled(mask);
        sprintf(messageBuffer, "SettingOK");
    }
    else 
    {
        sprintf(messageBuffer, "SettingNG");
    }

    sendMessage(messageBuffer);

    return;
}



void TechWingHandlerController::serviceSetname(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "SettingOK");

    sendMessage(messageBuffer);

    return;
}



void TechWingHandlerController::serviceSitemap(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "%s", msg);

    sendMessage(messageBuffer);

    return;
}



void TechWingHandlerController::serviceSetbin(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "SettingOK");

    replyToQuery(messageBuffer);

    return;
}



void TechWingHandlerController::serviceVersion(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "TechWing Test Simulator");

    replyToQuery(messageBuffer);

    return;
}



void TechWingHandlerController::serviceNameQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "Test Name");

    replyToQuery(messageBuffer);

    return;
}



void TechWingHandlerController::serviceLoader(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    // ??

    sprintf(messageBuffer, "done");

    sendMessage("done");

    return;
}



void TechWingHandlerController::serviceStart(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    TechWingStatusChanged(TechWingStatus::stopped, 0);

    sprintf(messageBuffer, "done");

    sendMessage(messageBuffer);

    return;
}



void TechWingHandlerController::serviceStop(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    TechWingStatusChanged(TechWingStatus::stopped, 0);

    sprintf(messageBuffer, "done");

    sendMessage(messageBuffer);

    return;
}



void TechWingHandlerController::servicePlunger(const char *msg)
{

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("done");

    return;
}



void TechWingHandlerController::serviceTestsetQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "TestSet 8,4,A");

    replyToQuery(messageBuffer);

    return;
}



void TechWingHandlerController::serviceSetTempQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "30.1");

    replyToQuery(messageBuffer);

    return;
}



void TechWingHandlerController::serviceMeasTempQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "33.3");

    replyToQuery(messageBuffer);

    return;
}



void TechWingHandlerController::serviceStatusQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "0000000000000000";

    phLogSimMessage(logId, LOG_DEBUG, "handling \"%s\"",msg);

    phLogSimMessage(logId, LOG_DEBUG,
                    "\nEntering TechWingHandlerController::serviceStatusQuery(): \n"
                    "    controlling_remote = %x \n"
                    "    input_empty = %x \n"
                    "    handler_empty = %x \n"
                    "    wait_for_soak = %x \n"
                    "    out_of_temp = %x \n"
                    "    temp_stable = %x \n"
                    "    temp_alarm = %x \n"
                    "    low_alarm = %x \n"
                    "    high_alarm = %x \n"
                    "    stopped = %x \n"
                    "    waiting_tester_response = %x \n",
                    pTechWingstatus->getStatusBit(TechWingStatus::controlling_remote),
                    pTechWingstatus->getStatusBit(TechWingStatus::input_empty),
                    pTechWingstatus->getStatusBit(TechWingStatus::handler_empty),
                    pTechWingstatus->getStatusBit(TechWingStatus::wait_for_soak),
                    pTechWingstatus->getStatusBit(TechWingStatus::out_of_temp),
                    pTechWingstatus->getStatusBit(TechWingStatus::temp_stable),
                    pTechWingstatus->getStatusBit(TechWingStatus::temp_alarm),
                    pTechWingstatus->getStatusBit(TechWingStatus::low_alarm),
                    pTechWingstatus->getStatusBit(TechWingStatus::high_alarm),
                    pTechWingstatus->getStatusBit(TechWingStatus::stopped),
                    pTechWingstatus->getStatusBit(TechWingStatus::waiting_tester_response)
                   );
    // prepare results;
    messageBuffer[3] = 
    pTechWingstatus->getStatusBit(TechWingStatus::controlling_remote) ? '1' : '0';
    messageBuffer[6] =
    pTechWingstatus->getStatusBit(TechWingStatus::input_empty) ? '1' : '0';
    messageBuffer[7] =
    pTechWingstatus->getStatusBit(TechWingStatus::handler_empty) ? '1' : '0';
    messageBuffer[8] = 
    pTechWingstatus->getStatusBit(TechWingStatus::wait_for_soak) ? '1' : '0';
    messageBuffer[9] = 
    pTechWingstatus->getStatusBit(TechWingStatus::out_of_temp) ? '1' : '0';
    messageBuffer[10] = 
    pTechWingstatus->getStatusBit(TechWingStatus::temp_stable) ? '1' : '0';
    messageBuffer[11] = 
    pTechWingstatus->getStatusBit(TechWingStatus::temp_alarm) ? '1' : '0';
    messageBuffer[12] = 
    pTechWingstatus->getStatusBit(TechWingStatus::low_alarm) ? '1' : '0';
    messageBuffer[13] = 
    pTechWingstatus->getStatusBit(TechWingStatus::high_alarm) ? '1' : '0';
    messageBuffer[14] = 
    pTechWingstatus->getStatusBit(TechWingStatus::stopped) ? '1' : '0';
    messageBuffer[15] = 
    pTechWingstatus->getStatusBit(TechWingStatus::waiting_tester_response) ? '1' : '0';

    replyToQuery(messageBuffer);

    phLogSimMessage(logId, LOG_DEBUG,
                    "\nExiting TechWingHandlerController::serviceStatusQuery(), reply: ->%s<-, length = %d\n",
                    messageBuffer, strlen(messageBuffer));

    return;
    }



void TechWingHandlerController::serviceJamQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "Jam 1");

    replyToQuery(messageBuffer);

    return;
}



void TechWingHandlerController::serviceJamcodeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "010102");

    replyToQuery(messageBuffer);

    return;
}

void TechWingHandlerController::serviceTestKind(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "TESTKIND 00");

    replyToQuery(messageBuffer);

    return;
}


void TechWingHandlerController::serviceJamqueQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "JamQue 010102, 030205, 020301;");

    replyToQuery(messageBuffer);

    return;
}

void TechWingHandlerController::serviceJamCount(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "Jamcount 3");

    replyToQuery(messageBuffer);

    return;
}


void TechWingHandlerController::serviceSetlampQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "SetLamp 0,2,1");

    replyToQuery(messageBuffer);

    return;
}




/*****************************************************************************
 * End of file
 *****************************************************************************/
