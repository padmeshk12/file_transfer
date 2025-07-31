/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : GuiServer.c
 * CREATED  : 26 Oct 1999
 *
 * CONTENTS : Server for synchronous and asynchronous GUIs
 *
 * AUTHORS  : Ulrich Frank, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 26 Oct 1999, Ulrich Frank, created
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
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/time.h>
/*Begin of Huatek Modifications, Donnie tu,04/06/2001*/
/*Issue Number: 188*/
#include <dirent.h>
/*End of Huatek Modifications*/
/*--- module includes -------------------------------------------------------*/

#include "ph_GuiServer.h"
#include "GuiProcess.h"
#include "Semaphore.h"
#include "ph_log.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define LOG_NORMAL  PHLOG_TYPE_MESSAGE_2
#define LOG_DEBUG   PHLOG_TYPE_MESSAGE_3
#define LOG_VERBOSE PHLOG_TYPE_MESSAGE_4

/* 
 * use same output log defined in ph_frame_private.h
 */

#define TEST_LOG_ENV_VAR "_PHDRIVER_TEST_LOG_OUT"



#define CheckGuiInitialized(x) \
    if (!((phGuiProcessId_t) x)->initialized) return PH_GUI_ERR_GUININIT

#define YES_NO_DIALOG        \
    "S:`yesNoDialog`:`Confirm`{@v\n"                                  \
        "\tL::`%s`"                                                   \
        "\tS::{@h\n"                                                  \
            "\t\tp:`yes`:`Yes`:e\n"                                   \
            "\t\t*p:`no`:`No`:e\n"                                    \
        "\t}\n"                                                       \
    "}\n"

#define YES_NO_CANCEL_DIALOG \
    "S:`yesNoCancelDialog`:`Confirm`{@v\n"                            \
        "\tL::`%s`"                                                   \
        "\tS::{@h\n"                                                  \
            "\t\tp:`yes`:`Yes`:e\n"                                   \
            "\t\t*p:`no`:`No`:e\n"                                    \
            "\t\t*p:`cancel`:`Cancel`:e\n"                            \
        "\t}\n"                                                       \
    "}\n"

#define OK_CANCEL_DIALOG     \
    "S:`okCancelDialog`:`Confirm`{@v\n"                               \
        "\tL::`%s`"                                                   \
        "\tS::{@h\n"                                                  \
            "\t\tp:`ok`:`OK`:e\n"                                     \
            "\t\t*p:`cancel`:`Cancel`:e\n"                            \
        "\t}\n"                                                       \
    "}\n"

#define PLAIN_MESSAGE_DIALOG \
    "S:`plainMessageDialog`:``{@v\n"                                  \
        "\tL::`%s`"                                                   \
        "\tS::{@h\n"                                                  \
            "\t\t*p:`ok`:`OK`:e*\n"                                   \
        "\t}\n"                                                       \
    "}\n"

#define WARN_MESSAGE_DIALOG  \
    "S:`Dialog`:`Warning`{@v\n"                                       \
        "\tL::`%s`"                                                   \
        "\tS::{@h\n"                                                  \
            "\t\t*p:`ok`:`OK`:e*\n"                                   \
        "\t}\n"                                                       \
    "}\n"

#define ERROR_MESSAGE_DIALOG \
    "S:`errorMessageDialog`:`Error`{@v\n"                             \
        "\tL::`%s`"                                                   \
        "\tS::{@h\n"                                                  \
            "\t\t*p:`ok`:`OK`:e*\n"                                   \
        "\t}\n"                                                       \
    "}\n"


#define ERROR_AR_MESSAGE_DIALOG \
    "S:`errorAbortRetryDialog`:`Error`{@v\n"                          \
        "\tL::`%s`"                                                   \
        "\tS::{@h\n"                                                  \
            "\t\tp:`abort`:`Abort`:e\n"                               \
            "\t\t*p:`retry`:`Retry`:e\n"                              \
        "\t}\n"                                                       \
    "}\n"

/*--- typedefs --------------------------------------------------------------*/

enum SimulationModeEnum {
    PH_GUI_SIMULATION,
    PH_GUI_INTERACTIVE
};

/*--- global variables ------------------------------------------------------*/

static enum SimulationModeEnum phGuiSimulationMode = PH_GUI_INTERACTIVE;
static int phGuiSimulateAll = 1;
static int phGuiCurrentSimEvent = 0;
static int phGuiCountSimEvent;
static phGuiEvent_t *phGuiSimulationEvent;
static int  createTestLog = 0;
static FILE *testLogOut=NULL;

/*--- functions -------------------------------------------------------------*/

