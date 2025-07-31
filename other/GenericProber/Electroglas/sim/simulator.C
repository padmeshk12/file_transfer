/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.C
 * CREATED  : 15 Feb 2000
 *
 * CONTENTS : TEL prober simulator
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *          : Jiawei Lin, R&D Shanghai, CR 27092 &CR25172
 *          : Xiaofei Feng, R&D Shanghai, CR25072
 *          : Danglin Li, R&D Shanghai, CR31204
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Feb 2000, Michael Vogt, created
 *             4 Apr 2000, Chris Joyce modified for Electroglas
 *          : August 2005, Jiawei Lin, CR27092 &CR25172
 *              Implement the "ur" command to support the information query
 *              from Tester. Such information contains WCR (Wafer Configuration
 *              Record) of STDF, like Wafer Size, Wafer Unit.
 *              All these information could be
 *              retrieved by Tester with the PROB_HND_CALL "ph_get_status"
 *          : Dec 2005, Xiaofei Feng, CR25072
 *              Modified DieManager::getDie() member funcions to fix segmentation
 *              fault.
 *          : Dec 2006, Dang-lin Li, CR31204 & CR31253
 *              Implement the "TR" command to support to reprobe from prober
 *              simulator, involved functions serviceTC and serviceET.
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
#if __GNUC__ >= 3
#include <iostream>
#include <utility>
#endif

#include <vector>
#include <list>
/*Begin of Huatek Modifications, Luke Lan, 02/27/2001*/
/*Issue Number: 64      */
#include <algorithm>
/*End of Huatek Modifications */
/*Begin of Huatek Modifications, Luke Lan, 05/31/2001*/
/*Issue Number: 20      */
#include <errno.h>
/*End of Huatek Modifications */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
/*Begin of Huatek Modifications, Luke Lan, 05/31/2001*/
/*Issue Number: 89      */
/*Use 'string' replace 'string.h' */
#include <string>
/*End of Huatek Modifications */
#include <signal.h>
#include <cstring>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/


#define NEW_PROBER_CLASS

// #undef RETURN_ID
#define RETURN_ID


#define LOG_NORMAL         PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG          PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE        PHLOG_TYPE_MESSAGE_2

#define EOC "\r\n"

#define CASS_COUNT         4
#define WAF_SLOTS          25
#define WAF_MAXDIM         100

#define WAF_COUNT          3

#define MAX_PROBES         32

#define DIP_POSITIONS      8
#define DIP_GROUPS         20

#define DIEFLAG_EMPTY      0x0000
#define DIEFLAG_OUTSIDE    0x0001
#define DIEFLAG_BINNED     0x0002
#define DIEFLAG_INKED      0x0004

#define SRQ_AND_MSG_DELAY       6


#define F_START            0x0001
#define F_ASSIST           0x0002
#define F_FATAL            0x0004
#define F_PAUSE            0x0008
#define F_WAFEND           0x0010
#define F_ALL              0xffff

#define CORRUPT_TEST_START  11

#define Die(a, x, y)       (a)->chuck.wafer.d[(x)+(a)->xmin][(y)+(a)->ymin]


enum proberMode_t { mProber, mLearnList, mSmarTest };
enum model_t { EG2001, EG2010, EG3001, EG4085, EG4060, EG4080, EG4090, EG4200, EG5300 };
enum family_t { PVS, COMMANDER };
enum waf_stat { WAF_UNTESTED, WAF_INTEST, WAF_TESTED };

#ifdef kawtest
int TC_hack_count = 0;  // kaw for testing CR13399
#endif

//////////////////////////////  Interface with VXI11 server //////////////////////
#include <mqueue.h>
#include <time.h>

#define MAX_PATH_LENTH 512
#define MAX_NAME_LENTH 128
#define MAX_CMD_LINE_LENTH 640

mqd_t from_vxi11_server_mq;
mqd_t to_vxi11_server_mq;
mqd_t to_vxi11_server_srq_mq;


phComError_t phComGpibOpenVXI11(
    phComId_t *newSession       /* The new session ID (result value). Will
				   be set to NULL, if an error occured */,
    phComMode_t mode            /* Commun mode: online|offline|simulation  */,
    const char *device                /* Symbolic Name of GPIB card in workstation*/,
    int port                    /* GPIB port to use */,
    phLogId_t logger            /* The data logger to use for error and
				   message log. Use NULL if data log is not 
				   used.                                    */,
    phComGpibEomAction_t act    /* To ensure that the EOI line is
                                   released after a message has been
                                   send, a end of message action may
                                   be defined here. The EOI line can
                                   not be released directly, only as a
                                   side affect of a serial poll or a
                                   toggle of the ATN line. */
) {
     
     
     ///////////////// Interface with VX11 server
    
     from_vxi11_server_mq = mq_open("/TO_SIMULATOR", O_RDONLY); 
     if(from_vxi11_server_mq  == -1) {
        printf("Error: cannot open message queue /TO_SIMULATOR\n"); 
        return (phComError_t)-1;
     } 

     to_vxi11_server_mq = mq_open("/FROM_SIMULATOR", O_WRONLY); 
     if(to_vxi11_server_mq  == -1) {
        printf("Error: cannot open message queue /FROM_SIMULATOR\n"); 
        return (phComError_t)-1;
     } 

     to_vxi11_server_srq_mq = mq_open("/FROM_SIMULATOR_SRQ", O_WRONLY); 
     if(to_vxi11_server_srq_mq  == -1) {
        printf("Error: cannot open message queue /FROM_SIMULATOR_SRQ\n"); 
        return (phComError_t)-1;
     } 
     
     ///////////////// Interface with VX11 server
     
     struct phComStruct *tmpId;
     /* allocate new communication data type */
     *newSession = NULL;
     tmpId = (struct phComStruct*) malloc(10);
     *newSession = tmpId;
     
     phComError_t return_val = (phComError_t)0;
     
     return return_val;

}

phComError_t phComGpibCloseVXI11(
    phComId_t session        /* session ID of an open GPIB session */
) {

    phComError_t return_val =(phComError_t)0;
     
    return return_val;
}

phComError_t phComGpibSendVXI11(
    phComId_t session           /* session ID of an open GPIB session. */,
    char *h_string              /* message to send, may contain '\0'
				   characters */,
    int length                  /* length of the message to be sent,
                                   must be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message must be
                                   sent */
) {

   phComError_t return_val =(phComError_t)0;
   
   if(mq_send(to_vxi11_server_mq, h_string,length, 0) == -1) { 
         printf("Error: error sending message:\n%s", h_string); 
   }
     
   return return_val;

}



phComError_t phComGpibReceiveVXI11(
    phComId_t session        /* session ID of an open GPIB session. */,
    const char **message              /* received message, may contain '\0' 
				   characters. The calling function is 
				   not allowed to modify the passed 
				   message buffer */,
    int *length                 /* length of the received message,
                                   will be < PHCOM_MAX_MESSAGE_LENGTH */,
    long timeout                /* timeout in usec, until the message 
				   must be received */
) {

   phComError_t return_val =(phComError_t)0;
   static char buffer[8192 + 1]; 
   ssize_t bytes_read;
   while(1) { 
      // will block here until a message is received
      //bytes_read = mq_receive(from_vxi11_server_mq, buffer, 8192, NULL);
      struct timespec currenttime_before;
      struct timespec abs_timeout;
      clock_gettime( CLOCK_REALTIME, &currenttime_before);  
      abs_timeout.tv_sec  = currenttime_before.tv_sec  + timeout/1000000;
      abs_timeout.tv_nsec = currenttime_before.tv_nsec + (timeout % 1000000)*1000; 
      bytes_read = mq_timedreceive(from_vxi11_server_mq, buffer, 8192, NULL, &abs_timeout);
      if(bytes_read >= 0) { 
         buffer[bytes_read] = '\0'; 
	 *message = buffer; 
	 break; 
      } else { 
         //protection code since this is an abnormal case let's go slow here
         //sleep(1);
	 return PHCOM_ERR_TIMEOUT;
      }
   }  
     
   return return_val;

}

phComError_t phComGpibSendEventVXI11(
    phComId_t session          /* session ID of an open GPIB session */,
    phComGpibEvent_t *event     /* current pending event or NULL */,
    long timeout                /* time in usec until the transmission
				   (not the reception) of the event
				   must be completed */
) {

   phComError_t return_val =(phComError_t)0;
   
   
   //event->type = PHCOM_GPIB_ET_SRQ;
   
   char srq[10];
   srq[0] = event->d.srq_byte ;
   srq[1] = '\0';
   
   if(mq_send(to_vxi11_server_srq_mq, srq, strlen(srq), 0) == -1) { 
         printf("Error: error sending SRQ message:\n%s", srq); 
   }
     
   return return_val;

}

void getVxi11Path(char *szpath, int lenth)
{
    char *szEnv = getenv("WORKSPACE");
    if(NULL == szEnv)
    {
        snprintf(szpath, lenth, "/opt/hp93000/testcell/phcontrol/drivers/Generic_93K_Driver/tools/bin/vxi11_server");
    }
    else
    {
        snprintf(szpath, lenth, "%s/ph-development/phcontrol/drivers/Generic_93K_Driver/common/mhcom/bin/vxi11_server", szEnv);
    }
}

/*--- class definitions -----------------------------------------------------*/

/*****************************************************************************
 *
 * Exception base class
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Base class from which all simulator exceptions may be derived.
 *
 *****************************************************************************/
class Exception
{
public:
    virtual void printMessage()=0;
};

/*****************************************************************************
 *
 * RangeException
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Out of range error.
 *
 *****************************************************************************/
class RangeException : public Exception
{
public:
    RangeException(const char *rStr);
    virtual void printMessage();
    virtual ~RangeException(){};
private:
    Std::string rangeString;
};

/*****************************************************************************
 *
 * FormatException
 *
 * Authors: Chris Joyce
 *
 * Description:
 * String string does not conform to expected boolean format.
 *
 *****************************************************************************/

class FormatException : public Exception
{
public:
    FormatException(const char *fStr);
    virtual void printMessage();
    virtual ~FormatException(){};
private:
    Std::string formatString;
};

/*****************************************************************************
 *
 * ProberException
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Some unexpected event has occurred
 *
 *****************************************************************************/

class ProberException : public Exception
{
public:
    ProberException(const char *fStr);
    virtual void printMessage();
    virtual ~ProberException(){};
private:
    Std::string proberString;
};












/*****************************************************************************
 *
 * Log Object base class
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Object with the ability to send messages to log files.
 *
 *****************************************************************************/
class LogObject
{
public:
    LogObject(phLogId_t l);
    LogObject(int debugLevel);
protected:
    phLogId_t        logId;
private:
    phLogMode_t getLogMode(int debugLevel);
};


/*****************************************************************************
 *
 * Simple timer
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Timer timer is started from the start() call and elapsed time in msec
 * is returned from getElapsedMsecTime()
 *
 *****************************************************************************/

class Timer : public LogObject
{
public:
    Timer(phLogId_t l);
    void start();
    double getElapsedMsecTime();
    void wait(double msec);
private:
    int localDelay_ms(int milliseconds);
    struct timeval startTime;
};


class Communication 
{
public:
    // singleton method
    static Communication* instance();
    static Communication* instance(int debugLevel, char *deviceName);
    phComId_t getSession() { return comId; };
    phLogId_t getLogger() { return logId; };

    void setNormalLog();
    void resetLog();

protected:
    Communication(int debugLevel, const char *gpioName);

private:
    static Communication* instancePtr;

    phLogId_t logId;
    phComId_t comId;
    phLogMode_t modeFlags;

    // Copy constructor, private so it can not be used and not defined
    Communication& operator= (const Communication&);
    Communication(const Communication&);
};



class PositionXY
{
public:
    PositionXY(int x, int y) { x_pos=x; y_pos=y; };
    PositionXY( const PositionXY& );
    PositionXY();
    int getX() const { return x_pos; };
    int getY() const { return y_pos; };
    void setX(int x) { x_pos=x; };
    void setY(int y) { y_pos=y; };
    bool operator==(const PositionXY &) const;
    PositionXY& operator+=(const PositionXY&);
protected:
    int x_pos;
    int y_pos;
};

PositionXY operator+(const PositionXY&, const PositionXY&);

class DieXY : public PositionXY
{
public:
    enum { NOT_BINNED = -1 };

    DieXY(int x, int y);
    DieXY(const PositionXY &Pos);
    int getBin() const { return bin; };
    void setBin(int b) { bin=b; };
protected:
    int bin;
};

class SubDieXY : public PositionXY
{
public:
    SubDieXY(int x, int y);
    SubDieXY(const PositionXY &Pos);
    ~SubDieXY();
    SubDieXY( const SubDieXY& );
    SubDieXY& operator=(const SubDieXY& );

    int getBin(int site) const;
    void setBin(int site, int b);
    bool operator==(const SubDieXY &) const;

protected:
    int numOfBins;
private:
    Std::vector<int> subDieBin;
};


