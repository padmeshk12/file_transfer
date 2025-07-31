/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SEhandlerController.C
 * CREATED  : 25 Jan 2001
 *
 * CONTENTS : Seiko-Epson handler controller implementation
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 25 Jan 2001, Chris Joyce , created
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
#include "SEhandlerController.h"
#include "handlerController.h"
#include "handler.h"
#include "SEviewController.h"
#include "eventObserver.h"
#include "simHelper.h"

#include "ph_tools.h"
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

/*--- SEHandlerController ---------------------------------------------------*/


SEHandlerController::SEHandlerController(SEHandlerControllerSetup *setup,
                                         ViewControllerSetup    *VCsetup) :
    HandlerController(setup),
    model(setup->model),
    verifyTest(setup->verifyTest),
    multipleSrqs(setup->multipleSrqs)
{
    try {

        pSEstatus = new SeikoEpsonStatus(logId);
        pSEstatus->registerObserver(this);
        pSEstatus->setStatusBit(this, SeikoEpsonStatus::stopped, true);

        pSEViewController = new SEViewController(logId,this,pSEstatus,VCsetup);
        pSEViewController->showView();
        pViewController=pSEViewController;
        dl = 0;
        retestTime = setup->retestTime;

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
                                               pSEstatus,
                                               pSEViewController,
                                               setup->recordFile);
        }

        if ( setup->playbackFile )
        {
            pEventPlayback = new EventPlayback(logId,
                                               pSEstatus,
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

    pSEViewController->setGpibStatus(ViewController::okay);
}

SEHandlerController::~SEHandlerController()
{
}

unsigned char SEHandlerController::getSrqByte()
{
    unsigned char srqByte = 0;

    pSEstatus->logStatus();

    switch ( model )
    {
      case NS5000:
      case NS6000:
      case NS7000:
      case HONTECH:
      case NS6040:
      case NS8040:
      case CHROMA:
                    // bit 0 Demand of start
        srqByte = (pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0 ;

                    // bit 1 (don't use) Demand of manual start

                    // bit 2 Alarm set on handler
        srqByte|= (1<<2) * pSEstatus->getStatusBit(SeikoEpsonStatus::alarm);

                    // bit 3 Error in transmission
        srqByte|= (1<<3) * pSEstatus->getStatusBit(SeikoEpsonStatus::errorTx);

                    // bit 4 Receive result of test

                    // bit 5 don't use

                    // bit 7 Spare
        break;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte before mask %d",int(srqByte));

                 // Mask out bits not needed
    srqByte&=srqMask;

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte with mask %d",int(srqByte));

    return srqByte;
}

void SEHandlerController::switchOn() 
{
    static int iRetestFlag = retestTime;    //retest flag, judge the srq kind
    while (!done)
    {
        if ( pEventPlayback )
        {
            pEventPlayback->checkEvent( getReceiveMessageCount() );
        }

        pSEViewController->checkEvent(0);

        tryReceiveMsg(1000);

        if ( pHandler->sendTestStartSignal() )
        {
            pSEstatus->setStatusBit(this, SeikoEpsonStatus::stopped, false);

            if (iRetestFlag == -1)
            {
                //normal model
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
            else
            {
                //retest model
                if (iRetestFlag == retestTime)
                {
                    //test start or retest start
                    unsigned char specialSrq=0x80;
                    specialSrq |= (1<<6);

                    sendSRQ(specialSrq);
                    iRetestFlag--;                              //to ensure will send 0x43 to tester next time
                }
                else
                {
                    //during test
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
            }
        }

        if ( pHandler->allTestsComplete() && pSEstatus->getStatusBit(SeikoEpsonStatus::empty)==false )
        {
            pHandler->logHandlerStatistics();
            retestTime--;                                       //reduce time of retest to ensure will send 0x80 next time
            if (retestTime > 0)
            {
                //retest model
                unsigned char specialSrq=0x80;
                specialSrq |= (1<<6);
                sendSRQ(specialSrq);
                pHandler->reset();
                pHandler->start();
            }
            else if (retestTime == -1)
            {
                //nomal model
                pSEstatus->setStatusBit(this, SeikoEpsonStatus::stopped, true);
                pSEstatus->setStatusBit(this, SeikoEpsonStatus::empty, true);
            }
            else
            {
                //retest model end
                unsigned char specialSrq=0x80;
                specialSrq |= (1<<6);
                sendSRQ(specialSrq);
                pSEstatus->setStatusBit(this, SeikoEpsonStatus::stopped, true);
                pSEstatus->setStatusBit(this, SeikoEpsonStatus::empty, true);
            }
        }
    }
}

void SEHandlerController::SEStatusChanged(SeikoEpsonStatus::eSEStatus ebit, bool b)
{
    switch (ebit)
    {
      case SeikoEpsonStatus::stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "SEHandlerController stop detected new value %s",
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
      case SeikoEpsonStatus::alarm:
        phLogSimMessage(logId, LOG_VERBOSE, "SEHandlerController alarm detected new value %s",
                        (b) ? "True" : "False");
        break;
      case SeikoEpsonStatus::errorTx:
        phLogSimMessage(logId, LOG_VERBOSE, "SEHandlerController errorTx detected new value %s",
                        (b) ? "True" : "False");
        break;
      case SeikoEpsonStatus::empty:
        phLogSimMessage(logId, LOG_VERBOSE, "SEHandlerController empty detected new value %s",
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

SEHandlerController::eSEModel SEHandlerController::getModelType() const
{
    return model;
}

/*--- SEHandlerController protected and public functions --------------------*/


void SEHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_NORMAL, "Received message \"%s\"",message);

    setStrlower(message);

    if (phToolsMatch(message, "fullsites\\?"))
        serviceFullsites(message);
    else if (phToolsMatch(message, "barcode\\?"))
        serviceBarcode(message);
    else if (phToolsMatch(message, "echocode:.*"))
        serviceEchoCode(message);
    else if (phToolsMatch(message, "binon:.*"))
        serviceBinon(message);
    else if (phToolsMatch(message, "echook"))
        serviceEchoOk(message);
    else if (phToolsMatch(message, "echong"))
        serviceEchoNg(message);
    else if(phToolsMatch(message,"inputqty.*"))
    {
        phLogSimMessage(logId, LOG_NORMAL, "input qty set success");
        replyToQuery("SETTINGOK");
    }
    else if (phToolsMatch(message, "fr\\?"))
        serviceStatus(message);
    else if (model == HONTECH && ( phToolsMatch(message, "bincount\\?") ||
                                   phToolsMatch(message, "force\\?") ||
                                   phToolsMatch(message, "setsitemap\\?") ||
                                   phToolsMatch(message, "setsoak\\?") ||
                                   phToolsMatch(message, "settemp\\?") ||
                                   phToolsMatch(message, "temparm\\?") ||
                                   phToolsMatch(message, "test arm\\?") ||
                                   phToolsMatch(message, "barcode\\?") ||
                                   phToolsMatch(message, "handler id\\?")))
        serviceStatus(message);
    else if (phToolsMatch(message, "srqmask .*"))
        serviceSrqmask(message);
    else if (phToolsMatch(message, "dl [0|1]"))
        serviceDl(message);
    else if(phToolsMatch(message,"lotorder [0|1|2]"))
        phLogSimMessage(logId, LOG_DEBUG, "handling \"%s\"", message);
    else if(phToolsMatch(message,"lotclear\\?"))
        replyToQuery("LOTCLEARED");
    else if(phToolsMatch(message,"lotretestclear\\?"))
        replyToQuery("LOTRETESTCLEAR");
    else if (phToolsMatch(message, "srqkind\\?"))
        serviceSrqKind(message);
    else if (phToolsMatch(message, "echocode:.*"))
        replyToQuery("ECHOCODEOK");
    else if (phToolsMatch(message, "srqkind\\?"))
        replyToQuery("SRQKIND 8");
    else
    {
        pSEViewController->setGpibStatus(ViewController::notUnderstood);
    }
}

void SEHandlerController::serviceStatus(const char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    if(strcmp(msg, "fr?") == 0)
    {
        sprintf(messageBuffer, "FR%d", int(!pSEstatus->getStatusBit(SeikoEpsonStatus::stopped)) );
    }
    else if(strcmp(msg, "bincount?") == 0)
    {
        sprintf(messageBuffer, "BIN1_123_BIN2_123_BIN3_123_BIN4_123_BIN5_123_BIN6_123_BIN7_123_BIN8_123_BIN9_123_BIN10_123_BIN11_123_BIN12_123_BIN13_123_BIN14_123_BIN15_123_");
    }
    else if(strcmp(msg, "barcode?") == 0)
    {
        sprintf(messageBuffer, "BARCODE:32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,MR1200HJKL00000H000000001;");
    }
    else if(strcmp(msg, "force?") == 0)
    {
        sprintf(messageBuffer, "40T");
    }
    else if(strcmp(msg, "setsitemap?") == 0)
    {
        sprintf(messageBuffer, "QUAL2X2-1-2-3-4_");
    }
    else if(strcmp(msg, "setsoak?") == 0)
    {
        sprintf(messageBuffer, "120");
    }
    else if(strcmp(msg, "settemp?") == 0)
    {
        sprintf(messageBuffer, "+25.0");
    }
    else if(strcmp(msg, "temparm?") == 0)
    {
        sprintf(messageBuffer, "QUAD2X2_60.3_60.3_60.4_60.4_");
    }
    else if(strcmp(msg, "test arm?") == 0)
    {
        sprintf(messageBuffer, "1");
    }
    else if(strcmp(msg, "handler id?") == 0)
    {
        sprintf(messageBuffer, "THT-7045-001");
    }

    pSEstatus->logStatus();

    replyToQuery(messageBuffer);
 
    return;
}

void SEHandlerController::serviceFullsites(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    unsigned long devicesReady;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    devicesReady=pHandler->getSitePopulationAsLong();
    if(model == CHROMA)
    {
        sprintf(messageBuffer, "FULLSITES: %.8lx,1#001,2#002", devicesReady );
    }
    else
    sprintf(messageBuffer, "FULLSITES %.8lx", devicesReady );

    replyToQuery(messageBuffer);

    return;
}

void SEHandlerController::serviceBarcode(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    char messageBuffer[MAX_MSG_LENGTH] = "BARCODE:0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,MR1200HJKL04,MR1200HJKL03,MR1200HJKL02,MR1200HJKL01;";
    replyToQuery(messageBuffer);
    return;
}

void SEHandlerController::serviceEchoCode(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    char messageBuffer[MAX_MSG_LENGTH] = "ECHOCODEOK";
    replyToQuery(messageBuffer);
    return;
}

void SEHandlerController::serviceBinon(const char *msg)
{
    phLogSimMessage(logId, LOG_NORMAL, "handling \"%s\"",msg);

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

    if (pHandler->getBinningFormat() == 16)
    {
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
    }
    else if (pHandler->getBinningFormat() == 32)
    {
        while ( posMsg && *posMsg && site>=0 )
        {
            posMsg=findExternalBinCode(posMsg);

            if(phToolsMatch(posMsg, "[0-9|a-w].*"))
            {
                if ( site < pHandler->getNumOfSites() )
                {
                    bin = convertExternalBinCodeToInt(posMsg, 32);
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
    }
    else if (pHandler->getBinningFormat() == 256)
    {
        while (posMsg && *posMsg && site>=0)
        {
            posMsg=findExternalBinCode(posMsg);

            if (phToolsMatch(posMsg, "[0-9]{3}.*"))
            {
                if (site < pHandler->getNumOfSites())
                {
                    bin = convertExternalBinCodeToInt(posMsg, 256);
                    if (bin != 0)
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
                posMsg+=3;
            }
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


void SEHandlerController::serviceEcho()
{
    char messageBuffer[MAX_MSG_LENGTH];
    char tmp[64];
    int site;
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
                  case NS5000:
                  case NS6000:
                  case NS7000:
                  case HONTECH:
                  case NS6040:
                  case NS8040:
                  case CHROMA:
                    bin=0;
                    break;
                }
            }
        }
        else
        {
            switch ( model )
            {
              case NS5000:
              case NS6000:
              case NS7000:
              case HONTECH:
              case NS6040:
              case NS8040:
              case CHROMA:
                bin=0;
                break;
            }
        }

        if (pHandler->getBinningFormat() == 256)
        {
            sprintf(tmp, "%03d", bin);
            strcat(messageBuffer, tmp);
        }
        else
        {
            if (bin <= 15)
            {
                sprintf(tmp, "%x", bin);
                strcat(messageBuffer,tmp);
            }
            else
            {
                sprintf(tmp, "%c", bin+55);
                strcat(messageBuffer,tmp);
            }
        }

        if ( site == 0 )
        {
            strcat(messageBuffer,";");
        }
        else if ( site % 8 == 0 )
        {
            strcat(messageBuffer,",");
        }
    }
    phLogSimMessage(logId, LOG_NORMAL, messageBuffer);
    sendMessage(messageBuffer);

    return;
}

void SEHandlerController::serviceEchoOk(const char *msg)
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

void SEHandlerController::serviceEchoNg(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    pHandler->binDataVerified();

    if ( pHandler->isStopped() )
    {
        pSEstatus->setStatusBit(this, SeikoEpsonStatus::stopped, true);
    }
    return;
}

void SEHandlerController::serviceDl(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    if (sscanf(msg, "dl %d",&dl) != 1)
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"dl n\"",msg);

    }

    return;
}

void SEHandlerController::serviceSrqKind(const char *msg)
{
    static int iLotStartFlag = 1;           //record retest status to judge how to deal with special SRQ
    if (iLotStartFlag == 1)
    {
        //first time special SRQ received
        phLogSimMessage(logId, LOG_DEBUG, "send lot start signal");
        char messageBuffer[MAX_MSG_LENGTH];
        char srqKind[] = "02";
        sprintf(messageBuffer, "SRQKIND %s", srqKind);
        replyToQuery(messageBuffer);
        iLotStartFlag = 0;                  //send lot end or final lot end signal
    }
    else if (iLotStartFlag == 2)
    {
        //lot retest signal
        phLogSimMessage(logId, LOG_DEBUG, "send lot retest start signal");
        char messageBuffer[MAX_MSG_LENGTH];
        char srqKind[] = "04";
        sprintf(messageBuffer, "SRQKIND %s", srqKind);
        replyToQuery(messageBuffer);
        iLotStartFlag = 0;
    }
    else if(iLotStartFlag == 0 && retestTime != 0)
    {
        //lot end signal
        phLogSimMessage(logId, LOG_DEBUG, "send lot end signal");
        char messageBuffer[MAX_MSG_LENGTH];
        char srqKind[] = "08";
        sprintf(messageBuffer, "SRQKIND %s", srqKind);
        replyToQuery(messageBuffer);
        iLotStartFlag = 2;                  //send lot restart signal
    }
    else if(iLotStartFlag == 0 && retestTime == 0)
    {
        //final lot end signal
        phLogSimMessage(logId, LOG_DEBUG, "send lot final end signal");
        char messageBuffer[MAX_MSG_LENGTH];
        char srqKind[] = "10";
        sprintf(messageBuffer, "SRQKIND %s", srqKind);
        replyToQuery(messageBuffer);
    }

    return;
}

void SEHandlerController::serviceSrqmask(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /*
     * Mask byte for SRQ generation.
     */

    if ( sscanf(msg,"srqmask %lx",&srqMask) != 1 )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"srqmask n\"",msg);
    }

    return;
}




/*****************************************************************************
 * End of file
 *****************************************************************************/
