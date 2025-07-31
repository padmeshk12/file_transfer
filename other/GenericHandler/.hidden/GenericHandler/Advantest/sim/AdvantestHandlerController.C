/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : AdvantestHandlerController.C
 * CREATED  : 29 Apri 2014
 *
 * CONTENTS : Advantest handler controller implementation
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

/*--- module includes -------------------------------------------------------*/
#include "AdvantestHandlerController.h"
#include "handlerController.h"
#include "handler.h"
#include "AdvantestViewController.h"
#include "eventObserver.h"
#include "simHelper.h"
#include "ph_tools.h"
#include <string>
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

using namespace std;


/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/



/*--- class implementations -------------------------------------------------*/

/*--- AdvantestHandlerController ---------------------------------------------------*/


AdvantestHandlerController::AdvantestHandlerController(AdvantestHandlerControllerSetup *setup,
                                         ViewControllerSetup    *VCsetup) :
    HandlerController(setup),
    model(setup->model),
    verifyTest(setup->verifyTest),
    retestTime(setup->retestTime)
{
    try {
        pAdvantestStatus = new AdvantestStatus(logId);
        pAdvantestStatus->registerObserver(this);
        pAdvantestStatus->setStatusBit(this, AdvantestStatus::stopped, true);
      
        pAdvantestViewController = new AdvantestViewController(logId,this,pAdvantestStatus,VCsetup);  
        pAdvantestViewController->showView();
        pViewController=pAdvantestViewController;

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
                                               pAdvantestStatus,
                                               pAdvantestViewController,
                                               setup->recordFile);
        }

        if ( setup->playbackFile )
        {
            pEventPlayback = new EventPlayback(logId,
                                               pAdvantestStatus,
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
    dl = 0;
    srq = 1;

    pAdvantestViewController->setGpibStatus(ViewController::okay);
}

AdvantestHandlerController::~AdvantestHandlerController()
{
}

unsigned char AdvantestHandlerController::getSrqByte()
{
    unsigned char srqByte = 0;

    pAdvantestStatus->logStatus();

    switch ( model )
    {
      case GS1:
      case M45:
      case M48:
      case DLT: /* add DLT for enhance*/
      case Yushan: /* add Yushan for enhance*/
                    // bit 0 Demand of start
        srqByte = (pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0 ;

                    // bit 1 (don't use) Demand of manual start

                    // bit 2 Alarm set on handler
        srqByte|= (1<<2) * pAdvantestStatus->getStatusBit(AdvantestStatus::alarm);

                    // bit 3 Error in transmission
        srqByte|= (1<<3) * pAdvantestStatus->getStatusBit(AdvantestStatus::errorTx);

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

void AdvantestHandlerController::switchOn() 
{
    static int iRetestFlag = retestTime;    //retest flag, judge the srq kind
    static int iTimeCtrl = -1;              //random control
    int iRandom = 0;
    while (!done)
    {
        if ( pEventPlayback )
        {
            pEventPlayback->checkEvent( getReceiveMessageCount() );
        }

        pAdvantestViewController->checkEvent(0);

        tryReceiveMsg(1000);

        iRandom = pViewController->getRandomFlag();             //get random value, it will not change before thermal alarm generated
        if (iRandom != 0)
        {
            iTimeCtrl++;
            phLogSimMessage(logId, LOG_DEBUG, "getRandomflag = %d, iTimeCtrl = %d", iRandom, iTimeCtrl);
            if (iTimeCtrl == iRandom*10)                        //the thermal alarm signal always be triggered at multiple of 10 seconds 
            {
                //ensure there will generate a thermal alarm signal in 100 seconds
                phLogSimMessage(logId, LOG_DEBUG, "getRandomflag = %d, iTimeCtrl = %d", iRandom, iTimeCtrl);
                iTimeCtrl = 0;                                  //reset time
                pViewController->setRandomFlag(rand()%10+1);    //new random value
                sendSRQ(96);
            }
        }
        if ( pHandler->sendTestStartSignal() ) 
        {
            pAdvantestStatus->setStatusBit(this, AdvantestStatus::stopped, false);
            
            if (retestTime == -1)
            {
                //normal model
                unsigned char srqSequence=getSrqByte();
                // bit 6 SRQ asserted
                srqSequence|= (1<<6); 
                sendSRQ(srqSequence);
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
                    sendSRQ(srqSequence);
                }
            }
        }

        if ( pHandler->allTestsComplete() && pAdvantestStatus->getStatusBit(AdvantestStatus::empty)==false )
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
                pAdvantestStatus->setStatusBit(this, AdvantestStatus::stopped, true);
                pAdvantestStatus->setStatusBit(this, AdvantestStatus::empty, true); 
            }
            else
            {
                //retest model end
                unsigned char specialSrq=0x80;
                specialSrq |= (1<<6);
                sendSRQ(specialSrq);
                pAdvantestStatus->setStatusBit(this, AdvantestStatus::stopped, true);
                pAdvantestStatus->setStatusBit(this, AdvantestStatus::empty, true); 
            }
        }
    }
}

void AdvantestHandlerController::AdvantestStatusChanged(AdvantestStatus::eAdvantestStatus ebit, bool b)
{
    switch (ebit)
    {
      case AdvantestStatus::stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "AdvantestHandlerController stop detected new value %s",
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
      case AdvantestStatus::alarm:
        phLogSimMessage(logId, LOG_VERBOSE, "AdvantestHandlerController alarm detected new value %s",
                        (b) ? "True" : "False");
        break;
      case AdvantestStatus::errorTx:
        phLogSimMessage(logId, LOG_VERBOSE, "AdvantestHandlerController errorTx detected new value %s",
                        (b) ? "True" : "False");
        break;
      case AdvantestStatus::empty:
        phLogSimMessage(logId, LOG_VERBOSE, "AdvantestHandlerController empty detected new value %s",
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

AdvantestHandlerController::eAdvantestModel AdvantestHandlerController::getModelType() const
{
    return model;
}

/*--- AdvantestHandlerController protected and public functions --------------------*/


void AdvantestHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_NORMAL, "Received message \"%s\"",message);

    setStrlower(message);

    if (phToolsMatch(message, "fullsites\\?"))
    {
         serviceFullsites(message);
    } 
    else if (phToolsMatch(message, "binon:.*"))
    {
        serviceBinon(message);
    }
    else if (phToolsMatch(message, "binoff:.*"))
    {
        serviceBinoff(message);
    }    
    else if (phToolsMatch(message, "echook"))
    {    
        serviceEchoOk(message);
    }    
    else if (phToolsMatch(message, "echong"))
    {
        serviceEchoNg(message);
    }
    else if (phToolsMatch(message, "fr\\?"))
    { 
        serviceStatus(message);
    } 
    else if (phToolsMatch(message, "srqmask .*"))
    { 
        serviceSrqmask(message);
    } 
    else if (phToolsMatch(message, "dl [0|1]"))
    {
        serviceDl(message);
    }
    else if (phToolsMatch(message, "srq .*") )
    {
        serviceSrq(message);
    }
    else if (phToolsMatch(message, "srqkind\\?"))
    {
        serviceSrqKind(message);
    }
    else if (phToolsMatch(message, "handlernum\\?"))
    {
        serviceHandlerNum(message);
    }
    else if (phToolsMatch(message, "hdlstatus\\?"))
    {
        serviceHdlStatus(message);
    }
    else if (phToolsMatch(message, "meastemp\\?"))
    {
         //serviceMeasTemp(message);
    }
    else if (phToolsMatch(message, "settemp\\?"))
    {
         serviceSetTemp(message);
    }
    else if (phToolsMatch(message, "start"))
    {
         serviceStart(message);
    }
    else if (phToolsMatch(message, "stop"))
    {
         serviceStop(message);
    }
    else if (phToolsMatch(message, "version\\?"))
    {
         serviceVersion(message);
    }
    else if(phToolsMatch(message,"ldpocket\\?"))  /*add DLT LDPOCKET case to enhance DLT*/
    {
         serviceLdPocket(message);
    }
    else if(phToolsMatch(message,"ldtray\\?")) /*add DLT LDTRAY case to enhance DLT */
    {
         serviceLdTray(message);
    }
    else if(phToolsMatch(message,"dvid\\?"))
    {
        serviceDvid(message);
    }
    else if(phToolsMatch(message,"tloop\\?"))
    {
        serviceTloop(message);
    }
    else if(phToolsMatch(message,"tsset\\?"))
    {
        serviceTsset(message);
    }
    else if(phToolsMatch(message,"lotorder [0|1|2]"))
    {
        phLogSimMessage(logId, LOG_DEBUG, "handling \"%s\"", message);
    }
    else if(phToolsMatch(message,"lotclear\\?"))
    {
        replyToQuery("SETTINGOK");
    }
    else if(phToolsMatch(message,"lotretestclear\\?"))
    {
        replyToQuery("LOTRETESTCLEAR");
    }
    else if(phToolsMatch(message,"overtemp\\?"))
    {
        serviceOvertemp(message);
    }
    else if(phToolsMatch(message,"currentalm\\?"))
    {
        replyToQuery("CURRENTALM_TZ0139,TZ0129,TZ0098,TZ0139,TZ0140,TZ0980,TZ8721;");
    }
    else if(phToolsMatch(message,"gpibtimeout.*"))
    {
        phLogSimMessage(logId, LOG_NORMAL, "gpib time out set success");
        replyToQuery("SETTINGOK");
    }
    else if(phToolsMatch(message,"inputqty.*"))
    {
        phLogSimMessage(logId, LOG_NORMAL, "input qty set success");
        replyToQuery("SETTINGOK");
    }
    else if(phToolsMatch(message, "profile.*"))
    {
        if( phToolsMatch(message, "profile (10|[1-9]):(\\w{1,256}).*") )
        {
            phLogSimMessage(logId, LOG_NORMAL, "success");
            replyToQuery("SETTINGOK");
        }
        else
        {
            phLogSimMessage(logId, LOG_NORMAL, "fail");
            replyToQuery("SETTINGNG");
        }
    }
    else if(phToolsMatch(message, "siteprofile.*"))
    {
        phLogSimMessage(logId, LOG_NORMAL, "handling \"%s\"",message);
        if( phToolsMatch(message, "siteprofile:(\\w{1,256}).*") )
        {
            phLogSimMessage(logId, LOG_NORMAL, "success");
            replyToQuery("SETTINGOK");
        }
        else
        {
            phLogSimMessage(logId, LOG_NORMAL, "fail");
            replyToQuery("SETTINGNG");
        }
    }
    else if(phToolsMatch(message, "ccd.*"))
    {
        if(phToolsMatch(message, "ccd 1\\?"))
        {
            replyToQuery("CCD 1,1234567800000000;2,2345678900000000;3,1345678900000000;4,1245678900000000;5,DATANG;6,DATANG;7,DATANG;8,DATANG;");
        }
        else if(phToolsMatch(message, "ccd 2\\?"))
        {
            replyToQuery("CCD 9,1234567800000000;10,2345678900000000;11,1345678900000000;12,1245678900000000;13,DATANG;14,DATANG;15,DATANG;16,DATANG;");
        }
        else if(phToolsMatch(message, "ccd 3\\?"))
        {
            replyToQuery("CCD 17,1234567800000000;18,2345678900000000;19,1345678900000000;20,1245678900000000;21,DATANG;22,DATANG;23,DATANG;24,DATANG;");
        }
        else if(phToolsMatch(message, "ccd 4\\?"))
        {
            replyToQuery("CCD 25,1234567800000000;26,2345678900000000;27,1345678900000000;28,1245678900000000;29,DATANG;30,DATANG;31,DATANG;32,DATANG;");
        }
    }
    else
    {  
        pAdvantestViewController->setGpibStatus(ViewController::notUnderstood);
    }
}

/*Begin CR92686, Adam Huang, 1 Feb 2015*/
void AdvantestHandlerController::serviceDvid(const char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);

    sprintf(messageBuffer, "DVID 001");

    replyToQuery(messageBuffer);

    return;
}

void AdvantestHandlerController::serviceTloop(const char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);

    sprintf(messageBuffer, "TLOOP 3");

    replyToQuery(messageBuffer);

    return;
}

