/******************************************************************************
 *
 *       (c) Copyright Advantest Ltd., 2017
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : private.c
 * CREATED  : 2017
 *
 * CONTENTS : Private handler specific implementation
 *
 * AUTHORS  : Jax Wu, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 2019, Jax Wu, created
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <iostream>
#include <fcntl.h>
#include<time.h>
#include<sys/time.h>
#include<pthread.h>
#include<mutex>
#include<queue>
/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_conf.h"
#include "ph_estate.h"
#include "ph_hfunc.h"
#include "private.h"
#include "ph_keys.h"
#include "gpib_conf.h"
#include "ph_hfunc_private.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include "secstwo_shared_ptr.h"
#include "secstwo_serialize.h"
#include "secstwomsg.h"
#include "dev/DriverDevKits.hpp"


#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

using namespace std;
using namespace freesecs;
using namespace freesecs::secstwo;
/*--- defines ---------------------------------------------------------------*/
#define MAX_HANDLER_ANSWER_LENGTH 8192
#define MAX_STRIP_ID_LENGTH 128
#define MAX_SITES_LENGTH 1024
#define MAX_SECS_MSG_LENGTH 8192

#define EVENT 1
#define PYRAMID_MAX_SITE_NUM 1024




static unsigned char dataSendBuf[MAX_SECS_MSG_LENGTH] = {0};
static unsigned char dataReceiveBuf[MAX_SECS_MSG_LENGTH] = {0};
static unsigned char SECSMsgBody[MAX_SECS_MSG_LENGTH] = {0};
static string sBarcode = "";
static string lot_id = "1";
static string quantity = "100";
static unsigned int serialNum = 0;
static bool receive_timout = false;
static bool isContinue = true;
static int  handler_site_number = 0;
static pthread_t thread_receive_message;

typedef struct DATAINFO
{
  DATAINFO(char* a, int b)
  {
    len = b;
    memcpy(data, a, b);
  }
  char data[MAX_SECS_MSG_LENGTH];
  int len;
} dataInfo;

queue<dataInfo> messages_received;
queue<dataInfo> events_received;
mutex mutex_send;
std::mutex mutex_message_buffer;


/* this macro allows to test a not fully implemented plugin */

/* #define ALLOW_INCOMPLETE */
#define ALLOW_INCOMPLETE

#ifdef ALLOW_INCOMPLETE
#define ReturnNotYetImplemented(x) \
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, \
        "%s not yet implemented (ignored)", x); \
    return PHFUNC_ERR_OK
#else
#define ReturnNotYetImplemented(x) \
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, \
        "%s not yet implemented", x); \
    return PHFUNC_ERR_NA
#endif

#define PointerCheck(x) \
    { \
        if ( (x) == NULL) \
        { \
            return PHFUNC_ERR_ANSWER; \
        } \
    } 

#define ChecksiteInfo(x) \
    { \
        if ( (x) == NULL) \
        { \
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"the length of device info should be 3,current length is 0 !!!" );\
            return PHFUNC_ERR_ANSWER; \
        } \
        if ( strlen( (x) ) != 3) \
        { \
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"the length of device info should be 3,current length is %d!",strlen( (x) ) );\
            return PHFUNC_ERR_ANSWER; \
        } \
    }

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/**
 * Send End Lot command to handler to stop lot
 */
void processEndLot(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
  phFuncTaStart(handlerID);
  sendEndLot(handlerID);
  phFuncTaStop(handlerID);
  sleep(1);
  isContinue = false;
}

/**
 * get the current time string in human format
 * @param none
 * @return the time string
 */
static char *getTimeString(void)
{
    static char theTimeString[64];
    struct tm theLocaltime;
    time_t theTime;
    struct timeval getTime;
    
    gettimeofday(&getTime, NULL);
    printf("seconds:%ld,us:%ld\n",getTime.tv_sec,getTime.tv_usec);
    theTime = (time_t) getTime.tv_sec;
    localtime_r(&theTime, &theLocaltime);
    sprintf(theTimeString, "%4d_%02d_%02d_%02d_%02d_%02d.%04d",
	theLocaltime.tm_year + 1900,
	theLocaltime.tm_mon + 1,
	theLocaltime.tm_mday,
	theLocaltime.tm_hour,
	theLocaltime.tm_min,
	theLocaltime.tm_sec,
	(int)(getTime.tv_usec));

    return theTimeString;	
}

/**
 * check the serial number(the last 4 btye in HSMS message header) between the des and src is same or not
 * @param des, the serial number in the message drive sent
 * @param src, the serial number in the response from handler
 * @return true if same, return false if not same
 */
static bool isExpeted(unsigned char* des, char* src, int len)
{
    if(src==NULL || des==NULL || len<14)
      return false;

    for(int i=0; i<4; ++i)
    {
        if(des[i] != src[10+i])
          return false;
    }
    return true;
}

/**
 * set serial number
 * @param buf, the data buffer will be set serial number
 * @param bufLength, the length of buf
 * @param num, the value of serial number
 * @return none
 */
static void setserialNum(unsigned char* buf,int bufLength,unsigned int num)
{
  if(buf && bufLength>=14)
  {
    // serial number cannot exceed 4 bytes
    if (num > 0x7f7f7f7f)
    {
      //reset serial number
      serialNum = 1;
      num = 1;
    }

    for(int i=10;i<14;++i)
      buf[i]=0;
    if(num <=0x7f)
      buf[13] = num & 0x7f;
    else if(num <= 0x7f7f)
    {
      buf[12] = num >> 7;
      buf[13] = num & 0x7f;
    }
    else if(num <= 0x7f7f7f)
    {
      buf[11] = num >> 14;
      buf[12] = (num >> 7) & 0x7f;
      buf[13] = num & 0x7f;
    }
    else
    {
      buf[10] = num >> 21;
      buf[11] = (num >> 14) & 0x7f;
      buf[12] = (num >> 7) & 0x7f;
      buf[13] = num & 0x7f;
    }
  }

}

/**
 * get SECS  binary data with text data 
 * @param len, recode the length of data that return this function
 * @param keyName, the key name that will be requested
 * @param value1, the first paramter of the key
 * @param value2, second paramter of the key, it may be null
 * @return the SECS binary data 
 */
char* getSECSMessage( int* len,const char* keyName,string value1="",string value2="")
{
   static char ret[MAX_SECS_MSG_LENGTH] = {0};
   memset(ret,0,sizeof(ret));
   if(strcmp(keyName,"LOT_LOAD") == 0)
   {
       string  lotload ( "<messages>                                                \
                    <secsIImsg name=\"CR\">                                 \
                        <stream>2</stream>                                  \
                        <function>41</function>                             \
                        <wbit>1</wbit>                                      \
                        <list>                                              \
                            <ascii name=\"RCMD\">LOT_LOAD</ascii>            \
                            <list>                                       \
                                <list>                                      \
                                    <ascii name=\"CPNAME\">LOT_ID</ascii>  \
                                    <ascii name=\"CPVAL\">"); lotload=lotload + value1 +"</ascii>  \
                                </list>                                     \
                                <list>                                      \
                                    <ascii name=\"CPNAME\">QUANTITY</ascii>  \
                                    <ascii name=\"CPVAL\">"; lotload= lotload + value2+"</ascii>  \
                                </list>                                     \
                            </list>                                         \
                        </list>                                             \
                    </secsIImsg>                                            \
                  </messages>";
       strncpy(ret,lotload.c_str(),sizeof(ret)-1);
       *len = strlen(ret);
   }
   else if(strcmp(keyName,"lotLoadOkAck") == 0)
   {
        string lotLoadOkAck("<messages>                                                \
                    <secsIImsg name=\"CR\">                                 \
                        <stream>6</stream>                                  \
                        <function>14</function>                             \
                        <wbit>1</wbit>                                      \
                        <list>                                              \
                                    <binary>0</binary>  \
                        </list>                                             \
                    </secsIImsg>                                            \
                  </messages>");
        strncpy(ret,lotLoadOkAck.c_str(),sizeof(ret)-1);
        *len = strlen(ret);
   }
   else if(strcmp(keyName,"LOT_START") == 0)
   {
         string  lotstart ( "<messages>                                                \
                    <secsIImsg name=\"CR\">                                 \
                        <stream>2</stream>                                  \
                        <function>41</function>                             \
                        <wbit>1</wbit>                                      \
                        <list>                                              \
                            <ascii name=\"RCMD\">LOT_START</ascii>            \
                            <list>                                       \
                                <list>                                      \
                                    <ascii name=\"CPNAME\">LOT_ID</ascii>  \
                                    <ascii name=\"CPVAL\">"); lotstart=lotstart + value1 +"</ascii>  \
                                </list>                                     \
                                                              \
                            </list>                                         \
                        </list>                                             \
                    </secsIImsg>                                            \
                  </messages>";
       strncpy(ret,lotstart.c_str(),sizeof(ret)-1);
       *len = strlen(ret);
   }
   else if(strcmp(keyName,"LOT_START_TEST") == 0)
   {
        string  lotload ( "<messages>                                                \
                    <secsIImsg name=\"CR\">                                 \
                        <stream>2</stream>                                  \
                        <function>41</function>                             \
                        <wbit>1</wbit>                                      \
                        <list>                                              \
                            <ascii name=\"RCMD\">LOT_START_TEST</ascii>            \
                            <list>                                       \
                                <list>                                      \
                                    <ascii name=\"CPNAME\">LOT_ID</ascii>  \
                                    <ascii name=\"CPVAL\">"); lotload=lotload + value1 +"</ascii>  \
                                </list>                                     \
                                <list>                                      \
                                    <ascii name=\"CPNAME\">SUMMARY_NAME</ascii>  \
                                    <ascii name=\"CPVAL\">"; lotload= lotload + value2+"</ascii>  \
                                </list>                                     \
                            </list>                                         \
                        </list>                                             \
                    </secsIImsg>                                            \
                  </messages>";
       strncpy(ret,lotload.c_str(),sizeof(ret)-1);
       *len = strlen(ret);
   }
    else if(strcmp(keyName,"BIN_UNITS") == 0)
    {
          char arr[] =  "<messages>                                         \
                    <secsIImsg name=\"CR\">                                 \
                        <stream>2</stream>                                  \
                        <function>41</function>                             \
                        <wbit>1</wbit>                                      \
                        <list>                                              \
                             <ascii name=\"RCMD\">BIN_UNITS</ascii>         \
                             <list>                                         \
                                <list>                                      \
                                 <ascii name=\"CPNAME\">BINS</ascii>        \
                                 <list>                                     \
                                 </list>                                    \
                                </list>                                     \
                             </list>                                        \
                        </list>                                             \
                          </secsIImsg>                                      \
                  </messages>"; 
        strncpy(ret,arr,sizeof(ret)-1);
        *len = strlen(ret);
    }
   else if(strcmp(keyName,"LOT_END") == 0)
   {
       string  lotend ( "<messages>                                                \
                    <secsIImsg name=\"CR\">                                 \
                        <stream>2</stream>                                  \
                        <function>41</function>                             \
                        <wbit>1</wbit>                                      \
                        <list>                                              \
                            <ascii name=\"RCMD\">LOT_END</ascii>            \
                            <list>                                       \
                                <list>                                      \
                                    <ascii name=\"CPNAME\">LOT_ID</ascii>  \
                                    <ascii name=\"CPVAL\">"); lotend=lotend + value1 +"</ascii>  \
                                </list>                                     \
                            </list>                                         \
                        </list>                                             \
                    </secsIImsg>                                            \
                  </messages>";
       strncpy(ret,lotend.c_str(),sizeof(ret)-1);
       *len = strlen(ret);
   }
   else if(strcmp(keyName,"RETEST") == 0)
   {
       string  retest ( "<messages>                                                \
                    <secsIImsg name=\"CR\">                                 \
                        <stream>2</stream>                                  \
                        <function>41</function>                             \
                        <wbit>1</wbit>                                      \
                        <list>                                              \
                            <ascii name=\"RCMD\">RETEST_FROM_BINTRAY</ascii>            \
                            <list>                                       \
                                <list>                                      \
                                    <ascii name=\"CPNAME\">LOT_ID</ascii>  \
                                    <ascii name=\"CPVAL\">"); retest=retest + value1 +"</ascii>  \
                                </list>                                     \
                                 <list>                                      \
                                    <ascii name=\"CPNAME\">TPS_LABEL</ascii>  \
                                    <ascii name=\"CPVAL\">"; retest= retest + value2+"</ascii>  \
                                </list>                                     \
                            </list>                                         \
                        </list>                                             \
                    </secsIImsg>                                            \
                  </messages>";
       strncpy(ret,retest.c_str(),sizeof(ret)-1);
       *len = strlen(ret);
   }
  
   return ret;
    
}
static int getCheckSum(unsigned char* buf,int size)
{
    int sum = 0;
    for(int i=0; i<size;++i )
      sum+=buf[i];
    return sum;
}


