/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.c
 * CREATED  : 15 Feb 2000
 *
 * CONTENTS : TSK prober simulator
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *          : Jeff Morton, STE-ADC, 
 *          : Jiawei Lin, R&D Shanghai, CR 27092 &CR25172 
 *          : David Pei, R&D Shanghai, CR 27997
 *          : Wei Pan, R&D Shanghai, CR 29557
 *          : Garry Xie, R&D Shanghai, CR28427
 *          : Kun Xiao, R&D Shanghai, CR21589
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Feb 2000, Michael Vogt, created
 * REVISED  : 22 Feb 2000, Jeff Morton
 *          : August 2005, Jiawei Lin, CR27092 &CR25172
 *              Implement the "ur" command to support the information query
 *              from Tester. Such information contains WCR (Wafer Configuration
 *              Record) of STDF, like Wafer Size, Wafer Unit.
 *              Also we support Lot Number, Wafer ID, Wafer Number, Cassette
 *              Status, Chuck Temperature. All these information could be
 *              retrieved by Tester with the PROB_HND_CALL "ph_get_status"
 *          : 21 Dec 2005, Wei Pan, CR 29557 supporting UF3000
 *          : February 2006, David Pei, CR27997
 *              Add item "I" to simulator option
 *              If I=1 ,ignore the STB 100 after binning die.
 *          : February 2006, Garry Xie , CR28427
 *              (1)add  four commands to support the query.These commands are listed below:
 *              "E"       --error code request
 *              "H"       --Multi-site location NO request response
 *              "w"       --wafer status request
 *              (2)add 3 commands to set value to the prober.These commands are listed below:
 *              "em"         --set alarm buzzer
 *          : July 2006, Kun Xiao, CR21589              
 *              Implement "Z+-" command to move chuck by step size
 *              Implement "z" command to set contact height
 *
 *
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*--- module includes -------------------------------------------------------*/
#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_GuiServer.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#define EOC "\r\n"
#define LOG_NORMAL  PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG   PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_2
#define WAF_DIM 3

#define CASS_COUNT 2

/*CR21589, Kun Xiao, 4 Jul 2006*/
#define Z_LIMIT 1000
#define Z_MIN 0

int wafercount = 6;
enum model_t
{
    MOD_APM90A, MOD_UF200, MOD_UF300, MOD_UF3000
};

typedef enum
{
    PROBER_IS_MASTER,
    SMARTEST_IS_MASTER
} indexMode_t;

struct all_t
{
    /* ids */
    phLogId_t        logId;
    phComId_t        comId;
    phGuiProcessId_t guiId;

    /* setup */
    int              debugLevel;
    char             *hpibDevice;
    int              timeout;
    char             *modelname;
    enum model_t     model;
    int              numSites;
    int              numSteps;
    int              numBinChars;
    int              master;
    int              autoStartTime;
    int              waferID_is_b;
    int              manualUnloadWafer;

    /* init */
    int              done;
    int              wafercount;
    int              cassettecount;
    int              abortProbing;
    int              firstContactWithProber;
    int              firstWaferInCass;
    int              initDone;

    /* lots */
    int              lotActive;
    long             lotDoneTime;

    /* wafer */
    int              waferBinTable[31];
    int              currDieIndx; 
    int              xref;
    int              yref;
    int              lastDie;
    int              chuckUp;
    int              loaded;
    int              markingDone;
    int              zlimit; /*CR21589, Kun Xiao, 4 Jul 2006*/
    int              zmin;  /*CR21589, Kun Xiao, 4 Jul 2006*/
    int              zcurrent; /*CR21589, Kun Xiao, 4 Jul 2006*/
    int              zcontact; /*contact point, CR21589, Kun Xiao, 4 Jul 2006*/

    /* pause */
    int              stopButtonPressed;
    int              startButtonPressed;
    int              sequenceBackPressed;
    int              lotEndPressed;
    int              proberHasStopped;

    /* faults */

    /* runtime */
    const char       *message;
    int              length;
    phComGpibEvent_t *comEvent;
    phGuiEvent_t     guiEvent;
    /* Associated with the option 'I'. ignore sending test done STB when testDoneIgnored == YES */
    int              testDoneIgnored;
    char             waferIdPrefix[128];
};

struct all_t allg;

/*--- global variables ------------------------------------------------------*/

static const char *slaveGuiDesc = " \
    S:`name`:`TSK prober simulator` \
    {@v \
        S::``{@v \
            S::{@h  f:`f_req`:`Request`:    p:`p_req`:`Receive` } \
            S::{@h  f:`f_answ`:`Answer`:    p:`p_answ`:`Send` } \
            S::{@h  f:`f_srq`:`SRQ`:        p:`p_srq`:`Send` } \
            S::{@h  f:`f_event`:`Events`:   p:`p_event`:`Clear` } \
            S::{@h  f:`f_status`:`Status`:  p:`p_quit`:`Quit` } \
            S::{@h                          p:`p_start`:`Start` } \
            S::{@h                          p:`p_stop`:`Stop` } \
            S::{@h                          p:`p_back`:`Seq Back` } \
            S::{@h                          p:`p_lotend`:`Lot end` } \
            S::{@h                          p:`p_flush`:`Flush` } \
            S::{@h  f:`f_delay`:`Delay [seconds]`:  * \
                    R::{`r_life_1`:`` `r_life_2`:``}:2:1 * } \
        } \
    } \
";


/* Note: format {x,y,onWafInfo}, onWafer = [0-15] for sites 4321  */
static int stepPat1Site[21][3] = {
    {0,1,1}, {0,2,1}, {0,3,1}, 
    {1,0,1}, {1,1,1}, {1,2,1}, {1,3,1}, {1,4,1},
    {2,0,1}, {2,1,1}, {2,2,1}, {2,3,1}, {2,4,1},
    {3,0,1}, {3,1,1}, {3,2,1}, {3,3,1}, {3,4,1},
    {4,1,1}, {4,2,1}, {4,3,1} 
};

/* Note: format {x,y,onWafInfo}, onWafer = [0-15] for sites 4321  */
static int stepPat2SiteP[12][3] = {
    {0,1,3}, {0,2,3}, {0,3,3}, 
    {1,0,3}, {1,1,3}, 
    {2,0,3},                   {2,3,3}, {2,4,1},
    {3,0,3},          {3,2,3}, {3,3,1}, 
    {4,2,1}
};

/* Note: format {x,y,onWafInfo}, onWafer = [0-15] for sites 4321  */
static int stepPat2SiteST[12][3] = {
    {0,1,3}, {0,2,3}, {0,3,3}, 
    {1,0,3}, {1,1,3},                         
    {2,0,3},                   {2,3,3}, {2,4,1},
    {3,0,3},          {3,2,3}, {3,3,1},            
    {4,2,1}         
};

#if 0
/* use array below to always treport both sites */
/* Note: format {x,y,onWafInfo}, onWafer = [0-15] for sites 4321  */
static int stepPat2SiteST[12][3] = {
    {0,1,3}, {0,2,3}, {0,3,3}, 
    {1,0,3}, {1,1,3},                         
    {2,0,3},                   {2,3,3}, {2,4,3},
    {3,0,3},          {3,2,3}, {3,3,3},            
    {4,2,3}         
};
#endif

/* Note: format {x,y,onWafInfo}, onWafer = [0-15] for sites 4321  */
static int stepPat4Site[9][3] = {
    {0,1,15}, {0,2,15}, {0,3,15}, 
    {1,0,15}, {1,1,15},                               
    {2,0,15},                                       
    {3,0,15},                                        
    {4,1,15}, {4,2,15}         
};


static int stepPat8Site[7][3] = {
    {-3,0,15}, {-2,-1,15}, {-1,-2,15}, 
    {0,-3,15}, {1,-2,15}, {2,-1,15},{3,0,15}                              
};


