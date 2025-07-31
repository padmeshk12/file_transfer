/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.C
 * CREATED  : 14 Jul 2023
 *
 * CONTENTS : Atom handler simulator
 *
 * AUTHORS  : Zuria Zhu, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 2023, Zuria Zhu create
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
/* Begin of Huatek Modification, Donnie Tu, 2001/11/26 */
/* Issue Number: 304 */
#include <ctype.h>
/* End of Huatek Modification */

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_GuiServer.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/* set to create new playback file from old playback file */
/* #define SAVE_NEW_PLAYBACK_FORMAT  */

#undef SAVE_NEW_PLAYBACK_FORMAT

#define LOG_NORMAL         PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG          PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE        PHLOG_TYPE_MESSAGE_2

#define CMD_REPLY_MSG "OK"

#define F_STOP             0x0002
#define F_JAM              0x0004
#define F_DOOROPEN         0x0008
#define F_ASSIST           0x0020
#define F_FATAL            0x0040
#define F_PAUSE            0x0080
#define F_ALL              0xffff

#define MAX_RECIPE_NAME_LENGTH 64

enum model_t {
  DBDESIGN
};

//////////////////////////////  Interface with VXI11 server //////////////////////
#include <mqueue.h>
#include <time.h>

#define MAX_PATH_LENTH 512
#define MAX_NAME_LENTH 128
#define MAX_CMD_LINE_LENTH 640

mqd_t from_vxi11_server_mq;
mqd_t to_vxi11_server_mq;
mqd_t to_vxi11_server_srq_mq;

phComError_t phComGpibOpenVXI11(phComId_t *newSession /* The new session ID (result value). Will
 be set to NULL, if an error occured */,
    phComMode_t mode /* Commun mode: online|offline|simulation  */,
    char *device /* Symbolic Name of GPIB card in workstation*/, int port /* GPIB port to use */,
    phLogId_t logger /* The data logger to use for error and
     message log. Use NULL if data log is not
     used.                                    */, phComGpibEomAction_t act /* To ensure that the EOI line is
     released after a message has been
     send, a end of message action may
     be defined here. The EOI line can
     not be released directly, only as a
     side affect of a serial poll or a
     toggle of the ATN line. */
    )
{

  ///////////////// Interface with VX11 server

  from_vxi11_server_mq = mq_open("/TO_SIMULATOR", O_RDONLY);
  if (from_vxi11_server_mq == -1) {
    printf("Error: cannot open message queue /TO_SIMULATOR\n");
    return (phComError_t) -1;
  }

  to_vxi11_server_mq = mq_open("/FROM_SIMULATOR", O_WRONLY);
  if (to_vxi11_server_mq == -1) {
    printf("Error: cannot open message queue /FROM_SIMULATOR\n");
    return (phComError_t) -1;
  }

  to_vxi11_server_srq_mq = mq_open("/FROM_SIMULATOR_SRQ", O_WRONLY);
  if (to_vxi11_server_srq_mq == -1) {
    printf("Error: cannot open message queue /FROM_SIMULATOR_SRQ\n");
    return (phComError_t) -1;
  }

  ///////////////// Interface with VX11 server

  struct phComStruct *tmpId;
  /* allocate new communication data type */
  *newSession = NULL;
  tmpId = (struct phComStruct*) malloc(10);
  *newSession = tmpId;

  phComError_t return_val = (phComError_t) 0;

  return return_val;

}

phComError_t phComGpibCloseVXI11(phComId_t session /* session ID of an open GPIB session */
)
{

  phComError_t return_val = (phComError_t) 0;

  return return_val;
}

phComError_t phComGpibSendVXI11(phComId_t session /* session ID of an open GPIB session. */,
    char *h_string /* message to send, may contain '\0'
     characters */, int length /* length of the message to be sent,
     must be < PHCOM_MAX_MESSAGE_LENGTH */, long timeout /* timeout in usec, until the message must be
     sent */
    )
{

  phComError_t return_val = (phComError_t) 0;

  if (mq_send(to_vxi11_server_mq, h_string, length, 0) == -1) {
    printf("Error: error sending message:\n%s", h_string);
  }

  return return_val;

}

phComError_t phComGpibReceiveVXI11(phComId_t session /* session ID of an open GPIB session. */,
    const char **message /* received message, may contain '\0'
     characters. The calling function is
     not allowed to modify the passed
     message buffer */, int *length /* length of the received message,
     will be < PHCOM_MAX_MESSAGE_LENGTH */, long timeout /* timeout in usec, until the message
     must be received */
    )
{

  phComError_t return_val = (phComError_t) 0;
  static char buffer[8192 + 1];
  ssize_t bytes_read;
  while (1) {
    // will block here until a message is received
    struct timespec currenttime_before;
    struct timespec abs_timeout;
    clock_gettime(CLOCK_REALTIME, &currenttime_before);
    abs_timeout.tv_sec = currenttime_before.tv_sec + timeout / 1000000;
    abs_timeout.tv_nsec = currenttime_before.tv_nsec + (timeout % 1000000) * 1000;
    bytes_read = mq_timedreceive(from_vxi11_server_mq, buffer, 8192, NULL, &abs_timeout);
    if (bytes_read >= 0) {
      buffer[bytes_read] = '\0';
      *message = buffer;
      break;
    } else {
      //protection code since this is an abnormal case let's go slow here
      return PHCOM_ERR_TIMEOUT;
    }
  }

  return return_val;

}

phComError_t phComGpibSendEventVXI11(phComId_t session /* session ID of an open GPIB session */,
    phComGpibEvent_t *event /* current pending event or NULL */, long timeout /* time in usec until the transmission
     (not the reception) of the event
     must be completed */
    )
{

  phComError_t return_val = (phComError_t) 0;
  char srq[10];
  srq[0] = event->d.srq_byte;
  srq[1] = '\0';

  if (mq_send(to_vxi11_server_srq_mq, srq, strlen(srq), 0) == -1) {
    printf("Error: error sending SRQ message:\n%s", srq);
  }

  return return_val;

}

void getVxi11Path(char *szpath, int lenth)
{
  char *szEnv = getenv("WORKSPACE");
  if (NULL == szEnv) {
    snprintf(szpath, lenth,
        "/opt/hp93000/testcell/phcontrol/drivers/Generic_93K_Driver/tools/bin/vxi11_server");
  } else {
    snprintf(szpath, lenth,
        "%s/ph-development/development/drivers/Generic_93K_Driver/common/mhcom/bin/vxi11_server",
        szEnv);
  }
}

/*--- class definitions -----------------------------------------------------*/

#if __GNUC__ >= 3
class TCIostrstream : public Std::ostringstream {
#else
class TCIostrstream : public ostrstream {
#endif
public:
  TCIostrstream();
  ~TCIostrstream();
  char* str();
private:
  char *mystr;
};

typedef Std::map<int, int> siteStatType;
typedef siteStatType::iterator siteStatIterator;

class LogObject {
public:
  LogObject(phLogId_t l);
protected:
  phLogId_t logId;
};

class Timer : public LogObject {
public:
  Timer(phLogId_t l);
  void start();
  double getElapsedMsecTime();
private:
  struct timeval startTime;
};

class SitePattern : public LogObject {
public:
  SitePattern(phLogId_t l);
  SitePattern(phLogId_t l, int n);
  virtual bool getSitePopulated(int s)=0;
  virtual void nextPattern();
protected:
  int numOfSites;
};

class AllOneNotOneIsWorkingPattern : public SitePattern {
  enum ePatternType {
    all_working, one_not_working, one_is_working
  };

public:
  AllOneNotOneIsWorkingPattern(phLogId_t l, int n);
  virtual bool getSitePopulated(int s);
  virtual void nextPattern();
private:
  int siteCounter;
  ePatternType patternType;
};

class AllSitesWorkingPattern : public SitePattern {
public:
  AllSitesWorkingPattern(phLogId_t l);
  virtual bool getSitePopulated(int s);
};

class Site : public LogObject {
public:
  enum eSiteStatus {
    empty, waitingBinData, waitingRelease
  };

  Site(phLogId_t l);
  bool getSitePopulated();
  void setSitePopulated(bool p);
  void sendDeviceToBin(int b);
  void setBinData(int b);
  int getBinData();
  int getPreviousBinData();
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

class Handler : public LogObject {
public:
  // Kind of pattern.
  enum eHandlerSitePattern {
    all_sites_working, all_one_not_one_is_working
  };
  // Handler Status
  enum eHandlerStatus {
    stopped, handlingDevices, waiting
  };

  // Handler Mode
  enum eHandlerReprobeMode {
    ignore_reprobe, perform_reprobe_separately, add_new_devices_during_reprobe
  };

