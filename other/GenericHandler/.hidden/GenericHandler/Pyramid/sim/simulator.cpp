/******************************************************************************
 *
 *       (c) Copyright Advantest 2022
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.cpp
 * CREATED  : 14 Jan 2022
 *
 * CONTENTS : Pyramid handler simulator
 *
 * AUTHORS  : Jax Wu, SH-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : Jax Wu, SH-R&D, Created
 *            8 Mar 2022, Zoyi Yu, add getopt function
 *****************************************************************************/
#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <fcntl.h>
#include <errno.h>
#include <cstdlib>
#include <iostream>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include "secstwo_shared_ptr.h"
#include "secstwo_serialize.h"
#include "secstwomsg.h"
#include "color.h"


#define MAX 1024
#define SA struct sockaddr 

using namespace std;
using namespace freesecs;
using namespace freesecs::secstwo;

enum STATUS
{
  STOP=0,
  START
};

enum STATUS commandStatu = STOP;
unsigned char SECSMsgBody[MAX] = {0};
unsigned char dataSendBuf[MAX] = {0};
unsigned char dataReceiveBuf[MAX] = {0};

static int port = 8888;
static int touchDownStart = 0; 
static unsigned char lastNum = 0;

/******************************************************************************
 * Copy SECS/GEM message body
 *****************************************************************************/
static int CopySECSMsgBody(unsigned char* dest,int len,unsigned char* src,int size)
{
  int i = 0;
  if(dest && src)
  {
    int j=14;
    while( i < len && j< (size+4) )
      dest[i++] = src[j++];
  }
  return i;
}

/******************************************************************************
 * Print message
 *****************************************************************************/
void mprintf(unsigned char* data,int num,int isPrintSECS=0)
{
  if(num<=0)
  {
    return;
  }

  printf("Msg is: ");

  int i = 0;
  for(i=0; i<num;++i)
    printf("0x%x,",data[i]);
  cout<<endl;
  if(isPrintSECS)
  {
    shared_ptr_t<data_item_t> pit;
    binary_deserializer_t bs(data,data+num);
    pit = bs(NULL);
    printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    pit->print(std::cout);
    printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
  }
  printf("\n");
}

/******************************************************************************
 * Copy message
 *****************************************************************************/
static int CopyMsg(unsigned char* dest,int len,unsigned char* src,int size)
{
  int i = 0;
  if(dest && src)
  {
    int j=0;
    while( i < len && j<=size)
      dest[i++] = src[j++];
  }
  return i;
}

/******************************************************************************
 * Process handler status
 *****************************************************************************/
int serviceHandlerStatus(int sockfd, unsigned char* systemBytes)
{
  unsigned char buf[] = {0,0,0,0, 0,0,1,4,0,0,0,0,0,0, 1,1,0xa5,1,0x41 };
  static int count=0;

  if(commandStatu == STOP)
    buf[sizeof(buf)-1] = 0x41;
  else if(commandStatu == START)
  {
    ++count;
    if(count == 1)
       buf[sizeof(buf)-1] = 0x55;
    else if(count == 2)
    {
     buf[sizeof(buf)-1] = 0x51;
     count=0;
    }
  }

  buf[3] = sizeof(buf) -4;
  for(int i=0;i<4;++i)
    buf[i+10] =  systemBytes[i];

  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buf,buf[3]+4);
  int ret = send(sockfd, dataSendBuf,buf[3]+4,0);
  printf("$$$$$$$$$$$$$$ Host Command S1F4:810 Acknowledge send,msg sent len:%d,errno:%d,content:\n",ret,errno);
  mprintf(buf,sizeof(buf));

  return ret;
}

/******************************************************************************
 * Construct communication message
 *****************************************************************************/
int constructCommunicationReply(int sockfd,unsigned char* systemBytes)
{
  unsigned char buf[] = {0,0,0,0x0f,0,0,0x02,0x2a,0x0,0x0,0,0,0,2,0x01,1,0x21,1,0};

  for(int i=0;i<4;++i)
    buf[i+10] =  systemBytes[i];

  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buf,buf[3]+4);
  int ret = send(sockfd, dataSendBuf,buf[3]+4,0);
  printf("$$$$$$$$$$$$$$ Host Command Acknowledge,msg sent len:%d,errno:%d\n",ret,errno);
  mprintf(dataSendBuf,ret);
  return ret;
}