class BinningManager : public LogObject
{
public:
    BinningManager(phLogId_t l);
    virtual void initialize()=0;
    virtual bool step()=0;
    virtual void setBin(const PositionXY &p, int b)=0;
    virtual void setBin(int b)=0;
    virtual int getBin() const =0;
    virtual void clear()=0;
    virtual bool onWafer(const PositionXY&p)=0;
    virtual const PositionXY &getPos() const=0;
    virtual const PositionXY &getMinPos() const=0;
    virtual const PositionXY &getMaxPos() const=0;
    virtual void selfTest() const=0;
    virtual void printDieInfo() = 0;
};


class DieManager : public BinningManager
{
public:
    DieManager(phLogId_t l); 
    virtual ~DieManager(){};
    virtual void initialize();
    virtual bool step();
    virtual void setBin(const PositionXY &p, int b);
    virtual void setBin(int b);
    virtual int getBin() const;
    virtual int getBin(const PositionXY &p) const;
    virtual void clear();
    virtual bool onWafer(const PositionXY&p);

    void addDie(DieXY *p);
    const PositionXY &getPos() const;
    virtual const PositionXY &getMinPos() const;
    virtual const PositionXY &getMaxPos() const;

//    void setPos(PositionXY& pos);
    void printDieInfo();
    virtual void selfTest() const;
    int getNumberOfDies() const;
protected:
    DieXY *getDie(const PositionXY& p); 
    const DieXY *getDie(const PositionXY& p) const; 
    DieXY &getDie();

    Std::vector<DieXY*> dieList;
    Std::vector<DieXY*>::iterator dieIterator;
private:
    bool atEndOfDieList() const;
    PositionXY minPos;
    PositionXY maxPos;
    void addDieToList(DieXY *p);
};


class SubDieManager : public BinningManager
{
public:
    SubDieManager(phLogId_t l); 
    virtual ~SubDieManager(){};
    void initialize();
    bool step();

    void setBin(int b);
    int getBin() const;

    int getSiteNum();

    void clear();
    void clearSubDie();
    void addSubDiePosition( const PositionXY &p );
    void addSubDie( SubDieXY d );
    SubDieXY &getSubDie();

    const PositionXY &getPos() const;
    void printDieInfo();
    virtual bool onWafer(const PositionXY&p);
    virtual void setBin(const PositionXY &p, int b);
    virtual void setBin(const PositionXY &posDie, const PositionXY &posSubdie, int b);
    virtual void selfTest() const;

    virtual const PositionXY &getMinPos() const;
    virtual const PositionXY &getMaxPos() const;
protected:
    const PositionXY &getSubDiePosition() const;


    Std::list<SubDieXY> subDieList;
    Std::list<SubDieXY>::iterator si;
    Std::list<PositionXY> subDiePosition;
    Std::list<PositionXY>::iterator pi;
    int siteNumber;
private:
    int numOfSites;
    int atLastDiePosition();
    int atLastSubDiePosition();
    bool subdieNumOfSitesSet;
    PositionXY minPos;
    PositionXY maxPos;
};

/*****************************************************************************
 *
 * ProberNeedle
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Generic prober simulator
 *
 * NEW_PROBER_CLASS
 *
 *****************************************************************************/

class ProbeNeedle : public LogObject
{
public:
    ProbeNeedle(phLogId_t l, const PositionXY &prim);
    PositionXY getPosition(int probeNum=0) const;
    void addProbePosition(const PositionXY &rel);
    int getNumberOfNeedles() const;
    void moveProber(const PositionXY &pos);
protected:
private:
    PositionXY primary;
    Std::vector<PositionXY> relative;
};




/*****************************************************************************
 *
 * Prober
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Abstract base class generic prober simulator
 *
 *****************************************************************************/

struct ProberSetup;


class Prober : public LogObject
{
public:
    // Prober Status
    enum eProberStatus { stopped, loadingWafer, probing };

    // Prober Mode
    enum eProberMode { learnlist, prober, smartest  };

    // Wafer Stepping Mode
    enum eProberSteppingMode { simple, efficient, compatible };

    Prober(phLogId_t l, const ProberSetup&);
    virtual ~Prober() {delete proberTimer;};

    // setup wafer
    virtual void loadWafer();

    // get current first probe position 
    virtual PositionXY getProbePosition() const;

    // bin die
    virtual void binDie(int bin, int probeNum=0) =0;

    // step to next probe position
    // return true step success
    //        false at end of wafer
    virtual bool stepProbe();

    // is probe probeNum on wafer
    virtual bool proberOnWafer(int probeNum);

    // print wafer info
    virtual void printWaferInfo();

    virtual int getNumberOfProbes() const;

    // move probe needles to primary postion p
    virtual void moveProber(const PositionXY &p);

    virtual bool sendTestStartSignal();
    virtual void start();
    virtual void stop();

protected:
    BinningManager *binningManager;
    ProbeNeedle *probeNeedle;
    eProberMode proberMode;
private:
    void selfTest() const;
    eProberSteppingMode steppingMode;
    eProberStatus status;
    bool autoSetupInit;
    bool running;
    double autoSetupDelayMsec;
    double handlingDelayMsec;
    Timer *proberTimer;
    int waferTestCount;
    int numOfWafersToTest;
};

struct ProberSetup
{
    ProberSetup();
    BinningManager *binningManager;
    ProbeNeedle *probeNeedle;
    Prober::eProberSteppingMode steppingMode;
    Prober::eProberMode proberMode;
    double autoSetupDelayMsec;
    double handlingDelayMsec;
    int numOfWafersToTest;
};



/*****************************************************************************
 *
 * DieProber
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Prober for probing Subdies
 *
 *****************************************************************************/

struct DieProberSetup;


class DieProber : public Prober
{
public:
    DieProber(phLogId_t l, const DieProberSetup&);

    // bin die
    virtual void binDie(int bin, int probeNum=0);

    void clearDieList();
    void addDie(DieXY *p);
protected:
    DieManager  *dieManager;
};

struct DieProberSetup : public ProberSetup
{
    DieProberSetup();
};


/*****************************************************************************
 *
 * SubDieProber
 *
 * Authors: Chris Joyce
 *
 * Description:
 * Prober for probing Subdies
 *
 *****************************************************************************/

struct SubDieProberSetup;


class SubDieProber : public Prober
{
public:
    SubDieProber(phLogId_t l, const SubDieProberSetup&);

    // bin die
    virtual void binDie(int bin, int probeNum=0);

    int getSiteNumber();

    void clearSubDieList();
    void clearSubDiePositionList();
    void addSubDiePosition( const PositionXY &p );
    void addSubDie( SubDieXY d );
    void moveSubDieProber(const PositionXY &p);
protected:
    SubDieManager  *subDieManager;
    PositionXY subDiePosition;
};

struct SubDieProberSetup : public ProberSetup
{
    SubDieProberSetup();
};


const PositionXY HOME_POSITION(-999,-999);

/*--- class implementations -------------------------------------------------*/



/*--- RangeException --------------------------------------------------------*/

RangeException::RangeException(const char *rStr) :
    rangeString(rStr)
{
}

void RangeException::printMessage()
{
    Std::cerr << rangeString << "\n";
}

/*--- FormatException -------------------------------------------------------*/

FormatException::FormatException(const char *fStr) :
    formatString(fStr)
{
}

void FormatException::printMessage()
{
    Std::cerr << formatString << "\n";
}

/*--- ProberException -------------------------------------------------------*/

ProberException::ProberException(const char *fStr) :
    proberString(fStr)
{
}

void ProberException::printMessage()
{
    Std::cerr << proberString << "\n";
}


/*--- Timer -----------------------------------------------------------------*/

Timer::Timer(phLogId_t l) : LogObject(l)
{
    start();
}
/*--- Timer public member functions -----------------------------------------*/

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

void Timer::wait(double wait_ms)
{
    start();

    do {

        localDelay_ms( int( wait_ms - getElapsedMsecTime() )  );

    } while ( getElapsedMsecTime() < wait_ms );

    return;
}

/*--- Timer private member functions ----------------------------------------*/

int Timer::localDelay_ms(int milliseconds)
{
    int seconds;
    int microseconds;
    struct timeval delay;

    /* ms must be converted to seconds and microseconds ** for use by select(2)*/
    if (milliseconds > 999) {
        microseconds = milliseconds % 1000;
        seconds = (milliseconds - microseconds) / 1000;
        microseconds *= 1000;
    } 
    else if ( milliseconds > 0 )
    {
        seconds = 0;
        microseconds = milliseconds * 1000;
    }
    else
    {
        seconds = 0;
        microseconds = 0;
    }

    delay.tv_sec = seconds;
    delay.tv_usec = microseconds;

    /* return 0, if select was interrupted, 1 otherwise */
    if (select(0, NULL, NULL, NULL, &delay) == -1 &&
        errno == EINTR)
        return 0;
    else
        return 1;
}


/*--- Communication ---------------------------------------------------------*/

Communication *Communication::instancePtr = 0;

Communication *Communication::instance() {

    if(!instancePtr) 
    {
        instancePtr = new Communication(0,"hpib");
    }

    return instancePtr;
}

Communication *Communication::instance(int debugLevel, char *deviceName) {

    if(!instancePtr) 
    {
        instancePtr = new Communication(debugLevel,deviceName);
    }
    return instancePtr;
}

Communication::Communication(int debugLevel, const char *deviceName) 
{
    modeFlags = PHLOG_MODE_NORMAL;

    /* open message logger */
    switch (debugLevel)
    {
      case -1:
	modeFlags = PHLOG_MODE_QUIET;
	break;
      case 0:
	modeFlags = PHLOG_MODE_DBG_0;
	break;
      case 1:
	modeFlags = PHLOG_MODE_DBG_1;
	break;
      case 2:
	modeFlags = PHLOG_MODE_DBG_2;
	break;
      case 3:
	modeFlags = PHLOG_MODE_DBG_3;
	break;
      case 4:
	modeFlags = PHLOG_MODE_DBG_4;
	break;
      case 5:
	modeFlags = PHLOG_MODE_DBG_4;
	break;
    }
    phLogInit(&logId, modeFlags, "-", NULL, "-", NULL, 0);

    /* open gpib communication */
    phLogSimMessage(logId, LOG_DEBUG,"Open (\"%s\") ", deviceName);    

    if (phComGpibOpenVXI11(&comId, PHCOM_ONLINE, 
	deviceName, 0, logId, PHCOM_GPIB_EOMA_NONE) != PHCOM_ERR_OK)
    { 
	exit(1);
    }
    else
    {
       phLogSimMessage(logId, LOG_VERBOSE,"GpibOpen PASSED ");    
    }
    return;
}

void Communication::setNormalLog() 
{
    phLogSetMode(logId,PHLOG_MODE_DBG_0);
}

void Communication::resetLog() 
{
    phLogSetMode(logId,modeFlags);
}

/*--- PositionXY ------------------------------------------------------------*/
PositionXY::PositionXY() : x_pos(0), y_pos(0)
{
}

PositionXY::PositionXY( const PositionXY& r )
{
    setX(r.getX());
    setY(r.getY());
}

bool PositionXY::operator==(const PositionXY &a) const
{
    return ( x_pos==a.getX() && y_pos==a.getY() );
}

PositionXY& PositionXY::operator+=(const PositionXY& a)
{
    x_pos+=a.x_pos;
    y_pos+=a.y_pos;

    return *this;
}


/*--- PositionXY helper functions -------------------------------------------*/

PositionXY operator+(const PositionXY &a, const PositionXY &b)
{
    PositionXY r=a;

    r+=b;

    return r;
}

/*--- DieXY -----------------------------------------------------------------*/
DieXY::DieXY(int x, int y) : PositionXY(x,y), bin(NOT_BINNED)
{
}
DieXY::DieXY(const PositionXY &Pos) : PositionXY(Pos), bin(NOT_BINNED)
{
}

/*--- SubDieXY --------------------------------------------------------------*/

SubDieXY::SubDieXY(int x, int y) : PositionXY(x,y), numOfBins(0)
{
}
SubDieXY::SubDieXY(const PositionXY &Pos) : PositionXY(Pos), numOfBins(0)
{
}

SubDieXY::SubDieXY( const SubDieXY& r) : PositionXY( r ), numOfBins(0)
{
#if 0
    Std::cout << "SubDieXY copy constructor number of bins is " << r.numOfBins << Std::endl;
    Std::cout << "SubDieXY while my number of bins is " << numOfBins << Std::endl;
#endif

    for ( int i=1 ; i <= r.numOfBins ; ++i )
    {
        setBin(i,r.getBin(i));
    }
}

SubDieXY::~SubDieXY() 
{
#if 0
    Std::cout << "SubDieXY destructor number of sites is " << numOfBins << Std::endl;
#endif
    numOfBins=0;
    subDieBin.erase( subDieBin.begin(), subDieBin.end() );
}

bool SubDieXY::operator==(const SubDieXY &a) const
{
    return ( x_pos==a.getX() && y_pos==a.getY() );
}


SubDieXY &SubDieXY::operator=( const SubDieXY& r)
{
#if 0
    Std::cout << "SubDieXY assignment operator number of sites is " << numOfBins << Std::endl;
#endif

    for ( int i=1 ; i <= r.numOfBins ; ++i )
    {
        setBin(i,r.getBin(i));
    }

    return *this;
}

