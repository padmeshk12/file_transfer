/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.c
 * CREATED  : 15 Feb 2000
 *
 * CONTENTS : TEL prober simulator
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, CR 15358
 *            Jiawei Lin, R&D Shanghai, CR 27092 &CR25172
 *            David Pei, R&D Shanghai, CR31014
 *            Kun Xiao, R&D Shanghai, CR21589
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 15 Feb 2000, Michael Vogt, created
 *            June 2005, CR 15358:
 *                Introduce a new option "-M method" to specify the binning
 *                method, could be "parity" or "binary" binning method. For P12
 *                model, both of binning methods are allowed, whereas other
 *                models like 20S, 78S, P8 only use "parity" binning method.
 *                With "binary", the capability of 256 bins are supported;
 *                "parity" method just supports up to 32 bins.
 *                Now, P-12 model is offically released to the customers
 *            August 2005, Jiawei Lin, CR27092 &CR25172
 *                Implement the "ur" command to support the information query
 *                from Tester. Such information contains WCR (Wafer Configuration
 *                Record) of STDF, like Wafer Size, Wafer Unit.
 *                All these information could be
 *                retrieved by Tester with the PROB_HND_CALL "ph_get_status"
 *            April 2006, David Pei, CR31014
 *                Change the format of sending negative die coordinate from "-xxyyy" 
 *                "xxx-yy", "-xx-yy" to "-xxxyyy", "xxx-yyy", "-xxx-yyy". 
 *            July 2006, Kun Xiao, CR21589
 *                modify regular expression for "b" command
 *                Implement "Z+-" command to move chuck by step size
 *                Implement "z" command to set contact height 
 *            February 2007, Kun Xiao, CR33707
 *              support retrieving more parameters from TEL prober. The parameters are:
 *              casse_status, chunck_temp, error_code, multisite_location_number and so on.
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_GuiServer.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL         PHLOG_TYPE_MESSAGE_0
#define LOG_DEBUG          PHLOG_TYPE_MESSAGE_1
#define LOG_VERBOSE        PHLOG_TYPE_MESSAGE_2

#define EOC "\r\n"

#define CASS_COUNT         4
#define WAF_SLOTS          25
#define WAF_MAXDIM         1024

#define MAX_PROBES         1024

#define DIP_POSITIONS      8
#define DIP_GROUPS         20

#define DIEFLAG_EMPTY      0x0000
#define DIEFLAG_OUTSIDE    0x0001
#define DIEFLAG_BINNED     0x0002
#define DIEFLAG_INKED      0x0004



#define F_START            0x0001
#define F_ASSIST           0x0002
#define F_FATAL            0x0004
#define F_PAUSE            0x0008
#define F_WAFEND           0x0010
#define F_LOTEND           0x0012
#define F_ALL              0xffff

/* 
 * When corruptEvntMsg is set true every 1st and CORRUPT_MSG_EVNT(th) of
 * certain events and messages should be corrupted
 */
#define CORRUPT_MSG_EVNT  11

#define Die(a, x, y)       (a)->chuck.wafer.d[(x)+(a)->xmin][(y)+(a)->ymin]
#define WDie(a, w, x, y)   (w)->d[(x)+(a)->xmin][(y)+(a)->ymin]

#define InBounds(a, x, y) \
    ((x) >= (a)->xmin && (x) <= (a)->xmax && \
     (y) >= (a)->ymin && (y) <= (a)->ymax)

#define MAX_QUERY_COMMAND_IGNORE 10

/*CR21589, Kun Xiao, 4 Jul 2006*/
#define Z_LIMIT 1000
#define Z_MIN 0
/*End CR21589*/
/*this global var is prepared for wafer count*/
int waferCount=5;

enum model_t { MOD_P8, MOD_P12, MOD_F12, MOD_78S, MOD_20S };
enum waf_stat { WAF_UNTESTED, WAF_INTEST, WAF_TESTED };

enum flag_t { PAUSE, WAFEND, LOTEND, ASSIST, FATAL, FLAG_END };

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
    phGuiProcessId_t guiId;

    /* setup */
    int              debugLevel;
    char             *hpibDevice;
    int              timeout;
    char             *modelname;
    enum model_t     model;
    int              reverseBinning;
    int              asciiOCommand;
    int              sendFirstWaferSRQ;
    int              dontSendPauseSRQ;
    int              noGUI;
    int              autoStartTime;
    int              corruptEvntMsg;
    int              corruptEvntMsgCount;
    int              binningMethod;        /* 0: parity;1:binary, CR 15358 */

    /* init */
    int              done;
    int              wafercount;
    int              enableAutoStartFeature;
    int              sendLotEnd;
    int              abortProbing;
    int              dip[DIP_POSITIONS][DIP_GROUPS];
    int              master;

    /* positioning */
    int              xmin;
    int              ymin;
    int              xmax;
    int              ymax;
    int              zmin; /*CR21589, Kun Xiao, 29 Jun 2006*/
    int              zlimit; /*CR21589, Kun Xiao, 29 Jun 2006*/
    int              xref;
    int              yref;
    int              xstep;
    int              ystep;

    int              xcurrent;
    int              ycurrent;
    int              zcurrent; /*CR21589, Kun Xiao, 29 Jun 2006*/
    int              zcontact; /*CR21589, Kun Xiao, 29 Jun 2006*/
    int              chuckUp;

    /* lots */
    int              lotActive;
    long             lotDoneTime;

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

    /* runtime */
    const char       *message;
    int              length;
    phComGpibEvent_t *comEvent;
    phGuiEvent_t     guiEvent;

    int              interactiveDelay;
    int              interactiveDelayCount;

    /* ignore query / command */
    char             *ignore_query_command[MAX_QUERY_COMMAND_IGNORE];
    int              ignore_query_command_counter; 

    /* playback */
    int              pbGpibStep;
    int              pbSubStep;
    int              doRecord;
    int              doPlayback;
    FILE             *recordFile;
    FILE             *playbackFile;

    int              flagStatus[FLAG_END];
    int              recordUsed;
    char             recordName[128];
    char             recordData[128];
    int              recordRtnVal;
    int              recordStep;
    int              createNewPlayback;
    char             waferIdPrefix[128];
};

static void doExitFail(struct all_t *all);
static long timeSinceLotDone(struct all_t *all);
static int stepDie(struct all_t *all);

/*--- global variables ------------------------------------------------------*/

static struct all_t allg;

static const char *slaveGuiDesc = " \
    S:`name`:`TEL prober simulator` \
    {@v \
        S::``{@v \
            S::{@h  f:`f_req`:`Request`:    p:`p_req`:`Receive` } \
            S::{@h  f:`f_answ`:`Answer`:    p:`p_answ`:`Send` } \
            S::{@h  f:`f_srq`:`SRQ`:        p:`p_srq`:`Send` } \
            S::{@h  f:`f_event`:`Events`:   p:`p_event`:`Clear` } \
            S::{@h  f:`f_status`:`Status`:  p:`p_quit`:`Quit` } \
            S::{@h  f:`f_delay`:`Delay [seconds]`:  * \
                    R::{`r_life_1`:`` `r_life_2`:``}:2:2 * } \
            S::{@h  p:`p_start`:`Start` } \
            S::{@h  p:`p_autoz`:`Auto-Z`[`Will trigger auto-z process`] }\
            S::{@h  t:`t_pause`:`Pause` } \
            S::{@h  t:`t_wafend`:`Wafer end` } \
            S::{@h  t:`t_lotend`:`Lot end` } \
            S::{@h  t:`t_assist`:`Assist` } \
            S::{@h  t:`t_fatal`:`Fatal` } \
        } \
    } \
";

/*--- playback functionality ------------------------------------------------*/

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


static void doGpibStep(struct all_t *all)
{
    all->pbGpibStep++;
    all->pbSubStep = 0;
}

static void doSubStep(struct all_t *all)
{
    all->pbSubStep++;
}

/*
 * convert from flag string to enum flag_t
 */
static enum flag_t getFlagType(struct all_t *all, const char* flagStr)
{
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    enum flag_t flag=PAUSE;
/* End of Huatek Modification */

    if ( strcmp(flagStr,"t_pause") == 0 )
    {
        flag=PAUSE;
    }
    else if ( strcmp(flagStr,"t_wafend") == 0 )
    {
        flag=WAFEND;
    }
    else if ( strcmp(flagStr,"t_lotend") == 0 )
    {
        flag=LOTEND;
    }
    else if ( strcmp(flagStr,"t_assist") == 0 )
    {
        flag=ASSIST;
    }
    else if ( strcmp(flagStr,"t_fatal") == 0 )
    {
        flag=FATAL;
    }
    else
    {
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
		        "unknown flag string %s", flagStr);
        doExitFail(all);
    }

    return flag;
}

/*
 * convert flag component string value into int value
 */