/**
 *  construct the SECS message header
 * @param data, the buffer that contain SECS messgae
 * @param stremName, stream value
 * @param functionName, the first paramter of the key
 * @param wBit, wait response or not
 * @return none
 */
static void constructHeader(unsigned char* data,int stremName,int functionName,int wBit)
{
    data[6] = stremName;
    data[6] |= (wBit<<7);
    data[7] = functionName;
    data[8] = 0;
    data[9] = 0;
    data[10] = 0;
    data[11] = 0;
    data[12] = 0;
    data[13]= 0;
}


/**
 *  construct the SECS message body
 * @param buf, xml message buffer
 * @param len, the length of buf
 * @param retData, ascii(binary) arr got from xml deserialize
 * @param dataLen,  max len of retData, this param used to prevent buf overlop
 * @return the actual element length of retData.
 */
int constructBody( phFuncId_t handlerID,char* buf,int len,unsigned char* retData,int dataLen)
{
    
    char schema_membuf[20*1024];
    string path;
    char* pEnv = getenv("WORKSPACE");
    if(pEnv)
    {
        path = path+pEnv;
        path+= "/ph-development/development/drivers/Generic_93K_Driver/GenericHandler/Pyramid/config/secstwo.xsd";
    }
    else
      path+="/opt/hp93000/testcell/phcontrol/drivers/Generic_93K_Driver/GenericHandler/Pyramid/config/secstwo.xsd";
    int schema_fd = open(path.c_str(), O_RDONLY);
    if(schema_fd < 0)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"open schema error." );
        return -1;
    }
    ssize_t schema_len;
    memset(schema_membuf,0,sizeof(schema_membuf));
    if(0 >= (schema_len = read(schema_fd, schema_membuf, sizeof(schema_membuf))))
    {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"read schema error." );
      close(schema_fd);
      return -1;
    }
    xml_serializer_t::msg_container_t msgs;
    int err = xml_deserializer_t::from_mem(buf, len,schema_membuf, sizeof(schema_membuf), msgs);

    pmsg_t pmsg;
    pdata_item_t pdata;
    int ix = 0,i=0;
    for(xml_serializer_t::msg_container_t::const_iterator it = msgs.begin(); it != msgs.end(); ++it, ++ix)
    {
      pmsg = *it;
      // get message data from index
      pdata = (*pmsg)[NUM(0)].clone();
      binary_serializer_t bs3(pdata);
      binary_serializer_t::data_container_t raw_data = bs3;
      for ( i = 0; i < raw_data.size() && i<dataLen; i++) {
        retData[i] = raw_data[i];
      }
    }
      close(schema_fd);
      return i;
}

/**
 * copy the SECS message body to the buffer pointed by dest
 *
 * @param dest
 * @param src, receive data from handler
 * data format: Message length(4 bytes) + Message Header(10 bytes) + Message Text(0~8Mbytes)
 *
 * @return the actual length of bytes copied
 */
static int CopySECSMsgBody(unsigned char* dest, unsigned char* src)
{
    if(dest == NULL || src == NULL)
    {
        return 0;
    }

    int nMsgLength = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | (src[3]);
    int i = 0, j = 14;
    while(i < MAX_SECS_MSG_LENGTH && j < nMsgLength+4)
      dest[i++] = src[j++];
    return i;
}

/**
 *  set the response serial number
 * @param dataSendBuf 
 * @param serialNum, the conten of serial number
 * @return none
 */
void setResponseSerialNum(unsigned char* dataSendBuf,unsigned char* serialNum)
{
    for(int i=0; i<4;++i)
        dataSendBuf[10+i] = serialNum[i];
}

/**
 *  echo the EVNET ACK when CEID is receied
 * @param funcID
 * @param serialNum
 * @return none
 */
phFuncError_t sendAnnotatedEventAck(phFuncId_t handlerID,unsigned char funcID,unsigned char* serialNum)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    memset(dataSendBuf,0,sizeof(dataSendBuf));
    constructHeader(dataSendBuf,6,funcID+1,0);
    int len = 0;
    char* p1 = getSECSMessage(&len,"lotLoadOkAck");
    int bodyLen = constructBody(handlerID,p1,len,dataSendBuf+14,sizeof(dataSendBuf));
    dataSendBuf[3] = 10+bodyLen;
    setResponseSerialNum(dataSendBuf,serialNum);
    
    // send Annotated Event Report Ack
    char command[128] = "";
    for(int i=0; i<dataSendBuf[3]+4;++i)
      command[i] = dataSendBuf[i];
    mutex_send.lock();
    phComError_t comError = phComLanSend(handlerID->myCom, command, dataSendBuf[3]+4, handlerID->heartbeatTimeout);
    if(comError != PHCOM_ERR_OK)
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"send event ack failed");
      retVal = PHFUNC_ERR_LAN;
    }
   // retVal = phFuncTaSend(handlerID, "%d%s",dataSendBuf[3]+4, dataSendBuf);
    mutex_send.unlock();
   //  PhFuncTaCheck(phFuncTaSend(handlerID, "%s", dataSendBuf));
    return retVal;
}



static int localDelay_us(long microseconds)
{
    long seconds;
    struct timeval delay;

    /* us must be converted to seconds and microseconds ** for use by select(2) */
    if (microseconds >= 1000000L)
    {
        seconds = microseconds / 1000000L;
        microseconds = microseconds % 1000000L;
    }
    else
    {
        seconds = 0;
    }

    delay.tv_sec = seconds;
    delay.tv_usec = microseconds;

    /* return 0, if select was interrupted, 1 otherwise */
    if (select(0, NULL, NULL, NULL, &delay) == -1 && errno == EINTR)
        return 0;
    else
        return 1;
}


/**
 *  get the value of int type data
 * @param buf,the data buffer
 * @param start, the index that start to check
 * @return the actual value of the part of data buffer
 */
int parseIntType(unsigned char* buf,int start)
{
    int arr[4];
    for(int i=start, index=0;i<start+4;++i,++index)
    {
        if(index == 0)
          arr[index] = (buf[i]<<24) & 0xff000000;
        else if(index == 1)
          arr[index] = (buf[i]<<16) & 0x00ff0000;
        else if(index == 2)
          arr[index] = (buf[i]<<8) & 0x0000ff00;
        else if(index == 3)
          arr[index] = buf[i] & 0x000000ff;
    }   
    int num = 0 ;
    for(int i=0; i<4;++i)
    {
      num|=arr[i];
    }
    if(num == 0xffffffff)
      num=-1;
    return num;
}

/**
 * parse the site info from handler
 * @param start,the start index of data
 * @param end, the end index of data
 * @return the parse result
 */