void AdvantestHandlerController::serviceTsset(const char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);

    sprintf(messageBuffer, "TSSET 1,-40,3,2,110,5,3,25,4");

    replyToQuery(messageBuffer);

    return;
}
/*End*/
void AdvantestHandlerController::serviceOvertemp(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    char szOvertempSites[] = "1,4";

    sprintf(messageBuffer, "OVERTEMP %s", szOvertempSites);

    phLogSimMessage(logId, LOG_DEBUG, "sending \"%s\"",messageBuffer);

    replyToQuery(messageBuffer);
 
    return;
}

void AdvantestHandlerController::serviceStatus(const char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    sprintf(messageBuffer, "FR%d", int(!pAdvantestStatus->getStatusBit(AdvantestStatus::stopped)) );

    pAdvantestStatus->logStatus();

    replyToQuery(messageBuffer);
 
    return;
}

void AdvantestHandlerController::serviceFullsites(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    unsigned int devicesReady;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    
    devicesReady=pHandler->getSitePopulationAsLong(); 

    sprintf(messageBuffer, "FULLSITES %08x", devicesReady );

    replyToQuery(messageBuffer);

    return;
}

void AdvantestHandlerController::getBinNum(int site, int bin, const char *binFirstbit, const char *binSecondBit)
{
    if (site < pHandler->getNumOfSites())
    {
        bin = hextoInt(binFirstbit, binSecondBit);

        if ( bin != 0xA )
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
}

int AdvantestHandlerController::hextoInt(const char *binFirstbit, const char *binSecondBit)
{
    if (binSecondBit == NULL)
    {
        return convertHexadecimalDigitIntoInt(*binFirstbit) - 1 + 1;
    }
    else
    {
        return convertHexadecimalDigitIntoInt(*binFirstbit)*16 + convertHexadecimalDigitIntoInt(*binSecondBit) - 1 + 1;
    }
}

void AdvantestHandlerController::serviceBinon(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    //
    // get binon information
    // expected format
    // 16 categories 
    // BINON: xxxxxxxx,xxxxxxxx,..,xxxxxxxx;
    // 32 categories
    // BINON: x,x,x,x,x,x,x,x,x,x,x,..,x,x,x,
    //
    int bin = 0;
    const char *posMsg=msg;

    /* find first colon */
    posMsg=strchr(posMsg,':') + 1;

    int categories = pHandler->getCategories();
    int site = getBinDataNum();
    if (categories == 32)
    {
        char *message = strtok((char*)posMsg, ",");
        while ( message && site >= 0 )
        {
            if (message)
            {
                int indexOfendBysem = string(message).find(";", 0);
                if ( indexOfendBysem > 0)
                {
                    message = strtok(message, ";");
                }

                if (message)
                {
                    if (strlen(message) == 1)
                    {
                        if (isxdigit(*message))
                        {
                            getBinNum(site, bin, message, 0);
                        }
                    }
                    else if (strlen(message) == 2)
                    {
                        string firstbit = string(message).substr(0, 1);
                        string secondbit = string(message).substr(1, 1);
                        const char *binFirstbit = firstbit.c_str();
                        const char *binSecbit = secondbit.c_str();
                        if (isxdigit(*binFirstbit) && isxdigit(*binSecbit))
                        {
                            getBinNum(site, bin, binFirstbit, binSecbit);
                        }
                    }
                }
            }
            message = strtok(NULL,",");
            --site;
        }
    }
    else if (categories == 16)
    {
        while ( posMsg && *posMsg && site>=0 )
        {
            posMsg=findHexadecimalDigit(posMsg);
            if ( isxdigit(*posMsg) )
            {
                getBinNum(site, bin, posMsg, 0);
                --site;
                ++posMsg;
            }
        }
    }

    if ( site>0 )
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

void AdvantestHandlerController::serviceBinoff(const char *msg)
{
    //does not request the handler to echo back the results.
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    return;
}

void AdvantestHandlerController::serviceEcho()
{
    phLogSimMessage(logId, LOG_VERBOSE, "serviceEcho()");
    char messageBuffer[MAX_MSG_LENGTH];
    char tmp[1024];

    int bin = 0;
    int site = 0;
    int binDataNum = getBinDataNum();
    int categories = pHandler->getCategories();

    //
    // echo bin information
    // format
    // 16 categories
    // BINON: xxxxxxxx,xxxxxxxx,..,xxxxxxxx;
    // 32 categories
    // BINON: x,x,x,x,x,x,x,x,x,x,x,..,x,x,x,
    //

    strcpy(messageBuffer,"ECHO:"); 
  
    for ( site=binDataNum; site>=0; --site)
    {
        if ( site < pHandler->getNumOfSites() )
        {
            bin=pHandler->getBinData(site); 

            if (bin == siteNotYetAssignedBinNumber)
            {
                switch ( model )
                {
                  case GS1:
                  case M45:
                  case M48:
                  case DLT:
                  case Yushan:
                    bin =0xA;
                    break;
                }
            }
        }
        else
        {
            switch ( model )
            {
              case GS1:
              case M45:
              case M48:
              case DLT:
              case Yushan:
                bin=0xA;
                break;
            }
        }
        sprintf(tmp, "%x", bin);
        strcat(messageBuffer,tmp); 

        if ( site == 0 )
        {
            strcat(messageBuffer,";");
        }
        else 
        {
            if (categories == 16)
            { 
                if ( site % 8 == 0 )
                {
                    strcat(messageBuffer,",");
                }
            }
            else if (categories == 32)
            {
                strcat(messageBuffer,",");
            }
        }
    } 
    sendMessage(messageBuffer);

    return;
}

void AdvantestHandlerController::serviceEchoOk(const char *msg)
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

void AdvantestHandlerController::serviceEchoNg(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    pHandler->binDataVerified();

    pAdvantestStatus->setStatusBit(this, AdvantestStatus::alarm, true);

    return;
}


void AdvantestHandlerController::serviceSrqmask(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /*
     * Mask byte for SRQ generation.
     */
    if ( sscanf(msg,"srqmask %x",&srqMask) != 1 )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"srqmask n\"",msg);
    }

    return;
}

