#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "gioBridge.h"
#include "ph_mhcom.h"
#include "ph_lan_mhcom.h"
#include "ph_tools.h"
#include "ph_mhcom_private.h"
#include "dev/DriverDevKits.hpp"


#define PH_SESSION_CHECK

#ifdef DEBUG
#define PH_SESSION_CHECK
#endif

#ifdef PH_SESSION_CHECK
#define CheckSession(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHCOM_ERR_UNKNOWN_DEVICE
#else
#define CheckSession(x)
#endif


#define RELEASE_BUFFER(x) {\
   if ( x != NULL ) \
   {\
     free(x);\
     x = NULL;\
   }\
}

#define BACKLOG 10
#define MAX_PORT_SIZE 10
#define MAX_EPOLL_EVENTS 64

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

/*--- functions -------------------------------------------------------------*/
/*****************************************************************************
 * handle return value
 *****************************************************************************/
static void handleReturnValue(phComError_t rtnValue)
{
    if(rtnValue == PHCOM_ERR_OK || rtnValue == PHCOM_ERR_TIMEOUT)
        return;
    driver_dev_kits::AlarmController alarmController;
    alarmController.raiseCommunicationAlarm(GPIB_ERROR_DESCRIPTION[rtnValue]);
}

static char *getTimeString(int withDate)
{
    static char theTimeString[64];
    struct tm *theLocaltime;
    time_t theTime;
    struct timeval getTime;

    gettimeofday(&getTime, NULL);
    theTime = (time_t) getTime.tv_sec;
    theLocaltime = localtime(&theTime);
    if (withDate)
        sprintf(theTimeString, "%4d/%02d/%02d %02d:%02d:%02d.%04d",
            theLocaltime->tm_year + 1900,
            theLocaltime->tm_mon + 1,
            theLocaltime->tm_mday,
            theLocaltime->tm_hour,
            theLocaltime->tm_min,
            theLocaltime->tm_sec,
            (int)(getTime.tv_usec/100));
    else
        sprintf(theTimeString, "%02d:%02d:%02d.%04d",
            theLocaltime->tm_hour,
            theLocaltime->tm_min,
            theLocaltime->tm_sec,
            (int)(getTime.tv_usec/100));


    return theTimeString;
}

/*****************************************************************************
 *
 * Establish a LAN server
 *
 * Authors: Xiaofei Han
 *
 * Description: Please refer to ph_mhcom.h
 *
 ***************************************************************************/