/******************************************************************************
 * Send start EVENT
 *****************************************************************************/
int sendStartEvent(int sockfd, unsigned char* systemBytes)
{
    
  constructCommunicationReply(sockfd,systemBytes);
   //TODO
#if 0
  sleep(1);
  unsigned char buf_1[] = {0,0,0,0x0f,0,0,0x86,0xd,0x0,0x0,0,0,0,2, 1,3,0xb1,4,0,0,0xf,0xd2,0xb1,4,0,0,0xf,0xd2, 0x1,2, 1,2,0xb1,4,0,0,0,8, 0x1,2, 0x1,2,0xb1,4,0,0,0x3,0x20,0xb1,4,0,0,0,0x41, 0x1,2,0xb1,4,0,0,0x3,0x2a,0xb1,4,0,0,0,0x55,  1,2,0xb1,4,0,0,0,0x66,0x1,1,0xb1,4,0,0,0,0x1f};
  for(int i=0;i<4;++i)
   buf_1[i+10] =  systemBytes[i];
  buf_1[3] = sizeof(buf_1) -4;
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buf_1,buf_1[3]+4);
  int ret = send(sockfd, dataSendBuf,buf_1[3]+4,0);
  printf("%s $$$$$$$$$$$$$$ event send,msg sent len:%d,errno:%d\n",GREEN,ret,errno);
  printf("%stry to receive Event Report Ack in sendStartEvent %s\n",GREEN,RESET);
  unsigned char buffer[128] = "";
  if( (ret=recv(sockfd, buffer, sizeof(buffer),0)) <= 0)
  {
    if( ret==0)
    {
      printf("the client socket has been closed\n");
      return 1;
    }
    if(ret == -1)
    {
      printf("there are error occurs:%s\n",strerror(errno));
      return 1;
    }
  }
  if(ret == 19)
      printf("in sendStartEvent,received Event Report Ack length:%d\n",ret);
  else
      cout<<"received unknow CEID in sendStartEvent"<<endl;
#endif
     return 0;
}

/******************************************************************************
 * Send end ACK
 *****************************************************************************/
int sendEndACK(int sockfd,unsigned char* systemBytes)
{
  constructCommunicationReply(sockfd,systemBytes);
  return 0;
}

/******************************************************************************
 * Link test
 *****************************************************************************/
int serviceLinkTest(int sockfd)
{
  unsigned char buf[] = {0,0,0,0x0a,0xff,0xff,0,0,0,0x05,0,0,0,2};
  int ret = send(sockfd,buf,14,0);
  if( ret==0)
  {
    printf("the client socket has been closed\n");
    return 1;
  }
  if(ret == -1)
  {
    printf("there are error occurs:%s\n",strerror(errno));
    return 1;
  }
  printf("$$$$$ linkTest.req sent\n");
  unsigned char buffer[128] = "";
  if( (ret=recv(sockfd, buffer, sizeof(buffer),0)) <= 0)
  {
    if( ret==0)
    {
      printf("the client socket has been closed\n");
      return 1;
    }
    if(ret == -1)
    {
      printf("there are error occurs:%s\n",strerror(errno));
      return 1;
    }
  }
  printf("linkTest.rsp received\n");
  return 0;
}

/******************************************************************************
 * Send select.rsp
 *****************************************************************************/
void sendSelectRsp(int sockfd)
{
  unsigned char buf[128] = {0,0,0,0xA,0xff,0xff,0,0,0,0x02, 0,0,0,1};

  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buf,buf[3]+4);
  int ret = send(sockfd, dataSendBuf,buf[3]+4,0);
  printf("$$$$$$$$$$$$$$ select.rsp send,len:%d,errno:%d\n",ret,errno);
  return ;
}

/******************************************************************************
 * Construct LOT_LOAD message
 *****************************************************************************/