/*****************************************************************************
 *
 * 
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
static phGuiError_t initProcess(
    phGuiProcessId_t *process,
    phLogId_t logger
    ) 
{
    struct phGuiProcessStruct *tmp;

    /* allocate new guiProcess data type */
    tmp = (struct phGuiProcessStruct *) 
        malloc(sizeof(struct phGuiProcessStruct));
    if (tmp == 0) {
	phLogGuiMessage(logger, PHLOG_TYPE_ERROR, 
	"not enough memory during call to initProcess()");
	return PH_GUI_ERR_MEMORY;
    }
    tmp->myself = tmp;

    tmp->logger = logger;
    tmp->initialized = False;

    if (pipe(tmp->requestPipe) == -1)
    {
        free(tmp);
        return PH_GUI_ERR_OTHER;
    }
    if (pipe(tmp->eventPipe) == -1)
    {
        close(tmp->requestPipe[0]);
        close(tmp->requestPipe[1]);
        free(tmp);
        return PH_GUI_ERR_OTHER;
    }
    if (pipe(tmp->dataPipe) == -1)
    {
        close(tmp->eventPipe[0]);
        close(tmp->eventPipe[1]);
        close(tmp->requestPipe[0]);
        close(tmp->requestPipe[1]);
        free(tmp);
        return PH_GUI_ERR_OTHER;
    }
    
    if (sem_create(&tmp->semid, IPC_PRIVATE, 1) != SEMAPHORE_ERR_OK) {
	phLogGuiMessage(logger, PHLOG_TYPE_ERROR, "%s (GuiServer:initProcess)", strerror(errno));
        close(tmp->requestPipe[0]);
        close(tmp->requestPipe[1]);
        close(tmp->eventPipe[0]);
        close(tmp->eventPipe[1]);
        close(tmp->requestPipe[0]);
        close(tmp->requestPipe[1]);
        free(tmp);
	return PH_GUI_ERR_OTHER;
    }

    /* CR36857:
     * change sem_wait() to sem_lock() to avoid segmentation violation error
     */
    sem_lock(tmp->semid, 0);

    /* 
     *  find out whether we want to append to output file 
     *  used for automatic driver test (same output log defined in
     *  ph_frame_private.h )
     */
    if (!createTestLog && getenv(TEST_LOG_ENV_VAR))
    {
        testLogOut = fopen(getenv(TEST_LOG_ENV_VAR), "a");
        if (testLogOut)
        {
            createTestLog = 1;
	    phLogGuiMessage(logger, LOG_DEBUG, 
	        "test log file successfully opened for appending \"%s\"",
                getenv(TEST_LOG_ENV_VAR));
        }
        else
        {
	    phLogGuiMessage(logger, PHLOG_TYPE_ERROR, 
	        "unable to open test log file for appending \"%s\"",getenv(TEST_LOG_ENV_VAR));
        }
    }

    *process = tmp;

    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 * 
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
static void initPipes(
    phGuiProcessId_t process,
    pid_t pid
    ) 
{
    const char *flags[2] = { "rb", "wb" };
    int pipenr = pid? 1 : 0;

    close(process->requestPipe[1 - (0 ^ pipenr)]);
    close(process->eventPipe[1 - (1 ^ pipenr)]);
    close(process->dataPipe[1 - (1 ^ pipenr)]);
    
    process->requestFp = fdopen(process->requestPipe[0 ^ pipenr], 
	                        flags[0 ^ pipenr]);
    process->eventFp   = fdopen(process->eventPipe[1 ^ pipenr],
	                        flags[1 ^ pipenr]);
    process->dataFp    = fdopen(process->dataPipe[1 ^ pipenr],
	                        flags[1 ^ pipenr]);

    setbuf(process->requestFp, NULL);
    setbuf(process->eventFp, NULL);
    setbuf(process->dataFp, NULL);
}

/*****************************************************************************
 *
 * 
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
static phGuiError_t sendRequest(
    phGuiProcessId_t process, 
    phGuiRequest_t request, 
    const char *secval, 
    const char *thirdval
    ) 
{
    int size;
    int done, ioerr;
    fd_set fds;
    struct timeval to;
    char lbuffer[32];

    to.tv_sec = 3;
    to.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(process->requestPipe[1], &fds);


    sprintf(lbuffer, "%ld", (long) request);
    size = strlen(lbuffer) + 3;
/* Begin of Huatek Modification, Donnie Tu, 04/19/2001*/
/* Issue Number: 5 */		
    if (secval != NULL) 
         size += strlen(secval);
    else 
	 secval="";
    if (thirdval != NULL) 
	size += strlen(thirdval);
    else 
        thirdval="";
/* End of Huatek Modification */

    clearerr(process->requestFp);

    do {
	done = (putw(size, process->requestFp) == 0);

	ioerr = ferror(process->requestFp);

	if (ioerr && done) {
	    phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
		"couldn't write messagelength in sendRequest()\n" \
		"error: %s\n" \
		"Trying to read again!", strerror(errno));
	}
    } while (!done);
    clearerr(process->requestFp);

    fprintf(process->requestFp, "%s %s %s", lbuffer, secval, thirdval);

    fflush(process->requestFp);

    return PH_GUI_ERR_OK;	
}

/*****************************************************************************
 *
 * Create a userdefined dialog
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiCreateUserDefDialog(
    phGuiProcessId_t *process,
    phLogId_t logger,
    phGuiSequenceMode_t mode,
    const char *description
)
{
    pid_t pid;
/*Begin of Huatek Modifications, Donnie tu,04/06/2001*/
/*Issue Number: 188*/
    char args[MAXNAMLEN];	