phComError_t phComLanServerCreateSocket(
        phComId_t *newSession,
        phComMode_t mode,
        int serverPort,
        phLogId_t logger )
{
    int iRet = -1;                   //return value of system invocation
    char szServerPort[MAX_PORT_SIZE] = {0}; //store the server port in string format
    struct addrinfo hints;           //socket info restriction
    struct addrinfo *results = NULL; //socket info returned by getaddrinfo
    struct addrinfo *pResult = NULL; //socket info to iterate results
    struct phComStruct *tmpId;
    struct epoll_event event;        //the epoll event restriction

    phLogComMessage(logger, PHLOG_TYPE_TRACE, "phComLanServerCreateSocket(P%p, %d, %d, P%p)", newSession, (int) mode, serverPort, logger);

    /* set session ID to NULL in case anything goes wrong */
    *newSession = NULL;

    /* allocate new communication data type */
    tmpId = PhNew(struct phComStruct);
    if (tmpId == 0)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL,"not enough memory during call to phComLanServerCreateSocket()");
        return PHCOM_ERR_MEMORY;
    }
    tmpId->lan = PhNew(struct phComLanStruct);
    if (tmpId->lan == 0)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "not enough memory during call to phComLanServerCreateSocket()");
        RELEASE_BUFFER(tmpId);
        return PHCOM_ERR_MEMORY;
    }

    /* initialize with default values */
    tmpId->gpib = NULL;
    tmpId->myself = NULL;
    tmpId->mode = mode;
    tmpId->loggerId = logger;
    tmpId->intfc = PHCOM_IFC_LAN_SERVER;
    tmpId->lan->sockFD = -1;
    tmpId->lan->serverPort = serverPort;
    strcpy(tmpId->lan->connectionType, "tcp");
    strcpy(tmpId->lan->serverAddr, "");
    tmpId->lan->sockAddrLen = sizeof(tmpId->lan->sockAddr);
    memset(&(tmpId->lan->sockAddr), 0, sizeof(tmpId->lan->sockAddr));
    tmpId->lan->epollFD = -1;

    //hints give the restriction and the results returned by getaddrinfo must match it
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; //the socket address is intended for "bind"
    snprintf(szServerPort, MAX_PORT_SIZE-1, "%d", serverPort);
    iRet = getaddrinfo(NULL, szServerPort, &hints, &results);

    if(iRet != 0)
    {
        //get error message from gai_strerror() since the getaddrinfo() will not set errno
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to get address info:%s", gai_strerror(iRet));
        RELEASE_BUFFER(tmpId->lan);
        RELEASE_BUFFER(tmpId);
        return PHCOM_ERR_NO_CONNECTION;
    }

    //travel the results to create socket fd
    for(pResult = results; pResult != NULL; pResult = pResult->ai_next)
    {
        tmpId->lan->sockFD = socket(pResult->ai_family, pResult->ai_socktype, pResult->ai_protocol);
        if(tmpId->lan->sockFD == -1) //try another valid address
            continue;

        iRet = bind(tmpId->lan->sockFD, pResult->ai_addr, pResult->ai_addrlen);
        if(iRet == 0)
            break;

        //close fd which cannot be bind
        close(tmpId->lan->sockFD);
    }

    if (pResult == NULL || tmpId->lan->sockFD == -1)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to bind sock fd for communication due to \"%s\"", strerror(errno));
        RELEASE_BUFFER(tmpId->lan);
        RELEASE_BUFFER(tmpId);
        freeaddrinfo(results);
        return PHCOM_ERR_NO_CONNECTION;
    }
    phLogComMessage(logger, PHLOG_TYPE_MESSAGE_4, "Socket created and bind...");

    tmpId->lan->sockAddr = *(pResult->ai_addr);
    tmpId->lan->sockAddrLen = pResult->ai_addrlen;

    freeaddrinfo(results);

    iRet = make_socket_non_blocking(tmpId->lan->sockFD);
    if(iRet == -1)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to make the sock fd non-blocking");
        close(tmpId->lan->sockFD);
        RELEASE_BUFFER(tmpId->lan);
        RELEASE_BUFFER(tmpId);
        return PHCOM_ERR_NO_CONNECTION;
    }

    iRet = listen(tmpId->lan->sockFD, BACKLOG);
    if(iRet == -1)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to listen the sock fd due to \"%s\"", strerror(errno));
        close(tmpId->lan->sockFD);
        RELEASE_BUFFER(tmpId->lan);
        RELEASE_BUFFER(tmpId);
        return PHCOM_ERR_NO_CONNECTION;
    }

    tmpId->lan->epollFD = epoll_create(1);
    if(tmpId->lan->epollFD == -1)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to create epoll fd due to \"%s\"", strerror(errno));
        close(tmpId->lan->sockFD);
        RELEASE_BUFFER(tmpId->lan);
        RELEASE_BUFFER(tmpId);
        return PHCOM_ERR_NO_CONNECTION;
    }

    event.data.fd = tmpId->lan->sockFD; //bind the socket fd
    event.events = EPOLLIN | EPOLLET;   //register the event type

    iRet = epoll_ctl(tmpId->lan->epollFD, EPOLL_CTL_ADD, tmpId->lan->sockFD, &event);
    if(iRet == -1)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to control epoll fd due to \"%s\"", strerror(errno));
        close(tmpId->lan->epollFD);
        close(tmpId->lan->sockFD);
        RELEASE_BUFFER(tmpId->lan);
        RELEASE_BUFFER(tmpId);
        return PHCOM_ERR_NO_CONNECTION;
    }

    tmpId->myself = tmpId;
    *newSession = tmpId;
    phLogComMessage(logger, LOG_VERBOSE,  "phComLanServerCreateSocket(): successful");
    return PHCOM_ERR_OK;
}

