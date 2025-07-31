  /******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : DeltaHController.C
 * CREATED  : 23 Feb 2001
 *
 * CONTENTS : Delta handler controller implementation
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Xiaofei Feng, R&D Shanghai, CR25072
 *            Jiawei Lin, R&D Shanghai, CR27090&CR27346
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 23 Feb 2001, Chris Joyce , created
 *            Dec 2005, Xiaofei Feng, CR25072
 *               Modified DeltaHandlerController:service_testresults() to fix 
 *               the segmentation fault.
 *
 *            Nov 2005, Jiawei Lin, CR27090&CR27346
 *              support the query of commands for temperature and "sm?"
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

#include <cstring>

/*--- module includes -------------------------------------------------------*/


#if __GNUC__ >= 3
#include <iostream>
#endif

#include "DeltaHController.h"
#include "handler.h"
#include "simHelper.h"
#include "status.h"


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


/*****************************************************************************
 *
 * Delta Handler Controller
 *
 * Authors: Chris Joyce
 *
 *****************************************************************************/

/*--- DeltaHandlerController ------------------------------------------------*/

/*--- DeltaHandlerController constructor ------------------------------------*/

DeltaHandlerController::DeltaHandlerController(DeltaHandlerControllerSetup *setup) :
HandlerController(setup)
{
    releasepartsWhenReady=setup->releasepartsWhenReady;
    softwareVersion="3.0";
    handlerNumber="H-XYZ-001";
    commandReply=false;
    commandReplyMessage="OK";
    systemMode=1;
    emulationMode=1;
    errorBit=0;
    dl = 0;
    retestTime = setup->retestTime;

    try {
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_retestOrtestagain,              false,  0, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_outputFull,                     false,  1, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_handlerEmpty,                   false,  2, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_motorDiagEnabled,               false,  3, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_dymContactorCalEnabled,         false,  4, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_inputInhibited,                 false,  5, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_sortInhibited,                  false,  6, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_outOfGuardBands,                false,  7, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_stopped,                        true,   9, "Stop") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_jammed,                         false,  8, "Jam") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_partsSoaking,                   false, 10, "") );
        pHstatus->setHandlerStatusList(
            new HandlerStatusBit(e_inputOrSortInhibitedOrDoorOpen, false, 11, "Door Open") );
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
        throw;
    }
}

DeltaHandlerController::~DeltaHandlerController()
{
    phLogSimMessage(logId, LOG_VERBOSE, "~DeltaHandlerController");
}


/*--- DeltaHandlerController public functions -------------------------------*/

void DeltaHandlerController::switchOn(deltaModel model)
{
    createView();

    static int iRetestFlag = retestTime;    //retest flag, judge the srq kind

    while (!done)
    {
        HandlerController::switchOn(model);

        if ( pHandler->sendTestStartSignal() )
        {
            pHstatus->setStatusBit(this, e_stopped, false);

            if (retestTime == -1)
            {

              //normal model
              unsigned char srqSequence=getSrqByte();

                           // bit 6 SRQ asserted
              srqSequence|= (1<<6);

              if ( multipleSrqs )
              {
                  if ( 0x01 & srqSequence )
                  {
                      sendSRQ(0xF1 & srqSequence);
                  }
                  if ( 0x02 & srqSequence )
                  {
                      sendSRQ(0xF2 & srqSequence);
                  }
                  if ( 0x04 & srqSequence )
                  {
                      sendSRQ(0xF4 & srqSequence);
                  }
                  if ( 0x08 & srqSequence )
                  {
                      sendSRQ(0xF8 & srqSequence);
                  }
              }
              else
              {
                  if(model == Eclipse)
                    sendSRQ(0x41);
                  else
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

                    if(model == Eclipse) {
                      sendSRQ(0xc0);
                    } else {
                      sendSRQ(specialSrq);
                    }
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
                            sendSRQ(0xF1 & srqSequence);
                        }
                        if ( 0x02 & srqSequence )
                        {
                            sendSRQ(0xF2 & srqSequence);
                        }
                        if ( 0x04 & srqSequence )
                        {
                            sendSRQ(0xF4 & srqSequence);
                        }
                        if ( 0x08 & srqSequence )
                        {
                            sendSRQ(0xF8 & srqSequence);
                        }
                    }
                    else
                    {
                      if(model == Eclipse) {
                        sendSRQ(0xc0);
                      } else {
                        sendSRQ(srqSequence);
                      }
                    }
                }
            }
        }

        if ( pHandler->allTestsComplete() && pHstatus->getStatusBit(e_handlerEmpty)==false )
        {
          pHandler->logHandlerStatistics();
          retestTime--;                                       //reduce time of retest to ensure will send 0x80 next time
          operatorPause=false;

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
              pHstatus->setStatusBit(this, e_handlerEmpty, true);
              pHstatus->setStatusBit(this, e_stopped, true);
          }
          else
          {
              //retest model end
              unsigned char specialSrq=0x80;
              specialSrq |= (1<<6);
              sendSRQ(specialSrq);
              pHstatus->setStatusBit(this, e_handlerEmpty, true);
              pHstatus->setStatusBit(this, e_stopped, true);
          }
      }
    }

    if ( pHstatus->getStatusBit(e_handlerEmpty)==false )
    {
        pHandler->logHandlerStatistics();
    }
}

void DeltaHandlerController::statusChanged(const HandlerStatusBit &sb)
{
    phLogSimMessage(logId, LOG_VERBOSE, 
                    "DeltaHandlerController::statusChanged \"%s\" detected new value %s",
                    sb.getName(),
                    (sb.getBit()) ? "True" : "False");
  
    switch ( sb.getId() )
    {
      case e_stopped:
        if ( sb.getBit() )
        {
            operatorPause=true;
            pHandler->stop();
        }
        else
        {
            if ( pHstatus->getStatusBit(e_handlerEmpty) )
            {
                pHandler->reset();
                pHstatus->setStatusBit(this, e_handlerEmpty, false);
            }
            if ( getHandlerIsReady() )
            {
                phLogSimMessage(logId, LOG_DEBUG, "start detected");
                pHandler->start();
            }
        }
        break;
      case e_jammed:
        if ( sb.getBit() )
        {
            operatorPause=false;
            pHstatus->setStatusBit(this, e_stopped, true);
            pHandler->stop();
        }
        else
        {
            if ( getHandlerIsReady() )
            {
                phLogSimMessage(logId, LOG_DEBUG, "start detected");
                pHstatus->setStatusBit(this, e_stopped, false);
                pHandler->start();
            }
        }
        break;
      case e_inputOrSortInhibitedOrDoorOpen:
        if ( sb.getBit() )
        {
            operatorPause=true;
            pHstatus->setStatusBit(this, e_stopped, true);
            pHandler->stop();
        }
        else
        {
            if ( getHandlerIsReady() )
            {
                phLogSimMessage(logId, LOG_DEBUG, "start detected");
                pHstatus->setStatusBit(this, e_stopped, false);
                pHandler->start();
            }
        }
        break;
    }
    return;
}

