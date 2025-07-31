#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <arpa/inet.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<netdb.h>
/*--- defines ---------------------------------------------------------------*/
#define MAX_QUEUE 10
struct all_t
{
    int  port;
    char ip_address[64];
    char model[64];
};

/*--- global variables ------------------------------------------------------*/
static struct all_t allg;

/*--- functions ------------------------------------------------------*/
int replyRunCommand(int clientSock)
{
    char buf[128] = "";
    static int endFlag = 0;
    static int flag = 1 ;
    
    if(flag == 1)
    {
      strcpy(buf,"START RANGE 0 3\r\n");
      ++flag;
    }
    else if(flag == 2)
    {
      strcpy(buf,"\r\nSTART RANGE \"stripId123\" AA0,AA1,AA2,AA3\r\n");
      ++flag ;
    }
    else if(flag == 3)
    {
      strcpy(buf,"START RANGE \"\" AA0,AA1,AA2,AA3\r\n");
      ++flag ;
    }
    else if(flag == 4)
    {
      strcpy(buf,"START RANGE  AA0,AA1,AA2,AA3\r\n");
      ++flag ;
    }
    else if(flag == 5)
    {
        strcpy(buf,"START EXPLICIT 0,3,1");
        flag = 1;
    }
    ++endFlag;
    if(endFlag == 20)
    {
      strcpy(buf,"END_OF_LOT\r\n");
      endFlag = 0;
    }
    printf("will send msg:%s\n",buf);
    ssize_t s = send(clientSock,buf,strlen(buf),0);
    if (s < 0)
    {
      return -1;
    }
    return 0;
}

int arguments(int argc, char *argv[], struct all_t *all)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;
    while ((c = getopt(argc, argv, ":m:p:")) != -1)
    {
      switch (c) 
      { 
        case 'm':
            if (strcmp(optarg, "TFS") == 0) 
                strcpy(all->model,"TFS");
            else ++errflg;
          break;

        case 'p':
            all->port = atoi(optarg);
            break;
        default:
            ++errflg;
            break;

      }
      if (errflg) 
      {
        fprintf(stderr, "usage: %s [<options>]\n", argv[0]);
        fprintf(stderr, " -m <prober model>       handler model, one of:\n");
        fprintf(stderr, "    TFS, default TFS\n");
        fprintf(stderr, " -p server port, default 6688\n");
        return 1;
      }
    }
    return 0;
}
static char* getIp()
{
    char hostbuffer[256] = "";
    if( gethostname(hostbuffer, sizeof(hostbuffer))== -1 )
    {
        perror("gethostname");
        return NULL;
    }
    struct hostent *host_entry = gethostbyname(hostbuffer);
    if(host_entry == NULL)
    {
        perror("gethostbyname");
        return NULL;
    }
    return inet_ntoa( *((struct in_addr*) host_entry->h_addr_list[0]) ); 
}

static int mainWork(struct all_t *all)
{
  int sockServer = socket(AF_INET,SOCK_STREAM,0);
  if(sockServer <0)
  {
    perror("socket");
    return 1;
  }
  printf("create socket successfully\n");
  int opt = 1;
  if (setsockopt(sockServer, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))) 
  { 
    perror("setsockopt"); 
    close(sockServer);
    exit(EXIT_FAILURE); 
  }

  struct sockaddr_in serInfo;
  memset(&serInfo,0,sizeof(serInfo));
  int addr_len = sizeof(serInfo);
  serInfo.sin_family = AF_INET;
  serInfo.sin_port = htons(all->port);
  serInfo.sin_addr.s_addr = inet_addr(all->ip_address);
  if(bind(sockServer,(struct sockaddr*)&serInfo,(socklen_t)addr_len ) <0 )
  {
    perror("bind");
    close(sockServer);
    return 1;
  }
  printf("bind socket ,ip:%s,port:%d successful\n",inet_ntoa(serInfo.sin_addr),ntohs(serInfo.sin_port));
  if( listen(sockServer,MAX_QUEUE) < 0)
  {
    perror("listen");
    close(sockServer);
    return 1;
  }
  char str[128] = "";
  struct sockaddr_in clientInfo;
  memset(&clientInfo,0,sizeof(clientInfo));
  int client;
  while(1)
  {
    printf("Waiting connection\n");
    client= accept(sockServer,(struct sockaddr*)(&clientInfo),(socklen_t*)&addr_len);
    if(client < 0)
    {
      perror("accept");
      close(sockServer);
      return 1;
    }
    inet_ntop(AF_INET,&(clientInfo.sin_addr),str,127);
    printf("establish a link,client ip:%s,port:%d\n",str,ntohs(clientInfo.sin_port));
    char buf[128] = "";
    while(1)
    {
      ssize_t s = read(client,buf,sizeof(buf)-1);

      if(s < 0)
      {
        perror("read");
        close(client);
        break;
      }
      else if (s > 0)
      {
        buf[s] = 0;
        printf("\033[1;32m");
        printf("receive from client:%s\n",buf);
        printf("\033[0m");
        if(strstr(buf,"RUN"))
        {
          int ret = replyRunCommand(client);
          if (ret < 0)
          {
            perror("replyRunCommand");
            close(client);
            break;
          }
        }
        else
        {
          ssize_t s = write(client,"ACK",3);
          if (s < 0)
          {
            perror("write");
            close(client);
            break;
          }
        }
      }
      else
      {
        printf("client disconnected\n");
        close(client);
        break;
      }
    }
  }
  return 0;
}
int main(int argc,char* argv[])
{
  allg.port = 6688;
  char *p = getIp();
  if(p == NULL)
  {
    fprintf(stderr,"can not get local ip address,will exit\n");
    return 1;
  }
  strcpy(allg.ip_address,p);
  if( arguments(argc,argv,&allg) )
    exit(1);
  mainWork(&allg);
  return 0;
}