/* Note: format {x,y,onWafInfo}, onWafer = 255 for sites 4321,8765,1211109...256255254253  */
static int stepPat256Site[10][3] = {
    {0,1,255}, {0,2,255}, {0,3,255},
    {1,0,255}, {1,1,255},
    {2,0,255},
    {3,0,255},
    {4,1,255}, {4,2,255}, {4,3,255}
};

/* Note: format {x,y,onWafInfo}, onWafer = 1023 for sites 4321,8765,1211109...2562552542531024102310221021  */
static int stepPat1024Site[31][3] = {
    {0,1,1023}, {0,2,1023}, {0,3,1023},{0,4,1023}, {0,5,1023}, {0,6,1023},{0,7,1023},
    {1,0,1023}, {1,1,1023},{1,2,1023}, {1,3,1023},{1,4,1023}, {1,5,1023},
    {2,0,1023},{2,1,1023},{2,2,1023},{2,3,1023},{2,4,1023},{2,5,1023},
    {3,0,1023},{3,1,1023},{3,2,1023},{3,3,1023},{3,4,1023},{3,5,1023},
    {4,1,1023}, {4,2,1023},{4,3,1023},  {4,4,1023}, {4,5,1023},{4,6,1023}
};

static int stepPat1536Site[31][3] = {
    {0,1,1535}, {0,2,1535}, {0,3,1535},{0,4,1535}, {0,5,1535}, {0,6,1535},{0,7,1535},
    {1,0,1535}, {1,1,1535},{1,2,1535}, {1,3,1535},{1,4,1535}, {1,5,1535},
    {2,0,1535},{2,1,1535},{2,2,1535},{2,3,1535},{2,4,1535},{2,5,1535},
    {3,0,1535},{3,1,1535},{3,2,1535},{3,3,1535},{3,4,1535},{3,5,1535},
    {4,1,1535}, {4,2,1535},{4,3,1535},  {4,4,1535}, {4,5,1535},{4,6,1535}
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

void getVxi11Path(char *szpath, int lenth)
{
    char *szEnv = getenv("WORKSPACE");
    if(NULL == szEnv)
    {
        snprintf(szpath, lenth, "/opt/hp93000/testcell/phcontrol/drivers/Generic_93K_Driver/tools/bin/vxi11_server");
    }
    else
    {
        snprintf(szpath, lenth, "%s/ph-development/development/drivers/Generic_93K_Driver/common/mhcom/bin/vxi11_server", szEnv);
    }
}

static int forceUnloadWafer = 0;

/*--- functions -------------------------------------------------------------*/
static long timeSinceLotDone(struct all_t *all);

/******************************************************************************/
/* FUNCTION: localDelay_ms()                                                  */
/* DESCRIPTION:                                                               */
/*   Delay interval = milliseconds                                            */
/******************************************************************************/
static void localDelay_ms(int milliseconds) {
    int seconds;
    int microseconds;
    struct timeval delay;

    /* ms must be converted to secs and microsecs for use by select(2) */
    if ( milliseconds > 999 )
    {
        microseconds = milliseconds % 1000;
        seconds = (milliseconds - microseconds) / 1000;
        microseconds *= 1000;
    }
    else
    {
        seconds = 0;
        microseconds = milliseconds * 1000;
    }

    delay.tv_sec = seconds; 
    delay.tv_usec = microseconds; 
    select(0, NULL, NULL, NULL, &delay);
}

/*****************************************************************************
 * sigHandler()
 *****************************************************************************/
void sigHandler(int sig)
{
    phGuiDestroyDialog(allg.guiId);
    phComGpibCloseVXI11(allg.comId);
    phLogDestroy(allg.logId);
    exit (1);
}

/*****************************************************************************
 * checkGuiError()
 *****************************************************************************/
static int checkGuiError(phGuiError_t guiError, struct all_t *all)
{
    switch ( guiError )
    {
        case PH_GUI_ERR_OK:
            break;
        case PH_GUI_ERR_TIMEOUT:
#if 0
            phLogSimMessage(all->logId, LOG_VERBOSE,
                            "received timeout error from gui server");
#endif 
            break;
        default:
            phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
                            "received error code %d from gui server", 
                            (int) guiError);
            return 0;
    }
    return 1;
}

/*****************************************************************************
 * checkComError()
 *****************************************************************************/
static int checkComError(phComError_t comError, struct all_t *all)
{
    switch ( comError )
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

/*****************************************************************************
 * heartbeat()  
 *****************************************************************************/
static void heartbeat(struct all_t *all)
{
    static int b = 1;

    if ( b == 1 )
    {
        b = 2;
        checkGuiError(phGuiChangeValue(all->guiId, "r_life_1", "False"), all);
        checkGuiError(phGuiChangeValue(all->guiId, "r_life_2", "True"), all);
    }
    else
    {
        b = 1;
        checkGuiError(phGuiChangeValue(all->guiId, "r_life_2", "False"), all);
        checkGuiError(phGuiChangeValue(all->guiId, "r_life_1", "True"), all);
    }
}

/*****************************************************************************
 * setWaiting()
 *****************************************************************************/
static void setWaiting(int wait, struct all_t *all)
{
    if ( wait )
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "waiting ..."), all);
    else
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "OK"), all);
}

/*****************************************************************************
 * setTimeout()
 *****************************************************************************/
static void setTimeout(int timeout, struct all_t *all)
{
    if ( timeout )
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "timed out"), all);
    else
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "OK"), all);
}

/*****************************************************************************
 * setSent()
 *****************************************************************************/
static void setSent(int sent, struct all_t *all)
{
    if ( sent )
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "sent"), all);
    else
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "not sent"), all);
}

/*****************************************************************************
 * setError()
 *****************************************************************************/
static void setError(int error, struct all_t *all)
{
    if ( error )
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "error, see log"), all);
    else
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "OK"), all);
}

/*****************************************************************************
 * setNotUnderstood()
 *****************************************************************************/
static void setNotUnderstood(int error, struct all_t *all)
{
    if ( error )
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "not understood"), all);
    else
        checkGuiError(phGuiChangeValue(all->guiId, "f_status", "OK"), all);
}

/*****************************************************************************
 * doDelay()
 *****************************************************************************/
static void doDelay(int seconds, struct all_t *all)
{
    char delay[64];

    while ( seconds > 0 )
    {
        sprintf(delay, "%d", seconds);
        checkGuiError(phGuiChangeValue(all->guiId, "f_delay", delay), all);
        seconds--;
        sleep(1);
    }
    checkGuiError(phGuiChangeValue(all->guiId, "f_delay", ""), all);
}

#if 0
/* comment for 'defined but not used' */
/*****************************************************************************
 * notSupported()
 *****************************************************************************/
static void notSupported(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
                    "message \"%s\" not supported by this prober simulator", m);
}
#endif

/*****************************************************************************
 * sendSrq()   
 *****************************************************************************/
static void sendSrq(struct all_t *all, unsigned char srq)
{
    char message[1024];
    phComGpibEvent_t sendEvent;
    phGuiError_t guiError;
    phComError_t comError;
    int sent = 0;

    sprintf(message, "0x%02x", (int) srq);
    phLogSimMessage(all->logId, LOG_NORMAL, "$$$ send SRQ:%s",message);
    guiError = phGuiChangeValue(all->guiId, "f_srq", message);
    checkGuiError(guiError, all);

    sendEvent.d.srq_byte = srq;
    sendEvent.type = PHCOM_GPIB_ET_SRQ;

    do
    {
        setWaiting(1, all);
        comError = phComGpibSendEventVXI11(all->comId, &sendEvent, 
                                      1000L * all->timeout);
        setWaiting(0, all);
        switch ( checkComError(comError, all) )
        {
            case 0:
                /* OK */
                sent = 1;
                break;
            case 1:
                setTimeout(1, all);
                doDelay(5, all);
                break;
            case 2:
            default:
                setError(1, all);
                break;
        }
    } while ( !sent );

#if 0
    phLogSimMessage(all->logId, LOG_DEBUG, 
                    "sent SRQ \"%s\"", message);    
#endif
}