/*--- DeltaHandlerController protected functions ----------------------------*/

unsigned char DeltaHandlerController::getSrqByte()
{
    unsigned char srqByte;

    pHstatus->logStatus();

    if ( pHandler->getNumOfSites() > 4 )
    {
        // bit 0 Devices Ready to test
        srqByte = (pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0 ;
    }
    else
    {
        // bit position 0-3 testsite A-D
        srqByte= 0xF & (unsigned char)(pHandler->getSitePopulationAsLong());
    }

    // bit 4 handler is jammed
    srqByte |= (1<<4) * pHstatus->getStatusBit(e_jammed);

    // bit 5 GPIB error

    // bit 7 Handler is in ready state
    srqByte |= (1<<7) * ( getHandlerIsReady() && !(pHstatus->getStatusBit(e_stopped))  );

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte before mask %d",int(srqByte));

                 // Mask out bits not needed
    srqByte&=srqMask;

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte with mask %d",int(srqByte));

    return srqByte;
}

void DeltaHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_NORMAL, "received %3d: \"%s\"",
                    getReceiveMessageCount(),
                    message);

    setStrlower(message);

    implHandleMessage(message);
}

void DeltaHandlerController::implHandleMessage(char* message)
{
    if (phToolsMatch(message, "testpartsready\\?")) 
        service_testpartsready(message);
    else if (phToolsMatch(message, "tpr\\?"))
        service_testpartsready(message);
    else if (phToolsMatch(message, "testparts\\?"))
        service_testpartsready(message);
    else if (phToolsMatch(message, "testresults .*"))
        service_testresults(message);
    else if (phToolsMatch(message, "trs .*"))
        service_testresults(message);
    else if (phToolsMatch(message, "start!?"))
        service_start(message);
    else if (phToolsMatch(message, "identify\\?"))
        service_identify(message);
    else if (phToolsMatch(message, "id\\?"))
        service_identify(message);
    else if (phToolsMatch(message, "which\\?"))
        service_which(message);
    else if (phToolsMatch(message, "whi\\?"))
        service_which(message);
    else if (phToolsMatch(message, "numtestsites\\?"))
        service_numtestsites(message);
    else if (phToolsMatch(message, "nts\\?"))
        service_numtestsites(message);
    else if (phToolsMatch(message, "sitespacing\\?"))
        service_sitespacing(message);
    else if (phToolsMatch(message, "set sitedisable\\?"))
        service_sites_disabled(message);
    else if (phToolsMatch(message, "sd\\?"))
        service_sites_disabled(message);
    else if (phToolsMatch(message, "mask sitedisable\\?"))
        service_sites_disabled(message);
    else if (phToolsMatch(message, ".* killchuck\\?"))
        service_killchuck(message);
    else if (phToolsMatch(message, "verifytest\\?"))
        service_verifytestQuery(message);
    else if (phToolsMatch(message, "verf\\?"))
        service_verifytestQuery(message);
    else if (phToolsMatch(message, "verifytest .*"))
        service_verifytestCommand(message);
    else if (phToolsMatch(message, "verf .*"))
        service_verifytestCommand(message);
    else if (phToolsMatch(message, "cmdreply\\?"))
        service_cmdReplyQuery(message);
    else if (phToolsMatch(message, "cr\\?"))
        service_cmdReplyQuery(message);
    else if (phToolsMatch(message, "cmdreply .*"))
        service_cmdReplyCommand(message);
    else if (phToolsMatch(message, "cr .*"))
        service_cmdReplyCommand(message);
    else if (phToolsMatch(message, "systemmode\\?"))
        service_systemModeQuery(message);
    else if (phToolsMatch(message, "smd\\?"))
        service_systemModeQuery(message);
    else if (phToolsMatch(message, "systemmode .*"))
        service_systemModeCommand(message);
    else if (phToolsMatch(message, "smd .*"))
        service_systemModeCommand(message);
    else if (phToolsMatch(message, "emulationmode\\?"))
        service_emulationModeQuery(message);
    else if (phToolsMatch(message, "emm\\?"))
        service_emulationModeQuery(message);
    else if (phToolsMatch(message, "emulationmode .*"))
        service_emulationModeCommand(message);
    else if (phToolsMatch(message, "emm .*"))
        service_emulationModeCommand(message);
    else if (phToolsMatch(message, "errorclear.*"))
        service_errorclear(message);
    else if (phToolsMatch(message, "ecl.*"))
        service_errorclear(message);
    else if (phToolsMatch(message, "testermode .*"))
        service_general(message);
    else if (phToolsMatch(message, "tmd .*"))
        service_general(message);
    else if (phToolsMatch(message, "setpoint .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* setpoint .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* setpoint\\?") )
      service_temp_command(message);
    else if (phToolsMatch(message, ".* sp .*"))
        service_general(message);
    else if (phToolsMatch(message, "soaktime .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, "skt .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, "soaktime\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "desoaktime .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, "dskt .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, "desoaktime\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "tempcontrol .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, "tempcontrol\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "testtemp .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, "testtemp\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* cool .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* cool\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* heat .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* heat\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* lowerguardband .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* lowerguardband\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "lowguard .*"))
        service_temp_command(message);    
    else if (phToolsMatch(message, ".* lowguard .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* lowguard\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "highguard .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* highguard .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* highguard\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* upperguardband .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* upperguardband\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "categorysource .*"))
        service_temp_command(message);
    else if (phToolsMatch(message, "status\\?"))
        service_status(message);
    else if (phToolsMatch(message, "srqbyte\\?"))
        service_srqbyte(message);
    else if (phToolsMatch(message, "sqb\\?"))
        service_srqbyte(message);
    else if (phToolsMatch(message, "querybins\\?"))
        service_querybins(message);
    else if (phToolsMatch(message, "releaseparts!?"))
        service_releaseparts(message);
    else if (phToolsMatch(message, "stop!?"))
        service_stop(message);
    else if (phToolsMatch(message, "sto!?"))
        service_stop(message);
    else if (phToolsMatch(message, "srqmask!? .*"))
        service_srqmask(message);
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
    else if (phToolsMatch(message, "sqm!? .*"))
        service_srqmask(message);
    else if (phToolsMatch(message, "srqmask\\?"))
        service_srqmaskQuery(message);
    else if (phToolsMatch(message, "srq\\?"))
        service_srqmaskQuery(message);
    else if (phToolsMatch(message, "stripmap\\?"))
        service_stripmap(message);
    else if (phToolsMatch(message, "index\\?"))
        service_index(message);
    else if (phToolsMatch(message, "materialid\\?"))
        service_materialid(message);
    else if (phToolsMatch(message, ".* airtemp\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, ".* masstemp\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "temperature\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "tempstatus\\?"))
        service_temp_command(message);
    else if (phToolsMatch(message, "sm\\?"))
        service_sitemapping(message);
    else if (phToolsMatch(message, "fr\\?"))
    {
        phLogSimMessage(logId, LOG_VERBOSE, "delta handling \"%s\"", message);
        sendMessage("FR1");
    }
    else if (phToolsMatch(message, "fullsites\\?"))
    {
        char messageBuffer[MAX_MSG_LENGTH];
        unsigned long devicesReady;

        phLogSimMessage(logId, LOG_VERBOSE, "delta handling \"%s\"", message);
    
        devicesReady=pHandler->getSitePopulationAsLong(); 

        sprintf(messageBuffer, "FULLSITES %.8lx", devicesReady );

        replyToQuery(messageBuffer);
    }
    else if (phToolsMatch(message, "echook"))
    {    
        serviceEchoOk(message);
    }    
    else if (phToolsMatch(message, "echong"))
    {
        serviceEchoNg(message);
    }
    else if (phToolsMatch(message, "binon:.*"))
    {
        serviceBinon(message);
    }
    else if(phToolsMatch(message,"inputqty.*"))
    {
        phLogSimMessage(logId, LOG_NORMAL, "input qty set success");
        replyToQuery("SETTINGOK");
    }
    else
    {
        HandlerController::implHandleMessage(message);
    }
}

void DeltaHandlerController::serviceEchoOk(const char *msg)
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

void DeltaHandlerController::serviceEchoNg(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    pHstatus->setStatusBit(this, e_stopped, true);

    return;
}

int DeltaHandlerController::getBinDataNum()
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

void DeltaHandlerController::getBinNum(int site, int bin, const char *binFirstbit, const char *binSecondBit)
{
    if (site < pHandler->getNumOfSites())
    {
        bin = hextoInt(binFirstbit, binSecondBit);

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

int DeltaHandlerController::hextoInt(const char *binFirstbit, const char *binSecondBit)
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

void DeltaHandlerController::serviceBinon(const char *msg)
{
    phLogSimMessage(logId, LOG_DEBUG, "handling \"%s\"",msg);

    //
    // get binon information
    // expected format
    // 16 categories 
    // BINON: xxxxxxxx,xxxxxxxx,..,xxxxxxxx;
    // 256 categories
    // BINON: xyxyxyxxyxyxyxyxy,xyxyxyxxyxyxyxyxy,xyxyxyxxyxyxyxyxy,xyxyxyxxyxyxyxyxy;
    //
    int bin = 0;
    const char *posMsg=msg;

    /* find first colon */
    posMsg=strchr(posMsg,':');

    int categories = pHandler->getCategories();
    int site = getBinDataNum();
    if (categories == 256)
    {
        while ( posMsg && *posMsg && site>=0 )
        {
            posMsg=findHexadecimalDigit(posMsg);
            if ( isxdigit(*posMsg) && isxdigit(*(posMsg+1)) )
            {
                getBinNum(site, bin, posMsg, posMsg+1);
                --site;
                posMsg += 2;
            }
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

void DeltaHandlerController::serviceEcho()
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
    // 256 categories
    // BINON: xyxyxyxxyxyxyxyxy,xyxyxyxxyxyxyxyxy,xyxyxyxxyxyxyxyxy,xyxyxyxxyxyxyxyxy;
    //

    strcpy(messageBuffer,"ECHO:"); 
  
    for ( site=binDataNum; site>=0; --site)
    {
        if ( site < pHandler->getNumOfSites() )
        {
            bin=pHandler->getBinData(site); 
        }
        else
        {
            bin=0x0;
        }

        if (categories == 16)
        { 
            sprintf(tmp, "%x", bin);
            strcat(messageBuffer,tmp); 
        }
        else if (categories == 256)
        {
            sprintf(tmp, "%02x", bin);
            strcat(messageBuffer,tmp); 
        }

        if ( site == 0 )
        {
            strcat(messageBuffer,";");
        }
        else 
        {
            if ( site % 8 == 0)
            {
                strcat(messageBuffer,",");
            }
        }
    }
    //phLogSimMessage(logId, LOG_NORMAL, "c = %d, send %s", categories, messageBuffer);
    sendMessage(messageBuffer);

    return;
}

void DeltaHandlerController::service_testpartsready(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    unsigned long devicesReady;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    devicesReady=pHandler->getSitePopulationAsLong();

    sprintf(messageBuffer, "%lu", devicesReady );

    if ( corruptMessage && (replyQueryCount % 11 == 0) )
    {
        phLogSimMessage(logId, LOG_DEBUG, "corrupt testpartsready from %s to %s ",
                        messageBuffer,"corrupted");
        strcpy(messageBuffer, "corrupted");
    }
    replyToQuery(messageBuffer);
}

void DeltaHandlerController::service_testresults(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    checkData();

    //
    // get testresults
    //
    int site=0;
    int bin;
    const char *posMsg=msg;
    char trstmp[4];

    if( msg == NULL )
    {
	return;
    }
   
    if( sscanf(msg,"%3s", trstmp) != 1)
    {
        phLogSimMessage(logId, LOG_VERBOSE,
            "unable to get the string  with sscanf from %s",
            msg);
    }
    if ( sscanf(msg,"testresults %d",&bin) != 1 && 0 != strncmp(trstmp,"trs",3))
    {
        phLogSimMessage(logId, LOG_VERBOSE,
            "unable to interpret testresults \"%s\" with sscanf \n"
            "for site 1 position format expected testresults n",
            msg);
    }
    else
    {
        if(0==strncmp(trstmp,"trs",3))
        {
           if( sscanf(msg,"trs %d",&bin) != 1 )
	   {
		
       		 phLogSimMessage(logId, LOG_VERBOSE,
           		 "unable to interpret trs \"%s\" with sscanf \n"
           		 "for site 1 position format expected trs n",
           		 msg);
	   }
        }
        if ( verifyTest )
            pHandler->setBinData(site,bin);
        else
            pHandler->sendDeviceToBin(site,bin);
    }


    do {
        posMsg=strchr(posMsg,',');

        if( posMsg == NULL )
        {
          break;
        }

        ++site;

        if ( sscanf(posMsg,",%d",&bin) != 1 )
        {
            phLogSimMessage(logId, LOG_VERBOSE,
                "unable to interpret \"%s\" with sscanf \n"
                "for site %d position format expected ,n ",
                posMsg,site+1);
        }
        else
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

        if ( *posMsg )
        {
            ++posMsg;
        }
    } while ( *posMsg );

    return;
}

void DeltaHandlerController::service_start(const char *msg)
{
    phLogSimMessage(logId, LOG_DEBUG, "handling \"%s\"",msg);

    checkData();

    if ( getHandlerIsReady() == true )
    {
        if ( operatorPause == true )
        {
            phLogSimMessage(logId, LOG_DEBUG, "start ignored: start must be initiated by operator ");
        }
        else
        {
            pHstatus->setStatusBit(this, e_stopped, false);
            pHandler->start();
            if ( commandReply )
                sendMessage(commandReplyMessage);
        }
    }
    else
    {
        if ( pHstatus->getStatusBit(e_jammed) )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                    "unable to start handler: Handler is jammed.");
        }
        else if ( pHstatus->getStatusBit(e_inputOrSortInhibitedOrDoorOpen) )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                    "unable to start handler: Door is open. ");
        }
        else
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                    "unable to start handler ");
        }
    }
    return;
}