phComError_t phComLanServerWaitingForConnection(phComId_t pmhcom, phLogId_t logger)
{
    int iRet = -1;                   //return value of system invocation
    struct epoll_event event;        //the epoll event restriction
    struct epoll_event *events = NULL;//the epoll events generated by the monitored socket fd

    //allocate the events
    events = (struct epoll_event*)calloc(MAX_EPOLL_EVENTS, sizeof(event));
    phLogComMessage(logger, PHLOG_TYPE_MESSAGE_1, "waiting for the connection from handler...");

    int iRetSockFD = epoll_wait(pmhcom->lan->epollFD, events, MAX_EPOLL_EVENTS, 4000);
    if(iRetSockFD > 1)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "There are too many connections exsit but only support single connection...");
        close(pmhcom->lan->epollFD);
        close(pmhcom->lan->sockFD);
        free(events);
        return PHCOM_ERR_NO_CONNECTION;
    }
    else if(iRetSockFD == -1)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "Epoll wait for events got error : %s", strerror(errno));
        close(pmhcom->lan->epollFD);
        close(pmhcom->lan->sockFD);
        free(events);
        return PHCOM_ERR_NO_CONNECTION;
    }
    else if(iRetSockFD == 0)
    {
        phLogComMessage(logger, PHLOG_TYPE_MESSAGE_1, "timing out when waiting for the connection from handler...");
        free(events);
        return PHCOM_ERR_TIMEOUT;
    }

    if(events[0].events & EPOLLERR || events[0].events & EPOLLHUP || !(events[0].events & EPOLLIN))
    {
        //error on epoll fd
        phLogComMessage(logger, PHLOG_TYPE_FATAL, "epoll error due to %s...", strerror(errno));
        close(pmhcom->lan->epollFD);
        close(pmhcom->lan->sockFD);
        free(events);
        return PHCOM_ERR_NO_CONNECTION;
    }
    else if(events[0].data.fd == pmhcom->lan->sockFD)
    {
        //have a notification on listening socket
        struct sockaddr clientAddr;
        socklen_t addrLen = sizeof(struct sockaddr);
        char hostBuf[NI_MAXHOST] = {0};
        char serviceBuf[NI_MAXSERV] = {0};
        pmhcom->lan->sockClientFD = accept(pmhcom->lan->sockFD, &clientAddr, &addrLen);
        if(pmhcom->lan->sockClientFD == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) //already accept this client, just break the loop
                return PHCOM_ERR_OK;
            else
            {
                phLogComMessage(logger, PHLOG_TYPE_FATAL, "accept client connection failed due to %s...", strerror(errno));
                close(pmhcom->lan->epollFD);
                close(pmhcom->lan->sockFD);
                free(events);
                return PHCOM_ERR_NO_CONNECTION;
            }
        }

        getnameinfo(&clientAddr, addrLen, hostBuf, sizeof(hostBuf), serviceBuf, sizeof(serviceBuf), NI_NUMERICHOST|NI_NUMERICSERV);
        phLogComMessage(logger, PHLOG_TYPE_MESSAGE_0, "accept connection on host = %s, port = %s", hostBuf, serviceBuf);
        iRet = make_socket_non_blocking(pmhcom->lan->sockClientFD);
        if(iRet == -1)
        {
            phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to make the sock fd non-blocking");
            close(pmhcom->lan->epollFD);
            close(pmhcom->lan->sockFD);
            free(events);
            return PHCOM_ERR_NO_CONNECTION;
        }

        //generally, we need add client fd to epoll
        //but we use poll to receive and send message below
        //thus, we will not add the client fd to epoll list
        return PHCOM_ERR_OK;
    }

    phLogComMessage(logger, LOG_VERBOSE,  "leaving phComLanServerWaitingForConnection()");
    close(pmhcom->lan->epollFD);
    return PHCOM_ERR_OK;
}

/*****************************************************************************
 *
 * Establish a LAN (either TCP or UDP) connection
 *
 * Authors: Xiaofei Han
 *
 * Description: Please refer to ph_mhcom.h
 *
 ***************************************************************************/