/*****************************************************************************
 * sendMessage()
 *****************************************************************************/
static void sendMessage(struct all_t *all, char *m)
{
    char message[2048];
    int sent = 0;
    phGuiError_t guiError;
    phComError_t comError;

    strncpy(message, m, sizeof(message) - 1);
   
    guiError = phGuiChangeValue(all->guiId, "f_answ", message);
    checkGuiError(guiError, all);

    strcat(message, EOC);
#if 0
    phLogSimMessage(all->logId, LOG_VERBOSE, 
                    "trying to send message \"%s\"", m);
#endif

    do 
    {
        setWaiting(1, all);
        comError = phComGpibSendVXI11(all->comId, message, strlen(message), 
                                 1000L * all->timeout);
        setWaiting(0, all);
        switch ( checkComError(comError, all) )
        {
            case 0: /* OK */
                sent = 1;
                break;
            case 1:
                setTimeout(1, all);
                doDelay(5, all);
                break;
            case 2:
            default:
                setError(1, all);
                break;
        }
    } while ( !sent );
     phLogSimMessage(all->logId, LOG_NORMAL, "message sent: \"%s\"", m);
    setSent(1, all);
#if 0
    phLogSimMessage(all->logId, LOG_DEBUG, 
                    "message \"%s\" sent", m);
#endif
}

/*****************************************************************************
 * phPbGuiGetEvent()
 *****************************************************************************/
phGuiError_t phPbGuiGetEvent(
                            struct all_t *all,
                            phGuiProcessId_t guiProcessID, /* the Gui-ID */
                            phGuiEvent_t *event,           /* the variable where the event type and
                                                              the triggering buttonname will be stored */
                            int timeout                    /* timeout after which phGuiGetEvent returns */
                            )
{
    phGuiError_t retVal;

    if ( all->autoStartTime > 0 && !all->lotActive )
    {
        if ( all->autoStartTime < timeSinceLotDone(all) )
        {
            event->type = PH_GUI_PUSHBUTTON_EVENT;
            strcpy(event->name, "p_start");
            event->secval = 0;
            all->lotActive = 1;
            return PH_GUI_ERR_OK;
        }
    }

    retVal = phGuiGetEvent(guiProcessID, event, timeout);

    return retVal;
}


/*****************************************************************************
 * chuckUp()
 *****************************************************************************/
static int chuckUp(struct all_t *all, int up)
{
    if ( up )
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

/*****************************************************************************
 * newWafer()         
 *****************************************************************************/
static int newWafer(struct all_t *all)
{
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
    int i/*, x, y*/;
/* End of Huatek Modification */

    if ( all->wafercount != wafercount )
    {
        ++ all->wafercount;
        for ( i=0; i< all->numSteps; i++ )
            all->waferBinTable[i] = -1;

        all->lastDie = 0;
        all->loaded = 1;
        all->currDieIndx = 0;

        phLogSimMessage(all->logId, LOG_NORMAL, "wafer initialized");
        return 1;

    }
    else
    {
        phLogSimMessage(all->logId, LOG_NORMAL, 
                        "wafer NOT initialized, no more wafers");
        return 0;   
    }
}

/*****************************************************************************
 * unloadWafer()
 *****************************************************************************/
static int unloadWafer(struct all_t *all, int sendLoadSRQ)
{


    sendSrq(all, 0x50);
    localDelay_ms(500);
    all->loaded = 0;    
    //all->wafercount--;

    phLogSimMessage(all->logId, LOG_DEBUG, "wafer unloaded");

    if(sendLoadSRQ == 1)
    {
      localDelay_ms(1000);
      sendSrq(all, 0x46);
      forceUnloadWafer = 1;
    }

    return all->wafercount == wafercount ? 0 : 1;
}

static void setLotDoneTime(struct all_t *all)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    all->lotDoneTime = now.tv_sec;
}

static long timeSinceLotDone(struct all_t *all)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    return(now.tv_sec - all->lotDoneTime);
}

/*****************************************************************************
 * comeOutOfPause()  
 *****************************************************************************/
static void comeOutOfPause(struct all_t *all)
{

    phLogSimMessage(all->logId, LOG_DEBUG, "comeOutOfPause()");

    all->proberHasStopped = 0;
    all->startButtonPressed = 0;
    all->stopButtonPressed = 0;

    localDelay_ms(100);
    sendSrq(all, 0x5b);    /* 91 = send START SRQ to tester now */
    localDelay_ms(100);
    sendSrq(all, 0x43);    /* 67 = Z up complete */
}


/*****************************************************************************
 * autoIndex()  
 *****************************************************************************/
static int autoIndex(struct all_t *all)
{
    int stepDone = 1;

    localDelay_ms(100);

    /* send STOP SRQ instead of normal index complete SRQ if stop button pressed */
    if ( all->stopButtonPressed )
    {
        sendSrq(all, 0x5a);    /* 90 = send STOP SRQ to tester now */
        all->proberHasStopped = 1;    /* prober is now stopped */
        stepDone = 0;
    }

    all->markingDone = 0; 
    all->currDieIndx++;

    if ( all->sequenceBackPressed )
    {
        chuckUp(all, 0);
        sendSrq(all, 0x46);    /* send sequence back SRQ to tester now */
        all->currDieIndx=0;
        all->sequenceBackPressed = 0;
        phLogSimMessage(all->logId, LOG_NORMAL, "sequence back accepted");
        stepDone = 0;
    }

    if ( all->lotEndPressed )
    {
        chuckUp(all, 0);
        sendSrq(all, 0x52);    /* send cassette end SRQ to tester now */
        sleep(1);
        sendSrq(all, 0x5e);    /* send cassette end SRQ to tester now */
        all->currDieIndx=0;
        all->lotEndPressed = 0;
        all->wafercount = 0;
        all->cassettecount = 0;
        all->initDone = 0;
        phLogSimMessage(all->logId, LOG_NORMAL, "lot end accepted");
        stepDone = 0;
    }

    phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): currDieIndx incremented =%d",all->currDieIndx);

    if ( all->currDieIndx +1 == all->numSteps )
    {
        all->lastDie = 1;
        phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): currDieIndx=numSteps");
    }

    /* log die coords */
    switch ( all->numSites )
    {
        case 1:
            phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): new die [%d,%d]",
                            stepPat1Site[all->currDieIndx][0], stepPat1Site[all->currDieIndx][1]);
            break;
        case 2:
            phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): new die [%d,%d]",
                            stepPat2SiteP[all->currDieIndx][0], stepPat2SiteP[all->currDieIndx][1]);
            break;
        case 4:
            phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): new die [%d,%d]",
                            stepPat4Site[all->currDieIndx][0], stepPat4Site[all->currDieIndx][1]);
            break;
        case 8:
            phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): new die [%d,%d]",
                            stepPat8Site[all->currDieIndx][0], stepPat8Site[all->currDieIndx][1]);
            break;
        case 256:
            phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): new die [%d,%d]",
                            stepPat256Site[all->currDieIndx][0], stepPat256Site[all->currDieIndx][1]);
            break;
        case 1024:
            phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): new die [%d,%d]",
                            stepPat1024Site[all->currDieIndx][0], stepPat1024Site[all->currDieIndx][1]);
            break;
        case 1536:
            phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): new die [%d,%d]",
                            stepPat1536Site[all->currDieIndx][0], stepPat1536Site[all->currDieIndx][1]);
            break;
        default:
             phLogSimMessage(all->logId, LOG_NORMAL, "autoIndex(): new die [%d,%d]",
                            stepPat1024Site[all->currDieIndx][0], stepPat1024Site[all->currDieIndx][1]);
            break;
    }

    if ( stepDone && !forceUnloadWafer)
    {
       // sleep(1);
        sendSrq(all, 0x43);    /* 67 = index to next die with auto Z complete */
    }

    return 0;
}


#if 0
/*****************************************************************************
 * setDie()            
 *****************************************************************************/