void DeltaHandlerController::service_numtestsites(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the number of testsites:
     */

    sprintf(messageBuffer, "%d", pHandler->getNumOfSites());
    if ( corruptMessage && (replyQueryCount % 11 == 0) )
    {
        phLogSimMessage(logId, LOG_DEBUG, "corrupt numtestsites %s to %s ",
                        messageBuffer,"corrupted");
        strcpy(messageBuffer, "corrupted");
        ++replyQueryCount;
    }
    sendMessage(messageBuffer);

    return;
}


void DeltaHandlerController::service_sitespacing(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the number of testsites:
     */

    sprintf(messageBuffer, "%d", pHandler->getNumOfSites());
    sendMessage(messageBuffer );

    return;
}


void DeltaHandlerController::service_sites_disabled(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns bit mask of sites which have been disabled.
     */

    sprintf(messageBuffer, "%ld", pHandler->getSitesDisabled() );

    sendMessage(messageBuffer );

    return;
}


void DeltaHandlerController::service_killchuck(const char *msg)
{
    char c;
    char messageBuffer[MAX_MSG_LENGTH];
    int site;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Disable specified Core chucks and corresponding Input nest pockets.
     * Format: [a,b,c,d] killchuck? (disable:0, enable:1)
     */

    /* get category [a-h] from message string */
    if ( sscanf(msg,"%c ",&c) == 1 )
    {
        site=c-'a';
        
        sprintf(messageBuffer, "%d", int(pHandler->getSiteDisabled(site)) );
        sendMessage(messageBuffer );
    }
    else
    {
        phLogSimMessage(logId, LOG_VERBOSE,
            "unable to get bin category \"%s\" with sscanf \n"
            "expected format [a-h] killchuck ? n",
            msg);
    }

    return;  
}