phFuncError_t parseSiteInfo(phFuncId_t handlerID,unsigned char* start,unsigned char* end)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    shared_ptr_t<data_item_t> pit; 
    binary_deserializer_t bs(start,end);
    pit = bs(NULL);
    unsigned int CEID =  (unsigned int)(*pit)[1];
    if(CEID == 8021)
    {
       phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"receive lot test complete CEID(8021)");
       return PHFUNC_ERR_LOT_DONE;
    }
    if(CEID != 8300)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"receive:%d,expected 8300",CEID);
        return PHFUNC_ERR_WAITING;
    }

    // S6F13R refer to web: www.hume.com/secs
    int rptid_counts = (unsigned int)(*pit)[2].size();  // report id counts
    int deviceInfo[PYRAMID_MAX_SITE_NUM] = {0};
    for(int i=0; i<rptid_counts; ++i)
    {
        int num = (*pit)[2][i][1].size();       // variables count
        for(int j=0; j<num; ++j)                // loop variables
        {
            int vid = (unsigned int)(*pit)[2][i][1][j][NUM(0)]; //variable name
            if(vid == 10095)    // site info
            {
                handler_site_number = (int)(*pit)[2][i][1][j][1].size();
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received 10095, site number is:%d",(int)(*pit)[2][i][1][j][1].size());
                for(int k=0; k<handler_site_number; k++)
                {
                    int site_value = (int)(*pit)[2][i][1][j][NUM(1)][k];
                    if( site_value > 0 )
                        deviceInfo[k] = 1;
                    else
                        deviceInfo[k] = 0;
                }
            }
            else if(vid == 10145)       //Barcode
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received 10145, site number is:%d",(int)(*pit)[2][i][1][j][NUM(1)].size());
                sBarcode = "";
                for(int k=0; k<handler_site_number; k++)
                {
                    string str = (*pit)[2][i][1][j][NUM(1)][k];
                    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"site index is: %d, barcode is: %s", k, str.c_str());
                    str = str.empty()? "0" : str;
                    sBarcode += str;
                    if(k != handler_site_number-1) sBarcode += ",";
                }
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"All sites barcode is: %s", sBarcode.c_str());
            }
        }
    }
    if(handler_site_number <= 0)
    {
         phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"the msg from handler didn't contain count of total site,receive again");
         return PHFUNC_ERR_WAITING;
    }
    int error = 0;
    for (int i=0; i<handler_site_number; i++)
    {
        if(i >= handlerID->noOfSites)
        {
            error = 1;
            break;
        }
        if( handlerID->noOfSites > PYRAMID_MAX_SITE_NUM )
        {
            error = 2;
            break;
        }
        
        if(deviceInfo[i] == 1)
        {
            handlerID->p.devicePending[i] = 1;
            handlerID->p.oredDevicePending = 1;
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                "device present at site \"%s\" (polled)", 
                handlerID->siteIds[i]);
        }
        else
        {
            handlerID->p.devicePending[i] = 0;
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                "no device at site \"%s\" (polled)", 
                handlerID->siteIds[i]);
        }
    }
    if(error == 1)
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
              "The handler seems to present more devices than configured\n"
              "The driver configuration must be changed to support more sites");
      return PHFUNC_ERR_ANSWER;
    }
    if(error == 2)
    {
      phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
              "current driver only support 1024 site, please check your configuration file");
      return PHFUNC_ERR_ANSWER;
    }
   
    return retVal;
}

/* perform the real reconfiguration */
static phFuncError_t doReconfigure(phFuncId_t handlerID)
{
    return PHFUNC_ERR_OK;
}

/* poll parts to be tested */
static phFuncError_t pollParts(phFuncId_t handlerID)
{
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s ...",__FUNCTION__);
  phFuncError_t retVal = PHFUNC_ERR_OK;
  unsigned char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {'0'};
  receive_timout = false;
  while(events_received.empty() == true && receive_timout==false );
  if(!events_received.empty())
  {
    mutex_message_buffer.lock(); 
    dataInfo d = events_received.front();
    events_received.pop();
    mutex_message_buffer.unlock();
    memcpy(handlerAnswer, d.data, d.len);
  }
  else
  {
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in pollParts:receive event timeout");
    return PHFUNC_ERR_WAITING;
  }


  memset(SECSMsgBody,0,sizeof(SECSMsgBody));
  int size = CopySECSMsgBody(SECSMsgBody, handlerAnswer);
  if(size == 0)
  {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"in pollParts:the response from handler doestn't contain any SECS message body");
    return PHFUNC_ERR_ANSWER;
  }

  retVal = parseSiteInfo(handlerID,SECSMsgBody,SECSMsgBody+size);

  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
  return retVal;
}

/* wait for parts to be tested in general */
static phFuncError_t waitForParts(phFuncId_t handlerID)
{
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s...",__FUNCTION__);
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int i =0;
    retVal = pollParts(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
    return retVal;
}
static phFuncError_t setupBinCode(phFuncId_t handlerID ,long binNumber ,unsigned char* thisBin)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
     if (  handlerID->binIds )
    {
        /* if handler bin id is defined (hardbin/softbin mapping), get the bin code from the bin id list*/
        if ( binNumber < 0 || binNumber >= handlerID->noOfBins )
        {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                "invalid binning data\n"
                "could not bin to bin index %ld \n"
                "maximum number of bins %d set by configuration %s",
                binNumber, handlerID->noOfBins,PHKEY_BI_HBIDS);
            return PHFUNC_ERR_BINNING;
        }
        int i_ret = strtol(handlerID->binIds[binNumber], (char **)NULL, 10);
        *thisBin = i_ret;
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"using default bin map,handler bi id is:%ld", binNumber);
        *thisBin = binNumber;
    }
    return retVal; 
}
/* create and send bin and reprobe information information */
static phFuncError_t binAndReprobe( phFuncId_t handlerID  /* driver plugin ID */,
                                    phEstateSiteUsage_t *oldPopulation /* current site population */,
                                    long *perSiteReprobe  /* TRUE, if a device needs reprobe*/,
                                    long *perSiteBinMap   /* valid binning data for each site where
                                                             the above reprobe flag is not set */)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    unsigned char thisBin = 0;
    char testResult[MAX_HANDLER_ANSWER_LENGTH] = ""; 
    int sendBinning = 0,i = 0;
    memset(dataSendBuf,0,sizeof(dataSendBuf));
    constructHeader(dataSendBuf,2,41,1);
    int len = 0;
    char* p1 = getSECSMessage(&len,"BIN_UNITS"); 
    int bodyLen = constructBody(handlerID,p1,len,dataSendBuf+14,sizeof(dataSendBuf));
    dataSendBuf[bodyLen+14-1] = handler_site_number;
    int k =  bodyLen+14; 
    int index =  k - 1;
    for(i=0; i<handler_site_number;++i)
    {
      dataSendBuf[k++] = 0xb1;
      dataSendBuf[k++] = 4;
      dataSendBuf[k++] = 0;
      dataSendBuf[k++] = 0;
      dataSendBuf[k++] = 0;
      dataSendBuf[k++] = 0;
    }

    switch(handlerID->model)
    {
        case PHFUNC_MOD_PYRAMID:
            for( i=0;i<handlerID->noOfSites;++i)
            {
                if(handlerID->activeSites[i] && (oldPopulation[i] == PHESTATE_SITE_POPULATED ||
                   oldPopulation[i] == PHESTATE_SITE_POPDEACT) )
                {
                  retVal = setupBinCode(handlerID,perSiteBinMap[i],&thisBin);
                  if ( retVal==PHFUNC_ERR_OK )
                  {
                       phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, 
                        "will bin device at site \"%s\" to %d", 
                        handlerID->siteIds[i], thisBin);
                       sendBinning = 1;
                       dataSendBuf[index+(i+1)*6] = thisBin;
                  }
                  else
                  {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, 
                        "unable to send binning at site \"%s\"", 
                        handlerID->siteIds[i]);
                    sendBinning = 0;
                  }
                }
            }
            break;
        default:
            break;
    }


    dataSendBuf[3] = k-4;
    setserialNum(dataSendBuf,sizeof(dataSendBuf),serialNum);
    unsigned char des[4]= "";
    for(int i=0;i<4;++i)
      des[i] = dataSendBuf[i+10];
    
    if(sendBinning)
    {
       mutex_send.lock(); 
       retVal = phFuncTaSend(handlerID, "%d%s",dataSendBuf[3]+4, dataSendBuf);
       mutex_send.unlock();
       
       if(retVal != PHFUNC_ERR_OK )
       {
         if(retVal != PHFUNC_ERR_HAVE_DEAL)
            return retVal;
       }
       if(retVal == PHFUNC_ERR_OK)
          ++serialNum; 
       if(retVal == PHFUNC_ERR_HAVE_DEAL)
       {   
        setserialNum(dataSendBuf,sizeof(dataSendBuf),serialNum-1);
        for(int i=0; i<4;++i)
          des[i] = dataSendBuf[10+i];
       }
       
       if(!events_received.empty())
       {
         mutex_message_buffer.lock(); 
         dataInfo d = events_received.front();
         events_received.pop();
         mutex_message_buffer.unlock();
         memcpy(dataReceiveBuf, d.data, d.len);
         memset(SECSMsgBody,0,sizeof(SECSMsgBody));
         int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
         if(size != 0)
         {
           shared_ptr_t<data_item_t> pit; 
           binary_deserializer_t bs(SECSMsgBody,SECSMsgBody+size);
           pit = bs(NULL);
           unsigned int CEID = (unsigned int)(*pit)[1];
           if(CEID == 8021)
           {
             phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in binAndReprobe function, receive lot test complete CEID(8021)");
             return PHFUNC_ERR_LOT_DONE;
           }
         }
       }
       
    }

    return retVal;
}

