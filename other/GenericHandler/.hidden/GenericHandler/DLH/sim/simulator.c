#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<netdb.h>
#include<signal.h>
#include<pthread.h>
#include<stdarg.h>
#include<fcntl.h>

/*--- defines ---------------------------------------------------------------*/
#define MAX_QUEUE 10
#define MESSAGE_SIZE 256
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3

#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGCYCLE    "templogcycle?"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGLENGTH   "temploglength?"
#define PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGSTS      "templogsts"

#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGCYCLE    "templogcycle"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGLENGTH   "temploglength"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSTART    "templogstart"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSTOP     "templogstop"
#define PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSEND     "templogsend"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGINI       "pretrgini"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGENABLE    "pretrgenable"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGSIGTYPE   "pretrgsigtype"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGWAITTIME  "pretrgwaittime"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTTIME   "pretrgouttime"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTVALUE  "pretrgoutvalue"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETMETHOD "pretrgretmethod"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETVALUE  "pretrgretvalue"
#define PHKEY_NAME_HANDLER_STATUS_SET_PRETRGTESTEND   "pretrgtestend"
#define PHKEY_NAME_HANDLER_STATUS_SET_SYNCTESTER      "synctester"

typedef enum 
{
  READ_ERR,
  RECV_ERR,
  TIME_OUT,
  SOCKET_CLOSE,
  SUCCESS,
  FAIL
}ERR_STATE;

typedef enum 
{
  TOCREATE,
  TOCONNECT,
  TOSEND,
  TORECV,
  WAITING,
  LOOP,
  EXIT,
}FLOW_STATE;
/* communication steps between tester and handler
 1. create a tester side service
 2. handler as a client to connect to the tester side service
 */
typedef struct TTTToolCommandParameter {
  unsigned int tempLogCycle;          // 1 to 2000
  unsigned int sizeOfLogFile;         // 10 to 600000
}TTTToolCmdParm;

struct all_t {
  int portForHandler;                 // prot number for handler as server
  int protForTester;                  // port number for tester as server
  char ipAddressForHandler[64];       // this ip for handler as server
  char ipAddressForTester[64];        // this ip for tester as server
  unsigned int numOfSites;            // define the number of sites
  char model[64];                     // to be defined
  int logLevel;
  int hasTesterStarted;
  int hasTesterRestart;
  int delayTime;
  TTTToolCmdParm tCmdParm;
};

/*--- global variables ------------------------------------------------------*/
static struct all_t gAll;
static int gLogLevel = LOG_LEVEL_DEBUG;
pthread_t clientThreadId;
pthread_t serverThreadId;
pthread_mutex_t lock;
pthread_mutex_t logLock;
int whetherToSendStartReq = 0;
pthread_cond_t threadExitCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t threadExitMutex = PTHREAD_MUTEX_INITIALIZER;

/*--- functions ------------------------------------------------------*/
void LogPrint(int level, const char *format, ...)
{
  if (level >= gAll.logLevel)
  {
    va_list args;
    va_start(args, format);

    pthread_mutex_lock(&logLock);
    switch (level) 
    {
        case LOG_LEVEL_DEBUG:
            printf("[DEBUG] - ");
            break;
        case LOG_LEVEL_INFO:
            printf("[INFO] - ");
            break;
        case LOG_LEVEL_WARNING:
            printf("[WARNING] - ");
            break;
        case LOG_LEVEL_ERROR:
            printf("[ERROR] - ");
            break;
        default:
            break;
    }
    vprintf(format, args);
    printf("\n");
    pthread_mutex_unlock(&logLock);
    va_end(args);
  }
}

void SigintHandler(int signum)
{
  LogPrint(LOG_LEVEL_INFO, "receive the signal SIGINT.");
  int ret = 0;
  LogPrint(LOG_LEVEL_INFO, "ready to cancel the local client thread.");
  ret = pthread_cancel(clientThreadId);
  if (ret != 0)
  {
    LogPrint(LOG_LEVEL_ERROR, "can't cancel the client thread.");
  }

  LogPrint(LOG_LEVEL_INFO, "ready to cancel the local server thread.");
  ret = pthread_cancel(serverThreadId);
  if (ret != 0)
  {
    LogPrint(LOG_LEVEL_ERROR, "can't cancel the server thread.");
  }

  sleep(1);
  pthread_mutex_destroy(&lock);
  pthread_mutex_destroy(&logLock);
  exit(-1);
}

