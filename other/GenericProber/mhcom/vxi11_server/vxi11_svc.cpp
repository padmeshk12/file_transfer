
///////////////////////////////////////////////////////////////////////////
///
///  Copyright (c)  Advantest
///
///  author: P.Gauthier
///  Global Production Solutions 2014
///
///  This is a VXI11 server application that is used to simulate
///  an Agilent E5810 lan/gpib gateway for simulation purposes.
///
///
///
///
////////////////////////////////////////////////////////////////////////////

 

#include <iostream>
#include "vxi11.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sstream>
#include <unistd.h>
#include <string>
#include <mqueue.h>
#include    <errno.h>

#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif

using namespace std;

//struct Create_LinkParms {
//    long clientId; /* implementation specific value.*/
//    bool lockDevice; /* attempt to lock the device */
//    unsigned long lock_timeout; /* time to wait on a lock */
//    string device<>; /* name of device */
//};

//Note that \u201edevice<>\u201c is 4 byte for \u201elength\u201c followed by the string in 4byte tuples, filled with zeros. In this certain case 4 bytes \u201elength\u201c and 4 bytes for the string itself.

//The reply (server to client) seems pretty clear, too: Green shaded RPC, followed by grey shaded VXI11 specific reply (16 Bytes, 4byte tuples):

//struct Create_LinkResp {
//    Device_ErrorCode error;
//    Device_Link lid;
//    unsigned short abortPort; /* for the abort RPC */
//    unsigned long maxRecvSize; /* specifies max data size in bytes
//                                device will accept on a write */
//};


/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 0, 0};

char connection[256];
char dataout[2];
  
struct link_handle *current_link_handle=NULL;

char cmd_response[256];
char handle_value[40];
u_int handle_len;
bool pause_state = false;

Device_WriteResp  *dev_write_response = NULL;
Device_ReadResp   *dev_read_response  = NULL;
Device_Error      *dev_error_response = NULL;
Device_DocmdResp  *docmd_response     = NULL;
Create_LinkResp   *linkresponse       = NULL;
Device_SrqParms   dev_srq_parms      ;    

Device_EnableSrqParms * dev_srq_enable = NULL;
Device_ReadStbResp * dev_readstb_resp = NULL;

CLIENT *rpcClient      = (CLIENT *)NULL;
pthread_t thread_send_srq;  
pthread_t thread_wait_srq; 
pthread_t thread_check_sim; 

string command;
mqd_t to_simulator_mq;
mqd_t from_simulator_mq;
mqd_t from_simulator_srq_mq;

unsigned char status_byte;
string ip_address = "192.168.0.1";
 
void *
device_intr_srq_1(Device_SrqParms *argp, CLIENT *clnt)
{ 
	
   static char clnt_res;

   memset((char *)&clnt_res, 0, sizeof(clnt_res));
   if (clnt_call (clnt, device_intr_srq,
	   (xdrproc_t) xdr_Device_SrqParms, (caddr_t) argp,
	   (xdrproc_t) xdr_void, (caddr_t) &clnt_res,
	   TIMEOUT) != RPC_SUCCESS) { 
	   return (NULL);
   }

   return ((void *)&clnt_res);
}

void* send_srq(void*)
{ 

   //usleep(10000);
   dev_srq_parms.handle.handle_len= handle_len; 
   cout << "Sending SRQ to SICL client" << endl;
   cout << "size of handle =" << handle_len << endl; 
   if(handle_len == 0)
      return NULL;
   dev_srq_parms.handle.handle_val = handle_value ; 
   device_intr_srq_1(&dev_srq_parms,rpcClient);

}

void* wait_srq(void*)
{ 
   static char buffer[8192 + 1]; 
   ssize_t bytes_read;
   while(1) { 
      // will block here until a message is received
      bytes_read = mq_receive(from_simulator_srq_mq, buffer, 8192, NULL);
      if(bytes_read >= 0) { 
         buffer[bytes_read] = '\0'; 
         status_byte = buffer[0];
         cout << "status byte=" << status_byte << endl;
         send_srq(NULL);
      } else { 
         //protection code since this is an abnormal case let's go slow here
         cout << "waiting in wait_srq" << endl;
         sleep(1);
      }
   }   

}

