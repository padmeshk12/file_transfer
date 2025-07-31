/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : status.C
 * CREATED  : 18 Jan 2001
 *
 * CONTENTS : handler status
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce, created
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

/*--- module includes -------------------------------------------------------*/

#include "status.h"
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
/*--- class implementations -------------------------------------------------*/

/*****************************************************************************
 *
 * HandlerStatusBit
 *
 *
 *
 *****************************************************************************/

/*--- HandlerStatusBit constructor ------------------------------------------*/
HandlerStatusBit::HandlerStatusBit() : 
    identity(No_identity),
    value(false), 
    position(No_position),
    name(""), 
    key(0)
{
}

HandlerStatusBit::HandlerStatusBit(int id, bool v, int p, const char* n) :
    identity(id),
    value(v), 
    position(p),
    name(n), 
    key(0) 
{
}

HandlerStatusBit::HandlerStatusBit(const char* k) :
    identity(No_identity),
    value(false), 
    position(No_position),
    name("")
{
    key = new char[strlen(k) + 1];
    strcpy(key, k);
}

/*--- HandlerStatusBit destructor -------------------------------------------*/
HandlerStatusBit::~HandlerStatusBit()
{
//  BUG FIX: destructor called for reference to HandlerStatusBit in 
//           HandlerStatus::getHandlerStatusBit()
    if ( key )
        delete[] key;

}

/*--- HandlerStatusBit public member functions ------------------------------*/
void HandlerStatusBit::setHandlerStatusBit(int id, bool v, int p, const char* n)
{
    identity=id;
    value=v;
    position=p;
    name=n;
    key=0;
}

void HandlerStatusBit::setBit(bool b)
{
    value=b;
}

long HandlerStatusBit::getBitAsLong() const
{
    return (position==No_position) ? 0 : value*(1<<position);
}

int HandlerStatusBit::getId() const
{
    return identity;
}

bool HandlerStatusBit::getBit() const
{
    return value;
}

const char *HandlerStatusBit::getName() const
{
    return name;
}

const char *HandlerStatusBit::getKey() const
{
    if ( !key )
    {
        const char *key_prefix = "";

        if ( *name )
        {
            key_prefix = "t_";
        }
        key= new char[ strlen(key_prefix) + strlen(name) + 1 ];
        strcpy(key,key_prefix);
        strcat(key,name);
        replaceWhiteSpaceInString(key, '_');
    }

    return key;
}

bool HandlerStatusBit::operator==(const HandlerStatusBit& sb) const
{
    bool equal;

    if ( identity != No_identity && sb.identity != No_identity )
    {
        equal = ( identity == sb.identity );
    }
    else
    {
        const char *key1=getKey();
        const char *key2=sb.getKey();

        if ( *key1 && *key2 )
        {
            equal = ( strcmp(key1,key2) == 0 );
        }
        else
        {
            equal=false;
        }
    }
    return equal;
}


/*****************************************************************************
 *
 * HandlerStatus
 *
 *
 *
 *
 *****************************************************************************/

/*--- HandlerStatus constructor ---------------------------------------------*/

HandlerStatus::HandlerStatus(phLogId_t l) : LogObject(l),
    pEventRecorder(0),
    pRecordObserver(0)
{
}

/*--- HandlerStatus public member functions ---------------------------------*/

/*****************************************************************************
 *
 * Set status List
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Add a HandlerStatusBit to HandlerStatus's list
 *
 *****************************************************************************/
void HandlerStatus::setHandlerStatusList(
        HandlerStatusBit     *pStatusBit /* pointer to handler status bit 
                                            to add to list */)
{
    statusBitList.push_back(pStatusBit);
}

/*****************************************************************************
 *
 * Set status Bit
 *
 * Authors: Chris Joyce
 *
 * Description:
 * A StatusObserver class has altered a HandlerStatusBit: HandlerStatus must
 * update this information and if the status bit has really been altered
 * then send the other StatusObserver(s) the updated information.
 *
 *****************************************************************************/
bool HandlerStatus::setStatusBit(
        StatusObserver            *obv /* observer object */, 
        const HandlerStatusBit    &b   /* changed status bit */) 
{
    bool hasChanged;

    HandlerStatusBit &bit=getHandlerStatusBit(b);

    hasChanged = ( b.getBit() != bit.getBit() );

    if ( hasChanged )
    {
        phLogSimMessage(logId, LOG_VERBOSE, "setStatusBit(%s, %s)",
                        bit.getName(),
                        (bit.getBit()) ? "True" : "False");

        bit.setBit( b.getBit() );

        Std::list<StatusObserver*>::iterator i;

        for ( i=observerList.begin() ; i != observerList.end() ; ++i )
        {
            if ( *i != obv )
            {
                (*i)->statusChanged(bit);
            }
        }

        if ( pEventRecorder && pRecordObserver )
        {
            if ( obv == pRecordObserver )
            {
                pEventRecorder->recordChangedStatus(bit.getName(), bit.getBit());
            }
        }
    }
    return hasChanged;
}


/*****************************************************************************
 *
 * Set status Bit using identity
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Same as bool setStatusBit(StatusObserver*, HandlerStatusBit&) but using
 * HandlerStatusBit identity
 *
 *****************************************************************************/
bool HandlerStatus::setStatusBit(
        StatusObserver            *obv /* observer object */, 
        int                       id   /* identity of HandlerStatusBit */,
        bool                      v    /* new HandlerStatusBit value */)
{
    HandlerStatusBit b(id,v);

    return setStatusBit(obv,b);
}

/*****************************************************************************
 *
 * get status Bit
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Retrieve actual HandlerStatusBit in HandlerStatus's list  from user's
 * set-up HandlerStatusBit (which may only have the key or the id value set).
 *
 *****************************************************************************/