void DeltaHandlerController::service_verifytestCommand(const char *msg)
{
    const char *format;
    int verifyTestInt;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Sets verifytest flag: for bin verification.
     */
    if ( strncmp("verf",msg,4) == 0 )
    {
        format="verf %d";
    }
    else
    {
        format="verifytest %d";
    }

    if ( sscanf(msg,format,&verifyTestInt) == 1 )
    {
        verifyTest = bool(verifyTestInt);
    }
    else
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"%s\" \n",msg,format);
    }

    return;
}

void DeltaHandlerController::service_verifytestQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns verifyTest flag: when set devices are not binning 
     * until bin numbers able to verified.
     */

    sprintf(messageBuffer, "%d", verifyTest);
    sendMessage(messageBuffer );

    return;
}

void DeltaHandlerController::service_cmdReplyCommand(const char *msg)
{
    const char *format;
    int commandReplyInt;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Sets cmdReply flag: whether non-query commands will return
     * an OK response string. Otherwise a space is returned which
     * may or may not be read by the tester.
     */
    if ( strncmp("cr",msg,2) == 0 )
    {
        format="cr %d";
    }
    else
    {
        format="cmdreply %d";
    }

    if ( sscanf(msg,format,&commandReplyInt) == 1 )
    {
        commandReply =  (commandReplyInt) ? true : false;
    }
    else
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"%s\" \n",msg,format);
    }

    return;
}

void DeltaHandlerController::service_cmdReplyQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns cmdReply flag: whether non-query commands will return
     * an OK response string. Otherwise a space is returned which
     * may or may not be read by the tester.
     */

    sprintf(messageBuffer, "%d", int(commandReply));
    sendMessage(messageBuffer );

    return;
}

void DeltaHandlerController::service_systemModeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns systemMode flag: current handler system mode.
     */

    sprintf(messageBuffer, "%d", systemMode);
    sendMessage(messageBuffer );

    return;
}

void DeltaHandlerController::service_systemModeCommand(const char *msg)
{
    const char *format;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Sets systemMode flag: current handler system mode.
     */

    if ( strncmp("smd",msg,3) == 0 )
    {
        format="smd %d";
    }
    else
    {
        format="systemmode %d";
    }

    if ( sscanf(msg,format,&systemMode) != 1 )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"%s\" \n",msg,format);
    }

    return;
}

void DeltaHandlerController::service_emulationModeQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns emulationMode flag: current handler emulation mode.
     */

    sprintf(messageBuffer, "%d", emulationMode);
    sendMessage(messageBuffer );

    return;
}

void DeltaHandlerController::service_emulationModeCommand(const char *msg)
{
    const char *format;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Sets emulationMode flag: current handler emulation mode.
     */

    if ( strncmp("emm",msg,3) == 0 )
    {
        format="emm %d";
    }
    else
    {
        format="emulationmode %d";
    }

    if ( sscanf(msg,format,&emulationMode) != 1 )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"%s\" \n",msg,format);
    }

    return;
}

void DeltaHandlerController::service_errorclear(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Clears the error bit in the serial poll register 
     * and srqbyte status information. 
     * Assume this refers to a GPIB error 
     * GPIB errors not yet implemented.
     */

    errorBit=0;

    return;
}

void DeltaHandlerController::serviceDl(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    if (sscanf(msg, "dl %d",&dl) != 1)
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"dl n\"",msg);

    }

    return;
}

void DeltaHandlerController::serviceSrqKind(const char *msg)
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

void DeltaHandlerController::service_srqmask(const char *msg)
{
    const char *format;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Mask byte for SRQ generation.
     */

    if ( strncmp("srqmask",msg,7) == 0 )
    {
        format="srqmask %d";
    }
    else
    {
        format="srq %d";
    }

    if ( sscanf(msg,format,&srqMask) != 1 )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" with sscanf expected format \"%s\" \n",msg,format);
    }

    if (srqMask == 0)
    {
      phLogSimMessage(logId, PHLOG_TYPE_ERROR,
          "please check message \"%s\" with correct format. \n",msg);
    }

    return;
}

void DeltaHandlerController::service_srqmaskQuery(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns srqmask
     */

    sprintf(messageBuffer, "%d", srqMask);
    sendMessage(messageBuffer );

    return;
}

void DeltaHandlerController::service_general(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    phLogSimMessage(logId, LOG_VERBOSE, "perform \"%s\"",msg);

    return;
}

void DeltaHandlerController::service_status(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    checkData();

    sprintf(messageBuffer, "%ld", pHstatus->getStatusAsLong() );

    pHstatus->logStatus();

    replyToQuery(messageBuffer);

    return;
}

