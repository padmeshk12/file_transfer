/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : Status.C
 * CREATED  : 25 March 2008
 *
 * CONTENTS : Handler Status
 *
 * AUTHORS  : Xiaofei Han, STS-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 25 March 2008, Xiaofei Han , created
 *            
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

#include "status.h"
#include "eventObserver.h"

/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/

/*--- class implementations -------------------------------------------------*/

/*--- HandlerStatusBit ------------------------------------------------------*/

/*--- HandlerStatusBit Constructor ------------------------------------------*/

template<class T> HandlerStatusBit<T>::HandlerStatusBit() : 
    name("none"), 
    value(false), 
    position(No_position)
{
}

template<class T> HandlerStatusBit<T>::HandlerStatusBit(
    T e, const char* n, bool v, int p) :
        enumValue(e),
        name(n), 
        value(v), 
        position(p)
{
}

template<class T> HandlerStatusBit<T>::HandlerStatusBit(T e) : 
    enumValue(e),
    name("none"), 
    value(false), 
    position(No_position)
{
}

/*--- HandlerStatusBit public member functions ------------------------------*/



template<class T> void HandlerStatusBit<T>::setHandlerStatusBit(
    T e, const char* n, bool b, int p)
{
    enumValue=e;
    name=n;
    value=b;
    position=p;
}

template<class T> void HandlerStatusBit<T>::setBit(bool b)
{
    value=b;
}

template<class T> T HandlerStatusBit<T>::getEnum() const
{
    return enumValue;
}

template<class T> const char *HandlerStatusBit<T>::getName() const
{
    return name;
}

template<class T> bool HandlerStatusBit<T>::getBit() const
{
    return value;
}

template<class T> int HandlerStatusBit<T>::getBitAsInt() const
{
    return (position==No_position) ? 0 : value*(1<<position);
}



/*--- Status ----------------------------------------------------------------*/

/*--- Status Constructor ----------------------------------------------------*/

template<class T> Status<T>::Status(phLogId_t l) : LogObject(l),
    pEventRecorder(NULL),
    pRecordObserver(NULL)
{
//    //
//    // NOTE: Status bits defined must follow enumeration 
//    // enum eSEStatus defined in  SEstatus.h
//    //
//
//    statusBitList[stopped]    =  StatusBit("stopped", false, 1);
//    statusBitList[alarm]      =  StatusBit("alarm"  , false, 2);
//    statusBitList[errorTx]    =  StatusBit("errorTx", false, 3);
//    statusBitList[empty]      =  StatusBit("empty"  , false, 0);
}


/*--- Status public member functions ----------------------------------------*/

template<class T> void Status<T>::setStatusList(
        HandlerStatusBit<T>     *pStatusBit /* pointer to handler status bit 
                                               to add to list */)
{
    statusBitList.push_back(pStatusBit);
}


template<class T> bool Status<T>::setStatusBit(
        StatusObserver<T>        *obv /* observer object */,
        T                        ebit /* enum status bit to set */, 
        bool                     b    /* new value of bit */)
{
    bool hasChanged;

    HandlerStatusBit<T> &bit=getHandlerStatusBit(ebit);

    hasChanged = ( b!=bit.getBit() );

    if ( hasChanged )
    {
        bit.setBit(b);

        typename Std::list<StatusObserver<T>*>::iterator i;

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

template<class T> bool Status<T>::getStatusBit(T ebit)
{
    HandlerStatusBit<T> &bit=getHandlerStatusBit(ebit);

    return bit.getBit();
}

template<class T> int Status<T>::getStatusAsInt()
{
    int statusInt=0;

    typedef Std::list<HandlerStatusBit<T>*> statusType;
    typename statusType::iterator i;

    for ( i = statusBitList.begin() ; i != statusBitList.end() ; i++ )
    {
        statusInt += (*i)->getBitAsInt();
    }

    return statusInt;
}

template<class T> void Status<T>::logStatus()
{
    char message[256];

    strcpy(message,"Status: ");

    typedef Std::list<HandlerStatusBit<T>*> statusType;
    typename statusType::iterator i;

    for ( i = statusBitList.begin() ; i != statusBitList.end() ; i++ )
    {
        if ( (*i).getBit() )
        {
            strcat(message,(*i).getName());
        }
        strcat(message," ");
    }

    phLogSimMessage(logId, LOG_DEBUG, "%s",message);
}

template<class T> void Status<T>::registerObserver(StatusObserver<T> *observer)
{
    observerList.push_back(observer);
}

template<class T> void Status<T>::registerRecorder(EventRecorder *rec, StatusObserver<T> *obv)
{
    pEventRecorder=rec;
    pRecordObserver=obv;
}

/*--- Status private member functions ---------------------------------------*/
template<class T> T Status<T>::getEnumStatus(const char* statusName)
{
    bool found=false;
    T estatus;

    typedef Std::list<HandlerStatusBit<T>*> statusType;
    typename statusType::iterator i;

    for ( i = statusBitList.begin() ; i != statusBitList.end() && !found ; i++ )
    {
        if ( strcmp(statusName,(*i)->getName()) == 0 )
        {
            found=true;
            estatus=(*i)->getEnum();
        }
    }

    if ( !found )
    {
        Std::string fStr="Unable to convert \"";
        fStr+=statusName;
        fStr+="\" to status enumeration";
        throw FormatException(fStr.c_str());
    }

    return estatus;
}


/*--- Status private member functions ---------------------------------------*/
template<class T> HandlerStatusBit<T>& Status<T>::getHandlerStatusBit(T e)
{
    typedef Std::list<HandlerStatusBit<T>*> statusType;
    typename statusType::iterator i;
    HandlerStatusBit<T> *pHandSB;

    for ( i = statusBitList.begin() ; i != statusBitList.end() ; i++ )
    {
        if ( (*i)->getEnum()  == e )
        {
            pHandSB= (*i);
        }
    }

    if ( i == statusBitList.end() )
    {
        throw FormatException("unknown enum");
    }

    return *pHandSB;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