static int setDie(struct all_t *all, int x, int y)
{
    all->xcurrent = x;
    all->ycurrent = y;
    if ( x >= 0 && x < WAF_DIM && y >= 0 && y < WAF_DIM )
    {
        phLogSimMessage(all->logId, LOG_DEBUG, 
                        "explicit step to die [%d,%d]", all->xcurrent, all->ycurrent);
        return 1;
    }
    else
    {
        phLogSimMessage(all->logId, LOG_DEBUG, 
                        "die [%d,%d] outside of wafer, but stepped", x, y);
        return 0;
    }
}
#endif

/*****************************************************************************
 * getDie()            
 *****************************************************************************/
static int getDie(struct all_t *all, int *x, int *y)
{
    switch ( all->numSites )
    {
        case 1:
            *x = stepPat1Site[all->currDieIndx][0];
            *y = stepPat1Site[all->currDieIndx][1];
            break;
        case 2:
            *x = stepPat2SiteP[all->currDieIndx][0];
            *y = stepPat2SiteP[all->currDieIndx][1];
            break;
        case 4:
            *x = stepPat4Site[all->currDieIndx][0];
            *y = stepPat4Site[all->currDieIndx][1];
            break;
        case 8:
            *x = stepPat8Site[all->currDieIndx][0];
            *y = stepPat8Site[all->currDieIndx][1];
            break;
        case 256:
            *x = stepPat256Site[all->currDieIndx][0];
            *y = stepPat256Site[all->currDieIndx][1];
            break;
        case 1024:
            *x = stepPat1024Site[all->currDieIndx][0];
            *y = stepPat1024Site[all->currDieIndx][1];
            break;
        case 1536:
            *x = stepPat1536Site[all->currDieIndx][0];
            *y = stepPat1536Site[all->currDieIndx][1];
            break;
        default:
            *x = stepPat1024Site[all->currDieIndx][0];
            *y = stepPat1024Site[all->currDieIndx][1];
            break;
    }

    phLogSimMessage(all->logId, LOG_DEBUG, 
                    "%d sites,current die position [%d,%d]",all->numSites, *x, *y);

    return 1;
}

#if 0
/* comment for 'defined but not used' */
/*****************************************************************************
 * getRefDie() 
 *****************************************************************************/
static int getRefDie(struct all_t *all, int *x, int *y)
{
    *x = all->xref;
    *y = all->yref;
    phLogSimMessage(all->logId, LOG_DEBUG, 
                    "reference die position [%d,%d]", *x, *y);
    return 1;
}
#endif

/*****************************************************************************
 * binDie()   
 *****************************************************************************/
static void binDie(struct all_t *all, int bin)
{
    all->waferBinTable[all->currDieIndx] = bin;
}

/*****************************************************************************
 * chuckUp_Z()         
 *****************************************************************************/
static void chuckUp_Z(struct all_t *all, char *m)
{
    /* phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"Z\""); */

    if ( chuckUp(all, 1) )
    {
        sleep(20);      /* It may take some minutes for wafer pre-heating before test */
        sendSrq(all, 0x43); /* OK */
    }
    else
        sendSrq(all, 0x53); /* Chuck limit reached */

}


/*****************************************************************************
 * chuckUp_ZStep(), CR21589, Kun Xiao, 4 Jul 2006
 *****************************************************************************/
static void chuckUp_ZStep(struct all_t *all, char *m)
{
    int step;
    int stepResult = SUCCEED;


    sscanf(m, "Z%d", &step);
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"Z+-\"");
    doDelay(0, all);

    
    if (step > 0) 
    {
      phLogSimMessage(all->logId, LOG_DEBUG, "chuck up by stepping %d", step);
      if ((all->zcurrent + step) > all->zlimit)
      {          
        phLogSimMessage(all->logId, LOG_DEBUG, 
                        "up limit of z-height achieved, chuck up by stepping %d failed!", step);
        stepResult = FAIL;
      }
    }
    else
    {
      phLogSimMessage(all->logId, LOG_DEBUG, "chuck down by stepping %d", step);
      if ((all->zcurrent + step) < all->zmin)
      {
        phLogSimMessage(all->logId, LOG_DEBUG, 
                        "down limit of z-height achieved, chuck down by stepping %d failed!", step);
        stepResult = FAIL;
      }
    }
    if (stepResult == SUCCEED) 
    {
      all->zcurrent = all->zcurrent + step;
    }
    sendSrq(all, 0x5c);   
    /* Z limit error not supported by TSK prober */
}

/*****************************************************************************
 * setContactPo(), CR21589, Kun Xiao, 4 Jul 2006
 *****************************************************************************/
static void setContactPo(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"z\"");
    doDelay(0, all);
    
    all->zcontact = all->zcurrent;
    phLogSimMessage(all->logId, LOG_VERBOSE, "contact position is set: %d", all->zcontact);
    sendSrq(all, 0x74); /* OK */
}

/*****************************************************************************
 * chuckDown_D()             
 *****************************************************************************/
static void chuckDown_D(struct all_t *all, char *m) 
{
    /* phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"D\""); */
    /* doDelay(0, all); */
    /*Begin CR21589, Kun Xiao, 4 Jul 2006*/
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"D\"");
    all->zcurrent = 0;
    /*End*/
    chuckUp(all, 0);

    sendSrq(all, 0x44); /* OK */
}
/*****************************************************************************
 * alarmBuzzer()             
 *****************************************************************************/
static void alarmBuzzer(struct all_t *all, char *m) 
{
      char message[64];
      sendSrq(all, 0x65); /* OK */
      strcpy(message, "");
      sprintf(message, "%s", m+2);
      phLogSimMessage(all->logId, LOG_NORMAL, 
                      "the alarm buzzer message from the Tester is: %s", message);
}
/*****************************************************************************
 * onWafer_O()          
 *****************************************************************************/