void AdvantestHandlerController::serviceDl(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    
    if (sscanf(msg, "dl %d",&dl) != 1)
    { 
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"dl n\"",msg);

    }
   
    return;
}

void AdvantestHandlerController::serviceSrqKind(const char *msg)
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

void AdvantestHandlerController::serviceSrq(const char *msg)
{

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    if (sscanf(msg, "srq %d",&srq) != 1)
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"srq n\"",msg);

    }

    return;
}

void AdvantestHandlerController::serviceHandlerNum(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    char handlerNum[]="123456";

    sprintf(messageBuffer, "HANDLERNUM  %s", handlerNum);

    replyToQuery(messageBuffer);

    return;
}

void AdvantestHandlerController::serviceHdlStatus(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    
    char hdlStatus[]="abec";

    sprintf(messageBuffer, "HDLSTATUS %s", hdlStatus);

    replyToQuery(messageBuffer);

    return;
}

void AdvantestHandlerController::serviceMeasTemp(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    char measTemp[]="56.13";

    sprintf(messageBuffer, "Meastemp %s", measTemp);

    replyToQuery(messageBuffer);

    return;

}

void AdvantestHandlerController::serviceSetTemp(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    char temp[]="10.1";

    sprintf(messageBuffer, "Settemp %s", temp);

    replyToQuery(messageBuffer);

    return;
}

