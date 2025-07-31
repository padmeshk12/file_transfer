/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.C
 * CREATED  : 3 Aug 2015
 *
 * CONTENTS : Ismeca handler simulator
 *
 * AUTHORS  : Magco Li, R&D Shanghai, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 3 Aug 2015, Magco Li, initial revision
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

#include <vector>
#include <list>
#include <map>
#include <algorithm>
#if __GNUC__ >= 3
#include <sstream>
#else
#include <strstream.h>
#endif


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_GuiServer.h"

#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif


/*--- defines ---------------------------------------------------------------*/


/* set to create new playback file from old playback file */
/* #define SAVE_NEW_PLAYBACK_FORMAT  */

#undef SAVE_NEW_PLAYBACK_FORMAT

#define LOG_NORMAL           PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG            PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE          PHLOG_TYPE_MESSAGE_2

#define CMD_REPLY_MSG "OK"

#define F_STOP               0x0002
#define F_JAM                0x0004
#define F_DOOROPEN           0x0008
#define F_ASSIST             0x0020
#define F_FATAL              0x0040
#define F_PAUSE              0x0080
#define F_ALL                0xffff

#define MAX_RECORD_LENGTH    128
#define MAX_RESPONSE_LENGTH  128
#define MAX_HANDLRID_LENGTH  256
#define MAX_MESSAGE_LENGTH   1024
#define MAX_GUI_DESC_LENGTH  1024
#define MAX_SITE_LENGTH      1024
#define MAX_LINE_BUFFER      2048


enum model_t { NX16, NY20 };
/*--- class definitions -----------------------------------------------------*/


#if __GNUC__ >= 3
class TCIostrstream : public Std::ostringstream {
#else
class TCIostrstream : public ostrstream {
#endif
public:
    TCIostrstream() ;
    ~TCIostrstream() ;
    char* str() ;
private:
    char* mystr;
};



typedef Std::map<int, int> siteStatType;
typedef siteStatType::iterator siteStatIterator;



class LogObject
{
public:
    LogObject(phLogId_t l);
protected:
    phLogId_t        logId;
};

class Timer : public LogObject
{
public:
    Timer(phLogId_t l);
    void start();
    double getElapsedMsecTime();
private:
    struct timeval startTime;
};

class SitePattern : public LogObject
{
public:
    SitePattern(phLogId_t l);
    SitePattern(phLogId_t l, int n);
    virtual bool getSitePopulated(int s)=0;
    virtual void nextPattern();
protected:
    int numOfSites;
};

class AllOneNotOneIsWorkingPattern : public SitePattern
{
    enum ePatternType { all_working, one_not_working, one_is_working  };

public:
    AllOneNotOneIsWorkingPattern(phLogId_t l, int n);
    virtual bool getSitePopulated(int s);
    virtual void nextPattern();
private:
    int siteCounter;
    ePatternType patternType;
};

class AllSitesWorkingPattern : public SitePattern
{
public:
    AllSitesWorkingPattern(phLogId_t l);
    virtual bool getSitePopulated(int s);
};


class Site : public LogObject
{
public:
    enum eSiteStatus { empty, waitingBinData, waitingRelease  };

    Site(phLogId_t l);
    bool getSitePopulated();
    void setSitePopulated(bool p);
    void sendDeviceToBin(int b);
    void setBinData(int b);
    int  getBinData();
    int  getPreviousBinData();
    bool releaseDevice();
    bool readyForRelease();
    bool setContact(bool c);

    siteStatType getSiteStatistics();
protected:
    Std::vector<int> binList;
private:
    eSiteStatus status;
    int binData;
    siteStatType siteStatistics;
    // contact flag : true when part is inserted
    // into contactors. Always true except when
    // contact( false ) is called. This being
    // manual way of performing a reprobe.
    bool contact;
};


const int handlerContinuousTesting = -1;
const int allSitesEnabled = -1;
const int siteNotYetAssignedBinNumber = -1;


class Handler : public LogObject
{
public:
    // Kind of pattern.
    enum eHandlerSitePattern { all_sites_working,
                               all_one_not_one_is_working };
    // Handler Status
    enum eHandlerStatus { stopped, handlingDevices, waiting  };

    // Handler Mode
    enum eHandlerReprobeMode { ignore_reprobe,
                               perform_reprobe_separately,
                               add_new_devices_during_reprobe };

    Handler(phLogId_t l,
            int s,
            long sem,
            double hd,
            int ndt,
            eHandlerSitePattern p,
            eHandlerReprobeMode hrm,
            bool cbd);
    ~Handler();
    inline int getNumOfSites();
    bool getSitePopulated(int s);
    int getSitePopulationAsInt();
    void sendDeviceToBin(int s, int b);
    void setBinData(int s, int b);
    void setContact(int s, bool c);
    int getBinData(int s);
    void releaseDevice(int s);
    bool readyForRelease();
    void start();
    void stop();
    bool sendTestStartSignal();
    bool allTestsComplete();
    void logHandlerStatistics();
    long getSitesEnabled();
    long getSitesDisabled();
    bool getSiteEnabled(int s);
    bool getSiteDisabled(int s);
    void setRetestCategory(int r);
    void setReprobeCategory(int r);
    int getRetestCategory() const;
    int getReprobeCategory() const;
    //
    // reset number of tested devices
    //
    void reset();
    void handleDevices();

protected:
    bool populateSites();
    void populateSitesToPattern();
    bool allSitesBinned();
    bool reprobePending();
    long getNumberOfSitesAsLong();
    eHandlerStatus status;
    Timer *binningTimer;
    int numOfSites;
    int numOfTestedDevices;
    long siteEnabledMask;
    double handlingDelayMsec;
    const int numOfDevicesToTest;
    eHandlerReprobeMode handlerReprobeMode;
    SitePattern *sitePattern;
    Std::vector<Site> siteList;
    bool running;
    int retestCat;
    int reprobeCat;
    bool corruptBinData;
    int setBinDataCount;
    int nextCorruptBinCount;
};


class StatusBit
{
public:
    StatusBit();
    StatusBit(bool v, int p);
    void setBitPosition(bool b, int p);
    void setBit(bool b);
    void setPosition(int p);
    int getBitAsInt();
    bool getBit();
private:
    bool value;
    int position;
};


class IsmecaStatus : public LogObject
{
public:
    IsmecaStatus(phLogId_t l);
    int getStatusAsInt();
    void setHandlerEmpty(bool b);
    void setOutOfGuardBand(bool b);
    void setJammed(bool b);
    void setStopped(bool b);
    void setDoorOpen(bool b);
    bool getHandlerEmpty();
    bool getOutOfGuardBand();
    bool getJammed();
    bool getStopped();
    bool getDoorOpen();
    bool getHandlerIsReady();
    void logStatus();
private:
    StatusBit             b00_retestOrtestagain;
    StatusBit             b01_outputFull;
    StatusBit             b02_handlerEmpty;
    StatusBit             b03_motorDiagEnabled;
    StatusBit             b04_dymContactorCalEnabled;
    StatusBit             b05_inputInhibited;
    StatusBit             b06_sortInhibited;
    StatusBit             b07_outOfGuardBands;
    StatusBit             b08_jammed;
    StatusBit             b09_stopped;
    StatusBit             b10_partsSoaking;
    StatusBit             b11_inputOrSortInhibitedOrDoorOpen;
};



/*--- class implementations -------------------------------------------------*/


#if __GNUC__ >= 3
TCIostrstream::TCIostrstream() : Std::ostringstream(), mystr(NULL)
#else
TCIostrstream::TCIostrstream() : ostrstream(), mystr(NULL)
#endif
{
}

TCIostrstream::~TCIostrstream()
{
    delete[] mystr;
}

char* TCIostrstream::str()
{

    // create null terminated string
#if __GNUC__ >= 3
    mystr = new char[Std::ostringstream::str().length() + 1 ];

    strncpy(mystr,Std::ostringstream::str().c_str(), Std::ostringstream::str().length());
    mystr[Std::ostringstream::str().length()]='\0';
#else
    mystr = new char[ostrstream::pcount() + 1 ];

    strncpy(mystr,ostrstream::str(),ostrstream::pcount());
    mystr[ostrstream::pcount()]='\0';
#endif

    return mystr;
}




/*--- LogObject -------------------------------------------------------------*/
LogObject::LogObject(phLogId_t l) : logId(l)
{
}


/*--- Timer -----------------------------------------------------------------*/

Timer::Timer(phLogId_t l) : LogObject(l)
{
    start();
}

void Timer::start()
{
    gettimeofday(&startTime, NULL);
}

double Timer::getElapsedMsecTime()
{
    struct timeval now;

    gettimeofday(&now, NULL);

    return (now.tv_sec - startTime.tv_sec)*1000.0 + (now.tv_usec - startTime.tv_usec)/1000.0;
}

/*--- SitePattern -----------------------------------------------------------*/
SitePattern::SitePattern(phLogId_t l, int n) : LogObject(l), numOfSites(n)
{
}

SitePattern::SitePattern(phLogId_t l) : LogObject(l), numOfSites(0)
{
}

void SitePattern::nextPattern()
{
}

/*--- AllOneNotOneIsWorkingPattern ---------------------------------*/
AllOneNotOneIsWorkingPattern::AllOneNotOneIsWorkingPattern(phLogId_t l, int n) :
        SitePattern(l,n), siteCounter(0), patternType(all_working)
{
}

bool AllOneNotOneIsWorkingPattern::getSitePopulated(int s)
{
    bool sitePopulated = false;

    switch ( patternType )
    {
        case all_working:
            sitePopulated=true;
            break;
        case one_not_working:
            sitePopulated = ( siteCounter != s );
            break;
        case one_is_working:
            sitePopulated = ( siteCounter == s );
            break;
    }

    return sitePopulated;
}


void AllOneNotOneIsWorkingPattern::nextPattern()
{
    ::std::string patternTypeStr;
    ++siteCounter;

    if ( siteCounter == numOfSites )
    {
        siteCounter=0;
        switch ( patternType )
        {
            case all_working:
                patternType=one_not_working;
                break;
            case one_not_working:
                patternType=one_is_working;
                break;
            case one_is_working:
                patternType=all_working;
                break;
        }
    }

    switch ( patternType )
    {
        case all_working:
            patternTypeStr="all working";
            break;
        case one_not_working:
            patternTypeStr="one not working";
            break;
        case one_is_working:
            patternTypeStr="one is working";
            break;
    }

    phLogSimMessage(logId, LOG_VERBOSE,
                    "AllOneNotOneIsWorkingPattern::nextPattern %s c = %d ",
                    patternTypeStr.c_str(),
                    siteCounter);
    return;
}


/*--- AllSitesWorkingPattern -------------- ---------------------------------*/
AllSitesWorkingPattern::AllSitesWorkingPattern(phLogId_t l) : SitePattern(l)
{
}

bool AllSitesWorkingPattern::getSitePopulated(int s)
{
    return true;
}


/*--- Handler ---------------------------------------------------------------*/

//
// CONSTRUCTOR:
//
Handler::Handler(phLogId_t l,
            int s,
            long sem,
            double hd,
            int ndt,
            eHandlerSitePattern p,
            eHandlerReprobeMode hrm,
            bool cbd) :
        LogObject(l),
        numOfSites(s),
        siteEnabledMask(sem),
        handlingDelayMsec(hd),
        numOfDevicesToTest(ndt),
        handlerReprobeMode(hrm),
        corruptBinData(cbd)
{
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::Handler s = %d hd = %lf ndt = %d p = %s ",
                    s, hd, ndt, p==all_sites_working ? "all" : "one" );

