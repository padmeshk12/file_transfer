/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : MiraehandlerController.C
 * CREATED  : 10 Jan 2005
 *
 * CONTENTS : Mirae handler controller implementation
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Ken Ward, BitsoftSystems, Mirae revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 25 Jan 2001, Chris Joyce , created
 *            Feb 2008, Jiawei Lin, CR38119
 *                  enhance to support M660 model and additional
 *                  gpib commands
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

#include "MiraehandlerController.h"
#include "handlerController.h"
#include "handler.h"
#include "Miraestatus.h"
#include "MiraeviewController.h"
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

/*--- MiraeHandlerController ---------------------------------------------------*/


MiraeHandlerController::MiraeHandlerController(MiraeHandlerControllerSetup *setup,
                                         ViewControllerSetup    *VCsetup) :
    HandlerController(setup),
    model(setup->model),
    verifyTest(setup->verifyTest),
    multipleSrqs(setup->multipleSrqs)
{
    try {

        pMiraestatus = new MiraeStatus(logId);
        pMiraestatus->registerObserver(this);

        pMiraestatus->setStatusBit(this, MiraeStatus::temp_stable, true);

        pMiraeViewController = new MiraeViewController(logId,this,pMiraestatus,VCsetup);
        pMiraeViewController->showView();
        pViewController=pMiraeViewController;

        if ( setup->recordFile )
        {
            pEventRecorder = new EventRecorder(logId,
                                               this,
                                               pMiraestatus,
                                               pMiraeViewController,
                                               setup->recordFile);
        }

        if ( setup->playbackFile )
        {
            pEventPlayback = new EventPlayback(logId,
                                               pMiraestatus,
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

    pMiraeViewController->setGpibStatus(ViewController::okay);
}

MiraeHandlerController::~MiraeHandlerController()
{
}

unsigned char MiraeHandlerController::getSrqByte()
{
    unsigned char srqByte = 0;

    pMiraestatus->logStatus();

    switch ( model )
    {
    case MR5800:
    case M660:
    case M330:
        phLogSimMessage(logId, LOG_VERBOSE,
                        "\nEntering MiraeHandlerController::getSrqByte(): \n"
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
                        pMiraestatus->getStatusBit(MiraeStatus::controlling_remote),
                        pMiraestatus->getStatusBit(MiraeStatus::input_empty),
                        pMiraestatus->getStatusBit(MiraeStatus::handler_empty),
                        pMiraestatus->getStatusBit(MiraeStatus::wait_for_soak),
                        pMiraestatus->getStatusBit(MiraeStatus::out_of_temp),
                        pMiraestatus->getStatusBit(MiraeStatus::temp_stable),
                        pMiraestatus->getStatusBit(MiraeStatus::temp_alarm),
                        pMiraestatus->getStatusBit(MiraeStatus::low_alarm),
                        pMiraestatus->getStatusBit(MiraeStatus::high_alarm),
                        pMiraestatus->getStatusBit(MiraeStatus::stopped),
                        pMiraestatus->getStatusBit(MiraeStatus::waiting_tester_response)
                       );
        if (((pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::controlling_remote)      == 0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::input_empty)             == 0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::handler_empty)           == 0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::wait_for_soak)           == 0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::out_of_temp)             == 0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::temp_stable)             == 1) &&
            (pMiraestatus->getStatusBit(MiraeStatus::temp_alarm)              == 0) &&
// don't care?   (pMiraestatus->getStatusBit(MiraeStatus::low_alarm) == 0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::high_alarm)              == 0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::stopped)                 == 0)
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::waiting_tester_response) == 0) &&
            )
        {
            srqByte = 0x47;
        }
        else if (
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::controlling_remote)      == 0) &&
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::input_empty)             == 0) &&
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::handler_empty)           == 0) &&
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::wait_for_soak)           == 0) &&
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::out_of_temp)             == 0) &&
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::temp_stable)             == 1) &&
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::temp_alarm)              == 0) &&
            (pMiraestatus->getStatusBit(MiraeStatus::low_alarm)               == 1) )
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::high_alarm)              == 0) &&
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::stopped)                 == 1) &&
// don't care?            (pMiraestatus->getStatusBit(MiraeStatus::waiting_tester_response) == 0))
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