void SigpipeHandler(int signum)
{
  LogPrint(LOG_LEVEL_INFO, "receive the signal SIGPIPE. It means tester restart.");

  pthread_mutex_lock(&lock);
  gAll.hasTesterRestart = 1;
  pthread_mutex_unlock(&lock);

}

void SetLogLevel(int level) {
    gLogLevel = level;
}

static char* GetIp()
{
  char hostbuffer[256] = "";
  if (gethostname(hostbuffer, sizeof(hostbuffer)) == -1) {
    LogPrint(LOG_LEVEL_ERROR, "failed to get host name: %s", strerror(errno));
    return NULL;
  }
  struct hostent *host_entry = gethostbyname(hostbuffer);
  if (host_entry == NULL) {
    LogPrint(LOG_LEVEL_ERROR, "failed to get host address: %s", strerror(errno));
    return NULL;
  }
  return inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));
}

int InitializeDefaults()
{
  gAll.numOfSites = 4;
  gAll.portForHandler = 6688;
  gAll.protForTester = 6689;
  gAll.logLevel = LOG_LEVEL_INFO;
  gAll.hasTesterStarted = 0;
  gAll.tCmdParm.tempLogCycle = 5;
  gAll.tCmdParm.sizeOfLogFile = 10;
  gAll.delayTime = 0;   // The unit of time is second.

  strcpy(gAll.ipAddressForTester, "127.0.0.1");
  char *p = GetIp();
  if (p == NULL) {
    LogPrint(LOG_LEVEL_ERROR, "can not get local ip address.");
    return -1;
  }
  strncpy(gAll.ipAddressForHandler, p, sizeof(gAll.ipAddressForHandler)-1);
  pthread_mutex_init(&lock, NULL);
  pthread_mutex_init(&logLock, NULL);

  return 0;
}

int Arguments(int argc, char *argv[], struct all_t *all)
{
  int c;
  int errflg = 0;
  extern char *optarg;
  extern int optopt;
 
  while ((c = getopt(argc, argv, ":p:i:s:d:a:")) != -1) {
    switch (c) {
    case 'p':
      all->portForHandler = atoi(optarg);
      break;
    case 'i':
      {
        char *token = strtok(optarg, ":");
        int num = 2;
        while (token != NULL && num) {
          if (num == 2)
            strncpy(all->ipAddressForTester, token, sizeof(all->ipAddressForHandler)-1);
          else
            all->protForTester = atoi(token);
          token = strtok(NULL, ":");
          --num;
        }
      }
      break;
    case 's':
      all->numOfSites = atoi(optarg);
      break;
    case 'd':
      all->logLevel = atoi(optarg);
      break;
    case 'a':
      all->delayTime = atoi(optarg);
      break;
    default:
      ++errflg;
      break;
    }
    if (errflg) {
      fprintf(stderr, "usage: %s [<options>]\n", argv[0]);
      fprintf(stderr, " -p <server port>, default 6688\n");
      fprintf(stderr, " -i <tester_ip:port> it will contect to tester server\n");
      fprintf(stderr, " -d <debug level>, default 1\n");
      fprintf(stderr, " -s <site number>, default 4. The site number range is 1 to 4. "
                      "The maximum site number is 4.\n");
      fprintf(stderr, " -a <delay time>, default 0. The parameter specifies a time period in seconds. "
                      "A StartRequest command is not sent until the time has elapsed.\n");
      fprintf(stderr, " example: ./simulator -d 1 -p 6688 -s 4 -i 192.168.0.103:6689 \n");
      return -1;
    }
  }
  return 0;
}

int ParameterCheck()
{
  if (gAll.numOfSites > 4 || gAll.numOfSites < 1) {
    LogPrint(LOG_LEVEL_ERROR, "The site number is out of range. It must be set between 1 and 4.");
    return -1;
  }
  return 0;
}

unsigned int GetSitePopulationAsInt()
{
  unsigned int sitePop = 0;

  int bitPosition = 0;
  for (bitPosition = 0; bitPosition < gAll.numOfSites; ++bitPosition) {
    sitePop |= (1 << bitPosition);
  }

  LogPrint(LOG_LEVEL_DEBUG, "get site population: %08x", sitePop);
  return sitePop;
}