#ifdef INIT_IMPLEMENTED
/*****************************************************************************
 *
 * Initialize handler specific plugin
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateInit(
    phFuncId_t handlerID      /* the prepared driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s...",__FUNCTION__);
    phFuncError_t retVal = PHFUNC_ERR_OK;
    int i = 0;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;

    /* do some really initial settings */
    handlerID->p.sites = handlerID->noOfSites;
    for (i = 0; i < handlerID->p.sites; i++)
    {
        handlerID->p.siteUsed[i] = handlerID->activeSites[i];
        handlerID->p.devicePending[i] = 0;
    }

    handlerID->p.strictPolling = 1; //always polling since it's acted as a client roll
    handlerID->p.pollingInterval = 200000L;
    handlerID->p.oredDevicePending = 0;
    handlerID->p.status = 0L;
    handlerID->p.paused = 0;
    handlerID->p.isNewStripOptional = 0;
    handlerID->p.isRetest = 0;
    handlerID->p.deviceGot = 0;
    isContinue = true;
    strcpy(handlerID->p.eol, "\r\n");

    /* assume, that the interface was just opened and cleared, so
     wait 1 second to give the handler time to be reset the
     interface */
    sleep(1);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
    return retVal;
}
#endif


#ifdef RECONFIGURE_IMPLEMENTED
/*****************************************************************************
 *
 * reconfigure driver plugin
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReconfigure(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s...",__FUNCTION__);
    phFuncError_t retVal = PHFUNC_ERR_OK;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);
    PhFuncTaCheck(doReconfigure(handlerID));
    phFuncTaStop(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
    return retVal;
}
#endif


#ifdef RESET_IMPLEMENTED
/*****************************************************************************
 *
 * reset the handler
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReset(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateReset");
}
#endif


#ifdef GETSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Step to next device
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateGetStart(
    phFuncId_t handlerID      /* driver plugin ID */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s...",__FUNCTION__);
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
        return PHFUNC_ERR_ABORTED;
    if(handlerID->p.deviceGot)
    {
         phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"device has got ");
         return PHFUNC_ERR_OK;
    }
      
    phFuncError_t retVal = PHFUNC_ERR_OK;
    phFuncTaStart(handlerID);
    phFuncTaMarkStep(handlerID);
    retVal = waitForParts(handlerID);
 

    if(retVal == PHFUNC_ERR_LOT_DONE)
    {
         phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"receive lot end signal");
         phFuncTaStop(handlerID);
         return PHFUNC_ERR_LOT_DONE;

    }
    
    
    /* do we have at least one part? If not, ask for the current status and return with waiting */
    if (!handlerID->p.oredDevicePending)
    {
        /* during the next call everything up to here should be repeated */
        phFuncTaRemoveToMark(handlerID);
        return retVal;
    }
    phFuncTaStop(handlerID);
    /* we have received devices for test. Change the equipment specific state now */
    int i = 0;
    handlerID->p.oredDevicePending = 0;
    for ( i = 0; i < handlerID->noOfSites; i++)
    {
        if (handlerID->activeSites[i] == 1)
        {
            if (handlerID->p.devicePending[i])
            {
                handlerID->p.devicePending[i] = 0;
                population[i] = PHESTATE_SITE_POPULATED;
            }
            else
                population[i] = PHESTATE_SITE_EMPTY;
        }
        else
        {
            if (handlerID->p.devicePending[i]) /* something is wrong here, we got a device for an inactive site */
                phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "received device for deactivated site \"%s\"",handlerID->siteIds[i]);
            population[i] = PHESTATE_SITE_DEACTIVATED;
        }
    }
    handlerID->p.deviceGot = 1;
    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Exit %s...",__FUNCTION__);
    return retVal;
}
#endif



#ifdef BINDEVICE_IMPLEMENTED
/*****************************************************************************
 *
 * Bin tested device
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateBinDevice(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteBinMap      /* binning information */
)
{
    phEstateSiteUsage_t *oldPopulation;
    int entries;
    phEstateSiteUsage_t population[PHESTATE_MAX_SITES];
    int i;

    /* abort in case of unsuccessful retry */
    if (phFuncTaAskAbort(handlerID))
    {
        return PHFUNC_ERR_ABORTED;
    }

    /* get current site population */
    phEstateAGetSiteInfo(handlerID->myEstate, &oldPopulation, &entries);

    phFuncTaStart(handlerID);
    PhFuncTaCheck(binAndReprobe(handlerID, oldPopulation, NULL, perSiteBinMap));
    phFuncTaStop(handlerID);

    /* modify site population, everything went fine, otherwise we would not reach this point */
    for (i = 0; i < handlerID->noOfSites; i++)
    {
        if (handlerID->activeSites[i] && (oldPopulation[i] == PHESTATE_SITE_POPULATED || oldPopulation[i] == PHESTATE_SITE_POPDEACT))
            population[i] = oldPopulation[i] == PHESTATE_SITE_POPULATED ? PHESTATE_SITE_EMPTY : PHESTATE_SITE_DEACTIVATED;
        else
            population[i] = oldPopulation[i];
    }
    
    handlerID->p.deviceGot = 0;
    /* change site population */
    phEstateASetSiteInfo(handlerID->myEstate, population, handlerID->noOfSites);

    return PHFUNC_ERR_OK;
}
#endif



#ifdef REPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateReprobe(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateReprobe");
}
#endif



#ifdef BINREPROBE_IMPLEMENTED
/*****************************************************************************
 *
 * reprobe devices
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateBinReprobe(
    phFuncId_t handlerID     /* driver plugin ID */,
    long *perSiteReprobe     /* TRUE, if a device needs reprobe*/,
    long *perSiteBinMap      /* valid binning data for each site where
                                the above reprobe flag is not set */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateBinReprobe");
}
#endif




phFuncError_t isCommTestSuccess( phFuncId_t handlerID  ,unsigned char* start,unsigned char* end,const char* requestName)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    if(start == end)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data");
        return PHFUNC_ERR_WAITING;
    }
    binary_deserializer_t bdes(start,end);
    shared_ptr_t<data_item_t> pit; 
    pit = bdes(NULL);
    //get the  COMMACK value, 0 - ok, 1 - denied
    unsigned int commack = (unsigned int) (*pit)[NUM(0)];
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in isCommTestSuccess,the command ack of \"%s\" is:%d",requestName,commack);
    if(commack == 0 || commack == 2)
      retVal = PHFUNC_ERR_OK;
    // 3 - parameter error for S2F42 RETEST_FROM_BINTRAY
    if(commack == 3 && strncmp(requestName, "RETEST_FROM_BINTRAY", 19) == 0)
    {
      retVal = PHFUNC_ERR_ANSWER;
      processEndLot(handlerID);
    }
   
    return retVal;

}

char* printComment(const char* msg,int len)
{
    char ch = 0;
    char tmp[8] = {0};
    static char ret[1024] = "";
    memset(ret,'\0',sizeof(ret));
    for(int i=0; i < len;++i)
    {
      ch = (int)msg[i];
      sprintf(tmp,"0x%x,",ch&0xff);
      strcat(ret,tmp);
    }

    return ret;
}

char* printComment(const unsigned char* msg,int len)
{
    char ch = 0;
    char tmp[8] = "";
    static char ret[1024] = "";
    memset(ret,'\0',sizeof(ret));
    for(int i=0; i < len && i< 1023;++i)
    {
      ch = (int)msg[i];
      sprintf(tmp,"0x%x,",ch&0xff);
      strcat(ret,tmp);
    }

    return ret;
}

// return value: -1 --> unexpeted LAN error
//               1 --> linkTest message
//               0 --> normal message.
int parseMessage(phFuncId_t handlerID, char* msg, int len, bool& isEvent)
{
    if(len>= 14)
    {
        if( (msg[3]&0xff) == 10)
        {
            // the ninth btyes(the sixth btye in header) is equal to 5, it means receive "Linktest.req"
            if( (msg[9]&0xff) == 5)
            {
                 phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received linkTest.req");
                 char command[] = {0,0,0,0xA,0x7f,0x7f,0,0,0,0x06, 0,0,0,0};
                 for(int i=0; i<4;++i)
                   command[i+10] = msg[i+10];
                 phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"trying to send linkTest.rsp");
                 mutex_send.lock();
                 phComError_t comError = phComLanSend(handlerID->myCom, command, 14, handlerID->heartbeatTimeout);
                 while(comError != PHCOM_ERR_OK)
                 {
                    if(comError == PHCOM_ERR_TIMEOUT)
                    {
                        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"send linkTest.rsp timeout,send again");
                        comError = phComLanSend(handlerID->myCom, command, 14, handlerID->heartbeatTimeout);
                    }
                    else
                    {
                        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"unexpeted LAN error occurs");
                        mutex_send.unlock();
                        return -1;
                    }
                 }
                 phLogFuncMessage(handlerID->myLogger,LOG_DEBUG,"linkTest.rsp sent");
                 mutex_send.unlock();
                 return 1;
            }
        }

        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"byte7(Stream):%d,byte8(Function):%d!!!",msg[6]&0x7f,msg[7]);
        // receive CEID
        if( (msg[6]&0x7f)==6 && (msg[7]==11||msg[7]==13) )
        {
           phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received CEID");
           isEvent = true;
           unsigned char systemBtyes[4] = "";
           for(int i=0; i<4; ++i)
             systemBtyes[i] = msg[10+i];
           sendAnnotatedEventAck(handlerID,msg[7],systemBtyes); 
        }
        
    }
    else
    {
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"the SECS format data is correct! "); 
       return -1;
    }
    
    return 0;
}
void*  receive_message(void* arg)
{
   phFuncId_t handlerID = (phFuncId_t)arg;
   char *answer = NULL;
   int length = 0;
   phComError_t comError = PHCOM_ERR_OK;
   bool isEvent = false;
   while(isContinue)
   {
     receive_timout = false;
     comError = phComLanReceiveForPyramid(handlerID->myCom, &answer, &length,handlerID->heartbeatTimeout);
     int ret = 0;
     dataInfo d(answer, length);
     switch(comError)
     {
       case PHCOM_ERR_OK:
         isEvent = false;
         if((ret=parseMessage(handlerID, d.data, d.len, isEvent)) == -1)
           exit(1);
         if(ret == 0)
         {
           if(isEvent)
           {
             phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"push event into sequence: %s", printComment(d.data, d.len));
             mutex_message_buffer.lock();
             events_received.push(d);
             mutex_message_buffer.unlock();
           }
           else
           {
             phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"push message into sequence: %s", printComment(d.data, d.len));
             mutex_message_buffer.lock();
             messages_received.push(d);
             mutex_message_buffer.unlock();
           }
         }
         break;
       case PHCOM_ERR_TIMEOUT:
         receive_timout = true;
         break;
       default:
         phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"unexpeted LAN error occurs when receive message");
         exit(1);
         break;
     }
   }
   
   return NULL;
   
}

