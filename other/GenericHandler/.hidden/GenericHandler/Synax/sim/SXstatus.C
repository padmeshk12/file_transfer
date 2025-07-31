/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SXstatus.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Helper classes for prober handler simulators
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce , created
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
#include <string.h>
#if __GNUC__ >= 3
  #include <iostream>
#endif

/*--- module includes -------------------------------------------------------*/

#include "SXstatus.h"
#include "eventObserver.h"
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


/*--- StatusStringToEnumConversionException ---------------------------------*/

StatusStringToEnumConversionException::StatusStringToEnumConversionException(const char *errStr) :
convertStringError(errStr)
{
}

void StatusStringToEnumConversionException::printMessage()
{
  Std::cerr << "Unable to convert \"" << convertStringError << "\" into an enum type ";
}


/*--- SynaxStatus ------------------------------------------------------*/

/*--- Constructor -----------------------------------------------------------*/

SynaxStatus::SynaxStatus(phLogId_t l) : LogObject(l),
pEventRecorder(NULL),
pRecordObserver(NULL)
{
  //
  // NOTE: Status bits defined must follow enumeration 
  // enum eSXStatus defined in  SXstatus.h
  //

  StatusBitArray[stopped]    =  StatusBit("stopped", false, 1);
  StatusBitArray[alarm]      =  StatusBit("alarm"  , false, 2);
  StatusBitArray[errorTx]    =  StatusBit("errorTx", false, 3);
  StatusBitArray[empty]      =  StatusBit("empty"  , false, 0);
}

/*--- SynaxStatus public member functions ------------------------------*/

bool SynaxStatus::setStatusBit(
                              SynaxStatusObserver *obv /* observer object */,
                              eSXStatus                ebit /* enum status bit to set */, 
                              bool                     b    /* new value of bit */)
{
  bool hasChanged;
  StatusBit &bit=StatusBitArray[ebit];

  hasChanged = ( b!=bit.getBit() );

  if( hasChanged ) {
    bit.setBit(b);

    Std::list<SynaxStatusObserver*>::iterator i;

    for( i=observerList.begin() ; i != observerList.end() ; ++i ) {
      if( *i != obv ) {
        (*i)->SXStatusChanged(ebit,b);
      }
    }

    if( pEventRecorder && pRecordObserver ) {
      if( obv == pRecordObserver ) {
        pEventRecorder->recordChangedStatus(bit.getName(), b);
      }
    }
  }
  return hasChanged;
}

bool SynaxStatus::getStatusBit(eSXStatus ebit)
{
  return StatusBitArray[ebit].getBit();
}

int SynaxStatus::getStatusAsInt()
{
  return StatusBitArray[alarm].getBitAsInt() +
  StatusBitArray[errorTx].getBitAsInt();
}

bool SynaxStatus::getHandlerIsReady()
{
  return !getStatusBit(empty);
}

void SynaxStatus::logStatus()
{
  char message[256];

  strcpy(message,"Status: ");

  if( getHandlerIsReady() ) {
    strcat(message,"ready, ");
  } else {
    strcat(message,"not ready, ");
  }

  if( getStatusBit(empty) ) {
    strcat(message,"handler empty, ");
  }

  if( getStatusBit(alarm) ) {
    strcat(message,"alarm, ");
  }

  if( getStatusBit(errorTx) ) {
    strcat(message,"error in transmission, ");
  }

  if( getStatusBit(stopped) ) {
    strcat(message,"stopped ");
  } else {
    strcat(message,"running ");
  }

  phLogSimMessage(logId, LOG_DEBUG, "%s",message);

}

void SynaxStatus::registerObserver(SynaxStatusObserver *observer)
{
  observerList.push_back(observer);
}

void SynaxStatus::registerRecorder(EventRecorder *rec, SynaxStatusObserver *obv)
{
  pEventRecorder=rec;
  pRecordObserver=obv;
}



/*--- SynaxStatus private member functions -----------------------------*/
SynaxStatus::eSXStatus SynaxStatus::getEnumStatus(const char* statusName)
{
  bool found=false;
  eSXStatus estatus = stopped;

  typedef Std::map<eSXStatus,StatusBit> statusType;
  statusType::iterator i;

  for( i = StatusBitArray.begin() ; i != StatusBitArray.end() && !found ; i++ ) {
    if( strcmp(statusName,((*i).second).getName()) == 0 ) {
      found=true;
      estatus=(*i).first;
    }
  }

  if( !found ) {
    throw StatusStringToEnumConversionException(statusName);
  }

  return estatus;
}




/*--- Yokogawa Constructor --------------------------------------------------*/

/*--- Yokogawa --------------------------------------------------------------*/

/*--- Constructor -----------------------------------------------------------*/

// YokogawaStatus::YokogawaStatus(phLogId_t l) : Status<eYStatus>(l)
// {
// //    //
// //     // NOTE: Status bits defined must follow enumeration 
// //     // enum eYStatus defined in  SXstatus.h
// //     //
// // 
// //     StatusBitArray[y_stopped]    =  StatusBit("stopped", false, 1);
// //     StatusBitArray[y_alarm]      =  StatusBit("alarm"  , false, 2);
// //     StatusBitArray[y_errorTx]    =  StatusBit("errorTx", false, 3);
// //     StatusBitArray[y_empty]      =  StatusBit("empty"  , false, 0);
// }





/*****************************************************************************
 * End of file
 *****************************************************************************/