const HandlerStatusBit& HandlerStatus::getHandlerStatusBit(
        const HandlerStatusBit    &b   /* user setup HandlerStatusBit */)  const
{
    Std::vector<HandlerStatusBit*>::const_iterator i;
    HandlerStatusBit *pHandSB=0;

    for ( i = statusBitList.begin() ; i!=statusBitList.end() && !pHandSB ; i++ )
    {
        if ( *(*i) == b )
        {
            pHandSB= *i;
        }
    }

    if ( i == statusBitList.end() && !pHandSB )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR, 
                        "HandlerStatus::getHandlerStatusBit() Unable to find HandlerStatusBit for \"%s\" \"%s\"",
                         b.getName(),
                         b.getKey());
        throw FormatException("unknown HandlerStatusBit");
    }

    return *pHandSB;
}

/*****************************************************************************
 *
 * get number of HandlerStatusBit (s) set up in HandlerStatus's list
 *
 * Authors: Chris Joyce
 *
 *****************************************************************************/
int HandlerStatus::getNumberOfHandlerStatusBits() const
{
    return statusBitList.end() - statusBitList.begin();
}

/*****************************************************************************
 *
 * get status Bit from index
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Retrieve HandlerStatusBit in HandlerStatus's list from an index.
 * Allowable values being 0 to getNumberOfHandlerStatusBits()
 *
 *****************************************************************************/
const HandlerStatusBit& HandlerStatus::getHandlerStatusBit(
        int index /* index of bit in list */)  const
{
    if ( index < 0 || index >=getNumberOfHandlerStatusBits() )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR, 
                        "Unable to get HandlerStatusBit for index %d: not in range 0 - %d",
                         index,
                         getNumberOfHandlerStatusBits()-1);
        throw RangeException("Unable to get HandlerStatusBit: Out of Range");
    }

    Std::vector<HandlerStatusBit*>::const_iterator i;

    i = statusBitList.begin() + index;

    return *(*i);
}


/*****************************************************************************
 *
 * get boolean HandlerStatus bit from identity
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Retrieve actual bit of HandlerStatusBit given HandlerStatusBit identity
 *
 *****************************************************************************/
bool HandlerStatus::getStatusBit(
        int id /* identity of HandlerStatusBit */)  const
{

    HandlerStatusBit b(id);

    const HandlerStatusBit &bit = getHandlerStatusBit(b);

    return bit.getBit();
}

/*****************************************************************************
 *
 * get status as Long
 *
 * Authors: Chris Joyce
 *
 * Description:
 * For each of the HandlerStatusBit(s) in HandlerStatus add together the
 * status bits to give an int.
 *
 *****************************************************************************/
long HandlerStatus::getStatusAsLong() const
{
    long statusInt=0;
    Std::vector<HandlerStatusBit*>::const_iterator i;

    for ( i = statusBitList.begin() ; i != statusBitList.end() ; i++ )
    {
        statusInt += (*i)->getBitAsLong();
    }

    return statusInt;
}

/*****************************************************************************
 *
 * log status 
 *
 * Authors: Chris Joyce
 *
 * Description:
 * For each printable HandlerStatusBit(s) in HandlerStatus print a log
 * message if that bit has been set
 *
 *****************************************************************************/
void HandlerStatus::logStatus() const
{
    char message[256];
    
    strcpy(message,"Status: ");

    Std::vector<HandlerStatusBit*>::const_iterator i;

    for ( i = statusBitList.begin() ; i != statusBitList.end() ; i++ )
    {
        if ( (*i)->getBit() )
        {
            strcat(message,(*i)->getName());
        }
        strcat(message," ");
    }

    phLogSimMessage(logId, LOG_DEBUG, "%s",message);
}

/*****************************************************************************
 *
 * register observer
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Add StatusObserver object to list.  StatusObserver objects are objects 
 * which want to know when the StatusHandlerBits have changed.
 *
 *****************************************************************************/
void HandlerStatus::registerObserver(StatusObserver *observer)
{
    observerList.push_back(observer);
}

/*****************************************************************************
 *
 * register recorder
 *
 * Authors: Chris Joyce
 *
 * Description:
 * A EventRecorder object wants to be informed when a particular observer
 * which has registered with HandlerStatus changes HandlerStatusBits.
 * (for later playback)
 *****************************************************************************/
void HandlerStatus::registerRecorder(HandlerEventRecorder *rec, StatusObserver *obv)
{
    pEventRecorder=rec;
    pRecordObserver=obv;
}


/*--- HandlerStatus private member functions ---------------------------------*/

/*****************************************************************************
 *
 * get status Bit
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Non-constant version of getHandlerStatusBit for private use only.
 *
 *****************************************************************************/
HandlerStatusBit& HandlerStatus::getHandlerStatusBit(
        const HandlerStatusBit    &b   /* user setup HandlerStatusBit */)  
{
    Std::vector<HandlerStatusBit*>::iterator i;
    HandlerStatusBit *pHandSB=0;

    for ( i = statusBitList.begin() ; i!=statusBitList.end() && !pHandSB ; i++ )
    {
        if ( *(*i) == b )
        {
            pHandSB= *i;
        }
    }

    if ( i == statusBitList.end() && !pHandSB )
    {
        phLogSimMessage(logId, PHLOG_TYPE_ERROR, 
                        "HandlerStatus::getHandlerStatusBit() Unable to find HandlerStatusBit for \"%s\" \"%s\"",
                         b.getName(),
                         b.getKey());
        throw FormatException("unknown HandlerStatusBit");
    }

    return *pHandSB;
}




/*****************************************************************************
 * End of file
 *****************************************************************************/