phFuncError_t mWaitMessageOrEvent( phFuncId_t handlerID,unsigned char* des,int isWaitCEID=0)
{
  phFuncError_t retVal = PHFUNC_ERR_OK;
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Enter %s",__FUNCTION__);
  receive_timout = false;
  if(isWaitCEID)
  {
    while(events_received.empty() == true && receive_timout==false );
    if(!events_received.empty())
    {
      mutex_message_buffer.lock(); 
      dataInfo d = events_received.front();
      events_received.pop();
      mutex_message_buffer.unlock();
      memcpy(dataReceiveBuf, d.data, d.len);
      return PHFUNC_ERR_OK;
    }

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"receive event timeout");
    return PHFUNC_ERR_WAITING;
  }
  else
  {
    while(messages_received.empty() == true && receive_timout==false );
    if(!messages_received.empty())
    {
      mutex_message_buffer.lock(); 
      dataInfo d = messages_received.front();
      messages_received.pop();
      mutex_message_buffer.unlock();
      if(isExpeted(des, d.data, d.len))
        memcpy(dataReceiveBuf, d.data, d.len);
      else
      {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"the message serial number is not same,message:%s,expected serial number:%s",printComment(d.data, d.len),printComment(des,4));
        return  PHFUNC_ERR_WAITING;
      }

    }
    else
    {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"receive message timeout");
      return PHFUNC_ERR_WAITING;
    }
  }
  return retVal;
}

phFuncError_t commandSend( phFuncId_t handlerID ,unsigned char* buf,int buf_length,unsigned char* des)
{
    setserialNum(buf,buf_length,serialNum);
    if(des != NULL)
    {
      for(int i=0;i<4;++i)
        des[i] = buf[10+i];
    }
    mutex_send.lock(); 
    phFuncError_t retVal = phFuncTaSend(handlerID, "%d%s",buf[3]+4, buf);
    mutex_send.unlock(); 

    if(retVal == PHFUNC_ERR_OK)
        serialNum++;
    if(retVal == PHFUNC_ERR_HAVE_DEAL)
    {
        setserialNum(dataSendBuf,sizeof(dataSendBuf),serialNum-1);
        if(des != NULL)
        {
          for(int i=0; i<4;++i)
            des[i] = dataSendBuf[10+i];
        }
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in commandSend,command has sent,not send again");
        return PHFUNC_ERR_HAVE_DEAL;
    }
    return PHFUNC_ERR_OK;

}


phFuncError_t sendRequest(
    phFuncId_t handlerID     /* driver plugin ID */,
    unsigned char* buf,      /*data buffer to send*/
    int buf_length,          /*data length*/
    const char* requestName, /*command name in pyramid handler*/
    unsigned char** serial_num /*record the serial number in the command  */
)
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    unsigned char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    static unsigned char des[4] = "";
    if(serial_num != NULL)
        *serial_num = des;
    {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"will send \"%s\" request",requestName);
      retVal = commandSend(handlerID,buf,buf_length,des);
      if( retVal == PHFUNC_ERR_HAVE_DEAL  )
      {
         phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the send result is PHFUNC_ERR_HAVE_DEAL");
         if( ( (strncmp(requestName,"START",5)==0) || (strncmp(requestName,"STOP",4)==0) || (strncmp(requestName,"RETEST",6)==0)  ) )
           return PHFUNC_ERR_HAVE_DEAL;
      }
    }
     mWaitMessageOrEvent(handlerID,des); 
     memset(SECSMsgBody,0,sizeof(SECSMsgBody));
     int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
     if(size == 0)
     {
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data body received when query handler status");
       return PHFUNC_ERR_WAITING;
     }
     else
       retVal = isCommTestSuccess(handlerID,SECSMsgBody,SECSMsgBody+size,requestName);

    return retVal;
}

phFuncError_t sendOnlineRequest(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    unsigned char buf[128] = {0,0,0,0x0c,0,0,0x81,0x11,0,0, 0,0,0,0,0x1,0}; 
    return sendRequest(handlerID,buf,sizeof(buf),"ON-LINE",NULL);

}

phFuncError_t defineReport(
    phFuncId_t handlerID     /* driver plugin ID */,
    bool flag
)
{
    if(flag == false)
    {
        unsigned char buf[] = {0,0,0,0,  0,0,0x82,0x21,0,0,0,0,0,0, 0x1,2, 0xb1,4,0,0,0,1, 0x1,0 };
        buf[3] = sizeof(buf) -4;
        return sendRequest(handlerID,buf,sizeof(buf),"define report--clean",NULL);
    }
    unsigned char buf[] = {0,0,0,0, 0,0,0x82,0x21,0,0,0,0,0,0, \
                           0x1,0x2,0xb1,4,0,0,0,0x3, \
                           0x1,5, \
                           0x1,2,0xb1,4,0,0,0,2,0x1,1,0xb1,4,0,0,0x27,0xc9, \
                           0x1,2,0xb1,4,0,0,0,4,0x1,1,0xb1,4,0,0,0x27,0xc4, \
                           0x1,2,0xb1,4,0,0,0,6,0x1,4,0xb1,4,0,0,0x27,0x6f,0xb1,4,0,0,0x27,0xa1, 0xb1,4,0,0,0x27,0xb5, 0xb1,4,0,0,0x27,0x8d, \
                           0x1,2,0xb1,4,0,0,0,8,0x1,2,0xb1,4,0,0,0x3,0x20,0xb1,4,0,0,0x3,0x2a, \
                           0x1,2,0xb1,4,0,0,0,0xa,0x1,0x10,0xb1,4,0,0,0x2e,0xea,0xb1,4,0,0,0x2e,0xf4,0xb1,4,0,0,0x2e,0xfe,0xb1,4,0,0,0x2f,0x08, \
                                                           0xb1,4,0,0,0x2f,0x12,0xb1,4,0,0,0x2f,0x1c,0xb1,4,0,0,0x2f,0x26,0xb1,4,0,0,0x2f,0x30, \
                                                           0xb1,4,0,0,0x2b,0x02,0xb1,4,0,0,0x2b,0x0c,0xb1,4,0,0,0x2b,0x16,0xb1,4,0,0,0x2b,0x20, \
                                                           0xb1,4,0,0,0x2b,0x2a,0xb1,4,0,0,0x2b,0x34,0xb1,4,0,0,0x2b,0x3e,0xb1,4,0,0,0x2b,0x48 };
    buf[3] = sizeof(buf) - 4;
    return sendRequest(handlerID,buf,sizeof(buf),"define report",NULL);

}

phFuncError_t linkEventToReport(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
     unsigned char buf[] = { 0,0,0,0, 0,0,0x82,0x23,0,0,0,0,0,0, 1,2,0xb1,4,0,0,0,5, 1,5,  0x1,2,0xb1,4,0,0,0x1f,0x4c,1,1,0xb1,4,0,0,0,4, 0x1,2,0xb1,4,0,0,0x20,0x6c,1,1,0xb1,4,0,0,0,6, 0x1,2,0xb1,4,0,0,0x1f,0x55,1,1,0xb1,4,0,0,0,4, 0x1,2,0xb1,4,0,0,0xf,0xd2,0x1,1,0xb1,4,0,0,0,8, 0x1,2,0xb1,4,0,0,0x1f,0x48,1,1,0xb1,4,0,0,0,0xa} ;
     buf[3] = sizeof(buf) -4;
    return sendRequest(handlerID,buf,sizeof(buf),"link event report",NULL);

}
phFuncError_t EnableOrDisableEventReport(
    phFuncId_t handlerID     /* driver plugin ID */,
    bool isEnable
)
{
    unsigned char value=0;
    if(isEnable)
        value=1;
    else
        value=0;
    unsigned char buf[] = {0,0,0,0x11,0,0,0x82,0x25,0,0, 0,0,0,0,0x1,2,0x25,1,1,0x1,5,  0xb1,4,0,0,0x1f,0x4c , 0xb1,4,0,0,0x20,0x6c, 0xb1,4,0,0,0x1f,0x55, 0xb1,4,0,0,0xf,0xd2, 0xb1,4,0,0,0x1f,0x48};
    buf[3] = sizeof(buf) -4;
    buf[18] = value;
    return sendRequest(handlerID,buf,sizeof(buf),"Enable/Disable event report",NULL);

}

phFuncError_t purgeSpoolData(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    unsigned char buf[] = {0,0,0,0xd,0,0,0x86,0x17,0,0, 0,0,0,0,0xa5,1,1}; 
    buf[3] = sizeof(buf) -4;
    return sendRequest(handlerID,buf,sizeof(buf),"purge spool data",NULL);

}

phFuncError_t configSpooling(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    unsigned char buf[] = {0,0,0,0xc,0,0,0x82,0x2b,0,0, 0,0,0,0,0x1,0}; 
    buf[3] = sizeof(buf) -4;
    return sendRequest(handlerID,buf,sizeof(buf),"config spooling",NULL);

}
phFuncError_t sendStartRequest(
    phFuncId_t handlerID     /* driver plugin ID */,
    unsigned char** preSerialNum
)
{
    unsigned char buf[] = {0,0,0,0x15,0,0,0x82,0x29,0,0, 0,0,0,0,0x1,2,0x41,5,0x53,0x54,0x41,0x52,0x54,0x1,0}; 
    buf[3] = sizeof(buf) -4;
    return sendRequest(handlerID,buf,sizeof(buf),"START",preSerialNum);

}