void GetResponseString(char *message, char *response)
{
  memset(response, 0, MESSAGE_SIZE);
  if (strcasecmp(message, "VERSION?") == 0) {
    strcpy(response, "ACK OK VERSION? ADVANTEST HAXXXX FULLSITEB Rev.1.0.3");
  } else if (strcasecmp(message, "FULLSITES?") == 0) {
    sprintf(response, "ACK OK FULLSITES? Fullsites=%08x", GetSitePopulationAsInt());
  } else if (strcasestr(message, "BINON") != NULL) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasecmp(message, "ECHOOK") == 0) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasecmp(message, "ECHONG") == 0) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGCYCLE) == 0) {
    sprintf(response, "ACK OK %s Parameter=%d", message, gAll.tCmdParm.tempLogCycle);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGLENGTH) == 0) {
    sprintf(response, "ACK OK %s Parameter=%d", message, gAll.tCmdParm.sizeOfLogFile);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_GET_TEMPLOGSTS) == 0) {
    sprintf(response, "ACK OK %s Parameter=1", message);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGCYCLE) != NULL) {
    sprintf(response, "ACK OK %s", message);
    strtok(message, "=");
    char *parm = strtok(NULL, "=");
    gAll.tCmdParm.tempLogCycle = atoi(parm);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGLENGTH) != NULL) {
    sprintf(response, "ACK OK %s", message);
    strtok(message, "=");
    char *parm = strtok(NULL, "=");
    gAll.tCmdParm.sizeOfLogFile = atoi(parm);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSTART) == 0) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSTOP) == 0) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_SET_TEMPLOGSEND) == 0) {
    sprintf(response, "ACK OK %s Parameter=0", message);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGINI) == 0) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGENABLE) != NULL) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGSIGTYPE) != NULL) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGWAITTIME) != NULL) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTTIME) != NULL) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGOUTVALUE) != NULL) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETMETHOD) != NULL) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasestr(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGRETVALUE) != NULL) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_SET_PRETRGTESTEND) == 0) {
    sprintf(response, "ACK OK %s", message);
  } else if (strcasecmp(message, PHKEY_NAME_HANDLER_STATUS_SET_SYNCTESTER) == 0) {
    sprintf(response, "ACK OK %s", message);
  } else {
    LogPrint(LOG_LEVEL_WARNING, "server: don't support the command [%s].", message);
    sprintf(response, "The command <%s> is not available, or may not be supported yet.", message);
  }

  LogPrint(LOG_LEVEL_INFO, "server: send message [%s]", response);
  strcat(response, "\r\n");
  response[strlen(response)] = '\0';

  return;
}

ERR_STATE SendMessage(int clientSock, const char *sendInfo, char *prefix)
{
  int dataSize = send(clientSock, sendInfo, strlen(sendInfo), 0);
  // int dataSize = send(clientSock, sendInfo, strlen(sendInfo), MSG_NOSIGNAL);
  if (dataSize == 0) {
    return SOCKET_CLOSE;
  } else if (dataSize < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR){
      LogPrint(LOG_LEVEL_DEBUG, "%s: send timeout [%s]", prefix, strerror(errno));
      return TIME_OUT;
    } else {
      LogPrint(LOG_LEVEL_ERROR, "%s: failed to send [%s]", prefix, strerror(errno));
      return FAIL;
    }
  } else {
    LogPrint(LOG_LEVEL_DEBUG, "%s: sended data size is %d", prefix, dataSize);
    return SUCCESS;
  }
}

ERR_STATE RecvMessage(int clientSock, char *recvInfo, char *prefix)
{
  memset(recvInfo, 0, MESSAGE_SIZE);
  int dataSize = recv(clientSock, recvInfo, MESSAGE_SIZE - 1, 0);
  if (dataSize == 0) {
    return SOCKET_CLOSE;
  } else if (dataSize < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
      LogPrint(LOG_LEVEL_DEBUG, "%s: receive timeout [%s]", prefix, strerror(errno));
      return TIME_OUT;
    } else {
      LogPrint(LOG_LEVEL_ERROR, "%s: failed to receive [%s]", prefix, strerror(errno));
      return FAIL;
    }
  } else {
    LogPrint(LOG_LEVEL_DEBUG, "%s: received data size is %d", prefix, dataSize);
    recvInfo[dataSize] = '\0';
    return SUCCESS;
  }
}

void VerifyCommandIsComplete(char *message, char *prefix)
{
  char *index = strstr(message, "\r\n");
  if (index == NULL) {
    LogPrint(LOG_LEVEL_WARNING, "%s: received message \"%s\" is not a complete command block. "
        "The end should be delimited by string \"\\r\\n\"",prefix, message);
  } else {
    *index = '\0';
    LogPrint(LOG_LEVEL_INFO, "%s: received message [%s]",prefix, message);
  }
  return;
}