phComError_t _phComLanOpen( phComId_t *newSession,
                           phComMode_t mode,
                           const char *lanAddress,
                           phLogId_t logger )
{
  struct phComStruct *tmpId;
  char *p = NULL;
  unsigned int port = 0;
  char tempPort[10] = "";
  //int sockFDFlag = 0;
  int ret = -1;
  char *tmpLanAddress = NULL;
  struct addrinfo tmpAddrinfo, *serverAddrinfo =  NULL;

  phLogComMessage(logger, PHLOG_TYPE_TRACE,
                  "phComLanOpen(P%p, %d, \"%s\", P%p)",
                  newSession, (int) mode, lanAddress?lanAddress : "(NULL)",logger);

  if(lanAddress == NULL)
  {
    phLogComMessage(logger, PHLOG_TYPE_FATAL, "No server address is specified");
    return PHCOM_ERR_NO_CONNECTION;
  }
  else
  {
    asprintf(&tmpLanAddress, "%s", lanAddress);
  }

  /* set session ID to NULL in case anything goes wrong */
  *newSession = NULL;

  /* allocate new communication data type */
  tmpId = PhNew(struct phComStruct);
  if (tmpId == 0)
  {
      phLogComMessage(logger, PHLOG_TYPE_FATAL,"not enough memory during call to phComLanOpen()");
      free(tmpLanAddress);
      return PHCOM_ERR_MEMORY;
  }
  tmpId->lan = PhNew(struct phComLanStruct);
  if (tmpId->lan == 0)
  {
      phLogComMessage(logger, PHLOG_TYPE_FATAL, "not enough memory during call to phComLanOpen()");
      RELEASE_BUFFER(tmpId);
      free(tmpLanAddress);
      return PHCOM_ERR_MEMORY;
  }

  /* initialize with default values */
  tmpId->gpib = NULL;
  tmpId->myself = NULL;
  tmpId->mode = mode;
  tmpId->gioId = NULL;
  tmpId->loggerId = logger;
  tmpId->intfc = PHCOM_IFC_LAN;
  tmpId->lan->sockFD = -1;
  tmpId->lan->serverPort = 0;
  strcpy(tmpId->lan->connectionType, "tcp");
  strcpy(tmpId->lan->serverAddr, "");
  tmpId->lan->sockAddrLen = sizeof(tmpId->lan->sockAddr);
  memset(&(tmpId->lan->sockAddr), 0, sizeof(tmpId->lan->sockAddr));
  tmpId->lan->epollFD = -1;

  /*
   * The format of the lan symbolic name in the configuration file
   * is "lan/[tcp,udp]/ip_address/port_number".
   * For example: tcp/10.150.1.1/9999
   */
  p = strtok(tmpLanAddress, "/");
  if(p == NULL)
  {
      phLogComMessage(logger, PHLOG_TYPE_FATAL, "Incorrect symbolic name for LAN connection: %s", lanAddress);
      RELEASE_BUFFER(tmpId->lan);
      RELEASE_BUFFER(tmpId);
      free(tmpLanAddress);
      return PHCOM_ERR_SYNTAX;
  }
  else
  {
    strncpy(tmpId->lan->connectionType, p, 3);
    if(strcmp(tmpId->lan->connectionType, "tcp") &&
       strcmp(tmpId->lan->connectionType, "udp") )
    {
      phLogComMessage(logger, PHLOG_TYPE_FATAL, "Incorrect symbolic name for LAN connection(tcp/udp is not specified): %s", lanAddress);
      RELEASE_BUFFER(tmpId->lan);
      RELEASE_BUFFER(tmpId);
      free(tmpLanAddress);
      return PHCOM_ERR_SYNTAX;
    }

    /* read the IP address */
    p = strtok(NULL, "/");
    if(p == NULL)
    {
      phLogComMessage(logger, PHLOG_TYPE_FATAL, "Incorrect symbolic name for LAN connection: %s (server address is not specified)", lanAddress);
      RELEASE_BUFFER(tmpId->lan);
      RELEASE_BUFFER(tmpId);
      free(tmpLanAddress);
      return PHCOM_ERR_SYNTAX;
    }
    strncpy(tmpId->lan->serverAddr, p, 49);

    /* read the port */
    p = strtok(NULL, "/");
    if(p == NULL)
    {
      phLogComMessage(logger, PHLOG_TYPE_FATAL, "Incorrect symbolic name for LAN connection: %s (server port is not specified)", lanAddress);
      RELEASE_BUFFER(tmpId->lan);
      RELEASE_BUFFER(tmpId);
      free(tmpLanAddress);
      return PHCOM_ERR_SYNTAX;
    }
    sscanf(p, "%u", &port);
    tmpId->lan->serverPort = (unsigned short)port;
    strncpy(tempPort, p, 9);
  }
  free(tmpLanAddress);

  //create the socket fd
  if(!strcmp(tmpId->lan->connectionType, "tcp"))
  {
    tmpId->lan->sockFD = socket(AF_INET, SOCK_STREAM, 0);
  }
  else
  {
    tmpId->lan->sockFD = socket(AF_INET, SOCK_DGRAM, 0);
  }

  if (tmpId->lan->sockFD == -1)
  {
    phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to open sock fd for communication due to \"%s\"", strerror(errno));
    RELEASE_BUFFER(tmpId->lan);
    RELEASE_BUFFER(tmpId);
    return PHCOM_ERR_NO_CONNECTION;
  }
  phLogComMessage(logger, PHLOG_TYPE_MESSAGE_4, "Socket created...");