void MiraeHandlerController::switchOn()
{
    bool SendLotStart = true; 
    while (!done)
    {
        if ( pEventPlayback )
        {
            pEventPlayback->checkEvent( getReceiveMessageCount() );
        }
        
        phLogSimMessage(logId, LOG_VERBOSE, "MiraeHandlerController::switchOn() calling checkEvent(0)");
        pMiraeViewController->checkEvent(0);
        
        phLogSimMessage(logId, LOG_VERBOSE, "MiraeHandlerController::switchOn() calling tryReceiveMsg(1000)");
        tryReceiveMsg(1);
       
        if ( pHandler->sendTestStartSignal() )
        {
            phLogSimMessage(logId, LOG_VERBOSE, "MiraeHandlerController sendTestStartSignal == TRUE");
            
            pMiraestatus->setStatusBit(this, MiraeStatus::stopped, false);

            unsigned char srqSequence=getSrqByte();
            if(SendLotStart)
            {
                sendSRQ(0x46); /* lot start SRQ */
                SendLotStart = false;
                sleep(3);
            }
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

        if ( pHandler->allTestsComplete() && pMiraestatus->getStatusBit(MiraeStatus::handler_empty)==false )
        {
            pHandler->logHandlerStatistics();

            pMiraestatus->setStatusBit(this, MiraeStatus::stopped, true);
            pMiraestatus->setStatusBit(this, MiraeStatus::handler_empty, true);
            sendSRQ(0x48);  /* send lot done */
        }
    }
    phLogSimMessage(logId, LOG_DEBUG, "MiraeHandlerController::switchOn() Exiting");
}

void MiraeHandlerController::MiraeStatusChanged(MiraeStatus::eMiraeStatus ebit, bool b)
{
    switch ( ebit )
    {
        case MiraeStatus::stopped:
            phLogSimMessage(logId, LOG_VERBOSE, "MiraeHandlerController stop detected new value %s",
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
        case MiraeStatus::handler_empty:
            phLogSimMessage(logId, LOG_VERBOSE, "MiraeHandlerController empty detected new value %s",
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

MiraeHandlerController::eMiraeModel MiraeHandlerController::getModelType() const
{
    return model;
}

/*--- MiraeHandlerController protected and public functions --------------------*/


void MiraeHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_DEBUG, "Received message \"%s\"",message);

    setStrlower(message);

    if (phToolsMatch(message, "fullsites\\?"))
        serviceFullsites(message);
    else if (phToolsMatch(message, "fr\\?"))
        serviceStatus(message);
    else if (phToolsMatch(message, "binon .*") || phToolsMatch(message, "binon:.*"))
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
    else if (phToolsMatch(message, "sitesel .*"))
        serviceSitesel(message);
    else if (phToolsMatch(message, "contactsel .*"))
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

    else if (phToolsMatch(message, "lotclear"))
        serviceLotClear(message);
    else if (phToolsMatch(message, "accclear"))
        serviceAccClear(message);

    else if (phToolsMatch(message, "version\\?"))
        serviceVersionQuery(message);
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
    else if (phToolsMatch(message, "tray_id\\?"))
        serviceTrayIDQuery(message);
    else if (phToolsMatch(message, "loterrcode\\?"))
        serviceLotErrCodeQuery(message);
    else if (phToolsMatch(message, "lotalmcode\\?"))
        serviceLotAlmCodeQuery(message);
    else if (phToolsMatch(message, "lotdata\\?"))
        serviceLotDataQuery(message);
    else if (phToolsMatch(message, "lottotal2\\?"))
        serviceLotTotal2Query(message);
    else if (phToolsMatch(message, "accsortdvc\\?"))
        serviceAccSortDvcQuery(message);
    else if (phToolsMatch(message, "accjamcode\\?"))
        serviceAccJamCodeQuery(message);
    else if (phToolsMatch(message, "accerrcode\\?"))
        serviceAccErrCodeQuery(message);
    else if (phToolsMatch(message, "accdata\\?"))
        serviceAccDataQuery(message);
    else if (phToolsMatch(message, "hdlstatus\\?"))
        serviceHdlStatusQuery(message);
    else if (phToolsMatch(message, "tempmode\\?"))
        serviceTempModeQuery(message);
    else
    {
        pMiraeViewController->setGpibStatus(ViewController::notUnderstood);
    }
}


void MiraeHandlerController::serviceStatus(const char* msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    bool condition;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    condition = ((pMiraestatus->getStatusBit(MiraeStatus::stopped)) ||
                 (pMiraestatus->getStatusBit(MiraeStatus::low_alarm)) ||
                 (pMiraestatus->getStatusBit(MiraeStatus::high_alarm)) ||
                 (pMiraestatus->getStatusBit(MiraeStatus::temp_alarm)));
    sprintf(messageBuffer, "FR %d", int(!condition));

    pMiraestatus->logStatus();

    replyToQuery(messageBuffer);
 
    return;
}

void MiraeHandlerController::serviceFullsites(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    unsigned long devicesReady;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    devicesReady=pHandler->getSitePopulationAsLong();

    //sprintf(messageBuffer, "FULLSITES %.8lx", devicesReady );
    if ( model == MiraeHandlerController::MR5800 ) {
      sprintf(messageBuffer, "FULLSITES FFFFFFFF" );
    } else if ( model == MiraeHandlerController::M660 ) {
      sprintf(messageBuffer, "FULLSITES FFFF" );
    } else if ( model == MiraeHandlerController::M330 ) {
      sprintf(messageBuffer, "FULLSITES FFFFFFFF" );
    }

    replyToQuery(messageBuffer);

    return;
}

void MiraeHandlerController::serviceBinon(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    //
    // get binon information
    // expected format
    // BINON xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx; (MR5800)
    // BINON xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx; (M660)
    // BINON:xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx; (M330)
    //
    int site = 0;
    int bin = 0;

    char buffer[128] = "";

    strcpy(buffer, msg);


    if ( model == MiraeHandlerController::MR5800 ) {
      site = 31;
    } else if ( model == MiraeHandlerController::M660 ) {
      site = 15;
    } else if ( model == MiraeHandlerController::M330 ) {
      site = 31;
    }


    phLogSimMessage(logId, LOG_DEBUG, "entering serviceBinon, message = ->%s<-\n",msg);

    char *p = buffer+5;    //ingore the "BINON"

    while ( *p != '\0'  ) {
        if ( ! isxdigit(*p) ) {
          p++;
          continue;
        }

        //now, the char is 0-9, A-Z; convert it to integer number
        if ( *p <='9' && *p >= '0' ) {
          bin = *p - 48;
        } else if ( *p <= 'F' && *p >= 'A' ) {
          bin = *p - 55;
        }


        //int bin = convertHexadecimalDigitIntoInt(*p);
        phLogSimMessage(logId, LOG_DEBUG, "In serviceBinon, site = %d, bin = %x\n",site+1, bin);

        //not otherwise bin
        if ((*p != 'A') && (*p != 'a')) {
           if ( verifyTest ) {
              pHandler->setBinData(site,bin);
           } else {
             pHandler->sendDeviceToBin(site,bin);
           }
        }

        site--;
        p++;
    }

    if ( verifyTest ) {
      serviceEcho();
    }


#if 0
    const char *posMsg=msg;
    /* find first space */
    if( model == MR5800 )
    {
      posMsg=strchr(posMsg,' ');
    }
    else if(model == M330)
    {
      posMsg=strchr(posMsg,':');
    }

    printf("the posMsg is %s\n", posMsg);

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
            serviceEcho();
        }
    }
#endif

    phLogSimMessage(logId, LOG_DEBUG, "exiting serviceBinon, message = ->%s<-\n",msg);
}


void MiraeHandlerController::serviceEcho()
{
    char messageBuffer[MAX_MSG_LENGTH];
    char tmp[64];
    int site = 0;
    int bin = 0;

    phLogSimMessage(logId, LOG_VERBOSE, "serviceEcho()");

    //
    // echo bin information
    // format
    // ECHO xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx;(MR5800)
    // ECHO xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx;(M660)
		// ECHO:xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx;(M330)
    //
		if(model == MR5800 || model == M660)
		{
      strcpy(messageBuffer,"ECHO ");
	  }
		else if(model == M330)
		{
		  strcpy(messageBuffer,"ECHO:");
		}

     if ( model == MiraeHandlerController::MR5800 ) {
      site = 31;
    } else if ( model == MiraeHandlerController::M660 ) {
      site = 15;
    } else if ( model == MiraeHandlerController::M330 ) {
      site = 31;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "  in serviceEcho(), numSites = %d", pHandler->getNumOfSites());

    for ( ; site>=0  ; --site)
    {
        if ( site < pHandler->getNumOfSites() )
        {
            bin=pHandler->getBinData(site);
            phLogSimMessage(logId, LOG_VERBOSE, "  in serviceEcho(), bin = %d", bin);

            if (bin == siteNotYetAssignedBinNumber)
            {
                switch ( model )
                {
                case MR5800:
                case M660:
                case M330:
                    bin=0xa;
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
            case MR5800:
            case M660:
            case M330:
                bin=0;
                break;
            default:
                break;
            }
        }

        if (bin > 15) bin = 0; // Make sure will fit in one hex digit

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

void MiraeHandlerController::serviceEchoOk(const char *msg)
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

void MiraeHandlerController::serviceEchoNg(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    pHandler->binDataVerified();

    if ( pHandler->isStopped() )
    {
        pMiraestatus->setStatusBit(this, MiraeStatus::stopped, true);
    }
    return;
}


void MiraeHandlerController::serviceSrqmask(const char *msg)
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


void MiraeHandlerController::serviceTestset(const char *msg)
{
    
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??
    
    // sprintf(messageBuffer, "",  );

    sendMessage("done");

    return;
}


void MiraeHandlerController::serviceRunmode(const char *msg)
{
    
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("done");

    return;
}



void MiraeHandlerController::serviceSettemp(const char *msg)
{
   
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("done");

    return;
}



void MiraeHandlerController::serviceSetband(const char *msg)
{
  
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("done");

    return;
}



void MiraeHandlerController::serviceSetsoak(const char *msg)
{
    
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("done");

    return;
}



void MiraeHandlerController::serviceTempctrl(const char *msg)
{
    
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("done");

    return;
}



void MiraeHandlerController::serviceSitesel(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";
    int retVal = 0;
    char command[MAX_MSG_LENGTH] = "";
    long mask = 0;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    
    retVal = sscanf(msg, "%s %lx", command, &mask);
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



void MiraeHandlerController::serviceSetname(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "SettingOK");

    sendMessage(messageBuffer);

    return;
}



void MiraeHandlerController::serviceSitemap(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "%s", msg);

    sendMessage(messageBuffer);

    return;
}



void MiraeHandlerController::serviceSetbin(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "SettingOK");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceVersionQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "Mirae Test Simulator");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceNameQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "Name Mirae1234");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceLoader(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    // ??

    sprintf(messageBuffer, "settingOK");

    sendMessage("settingOK");

    return;
}



void MiraeHandlerController::serviceStart(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    MiraeStatusChanged(MiraeStatus::stopped, 0);

    sprintf(messageBuffer, "settingOK");

    sendMessage(messageBuffer);

    return;
}



void MiraeHandlerController::serviceStop(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    MiraeStatusChanged(MiraeStatus::stopped, 0);

    sprintf(messageBuffer, "settingOK");

    sendMessage(messageBuffer);

    return;
}



void MiraeHandlerController::servicePlunger(const char *msg)
{
   
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;
    // ??

    // sprintf(messageBuffer, "",  );

    sendMessage("settingOK");

    return;
}

//MR660, CR38119
void MiraeHandlerController::serviceLotClear(const char *msg)
{
   
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);


    sendMessage("settingOK");

    return;
}