int SubDieXY::getBin(int site) const
{
    Std::vector<int>::const_iterator i;

#if 0
    Std::cout << "SubDieXY::getBin " << site << Std::endl;
#endif

    i=subDieBin.begin() + site -1;

    return *i;
}

void SubDieXY::setBin(int site, int b)
{

    if ( site > numOfBins )
    {
#if 0
        Std::cout << "SubDieXY::setBin push on bin " << b << Std::endl;
#endif
        numOfBins++;
        subDieBin.push_back(b);
    }
    else
    {
        Std::vector<int>::iterator di;

#if 0
        Std::cout << "SubDieXY::setBin numOfBins is " << numOfBins << " replace bin " << site << " with " << b << Std::endl;
#endif
        di=subDieBin.begin() + site -1;
        *di=b;
    }
}

/*--- BinningManager --------------------------------------------------------*/
BinningManager::BinningManager(phLogId_t l) : LogObject(l)
{
}

/*--- DieManager ------------------------------------------------------------*/

/*--- DieManager constructor ------------------------------------------------*/
DieManager::DieManager(phLogId_t l) : BinningManager(l)
{
#if __GNUC__ >= 3
    dieIterator = dieList.begin();
#else
    dieIterator=0;
#endif
}

/*--- DieManager public member functions ------------------------------------*/

void DieManager::initialize()
{
    phLogSimMessage(logId, LOG_VERBOSE,"DieManager::initialize");    

    dieIterator=dieList.begin();
    return;
}

bool DieManager::step()
{
    phLogSimMessage(logId, LOG_VERBOSE,"DieManager::step()");    

    if ( !atEndOfDieList() )
        dieIterator++;

    return !atEndOfDieList();
}       

void DieManager::setBin(const PositionXY &p, int b)
{
    phLogSimMessage(logId, LOG_VERBOSE,"DieManager::setBin (%d,%d) bin %d ", p.getX(), p.getY(),b);    

    DieXY *pDie=getDie(p);

    if ( !pDie )
    {
	phLogSimMessage(logId, PHLOG_TYPE_ERROR,
	    "DieManager::setBin die not found at position %d,%d ", 
	    p.getX(),p.getY());
        throw FormatException("unknown die");
    }

    pDie->setBin(b);
}

void DieManager::setBin(int b)
{
    if ( atEndOfDieList() )
    {
	phLogSimMessage(logId, PHLOG_TYPE_ERROR,
	    "DieManager::setBin(): Attempt to bin to die when already off edge");
        throw RangeException("DieManager::setBin(): Attempt to bin to die when already off edge");
    }

    (*dieIterator)->setBin(b);
}

int DieManager::getBin() const
{
    return (*dieIterator)->getBin();
}

int DieManager::getBin(const PositionXY &p) const
{
    phLogSimMessage(logId, LOG_VERBOSE,"DieManager::getBin (%d,%d) ", p.getX(), p.getY());    

    const DieXY *pDie=getDie(p);

    if ( !pDie )
    {
	phLogSimMessage(logId, PHLOG_TYPE_ERROR,
	    "DieManager::setBin die not found at position %d,%d ", 
	    p.getX(),p.getY());
        throw FormatException("unknown die");
    }

    return pDie->getBin();
}

void DieManager::clear()
{
    dieList.erase( dieList.begin(),  dieList.end() );
}

bool DieManager::onWafer(const PositionXY &p)
{
    DieXY *pDie=getDie(p);

    return (pDie) ? true : false;
}

void DieManager::addDie(DieXY *p)
{
    if ( onWafer(*p) )
    {
        phLogSimMessage(logId, LOG_VERBOSE,"already have this die ");
        delete p;
    }
    else
    {
        addDieToList(p);
    }
}

const PositionXY &DieManager::getMinPos() const
{
    return minPos;
}

const PositionXY &DieManager::getMaxPos() const
{
    return maxPos;
}

const PositionXY &DieManager::getPos() const
{
    if ( atEndOfDieList() )
    {
        throw ProberException("DieManager::getPos() after step off end");
    }
    return *(*dieIterator);
}

void DieManager::printDieInfo()
{
    int bin;
    PositionXY pos;
    char results[1024];
    char oneResult[8];

    for ( int y = minPos.getY() ; y <= maxPos.getY() ; ++y )
    {
        strcpy(results,"");
        for ( int x = minPos.getX() ; x <= maxPos.getX() ; ++x )
        {
            pos = PositionXY(x,y);

            if ( onWafer( pos ) )
            {
                bin = getBin( pos );
                sprintf(oneResult, "%3d ", bin );
            }
            else
            {
                strcpy(oneResult, "    ");
            } 
            strcat(results,oneResult);
        }
        phLogSimMessage(logId, LOG_DEBUG, "wafer result: %s", results);
    }


//    int bin;
//    int x;
//    int y;
//    int current_line = -1;
//    int i;
//
//    char results[1024];
//    char oneResult[8];
//
//    initialize();
//    strcpy(results,"");
//    do {
//        bin = getBin();
//        const PositionXY& p = getPos();
//        x = p.getX();
//        y = p.getY();
//
//        if ( y != current_line )
//        {
//            if ( *results )
//            {
//                phLogSimMessage(logId, LOG_DEBUG, "wafer result: %s", results);
//                strcpy(results,"");
//            }
//            current_line = y;
//        }
//
//        sprintf(oneResult, "%3d ", bin );
//        strcat(results,oneResult);
//
//    } while ( step() );
//
//    if ( *results )
//    {
//        phLogSimMessage(logId, LOG_DEBUG, "wafer result: %s", results);
//    }

    sleep(5);
}

int DieManager::getNumberOfDies() const
{
    return dieList.end() - dieList.begin();
}

void DieManager::selfTest() const
{
    if ( getNumberOfDies() < 1 )
    {
        throw ProberException("DieManager:: no dies setup");
    }
    return;
}


/*--- DieManager protected member functions ---------------------------------*/

DieXY *DieManager::getDie(const PositionXY &p)
{
    DieXY *pDie=0;
    Std::vector<DieXY*>::iterator i;

    phLogSimMessage(logId, LOG_VERBOSE,"DieManager::getDie (%d,%d) ", p.getX(), p.getY());    

    for ( i = dieList.begin() ; i!=dieList.end() && !pDie ; i++ )
    {
        if ( *(*i) == p )
        {
            pDie= *i;
        }
    }

    
    if ( pDie != NULL )
    {
      phLogSimMessage(logId, LOG_VERBOSE,"DieManager::getDie P%p ", pDie);
    }
    else
    {
      phLogSimMessage(logId, LOG_VERBOSE,"DieManager::No match die found. ");
    }

    return pDie;
}

const DieXY *DieManager::getDie(const PositionXY &p) const
{
    const DieXY *pDie=0;
    Std::vector<DieXY*>::const_iterator i;

    phLogSimMessage(logId, LOG_VERBOSE,"DieManager::getDie (%d,%d) const ", p.getX(), p.getY());    

    for ( i = dieList.begin() ; i!=dieList.end() && !pDie ; i++ )
    {
        if ( *(*i) == p )
        {
            pDie= *i;
        }
    }

    if ( pDie != NULL )
    {
      phLogSimMessage(logId, LOG_VERBOSE,"DieManager::getDie P%p ", pDie);
    }
    else
    {
      phLogSimMessage(logId, LOG_VERBOSE,"DieManager::No match die found. ");
    }

    return pDie;
}


/*--- DieManager private member functions -----------------------------------*/

bool DieManager::atEndOfDieList() const
{
    return (dieIterator == dieList.end());
}

void DieManager::addDieToList(DieXY *p)
{
    if ( getNumberOfDies() == 0 )
    {
        minPos = *p;
        maxPos = *p;
    }
    else
    { 
        int x = p->getX();
        int y = p->getY();

        if ( x < minPos.getX() )
        {
            minPos.setX(x);
        }
        else if ( x > maxPos.getX() )
        {
            maxPos.setX(x);
        }

        if ( y < minPos.getY() )
        {
            minPos.setY(y);
        }
        else if ( y > maxPos.getY() )
        {
            maxPos.setY(y);
        }
    }

    dieList.push_back(p);
    return;
}


/*--- SubDieManager ---------------------------------------------------------*/
/*--- SubDieManager ---------------------------------------------------------*/
/*--- SubDieManager constructor ---------------------------------------------*/
/*--- SubDieManager public member functions ---------------------------------*/

SubDieManager::SubDieManager(phLogId_t l) : BinningManager(l),
    siteNumber(0),
    numOfSites(0), 
    subdieNumOfSitesSet(false)
{
}

/*--- SubDieManager public member functions ---------------------------------*/


int SubDieManager::atLastDiePosition()
{
    return (si == subDieList.end() );
}

int SubDieManager::atLastSubDiePosition()
{
    return (pi == subDiePosition.end() );
}

void SubDieManager::initialize()
{
    phLogSimMessage(logId, LOG_VERBOSE,"SubDieManager::initialize");    


    si=subDieList.begin();
    pi=subDiePosition.begin();

    if ( si != subDieList.end() && pi != subDiePosition.end() )
    {
        phLogSimMessage(logId, LOG_VERBOSE,"subdie list set ");    
        siteNumber=1;
    }
    else
    {
        phLogSimMessage(logId, LOG_VERBOSE,"subdie list not set cannot initialize ");    
        siteNumber=0;
    }

    return;
}

bool SubDieManager::step()
{
    phLogSimMessage(logId, LOG_VERBOSE,"SubDieManager::step");    

    if ( !siteNumber )
    {
        phLogSimMessage(logId, LOG_VERBOSE,"not yet initialized ");    
        initialize();
        phLogSimMessage(logId, LOG_VERBOSE," step onto first wafer ");    
    }
    else if ( !atLastSubDiePosition() )
    {
        phLogSimMessage(logId, LOG_VERBOSE,"not at last subdie position so step ");
        pi++;
        siteNumber++;

        if ( atLastSubDiePosition() )
        {
            phLogSimMessage(logId, LOG_VERBOSE,"now at last subdie position so increase die position ");
            si++;

            if ( !atLastDiePosition() )
            {
                phLogSimMessage(logId, LOG_VERBOSE,"now at last die position ");
                pi=subDiePosition.begin();
                siteNumber=1;
            }
        }
    }
    return !atLastDiePosition();
}


void SubDieManager::setBin(int b)
{
    (*si).setBin(siteNumber,b);
}

int SubDieManager::getBin() const
{
    return (*si).getBin(siteNumber);
}

int SubDieManager::getSiteNum()
{
    return siteNumber;
}

void SubDieManager::addSubDiePosition( const PositionXY &p )
{
    /* 
     *  first check to see if position p is not already
     *  in list. If it isn't then add to list
     */

    phLogSimMessage(logId, LOG_VERBOSE,"SubDieManager::addSubDiePosition ");

    Std::list<PositionXY>::iterator i;
    i = find(subDiePosition.begin(),subDiePosition.end(), p);

    if ( i == subDiePosition.end() )
    {
        phLogSimMessage(logId, LOG_VERBOSE,"subdie position not found create ");
        subDiePosition.push_back(p);
        ++numOfSites;
    }
    else
    {
        phLogSimMessage(logId, LOG_VERBOSE,"already have this subdie position ");
    }

    return;
}

const PositionXY &SubDieManager::getSubDiePosition() const
{
    return *pi;
}

void SubDieManager::clear()
{
    subDieList.erase( subDieList.begin(),  subDieList.end() );
}
void SubDieManager::clearSubDie()
{
    subDiePosition.erase( subDiePosition.begin(),  subDiePosition.end() );
}

void SubDieManager::addSubDie( SubDieXY d )
{
    phLogSimMessage(logId, LOG_VERBOSE,"SubDieManager::addSubDie numOfSites is %d  ",numOfSites);

    Std::list<SubDieXY>::iterator i;
    i = find(subDieList.begin(),subDieList.end(), d);

    if ( i == subDieList.end() )
    {
        phLogSimMessage(logId, LOG_VERBOSE,"subdie not found create ");
        subDieList.push_back(d);
    }
    else
    {
        phLogSimMessage(logId, LOG_VERBOSE,"already have this subdie ");
    }

    phLogSimMessage(logId, LOG_VERBOSE,"SubDieManager::addSubDie complete number of sites is %d ",numOfSites);
    return;
}

SubDieXY &SubDieManager::getSubDie()
{
    return *si;
}

const PositionXY &SubDieManager::getPos() const
{
    return *si;
}