phFuncError_t sendStopRequest(
    phFuncId_t handlerID     /* driver plugin ID */,
    unsigned char** preSerialNum
)
{
    unsigned char buf[] = {0,0,0,0x15,0,0,0x82,0x29,0,0, 0,0,0,0,0x1,2,0x41,4,0x53,0x54,0x4f,0x50,0x1,0}; 
    buf[3] = sizeof(buf) -4;
    return sendRequest(handlerID,buf,sizeof(buf),"STOP",preSerialNum);

}

phFuncError_t sendEndLot(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    memset(dataSendBuf,0,sizeof(dataSendBuf));
    constructHeader(dataSendBuf,2,41,1);
    int len = 0;
    char* p1 = getSECSMessage(&len,"LOT_END",lot_id);
    int bodyLen = constructBody(handlerID,p1,len,dataSendBuf+14,sizeof(dataSendBuf));
    dataSendBuf[3] = 10+bodyLen;
    setserialNum(dataSendBuf,sizeof(dataSendBuf),serialNum);

    return sendRequest(handlerID,dataSendBuf,dataSendBuf[3]+4,"LOT_END",NULL);

}

phFuncError_t sendLotStartTest(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    memset(dataSendBuf,0,sizeof(dataSendBuf));
    constructHeader(dataSendBuf,2,41,1);
    int len = 0;
    char* p1 = getSECSMessage(&len,"LOT_START_TEST",lot_id);
    int bodyLen = constructBody(handlerID,p1,len,dataSendBuf+14,sizeof(dataSendBuf));
    dataSendBuf[3] = 10+bodyLen;

    return sendRequest(handlerID,dataSendBuf,dataSendBuf[3]+4,"LOT_START_TEST",NULL);

}

phFuncError_t sendRetest(
    phFuncId_t handlerID     /* driver plugin ID */,
    const char* param
   
)
{
    memset(dataSendBuf,0,sizeof(dataSendBuf));
    constructHeader(dataSendBuf,2,41,1);
    int len = 0;
    char* p1 = getSECSMessage(&len,"RETEST",lot_id,param);
    int bodyLen = constructBody(handlerID,p1,len,dataSendBuf+14,sizeof(dataSendBuf));
    dataSendBuf[3] = 10+bodyLen;
    setserialNum(dataSendBuf,sizeof(dataSendBuf),serialNum);
    return sendRequest(handlerID,dataSendBuf,dataSendBuf[3]+4,"RETEST_FROM_BINTRAY",NULL);

}

int isLot_Start_OK(phFuncId_t handlerID,unsigned char* start,unsigned char* end)
{
    shared_ptr_t<data_item_t> pit; 
    binary_deserializer_t bs(start,end);
    pit = bs(NULL);
    unsigned int CEID = (unsigned int)(*pit)[1];
    int ret = 0;
    if(8012 == CEID)
    { 
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Received Lot_Test_Reday event(8012), lot start ok! ");
        ret = 1;
    }
    else
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Received event(%d), expected 8012! ",CEID);
    }
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"will exit isLot_Start_OK,return value:%d",ret);
    return ret;
}

unsigned char getHandlerStatusValue(unsigned char* start,unsigned char* end)
{
    shared_ptr_t<data_item_t> pit;
    binary_deserializer_t bs(start,end);
    pit = bs(NULL);
    unsigned char PID = (unsigned char)(*pit)[NUM(0)];
    return PID;
}

int getRetestDeviceCount( phFuncId_t handlerID, const char* retest_label)
{
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"try to get the LOT_SORTING_COMPLETE(8008) event.");
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"get retest device count for label: %s", retest_label);
  memset(dataReceiveBuf, 0, sizeof(dataReceiveBuf));

  if(mWaitMessageOrEvent(handlerID,NULL,EVENT) != PHFUNC_ERR_OK)
    return -1; // step in branch indicated that receive message timeout, need to receive again.

  memset(SECSMsgBody,0,sizeof(SECSMsgBody));
  int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
  if(size == 0)
  {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data body received in serviceRetest");
    return -1; // step in branch indicated that receive message timeout, need to receive again.
  }
  else
  {
    // parse event 8008
    shared_ptr_t<data_item_t> pit;
    binary_deserializer_t bs(SECSMsgBody, SECSMsgBody+size);
    pit = bs(NULL);
    unsigned int CEID = (unsigned int)(*pit)[1];

    if(8008 != CEID)
    {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Received event(%d), expected 8008! ",CEID);
      return -1; // step in branch indicated that receive message is not expected, need to receive again.
    }

    int rpts_num = (*pit)[2].size();
    for(int i=0; i<rpts_num; ++i)
    {
      unsigned int reportID = (unsigned int)(*pit)[2][i][NUM(0)];
      if(10 == reportID)
      {
        unsigned int vid_retest = 0;
        int num = (*pit)[2][i][1].size();
        for(int j=0; j<num; ++j)
        {
          unsigned int vid = (unsigned int)(*pit)[2][i][1][j][NUM(0)];  // check vid = 12xxx
          string vidValue = (*pit)[2][i][1][j][1];                      // retest label name
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the current variable is: %d, and value is: %s", vid, vidValue.c_str());
          if (strcmp(vidValue.c_str(), retest_label) == 0 && (vid > 12000))
          {
            vid_retest = vid - 1000;
            break;
          }
        }

        for(int j=0; j<num; ++j)
        {
          unsigned int vid = (unsigned int)(*pit)[2][i][1][j][NUM(0)];  // check vid = 11xxx
          unsigned int vidValue = (unsigned int)(*pit)[2][i][1][j][1];  // check value for retest device count
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the current variable is: %d, and value is: %d", vid, vidValue);
          if (vid_retest != 0 && vid_retest == vid)
          {
            return vidValue;
          }
        }
      }
    }
  }

  // not found specify label
  return -2;
}

int getCurrentHandlerStatus( phFuncId_t handlerID)
{
   unsigned char buf[] = {0,0,0,0x12,0,0,0x81,0x3,0,0,0,0,0,0,0x1,1,0xb1,4,0,0,0x3,0x2a};       //810
   buf[3] = sizeof(buf) - 4;
   unsigned char des[4] = "";
   commandSend(handlerID,buf,sizeof(buf),des) ;
   phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"try to get the current handler status,expected serial number is:%s",printComment(des,4));
   if(mWaitMessageOrEvent(handlerID,des) == PHFUNC_ERR_OK)
   {
     memset(SECSMsgBody,0,sizeof(SECSMsgBody));
     int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
     if(size == 0)
     {
       phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data body received when query handler status");
       return 0;
     }
     else
     {
       unsigned char PID = getHandlerStatusValue(SECSMsgBody,SECSMsgBody+size);
       phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"handler status value:%d",PID);
       return PID;
     }
   }
   else
     return -1; // step in branch indicated that the serial number is not same or receive message timeout,need to receive again.
  return 0;
}

phFuncError_t serviceRetest(
    phFuncId_t handlerID     /* driver plugin ID */,
    const char* param
)
{
      
      phFuncError_t retVal = PHFUNC_ERR_OK;
      static bool isHaveSendRetest = false;
      static bool isHaveReceiveLotTestReady = false; 
      static bool isHaveSendLotStartTest = false;
      static bool isHaveSendStop = false;
      static bool isHaveSendStart = false;
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Enter %s",__FUNCTION__);
      //sleep(0.5);
      if(!isHaveSendStop)
      {
        // in case the "stop" don't be sent successfully,check the return value
        if( sendStopRequest(handlerID,NULL) == PHFUNC_ERR_HAVE_DEAL)
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"stop command didn't send successfully, send again");
            phFuncTaDoReset(handlerID);
            return PHFUNC_ERR_WAITING;
        }
        isHaveSendStop=true;
      }
      
      if(!isHaveSendRetest)
      {
        sleep(1);
        int handlerStatusValue = getCurrentHandlerStatus(handlerID) ;
        if( handlerStatusValue == -1 )
        {
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in %s,timeout when wait handler response or serial number not same, ,just wait,not send command",__FUNCTION__);
          return PHFUNC_ERR_WAITING;
        }
        else if(handlerStatusValue == 65)
        {
          if(!isHaveSendStart)
          {
            if( sendStartRequest(handlerID,NULL) == PHFUNC_ERR_HAVE_DEAL)
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"start command didn't send successfully, send again");
                phFuncTaDoReset(handlerID);
                return PHFUNC_ERR_WAITING;
            }
            isHaveSendStart=true;
          }
          phFuncTaDoReset(handlerID);
          return PHFUNC_ERR_WAITING;
        }

        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"try to wait handler status to be working before send retest command");
        if(handlerStatusValue != 81)
        {
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in %s, wait handler status to be working timeout",__FUNCTION__);
          phFuncTaDoReset(handlerID);
          return PHFUNC_ERR_WAITING;
        }      
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the handler status have been working,try to send RETEST_FROM_BINTRAY command");
        retVal = sendRetest(handlerID,param);
        if(retVal == PHFUNC_ERR_HAVE_DEAL)
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"retest_from_bintray command didn't send successfully, send again");
            phFuncTaDoReset(handlerID);
            return PHFUNC_ERR_WAITING;
        }
        else if(retVal == PHFUNC_ERR_ANSWER)
        {
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"retest_from_bintray command with error parameter.");
          return PHFUNC_ERR_ANSWER;
        }
        isHaveSendRetest = true;
      }
      else
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Have send \"RETEST_FROM_BINTRAY\",not send again");

      if(!isHaveReceiveLotTestReady)
      {
        memset(dataReceiveBuf, 0, sizeof(dataReceiveBuf));
        PhFuncTaCheck( mWaitMessageOrEvent(handlerID,NULL,EVENT) );
        memset(SECSMsgBody,0,sizeof(SECSMsgBody));
        int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
        if(size == 0)
        {
          phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data body received in serviceRetest");
          return PHFUNC_ERR_WAITING;
        }
        else
        {
          if(!isLot_Start_OK(handlerID,SECSMsgBody,SECSMsgBody+size))
            return PHFUNC_ERR_WAITING;
          else
            isHaveReceiveLotTestReady = true;
        } 
      }
      else
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Have received \"LOT_TEST_READY(8012)\",not receive again");
      if(!isHaveSendLotStartTest)
      {
        phFuncTaDoReset(handlerID);
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"start to send LOT_START_TEST in serviceRetest");
        retVal = sendLotStartTest(handlerID);
        isHaveSendLotStartTest = true;
      }
      return retVal;
}