/*End of Huatek Modifications*/
    phGuiError_t result = PH_GUI_ERR_OK;
    int dupRP, dupEP, dupDP;
    phGuiEvent_t event;
    struct sigaction act = {{0}};
    sigset_t signalmask;
    char guiPath[1024];
    FILE *testFP;

    if ((result = initProcess(process, logger)) != PH_GUI_ERR_OK) {
	phLogGuiMessage(logger, PHLOG_TYPE_ERROR, 
	    "initializing gui-process failed");
	return result;
    }

    phLogGuiMessage(logger, PHLOG_TYPE_TRACE, 
	"phGuiCreateUserDefDialog(P%p, P%p, 0x%08lx, P%p)",
	*process, logger, mode, description);

    if (strlen(description) < PHLOG_MAX_MESSAGE_SIZE - 20) {
	phLogGuiMessage(logger, LOG_DEBUG, 
	    "GuiDescription:\n%s", description);
    }

    guiPath[0] = '\0';
    testFP = fopen(GUIPATH1, "r");
    if (testFP)
    {
	fclose(testFP);
	strcpy(guiPath, GUIPATH1);
    }
    else
    {
	testFP = fopen(GUIPATH2, "r");
	if (testFP)
	{
	    fclose(testFP);
	    strcpy(guiPath, GUIPATH2);
	}
    }

    if (strlen(guiPath) > 0)
    {
	phLogGuiMessage(logger, LOG_VERBOSE, "using %s", guiPath);
    }
    else
    {
	phLogGuiMessage(logger, PHLOG_TYPE_FATAL, 
	    "could not locate guiProcess, searched for:\n"
	    GUIPATH1 "\n" GUIPATH2);
	return PH_GUI_ERR_OTHER;
    }

    if ((dupRP = dup((*process)->requestPipe[0])) == -1)
    {
        return PH_GUI_ERR_OTHER;
    }
    if ((dupEP = dup((*process)->eventPipe[1])) == -1)
    {
        close(dupRP);
        return PH_GUI_ERR_OTHER;
    }
    if ((dupDP = dup((*process)->dataPipe[1])) == -1)
    {
        close(dupRP);
        close(dupEP);
        return PH_GUI_ERR_OTHER;
    }

    sprintf(args, "%s %d %d %d %d", 
	PH_GUI_PROCESS_NAME,
	dupRP, dupEP, dupDP, (*process)->semid);

    (*process)->mode = mode;

    sigemptyset(&signalmask);
    act.sa_mask = signalmask;
    act.sa_handler = SIG_IGN;
    act.sa_flags = SA_NOCLDSTOP; /*SA_NOCLDWAIT;  */

    sigaction(SIGCHLD, &act, NULL);

    pid = (*process)->guipid = fork();
    
    switch(pid) {
      case 0: /* kiddy code */			
	if (execl(guiPath, args, NULL) == -1)
	    perror("exec failed");
	exit(1);
        break;
      case -1:
	phLogGuiMessage(logger, PHLOG_TYPE_ERROR, 
	    "Fork failed at phGuiCreateUserDefDialogon (%s)", strerror(errno));

	close(dupRP);
	close(dupEP);
	close(dupDP);
	return PH_GUI_ERR_FORK;
      default: 
	initPipes(*process, pid);
      
        /* CR36857:
         * change sem_wait() to sem_lock() to avoid segmentation violation error
         */
	sem_lock((*process)->semid, 0);

	close(dupRP);
	close(dupEP);
	close(dupDP);
	
	phLogGuiMessage(logger, PHLOG_TYPE_TRACE, 
	    "GuiProcess (%d) created", pid);
	if ((result = sendRequest(*process, configure, description, NULL)) != PH_GUI_ERR_OK) {
	    return result;
	}
	break;
    }

    if (phGuiSimulationMode == PH_GUI_SIMULATION && !phGuiSimulateAll) {
	phLogGuiMessage((*process)->logger, PHLOG_TYPE_TRACE, 
	    "succesfully parsed GuiDescription");
	(*process)->initialized = True;

	return PH_GUI_ERR_OK;		
    }
    if (phGuiGetEvent(*process, &event, 5) == PH_GUI_ERR_OK) {
	if (event.type == PH_GUI_NO_ERROR) {
	    phLogGuiMessage((*process)->logger, PHLOG_TYPE_TRACE, 
		"succesfully parsed GuiDescription");
	    (*process)->initialized = True;
	    return PH_GUI_ERR_OK;
	}
	phLogGuiMessage((*process)->logger, PHLOG_TYPE_ERROR, 
	    "failed on parsing GuiDescription");

	phGuiDestroyDialog(*process);

	return PH_GUI_ERR_PARSEGUI;
    }
    else
    {
	phLogGuiMessage((*process)->logger, PHLOG_TYPE_ERROR, 
	    "phGuiGetEvent() has failed");
	
	return PH_GUI_ERR_OTHER;
    }
}

/*****************************************************************************
 *
 * Create a predefined option dialog 
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description:
 *
 * This function creates a predefined dialog with synchron communication
 *  
 * Returns: error code
 *
 ***************************************************************************/