void SubDieManager::printDieInfo()
{
    char c;
    int s;
    PositionXY p;
    int x;
    int y;
    int current_y=-1;
    int current_x=0;
    int i;
    const char *margin="                ";
    const char *microSiteSpace="   ";
    int x_min;
    int x_max=0;
    int y_min;
    int y_max=0;
    int binCode;
    bool charBinCode;

    initialize();
    p = getPos();
    x_min = p.getX();
    x_max = x_min;
    y_min = p.getY();
    y_max = y_min;
    do {
        p = getPos();
        x = p.getX();
        y = p.getY();
        x_min=Std::min(x_min,x);
        y_min=Std::min(y_min,y);
        x_max=Std::max(x_max,x);
        y_max=Std::max(y_max,y);

    } while ( step() );

    initialize();
    do {
        c = getBin();
        s = getSiteNum();
        p = getPos();
        x = p.getX();
        y = p.getY();
        binCode= getBin();

        if ( binCode < 16 )
        {
            charBinCode=true;
            c='0'+binCode;
 
        }
        else 
            charBinCode=true;

        if ( y != current_y )
        {
            Std::cout << Std::endl << Std::endl << margin;
            current_y=y;
            current_x=x;
        }

        if ( current_x != x_min && current_x != x_max )
        {
            current_x=Std::min(current_x,(x_max-current_x));
            while ( current_x > x_min )
            {
                for ( i=0 ; i < numOfSites ; i++ )
                {
                    Std::cout << " ";
                }
                Std::cout << microSiteSpace;
                --current_x;
            }
        } 

        Std::cout << c; 

        if ( s == numOfSites )
            Std::cout << "   ";

    } while ( step() );

    Std::cout << Std::endl << Std::endl << Std::endl;

    sleep(10);

}

bool SubDieManager::onWafer(const PositionXY &p)
{
    return true;
}

void SubDieManager::setBin(const PositionXY &p, int b)
{
    setBin(b);
}

void SubDieManager::setBin(const PositionXY &posDie, const PositionXY &posSubdie, int b)
{
    int loopRound = 0;
    bool found=false;

    do {
        if ( !step() )
        {
            initialize();
            ++loopRound;
        }

        if ( getPos() == posDie && getSubDiePosition() == posSubdie )
        {
            found=true;
            setBin(b);
        } 
    } while ( !found && loopRound <= 1 );

    if ( !found )
    {
        throw ProberException("SubDieManager:: setBin(die,subdie) not found ");
    }
}

const PositionXY &SubDieManager::getMinPos() const
{
    return minPos;
}

const PositionXY &SubDieManager::getMaxPos() const
{
    return maxPos;
}


void SubDieManager::selfTest() const
{
    return;
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

//
// NEW_PROBER_CLASS
//

/*--- LogObject -------------------------------------------------------------*/
LogObject::LogObject(phLogId_t l) : logId(l)
{
}


/*--- ProbeNeedle ----------------------------------------------------------*/

/*--- ProbeNeedle constructor ----------------------------------------------*/
ProbeNeedle::ProbeNeedle(phLogId_t l, const PositionXY& prim) : LogObject(l)
{
    primary=prim;
    relative.push_back(PositionXY(0,0));
}

PositionXY ProbeNeedle::getPosition(int probeNum) const
{
    if ( probeNum < 0 || probeNum > getNumberOfNeedles() )
    {
	phLogSimMessage(logId, PHLOG_TYPE_ERROR,
	    "probe needle out of range %d (0 to %d)", 
	    probeNum,getNumberOfNeedles());
        throw RangeException("probe needle position out of range: see error log");
    }

    Std::vector<PositionXY>::const_iterator i = relative.begin() + probeNum;

    return primary + *i;
}

void ProbeNeedle::addProbePosition(const PositionXY &rel)
{
    relative.push_back(rel);
}

int ProbeNeedle::getNumberOfNeedles() const
{
    return relative.end() - relative.begin();
}

void ProbeNeedle::moveProber(const PositionXY &pos)
{
    primary=pos;
}



/*--- ProberSetup ----------------------------------------------------------*/


/*--- ProberSetup constructor ----------------------------------------------*/

ProberSetup::ProberSetup()
{
    binningManager          =  0;
    probeNeedle             =  0;
    steppingMode            = Prober::simple;
    proberMode              = Prober::prober;
    autoSetupDelayMsec      = 0.0;
    handlingDelayMsec       = 0.0;
    numOfWafersToTest       = 3;
}


/*--- Prober ---------------------------------------------------------------*/

/*--- Prober constructor ---------------------------------------------------*/

Prober::Prober(phLogId_t l, const ProberSetup& setup) : LogObject(l)
{
    binningManager        = setup.binningManager;
    probeNeedle           = setup.probeNeedle;
    steppingMode          = setup.steppingMode;
    proberMode            = setup.proberMode;
    autoSetupDelayMsec    = setup.autoSetupDelayMsec;
    handlingDelayMsec     = setup.handlingDelayMsec;
    status                = stopped;
    autoSetupInit         = false;
    running               = true;
    waferTestCount        = 0;
    numOfWafersToTest     = 3;

    proberTimer=new Timer(l);
    selfTest();
}

/*--- Prober public member functions  --------------------------------------*/

//
// NEW_PROBER_CLASS
//
// provide low level prober functions for Electoglas prober services
//

void Prober::loadWafer() 
{

    switch ( status )
    {
      case stopped:
        if ( waferTestCount < numOfWafersToTest )
        {
            status=loadingWafer;
            ++waferTestCount;
        }
        break;
      case probing:
        if ( waferTestCount < numOfWafersToTest )
        {
            printWaferInfo();
            status=loadingWafer;
            ++waferTestCount;
        }
        else if ( waferTestCount == numOfWafersToTest )
        {
            printWaferInfo();
            status=stopped;
        }
        break;
      case loadingWafer:
        // retest ?
        printWaferInfo();
        break;
    }

    if ( status==loadingWafer )
    {
        proberTimer->start();

        if ( proberMode==prober || proberMode==learnlist ) 
        {
            binningManager->selfTest();

            binningManager->initialize();
    
            const PositionXY &pos=binningManager->getPos();

            probeNeedle->moveProber(pos);
        }
    }

}

PositionXY Prober::getProbePosition() const
{
    return probeNeedle->getPosition();
}

bool Prober::stepProbe()
{
    bool stepSuccess = false;
    int bin;
    PositionXY pos;

    phLogSimMessage(logId, LOG_VERBOSE,"Prober::stepProbe() ");

    switch ( steppingMode )
    {
      case Prober::simple:
        stepSuccess=binningManager->step();
        if ( stepSuccess )
        {
            pos=binningManager->getPos();
        }
        break;
      case Prober::efficient:
        do {
            stepSuccess=binningManager->step();

            if ( stepSuccess )
            {
                bin = binningManager->getBin();
            }
        } while ( stepSuccess && bin!=DieXY::NOT_BINNED );
        if ( stepSuccess )
        { 
            pos=binningManager->getPos();
        }
        break;
      case Prober::compatible:
        pos=getProbePosition();

        int x = pos.getX();
        int y = pos.getY();

        pos.setX(++x);

        stepSuccess = binningManager->onWafer(pos);

        if ( stepSuccess == false )
        {
            PositionXY const &minPos = binningManager->getMinPos();

            x = minPos.getX() -1;

            pos.setX(x);
            pos.setY(++y);
            probeNeedle->moveProber(pos);

            for ( int i = 0 ; i < probeNeedle->getNumberOfNeedles() && stepSuccess == false ; ++i )
            {
                PositionXY probePos = probeNeedle->getPosition(i);
                stepSuccess = binningManager->onWafer(probePos);
            }

            if ( stepSuccess == false )
            {
                pos.setX(++x);
                probeNeedle->moveProber(pos);

                for ( int i = 0 ; i < probeNeedle->getNumberOfNeedles() && stepSuccess == false ; ++i )
                {
                    PositionXY probePos = probeNeedle->getPosition(i);
                    stepSuccess = binningManager->onWafer(probePos);
                }
            }
        }
        break;
    }

    if ( stepSuccess )
    {
        PositionXY oldPos = probeNeedle->getPosition();
#if __GNUC__ >= 3
        using namespace std::rel_ops;
#endif
        if ( oldPos != pos )
        {
            probeNeedle->moveProber(pos);
            phLogSimMessage(logId, LOG_DEBUG,"auto step to die [%d,%d]",pos.getX(),pos.getY());
        }
    }
    else
    {
        phLogSimMessage(logId, LOG_DEBUG, "auto step past last die");
        probeNeedle->moveProber(HOME_POSITION);
    }

    return stepSuccess;
}

bool Prober::proberOnWafer(int probeNum)
{
    PositionXY pos = probeNeedle->getPosition(probeNum);

    bool onWafer = binningManager->onWafer(pos);

    if ( onWafer )
    {
	phLogSimMessage(logId, LOG_DEBUG, 
                        "position [%d,%d] is on wafer", 
                        pos.getX(), 
                        pos.getY());
    }
    else
    {
	phLogSimMessage(logId, LOG_DEBUG, 
	                "position [%d,%d] is outside of wafer",
                        pos.getX(), 
                        pos.getY());
    }

    return onWafer;
}

void Prober::printWaferInfo()
{
    binningManager->printDieInfo();
}

int Prober::getNumberOfProbes() const
{
    return probeNeedle->getNumberOfNeedles();
}


void Prober::moveProber(const PositionXY &p)
{
    probeNeedle->moveProber(p);
}

void Prober::start()
{
    running=true;
}

void Prober::stop()
{
    running=false;
}

bool Prober::sendTestStartSignal()
{
    bool sendTestStart=false;

    if ( !autoSetupInit && autoSetupDelayMsec > 0.0 )
    {
        phLogSimMessage(logId, LOG_VERBOSE,
                        "Handler::sendTestStartSignal automatic start time so far %lf wait %lf ",
                        proberTimer->getElapsedMsecTime(),
                        autoSetupDelayMsec);
        if ( proberTimer->getElapsedMsecTime() >= autoSetupDelayMsec )
        {
            loadWafer();
            autoSetupInit=true;
        }
    }
    else
    {
        phLogSimMessage(logId, LOG_VERBOSE,
                        "Handler::sendTestStartSignal elapsed time so far %lf wait %lf ",
                        proberTimer->getElapsedMsecTime(),
                        handlingDelayMsec);
    }

    switch ( status )
    {
        case stopped:
            sendTestStart=false;
            break;
        case loadingWafer:
            if ( proberTimer->getElapsedMsecTime() >= handlingDelayMsec && running )
            {
                switch ( proberMode ) 
                {
                  case learnlist:
                  case prober:
                    sendTestStart=true;
                    break;
                  case smartest:
                    sendTestStart=false;
                    break;
                }
                status=probing;
            }
            else
            {
                sendTestStart=false;
            }
            break;
        case probing:
            if ( proberMode==smartest && waferTestCount == 1 )
            {
                printWaferInfo();
                status=stopped;
            }
            sendTestStart=false;
            break;
    }

    phLogSimMessage(logId, LOG_VERBOSE, "Handler::sendTestStartSignal is %s ",
                    sendTestStart ? "true" : "false");

    return sendTestStart;
}


/*--- Prober private member functions  -------------------------------------*/
void Prober::selfTest() const
{
    if ( !binningManager )
    {
        throw ProberException("Prober::self test binningManager not setup");
    }
    if ( !probeNeedle )
    {
        throw ProberException("Prober::self test probeNeedle not setup");
    }
}


/*--- DieProberSetup -------------------------------------------------------*/


/*--- DieProberSetup constructor --------------------------------------------*/

DieProberSetup::DieProberSetup() : ProberSetup()
{
}


/*--- DieProber ------------------------------------------------------------*/

/*--- DieProber constructor ------------------------------------------------*/

DieProber::DieProber(phLogId_t l, const DieProberSetup& setup) : Prober(l,setup)
{
    dieManager = dynamic_cast<DieManager*>(setup.binningManager);

    if ( !dieManager )
    {
        throw ProberException("Prober::self test DieManager not setup");
    }
}

/*--- DieProber public member functions  -----------------------------------*/


void DieProber::binDie(int bin, int probeNum)
{
    phLogSimMessage(logId, LOG_VERBOSE,"DieProber::binDie %d %d ",bin,probeNum);

    PositionXY pos = probeNeedle->getPosition(probeNum);

    bool onWafer = binningManager->onWafer(pos);

    if ( onWafer )
    {
        phLogSimMessage(logId, LOG_DEBUG,
                        "die [%d,%d] binned to bin %d",
                        pos.getX(),
                        pos.getY(),
                        bin);
        binningManager->setBin(pos,bin);
    }
    else
    {
	phLogSimMessage(logId, LOG_DEBUG, 
	                "die [%d,%d] outside wafer, not binned", 
                        pos.getX(),
                        pos.getY());
    }
}

void DieProber::clearDieList()
{
    binningManager->clear();
}

void DieProber::addDie(DieXY *p)
{
    dieManager->addDie(p);
}








/*--- SubDieProberSetup ----------------------------------------------------*/


/*--- SubDieProberSetup constructor -----------------------------------------*/

SubDieProberSetup::SubDieProberSetup() : ProberSetup()
{
}


/*--- SubDieProber ---------------------------------------------------------*/

/*--- SubDieProber constructor ---------------------------------------------*/

SubDieProber::SubDieProber(phLogId_t l, const SubDieProberSetup& setup) : Prober(l,setup)
{
    subDieManager = dynamic_cast<SubDieManager*>(setup.binningManager);

    subDiePosition = HOME_POSITION;

    if ( !subDieManager )
    {
        throw ProberException("Prober::self test subDieManager not setup");
    }
}

/*--- SubDieProber public member functions  --------------------------------*/

void SubDieProber::binDie(int bin, int probeNum)
{
    switch ( proberMode ) 
    {
      case learnlist:
      case prober:
        subDieManager->setBin(bin);
        break;
      case smartest:
        subDieManager->setBin(probeNeedle->getPosition(probeNum),subDiePosition,bin);
        break;
    }
}

int SubDieProber::getSiteNumber()
{
    return subDieManager->getSiteNum();
}

void SubDieProber::clearSubDieList()
{
    subDieManager->clear();
}

void SubDieProber::clearSubDiePositionList()
{
    subDieManager->clearSubDie();
}

void SubDieProber::addSubDiePosition( const PositionXY &p )
{
    subDieManager->addSubDiePosition(p);
}

void SubDieProber::addSubDie( SubDieXY d )
{
    subDieManager->addSubDie(d);
}

void SubDieProber::moveSubDieProber(const PositionXY &p)
{
    subDiePosition = p;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/




struct die
{
    int              bin;
    int              ink;
    int              flags;
};

struct wafer
{
    struct die       d[WAF_MAXDIM][WAF_MAXDIM];
    enum waf_stat    status;
};

struct slot
{
    int              loaded;
    struct wafer     wafer;
};

struct cassette
{
    struct slot      s[WAF_SLOTS];
};

struct all_t
{
    /* ids */
    phLogId_t        logId;
    phComId_t        comId;

    /* setup */
    int              debugLevel;
    char             *hpibDevice;
    int              timeout;
    char             *modelname;
    char             *modename;
    enum model_t     model;
    enum proberMode_t proberMode;
    enum family_t    family;
    int              reverseBinning;
    int              asciiOCommand;
    int              sendFirstWaferSRQ;
    int              dontSendPauseSRQ;
    int              retestBinNumber;

    /* init */
    int              done;
    int              wafercount;
    int              sendLotEnd;
    int              abortProbing;
    int              dip[DIP_POSITIONS][DIP_GROUPS];
    int              master;

    /* positioning */
    int              xmin;
    int              ymin;
    int              xmax;
    int              ymax;
    int              xref;
    int              yref;
    int              xstep;
    int              ystep;

    int              xsubcurrent;
    int              ysubcurrent;

    int              xcurrent;
    int              ycurrent;
    int              chuckUp;

    /* cassettes */
    struct cassette  cass[CASS_COUNT];

    /* chuck */
    struct slot      chuck;

    /* probe */
    int              probes;
    int              pxoff[MAX_PROBES];
    int              pyoff[MAX_PROBES];

    int              maxxoff;
    int              minxoff;
    int              maxyoff;
    int              minyoff;

    /* prober paused */
    bool             pause;


    /* runtime */
    const char       *message;
    int              length;
    phComGpibEvent_t *comEvent;

    int              interactiveDelay;
    int              interactiveDelayCount;

    bool             enableMicroProbing;
//    int              sendTestSignal;

//    BinningManager   *binningManager;
//    DieManager       *dieManager;
//    SubDieManager    *subdieManager;
    int              subdie;

//    bool             autoSendTestStart;
    bool             corruptTestStart;

    bool addDiePositionToTS;

    double           autoSetupDelay;
    double           handlingDelay;

#ifdef NEW_PROBER_CLASS
    Prober           *prober;
    DieProber        *dieProber;
    SubDieProber     *subdieProber;
#endif

    Timer            *sendSrqDelayTimer;
    int              sendSrqDelay;
    int              numOfWafersToTest;
};

struct all_t allg;

/*--- global variables ------------------------------------------------------*/


/*--- functions -------------------------------------------------------------*/
// static int stepDie(struct all_t *all);

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


static void doDelay(int seconds, struct all_t *all)
{
    char delay[64];
    char typed[80];

    while (seconds > 0)
    {
	sprintf(delay, "%d", seconds);
	seconds--;
	sleep(1);
    }

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

static void sendSRQ(struct all_t *all, unsigned char srq)
{
    char message[1024];
    phComGpibEvent_t sendEvent;
    phComError_t comError;
    int sent = 0;


    /*                                                          */
    /* added to simulator to detect test case failures where    */
    /* whole message is not sent to driver.                     */
    /* delay increased to SRQ_AND_MSG_DELAY seconds to test     */ 
    /* driver recovery                                          */
    /*                                                          */
    if ( all->sendSrqDelay )
    {
        phLogSimMessage(all->logId, LOG_VERBOSE, "delay by %d msec before sending SRQ ",
                        all->sendSrqDelay);    

        all->sendSrqDelayTimer->wait(all->sendSrqDelay);
    }

    sprintf(message, "0x%02x", (int) srq);

    sendEvent.d.srq_byte = srq;
    sendEvent.type = PHCOM_GPIB_ET_SRQ;

    phLogSimMessage(all->logId, LOG_VERBOSE, 
	"trying to send SRQ \"%s\"", message);

    do
    {
	comError = phComGpibSendEventVXI11(all->comId, &sendEvent, 
	    1000L * all->timeout);
	switch (checkComError(comError, all))
	{
	  case 0:
	    /* OK */
	    sent = 1;
	    break;
	  case 1:
            sleep(1);
	  case 2:
	  default:
	    break;
	}
    } while (!sent);

    phLogSimMessage(all->logId, LOG_VERBOSE, 
	"sent SRQ \"%s\"", message);    

}

static void sendMessage(struct all_t *all, const char *m)
{
    char message[1024];
    int sent = 0;
    phComError_t comError;
    strcpy(message, m);
    strcat(message, EOC);

    /*                                                          */
    /* delay added to test driver recovery from failed          */
    /* transaction.                                             */
    /*                                                          */
//    if ( all->sendSrqDelay )
//    {
//        phLogSimMessage(all->logId, LOG_VERBOSE, "delay by %d seconds before sending message ",
//                        SRQ_AND_MSG_DELAY);    
//        doDelay(SRQ_AND_MSG_DELAY, all);
//    }

    phLogSimMessage(all->logId, LOG_DEBUG, 
	"trying to send message \"%s\"", m);

    do 
    {
	comError = phComGpibSendVXI11(all->comId, message, strlen(message), 
	    1000L * all->timeout);
	switch (checkComError(comError, all))
	{
	  case 0:
	    /* OK */
	    sent = 1;
	    break;
	  case 1:
	    break;
	  case 2:
	  default:
	    break;
	}
    } while (!sent);
    
    phLogSimMessage(all->logId, LOG_VERBOSE, 
	"message \"%s\" sent", m);
}

/*---------------------------------------------------------------------------*/

static int chuckUp(struct all_t *all, int up)
{
    if (up)
    {
	all->chuckUp = 1;
	phLogSimMessage(all->logId, LOG_DEBUG, "chuck up");
    }
    else
    {
	all->chuckUp = 0;
	phLogSimMessage(all->logId, LOG_DEBUG, "chuck down");
    }

    return 1;
}

static int realignWafer(struct all_t *all)
{
//    int p;
//    int probed;

    if (all->chuck.loaded)
    {
        if ( all->subdie == 1 )
        {
            if ( all->proberMode == mProber || all->proberMode == mLearnList )
            {
	        phLogSimMessage(all->logId, LOG_DEBUG, "subdie initialized");
            }
            return 1;
        }

        if ( all->proberMode == mProber || all->proberMode == mSmarTest )
        {
	    phLogSimMessage(all->logId, LOG_DEBUG, "wafer initialized");
	    return 1;
        }
        else if ( all->proberMode == mLearnList )
        { 
	    phLogSimMessage(all->logId, LOG_DEBUG, "wafer initialized");
	    return 1;
        }
    }
    else
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "wafer not initialized, chuck empty");
	return 0;	
    }
    return 0;
}

static int loadWafer(struct all_t *all)
{
    if (all->wafercount > 0)
    {
        all->wafercount--;
	all->chuck.loaded = 1;    
	all->chuck.wafer.status = WAF_INTEST;

	phLogSimMessage(all->logId, LOG_DEBUG, "wafer loaded");
	return 1;
    }
    else
    {
	phLogSimMessage(all->logId, LOG_VERBOSE, "no more wafers to load");
	return 0;	
    }
}

static void cleanProbes(struct all_t *all)
{
    phLogSimMessage(all->logId, LOG_DEBUG, "cleaning probe tips");
}

static void setFamilyType(struct all_t *all)
{
    switch (all->model)
    {
      case EG2001:
      case EG2010:
      case EG3001:
      case EG4085:
        all->family=PVS;
        break;
      case EG4060: 
      case EG4080: 
      case EG4090:
      case EG4200:
      case EG5300:
        all->family=COMMANDER;
        break;
    }
    return;
}

/*
 *   Request Prober ID and S/W Revision
 *   Format: ID
 *   Returns a text string identifying the prober for both
 *   standard and special applications
 */ 
static void serviceID(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"ID\"");

#ifdef RETURN_ID
    sendSRQ(all, 0x40);
    switch (all->model)
    {
      case EG2001:
        sendMessage(all, "2001.EGCMD-000-6.0.00-0031a.HU99020029");
        break;
      case EG2010:
        sendMessage(all, "2010.EGCMD-000-6.0.00-0031a.HU99020029");
        break;
      case EG3001: 
        sendMessage(all, "3001.EGCMD-000-6.0.00-0031a.HU99020029");
        break;
      case EG4085: 
        sendMessage(all, "4085.EGCMD-000-6.0.00-0031a.HU99020029");
        break;
      case EG4060: 
        sendMessage(all, "4060.EGCMD-000-6.0.00-0031a.HU99020029");
        break;
      case EG4080: 
        sendMessage(all, "4080.EGCMD-000-6.0.00-0031a.HU99020029");
        break;
      case EG4090: 
        sendMessage(all, "4090.EGCMD-000-6.0.00-0031a.HU99020029");
        break;
      case EG4200: 
        sendMessage(all, "4/200.EGCMD-000-Q.4.00-0003a.H496040831");
        break;
      case EG5300: 
        sendMessage(all, "5/300.EGCMD-000-Q.4.00-0003a.H496040831");
        break;
    }
#endif
}

