/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : AdvantestStatus.C
 * CREATED  : 29 Apri 2014
 *
 * CONTENTS : Helper classes for prober handler simulators
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
#include <string.h>
#if __GNUC__ >= 3
#include <iostream>
#endif

/*--- module includes -------------------------------------------------------*/

#include "AdvantestStatus.h"
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


/*--- AdvantestStatus ------------------------------------------------------*/

/*--- Constructor -----------------------------------------------------------*/

AdvantestStatus::AdvantestStatus(phLogId_t l) : LogObject(l),
    pEventRecorder(NULL),
    pRecordObserver(NULL)
{
    //
    // NOTE: Status bits defined must follow enumeration 
    // enum eAdvantestStatus defined in  AdvantestStatus.h
    //

    StatusBitArray[stopped]    =  StatusBit("stopped", false, 1);
    StatusBitArray[alarm]      =  StatusBit("alarm"  , false, 2);
    StatusBitArray[errorTx]    =  StatusBit("errorTx", false, 3);
    StatusBitArray[empty]      =  StatusBit("empty"  , false, 0);
}

/*--- AdvantestStatus public member functions ------------------------------*/



bool AdvantestStatus::setStatusBit(
        AdvantestStatusObserver *obv /* observer object */,
        eAdvantestStatus                ebit /* enum status bit to set */, 
        bool                     b    /* new value of bit */)
{
    bool hasChanged;
    StatusBit &bit=StatusBitArray[ebit];

    hasChanged = ( b!=bit.getBit() );

    if ( hasChanged )
    {
        bit.setBit(b);

        Std::list<AdvantestStatusObserver*>::iterator i;

        for ( i=observerList.begin() ; i != observerList.end() ; ++i )
        {
            if ( *i != obv )
            { 
                (*i)->AdvantestStatusChanged(ebit,b);
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

bool AdvantestStatus::getStatusBit(eAdvantestStatus ebit)
{
    return StatusBitArray[ebit].getBit();
}

int AdvantestStatus::getStatusAsInt()
{
    return StatusBitArray[alarm].getBitAsInt() +
           StatusBitArray[errorTx].getBitAsInt();
}

bool AdvantestStatus::getHandlerIsReady()
{
    return !getStatusBit(empty);
}

void AdvantestStatus::logStatus()
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

    if ( getStatusBit(empty) )
    {
        strcat(message,"handler empty, ");
    }

    if ( getStatusBit(alarm) )
    {
        strcat(message,"alarm, ");
    }

    if ( getStatusBit(errorTx) )
    {
        strcat(message,"error in transmission, ");
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

void AdvantestStatus::registerObserver(AdvantestStatusObserver *observer)
{
    observerList.push_back(observer);
}

void AdvantestStatus::registerRecorder(EventRecorder *rec, AdvantestStatusObserver *obv)
{
    pEventRecorder=rec;
    pRecordObserver=obv;
}



/*--- AdvantestStatus private member functions -----------------------------*/
AdvantestStatus::eAdvantestStatus AdvantestStatus::getEnumStatus(const char* statusName)
{
    bool found=false;
    eAdvantestStatus estatus = empty;

    typedef Std::map<eAdvantestStatus,StatusBit> statusType;
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