#if 0
  //make the socket unblocking
  sockFDFlag = fcntl(tmpId->lan->sockFD, F_GETFL, 0);
  ret = fcntl(tmpId->lan->sockFD, F_SETFL, O_NONBLOCK|sockFDFlag);
  if(ret == -1)
  {
    phLogComMessage(logger, PHLOG_TYPE_FATAL, "Failed to make the sock fd non-blocking");
    RELEASE_BUFFER(tmpId->lan);
    RELEASE_BUFFER(tmpId);
    return PHCOM_ERR_NO_CONNECTION;
  }
#endif


  //call getaddrinfo() to make the sockaddr structure;
  memset(&tmpAddrinfo, 0, sizeof(tmpAddrinfo));
  tmpAddrinfo.ai_family = AF_UNSPEC;
  if(!strcmp(tmpId->lan->connectionType, "tcp"))
  {
    tmpAddrinfo.ai_socktype = SOCK_STREAM;
  }
  else
  {
    tmpAddrinfo.ai_socktype = SOCK_DGRAM;
  }

  if(getaddrinfo(tmpId->lan->serverAddr, tempPort, &tmpAddrinfo, &serverAddrinfo) < 0 )
  {
    phLogComMessage(logger, PHLOG_TYPE_FATAL,
                    "Failed to convert the address:%s:%u, give up",
                    tmpId->lan->serverAddr,
                    tmpId->lan->serverPort);
    RELEASE_BUFFER(tmpId->lan);
    RELEASE_BUFFER(tmpId);
  }
  tmpId->lan->sockAddr = *(serverAddrinfo->ai_addr);
  tmpId->lan->sockAddrLen = serverAddrinfo->ai_addrlen;
  freeaddrinfo(serverAddrinfo);

  /* for a TCP LAN connection, establish the connection */
  if(!strcmp(tmpId->lan->connectionType, "tcp"))
  {
    ret = connect(tmpId->lan->sockFD, &(tmpId->lan->sockAddr), tmpId->lan->sockAddrLen);
    if(ret < 0)
    {
      phLogComMessage(logger, PHLOG_TYPE_FATAL,
                      "Failed to connect to the server at address: %s:%u due to \"%s\", give up",
                      tmpId->lan->serverAddr,
                      tmpId->lan->serverPort,
                      strerror(errno));
      RELEASE_BUFFER(tmpId->lan);
      RELEASE_BUFFER(tmpId);
      return PHCOM_ERR_NO_CONNECTION;
    }

    phLogComMessage(logger, PHLOG_TYPE_MESSAGE_4, 
                    "Successfully connected to the server at address: %s:%u",
                    tmpId->lan->serverAddr,
                    tmpId->lan->serverPort);
  }

  tmpId->myself = tmpId;
  *newSession  = tmpId;

  phLogComMessage(logger, LOG_VERBOSE,  "phComLanOpen(): successful");

  return PHCOM_ERR_OK;
}