static int getComponentValueInt(struct all_t *all, const char* cmptData)
{
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    int i=0;
/* End of Huatek Modification */

    if (strcasecmp(cmptData, "true") == 0)
    {
        i=1;
    }
    else if ( strcasecmp(cmptData, "false") == 0)
    {
        i=0;
    }
    else
    {
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
		        "unknown component value %s", cmptData);
        doExitFail(all);
    }

    return i;
}

/*
 * convert flag component int value into string value
 */
static const char *getComponentValueStr(struct all_t *all, int cmptData)
{
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    const char* cmptString=0;
/* End of Huatek Modification */

    if (cmptData==1)
    {
        cmptString="True";
    }
    else if (cmptData==0)
    {
        cmptString="False";
    }
    else
    {
        phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
		        "unknown component value %d", cmptData);
        doExitFail(all);
    }

    return cmptString;
}

static const char *getFlagStatusStr(struct all_t *all, const char* flagStr)
{
    return getComponentValueStr(all, all->flagStatus[ (int) getFlagType(all,flagStr) ] );
}

static void setFlagStatusStr(struct all_t *all, const char* flagStr, const char* status)
{
    all->flagStatus[ (int) getFlagType(all,flagStr) ] = getComponentValueInt(all, status);

    phLogSimMessage(all->logId, LOG_VERBOSE, "set[%s] = %s  : set[%d]=%d ",
                        flagStr, status, (int)getFlagType(all,flagStr), getComponentValueInt(all, status) );

}

static int getPlaybackRecord(struct all_t *all)
{
    int fscanfRtn;

    strcpy(all->recordName,"");
    strcpy(all->recordData,"");

    fscanfRtn = fscanf(all->playbackFile, "%d %d %127s %127s", &(all->recordStep),
      &(all->recordRtnVal), all->recordName, all->recordData);

    if ( fscanfRtn == 4 )
    {
        phLogSimMessage(all->logId, LOG_VERBOSE, "readPlaybackFile() %d %d %s %s",
                        all->recordStep, all->recordRtnVal, all->recordName, all->recordData );

        all->recordUsed=0;
    }
    else if ( fscanfRtn == EOF )
    {
        phLogSimMessage(all->logId, LOG_VERBOSE, "readPlaybackFile() EOF detected ");
        all->recordUsed=1;
    }
    else
    {
        phLogSimMessage(all->logId, LOG_VERBOSE,"readPlaybackFile() unexpected return value from fscanf  %d ",
                        fscanfRtn );
    }

    return (fscanfRtn == 4);
}

static phGuiError_t getPlaybackData(
    struct all_t *all,
    const char *componentName,     /* name of the component */
    char *data                     /* the data value of component */
)
{
    phGuiError_t retVal;

    if ( all->pbGpibStep >= all->recordStep )
    {
        if ( all->recordUsed==1 )
        {
            getPlaybackRecord(all);
        }
        else
        {
            if ( all->pbGpibStep > all->recordStep )
            {
                phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
                                "record not made use of %d %d %s %s current step %d %s ",
                                all->recordStep, all->recordRtnVal, all->recordName, all->recordData,
                                all->pbGpibStep, componentName);
                doExitFail(all);
            }
        }
    }

    if ( all->pbGpibStep==all->recordStep && strcmp(componentName,all->recordName)==0 )
    {
        retVal = (phGuiError_t) all->recordRtnVal;
        strcpy(data, all->recordData);
        all->recordUsed=1;
        setFlagStatusStr(all, componentName, data);
    }
    else
    {
        strcpy(data, getFlagStatusStr(all,componentName));
        retVal = (phGuiError_t) 0;
    }

    return retVal;
}


static void recordPlaybackData(
    struct all_t *all,
    int step,                      /* current step count */
    int retVal,                    /* return value from phGuiGetData */
    const char *componentName,     /* name of the component */
    char *data                     /* current value of component */
)
{
    if ( strcasecmp(data,getFlagStatusStr(all,componentName)) != 0 || 
        (getFlagType(all,componentName)==PAUSE && getComponentValueInt(all,data)) ||
         retVal != 0 )
    {
	fprintf(all->recordFile, "%d %d %s %s\n", 
	    step, retVal, componentName, data);

        setFlagStatusStr(all, componentName, data);
    }

    return;
}


phGuiError_t phPbGuiGetData(
    struct all_t *all,
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    const char *componentName,     /* name of the component */
    char *data,                    /* the variable where the data will be stored */
    int timeout                    /* timeout after which phGuiGetData returns */
)
{
    phGuiError_t retVal;

    if (all->doPlayback)
    {
        retVal = getPlaybackData(all, componentName, data);

        if (! all->noGUI)
            phGuiChangeValue(guiProcessID, componentName, data);

	return retVal;
    }

    if (all->noGUI)
    {
	strcpy(data, "");
	return PH_GUI_ERR_OK;
    }

    retVal = phGuiGetData(guiProcessID, componentName, data, timeout);

    if (all->doRecord)
    {
        recordPlaybackData(all,all->pbGpibStep, (int)retVal, componentName, data);
    }

    return retVal;
}

phGuiError_t phPbGuiChangeValue(
    struct all_t *all,
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    const char *componentName,     /* name of the component to change */
    const char *newValue           /* new value */
)
{
    if (all->noGUI)
	return PH_GUI_ERR_OK;
    else
	return phGuiChangeValue(guiProcessID, componentName, newValue);
}

phGuiError_t phPbGuiDestroyDialog(
    struct all_t *all,
    phGuiProcessId_t process      /* the Gui-ID */
)
{
    if (all->noGUI)
	return PH_GUI_ERR_OK;
    else
	return phGuiDestroyDialog(process);
}