    for ( int i=0 ; i < numOfSites ; i++ )
    {
        siteList.push_back(Site(l));
    }

    switch ( p )
    {
        case all_sites_working:
            sitePattern = new AllSitesWorkingPattern(l);
            break;
        case all_one_not_one_is_working:
            sitePattern = new AllOneNotOneIsWorkingPattern(l,numOfSites);
            break;
        default:
            sitePattern = NULL;
    }
    status=stopped;
    running=false;
    numOfTestedDevices=0;
    binningTimer=new Timer(l);
    setRetestCategory(-1);
    setReprobeCategory(-2);
    setBinDataCount=0;
    nextCorruptBinCount=0;
    if ( siteEnabledMask == allSitesEnabled || siteEnabledMask == 0 )
    {
        siteEnabledMask=getNumberOfSitesAsLong();
    }
}

Handler::~Handler()
{
    if (sitePattern != NULL)
    {
        delete sitePattern;
        sitePattern = NULL;
    }

    if (binningTimer != NULL)
    {
        delete binningTimer;
        binningTimer = NULL;
    }
}

//
// PUBLIC interface functions:
//

inline int Handler::getNumOfSites()
{
    return numOfSites;
}

int Handler::getSitePopulationAsInt()
{
    char message[MAX_MESSAGE_LENGTH];
    char tmpString[MAX_MESSAGE_LENGTH];
    int sitePop=0;
    Std::vector<Site>::iterator i;
    int bitPosition=0;

    phLogSimMessage(logId, LOG_VERBOSE, "Handler::getSitePopulationAsInt()");

    strcpy(message,"Site: ");

    for ( i=siteList.begin(), bitPosition=0 ; i != siteList.end() ; ++i, ++bitPosition )
    {
        sprintf(tmpString, "%d[%s] ", bitPosition+1, i->getSitePopulated() ? "P" : "E");
        strcat(message, tmpString);

        if ( i->getSitePopulated() )
        {
            sitePop |= ( 1 << bitPosition );
        }
    }

    phLogSimMessage(logId, LOG_DEBUG, "%s", message);
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::getSitePopulationAsInt() returned value is %d",sitePop);

    return sitePop;
}

void Handler::sendDeviceToBin(int s, int b)
{
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::sendDeviceToBin(%d,%d) ",s,b);

    if ( s < 0 || s > numOfSites )
    {
        phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s);
        return;
    }
    else
    {
        if ( b == getReprobeCategory() )
        {
            phLogSimMessage(logId, LOG_DEBUG, "reprobe device on site %d ",s);
        }
        else if ( b == getRetestCategory() )
        {
            phLogSimMessage(logId, LOG_DEBUG, "retest device on site %d ",s);
        }
        else
        {
            phLogSimMessage(logId, LOG_DEBUG, "send device on site %d to bin %d ",s,b);
        }
    }

    Std::vector<Site>::iterator i;

    i = siteList.begin() + s - 1;
    i->sendDeviceToBin(b);
    if ( allSitesBinned() )
    {
        handleDevices();
    }
    return;
}

void Handler::setBinData(int s, int b)
{
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::setBinData(%d,%d) ",s,b);

    if ( s < 0 || s > numOfSites )
    {
        phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s);
        return;
    }

    if ( corruptBinData )
    {
        ++setBinDataCount;

        if ( setBinDataCount % 11 == 0 || setBinDataCount == nextCorruptBinCount )
        {
            phLogSimMessage(logId, LOG_DEBUG, "corrupt bin data for site %d from %d to %d ",
                            s,b,(b+setBinDataCount) % 100 );
            b = (b+setBinDataCount) % 100;
            if ( setBinDataCount % 11 == 0 )
            {
                nextCorruptBinCount=setBinDataCount + 4;
            }
            else
            {
                setBinDataCount+=7;
            }
        }
    }

    if ( b == getReprobeCategory() )
    {
        phLogSimMessage(logId, LOG_DEBUG, "set reprobe device on site %d ",s);
    }
    else if ( b == getRetestCategory() )
    {
        phLogSimMessage(logId, LOG_DEBUG, "set retest device on site %d ",s);
    }
    else
    {
        phLogSimMessage(logId, LOG_DEBUG, "set device on site %d to bin %d ",s,b);
    }

    Std::vector<Site>::iterator i;

    i = siteList.begin() + s;

    i->setBinData(b);

    return;
}


void Handler::setContact(int s, bool c)
{
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::setContact(%d,%d) ",s,int(c));

    if ( s < 0 || s > numOfSites )
    {
        phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s);
        return;
    }
    else
    {
        phLogSimMessage(logId, LOG_DEBUG, "set contact site %d to %d ",s,int(c));
    }


    bool contactMoved;
    Std::vector<Site>::iterator i;

    i = siteList.begin() + s;

    contactMoved=i->setContact(c);

    if ( c == true && contactMoved )
    {
        phLogSimMessage(logId, LOG_DEBUG, "reprobe action detected on site %d ",s);
        sendDeviceToBin(s,getReprobeCategory());
    }

    return;
}


int Handler::getBinData(int s)
{
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::getBinData(%d) ",s);

    if ( s < 0 || s > numOfSites )
    {
        phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s);
        return 0;
    }

    Std::vector<Site>::iterator i;

    i = siteList.begin() + s;

    if ( i->getBinData() == siteNotYetAssignedBinNumber )
    {
        phLogSimMessage(logId, LOG_DEBUG, "no bin data assigned for site %d  ",s );
    }
    else if ( i->getBinData() == getReprobeCategory() )
    {
        phLogSimMessage(logId, LOG_DEBUG, "reprobe assigned for site %d  ",s );
    }
    else
    {
        phLogSimMessage(logId, LOG_DEBUG, "bin data for site %d is %d ",
                        s,
                        i->getBinData());
    }

    return i->getBinData();
}

void Handler::releaseDevice(int s)
{
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::releaseDevice(%d) ",s);

    if ( s < 0 || s > numOfSites )
    {
        phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s);
        return;
    }

    Std::vector<Site>::iterator i;

    i = siteList.begin() + s;

    if ( i->releaseDevice() )
    {
        phLogSimMessage(logId, LOG_DEBUG, "release device for site %d ",s);

        if ( allSitesBinned() )
        {
            handleDevices();
        }
    }
    return;
}

bool Handler::readyForRelease()
{
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::readyForRelease() ");

    //
    // Handler is only ready for release if all sites are ready.
    //

    bool mayReleaseDevice=true;

    Std::vector<Site>::iterator i;

    for ( i=siteList.begin() ; i != siteList.end() && mayReleaseDevice==true ; ++i )
    {
        mayReleaseDevice=i->readyForRelease();
    }

    return mayReleaseDevice;
}



void Handler::start()
{
    binningTimer->start();
    if ( status==stopped )
    {
        status=handlingDevices;
    }
    running=true;
}

void Handler::stop()
{
    running=false;
}


bool Handler::sendTestStartSignal()
{
    bool sendTestStart = false;

    phLogSimMessage(logId, LOG_VERBOSE, "Handler::sendTestStartSignal elapsed time so far %lf wait %lf ",
                    binningTimer->getElapsedMsecTime(),
                    handlingDelayMsec);
    switch ( status )
    {
        case stopped:
            sendTestStart=false;
            break;
        case handlingDevices:
            if ( running )
            {
                if ( populateSites() )
                {
                    sendTestStart=true;
                    status=waiting;
                }
                else
                {
                    sendTestStart=false;
                    status=stopped;
                    running=false;
                }
            }
            else
            {
                sendTestStart=false;
            }
            break;
        case waiting:
            sendTestStart=false;
            break;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "Handler::sendTestStartSignal is %s ",
                    sendTestStart ? "true" : "false");

    return sendTestStart;
}

bool Handler::allTestsComplete()
{
    return (numOfDevicesToTest==handlerContinuousTesting) ?
               false :  ( numOfTestedDevices >= numOfDevicesToTest ) && allSitesBinned() && !reprobePending();
}