/*
 *   Request Current XY Position
 *   Format: ?P
 *   Returns the position of the forcer in die coordinates with respect
 *   to first die.
 *   Format returned: XnYn
 */ 
static void serviceQP(struct all_t *all, char *m)
{
    char message[64];
//    int x, y;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?P\"");
    doDelay(0, all);

#ifdef NEW_PROBER_CLASS

    PositionXY pos= all->prober->getProbePosition();

    sprintf(message, "current die position [%d,%d]", pos.getX(), pos.getY() );
    phLogSimMessage(all->logId, LOG_DEBUG, "%s", message);
    sprintf(message, "X%03dY%03d", pos.getX(), pos.getY() );
    sendSRQ(all, 0x40);
    sendMessage(all, message);

#endif


}



/*
 *   Request Wafer ID
 *   Format: ?Wn OR ?W
 *   
 *   ?W0 returns the wafer ID string
 */ 
static void serviceQW(struct all_t *all, char *m)
{
    char message[64];

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?W\"");
    doDelay(0, all);

    sprintf(message, "C90101F6-%02d", WAF_COUNT - all->wafercount );
    sendSRQ(all, 0x40);
    sendMessage(all, message);
}


/*
 *   Request State Variables 
 *   Format: ?R
 */ 
static void serviceQR(struct all_t *all, char *m)
{
//    char message[64];

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?R\"");
    doDelay(0, all);

    sendSRQ(all, 0x40);
    if ( all->pause == true )
    {
        sendMessage(all, "C-4S7F1R1W1P1E1A1");
    }
    else
    {
        sendMessage(all, "C4S7F1R1W1P1E1A1");
    }
}

/*
 * Request Wafer Size
 *    -> "?SP4"
 * 
 */
static void serviceQSP4(struct all_t *all, char *m)
{
  char message[64];

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?SP4\"");
  doDelay(0, all);

  sprintf(message, "SP4D120");  /* the wafer size = 120 mm */
  sendSRQ(all, 0x40);
  sendMessage(all, message);
}

/*
 * Request Die height and Die width
 *    -> "?SP29"
 * 
 */
static void serviceQSP29(struct all_t *all, char *m)
{
  char message[64];

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?SP29\"");
  doDelay(0, all);

  sprintf(message, "SP29X345.25Y301.75"); /* X=345.25, Y=301.75 */
  sendSRQ(all, 0x40);
  sendMessage(all, message);
}


/*
 * Request Wafer Units
 *    -> "?SM1"
 * 
 */
static void serviceQSM1(struct all_t *all, char *m)
{
  char message[64];

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?SM1\"");
  doDelay(0, all);

  /* U1 means the unit is "um", U0 means "mil" ( we do not use "0.1mil" because of using "SP29" */
  sprintf(message, "SM1U0"); 
  sendSRQ(all, 0x40);
  sendMessage(all, message);
}


/*
 * Request Wafer Flat
 *    -> "?SM3"
 * 
 */
static void serviceQSM3(struct all_t *all, char *m)
{
  char message[64];

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?SM3\"");
  doDelay(0, all);

  sprintf(message, "SM3F270");  /* the flat is at 270 deg */
  sendSRQ(all, 0x40);
  sendMessage(all, message);
}


/*
 * Request Center X and Center Y
 *    -> "?I"
 * 
 */
static void serviceQI(struct all_t *all, char *m)
{
  char message[128];

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?I\"");
  doDelay(0, all);

  sprintf(message, "IX68161Y15547X77432Y38683D120");
  sendSRQ(all, 0x40);
  sendMessage(all, message);
}