phGuiError_t phPbGuiGetEvent(
    struct all_t *all,
    phGuiProcessId_t guiProcessID, /* the Gui-ID */
    phGuiEvent_t *event,           /* the variable where the event type and
                                      the triggering buttonname will be stored */
    int timeout                    /* timeout after which phGuiGetEvent returns */
)
{
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
    phGuiError_t retVal;
/*    char fname[128];*/
/*    char fdata[128];*/
/*    int fretval;*/
/*    int fstep;*/
/*    int fsubstep;*/
/* End of Huatek Modification */

    if (all->autoStartTime > 0 && !all->lotActive)
    {
	if (all->autoStartTime < timeSinceLotDone(all))
	{
	    event->type = PH_GUI_PUSHBUTTON_EVENT;
	    strcpy(event->name, "p_start");
	    event->secval = 0;
	    return PH_GUI_ERR_OK;
	}
    }

    if (all->noGUI)
    {
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

phGuiError_t phPbGuiCreateUserDefDialog(
    struct all_t *all,
    phGuiProcessId_t *id      /* the resulting Gui-ID                    */,
    phLogId_t logger          /* the data logger to be used              */,
    phGuiSequenceMode_t mode  /* mode in which the gui should be started */,
    const char *description   /* description of the gui-representation   */
)
{
    if (all->noGUI)
	return PH_GUI_ERR_OK;
    else
	return phGuiCreateUserDefDialog(id, logger, mode, description);
}

phGuiError_t phPbGuiShowDialog(
    struct all_t *all,
    phGuiProcessId_t process,      /* the Gui-ID */
    int closeOnReturn              /* if set to 1 dialog will close when
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
    phComGpibCloseVXI11(all->comId);
    phLogDestroy(all->logId);
    exit(1);    
}

static int checkGuiError(phGuiError_t guiError, struct all_t *all)
{
    switch (guiError)
    {
      case PH_GUI_ERR_OK:
	break;
      case PH_GUI_ERR_TIMEOUT:
	phLogSimMessage(all->logId, LOG_VERBOSE,
	    "received timeout error from gui server");
	break;
      default:
	phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	    "received error code %d from gui server", 
	    (int) guiError);
	return 0;
    }
    return 1;
}

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

static void beAlife(struct all_t *all)
{
    static int b = 1;

    if (b == 1)
    {
	b = 2;
	checkGuiError(phPbGuiChangeValue(all, all->guiId, "r_life_1", "False"), all);
	checkGuiError(phPbGuiChangeValue(all, all->guiId, "r_life_2", "True"), all);
    }
    else
    {
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
	checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "not understood"), all);
    else
	checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_status", "OK"), all);
}

static void doDelay(int seconds, struct all_t *all)
{
    char delay[64];
    char typed[80];

    while (seconds > 0)
    {
	sprintf(delay, "%d", seconds);
	checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_delay", delay), all);
	seconds--;
	sleep(1);
    }
    checkGuiError(phPbGuiChangeValue(all, all->guiId, "f_delay", ""), all);

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

static void notSupported(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, PHLOG_TYPE_ERROR,
	"message \"%s\" not supported by this prober simulator", m);
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

    do
    {
	setWaiting(1, all);
	comError = phComGpibSendEventVXI11(all->comId, &sendEvent, 
	    1000L * all->timeout);
	setWaiting(0, all);
	switch (checkComError(comError, all))
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
    } while (!sent);

    phLogSimMessage(all->logId, LOG_DEBUG, 
	"sent SRQ \"%s\"", message);    
}

static void sendMessage(struct all_t *all, const char *m)
{
    char message[1024];
    int sent = 0;
    phGuiError_t guiError;
    phComError_t comError;

    strcpy(message, m);
    guiError = phPbGuiChangeValue(all, all->guiId, "f_answ", message);
    checkGuiError(guiError, all);

    strcat(message, EOC);
    phLogSimMessage(all->logId, LOG_VERBOSE, 
	"trying to send message \"%s\"", m);

    do 
    {
	setWaiting(1, all);
	comError = phComGpibSendVXI11(all->comId, message, strlen(message), 
	    1000L * all->timeout);
	setWaiting(0, all);
	switch (checkComError(comError, all))
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
    } while (!sent);
    
    setSent(1, all);
    phLogSimMessage(all->logId, LOG_DEBUG, 
	"message \"%s\" sent", m);
}


static void askDie_Height(struct all_t *all)
{
    char message[64];
    static int currentZheight = 0;
    sprintf(message, "%d",all->zcurrent);
    sendMessage(all, message);
}

/*---------------------------------------------------------------------------*/

static int corruptEventMessage(struct all_t *all)
{
    int corrupt=0;

    all->corruptEvntMsgCount++;

    if ( all->corruptEvntMsg && all->corruptEvntMsgCount==CORRUPT_MSG_EVNT )
    {
        phLogSimMessage(all->logId, LOG_DEBUG, "corrupt next event or message");
        corrupt=1;
        all->corruptEvntMsgCount=0;
    }

    return corrupt;
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

static void clearWafer(struct all_t *all, struct wafer *w)
{
    int x, y;
    int xc, yc;
    int rsqr;

    xc = (all->xmax + all->xmin)/2;
    yc = (all->ymax + all->ymin)/2;
    rsqr = (all->xmax - xc)*(all->xmax - xc);

    for (x=all->xmin; x<=all->xmax; x++)
	for (y=all->ymin; y<=all->ymax; y++)
	{
	    WDie(all, w, x, y).bin = -1;
	    WDie(all, w, x, y).ink = -1;
	    WDie(all, w, x, y).flags = 
		(x-xc)*(x-xc) + (y-yc)*(y-yc) <= rsqr ? 
		DIEFLAG_EMPTY : DIEFLAG_OUTSIDE;
	}

    w->status = WAF_UNTESTED;
    phLogSimMessage(all->logId, LOG_DEBUG, "wafer data reset");
}

static int realignWafer(struct all_t *all)
{
    int p;
    int probed;

    if (all->chuck.loaded)
    {
	clearWafer(all, &(all->chuck.wafer));
	all->chuck.wafer.status = WAF_INTEST;

	all->xcurrent = all->xmin-all->maxxoff;
	all->ycurrent = all->ymin-all->maxyoff;

	probed = 0;
	for (p=0; p<all->probes; p++)
	{
	    if (InBounds(all, all->xcurrent+all->pxoff[p], all->ycurrent+all->pyoff[p]))
	    {
		if (!(Die(all, all->xcurrent+all->pxoff[p], all->ycurrent+all->pyoff[p]).flags & DIEFLAG_OUTSIDE))
		    probed = 1;
	    }
	    
	}
	if (!probed)
	    stepDie(all);

	phLogSimMessage(all->logId, LOG_DEBUG, "wafer initialized");
	return 1;
    }
    else
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "wafer not initialized, chuck empty");
	return 0;	
    }
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
    return (now.tv_sec - all->lotDoneTime);
}

static int loadWafer(struct all_t *all)
{
    if (waferCount != all->wafercount)
    {
        all->wafercount++;
	all->chuck.loaded = 1;    
	all->chuck.wafer.status = WAF_INTEST;

	phLogSimMessage(all->logId, LOG_DEBUG, "wafer loaded");
	return 1;
    }
    else
    {
	phLogSimMessage(all->logId, LOG_DEBUG, "no more wafers to load");
	all->lotActive = 0;
	setLotDoneTime(all);
	return 0;	
    }
}

static void unloadWafer(struct all_t *all)
{
    int x, y;
    char results[WAF_MAXDIM*6 + 1];
    char oneResult[8];

    all->chuck.loaded = 0;    
    all->chuck.wafer.status = WAF_TESTED;

    for (y=all->ymin; y<=all->ymax; y++)
    {
	results[0] = '\0';
	for (x=all->xmin; x<=all->xmax; x++)
	{
	    if (Die(all, x, y).flags & DIEFLAG_OUTSIDE)
		strcat(results, "  .   ");
	    else
	    {
		sprintf(oneResult, "%2d/%-2d ", 
		    Die(all, x, y).bin, Die(all, x, y).ink);
		strcat(results, oneResult);
	    }
	}
	phLogSimMessage(all->logId, LOG_DEBUG,
	    "wafer result: %s", results);
    }
    
    phLogSimMessage(all->logId, LOG_DEBUG, "wafer unloaded");
}


static void forceEndLot(struct all_t *all)
{
    unloadWafer(all);
    all->wafercount = waferCount;
    all->lotActive = 0;
    setLotDoneTime(all);
    phLogSimMessage(all->logId, LOG_DEBUG, "lot end forced");
}

static void forceStartLot(struct all_t *all)
{
    all->wafercount = 0;
    all->lotActive = 1;
    chuckUp(all, 0);
    loadWafer(all);
    realignWafer(all);
    phLogSimMessage(all->logId, LOG_DEBUG, "lot start forced");
}

static void cleanNeedles(struct all_t *all)
{
    phLogSimMessage(all->logId, LOG_DEBUG, "performing probe needle cleaning");
}

static int stepDie(struct all_t *all)
{
    int probed;
    int p;

    if (all->chuck.wafer.status == WAF_TESTED)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, "auto step past last die");
	return 0;
    }

    if (all->probes == 1)
	do
	{
	    all->xcurrent += all->xstep;
	    if (all->xcurrent > all->xmax)
	    {
		all->xcurrent = all->xmin;
		all->ycurrent += all->ystep;
		if (all->ycurrent > all->ymax)
		{
		    all->ycurrent = all->ymin;
		    all->chuck.wafer.status = WAF_TESTED;
		    phLogSimMessage(all->logId, LOG_DEBUG, "auto step past last die");
		    return 0;
		}
	    }
	} while (Die(all, all->xcurrent, all->ycurrent).flags & DIEFLAG_OUTSIDE);
    else
	do
	{
	    all->xcurrent += all->xstep;
	    if (all->xcurrent+all->minxoff > all->xmax)
	    {
		all->xcurrent = all->xmin-all->maxxoff;
		all->ycurrent += all->ystep;
		if (all->ycurrent+all->minyoff > all->ymax)
		{
		    all->ycurrent = all->ymin-all->maxyoff;
		    all->chuck.wafer.status = WAF_TESTED;
		    phLogSimMessage(all->logId, LOG_DEBUG, "auto step past last die");
		    return 0;
		}
	    }

	    probed = 0;
	    for (p=0; p<all->probes; p++)
	    {
		if (InBounds(all, all->xcurrent+all->pxoff[p], all->ycurrent+all->pyoff[p]))
		{
		    if (!(Die(all, all->xcurrent+all->pxoff[p], all->ycurrent+all->pyoff[p]).flags & DIEFLAG_OUTSIDE))
			probed = 1;
		}
	    }
	} while (!probed);
	

    phLogSimMessage(all->logId, LOG_DEBUG, "auto step to die [%d,%d]",
	all->xcurrent, all->ycurrent);
    return 1;
}

static int posOnWafer(struct all_t *all, int x, int y)
{
    if (InBounds(all, x, y) && !(Die(all, x, y).flags & DIEFLAG_OUTSIDE))
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "position [%d,%d] is on wafer", x, y);
	return 1;
    }
    else
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "position [%d,%d] is outside of wafer", x, y);
	return 0;	
    }
}

static int probeOnWafer(struct all_t *all, int probe)
{
    if (probe < 0 || probe >= all->probes)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "probe %d does not exist", probe);
	return 0;
    }
    else
	return posOnWafer(all, 
	    all->xcurrent + all->pxoff[probe],
	    all->ycurrent + all->pyoff[probe]);
}

static int setDie(struct all_t *all, int x, int y)
{
    all->xcurrent = x;
    all->ycurrent = y;
    phLogSimMessage(all->logId, LOG_DEBUG, 
        "explicit step to position [%d,%d]", x, y);
    return posOnWafer(all, x, y);
}