int constuctLOT_LOAD(int sockfd, unsigned char* systemBytes)
{
   constructCommunicationReply(sockfd,systemBytes);
   unsigned char buf[] = {0,0,0,0x0f,0,0,0x06,0x0d,0x80,0x01,0x40,0x5d,0x4a,0x9f,0x01,2,0xb1,4,0,0,0x1f,0x4a,0xb1,4,0,0,0x1f,0x4a};
   buf[3] = sizeof(buf) -4;
   memset(dataSendBuf,0,sizeof(dataSendBuf));
   CopyMsg(dataSendBuf,sizeof(dataSendBuf),buf,buf[3]+4);
   sleep(1);
   int ret =send(sockfd, dataSendBuf,buf[3]+4,0);
   printf("send Event Report msg sent len:%d,errno:%d\n",ret,errno);

  unsigned char buffer[MAX] = "";
  ret = 0;
  printf("try to receive  Event Report Ack in constuctLOT_LOAD\n");
  if( (ret=recv(sockfd, buffer, sizeof(buffer),0)) <= 0)
  {
    if( ret==0)
    {
      printf("the client socket has been closed\n");
      return 1;
    }
    if(ret == -1)
    {
      printf("there are error occurs:%s\n",strerror(errno));
      return 1;
    }
  }
  printf("in constuctLOT_LOAD,received Event Report Ack length:%d\n",ret);

  unsigned char buff[] = {0,0,0,0x0f,0,0,0x06,0x0d,0x80,0x01,0x40,0x5d,0x4a,0x9f,0x01,2,0xb1,4,0,0,0x1f,0x4b,0xb1,4,0,0,0x1f,0x4b};
  buff[3] = sizeof(buff) -4;
  memset(dataSendBuf,0,sizeof(dataSendBuf));
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buff,buff[3]+4);
  ret =send(sockfd, dataSendBuf,buff[3]+4,0);
  printf("send Event Report msg sent len:%d,errno:%d\n",ret,errno);

  return sizeof(buf);
}

/******************************************************************************
 * Construct LOT_START message
 *****************************************************************************/
int constuctLOT_START(int sockfd, unsigned char* systemBytes)
{
  constructCommunicationReply(sockfd,systemBytes);
  //send CEID 8013
  unsigned char buf[] = {0,0,0,0x0f,0,0,0x06,0x0d,0x80,0x01,0x40,0x5d,0x4a,0x9f,0x01,2,0xb1,4,0,0,0x1f,0x4d,0xb1,4,0,0,0x1f,0x4d};
  buf[3] = sizeof(buf) -4;
  // for(int i=0; i<4;++i)
  //   buf[10+i] = systemBytes[i];
  memset(dataSendBuf,0,sizeof(dataSendBuf));
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buf,buf[3]+4);
  int ret =send(sockfd, dataSendBuf,buf[3]+4,0);
  printf("send Event Report msg sent len:%d,errno:%d\n",ret,errno);
  mprintf(dataSendBuf,ret);

  unsigned char buffer[MAX] = "";
  ret = 0;
  printf("try to receive Event Report Ack in constuctLOT_START\n");
  if( (ret=recv(sockfd, buffer, sizeof(buffer),0)) <= 0)
  {
  if( ret==0)
  {
    printf("the client socket has been closed\n");
    return 1;
  }
  if(ret == -1)
  {
    printf("there are error occurs:%s\n",strerror(errno));
    return 1;
  }
  }
  if(ret == 19)
    printf("in constuctLOT_START,received Event Report Ack length:%d\n",ret);
  else
    cout<<"received unknow CEID in constuctLOT_START"<<endl;

  //send CEID 8012
  unsigned char buff[] = {0,0,0,0x0f,0,0,0x06,0x0d,0x80,0x01,0x40,0x5d,0x4a,0x9f,0x01,2,0xb1,4,0,0,0x1f,0x4c,0xb1,4,0,0,0x1f,0x4c};
  buff[3] = sizeof(buff) -4;

  memset(dataSendBuf,0,sizeof(dataSendBuf));
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buff,buff[3]+4);
  ret =send(sockfd, dataSendBuf,buff[3]+4,0);
  printf("send Event Report,msg sent len:%d,errno:%d\n",ret,errno);
  return sizeof(buf);
}

/******************************************************************************
 * Construct LOT_START_TEST message
 *****************************************************************************/