void DeltaHandlerController::service_srqbyte(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    int srqByte;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    checkData();

    srqByte = getSrqByte();
    sprintf(messageBuffer, "%d", srqByte);

    if ( corruptMessage && (replyQueryCount % 11 == 0) )
    {
        phLogSimMessage(logId, LOG_DEBUG, "corrupt srqbyte from %s to %s ",
                        messageBuffer,"corrupted");
        strcpy(messageBuffer, "corrupted");
    }

    replyToQuery(messageBuffer);

    return;
}

void DeltaHandlerController::service_querybins(const char *msg)
{
    char messageBuffer[256];
    char tmp[64];
    int s;
    int bin;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Determine which categories have been assigned to devices
     * in the testsite.
     */

    strcpy(messageBuffer,"");

    for ( s=0 ; s < pHandler->getNumOfSites() ; ++s)
    {
        bin=pHandler->getBinData(s);

        //
        // Offsite bug fixes reprobe is -3 for Flex
        // if ( all->model==Flex && bin==pHandler->getReprobeCategory() )
        // {
        //     bin=-1;
        // }
        //

        sprintf(tmp, "%d ", bin );
        strcat(messageBuffer,tmp);
    }

    sendMessage(messageBuffer );

    return;
}

void DeltaHandlerController::service_releaseparts(const char *msg)
{
    int s;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    checkData();

    /* 
     * allow the testsite to retract from the contractor and continue
     * processing
     */

    if ( releasepartsWhenReady==true && pHandler->readyForRelease()==false )
    {
        phLogSimMessage(logId, LOG_DEBUG, "releaseparts ignored: all bin data not yet received ");
    }
    else
    {
        for ( s=0 ; s < pHandler->getNumOfSites() ; ++s)
        {
            pHandler->releaseDevice(s);
        }

        if ( commandReply )
            sendMessage(commandReplyMessage);
    }
    return;
}

void DeltaHandlerController::service_stop(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    checkData();

    /* 
     * Stops the handler from processing and initiating any 
     * motor movements
     */
    operatorPause=false;
    pHstatus->setStatusBit(this, e_stopped, true);
    pHandler->stop();

    if ( commandReply )
        sendMessage(commandReplyMessage);
    return;
}

void DeltaHandlerController::service_temp_command(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH] = "";

    phLogSimMessage(logId, LOG_VERBOSE, "delta handling \"%s\"",msg);

    if( strstr(msg, "storage airtemp?") != NULL ) {
      strcpy(messageBuffer, "10.0C");
      sendMessage(messageBuffer);
    } else if( strstr(msg, "testsite airtemp?") != NULL ) {
      strcpy(messageBuffer, "20.0C");
      sendMessage(messageBuffer);
    } else if( strstr(msg, "tcu airtemp?") != NULL ) {
      strcpy(messageBuffer, "30.0C");
      sendMessage(messageBuffer);
    } else if ( strstr(msg, "masstemp?") != NULL ) {
      strcpy(messageBuffer, "40.0C");
      sendMessage(messageBuffer);
    } else if ( strstr(msg, "cool?") != NULL ) {
      sprintf(messageBuffer, "%d", 0);  /*off*/
      sendMessage(messageBuffer);
    } else if ( strstr(msg, "heat?") != NULL ) {
      sprintf(messageBuffer, "%d", 1);  /*on*/
      sendMessage(messageBuffer);
    } else if ( (strstr(msg, "lowerguardband?")!=NULL) || (strstr(msg, "lowguard?")!=NULL) ) {
      strcpy(messageBuffer, "2.0");
      sendMessage(messageBuffer);
    } else if ( (strstr(msg, "upperguardband?")!=NULL) || (strstr(msg, "highguard?")!=NULL) ) {
      strcpy(messageBuffer, "2.0");
      sendMessage(messageBuffer);
    } else if ( strstr(msg, "soaktime?") != NULL ) {
      strcpy(messageBuffer, "120");
      sendMessage(messageBuffer);
    } else if ( strstr(msg, "temperature?") != NULL ) {
      strcpy(messageBuffer, "30.0C, 40.0C");
      sendMessage(messageBuffer);
    } else if ( strstr(msg, "tempstatus?") != NULL ) {
      strcpy(messageBuffer, "0x800FFFF5");
      sendMessage(messageBuffer);
    } else if ( strstr(msg, "setpoint?") != NULL ) {
      strcpy(messageBuffer, "25.0C");
      sendMessage(messageBuffer);
    } else if(strstr(msg,"categorysource") != NULL){
      return;
    }
    else if ( strcasecmp(msg, "tempcontrol?") == 0 ) {
      sprintf(messageBuffer, "%d", 1);  /*enable*/      
      sendMessage(messageBuffer);
    } else if ( strcasecmp(msg, "testtemp?") == 0 ) {
      sprintf(messageBuffer, "%d", 1);  /*enable*/      
      sendMessage(messageBuffer);
    } else {
      strcpy(messageBuffer, "12345abcde");
      sendMessage(messageBuffer);
    }

    return;
}

void DeltaHandlerController::service_sitemapping(const char *msg)
{
  char messageBuffer[MAX_MSG_LENGTH] = "";
  phLogSimMessage(logId, LOG_VERBOSE, "delta handling \"%s\"",msg);
  strcpy(messageBuffer, "SITEMAP 1,2,3,4,5,6,7,8;");
  sendMessage(messageBuffer );
}


void DeltaHandlerController::service_bin(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    checkData();

    /* 
     * Sends selected part into designated category.
     * Format: [a,b,c,d,e,f,g,h] bin (retest:0,1..99)
     */

    int errflg = 0;
    char c;
    int site = 0;
    const char *posMsg;
    int bin = 0;
    char binString[64];

    /* get category [a-h] from message string */
    if ( sscanf(msg,"%c ",&c) == 1 )
    {
        site=c-'a';
    }
    else
    {
        ++errflg;
        phLogSimMessage(logId, LOG_VERBOSE,
            "unable to get bin category \"%s\" with sscanf \n"
            "expected format [a-h] bin n",
            msg);
    }

    /* get bin number (retest:0,1..99) from message string */
    posMsg = strstr(msg,"bin");
    if ( (posMsg != NULL) && (sscanf(posMsg,"bin %d",&bin) != 1) ) 
    {
        if ( sscanf(posMsg,"bin %s",binString) == 1 )
        {
            if ( strcmp(binString,"retest") == 0 )
            { 
                bin=pHandler->getRetestCategory();
            }
            else
            {
                ++errflg;
            } 
        }
        else 
        {
            ++errflg;
        }
    }

    if ( !errflg )
    {
        if ( verifyTest )
            pHandler->setBinData(site,bin);
        else
            pHandler->sendDeviceToBin(site,bin);
    }

    if ( commandReply )
        sendMessage(commandReplyMessage);

    return;  
}