static int getPrimaryDie(struct all_t *all, int *x, int *y)
{
    *x = all->xcurrent;
    *y = all->ycurrent;
    phLogSimMessage(all->logId, LOG_DEBUG, 
	"current die position [%d,%d]", *x, *y);
    return 1;
}

static int getRefDie(struct all_t *all, int *x, int *y)
{
    *x = all->xref;
    *y = all->yref;
    phLogSimMessage(all->logId, LOG_DEBUG, 
	"reference die position [%d,%d]", *x, *y);
    return 1;
}

static int binDie(struct all_t *all, int bin, int probe)
{
    int x, y;

    if (probe < 0 || probe >= all->probes)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "probe %d does not exist", probe);
	return 0;
    }

    x = all->xcurrent + all->pxoff[probe];
    y = all->ycurrent + all->pyoff[probe];

    if (InBounds(all, x, y) && !(Die(all, x, y).flags & DIEFLAG_OUTSIDE))
    {
	Die(all, x, y).bin = bin;
	Die(all, x, y).flags |= DIEFLAG_BINNED;
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "die [%d,%d] binned to bin %d", x, y, bin);
	return 1;
    }
    else
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "die [%d,%d] outside wafer, not binned", x, y);
	return 0;
    }
}

static int inkDie(struct all_t *all, int ink, int probe)
{
    int x, y;

    if (probe < 0 || probe >= all->probes)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "probe %d does not exist", probe);
	return 0;
    }

    x = all->xcurrent + all->pxoff[probe];
    y = all->ycurrent + all->pyoff[probe];

    if (InBounds(all, x, y) && !(Die(all, x, y).flags & DIEFLAG_OUTSIDE))
    {
	if (ink)
	{
	    Die(all, x, y).ink = ink;
	    Die(all, x, y).flags |= DIEFLAG_INKED;
	    phLogSimMessage(all->logId, LOG_DEBUG, 
		"die [%d,%d] inked with ink %d", x, y, ink);
	}
	else
	    phLogSimMessage(all->logId, LOG_DEBUG, 
		"die [%d,%d] not inked (passed)", x, y);
	return 1;
    }
    else
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "die [%d,%d] outside wafer, not inked", x, y);
	return 0;
    }
}

static int getDipSwitch(struct all_t *all, int group, int pos)
{
    if (group < 0 || group >= DIP_GROUPS ||
	pos < 0 || pos >= DIP_POSITIONS)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, 
	    "dip request out of range: %d, %d", group, pos);
	return 0;
    }

    return all->dip[pos][group];
}

/*---------------------------------------------------------------------------*/

static int check_assist(struct all_t *all)
{
    char response[128];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_assist", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, "rise assist SRQ");
	sendSRQ(all, 0x4e);
	doDelay(5, all);
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
    if (strcasecmp(response, "true") == 0)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, "rise fatal SRQ");
	sendSRQ(all, 0x50);
	doDelay(5, all);
	phPbGuiChangeValue(all, all->guiId, "t_fatal", "false");
	return F_FATAL;
    }
    return 0;
}

static int check_wafend(struct all_t *all)
{
    char response[128];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_wafend", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, "rise wafer end SRQ");
	sendSRQ(all, 0x4b);
	doDelay(5, all);
	phPbGuiChangeValue(all, all->guiId, "t_wafend", "false");
	return F_WAFEND;
    }
    return 0;
}

static int check_lotend(struct all_t *all)
{
    char response[128];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_lotend", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
        all->wafercount =0;
        all->sendLotEnd=1;
        all->lotActive=0;
        phLogSimMessage(all->logId, LOG_DEBUG, "rise wafer end SRQ");
        sendSRQ(all, 0x4b);
        phLogSimMessage(all->logId, LOG_DEBUG, "rise lot end SRQ");
        sendSRQ(all, 0x48);
        doDelay(5, all);
        phPbGuiChangeValue(all, all->guiId, "t_lotend", "false");
        return F_LOTEND;
    }
    return 0;
}


static int check_pause(struct all_t *all)
{
    char response[128];

    response[0] = '\0';
    phPbGuiGetData(all, all->guiId, "t_pause", response, 0);
    if (strcasecmp(response, "true") == 0)
    {
	phLogSimMessage(all->logId, LOG_DEBUG, "unset prober pause flag to resume");
	if (!all->dontSendPauseSRQ)
	{
	    phLogSimMessage(all->logId, LOG_DEBUG, "rise pause SRQ");
	    sendSRQ(all, 0x62);
	}
	all->dontSendPauseSRQ = 0;
	do
	{
	    sleep(2);
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
    if (flags & F_WAFEND)
	flags_handled |= check_wafend(all);
    if (flags & F_LOTEND)
        flags_handled |= check_lotend(all);

    return flags_handled;
}

/*---------------------------------------------------------------------------*/

static void func_b(struct all_t *all, char *m)
{
    int x, y;
    int dieSet;
    char temp[10];
    char XYLocation[10];

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"b\"");
    doDelay(0, all);
    /* sscanf(m, "b%04d%04d", &x, &y); */
    
    all->zcurrent=0;

    sscanf(m, "b%8s", temp);
    if((temp[0] != '-') && (temp[0] != '+'))
    {
      strcpy(XYLocation, "+");
      strcat(XYLocation, temp);
    }
    else 
    {
      strcpy(XYLocation, temp);
    }
    sscanf(XYLocation, "%04d%04d", &x, &y);
    
    dieSet = setDie(all, x, y);
    if (check_flags(all, F_ALL) & F_WAFEND)
	return;
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
	if (all->abortProbing)
	    sendSRQ(all, 0x4b); /* abort */
	else
	{
	    if (dieSet) {
		sendSRQ(all, 0x47); /* OK */
	    }
	    else 
		sendSRQ(all, 0x4f); /* off wafer */
	}
	break;
      case MOD_20S: 
	if (dieSet)
	    sendSRQ(all, 0x47); /* OK */
	else 
	    sendSRQ(all, 0x4f); /* off wafer */
	break;
    }
}

static void func_A(struct all_t *all, char *m)
{
    char message[64];
    int x, y;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"A\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S: 
	getPrimaryDie(all, &x, &y);
        /* sprintf(message, "%04d%04d", x, y); */
        if ((x>=0) && (y>=0))
        {
          sprintf(message, "%03d%03d", x, y);
        }
        else if ((x<0) && (y>=0))
        {
          sprintf(message, "%04d%03d", x, y);
        }
        else if ((x>=0) && (y<0))   
        {
          sprintf(message, "%03d%04d", x, y);
        }
        else
        {
          sprintf(message, "%04d%04d", x, y);
        }
	
	sendMessage(all, message);
	break;
    }
}

static void func_Z(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"Z\"");
    doDelay(0, all);
    check_flags(all, F_ALL^F_PAUSE);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12: 
      case MOD_F12:
      case MOD_78S: 
      case MOD_20S:
	if (chuckUp(all, 1))
	    sendSRQ(all, 0x43); /* OK */
	else
	    sendSRQ(all, 0x53); /* Chuck limit reached */
	break;
    }
}

/*CR21589, Kun Xiao, 29 Jun 2006*/
static void func_ZStep(struct all_t *all, char *m)
{
    int step;
    sscanf(m, "Z%d", &step);


    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"Z+-\"");
    doDelay(0, all);
    check_flags(all, F_ALL^F_PAUSE);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
      {
        int stepResult = SUCCEED;

        if (step > 0) 
        {
          phLogSimMessage(all->logId, LOG_DEBUG, "chuck up by stepping %d", step);
          if ((all->zcurrent + step) > all->zlimit)
          {
            phLogSimMessage(all->logId, LOG_DEBUG, "up limit of z-height achieved, chuck up by stepping %d failed!", step);
            all->zcurrent = all->zlimit;
            stepResult = FAIL;
          }
        }
        else
        {
          phLogSimMessage(all->logId, LOG_DEBUG, "chuck down by stepping %d", step);
          if ((all->zcurrent + step) < all->zmin)
          {
            phLogSimMessage(all->logId, LOG_DEBUG, "down limit of z-height achieved, chuck down by stepping %d failed!", step);
            all->zcurrent = all->zmin;
            stepResult = FAIL;
          }
        }

        if (stepResult == SUCCEED)
        {
          all->zcurrent = all->zcurrent + step;
          sendSRQ(all, 0x43); /* OK */
        }
        else
        {
          sendSRQ(all, 0x53); /* Chuck limit reached */
        }
        break;
      }
    } /* end for switch all->model */
    

}

/*CR21589, Kun Xiao, 29 Jun 2006*/
static void func_z(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"z\"");
    doDelay(0, all);
    check_flags(all, F_ALL^F_PAUSE); 
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
        all->zcontact = all->zcurrent;
        phLogSimMessage(all->logId, LOG_VERBOSE, "contact position is set: %d", all->zcontact);
        sendSRQ(all, 0x59); /* OK */
        break;
    }
}