int IsResetSendStartReqFlag(const char *message)
{
  if (strstr(message, "ECHOOK")) {
    return 1;
  } else if (strstr(message, "ECHONG")) {
    return 1;
  }
  return 0;
}

int SetNonBlocking(int sockfd)
{
  int flags = -1;
  flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1)
    return -1;

  flags |= O_NONBLOCK;
  if (fcntl(sockfd, F_SETFL, flags) == -1)
    return -1;

  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
    return -1;

  return 0;
}

void* ClientWork(void *arg)
{
  pthread_detach(pthread_self());
  int ret = 0;
  int threadExitFlag = 0;
  char *sendMessage = "StartRequest\r\n";
  char recvBuf[MESSAGE_SIZE] = {0};
  char message[MESSAGE_SIZE] = {0};

  int localClientSock = -1;
  struct sockaddr_in remoteServerAddr;
  memset(&remoteServerAddr, 0, sizeof(remoteServerAddr));
  remoteServerAddr.sin_family = AF_INET;
  remoteServerAddr.sin_port = htons(gAll.protForTester);
  remoteServerAddr.sin_addr.s_addr = inet_addr(gAll.ipAddressForTester);
  LogPrint(LOG_LEVEL_INFO, "client: ready to connect to tester server...");

  ERR_STATE errState = FAIL;
  FLOW_STATE flowState = TOCONNECT;

  while(1) {
    pthread_testcancel();
    if (!gAll.hasTesterStarted) {
      LogPrint(LOG_LEVEL_DEBUG, "client: waiting to start tester.");
      flowState = TOCONNECT;
      if (localClientSock != -1) {
        LogPrint(LOG_LEVEL_INFO, "client: close client socket.");
        close(localClientSock);
        localClientSock = -1;
      }
      sleep(1);
      continue;
    }

    switch(flowState) {
    case TOCONNECT:
      localClientSock = socket(AF_INET, SOCK_STREAM, 0);
      if (localClientSock < 0) {
        LogPrint(LOG_LEVEL_ERROR, "client: failed to create local client socket: %s", strerror(errno));
        flowState = EXIT;
        continue;
      }

      if ( connect(localClientSock, (struct sockaddr*) &remoteServerAddr, sizeof(remoteServerAddr)) < 0) {
        LogPrint(LOG_LEVEL_WARNING, "client: connect error...");
        close(localClientSock);
        sleep(10);
        continue;
      }

      LogPrint(LOG_LEVEL_INFO, "client: connect tester server successfully.");
      flowState = TOSEND;

    case TOSEND: {
      /*send start request*/
      if (gAll.delayTime > 0) {
        LogPrint(LOG_LEVEL_INFO, "delay %d seconds to send StartRequest message.", gAll.delayTime);
        sleep(gAll.delayTime);
      }
      errState = SendMessage(localClientSock, sendMessage, "client");
      if (errState == SUCCESS) {
        LogPrint(LOG_LEVEL_INFO, "client: send message [StartRequest]");
        flowState = TORECV;
      } else if (errState == TIME_OUT) {
        usleep(100000);
        break;
      } else if (errState == SOCKET_CLOSE) {
        LogPrint(LOG_LEVEL_ERROR, "client: tester server has closed the socket. "
          "Please restart driver and reconnect to simulator.");
        pthread_mutex_lock(&lock);
        gAll.hasTesterStarted = 0;
        pthread_mutex_unlock(&lock);
        close(localClientSock);
        flowState = TOCONNECT;
        continue;
      } else {
        LogPrint(LOG_LEVEL_ERROR, "client: failed to send message [StartRequest], and try to reconnect server.");
        close(localClientSock);
        flowState = TOCONNECT;
        continue;
      }
    }

    case TORECV: {
      /*receive the response message*/
      errState = RecvMessage(localClientSock, recvBuf, "client");
      if (errState == SUCCESS) {
        VerifyCommandIsComplete(recvBuf, "client");

        /*verify the response message*/
        if (strcmp(recvBuf, "ACK StartRequest") != 0)
          LogPrint(LOG_LEVEL_WARNING, "response for start request is incorrect.");

        flowState = WAITING;
        pthread_mutex_lock(&lock);
        whetherToSendStartReq = 0;
        pthread_mutex_unlock(&lock);
      } else if (errState == SOCKET_CLOSE) {
        LogPrint(LOG_LEVEL_ERROR, "client: tester server has closed the socket. "
          "Please restart driver and reconnect to simulator.");
        pthread_mutex_lock(&lock);
        gAll.hasTesterStarted = 0;
        pthread_mutex_unlock(&lock);
        close(localClientSock);
        flowState = TOCONNECT;
        continue;
      } else if (errState == TIME_OUT) {
        usleep(100000);
        break;
      } else {
        LogPrint(LOG_LEVEL_ERROR, "client: failed to receive message, "
          "and try to reconnect tester server.");
        close(localClientSock);
        flowState = TOCONNECT;
        break;
      }
    }
    case WAITING: {
      if (!whetherToSendStartReq) {
        usleep(100000);
        LogPrint(LOG_LEVEL_DEBUG, "client: waiting the instruction to send start request.");
      } else {
        flowState = TOSEND;
      }
    }
    break;
    case EXIT:
      break;
    default:
      break;
    }
  }

  if (localClientSock != -1)
    close(localClientSock);

  pthread_mutex_lock(&threadExitMutex);
  pthread_cond_signal(&threadExitCond);
  pthread_mutex_unlock(&threadExitMutex);

  LogPrint(LOG_LEVEL_WARNING, "client: client thread ready to exit.");
  return NULL;
}

