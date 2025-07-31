/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : TesecHandlerController.C
 * CREATED  : 12 Feb 2002
 *
 * CONTENTS : Tesec handler controller implementation
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 12 Feb 2002, Chris Joyce , created
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

#include "TesecHandlerController.h"
#include "handlerController.h"
#include "handler.h"
#include "HandlerViewController.h"
#include "eventObserver.h"
#include "simHelper.h"
#include "ph_tools.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/



/*--- class implementations -------------------------------------------------*/

/*--- TesecHandlerController ------------------------------------------------*/


TesecHandlerController::TesecHandlerController(
        TesecHandlerControllerSetup   *setup       /* configurable setup values */,
        ViewControllerSetup           *VCsetup     /* view controller */) :
    HandlerController(setup),
    model(setup->model)
{
    try {

        pStatus = new Status<eTesecStatus>(logId);
        pStatus->setStatusList(new HandlerStatusBit<eTesecStatus>(stopped,"stopped",false,1,true) );
        pStatus->setStatusList(new HandlerStatusBit<eTesecStatus>(empty,"empty",false,1,true) );
        pStatus->registerObserver(this);
        pStatus->selfTest();

        pHViewController = new HandlerViewController<eTesecStatus>(logId,this,pStatus,VCsetup);
        pHViewController->showView();
        pViewController=pHViewController;

        if ( setup->recordFile )
        {
            pEventRecorder = new EventRecorder<eTesecStatus>(logId,
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
            pEventPlayback = new EventPlayback<eTesecStatus>(logId,
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
}

TesecHandlerController::~TesecHandlerController()
{
}

unsigned char TesecHandlerController::getSrqByte()
{
    unsigned char srqByte = 0;

    switch ( model )
    {
      case T9588IH:

        srqByte = char(pHandler->getSitePopulationAsLong());

        break;
      case T3270IH:
        srqByte = (pHandler->getSitePopulationAsLong()) ? 0x40:0x00;
        break;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "srqByte %d",int(srqByte));

    return srqByte;
}

void TesecHandlerController::switchOn() 
{
    while (!done)
    {
        if ( pEventPlayback )
        {
            pEventPlayback->checkEvent( getReceiveMessageCount() );
        }

        pHViewController->checkEvent(0);

        tryReceiveMsg(1000);

     switch(model){
       case T9588IH:
       if ( pHandler->sendTestStartSignal() )
       {
#if    0
           if ( pHandler->testsNotStarted() )
           {
                // send when power switched on
                sendSRQ(0x40);

                // handler has received a device clear
                sendSRQ(0x40);
                pStatus->setStatusBit(this, stopped, false);
           }
#endif
           unsigned char      srqSequence=getSrqByte();

                         // bit 6 SRQ asserted
           srqSequence|= (1<<6);
           sendSRQ(srqSequence);
         }
         break;
     case T3270IH:
        if ( pHandler->sendTestStartSignal() )
        {
            if ( pHandler->testsNotStarted() )
            {
                // send when power switched on
                

                // handler has received a device clear
               
                pStatus->setStatusBit(this, stopped, false);
            }

           unsigned char   srqSequence=getSrqByte();

           // bit 6 SRQ asserted
           srqSequence &= (1<<6);
           phLogSimMessage(logId, LOG_DEBUG, "Send Srq is \"%lx\"",srqSequence);
           sendSRQ(srqSequence);
        }
        break;
     case T5170IH:
        if (pHandler->sendTestStartSignal())
        {
          if(pHandler->isStartFor5170())
          {
              unsigned char srqSequence = 0x42;
              sendSRQ(srqSequence);
              sleep(1);
              srqSequence = 0x41;
              sendSRQ(srqSequence);
              sleep(1);
              srqSequence = 0x40;
              sendSRQ(srqSequence);
              pHandler->setStartFlagFor5170(false);
          }
        }
        break;
     default:
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

void TesecHandlerController::StatusChanged(eTesecStatus ebit, bool b)
{
    switch (ebit)
    {
      case stopped:
        phLogSimMessage(logId, LOG_VERBOSE, "TesecHandlerController stop detected new value %s",
                        (b) ? "True" : "False");
        if ( b )
        {
            pHandler->stop();
        }
        else
        {
             phLogSimMessage(logId, LOG_DEBUG,"Enter StatusChanged,value of stopped is:false");
            pHandler->start();
        }
        break;
      case empty:
        phLogSimMessage(logId, LOG_VERBOSE, "TesecHandlerController empty detected new value %s",
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

TesecHandlerController::eTesecModel TesecHandlerController::getModelType() const
{
    return model;
}

const char *TesecHandlerController::getModelName() const
{
    const char *name;

    switch ( model )
    {
      case T9588IH:
        name="Tesec GPIB Handler 9588-IH";
        break;
      case T3270IH:
        name="Tesec GPIB Handler 3270-IH";
        break;
      case T5170IH:
        name="Tesec GPIB Handler 5170-IH";
        break;
      defalut:
        name="no GPIB Handler";
        break;

    }

    return name;
}


/*--- TesecHandlerController protected and public functions --------------------*/


void TesecHandlerController::handleMessage(char* message)
{
    phLogSimMessage(logId, LOG_DEBUG, "Received message \"%s\"",message);

    if (phToolsMatch(message, "S.*"))
        serviceSortSignal(message);
    else if(phToolsMatch(message, "D.*")){
        serviceAckSignal(message);  
    }
    else if(phToolsMatch(message, "I")){
        serviceRingID(message);  
    }
    else   
    {
        pHViewController->setGpibStatus(ViewController::notUnderstood);
    }
}
void TesecHandlerController::serviceAckSignal(const char *msg)
{   
    char messageBuffer[MAX_MSG_LENGTH]="";
    char devicesReady[16]="";
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

//    pHandler->getSitePopulationAsLong_Tesec3270(devicesReady);

    //sprintf(messageBuffer,"D%s", devicesReady );
    strcat(messageBuffer,"D");
    
    if(model == T5170IH)
    {
        int num_of_sites=pHandler->getNumOfSites(); 
        if(num_of_sites == 4)
            strcat(messageBuffer,"004001002,003002003,002006008,001003004");
        else if(num_of_sites == 8)
            strcat(messageBuffer,"008002004,006001007,004001002,003002003,002006008,001003004");
        else if(num_of_sites == 72)
            strcat(messageBuffer,"072003004,070005006,008002004,006001007,004001002,003002003,002006008,001003004");
    }
    else
    strcat(messageBuffer,devicesReady);

    replyToQuery(messageBuffer);
 
}

void TesecHandlerController::serviceRingID(const char *msg)
{   
    char messageBuffer[MAX_MSG_LENGTH]="";
    char devicesReady[16]="";
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    
    if(model == T5170IH)
    {
        static int num = 1;
        if(num>1000)
          num=1;
        sprintf(messageBuffer,"IRING_ID_%d",num++);
    }
    
    replyToQuery(messageBuffer);
 
}

void TesecHandlerController::serviceSortSignal(const char *msg)
{
    phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

    //
    // Sort Signal Command expected format
    // SCXXXXQ\r\n 
    //  ^  ^
    //  |  |____ test result station 1,2,3,4
    //  |
    //  |_______ Condition (number of sites)
    //

    const char *posMsg=msg;
    int num_of_sites=pHandler->getNumOfSites();
    int bin;
    int i;
    int site=0;
    static int touchDownNum = 4 ;
    static int lotNum = 3;
    static int index = 0;
 
    /* go past first S in message */
    ++posMsg;
    char tmpMsg[128] = "";
    strcpy(tmpMsg,msg);
    char* token = tmpMsg;
    if(model == T5170IH)
    {
      while( (token=strtok(token,",")) != NULL)
      {
        char arr[12] = "";
        strcpy(arr,token);
        int index = 0;
        int binId =0;
        if(strchr(arr,'S') == NULL)
        {
          char tmp[12] = "";
          strncpy(tmp,arr,3);
          index = atoi(tmp);
          strcpy(tmp,arr+3);
          binId = atoi(tmp);
        }
        else
        {
          char tmp[12] = "";
          strncpy(tmp,arr+1,3);
          index = atoi(tmp);
          strcpy(tmp,arr+4);
          binId = atoi(tmp);
        }
        phLogSimMessage(logId, LOG_DEBUG,"Send device on site %d to bin %d",index,binId);
        token = NULL;
      }
      ++index;
     
      if(lotNum <= 0)
      {
        sendSRQ(0X44);
        lotNum = 3;
      }
      else
      {
        if(index < touchDownNum)
          sendSRQ(0x40);
        else
        {
          index =0;
          --lotNum;
          if(lotNum <=0)
          {
             sendSRQ(0X44);
             lotNum = 3;
             return;
          }
          sendSRQ(0x43);
          sleep(1);
          sendSRQ(0x41);
          sleep(1);
          sendSRQ(0x40);
        }

      }
      return;

    }
    /* check Condition part of command (number of sites) */
    if ( num_of_sites != ( 0xBF & *posMsg ) )
    {
       // phLogSimMessage(logId, PHLOG_TYPE_ERROR,
       //                 "number of sites in sort command \"%s\" (%d) does not match number in handler set-up %d ",
       //                 msg,( 0xBF & *posMsg),num_of_sites );
          phLogSimMessage(logId, LOG_DEBUG,
                        "number of sites in sort command \"%s\" (%d) does not match number in handler set-up %d ",
                        msg,( 0xBF & *posMsg),num_of_sites );

    }
     switch(model){
    case T3270IH:
    site=(0xBF & *posMsg)-1;
    for(i=num_of_sites-1;i>site;i--){
          pHandler->setSitePopulated(i,0);
    }
    ++posMsg;

    /* get binning information */
   // int site=num_of_sites-1;
    while ( posMsg && *posMsg && (site > -1) )
    {
        bin=( 0xBF & *posMsg );

        if ( pHandler->getSitePopulated(site) )
        {
            pHandler->sendDeviceToBin(site,bin);
        }
        else if ( bin != 0 )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                            "in sort command \"%s\" received binning information (%d) for site (%d) which was not populated",
                            msg, bin, site+1);
        }
        --site;
        ++posMsg;
    }

    /* check received all binning information and the final Q */
    if ( site!=-1 || !(posMsg) || *posMsg!='Q' )
    {
        if ( site != num_of_sites )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                            "in sort command \"%s\" did not receive binning information for all sites (%d) but only %d ",
                            msg, num_of_sites, site);
           //   phLogSimMessage(logId, LOG_DEBUG,
           //                 "in sort command \"%s\" did not receive binning information for all sites (%d) but only %d ",
           //                 msg, num_of_sites, site);

        }
        else 
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                            "in sort command \"%s\" not terminated correctly ", msg);

        }
    }
    break;
    case T9588IH:
     /* go past first S in message */

    ++posMsg;
    site=0;  
    while ( posMsg && *posMsg && (site < num_of_sites) )
    {
        bin=( 0xBF & *posMsg );

        if ( pHandler->getSitePopulated(site) )
        {
            pHandler->sendDeviceToBin(site,bin);
        }
        else if ( bin != 0 )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                            "in sort command \"%s\" received binning information (%d) for site (%d) which was not populated",
                            msg, bin, site+1);
        }
        ++site;
        ++posMsg;
    }

    /* check received all binning information and the final Q */
    if ( site!=num_of_sites || !(posMsg) || *posMsg!='Q' )
    {
        if ( site != num_of_sites )
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                            "in sort command \"%s\" did not receive binning information for all sites (%d) but only %d ",
                            msg, num_of_sites, site);
        }
        else
        {
            phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                            "in sort command \"%s\" not terminated correctly ", msg);

        }
    }
    break;
    default:
    break;

}
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