void Handler::logHandlerStatistics()
{
    Std::vector<Site>::iterator i;
    int s=0;
    Std::vector<siteStatType> siteStatList;
    Std::vector<siteStatType>::iterator si;
    siteStatIterator mi;

    Std::list<int> siteStatIndex;
    Std::list<int>::iterator ssii;

    Std::vector<int> columnTotal;
    Std::vector<int>::iterator cti;
    int rowTotal;

    for ( i=siteList.begin() ; i != siteList.end() ; ++i )
    {
        siteStatList.push_back(i->getSiteStatistics());
    }

    s=0;
    for ( si=siteStatList.begin() ; si != siteStatList.end() ; ++si )
    {
        int stat;
        int n;

        for ( mi=(*si).begin() ; mi != (*si).end() ; ++mi )
        {
            stat = (*mi).first;
            n    = (*mi).second;

            siteStatIndex.push_back(stat);
        }
    }

    siteStatIndex.sort();
    siteStatIndex.unique();

    phLogSimMessage(logId, LOG_DEBUG, " ");
    phLogSimMessage(logId, LOG_DEBUG, "                Yield Statistics ");
    phLogSimMessage(logId, LOG_DEBUG, " ");

    Std::vector<int>::iterator siteIterator;

    char lineBuffer[MAX_LINE_BUFFER];
    char tmpString[MAX_MESSAGE_LENGTH];
    s=0;
    strcpy(lineBuffer,"         ");
    for ( si=siteStatList.begin() ; si != siteStatList.end() ; ++si )
    {
        sprintf(tmpString, " Chuck%-2d ", ++s);
        strcat(lineBuffer,tmpString);
        columnTotal.push_back(0);
    }
    sprintf(tmpString, " Total ");
    strcat(lineBuffer,tmpString);
    phLogSimMessage(logId, LOG_DEBUG, "%s",lineBuffer );

    for ( ssii = siteStatIndex.begin() ; ssii != siteStatIndex.end() ; ++ssii )
    {

        if ( *ssii == getReprobeCategory() )
        {
            strcpy(lineBuffer," Reprobe ");
        }
        else if ( *ssii == getRetestCategory() )
        {
            strcpy(lineBuffer," Retest  ");
        }
        else
        {
            sprintf(tmpString, " Cat %3d ", *ssii);
            strcpy(lineBuffer,tmpString);
        }

        rowTotal=0;
        s=0;
        for ( si=siteStatList.begin() ; si != siteStatList.end() ; ++si, ++s )
        {
            mi=(*si).find(*ssii);

            if ( mi == (*si).end() )
            {
                strcat(lineBuffer,"         ");
            }
            else
            {
                sprintf(tmpString, " %4d    ", (*mi).second);
                strcat(lineBuffer,tmpString);
                rowTotal+=(*mi).second;
                if ( *ssii != getReprobeCategory() )
                {
                    columnTotal[s]+=(*mi).second;
                }
            }
        }
        sprintf(tmpString, " %4d    ", rowTotal);
        strcat(lineBuffer,tmpString);

        phLogSimMessage(logId, LOG_DEBUG, "%s",lineBuffer );
    }

    phLogSimMessage(logId, LOG_DEBUG, " ");
    strcpy(lineBuffer,"Total    ");
    for ( cti=columnTotal.begin() ; cti != columnTotal.end() ; ++cti )
    {
        sprintf(tmpString, " %4d    ", *cti);
        strcat(lineBuffer,tmpString);
    }
    phLogSimMessage(logId, LOG_DEBUG, "%s",lineBuffer );
    phLogSimMessage(logId, LOG_DEBUG, " ");
    phLogSimMessage(logId, LOG_DEBUG, "Total Number of tested devices = %d",numOfTestedDevices );
    phLogSimMessage(logId, LOG_DEBUG, " ");

    return;
}

long Handler::getSitesEnabled()
{
    return siteEnabledMask;
}

long Handler::getSitesDisabled()
{
    return ( getNumberOfSitesAsLong() & ~siteEnabledMask );
}

bool Handler::getSiteEnabled(int s)
{
    return siteEnabledMask & ( 1 << s );
}

bool Handler::getSiteDisabled(int s)
{
    return !getSiteEnabled(s);
}

void Handler::setRetestCategory(int r)
{
    retestCat=r;
}

void Handler::setReprobeCategory(int r)
{
    reprobeCat=r;
}

int Handler::getRetestCategory() const
{
    return retestCat;
}

int Handler::getReprobeCategory() const
{
    return reprobeCat;
}

void Handler::reset()
{
    phLogSimMessage(logId, LOG_DEBUG, "New lot loaded ",numOfTestedDevices );
    numOfTestedDevices=0;
}

//
// PROTECTED interface functions:
//
void Handler::handleDevices()
{
    binningTimer->start();

    if ( handlerReprobeMode==perform_reprobe_separately && reprobePending() )
    {
        phLogSimMessage(logId, LOG_VERBOSE,
            "no new pattern: reprobe pending and perform reprobe separately");
    }
    else
    {
        sitePattern->nextPattern();
    }

    status=handlingDevices;
    return;
}

bool Handler::populateSites()
{
    populateSitesToPattern();
    while( allSitesBinned() && !allTestsComplete() )
    {
        sitePattern->nextPattern();
        populateSitesToPattern();
    }
    return !allSitesBinned();
}


void Handler::populateSitesToPattern()
{
    Std::vector<Site>::iterator i;
    int s = 0;
    bool popSite = false;
    bool popSiteReprobe = false;
    bool popSitePattern = false;

    for ( i=siteList.begin(), s=0 ; i != siteList.end() ; ++i, ++s )
    {
        popSitePattern=sitePattern->getSitePopulated(s) &&
                      ((1<<s)&siteEnabledMask) &&
                      ( numOfTestedDevices < numOfDevicesToTest );
        popSiteReprobe=(i->getPreviousBinData() == getReprobeCategory());
        switch ( handlerReprobeMode )
        {
            case ignore_reprobe:
                popSite=popSitePattern;
                break;
            case perform_reprobe_separately:
                popSite = ( reprobePending() ) ? popSiteReprobe : popSitePattern;
                break;
            case add_new_devices_during_reprobe:
                popSite = popSitePattern || popSiteReprobe;
                break;
        }

        if ( i->getSitePopulated() )
        {
            phLogSimMessage(logId, LOG_VERBOSE,
                            "Handler::populateSitesToPattern cannot populate site %d since it is not empty ",
                            s);
        }
        else
        {
            if ( popSite && !popSiteReprobe )
            {
                ++numOfTestedDevices;
            }
            if ( !popSite && popSiteReprobe )
            {
                --numOfTestedDevices;
            }

            i->setSitePopulated( popSite );
        }
    }
    return;
}

bool Handler::allSitesBinned()
{
    bool checkAllSitesBinned=true;
    Std::vector<Site>::iterator i;

    for ( i=siteList.begin() ; i != siteList.end() && checkAllSitesBinned==true ; ++i )
    {
        if ( i->getSitePopulated() == true )
        {
          checkAllSitesBinned=false;
        }
    }
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::allSitesBinned is %s",
                    checkAllSitesBinned ? "true" : "false");

    return checkAllSitesBinned;
}


bool Handler::reprobePending()
{
    bool checkReprobePending=false;
    Std::vector<Site>::iterator i;

    for ( i=siteList.begin() ; i != siteList.end() && checkReprobePending==false ; ++i )
    {
        if ( i->getPreviousBinData() == getReprobeCategory() )
        {
            checkReprobePending=true;
        }
    }
    phLogSimMessage(logId, LOG_VERBOSE, "Handler::reprobePending is %s ",
                    checkReprobePending ? "true" : "false");

    return checkReprobePending;
}

long Handler::getNumberOfSitesAsLong()
{
    long numOfSitesAsLong=0;

    for ( int i=0 ; i< numOfSites ; ++i )
    {
        numOfSitesAsLong+= 1 << i;
    }

    return numOfSitesAsLong;
}








/*--- Site ------------------------------------------------------------------*/

//
// CONSTRUCTOR:
//

Site::Site(phLogId_t l) : LogObject(l),
                          status(Site::empty),
                          binData(siteNotYetAssignedBinNumber),
                          contact(true)
{
}

//
// PUBLIC interface functions:
//

bool Site::getSitePopulated()
{
    return (status==waitingBinData || status==waitingRelease);
}

void Site::setSitePopulated(bool p)
{
    if ( p )
    {
        status=waitingBinData;
    }
    else
    {
        status=empty;
    }
}

void Site::sendDeviceToBin(int b)
{
    if ( status==empty )
    {
        phLogSimMessage(logId, LOG_DEBUG,
            "Bin data received for a site which is empty: ignored");
    }
    else
    {
        binData=b;
        binList.push_back(b);
        status=empty;
    }
}

void Site::setBinData(int b)
{

    if ( status==empty )
    {
        phLogSimMessage(logId, LOG_DEBUG,
            "Set bin data received for a site which is empty: ignored");
    }
    else
    {
        binData=b;
        status=waitingRelease;
    }
}

int Site::getBinData()
{
    return status==waitingRelease ? binData : siteNotYetAssignedBinNumber;
}

int Site::getPreviousBinData()
{
    return binData;
}

bool Site::releaseDevice()
{
    bool deviceReleased = false;

    switch ( status )
    {
        case empty:
            phLogSimMessage(logId, LOG_VERBOSE,
                "Release device received for a site which is empty: ignored");
            deviceReleased=false;
            break;
        case waitingBinData:
            phLogSimMessage(logId, LOG_VERBOSE,
                "Release device received for a site which is still waiting bin data: ignored");
            deviceReleased=false;
            break;
        case waitingRelease:
            binList.push_back(binData);
            status=empty;
            deviceReleased=true;
            break;
    }
    return deviceReleased;
}

bool Site::readyForRelease()
{
    bool mayReleaseDevice = false;

    switch ( status )
    {
        case empty:
            phLogSimMessage(logId, LOG_VERBOSE,
                "site is empty: release is okay");
            mayReleaseDevice=true;
            break;
        case waitingBinData:
            phLogSimMessage(logId, LOG_VERBOSE,
                "waiting bin data: not ready for release");
            mayReleaseDevice=false;
            break;
        case waitingRelease:
            phLogSimMessage(logId, LOG_VERBOSE,
                "waiting bin data: ready for release");
            mayReleaseDevice=true;
            break;
    }
    return mayReleaseDevice;
}


bool Site::setContact(bool c)
{
    bool logMessage=false;

    if ( contact != c )
    {
        logMessage=true;
        if ( contact )
        {
            phLogSimMessage(logId, LOG_DEBUG, "retract contact motors ");
        }
        else
        {
            phLogSimMessage(logId, LOG_DEBUG, "extend contact motors ");
        }
    }
    contact=c;
    return logMessage;
}

siteStatType Site::getSiteStatistics()
{
    siteStatIterator mi;

    Std::vector<int>::iterator i;
    int c=0;

    for ( i=binList.begin() ; i != binList.end() ; ++i )
    {
        phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ",++c,*i);
    }

    phLogSimMessage(logId, LOG_VERBOSE, "Sort Data ");
    std::sort(binList.begin(), binList.end());
    c=0;
    for ( i=binList.begin() ; i != binList.end() ; ++i )
    {
        phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ",++c,*i);
    }

    i=binList.begin();
    int n;
    int stat;
    while ( i != binList.end() )
    {

        stat=*i;

        phLogSimMessage(logId, LOG_VERBOSE, "stat is %d ",stat );

#if __GNUC__ >= 3
        n = std::count(i, binList.end(), stat);
#else
        n=0;
        count(i, binList.end(), stat, n);
#endif

        phLogSimMessage(logId, LOG_VERBOSE, "count is %d ",n );


        siteStatistics[stat]=n;

        i += n;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "Data Statistics ");

    for ( mi=siteStatistics.begin() ; mi != siteStatistics.end() ; ++mi )
    {
       stat = (*mi).first;
       n    = (*mi).second;
       phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ",stat,n);
    }


    return siteStatistics;
}