/*****************************************************************************
 * wrapper of phComLanOpen
 *****************************************************************************/

phComError_t phComLanOpen(phComId_t *newSession, phComMode_t mode, const char *lanAddress, phLogId_t logger)
{
    phComError_t rtnValue = _phComLanOpen(newSession, mode, lanAddress, logger);
    handleReturnValue(rtnValue);
    return rtnValue;
}


/*****************************************************************************
 *
 * Close a LAN communication link
 *
 * Authors: Xiaofei Han
 *
 * Description: Please refer to ph_mhcom.h
 *
 ***************************************************************************/
phComError_t _phComLanClose( phComId_t session )
{
  phComError_t rtnValue=PHCOM_ERR_OK;

  CheckSession(session);

  phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE, "phComLanClose(P%p)", session);

  if (session->mode == PHCOM_OFFLINE || session->lan->sockFD == -1)
  {
      /* make phComId invalid */
      session -> myself = NULL;
  }
  else
  {
    if(session->lan->sockClientFD > 0)
        close(session->lan->sockClientFD);
    if(session->lan->epollFD > 0)
        close(session->lan->epollFD);
    if (close(session->lan->sockFD) < 0)
    {
      phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL, "Failed to close the sock fd due to \"%s\"", strerror(errno));
      /* make phComId invalid */
      session -> myself = NULL;
      rtnValue = PHCOM_ERR_NO_CONNECTION;
    }
  }

  return rtnValue;
}

/*****************************************************************************
 * wrapper of phComLanClose
 *****************************************************************************/
phComError_t phComLanClose(phComId_t session)
{
    phComError_t rtnValue = _phComLanClose(session);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Send a message over the LAN
 *
 * Authors: Xiaofei Han
 *
 * Description:Please refer to ph_mhcom.h
 *
 ***************************************************************************/
char* printComment(char* msg,int len)
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
phComError_t _phComLanSend( phComId_t session,
                           char *message,
                           int length,
                           long timeout )
{
   ssize_t ret = 0;;
   struct pollfd sockFDToPoll[1];
   int retPoll = 0;

   CheckSession(session);

   phLogComMessage(session->loggerId,
                   PHLOG_TYPE_TRACE,
                   "phComGpibSend(P%p, \"%s\", %d, %ld)",
                   session, message?message:"(NULL)", length, timeout);

   if (message == NULL || session->mode == PHCOM_OFFLINE)
   {
     return PHCOM_ERR_OK;
   }

   //call poll() to determine if the socket is ready to send
   //message within a timeout
   if(session->intfc == PHCOM_IFC_LAN_SERVER)
      sockFDToPoll[0].fd = session->lan->sockClientFD;
  else
      sockFDToPoll[0].fd = session->lan->sockFD;
   sockFDToPoll[0].events = POLLOUT;
   retPoll = poll(sockFDToPoll, 1, timeout/1000);

   if(retPoll < 0)
   {
     //the poll function call failed
     phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL, "Failed to poll the socket.");
     return PHCOM_ERR_NO_CONNECTION;
   }
   else if(retPoll == 0)
   {
     //timeout occured
     phLogComMessage(session->loggerId, PHLOG_TYPE_WARNING, "Timeout sending the message over LAN: %s", message);
     return PHCOM_ERR_TIMEOUT;
   }
   else
   {
     //we are ready to send message by timeout
     phLogComMessage(session->loggerId, PHLOG_TYPE_MESSAGE_4, "%s:Ready to send message over LAN: %s !!!",getTimeString(1), printComment(message,length));
     if(!strcmp(session->lan->connectionType, "tcp"))
     {
        ret = send(session->intfc == PHCOM_IFC_LAN_SERVER ? session->lan->sockClientFD : session->lan->sockFD,
           message, length, 0);
     }
     else
     {
       ret = sendto(session->lan->sockFD,
                    message,
                    length,
                    0,
                    &(session->lan->sockAddr),
                    session->lan->sockAddrLen);
     }
   }

   if(ret == -1)
   {
     //Failed to send the message
     phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL, "Failed to send \"%s\" over LAN due to \"%s\"", message, strerror(errno));
     return PHCOM_ERR_NO_CONNECTION;
   }

   phLogComMessage(session->loggerId, PHLOG_TYPE_MESSAGE_4,"Send msg count:%d",ret);
   /* save the history */
   phLogComMessage(session->loggerId, PHLOG_TYPE_COMM_HIST,
       "(%s) T ---> E: \"%s\"", getTimeString(1), message);

   return PHCOM_ERR_OK;
}