static void func_D(struct all_t *all, char *m)
{
    /* move chuck down */
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"D\"");
    doDelay(0, all);
    check_flags(all, F_ALL^F_PAUSE);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
	chuckUp(all, 0);
    all->zcurrent = 0; /*CR21589, kunxiao, 29 Jun 2006*/
	sendSRQ(all, 0x44); /* OK */
	break;
    }
}

static void func_e(struct all_t *all, char *m)
{
    /* pause the prober */
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"e\"");
    doDelay(0, all);
    check_flags(all, F_ALL^F_PAUSE);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
	phPbGuiChangeValue(all, all->guiId, "t_pause", "true");
	all->dontSendPauseSRQ = 1;
	sleep(2);
	if (! (check_flags(all, F_PAUSE|F_WAFEND) & F_WAFEND))
	    sendSRQ(all, 0x59); /* OK, regular unpaused */
	break;
    }
}

static void func_O(struct all_t *all, char *m)
{
    char message[1024];
    int probe;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"O\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_20S:
	if (all->asciiOCommand)
	{
	    sprintf(message, "00000000");
	    for (probe=0; probe<8 && probe<all->probes; probe++)
            {
		message[probe] = probeOnWafer(all, probe) ? '1' : '0';
            }
	    break;
	}
	/* else fall through to next case ... */
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
        int count = (all->probes -1)/4 +1;
        if(count < 8)
        {
          count = 8;
        }
        for (probe=0; probe<MAX_PROBES && probe<count; probe++)
        {
          message[probe]='@';
        }

	for (probe=0; probe<MAX_PROBES && probe<all->probes; probe++)
	    if (probeOnWafer(all, probe))
		message[probe/4] |= (1 << probe%4);
	message[1+(probe-1)/4] = '\0';
	break;
    }
    sendMessage(all, message);	
}

static void func_C0(struct all_t *all, char *m)
{
    int bin;
    int p;
    int probe;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"C0\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8: 
      case MOD_P12:
      case MOD_F12:
      case MOD_78S: 
        notSupported(all, m); break;
      case MOD_20S:
	for (probe=0; probe<all->probes; probe++)
	{
	    inkDie(all, (m[1+(probe/4)*33] & (1 << probe)) ? 1 : 0, probe);
	    bin = -1;
	    for (p=0; p<8; p++)
	    {
	        switch (m[2+(probe/4)*33+(probe%4)*8+p])
	        {
	          case '0': break;
	          case '1': bin = 4*p + 0; break;
		  case '2': bin = 4*p + 1; break;
		  case '4': bin = 4*p + 2; break;
		  case '8': bin = 4*p + 3; break;
		}
	    }
	    if (all->reverseBinning)
		binDie(all, bin, all->probes - probe - 1);
	    else
		binDie(all, bin, probe);
	}
	check_flags(all, F_ALL^F_PAUSE);

        if ( corruptEventMessage(all) )
        {
            phLogSimMessage(all->logId, LOG_DEBUG, "0x59 not sent");
        }
        else
        {
	    sendSRQ(all, 0x59); /* Test Complete */
        }
	break;
    }
}

static void func_CA(struct all_t *all, char *m)
{
    int bin;
    int p;
    int probe;

    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"C@\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8: 
      case MOD_78S: 
	for (probe=0; probe<all->probes; probe++)
	{
	    inkDie(all, (m[1+(probe/4)*33] & (1 << probe)) ? 1 : 0, probe);
	    bin = -1;
	    for (p=0; p<8; p++)
	    {
	        switch (m[2+(probe/4)*33+(probe%4)*8+p])
		{
		  case '@': break;
		  case 'A': bin = 4*p + 0; break;
		  case 'B': bin = 4*p + 1; break;
		  case 'D': bin = 4*p + 2; break;
		  case 'H': bin = 4*p + 3; break;
		}
	    }
	    binDie(all, bin, probe);
        }
	sendSRQ(all, 0x59); /* Test Complete */
	check_flags(all, F_ALL^F_PAUSE);
	break;	

      case MOD_P12:
      case MOD_F12:
        /* CR 15358 */
        for(probe=0; probe<all->probes; probe++) {
          inkDie(all, (m[1+(probe/4)*33] & (1 << probe)) ? 1 : 0, probe);
          bin = -1;
          if(all->binningMethod == 0) {
            for(p=0; p<8; p++) {
              switch(m[2+(probe/4)*33+(probe%4)*8+p]) {
                case '@': break;
                case 'A': bin = 4*p + 0; break;
                case 'B': bin = 4*p + 1; break;
                case 'D': bin = 4*p + 2; break;
                case 'H': bin = 4*p + 3; break;
              }
            }
          }else{
            /* This is the binary binning method for P12 model, just collect byte0 and byte1 */
            bin = ((m[2+(probe/4)*33+(probe%4)*8+1] & 0x0F)<<4) | (m[2+(probe/4)*33+(probe%4)*8] & 0x0F);
          }
          binDie(all, bin, probe);
        }
        check_flags(all, F_ALL^F_PAUSE);
        sendSRQ(all, 0x59);  /* Test Complete */
        break;

      case MOD_20S: 
        notSupported(all, m); 
	break;
    }
}

static void func_Q(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"Q\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
	inkDie(all, 0, 0);
	if (check_flags(all, F_ALL) & F_WAFEND)
	    /* forced wafer end already sent */
	    break;
	if (stepDie(all))
	    sendSRQ(all, 0x47); /* index complete */
	else
	    sendSRQ(all, 0x4b); /* end of wafer */
	break;
    }
}

static void func_r(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"r\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
	inkDie(all, 1, 0);
	if (check_flags(all, F_ALL) & F_WAFEND)
	    /* forced wafer end already sent */
	    break;
	if (stepDie(all))
	    sendSRQ(all, 0x47); /* index complete */
	else
	    sendSRQ(all, 0x4b); /* end of wafer */
	break;
    }
}

static void func_B(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"B\"");
    doDelay(2, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12: 
      case MOD_F12:
      case MOD_78S:
      case MOD_20S: 
	realignWafer(all);
	chuckUp(all, 0);
	check_flags(all, F_ALL);
	sendSRQ(all, 0x46); /* Reference die positioned */
    }
}

static void func_a(struct all_t *all, char *m)
{
    char message[64];
    int x, y;
    
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"a\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12: 
      case MOD_F12:
      case MOD_78S:
      case MOD_20S: 
	getRefDie(all, &x, &y);
        /* sprintf(message, "%04d%04d", x, y); */
  
        if ((x>=0) && (y>=0))
        {
          sprintf(message, "%03d%03d", x, y);
        }
        else if ((x<0) && (y>=0))
        {
          sprintf(message, "%04d%03d", x, y);
        }
        else if ((x>=0) && (y<0))   
        {
          sprintf(message, "%03d%04d", x, y);
        }
        else
        {
          sprintf(message, "%04d%04d", x, y);
        }

	sendMessage(all, message);
	break;
    }
}

static void func_U(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"U\"");
    doDelay(2, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
        unloadWafer(all);
	check_flags(all, F_ALL);
	if (loadWafer(all))
	{
	    realignWafer(all);
	    sendSRQ(all, 0x46); /* next wafer aligned */
	}
	else
	{
	    sendSRQ(all, 0x48); /* no more wafers */
	    if (all->sendLotEnd)
		sendSRQ(all, 0x4e); /* lot end */
	}
    }
}

static void func_VF(struct all_t *all, char *m)
{
    /* force lot end */
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"VF\"");
    doDelay(2, all);
    check_flags(all, F_ALL);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
        forceEndLot(all);
	check_flags(all, F_ALL);
	sendSRQ(all, 0x48); /* no more wafers */
	break;
    }
}

static void func_p(struct all_t *all, char *m)
{
    /* probe needle cleaning */
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"p\"");
    doDelay(1, all);
    check_flags(all, F_ALL);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
        cleanNeedles(all);
	check_flags(all, F_ALL);
	sendSRQ(all, 0x59); /* needles cleaned */
	break;
    }
}
static void func_b_startLot(struct all_t *all);
static void func_F(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"F\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
	sendMessage(all, "314");
	break;
    }
    if(all->enableAutoStartFeature)
        func_b_startLot(all);
}

static void func_qV(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"?V\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
	sendMessage(all, 
	    "sn345678901234567890,rev45678901234,P-8,0000000000000000");
	break;
      case MOD_P12:
        sendMessage(all, 
                    "sn345678901234567890,rev45678901234,P-12,0000000000000000");
        break;
      case MOD_F12:
        sendMessage(all,
                    "sn345678901234567890,rev45678901234,F-12,0000000000000000");
        break;
      case MOD_78S:
	sendMessage(all, "S123456789,V00.00,78S,L");
	break;
      case MOD_20S:
	sendMessage(all, "");
	break;
    }
}