//
// PROTECTED interface functions:
//


/*--- StatusBit -------------------------------------------------------------*/

StatusBit::StatusBit() : value(false), position(0)
{
}

StatusBit::StatusBit(bool v, int p) : value(v), position(p)
{
}

void StatusBit::setBit(bool b)
{
    value=b;
}

void StatusBit::setPosition(int p)
{
    position=p;
}

void StatusBit::setBitPosition(bool b, int p)
{
    value=b;
    position=p;
}

int StatusBit::getBitAsInt()
{
    return value*(1<<position);
}

bool StatusBit::getBit()
{
    return value;
}


/*--- IsmecaStatus -----------------------------------------------------------*/

IsmecaStatus::IsmecaStatus(phLogId_t l) : LogObject(l)
{
    b00_retestOrtestagain.setBitPosition(false, 0);
    b01_outputFull.setBitPosition(false, 1);
    b02_handlerEmpty.setBitPosition(false, 2);
    b03_motorDiagEnabled.setBitPosition(false, 3);
    b04_dymContactorCalEnabled.setBitPosition(false, 4);
    b05_inputInhibited.setBitPosition(false, 5);
    b06_sortInhibited.setBitPosition(false, 6);
    b07_outOfGuardBands.setBitPosition(false, 7);
    b08_jammed.setBitPosition(false, 8);
    b09_stopped.setBitPosition(false, 9);
    b10_partsSoaking.setBitPosition(false,10);
    b11_inputOrSortInhibitedOrDoorOpen.setBitPosition(false,11);
}


int IsmecaStatus::getStatusAsInt()
{
    return b00_retestOrtestagain.getBitAsInt() +
           b01_outputFull.getBitAsInt() +
           b02_handlerEmpty.getBitAsInt() +
           b03_motorDiagEnabled.getBitAsInt() +
           b04_dymContactorCalEnabled.getBitAsInt() +
           b05_inputInhibited.getBitAsInt() +
           b06_sortInhibited.getBitAsInt() +
           b07_outOfGuardBands.getBitAsInt() +
           b08_jammed.getBitAsInt() +
           b09_stopped.getBitAsInt() +
           b10_partsSoaking.getBitAsInt() +
           b11_inputOrSortInhibitedOrDoorOpen.getBitAsInt();
}

void IsmecaStatus::setHandlerEmpty(bool b)
{
   b02_handlerEmpty.setBit(b);
}

void IsmecaStatus::setOutOfGuardBand(bool b)
{
   b07_outOfGuardBands.setBit(b);
}

void IsmecaStatus::setJammed(bool b)
{
   b08_jammed.setBit(b);
}

void IsmecaStatus::setStopped(bool b)
{
   b09_stopped.setBit(b);
}

void IsmecaStatus::setDoorOpen(bool b)
{
   b11_inputOrSortInhibitedOrDoorOpen.setBit(b);
}

bool IsmecaStatus::getHandlerEmpty()
{
   return b02_handlerEmpty.getBit();
}

bool IsmecaStatus::getOutOfGuardBand()
{
   return b07_outOfGuardBands.getBit();
}

bool IsmecaStatus::getJammed()
{
   return b08_jammed.getBit();
}

bool IsmecaStatus::getStopped()
{
   return b09_stopped.getBit();
}

bool IsmecaStatus::getDoorOpen()
{
   return b11_inputOrSortInhibitedOrDoorOpen.getBit();
}

bool IsmecaStatus::getHandlerIsReady()
{
   return !getHandlerEmpty() && !getOutOfGuardBand() && !getJammed() &&
              !getDoorOpen();
}

void IsmecaStatus::logStatus()
{
    char message[MAX_MESSAGE_LENGTH];

    strcpy(message,"Status: ");

    if ( getHandlerIsReady() )
    {
        strcat(message,"ready, ");
    }
    else
    {
        strcat(message,"not ready, ");
    }

    if ( getHandlerEmpty() )
    {
        strcat(message,"handler empty, ");
    }

    if ( getOutOfGuardBand() )
    {
        strcat(message,"out of guard band, ");
    }

    if ( getJammed() )
    {
        strcat(message,"jammmed, ");
    }

    if ( getDoorOpen() )
    {
        strcat(message,"door open, ");
    }

    if ( getStopped() )
    {
        strcat(message,"stopped ");
    }
    else
    {
        strcat(message,"running ");
    }

    phLogSimMessage(logId, LOG_DEBUG, "%s",message);

}


/*--- End -------------------------------------------------------------------*/


/*--- all_t -----------------------------------------------------------------*/

struct all_t
{

    int              numOfSites;
    int              numberOfDevicesToTest;
    double           handlingDelay;
    long             siteEnabledMask;
    Handler::eHandlerSitePattern pattern;
    int              verifyTest;
    int              cmdReply;
    int              systemMode;
    int              emulationMode;
    int              errorBit;
    Handler::eHandlerReprobeMode reprobeMode;

    bool             corruptBinData;
    bool             releasepartsWhenReady;

    /* reprobe */
    int              reprobeBinNumber;
    /* retest */
    int              retestBinNumber;

// Handler has been stopped by operator
    bool             operatorPause;

    IsmecaStatus      *ismecaStatus;
    Handler          *handler;

    /* ids */
    phLogId_t        logId;
    phComId_t        comId;
    phGuiProcessId_t guiId;

    /* setup */
    int              debugLevel;
    char             *softwareVersion;
    char             *handlerNumber;
    int              timeout;
    char             *modelname;
    enum model_t     model;

    int              noGUI;
    int              autoStartTime;

    /* init */
    int              done;

    /* lots */
    int              lotActive;
    long             lotDoneTime;

    /* runtime */
    const char       *message;
    int              length;
    phComGpibEvent_t *comEvent;
    phGuiEvent_t     guiEvent;

    int              interactiveDelay;
    int              interactiveDelayCount;

    /* playback */
    int              pbRs232Step;
    int              pbSubStep;
    int              doRecord;
    int              doPlayback;
    FILE             *newRecordFile;

    FILE             *recordFile;
    FILE             *playbackFile;

    /* record data */
    char recordName[MAX_RECORD_LENGTH];
    char recordData[MAX_RECORD_LENGTH];
    int recordRetval;
    int recordStep;
    bool recordUsed;

    const char             *eoc;

    struct phComRs232Struct rs232Config;
};

static void doExitFail(struct all_t *all);
static void checkGuiEvent(struct all_t *all);

/*--- global variables ------------------------------------------------------*/

static struct all_t allg;

const static char *slaveGuiDescHead = " \
    S:`name`:` \
";

const static char *slaveGuiDescTail = " \
    ` \
    {@v \
        S::``{@v \
            S::{@h  f:`f_req`:`Request`:    p:`p_req`:`Receive` } \
            S::{@h  f:`f_answ`:`Answer`:    p:`p_answ`:`Send` } \
            S::{@h  f:`f_event`:`Events`:   p:`p_event`:`Clear` } \
            S::{@h  f:`f_status`:`Status`:  p:`p_quit`:`Quit` } \
            S::{@h  f:`f_delay`:`Delay [seconds]`:  * \
                    R::{`r_life_1`:`` `r_life_2`:``}:2:2 * } \
            S::{@h  p:`p_dump`:`Dump` } \
            S::{@h  p:`p_incDebug`:`+Debug` } \
            S::{@h  p:`p_decDebug`:`-Debug` } \
            S::{@h  t:`t_stop`:`Stop` } \
            S::{@h  t:`t_jam`:`Jam` } \
            S::{@h  t:`t_dooropen`:`Door open` } \
        } \
    } \
";

enum eGuiDataType { stop,
                    jam,
                    dooropen,
                    unknown };

static eGuiDataType getGuiDataType(struct all_t *all, const char* dtstr)
{
    eGuiDataType dt;

    if ( strcmp(dtstr,"t_stop") == 0 )
        dt=stop;
    else if ( strcmp(dtstr,"t_jam") == 0 )
        dt=jam;
    else if ( strcmp(dtstr,"t_dooropen") == 0 )
        dt=dooropen;
    else
        {
            dt=unknown;
            phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
                "unable to determine gui data type from %s", dtstr);
        }
    return dt;
}

static bool getIsmecaStatus(struct all_t *all, const char* dtstr)
{
    bool status;

    switch ( getGuiDataType(all,dtstr) )
    {
        case stop:
            status=all->ismecaStatus->getStopped();
            break;
        case jam:
            status=all->ismecaStatus->getJammed();
            break;
        case dooropen:
            status=all->ismecaStatus->getDoorOpen();
            break;
        case unknown:
            status=false;
            break;
    }
    return status;
}

static bool getPlaybackCmpt(struct all_t *all)
{
    int rtnfscanf;

    strcpy(all->recordName,"");
    strcpy(all->recordData,"");

    rtnfscanf=fscanf(all->playbackFile, "%d %d %s %s",
                     &(all->recordStep), &(all->recordRetval), all->recordName, all->recordData);

    if ( rtnfscanf == 4 )
    {
        phLogSimMessage(all->logId, LOG_VERBOSE, "getPlaybackCmpt() %d %d %s %s",
                        all->recordStep, all->recordRetval, all->recordName, all->recordData );

        all->recordUsed=false;
    }
    else if ( rtnfscanf == EOF )
    {
        phLogSimMessage(all->logId, LOG_VERBOSE, "getPlaybackCmpt() EOF detected ");
        all->recordUsed=true;
    }
    else
    {
        phLogSimMessage(all->logId, LOG_VERBOSE,"getPlaybackCmpt() unexpected return value from fscanf  %d ",
                        rtnfscanf );
    }

    return (rtnfscanf == 4);
}


/*--- playback functionality ------------------------------------------------*/

static void doRs232Step(struct all_t *all)
{
    all->pbRs232Step++;
    all->pbSubStep = 0;
}

static void doSubStep(struct all_t *all)
{
    all->pbSubStep++;
}