int constructLOT_START_TEST(int sockfd,unsigned char* systemBytes)
{
  constructCommunicationReply(sockfd,systemBytes);
  unsigned char buf[] = {0,0,0,0x0f,0,0,0x06,0x0d,0x80,0x01,0x40,0x5d,0x4a,0x9f,0x01,2,0xb1,4,0,0,0x1f,0x4f,0xb1,4,0,0,0x1f,0x4f};
  buf[3] = sizeof(buf) -4;
  for(int i=0; i<4;++i)
    buf[10+i] = systemBytes[i];
  memset(dataSendBuf,0,sizeof(dataSendBuf));
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buf,buf[3]+4);
  int ret =send(sockfd, dataSendBuf,buf[3]+4,0);
  printf(" send Event Report,msg sent len:%d,errno:%d\n",ret,errno);

  unsigned char buffer[MAX] = "";
  ret = 0;
  printf("try to receive Event Report Ack in constructLOT_START_TEST\n");
  if( (ret=recv(sockfd, buffer, sizeof(buffer),0)) <= 0)
  {
    if( ret==0)
    {
      printf("the client socket has been closed\n");
      return 1;
    }
    if(ret == -1)
    {
      printf("there are error occurs:%s\n",strerror(errno));
      return 1;
    }
  }
  if(ret == 19)
    cout<<"received Event Report Ack in constructLOT_START_TEST"<<endl;
  else
    cout<<"received unknown CEID"<<endl;

  sleep(1);
  touchDownStart = 1;
  return sizeof(buf);
}

/******************************************************************************
 * Send event to host
 *****************************************************************************/
int sendEvent(int sockfd,int CEID)
{
  cout<<GREEN<<"######### will send EVENT:"<<CEID<<" "<<RESET<<endl;
  unsigned char buf[] = {0,0,0,0x0f,0,0,0x86,0x0d,0x0,0x0,0x0,0x0,0x0,0x01,0x01,3,0xb1,4,0,0,0x1f,0x4f,0xb1,4,0,0,0x1f,0x4f,0x1,0};
  buf[26] = 0xff&(CEID>>8);
  buf[27] = 0xff&CEID;
  buf[3] = sizeof(buf) -4;
  memset(dataSendBuf,0,sizeof(dataSendBuf));
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buf,buf[3]+4);
  int ret =send(sockfd, dataSendBuf,buf[3]+4,0);
  cout<<GREEN<<"######### EVENT:"<<CEID<<" sent"<<RESET<<endl;

  unsigned char buffer[MAX] = "";
  ret = 0;
  printf("try to receive Event Report Ack in sendEvent\n");
  if( (ret=recv(sockfd, buffer, sizeof(buffer),0)) <= 0)
  {
    if( ret==0)
    {
      printf("the client socket has been closed\n");
      return 1;
    }
    if(ret == -1)
    {
      printf("there are error occurs:%s\n",strerror(errno));
      return 1;
    }
  }
  if(ret == 19)
    cout<<"received Event Report Ack in sendEvent"<<endl;
  else
    cout<<"received unknown CEID"<<endl;
  return 0;
}

/******************************************************************************
 * Parse SECS/GEM message
 *****************************************************************************/
