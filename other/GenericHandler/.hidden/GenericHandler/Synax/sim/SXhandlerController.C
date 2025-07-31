/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SXhandlerController.C
 * CREATED  : 25 Jan 2001
 *
 * CONTENTS : Synax handler controller implementation
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

#include "SXhandlerController.h"
#include "handlerController.h"
#include "handler.h"
#include "SXstatus.h"
#include "SXviewController.h"
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

/*--- SXHandlerController ---------------------------------------------------*/


SXHandlerController::SXHandlerController(SXHandlerControllerSetup *setup,
                                         ViewControllerSetup    *VCsetup) :
HandlerController(setup),
model(setup->model),
verifyTest(setup->verifyTest),
multipleSrqs(setup->multipleSrqs)
{
  try {

    pSXstatus = new SynaxStatus(logId);
    pSXstatus->registerObserver(this);
    pSXstatus->setStatusBit(this, SynaxStatus::stopped, true);

    pSXViewController = new SXViewController(logId,this,pSXstatus,VCsetup);
    pSXViewController->showView();
    pViewController=pSXViewController;

//        Status<eYStatus> yStatus(logId);
//        yStatus.setStatusList( 
//            new HandlerStatusBit<eYStatus>(y_stopped, "stopped", false, 0) );
//        yStatus.setStatusList( 
//            new HandlerStatusBit<eYStatus>(y_alarm,   "alarm",   false, 0) );
//        yStatus.setStatusList( 
//            new HandlerStatusBit<eYStatus>(y_errorTx, "errorTx", false, 0) );
//        yStatus.setStatusList( 
//            new HandlerStatusBit<eYStatus>(y_empty,   "empty",   false, 0) );

    if( setup->recordFile ) {
      pEventRecorder = new EventRecorder(logId,
                                         this,
                                         pSXstatus,
                                         pSXViewController,
                                         setup->recordFile);
    }

    if( setup->playbackFile ) {
      pEventPlayback = new EventPlayback(logId,
                                         pSXstatus,
                                         setup->playbackFile);
    }
  } catch( Exception &e ) {
    Std::cerr << "FATAL ERROR: ";
    e.printMessage();
    throw;
  }
  srqMask=0xFF;

  pSXViewController->setGpibStatus(ViewController::okay);
}

SXHandlerController::~SXHandlerController()
{
}