/*****************************************************************************
 * wrapper of phComLanSend
 *****************************************************************************/
phComError_t phComLanSend(phComId_t session, char *message, int length, long timeout)
{
    phComError_t rtnValue = _phComLanSend(session, message, length, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Receive a message over the LAN
 *
 * Authors: Xiaofei Han
 *
 * Description:Please refer to ph_mhcom.h
 *
 ***************************************************************************/

phComError_t _phComLanReceive( phComId_t session,
                              const char **message,
                              int *length,
                              long timeout )
{
  static char receivedMessage[PHCOM_MAX_MESSAGE_LENGTH + 1] = "";
  struct pollfd sockFDToPoll[1];
  int retPoll = 0;
  ssize_t ret = 0;
  memset(receivedMessage, 0, PHCOM_MAX_MESSAGE_LENGTH+1);

  CheckSession(session);

  phLogComMessage(session->loggerId,
                  PHLOG_TYPE_TRACE,
                  "phComLanReceive(P%p, P%p, P%p, %ld)",
                  session, message, length, timeout);

  if (session->mode == PHCOM_OFFLINE)
  {
    return PHCOM_ERR_OK;
  }

  //call poll() to determine if the socket is ready to send
  //message within a timeout
  if(session->intfc == PHCOM_IFC_LAN_SERVER)
      sockFDToPoll[0].fd = session->lan->sockClientFD;
  else
      sockFDToPoll[0].fd = session->lan->sockFD;
  sockFDToPoll[0].events = POLLIN;
  retPoll = poll(sockFDToPoll, 1, timeout / 1000);

  if (retPoll < 0)
  {
    //the poll function call failed
    phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL, "Failed to poll the socket.");
    return PHCOM_ERR_NO_CONNECTION;
  }
  else if (retPoll == 0)
  {
    //timeout occured
    phLogComMessage(session->loggerId, PHLOG_TYPE_WARNING, "Timeout receiving message over LAN");
    return PHCOM_ERR_TIMEOUT;
  }
  else
  {
    //we are ready to receive message by timeout
    phLogComMessage(session->loggerId, PHLOG_TYPE_MESSAGE_4, "Ready to receive message over LAN");
    if (!strcmp(session->lan->connectionType, "tcp"))
    {
       ret = recv(session->intfc == PHCOM_IFC_LAN_SERVER ? session->lan->sockClientFD : session->lan->sockFD,
                receivedMessage,
                PHCOM_MAX_MESSAGE_LENGTH,
                0);
    } else
    {
      ret = recvfrom(session->lan->sockFD,
                     receivedMessage,
                     PHCOM_MAX_MESSAGE_LENGTH,
                     0,
                     NULL,
                     NULL);
    }
  }

  if (ret == -1)
  {
    //Failed to receive the message
    phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL,
        "Failed to receive the message over LAN due to \"%s\"", strerror(errno));
    return PHCOM_ERR_NO_CONNECTION;
  }
  else if (ret == 0)
  {
    //The peer socket has been closed
    phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL,
        "The server socket has been closed");
    return PHCOM_ERR_NO_CONNECTION;
  }

  receivedMessage[ret] = '\0';
  *length = ret;
  *message = receivedMessage;

  phLogComMessage(session->loggerId, PHLOG_TYPE_MESSAGE_4,"%s:message receive from LAN:%s",getTimeString(1),printComment(receivedMessage,ret));
  /* save the history */
  phLogComMessage(session->loggerId, PHLOG_TYPE_COMM_HIST,
      "(%s) T <--- E: \"%s\"", getTimeString(1), receivedMessage);

  return PHCOM_ERR_OK;
}