void AdvantestHandlerController::serviceVersion(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];   
   
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    char version[]="Rev 1.00 FULLSITEB";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg); 

    sprintf(messageBuffer,"ADVANTEST GS1 %s",version);
   
    replyToQuery(messageBuffer);

    return;
}


/* LDTRAY command reply to enhance DLT */
void AdvantestHandlerController::serviceLdTray(const  char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId,LOG_VERBOSE,"handling \"%s\"",msg);

    int data1=1; /*sim data*/
    int data2=2;
    int data3=3;

    sprintf(messageBuffer,"LDTRAY %d,%d,%d",data1,data2,data3);

    replyToQuery(messageBuffer);

    return;
}

/* LDPOCKET command reply to enhance DLT */
void AdvantestHandlerController::serviceLdPocket(const char *msg)
{
   char messageBuffer[MAX_MSG_LENGTH];

   phLogSimMessage(logId,LOG_VERBOSE,"handling \"%s\"",msg);

   int data1 =1; /*sim data*/
   int data2 =2;
   int data3 =3;

   sprintf(messageBuffer,"LDPOCKET %d,%d,%d",data1,data2,data3);

   replyToQuery(messageBuffer);

   return;
}


void AdvantestHandlerController::serviceStart(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
 
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    AdvantestStatusChanged(AdvantestStatus::stopped,0);

    if (!pHandler->isStopped())
    {
        sprintf(messageBuffer,"settingok");
    }
    else
    {
        sprintf(messageBuffer,"settingng");
    }

    sendMessage(messageBuffer);
 
    return; 
         
}

void AdvantestHandlerController::serviceStop(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    AdvantestStatusChanged(AdvantestStatus::stopped,1);

    if (pHandler->isStopped())
    {
        sprintf(messageBuffer, "settingok");
    }
    else
    {
        sprintf(messageBuffer, "settingng");
    }

    sendMessage(messageBuffer);

    return;
 
}

int AdvantestHandlerController::getBinDataNum()
{
    int binDataNum = pHandler->getNumOfSites();

    if (binDataNum == 32 || binDataNum == 16
        || binDataNum == 4 || binDataNum == 8
        || binDataNum == 1 || binDataNum == 2)
    {
        binDataNum = 31;
    }
    else if (binDataNum == 128)
    {
        binDataNum = 127;
    }
    else if (binDataNum == 256)
    {
        binDataNum = 255;
    }
    else if (binDataNum == 512)
    {
        binDataNum = 511;
    }
    else if (binDataNum == 1024)
    {
        binDataNum = 1023;
    }
    return binDataNum;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