void* check_simulator(void*)
{
    int ictrl = 1;
    while(ictrl != 0)
    {
        sleep(2);
        char szPopenCMD[256] = {0};
        //sprintf(szPopenCMD, "ps -ef |grep [s]imulator|awk '{print $2}'");
        sprintf(szPopenCMD, "pgrep -f simulator");

        FILE *pShellKillSim = popen(szPopenCMD, "r");
        char szResult[1024] = {0};
        if( NULL == pShellKillSim)
        {
            ictrl = 0;
            cerr << "error happened in popen(), we have to exit vxi11 server process!" << endl;
        }
        else
        {
            fgets(szResult, sizeof(szResult), pShellKillSim);
            int iPID = 0;
            iPID = atoi(szResult);
            if (0 == iPID)
            {
                ictrl = 0;
            }
            //static int iPID = -1;
            //static int it = 0;
            //it = iPID;
            //iPID = atoi(szResult);
            //if (it!=-1)
            //{
                //if( 0 == iPID  || it != iPID)
                //{
                    //ictrl = 0;
                //}
            //}
        }
        pclose(pShellKillSim);
    }
    cout << "couldn't find simulator, will exit vxi11 server process!" << endl;
    exit(0);
}

Device_WriteResp  * device_write_1_svc(Device_WriteParms * write_parms,  struct svc_req *)
{
        
    command.assign(write_parms->data.data_val,write_parms->data.data_len);
    cout << "Command:" << command << " received" << endl; 
    dev_write_response->error=0;        
    dev_write_response->size=write_parms->data.data_len;

    if(mq_send(to_simulator_mq, command.c_str(),command.size(), 0) == -1) { 
      cerr << "Message Queue Error: error sending message:" << command << endl; 
    }

    return dev_write_response;
}

Device_ReadResp  * device_read_1_svc(Device_ReadParms * read_parms,  struct svc_req *)
{

   dev_read_response->error=0; 
   dev_read_response->reason = 4; //set EOI 

   strcpy(cmd_response,"\n");

   static char buffer[8192 + 1]; 
   ssize_t bytes_read;
   while(1) { 
      // will block here until a message is received
      bytes_read = mq_receive(from_simulator_mq, buffer, 8192, NULL);
      if(bytes_read >= 0) { 
         buffer[bytes_read] = '\0';  
	 break; 
      } else { 
         //protection code since this is an abnormal case let's go slow here
         sleep(1);
      }
   }   
   dev_read_response->data.data_len = strlen(buffer);
   dev_read_response->data.data_val = buffer;

   return dev_read_response;
}

Device_Error  * destroy_link_1_svc(Device_Link * devlink,  struct svc_req *)
{
        
   dev_error_response->error=0;

   return dev_error_response;
}
 

Device_Error  * destroy_intr_chan_1_svc(void *,struct svc_req *)
{
        
   dev_error_response->error=0;

   return dev_error_response;
}

Device_Error  * device_clear_1_svc(Device_GenericParms * devgenparams,  struct svc_req *)
{
        
   dev_error_response->error=0;

   return dev_error_response;
}


Device_ReadStbResp  * device_readstb_1_svc(Device_GenericParms * devgenparams,  struct svc_req *)
{
        
   Device_ErrorCode device_errorcode;
   device_errorcode =0; 
      dev_readstb_resp->stb = status_byte;   

   dev_readstb_resp->error=device_errorcode;

   return dev_readstb_resp;
}

void * device_intr_srq_1_svc(Device_SrqParms *argp, struct svc_req *)
{
  Device_ErrorCode device_errorcode;
  device_errorcode =0;
     dev_readstb_resp->stb = status_byte;

  dev_readstb_resp->error=device_errorcode;
  return(NULL);
}

Device_DocmdResp * device_docmd_1_svc(Device_DocmdParms *cmdparms,  struct svc_req *)
{
         
   docmd_response->error = 0;

   docmd_response->data_out.data_out_len = 2;
   strcpy(dataout,"1");
   docmd_response->data_out.data_out_val = dataout;
   return docmd_response;
}

Device_Error  * create_intr_chan_1_svc(Device_RemoteFunc * devlink,  struct svc_req *)
{ 
   cout << "Host Port=" << devlink->hostPort << endl;
   cout << "Program Number=" << devlink->progNum << endl;
   cout << "Program Version=" << devlink->progVers<< endl;
   cout << "Program AddrFamily=" << devlink->progFamily<< endl;

   cout << "Program DEVICE_INTR=" << DEVICE_INTR << " Version=" << DEVICE_INTR_VERSION << endl; 

   struct sockaddr_in sa;
   char ip_str[INET_ADDRSTRLEN];
   sa.sin_addr.s_addr = ntohl(devlink->hostAddr);
   inet_ntop(AF_INET,&(sa.sin_addr), ip_str, INET_ADDRSTRLEN);

   cout << "Creating client for host=" << ip_str << endl;
   if ( (rpcClient=clnt_create(ip_address.c_str() , DEVICE_INTR,DEVICE_INTR_VERSION,"tcp")) == (CLIENT *)NULL ) { 

       (void) printf("Error creating client\n");
       clnt_pcreateerror(ip_str); 
       dev_error_response->error=6;
   } else {

       dev_error_response->error=0; 
   }


   return dev_error_response;
}