phGuiError_t phGuiShowOptionDialog(
    phLogId_t logger,             /* the data logger to be used              */
    phGuiOptionDialogType_t type, /* type of the dialog e.g. OK-Cancel-Dialog
				     (this is a dialog with only 2 Buttons,
                                     OK and Cancel and a simple messagetext) */
    const char *text,             /* this is the shown message text */
    char *response
    )
{
    phGuiError_t returnVal;
    phGuiProcessId_t gui;
    char *descr;

    phLogGuiMessage(logger, PHLOG_TYPE_TRACE, 
	"phGuiShowOptionDialog (P%p, 0x%08lx, %s, P%p)", logger, type, text, response);

    descr = (char *) malloc(strlen(text) + 512);
    if (descr == 0) {
	return PH_GUI_ERR_MEMORY;
    }

    switch (type) {
      case PH_GUI_YES_NO:
	sprintf(descr, YES_NO_DIALOG, text);
	break;
      case PH_GUI_YES_NO_CANCEL:
	sprintf(descr, YES_NO_CANCEL_DIALOG, text);
	break;
      case PH_GUI_OK_CANCEL:
	sprintf(descr, OK_CANCEL_DIALOG, text);
	break;
      case PH_GUI_PLAIN_MESSAGE:
	sprintf(descr, PLAIN_MESSAGE_DIALOG, text);
	break;
      case PH_GUI_WARN_MESSAGE:
	sprintf(descr, WARN_MESSAGE_DIALOG, text);
	break;
      case PH_GUI_ERROR_MESSAGE:
	sprintf(descr, ERROR_MESSAGE_DIALOG, text);
	break;
      case PH_GUI_ERROR_ABORT_RETRY:
	sprintf(descr, ERROR_AR_MESSAGE_DIALOG, text);
	break;
    }

    if ((returnVal = phGuiCreateUserDefDialog(&gui, logger, PH_GUI_COMM_SYNCHRON, descr))
	!= PH_GUI_ERR_OK) {
        free(descr);
        return returnVal;
    }

    if (response != NULL) returnVal = phGuiShowDialogGetResponse(gui, 1, response);
    else returnVal = phGuiShowDialog(gui, 1);

    phGuiDestroyDialog(gui);
    free(descr);

    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 * Show the dialog
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiShowDialog(
    phGuiProcessId_t process,      /* the resulting Gui-ID */
    int closeOnReturn
    )
{
    phGuiEvent_t event;
    memset(&event, 0, sizeof(event));
    phGuiError_t result = PH_GUI_ERR_OK;

    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiShowDialog(P%p)", process);

    if ((result = sendRequest(process, showDialog, NULL, NULL)) != PH_GUI_ERR_OK) {
	return result;
    }

    if (process->mode == PH_GUI_COMM_SYNCHRON) {
	while (event.type != PH_GUI_EXITBUTTON_EVENT && event.type != PH_GUI_ERROR_EVENT) {
	    phGuiGetEvent(process, &event, 900);
	}
	if (event.type == PH_GUI_ERROR_EVENT) {
	    phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
		"error event received from gui (%s)", event.name);
	    return PH_GUI_ERR_OTHER;
	}
	if (closeOnReturn) {
	    return sendRequest(process, hideDialog, NULL, NULL);
	}
    }
    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 * Show the dialog and get a response
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiShowDialogGetResponse(
    phGuiProcessId_t process      /* the resulting Gui-ID */,
    int closeOnReturn,
    char *name
    )
{
    phGuiEvent_t event;
    phGuiError_t result = PH_GUI_ERR_OK;

    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiShowDialogGetResponse (%d, P%p)", process, name);

    event.type =  PH_GUI_NO_EVENT;
    name[0] = 0;
    if ((result = sendRequest(process, showDialog, NULL, NULL)) != PH_GUI_ERR_OK) {
	return result;
    }

    if (process->mode == PH_GUI_COMM_SYNCHRON) {
	while (event.type != PH_GUI_EXITBUTTON_EVENT && event.type != PH_GUI_ERROR_EVENT) {
	    phGuiGetEvent(process, &event, 900);
	}
	if (event.type == PH_GUI_ERROR_EVENT) {
	    phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
		"error event received from gui (%s)", event.name);
	    return PH_GUI_ERR_OTHER;
	}
	strcpy(name, event.name);
	if (closeOnReturn) {
	    return sendRequest(process, hideDialog, NULL, NULL);
	}
    }
    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 * Close the dialog
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiCloseDialog(
    phGuiProcessId_t process      /* the resulting Gui-ID */
    )
{
    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiCloseDialog (P%p)", process);

    return sendRequest(process, hideDialog, NULL, NULL);
}

/*****************************************************************************
 *
 * Kill the GUI-process and free the data struct
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiDestroyDialog(
    phGuiProcessId_t process      /* the resulting Gui-ID */
    )
{
    phGuiError_t result = PH_GUI_ERR_OK;

    CheckHandle(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiDestroyDialog (P%p)", process);

    if ((result = sendRequest(process, disposeDialog, NULL, NULL)) != PH_GUI_ERR_OK) { 
	return result;  
    }    

/*    while (wait((int *) 0) != process->guipid);*/
    kill(process->guipid, SIGKILL);
    sem_remove(process->semid);

    fclose(process->requestFp);
    fclose(process->eventFp);
    fclose(process->dataFp);

    close(process->requestPipe[1]);
    close(process->eventPipe[0]);
    close(process->dataPipe[0]);

    process->myself = NULL;

    free(process); 

    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 * Wait for the next event for a maximum of <timeout> seconds and return it.
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiGetEvent(
    phGuiProcessId_t process,     /* the resulting Gui-ID */
    phGuiEvent_t *event,
    int timeout
    )
{
    int length;
    phGuiError_t result = PH_GUI_ERR_OK;
    int retValue;
    int type;
    int usedTimeOut;
    int currentReadLength, readLength;
    int ioerr;
    char *input;

    fd_set fds;
    struct timeval to;
    struct timeval first, second, lapsed;
    struct timezone tzp;    

    CheckHandle(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiGetEvent (P%p, P%p, %d)", process, event, timeout);

    if (phGuiSimulationMode == PH_GUI_SIMULATION) {
	strcpy(event->name, phGuiSimulationEvent[phGuiCurrentSimEvent].name);
	event->type = phGuiSimulationEvent[phGuiCurrentSimEvent].type;
	phGuiCurrentSimEvent ++;
	if (phGuiCurrentSimEvent == phGuiCountSimEvent) {
	    phGuiSimulationMode = PH_GUI_INTERACTIVE;
	}
	phLogGuiMessage(process->logger, LOG_NORMAL, 
	    "simulate phGuiGetEvent (%s, %d)", event->name, event->type);
	return PH_GUI_ERR_OK;
    }

    if ((result = sendRequest(process, requestEvent, NULL, NULL)) != PH_GUI_ERR_OK) {
	return result;
    }
    

    usedTimeOut = to.tv_sec = timeout >= 0? timeout : 0;
    to.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(process->eventPipe[0], &fds);

    gettimeofday(&first, &tzp);
	
    while (1) {
	retValue = select(process->eventPipe[0] + 1, &fds, NULL, NULL, &to);
	if (retValue > 0) {
	    clearerr(process->eventFp);
	    do {
		length = getw(process->eventFp);
		ioerr = ferror(process->eventFp);

		if (ioerr && length == EOF) {
		    phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
			"couldn't read eventmessage length in phGuiGetEvent()\n" \
			"error: %s\n" \
                        "Trying to read again!", strerror(errno));
		    if (errno != EINTR) return PH_GUI_ERR_OTHER;
		}
	    } while (ioerr && errno == EINTR);
	    clearerr(process->eventFp);

	    if (length != EOF) {
		input = (char *) malloc(length * sizeof(char));
		readLength = 0;
		while (readLength < length - 1) {
		    currentReadLength = 
			fread(input + readLength, 1,(length - readLength) - 1, process->eventFp);

		    if (currentReadLength > 0) {
			readLength += currentReadLength;
		    }
		    if (readLength < length - 1) { 
			phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
			    "couldn't read complete eventmessage in phGuiGetEvent()\n" \
			    "error: %s\n" \
			    "Trying to read again!", strerror(errno));
		    }
		}
		input[length-1] = 0;
	    
		sscanf(input, "%d %s", &type, event->name);
		event->type = (phGuiEventType_t) type;

		free(input);

                if (createTestLog && (event->type==PH_GUI_PUSHBUTTON_EVENT ||
                                      event->type==PH_GUI_EXITBUTTON_EVENT) )
                {

                    phLogGuiMessage(process->logger, LOG_DEBUG, 
	                "Write to test log: GUIEVENT|[%s:\"%s\"]",
                        (event->type==PH_GUI_PUSHBUTTON_EVENT) ? "PB" : "EB",
                        event->name);
                    fprintf(testLogOut, "GUIEVENT|[%s:\"%s\"]\n",
                        (event->type==PH_GUI_PUSHBUTTON_EVENT) ? "PB" : "EB",
                        event->name);
                    fflush(testLogOut);
                }
		return PH_GUI_ERR_OK;
	    }
	    else return PH_GUI_ERR_OTHER;
	}
	else if (retValue == 0) {
	    phLogGuiMessage(process->logger, LOG_VERBOSE, 
		"a timeout occured in phGuiGetEvent()");
	    return PH_GUI_ERR_TIMEOUT;
	}
	else if (retValue == -1 && errno == EINTR) {
	    gettimeofday(&second, &tzp);

	    if (first.tv_usec > second.tv_usec) {
		second.tv_usec += 1000000;
		second.tv_sec--;
	    }
	    lapsed.tv_sec  = second.tv_sec  - first.tv_sec;
	    lapsed.tv_usec = second.tv_usec - first.tv_usec;

	    if (lapsed.tv_usec > 0) {
		to.tv_usec = 1000000 - lapsed.tv_usec;
		lapsed.tv_sec++;
	    }
	    else {
		to.tv_usec = 0;
	    }
	    to.tv_sec  = usedTimeOut - lapsed.tv_sec;

	    if (to.tv_sec < 0) {
		return PH_GUI_ERR_TIMEOUT;
	    }
	}
	else {
	    phLogGuiMessage(process->logger, LOG_VERBOSE, 
		"select aborted in phGuiGetEvent(): %s", strerror(errno));
	    return PH_GUI_ERR_OTHER;
	}
    }
}

/*****************************************************************************
 *
 * Return the entry of a component
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiGetData(
    phGuiProcessId_t process, /* the resulting Gui-ID */
    const char *compName,     /* name of the component */
    char *data,
    int timeout
    )
{
    int length;
    phGuiError_t result = PH_GUI_ERR_OK;
    int retValue;
    int usedTimeOut;
    int currentReadLength, readLength;
    int ioerr;
    char *widget;
    char *input;

    fd_set fds;
    struct timeval to;
    struct timeval first, second, lapsed;
    struct timezone tzp;    

    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiGetData (P%p, %s, P%p, %d)", process, compName, data, timeout);

    if ((result = sendRequest(process, requestData, compName, NULL)) != PH_GUI_ERR_OK) {
	return result;
    }

    usedTimeOut = to.tv_sec = timeout > 0? timeout : 60;
    to.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(process->dataPipe[0], &fds);

    gettimeofday(&first, &tzp);

    while (1) {
	retValue = select(process->dataPipe[0] + 1, &fds, NULL, NULL, &to);
	if (retValue > 0) {
	    clearerr(process->dataFp);
	    do {
		length = getw(process->dataFp);
		ioerr = ferror(process->dataFp);

		if (ioerr && length != EOF) {
		    phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
			"couldn't read datamessage length in phGuiGetData()\n" \
			"error: %s\n" \
                        "Trying to read again!", strerror(errno));
		    if (errno != EINTR) return PH_GUI_ERR_OTHER;
		}
	    } while (ioerr && errno == EINTR);
	    clearerr(process->dataFp);

	    if (length != EOF) {
		input = (char *) malloc(length * sizeof(char));
		widget = (char *) malloc(length * sizeof(char));

		readLength = 0;
		while (readLength < length - 1) {
		    currentReadLength = 
			fread(input + readLength, 1, length - readLength - 1, process->dataFp);

		    if (currentReadLength > 0) {
			readLength += currentReadLength;
		    }

		    if (readLength < length - 1) { 
			phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
			    "couldn't read complete datamessage in phGuiGetData()\n" \
			    "error: %s\n" \
			    "Trying to read again!", strerror(errno));
		    }
		}
		input[length-1] = 0;
	    
		sscanf(input, "%s", widget);
		strcpy(data, input + strlen(widget) + 1);

		free(widget);
		free(input);
		return PH_GUI_ERR_OK;
	    }
	    else return PH_GUI_ERR_OTHER;
	}
	else if (retValue == 0) {
	    phLogGuiMessage(process->logger, LOG_VERBOSE, 
		"a timeout occured in phGuiGetData()");
	    return PH_GUI_ERR_TIMEOUT;
	}
	else if (retValue == -1 && errno == EINTR) {
	    gettimeofday(&second, &tzp);

	    if (first.tv_usec > second.tv_usec) {
		second.tv_usec += 1000000;
		second.tv_sec--;
	    }
	    lapsed.tv_sec  = second.tv_sec  - first.tv_sec;
	    lapsed.tv_usec = second.tv_usec - first.tv_usec;

	    if (lapsed.tv_usec > 0) {
		to.tv_usec = 1000000 - lapsed.tv_usec;
		lapsed.tv_sec++;
	    }
	    else {
		to.tv_usec = 0;
	    }
	    to.tv_sec  = usedTimeOut - lapsed.tv_sec;

	    if (to.tv_sec < 0) {
		return PH_GUI_ERR_TIMEOUT;
	    }
	}
	else {
	    phLogGuiMessage(process->logger, LOG_VERBOSE, 
		"select aborted in phGuiGetData(): %s", strerror(errno));
	    return PH_GUI_ERR_OTHER;
	}
    }
}

