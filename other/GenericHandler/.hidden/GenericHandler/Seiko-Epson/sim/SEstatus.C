/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : SEstatus.C
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

#include "SEstatus.h"
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


/*--- SeikoEpsonStatus ------------------------------------------------------*/

/*--- Constructor -----------------------------------------------------------*/

SeikoEpsonStatus::SeikoEpsonStatus(phLogId_t l) : LogObject(l),
    pEventRecorder(NULL),
    pRecordObserver(NULL)
{
    //
    // NOTE: Status bits defined must follow enumeration 
    // enum eSEStatus defined in  SEstatus.h
    //

    StatusBitArray[stopped]    =  StatusBit("stopped", false, 1);
    StatusBitArray[alarm]      =  StatusBit("alarm"  , false, 2);
    StatusBitArray[errorTx]    =  StatusBit("errorTx", false, 3);
    StatusBitArray[empty]      =  StatusBit("empty"  , false, 0);
}

/*--- SeikoEpsonStatus public member functions ------------------------------*/

bool SeikoEpsonStatus::setStatusBit(
        SeikoEpsonStatusObserver *obv /* observer object */,
        eSEStatus                ebit /* enum status bit to set */, 
        bool                     b    /* new value of bit */)
{
    bool hasChanged;
    StatusBit &bit=StatusBitArray[ebit];

    hasChanged = ( b!=bit.getBit() );

    if ( hasChanged )
    {
        bit.setBit(b);

        Std::list<SeikoEpsonStatusObserver*>::iterator i;

        for ( i=observerList.begin() ; i != observerList.end() ; ++i )
        {
            if ( *i != obv )
            {
                (*i)->SEStatusChanged(ebit,b);
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

bool SeikoEpsonStatus::getStatusBit(eSEStatus ebit)
{
    return StatusBitArray[ebit].getBit();
}

int SeikoEpsonStatus::getStatusAsInt()
{
    return StatusBitArray[alarm].getBitAsInt() +
           StatusBitArray[errorTx].getBitAsInt();
}

bool SeikoEpsonStatus::getHandlerIsReady()
{
    return !getStatusBit(empty);
}

void SeikoEpsonStatus::logStatus()
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

void SeikoEpsonStatus::registerObserver(SeikoEpsonStatusObserver *observer)
{
    observerList.push_back(observer);
}

void SeikoEpsonStatus::registerRecorder(EventRecorder *rec, SeikoEpsonStatusObserver *obv)
{
    pEventRecorder=rec;
    pRecordObserver=obv;
}



/*--- SeikoEpsonStatus private member functions -----------------------------*/
SeikoEpsonStatus::eSEStatus SeikoEpsonStatus::getEnumStatus(const char* statusName)
{
    bool found=false;
    eSEStatus estatus = empty;

    typedef Std::map<eSEStatus,StatusBit> statusType;
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