//MR660, CR38119
void MiraeHandlerController::serviceAccClear(const char *msg)
{
   
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    sendMessage("settingOK");

    return;
}


void MiraeHandlerController::serviceTestsetQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "TestSet 8,4,A");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceSetTempQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "30.1");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceMeasTempQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "33.3");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceStatusQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "0000000000000000";

    phLogSimMessage(logId, LOG_DEBUG, "handling \"%s\"",msg);

    phLogSimMessage(logId, LOG_DEBUG,
                    "\nEntering MiraeHandlerController::serviceStatusQuery(): \n"
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
                    pMiraestatus->getStatusBit(MiraeStatus::controlling_remote),
                    pMiraestatus->getStatusBit(MiraeStatus::input_empty),
                    pMiraestatus->getStatusBit(MiraeStatus::handler_empty),
                    pMiraestatus->getStatusBit(MiraeStatus::wait_for_soak),
                    pMiraestatus->getStatusBit(MiraeStatus::out_of_temp),
                    pMiraestatus->getStatusBit(MiraeStatus::temp_stable),
                    pMiraestatus->getStatusBit(MiraeStatus::temp_alarm),
                    pMiraestatus->getStatusBit(MiraeStatus::low_alarm),
                    pMiraestatus->getStatusBit(MiraeStatus::high_alarm),
                    pMiraestatus->getStatusBit(MiraeStatus::stopped),
                    pMiraestatus->getStatusBit(MiraeStatus::waiting_tester_response)
                   );
    // prepare results;
    messageBuffer[3] = 
    pMiraestatus->getStatusBit(MiraeStatus::controlling_remote) ? '1' : '0';
    messageBuffer[6] =
    pMiraestatus->getStatusBit(MiraeStatus::input_empty) ? '1' : '0';
    messageBuffer[7] =
    pMiraestatus->getStatusBit(MiraeStatus::handler_empty) ? '1' : '0';
    messageBuffer[8] = 
    pMiraestatus->getStatusBit(MiraeStatus::wait_for_soak) ? '1' : '0';
    messageBuffer[9] = 
    pMiraestatus->getStatusBit(MiraeStatus::out_of_temp) ? '1' : '0';
    messageBuffer[10] = 
    pMiraestatus->getStatusBit(MiraeStatus::temp_stable) ? '1' : '0';
    messageBuffer[11] = 
    pMiraestatus->getStatusBit(MiraeStatus::temp_alarm) ? '1' : '0';
    messageBuffer[12] = 
    pMiraestatus->getStatusBit(MiraeStatus::low_alarm) ? '1' : '0';
    messageBuffer[13] = 
    pMiraestatus->getStatusBit(MiraeStatus::high_alarm) ? '1' : '0';
    messageBuffer[14] = 
    pMiraestatus->getStatusBit(MiraeStatus::stopped) ? '1' : '0';
    messageBuffer[15] = 
    pMiraestatus->getStatusBit(MiraeStatus::waiting_tester_response) ? '1' : '0';

    replyToQuery(messageBuffer);

    phLogSimMessage(logId, LOG_DEBUG,
                    "\nExiting MiraeHandlerController::serviceStatusQuery(), reply: ->%s<-, length = %d\n",
                    messageBuffer, strlen(messageBuffer));

    return;
    }