void DeltaHandlerController::service_reprobe(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    checkData();

    /* 
     * Commands the contactor to do a reprobe operation.
     * Format: [a,b,c,d,e,f,g,h] reprobe!
     */

    char c;
    int site;
    int bin;

    /* get category [a-h] from message string */
    if ( sscanf(msg,"%c ",&c) == 1 )
    {
        site=c-'a';
        bin=pHandler->getReprobeCategory();
        if ( verifyTest )
            pHandler->setBinData(site,bin);
        else
            pHandler->sendDeviceToBin(site,bin);
    }
    else
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to get site category \"%s\" with sscanf \n"
            "expected format [a-h] reprobe!",
            msg);
    }
    if ( commandReply )
        sendMessage(commandReplyMessage);

    return;  
}


void DeltaHandlerController::service_stripmap(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    phLogSimMessage(logId, LOG_NORMAL, "The \"stripmap?\" query is only supported on Orion model.");
    return;
}

void DeltaHandlerController::service_index(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);
    phLogSimMessage(logId, LOG_NORMAL, "The \"index?\" query is only supported on Orion model.");
    return;
}

void DeltaHandlerController::service_materialid(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);
    phLogSimMessage(logId, LOG_NORMAL, "The \"materialid?\" query is only supported on Orion model.");
    return;
}


bool DeltaHandlerController::getHandlerIsReady() const
{
    return !(pHstatus->getStatusBit(e_handlerEmpty)) &&
           !(pHstatus->getStatusBit(e_outOfGuardBands)) &&
           !(pHstatus->getStatusBit(e_jammed)) &&
           !(pHstatus->getStatusBit(e_inputOrSortInhibitedOrDoorOpen));
}


/*****************************************************************************
 *
 * Castle Delta Handler Controller
 *
 * Authors: Chris Joyce
 *
 *****************************************************************************/

/*--- CastleHandlerController constructor -----------------------------------*/

CastleHandlerController::CastleHandlerController(DeltaHandlerControllerSetup *setup) :
    DeltaHandlerController(setup)
{
    pHandler->setReprobeCategory(100);
}

CastleHandlerController::~CastleHandlerController()
{
}

/*--- CastleHandlerController public functions ------------------------------*/

const char *CastleHandlerController::getName() const
{
    phLogSimMessage(logId, LOG_VERBOSE, "getName(): Castle");
    return "Delta Castle Handler";
}

/*--- CastleHandlerController protected functions ---------------------------*/

void CastleHandlerController::service_identify(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * handler type
     * GPIB address
     * softwareversion
     */

    sprintf(messageBuffer, "Castle Handler 42 Ver %sc Logic",softwareVersion);
    sendMessage(messageBuffer );

    return;
}

void CastleHandlerController::service_which(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * unique handler number
     * softwareversion
     */

    sprintf(messageBuffer, "%s:%sc", handlerNumber, softwareVersion);
    sendMessage(messageBuffer );
    return;
}


/*****************************************************************************
 *
 * Matrix Delta Handler Controller
 *
 *****************************************************************************/

/*--- MatrixHandlerController constructor -----------------------------------*/

MatrixHandlerController::MatrixHandlerController(DeltaHandlerControllerSetup *setup) :
    DeltaHandlerController(setup)
{
    pHandler->setReprobeCategory(100);
    softwareVersion="1.2.1.0";
}

MatrixHandlerController::~MatrixHandlerController()
{
}

/*--- MatrixHandlerController public functions ------------------------------*/

const char *MatrixHandlerController::getName() const
{
    phLogSimMessage(logId, LOG_VERBOSE, "getName(): Matrix");
    return "Delta Matrix Handler";
}

/*--- MatrixHandlerController protected functions ---------------------------*/

void MatrixHandlerController::service_identify(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * handler type
     * GPIB address
     * softwareversion
     */

    sprintf(messageBuffer, "Matrix Handler 06 %s",softwareVersion);
    sendMessage(messageBuffer );

    return;
}

void MatrixHandlerController::service_which(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * unique handler number
     * softwareversion
     */

    sprintf(messageBuffer, "%s:%sc", handlerNumber, softwareVersion);
    sendMessage(messageBuffer );
    return;
}


/*****************************************************************************
 *
 * Flex Delta Handler Controller
 *
 * Authors: Chris Joyce
 *
 *****************************************************************************/

/*--- FlexHandlerController constructor -------------------------------------*/

FlexHandlerController::FlexHandlerController(DeltaHandlerControllerSetup *setup) :
    DeltaHandlerController(setup)
{
    pHandler->setRetestCategory(0);
    pHandler->setReprobeCategory(-3);
}

FlexHandlerController::~FlexHandlerController()
{
}

/*--- FlexHandlerController public functions --------------------------------*/

const char *FlexHandlerController::getName() const
{
    phLogSimMessage(logId, LOG_VERBOSE, "getName(): Flex");
    return "Delta Flex Handler";
}
/*--- FlexHandlerController protected functions -----------------------------*/

void FlexHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_DEBUG, "received %3d: \"%s\"",
                    getReceiveMessageCount(),
                    message);

    setStrlower(message);

    implHandleMessage(message);
}

void FlexHandlerController::implHandleMessage(char* message)
{
    if (phToolsMatch(message, ".* bin .*")) 
        service_bin(message);
    else if (phToolsMatch(message, ".* reprobe!?"))
        service_reprobe(message);
    else if (phToolsMatch(message, "retract .*"))
        service_retract(message);
    else if (phToolsMatch(message, "contact .*"))
        service_contact(message);
    else
    {
        DeltaHandlerController::implHandleMessage(message);
    }
}

void FlexHandlerController::service_identify(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    sendMessage("Delta Design Flex Handler 6 Sun May 19 1996 08:33 ");

    return;
}

void FlexHandlerController::service_which(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * unique handler number
     * softwareversion
     */

    sprintf(messageBuffer, "%s:%sc 12.0", handlerNumber, softwareVersion);
    sendMessage(messageBuffer );
    return;
}

void FlexHandlerController::service_retract(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    checkData();

    /* 
     * Retracts parts represented by the bitmask from their
     * respective testsites to the REPROBE position inserting
     * all others.
     * format: retract (0..255)
     */

    int bitMask;

    /* get bitMask from message string */
    if ( sscanf(msg,"retract %d ",&bitMask) == 1 )
    {
        for ( int s=0 ; s < pHandler->getNumOfSites() ; ++s)
        {
            pHandler->setContact(s, !( (1 << s) & bitMask ) );
        }
    }
    else
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to get bitmask from \"%s\" with sscanf \n"
            "expected format retract n",
            msg);
    }
    if ( commandReply )
        sendMessage(commandReplyMessage);

    return;  
}


void FlexHandlerController::service_contact(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    checkData();

    /* 
     * Inserts parts represented by the bitmask into their
     * respective testsites retracting all others to the 
     * REPROBE position.
     * format: contact (1..255)
     */

    int bitMask;

    /* get bitMask from message string */
    if ( sscanf(msg,"contact %d ",&bitMask) == 1 )
    {
        for ( int s=0 ; s < pHandler->getNumOfSites() ; ++s)
        {
            pHandler->setContact(s, ( (1 << s) & bitMask ) );
        }
    }
    else
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unable to get bitmask from \"%s\" with sscanf \n"
            "expected format contact n",
            msg);
    }
    if ( commandReply )
        sendMessage(commandReplyMessage);

    return;  
}