phGuiError_t phPbGuiGetData(
    struct all_t *all,
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    const char *componentName,     /* name of the component */
    char *data,                    /* the variable where the data will be stored */
    int timeout                    /* timeout after which phGuiGetData returns */
)
{
    phGuiError_t retVal=(phGuiError_t)0;

    if (all->doPlayback)
    {
        if ( all->pbRs232Step >= all->recordStep )
        {
            if ( all->recordUsed==true )
            {
                getPlaybackCmpt(all);
            }
            else
            {
                if ( all->pbRs232Step > all->recordStep )
                {
                    phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
                                    "record not made use of %d %d %s %s current step %d %s ",
                                    all->recordStep, all->recordRetval, all->recordName, all->recordData,
                                    all->pbRs232Step, componentName);
                    doExitFail(all);
                }
            }
        }

        if ( all->pbRs232Step==all->recordStep && strcmp(componentName,all->recordName)==0 )
        {
            retVal = (phGuiError_t) all->recordRetval;
            strcpy(data, all->recordData);
            all->recordUsed=true;
        }
        else
        {
            if ( getIsmecaStatus(all,componentName) )
                strcpy(data,"True");
            else
                strcpy(data,"False");
            retVal = (phGuiError_t) 0;
        }
        if (! all->noGUI)
            phGuiChangeValue(guiProcessID, componentName, data);

#ifdef SAVE_NEW_PLAYBACK_FORMAT
        bool IsmecaStatus=getIsmecaStatus(all,componentName);
        bool recordGuiData;

        if ( strcasecmp(data, "true")==0  && IsmecaStatus==false )
            recordGuiData=true;
        else if ( strcasecmp(data, "false")==0  && IsmecaStatus==true )
            recordGuiData=true;
        else
            recordGuiData=false;

        if ( recordGuiData )
        {
            fprintf(all->newRecordFile, "%d %d %s %s\n",
                all->pbRs232Step, (int) retVal, componentName, data);
        }
#endif

        return retVal;
    }

    if (all->noGUI)
    {
        strcpy(data, "");
        return PH_GUI_ERR_OK;
    }

    retVal = phGuiGetData(guiProcessID, componentName, data, timeout);

    if (all->doRecord)
    {
        bool IsmecaStatus=getIsmecaStatus(all,componentName);
        bool recordGuiData;

        if ( strcasecmp(data, "true")==0  && IsmecaStatus==false )
            recordGuiData=true;
        else if ( strcasecmp(data, "false")==0  && IsmecaStatus==true )
            recordGuiData=true;
        else
            recordGuiData=false;

        if ( recordGuiData )
        {
            fprintf(all->recordFile, "%d %d %s %s\n",
                all->pbRs232Step, (int) retVal, componentName, data);
        }
    }

    return retVal;
}

phGuiError_t phPbGuiChangeValue(
    struct all_t *all,
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    const char *componentName,     /* name of the component to change */
    const char *newValue           /* new value */
)
{
    if (all->noGUI)
        return PH_GUI_ERR_OK;
    else
        return phGuiChangeValue(guiProcessID, componentName, newValue);
}

phGuiError_t phPbGuiDestroyDialog(
    struct all_t *all,
    phGuiProcessId_t process      /* the Gui-ID */
)
{
    if (all->noGUI)
        return PH_GUI_ERR_OK;
    else
        return phGuiDestroyDialog(process);
}

static long timeSinceLotDone(struct all_t *all);

phGuiError_t phPbGuiGetEvent(
    struct all_t *all,
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    phGuiEvent_t *event,           /* the variable where the event type and
                                      the triggering buttonname will be stored */
    int timeout                    /* timeout after which phGuiGetEvent returns */
)
{
    phGuiError_t retVal;

    if (all->autoStartTime > 0 && !all->lotActive)
    {
        if (all->autoStartTime < timeSinceLotDone(all))
        {
            event->type = PH_GUI_PUSHBUTTON_EVENT;
            strcpy(event->name, "p_start");
            event->secval = 0;
            return PH_GUI_ERR_OK;
        }
    }

    if (all->noGUI)
    {
        if (timeout > 0)
            sleep(timeout);
        event->type = PH_GUI_NO_EVENT;
        return PH_GUI_ERR_OK;
    }

    retVal = phGuiGetEvent(guiProcessID, event, timeout);

#if 0
    if (all->doRecord)
    {
        fprintf(all->recordFile, "e %d %d %d %d %s %d\n",
            all->pbRs232Step, all->pbSubStep, (int) retVal,
            (int) event->type, event->name, event->secval);
    }
#endif

    return retVal;
}

phGuiError_t phPbGuiCreateUserDefDialog(
    struct all_t *all,
    phGuiProcessId_t *id      /* the resulting Gui-ID                    */,
    phLogId_t logger          /* the data logger to be used              */,
    phGuiSequenceMode_t mode  /* mode in which the gui should be started */,
    char *description         /* description of the gui-representation   */
)
{
    if (all->noGUI)
        return PH_GUI_ERR_OK;
    else
        return phGuiCreateUserDefDialog(id, logger, mode, description);
}

phGuiError_t phPbGuiShowDialog(
    struct all_t *all,
    phGuiProcessId_t process,      /* the Gui-ID */
    int closeOnReturn              /* if set to 1 dialog will close when
                                      exit button is pressed */
)
{
    if (all->noGUI)
        return PH_GUI_ERR_OK;
    else
        return phGuiShowDialog(process, closeOnReturn);
}

/*--- functions -------------------------------------------------------------*/

static void doExitFail(struct all_t *all)
{
    phPbGuiDestroyDialog(all, all->guiId);
    all->guiId = 0;
    //phComGpibClose(all->comId);
    phComRs232Close(all->comId);
    //phComGpibCloseVXI11(all->comId);
    phLogDestroy(all->logId);
    exit(1);
}

static int checkGuiError(phGuiError_t guiError, struct all_t *all)
{
    switch (guiError)
    {
      case PH_GUI_ERR_OK:
        break;
      case PH_GUI_ERR_TIMEOUT:
        phLogSimMessage(all->logId, LOG_VERBOSE,
            "received timeout error from gui server");
        break;
      default:
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
            "received error code %d from gui server",
            (int) guiError);
        return 0;
    }
    return 1;
}

static int checkComError(phComError_t comError, struct all_t *all)
{
    switch (comError)
    {
      case PHCOM_ERR_OK:
        return 0;
      case PHCOM_ERR_TIMEOUT:
        phLogSimMessage(all->logId, LOG_VERBOSE,
            "received timeout error from com layer");
        return 1;
      default:
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
            "received error code %d from com layer",
            (int) comError);
        return 2;
    }
}

/*---------------------------------------------------------------------------*/

static void beAlife(struct all_t *all)
{
    static int b = 1;

    if (b == 1)
    {
        b = 2;
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "r_life_1", "False"), all);
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "r_life_2", "True"), all);
    }
    else
    {
        b = 1;
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "r_life_2", "False"), all);
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "r_life_1", "True"), all);
    }
}

static void setWaiting(int wait, struct all_t *all)
{
    if (wait)
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "waiting ..."), all);
    else
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "OK"), all);
}

static void setTimeout(int timeout, struct all_t *all)
{
    if (timeout)
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "timed out"), all);
    else
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "OK"), all);
}

static void setSent(int sent, struct all_t *all)
{
    if (sent)
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "sent"), all);
    else
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "not sent"), all);
}

static void setError(int error, struct all_t *all)
{
    if (error)
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "error, see log"), all);
    else
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "OK"), all);
}

static void setNotUnderstood(int error, struct all_t *all)
{
    if (error)
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "not understood"), all);
    else
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "OK"), all);
}

static void doDelay(int seconds, struct all_t *all)
{
    char delay[MAX_MESSAGE_LENGTH];
    char typed[MAX_MESSAGE_LENGTH];

    while (seconds > 0)
    {
        sprintf(delay, "%d", seconds);
        checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_delay", delay), all);
        seconds--;
        sleep(1);
    }
    checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_delay", ""), all);

    if (all->interactiveDelay)
    {
        if (all->interactiveDelayCount > 0)
        {
            all->interactiveDelayCount--;
        }
        else
        {
            printf("press <enter> to go on one step or enter number of steps: ");
            do ;
            while (!fgets(typed, 79, stdin));

            if (!sscanf(typed, "%d", &all->interactiveDelayCount))
                all->interactiveDelayCount = 0;
        }
    }
}

/*---------------------------------------------------------------------------*/

static void sendMessageEoc(struct all_t *all, const char *m, const char *eoc)
{
    char message[MAX_MESSAGE_LENGTH];
    int sent = 0;
    phGuiError_t guiError;
    phComError_t comError;

    strcpy(message, m);
    guiError = phPbGuiChangeValue(all, all->guiId, "f_answ", message);
    checkGuiError(guiError, all);

    strcat(message, eoc);
    phLogSimMessage(all->logId, LOG_VERBOSE,
        "trying to send message \"%s\"", m);

    do
    {
        setWaiting(1, all);
        comError = phComRs232Send(all->comId, message, strlen(message),
        //comError = phComGpibSendVXI11(all->comId, message, strlen(message),
            1000L * all->timeout);
        setWaiting(0, all);
        switch (checkComError(comError, all))
        {
          case 0:
            /* OK */
            sent = 1;
            break;
          case 1:
            checkGuiEvent(all);
            setTimeout(1, all);
            doDelay(5, all);
            break;
          case 2:
          default:
            checkGuiEvent(all);
            setError(1, all);
            break;
        }
    } while (!sent);

    setSent(1, all);
    phLogSimMessage(all->logId, LOG_DEBUG,
        "message \"%s\" sent", m);
}

static void sendMessage(struct all_t *all, const char *m)
{
    sendMessageEoc(all,m,all->eoc);
    return;
}


/*---------------------------------------------------------------------------*/

static long timeSinceLotDone(struct all_t *all)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    return (now.tv_sec - all->lotDoneTime);
}


static phLogMode_t getLogMode(struct all_t *all)
{
    phLogMode_t modeFlags = PHLOG_MODE_NORMAL;

    /* open message logger */
    switch (all->debugLevel)
    {
      case -1:
        modeFlags = PHLOG_MODE_QUIET;
        break;
      case 0:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_0 );
        break;
      case 1:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_1 );
        break;
      case 2:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_2 );
        break;
      case 3:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_3 );
        break;
      case 4:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_4 );
        break;
      case 5:
        modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_TRACE | PHLOG_MODE_DBG_4 );
        break;
    }

    return modeFlags;
}


static void incDebugLevel(struct all_t *all)
{
    ++all->debugLevel;

    if (all->debugLevel > 5)
        all->debugLevel=5;

    phLogSetMode(all->logId, getLogMode(all) );
}

