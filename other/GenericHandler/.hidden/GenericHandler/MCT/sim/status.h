/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : status.h
 * CREATED  : 5 Jan 2015
 *
 * CONTENTS : status
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 5 Jan 2015, Magco Li, created
 *
 * Instructions:
 *
 * 1) Copy this template to as many .h files as you require
 *
 * 2) Use the command 'make depend' to make the new
 *    source files visible to the makefile utility
 *
 * 3) To support automatic man page (documentation) generation, follow the
 *    instructions given for the function template below.
 *
 * 4) Put private functions and other definitions into separate header
 * files. (divide interface and private definitions)
 *
 *****************************************************************************/

#ifndef _STATUS_H_
#define _STATUS_H_

/*--- system includes -------------------------------------------------------*/
#include <map>
#include <list>
#include <vector>
#include <string.h>

/*--- module includes -------------------------------------------------------*/


/*--- module includes -------------------------------------------------------*/

#include "logObject.h"
#include "simHelper.h"

/*--- defines ---------------------------------------------------------------*/



/*--- typedefs --------------------------------------------------------------*/


/*--- class definition  -----------------------------------------------------*/

/*****************************************************************************
 * To allow a consistent interface definition and documentation, the
 * documentation is automatically extracted from the comment section
 * of the below function declarations. All text within the comment
 * region just in front of a function header will be used for
 * documentation. Additional text, which should not be visible in the
 * documentation (like this text), must be put in a separate comment
 * region, ahead of the documentation comment and separated by at
 * least one blank line.
 *
 * To fill the documentation with life, please fill in the angle
 * bracketed text portions (<>) below. Each line ending with a colon
 * (:) or each line starting with a single word followed by a colon is
 * treated as the beginning of a separate section in the generated
 * documentation man page. Besides blank lines, there is no additional
 * format information for the resulting man page. Don't expect
 * formated text (like tables) to appear in the man page similar as it
 * looks in this header file.
 *
 * Function parameters should be commented immediately after the type
 * specification of the parameter but befor the closing bracket or
 * dividing comma characters.
 *
 * To use the automatic documentation feature, c2man must be installed
 * on your system for man page generation.
 *****************************************************************************/

/*****************************************************************************
 *
 * Handler status bit
 *
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
 *
 * Description:
 * Status bit used by handler to indicate how handler is functioning and
 * any error conditions
 *
 *****************************************************************************/
template<class T> class HandlerStatusBit
{
public:
    enum { No_position = -1 };
    HandlerStatusBit();
    HandlerStatusBit(T e);
    HandlerStatusBit(T e, const char* n, bool b, int p, bool v);
    void setHandlerStatusBit(T e, const char* n, bool b, int p, bool v);
    void setBit(bool b);
    T getEnum() const;
    const char *getName() const;
    bool getBit() const;
    int getBitAsInt() const;
    bool getVisible() const;
private:
    T           enumValue;
    const char* name;
    bool        value;
    int         position;
    bool        visible;
};


/*****************************************************************************
 *
 * Status
 *
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
 *
 * Description:
 * Collection of status bits
 *
 *****************************************************************************/

template<class T> class EventRecorder;
template<class T> class StatusObserver;
template<class T> class StatusPtr;

template<class T> class Status : public LogObject
{
public:
    friend class StatusPtr<T>;

    // public member functions
    Status(phLogId_t l);
    ~Status();
    void setStatusList(HandlerStatusBit<T>*);
    bool setStatusBit(StatusObserver<T>*, T, bool b);
    bool getStatusBit(T) const;
    const char *getStatusName(T) const;
    bool getStatusVisible(T) const;
    int getStatusAsInt() const;
    int getNumStatusLevels() const;
    void logStatus() const;
    void registerObserver(StatusObserver<T>*);
    void registerRecorder(EventRecorder<T>*, StatusObserver<T>*);
    T getEnumStatus(const char* statusName) const;
    StatusPtr<T> getStatusPtr();
    void selfTest() const;
private:
    StatusPtr<T> *pSPtr;

    HandlerStatusBit<T>& getHandlerStatusBit(T e);
    const HandlerStatusBit<T>& getHandlerStatusBit(T e) const;

    Std::list<StatusObserver<T>*>          observerList;
    Std::vector<HandlerStatusBit<T>*>      statusBitVector;
    EventRecorder<T>                  *pEventRecorder;
    StatusObserver<T>                 *pRecordObserver;
};