/*****************************************************************************
 *
 * Rfs Delta Handler Controller
 *
 * Authors: Chris Joyce
 *
 *****************************************************************************/

/*--- RfsHandlerController constructor --------------------------------------*/

RfsHandlerController::RfsHandlerController(DeltaHandlerControllerSetup *setup) :
    DeltaHandlerController(setup)
{
    pHandler->setRetestCategory(0);
    pHandler->setReprobeCategory(-3);
}

RfsHandlerController::~RfsHandlerController()
{
}

/*--- RfsHandlerController public functions ---------------------------------*/

const char *RfsHandlerController::getName() const
{
    phLogSimMessage(logId, LOG_VERBOSE, "getName(): Rfs");
    return "Delta Rfs Handler";
}

/*--- RfsHandlerController protected functions ------------------------------*/

void RfsHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_DEBUG, "received %3d: \"%s\"",
                    getReceiveMessageCount(),
                    message);

    setStrlower(message);

    implHandleMessage(message);
}

void RfsHandlerController::implHandleMessage(char* message)
{
    if (phToolsMatch(message, ".* bin .*")) 
        service_bin(message);
    else if (phToolsMatch(message, ".* reprobe!?"))
        service_reprobe(message);
    else if (phToolsMatch(message, "conehdnum\\??"))
        service_numtestsites(message);
    else if (phToolsMatch(message, "chn\\??"))
        service_numtestsites(message);
    else if (phToolsMatch(message, ".* chuckdisable\\?"))
        service_site_disabled(message);
    else if (phToolsMatch(message, ".* cds\\?"))
        service_site_disabled(message);
    else
    {
        DeltaHandlerController::implHandleMessage(message);
    }
}

void RfsHandlerController::service_start(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    checkData();

    if ( getHandlerIsReady() == true )
    {
        phLogSimMessage(logId, LOG_DEBUG, "start ignored: start must be initiated by operator ");
    }
    else
    {
        if ( pHstatus->getStatusBit(e_jammed) )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                    "unable to start handler: Handler is jammed.");
        }
        else if ( pHstatus->getStatusBit(e_inputOrSortInhibitedOrDoorOpen) )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                    "unable to start handler: Door is open. ");
        }
        else
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                    "unable to start handler ");
        }
    }
    return;
}


void RfsHandlerController::service_identify(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    sprintf(messageBuffer, "TRFS 42 V%sc",softwareVersion);
    sendMessage(messageBuffer);

    return;
}


void RfsHandlerController::service_which(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * unique handler number
     * softwareversion
     */

    sprintf(messageBuffer, "%s:%sc", handlerNumber, softwareVersion);
    sendMessage(messageBuffer );
    return;
}


void RfsHandlerController::service_site_disabled(const char *msg)
{
    char c;
    char messageBuffer[MAX_MSG_LENGTH];
    int site;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Format: [a,b,c,d] chuckdisable? 
     * return 1 if disabled
     */

    /* get category [a-d] from message string */
    if ( sscanf(msg,"%c ",&c) == 1 )
    {
        site=c-'a';
        
        sprintf(messageBuffer, "%d", int(pHandler->getSiteDisabled(site)) );
        sendMessage(messageBuffer );
    }
    else
    {
        phLogSimMessage(logId, LOG_VERBOSE,
            "unable to get bin category \"%s\" with sscanf \n"
            "expected format [a-h] chuckdisable? n",
            msg);
    }

    return;
}

/*****************************************************************************
 *
 * Summit Delta Handler Controller
 *
 * Authors: Chris Joyce
 *
 *****************************************************************************/

/*--- SummitHandlerController constructor -----------------------------------*/

SummitHandlerController::SummitHandlerController(DeltaHandlerControllerSetup *setup) :
    DeltaHandlerController(setup)
{
    pHandler->setReprobeCategory(100);
}

SummitHandlerController::~SummitHandlerController()
{
}

/*--- SummitHandlerController public functions ------------------------------*/

const char *SummitHandlerController::getName() const
{
    phLogSimMessage(logId, LOG_VERBOSE, "getName(): Summit");
    return "Delta Summit Handler";
}

/*--- SummitHandlerController protected functions ---------------------------*/

void SummitHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_NORMAL, "received %3d: \"%s\"",
                    getReceiveMessageCount(),
                    message);

    setStrlower(message);

    implHandleMessage(message);
}

void SummitHandlerController::implHandleMessage(char* message)
{
    if (phToolsMatch(message, ".* bin .*")) 
        service_bin(message);
    else if (phToolsMatch(message, "reprobe!? .*"))
        service_reprobe(message);
    else if (phToolsMatch(message, "recipestart .*"))
        service_recipestart(message);
    else if (phToolsMatch(message, "recipeend .*"))
        service_recipeend(message);
    else if (phToolsMatch(message, "recipe activategrp .*"))
        service_general(message);
    else
    {
        DeltaHandlerController::implHandleMessage(message);
    }
}

unsigned char SummitHandlerController::getSrqByte()
{
    unsigned char srqByte;

    pHstatus->logStatus();

    // bit 0 Devices Ready to test
    srqByte = (pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0 ;

    // bit 1 Device Notification Pending

    // bit 2 Lot Notification Pending

    // bit 3 Spare

    // bit 4 Handler Notification Pending
    srqByte|= (1<<4) * !getHandlerIsReady();

    // bit 5 Interface Notification Pending
        
    // bit 7 Spare

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte before mask %d",int(srqByte));

                 // Mask out bits not needed
    srqByte&=srqMask;

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte with mask %d",int(srqByte));

    return srqByte;
}

void SummitHandlerController::service_identify(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    sprintf(messageBuffer, "Summit %s.0",softwareVersion);
    sendMessage(messageBuffer );

    return;
}

void SummitHandlerController::service_which(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * unique handler number
     * softwareversion
     */
    sprintf(messageBuffer, "%s:%s", handlerNumber, softwareVersion);
    sendMessage(messageBuffer );
    return;
}


void SummitHandlerController::service_temp_command(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "summit handling \"%s\"",msg);

    sendMessage("command OK");
    return;
}

void SummitHandlerController::service_reprobe(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);
    checkData();

    /* 
     * Commands the contactor to do a reprobe operation.
     * Format: reprobe 1
     * meaning reprobe all sites
     */

    int site=0;
    long binPattern;
    int bin;

    /* get categoriesl from message string */
    if ( strcmp(msg,"reprobe 1") == 0 )
    {
        binPattern = pHandler->getSitePopulationAsLong();

        for ( site=0 ; (1<<site) <= binPattern  ; ++site )
        {
            if ( binPattern & (1<<site) )
            {
                bin=pHandler->getReprobeCategory();
                if ( verifyTest )
                    pHandler->setBinData(site,bin);
                else
                    pHandler->sendDeviceToBin(site,bin);
            }
        }
        pHandler->handleDevices();
    }
    else
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR,
            "unexpected reprobe command: \"%s\"\n"
            "expected format reprobe 1",
            msg);
    }
    if ( commandReply )
        sendMessage(commandReplyMessage);

    return;  
}