static void func_G(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"G\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
	sendMessage(all,
	    "wafer-12345         "
	    "1"
	    "1"
	    "12345"
	    "12345"
	    "123"
	    "12"
	    "1"
	    "12"
	    "1"
	    "123"
	    "123"
	    "000000000000000"
	    "1"
	    "1"
	    "12"
	    "1"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0");
	break;
      case MOD_78S:
	sendMessage(all,
	    "wafer-12345         "
	    "1"
	    "1"
	    "12345"
	    "12345"
	    "123"
	    "12"
	    "1"
	    "12"
	    "1"
	    "123"
	    "123"
	    "000000000000000"
	    "1"
	    "1"
	    "12"
	    "1"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0");
	break;
      case MOD_20S:
	sendMessage(all,
	    "su-filename "

	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"

	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"
	    "0000000000"

	    "0000000000");
	break;
    }
}

static void func_W(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"W\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8: 
      case MOD_P12:
      case MOD_F12:
      case MOD_78S: 
        notSupported(all, m); break;
      case MOD_20S:
	if (getDipSwitch(all, 19, 4))
	    sendMessage(all, 
		"2"
		"1111111111111111111111111"
		"1111111111111111111111111"
		"1111111111111111111111111"
		"1111111111111111111111111");
	else
    {
        /* send a 18 char string */
        /*sendMessage(all, "wafer-12345       ");*/
        char id_str[128] = {0};
        sprintf(id_str, "%s%04d", all->waferIdPrefix,all->wafercount);
        phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"n\"");
        sendMessage(all, id_str);
    }
	break;
    }
}

//wafer ID
static void func_n(struct all_t *all, char *m)
{
#if 0
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"n\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
	sendMessage(all, "wafer-12345     9    ");
	break;
      case MOD_20S: notSupported(all, m); break;
    }
#endif
    char id_str[128] = {0};
    sprintf(id_str, "%s%04d", all->waferIdPrefix,all->wafercount);
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"n\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
          sendMessage(all, id_str);
          break;
      case MOD_20S: notSupported(all, m); break;
    }
}

static void func_g(struct all_t *all, char *m)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"g\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
	sendMessage(all, "101Lot3456789ABCDEFGHIJ");
	break;
      case MOD_20S:
	sendMessage(all, "101Lot3456789ABCDEFGH0000000000PID0AOIDMV");
	break;
    }
}

void func_b_startLot(struct all_t *all)
{
    phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"Start\"");
    doDelay(0, all);
    switch (all->model)
    {
      case MOD_P8:
      case MOD_P12:
      case MOD_F12:
      case MOD_78S:
      case MOD_20S:
	forceStartLot(all);
	if (all->sendFirstWaferSRQ)
	{
            if ( corruptEventMessage(all) )
            {
                phLogSimMessage(all->logId, LOG_DEBUG, "0x4a not sent");
            }
            else
            {
	        sendSRQ(all, 0x4a);
            }
	    doDelay(5, all);
	}
        if ( corruptEventMessage(all) )
        {
            phLogSimMessage(all->logId, LOG_DEBUG, "0x46 not sent");
        }
        else
        {
	    sendSRQ(all, 0x46);
        }
	doDelay(5, all);
	break;
    }    
}

static void func_b_autoz(struct all_t *all)
{
    phLogSimMessage(all->logId, LOG_NORMAL, "handling \"AUTOZ\"");
    doDelay(0, all);
    sendSRQ(all, 0x6C);
}

/* Wafer Size */
static void func_WS(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"WS\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "120");  /* the wafer size is 120mm */
    case MOD_78S:
    case MOD_20S: 
      notSupported(all, m); 
      break;
  }
}

/* Wafer Flat */
static void func_WF(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"WF\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "180");  /* the wafer flat is 180deg */
      break;
    case MOD_78S:
    case MOD_20S: 
      notSupported(all, m); 
      break;
  }
}

/* Die height and Die width */
static void func_DD(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"DD\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "X123.4Y678.9");  /* x=123.4, y=678.9 */
      break;
    case MOD_78S:
    case MOD_20S: 
      notSupported(all, m); 
      break;
  }
}

/* Wafer and Die Units */
static void func_WU(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"WU\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "11");  /* a=1:mm, b=1:um */
      break;
    case MOD_78S:
    case MOD_20S: 
      notSupported(all, m); 
      break;
  }
}

/* Center X and Center Y */
static void func_CD(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"CD\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "X003Y004");  /* X=003, Y=004 */
      break;
    case MOD_78S:
    case MOD_20S: 
      notSupported(all, m); 
      break;
  }
}

/* Positive X and Positive Y */
static void func_WD(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"WD\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "3");  /* N=3, Quadrant I, so Left->Right and Botttom->Up */
      break;
    case MOD_78S:
    case MOD_20S: 
      notSupported(all, m); 
      break;
  }
}

/* Cassette status */
static void func_S(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"S\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "012420");
      break;
    case MOD_78S:
    case MOD_20S:
      notSupported(all, m);
      break;
  }
}

/* wafer number */
static void func_w(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"w\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12: /* ? */
    case MOD_F12:
      sendMessage(all, "12");
      break;
    case MOD_78S:
    case MOD_20S:
      notSupported(all, m);
      break;
  }
}

/* error code */
static void func_s(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"s\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "1534");
      break;
    case MOD_78S:
    case MOD_20S:
      notSupported(all, m);
      break;
  }
}

/* hot chuck probing temperature */
static void func_f1(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"f1\"");
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, "050.5");
      break;
    case MOD_78S:
    case MOD_20S:
      notSupported(all, m);
      break;
  }
}

/* hot chuck setting temperature */
static void func_f2(struct all_t *all, char *m)
{
  phLogSimMessage(all->logId, LOG_VERBOSE, "handling \"f2\"");
  char messages[2][10] = {"085.5", "-085"};
  static int index = 0;
  doDelay(0, all);
  switch (all->model) {
    case MOD_P8:
    case MOD_P12:
    case MOD_F12:
      sendMessage(all, messages[index++]);
      break;
    case MOD_78S:
    case MOD_20S:
      notSupported(all, m);
      break;
  }
  if (index >= 2)
    index = 0;
}

/*---------------------------------------------------------------------------*/

static int ignore_query(struct all_t *all, char *m)
{
    int i;
    int query_ignored=0;

    for ( i=0 ; i< all->ignore_query_command_counter && !query_ignored ; ++i )
    {
        if ( phToolsMatch(m, all->ignore_query_command[i] ) || 
             strcmp(m, all->ignore_query_command[i] ) == 0 )
        {
            query_ignored=1;
        }
    }

    return query_ignored;
}


