/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : gpibComm.C
 * CREATED  : 17 Jan 2001
 *
 * CONTENTS : Helper classes for prober handler simulators
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

#if __GNUC__ >= 3
#include <iostream>
#include    <string.h>
#include    <stdio.h>
#endif

/*--- module includes -------------------------------------------------------*/

#include "gpibComm.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

#include <mqueue.h>

//////////////////////////////  Interface with VXI11 server //////////////////////

mqd_t from_vxi11_server_mq;
mqd_t to_vxi11_server_mq;
mqd_t to_vxi11_server_srq_mq;


phComError_t phComGpibOpenVXI11(
    phComId_t *newSession       /* The new session ID (result value). Will
				   be set to NULL, if an error occured */,
    phComMode_t mode            /* Commun mode: online|offline|simulation  */,
    char *device                /* Symbolic Name of GPIB card in workstation*/,
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




/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/



/*--- class implementations -------------------------------------------------*/

/*--- Gpib Communication Exceptions -----------------------------------------*/

CommErrorException::CommErrorException(phComError_t e) : comError(e)
{
}

void CommErrorException::printMessage()
{
    Std::cerr << "Gpib Communication: ";

    switch (comError)
    {
      case PHCOM_ERR_OK:
        Std::cerr << "no error returned from ph_mhcom layer";
        break;
      case PHCOM_ERR_UNKNOWN_DEVICE:
        Std::cerr << "unknown device";
        break;
      case PHCOM_ERR_NO_CONNECTION:
        Std::cerr << "no connection";
        break;
      case PHCOM_ERR_GPIB_BAD_ADDRESS:
        Std::cerr << "bad address";
        break;
      case PHCOM_ERR_GPIB_INVALID_INST:
        Std::cerr << "invalid initialization";
        break;
      case PHCOM_ERR_TIMEOUT:
        Std::cerr << "time out";
        break;
      case PHCOM_ERR_SYNTAX:
        Std::cerr << "syntax error";
        break;
      case PHCOM_ERR_VERSION:
        Std::cerr << "version error";
        break;
      case PHCOM_ERR_NO_GPIB_SESSION:
        Std::cerr << "no gpib session";
        break;
      default:
        Std::cerr << "error number " << comError << " returned from mhcom layer";
        break;
    }   
    Std::cerr << "\n";
}



/*--- Gpib communication ----------------------------------------------------*/

GpibCommunication::GpibCommunication(phLogId_t l, const GpibCommSetup &setup) : 
    LogObject(l)
{
    phComError_t phCommError;

    phCommError=phComGpibOpenVXI11(&comId, PHCOM_ONLINE, 
                            setup.hpibDevice, 0, l, PHCOM_GPIB_EOMA_NONE);

    if ( convertPhCommErrorIntoCommError(phCommError) != comm_ok )
    {
        throw CommErrorException(phCommError);
    }
}

GpibCommunication::~GpibCommunication()
{
    phComGpibCloseVXI11(comId);
}


GpibCommunication::commError GpibCommunication::sendEvent(phComGpibEvent_t *event, long timeout)
{
    phComError_t phCommError;

    phCommError = phComGpibSendEventVXI11(comId, event, timeout);

    return convertPhCommErrorIntoCommError(phCommError);
}

GpibCommunication::commError GpibCommunication::send(char *h_string, int length, long timeout)
{
    phComError_t phCommError;

    phCommError = phComGpibSendVXI11(comId, h_string, length, timeout);

    return convertPhCommErrorIntoCommError(phCommError);
}


GpibCommunication::commError GpibCommunication::receive(const char **message, int *length, long timeout)
{
    phComError_t phCommError;

    phCommError = phComGpibReceiveVXI11(comId, message, length, timeout);

    return convertPhCommErrorIntoCommError(phCommError);
}


GpibCommunication::commError GpibCommunication::convertPhCommErrorIntoCommError(phComError_t e)
{
    commError cError;

    switch ( e )
    {
      case PHCOM_ERR_OK:
        cError=comm_ok;
        break;
      case PHCOM_ERR_TIMEOUT:
        cError=comm_timeout;
        break;
      default:
        cError=comm_error;
        break;
    }

    return cError;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