/*
 * Request Positive X and Positive Y
 *    -> "?SM11"
 * 
 */
static void serviceQSM11(struct all_t *all, char *m)
{
  char message[64];

  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?SM11\"");
  doDelay(0, all);

  sprintf(message, "SM11Q2");  /* 2 means Quadrant I: Left->Right, Bottom->Up*/
  sendSRQ(all, 0x40);
  sendMessage(all, message);
}


/*
 *   Test Complete and Bin Device 
 *   Format: TCn
 *   Signal completion of testing on the current die and actuates one
 *   or more inkers
 *   The prober will also step to the next die in the probe pattern
 *   and issue a TS (Test Start) message and pulse
 */ 
static void serviceTC(struct all_t *all, char *m)
{
    int bin;
//    int p;
//    int probe;
    static int testStartCount=0;
    char message[64];

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"TC\"");
    doDelay(0, all);
#ifdef kawtest
    // kaw - testing for CR13399 - 29 Nov 2004
    // testing only: every seventh "TC", go into, then out of pause
    ++TC_hack_count;
    if (TC_hack_count == 7) {
        TC_hack_count = 0;
        sendSRQ(all, 0x40);
        sendSRQ(all, 0x40);
        sendMessage(all, "PA");
        doDelay(2, all);
        sendSRQ(all, 0x40);
        sendSRQ(all, 0x40);
        sendMessage(all, "CO");
        doDelay(1, all);
        sendSRQ(all, 0x40);

        if ( all->prober->stepProbe() )
        {
            testStartCount++;

            if ( all->corruptTestStart && testStartCount==CORRUPT_TEST_START )
            {
                testStartCount=0;
                sendMessage(all, "T");
            }
            else
            {
                if ( all->addDiePositionToTS )
                {
                    PositionXY pos= all->prober->getProbePosition();

                    sprintf(message, "TAX%dY%d", pos.getX(), pos.getY() );
                }
                else
                {
                    strcpy(message,"TA");
                }
                sendMessage(all, message);
            }
        }
    }
    else {
#endif
#ifdef NEW_PROBER_CLASS
    if ( all->prober->getNumberOfProbes() > 1 )
    {
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
                        "prober setup with %d probes \n"
                        "but \"%s\" is a single probe command",
                        all->prober->getNumberOfProbes(),
                        m);
        throw ProberException("probe set up for multiple probes but \"TC\" command received");
    }

    if ( sscanf(m, "TC%d", &bin) == 1 )
    {
        all->prober->binDie(bin);
    }
    else
    {
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
                        "unable to interpret \"%s\" expected format TCn ", m);
        throw ProberException("unable to interpret TCn command");
    }

    sendSRQ(all, 0x40);

    if ( bin == all->retestBinNumber )
    {
        sendMessage(all, "TR");
    }
    else
    {
        if ( all->prober->stepProbe() )
        {
            testStartCount++;

            if ( all->corruptTestStart && testStartCount==CORRUPT_TEST_START )
            {
                testStartCount=0;
                sendMessage(all, "T");
            }
            else
            {
                if ( all->addDiePositionToTS )
                {
                    PositionXY pos= all->prober->getProbePosition();

                    sprintf(message, "TSX%dY%d", pos.getX(), pos.getY() );
                }
                else
                {
                    strcpy(message,"TS");
                }
                sendMessage(all, message);
            }
        }
        else
        {
            sendMessage(all, "PC");
            all->prober->loadWafer();

//          if ( all->autoSendTestStart )
//          {
//              all->sendTestSignal=1;
//          }
        }
    }
#endif
#ifdef kawtest
    }
#endif

}


/*
 *   Multiple Die Test Complete
 *   Format ETn,n,n,n
 *   Signals the completion of testing on the current multiple die.
 */ 
static void serviceET(struct all_t *all, char *m)
{
    char message[64];
    int bin;
    int probe;
    int errflg=0; 
    char *pos=m;
    bool needRetest=false;
    int onWaferCode_forRetest=0;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"ET\"");
    doDelay(0, all);
    for (probe=0; probe<all->probes; probe++)
    {
        if ( probe == 0 )
        {
            if ( sscanf(pos, "ET%d", &bin) != 1 )
            {
	        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	            "unable to interpret \"%s\" expected format ETn ", pos);
                errflg++;
            }
        }
        else
        {
	    pos = strchr(pos, ',');
            if ( sscanf(pos, ",%d",&bin) != 1 ) 
            {
	        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	            "unable to interpret \"%s\" expected format ,n ", pos);
                errflg++;
            }
        }
        all->prober->binDie(bin, probe);
        if ( bin == all->retestBinNumber )
        {
            if ( all->prober->proberOnWafer(probe) )
            {
                onWaferCode_forRetest+=1<<probe;
            }
            needRetest=true;
        }
    }

    if ( errflg )
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to interpret \"%s\" expected format ETn,n ", m);
        strcpy(message, "MF");
    }
//    else if (stepDie(all))
    else
    {
        if ( needRetest )
        {
            sprintf(message, "TR,%d", onWaferCode_forRetest);
        }
        else
        {
            if ( all->prober->stepProbe() )
            {
                int onWaferCode=0;
                for (probe=0; probe < all->probes; probe++)
                {
                    if ( all->prober->proberOnWafer(probe) )
                    {
                        onWaferCode+=1<<probe;
                    }
                }

                if ( all->addDiePositionToTS )
                {
                    PositionXY pos= all->prober->getProbePosition();
                    sprintf(message, "TSX%dY%d,%d", pos.getX(), pos.getY(), onWaferCode);
                }
                else
                {
                    sprintf(message, "TS,%d", onWaferCode);
                }

            }
            else
            {
                strcpy(message, "PC");
                all->prober->loadWafer();

//             if ( all->autoSendTestStart )
//             {
//                 all->sendTestSignal=1;
//             }
            }
        }
    }
    sendSRQ(all, 0x40);
    sendMessage(all, message);
    return;
}


/*
 *   Micro Test Complete
 *   Format: FCn
 *   Microtest has been completed.
 */ 
static void serviceFC(struct all_t *all, char *m)
{
    char message[64];
    char c;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"FC\"");
    doDelay(0, all);

    if ( sscanf(m, "FC%c", &c) != 1 )
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to interpret \"%s\" expected format FCn ", m);
        sendSRQ(all, 0x40);
        sendMessage(all, "MF");
    }
    else
    {
        all->prober->binDie(c);

        if ( all->prober->stepProbe() )
        {
            if ( all->addDiePositionToTS )
            {
                PositionXY pos= all->prober->getProbePosition();
                sprintf(message, "TSX%dY%dS%d", pos.getX(), pos.getY(), all->subdieProber->getSiteNumber() );
            }
            else
            {
                sprintf(message, "TSS%d",  all->subdieProber->getSiteNumber() );
            }
        }
        else
        {
            all->prober->loadWafer();
            strcpy(message, "PC");
        }

        sendSRQ(all, 0x40);
        sendMessage(all, message);
    }
    return;
}


/*  
 *  Clear Learn List 
 *  command format is RS
 */
static void serviceRS(struct all_t *all, char *m)
{
    sendSRQ(all, 0x40);
    if ( all->proberMode == mLearnList )
    {
        if ( all->subdie == 1 )
        {
//            all->subdieManager->clear();

            all->subdieProber->clearSubDieList();
        }
        else
        {
//            all->dieManager->clear();

            all->dieProber->clearDieList();
        }
        sendMessage(all, "MC");
    }
    else
    {
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
            "clear learnt list command (%s) received but not in LearnList mode ", m);
        sendMessage(all, "MF");
    }

    return;
}

/*
 *  Clear Learn List
 *  command format is $LSCL
 */
static void serviceLSCL(struct all_t *all, char *m)
{
    serviceRS(all, m);
    return;
}

/*
 *  Add die to learn list  PVS + COMMANDER
 *  format is XnYn
 */
static void addDieToLearnList(struct all_t *all, char *m)
{
    int x;
    int y;

    sendSRQ(all, 0x40);
    if ( sscanf(m, "X%dY%d", &x, &y) != 2 )
    {
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
            "unable to interpret \"%s\" expected format XdYd ", m);
        sendMessage(all, "MF");
    }
    else
    {
        if ( all->subdie == 1 )
        {
//            all->subdieManager->addSubDie( SubDieXY(x,y) );

            all->subdieProber->addSubDie( SubDieXY(x,y) );
        }
        else
        {
//            all->dieManager->addDie( new DieXY(x,y) );

            all->dieProber->addDie( new DieXY(x,y) );
        }
        sendMessage(all, "MC");
    }
    return;
}

/*
 *  Add die to learn list  PVS
 *  command format is ADXnYn
 */
static void serviceAD(struct all_t *all, char *m)
{
    char *coorStr;

    coorStr = strrchr(m,'X');
    if(coorStr != NULL)
      addDieToLearnList(all, coorStr);

    return;
}

/*  
 *  Add die to learn list COMMANDER
 *  command format is $LSADsXdYd
 *  eg.  $LSAD"rpf"X123Y010
 */

static void serviceLSAD(struct all_t *all, char *m)
{
    char *coorStr;

    coorStr = strrchr(m,'X');
    if(coorStr != NULL)
      addDieToLearnList(all, coorStr);

    return;
}

/*  
 *  Clear Micro List
 *  command format is RF
 */
static void serviceRF(struct all_t *all, char *m)
{
    sendSRQ(all, 0x40);
    if ( all->proberMode == mLearnList && all->subdie == 1 )
    { 
//        all->subdieManager->clearSubDie();

        all->subdieProber->clearSubDiePositionList();

        sendMessage(all, "MC");
    }
    else
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "Clear Micro List (%s) command received but not in LearnList Subdie mode ", m);
        sendMessage(all, "MF");
    }
    return;
}



/*  
 *  Add Site to Micro List
 *  command format is FAXxYySn
 *  xy = +/- 32767
 *   n = 1-126 Microsite number
 *  Units: 0.1/1 micron
 * 
 *  Adds a die site to the micro list.
 */
static void serviceFAX(struct all_t *all, char *m)
{
    int x;
    int y;
    int site;

    sendSRQ(all, 0x40);
    if ( sscanf(m, "FAX%dY%dS%d", &x, &y, &site ) != 3 )
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to interpret \"%s\" expected format FAXxYySn ", m);
        sendMessage(all, "MF");
    }
    else
    {
//        all->subdieManager->addSubDiePosition( PositionXY(x,y) );

        all->subdieProber->addSubDiePosition( PositionXY(x,y) );

        sendMessage(all, "MC");
    }
    return;
}





/*  
 *  SM4 Select Probe Mode
 *  command format is SM4Pn
 *  n = probe pattern
 *  0 - Off          2 - Matrix     4 - Learn       8 - Partial
 *  1 - Edge Sense   3 - Circular   5 - Row/Column  10 - External
 *  eg.  $LSAD"rpf"X123Y010
 */

static void serviceSM4P(struct all_t *all, char *m)
{
    int pm;

    doDelay(0, all);

    sendSRQ(all, 0x40);
    if ( sscanf(m, "SM4P%d", &pm ) != 1 )
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to interpret \"%s\" expected format SM4Pn ", m);
        sendMessage(all, "MF");
    }
    else
    {
        if ( pm == 4 &&  all->proberMode == mLearnList )
        {
//            all->prober->loadWafer();

//            if ( all->autoSendTestStart )
//            {
//                all->prober->loadWafer();
//                all->sendTestSignal=1;
//            }

        }
        sendMessage(all, "MC");
    }

    return;
}


/*  
 *  SM35 Enable MicroProbing
 *  command format is SM35Bn
 *  n = "0" disable
 *  n = "1" enable
 *
 */
static void serviceSM35B(struct all_t *all, char *m)
{
    int enable;

    doDelay(0, all);

    sendSRQ(all, 0x40);
    if ( sscanf(m, "SM35B%d", &enable ) != 1 )
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to interpret \"%s\" expected format SM35Bn ", m);
        sendMessage(all, "MF");
    }
    else
    {
        all->enableMicroProbing=(bool)enable;
        sendMessage(all, "MC");
    }

    return;
}


static void serviceSM15M(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "message complete for \"%s\"",m);
    sendSRQ(all, 0x40);
    sendMessage(all, "MC");

    if ( all->proberMode == mProber )
    {
//        all->prober->loadWafer();
//
//        if ( all->autoSendTestStart )
//        {
//            all->sendTestSignal=1;
//        }
    }
}

static void serviceHW(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "message complete for \"%s\"",m);
    doDelay(0, all);

    chuckUp(&allg, 0);
    if ( all->chuck.loaded )
    {
        all->chuck.loaded = 0;    
    }
    all->prober->loadWafer();
    if ( loadWafer(&allg) )
    {
        realignWafer(&allg);
        sendSRQ(all, 0x40);
        sendMessage(all, "MC");
    }
    else
    {
        sendSRQ(all, 0x40);
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to handle wafer (%s): no wafers to handle ", m);
        sendMessage(all, "MF");
    }
}

static void serviceZD(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"",m);
    doDelay(0, all);

    chuckUp(all, 0);

    sendSRQ(all, 0x40);
    sendMessage(all, "MC");
}