Device_Error  * device_enable_srq_1_svc(Device_EnableSrqParms * devlink,  struct svc_req *) 
{
      
   //handle_value = strndup(devlink->handle.handle_val , devlink->handle.handle_len); 
   memcpy(handle_value,devlink->handle.handle_val,devlink->handle.handle_len);
   handle_len = devlink->handle.handle_len;

   cout << "Link ID=" << devlink->lid << endl;
   cout << "SRQ Enabled=" << devlink->enable << endl;

   cout << "device_enable_srq  :handle_value="<< handle_value
        << " size=" << devlink->handle.handle_len << endl; 


   dev_error_response->error=0;

   return dev_error_response;
}

Create_LinkResp * create_link_1_svc(Create_LinkParms *linkparms, struct svc_req *)
{
         
   linkresponse->error = 0;
   linkresponse->lid = 1024;
   linkresponse->abortPort = 0;
   linkresponse->maxRecvSize = 30000;

   return linkresponse;
}

static void
device_async_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
	
   cout << "Entered device_aync_1" << endl;

   union {
	   Device_Link device_abort_1_arg;
   } argument;
   char *result;
   xdrproc_t _xdr_argument, _xdr_result;
   char *(*local)(char *, struct svc_req *);


   switch (rqstp->rq_proc) {
   case NULLPROC:
	   (void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
	   return;

   case device_abort:
	   _xdr_argument = (xdrproc_t) xdr_Device_Link;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
//		local = (char *(*)(char *, struct svc_req *)) device_abort_1_svc;
	   break;

   default:
	   svcerr_noproc (transp);
	   return;
   }

   memset ((char *)&argument, 0, sizeof (argument));
   if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
	   svcerr_decode (transp);
	   return;
   }
   result = (*local)((char *)&argument, rqstp);
   if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
	   svcerr_systemerr (transp);
   }
   if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
	   fprintf (stderr, "%s", "unable to free arguments");
	   exit (1);
   }
   return;
}