/*****************************************************************************
 *
 * Modify the Guiform dynamically 
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiConfigure(
    phGuiProcessId_t process, /* the resulting Gui-ID */
    char *description
    )
{
    CheckHandle(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiConfigure (P%p, P%p)", process, description);

    phLogGuiMessage(process->logger, LOG_DEBUG, 
	"GuiDescription:\n%s", description);

    return sendRequest(process, configure, description, NULL);
}

/*****************************************************************************
 *
 * Change the entry of a component
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiChangeValue(
    phGuiProcessId_t process, /* the resulting Gui-ID */
    const char *componentName,
    const char *newValue
    )
{
    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiChangeValue (P%p, %s, %s)", process, componentName, newValue);

    return sendRequest(process, changeData, componentName, newValue);
}

/*****************************************************************************
 *
 * Enable (deactivate) component
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiEnableComponent(
    phGuiProcessId_t process,
    const char *componentName
    )
{
    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiEnableComponent (P%p, %s)", process, componentName);

    return sendRequest(process, enableComponent, componentName, NULL);
}

/*****************************************************************************
 *
 * Disable (deactivate) component
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiDisableComponent(
    phGuiProcessId_t process,
    const char *componentName
    )
{
    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiDisableComponent (P%p, %s)", process, componentName);

    return sendRequest(process, disableComponent, componentName, NULL);
}

/*****************************************************************************
 *
 * Test the communication between client and server
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiComTest(
    phGuiProcessId_t process
    )
{
    const char *testString = "abcdefghijklmnopqrstuvwxyz";
    phGuiEvent_t event;
    phGuiError_t result = PH_GUI_ERR_OK;
    event.type = PH_GUI_NO_EVENT;

    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiComTest (P%p)", process);

    if ((result = sendRequest(process, comTest, testString, NULL)) != PH_GUI_ERR_OK) {
	phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
	    "guiserver communication test failed");
	return result;
    }

    phGuiGetEvent(process, &event, 3);
    if (event.type == PH_GUI_COMTEST_RECEIVED) {
	if (strcmp(testString, event.name) == 0) {
	    return PH_GUI_ERR_OK;
	}
    }
    phLogGuiMessage(process->logger, PHLOG_TYPE_WARNING, 
	"guiserver communication test failed");
    return PH_GUI_ERR_OTHER;
}

/*****************************************************************************
 *
 * Simulates button press
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiSimulateAction(
    phGuiProcessId_t process,
    char *name
    )
{
    phGuiError_t result = PH_GUI_ERR_OK;

    CheckHandle(process);
    CheckGuiInitialized(process);
 
    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiSimulateAction (P%p, %s)", process, name);

    if ((result = sendRequest(process, simAction, name, NULL)) != PH_GUI_ERR_OK)
	return result;

    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 * Simulates button presses
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiSimulateActions(
    phGuiProcessId_t process,
    char **names
    )
{
    char **widget = names;
    phGuiError_t result = PH_GUI_ERR_OK;
    int i = 0;

    CheckHandle(process);
    CheckGuiInitialized(process);

    phLogGuiMessage(process->logger, PHLOG_TYPE_TRACE, 
	"phGuiSimulateActions (P%p, P%p)", process, names);

    while (widget[i] != NULL) {
	result = sendRequest(process, simAction, widget[i], NULL);
        if (result != PH_GUI_ERR_OK)
            return result;
        i ++;
    }
    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 * start simulation mode and define events
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiSimulationStart(
    phLogId_t logger,    
    int eventCount,      /* the number of events defined in the event array */
    phGuiEvent_t *event, /* the event array, values will be used one
		            after the other until <eventCount> is hit. */
    int simulateAll
    )
{
    phLogGuiMessage(logger, PHLOG_TYPE_TRACE, 
	"phGuiSimulationStart (%d, P%p)", eventCount, event);

    phGuiSimulationMode = PH_GUI_SIMULATION;
    phGuiCountSimEvent = eventCount;
    phGuiSimulationEvent = event;
    phGuiCurrentSimEvent = 0;
    phGuiSimulateAll = simulateAll;

    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 * end simulation mode
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiSimulationEnd(
    phLogId_t logger
)
{
    phLogGuiMessage(logger, PHLOG_TYPE_TRACE, "phGuiSimulationEnd ()");

    phGuiSimulationMode = PH_GUI_INTERACTIVE;

    return PH_GUI_ERR_OK;
}

/*****************************************************************************
 *
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
static char const* msgDisplayMsgButton[9];

static void setUpButtons(
    char const *b2s         /* user button 2 string */,
    char const *b3s         /* user button 3 string */,
    char const *b4s         /* user button 4 string */,
    char const *b5s         /* user button 5 string */,
    char const *b6s         /* user button 6 string */,
    char const *b7s         /* user button 7 string */
)
{
    /* set-up msgDisplayMsgButton array */
    msgDisplayMsgButton[1]="Quit";
    msgDisplayMsgButton[2]=b2s;
    msgDisplayMsgButton[3]=b3s;
    msgDisplayMsgButton[4]=b4s;
    msgDisplayMsgButton[5]=b5s;
    msgDisplayMsgButton[6]=b6s;
    msgDisplayMsgButton[7]=b7s;
    msgDisplayMsgButton[8]="Continue";
}