/*****************************************************************************
 *
 * Status pointer
 *
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
 *
 * Description:
 *
 *****************************************************************************/

template<class T> class StatusPtr
{
public:
    typedef Std::vector<HandlerStatusBit<T>*> statusType;
    friend class Status<T>;

    StatusPtr(const StatusPtr&);
    StatusPtr& operator=(const StatusPtr&);
    ~StatusPtr();

    T operator*();
    StatusPtr& operator++();
    void begin();
    bool atEnd();
private:
    StatusPtr(Status<T>*);
    Status<T> *pStatus;
    typename statusType::const_iterator statusIterator;
};



/*****************************************************************************
 *
 * Status observer
 *
 * Authors: Magco Li
 *
 * History: 5 Jan 2015, Magco Li , created
 *
 * Description:
 *
 *****************************************************************************/


template<class T> class StatusObserver
{
public:
    virtual void StatusChanged(T, bool b)=0;
};



/*--- template class implementations ----------------------------------------*/

/*--- HandlerStatusBit ------------------------------------------------------*/

/*--- HandlerStatusBit Constructor ------------------------------------------*/

template<class T> HandlerStatusBit<T>::HandlerStatusBit() :
    name("none"),
    value(false),
    position(No_position),
    visible(false)
{
}

template<class T> HandlerStatusBit<T>::HandlerStatusBit(
    T e, const char* n, bool b, int p, bool v) :
        enumValue(e),
        name(n),
        value(b),
        position(p),
        visible(v)
{
}

template<class T> HandlerStatusBit<T>::HandlerStatusBit(T e) :
    enumValue(e),
    name("none"),
    value(false),
    position(No_position),
    visible(false)
{
}

/*--- HandlerStatusBit public member functions ------------------------------*/