/*****************************************************************************
 * wrapper of phComLanReceive
 *****************************************************************************/
phComError_t phComLanReceive(phComId_t session, const char **message, int *length, long timeout)
{
    phComError_t rtnValue = _phComLanReceive(session, message, length, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Receive a message over the LAN for Pyramid
 *
 * Authors: Zoyi Yu
 *
 * Description:Please refer to ph_mhcom.h
 *
 * This function is used to get around of the issue
 * that convert unsigned char to const char.
 *
 ***************************************************************************/

phComError_t _phComLanReceiveForPyramid( phComId_t session,
                              char **message,
                              int *length,
                              long timeout )
{
  static char receivedMessage[PHCOM_MAX_MESSAGE_LENGTH + 1] = "";
  struct pollfd sockFDToPoll[1];
  int retPoll = 0;
  ssize_t ret = 0;
  memset(receivedMessage, 0, PHCOM_MAX_MESSAGE_LENGTH+1);

  CheckSession(session);

  phLogComMessage(session->loggerId,
                  PHLOG_TYPE_TRACE,
                  "phComLanReceiveForPyramid(P%p, P%p, P%p, %ld)",
                  session, message, length, timeout);

  if (session->mode == PHCOM_OFFLINE)
  {
    return PHCOM_ERR_OK;
  }

  //call poll() to determine if the socket is ready to send
  //message within a timeout
  if(session->intfc == PHCOM_IFC_LAN_SERVER)
      sockFDToPoll[0].fd = session->lan->sockClientFD;
  else
      sockFDToPoll[0].fd = session->lan->sockFD;
  sockFDToPoll[0].events = POLLIN;
  retPoll = poll(sockFDToPoll, 1, timeout / 1000);

  if (retPoll < 0)
  {
    //the poll function call failed
    phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL, "Failed to poll the socket.");
    return PHCOM_ERR_NO_CONNECTION;
  }
  else if (retPoll == 0)
  {
    //timeout occured
    phLogComMessage(session->loggerId, PHLOG_TYPE_WARNING, "Timeout receiving message over LAN");
    return PHCOM_ERR_TIMEOUT;
  }
  else
  {
    //we are ready to receive message by timeout
    phLogComMessage(session->loggerId, PHLOG_TYPE_MESSAGE_4, "Ready to receive message over LAN");
    if (!strcmp(session->lan->connectionType, "tcp"))
    {
       ret = recv(session->intfc == PHCOM_IFC_LAN_SERVER ? session->lan->sockClientFD : session->lan->sockFD,
                receivedMessage,
                PHCOM_MAX_MESSAGE_LENGTH,
                0);
    } else
    {
      ret = recvfrom(session->lan->sockFD,
                     receivedMessage,
                     PHCOM_MAX_MESSAGE_LENGTH,
                     0,
                     NULL,
                     NULL);
    }
  }

  if (ret == -1)
  {
    //Failed to receive the message
    phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL,
        "Failed to receive the message over LAN due to \"%s\"", strerror(errno));
    return PHCOM_ERR_NO_CONNECTION;
  }
  else if (ret == 0)
  {
    //The peer socket has been closed
    phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL,
        "The server socket has been closed");
    return PHCOM_ERR_NO_CONNECTION;
  }

  receivedMessage[ret] = '\0';
  *length = ret;
  *message = receivedMessage;

  phLogComMessage(session->loggerId, PHLOG_TYPE_MESSAGE_4,"%s:message receive from LAN:%s",getTimeString(1),printComment(receivedMessage,ret));
  /* save the history */
  phLogComMessage(session->loggerId, PHLOG_TYPE_COMM_HIST,
      "(%s) T <--- E: \"%s\"", getTimeString(1), receivedMessage);

  return PHCOM_ERR_OK;
}

/*****************************************************************************
 * wrapper of phComLanReceiveForPyramid
 *****************************************************************************/
phComError_t phComLanReceiveForPyramid(phComId_t session, char **message, int *length, long timeout)
{
    phComError_t rtnValue = _phComLanReceiveForPyramid(session, message, length, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}