static void onWafer_O(struct all_t *all, char *m) 
{
    char message[2048];
    static int num = 1;

    /* phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"O\""); */

    if ( all->numSites==1 )
    {
        sprintf(message, "OA");
    }
    else if ( all->numSites==2 )
    {
        if ( all->master==SMARTEST_IS_MASTER )
        {
            switch ( stepPat2SiteST[all->currDieIndx][2] )
            {
                case 0: sprintf(message, "O@");
                    break;
                case 1: sprintf(message, "OA");
                    break;
                case 2: sprintf(message, "OB");
                    break;
                case 3: sprintf(message, "OC");
                    break;
                default: break;
            }
        }
        else if ( all->master==PROBER_IS_MASTER )
        {
            switch ( stepPat2SiteP[all->currDieIndx][2] )
            {
                case 0: sprintf(message, "O@");
                    break;
                case 1: sprintf(message, "OA");
                    break;
                case 2: sprintf(message, "OB");
                    break;
                case 3: sprintf(message, "OC");
                    break;
                default: break;
            }
        }
    }
    else if ( all->numSites==4 )
    {
        switch ( stepPat4Site[all->currDieIndx][2] )
        {
            case 0: sprintf(message, "O@");
                break;
            case 1: sprintf(message, "OA");
                break;
            case 2: sprintf(message, "OB");
                break;
            case 3: sprintf(message, "OC");
                break;
            case 4: sprintf(message, "OD");
                break;
            case 5: sprintf(message, "OE");
                break;
            case 6: sprintf(message, "OF");
                break;
            case 7: sprintf(message, "OA");
                break;
            case 8: sprintf(message, "OA");
                break;
            case 9: sprintf(message, "OA");
                break;
            case 10: sprintf(message, "OA");
                break;
            case 11: sprintf(message, "OA");
                break;
            case 12: sprintf(message, "OA");
                break;
            case 13: sprintf(message, "OA");
                break;
            case 14: sprintf(message, "OA");
                break;
            case 15: sprintf(message, "OA");
                break;
            default: sprintf(message, "OA");
                break;
        }
    }
    else if(all->numSites==8 )
    {
        switch ( num )
        {
            case 0: sprintf(message, "O@");
                break;
            case 1: sprintf(message, "OA@");
                break;
            case 2: sprintf(message, "OG@");
                break;
            case 3: sprintf(message, "OOA");
                break;
            case 4: sprintf(message, "OOG");
                break;
            case 5: sprintf(message, "OOA");
                break;
            case 6: sprintf(message, "OG@");
                break;
            case 7: sprintf(message, "OA@");
                break;
            default:
                    break;

        }
        if(num == 7)
            num = 0;
        ++num;
    }
    else if( all->numSites == 256 )
    {
        switch( stepPat256Site[all->currDieIndx][2] ){
             case 255:
                 sprintf(message, "OAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
                 break;
             default: 
                 sprintf(message, "OAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
                break;
        }
    }
    else if( all->numSites == 1024 )
    { 
        switch( stepPat1024Site[all->currDieIndx][2] ){
            case 1023:
            sprintf(message, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");
                break;
            default:
              sprintf(message, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");
                break;
        }
    }
    else if( all->numSites == 1536 )
    {
        switch( stepPat1536Site[all->currDieIndx][2] ){
            case 1535:
           //   "O" x 385
            sprintf(message, "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");
                break;
            default:
              sprintf(message, "OAOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO");  
                break;
        }
    }
    else
    {
       memset(message,0,sizeof(message));
       strcat(message,"O");
       for(int i = 0 ; i < (all->numSites-1)/4+1; ++i)
           strcat(message,"A");
    }
    
    sendMessage(all, message);  
    localDelay_ms(100);
}

/*****************************************************************************
 * loadWafer_L()
 *****************************************************************************/
static void loadWafer_L(struct all_t *all, char *m)
{
    localDelay_ms(200);

    if ( unloadWafer(all, 0) )
    {
        if ( all->master==SMARTEST_IS_MASTER )
        {
            sendSrq(all, 0x51); /* 81 = wafer end */
            localDelay_ms(100);
        }

        sleep(1);
        sendSrq(all, 0x46); /* 70 = next wafer loaded */
        sleep(1);

    }
    else
    {
        if ( all->master==SMARTEST_IS_MASTER )
        {
            sendSrq(all, 0x51);    /* 81 = wafer end */
            localDelay_ms(100);
        }
        if ( all->cassettecount > 0 )
            sendSrq(all, 0x52);        /* 82 = cassette end */
        else
            sendSrq(all, 0x5e);        /* 94 = lot end */

        all->initDone = 0;
    }
}

/*****************************************************************************
 * equipId_B()        
 *****************************************************************************/
static void equipId_B(struct all_t *all, char *m)
{
    char message[64];

    sprintf(message, "tskapm90a");
    sendMessage(all, message);  
}

/*****************************************************************************
 * waferId_W()        
 *****************************************************************************/
static void waferId_W(struct all_t *all, char *m)
{
    char message[64];

    sprintf(message, "%s%04d",all->waferIdPrefix,all->wafercount);
    sendMessage(all, message);  
}
/*****************************************************************************
 * multiLocNum()        
 *****************************************************************************/
static void multiLocNum(struct all_t *all, char *m)
{
    char message[64];

    sprintf(message, "H04");
    sendMessage(all, message); 
}
/*****************************************************************************
 * errType()        
 *****************************************************************************/
static void errType(struct all_t *all, char *m)
{
    char message[64];

    /* system error, the message could be "E[EOWSI]dddd", where dddd is the error code */
    sprintf(message, "ES0001");
    /*sprintf(message, "E");*/
    sendMessage(all, message); 
}
/****************************************************************************
 * waferId_b()
 ****************************************************************************/
static void waferId_b(struct all_t *all, char *m)
{
    char message[64];

    sprintf(message, "%s%04d",all->waferIdPrefix,all->wafercount);
    sendMessage(all, message);
}
/****************************************************************************
 * waferstatus()
 ****************************************************************************/
static void waferStatus(struct all_t *all, char *m)
{
    char message[128];

    sprintf(message, "w12310000011111000001101111a00000000000000000000000000");
    sendMessage(all, message);
}

/*****************************************************************************
 * lotId_V()        
 *****************************************************************************/
static void lotId_V(struct all_t *all, char *m)
{
    char message[64];

    sprintf(message, "Vabcdefghijklmnop");
    sendMessage(all, message);  
}

/*****************************************************************************
 * cleanProbes_b()        
 *****************************************************************************/
static void cleanProbes_b(struct all_t *all, char *m)
{
    localDelay_ms(200);
    sendSrq(all, 0x59);
}


/*****************************************************************************
 * askDie_Q()     
 *****************************************************************************/
static void askDie_Q(struct all_t *all, char *m)
{
    char message[64];
    int x, y;

    getDie(all, &x, &y);
    sprintf(message, "QY%03dX%03d", y, x);

    localDelay_ms(100);
    sendMessage(all, message);
    localDelay_ms(100);
}


static void askDie_Height(struct all_t *all, char *m)
{
    char message[64];
    static int currentZheight = 0;
    sprintf(message, "i%d",all->zcurrent);
    sendMessage(all, message);
}
/*****************************************************************************
 * markDie_M()      
 *****************************************************************************/
static void markDie_M(struct all_t *all, char *m)
{ 
    int bin = 0;
    int bin_lb = 0;
    int bin_hb = 0;
    int p = 0;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    char message[64];*/
/* End of Huatek Modification */
    char message[20480] = "";
    char temp_m[64] = "";
    int s = 0;
    char pass_fail;

    localDelay_ms(100);

#if 0
    inkDie(all, (m[1] & 0x01) ? 1 : 0); 
#endif
    bin = -1;

#if 0
    for ( p=0; p<8; p++ )
    {
        switch ( m[2+p] )
        {
            case '0': break;
            case '1': bin = 4*p + 0; break;
            case '2': bin = 4*p + 1; break;
            case '4': bin = 4*p + 2; break;
            case '8': bin = 4*p + 3; break;
        }
    }
#endif

    /*                                                                    */
    /*                                                                    */
    /*                                                                    */
    /*                                                                    */
    /* CR3462 increase number of bins to 0-63                             */
    /* number of sites is 1-2-4                                           */
    /*                                                                    */
    /* M command goes some thing like:                                    */
    /* M | P/F | C1 C2 C3 C4                                              */
    /*                                                                    */
    /*                                                                    */


    ++m;   /* move off M part of command */

    pass_fail=*m++;
    strcpy(message,"");

    for ( s=0 ; s < all->numSites ; ++s )
    {
        /* CR23211 allows up to 256 bins                                      */
        /* however, we must check for one vs two characters per bin code,     */
        /* set in .cfg file as max_bins_allowed, specified to simulator as    */
        /* value 1 or 2 in -M parameter                                       */
        if (all->numBinChars == 2) {
            bin_hb = *m++;
            bin_hb -= 64; // subtract out ascii '@'
            bin_lb = *m++;
            bin_lb -= 64; // subtract out ascii '@'
            bin = (bin_hb << 6) + bin_lb;
            bin &= 0xFF;
        }
        else {
            bin=*m++;
            bin-=64;
            bin&=0x3F;
        }

        p = pass_fail & ( 1 << s );

        sprintf(temp_m, "Site %d %c %d ", 
                s+1,
                ( p ) ? 'P' : 'F',
                bin);
        strcat(message,temp_m);
    }
    binDie(all, bin);
    localDelay_ms(100);

    if ( !all->lastDie )
    {
        if ( all->master==PROBER_IS_MASTER )
        {
        /* 
         * CR 27997 "STB 100(0x64)" has been switched off 
         * testDoneIgnored == YES means cancel sending STB100 from prober to tester, where 0x64 = 100 means Test Complete 
         */
        if( all->testDoneIgnored != YES )
        {
          sendSrq(all, 0x64);
        }                
            all->markingDone = 1;
        }
        else
        {
            sendSrq(all, 0x45);    /* 69 = Marking done */
        }

        if(all->manualUnloadWafer)
        {
          forceUnloadWafer = 0;
          if(all->wafercount %2)
          {
            unloadWafer(all, 1);
          }
        }
    }
    else
    {
        if ( all->master==PROBER_IS_MASTER )
        {
            sendSrq(all, 0x51);    /* 81 = Wafer Complete */
        }
        else
        {
            sendSrq(all, 0x45);    /* 69 = since prober shouldn't know when last die*/
        }

        newWafer(all);
        return;
    }

}


/*****************************************************************************
 * moveDie_J()      
 *****************************************************************************/
static void moveDie_J(struct all_t *all, char *m)
{
    int x, y;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int dieSet;*/
/* End of Huatek Modification */
    int enabled100 = 1;
    all->zcurrent=0;

    sscanf(m, "JY%03dX%03d", &y, &x);
    /* dieSet = setDie(all, x, y); */
    localDelay_ms(300);



    /* needs work, only send if enabled at prober */
    if ( enabled100 )
    {
        sendSrq(all, 0x64);    /* 100 = test complete if enabled at prober */
        localDelay_ms(500);
    }

    if ( all->stopButtonPressed )
    {
        sendSrq(all, 0x5a);    /* 90 = send STOP SRQ to tester now */
        all->proberHasStopped = 1;    /* prober is now stopped */
        localDelay_ms(100);
    }
    else
    {
        if ( all->chuckUp )
            sendSrq(all, 0x43);    /* 67 = movement complete Z-up */
        else
            sendSrq(all, 0x42);    /* 66 = movement complete Z-down */
    }

}


/*****************************************************************************
 * lcTSM()      
 *****************************************************************************/
static void lcTSM(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"",m);

    localDelay_ms(100);

/*    sendSrq(all, 0x64);     */
}

/*****************************************************************************
 * readDataId()
 *
 * Description:
 *    Prober send the value at appointed Data ID, which is set up in Prober
 *
 * support:
 *    ur0002  ->   wafer size or wafer diameter
 *    ur0005  ->   die height
 *    ur0004  ->   die width
 *    ur0708  ->   wafer unit
 *    ur0008  ->   wafer flat
 *    ur10238 ->   X coordinate of center Die
 *    ur10239 ->   Y cooridnate of center Die
 *    ur1224  ->   Positive X direction of Wafer
 *    ur1225  ->   Positive Y direction of Wafer
 *
 *           above make up of STDF WCR record
 *      
 *****************************************************************************/
static void readDataId(struct all_t *all, char *m)
{
  char message[64] = "";
  phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"",m);

  localDelay_ms(100);

     
  if( strcmp(m, "ur0001") == 0 ) {
    sprintf(message, "urSetupFile.001");
  }else if( strcmp(m, "ur0002") == 0 ){
    sprintf(message, "ur8inch");
  } else if( strcmp(m, "ur0005")  == 0 ) {
    sprintf(message, "ur2.909000mm");
  } else if( strcmp(m, "ur0004") == 0 ) {
    sprintf(message, "ur3.20600mm");
  } else if( strcmp(m, "ur0708") == 0 ) {
    sprintf(message, "ur3");
  } else if( strcmp(m, "ur0008") == 0 ) {
    sprintf(message, "ur180deg");
  } else if( strcmp(m, "ur10238") == 0 ) {
    sprintf(message, "ur005");
  } else if( strcmp(m, "ur10239") == 0 ) {
    sprintf(message, "ur006");
  } else if( strcmp(m, "ur1224") == 0 ) {
    sprintf(message, "ur0");
  } else if( strcmp(m, "ur1225") == 0 ) {
    sprintf(message, "ur1");
  } else {
    sprintf(message, "%s%s","not_implemented_yet_for_", m);
  }
  sendMessage(all, message);  
}

/*****************************************************************************
 * cassStatus()      
 *****************************************************************************/
static void cassStatus(struct all_t *all, char *m)
{
  char message[64] = "";
  phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"",m);

  localDelay_ms(100);

  /* 
     Slot Number of wafer on the chuck in current cassette=01, 
     The number of not Finished Wafers in current cass=24,
     Cass 1: Testing under way(2)
     Cass 2: No cassette(0)
  */
  sprintf(message, "x012420"); 
  sendMessage(all, message);  
}

/*****************************************************************************
 * waferNumber()      
 *****************************************************************************/
static void waferNumber(struct all_t *all, char *m)
{
  char message[64] = "";
  phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"",m);

  localDelay_ms(100);

  /* Slot Number = 12, Cassette Nummber = 1 */
  sprintf(message, "X121");
  sendMessage(all, message);  
}