void MiraeHandlerController::serviceJamQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "Jam 1");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceJamcodeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "JamCode 010102 17");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceJamqueQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "JamQue 010102, 030205, 020301;");

    replyToQuery(messageBuffer);

    return;
}

void MiraeHandlerController::serviceJamCount(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "JamCount 3");

    replyToQuery(messageBuffer);

    return;
}


void MiraeHandlerController::serviceSetlampQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "SetLamp 0,2,1");

    replyToQuery(messageBuffer);

    return;
}


//For M660, CR38119
void MiraeHandlerController::serviceTrayIDQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "TRAY_ID 8");

    replyToQuery(messageBuffer);

    return;
}


void MiraeHandlerController::serviceLotErrCodeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "ERROR4:1-030002,2-04333,3-33434,4-00100");

    replyToQuery(messageBuffer);

    return;
}

void MiraeHandlerController::serviceLotAlmCodeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "LOTALMCODE 1");

    replyToQuery(messageBuffer);

    return;
}


void MiraeHandlerController::serviceLotDataQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "times 3340:82:2623");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceLotTotal2Query(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "TOTAL_3024 ; 170 ; 3024 ; 2854 ; 170 ; 0 , 2433 , 267 , 170 , 154 , 0 , 0 , 0, 0 , 0 , 0");

    replyToQuery(messageBuffer);

    return;
}



void MiraeHandlerController::serviceAccSortDvcQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "TOTAL 62379 ; 18732 ; 49643 ; 30911 ; 18686 ; 0 , 7720 , 5252 , 18469 , 809 , 14820 , 0 , 57 , 2470 , 0 , 0");

    replyToQuery(messageBuffer);

    return;
}


void MiraeHandlerController::serviceAccJamCodeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "JAM0");

    replyToQuery(messageBuffer);

    return;
}


void MiraeHandlerController::serviceAccErrCodeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "ERROR4:1-03000,2-04000,3-05000,4-06000");

    replyToQuery(messageBuffer);

    return;
}


void MiraeHandlerController::serviceAccDataQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "times 3340:82:2263");

    replyToQuery(messageBuffer);

    return;
}

void MiraeHandlerController::serviceHdlStatusQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "HdlStatus 1");

    replyToQuery(messageBuffer);

    return;
}


void MiraeHandlerController::serviceTempModeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    // prepare results;

    sprintf(messageBuffer, "TempMode 1");

    replyToQuery(messageBuffer);

    return;
}



/*****************************************************************************
 * End of file
 *****************************************************************************/