int parseSECSMsg(int sockfd,unsigned char* start,unsigned char* end ,int* len,unsigned char* systemBytes,unsigned char* originData,int length)
{
  if( (originData[6]&0x7f) == 6 && (originData[7] == 0x17))
  {
    cout<<GREEN<<"########$$$$$$$$$$$$$$$$ receive purge Spool Data inquest!!!!!"<<RESET<<endl;
    *len = constructCommunicationReply(sockfd,systemBytes);
    cout<<"$$$$$$$$$$$$$$$ will call sendEvent"<<endl;
    sendEvent(sockfd,4081);
    cout<<"after call sendEvent"<<endl;
    return 0;
  }

  if( (originData[6]&0x7f) == 1 && (originData[7] == 3))
  {
    cout<<GREEN<<"########$$$$$$$$$$$$$$$$ receive Selected Equipment Status request!!!!!"<<RESET<<endl;
    *len = serviceHandlerStatus(sockfd,systemBytes);

    return 0;
  }
  shared_ptr_t<data_item_t> pit;
  binary_deserializer_t bs(start,end);
  pit = bs(NULL);
  string key = (*pit)[NUM(0)];
  if( *start == 1 && *(start+1) ==0)
  {
    cout<<GREEN<<"######### receive connect or (config spooling) request"<<RESET<<endl;
    *len = constructCommunicationReply(sockfd,systemBytes);
    return 0;
  }

  if(*start == 1 && *(start+2)== 0x25 )
  {
    if(*(start+4) == 1)
      cout<<GREEN<<"######### receive enable event report request inquest"<<RESET<<endl;
    if(*(start+4) == 0)
      cout<<GREEN<<"######### receive disable event report request inquest"<<RESET<<endl;
    *len = constructCommunicationReply(sockfd,systemBytes);
    return 0;
  }
  if(*start == 0xa5)
  {
    cout<<GREEN<<"######### receive purge spool data inquest"<<RESET<<endl;
    *len = constructCommunicationReply(sockfd,systemBytes);
    return 0;
  }
  if(*(start+2) == 0xb1 && *(start+7) == 1)
  {
    cout<<GREEN<<"######### receive define report clean"<<RESET<<endl;
    *len = constructCommunicationReply(sockfd,systemBytes);
    return 0;
  }
  if(*(start+2) == 0xb1 && *(start+7) == 3)
  {
    cout<<GREEN<<"######### receive define report "<<RESET<<endl;
    *len = constructCommunicationReply(sockfd,systemBytes);
    return 0;
  }
  if(*(start+2) == 0xb1  && *(start+7) == 5)
  {
    cout<<GREEN<<"######### receive link evnet report"<<RESET<<endl;
    *len = constructCommunicationReply(sockfd,systemBytes);
    return 0;
  }

  if(key == "LOT_LOAD")
  {
    cout<<GREEN<<"######### receive LOT_LOAD inquest"<<RESET<<endl;
    *len = constuctLOT_LOAD(sockfd,systemBytes);
  }
  else if(key == "LOT_START")
  {
    cout<<GREEN<<"######### receive LOT_START inquest"<<RESET<<endl;
    *len = constuctLOT_START(sockfd,systemBytes);
  }
  else if(key == "LOT_START_TEST")
  {
    cout<<GREEN<<"######### receive LOT_START_TEST inquest"<<RESET<<endl;
    *len = constructLOT_START_TEST(sockfd,systemBytes);
  }
  else if(key == "START")
  {
    cout<<GREEN<<"######### receive START request"<<RESET<<endl;
    commandStatu = START;
    *len = sendStartEvent(sockfd,systemBytes);
    return 0;
  }
  else if(key == "STOP")
  {
    cout<<GREEN<<"######### receive STOP request "<<RESET<<endl;
    commandStatu = STOP;
    *len = constructCommunicationReply(sockfd,systemBytes);
    return 0;
  }
  else if(key == "LOT_END")
  {
    cout<<GREEN<<"######### receive LOT_END request"<<RESET<<endl;
    *len = sendEndACK(sockfd,systemBytes);
    return 0;
  }
  else if(key == "RETEST_FROM_BINTRAY")
  {
    cout<<GREEN<<"######### receive RETEST_FROM_BINTRAY request"<<RESET<<endl;
    *len = constuctLOT_START(sockfd,systemBytes);
    return 0;
  }
  else
  {
    unsigned int ret = (unsigned int)(*pit)[NUM(0)];
    if(ret == 0)
      cout<<"######## receive event report ack"<<endl;
    else
    {
      cout<<"######### receive unknow msg"<<endl;
      return -1;
    }
  }
  return 0;
}

/******************************************************************************
 * Make socket unblocking
 *****************************************************************************/
static int make_socket_non_blocking(int sfd)
{
  //make the socket unblocking
  int flags = -1;
  int iRet = -1;
  flags = fcntl(sfd, F_GETFL, 0);
  if(flags == -1)
    return -1;
  flags |= O_NONBLOCK;
  iRet = fcntl(sfd, F_SETFL, flags);
  if(iRet == -1)
    return -1;
  return 0;
}

/******************************************************************************
 * Process lot
 *****************************************************************************/
static void  serviceLotDone(int sockfd)
{
  unsigned char buff[] = {0,0,0,0x0f,0,0,0x06,0x0d,0x80,0x01,0x40,0x5d,0x4a,0x9f,0x01,2,0xb1,0x4,0x0,0x0,0x1f,0x55,0xb1,0x4,0x0,0x0,0x1f,0x55};
  buff[3] = sizeof(buff) -4;
  memset(dataSendBuf,0,sizeof(dataSendBuf));
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buff,buff[3]+4);
  int ret =send(sockfd, dataSendBuf,buff[3]+4,0);
  printf("in serviceLotDone:msg sent len:%d,errno:%d\n",ret,errno);
  memset(buff,0,sizeof(buff));
  if( (ret=recv(sockfd, buff, sizeof(buff),0)) <= 0)
  {
    if(ret==0)
    {
      printf("the client socket has been closed\n");
    }
    if(ret == -1)
    {
      printf("there are error occurs:%s\n",strerror(errno));
    }
  }
  if(ret == 19)
    cout<<"receive CEID ACK"<<endl;
  else
    cout<<"receive unknow event"<<endl;
}