  Handler(phLogId_t l, int s, long sem, double asud, double hd, int ndt, eHandlerSitePattern p,
      eHandlerReprobeMode hrm, bool cbd);
  ~Handler();
  inline int getNumOfSites();
  bool getSitePopulated(int s);
  int getSitePopulationAsInt();
  unsigned long getSitePopulationAsLong();
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
  double autoSetupDelayMsec;
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
  bool autoSetupInit;
};

class StatusBit {
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

class DeltaStatus : public LogObject {
public:
  DeltaStatus(phLogId_t l);
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
  StatusBit b00_retestOrtestagain;
  StatusBit b01_outputFull;
  StatusBit b02_handlerEmpty;
  StatusBit b03_motorDiagEnabled;
  StatusBit b04_dymContactorCalEnabled;
  StatusBit b05_inputInhibited;
  StatusBit b06_sortInhibited;
  StatusBit b07_outOfGuardBands;
  StatusBit b08_jammed;
  StatusBit b09_stopped;
  StatusBit b10_partsSoaking;
  StatusBit b11_inputOrSortInhibitedOrDoorOpen;
};

/*--- class implementations -------------------------------------------------*/

#if __GNUC__ >= 3
TCIostrstream::TCIostrstream() : Std::ostringstream(), mystr(NULL)
#else
TCIostrstream::TCIostrstream() :
    ostrstream(), mystr(NULL)
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
  mystr = new char[ostrstream::pcount() + 1];

  strncpy(mystr, ostrstream::str(), ostrstream::pcount());
  mystr[ostrstream::pcount()] = '\0';
#endif

  return mystr;
}

/*--- LogObject -------------------------------------------------------------*/
LogObject::LogObject(phLogId_t l) :
    logId(l)
{
}

/*--- Timer -----------------------------------------------------------------*/

Timer::Timer(phLogId_t l) :
    LogObject(l)
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

  return (now.tv_sec - startTime.tv_sec) * 1000.0 + (now.tv_usec - startTime.tv_usec) / 1000.0;
}

/*--- SitePattern -----------------------------------------------------------*/
SitePattern::SitePattern(phLogId_t l, int n) :
    LogObject(l), numOfSites(n)
{
}

SitePattern::SitePattern(phLogId_t l) :
    LogObject(l), numOfSites(0)
{
}

void SitePattern::nextPattern()
{
}

/*--- AllOneNotOneIsWorkingPattern ---------------------------------*/
AllOneNotOneIsWorkingPattern::AllOneNotOneIsWorkingPattern(phLogId_t l, int n) :
    SitePattern(l, n), siteCounter(0), patternType(all_working)
{
}

bool AllOneNotOneIsWorkingPattern::getSitePopulated(int s)
{
  bool sitePopulated = false;

  switch (patternType) {
  case all_working:
    sitePopulated = true;
    break;
  case one_not_working:
    sitePopulated = (siteCounter != s);
    break;
  case one_is_working:
    sitePopulated = (siteCounter == s);
    break;
  }

  return sitePopulated;
}

void AllOneNotOneIsWorkingPattern::nextPattern()
{
  ::std::string patternTypeStr;
  ++siteCounter;

  if (siteCounter == numOfSites) {
    siteCounter = 0;
    switch (patternType) {
    case all_working:
      patternType = one_not_working;
      break;
    case one_not_working:
      patternType = one_is_working;
      break;
    case one_is_working:
      patternType = all_working;
      break;
    }
  }

  switch (patternType) {
  case all_working:
    patternTypeStr = "all working";
    break;
  case one_not_working:
    patternTypeStr = "one not working";
    break;
  case one_is_working:
    patternTypeStr = "one is working";
    break;
  }

  phLogSimMessage(logId, LOG_VERBOSE, "AllOneNotOneIsWorkingPattern::nextPattern %s c = %d ",
      patternTypeStr.c_str(), siteCounter);
  return;
}

/*--- AllSitesWorkingPattern -------------- ---------------------------------*/
AllSitesWorkingPattern::AllSitesWorkingPattern(phLogId_t l) :
    SitePattern(l)
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
            double asud,
            double hd, 
            int ndt, 
            eHandlerSitePattern p, 
            eHandlerReprobeMode hrm,
            bool cbd) :
        LogObject(l), 
        numOfSites(s), 
        siteEnabledMask(sem), 
        handlingDelayMsec(hd),
        autoSetupDelayMsec(asud),
        numOfDevicesToTest(ndt),
        handlerReprobeMode(hrm),
        corruptBinData(cbd)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::Handler s = %d hd = %lf ndt = %d p = %s ", s, hd,
      ndt, p == all_sites_working ? "all" : "one");

  for (int i = 0; i < numOfSites; i++) {
    siteList.push_back(Site(l));
  }

  switch (p) {
  case all_sites_working:
    sitePattern = new AllSitesWorkingPattern(l);
    break;
  case all_one_not_one_is_working:
    sitePattern = new AllOneNotOneIsWorkingPattern(l, numOfSites);
    break;
  default:
    sitePattern = NULL;
  }
  status = stopped;
  running = false;
  numOfTestedDevices = 0;
  binningTimer = new Timer(l);
  setRetestCategory(-1);
  setReprobeCategory(-2);
  setBinDataCount = 0;
  nextCorruptBinCount = 0;
  autoSetupInit = false;
  if (siteEnabledMask == allSitesEnabled || siteEnabledMask == 0) {
    siteEnabledMask = getNumberOfSitesAsLong();
  }
}

Handler::~Handler()
{
  if (sitePattern != NULL) {
    delete sitePattern;
    sitePattern = NULL;
  }

  if (binningTimer != NULL) {
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
  char message[256];
  char tmpString[64];
  int sitePop = 0;
  Std::vector<Site>::iterator i;
  int bitPosition = 0;

  phLogSimMessage(logId, LOG_VERBOSE, "Handler::getSitePopulationAsInt()");

  strcpy(message, "Site: ");

  for (i = siteList.begin(), bitPosition = 0; i != siteList.end(); ++i, ++bitPosition) {
    sprintf(tmpString, "%d[%s] ", bitPosition + 1, i->getSitePopulated() ? "P" : "E");
    strcat(message, tmpString);

    if (i->getSitePopulated()) {
      sitePop |= (1 << bitPosition);
    }
  }

  phLogSimMessage(logId, LOG_DEBUG, "%s", message);
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::getSitePopulationAsInt() returned value is %d",
      sitePop);

  return sitePop;
}

unsigned long Handler::getSitePopulationAsLong()
{
  char message[256];
  char tmpString[64];
  unsigned long sitePop = 0;
  Std::vector<Site>::iterator it = siteList.begin();
  unsigned long bitPosition = 0;
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::getSitePopulationAsLong()");
  strcpy(message, "Fullsites: ");
  for (bitPosition = 0; it != siteList.end(); ++it, ++bitPosition) {
    sprintf(tmpString, "%lu[%s] ", bitPosition + 1, it->getSitePopulated() ? "P" : "E");
    strcat(message, tmpString);

    if (it->getSitePopulated()) {
      sitePop |= (1UL << bitPosition);
    }
  }

  phLogSimMessage(logId, LOG_DEBUG, "%s", message);
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::getSitePopulationAsLong() returned value is %08lx",
      sitePop);
  return sitePop;
}

void Handler::sendDeviceToBin(int s, int b)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::sendDeviceToBin(%d,%d) ", s, b);

  if (s < 0 || s >= numOfSites) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ", s + 1);
    return;
  } else {
    if (b == getReprobeCategory()) {
      phLogSimMessage(logId, LOG_DEBUG, "reprobe device on site %d ", s + 1);
    } else if (b == getRetestCategory()) {
      phLogSimMessage(logId, LOG_DEBUG, "retest device on site %d ", s + 1);
    } else {
      phLogSimMessage(logId, LOG_DEBUG, "send device on site %d to bin %d ", s + 1, b);
    }
  }

  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  i->sendDeviceToBin(b);

  if (allSitesBinned()) {
    handleDevices();
  }
  return;
}

void Handler::setBinData(int s, int b)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::setBinData(%d,%d) ", s, b);

  if (s < 0 || s >= numOfSites) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ", s + 1);
    return;
  }

  if (corruptBinData) {
    ++setBinDataCount;

    if (setBinDataCount % 11 == 0 || setBinDataCount == nextCorruptBinCount) {
      phLogSimMessage(logId, LOG_DEBUG, "corrupt bin data for site %d from %d to %d ", s + 1, b,
          (b + setBinDataCount) % 100);
      b = (b + setBinDataCount) % 100;
      if (setBinDataCount % 11 == 0) {
        nextCorruptBinCount = setBinDataCount + 4;
      } else {
        setBinDataCount += 7;
      }
    }
  }

  if (b == getReprobeCategory()) {
    phLogSimMessage(logId, LOG_DEBUG, "set reprobe device on site %d ", s + 1);
  } else if (b == getRetestCategory()) {
    phLogSimMessage(logId, LOG_DEBUG, "set retest device on site %d ", s + 1);
  } else {
    phLogSimMessage(logId, LOG_DEBUG, "set device on site %d to bin %d ", s + 1, b);
  }

  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  i->setBinData(b);

  return;
}

void Handler::setContact(int s, bool c)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::setContact(%d,%d) ", s, int(c));

  if (s < 0 || s >= numOfSites) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ", s + 1);
    return;
  } else {
    phLogSimMessage(logId, LOG_DEBUG, "set contact site %d to %d ", s + 1, int(c));
  }

  bool contactMoved;
  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  contactMoved = i->setContact(c);

  if (c == true && contactMoved) {
    phLogSimMessage(logId, LOG_DEBUG, "reprobe action detected on site %d ", s);
    sendDeviceToBin(s, getReprobeCategory());
  }

  return;
}