static void handleMessage(struct all_t *all)
{
    char m[10240];

    /* copy message and strip EOC */
    strcpy(m, all->message);
    if (strlen(m) >= strlen(EOC) &&
	strcmp(&m[strlen(m) - strlen(EOC)], EOC) == 0)
	m[strlen(m) - strlen(EOC)] = '\0';

    /* show message and log it */
    phPbGuiChangeValue(all, all->guiId, "f_req", m);
    phLogSimMessage(all->logId, LOG_DEBUG, "received: \"%s\"", all->message);
    phPbGuiChangeValue(all, all->guiId, "f_answ", "");
    phPbGuiChangeValue(all, all->guiId, "f_srq", "");

    /* handle the message */
    setNotUnderstood(0, all);

    /* make sure query should not be ignored */
    if ( ignore_query(all, m) )
    {
        phLogSimMessage(all->logId, LOG_DEBUG, "ignored: \"%s\"", m);
	setNotUnderstood(1, all);
    }

    /* index die and chuck */     
    /* else if (phToolsMatch(m, "b[+-0-9]{6,8}"))*/
    else if (phToolsMatch(m, "b[\\+\\-]?[0-9]{3}[\\+\\-]?[0-9]{3}"))
      func_b(all, m);
    else if (phToolsMatch(m, "A"))
	    func_A(all, m);
    else if (phToolsMatch(m, "Z"))
	    func_Z(all, m);
    else if (phToolsMatch(m, "Z[\\+\\-][0-9]{2}"))
      func_ZStep(all, m);
    else if (phToolsMatch(m, "z"))
      func_z(all, m); 
    else if (phToolsMatch(m, "O"))
	    func_O(all, m);
    else if (phToolsMatch(m,
	         "C([0-?]([01248]{8})([01248]{8})([01248]{8})([01248]{8}))*([0-?]([01248]{8})([01248]{8})?([01248]{8})?([01248]{8})?)"))
	func_C0(all, m);

    else if(phToolsMatch(m, 
                      "C([@-O]([@-O]{8})([@-O]{8})([@-O]{8})([@-O]{8}))*([@-O]([@-O]{8})([@-O]{8})?([@-O]{8})?([@-O]{8})?)"))
  /*else if(phToolsMatch(m, 
      "C([@-O]([@ABDH]{8})([@ABDH]{8})([@ABDH]{8})([@ABDH]{8}))*([@-O]([@ABDH]{8})([@ABDH]{8})?([@ABDH]{8})?([@ABDH]{8})?)"))*/
  /* 
  * The binning data will be like "C@[@-O][@-O]@@@@@@\r\n"
  * CR 15358
  */
      func_CA(all, m);

    else if (phToolsMatch(m, "Q"))
	func_Q(all, m);
    else if (phToolsMatch(m, "r"))
	func_r(all, m);
    else if (phToolsMatch(m, "D"))
	func_D(all, m);
    else if (phToolsMatch(m, "e"))
	func_e(all, m);

    /* wafer */
    else if (phToolsMatch(m, "B"))
	func_B(all, m);
    else if (phToolsMatch(m, "a"))
	func_a(all, m);
    else if (phToolsMatch(m, "U"))
	func_U(all, m);
    else if (phToolsMatch(m, "VF"))
	func_VF(all, m);
    else if (phToolsMatch(m, "p"))
	func_p(all, m);

    /* ids */
    else if (phToolsMatch(m, "F"))
	func_F(all, m);
    else if (phToolsMatch(m, "\\?V"))
	func_qV(all, m);
    else if (phToolsMatch(m, "W"))
	func_W(all, m);
    else if (phToolsMatch(m, "G"))
	func_G(all, m);
    else if (phToolsMatch(m, "n"))
	func_n(all, m);
    else if (phToolsMatch(m, "g"))
	func_g(all, m);

    /* get status */
    else if(phToolsMatch(m, "WS")) {
      func_WS(all, m);
    } else if(phToolsMatch(m, "DD")) {
      func_DD(all, m);
    } else if(phToolsMatch(m, "WU")) {
      func_WU(all, m);
    } else if(phToolsMatch(m, "WF")) {
      func_WF(all, m);
    } else if(phToolsMatch(m, "CD")) {
      func_CD(all, m);
    } else if(phToolsMatch(m, "WD")) {
      func_WD(all, m);
    } else if(phToolsMatch(m, "S")) {
      func_S(all, m);
    } else if(phToolsMatch(m, "w")) {
      func_w(all, m);
    } else if(phToolsMatch(m, "s")) {
      func_s(all, m);
    } else if(phToolsMatch(m, "f1")) {
      func_f1(all, m);
    } else if(phToolsMatch(m, "f2")) {
      func_f2(all, m);
    } else if ( phToolsMatch(m, "\\?Z")) {
        askDie_Height(all);
    }
    
    /* else */
    else
	setNotUnderstood(1, all);
}

/*---------------------------------------------------------------------------*/

static void handleGuiEvent(struct all_t *all)
{
    phGuiError_t guiError;
    char request[10240];
    phComError_t comError;
    phComGpibEvent_t sendEvent;

    switch (all->guiEvent.type)
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
	guiError = phPbGuiChangeValue(all, all->guiId, "f_event", request);
	checkGuiError(guiError, all);

	/* now handle the button press event */
	if (strcmp(all->guiEvent.name, "p_quit") == 0)
	{
	    phPbGuiDestroyDialog(all, all->guiId);
        all->guiId = NULL;
	    all->done = 1;
	}
	else if (strcmp(all->guiEvent.name, "p_start") == 0)
	{
	    func_b_startLot(all);
	}
    else if (strcmp(all->guiEvent.name, "p_autoz") == 0)
    {
        func_b_autoz(all);
    }
	else if(strcmp(all->guiEvent.name, "p_req") == 0)
	{
	    guiError = phPbGuiChangeValue(all, all->guiId, "f_req", "");
	    checkGuiError(guiError, all);

	    setWaiting(1, all);
	    comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length, 1000L * all->timeout);
	    setWaiting(0, all);
	    switch (checkComError(comError, all))
	    {
	      case 0:
		strcpy(request, all->message);
		if (strlen(request) >= strlen(EOC) &&
		    strcmp(&request[strlen(request) - strlen(EOC)], EOC) == 0)
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
	    guiError = phPbGuiChangeValue(all, all->guiId, "f_req", request);
	    checkGuiError(guiError, all);
	}
	else if(strcmp(all->guiEvent.name, "p_answ") == 0)
	{
	    guiError = phPbGuiGetData(all, all->guiId, "f_answ", request, 5);
	    if (!checkGuiError(guiError, all))
		return;
	    phLogSimMessage(all->logId, LOG_DEBUG,
		"trying to send message '%s'", request);
	    strcat(request, EOC);
	    setWaiting(1, all);
	    comError = phComGpibSendVXI11(all->comId, request, strlen(request), 1000L * all->timeout);
	    setWaiting(0, all);
	    switch (checkComError(comError, all))
	    {
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
	}
	else if(strcmp(all->guiEvent.name, "p_srq") == 0)
	{
	    sendEvent.type = PHCOM_GPIB_ET_SRQ;
	    guiError = phPbGuiGetData(all, all->guiId, "f_srq", request, 5);
	    if (!checkGuiError(guiError, all))
		return;
	    sendEvent.d.srq_byte = (unsigned char) strtol(request, NULL, 0);
	    setWaiting(1, all);
	    comError = phComGpibSendEventVXI11(all->comId, &sendEvent, 1000L * all->timeout);
	    setWaiting(0, all);
	    if (!checkComError(comError, all))
	    {
		guiError = phPbGuiChangeValue(all, all->guiId, "f_srq", "");
		checkGuiError(guiError, all);
	    }
	}
	else if(strcmp(all->guiEvent.name, "p_event") == 0)
	{
	    strcpy(request, "");
	    guiError = phPbGuiChangeValue(all, all->guiId, "f_event", request);
	    checkGuiError(guiError, all);
	}
	break;
    }
}

/*---------------------------------------------------------------------------*/

