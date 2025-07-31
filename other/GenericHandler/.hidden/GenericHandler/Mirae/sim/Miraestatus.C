/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : Miraestatus.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Helper classes for prober handler simulators
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Ken Ward, BitsoftSystems, Mirae revision
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

#include "Miraestatus.h"
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


/*--- MiraeStatus ------------------------------------------------------*/

/*--- Constructor -----------------------------------------------------------*/

MiraeStatus::MiraeStatus(phLogId_t l) : LogObject(l),
    pEventRecorder(NULL),
    pRecordObserver(NULL)
{
    //
    // NOTE: Status bits defined must follow enumeration 
    // enum eMiraeStatus defined in  Miraestatus.h
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

MiraeStatus::~MiraeStatus()
{
}

/*--- MiraeStatus public member functions ------------------------------*/

bool MiraeStatus::setStatusBit(
        MiraeStatusObserver *obv /* observer object */,
        eMiraeStatus                ebit /* enum status bit to set */, 
        bool                     b    /* new value of bit */)
{
    bool hasChanged;
    StatusBit &bit=StatusBitArray[ebit];

    hasChanged = ( b!=bit.getBit() );

    if ( hasChanged )
    {
        bit.setBit(b);

        Std::list<MiraeStatusObserver*>::iterator i;

        for ( i=observerList.begin() ; i != observerList.end() ; ++i )
        {
            if ( *i != obv )
            {
                (*i)->MiraeStatusChanged(ebit,b);
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

bool MiraeStatus::getStatusBit(eMiraeStatus ebit)
{
    return StatusBitArray[ebit].getBit();
}

int MiraeStatus::getStatusAsInt()
{
    return StatusBitArray[high_alarm].getBitAsInt();
}

bool MiraeStatus::getHandlerIsReady()
{
    return !( getStatusBit(handler_empty) || getStatusBit(low_alarm) || getStatusBit(high_alarm));
}

void MiraeStatus::logStatus()
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

void MiraeStatus::registerObserver(MiraeStatusObserver *observer)
{
    observerList.push_back(observer);
}

void MiraeStatus::registerRecorder(EventRecorder *rec, MiraeStatusObserver *obv)
{
    pEventRecorder=rec;
    pRecordObserver=obv;
}



/*--- MiraeStatus private member functions -----------------------------*/
MiraeStatus::eMiraeStatus MiraeStatus::getEnumStatus(const char* statusName)
{
    bool found=false;
    eMiraeStatus estatus = waiting_tester_response;

    typedef Std::map<eMiraeStatus,StatusBit> statusType;
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