static char const* displayButton(int button)
{
    char const* buttonStr;

    if ( button >= 1 && button <= 8 )
    {
        buttonStr=msgDisplayMsgButton[button];
    }
    else
    {
        buttonStr="Undefined button";
    }

    return buttonStr;
}

/*****************************************************************************
 *
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * Description: 
 *
 ***************************************************************************/
phGuiError_t phGuiMsgDisplayMsgGetResponse(
    phLogId_t logger,
    int  needQuitAndCont        /* we really need the default QUIT
				   and CONTINUE buttons to be
				   displayed */,
    const char *title           /* title of the dialog */,
    const char *msg             /* message to be displayed */,
    const char *b2s             /* user button 2 string */,
    const char *b3s             /* user button 3 string */,
    const char *b4s             /* user button 4 string */,
    const char *b5s             /* user button 5 string */,
    const char *b6s             /* user button 6 string */,
    const char *b7s             /* user button 7 string */,
    long *response              /* return button selected */
)
{
    phGuiError_t returnVal = PH_GUI_ERR_OK;
    phGuiProcessId_t gui;
    char descr[4096]; 
    char button[2];
    const char *quitButton = "Quit";
    const char *continueButton = "Continue";
    const char *usedButton[8];
    int usedButtonNr[8];
    int i = 0;
    int j;

    phLogGuiMessage(logger, PHLOG_TYPE_TRACE, "phGuiMsgDisplayMsgGetResponse ()");

    if (needQuitAndCont) { 
	usedButton[i] = quitButton; usedButtonNr[i++] = 1; 
    }
    if (b2s && strlen(b2s) > 0) {
	usedButton[i] = b2s; usedButtonNr[i++] = 2; 
    }
    if (b3s && strlen(b3s) > 0) {
	usedButton[i] = b3s; usedButtonNr[i++] = 3; 
    }
    if (b4s && strlen(b4s) > 0) {
	usedButton[i] = b4s; usedButtonNr[i++] = 4; 
    }
    if (b5s && strlen(b5s) > 0) {
	usedButton[i] = b5s; usedButtonNr[i++] = 5; 
    }
    if (b6s && strlen(b6s) > 0) {
	usedButton[i] = b6s; usedButtonNr[i++] = 6; 
    }
    if (b7s && strlen(b7s) > 0) {
	usedButton[i] = b7s; usedButtonNr[i++] = 7; 
    }
    if (needQuitAndCont) { 
	usedButton[i] = continueButton; usedButtonNr[i++] = 8; 
    }

    sprintf(descr, "S::`%s`{@v\n\tL:`label`:`%s`\n\tS::{@h\n", title, msg);
    if (i == 1) sprintf(descr + strlen(descr), "\t\t*\n");
    for (j = 0; j < i-1; j++) {
	sprintf(descr + strlen(descr), "\t\tp:`%d`:`%s`:e\n", usedButtonNr[j], usedButton[j]);
	sprintf(descr + strlen(descr), "\t\t*\n");
    }
    sprintf(descr + strlen(descr), "\t\tp:`%d`:`%s`:e\n", usedButtonNr[j], usedButton[j]);
    if (i == 1) sprintf(descr + strlen(descr), "\t\t*\n");
    sprintf(descr + strlen(descr), "\t}\n}\n");


    if ((returnVal = phGuiCreateUserDefDialog(&gui, logger, PH_GUI_COMM_SYNCHRON, descr))
	!= PH_GUI_ERR_OK) {
        return returnVal;
    }

    /* set-up buttons array */
    setUpButtons(b2s, b3s, b4s, b5s, b6s, b7s);

    if ((returnVal = phGuiShowDialogGetResponse(gui, 1, button)) == PH_GUI_ERR_OK)
    {
	*response = atol(button);
	phLogGuiMessage(logger,
	    PHLOG_TYPE_MESSAGE_0,
	    "User Dialog:\n"
	    "%s\n"
	    "User Dialog: %s '%s' '%s' '%s' '%s' '%s' '%s' %s\n"
	    "User Dialog: User pressed '%s' ",
	    msg,
	    needQuitAndCont? "'Quit'" : "",
	    b2s,
	    b3s,
	    b4s,
	    b5s,
	    b6s,
	    b7s,
	    needQuitAndCont? "'Continue'" : "",
	    displayButton(*response));
    }

    phGuiDestroyDialog(gui);

    return returnVal;
}