void* ServerWork(void* arg)
{
  pthread_detach(pthread_self());
  char sendBuf[MESSAGE_SIZE] = {0};
  char recvBuf[MESSAGE_SIZE] = {0};

  int localServerSock = -1;
  int remoteClientSock = -1;

  FLOW_STATE flowState = TOCREATE;
  switch (flowState) {
    case TOCREATE: {
      localServerSock = socket(AF_INET, SOCK_STREAM, 0);
      if (localServerSock < 0) {
        LogPrint(LOG_LEVEL_ERROR, "server: failed to create socket [%s].", strerror(errno));
        localServerSock = -1;
        break;
      }

      int opt = 1;
      if (setsockopt(localServerSock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        LogPrint(LOG_LEVEL_ERROR, "server: failed to set socketopt [%s].", strerror(errno));
        break;
      }

      struct addrinfo hints, *localServerAddr;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_flags = AI_PASSIVE;

      char serverPort[10] = {0};
      snprintf(serverPort, 9, "%d", gAll.portForHandler);
      if (getaddrinfo(NULL, serverPort, &hints, &localServerAddr) < 0) {
        LogPrint(LOG_LEVEL_ERROR, "server: failed to getaddrinfo [%s].", strerror(errno));
        freeaddrinfo(localServerAddr);
        break;
      }

      if (bind(localServerSock, localServerAddr->ai_addr, localServerAddr->ai_addrlen) < 0) {
        LogPrint(LOG_LEVEL_ERROR, "server: failed to bind [%s].", strerror(errno));
        freeaddrinfo(localServerAddr);
        break;
      }
      freeaddrinfo(localServerAddr);

      if (listen(localServerSock, MAX_QUEUE) < 0) {
        LogPrint(LOG_LEVEL_ERROR, "server: failed to listen [%s].", strerror(errno));
        break;
      }
      flowState = TOCONNECT;

      struct sockaddr_in remoteClientAddr;
      int addr_len = 0;
      ERR_STATE errState = FAIL;

      while (1)
      {
        pthread_testcancel();
        if (flowState == EXIT)
          break;

        switch (flowState) {
        case TOCONNECT: {
          LogPrint(LOG_LEVEL_INFO, "server: waiting connection from tester client...");

          memset(&remoteClientAddr, 0, sizeof(remoteClientAddr));
          remoteClientSock = accept(localServerSock, (struct sockaddr*) (&remoteClientAddr),
              (socklen_t*) &addr_len);
          if (remoteClientSock < 0) {
            LogPrint(LOG_LEVEL_ERROR, "server: failed to accept [%s].", strerror(errno));
            flowState = EXIT;
            continue;
          }

          pthread_mutex_lock(&lock);
          gAll.hasTesterStarted = 1;
          pthread_mutex_unlock(&lock);
          inet_ntop(AF_INET, &(remoteClientAddr.sin_addr), recvBuf, INET_ADDRSTRLEN);
          LogPrint(LOG_LEVEL_INFO, "server: establish a connection, and client ip:%s,port:%d", recvBuf,
              ntohs(remoteClientAddr.sin_port));
          flowState = TORECV;
        }
        case TORECV: {
          errState = RecvMessage(remoteClientSock, recvBuf, "server");
          if (errState == SUCCESS) {
            VerifyCommandIsComplete(recvBuf, "server");
            flowState = TOSEND;
          } else if (errState == TIME_OUT) {
            usleep(100000);
            break;
          } else if (errState == SOCKET_CLOSE) {
            LogPrint(LOG_LEVEL_ERROR, "server: tester client has closed the socket. "
              "Please restart driver and reconnect to simulator.");
            pthread_mutex_lock(&lock);
            gAll.hasTesterStarted = 0;
            pthread_mutex_unlock(&lock);
            close(remoteClientSock);
            flowState = TOCONNECT;
            continue;
          } else {
            LogPrint(LOG_LEVEL_ERROR, "server: failed to receive message, and try to reestablish a new connection with tester client.");
            close(remoteClientSock);
            flowState = TOCONNECT;
            continue;
          }
        }
          
        case TOSEND: {
          GetResponseString(recvBuf, sendBuf);
          errState = SendMessage(remoteClientSock, sendBuf, "server");
          if (errState == SUCCESS) {
            if (IsResetSendStartReqFlag(recvBuf))
            {
              pthread_mutex_lock(&lock);
              whetherToSendStartReq = 1;
              pthread_mutex_unlock(&lock);
            }
            flowState = TORECV;
          } else if (errState == TIME_OUT) {
            usleep(100000);
            break;
          } else if (errState == SOCKET_CLOSE) {
            LogPrint(LOG_LEVEL_ERROR, "server: tester client has closed the socket. "
              "Please restart driver and reconnect to simulator.");
            pthread_mutex_lock(&lock);
            gAll.hasTesterStarted = 0;
            pthread_mutex_unlock(&lock);
            close(remoteClientSock);
            flowState = TOCONNECT;
            continue;
          } else {
            LogPrint(LOG_LEVEL_ERROR, "server: failed to send message, and try to reconnect client.");
            close(remoteClientSock);
            flowState = TOCONNECT;
            break;
          }
        }
        default:
          break;
        }
      }
    }
    break;
    default:
      break;
  }

  if (localServerSock != -1)
    close(localServerSock);

  pthread_mutex_lock(&threadExitMutex);
  pthread_cond_signal(&threadExitCond);
  pthread_mutex_unlock(&threadExitMutex);

  LogPrint(LOG_LEVEL_DEBUG, "server: thread ready to exit.");
  return NULL;
}

static int MainWork(struct all_t *all)
{
  int ret = 0;
  ret = pthread_create(&clientThreadId, NULL, ClientWork, NULL);
  if (ret != 0) {
    LogPrint(LOG_LEVEL_ERROR, "failed to create client thread. error message:%s", strerror(ret));
    return ret;
  }

  ret = pthread_create(&serverThreadId, NULL, ServerWork, NULL);
  if (ret != 0) {
    LogPrint(LOG_LEVEL_ERROR, "failed to create server thread. error message:%s", strerror(ret));
    return ret;
  }

  LogPrint(LOG_LEVEL_DEBUG, "start monitoring client and server thread.");
  pthread_mutex_lock(&threadExitMutex);
  pthread_cond_wait(&threadExitCond, &threadExitMutex);
  pthread_mutex_unlock(&threadExitMutex);

  if (pthread_kill(clientThreadId, 0) == 0) {
    if (pthread_cancel(clientThreadId) != 0) {
      LogPrint(LOG_LEVEL_WARNING, "fialed to cancel the client thread.");
      ret = -1;
    } else {
      LogPrint(LOG_LEVEL_INFO, "client thread exit.");
    }
  }

  if (pthread_kill(serverThreadId, 0) == 0) {
    if (pthread_cancel(serverThreadId) != 0) {
      LogPrint(LOG_LEVEL_WARNING, "failed to cancel the server thread.");
      ret = -1;
    } else {
      LogPrint(LOG_LEVEL_INFO, "server thread exit.");
    }
  }
  
  return ret;
}

int main(int argc, char *argv[])
{
  int ret = 0;
  signal(SIGINT, SigintHandler);

  ret = InitializeDefaults();
  if (ret < 0)
    return -1;

  if (Arguments(argc, argv, &gAll))
    return -1;

  if (ParameterCheck() < 0)
    return -1;

  MainWork(&gAll);
  pthread_mutex_destroy(&lock);
  pthread_mutex_destroy(&logLock);
  return 0;
}
