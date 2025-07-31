/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : YACStatus.C
 * CREATED  : 29 Dec 2006
 *
 * CONTENTS : Helper classes for prober handler simulators
 *
 * AUTHORS  : Kun Xiao, STS R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Dec 2006, Kun Xiao , created
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

#include "YACStatus.h"
#include "eventObserver.h"

/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

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


/*--- YACStatus ------------------------------------------------------*/

/*--- Constructor -----------------------------------------------------------*/

YACStatus::YACStatus(phLogId_t l) : LogObject(l),
    pEventRecorder(NULL),
    pRecordObserver(NULL)
{
    //
    // NOTE: Status bits defined must follow enumeration 
    // enum eYACStatus defined in  SEstatus.h
    //

    StatusBitArray[stopped]    =  StatusBit("stopped", false, 1);
    StatusBitArray[alarm]      =  StatusBit("alarm"  , false, 2);
    StatusBitArray[errorTx]    =  StatusBit("errorTx", false, 3);
    StatusBitArray[empty]      =  StatusBit("empty"  , false, 0);
}

/*--- YACStatus public member functions ------------------------------*/

bool YACStatus::setStatusBit(
        YACStatusObserver *obv /* observer object */,
        eYACStatus                ebit /* enum status bit to set */, 
        bool                     b    /* new value of bit */)
{
    bool hasChanged;
    StatusBit &bit=StatusBitArray[ebit];

    hasChanged = ( b!=bit.getBit() );

    if ( hasChanged )
    {
        bit.setBit(b);

        Std::list<YACStatusObserver*>::iterator i;

        for ( i=observerList.begin() ; i != observerList.end() ; ++i )
        {
            if ( *i != obv )
            {
                (*i)->YACStatusChanged(ebit,b);
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

bool YACStatus::getStatusBit(eYACStatus ebit)
{
    return StatusBitArray[ebit].getBit();
}

int YACStatus::getStatusAsInt()
{
    return StatusBitArray[alarm].getBitAsInt() +
           StatusBitArray[errorTx].getBitAsInt();
}

bool YACStatus::getHandlerIsReady()
{
    return !getStatusBit(empty);
}

void YACStatus::logStatus()
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

void YACStatus::registerObserver(YACStatusObserver *observer)
{
    observerList.push_back(observer);
}

void YACStatus::registerRecorder(EventRecorder *rec, YACStatusObserver *obv)
{
    pEventRecorder=rec;
    pRecordObserver=obv;
}



/*--- YACStatus private member functions -----------------------------*/
YACStatus::eYACStatus YACStatus::getEnumStatus(const char* statusName)
{
    bool found=false;
    eYACStatus estatus = stopped;

    typedef Std::map<eYACStatus,StatusBit> statusType;
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