/******************************************************************************
 * Process lot sort
 *****************************************************************************/
static void  serviceLotSort(int sockfd)
{
  unsigned char buff[] =
  {
    0x0,0x0,0x1,0x10,0x0,0x0,0x86,0xd,0x0,0x0,0x0,0x0,0x0,0x19,\
    0x1,0x3,0xb1,0x4,0x0,0x0,0x1f,0x48,0xb1,0x4,0x0,0x0,0x1f,0x48,\
    0x1,0x1,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x0,0xa,\
    0x1,0x10,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2e,0xea,0x41,0x7,0x49,0x6e,0x70,0x75,0x74,0x20,0x31,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2e,0xf4,0x41,0x7,0x49,0x6e,0x70,0x75,0x74,0x20,0x32,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2e,0xfe,0x41,0x7,0x49,0x6e,0x70,0x75,0x74,0x20,0x33,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2f,0x8,0x41,0x5,0x45,0x6d,0x70,0x74,0x79,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2f,0x12,0x41,0x6,0x52,0x65,0x74,0x65,0x73,0x74,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2f,0x1c,0x41,0x4,0x46,0x61,0x69,0x6c,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2f,0x26,0x41,0x4,0x50,0x61,0x73,0x73,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2f,0x30,0x41,0x4,0x50,0x61,0x73,0x73,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2b,0x2,0xb1,0x4,0x0,0x0,0x0,0x0,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2b,0xc,0xb1,0x4,0x0,0x0,0x0,0x0,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2b,0x16,0xb1,0x4,0x0,0x0,0x0,0x0,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2b,0x20,0xb1,0x4,0x0,0x0,0x0,0x0,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2b,0x2a,0xb1,0x4,0x0,0x0,0x0,0x4,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2b,0x34,0xb1,0x4,0x0,0x0,0x0,0x0,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2b,0x3e,0xb1,0x4,0x0,0x0,0x0,0x0,\
    0x1,0x2,0xb1,0x4,0x0,0x0,0x2b,0x48,0xb1,0x4,0x0,0x0,0x0,0x0
  };
  mprintf(buff+14,sizeof(buff)-14,1);
  memset(dataSendBuf,0,sizeof(dataSendBuf));
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buff,sizeof(buff));

  int ret = send(sockfd, dataSendBuf,sizeof(buff),0);
  printf("in serviceLotSort:msg sent len:%d,errno:%d\n",ret,errno);

  memset(buff,0,sizeof(buff));
  if( (ret=recv(sockfd, buff, sizeof(buff),0)) <= 0)
  {
    if(ret==0)
    {
      printf("the client socket has been closed\n");
    }
    if(ret == -1)
    {
      printf("there are error occurs:%s\n",strerror(errno));
    }
  }
  if(ret == 19)
    cout<<"receive CEID ACK"<<endl;
  else
    cout<<"receive unknown event"<<endl;

  return;
}

/******************************************************************************
 * Process message
 *****************************************************************************/