template<class T> void HandlerStatusBit<T>::setHandlerStatusBit(
    T e, const char* n, bool b, int p, bool v)
{
    enumValue=e;
    name=n;
    value=b;
    position=p;
    visible=v;
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

template<class T> bool HandlerStatusBit<T>::getVisible() const
{
    return visible;
}


/*--- Status ----------------------------------------------------------------*/

/*--- Status Constructor ----------------------------------------------------*/

template<class T> Status<T>::Status(phLogId_t l) : LogObject(l),
    pEventRecorder(NULL),
    pRecordObserver(NULL)
{
    pSPtr = new StatusPtr<T>(this);
}

template<class T> Status<T>::~Status()
{
    if (pSPtr != NULL)
    {
        delete pSPtr;
        pSPtr = NULL;
    }
}

/*--- Status public member functions ----------------------------------------*/

template<class T> void Status<T>::setStatusList(
        HandlerStatusBit<T>     *pStatusBit /* pointer to handler status bit
                                               to add to list */)
{
    statusBitVector.push_back(pStatusBit);
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
                (*i)->StatusChanged(ebit,b);
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

template<class T> bool Status<T>::getStatusBit(T ebit) const
{
    const HandlerStatusBit<T> &bit=getHandlerStatusBit(ebit);

    return bit.getBit();
}

template<class T> const char *Status<T>::getStatusName(T ebit) const
{
    const HandlerStatusBit<T> &bit=getHandlerStatusBit(ebit);

    return bit.getName();
}

template<class T> bool Status<T>::getStatusVisible(T ebit) const
{
    const HandlerStatusBit<T> &bit=getHandlerStatusBit(ebit);

    return bit.getVisible();
}

template<class T> int Status<T>::getStatusAsInt() const
{
    int statusInt=0;

    typedef Std::vector<HandlerStatusBit<T>*> statusType;
    typename statusType::iterator i;

    for ( i = statusBitVector.begin() ; i != statusBitVector.end() ; i++ )
    {
        statusInt += (*i)->getBitAsInt();
    }

    return statusInt;
}


template<class T> int Status<T>::getNumStatusLevels() const
{
    return statusBitVector.end() - statusBitVector.begin();
}

template<class T> void Status<T>::logStatus() const
{
    char message[256];

    strcpy(message,"Status: ");

    typedef Std::vector<HandlerStatusBit<T>*> statusType;
    typename statusType::const_iterator i;

    for ( i = statusBitVector.begin() ; i != statusBitVector.end() ; i++ )
    {
        strcat(message,(*i)->getName());
        strcat(message," ");
        strcat(message, ((*i)->getBit()) ? "True" : "False");
        strcat(message," ");
    }

    phLogSimMessage(logId, LOG_DEBUG, "%s",message);
}

template<class T> void Status<T>::registerObserver(StatusObserver<T> *observer)
{
    observerList.push_back(observer);
}

template<class T> void Status<T>::registerRecorder(EventRecorder<T> *rec, StatusObserver<T> *obv)
{
    pEventRecorder=rec;
    pRecordObserver=obv;
}

template<class T> T Status<T>::getEnumStatus(const char* statusName) const
{
    bool found=false;
    T estatus = (T)0;

    typedef Std::vector<HandlerStatusBit<T>*> statusType;
    typename statusType::const_iterator i;

    for ( i = statusBitVector.begin() ; i != statusBitVector.end() && !found ; i++ )
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

template<class T> StatusPtr<T> Status<T>::getStatusPtr()
{
    return *pSPtr;
}

template<class T> void Status<T>::selfTest() const
{
    typedef Std::vector<HandlerStatusBit<T>*> statusType;
    typename statusType::const_iterator i;

    phLogSimMessage(logId, LOG_VERBOSE, "Status::selfTest ");
    for ( i = statusBitVector.begin() ; i != statusBitVector.end() ; i++ )
    {
        phLogSimMessage(logId, LOG_VERBOSE, "%s enum(%d) bit(%s) int(%d) visible(%s) ",
                        (*i)->getName(),
                        int((*i)->getEnum()),
                        ((*i)->getBit()) ? "True" : "False",
                        (*i)->getBitAsInt(),
                        ((*i)->getVisible()) ? "True" : "False");
    }
    phLogSimMessage(logId, LOG_VERBOSE, "Status::selfTest PASS");
}



/*--- Status private member functions ---------------------------------------*/
template<class T> HandlerStatusBit<T>& Status<T>::getHandlerStatusBit(T e)
{
    typedef Std::vector<HandlerStatusBit<T>*> statusType;
    typename statusType::iterator i;
    HandlerStatusBit<T> *pHandSB=0;

    for ( i = statusBitVector.begin() ; i!=statusBitVector.end() && !pHandSB ; i++ )
    {
        if ( (*i)->getEnum()  == e )
        {
            pHandSB= (*i);
        }
    }

    if ( !pHandSB )
    {
        throw FormatException("unknown enum");
    }

    return *pHandSB;
}

template<class T> const HandlerStatusBit<T>& Status<T>::getHandlerStatusBit(T e) const
{
    typedef Std::vector<HandlerStatusBit<T>*> statusType;
    typename statusType::const_iterator i;
    const HandlerStatusBit<T> *pHandSB=0;

    for ( i = statusBitVector.begin() ; i != statusBitVector.end() && !pHandSB ; i++ )
    {
        if ( (*i)->getEnum()  == e )
        {
            pHandSB= (*i);
        }
    }

    if ( !pHandSB )
    {
        throw FormatException("unknown enum");
    }

    return *pHandSB;
}


/*--- StatusPtr -------------------------------------------------------------*/

/*--- private StatusPtr Constructor -----------------------------------------*/

template<class T> StatusPtr<T>::StatusPtr(Status<T>* pS) : pStatus(pS)
{
    statusIterator=pStatus->statusBitVector.begin();
}

// copy constructor
template<class T> StatusPtr<T>::StatusPtr(const StatusPtr& s)
{
    pStatus=s.pStatus;
    statusIterator=s.statusIterator;
}

// assignment
template<class T> StatusPtr<T>& StatusPtr<T>::operator=(const StatusPtr& s)
{
    // beware of self-assignment: s=s

    if ( this != &s )
    {
        pStatus=s.pStatus;
        statusIterator=s.statusIterator;
    }

    return *this;
}

// destructor
template<class T> StatusPtr<T>::~StatusPtr()
{
    pStatus=0;
}


/*--- StatusPtr public member functions --------------------------------------*/

template<class T> T StatusPtr<T>::operator*()
{
    return (*statusIterator)->getEnum();
}

template<class T> StatusPtr<T>& StatusPtr<T>::operator++()
{
    if ( statusIterator != pStatus->statusBitVector.end() )
    {
        ++statusIterator;
    }

    return *this;
}

template<class T> void StatusPtr<T>::begin()
{
    statusIterator=pStatus->statusBitVector.begin();
}

template<class T> bool StatusPtr<T>::atEnd()
{
    return (statusIterator==pStatus->statusBitVector.end());
}



#endif /* ! _STATUS_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