int Handler::getBinData(int s)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::getBinData(%d) ", s);

  if (s < 0 || s >= numOfSites) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ", s + 1);
    return 0;
  }

  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  if (i->getBinData() == siteNotYetAssignedBinNumber) {
    phLogSimMessage(logId, LOG_DEBUG, "no bin data assigned for site %d  ", s + 1);
  } else if (i->getBinData() == getReprobeCategory()) {
    phLogSimMessage(logId, LOG_DEBUG, "reprobe assigned for site %d  ", s + 1);
  } else {
    phLogSimMessage(logId, LOG_DEBUG, "bin data for site %d is %d ", s + 1, i->getBinData());
  }

  return i->getBinData();
}

void Handler::releaseDevice(int s)
{
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::releaseDevice(%d) ", s);

  if (s < 0 || s >= numOfSites) {
    phLogSimMessage(logId, LOG_DEBUG, "error on site number %d ", s + 1);
    return;
  }

  Std::vector<Site>::iterator i;

  i = siteList.begin() + s;

  if (i->releaseDevice()) {
    phLogSimMessage(logId, LOG_DEBUG, "release device for site %d ", s + 1);

    if (allSitesBinned()) {
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

  bool mayReleaseDevice = true;

  Std::vector<Site>::iterator i;

  for (i = siteList.begin(); i != siteList.end() && mayReleaseDevice == true; ++i) {
    mayReleaseDevice = i->readyForRelease();
  }

  return mayReleaseDevice;
}

void Handler::start()
{
  binningTimer->start();
  if (status == stopped) {
    status = handlingDevices;
  }
  running = true;
}

void Handler::stop()
{
  running = false;
}

bool Handler::sendTestStartSignal()
{
  bool sendTestStart = false;
  if (!autoSetupInit && autoSetupDelayMsec > 0) {
    phLogSimMessage(logId, LOG_VERBOSE,
        "Handler::sendTestStartSignal automatic start time so far %lf wait %lf ",
        binningTimer->getElapsedMsecTime(), autoSetupDelayMsec);
    if (binningTimer->getElapsedMsecTime() >= autoSetupDelayMsec) {
      start();
      autoSetupInit = true;
    }
  } else {
    phLogSimMessage(logId, LOG_VERBOSE,
        "Handler::sendTestStartSignal elapsed time so far %lf wait %lf ",
        binningTimer->getElapsedMsecTime(), handlingDelayMsec);
  }

  switch (status) {
  case stopped:
    sendTestStart = false;
    break;
  case handlingDevices:
    if (binningTimer->getElapsedMsecTime() >= handlingDelayMsec && running) {

      if (populateSites()) {
        sendTestStart = true;
        status = waiting;
      } else {
        sendTestStart = false;
        status = stopped;
        running = false;
      }
    } else {
      sendTestStart = false;
    }
    break;
  case waiting:
    sendTestStart = false;
    break;
  }

  phLogSimMessage(logId, LOG_VERBOSE, "Handler::sendTestStartSignal is %s ",
      sendTestStart ? "true" : "false");
  return sendTestStart;
}

bool Handler::allTestsComplete()
{
  return
      (numOfDevicesToTest == handlerContinuousTesting) ? false :
          (numOfTestedDevices >= numOfDevicesToTest) && allSitesBinned() && !reprobePending();
}

void Handler::logHandlerStatistics()
{
  Std::vector<Site>::iterator i;
  int s = 0;
  Std::vector<siteStatType> siteStatList;
  Std::vector<siteStatType>::iterator si;
  siteStatIterator mi;

  Std::list<int> siteStatIndex;
  Std::list<int>::iterator ssii;

  Std::vector<int> columnTotal;
  Std::vector<int>::iterator cti;
  int rowTotal;

  for (i = siteList.begin(); i != siteList.end(); ++i) {
    siteStatList.push_back(i->getSiteStatistics());
  }

  s = 0;
  for (si = siteStatList.begin(); si != siteStatList.end(); ++si) {
    int stat;
    int n;

    for (mi = (*si).begin(); mi != (*si).end(); ++mi) {
      stat = (*mi).first;
      n = (*mi).second;

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
  s = 0;
  strcpy(lineBuffer, "         ");
  for (si = siteStatList.begin(); si != siteStatList.end(); ++si) {
    sprintf(tmpString, " Chuck%-2d ", ++s);
    strcat(lineBuffer, tmpString);
    columnTotal.push_back(0);
  }
  sprintf(tmpString, " Total ");
  strcat(lineBuffer, tmpString);
  phLogSimMessage(logId, LOG_DEBUG, "%s", lineBuffer);

  for (ssii = siteStatIndex.begin(); ssii != siteStatIndex.end(); ++ssii) {

    if (*ssii == getReprobeCategory()) {
      strcpy(lineBuffer, " Reprobe ");
    } else if (*ssii == getRetestCategory()) {
      strcpy(lineBuffer, " Retest  ");
    } else {
      sprintf(tmpString, " Cat %3d ", *ssii);
      strcpy(lineBuffer, tmpString);
    }

    rowTotal = 0;
    s = 0;
    for (si = siteStatList.begin(); si != siteStatList.end(); ++si, ++s) {
      mi = (*si).find(*ssii);

      if (mi == (*si).end()) {
        strcat(lineBuffer, "         ");
      } else {
        sprintf(tmpString, " %4d    ", (*mi).second);
        strcat(lineBuffer, tmpString);
        rowTotal += (*mi).second;
        if (*ssii != getReprobeCategory()) {
          columnTotal[s] += (*mi).second;
        }
      }
    }
    sprintf(tmpString, " %4d    ", rowTotal);
    strcat(lineBuffer, tmpString);

    phLogSimMessage(logId, LOG_DEBUG, "%s", lineBuffer);
  }

  phLogSimMessage(logId, LOG_DEBUG, " ");
  strcpy(lineBuffer, "Total    ");
  for (cti = columnTotal.begin(); cti != columnTotal.end(); ++cti) {
    sprintf(tmpString, " %4d    ", *cti);
    strcat(lineBuffer, tmpString);
  }
  phLogSimMessage(logId, LOG_DEBUG, "%s", lineBuffer);
  phLogSimMessage(logId, LOG_DEBUG, " ");
  phLogSimMessage(logId, LOG_DEBUG, "Total Number of tested devices = %d", numOfTestedDevices);
  phLogSimMessage(logId, LOG_DEBUG, " ");

  return;
}

long Handler::getSitesEnabled()
{
  return siteEnabledMask;
}

long Handler::getSitesDisabled()
{
  return (getNumberOfSitesAsLong() & ~siteEnabledMask);
}

bool Handler::getSiteEnabled(int s)
{
  return siteEnabledMask & (1 << s);
}

bool Handler::getSiteDisabled(int s)
{
  return !getSiteEnabled(s);
}

void Handler::setRetestCategory(int r)
{
  retestCat = r;
}

void Handler::setReprobeCategory(int r)
{
  reprobeCat = r;
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
  phLogSimMessage(logId, LOG_DEBUG, "New lot loaded ", numOfTestedDevices);
  numOfTestedDevices = 0;
}

//
// PROTECTED interface functions:
//
void Handler::handleDevices()
{
  binningTimer->start();

  if (handlerReprobeMode == perform_reprobe_separately && reprobePending()) {
    phLogSimMessage(logId, LOG_VERBOSE,
        "no new pattern: reprobe pending and perform reprobe separately");
  } else {
    sitePattern->nextPattern();
  }

  status = handlingDevices;
  return;
}

bool Handler::populateSites()
{
  populateSitesToPattern();

  while (allSitesBinned() && !allTestsComplete()) {
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

  for (i = siteList.begin(), s = 0; i != siteList.end(); ++i, ++s) {
    popSitePattern = sitePattern->getSitePopulated(s) && ((1 << s) & siteEnabledMask)
        && (numOfTestedDevices < numOfDevicesToTest);
    popSiteReprobe = (i->getPreviousBinData() == getReprobeCategory());

    switch (handlerReprobeMode) {
    case ignore_reprobe:
      popSite = popSitePattern;
      break;
    case perform_reprobe_separately:
      popSite = (reprobePending()) ? popSiteReprobe : popSitePattern;
      break;
    case add_new_devices_during_reprobe:
      popSite = popSitePattern || popSiteReprobe;
      break;
    }

    if (i->getSitePopulated()) {
      phLogSimMessage(logId, LOG_VERBOSE,
          "Handler::populateSitesToPattern cannot populate site %d since it is not empty ", s + 1);
    } else {
      if (popSite && !popSiteReprobe) {
        ++numOfTestedDevices;
      }
      if (!popSite && popSiteReprobe) {
        --numOfTestedDevices;
      }

      i->setSitePopulated(popSite);
    }
  }
  return;
}

bool Handler::allSitesBinned()
{
  bool checkAllSitesBinned = true;
  Std::vector<Site>::iterator i;

  for (i = siteList.begin(); i != siteList.end() && checkAllSitesBinned == true; ++i) {
    if (i->getSitePopulated() == true) {
      checkAllSitesBinned = false;
    }
  }
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::allSitesBinned is %s ",
      checkAllSitesBinned ? "true" : "false");

  return checkAllSitesBinned;
}

bool Handler::reprobePending()
{
  bool checkReprobePending = false;
  Std::vector<Site>::iterator i;

  for (i = siteList.begin(); i != siteList.end() && checkReprobePending == false; ++i) {
    if (i->getPreviousBinData() == getReprobeCategory()) {
      checkReprobePending = true;
    }
  }
  phLogSimMessage(logId, LOG_VERBOSE, "Handler::reprobePending is %s ",
      checkReprobePending ? "true" : "false");

  return checkReprobePending;
}

long Handler::getNumberOfSitesAsLong()
{
  long numOfSitesAsLong = 0;

  for (int i = 0; i < numOfSites; ++i) {
    numOfSitesAsLong += 1 << i;
  }

  return numOfSitesAsLong;
}

/*--- Site ------------------------------------------------------------------*/

//
// CONSTRUCTOR:
//
Site::Site(phLogId_t l) :
    LogObject(l), status(Site::empty), binData(siteNotYetAssignedBinNumber), contact(true)
{
}

//
// PUBLIC interface functions:
//

bool Site::getSitePopulated()
{
  return (status == waitingBinData || status == waitingRelease);
}

void Site::setSitePopulated(bool p)
{
  if (p) {
    status = waitingBinData;
  } else {
    status = empty;
  }
}

void Site::sendDeviceToBin(int b)
{
  if (status == empty) {
    sleep(2);
    phLogSimMessage(logId, LOG_DEBUG, "Bin data received for a site which is empty: ignored");
  } else {
    binData = b;
    binList.push_back(b);
    status = empty;
  }
}

void Site::setBinData(int b)
{

  if (status == empty) {
    phLogSimMessage(logId, LOG_DEBUG, "Set bin data received for a site which is empty: ignored");
  } else {
    binData = b;
    status = waitingRelease;
  }
}

int Site::getBinData()
{
  return status == waitingRelease ? binData : siteNotYetAssignedBinNumber;
}

int Site::getPreviousBinData()
{
  return binData;
}

bool Site::releaseDevice()
{
  bool deviceReleased = false;

  switch (status) {
  case empty:
    phLogSimMessage(logId, LOG_VERBOSE,
        "Release device received for a site which is empty: ignored");
    deviceReleased = false;
    break;
  case waitingBinData:
    phLogSimMessage(logId, LOG_VERBOSE,
        "Release device received for a site which is still waiting bin data: ignored");
    deviceReleased = false;
    break;
  case waitingRelease:
    binList.push_back(binData);
    status = empty;
    deviceReleased = true;
    break;
  }
  return deviceReleased;
}

bool Site::readyForRelease()
{
  bool mayReleaseDevice = false;

  switch (status) {
  case empty:
    phLogSimMessage(logId, LOG_VERBOSE, "site is empty: release is okay");
    mayReleaseDevice = true;
    break;
  case waitingBinData:
    phLogSimMessage(logId, LOG_VERBOSE, "waiting bin data: not ready for release");
    mayReleaseDevice = false;
    break;
  case waitingRelease:
    phLogSimMessage(logId, LOG_VERBOSE, "waiting bin data: ready for release");
    mayReleaseDevice = true;
    break;
  }
  return mayReleaseDevice;
}

bool Site::setContact(bool c)
{
  bool logMessage = false;

  if (contact != c) {
    logMessage = true;
    if (contact) {
      phLogSimMessage(logId, LOG_DEBUG, "retract contact motors ");
    } else {
      phLogSimMessage(logId, LOG_DEBUG, "extend contact motors ");
    }
  }
  contact = c;
  return logMessage;
}

siteStatType Site::getSiteStatistics()
{
  siteStatIterator mi;

  Std::vector<int>::iterator i;
  int c = 0;

  for (i = binList.begin(); i != binList.end(); ++i) {
    phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ", ++c, *i);
  }

  phLogSimMessage(logId, LOG_VERBOSE, "Sort Data ");
  std::sort(binList.begin(), binList.end());
  c = 0;
  for (i = binList.begin(); i != binList.end(); ++i) {
    phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ", ++c, *i);
  }

  i = binList.begin();
  int n;
  int stat;
  while (i != binList.end()) {

    stat = *i;

    phLogSimMessage(logId, LOG_VERBOSE, "stat is %d ", stat);

#if __GNUC__ >= 3
        n = std::count(i, binList.end(), stat);
#else
    n = 0;
    count(i, binList.end(), stat, n);
#endif

    phLogSimMessage(logId, LOG_VERBOSE, "count is %d ", n);

    siteStatistics[stat] = n;

    i += n;
  }

  phLogSimMessage(logId, LOG_VERBOSE, "Data Statistics ");

  for (mi = siteStatistics.begin(); mi != siteStatistics.end(); ++mi) {
    stat = (*mi).first;
    n = (*mi).second;
    phLogSimMessage(logId, LOG_VERBOSE, "Bin[%d] = %d  ", stat, n);
  }

  return siteStatistics;
}

//
// PROTECTED interface functions:
//

/*--- StatusBit -------------------------------------------------------------*/

StatusBit::StatusBit() :
    value(false), position(0)
{
}

StatusBit::StatusBit(bool v, int p) :
    value(v), position(p)
{
}

void StatusBit::setBit(bool b)
{
  value = b;
}

void StatusBit::setPosition(int p)
{
  position = p;
}

void StatusBit::setBitPosition(bool b, int p)
{
  value = b;
  position = p;
}

int StatusBit::getBitAsInt()
{
  return value * (1 << position);
}

bool StatusBit::getBit()
{
  return value;
}

/*--- DeltaStatus -----------------------------------------------------------*/

DeltaStatus::DeltaStatus(phLogId_t l) :
    LogObject(l)
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
  b10_partsSoaking.setBitPosition(false, 10);
  b11_inputOrSortInhibitedOrDoorOpen.setBitPosition(false, 11);
}

int DeltaStatus::getStatusAsInt()
{
  return b00_retestOrtestagain.getBitAsInt() + b01_outputFull.getBitAsInt()
      + b02_handlerEmpty.getBitAsInt() + b03_motorDiagEnabled.getBitAsInt()
      + b04_dymContactorCalEnabled.getBitAsInt() + b05_inputInhibited.getBitAsInt()
      + b06_sortInhibited.getBitAsInt() + b07_outOfGuardBands.getBitAsInt()
      + b08_jammed.getBitAsInt() + b09_stopped.getBitAsInt() + b10_partsSoaking.getBitAsInt()
      + b11_inputOrSortInhibitedOrDoorOpen.getBitAsInt();
}

void DeltaStatus::setHandlerEmpty(bool b)
{
  b02_handlerEmpty.setBit(b);
}

void DeltaStatus::setOutOfGuardBand(bool b)
{
  b07_outOfGuardBands.setBit(b);
}

void DeltaStatus::setJammed(bool b)
{
  b08_jammed.setBit(b);
}

void DeltaStatus::setStopped(bool b)
{
  b09_stopped.setBit(b);
}

void DeltaStatus::setDoorOpen(bool b)
{
  b11_inputOrSortInhibitedOrDoorOpen.setBit(b);
}

bool DeltaStatus::getHandlerEmpty()
{
  return b02_handlerEmpty.getBit();
}

bool DeltaStatus::getOutOfGuardBand()
{
  return b07_outOfGuardBands.getBit();
}

bool DeltaStatus::getJammed()
{
  return b08_jammed.getBit();
}

bool DeltaStatus::getStopped()
{
  return b09_stopped.getBit();
}

bool DeltaStatus::getDoorOpen()
{
  return b11_inputOrSortInhibitedOrDoorOpen.getBit();
}

bool DeltaStatus::getHandlerIsReady()
{
  return !getHandlerEmpty() && !getOutOfGuardBand() && !getJammed() && !getDoorOpen();
}

void DeltaStatus::logStatus()
{
  char message[256];

  strcpy(message, "Status: ");

  if (getHandlerIsReady()) {
    strcat(message, "ready, ");
  } else {
    strcat(message, "not ready, ");
  }

  if (getHandlerEmpty()) {
    strcat(message, "handler empty, ");
  }

  if (getOutOfGuardBand()) {
    strcat(message, "out of guard band, ");
  }

  if (getJammed()) {
    strcat(message, "jammmed, ");
  }

  if (getDoorOpen()) {
    strcat(message, "door open, ");
  }

  if (getStopped()) {
    strcat(message, "stopped ");
  } else {
    strcat(message, "running ");
  }

  phLogSimMessage(logId, LOG_DEBUG, "%s", message);

}

/*--- End -------------------------------------------------------------------*/

/*--- all_t -----------------------------------------------------------------*/

struct all_t {

  int numOfSites;
  int numberOfDevicesToTest;
  double handlingDelay;
  double autoSetupDelay;
  long siteEnabledMask;
  Handler::eHandlerSitePattern pattern;
  int verifyTest;
  int cmdReply;
  int systemMode;
  int emulationMode;
  int errorBit;
  int srqMask;
  Handler::eHandlerReprobeMode reprobeMode;
//    bool             populate_reprobe_pending_sites;
  bool multiple_srqs;
  bool corruptBinData;
  bool releasepartsWhenReady;

  /* reprobe */
  int reprobeBinNumber;
  /* retest */
  int retestBinNumber;

// Handler has been stopped by operator
  bool operatorPause;

  DeltaStatus *deltaStatus;
  Handler *handler;

  bool queryError;
  int queryErrorCount;

  /* ids */
  phLogId_t logId;
  phComId_t comId;
  phGuiProcessId_t guiId;

  /* setup */
  int debugLevel;
  char *hpibDevice;
  char *softwareVersion;
  char *handlerNumber;
  int timeout;
  char *modelname;
  enum model_t model;

  int dontSendPauseSRQ;
  int noGUI;
  int autoStartTime;

  /* init */
  int done;
  int abortProbing;
  int master;

  /* lots */
  int lotActive;
  long lotDoneTime;

  /* runtime */
  const char *message;
  int length;
  phComGpibEvent_t *comEvent;
  phGuiEvent_t guiEvent;

  int interactiveDelay;
  int interactiveDelayCount;

  /* playback */
  int pbGpibStep;
  int pbSubStep;
  int doRecord;
  int doPlayback;
  FILE *newRecordFile;
  FILE *recordFile;
  FILE *playbackFile;

  /* record data */
  char recordName[128];
  char recordData[128];
  int recordRetval;
  int recordStep;
  bool recordUsed;

  const char *eoc;
};

static void doExitFail(struct all_t *all);
static void checkGuiEvent(struct all_t *all);

/*--- global variables ------------------------------------------------------*/

static struct all_t allg;

const static char *slaveGuiDescHead = " \
    S:`name`:` \
";

const static char *slaveGuiDescTail =
    " \
    ` \
    {@v \
        S::``{@v \
            S::{@h  f:`f_req`:`Request`:    p:`p_req`:`Receive` } \
            S::{@h  f:`f_answ`:`Answer`:    p:`p_answ`:`Send` } \
            S::{@h  f:`f_srq`:`SRQ`:        p:`p_srq`:`Send` } \
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

enum eGuiDataType {
  stop, jam, dooropen, unknown
};

static eGuiDataType getGuiDataType(struct all_t *all, const char *dtstr)
{
  eGuiDataType dt;

  if (strcmp(dtstr, "t_stop") == 0)
    dt = stop;
  else if (strcmp(dtstr, "t_jam") == 0)
    dt = jam;
  else if (strcmp(dtstr, "t_dooropen") == 0)
    dt = dooropen;
  else {
    dt = unknown;
    phLogSimMessage(all->logId, PHLOG_TYPE_ERROR, "unable to determine gui data type from %s",
        dtstr);
  }
  return dt;
}

static bool getDeltaStatus(struct all_t *all, const char *dtstr)
{
  bool status;

  switch (getGuiDataType(all, dtstr)) {
  case stop:
    status = all->deltaStatus->getStopped();
    break;
  case jam:
    status = all->deltaStatus->getJammed();
    break;
  case dooropen:
    status = all->deltaStatus->getDoorOpen();
    break;
  case unknown:
    status = false;
    break;
  }
  return status;
}

static bool getPlaybackCmpt(struct all_t *all)
{
  int rtnfscanf;

  strcpy(all->recordName, "");
  strcpy(all->recordData, "");

  rtnfscanf = fscanf(all->playbackFile, "%d %d %s %s", &(all->recordStep), &(all->recordRetval),
      all->recordName, all->recordData);

  if (rtnfscanf == 4) {
    phLogSimMessage(all->logId, LOG_VERBOSE, "getPlaybackCmpt() %d %d %s %s", all->recordStep,
        all->recordRetval, all->recordName, all->recordData);

    all->recordUsed = false;
  } else if (rtnfscanf == EOF) {
    phLogSimMessage(all->logId, LOG_VERBOSE, "getPlaybackCmpt() EOF detected ");
    all->recordUsed = true;
  } else {
    phLogSimMessage(all->logId, LOG_VERBOSE,
        "getPlaybackCmpt() unexpected return value from fscanf  %d ", rtnfscanf);
  }

  return (rtnfscanf == 4);
}

/*--- playback functionality ------------------------------------------------*/

static void doGpibStep(struct all_t *all)
{
  all->pbGpibStep++;
  all->pbSubStep = 0;
}

static void doSubStep(struct all_t *all)
{
  all->pbSubStep++;
}

phGuiError_t phPbGuiGetData(struct all_t *all, phGuiProcessId_t guiProcessID, /* the Gui-ID */
const char *componentName, /* name of the component */
char *data, /* the variable where the data will be stored */
int timeout /* timeout after which phGuiGetData returns */
)
{
  phGuiError_t retVal = (phGuiError_t) 0;

  if (all->doPlayback) {
    if (all->pbGpibStep >= all->recordStep) {
      if (all->recordUsed == true) {
        getPlaybackCmpt(all);
      } else {
        if (all->pbGpibStep > all->recordStep) {
          phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
              "record not made use of %d %d %s %s current step %d %s ", all->recordStep,
              all->recordRetval, all->recordName, all->recordData, all->pbGpibStep, componentName);
          doExitFail(all);
        }
      }
    }

    if (all->pbGpibStep == all->recordStep && strcmp(componentName, all->recordName) == 0) {
      retVal = (phGuiError_t) all->recordRetval;
      strcpy(data, all->recordData);
      all->recordUsed = true;
    } else {
      if (getDeltaStatus(all, componentName))
        strcpy(data, "True");
      else
        strcpy(data, "False");
      retVal = (phGuiError_t) 0;
    }
    if (!all->noGUI)
      phGuiChangeValue(guiProcessID, componentName, data);

#ifdef SAVE_NEW_PLAYBACK_FORMAT
        bool deltaStatus=getDeltaStatus(all,componentName);
        bool recordGuiData;
        
        if ( strcasecmp(data, "true")==0  && deltaStatus==false )
            recordGuiData=true;
        else if ( strcasecmp(data, "false")==0  && deltaStatus==true )
            recordGuiData=true;
        else 
            recordGuiData=false;
       
        if ( recordGuiData ) 
        {
	    fprintf(all->newRecordFile, "%d %d %s %s\n", 
	        all->pbGpibStep, (int) retVal, componentName, data);
        }
#endif

    return retVal;
  }

  if (all->noGUI) {
    strcpy(data, "");
    return PH_GUI_ERR_OK;
  }

  retVal = phGuiGetData(guiProcessID, componentName, data, timeout);

  if (all->doRecord) {
    bool deltaStatus = getDeltaStatus(all, componentName);
    bool recordGuiData;

    if (strcasecmp(data, "true") == 0 && deltaStatus == false)
      recordGuiData = true;
    else if (strcasecmp(data, "false") == 0 && deltaStatus == true)
      recordGuiData = true;
    else
      recordGuiData = false;

    if (recordGuiData) {
      fprintf(all->recordFile, "%d %d %s %s\n", all->pbGpibStep, (int) retVal, componentName, data);
    }
  }

  return retVal;
}

phGuiError_t phPbGuiChangeValue(struct all_t *all, phGuiProcessId_t guiProcessID, /* the Gui-ID */
const char *componentName, /* name of the component to change */
const char *newValue /* new value */
)
{
  if (all->noGUI)
    return PH_GUI_ERR_OK;
  else
    return phGuiChangeValue(guiProcessID, componentName, newValue);
}

phGuiError_t phPbGuiDestroyDialog(struct all_t *all, phGuiProcessId_t process /* the Gui-ID */
)
{
  if (all->noGUI)
    return PH_GUI_ERR_OK;
  else
    return phGuiDestroyDialog(process);
}

static long timeSinceLotDone(struct all_t *all);

phGuiError_t phPbGuiGetEvent(struct all_t *all, phGuiProcessId_t guiProcessID, /* the Gui-ID */
phGuiEvent_t *event, /* the variable where the event type and
 the triggering buttonname will be stored */
int timeout /* timeout after which phGuiGetEvent returns */
)
{
  phGuiError_t retVal;

  if (all->autoStartTime > 0 && !all->lotActive) {
    if (all->autoStartTime < timeSinceLotDone(all)) {
      event->type = PH_GUI_PUSHBUTTON_EVENT;
      strcpy(event->name, "p_start");
      event->secval = 0;
      return PH_GUI_ERR_OK;
    }
  }

  if (all->noGUI) {
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
	    all->pbGpibStep, all->pbSubStep, (int) retVal, 
	    (int) event->type, event->name, event->secval);
    }
#endif

  return retVal;
}

phGuiError_t phPbGuiCreateUserDefDialog(struct all_t *all,
    phGuiProcessId_t *id /* the resulting Gui-ID                    */,
    phLogId_t logger /* the data logger to be used              */,
    phGuiSequenceMode_t mode /* mode in which the gui should be started */, char *description /* description of the gui-representation   */
    )
{
  if (all->noGUI)
    return PH_GUI_ERR_OK;
  else
    return phGuiCreateUserDefDialog(id, logger, mode, description);
}

phGuiError_t phPbGuiShowDialog(struct all_t *all, phGuiProcessId_t process, /* the Gui-ID */
int closeOnReturn /* if set to 1 dialog will close when
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
  phComGpibCloseVXI11(all->comId);
  phLogDestroy(all->logId);
  exit(1);
}

static int checkGuiError(phGuiError_t guiError, struct all_t *all)
{
  switch (guiError) {
  case PH_GUI_ERR_OK:
    break;
  case PH_GUI_ERR_TIMEOUT:
    phLogSimMessage(all->logId, LOG_VERBOSE, "received timeout error from gui server");
    break;
  default:
    phLogSimMessage(all->logId, PHLOG_TYPE_ERROR, "received error code %d from gui server",
        (int) guiError);
    return 0;
  }
  return 1;
}

static int checkComError(phComError_t comError, struct all_t *all)
{
  switch (comError) {
  case PHCOM_ERR_OK:
    return 0;
  case PHCOM_ERR_TIMEOUT:
    phLogSimMessage(all->logId, LOG_VERBOSE, "received timeout error from com layer");
    return 1;
  default:
    phLogSimMessage(all->logId, PHLOG_TYPE_ERROR, "received error code %d from com layer",
        (int) comError);
    return 2;
  }
}

/*---------------------------------------------------------------------------*/

static void beAlife(struct all_t *all)
{
  static int b = 1;

  if (b == 1) {
    b = 2;
    checkGuiError(phPbGuiChangeValue(all, all->guiId, "r_life_1", "False"), all);
    checkGuiError(phPbGuiChangeValue(all, all->guiId, "r_life_2", "True"), all);
  } else {
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
    checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "Invalid Command"), all);
  else
    checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "OK"), all);
}