static void doWork(struct all_t *all)
{
    phGuiError_t guiError;
    phComError_t comError;

    setError(0, all);
    all->done = 0;
    do
    {
	beAlife(all);

	comError = phComGpibReceiveVXI11(all->comId, &all->message, &all->length,
	    1000000L);
	if (checkComError(comError, all) == 0)
	{
	    doGpibStep(all);
	    handleMessage(all);
	}

	guiError = phPbGuiGetEvent(all, all->guiId, &all->guiEvent, 0);
	checkGuiError(guiError, all);
	if (guiError == PH_GUI_ERR_OK)
	{
	    handleGuiEvent(all);
	}

	doSubStep(all);
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

    while ((c = getopt(argc, argv, ":d:i:t:q:m:M:p:s:w:z:ADrafZxo:P:S:C:nF:")) != -1)
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
	    if (strcmp(optarg, "P8") == 0) all->model = MOD_P8;
            else if(strcmp(optarg, "P12") == 0) all->model = MOD_P12;
            else if(strcmp(optarg, "F12") == 0) all->model = MOD_F12;
	    else if (strcmp(optarg, "78S") == 0) all->model = MOD_78S;
	    else if (strcmp(optarg, "20S") == 0) all->model = MOD_20S;
	    else errflg++;
	    break;
	  case 'p':
	    all->probes = 0;
	    pos = optarg;
	    if (strchr(pos, '[') == NULL)
	    {
	      int sitesnumber = atoi(optarg);
	      int radical = sqrt(sitesnumber);
	      int x = 0;
	      int y = 0;
	      for (x=0 ; x<=radical ; ++x )
	      {
	        for (y=0 ; y<=radical ; ++y )
	        {
	          if(sitesnumber==0)
	          {
	            break;
	          }
	          all->pxoff[all->probes]=x;
	          all->pyoff[all->probes]=y;
	          if (all->pxoff[all->probes] > all->maxxoff)
                      all->maxxoff = all->pxoff[all->probes];
                  if (all->pxoff[all->probes] < all->minxoff)
                      all->minxoff = all->pxoff[all->probes];
                  if (all->pyoff[all->probes] > all->maxyoff)
                      all->maxyoff = all->pyoff[all->probes];
                  if (all->pyoff[all->probes] < all->minyoff)
                      all->minyoff = all->pyoff[all->probes];

                  all->probes++;
                  sitesnumber--;
	        }
	      }
	    }
	    else
	    {
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
              }
	    }
	    if (all->probes == 0)
		errflg++;
	    break;
	  case 's':
	    if (sscanf(optarg, "%d,%d", &(all->xstep), &(all->ystep)) != 2)
		errflg++;
	    break;
	  case 'q':
            if ( all->ignore_query_command_counter >= MAX_QUERY_COMMAND_IGNORE )
            {
		errflg++;
            }
            else
            {
                all->ignore_query_command[all->ignore_query_command_counter] = strdup(optarg);
                ++(all->ignore_query_command_counter);
            }
	    break;
	  case 'w':
	    if (sscanf(optarg, "%d,%d,%d,%d,%d,%d", 
		&(all->xmin), &(all->ymin), 
		&(all->xmax), &(all->ymax), 
		&(all->xref), &(all->yref)) != 6)
		errflg++;
	    break;
      case 'F':
        strncpy(all->waferIdPrefix,optarg,100);
        break;
	  case 'D':
	    all->interactiveDelay = 1;
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
	  case 'x':
            all->corruptEvntMsg = 1;
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
	  case 'S':
	    all->autoStartTime = atoi(optarg);
	    break;
	  case 'n':
	    all->noGUI = 1;
	    break;
	  case 'Z':
	    all->createNewPlayback = 1;
	    break;

      /*CR21589, kunxiao, 29 Jun 2006*/ 
      case 'z':
        all->zlimit = atoi(optarg);
        break;


      /* CR 15358 */
      case 'M':
        if(strcmp(optarg, "parity") == 0)
          all->binningMethod = 0;
        else if(strcmp(optarg, "binary") == 0)
          all->binningMethod = 1;
        else
          errflg++;
        break;
      case 'C':
            waferCount=atoi(optarg);
            break;
      case 'A':
            all->enableAutoStartFeature=1;
            break;
	  case ':':
	    errflg++;
	    break;
	  case '?':
	    fprintf(stderr, "Unrecognized option: - %c\n", (char) optopt);
	    errflg++;
        break;
      default:
            errflg++;
            break;
	}
	if (errflg) 
	{
	    fprintf(stderr, "usage: %s [<options>]\n", argv[0]);
	    fprintf(stderr, " -d <debug-level         -1 to 4, default 0\n");
	    fprintf(stderr, " -D                      perform interactive stepping\n");
	    fprintf(stderr, " -i <gpib-interface>     GPIB interface device, default hpib\n");
	    fprintf(stderr, " -t <timeout>            GPIB timeout to be used [msec], default 5000\n");
            fprintf(stderr, " -m <prober model>       prober model, one of:\n");
            fprintf(stderr, "                         P8, P12, F12, 78S, 20S, default 20S\n");
            fprintf(stderr, " -M <binning method>     binning method (only valid for P12/F12 model), one of :\n");
            fprintf(stderr, "                         parity, binary\n");
	    fprintf(stderr, " -p [<x>,<y>]+           probe coordinate list in detail like [0,0] or only provide site number \n");
	    fprintf(stderr, " -s <x>,<y>              probe step size, default 1,1\n");
	    fprintf(stderr, " -w <xmin>,<ymin>,<xmax>,<ymax>,<xref>,<yref>\n");
	    fprintf(stderr, "                         wafer layout, default 0,0,4,4,0,0\n");
	    fprintf(stderr, " -q <query-command>      ignore <query-command> \n");
	    fprintf(stderr, "                         may set to \"?V\" or \"VF\" \n");
	    fprintf(stderr, " -r                      use reverse binning\n");
	    fprintf(stderr, " -a                      use ASCII style 'O' command\n");
	    fprintf(stderr, " -f                      send first wafer SRQ\n");
            fprintf(stderr, " -x                      corrupt 1st and every %dth (selected) message or event\n",
                                                      CORRUPT_MSG_EVNT);
	    fprintf(stderr, " -o <record file>        record GUI states\n");
	    fprintf(stderr, " -P <playback file>      play back GUI states\n");
	    fprintf(stderr, " -S <seconds>            auto start lot this time after lot end\n");
	    fprintf(stderr, " -n                      no GUI\n");
	    fprintf(stderr, " -Z                      convert <playback file> into <record file> with \n");
	    fprintf(stderr, "                         new format \n");
            fprintf(stderr, " -z                      set z up limit, default is 1000 \n"); /*CR21589, Kun Xiao, 29 Jun 2006*/
        fprintf(stderr, " -C <wafer count>         set wafer count, default 5\n");
        fprintf(stderr, " -A                       enable auto start\n");
        fprintf(stderr, " -F                       specify the prefix of wafer ID\n");
        fprintf(stderr, " \n");
        fprintf(stderr, "Sample command with probe coordinate list in detail : \n");
        fprintf(stderr, "./simulator -d 1 -w 2,2,8,8,5,5 -m P8 -S 20 -p [0,0][1,1][2,2][3,3] -s 1,1 \n");
        fprintf(stderr, "Sample command with site number and simulator will generate probe coordinate list automatically: \n");
        fprintf(stderr, "./simulator -d 1 -w 2,2,8,8,5,5 -m P8 -S 20 -p 4 -s 1,1 \n");
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

    phLogMode_t modeFlags = PHLOG_MODE_NORMAL;

    int i, j;
    phGuiError_t guiError;

    /* Disable the generation of the HVM alarm */
    setenv("DISABLE_HVM_ALARM_GENERATION_FOR_TCI", "1", 1);

    /* init defaults */
    allg.debugLevel = 0;
    allg.hpibDevice = strdup("hpib");
    allg.timeout = 5000;
    allg.modelname = strdup("P8");
    allg.model = MOD_20S;
    allg.reverseBinning = 0;
    allg.asciiOCommand = 0;
    allg.sendFirstWaferSRQ = 0;
    allg.dontSendPauseSRQ = 0;
    allg.binningMethod = 0;   /* CR 15358 */
    allg.noGUI = 0;
    allg.autoStartTime = 0;
    allg.enableAutoStartFeature=0;

    allg.corruptEvntMsg = 0;
    allg.corruptEvntMsgCount = CORRUPT_MSG_EVNT - 1;
                             /* ensure 1st event or message is corrupted */
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
    allg.zmin = Z_MIN;     /*CR21589, Kun Xiao, 29 Jun 2006*/
    allg.zlimit = Z_LIMIT; /*CR21589, Kun Xiao, 29 Jun 2006*/
    allg.zcontact = -1;    /*CR21589, Kun Xiao, 29 Jun 2006*/

    allg.lotActive = 0;
    allg.lotDoneTime = 0;

    allg.ignore_query_command_counter = 0;

    allg.interactiveDelay = 0;
    allg.interactiveDelayCount = 0;

    allg.pbGpibStep = 0;
    allg.pbSubStep = 0;
    allg.doRecord = 0;
    allg.doPlayback = 0;
    allg.recordFile = NULL;
    allg.playbackFile = NULL;
    // set default wafer ID prefix to wafer- 
    strcpy(allg.waferIdPrefix,"wafer-");
    

    for (i=PAUSE ; i<FLAG_END ; ++i )
    {
        allg.flagStatus[i]=0;
    }
    allg.recordUsed = 1;
    allg.recordStep = 0;
    allg.createNewPlayback = 0;

    /* get real operational parameters */
    if (!arguments(argc, argv, &allg))
	exit(1);

    /* init internal values */
    allg.master = 1;
    allg.sendLotEnd = 0;
    allg.abortProbing = 0;
    allg.wafercount = 0;
    for (i=0; i<DIP_GROUPS; i++)
	for (j=0; j<DIP_POSITIONS; j++)
	{
	    allg.dip[j][i] = 0;
	}

    /* open message logger */
    switch (allg.debugLevel)
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
      case 6:
        modeFlags = phLogMode_t( PHLOG_MODE_ALL );
        break;
    }
    phLogInit(&allg.logId, modeFlags, "-", NULL, "-", NULL, 0);
    if ( allg.createNewPlayback )
    {
        if ( !allg.doPlayback || !allg.doRecord )
        {
            phLogSimMessage(allg.logId, PHLOG_TYPE_ERROR, "must include -o and -P option with the -Z option");
        }
        else
        {
            phLogSimMessage(allg.logId, LOG_NORMAL, "create new playback file ");
            while ( getPlaybackRecord(&allg) )
            {
                recordPlaybackData(&allg,allg.recordStep, allg.recordRtnVal, allg.recordName, allg.recordData);
            }
        }
        doExitFail(&allg);
    }

    /* open gpib communication */
    if (phComGpibOpenVXI11(&allg.comId, PHCOM_ONLINE,
	allg.hpibDevice, 0, allg.logId, PHCOM_GPIB_EOMA_NONE) != PHCOM_ERR_OK)
	exit(1);

    /* create user dialog */
    guiError = phPbGuiCreateUserDefDialog(&allg, &allg.guiId, allg.logId, 
	PH_GUI_COMM_ASYNCHRON, 	slaveGuiDesc);
    checkGuiError(guiError, &allg);
    if (guiError != PH_GUI_ERR_OK)
	exit(1);
    phPbGuiShowDialog(&allg, allg.guiId, 0);

    /* enter work loop */
    setLotDoneTime(&allg);
    doWork(&allg);

    return 0;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