static void serviceZU(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"",m);
    doDelay(0, all);

    chuckUp(all, 1);

    sendSRQ(all, 0x40);
    sendMessage(all, "MC");
}


/* 
 *
 * Move In Absolute Die Steps
 * Format MOXnYn
 * n = absolute die coordinates values in units of die size
 *
 */
static void serviceMOX(struct all_t *all, char *m)
{
    int x;
    int y;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"",m);
    doDelay(0, all);

    if ( sscanf(m, "MOX%dY%d", &x, &y) != 2 )
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to interpret \"%s\" expected format MOXdYd ", m);
        sendMessage(all, "MF");
    }
    else
    {
        if ( all->subdie == 1 )
        {
//            all->subdieManager->addSubDie( SubDieXY(x,y) );

            all->subdieProber->addSubDie( SubDieXY(x,y) );
        }
        else
        {
            all->xcurrent=x;
            all->ycurrent=y;
      
            all->dieProber->addDie(new DieXY(x,y));
        }

        all->prober->moveProber(PositionXY(x,y));

        sendSRQ(all, 0x40);
        sendMessage(all, "MC");
    }
    return;
}


/* 
 * Move Micro Coordinates
 * Format FMXxYy
 * Units 0.1 mil/1 micron 
 * Specifies an absolute movement to a specific micro coordinate 
 * position.
 *
 */
static void serviceFMX(struct all_t *all, char *m)
{
    int micro_x;
    int micro_y;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"",m);
    doDelay(0, all);

    if ( sscanf(m, "FMX%dY%d", &micro_x, &micro_y) != 2 )
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to interpret \"%s\" expected format FMXxYy ", m);
        sendMessage(all, "MF");
    }
    else
    {
        all->subdieProber->addSubDiePosition( PositionXY(  micro_x,  micro_y ) );
        all->subdieProber->moveSubDieProber( PositionXY(  micro_x,  micro_y ) );

//        if ( !(all->subdieManager->step()) )
//        {
//            all->subdieManager->initialize();
//        }
//        all->subdieProber->loadWafer();
//        all->subdieProber->stepProbe();
//        {
//            all->subdieProber->loadWafer();
//        }

        sendSRQ(all, 0x40);
        sendMessage(all, "MC");
    }
    return;
}


/* 
 * IK Ink Device
 * Format IKc
 * Fires any inker(s) assigned to the bincode "c".
 *
 */
static void serviceIK(struct all_t *all, char *m)
{
    int bincode;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"",m);
    doDelay(0, all);

    sendSRQ(all, 0x40);
    if ( sscanf(m, "IK%d", &bincode ) != 1 )
    {
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "unable to interpret \"%s\" expected format IKc ", m);
        sendMessage(all, "MF");
    }
    else
    {
        if ( all->subdie == 1 )
        {
//            all->subdieManager->setBin(bincode);

              all->subdieProber->binDie(bincode);
        }
        else
        {
            all->prober->binDie(bincode);
        }
        sendMessage(all, "MC");
    }
    return;
}





/* 
 * Pause/Continue Probing
 * Format PA
 *
 */
static void servicePA(struct all_t *all, char *m)
{
    char message[64];

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"",m);

    if ( all->pause == true )
    {
        doDelay(0, all);

        all->pause=false;

        sendSRQ(all, 0x40);
        sendMessage(all, "CO");

        sendSRQ(all, 0x40);
        sendMessage(all, "MC");

        sendSRQ(all, 0x40);
        if ( all->subdie == 1 )
        {
//            sprintf(message, "TAS%d",  all->subdieManager->getSiteNum() );

            sprintf(message, "TAS%d",  all->subdieProber->getSiteNumber() );
            sendMessage(all, message);
        }
        else if ( all->probes > 1 )
        {
            int onWaferCode=0;
            for (int probe=0; probe < all->probes; probe++)
            {
                if ( all->prober->proberOnWafer(probe) )
                {
                    onWaferCode+=1<<probe;
                }
            }
            sprintf(message, "TA,%d", onWaferCode);
            sendMessage(all, message);
        }
        else 
        {
            sendMessage(all, "TA");
        }
    }
    else
    {
        all->pause=true;
        sendSRQ(all, 0x40);
        sendMessage(all, "MC");
    }
}




static void serviceCP(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"%s\"",m);
    doDelay(20, all);

    cleanProbes(all);

    sendSRQ(all, 0x40);
    sendMessage(all, "MC");
}

static void messageNotUnderstood(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "message \"%s\" not understood ",m);
    sendSRQ(all, 0x40);
    sendMessage(all, "MF");
}




/*---------------------------------------------------------------------------*/

static void handleMessage(struct all_t *all)
{
    char m[1024];

    /* copy message and strip EOC */
    strcpy(m, all->message);
    if (strlen(m) >= strlen(EOC) &&
	strcmp(&m[strlen(m) - strlen(EOC)], EOC) == 0)
	m[strlen(m) - strlen(EOC)] = '\0';

    printf("Receive message: \"%s\" \n",m);

    if (phToolsMatch(m, "SM15M[0-1]*"))
	serviceSM15M(all, m);
    else if (phToolsMatch(m, "ID"))
	serviceID(all, m);
    else if (phToolsMatch(m, "\\?P"))
	serviceQP(all, m);
    else if (phToolsMatch(m, "\\?W.*"))
	serviceQW(all, m);
    else if (phToolsMatch(m, "\\?R"))
        serviceQR(all, m);
    else if (phToolsMatch(m, "\\?SP4")) {
      serviceQSP4(all, m);
    } else if (phToolsMatch(m, "\\?SP29")) {
      serviceQSP29(all, m);
    } else if (phToolsMatch(m, "\\?SM11")) {
      serviceQSM11(all, m);
    } else if (phToolsMatch(m, "\\?SM3")) {
      serviceQSM3(all, m);
    } else if (phToolsMatch(m, "\\?I")) {
      serviceQI(all, m);
    } else if (phToolsMatch(m, "\\?SM1")) {
      serviceQSM1(all, m);
    }
    else if (phToolsMatch(m, "TC.*"))
	serviceTC(all, m);
    else if (phToolsMatch(m, "ET.*"))
	serviceET(all, m);
    else if (phToolsMatch(m, "AD.*"))
	serviceAD(all, m);
    else if (phToolsMatch(m, "RS"))
	serviceRS(all, m);
    else if (phToolsMatch(m, "\\$LSCL.*"))
	serviceLSCL(all, m);
    else if (phToolsMatch(m, "\\$LSAD.*"))
	serviceLSAD(all, m);
    else if (phToolsMatch(m, "RF"))
	serviceRF(all, m);
    else if (phToolsMatch(m, "SM4P.*"))
	serviceSM4P(all, m);
    else if (phToolsMatch(m, "SM35B.*"))
        serviceSM35B(all, m);
    else if (phToolsMatch(m, "HW"))
	serviceHW(all, m);
    else if (phToolsMatch(m, "ZD"))
	serviceZD(all, m);
    else if (phToolsMatch(m, "ZU"))
	serviceZU(all, m);
    else if (phToolsMatch(m, "MOX.*"))
	serviceMOX(all, m);
    else if (phToolsMatch(m, "FMX.*"))
	serviceFMX(all, m);
    else if (phToolsMatch(m, "IK.*"))
	serviceIK(all, m);
    else if (phToolsMatch(m, "FC.*"))
	serviceFC(all, m);
    else if (phToolsMatch(m, "FAX.*"))
	serviceFAX(all, m);
    else if (phToolsMatch(m, "PA"))
	servicePA(all, m);
    else if (phToolsMatch(m, "CP"))
	serviceCP(all, m);
    else
    {
        printf("message not understood \"%s\"",m);
        messageNotUnderstood(all,m);
    }

}

/*---------------------------------------------------------------------------*/

static void startTest(struct all_t *all)
{
    phComError_t comError;
//    int pending;
    char message[64];

    all->done = 0;
    do
    {

	comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length,
	    1000000L);
	checkComError(comError, all);
	if (comError == PHCOM_ERR_OK)
        {
	    handleMessage(all);
        }
        else
        {
            if ( all->prober->sendTestStartSignal() )
            {
                phLogSimMessage(all->logId, LOG_DEBUG, "send start signal ");


                if ( all->chuck.loaded )
                {
                    chuckUp(&allg, 0);
                    all->chuck.loaded = 0;    
                }
                if ( loadWafer(&allg) )
                {
                    phLogSimMessage(all->logId, LOG_VERBOSE, "send start signal ");
                    realignWafer(&allg);
                    chuckUp(&allg, 1);
                    sendSRQ(all, 0x40);

//                    all->prober->loadWafer();

                    PositionXY pos= all->prober->getProbePosition();

	            if ( all->subdie == 1 )
                    {
//	                sprintf(message, "TFS%d",  all->subdieManager->getSiteNum() );

                        if ( all->addDiePositionToTS )
                        {
                            sprintf(message, "TFX%dY%dS%d", pos.getX(), pos.getY(), all->subdieProber->getSiteNumber());
                        }
                        else
                        {
                            sprintf(message, "TFS%d",  all->subdieProber->getSiteNumber() );
                        }
                    }
                    else if ( all->probes > 1 )
                    {
                        int onWaferCode=0;
	                for (int probe=0; probe < all->probes; probe++)
                        {
                            if ( all->prober->proberOnWafer(probe) )
                            {
                                onWaferCode+=1<<probe;
                            }
                        }

                        if ( all->addDiePositionToTS )
                        {
                            sprintf(message, "TFX%dY%d,%d", pos.getX(), pos.getY(), onWaferCode);
                        }
                        else
                        {
	                    sprintf(message, "TF,%d", onWaferCode);
                        }
                    }
                    else 
                    {
                        if ( all->addDiePositionToTS )
                        {
                            sprintf(message, "TFX%dY%d", pos.getX(), pos.getY() );
                        }
                        else
                        {
                            strcpy(message,"TF");
                        }
                    }

                    if ( all->corruptTestStart )
                    {
                        sendMessage(all, "T");
                    }
                    else
                    {
                        sendMessage(all, message);
                    }
//                    all->sendTestSignal=0;
                }
                else
                {
                    phLogSimMessage(all->logId, LOG_VERBOSE, "wafer end: ");
                } 
            }
            phLogSimMessage(all->logId, LOG_VERBOSE, ".");
        }

    } while (!all->done);
}


/*---------------------------------------------------------------------------*/

int arguments(int argc, char *argv[], struct all_t *all)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;
    char *pos;
    char *newpos;

    /* setup default: */

    while ((c = getopt(argc, argv, ":d:i:t:m:D:A:M:p:P:s:w:X:N:R:SCOxraf")) != -1)
    {
	switch (c) 
	{
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
	    if (strcmp(optarg, "2001") == 0) all->model = EG2001;
	    else if (strcmp(optarg, "4085") == 0) all->model = EG4085;
	    else if (strcmp(optarg, "4080") == 0) all->model = EG4080;
	    else if (strcmp(optarg, "4200") == 0) all->model = EG4200;
	    else errflg++;
	    break;
	  case 'M':
	    all->modename = strdup(optarg);
	    if (strcmp(optarg, "Prober") == 0) all->proberMode = mProber;
	    else if (strcmp(optarg, "LearnList") == 0) all->proberMode = mLearnList;
	    else if (strcmp(optarg, "SmarTest") == 0) all->proberMode = mSmarTest;
	    else errflg++;
	    break;
	  case 'p':
            Std::cout << "p- option detected " << Std::endl;
	    all->probes = 0;
	    pos = optarg;
	    while ((newpos = strchr(pos, '[')))
	    {
		if (sscanf(newpos, "[%d,%d]", 
		    &(all->pxoff[all->probes]), 
		    &(all->pyoff[all->probes])) != 2)
		    errflg++;

                if (all->pxoff[all->probes] > all->maxxoff)
                    all->maxxoff = all->pxoff[all->probes];
                if (all->pxoff[all->probes] < all->minxoff)
                    all->minxoff = all->pxoff[all->probes];
                if (all->pyoff[all->probes] > all->maxyoff)
                    all->maxyoff = all->pyoff[all->probes];
                if (all->pyoff[all->probes] < all->minyoff)
                    all->minyoff = all->pyoff[all->probes];
 
		all->probes++;
		pos = newpos + 1;
                Std::cout << "probe count is " << all->probes << Std::endl;
	    }
	    if (all->probes == 0)
		errflg++;
	    break;
	  case 's':
	    if (sscanf(optarg, "%d,%d", &(all->xstep), &(all->ystep)) != 2)
		errflg++;
	    break;
	  case 'w':
	    if (sscanf(optarg, "%d,%d,%d,%d,%d,%d", 
		&(all->xmin), &(all->ymin), 
		&(all->xmax), &(all->ymax), 
		&(all->xref), &(all->yref)) != 6)
		errflg++;
	    break;
          case 'A':
            all->autoSetupDelay =  atof(optarg) * 1000;
            break;
          case 'D':
            all->handlingDelay = atof(optarg) * 1000;
            break;
	  case 'S':
	    all->subdie = 1;
	    break;
	  case 'C':
	    all->addDiePositionToTS = true;
            break;
	  case 'X':
	    all->sendSrqDelay = atoi(optarg);
	    break;
	  case 'N':
	    all->numOfWafersToTest = atoi(optarg);
	    break;
          case 'R':
            all->retestBinNumber = atoi(optarg);
            break;
	  case 'O':
//	    all->autoSendTestStart = false;
	    break;
	  case 'x':
	    all->corruptTestStart = true;
	    break;
	  case 'r':
	    all->reverseBinning = 1;
	    break;
	  case 'a':
	    all->asciiOCommand = 1;
	    break;
	  case 'f':
	    all->sendFirstWaferSRQ = 1;
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
	    fprintf(stderr, " -d <debug-level         -1 to 4, default 0\n");
	    fprintf(stderr, " -D                      perform interactive stepping\n");

            fprintf(stderr, " -D <delay>              parts handling delay [sec], default: 1\n");
            fprintf(stderr, " -A <delay>              automatic start after initialization delay [sec]\n");

	    fprintf(stderr, " -i <gpib-interface>     GPIB interface device, default hpib\n");
	    fprintf(stderr, " -t <timeout>            GPIB timeout to be used [msec], default 5000\n");
	    fprintf(stderr, " -m <prober model>       prober model, one of:\n");
	    fprintf(stderr, "                         2001, 2010, 4080, 4200 default 4080\n");
	    fprintf(stderr, " -M <prober mode>        prober mode, one of:\n");
	    fprintf(stderr, "                         Prober, LearnList, SmarTest, default Prober\n");
	    fprintf(stderr, " -P <playback>           read playback file \n");
	    fprintf(stderr, " -p [<x>,<y>]+           probe coordinate list, default [0,0]\n");
	    fprintf(stderr, " -s <x>,<y>              probe step size, default 1,1\n");
	    fprintf(stderr, " -C                      add die coordinates to TF and TS signal\n");
	    fprintf(stderr, " -X <delay>              add millisecond <delay> before sending SRQ \n");
	    fprintf(stderr, " -N <num>                <num> of wafers to test, default 3 \n");
            fprintf(stderr, " -R <num>                the bin number of need to be reprobe\n");
	    fprintf(stderr, " -O                      requires operator start \n");
	    fprintf(stderr, " -x                      corrupt %dth test start signal\n",
                                                      CORRUPT_TEST_START);
	    fprintf(stderr, " -w <xmin>,<ymin>,<xmax>,<ymax>,<xref>,<yref>\n");
	    fprintf(stderr, "                         wafer layout, default 0,0,4,4,0,0\n");
	    fprintf(stderr, " -S                      subdie probing \n");
	    fprintf(stderr, " -r                      use reverse binning\n");
	    fprintf(stderr, " -a                      use ASCII style 'O' command\n");
	    fprintf(stderr, " -f                      send first wafer SRQ\n");
			    
	    return 0;
	}
    }
    return 1;
}