int isHandlerProcessTobeReady(phFuncId_t handlerID,unsigned char* start,unsigned char* end)
{
    shared_ptr_t<data_item_t> pit; 
    binary_deserializer_t bs(start,end);
    pit = bs(NULL);
    unsigned int CEID = (unsigned int)(*pit)[1];
    if(4050 != CEID)
    {
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in:isHandlerProcessTobeReady,the CEID is:%d not 4050, continue to wait",CEID);
        return 0;
    }
    int rpts_num = (*pit)[2].size();
    int ret = 0;
    for(int i=0; i<rpts_num; ++i)
    {
        unsigned int reportID = (unsigned int)(*pit)[2][i][NUM(0)];
        if(8 == reportID)
        {
            unsigned int processID = (unsigned int)(*pit)[2][i][1][1][1];
            if(85 == processID)
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the current process is:READY(85),move to the next step(LOT_LOAD)!");
                ret = 1;
            }
        }
    }
    
    if(ret != 1)
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"receive 4050,but the status value is not 85(READY status), continue to wait");
    return ret;
}


#ifdef COMMTEST_IMPLEMENTED
/*****************************************************************************
 *
 * Communication test
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Prober specific implementation of the corresponding plugin function
 * as defined in ph_pfunc.h
 ***************************************************************************/
phFuncError_t privateCommTest(
    phFuncId_t handlerID     /* driver plugin ID */,
    int *testPassed          /* whether the communication test has passed
                                or failed        */
)
{
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"Entering %s ...",__FUNCTION__);
    phFuncTaStart(handlerID);
    phFuncError_t retVal = PHFUNC_ERR_OK;
    if(pthread_create(&thread_receive_message,NULL,&receive_message,handlerID) != 0)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"create thread failed");
        return PHFUNC_ERR_LAN;
    }
    //reset serial num when the driver initial
    serialNum = 1;
    
    static int isFirstTime = 1;
    unsigned char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    if(isFirstTime)
    {
      unsigned char req[] = {0,0,0,0xA,0xff,0xff,0,0,0,0x01, 0,0,0,0};
      unsigned char des[4] = "";
      commandSend(handlerID,req,sizeof(req),des); 
      receive_timout = false;
       while(messages_received.empty() == true && receive_timout==false );
       if(!messages_received.empty())
       {
            
            mutex_message_buffer.lock(); 
            dataInfo d = messages_received.front();
            messages_received.pop();
            mutex_message_buffer.unlock();

            if( isExpeted(des, d.data, d.len) )
            {
              if( (d.data[9]&0xff) == 2 )
              {
                if( (d.data[7]&0xff) != 0)
                  phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"receive select.rsp, but the select status is:%d",d.data[7]&0xff);
                else
                  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received select.rsp, SECSGEM connection is established");
              }
              else
              {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"establish SECSGEM connection failed ");
                    isContinue = false;
                    return PHFUNC_ERR_ANSWER; 
              }
            }
            else
            {
                 phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"the message serial number is not same, receive other message");
                 return  PHFUNC_ERR_WAITING;
            }
            
       }
       else
       {
            phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"establish SECSGEM connection failed due to not receive handler response");
            isContinue = false;
            return PHFUNC_ERR_ANSWER; 
       }
      
      isFirstTime= 0;
    }
   
    unsigned char buf[128] = {0,0,0,0x0c,0,0,0x81,0x0d,0,0, 0,0,0,0,0x01,0};
    retVal = sendRequest(handlerID,buf,sizeof(buf),"Establish Communications",NULL);
     if (testPassed)
        *testPassed = (retVal == PHFUNC_ERR_OK) ? 1 : 0;
    sendOnlineRequest(handlerID);
    purgeSpoolData(handlerID);
    configSpooling(handlerID);
    EnableOrDisableEventReport(handlerID,false); // disable all events and it's report
    defineReport(handlerID,false); //delete all reports and event links
    defineReport(handlerID,true);
    linkEventToReport(handlerID);
    EnableOrDisableEventReport(handlerID,true);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"exit %s  ...",__FUNCTION__);
    phFuncTaStop(handlerID);
    return PHFUNC_ERR_OK;
}
#endif




#ifdef DESTROY_IMPLEMENTED
/*****************************************************************************
 *
 * destroy the driver
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 * Handler specific implementation of the corresponding plugin function
 * as defined in ph_hfunc.h
 ***************************************************************************/
phFuncError_t privateDestroy(
    phFuncId_t handlerID     /* driver plugin ID */
)
{
    /* by default not implemented */
    ReturnNotYetImplemented("privateDestroy");
}
#endif



int isLot_Load_OK(phFuncId_t handlerID,unsigned char* start,unsigned char* end)
{
    shared_ptr_t<data_item_t> pit; 
    binary_deserializer_t bs(start,end);
    pit = bs(NULL);
    unsigned int CEID = (unsigned int)(*pit)[1];
    int ret = 0;
    if(8011 == CEID)
    {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"received the CEID:8011,lot load successfully!");
      ret = 1;
    }
    else
    {
      phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"receive CEID:%d, expeted:8011",CEID);
      ret = 0;
    }
    return ret;
}

int isLot_Start_Test_OK(phFuncId_t handlerID,unsigned char* start,unsigned char* end)
{
    shared_ptr_t<data_item_t> pit; 
    binary_deserializer_t bs(start,end);
    pit = bs(NULL);
    unsigned int CEID = (unsigned int)(*pit)[NUM(0)];
    int ret = 0;
    if(CEID == 0)
    {
      ret = 1;
    }
    
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"exit isLot_Start_Test_OK,return value is:%d",ret);
    return ret;

}


