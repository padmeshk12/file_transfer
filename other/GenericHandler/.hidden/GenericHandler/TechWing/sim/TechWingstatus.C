/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : TechWingstatus.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Helper classes for prober handler simulators
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Ken Ward, BitsoftSystems, TechWing revision
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

#include "TechWingstatus.h"
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

StatusStringToEnumConversionException::~StatusStringToEnumConversionException()
{
}

void StatusStringToEnumConversionException::printMessage()
{
    Std::cerr << "Unable to convert \"" << convertStringError << "\" into an enum type ";
}


/*--- TechWingStatus ------------------------------------------------------*/

/*--- Constructor -----------------------------------------------------------*/

TechWingStatus::TechWingStatus(phLogId_t l) : LogObject(l),
    pEventRecorder(NULL),
    pRecordObserver(NULL)
{
    //
    // NOTE: Status bits defined must follow enumeration 
    // enum eTechWingStatus defined in  TechWingstatus.h
    //

    StatusBitArray[controlling_remote]              = StatusBit("", false, 12);
    StatusBitArray[input_empty]                     = StatusBit("", false, 9);
    StatusBitArray[handler_empty]                   = StatusBit("", false, 8);
    StatusBitArray[wait_for_soak]                   = StatusBit("", false, 7);
    StatusBitArray[out_of_temp]                     = StatusBit("", false, 6);
    StatusBitArray[temp_stable]                     = StatusBit("", false, 5);
    StatusBitArray[temp_alarm]                      = StatusBit("", false, 4);
    StatusBitArray[low_alarm]                       = StatusBit("", false, 3);
    StatusBitArray[high_alarm]                      = StatusBit("", false, 2);
    StatusBitArray[stopped]                         = StatusBit("", false, 1);
    StatusBitArray[waiting_tester_response]         = StatusBit("", false, 0);

}

TechWingStatus::~TechWingStatus()
{
}

/*--- TechWingStatus public member functions ------------------------------*/

bool TechWingStatus::setStatusBit(
        TechWingStatusObserver *obv /* observer object */,
        eTechWingStatus                ebit /* enum status bit to set */, 
        bool                     b    /* new value of bit */)
{
    bool hasChanged;
    StatusBit &bit=StatusBitArray[ebit];

    hasChanged = ( b!=bit.getBit() );

    if ( hasChanged )
    {
        bit.setBit(b);

        Std::list<TechWingStatusObserver*>::iterator i;

        for ( i=observerList.begin() ; i != observerList.end() ; ++i )
        {
            if ( *i != obv )
            {
                (*i)->TechWingStatusChanged(ebit,b);
            }
        }

        if ( pEventRecorder && pRecordObserver )
        {
            if ( obv == pRecordObserver )
            {
                pEventRecorder->recordChangedStatus(bit.getName(), b);
            }
        }
    }
    return hasChanged;
}

bool TechWingStatus::getStatusBit(eTechWingStatus ebit)
{
    return StatusBitArray[ebit].getBit();
}

int TechWingStatus::getStatusAsInt()
{
    return StatusBitArray[high_alarm].getBitAsInt();
}

bool TechWingStatus::getHandlerIsReady()
{
    return !( getStatusBit(handler_empty) || getStatusBit(low_alarm) || getStatusBit(high_alarm));
}

void TechWingStatus::logStatus()
{
    char message[256];

    strcpy(message,"Status: ");

    if ( getHandlerIsReady() )
    {
        strcat(message,"ready, ");
    }
    else
    {
        strcat(message,"not ready, ");
    }

    if ( getStatusBit(stopped) )
    {
        strcat(message,"stopped ");
    }

    else
    {
        strcat(message,"running ");
    }

    phLogSimMessage(logId, LOG_DEBUG, "%s",message);

}

void TechWingStatus::registerObserver(TechWingStatusObserver *observer)
{
    observerList.push_back(observer);
}

void TechWingStatus::registerRecorder(EventRecorder *rec, TechWingStatusObserver *obv)
{
    pEventRecorder=rec;
    pRecordObserver=obv;
}



/*--- TechWingStatus private member functions -----------------------------*/
TechWingStatus::eTechWingStatus TechWingStatus::getEnumStatus(const char* statusName)
{
    bool found=false;
    eTechWingStatus estatus = waiting_tester_response; 

    typedef Std::map<eTechWingStatus,StatusBit> statusType;
    statusType::iterator i;

    for ( i = StatusBitArray.begin() ; i != StatusBitArray.end() && !found ; i++ )
    {
        if ( strcmp(statusName,((*i).second).getName()) == 0 )
        {
            found=true;
            estatus=(*i).first;
        }
    }

    if ( !found )
    {
        throw StatusStringToEnumConversionException(statusName);
    }

    return estatus;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