int main(int argc, char *argv[])
{
    if(getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0)
    {
        fprintf(stderr,"The simulator can not be started by root user\n");
        return -1;
    }
    char szHostName[MAX_NAME_LENTH] = {0};
    gethostname(szHostName, MAX_NAME_LENTH-1);

    char szVxi11Server[MAX_PATH_LENTH] = {0};
    getVxi11Path(szVxi11Server, MAX_PATH_LENTH);

    char szCmdline[MAX_CMD_LINE_LENTH] = {0};
    snprintf(szCmdline, MAX_CMD_LINE_LENTH, "%s -ip %s 1>/dev/null &", szVxi11Server, szHostName);

    system(szCmdline);
    
    sleep(1);

//    phLogMode_t modeFlags = PHLOG_MODE_NORMAL;

    int i, j;

    /* Disable the generation of the HVM alarm */
    setenv("DISABLE_HVM_ALARM_GENERATION_FOR_TCI", "1", 1);

    /* initialize prober */
    allg.debugLevel = 0;
    allg.hpibDevice = strdup("hpib");
    allg.timeout = 5000;
    allg.modelname = strdup("4080");
    allg.modename = strdup("Prober");
    allg.model = EG4080;
    allg.proberMode=mProber;
    allg.reverseBinning = 0;
    allg.asciiOCommand = 0;
    allg.sendFirstWaferSRQ = 0;
    allg.dontSendPauseSRQ = 0;
    allg.retestBinNumber = -99;
//    allg.autoSendTestStart = true;
    allg.corruptTestStart = false;

    allg.probes = 1;
    allg.pxoff[0] = 0;
    allg.pyoff[0] = 0;

    allg.maxxoff = 0;
    allg.minxoff = 0;
    allg.maxyoff = 0;
    allg.minyoff = 0;

    allg.xstep = 1;
    allg.ystep = 1;

    allg.xmin = 0;
    allg.ymin = 0;
    allg.xmax = 4;
    allg.ymax = 4;
    allg.xref = 0;
    allg.yref = 0;

    allg.interactiveDelay = 0;
    allg.subdie=0;
    allg.interactiveDelayCount = 0;

    allg.autoSetupDelay = 0.0;
    allg.handlingDelay = 0.0;

    allg.addDiePositionToTS = false;

    allg.sendSrqDelay = 0;

    allg.numOfWafersToTest = 3;

    /* get real operational parameters */
    if (!arguments(argc, argv, &allg))
	exit(1);

    /* init internal values */
    allg.master = 1;
    allg.wafercount = WAF_COUNT;
    allg.sendLotEnd = 0;
    allg.abortProbing = 0;
    for (i=0; i<DIP_GROUPS; i++)
	for (j=0; j<DIP_POSITIONS; j++)
	{
	    allg.dip[j][i] = 0;
	}
//    allg.sendTestSignal=0;
    allg.enableMicroProbing=false;
    allg.pause=false;

    Communication *Com=Communication::instance(allg.debugLevel,allg.hpibDevice);
    allg.logId=Com->getLogger();
    allg.comId=Com->getSession();


    BinningManager *pBinningManager;

    if ( allg.subdie )
    {
        pBinningManager = new SubDieManager(allg.logId);
    }
    else
    {
        pBinningManager = new DieManager(allg.logId);
    }

    /* prepare state */
    setFamilyType(&allg);
    chuckUp(&allg, 0);
    allg.chuck.loaded = 0;    


    allg.sendSrqDelayTimer = new Timer(allg.logId);

#ifdef NEW_PROBER_CLASS
    ProbeNeedle *pProbeNeedle = new ProbeNeedle(allg.logId, HOME_POSITION);

    if ( allg.probes > 1 )
    {
        for (int p=1 ; p < allg.probes ; p++)
        {
            pProbeNeedle->addProbePosition(PositionXY(allg.pxoff[p],allg.pyoff[p]));
        }
    }


    ProberSetup *proberSetup=0;
    SubDieProberSetup *subdieproberSetup=0;
    DieProberSetup *dieproberSetup=0;

    if ( allg.subdie == 1 )
    {
        subdieproberSetup = new SubDieProberSetup();
        proberSetup=subdieproberSetup;
    }
    else
    {
        dieproberSetup = new DieProberSetup();
        proberSetup=dieproberSetup;
    }

    proberSetup->binningManager   = pBinningManager;
    proberSetup->probeNeedle      = pProbeNeedle;

    if ( allg.probes == 1 )
    {
        proberSetup->steppingMode = Prober::simple;
    }
    else
    {
        proberSetup->steppingMode = Prober::compatible;
    }
    proberSetup->autoSetupDelayMsec = allg.autoSetupDelay;
    proberSetup->handlingDelayMsec =  allg.handlingDelay;

    proberSetup->numOfWafersToTest =  allg.numOfWafersToTest;

    switch ( allg.proberMode )
    {
      case mProber:
        proberSetup->proberMode = Prober::prober;
        break;
      case mLearnList:
        proberSetup->proberMode = Prober::learnlist;
        break;
      case mSmarTest:
        proberSetup->proberMode = Prober::smartest;
        break;
    }
#endif

    try {

#ifdef NEW_PROBER_CLASS

        if ( allg.subdie == 1 )
        {
            allg.subdieProber = new SubDieProber( allg.logId, *subdieproberSetup );
            allg.dieProber    = 0;
            allg.prober = allg.subdieProber;
        }
        else
        {
            allg.subdieProber = 0;
            allg.dieProber    = new DieProber( allg.logId, *dieproberSetup );
            allg.prober       = allg.dieProber;
        }
#endif

        if ( allg.subdie == 1 && allg.proberMode == mProber )
        {
            allg.subdieProber->addSubDiePosition( PositionXY(  0,  0) );
            allg.subdieProber->addSubDiePosition( PositionXY(  0,100) );
            allg.subdieProber->addSubDiePosition( PositionXY(  0,200) );
            allg.subdieProber->addSubDiePosition( PositionXY(  0,300) );
            allg.subdieProber->addSubDiePosition( PositionXY(100,  0) );
            allg.subdieProber->addSubDiePosition( PositionXY(100,100) );
            allg.subdieProber->addSubDiePosition( PositionXY(100,200) );
            allg.subdieProber->addSubDiePosition( PositionXY(100,300) );

/* 
 *    Add subdie list 
 * 
 *             X-> 0 1 2 3 4
 *            Y  0   x x x
 *               1 x x x x x
 *               2 x x x x x
 *               3 x x x x x
 *               4   x x x
 */

            allg.subdieProber->addSubDie( SubDieXY(  1,  0) );
            allg.subdieProber->addSubDie( SubDieXY(  2,  0) );
            allg.subdieProber->addSubDie( SubDieXY(  3,  0) );

            allg.subdieProber->addSubDie( SubDieXY(  0,  1) );
            allg.subdieProber->addSubDie( SubDieXY(  1,  1) );
            allg.subdieProber->addSubDie( SubDieXY(  2,  1) );
            allg.subdieProber->addSubDie( SubDieXY(  3,  1) );
            allg.subdieProber->addSubDie( SubDieXY(  4,  1) );

            allg.subdieProber->addSubDie( SubDieXY(  0,  2) );
            allg.subdieProber->addSubDie( SubDieXY(  1,  2) );
            allg.subdieProber->addSubDie( SubDieXY(  2,  2) );
            allg.subdieProber->addSubDie( SubDieXY(  3,  2) );
            allg.subdieProber->addSubDie( SubDieXY(  4,  2) );

            allg.subdieProber->addSubDie( SubDieXY(  0,  3) );
            allg.subdieProber->addSubDie( SubDieXY(  1,  3) );
            allg.subdieProber->addSubDie( SubDieXY(  2,  3) );
            allg.subdieProber->addSubDie( SubDieXY(  3,  3) );
            allg.subdieProber->addSubDie( SubDieXY(  4,  3) );

            allg.subdieProber->addSubDie( SubDieXY(  1,  4) );
            allg.subdieProber->addSubDie( SubDieXY(  2,  4) );
            allg.subdieProber->addSubDie( SubDieXY(  3,  4) );
        }
        else if ( allg.subdie == 0 && allg.proberMode == mProber )
        {

/* 
 *    Add die list 
 * 
 *             X-> 0 1 2 3 4
 *            Y  0   x x x
 *               1 x x x x x
 *               2 x x x x x
 *               3 x x x x x
 *               4   x x x
 *
 *
 *             X-> 0 1 2 3 4
 *            Y  0 x x x x x
 *               1 x x x x x
 *               2 x x x x x
 *               3 x x x x x
 *               4 x x x x x
 *
 */
            allg.dieProber->addDie( new DieXY(  0,  0) );
            allg.dieProber->addDie( new DieXY(  1,  0) );
            allg.dieProber->addDie( new DieXY(  2,  0) );
            allg.dieProber->addDie( new DieXY(  3,  0) );
            allg.dieProber->addDie( new DieXY(  4,  0) );

            allg.dieProber->addDie( new DieXY(  0,  1) );
            allg.dieProber->addDie( new DieXY(  1,  1) );
            allg.dieProber->addDie( new DieXY(  2,  1) );
            allg.dieProber->addDie( new DieXY(  3,  1) );
            allg.dieProber->addDie( new DieXY(  4,  1) );

            allg.dieProber->addDie( new DieXY(  0,  2) );
            allg.dieProber->addDie( new DieXY(  1,  2) );
            allg.dieProber->addDie( new DieXY(  2,  2) );
            allg.dieProber->addDie( new DieXY(  3,  2) );
            allg.dieProber->addDie( new DieXY(  4,  2) );

            allg.dieProber->addDie( new DieXY(  0,  3) );
            allg.dieProber->addDie( new DieXY(  1,  3) );
            allg.dieProber->addDie( new DieXY(  2,  3) );
            allg.dieProber->addDie( new DieXY(  3,  3) );
            allg.dieProber->addDie( new DieXY(  4,  3) );

            allg.dieProber->addDie( new DieXY(  0,  4) );
            allg.dieProber->addDie( new DieXY(  1,  4) );
            allg.dieProber->addDie( new DieXY(  2,  4) );
            allg.dieProber->addDie( new DieXY(  3,  4) );
            allg.dieProber->addDie( new DieXY(  4,  4) );

        }

        /* enter work loop */
        startTest(&allg);
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
    }
    catch ( ... )
    {
        Std::cerr << "FATAL ERROR: UNKNOWN EXCEPTION";
    }

    exit(0);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