/*****************************************************************************
 * chuckTemperature()      
 *****************************************************************************/
static void chuckTemperature(struct all_t *all, char *m)
{
  char message[64] = "";
  phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"",m);

  localDelay_ms(100);

  /* 0855=85.5C (setting temperature), 0500=50.0C (current temperature) */
  sprintf(message, "f08550500");  
  sendMessage(all, message);  
}

/*Begin CR91531, Adam Huang, 24 Nov 2014*/

/*****************************************************************************
 * CardContactCount()
 *****************************************************************************/
static void CardContactCount_kc(struct all_t *all, char *m)
{
    char szMessage[64] = "";
    phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"", m);

    /*card contact count = 907050301*/
    localDelay_ms(100);
    sprintf(szMessage, "kc0907050301");
    sendMessage(all, szMessage);
}

/*****************************************************************************
 * DeviceParameter()
 *****************************************************************************/
static void DeviceParameter_ku(struct all_t *all, char *m)
{
    char szMessage[256] = "";
    phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"", m);

    localDelay_ms(100);
    sprintf(szMessage, "kuWaferName000C3609999900000000000000000000000XN1199991000000000000+99-99N000000N00Y0501+99+99+99+99+99+99+99+99+99+99+99+99+99+99+99+9965535000X19999000000008??Y9922?19999999YY9999999999999991359999");
    sendMessage(all, szMessage);
}

/*****************************************************************************
 * GrossValueRequest()
 *****************************************************************************/
static void GrossValueRequest_Y(struct all_t *all, char *m)
{
    char szMessage[64] = "";
    phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"", m);

    /*yield data = 99.99%*/
    localDelay_ms(100);
    sprintf(szMessage, "y0999");
    sendMessage(all, szMessage);
}

/*End CR91531*/

/*****************************************************************************
 * parseCommand()
 *****************************************************************************/