phGuiError_t phGuiMsgDisplayMsgGetStringResponse(
    phLogId_t logger,
    int  needQuitAndCont        /* we really need the default QUIT
				   and CONTINUE buttons to be
				   displayed */,
    const char *title           /* title of the dialog */,
    const char *msg             /* message to be displayed */,
    const char *b2s             /* user button 2 string */,
    const char *b3s             /* user button 3 string */,
    const char *b4s             /* user button 4 string */,
    const char *b5s             /* user button 5 string */,
    const char *b6s             /* user button 6 string */,
    const char *b7s             /* user button 7 string */,
    long *response              /* return button selected */,
    char *string                /* return string */
)
{
    phGuiError_t returnVal = PH_GUI_ERR_OK;
    phGuiProcessId_t gui;
    char descr[4096]; 
    char button[2];
    const char *quitButton = "Quit";
    const char *continueButton = "Continue";
    const char *usedButton[8];
    int usedButtonNr[8];
    int i = 0;
    int j;

    phLogGuiMessage(logger, PHLOG_TYPE_TRACE, "phGuiMsgDisplayMsgGetStringResponse ()");

    if (needQuitAndCont) { 
	usedButton[i] = quitButton; usedButtonNr[i++] = 1; 
    }
    if (b2s && strlen(b2s) > 0) {
	usedButton[i] = b2s; usedButtonNr[i++] = 2; 
    }
    if (b3s && strlen(b3s) > 0) {
	usedButton[i] = b3s; usedButtonNr[i++] = 3; 
    }
    if (b4s && strlen(b4s) > 0) {
	usedButton[i] = b4s; usedButtonNr[i++] = 4; 
    }
    if (b5s && strlen(b5s) > 0) {
	usedButton[i] = b5s; usedButtonNr[i++] = 5; 
    }
    if (b6s && strlen(b6s) > 0) {
	usedButton[i] = b6s; usedButtonNr[i++] = 6; 
    }
    if (b7s && strlen(b7s) > 0) {
	usedButton[i] = b7s; usedButtonNr[i++] = 7; 
    }
    if (needQuitAndCont) { 
	usedButton[i] = continueButton; usedButtonNr[i++] = 8; 
    }

    sprintf(descr, "S::`%s`{@v\n\tL:`label`:`%s`\n\tS::{@h\n", title, msg);
    if (i == 1) sprintf(descr + strlen(descr), "\t\t*\n");
    for (j = 0; j < i-1; j++) {
	sprintf(descr + strlen(descr), "\t\tp:`%d`:`%s`:e\n", usedButtonNr[j], usedButton[j]);
	sprintf(descr + strlen(descr), "\t\t*\n");
    }
    sprintf(descr + strlen(descr), "\t\tp:`%d`:`%s`:e\n", usedButtonNr[j], usedButton[j]);
    if (i == 1) sprintf(descr + strlen(descr), "\t\t*\n");    
    sprintf(descr + strlen(descr), "\t}\n");
    if (string != NULL) sprintf(descr + strlen(descr), "\tf:`text`::\n");
    sprintf(descr + strlen(descr), "}\n");

    if ((returnVal = phGuiCreateUserDefDialog(&gui, logger, PH_GUI_COMM_SYNCHRON, descr))
	!= PH_GUI_ERR_OK) {
        return returnVal;
    }

    /* set-up buttons array */
    setUpButtons(b2s, b3s, b4s, b5s, b6s, b7s);

    if ((returnVal = phGuiShowDialogGetResponse(gui, 1, button)) == PH_GUI_ERR_OK)
    {
	*response = atol(button);
	phGuiGetData(gui, "text", string, 3);

	phLogGuiMessage(logger,
	    PHLOG_TYPE_MESSAGE_0,
	    "User Dialog:\n"
	    "%s\n"
	    "User Dialog: %s '%s' '%s' '%s' '%s' '%s' '%s' %s\n"
	    "User Dialog: User entered \"%s\" and pressed '%s' ",
	    msg,
	    needQuitAndCont? "'Quit'" : "",
	    b2s,
	    b3s,
	    b4s,
	    b5s,
	    b6s,
	    b7s,
	    needQuitAndCont? "'Continue'" : "",
	    string,
	    displayButton(*response));
    }

    phGuiDestroyDialog(gui);

    return returnVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/