phFuncError_t waitProcessReady(phFuncId_t handlerID,unsigned char* preSerialNum)
{
  phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in waitProcessReady");
  PhFuncTaCheck( mWaitMessageOrEvent(handlerID,preSerialNum,EVENT) );
  memset(SECSMsgBody,0,sizeof(SECSMsgBody));
  int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
  if(size == 0)
  {
    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data body received when wait handler process change to ready");
    return PHFUNC_ERR_WAITING;
  }
  else
  {
    if(!isHandlerProcessTobeReady(handlerID,SECSMsgBody,SECSMsgBody+size))
      return PHFUNC_ERR_WAITING;

  }
  return PHFUNC_ERR_OK;
}
#ifdef LOTSTART_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot start signal from handler
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 ***************************************************************************/
phFuncError_t privateLotStart(
                              phFuncId_t handlerID,      /* driver plugin ID */
                              int timeoutFlag
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    static int isLotLoadOK = 0;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Entering privateLotStart, %s", timeoutFlag ? "TRUE" : "FALSE");

    static bool isStartSend = false;
    static int isHandlerProcessReady = false;
    unsigned char* preSerialNum = NULL;
    phFuncTaStart(handlerID);
   
    if(!isHandlerProcessReady)
    {
        int handlerStatusValue = getCurrentHandlerStatus(handlerID) ;
        if( handlerStatusValue == -1 )
        {
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in %s,timeout when wait handler response or serial numeber not same, ,just wait,not send command",__FUNCTION__);
          return PHFUNC_ERR_WAITING;
        }
        else if(handlerStatusValue == 65)
        {
          if(!isStartSend)
          {
            if( sendStartRequest(handlerID,NULL) == PHFUNC_ERR_HAVE_DEAL)
            {
                phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"start command didn't send successfully, send again");
                phFuncTaDoReset(handlerID);
                return PHFUNC_ERR_WAITING;
            }
            isStartSend=true;
          }
          else
          {
             phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"start command have been sent, not send START command again");
             isStartSend = true;
          }
          phFuncTaDoReset(handlerID);
          return PHFUNC_ERR_WAITING;
        }
        else if(handlerStatusValue == 85)
        {
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"handler status have been ready(85)");
            phFuncTaDoReset(handlerID);
            isHandlerProcessReady = true;
        }
        else 
        {
          phFuncTaDoReset(handlerID); // clean the previous steps
          phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"try to wait process ready");
          PhFuncTaCheck(waitProcessReady(handlerID,preSerialNum));
          isHandlerProcessReady = true;
        }
       
    }

    memset(dataSendBuf,0,sizeof(dataSendBuf));
    constructHeader(dataSendBuf,2,41,1);
    int len = 0;
    char* p1 = getSECSMessage(&len,"LOT_LOAD",lot_id,quantity); 
        
    int bodyLen = constructBody(handlerID,p1,len,dataSendBuf+14,sizeof(dataSendBuf));
    if(bodyLen == -1)
    {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR, "can't find the \"secstwo.xsd\" file. ");
        return PHFUNC_ERR_ANSWER;
    }
    dataSendBuf[3] = 10+bodyLen;
    unsigned char des[4] = "";
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"try to send LOT_LOAD command");
    commandSend(handlerID,dataSendBuf,sizeof(dataSendBuf),des);
    if( !isLotLoadOK)
    {
      memset(dataReceiveBuf, 0, sizeof(dataReceiveBuf));
      phFuncTaMarkStep(handlerID);
      PhFuncTaCheck( mWaitMessageOrEvent(handlerID,des,EVENT) );
      memset(SECSMsgBody,0,sizeof(SECSMsgBody));
      
      int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
      if(size == 0)
      {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data body received when send LOT_LOAD command");
        phFuncTaRemoveToMark(handlerID);
        return PHFUNC_ERR_WAITING;
      }
      else
      {
        if(!isLot_Load_OK(handlerID,SECSMsgBody,SECSMsgBody+size))
        {
          phFuncTaRemoveToMark(handlerID);
          return PHFUNC_ERR_WAITING;
        }

      }
    }
    isLotLoadOK = 1;

    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Start to execute LOT_START command");
    static int isLotStartOK = 0;
    // send lot_start command
    memset(dataSendBuf,0,sizeof(dataSendBuf));
    constructHeader(dataSendBuf,2,41,1);
    p1 = getSECSMessage(&len,"LOT_START",lot_id);
    bodyLen = constructBody(handlerID,p1,len,dataSendBuf+14,sizeof(dataSendBuf));
    dataSendBuf[3] = 10+bodyLen;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"try to send LOT_START command");
    commandSend(handlerID,dataSendBuf,sizeof(dataSendBuf),des);
    if( !isLotStartOK)
    {
      memset(dataReceiveBuf, 0, sizeof(dataReceiveBuf));
      phFuncTaMarkStep(handlerID);
      PhFuncTaCheck( mWaitMessageOrEvent(handlerID,des,EVENT) );
      memset(SECSMsgBody,0,sizeof(SECSMsgBody));
      int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
      if( !isLotStartOK && size < 0)
      {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"the response from handler doestn't match SECS stanard");
        isContinue = false;
        return PHFUNC_ERR_ANSWER;
      }
      if(size == 0)
      {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data body received when send  LOT_START command");
        phFuncTaRemoveToMark(handlerID);
        return PHFUNC_ERR_WAITING;
      }
      else
      {
        if(!isLot_Start_OK(handlerID,SECSMsgBody,SECSMsgBody+size))
        {
          phFuncTaRemoveToMark(handlerID);
          return PHFUNC_ERR_WAITING;
        }

      }
    }


    isLotStartOK = 1;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Start to execute LOT_START_TEST command");
    static int isLotStartTestOK = 0;
    memset(dataSendBuf,0,sizeof(dataSendBuf));
    constructHeader(dataSendBuf,2,41,1);
    string summaryName(getTimeString());
    p1 = getSECSMessage(&len,"LOT_START_TEST",lot_id,summaryName);
    bodyLen = constructBody(handlerID,p1,len,dataSendBuf+14,sizeof(dataSendBuf));
    dataSendBuf[3] = 10+bodyLen;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"try to send LOT_START_TEST command");
    commandSend(handlerID,dataSendBuf,sizeof(dataSendBuf),des);
    if( !isLotStartTestOK)
    {
      memset(dataReceiveBuf, 0, sizeof(dataReceiveBuf));
      phFuncTaMarkStep(handlerID);
      PhFuncTaCheck( mWaitMessageOrEvent(handlerID,des) );
      memset(SECSMsgBody,0,sizeof(SECSMsgBody));
      int size = CopySECSMsgBody(SECSMsgBody, dataReceiveBuf);
      if( !isLotStartTestOK && size < 0)
      {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,"the response from handler:\"%s\" doestn't match SECS standard");
        isContinue = false;
        return PHFUNC_ERR_ANSWER;
      }

      if(size == 0)
      {
        phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING,"no secs data body received when send  LOT_START_TEST command");
        phFuncTaRemoveToMark(handlerID);
        return PHFUNC_ERR_WAITING;
      }
      else
      {
        if(!isLot_Start_Test_OK(handlerID,SECSMsgBody,SECSMsgBody+size))
        {
          phFuncTaRemoveToMark(handlerID);
          return PHFUNC_ERR_WAITING;
        }

      }
    }

     
    isLotStartTestOK = 1;
    phFuncTaStop(handlerID);
    //reset all  flag in case there are more lots
    isLotLoadOK =0,isLotStartOK=0,isLotStartTestOK=0;
    retVal = PHFUNC_ERR_OK;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Exiting privateLotStart, retVal = %d", retVal);
    return retVal;
}
#endif

#ifdef LOTDONE_IMPLEMENTED
/*****************************************************************************
 *
 * Wait for lot end signal from handler
 *
 * Authors: Jax Wu
 *
 * Description:
 *
 ***************************************************************************/
phFuncError_t privateLotDone(
                              phFuncId_t handlerID,      /* driver plugin ID */
                              int timeoutFlag
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;
    processEndLot(handlerID);
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "Exiting privateLotDone, retVal = %d", retVal);
    return retVal;
}
#endif

#ifdef SETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The stub function for set status
 *
 * Authors: Jax wu
 *
 * Description:
 *  use this function to set status control command to the handler.
 *
 ***************************************************************************/
phFuncError_t privateSetStatus(
                              phFuncId_t handlerID,       /* driver plugin ID */
                              const char *commandString,          /* the string of command, i.e. the key to
                                                             set the information from Handler */
                              const char *paramString           /* the parameter for command string */
                              )
{
    phFuncError_t retVal = PHFUNC_ERR_OK;

    
    if ( phFuncTaAskAbort(handlerID) )
        return PHFUNC_ERR_ABORTED;

    phFuncTaStart(handlerID);
    const char *token = commandString;
    const char *param = paramString;
    phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateSetStatus, token = ->%s<-, param = ->%s<-", token, param);
    if(strcasecmp(token,"lot_id") == 0)
    {
        lot_id = param;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateSetStatus, set lot_id to %s",lot_id.c_str());
    }
    else if(strcasecmp(token,"quantity") == 0)
    {
        quantity = param;
        phLogFuncMessage(handlerID->myLogger, LOG_DEBUG, "privateSetStatus, set quantity to %s",quantity.c_str());
    }
    else if(strcasecmp(token,"retest_from_bin_tray") ==0) 
    {
        /* initialize PH variable NO_NEED_RETEST */
        driver_dev_kits::AccessPHVariable& aphv = driver_dev_kits::AccessPHVariable::getInstance();
        aphv.setPHVariable("NO_NEED_RETEST", "FALSE");

        static bool isNeedRetest = false;
        if(isNeedRetest)
        {
            // need retest
            isNeedRetest = true;
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"will call serviceRetest ");
            PhFuncTaCheck( serviceRetest(handlerID,param) );
        }
        else
        {
            int value = getRetestDeviceCount(handlerID, param);
            phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in %s, get retest device count value is %d.",__FUNCTION__, value);
            if(value > 0)
            {
              // need retest
              isNeedRetest = true;
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"will call serviceRetest ");
              PhFuncTaCheck( serviceRetest(handlerID,param) );
            }
            else if(value == -1)
            {
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"in %s,timeout when wait handler response or serial number not same, just wait,not send command",__FUNCTION__);
              return PHFUNC_ERR_WAITING;
            }
            else if(value == -2)
            {
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"retest_from_bintray command with label %s not found from handler.", param);
              processEndLot(handlerID);
              retVal = PHFUNC_ERR_ANSWER;
            }
            else
            {
              //no need retest
              /* set PH variable to RETEST */
              aphv.setPHVariable("NO_NEED_RETEST", "TRUE");
              phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"No need to retest since retest unit count is not more than 0");
            }
        }
    }
    else
       phLogFuncMessage(handlerID->myLogger, LOG_DEBUG,"the %s request not support yet",token);
    phFuncTaStop(handlerID);

    return retVal;
}
#endif

#ifdef GETSTATUS_IMPLEMENTED
/*****************************************************************************
 *
 * The stub function for get status
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *  use this function to retrieve the information/parameter 
 *  from handler.
 *
 ***************************************************************************/
phFuncError_t privateGetStatus(
                              phFuncId_t handlerID,       /* driver plugin ID */
                              const char *commandString,  /* the string of command, i.e. the key to
                                                             get the information from Handler */
                              const char *paramString,    /* the parameter for command string */
                              char **responseString       /* output of the response string */
                              )
{
    static char handlerAnswer[MAX_HANDLER_ANSWER_LENGTH] = {0};
    const char *token = commandString;
    const char *param = paramString;
    memset(handlerAnswer, 0, MAX_HANDLER_ANSWER_LENGTH);
    phFuncError_t retVal = PHFUNC_ERR_OK;
    *responseString = handlerAnswer;

    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_TRACE, "privateGetStatus, token = ->%s<-, param = ->%s<-", token, param);

    if( 0 == strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_BARCODE) || 0 == strcasecmp(token, PHKEY_NAME_HANDLER_STATUS_GET_DEVICEID) )
    {
        if (strlen(param) > 0)
        {
            /* return the barcode for specific site id */
            int iSiteNum = atoi(param);
            if(iSiteNum <= 0 || iSiteNum > handlerID->p.sites)
            {
              strcpy(*responseString, "");
              phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_WARNING, "The site number: %d is illegal!", iSiteNum);
            }
            else
            {
                char *ppArray[handlerID->p.sites];
                int length = 0;

                if(phSplitStringByDelimiter(sBarcode.c_str(), ',', ppArray, handlerID->p.sites, &length) == 0 )
                {
                    for(int i=0; i<length; i++ ) {
                        if(i+1 != iSiteNum) continue;
                        strncpy(handlerAnswer, ppArray[i], MAX_STATUS_MSG-1);
                        break;
                    }
                }
                else
                {
                    phLogFuncMessage(handlerID->myLogger, PHLOG_TYPE_ERROR,
                                   "The format of barcode: %s is invalid, correct format should be \"xx,xx,xx,...\", unable to parse it\n", sBarcode.c_str());
                }

                //FREE THE MEMORY USED BY phSplitStringByDelimiter
                for(int i=0; i<length; i++)
                {
                    free(ppArray[i]);
                }
            }
        }
        else
        {
            /* return the stored barcode for pyramid driver */
            strncpy(handlerAnswer, sBarcode.c_str(), MAX_STATUS_MSG-1);
        }
    }

    return retVal;
}
#endif


/*****************************************************************************
 * End of file
 *****************************************************************************/