int serviceTouchDown(int sockfd,unsigned char* systemBytes)
{
  static int times = 0;
  unsigned char buff[] = //{0,0,0,0x0f,0,0,0x06,0x0d,0x80,0x01,0x40,0x5d,0x4a,0x9f, 0x1,0x3,0xb1,0x4,0x0,0x0,0x20,0x6c,0xb1,0x4,0x0,0x0,0x20,0x6c,0x1,0x1,0x1,0x2,0xb1,0x4,0x0,0x0,0x0,0x6,0x1,0x4,0x1,0x2,0xb1,0x4,0x0,0x0,0x27,0x6f,0x71,0x20,0x0,0x0,0x0,0x1,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x1,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x1,0x2,0xb1,0x4,0x0,0x0,0x27,0xa1,0x1,0x8,0x41,0x0,0x41,0x0,0x41,0xd,0x44,0x39,0x4d,0x58,0x32,0x34,0x38,0x31,0x30,0x32,0x30,0x31,0x38,0x41,0x0,0x41,0x0,0x41,0x0,0x41,0x0,0x41,0x0,0x1,0x2,0xb1,0x4,0x0,0x0,0x27,0xb5,0x1,0x8,0x41,0x0,0x41,0x0,0x41,0x3,0x4c,0x41,0x33,0x41,0x0,0x41,0x0,0x41,0x0,0x41,0x0,0x41,0x0,0x1,0x2,0xb1,0x4,0x0,0x0,0x27,0x8d,0xb1,0x4,0x0,0x0,0x0,0x8,};
  {
      0x0,0x0,0x0,0xad,0x0,0x0,0x86,0xd,0x0,0x0,0x0,0x0,0x0,0x11,\
      0x1,0x3,0xb1,0x4,0x0,0x0,0x20,0x6c,0xb1,0x4,0x0,0x0,0x20,0x6c,\
      0x1,0x1,\
      0x1,0x2,0xb1,0x4,0x0,0x0,0x0,0x6,\
      0x1,0x4,\
      0x1,0x2,\
      0xb1,0x4,0x0,0x0,0x27,0x6f,\
      0x71,0x20,\
      0x0,0x0,0x0,0x1,\
      0x0,0x0,0x0,0x1,\
      0xff,0xff,0xff,0xff,\
      0xff,0xff,0xff,0xff,\
      0xff,0xff,0xff,0xff,\
      0xff,0xff,0xff,0xff,\
      0xff,0xff,0xff,0xff,\
      0xff,0xff,0xff,0xff,\
      0x1,0x2,0xb1,0x4,0x0,0x0,0x27,0xa1,\
      0x1,0x8,\
      0x41,0xd,0x44,0x39,0x4d,0x58,0x32,0x34,0x38,0x31,0x30,0x32,0x30,0x31,0x32,\
      0x41,0xd,0x44,0x39,0x4d,0x58,0x32,0x34,0x38,0x31,0x30,0x32,0x30,0x31,0x33,\
      0x41,0x0,\
      0x41,0x0,\
      0x41,0x0,\
      0x41,0x0,\
      0x41,0x0,\
      0x41,0x0,\
      0x1,0x2,0xb1,0x4,0x0,0x0,0x27,0xb5,0x1,0x8,0x41,0x0,0x41,0x0,0x41,0x3,0x4c,0x41,0x33,0x41,0x0,0x41,0x0,0x41,0x0,0x41,0x0,0x41,0x0,\
      0x1,0x2,0xb1,0x4,0x0,0x0,0x27,0x8d,0xb1,0x4,0x0,0x0,0x0,0x8
  };
  mprintf(buff+14,sizeof(buff)-14,1);

  buff[3] = sizeof(buff) -4;
  memset(dataSendBuf,0,sizeof(dataSendBuf));
  CopyMsg(dataSendBuf,sizeof(dataSendBuf),buff,buff[3]+4);
  int ret = send(sockfd, dataSendBuf,buff[3]+4,0);
  printf("In serviceTouchDown, CEID sent len:%d,errno:%d\n",ret,errno);

  memset(buff,0,sizeof(buff));
  ret = 0;
  if( (ret = recv(sockfd, buff, sizeof(buff),0)) <= 0)
  {
    if( ret == 0)
    {
      printf("The client socket has been closed\n");
      return 1;
    }
    if(ret == -1)
    {
      printf("There are error occurs:%s\n",strerror(errno));
      return 1;
    }
  }
  if(ret == 19)
    cout<<"Receive CEID ACK, will try to receive bin info."<<endl;
  else
    cout<<"Receive unknown event"<<endl;

  memset(buff,0,sizeof(buff));
  if( (ret=recv(sockfd, buff, sizeof(buff),0)) <= 0)
  {
    if( ret==0)
    {
      printf("The client socket has been closed\n");
      return 1;
    }
    if(ret == -1)
    {
      printf("There are error occurs:%s\n",strerror(errno));
      return 1;
    }
  }
  cout<<"In serviceTouchDown,msg received length:"<<ret<<endl;
  mprintf(buff,ret,0);

  int num = CopySECSMsgBody(SECSMsgBody,sizeof(SECSMsgBody),buff,buff[3]);
  mprintf(SECSMsgBody,num,1);

  shared_ptr_t<data_item_t> pit;
  binary_deserializer_t bs(SECSMsgBody,SECSMsgBody+num);
  pit = bs(NULL);
  string key = (*pit)[NUM(0)];
  printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
  pit->print(std::cout);
  printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
  if(key != "BIN_UNITS")
  {
    cout<<"Received unknown info:"<<key<<endl;
    return -1;
  }
  else
  {
    cout<<"Received bin result info, system byte:"<<endl;
    unsigned char arr[4] = "";
    for(int i=0;i<4;++i)
    {
      arr[i] = buff[10+i];
      printf("0x%.02x,",arr[i]);
    }
    cout<<endl;
    constructCommunicationReply(sockfd,arr);
    if(times != 2)
    {
      ++times;
      sleep(1);
      return 1;
    }
    else
    {
      serviceLinkTest(sockfd);
      times = 0;
      serviceLotDone(sockfd);
      serviceLotSort(sockfd);
      return -1;
    }
  }
}