static void decDebugLevel(struct all_t *all)
{
    --all->debugLevel;

    if (all->debugLevel < -1)
        all->debugLevel=-1;

    phLogSetMode(all->logId, getLogMode(all) );
}



/*---------------------------------------------------------------------------*/

static int check_assist(struct all_t *all)
{
    char response[MAX_RESPONSE_LENGTH];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_assist", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
        phPbGuiChangeValue(all, all->guiId, "t_assist", "false");
        return F_ASSIST;
    }
    return 0;
}

static int check_fatal(struct all_t *all)
{
    char response[MAX_RESPONSE_LENGTH];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_fatal", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
        phPbGuiChangeValue(all, all->guiId, "t_fatal", "false");
        return F_FATAL;
    }
    return 0;
}

static int check_stop(struct all_t *all)
{
    char response[MAX_RESPONSE_LENGTH];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_stop", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
        if ( all->ismecaStatus->getStopped() == false )
        {
            phLogSimMessage(all->logId, LOG_DEBUG, "stop detected");
            all->operatorPause=true;
            all->ismecaStatus->setStopped(true);
            all->handler->stop();
        }
        return F_STOP;
    }
    else
    {
        if ( all->ismecaStatus->getStopped() == true )
        {
            if ( all->ismecaStatus->getHandlerEmpty() == true )
            {
                all->handler->reset();
                all->ismecaStatus->setHandlerEmpty(false);
            }
            if ( all->ismecaStatus->getHandlerIsReady() == true )
            {
                phLogSimMessage(all->logId, LOG_DEBUG, "start detected");
                all->ismecaStatus->setStopped(false);
                all->handler->start();
            }
        }
        return 0;
    }
}


static int check_jam(struct all_t *all)
{
    char response[MAX_RESPONSE_LENGTH];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_jam", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
        if ( all->ismecaStatus->getJammed() == false )
        {
            phLogSimMessage(all->logId, LOG_DEBUG, "jam in handler detected");
            all->ismecaStatus->setJammed(true);
            all->operatorPause=false;
            all->ismecaStatus->setStopped(true);
            phPbGuiChangeValue(all, all->guiId, "t_stop", "true");
            all->handler->stop();
        }
        return F_JAM;
    }
    else
    {
        if ( all->ismecaStatus->getJammed() == true )
        {
            phLogSimMessage(all->logId, LOG_DEBUG, "handler freed");
            all->ismecaStatus->setJammed(false);
            if ( all->ismecaStatus->getHandlerIsReady() == true )
            {
                all->ismecaStatus->setStopped(false);
                phPbGuiChangeValue(all, all->guiId, "t_stop", "false");
                all->handler->start();
            }
        }
        return 0;
    }
}


static int check_dooropen(struct all_t *all)
{
    char response[MAX_RESPONSE_LENGTH];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_dooropen", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
        if ( all->ismecaStatus->getDoorOpen() == false )
        {
            phLogSimMessage(all->logId, LOG_DEBUG, "door open detected");
            all->ismecaStatus->setDoorOpen(true);
            all->operatorPause=true;
            all->ismecaStatus->setStopped(true);
            phPbGuiChangeValue(all, all->guiId, "t_stop", "true");
            all->handler->stop();
        }
        return F_DOOROPEN;
    }
    else
    {
        if ( all->ismecaStatus->getDoorOpen() == true )
        {
            phLogSimMessage(all->logId, LOG_DEBUG, "door closed detected");
            all->ismecaStatus->setDoorOpen(false);
            if ( all->ismecaStatus->getHandlerIsReady() == true )
            {
                all->ismecaStatus->setStopped(false);
                phPbGuiChangeValue(all, all->guiId, "t_stop", "false");
                all->handler->start();
            }
        }
        return 0;
    }
}







static int check_pause(struct all_t *all)
{
    char response[MAX_RESPONSE_LENGTH];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_pause", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
        phLogSimMessage(all->logId, LOG_DEBUG, "unset prober pause flag to resume");
        do
        {
            phPbGuiGetData(all, all->guiId, "t_pause", response, 0);
        } while (strcasecmp(response, "true") == 0);
        return F_PAUSE;
    }
    return 0;
}

static int check_flags(struct all_t *all, int flags)
{
    int flags_handled = 0;

    if (flags & F_ASSIST)
        flags_handled |= check_assist(all);
    if (flags & F_FATAL)
        flags_handled |= check_fatal(all);
    if (flags & F_PAUSE)
        flags_handled |= check_pause(all);
    if (flags & F_STOP)
        flags_handled |= check_stop(all);
    if (flags & F_JAM)
        flags_handled |= check_jam(all);
    if (flags & F_DOOROPEN)
        flags_handled |= check_dooropen(all);

    return flags_handled;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/


/*--- service functions -----------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static void service_ackEnqId(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "service_ackEnqId handling \"%s\"",m);

    sendMessage(all, m);
    return;
}

static void service_requestError(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "service_requestError handling \"%s\"",m);

    char message[MAX_MESSAGE_LENGTH];
    if (all->ismecaStatus->getHandlerEmpty()==true)
    {
        sprintf(message, "%c%c%c", 0x02,0x4B, 0x03);
    }
    else
    {
        sprintf(message, "%c%c", 0x02, 0x03);
    }
    sendMessage(all, message);
    return;
}

static void service_requestStartTest(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "service_requestStartTest handling \"%s\"",m);

    int devicesReady = 0;
    char message[MAX_MESSAGE_LENGTH] = "";
    //get device for testing
    if(all->handler->sendTestStartSignal())
    {
        devicesReady=all->handler->getSitePopulationAsInt();
    }
    switch (devicesReady)
    {
    case 1:
      //site 1
      sprintf(message, "%c%c%c", 0x02, 0x31, 0x03);
      break;
    case 2:
      //site 2
      sprintf(message, "%c%c%c", 0x02, 0x32, 0x03);
      break;
    case 3:
      //site 1,2
      sprintf(message, "%c%c%c%c%c", 0x02, 0x31, 0x2C, 0x32, 0x03);
      break;
    case 4:
      //site 3
      sprintf(message, "%c%c%c", 0x02, 0x33, 0x03);
      break;
    case 5:
      //site 1,3
      sprintf(message, "%c%c%c%c%c", 0x02, 0x31, 0x2C, 0x33, 0x03);
      break;
    case 6:
      //site 2,3
      sprintf(message, "%c%c%c%c%c", 0x02, 0x32,0x2C,0x33, 0x03);
      break;
    case 7:
      //site 1,2,3
      sprintf(message, "%c%c%c%c%c%c%c", 0x02, 0x31, 0x2C, 0x32, 0x2C, 0x33, 0x03);
      break;
    case 8:
      //site 4
      sprintf(message, "%c%c%c", 0x02, 0x34, 0x03);
      break;
    case 9:
      //site 1,4
      sprintf(message, "%c%c%c%c%c", 0x02, 0x31, 0x2C, 0x34, 0x03);
      break;
    case 10:
      //site 2,4
      sprintf(message, "%c%c%c%c%c", 0x02, 0x32, 0x2C, 0x34, 0x03);
      break;
    case 11:
      //site 1,2,4
      sprintf(message, "%c%c%c%c%c%c%c", 0x02, 0x31, 0x2C, 0x32, 0x2C, 0x34, 0x03);
      break;
    case 12:
      //site 3,4
      sprintf(message, "%c%c%c%c%c", 0x02, 0x33, 0x2C, 0x34, 0x03);
      break;
    case 13:
      //site 1,3,4
      sprintf(message, "%c%c%c%c%c%c%c", 0x02, 0x31, 0x2C, 0x33, 0x2C, 0x34, 0x03);
      break;
    case  14:
      //site 2,3,4
      sprintf(message, "%c%c%c%c%c%c%c", 0x02, 0x32, 0x2C, 0x33, 0x2C, 0x34, 0x03);
      break;
    case 15:
       //site 1,2,3,4
      sprintf(message, "%c%c%c%c%c%c%c%c%c", 0x02, 0x31, 0x2C, 0x32, 0x2C, 0x33, 0x2C, 0x34, 0x03);
       break;
    default:
      break;
    }
    sendMessage(all, message);
    return;
}

static void service_bin(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "service_bin handling \"%s\"",m);
    check_flags(all, F_STOP | F_JAM | F_DOOROPEN );

    /*
     * Sends selected part into designated category.
     * Format: <STX>BAsiteNum,0,binNum;...<ETX>
     *         example: <STX>BA1,0,3;2,0,5;4,0,2<ETX>
     *         ...
     */

    char *end = NULL;
    char *start = NULL;
    char siteOrBin[MAX_SITE_LENGTH] = "";
    int count = 0;
    int site = 0;
    int bin = 0;

    strcpy(siteOrBin, m+3);

    end = strtok(siteOrBin, ";");

    do
    {
        start = end;
        end = strchr(end, ',');
        if (end != NULL)
        {
           end++;
        }
        sscanf(start, "%100[1-9]", siteOrBin);

        count++;
        if (count == 1)
        {
            site = atoi(siteOrBin);
        }
        if (count == 3)
        {
            bin = atoi(siteOrBin);
            if ( all->verifyTest )
            {
                all->handler->setBinData(site,bin);
            }
            else
            {
                all->handler->sendDeviceToBin(site,bin);
            }
            end=strtok(NULL,";");
            count = 0;
        }
    }while(end != NULL);


    return;
}

static void func_b_startLot(struct all_t *all)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"Start\"");
    doDelay(0, all);
}

/*---------------------------------------------------------------------------*/


static void setStrlower(char *s1)
{
    int i=0;

    while ( s1[i] )
    {
        s1[i]=tolower( s1[i] );
        ++i;
    }
}

static void removeEoc(char *s1)
{
    int lastChar;
    bool eocChar;
    do {
       lastChar=strlen(s1)-1;

       if ( lastChar >= 0 )
       {
           eocChar=s1[lastChar]=='\n' || s1[lastChar]=='\r';
       }
       else
       {
           eocChar=false;
       }

       if ( eocChar )
       {
           s1[lastChar]='\0';
       }

    } while ( eocChar );
}