static void
device_core_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
   //cout << "Entered device_core_1" << endl;

   union {
	   Create_LinkParms create_link_1_arg;
	   Device_WriteParms device_write_1_arg;
	   Device_ReadParms device_read_1_arg;
	   Device_GenericParms device_readstb_1_arg;
	   Device_GenericParms device_trigger_1_arg;
	   Device_GenericParms device_clear_1_arg;
	   Device_GenericParms device_remote_1_arg;
	   Device_GenericParms device_local_1_arg;
	   Device_LockParms device_lock_1_arg;
	   Device_Link device_unlock_1_arg;
	   Device_EnableSrqParms device_enable_srq_1_arg;
	   Device_DocmdParms device_docmd_1_arg;
	   Device_Link destroy_link_1_arg;
	   Device_RemoteFunc create_intr_chan_1_arg;
   } argument;
   char *result;
   xdrproc_t _xdr_argument, _xdr_result;
   char *(*local)(char *, struct svc_req *);


   switch (rqstp->rq_proc) {
   case NULLPROC:
	   cout << "case NULLPROC" << endl;
	   (void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
	   return;

   case create_link:
	    cout << "create_link" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Create_LinkParms;
	   _xdr_result = (xdrproc_t) xdr_Create_LinkResp;
	   local = (char *(*)(char *, struct svc_req *)) create_link_1_svc;
	   break;

   case device_write:
	   cout << "device_write" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_WriteParms;
	   _xdr_result = (xdrproc_t) xdr_Device_WriteResp;
	   local = (char *(*)(char *, struct svc_req *)) device_write_1_svc;
	   break;

   case device_read:
	   cout << "device_read" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_ReadParms;
	   _xdr_result = (xdrproc_t) xdr_Device_ReadResp;
	   local = (char *(*)(char *, struct svc_req *)) device_read_1_svc;
	   break;

   case device_readstb:
	   cout << "device_readstb" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_GenericParms;
	   _xdr_result = (xdrproc_t) xdr_Device_ReadStbResp;
	   local = (char *(*)(char *, struct svc_req *)) device_readstb_1_svc;
	   break;

   case device_trigger:
	    cout << "device_trigger" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_GenericParms;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
//		local = (char *(*)(char *, struct svc_req *)) device_trigger_1_svc;
	   break;

   case device_clear:
	   cout << "device_clear" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_GenericParms;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
	   local = (char *(*)(char *, struct svc_req *)) device_clear_1_svc;
	   break;

   case device_remote:
	   cout << "device_remote" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_GenericParms;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
//		local = (char *(*)(char *, struct svc_req *)) device_remote_1_svc;
	   break;

   case device_local:
	   cout << "device_local" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_GenericParms;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
//		local = (char *(*)(char *, struct svc_req *)) device_local_1_svc;
	   break;

   case device_lock:
	   cout << "device_lock" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_LockParms;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
//		local = (char *(*)(char *, struct svc_req *)) device_lock_1_svc;
	   break;

   case device_unlock:
	   cout << "device_unlock" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_Link;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
//		local = (char *(*)(char *, struct svc_req *)) device_unlock_1_svc;
	   break;

   case device_enable_srq:
	   cout << "device_enable_srq" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_EnableSrqParms;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
	   local = (char *(*)(char *, struct svc_req *)) device_enable_srq_1_svc;





	   break;

   case device_docmd:
	   cout << "device_docmd" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_DocmdParms;
	   _xdr_result = (xdrproc_t) xdr_Device_DocmdResp;
	   local = (char *(*)(char *, struct svc_req *)) device_docmd_1_svc; 

	   break;

   case destroy_link:
	   cout << "destroy_link" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_Link;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
	   local = (char *(*)(char *, struct svc_req *)) destroy_link_1_svc;
	   break;

   case create_intr_chan:
	  cout << "create_intr_chan" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_RemoteFunc;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
	   local = (char *(*)(char *, struct svc_req *)) create_intr_chan_1_svc;
	   break;

   case destroy_intr_chan:
	   cout << "destroy_intr_chan" << endl;
	   _xdr_argument = (xdrproc_t) xdr_void;
	   _xdr_result = (xdrproc_t) xdr_Device_Error;
	   local = (char *(*)(char *, struct svc_req *)) destroy_intr_chan_1_svc;
	   break;

   default:
	   cout << "default" << endl;
	   svcerr_noproc (transp);
	   return;
   }


   memset ((char *)&argument, 0, sizeof (argument));
   if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
	   svcerr_decode (transp);
	   return;
   }
   result = (*local)((char *)&argument, rqstp);
   if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
	   svcerr_systemerr (transp);
   }
   if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
	   fprintf (stderr, "%s", "unable to free arguments");
	   exit (1);
   }

   //cout << "returned from device_core_1" << endl;
   return;
}

static void
device_intr_1(struct svc_req *rqstp, register SVCXPRT *transp)
{
	
   cout << "Entered device_intr_1" << endl;

   union {
	   Device_SrqParms device_intr_srq_1_arg;
   } argument;
   char *result;
   xdrproc_t _xdr_argument, _xdr_result;
   char *(*local)(char *, struct svc_req *);


   switch (rqstp->rq_proc) {
   case NULLPROC:
	   (void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
	   return;

   case device_intr_srq:
	   cout << "device_intr_srq" << endl;
	   _xdr_argument = (xdrproc_t) xdr_Device_SrqParms;
	   _xdr_result = (xdrproc_t) xdr_void;
 	   // fix CR-165836 cannot get SRQ when start remote vxi11 server 
	   local = (char *(*)(char *, struct svc_req *)) device_intr_srq_1_svc;
	   break;

   default:
	   svcerr_noproc (transp);
	   return;
   }

   memset ((char *)&argument, 0, sizeof (argument));
   if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
	   svcerr_decode (transp);
	   return;
   }
   result = (*local)((char *)&argument, rqstp);
   if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
	   svcerr_systemerr (transp);
   }
   if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
	   fprintf (stderr, "%s", "unable to free arguments");
	   exit (1);
   }
   return;
}

void cleanMq()
{
   mq_close(to_simulator_mq);
   mq_unlink("/TO_SIMULATOR"); 
   mq_close(from_simulator_mq);
   mq_unlink("/FROM_SIMULATOR");
   mq_close(from_simulator_srq_mq);
   mq_unlink("/FROM_SIMULATOR_SRQ");
}