static void parseCommand(struct all_t *all)
{
    char m[10000] = "";

    strncpy(m, all->message, sizeof(m) - 1);
    phLogSimMessage(all->logId, LOG_NORMAL, "handling \"%s\"", m);
    if ( strlen(m) >= strlen(EOC) &&
         strcmp(&m[strlen(m) - strlen(EOC)], EOC) == 0 )
        m[strlen(m) - strlen(EOC)] = '\0';

    phGuiChangeValue(all->guiId, "f_req", m);
    setNotUnderstood(0, all);

    if ( phToolsMatch(m, "Z") )
        chuckUp_Z(all, m);

    /*Begin CR21589, Kun Xiao, 4 Jul 2006*/
    else if (phToolsMatch(m, "Z[\\+-][0-9]{2}"))
        chuckUp_ZStep(all, m);
    else if (phToolsMatch(m, "z"))
        setContactPo(all, m); 
    /*End CR21589*/

    else if ( phToolsMatch(m, "O") )
        onWafer_O(all, m);
    else if ( phToolsMatch(m, "D") )
        chuckDown_D(all, m);
    else if ( phToolsMatch(m, "L") )
        loadWafer_L(all, m);
    else if ( phToolsMatch(m, "B") )
        equipId_B(all, m);
    else if ( phToolsMatch(m, "W") )
        waferId_W(all, m);
    else if ( phToolsMatch(m, "V") )
        lotId_V(all, m);
    else if ( phToolsMatch(m, "Q") )
        askDie_Q(all, m);
    else if ( phToolsMatch(m, "i24") )
        askDie_Height(all, m);
    else if ( phToolsMatch(m, "M[@A-_].*") )
        markDie_M(all, m);
    else if ( phToolsMatch(m, "JY[-0-9]*X[-0-9]*") )
        moveDie_J(all, m);
    else if ( phToolsMatch(m, "b") )
    {
        if ( all->waferID_is_b )
            waferId_b(all, m);
        else
            cleanProbes_b(all, m);
    }
    else if ( phToolsMatch(m, "lcTSM.*") )
        lcTSM(all, m);
    else if( phToolsMatch(m, "ur[0-9]*") ){
      readDataId(all, m);
    } else if( phToolsMatch(m, "X") ) {
      waferNumber(all, m);
    } else if( phToolsMatch(m, "x") ) {
      cassStatus(all, m);
    } else if( phToolsMatch(m, "f") ) {
      chuckTemperature(all, m);
    } else if( phToolsMatch(m, "H") ) {
      multiLocNum(all, m);
    } else if( phToolsMatch(m, "E") ) {
      errType(all, m);
    } else if( phToolsMatch(m, "w") ) {
      waferStatus(all, m);
    } else if( phToolsMatch(m, "em.*") ) {
      alarmBuzzer(all,m);
    }
    
    /*Begin CR91531, Adam Huang, 24 Nov 2014*/
    else if(phToolsMatch(m, "kc")) {
        CardContactCount_kc(all, m);
    } else if(phToolsMatch(m, "ku")) {
        DeviceParameter_ku(all, m);
    } else if(phToolsMatch(m, "Y")) {
        GrossValueRequest_Y(all, m);
    }
    /*End CR91531*/

    else
        setNotUnderstood(1, all);
}

/*****************************************************************************
 * handleGuiEvent
 *****************************************************************************/
static void handleGuiEvent(struct all_t *all)
{
    phGuiError_t guiError;
    char request[1024];
    phComError_t comError;
    phComGpibEvent_t sendEvent;

    switch ( all->guiEvent.type )
    {
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
            guiError = phGuiChangeValue(all->guiId, "f_event", request);
            checkGuiError(guiError, all);

            /* now handle the button press event */
            if ( strcmp(all->guiEvent.name, "p_quit") == 0 )
            {
                phGuiDestroyDialog(all->guiId);
                all->done = 1;
            }
            else if ( strcmp(all->guiEvent.name, "p_stop") == 0 )
            {
                phLogSimMessage(all->logId, LOG_NORMAL,
                                "STOP button pressed");
                all->stopButtonPressed = 1;

            }
            else if ( strcmp(all->guiEvent.name, "p_start") == 0 )
            {
                phLogSimMessage(all->logId, LOG_NORMAL,
                                "START button pressed");
                if ( !all->initDone )
                {
                    
                    all->cassettecount--;
                    chuckUp(all, 0);
                    newWafer(all);      
                    sendSrq(all, 0x46);
                    all->initDone = 1;
                }
                else
                {
                    if ( all->proberHasStopped )
                    {
                        all->startButtonPressed = 1;
                    }
                }
            }
            else if ( strcmp(all->guiEvent.name, "p_back") == 0 )
            {
                phLogSimMessage(all->logId, LOG_NORMAL,
                                "sequence back button pressed");
                all->sequenceBackPressed = 1;

            }
            else if ( strcmp(all->guiEvent.name, "p_lotend") == 0 )
            {
                phLogSimMessage(all->logId, LOG_NORMAL,
                                "lot end button pressed");
                all->lotEndPressed = 1;

            }
            else if ( strcmp(all->guiEvent.name, "p_flush") == 0 )
            {
                phLogSimMessage(all->logId, LOG_NORMAL,
                                "FLUSH button pressed");

                sendSrq(all, 0x47);        /* 71 */
                localDelay_ms(200);
                sendSrq(all, 0x48);        /* 72 */
                localDelay_ms(200);
                sendSrq(all, 0x49);        /* 73 */
            }
            else if ( strcmp(all->guiEvent.name, "p_req") == 0 )
            {
                guiError = phGuiChangeValue(all->guiId, "f_req", "");
                checkGuiError(guiError, all);

                setWaiting(1, all);
                comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length, 1000L * all->timeout);
                setWaiting(0, all);
                switch ( checkComError(comError, all) )
                {
                    case 0:
                        strcpy(request, all->message);
                        if ( strlen(request) >= strlen(EOC) &&
                             strcmp(&request[strlen(request) - strlen(EOC)], EOC) == 0 )
                            request[strlen(request) - strlen(EOC)] = '\0';
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
                guiError = phGuiChangeValue(all->guiId, "f_req", request);
                checkGuiError(guiError, all);
            }
            else if ( strcmp(all->guiEvent.name, "p_answ") == 0 )
            {
                guiError = phGuiGetData(all->guiId, "f_answ", request, 5);
                if ( !checkGuiError(guiError, all) )
                    return;
#if 0
                phLogSimMessage(all->logId, LOG_DEBUG,
                                "trying to send message '%s'", request);
#endif
                strcat(request, EOC);
                setWaiting(1, all);
                comError = phComGpibSendVXI11(all->comId, request, strlen(request), 1000L * all->timeout);
                setWaiting(0, all);
                switch ( checkComError(comError, all) )
                {
                    case 0:
                        guiError = phGuiChangeValue(all->guiId, "f_answ", "");
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
            }
            else if ( strcmp(all->guiEvent.name, "p_srq") == 0 )
            {
                sendEvent.type = PHCOM_GPIB_ET_SRQ;
                guiError = phGuiGetData(all->guiId, "f_srq", request, 5);
                if ( !checkGuiError(guiError, all) )
                    return;
                sendEvent.d.srq_byte = (unsigned char) strtol(request, NULL, 0);
                setWaiting(1, all);
                comError = phComGpibSendEventVXI11(all->comId, &sendEvent, 1000L * all->timeout);
                setWaiting(0, all);
                if ( !checkComError(comError, all) )
                {
                    guiError = phGuiChangeValue(all->guiId, "f_srq", "");
                    checkGuiError(guiError, all);
                }
            }
            else if ( strcmp(all->guiEvent.name, "p_event") == 0 )
            {
                strcpy(request, "");
                guiError = phGuiChangeValue(all->guiId, "f_event", request);
                checkGuiError(guiError, all);
            }
            break;
    }
}

/*****************************************************************************
 * doWork()        
 *****************************************************************************/
static void doWork(struct all_t *all)
{
    phGuiError_t guiError;
    phComError_t comError;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int pending;*/
/* End of Huatek Modification */

    setError(0, all);

    all->done = 0;
    do
    {
        heartbeat(all);

        if ( all->proberHasStopped )  /* prober paused */
        {
            localDelay_ms(100);  /* slow down a smidge */
            if ( all->startButtonPressed )
            {
                comeOutOfPause(all);
            }
            else
            {
                phLogSimMessage(all->logId, LOG_DEBUG, "prober stopped, keep waiting for START");
            }
        }
        else   /* prober not paused */
        {
            if ( all->master==PROBER_IS_MASTER )
            {
                if ( all->markingDone )
                {
                    phLogSimMessage(all->logId, LOG_DEBUG, "all->markingDone must be true to get this far");
                    autoIndex(all);
                }
                else
                {
                    comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length, 1000000);
                    checkComError(comError, all);
                    if ( comError == PHCOM_ERR_OK )
                    {
                        parseCommand(all);
                    }
                } 
            }
            else  /* smartest is master */
            {
                comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length, 1000000);
                checkComError(comError, all);
                if ( comError == PHCOM_ERR_OK )
                {
                    parseCommand(all);
                }
            }
        }

        guiError = phPbGuiGetEvent(all ,all->guiId, &all->guiEvent, 0);
        checkGuiError(guiError, all);
        if ( guiError == PH_GUI_ERR_OK )
        {
            handleGuiEvent(all);
        }

    } while ( !all->done );
}

/*****************************************************************************
 * arguments()
 *****************************************************************************/