static void handleMessage(struct all_t *all)
{
    char m[MAX_MESSAGE_LENGTH];
    char *message = NULL;

    static int requestHID = 0;
    static int requestError = 0;
    static int requestST = 0;

    static int ack_hex = 0x06;
    static int enq_hex = 0x05;
    static int stx_hex = 0x02;

    static char ack[MAX_MESSAGE_LENGTH] = "";
    static char enq[MAX_MESSAGE_LENGTH] = "";
    static char handlerID[MAX_HANDLRID_LENGTH] = "";
    sprintf(ack, "%c", ack_hex);
    sprintf(enq, "%c", enq_hex);
    sprintf(handlerID, "%c%s%c", 0x02, "NX116", 0x03);

    /* copy string remove eoc and set to lowercase */
    strcpy(m, all->message);
    removeEoc(m);
    setStrlower(m);
    message = m;
    /* show message and log it */
    phPbGuiChangeValue(all, all->guiId, "f_req", m);
    phLogSimMessage(all->logId, LOG_DEBUG, "received %3d: \"%s\" ", all->pbRs232Step, all->message);

    phPbGuiChangeValue(all, all->guiId, "f_answ", "");

    /* handle the message */
    setNotUnderstood(0, all);

    if(phToolsMatch(m, enq))//0x05
    {
        service_ackEnqId(all, ack);//send ack(0x06)
    }
    else if (*message == stx_hex)//0x02
    {
        message++;
        if(phToolsMatch(message, "cd\x03$") || phToolsMatch(message, "ca\x03$")
            || phToolsMatch(message, "ce\x03$"))
        {
          service_ackEnqId(all, enq);//send enq(0x05)
        }
        else if (phToolsMatch(message, "ba([1-9],0,[1-9](;)?)+\x03$"))
        {
            service_bin(all,m);
        }
    }
    else if(phToolsMatch(m, ack))//0x06
    {
        if (requestHID == 0)
        {
            //send Handler ID
            service_ackEnqId(all, handlerID);//send handler ID
            requestHID = 1;
        }
        else if (requestError == 0)
        {
            //send Request Error
            service_requestError(all, m);
            requestError = 1;
            requestST = 0;
        }
        else if (requestST == 0)
        {
            //send Request Error
            service_requestStartTest(all, m);
            requestST = 1;
            requestError = 0;
        }
    }
    else
    {
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,"not understood \"%s\"",m);
        setNotUnderstood(1, all);
    }
}

/*---------------------------------------------------------------------------*/

static void handleGuiEvent(struct all_t *all)
{
    phGuiError_t guiError;
    char request[MAX_MESSAGE_LENGTH];
    phComError_t comError;

    switch (all->guiEvent.type)
    {
      case PH_GUI_NO_EVENT:
        break;
      case PH_GUI_NO_ERROR:
        break;
      case PH_GUI_COMTEST_RECEIVED:
        break;
      case PH_GUI_IDENTIFY_EVENT:
        break;
      case PH_GUI_ERROR_EVENT:
        break;
      case PH_GUI_PUSHBUTTON_EVENT:
      case PH_GUI_EXITBUTTON_EVENT:
        /* first clear the event queue display */
        strcpy(request, "");
        guiError = phPbGuiChangeValue(all, all->guiId, "f_event", request);
        checkGuiError(guiError, all);

        /* now handle the button press event */
        if (strcmp(all->guiEvent.name, "p_quit") == 0)
        {
            phPbGuiDestroyDialog(all, all->guiId);
        all->guiId = 0;
            all->done = 1;

    }
        else if (strcmp(all->guiEvent.name, "p_start") == 0)
        {
            func_b_startLot(all);
        }
        else if (strcmp(all->guiEvent.name, "p_dump") == 0)
        {
            phLogDump(all->logId, PHLOG_MODE_ALL);
        }
        else if (strcmp(all->guiEvent.name, "p_incDebug") == 0)
        {
            incDebugLevel(all);
        }
        else if (strcmp(all->guiEvent.name, "p_decDebug") == 0)
        {
            decDebugLevel(all);
        }
        else if(strcmp(all->guiEvent.name, "p_req") == 0)
        {
            guiError = phPbGuiChangeValue(all, all->guiId, "f_req", "");
            checkGuiError(guiError, all);

            setWaiting(1, all);
            comError = phComRs232Receive(all->comId, &all->message, &all->length, 1000L * all->timeout);
            //comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length, 1000L * all->timeout);
            setWaiting(0, all);
            switch (checkComError(comError, all))
            {
              case 0:
                strcpy(request, all->message);
                if (strlen(request) >= strlen(all->eoc) &&
                    strcmp(&request[strlen(request) - strlen(all->eoc)], all->eoc) == 0)
                    request[strlen(request) - strlen(all->eoc)] = '\0';
                break;
              case 1:
                strcpy(request, "");
                setTimeout(1, all);
                break;
              case 2:
              default:
                strcpy(request, "");
                setError(1, all);
                break;
            }
            guiError = phPbGuiChangeValue(all, all->guiId, "f_req", request);
            checkGuiError(guiError, all);
        }
        else if(strcmp(all->guiEvent.name, "p_answ") == 0)
        {
            guiError = phPbGuiGetData(all, all->guiId, "f_answ", request, 5);
            if (!checkGuiError(guiError, all))
                return;
            phLogSimMessage(all->logId, LOG_DEBUG,
                "trying to send message '%s'", request);
            strcat(request, all->eoc);
            setWaiting(1, all);
            comError = phComRs232Send(all->comId, request, strlen(request), 1000L * all->timeout);
            //comError = phComGpibSendVXI11(all->comId, request, strlen(request), 1000L * all->timeout);
            setWaiting(0, all);
            switch (checkComError(comError, all))
            {
              case 0:
                guiError = phPbGuiChangeValue(all, all->guiId, "f_answ", "");
                checkGuiError(guiError, all);
                break;
              case 1:
                setTimeout(1, all);
                break;
              case 2:
              default:
                setError(1, all);
                break;
            }
        }
        else if(strcmp(all->guiEvent.name, "p_event") == 0)
        {
            strcpy(request, "");
            guiError = phPbGuiChangeValue(all, all->guiId, "f_event", request);
            checkGuiError(guiError, all);
        }
        break;
    }
}

/*---------------------------------------------------------------------------*/

static void checkGuiEvent(struct all_t *all)
{
    phGuiError_t guiError;

    guiError = phPbGuiGetEvent(all, all->guiId, &all->guiEvent, 0);
    checkGuiError(guiError, all);
    if (guiError == PH_GUI_ERR_OK)
    {
        handleGuiEvent(all);
    }
    return;
}

static void checkRs232Received(struct all_t *all)
{
    phComError_t comError;
    comError = phComRs232Receive(all->comId, &all->message, &all->length,
                                1000000L);
    //comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length,
                                //1000000L);
    if (checkComError(comError, all) == 0)
    {
        doRs232Step(all);
        handleMessage(all);
    }
}

/*---------------------------------------------------------------------------*/


static void doWork(struct all_t *all)
{

    setError(0, all);
    all->done = 0;
    do
    {
        beAlife(all);
        checkRs232Received(all);
        checkGuiEvent(all);

        if ( all->handler->allTestsComplete() && all->ismecaStatus->getHandlerEmpty()==false )
        {
            all->handler->stop();
            all->handler->logHandlerStatistics();

            all->ismecaStatus->setHandlerEmpty(true);
            all->operatorPause=false;
            all->ismecaStatus->setStopped(true);
            phPbGuiChangeValue(all, all->guiId, "t_stop", "true");
#ifdef SAVE_NEW_PLAYBACK_FORMAT
            fclose(all->newRecordFile);
#endif
        }

        doSubStep(all);
    } while (!all->done);

    if ( !all->ismecaStatus->getHandlerEmpty() )
    {
        all->handler->logHandlerStatistics();
    }
}


/*---------------------------------------------------------------------------*/

