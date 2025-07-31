/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : handler.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Generic Handler definition
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
#include <cstring>

/*--- module includes -------------------------------------------------------*/

#include "handler.h"
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

  switch( patternType ) {
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

  if( siteCounter == numOfSites ) {
    siteCounter=0;
    switch( patternType ) {
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

  switch( patternType ) {
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

/*--- CorruptBinDataScheme --------------------------------------------------*/

CorruptBinDataScheme::CorruptBinDataScheme(phLogId_t l) : LogObject(l)
{
}

/*--- NoDataCorruptionScheme ------------------------------------------------*/

NoDataCorruptionScheme::NoDataCorruptionScheme(phLogId_t l) :
CorruptBinDataScheme(l)
{
}

int NoDataCorruptionScheme::corruptBin(int s, int b)
{
  return b;
}

/*--- CorruptBinDataOnceScheme ----------------------------------------------*/

CorruptBinDataOnceScheme::CorruptBinDataOnceScheme(phLogId_t l, int c1) :
CorruptBinDataScheme(l), corruptOnce(c1)
{
  binDataCount=0;
}

int CorruptBinDataOnceScheme::corruptBin(int s, int b)
{
  ++binDataCount;

  if( binDataCount % corruptOnce == 0 ) {
    phLogSimMessage(logId, LOG_DEBUG, "corrupt bin data for site %d from %d to %d ",
                    s+1,b,(b+binDataCount) % 100 );
    b = (b+binDataCount) % 100;
  }

  return b;
}



/*--- CorruptBinDataTwiceScheme ---------------------------------------------*/

CorruptBinDataTwiceScheme::CorruptBinDataTwiceScheme(phLogId_t l, int c1, int c2) :
CorruptBinDataScheme(l), corruptOnce(c1), corruptTwice(c2)
{
  binDataCount=0;
}

int CorruptBinDataTwiceScheme::corruptBin(int s, int b)
{
  ++binDataCount;

  if( binDataCount % corruptOnce == 0 || binDataCount % corruptTwice == 0 ) {
    phLogSimMessage(logId, LOG_DEBUG, "corrupt bin data for site %d from %d to %d ",
                    s+1,b,(b+binDataCount) % 100 );
    b = (b+binDataCount) % 100;
  }

  return b;
}


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

/*--- Site ------------------------------------------------------------------*/

Site::Site(phLogId_t l) : LogObject(l),
status(Site::empty),
binData(siteNotYetAssignedBinNumber),
contact(true)
{
}

bool Site::getSitePopulated()
{
  return(status==waitingBinData || status==waitingRelease);
}

void Site::setSitePopulated(bool p)
{
  if( p ) {
    status=waitingBinData;
  } else {
    status=empty;
  }
}


void Site::sendDeviceToBin(int b)
{
  if( status==empty ) {
    phLogSimMessage(logId, LOG_DEBUG,
                    "Bin data received for a site which is empty: ignored");
  } else {
    binData=b;
    binList.push_back(b);
    status=empty;
  }
}

void Site::setBinData(int b)
{

  if( status==empty ) {
    phLogSimMessage(logId, LOG_DEBUG,
                    "Set bin data received for a site which is empty: ignored");
  } else {
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

  switch( status ) {
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

  switch( status ) {
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

  if( contact != c ) {
    logMessage=true;
    if( contact ) {
      phLogSimMessage(logId, LOG_DEBUG, "retract contact motors ");
    } else {
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

  for( i=binList.begin() ; i != binList.end() ; ++i ) {
    phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ",++c,*i);
  }

  phLogSimMessage(logId, LOG_VERBOSE, "Sort Data ");
  std::sort(binList.begin(), binList.end());
  c=0;
  for( i=binList.begin() ; i != binList.end() ; ++i ) {
    phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ",++c,*i);
  }

  i=binList.begin();
  int n;
  int stat;
  while( i != binList.end() ) {

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

  for( mi=siteStatistics.begin() ; mi != siteStatistics.end() ; ++mi ) {
    stat = (*mi).first;
    n    = (*mi).second;
    phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ",stat,n);
  }


  return siteStatistics;
}




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



/*--- Handler ---------------------------------------------------------------*/

//
// CONSTRUCTOR:
//
// Handler::Handler(phLogId_t l,
//             int s,
//             long sem,
//             double asud,
//             double hd,
//             int ndt,
//             eHandlerSitePattern p,
//             eHandlerReprobeMode hrm,
//             int mvc,
//             eCorruptBinDataScheme cbds) :


Handler::Handler(phLogId_t l,
                 const HandlerSetup& setup) :
LogObject(l),
numOfSites(setup.numOfSites),
siteEnabledMask(setup.siteEnabledMask),
autoSetupDelayMsec(setup.autoSetupDelay),
handlingDelayMsec(setup.handlingDelay),
numOfDevicesToTest(setup.numOfDevicesToTest),
handlerReprobeMode(setup.handlerReprobeMode),
maxVerifyCount(setup.maxVerifyCount)
{
  phLogSimMessage(logId, LOG_VERBOSE, 
                  "Handler::Handler s=%d m=%ld sd=%lf hd=%lf ndt=%d p=%s r=%s m=%d c=%s ",
                  numOfSites, 
                  siteEnabledMask, 
                  autoSetupDelayMsec, 
                  handlingDelayMsec, 
                  numOfDevicesToTest,
                  setup.pattern==all_sites_working ? "all" : "one" ,
                  handlerReprobeMode==ignore_reprobe ? "ignore" : "reprobe",
                  maxVerifyCount,
                  setup.corruptBinDataScheme==none ? "none" : "corrupt"
                 );

  for( int i=0 ; i < numOfSites ; i++ ) {
    siteList.push_back(Site(l));
  }

  switch( setup.pattern ) {
    case all_sites_working:
      sitePattern = new AllSitesWorkingPattern(l);
      break;
    case all_one_not_one_is_working:
      sitePattern = new AllOneNotOneIsWorkingPattern(l,numOfSites);
      break;
    default:
      sitePattern = NULL;
  }

  switch( setup.corruptBinDataScheme ) {
    case none:
      corruptBinDataScheme = new NoDataCorruptionScheme(l);
      break;
    case every_17th_bin_data:
      corruptBinDataScheme = new CorruptBinDataOnceScheme(l,17);
      break;
    case every_17th_and_18th_bin_data:
      corruptBinDataScheme = new CorruptBinDataTwiceScheme(l,17,18);
      break;
    default:
      corruptBinDataScheme = NULL;
  }

  status=stopped;
  running=false;
  numOfTestedDevices=0;
  binningTimer=new Timer(l);
  setRetestCategory(-1);
  setReprobeCategory(-2);
  nextCorruptBinCount=0;
  verifyCount=0;
  autoSetupInit=false;
  if( siteEnabledMask == allSitesEnabled || siteEnabledMask == 0 ) {
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

  if (corruptBinDataScheme != NULL)
  {
    delete corruptBinDataScheme;
    corruptBinDataScheme = NULL;
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

int Handler::getNumOfSites()
{
  return numOfSites;
}

unsigned long Handler::getSitePopulationAsLong()
{
  char message[256];
  char tmpString[64];
  unsigned long sitePop=0;
  Std::vector<Site>::iterator i;
  int bitPosition=0;

  phLogSimMessage(logId, LOG_VERBOSE, "Handler::getSitePopulationAsLong()");

  strcpy(message,"Site: ");

  for( i=siteList.begin(), bitPosition=0 ; i != siteList.end() ; ++i, ++bitPosition ) {
    sprintf(tmpString, "%d[%s] ", bitPosition+1, i->getSitePopulated() ? "P" : "E");
    strcat(message, tmpString);

    if( i->getSitePopulated() ) {
      sitePop |= ( 1 << bitPosition );
    }
  }

  phLogSimMessage(logId, LOG_DEBUG, "%s", message);
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::getSitePopulationAsLong() returned value is %lx",sitePop);

  return sitePop;
}


void Handler::sendDeviceToBin(int s, int b)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::sendDeviceToBin(%d,%d) ",s,b);

  if( s < 0 || s >= numOfSites ) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s+1);
    return;
  } else {
    if( b == getReprobeCategory() ) {
      phLogSimMessage(logId, LOG_DEBUG, "reprobe device on site %d ",s+1);
    } else if( b == getRetestCategory() ) {
      phLogSimMessage(logId, LOG_DEBUG, "retest device on site %d ",s+1);
    } else {
      phLogSimMessage(logId, LOG_DEBUG, "send device on site %d to bin %d ",s+1,b);
    }
  }

  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  i->sendDeviceToBin(b);

  if( allSitesBinned() ) {
    handleDevices();
  }
  return;
}




void Handler::setBinData(int s, int b)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::setBinData(%d,%d) ",s,b);

  if( s < 0 || s >= numOfSites ) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s+1);
    return;
  }

  b=corruptBinDataScheme->corruptBin(s,b);

  if( b == getReprobeCategory() ) {
    phLogSimMessage(logId, LOG_DEBUG, "set reprobe device on site %d ",s+1);
  } else if( b == getRetestCategory() ) {
    phLogSimMessage(logId, LOG_DEBUG, "set retest device on site %d ",s+1);
  } else {
    phLogSimMessage(logId, LOG_DEBUG, "set device on site %d to bin %d ",s+1,b);
  }

  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  i->setBinData(b);

  return;
}


void Handler::setContact(int s, bool c)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::setContact(%d,%d) ",s,int(c));

  if( s < 0 || s >= numOfSites ) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s+1);
    return;
  } else {
    phLogSimMessage(logId, LOG_DEBUG, "set contact site %d to %d ",s+1,int(c));
  }


  bool contactMoved;
  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  contactMoved=i->setContact(c);

  if( c == true && contactMoved ) {
    phLogSimMessage(logId, LOG_DEBUG, "reprobe action detected on site %d ",s);
    sendDeviceToBin(s,getReprobeCategory());
  }

  return;
}



int Handler::getBinData(int s)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::getBinData(%d) ",s);

  if( s < 0 || s >= numOfSites ) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s+1);
    return 0;
  }

  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  if( i->getBinData() == siteNotYetAssignedBinNumber ) {
    phLogSimMessage(logId, LOG_DEBUG, "no bin data assigned for site %d  ",s+1 );
  } else if( i->getBinData() == getReprobeCategory() ) {
    phLogSimMessage(logId, LOG_DEBUG, "reprobe assigned for site %d  ",s+1 );
  } else {
    phLogSimMessage(logId, LOG_DEBUG, "bin data for site %d is %d ",
                    s+1,
                    i->getBinData());
  }

  return i->getBinData();
}



void Handler::releaseDevice(int s)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::releaseDevice(%d) ",s);

  if( s < 0 || s >= numOfSites ) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ",s+1);
    return;
  }

  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  if( i->releaseDevice() ) {
    phLogSimMessage(logId, LOG_DEBUG, "release device for site %d ",s+1);

    if( allSitesBinned() ) {
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

  for( i=siteList.begin() ; i != siteList.end() && mayReleaseDevice==true ; ++i ) {
    mayReleaseDevice=i->readyForRelease();
  }

  return mayReleaseDevice;
}

void Handler::start()
{
  binningTimer->start();
  if( status==stopped ) {
    status=handlingDevices;
  }
  running=true;
  resetVerifyCount();
}

void Handler::stop()
{
  running=false;
}

bool Handler::isStopped()
{
  return !running;
}

bool Handler::sendTestStartSignal()
{
  bool sendTestStart = false;

  if( !autoSetupInit && autoSetupDelayMsec > 0 ) {
    phLogSimMessage(logId, LOG_VERBOSE,
                    "Handler::sendTestStartSignal automatic start time so far %lf wait %lf ",
                    binningTimer->getElapsedMsecTime(),
                    autoSetupDelayMsec);
    if( binningTimer->getElapsedMsecTime() >= autoSetupDelayMsec ) {
      start();
      autoSetupInit=true;
    }
  } else {
    phLogSimMessage(logId, LOG_VERBOSE,
                    "Handler::sendTestStartSignal elapsed time so far %lf wait %lf ",
                    binningTimer->getElapsedMsecTime(),
                    handlingDelayMsec);
  }

  switch( status ) {
    case stopped:
      sendTestStart=false;
      break;
    case handlingDevices:
      if( binningTimer->getElapsedMsecTime() >= handlingDelayMsec && running ) {
        if( populateSites() ) {
          sendTestStart=true;
          status=waiting;
        } else {
          sendTestStart=false;
          status=stopped;
          running=false;
        }
      } else {
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
  return(numOfDevicesToTest==handlerContinuousTesting) ?
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

  for( i=siteList.begin() ; i != siteList.end() ; ++i ) {
    siteStatList.push_back(i->getSiteStatistics());
  }

  s=0;
  for( si=siteStatList.begin() ; si != siteStatList.end() ; ++si ) {
    int stat;
    int n;

    for( mi=(*si).begin() ; mi != (*si).end() ; ++mi ) {
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

  char lineBuffer[2048];
  char tmpString[64];
  s=0;
  strcpy(lineBuffer,"         ");
  for( si=siteStatList.begin() ; si != siteStatList.end() ; ++si ) {
    sprintf(tmpString, " Chuck%-2d ", ++s);
    strcat(lineBuffer,tmpString);
    columnTotal.push_back(0);
  }
  sprintf(tmpString, " Total ");
  strcat(lineBuffer,tmpString);
  phLogSimMessage(logId, LOG_DEBUG, "%s",lineBuffer );

  for( ssii = siteStatIndex.begin() ; ssii != siteStatIndex.end() ; ++ssii ) {

    if( *ssii == getReprobeCategory() ) {
      strcpy(lineBuffer," Reprobe ");
    } else if( *ssii == getRetestCategory() ) {
      strcpy(lineBuffer," Retest  ");
    } else {
      sprintf(tmpString, " Cat %3d ", *ssii);
      strcpy(lineBuffer,tmpString);
    }

    rowTotal=0;
    s=0;
    for( si=siteStatList.begin() ; si != siteStatList.end() ; ++si, ++s ) {
      mi=(*si).find(*ssii);

      if( mi == (*si).end() ) {
        strcat(lineBuffer,"         ");
      } else {
        sprintf(tmpString, " %4d    ", (*mi).second);
        strcat(lineBuffer,tmpString);
        rowTotal+=(*mi).second;
        if( *ssii != getReprobeCategory() ) {
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
  for( cti=columnTotal.begin() ; cti != columnTotal.end() ; ++cti ) {
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
  return( getNumberOfSitesAsLong() & ~siteEnabledMask );
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

void Handler::resetVerifyCount()
{
  verifyCount=0;
}

void Handler::binDataVerified()
{
  ++verifyCount;

  phLogSimMessage(logId, LOG_DEBUG,
                  "Bin data verified count %d max %d ",
                  verifyCount,
                  maxVerifyCount);

  if( verifyCount == maxVerifyCount ) {
    phLogSimMessage(logId, LOG_DEBUG, "stop handler");
    stop();
  }
}



//
// PROTECTED interface functions:
//
void Handler::handleDevices()
{
  binningTimer->start();

  if( handlerReprobeMode==perform_reprobe_separately && reprobePending() ) {
    phLogSimMessage(logId, LOG_VERBOSE,
                    "no new pattern: reprobe pending and perform reprobe separately");
  } else {
    sitePattern->nextPattern();
  }

  //  if ( !reprobePending() || !populate_reprobe_pending_sites )
  //  {
  //      sitePattern->nextPattern();
  //  }

  status=handlingDevices;
  return;
}

bool Handler::populateSites()
{
  populateSitesToPattern();

  while( allSitesBinned() && !allTestsComplete() ) {
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

  for( i=siteList.begin(), s=0 ; i != siteList.end() ; ++i, ++s ) {
    popSitePattern=sitePattern->getSitePopulated(s) &&
                   ((1<<s)&siteEnabledMask) &&
                   ( numOfTestedDevices < numOfDevicesToTest );
    popSiteReprobe=(i->getPreviousBinData() == getReprobeCategory());

    switch( handlerReprobeMode ) {
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

    if( i->getSitePopulated() ) {
      phLogSimMessage(logId, LOG_VERBOSE,
                      "Handler::populateSitesToPattern cannot populate site %d since it is not empty ",
                      s+1);
    } else {
      if( popSite && !popSiteReprobe ) {
        ++numOfTestedDevices;
      }
      if( !popSite && popSiteReprobe ) {
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

  for( i=siteList.begin() ; i != siteList.end() && checkAllSitesBinned==true ; ++i ) {
    if( i->getSitePopulated() == true ) {
      checkAllSitesBinned=false;
    }
  }
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::allSitesBinned is %s ",
                  checkAllSitesBinned ? "true" : "false");

  return checkAllSitesBinned;
}


bool Handler::reprobePending()
{
  bool checkReprobePending=false;
  Std::vector<Site>::iterator i;

  for( i=siteList.begin() ; i != siteList.end() && checkReprobePending==false ; ++i ) {
    if( i->getPreviousBinData() == getReprobeCategory() ) {
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

  for( int i=0 ; i< numOfSites ; ++i ) {
    numOfSitesAsLong+= 1 << i;
  }

  return numOfSitesAsLong;
}



/*****************************************************************************
 * End of file
 *****************************************************************************/