int arguments(int argc, char *argv[], struct all_t *all)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;

    /* setup default: */

    while ( (c = getopt(argc, argv, ":d:i:t:m:s:P:S:c:b:M:I:z:C:n:F:")) != -1 )
    {
        switch ( c )
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
                if ( strcmp(optarg, "APM90A") == 0 ) all->model = MOD_APM90A;
                else if ( strcmp(optarg, "UF200") == 0 ) all->model = MOD_UF200;
                else if ( strcmp(optarg, "UF300") == 0 ) all->model = MOD_UF300;
                else if ( strcmp(optarg, "UF3000") == 0 ) all->model = MOD_UF3000;
                else errflg++;
                break;
            case 's':
                all->numSites = atoi(optarg);
                switch ( all->numSites )
                {
                    case 1: all->numSteps = 20;
                        break;
                    case 2: all->numSteps = 11; 
                        break;
                    case 4: all->numSteps = 9;
                        break;
                    case 8: all->numSteps = 7;
                        break;
                    case 256:
                    case 1024:
                    case 1536:
                    case 2048:
                        all->numSteps = 30;
                        break;
                    default: all->numSteps = 9;
                        phLogSimMessage(all->logId, LOG_VERBOSE,
                                        "number of sites wrong: %d", all->numSites);
                        break;
                }
                break;
            case 'c':
                if ( strcmp(optarg, "prober")==0 ) all->master=PROBER_IS_MASTER;
                else if ( strcmp(optarg, "smartest")==0 ) all->master=SMARTEST_IS_MASTER;
                else errflg++;
                break;
            case 'S':
                all->autoStartTime = atoi(optarg);
                break;
            case 'P':
                /* not yet implemented */
                break;
            case 'b':
                /* Use "b" as the wafer ID command, rather than default "W". */
                all->waferID_is_b = atoi(optarg);
                break;
            case 'M':
                all->numBinChars = atoi(optarg);
                break;
            case 'I':
                all->testDoneIgnored = atoi(optarg);
                break;
            case 'z':
                /*CR21589, Kun Xiao, 4 Jul 2006*/
                all->zlimit = atoi(optarg);
                break;
            case 'n':
                all->manualUnloadWafer = 1;
                break;
            case 'C':
                wafercount=atoi(optarg)+1;
                break;
            case 'F':
                strncpy(all->waferIdPrefix,optarg,100);
                break;
            case ':':
                errflg++;
                break;
            case '?':
                fprintf(stderr, "Unrecognized option: - %c\n", (char) optopt);
                errflg++;
        }
        if ( errflg )
        {
            fprintf(stderr, "usage: %s [<options>]\n", argv[0]);
            fprintf(stderr, "    [-d <debug-level]         -1 to 4, default 0\n");
            fprintf(stderr, "    [-i <gpib-interface>]     GPIB interface alias name, default hpib\n");
            fprintf(stderr, "    [-t <timeout>]            GPIB timeout to be used [msec], default 5000\n");
            fprintf(stderr, "    [-m <prober model>]       model [APM90A|UF200|UF300|UF3000], default APM90A\n");
            fprintf(stderr, "    [-s <number sites>]       number sites [1|2|4|8|256|1024|1536],  default 1 \n");
            fprintf(stderr, "    [-c <index control>]      [prober|smartest], default prober \n");
            fprintf(stderr, "    [-S <seconds> ]           auto start lot this time after lot end\n");
            fprintf(stderr, "    [-C <wafer count>]        set wafer count,default 5\n");
            fprintf(stderr, "    [-b <number> ]            set wafer ID command as 'b' instead of 'W' when number !=0\n");
            fprintf(stderr, "    [-M <number> ]            set number of characters used per bin code in M command, value = 1 or 2, default = 1 \n");
            fprintf(stderr, "    [-I <value>]              Ignore sending test-done SRQ (100) when value = 1, default = 0 \n");    
            fprintf(stderr, "    [-z <value>]              set z up limit, default is 1000 \n"); /*CR21589, Kun Xiao, 4 Jul 2006*/
            fprintf(stderr, "    [-n]                      randomly unload the unfinished wafer to simulate manual unload/next wafer  \n");
            fprintf(stderr, "     -F                       specify the prefix of wafer ID\n");
            return 0;
        }
    }
    return 1;
}


/*****************************************************************************
 * main() 
 *****************************************************************************/
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

/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    int i; */
/* End of Huatek Modification */
    phLogMode_t modeFlags = PHLOG_MODE_NORMAL;

    /* Disable the generation of the HVM alarm */
    setenv("DISABLE_HVM_ALARM_GENERATION_FOR_TCI", "1", 1);

    /* init defaults */
    allg.debugLevel = 0;
    allg.master = PROBER_IS_MASTER;       /* default, step pattern determined by prober */
    allg.hpibDevice = strdup("hpib");
    allg.timeout = 5000;
    allg.modelname = strdup("APM90");
    allg.model = MOD_APM90A;
    allg.numSites = 1;
    allg.numSteps=30;
    allg.numBinChars = 1;
    allg.autoStartTime = 0;
    allg.lotActive = 0;
    allg.waferID_is_b = 0;  /* The wafer ID command can be either "b" or "W" - default "W". */
    allg.testDoneIgnored = NO ;
    allg.manualUnloadWafer = 0;
    allg.wafercount = 0;
    strcpy(allg.waferIdPrefix,"wafer-");

    /*Begin CR21589, Kun Xiao, 4 Jul 2006*/
    allg.zmin = Z_MIN;
    allg.zlimit = Z_LIMIT;
    allg.zcontact = -1;
    /*End CR21589*/

    /* get real operational parameters */
    if ( !arguments(argc, argv, &allg) )
    {
        exit(1);
    }
    
    /* open message logger */
    switch ( allg.debugLevel )
    {
      case -1:
	modeFlags = PHLOG_MODE_QUIET;
	break;
      case 0:
	modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_0 );
	break;
      case 1:
	modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_1 );
	break;
      case 2:
	modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_2 );
	break;
      case 3:
	modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_3 );
	break;
      case 4:
	modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_DBG_4 );
	break;
      case 5:
	modeFlags = phLogMode_t( PHLOG_MODE_WARN | PHLOG_MODE_TRACE | PHLOG_MODE_DBG_4 );
	break;
    }
    phLogInit(&allg.logId, modeFlags, "-", NULL, "-", NULL, 0);

    /* open gpib communication */
    if ( phComGpibOpenVXI11(&allg.comId, PHCOM_ONLINE, 
                       allg.hpibDevice, 0, allg.logId, PHCOM_GPIB_EOMA_NONE) != PHCOM_ERR_OK )
    {
        exit(1);
    }

    /* create user dialog */
    if ( phGuiCreateUserDefDialog(&allg.guiId, allg.logId, PH_GUI_COMM_ASYNCHRON, 
                                  slaveGuiDesc) != PH_GUI_ERR_OK )
    {   
        exit(1);
    }
    phGuiShowDialog(allg.guiId, 0);

    /* init internal values */
    allg.stopButtonPressed = 0; 
    allg.startButtonPressed = 0; 
    allg.sequenceBackPressed = 0; 
    allg.lotEndPressed = 0; 
    allg.proberHasStopped = 0; 
    allg.firstWaferInCass = 1; 
    allg.firstContactWithProber = 1; 
    allg.initDone = 0;
    allg.cassettecount = CASS_COUNT;
    allg.abortProbing = 0;
    allg.markingDone = 0;

#if 0
    for ( i=0; i<21; i++ )
    {
        printf("stepPat1Site[%d] = %d %d\n", i,stepPat1Site[i][0],stepPat1Site[i][1]);
    }
#endif

    chuckUp(&allg, 0);
   // newWafer(&allg);

    

    /* enter work loop */
    setLotDoneTime(&allg);
    doWork(&allg);
    free(allg.hpibDevice);
    free(allg.modelname);
    allg.hpibDevice = NULL;
    allg.modelname = NULL;
    return 0;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