static void doDelay(int seconds, struct all_t *all)
{
  char delay[64];
  char typed[80];

  while (seconds > 0) {
    sprintf(delay, "%d", seconds);
    checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_delay", delay), all);
    seconds--;
    sleep(1);
  }
  checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_delay", ""), all);

  if (all->interactiveDelay) {
    if (all->interactiveDelayCount > 0) {
      all->interactiveDelayCount--;
    } else {
      printf("press <enter> to go on one step or enter number of steps: ");
      do
        ; while (!fgets(typed, 79, stdin));

      if (!sscanf(typed, "%d", &all->interactiveDelayCount))
        all->interactiveDelayCount = 0;
    }
  }
}

/*---------------------------------------------------------------------------*/

static void sendSRQ(struct all_t *all, unsigned char srq)
{
  char message[1024];
  phComGpibEvent_t sendEvent;
  phGuiError_t guiError;
  phComError_t comError;
  int sent = 0;

  sprintf(message, "0x%02x", (int) srq);
  guiError = phPbGuiChangeValue(all, all->guiId, "f_srq", message);
  checkGuiError(guiError, all);

  sendEvent.d.srq_byte = srq;
  sendEvent.type = PHCOM_GPIB_ET_SRQ;

  do {
    setWaiting(1, all);
    comError = phComGpibSendEventVXI11(all->comId, &sendEvent, 1000L * all->timeout);
    setWaiting(0, all);
    switch (checkComError(comError, all)) {
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

  phLogSimMessage(all->logId, LOG_DEBUG, "sent SRQ \"%s\"", message);
}

static void sendMessageEoc(struct all_t *all, const char *m, const char *eoc)
{
  char message[1024];
  int sent = 0;
  phGuiError_t guiError;
  phComError_t comError;

  strcpy(message, m);
  guiError = phPbGuiChangeValue(all, all->guiId, "f_answ", message);
  checkGuiError(guiError, all);

  strcat(message, eoc);
  phLogSimMessage(all->logId, LOG_VERBOSE, "trying to send message \"%s\"", m);

  do {
    setWaiting(1, all);
    comError = phComGpibSendVXI11(all->comId, message, strlen(message), 1000L * all->timeout);
    setWaiting(0, all);
    switch (checkComError(comError, all)) {
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
  phLogSimMessage(all->logId, LOG_DEBUG, "message \"%s\" sent", m);
}

static void sendMessage(struct all_t *all, const char *m)
{
  sendMessageEoc(all, m, all->eoc);
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
  switch (all->debugLevel) {
  case -1:
    modeFlags = PHLOG_MODE_QUIET;
    break;
  case 0:
    modeFlags = phLogMode_t(PHLOG_MODE_WARN | PHLOG_MODE_DBG_0);
    break;
  case 1:
    modeFlags = phLogMode_t(PHLOG_MODE_WARN | PHLOG_MODE_DBG_1);
    break;
  case 2:
    modeFlags = phLogMode_t(PHLOG_MODE_WARN | PHLOG_MODE_DBG_2);
    break;
  case 3:
    modeFlags = phLogMode_t(PHLOG_MODE_WARN | PHLOG_MODE_DBG_3);
    break;
  case 4:
    modeFlags = phLogMode_t(PHLOG_MODE_WARN | PHLOG_MODE_DBG_4);
    break;
  case 5:
    modeFlags = phLogMode_t(PHLOG_MODE_WARN | PHLOG_MODE_TRACE | PHLOG_MODE_DBG_4);
    break;
  }

  return modeFlags;
}

static void incDebugLevel(struct all_t *all)
{
  ++all->debugLevel;

  if (all->debugLevel > 5)
    all->debugLevel = 5;

  phLogSetMode(all->logId, getLogMode(all));
}

static void decDebugLevel(struct all_t *all)
{
  --all->debugLevel;

  if (all->debugLevel < -1)
    all->debugLevel = -1;

  phLogSetMode(all->logId, getLogMode(all));
}

/*---------------------------------------------------------------------------*/

static int check_assist(struct all_t *all)
{
  char response[128];

  response[0] = '\0';
  phPbGuiGetData(all, all->guiId, "t_assist", response, 0);
  if (strcasecmp(response, "true") == 0) {
    phLogSimMessage(all->logId, LOG_DEBUG, "rise assist SRQ");
    phPbGuiChangeValue(all, all->guiId, "t_assist", "false");
    return F_ASSIST;
  }
  return 0;
}

static int check_fatal(struct all_t *all)
{
  char response[128];

  response[0] = '\0';
  phPbGuiGetData(all, all->guiId, "t_fatal", response, 0);
  if (strcasecmp(response, "true") == 0) {
    phLogSimMessage(all->logId, LOG_DEBUG, "rise fatal SRQ");
    phPbGuiChangeValue(all, all->guiId, "t_fatal", "false");
    return F_FATAL;
  }
  return 0;
}

static int check_stop(struct all_t *all)
{
  if (all->noGUI) {
    if (all->deltaStatus->getStopped() == true) {
      phLogSimMessage(all->logId, LOG_DEBUG, "stop detected");
      all->handler->stop();
    }
    return F_STOP;
  }

  char response[128];

  response[0] = '\0';
  phPbGuiGetData(all, all->guiId, "t_stop", response, 0);
  if (strcasecmp(response, "true") == 0) {
    if (all->deltaStatus->getStopped() == false) {
      phLogSimMessage(all->logId, LOG_DEBUG, "stop detected");
      all->operatorPause = true;
      all->deltaStatus->setStopped(true);
      all->handler->stop();
    }
    return F_STOP;
  } else {
    if (all->deltaStatus->getStopped() == true) {
      if (all->deltaStatus->getHandlerEmpty() == true) {
        all->handler->reset();
        all->deltaStatus->setHandlerEmpty(false);
      }
      if (all->deltaStatus->getHandlerIsReady() == true) {
        phLogSimMessage(all->logId, LOG_DEBUG, "start detected");
        all->deltaStatus->setStopped(false);
        all->handler->start();
      }
    }
    return 0;
  }
}

static int check_jam(struct all_t *all)
{
  char response[128];

  response[0] = '\0';
  phPbGuiGetData(all, all->guiId, "t_jam", response, 0);
  if (strcasecmp(response, "true") == 0) {
    if (all->deltaStatus->getJammed() == false) {
      phLogSimMessage(all->logId, LOG_DEBUG, "jam in handler detected");
      all->deltaStatus->setJammed(true);
      all->operatorPause = false;
      all->deltaStatus->setStopped(true);
      phPbGuiChangeValue(all, all->guiId, "t_stop", "true");
      all->handler->stop();
    }
    return F_JAM;
  } else {
    if (all->deltaStatus->getJammed() == true) {
      phLogSimMessage(all->logId, LOG_DEBUG, "handler freed");
      all->deltaStatus->setJammed(false);
      if (all->deltaStatus->getHandlerIsReady() == true) {
        all->deltaStatus->setStopped(false);
        phPbGuiChangeValue(all, all->guiId, "t_stop", "false");
        all->handler->start();
      }
    }
    return 0;
  }
}

static int check_dooropen(struct all_t *all)
{
  char response[128];

  response[0] = '\0';
  phPbGuiGetData(all, all->guiId, "t_dooropen", response, 0);
  if (strcasecmp(response, "true") == 0) {
    if (all->deltaStatus->getDoorOpen() == false) {
      phLogSimMessage(all->logId, LOG_DEBUG, "door open detected");
      all->deltaStatus->setDoorOpen(true);
      all->operatorPause = true;
      all->deltaStatus->setStopped(true);
      phPbGuiChangeValue(all, all->guiId, "t_stop", "true");
      all->handler->stop();
    }
    return F_DOOROPEN;
  } else {
    if (all->deltaStatus->getDoorOpen() == true) {
      phLogSimMessage(all->logId, LOG_DEBUG, "door closed detected");
      all->deltaStatus->setDoorOpen(false);
      if (all->deltaStatus->getHandlerIsReady() == true) {
        all->deltaStatus->setStopped(false);
        phPbGuiChangeValue(all, all->guiId, "t_stop", "false");
        all->handler->start();
      }
    }
    return 0;
  }
}

static int check_pause(struct all_t *all)
{
  char response[128];

  response[0] = '\0';
  phPbGuiGetData(all, all->guiId, "t_pause", response, 0);
  if (strcasecmp(response, "true") == 0) {
    phLogSimMessage(all->logId, LOG_DEBUG, "unset prober pause flag to resume");
    if (!all->dontSendPauseSRQ) {
      phLogSimMessage(all->logId, LOG_DEBUG, "rise pause SRQ");
    }
    all->dontSendPauseSRQ = 0;
    do {
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

static unsigned char getSrqByte(struct all_t *all)
{
  unsigned char srqByte = 0;

  all->deltaStatus->logStatus();
  // bit position 0 testsite A
  srqByte = 0xF & (unsigned char) (all->handler->getSitePopulationAsInt());

  // bit 4 handler is jammed, TODO what's the meaning?
  srqByte |= (1 << 4) * all->deltaStatus->getJammed();

  // bit 7 Handler is in ready state
  srqByte |= (1 << 7)
      * (all->deltaStatus->getHandlerIsReady() && !(all->deltaStatus->getStopped()));

  phLogSimMessage(all->logId, LOG_VERBOSE, "srqByte before mask %d", int(srqByte));

  // Mask out bits not needed
  srqByte &= all->srqMask;

  phLogSimMessage(all->logId, LOG_VERBOSE, "srqByte with mask %d", int(srqByte));

  return srqByte;
}

static void replyToQuery(struct all_t *all, const char *m)
{
  ++(all->queryErrorCount);

  if (all->queryError && (all->queryErrorCount % 7 == 0)) {
    phLogSimMessage(all->logId, LOG_DEBUG, "corrupt query reply from %s to %s ", m, " \\n");

    sendMessageEoc(all, " ", "\n");
  } else {
    sendMessage(all, m);
  }
}

/*--- service functions -----------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void service_start(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"", m);
  check_flags(all, F_STOP | F_JAM | F_DOOROPEN);

  bool startIgnored = false;

  if (all->deltaStatus->getHandlerIsReady() == true) {
    if (all->autoSetupDelay > 0) {
      startIgnored = true;
    } else {
      if (all->operatorPause == true) {
        startIgnored = true;
      }
    }

    if (startIgnored) {
      phLogSimMessage(all->logId, LOG_DEBUG, "start ignored: start must be initiated by operator ");
    } else {
      all->deltaStatus->setStopped(false);
      phPbGuiChangeValue(all, all->guiId, "t_stop", "false");
      all->handler->start();
      if (all->cmdReply)
        sendMessage(all, CMD_REPLY_MSG);
    }
  } else {
    if (all->deltaStatus->getJammed() == true) {
      phLogSimMessage(all->logId, PHLOG_TYPE_ERROR, "unable to start handler: Handler is jammed.");
    } else if (all->deltaStatus->getDoorOpen() == true) {
      phLogSimMessage(all->logId, PHLOG_TYPE_ERROR, "unable to start handler: Door is open. ");
    } else {
      phLogSimMessage(all->logId, PHLOG_TYPE_ERROR, "unable to start handler ");
    }
  }
  return;
}

static void service_identify(struct all_t *all, char *m)
{
  char message[64];

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"", m);

  /*
   * Returns the following handler information:
   * handler type
   * softwareversion
   */
  sprintf(message, "%s V%s", all->modelname, all->softwareVersion);
  sendMessage(all, message);
  return;
}

static void service_error(struct all_t *all, char *m)
{
  char message[64];

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"", m);

  /*
   * Returns 'No Error' if no error occurred
   */
  sprintf(message, "%s", "No Error");
  sendMessage(all, message);
  service_start(all, m);
  return;
}

static void service_readyfortest(struct all_t *all, char *m)
{
  char message[64];
  int srqByte;

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"", m);
  check_flags(all, F_STOP | F_JAM | F_DOOROPEN);

  srqByte = getSrqByte(all);
  /* CR8148 start */
  message[0] = '0' | (srqByte & 0xf);
  if (message[0] == '0') {
    message[0] = '1';
  } else if (message[0] == '1') {
    message[0] = '0';
  }
  message[1] = '\0';
  /* CR8148 end */
  replyToQuery(all, message);

  return;
}

/*
 * binning commands / queries
 */

static void service_bin(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"", m);
  check_flags(all, F_STOP | F_JAM | F_DOOROPEN);

  /*
   * Sends selected part into designated category.
   * Format: [ 1,2 ] bin (retest:0,1..99)
   *         ...
   */

  int errflg = 0;
  char c;
  int site = 0;
  char *posMsg = m;
  int bin = 0;
  char binString[64];

  if ((posMsg != NULL)) {
    phLogSimMessage(all->logId, LOG_VERBOSE, "analyse \"%s\"", posMsg);
    if (sscanf(posMsg, "bin%d", &bin) == 1) {
      all->handler->sendDeviceToBin(site, bin);
    }
  } else {
    phLogSimMessage(all->logId, PHLOG_TYPE_ERROR, "binning command is null, please check!");
  }

  if (all->cmdReply)
    sendMessage(all, CMD_REPLY_MSG);

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
  int i = 0;

  while (s1[i]) {
    s1[i] = tolower(s1[i]);
    ++i;
  }
}

static void removeEoc(char *s1)
{
  int lastChar;
  bool eocChar;
  do {
    lastChar = strlen(s1) - 1;

    if (lastChar >= 0) {
      eocChar = s1[lastChar] == '\n' || s1[lastChar] == '\r';
    } else {
      eocChar = false;
    }

    if (eocChar) {
      s1[lastChar] = '\0';
    }

  } while (eocChar);
}

static void handleMessage(struct all_t *all)
{
  char m[1024];

  /* copy string remove eoc and set to lowercase */
  strcpy(m, all->message);
  removeEoc(m);
  setStrlower(m);

  /* show message and log it */
  phPbGuiChangeValue(all, all->guiId, "f_req", m);
  phLogSimMessage(all->logId, LOG_DEBUG, "received %3d: \"%s\"", all->pbGpibStep, all->message);
  phPbGuiChangeValue(all, all->guiId, "f_answ", "");
  phPbGuiChangeValue(all, all->guiId, "f_srq", "");

  /* handle the message */
  setNotUnderstood(0, all);

  if (phToolsMatch(m, "idn\\?"))
    service_identify(all, m);
  else if (phToolsMatch(m, "error\\?"))
    service_error(all, m);
  else if (phToolsMatch(m, "rft\\?"))
    service_readyfortest(all, m);
  else if (phToolsMatch(m, "bin[1,2]+"))
    service_bin(all, m);

  /* else */
  else {
    phLogSimMessage(all->logId, PHLOG_TYPE_ERROR, "not understood \"%s\", invalid command.", m);
    char message[64] = "";
    sprintf(message, "%s", "Invalid Command");
    sendMessage(all, message);
    setNotUnderstood(1, all);
  }
}

/*---------------------------------------------------------------------------*/

static void handleGuiEvent(struct all_t *all)
{
  phGuiError_t guiError;
  char request[1024];
  phComError_t comError;
  phComGpibEvent_t sendEvent;

  switch (all->guiEvent.type) {
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
    if (strcmp(all->guiEvent.name, "p_quit") == 0) {
      phPbGuiDestroyDialog(all, all->guiId);
      all->guiId = 0;
      all->done = 1;

    } else if (strcmp(all->guiEvent.name, "p_start") == 0) {
      func_b_startLot(all);
    } else if (strcmp(all->guiEvent.name, "p_dump") == 0) {
      phLogDump(all->logId, PHLOG_MODE_ALL);
    } else if (strcmp(all->guiEvent.name, "p_incDebug") == 0) {
      incDebugLevel(all);
    } else if (strcmp(all->guiEvent.name, "p_decDebug") == 0) {
      decDebugLevel(all);
    } else if (strcmp(all->guiEvent.name, "p_req") == 0) {
      guiError = phPbGuiChangeValue(all, all->guiId, "f_req", "");
      checkGuiError(guiError, all);

      setWaiting(1, all);
      comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length,
          1000L * all->timeout);
      setWaiting(0, all);
      switch (checkComError(comError, all)) {
      case 0:
        strcpy(request, all->message);
        if (strlen(request) >= strlen(all->eoc)
            && strcmp(&request[strlen(request) - strlen(all->eoc)], all->eoc) == 0)
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
    } else if (strcmp(all->guiEvent.name, "p_answ") == 0) {
      guiError = phPbGuiGetData(all, all->guiId, "f_answ", request, 5);
      if (!checkGuiError(guiError, all))
        return;
      phLogSimMessage(all->logId, LOG_DEBUG, "trying to send message '%s'", request);
      strcat(request, all->eoc);
      setWaiting(1, all);
      comError = phComGpibSendVXI11(all->comId, request, strlen(request), 1000L * all->timeout);
      setWaiting(0, all);
      switch (checkComError(comError, all)) {
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
    } else if (strcmp(all->guiEvent.name, "p_srq") == 0) {
      sendEvent.type = PHCOM_GPIB_ET_SRQ;
      guiError = phPbGuiGetData(all, all->guiId, "f_srq", request, 5);
      if (!checkGuiError(guiError, all))
        return;
      sendEvent.d.srq_byte = (unsigned char) strtol(request, NULL, 0);
      setWaiting(1, all);
      comError = phComGpibSendEventVXI11(all->comId, &sendEvent, 1000L * all->timeout);
      setWaiting(0, all);
      if (!checkComError(comError, all)) {
        guiError = phPbGuiChangeValue(all, all->guiId, "f_srq", "");
        checkGuiError(guiError, all);
      }
    } else if (strcmp(all->guiEvent.name, "p_event") == 0) {
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
  if (guiError == PH_GUI_ERR_OK) {
    handleGuiEvent(all);
  }
  return;
}

static void checkGpibReceived(struct all_t *all)
{
  phComError_t comError;

  comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length, 1000000L);
  if (checkComError(comError, all) == 0) {
    doGpibStep(all);
    handleMessage(all);
  }
}

/*---------------------------------------------------------------------------*/

static void doWork(struct all_t *all)
{

  setError(0, all);
  all->done = 0;
  do {
    beAlife(all);

    checkGpibReceived(all);

    checkGuiEvent(all);

    if (all->handler->sendTestStartSignal()) {
      unsigned char srqSequence = getSrqByte(all);

      // bit 6 SRQ asserted
      srqSequence |= (1 << 6);
      sendSRQ(all, srqSequence);

    }

    if (all->handler->allTestsComplete() && all->deltaStatus->getHandlerEmpty() == false) {
      all->handler->logHandlerStatistics();

      all->deltaStatus->setHandlerEmpty(true);
      all->operatorPause = false;
      all->deltaStatus->setStopped(true);
      phPbGuiChangeValue(all, all->guiId, "t_stop", "true");
#ifdef SAVE_NEW_PLAYBACK_FORMAT
            fclose(all->newRecordFile);
#endif
    }

    doSubStep(all);
  } while (!all->done);

  if (!all->deltaStatus->getHandlerEmpty()) {
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

  while ((c = getopt(argc, argv, "A:d:i:t:m:s:M:p:D:N:Io:P:T:Qn")) != -1) {
    switch (c) {
    case 'A':
      all->autoSetupDelay = atof(optarg) * 1000;
      break;
    case 'd':
      all->debugLevel = atoi(optarg);
      break;
    case 'i':
      all->hpibDevice = strdup(optarg);
      break;
    case 't':
      all->timeout = atoi(optarg);
      break;

    case 'm':
      all->modelname = strdup(optarg);
      if (strcmp(optarg, "DB Design") == 0)
        all->model = DBDESIGN;
      else
        errflg++;
      break;
    case 's':
      all->numOfSites = atoi(optarg);
      break;
    case 'M':
      all->siteEnabledMask = atol(optarg);
      break;
    case 'p':
      if (strcmp(optarg, "one") == 0)
        all->pattern = Handler::all_one_not_one_is_working;
      else if (strcmp(optarg, "all") == 0)
        all->pattern = Handler::all_sites_working;
      else
        errflg++;
      break;
    case 'Q':
      all->queryError = true;
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
      if (strcmp(optarg, "1") == 0) {
        all->eoc = "\r\n";
      } else if (strcmp(optarg, "2") == 0) {
        all->eoc = "\n";
      } else
        errflg++;
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
    if (errflg) {
      fprintf(stderr, "usage: %s [<options>]\n", argv[0]);
      fprintf(stderr, " -d <debug-level>       -1 to 4, default: 0\n");

      fprintf(stderr, " -m <handler model>      handler model,\n");
      fprintf(stderr, "                         default: DB Design\n");
      fprintf(stderr, " -s <number>             number of sites, default: 1\n");
      fprintf(stderr, " -M <number>             site enabled mask, default all sites enabled \n");
      fprintf(stderr, " -p <pattern>            site population pattern, one of: \n");
      fprintf(stderr, "                         one (all sites, one not, one is working), \n");
      fprintf(stderr, "                         all (all sites working), default: all\n");
      fprintf(stderr, " -D <delay>              parts handling delay [sec], default: 1\n");
      fprintf(stderr,
          " -A <delay>              automatic start after initialization delay [sec]\n");
      fprintf(stderr, " -N <number>             number of parts to be handled before \n");
      fprintf(stderr, "                         end of test, default: infinite\n");
      fprintf(stderr,
          " -Q                      error in sending replies to queries (status?, srqbyte? \n");
      fprintf(stderr,
          "                         testpartsready? ) every 7th such query will result in  \n");
      fprintf(stderr,
          "                         a \" \\n\" being sent instead of the correct response \n");
      fprintf(stderr, " -i <gpib-interface>     GPIB interface device, default: hpib2 \n");
      fprintf(stderr, " -t <timeout>            GPIB timeout to be used [msec], default: 5000\n");
      fprintf(stderr, " -T <term string>        terminate message string, one of:\n");
      fprintf(stderr, "                         1 - \\r\\n, 2 - \\n, default: \\n \n");
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
  if (getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0) {
    fprintf(stderr, "The simulator can not be started by root user\n");
    return -1;
  }
  char szHostName[MAX_NAME_LENTH] = { 0 };
  gethostname(szHostName, MAX_NAME_LENTH - 1);

  char szVxi11Server[MAX_PATH_LENTH] = { 0 };
  getVxi11Path(szVxi11Server, MAX_PATH_LENTH);

  char szCmdline[MAX_CMD_LINE_LENTH] = { 0 };
  snprintf(szCmdline, MAX_CMD_LINE_LENTH, "%s -ip %s 1>/dev/null &", szVxi11Server, szHostName);

  system(szCmdline);

  sleep(1);

  phGuiError_t guiError;
  static char slaveGuiDesc[1024];

  /* Disable the generation of the HVM alarm */
  setenv("DISABLE_HVM_ALARM_GENERATION_FOR_TCI", "1", 1);

  /* init defaults */

  allg.debugLevel = 0;
  allg.hpibDevice = strdup("hpib2");
  allg.softwareVersion = strdup("1.00");
  //I don't know handlerNumber
  allg.handlerNumber = strdup("H-XYZ-001");
  allg.timeout = 5000;

  allg.modelname = strdup("DB Design");
  allg.model = DBDESIGN;

  allg.dontSendPauseSRQ = 0;
  allg.noGUI = 0;
  allg.autoStartTime = 0;

  allg.interactiveDelay = 0;
  allg.interactiveDelayCount = 0;

  allg.pbGpibStep = 0;
  allg.pbSubStep = 0;
  allg.doRecord = 0;
  allg.doPlayback = 0;

  allg.recordFile = NULL;
  allg.playbackFile = NULL;
  allg.newRecordFile = NULL;
//    allg.newPlaybackFile = NULL;

  allg.numOfSites = 1;
  allg.numberOfDevicesToTest = 1000;
  allg.handlingDelay = 1000;
  allg.autoSetupDelay = 0.0;
  allg.siteEnabledMask = allSitesEnabled;
  allg.pattern = Handler::all_sites_working;
  allg.reprobeMode = Handler::perform_reprobe_separately;
//    allg.populate_reprobe_pending_sites=true;
  allg.multiple_srqs = false;
  allg.queryError = false;
  allg.queryErrorCount = 0;
  allg.verifyTest = 0;
  allg.corruptBinData = false;
  allg.releasepartsWhenReady = false;
  allg.cmdReply = 0;
  allg.systemMode = 1;
  allg.emulationMode = 1;
  allg.errorBit = 0;
  allg.srqMask = 0xFF;

  allg.retestBinNumber = -1;
  allg.reprobeBinNumber = -2;

  strcpy(allg.recordName, "");
  strcpy(allg.recordData, "");
  allg.recordRetval = 0;
  allg.recordStep = -1;
  allg.recordUsed = true;

  allg.eoc = "\n";

  /* get real operational parameters */
  if (!arguments(argc, argv, &allg))
    exit(1);

  /* init internal values */
  allg.master = 1;
  allg.abortProbing = 0;

  phLogInit(&allg.logId, getLogMode(&allg), "-", NULL, "-", NULL, -1);

  /* open gpib communication */
  if (phComGpibOpenVXI11(&allg.comId, PHCOM_ONLINE, allg.hpibDevice, 0, allg.logId,
      PHCOM_GPIB_EOMA_NONE) != PHCOM_ERR_OK)
    exit(1);

  /* create user dialog */
  strcpy(slaveGuiDesc, slaveGuiDescHead);
  strcat(slaveGuiDesc, "Atom ");
  strcat(slaveGuiDesc, allg.modelname);
  strcat(slaveGuiDesc, slaveGuiDescTail);
  guiError = phPbGuiCreateUserDefDialog(&allg, &allg.guiId, allg.logId, PH_GUI_COMM_ASYNCHRON,
      slaveGuiDesc);
  checkGuiError(guiError, &allg);
  if (guiError != PH_GUI_ERR_OK)
    exit(1);
  phPbGuiShowDialog(&allg, allg.guiId, 0);

  /* create handler object */
  allg.handler = new Handler(allg.logId, allg.numOfSites, allg.siteEnabledMask, allg.autoSetupDelay,
      allg.handlingDelay, allg.numberOfDevicesToTest, allg.pattern, allg.reprobeMode,
      allg.corruptBinData);

  /* create Delta Status object */
  allg.deltaStatus = new DeltaStatus(allg.logId);

  /* set reprobe and retest bin numbers */

  allg.handler->setRetestCategory(allg.retestBinNumber);
  allg.handler->setReprobeCategory(allg.reprobeBinNumber);

  allg.operatorPause = false;
  allg.deltaStatus->setStopped(true);
  phPbGuiChangeValue(&allg, allg.guiId, "t_stop", "true");

#ifdef SAVE_NEW_PLAYBACK_FORMAT
    allg.newRecordFile = fopen("newRecordFile", "w");

    if ( ! allg.newRecordFile )
        doExitFail(&allg);
#endif

  /* enter work loop */
  doWork(&allg);

  exit(0);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