void SummitHandlerController::service_recipestart(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * handler may respond with recipegood or errorcode recipebad
     */

    sendMessage("recipegood");

    return;  
}


void SummitHandlerController::service_recipeend(const char *msg)
{
    char message[MAX_RECIPE_NAME_LENGTH+1];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * handler may respond with recipeend recipe_name or errorcode recipebad
     */

    /* get recipe_name from message string */
    if ( strlen(msg) > MAX_RECIPE_NAME_LENGTH || sscanf(msg,"%64[^?]",message) != 1 )
    {
        phLogSimMessage(logId, LOG_VERBOSE,
            "unable to get recipe_name from message \"%s\" with sscanf \n"
            "expected format: recipeend recipe_name",
            msg);
    }
    else
    {
        sendMessage( message );
    }

    return;  
}


/*****************************************************************************
 *
 * Delta Orion Handler Controller
 *
 * Authors: Ryan Lessman
 *
 *****************************************************************************/

/*--- OrionHandlerController constructor -----------------------------------*/

OrionHandlerController::OrionHandlerController(DeltaHandlerControllerSetup *setup) :
    DeltaHandlerController(setup)
{
    pHandler->setRetestCategory(0);
    //pHandler->start();
}

OrionHandlerController::~OrionHandlerController()
{
}

/*--- OrionHandlerController public functions ------------------------------*/

const char *OrionHandlerController::getName() const
{
    phLogSimMessage(logId, LOG_VERBOSE, "getName(): Orion");
    return "Delta Orion Strip Handler";
}

/*--- OrionHandlerController protected functions ---------------------------*/

void OrionHandlerController::service_identify(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * handler "identify?" query string
     */

    sprintf(messageBuffer, "Orion");
    sendMessage(messageBuffer );

    return;
}

void OrionHandlerController::service_which(const char *msg)
{
    phLogSimMessage(logId, LOG_NORMAL, "Command \"which?\" not supported by Orion");

    return;
}


void OrionHandlerController::service_testpartsready(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];
    unsigned long devicesReady;

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    devicesReady=pHandler->getSitePopulationAsLong();

    /*phLogSimMessage(logId, LOG_NORMAL, "RL - devicesReady = %lu", devicesReady);*/

    sprintf(messageBuffer, "%lu", devicesReady );

    /*phLogSimMessage(logId, LOG_NORMAL, "RL - messagebuffer = %s", messageBuffer);*/

    /* The Orion handler responds differently to 'testpartsready?' compared to the other
       Delta handlers.  It is not just a number, but a series of values:
       M,I,S,Y,X
       M = value representing bit pattern of site population.  1 = device ready. 
       I = the current index count.
       S = Strip ID read by DCR (what is DCR?) 
       Y = the current row.
       X = the current column.  
       
       For now, I don't think the I,S,Y,X values are important, so values are hardcoded. */
    strcat(messageBuffer, ",1,,0,0");

    if ( corruptMessage && (replyQueryCount % 11 == 0) )
    {
        phLogSimMessage(logId, LOG_DEBUG, "corrupt testpartsready from %s to %s ",
                        messageBuffer,"corrupted");
        strcpy(messageBuffer, "corrupted");
    }
    replyToQuery(messageBuffer);
}


void OrionHandlerController::service_stripmap(const char *msg)
{
    char stripmap_answer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);

    /* Will hardcode a stripmap response for now.  (4x8 strip, 32 sites, single index.)
    Testing at Delta Design only had a "single index" strip.
    These numbers would be different if mulitple indexing was
    used.  If a 4x8 strip was split into two indices, the answer might look like:
    "strip,8,4,step,4,4,2".*/
    strcpy(stripmap_answer, "strip,8,4,step,8,4,1");
    replyToQuery(stripmap_answer);
}

void OrionHandlerController::service_index(const char *msg)
{
    char index_answer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);

    /* Will hardcode index response for now.  This number is per-strip, meaning if a strip only has
    one index location (single index) then this is always 1.  If a strip has five index points for
    example, this could be 1, 2, 3, 4 or 5.  Testing at Delta Design only had one index per strip.*/
    strcpy(index_answer, "1");  /* It would be nice to have this auto-increment at some point. */
    replyToQuery(index_answer);
}

void OrionHandlerController::service_materialid(const char *msg)
{
    char materialid_answer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);

    /* Will hardcode materialid response for now.*/
    strcpy(materialid_answer, "123456789ABC");
    replyToQuery(materialid_answer);
}


unsigned char OrionHandlerController::getSrqByte()
{
    unsigned char srqByte;

    pHstatus->logStatus();

    // bit 0 Devices Ready to test
    srqByte = (pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0 ;

    // bit 6 Orion is asserting the SRQ line (should be cleared after serial poll from tester).
    srqByte |= (1<<6);

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte before mask %d",int(srqByte));

    // Mask out bits not needed
    srqMask=0x41;
    srqByte&=srqMask;

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte with mask %d",int(srqByte));

    return srqByte;
}

/*****************************************************************************
 *
 * Delta Eclipse Handler Controller
 *
 * Authors: Adam Huang
 *
 *****************************************************************************/

/*--- EclipseHandlerController constructor -----------------------------------*/

EclipseHandlerController::EclipseHandlerController(DeltaHandlerControllerSetup *setup) :
    DeltaHandlerController(setup)
{
    verifyTest = true;
    pHandler->setRetestCategory(0);
    //pHandler->start();
}

EclipseHandlerController::~EclipseHandlerController()
{
}

/*--- EclipseHandlerController public functions ------------------------------*/

const char *EclipseHandlerController::getName() const
{
    phLogSimMessage(logId, LOG_VERBOSE, "getName(): Eclipse");
    return "Delta Eclipse Handler";
}

/*--- EclipseHandlerController protected functions ---------------------------*/

void EclipseHandlerController::service_identify(const char *msg)
{
    char messageBuffer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    /* 
     * Returns the following handler information:
     * handler "identify?" query string
     */

    sprintf(messageBuffer, "Eclipse");
    sendMessage(messageBuffer );

    return;
}

void EclipseHandlerController::service_which(const char *msg)
{
    phLogSimMessage(logId, LOG_NORMAL, "Command \"which?\" not supported by Eclipse");

    return;
}

void EclipseHandlerController::service_materialid(const char *msg)
{
    char materialid_answer[MAX_MSG_LENGTH];

    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"", msg);

    /* Will hardcode materialid response for now.*/
    strcpy(materialid_answer, "1234000000005678:2234000000005678:3234000000005678:4234000000005678");
    replyToQuery(materialid_answer);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/