/******************************************************************************
 * Function designed for chat between client and server.
 *****************************************************************************/
int func(int sockfd) 
{ 
  unsigned char buff[MAX];
  int n;
  // infinite loop for chat
  for (;;) {
    bzero(buff, MAX);

    // read the message from client and copy it in buffer
    int ret = 0;
    if( (ret=recv(sockfd, buff, sizeof(buff),0)) <= 0)
    {
      if( ret==0)
      {
        printf("The client socket has been closed\n");
        return 1;
      }
      if(ret == -1)
      {
        printf("There are error occurs:%s\n",strerror(errno));
        return 1;
      }
    }
    // print buffer which contains the client contents
    printf("From client,receive data len:%d,content:",ret);
    mprintf(buff,ret);
    unsigned char systemBytes[4] = "";
    lastNum = buff[13];
    for(int i=0;i<4;++i)
      systemBytes[i] = buff[10+i];
    int num = CopySECSMsgBody(SECSMsgBody,sizeof(SECSMsgBody),buff,buff[3]);
    mprintf(SECSMsgBody,num,1);

    if(num == 0)
    {
      sendSelectRsp(sockfd);
    }
    else
    {
      static int len = 0;
      len = 0;
      int flag = parseSECSMsg(sockfd,SECSMsgBody,SECSMsgBody+num,&len,systemBytes,buff,ret);
      if(flag != -1)
      {
        printf("Send communication ACK successfully\n");
      }
      else
      {
        printf("Will not send communication reply\n");
        ret =send(sockfd, buff, 1,0);
      }

      // if msg contains "Exit" then server exit and chat ended.
      if ( flag == -1) {
        printf("Server Exit...\n");
        return 0;
      }

      if(touchDownStart)
      {
        int ret = serviceTouchDown(sockfd,systemBytes) ;
        while(ret == 1)
        {
          ret = serviceTouchDown(sockfd,systemBytes);
          sleep(1);
        }
        touchDownStart = 0;
      }
    }
  }

  return 0;
}

/******************************************************************************
 * parse arguments
 *****************************************************************************/
int arguments(int argc, char *argv[])
{
  int c;
  int errflg = 0;
  extern char *optarg;
  extern int optopt;

  /* setup default: */
  while ((c = getopt(argc, argv, "p:")) != -1)
  {
    switch (c)
    {
      case 'p':
        port = atoi(optarg);
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
      fprintf(stderr, "Usage: %s [<options>]\n", argv[0]);
      fprintf(stderr, " -p <port>       default: 8888\n");
      return 0;
    }
  }
  return 1;
}

/******************************************************************************
 * Main entry
 *****************************************************************************/
int main(int argc, char *argv[])
{ 
  if(getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0)
  {
    fprintf(stderr,"The simulator can not be started by root user\n");
    return -1;
  }

  /* get real operational parameters */
  if (!arguments(argc, argv))
      exit(1);

  int sockfd, connfd;
  unsigned int len = 0;
  struct sockaddr_in servaddr, cli;

  // socket create and verification
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    printf("Socket creation failed...\n");
    exit(0);
  }
  else
  printf("Socket successfully created..\n");
  bzero(&servaddr, sizeof(servaddr));

  // assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  int sockoptval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int));
  // Binding newly created socket to given IP and verification
  if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
    printf("Socket bind failed: %s...\n",strerror(errno));
    exit(0);
  }
  else
    printf("Socket successfully binded..\n");

  // Now server is ready to listen and verification
  if ((listen(sockfd, 5)) != 0) {
    printf("Listen failed...\n");
    exit(0);
  }
  else
  printf("Server listening..\n");
  len = sizeof(cli);

  // Accept the data packet from client and verification
  connfd = accept(sockfd, (SA*)&cli, &len);
  if (connfd < 0) {
    printf("Server acccept failed...\n");
    exit(0);
  }
  else
    printf("Server acccept the client...\n");

  func(connfd);
    close(sockfd);
} 