unsigned char SXHandlerController::getSrqByte()
{
  unsigned char srqByte = 0;

  pSXstatus->logStatus();

  switch( model ) {
    case SX1101:
    case SX1701:
    case SX2400:
      // bit 0 Demand of start
      srqByte = (pHandler->getSitePopulationAsLong()) ? 0x1 : 0x0 ;

      // bit 1 (don't use) Demand of manual start

      // bit 2 Alarm set on handler
      srqByte|= (1<<2) * pSXstatus->getStatusBit(SynaxStatus::alarm);

      // bit 3 Error in transmission
      srqByte|= (1<<3) * pSXstatus->getStatusBit(SynaxStatus::errorTx);

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

void SXHandlerController::switchOn() 
{
  while(!done) {
    if( pEventPlayback ) {
      pEventPlayback->checkEvent( getReceiveMessageCount() );
    }

    pSXViewController->checkEvent(0);

    tryReceiveMsg(1000);

    if( pHandler->sendTestStartSignal() ) {
      pSXstatus->setStatusBit(this, SynaxStatus::stopped, false);

      unsigned char srqSequence=getSrqByte();

      // bit 6 SRQ asserted
      srqSequence|= (1<<6);

      if( multipleSrqs ) {
        if( 0x01 & srqSequence ) {
          sendSRQ(srqSequence);
          sendSRQ(srqSequence);
          sendSRQ(srqSequence);
          sendSRQ(srqSequence);
        }
      } else {
        sendSRQ(srqSequence);
      }
    }

    if( pHandler->allTestsComplete() && pSXstatus->getStatusBit(SynaxStatus::empty)==false ) {
      pHandler->logHandlerStatistics();

      pSXstatus->setStatusBit(this, SynaxStatus::stopped, true);
      pSXstatus->setStatusBit(this, SynaxStatus::empty, true);
    }
  }
}

void SXHandlerController::SXStatusChanged(SynaxStatus::eSXStatus ebit, bool b)
{
  switch(ebit) {
    case SynaxStatus::stopped:
      phLogSimMessage(logId, LOG_VERBOSE, "SXHandlerController stop detected new value %s",
                      (b) ? "True" : "False");
      if( b ) {
        pHandler->stop();
      } else {
        pHandler->start();
      }
      break;
    case SynaxStatus::alarm:
      phLogSimMessage(logId, LOG_VERBOSE, "SXHandlerController alarm detected new value %s",
                      (b) ? "True" : "False");
      break;
    case SynaxStatus::errorTx:
      phLogSimMessage(logId, LOG_VERBOSE, "SXHandlerController errorTx detected new value %s",
                      (b) ? "True" : "False");
      break;
    case SynaxStatus::empty:
      phLogSimMessage(logId, LOG_VERBOSE, "SXHandlerController empty detected new value %s",
                      (b) ? "True" : "False");
      if( b ) {
        pHandler->stop();
      } else {
        pHandler->reset();
      }
      break;
  }
}

SXHandlerController::eSXModel SXHandlerController::getModelType() const
{
  return model;
}

/*--- SXHandlerController protected and public functions --------------------*/


void SXHandlerController::handleMessage(char* message)
{
  phLogSimMessage(logId, LOG_DEBUG, "Received message \"%s\"",message);

  setStrlower(message);

  if(phToolsMatch(message, "fullsites\\?"))
    serviceFullsites(message);
  else if(phToolsMatch(message, "binon:.*"))
    serviceBinon(message);
  else if(phToolsMatch(message, "echook"))
    serviceEchoOk(message);
  else if(phToolsMatch(message, "echong"))
    serviceEchoNg(message);
  else if(phToolsMatch(message, "fr\\?"))
    serviceStatus(message);
  else if(phToolsMatch(message, "srqmask .*"))
    serviceSrqmask(message);
  else {
    pSXViewController->setGpibStatus(ViewController::notUnderstood);
  }
}


void SXHandlerController::serviceStatus(const char* msg)
{
  char messageBuffer[MAX_MSG_LENGTH];

  phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

  sprintf(messageBuffer, "FR%d", int(!pSXstatus->getStatusBit(SynaxStatus::stopped)) );

  pSXstatus->logStatus();

  replyToQuery(messageBuffer);

  return;
}

void SXHandlerController::serviceFullsites(const char *msg)
{
  char messageBuffer[MAX_MSG_LENGTH];
  unsigned long devicesReady;

  phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

  devicesReady=pHandler->getSitePopulationAsLong();

  sprintf(messageBuffer, "FULLSITES %.8lx", devicesReady );

  replyToQuery(messageBuffer);

  return;
}

void SXHandlerController::serviceBinon(const char *msg)
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

  while( posMsg && *posMsg && site>=0 ) {
    posMsg=findalphanum(posMsg);

    if( isalnum(*posMsg) ) {
      if( site < pHandler->getNumOfSites() ) {

        bin=convertHexadecimalDigitIntoInt(*posMsg);

        if( bin >= 0 ) {
          if( verifyTest ) {
            pHandler->setBinData(site,bin);
          } else {
            pHandler->sendDeviceToBin(site,bin);
          }
        }
      }
      --site;
      ++posMsg;
    }
  }


  if( site>=0 ) {
    phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                    "unable to interpret BINON data: \"%s\" after position at site %d ",
                    msg,site+1);
  } else {
    if( verifyTest ) {
      serviceEcho();
    }
  }
}


void SXHandlerController::serviceEcho()
{
  char messageBuffer[MAX_MSG_LENGTH];
  char tmp[64];
  int site = 0;
  char bin = 0;

  phLogSimMessage(logId, LOG_VERBOSE, "serviceEcho()");

  //
  // echo bin information
  // format
  // ECHO:xxxxxxxx,xxxxxxxx,xxxxxxxx,xxxxxxxx;
  //
  strcpy(messageBuffer,"ECHO:");
  for( site=31 ; site>=0  ; --site)
  {
      bin = (model==SX2400?'A':'0');
    if( site < pHandler->getNumOfSites() )
    {
        int itmp = pHandler->getBinData(site);
        if(itmp == siteNotYetAssignedBinNumber)
            bin = (model==SX2400?'A':'0');
        else if(itmp < 10)
            bin = '0' + itmp;
        else
            bin = 'A' + itmp - 10;
    }

    sprintf(tmp, "%c", bin);
    strcat(messageBuffer,tmp);

    if( site == 0 ) {
      strcat(messageBuffer,";");
    } else if( site % 8 == 0 ) {
      strcat(messageBuffer,",");
    }
  }
  sendMessage(messageBuffer);

  return;
}

void SXHandlerController::serviceEchoOk(const char *msg)
{
  int s;

  phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

  for( s=0 ; s < pHandler->getNumOfSites() ; ++s) {
    pHandler->releaseDevice(s);
  }
  pHandler->resetVerifyCount();

  return;
}

void SXHandlerController::serviceEchoNg(const char *msg)
{
  phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

  pHandler->binDataVerified();

  if( pHandler->isStopped() ) {
    pSXstatus->setStatusBit(this, SynaxStatus::stopped, true);
  }
  return;
}


void SXHandlerController::serviceSrqmask(const char *msg)
{
  phLogSimMessage(logId, LOG_VERBOSE, "handling \"%s\"",msg);

  /*
   * Mask byte for SRQ generation.
   */

  if( sscanf(msg,"srqmask %d",&srqMask) != 1 ) {
    phLogSimMessage(logId, PHLOG_TYPE_ERROR,
                    "unable to interpret \"%s\" with sscanf expected format \"srqmask n\"",msg);
  }

  return;
}




/*****************************************************************************
 * End of file
 *****************************************************************************/
