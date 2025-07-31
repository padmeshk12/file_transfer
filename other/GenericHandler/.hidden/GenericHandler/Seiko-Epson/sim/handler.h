/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : handler.h
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Interface header for handler simulator
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 17 Jan 2001, Chris Joyce, created
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

#ifndef _HANDLER_H_
#define _HANDLER_H_

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

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "logObject.h"
#include "simHelper.h"


/*--- defines ---------------------------------------------------------------*/
const int handlerContinuousTesting = -1;
const int allSitesEnabled = -1;
const int siteNotYetAssignedBinNumber = -1;

/*--- typedefs --------------------------------------------------------------*/

typedef Std::map<int, int> siteStatType;
typedef siteStatType::iterator siteStatIterator;




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
 * Handler Site Pattern
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Possible set of site patterns that the handler can provide.
 *
 *****************************************************************************/

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


/*****************************************************************************
 *
 * Handler CorruptBinDataScheme
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Generic handler simulator
 *
 *****************************************************************************/

class CorruptBinDataScheme : public LogObject
{
public:
    CorruptBinDataScheme(phLogId_t l);
    virtual int corruptBin(int s, int b)=0;
};

class NoDataCorruptionScheme : public CorruptBinDataScheme
{
public:
    NoDataCorruptionScheme(phLogId_t l);
    virtual int corruptBin(int s, int b);
};

class CorruptBinDataOnceScheme : public CorruptBinDataScheme
{
public:
    CorruptBinDataOnceScheme(phLogId_t l, int c1);
    virtual int corruptBin(int s, int b);
private:
    int corruptOnce;
    int binDataCount;
};

class CorruptBinDataTwiceScheme : public CorruptBinDataScheme
{
public:
    CorruptBinDataTwiceScheme(phLogId_t l, int c1, int c2);
    virtual int corruptBin(int s, int b);
private:
    int corruptOnce;
    int corruptTwice;
    int binDataCount;
};


/*****************************************************************************
 *
 * Handler Site
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Generic handler simulator
 *
 *****************************************************************************/

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


/*****************************************************************************
 *
 * Handler 
 *
 * Authors: Chris Joyce
 *
 * Description: 
 * Generic handler simulator
 *
 *****************************************************************************/

struct HandlerSetup;


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

    // Corrupt bin data scheme
    enum eCorruptBinDataScheme { none,
                                 every_17th_bin_data,
                                 every_17th_and_18th_bin_data };

//    Handler(phLogId_t l,
//            int s,
//            long sem,
//            double asud,
//            double hd,
//            int ndt,
//            eHandlerSitePattern p,
//            eHandlerReprobeMode hrm,
//            int mvc,
//            eCorruptBinDataScheme cbds);
    Handler(phLogId_t l,
            const HandlerSetup&);
    ~Handler();
    int getBinningFormat();
    int getNumOfSites();
    bool getSitePopulated(int s);
    unsigned long getSitePopulationAsLong();
    void sendDeviceToBin(int s, int b);
    void setBinData(int s, int b);
    void setContact(int s, bool c);
    int getBinData(int s);
    void releaseDevice(int s);
    bool readyForRelease();
    void start();
    void stop();
    bool isStopped();
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

    // verify bin information
    void resetVerifyCount();
    void binDataVerified();

protected:
    bool populateSites();
    void populateSitesToPattern();
    bool allSitesBinned();
    bool reprobePending();
    long getNumberOfSitesAsLong();
    eHandlerStatus status;
    Timer *binningTimer;
    int numOfSites;
    long siteEnabledMask;
    double autoSetupDelayMsec;
    double handlingDelayMsec;
    const int numOfDevicesToTest;
    int numOfTestedDevices;
    eHandlerReprobeMode handlerReprobeMode;
    SitePattern *sitePattern;
    Std::vector<Site> siteList;
    bool running;
    int retestCat;
    int reprobeCat;
    int nextCorruptBinCount;
    int verifyCount;
    int maxVerifyCount;
    bool autoSetupInit;
    CorruptBinDataScheme *corruptBinDataScheme;
    int binningFormat;

//    bool populate_reprobe_pending_sites;
};



struct HandlerSetup
{
    int numOfSites;
    long siteEnabledMask;
    double autoSetupDelay;
    double handlingDelay;
    int numOfDevicesToTest;
    Handler::eHandlerSitePattern pattern;
    Handler::eHandlerReprobeMode handlerReprobeMode;
    int maxVerifyCount;
    Handler::eCorruptBinDataScheme corruptBinDataScheme;
    int reprobeCat;
    int retestCat;
    int binningFormat;
};


#endif /* ! _HANDLER_H_ */

/*****************************************************************************
 * End of file
 *****************************************************************************/