int arguments(int argc, char *argv[], struct all_t *all)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;

    /* setup default: */

    while ((c = getopt(argc, argv, ":d:i:b:w:B:f:y:e1:e2:e3:t:m:s:M:p:r:R:V:D:N:ISo:P:T:n")) != -1)
    {
        switch (c)
        {
          case 'd':
            all->debugLevel = atoi(optarg);
            break;
          case 'i':
            strcpy(all->rs232Config.device, optarg);
            break;
          case 'b':
            all->rs232Config.baudRate = atol(optarg);
            break;
          case 'w':
            all->rs232Config.dataBits = atol(optarg);
            break;
          case 'B':
            all->rs232Config.stopBits = atol(optarg);
            break;
          case 'f':
            strcpy(all->rs232Config.flowControl, optarg);
            break;
          case 'y':
            strcpy(all->rs232Config.parity, optarg);
            break;
          case '1':
            all->rs232Config.eois[0] = atol(optarg);
            break;
          case '2':
            all->rs232Config.eois[1] = atol(optarg);
            break;
          case '3':
            all->rs232Config.eois[2] = atol(optarg);
            break;
          case 't':
            all->timeout = atoi(optarg);
            break;
          case 'm':
            all->modelname = strdup(optarg);
            if (strcmp(optarg, "NX16") == 0) all->model = NX16;
            else if (strcmp(optarg, "NY20") == 0) all->model = NY20;
            else errflg++;
            break;
          case 's':
            all->numOfSites = atoi(optarg);
            break;
          case 'M':
            all->siteEnabledMask = atol(optarg);
            break;
          case 'p':
            if (strcmp(optarg, "one") == 0) all->pattern = Handler::all_one_not_one_is_working;
            else if (strcmp(optarg, "all") == 0) all->pattern = Handler::all_sites_working;
            else errflg++;
            break;
          case 'R':
            if ( sscanf(optarg,"%d",&all->retestBinNumber) != 1 )
                errflg++;
            break;
          case 'r':
            if (strcmp(optarg, "none") == 0) all->reprobeMode = Handler::ignore_reprobe;
            else if (strcmp(optarg, "sep") == 0) all->reprobeMode = Handler::perform_reprobe_separately;
            else if (strcmp(optarg, "add") == 0) all->reprobeMode = Handler::add_new_devices_during_reprobe;
            else
            {
                if ( sscanf(optarg,"%d",&all->reprobeBinNumber) != 1 )
                    errflg++;
            }
            break;
          case 'V':
            if (strcmp(optarg, "none") == 0)
            {
                all->verifyTest = 0;
                all->corruptBinData = false;
                all->releasepartsWhenReady = false;
            }
            else if (strcmp(optarg, "vok") == 0)
            {
                all->verifyTest = 1;
                all->corruptBinData = false;
            }
            else if (strcmp(optarg, "verr") == 0)
            {
                all->verifyTest = 1;
                all->corruptBinData = true;
            }
            else if (strcmp(optarg, "vrp") == 0)
            {
                all->verifyTest = 1;
                all->releasepartsWhenReady = false;
            }
            else if (strcmp(optarg, "vrpi") == 0)
            {
                all->verifyTest = 1;
                all->releasepartsWhenReady = true;
            }
            else errflg++;
            break;
          case 'D':
            all->handlingDelay = atof(optarg) * 1000;
            break;
          case 'N':
            all->numberOfDevicesToTest = atoi(optarg);
            break;

          case 'I':
            all->interactiveDelay = 1;
            break;
          case 'o':
            if (strcmp(optarg, "-") == 0)
                all->recordFile = stdout;
            else
                all->recordFile = fopen(optarg, "w");
            if (all->recordFile)
                all->doRecord = 1;
            break;

          case 'P':
            if (strcmp(optarg, "-") == 0)
                all->playbackFile = stdin;
            else
                all->playbackFile = fopen(optarg, "r");
            if (all->playbackFile)
                all->doPlayback = 1;
            break;

          case 'T':
            if ( strcmp(optarg, "1") == 0 )
            {
                all->eoc = "";
            }
            else if ( strcmp(optarg, "2") == 0 )
            {
                all->eoc = "\r\n";
            }
            else if (strcmp(optarg, "3") == 0)
            {
                all->eoc = "\n";
            }
            else errflg++;
            break;
          case 'n':
            all->noGUI = 1;
            break;
          case ':':
            errflg++;
            break;
          case '?':
            fprintf(stderr, "Unrecognized option: - %c\n", (char) optopt);
            errflg++;
        }
        if (errflg)
        {
            fprintf(stderr, "usage: %s [<options>]\n", argv[0]);
            fprintf(stderr, " -d <debug-level>       -1 to 4, default: 0\n");

            fprintf(stderr, " -m <prober model>       prober model, one of:\n");
            fprintf(stderr, "                         NX16,NY20, default: NY20\n");
            fprintf(stderr, " -s <number>             number of sites, default: 4\n");
            fprintf(stderr, " -M <number>             site enabled mask, default all sites enabled \n");
            fprintf(stderr, " -p <pattern>            site population pattern, one of: \n");
            fprintf(stderr, "                         one (all sites, one not, one is working), \n");
            fprintf(stderr, "                         all (all sites working), default: all\n");
            fprintf(stderr, " -R <number>             retest bin number, default -1 \n");
            fprintf(stderr, " -r <reprobe_mode>       handler reprobe mode, one of: \n");
            fprintf(stderr, "                         none (ignore reprobes and follow <pattern>) \n");
            fprintf(stderr, "                         sep (perform reprobes separately), \n");
            fprintf(stderr, "                         add (add new devices during reprobe), default sep \n");
            fprintf(stderr, "                         <number> reprobe bin number, default -2 \n");
            fprintf(stderr, " -D <delay>              parts handling delay [sec], default: 1\n");
            fprintf(stderr, " -N <number>             number of parts to be handled before \n");
            fprintf(stderr, "                         end of test, default: infinite\n");
            fprintf(stderr, " -V <verify_mode>        verify test mode, one of: \n");
            fprintf(stderr, "                         none (no verification, normal binning), \n");
            fprintf(stderr, "                         vok  (devices must be verified but always correct), \n");
            fprintf(stderr, "                         verr (devices must be verified bin data corrupted  \n");
            fprintf(stderr, "                               for every 11th and then 4th set bin command), \n");
            fprintf(stderr, "                         vrp  (releaseparts command accepted: each populated  \n");
            fprintf(stderr, "                               site with bin information is released), \n");
            fprintf(stderr, "                         vrpi (releaseparts command is ignored if all populated \n");
            fprintf(stderr, "                               sites have not received bin information), \n");
            fprintf(stderr, "                               default: none (when verification set: vok vrp) \n");

            fprintf(stderr, " -i <rs232-interface>    RS232 interface device, default: rs232/dev/usb/ttyUSB0 \n");
            fprintf(stderr, " -b <rs232-interface>    RS232 interface baud rate, default: 9600 \n");
            fprintf(stderr, " -w <rs232-interface>    RS232 interface date bits, default: 8 \n");
            fprintf(stderr, " -B <rs232-interface>    RS232 interface stop bits, default: 1 \n");
            fprintf(stderr, " -f <rs232-interface>    RS232 interface flow control, default: none \n");
            fprintf(stderr, " -y <rs232-interface>    RS232 interface parity, default: none \n");
            fprintf(stderr, " -1 <rs232-interface>   RS232 interface end of input data(first), default: 0x03 \n");
            fprintf(stderr, " -2 <rs232-interface>   RS232 interface end of input data(second), default: 0x05 \n");
            fprintf(stderr, " -3 <rs232-interface>   RS232 interface end of input data(third), default: 0x06 \n");
            fprintf(stderr, " -t <timeout>            RS232 timeout to be used [msec], default: 5000\n");

            fprintf(stderr, " -T <term string>        terminate message string, one of:""");
            fprintf(stderr, "                         1 - "", 2 - \\r\\n, 3 - \\n, default: "" \n");

            fprintf(stderr, " -o <record file>        record GUI states\n");
            fprintf(stderr, " -P <playback file>      play back GUI states\n");

            fprintf(stderr, " -n                      no GUI\n");

            return 0;
        }
    }
    return 1;
}


int main(int argc, char *argv[])
{
    //char szHostName[MAX_NAME_LENTH] = {0};
    //gethostname(szHostName, MAX_NAME_LENTH-1);

    //char szVxi11Server[MAX_PATH_LENTH] = {0};
    //getVxi11Path(szVxi11Server, MAX_PATH_LENTH);

    //char szCmdline[MAX_CMD_LINE_LENTH] = {0};
    //snprintf(szCmdline, MAX_CMD_LINE_LENTH, "%s -ip %s 1>/dev/null &", szVxi11Server, szHostName);

    //system(szCmdline);
    
    //sleep(1);

    phGuiError_t guiError;
    static char slaveGuiDesc[MAX_GUI_DESC_LENGTH];

    /* Disable the generation of the HVM alarm */
    setenv("DISABLE_HVM_ALARM_GENERATION_FOR_TCI", "1", 1);

    /* init defaults */

    allg.debugLevel = 0;

    allg.rs232Config.baudRate = 9600;
    allg.rs232Config.dataBits = 8;
    allg.rs232Config.stopBits = 1;
    allg.rs232Config.eoisNumber = 3;
    strcpy(allg.rs232Config.device, "rs232/dev/pts/5");
    allg.rs232Config.eois[0] = 0x03;
    allg.rs232Config.eois[1] = 0x05;
    allg.rs232Config.eois[2] = 0x06;
    strcpy(allg.rs232Config.flowControl, "none");
    strcpy(allg.rs232Config.parity, "none");

    allg.softwareVersion = strdup("3.0");
    allg.handlerNumber = strdup("H-XYZ-001");
    allg.timeout = 5000;


    allg.modelname = strdup("NY20");
    allg.model = NY20;

    allg.noGUI = 0;
    allg.autoStartTime = 0;

    allg.interactiveDelay = 0;
    allg.interactiveDelayCount = 0;

    allg.pbRs232Step = 0;
    allg.pbSubStep = 0;
    allg.doRecord = 0;
    allg.doPlayback = 0;

    allg.recordFile = NULL;
    allg.playbackFile = NULL;
    allg.newRecordFile = NULL;

    allg.numOfSites=4;
    allg.numberOfDevicesToTest=1000;
    allg.handlingDelay=1000;
    allg.siteEnabledMask=allSitesEnabled;
    allg.pattern=Handler::all_sites_working;
    allg.reprobeMode=Handler::perform_reprobe_separately;

    allg.verifyTest=0;
    allg.corruptBinData=false;
    allg.releasepartsWhenReady=false;
    allg.cmdReply=0;
    allg.systemMode=1;
    allg.emulationMode=1;
    allg.errorBit=0;

    allg.retestBinNumber=-1;
    allg.reprobeBinNumber=-2;


    strcpy(allg.recordName,"");
    strcpy(allg.recordData,"");
    allg.recordRetval=0;
    allg.recordStep=-1;
    allg.recordUsed=true;

    allg.eoc="";

    /* get real operational parameters */
    if (!arguments(argc, argv, &allg))
        exit(1);


    phLogInit(&allg.logId, getLogMode(&allg) , "-", NULL, "-", NULL, -1);

    /* open rs232 communication */
    if (phComRs232Open(&allg.comId, PHCOM_ONLINE, allg.rs232Config, allg.logId) != PHCOM_ERR_OK)
    //if (phComGpibOpenVXI11(&allg.comId, PHCOM_ONLINE, allg.rs232Config, allg.logId) != PHCOM_ERR_OK)
    {
        exit(1);
    }


    /* create user dialog */
    strcpy(slaveGuiDesc,slaveGuiDescHead);
    strcat(slaveGuiDesc,"Ismeca ");
    strcat(slaveGuiDesc,allg.modelname);
    strcat(slaveGuiDesc,slaveGuiDescTail);
    guiError = phPbGuiCreateUserDefDialog(&allg, &allg.guiId, allg.logId,
        PH_GUI_COMM_ASYNCHRON,  slaveGuiDesc);
    checkGuiError(guiError, &allg);
    if (guiError != PH_GUI_ERR_OK)
        exit(1);
    phPbGuiShowDialog(&allg, allg.guiId, 0);


    /* create handler object */
    allg.handler = new Handler( allg.logId,
                                allg.numOfSites,
                                allg.siteEnabledMask,
                                allg.handlingDelay,
                                allg.numberOfDevicesToTest,
                                allg.pattern,
                                allg.reprobeMode,
                                allg.corruptBinData);

    /* create Ismeca Status object */
    allg.ismecaStatus = new IsmecaStatus( allg.logId );

    /* set reprobe and retest bin numbers */

    allg.handler->setRetestCategory( allg.retestBinNumber );
    allg.handler->setReprobeCategory( allg.reprobeBinNumber );

    allg.operatorPause=false;
    allg.ismecaStatus->setStopped(true);
    phPbGuiChangeValue(&allg, allg.guiId, "t_stop", "true");
#ifdef SAVE_NEW_PLAYBACK_FORMAT
    allg.newRecordFile = fopen("newRecordFile", "w");

    if ( ! allg.newRecordFile )
        doExitFail(&allg);
#endif
    /* enter work loop */
    allg.handler->start();
    doWork(&allg);

    exit(0);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