int
main (int argc, char **argv)
{

   string argument;
   for(int i=0;i<argc;i++) { 
      argument = argv[i];
      if(argument == "-ip") { 
         if(argc == 3) {
            ip_address = argv[2];
            cout << "Listening to IP address:" << ip_address << endl;
         } else {
            cerr << "Missing ip address. Will use default IP address:" << ip_address <<  endl;
         }
      }
   }
        
   //initialization
   dev_write_response = new Device_WriteResp;
   dev_read_response  = new Device_ReadResp; 
   dev_error_response = new Device_Error;
   docmd_response     = new Device_DocmdResp;
   linkresponse       = new Create_LinkResp;
   //dev_srq_parms      = new Device_SrqParms;
   dev_readstb_resp   = new Device_ReadStbResp;

   register SVCXPRT *transp;

   pmap_unset (DEVICE_ASYNC, DEVICE_ASYNC_VERSION);
   pmap_unset (DEVICE_CORE, DEVICE_CORE_VERSION);
   pmap_unset (DEVICE_INTR, DEVICE_INTR_VERSION);

   transp = svcudp_create(RPC_ANYSOCK);
   if (transp == NULL) {
	   fprintf (stderr, "%s", "cannot create udp service.");
	   exit(1);
   }
   if (!svc_register(transp, DEVICE_ASYNC, DEVICE_ASYNC_VERSION, device_async_1, IPPROTO_UDP)) {
	   fprintf (stderr, "%s", "unable to register (DEVICE_ASYNC, DEVICE_ASYNC_VERSION, udp).");
	   exit(1);
   }
   if (!svc_register(transp, DEVICE_CORE, DEVICE_CORE_VERSION, device_core_1, IPPROTO_UDP)) {
	   fprintf (stderr, "%s", "unable to register (DEVICE_CORE, DEVICE_CORE_VERSION, udp).");
	   exit(1);
   }
   if (!svc_register(transp, DEVICE_INTR, DEVICE_INTR_VERSION, device_intr_1, IPPROTO_UDP)) {
	   fprintf (stderr, "%s", "unable to register (DEVICE_INTR, DEVICE_INTR_VERSION, udp).");
	   exit(1);
   }

   transp = svctcp_create(RPC_ANYSOCK, 0, 0);
   if (transp == NULL) {
	   fprintf (stderr, "%s", "cannot create tcp service.");
	   exit(1);
   }
   if (!svc_register(transp, DEVICE_ASYNC, DEVICE_ASYNC_VERSION, device_async_1, IPPROTO_TCP)) {
	   fprintf (stderr, "%s", "unable to register (DEVICE_ASYNC, DEVICE_ASYNC_VERSION, tcp).");
	   exit(1);
   }
   if (!svc_register(transp, DEVICE_CORE, DEVICE_CORE_VERSION, device_core_1, IPPROTO_TCP)) {
	   fprintf (stderr, "%s", "unable to register (DEVICE_CORE, DEVICE_CORE_VERSION, tcp).");
	   exit(1);
   }

   if (!svc_register(transp, DEVICE_INTR, DEVICE_INTR_VERSION, device_intr_1, IPPROTO_TCP)) {
	   fprintf (stderr, "%s", "unable to register (DEVICE_INTR, DEVICE_INTR_VERSION, tcp).");
	   exit(1);
   }

   mq_unlink("/TO_SIMULATOR");
   to_simulator_mq = mq_open("/TO_SIMULATOR", O_CREAT |O_WRONLY, 0666,NULL); 
   if(to_simulator_mq == -1) {
     cerr << "Error: cannot open message queue TO_SIMULATOR" << endl; 
     cerr << strerror(errno )<< endl;
     return -1;
   } 

   mq_unlink("/FROM_SIMULATOR");
   from_simulator_mq = mq_open("/FROM_SIMULATOR", O_CREAT | O_RDONLY, 0666, NULL); 
   if(to_simulator_mq == -1) {
     cerr << "Error: cannot open message queue FROM_SIMULATOR" << endl; 
     cerr << strerror(errno )<< endl;
     return -1;
   } 

   mq_unlink("/FROM_SIMULATOR_SRQ");
   from_simulator_srq_mq = mq_open("/FROM_SIMULATOR_SRQ", O_CREAT | O_RDONLY, 0666, NULL); 
   if(from_simulator_srq_mq == -1) {
     cerr << strerror(errno )<< endl;
     cerr << "Error: cannot open message queue FROM_SIMULATOR_SRQ" << endl; 
     return -1;
   } 

   if(pthread_create (&thread_wait_srq, NULL, &wait_srq, NULL) !=0) {  
      return 1;
   } 

   if(pthread_create (&thread_check_sim, NULL, &check_simulator, NULL) !=0) {  
      return 1;
   } 
   
   atexit(cleanMq);
   cout << "Starting VXI11 Server..." << endl;
   svc_run ();
   fprintf (stderr, "%s", "svc_run returned");
   exit (1);
	 
}

