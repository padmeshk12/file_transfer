/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : GuiProcess.c
 * CREATED  : 26 Oct 1999
 *
 * CONTENTS : GUI functions
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

/*--- module includes -------------------------------------------------------*/

#include <X11/Intrinsic.h> 

#include <Xm/Xm.h> 
#include <Xm/MwmUtil.h> 

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <signal.h>

#include "GuiProcess.h"
#include "GuiComponent.h"
#include "Semaphore.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- global variables ------------------------------------------------------*/

static phGuiProcessId_t thisProcess;

/*--- functions -------------------------------------------------------------*/

extern phGuiComponent_t *createGroup(
    phGuiComponentType_t type,
    char *name,
    char *label,
    phGuiComponent_t *firstChild
    );

extern phGuiComponent_t *phGuiParseConf(
    phGuiProcessId_t process,
    char *text
    );

/*****************************************************************************
 *
 * Dispatches the list of actions which schould be simulated
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: 
 *
 * Description: 
 *
 ***************************************************************************/
void dispatchSimAction(phGuiProcessId_t process) {
    phGuiComponent_t *c;
    struct simListStruct *list;

    if (process->simList->next != NULL) {
	list = process->simList->next;
    
	c = getComponent((phGuiComponent_t *) process->toplevel, list->name);
	if (c != NULL) {
	    simulateAction(process, c);
	    process->simList->next = list->next;
	    free(list);
	}
    }
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void sendEvent(phGuiProcessId_t process, char *widgetName, int eventType) {
    int size;
    char eventTypeString[32];

    sprintf(eventTypeString, "%ld", (long) eventType);

    size = strlen(widgetName) + strlen(eventTypeString) + 2;

    putw(size, process->eventFp);

    fprintf(process->eventFp, "%s %s", eventTypeString, widgetName);
    fflush(process->eventFp);
}	


/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void sendData(phGuiProcessId_t process, char *widgetName, char *data) {
    int size;

    size = strlen(widgetName) + strlen(data) + 2;

    putw(size, process->dataFp);

    fprintf(process->dataFp, "%s %s", widgetName, data);
    fflush(process->dataFp);
}	


/*****************************************************************************
 *
 * handler for TERM INT HUP QUIT signals
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: 
 *
 * Description: 
 *
 ***************************************************************************/
void quitHandler(int sig) {
    sem_remove(thisProcess->semid);
    exit(0);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void GetInputProc(XtPointer client_data, XtIntervalId *id) {
    phGuiProcessId_t process = (phGuiProcessId_t) client_data;
    int length;
    char *input;
    char *secval;
    char *thirdval;
    int requestInt;
    phGuiRequest_t request;
    phGuiComponent_t *c;
    phGuiComponent_t *parseResult;
    fd_set fds;
    struct timeval to;
    struct simListStruct *list, *prev;

    request = (phGuiRequest_t)-1;

    to.tv_sec =  0;
    to.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(process->requestPipe[0], &fds);

    while (select(process->requestPipe[0] + 1, &fds, NULL, NULL, &to) > 0) {

	if ((length = getw(process->requestFp)) != EOF) {
	    input = (char *) malloc(length * sizeof(char));
	    if (fread(input, 1, length - 1, process->requestFp) < length - 1) {
		perror("fread");
		return;
	    }
	    input[length-1] = 0;
	    sscanf(input, "%d", &requestInt);
	    request = (phGuiRequest_t) requestInt;

	    switch (request) {
	      case showDialog:
		if (process->visible == False) {
		    XtRealizeWidget(process->toplevel->frame);
		    resizeComponent((phGuiComponent_t *) process->toplevel);
		    process->visible = True;
		}
		break;

	      case hideDialog:
		XtUnrealizeWidget(process->toplevel->frame);
		process->visible = False;
		break;

	      case disposeDialog:
		exit(0);
		break;

	      case enableComponent:
		secval = (char *) malloc(sizeof(char) * strlen(input) + 3);
		sscanf(input, "%d %s", &requestInt, secval);
		request = (phGuiRequest_t) requestInt;

		c = getComponent((phGuiComponent_t *) process->toplevel, secval);
		if (c != NULL) {
		    if (c->frame != NULL) XtSetSensitive(c->frame, True);
		    else XtSetSensitive(c->w, True);
		}

		free(secval);
		break;

	      case disableComponent:
		secval = (char *) malloc(sizeof(char) * strlen(input) + 3);
		sscanf(input, "%d %s", &requestInt, secval);
		request = (phGuiRequest_t) requestInt;

		c = getComponent((phGuiComponent_t *) process->toplevel, secval);
		if (c != NULL) {
		    if (c->frame != NULL) XtSetSensitive(c->frame, False);
		    else XtSetSensitive(c->w, False);
		}

		free(secval);
		break;

	      case configure:
		parseResult = phGuiParseConf(process, (input + 2));
		if (parseResult != NULL) {
		    if (process->toplevel->firstChild != NULL) {
			XtUnrealizeWidget(process->toplevel->frame);
			freeDialog((phGuiComponent_t *) process->toplevel);
		    }
		    process->toplevel->lastChild = 
			process->toplevel->firstChild = 
			(phGuiComponent_t *) parseResult;
/* Begin of Huatek Modification, Donnie Tu, 04/19/2001 */
/* Issue Number: 5 */		
		    if(parseResult->label!=NULL)
			    process->toplevel->label = 
					strdup(parseResult->label);
		    else
			    process->toplevel->label=NULL;		
/* End of Huatek Modification */

		    parseResult->parent = process->toplevel;
		    setupDialog(process);
		    if (process->visible) {
			XtRealizeWidget(process->toplevel->frame);
			resizeComponent((phGuiComponent_t *) process->toplevel);
		    }
		    sendEvent(process, "parsing succeded", PH_GUI_NO_ERROR);
		}
		else {
                    process->toplevel->lastChild =
			process->toplevel->firstChild = NULL;
		    sendEvent(process, "error in parsing", PH_GUI_ERROR_EVENT);
		}
		break;

	      case requestEvent: /* only important in Simulation Mode */
		dispatchSimAction(process);
		break;

	      case requestData:
		secval = (char *) malloc(sizeof(char) * strlen(input) + 3);
		sscanf(input, "%d %s", &requestInt, secval);
		request = (phGuiRequest_t) requestInt;

		c = getComponent((phGuiComponent_t *) process->toplevel, secval);
		if (c != NULL) {
		    sendData(process, c->name, getComponentData(process, c));
		}
		/*else printf("\nComponent not found!\n\n");*/

		free(secval);
		break;

	      case changeData:
		secval = (char *) malloc(sizeof(char) * strlen(input) + 3);
		sscanf(input, "%d %s", &requestInt, secval);
		request = (phGuiRequest_t) requestInt;

		/* copy third value in request to thirdval (pos: secval+request+2*blank */
		thirdval = strdup(input + strlen(secval) + 3);
		c = getComponent((phGuiComponent_t *) process->toplevel, secval);
		if (c != NULL) {
		    setComponentData(process, c, thirdval);
		}

		free(secval);
		free(thirdval);
		break;

	      case comTest:
		secval = (char *) malloc(sizeof(char) * strlen(input) + 3);
		sscanf(input, "%d %s", &requestInt, secval);
		request = (phGuiRequest_t) requestInt;
		sendEvent(process, secval, PH_GUI_COMTEST_RECEIVED);
		free(secval);
		break;

	      case simAction:
		list = process->simList;
		do {
		    prev = list;
		    list = list->next;
		} while (list != NULL);
		list = (struct simListStruct *) malloc(sizeof(struct simListStruct));
		prev->next = list;
		list->next = NULL;
		list->name = (char *) malloc(sizeof(char) * strlen(input) + 3);
		sscanf(input, "%d %s", &requestInt, list->name);
		request = (phGuiRequest_t) requestInt;
		break;

	      case identifyGui:
		sendEvent(process, 
		    process->toplevel->firstChild->name, 
		    PH_GUI_IDENTIFY_EVENT);
		break;

	      default:
/*		fprintf(stderr, "GuiProcess: Unknown command %ld\n", request);*/
		break;
    
	    }
	    free(input);
	}
	else {
	    perror("getc");
	}
    }
    XtAppAddTimeOut(process->context, 
	50, 
	GetInputProc, 
	process);
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void AppMainLoop(phGuiProcessId_t process) {
    XEvent event;

    sem_signal(process->semid);
  
    while (True) {
        XtAppNextEvent(process->context, &event);
        XtDispatchEvent(&event);
    }
}


/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
void initColors(
    phGuiProcessId_t process
    )
 {
    XColor act_color, exact_color;
    Display *display;
    Screen *screen;

    display = XtDisplay(process->toplevel->frame);
    screen = XtScreen(process->toplevel->frame);

    XAllocNamedColor(
	display, 
	DefaultColormapOfScreen(screen),
	"yellow",
	&act_color,
	&exact_color);
    process->color[0] = act_color.pixel;

    XAllocNamedColor(
	display, 
	DefaultColormapOfScreen(screen),
	"black",
	&act_color,
	&exact_color);
    process->color[1] = act_color.pixel;    

}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
int LoadFont(
    phGuiProcessId_t process,
    char *fontName
    )
{
    XFontStruct *font_info = NULL;
    XmFontListEntry entry;
    Display *display;

    display = XtDisplay(process->toplevel->frame);

    font_info = XLoadQueryFont(display, fontName);
    if (font_info == NULL)
    {
	fprintf(stderr, "guiProcess: font %s not found\n", fontName);
	return 0;
    }
    entry = XmFontListEntryCreate("", XmFONT_IS_FONT, font_info);
    process->fontList = XmFontListAppendEntry(process->fontList, entry);
    return 1;
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
phGuiProcessError_t initClient(
    phGuiProcessId_t process
    )
{
    int argc_in_out = 0;
    String argv_in_out;
    int fontFound = 0;

    /* create the toplevel shell */
    XtSetLanguageProc(NULL, NULL, NULL);
    process->toplevel->frame = 
	XtVaAppInitialize(&process->context, "phGuiProcess", NULL, 0, &argc_in_out, &argv_in_out, NULL, NULL);

    if (process->toplevel->frame == NULL) 
	return PH_GUIP_ERR_UNKNOWN;

    initColors(process);

    process->fontList = NULL;
    fontFound += LoadFont(process, "-*-helvetica-medium-r-*--14-140-*-*-p-77-*");
    fontFound += LoadFont(process, "8x16");
    fontFound += LoadFont(process, "8x13");

    if (!fontFound)
    {
	fprintf(stderr, "guiProcess: no fonts found, giving up\n");
	return PH_GUIP_ERR_UNKNOWN;
    }

    XtAppAddTimeOut(process->context, 
	50, 
	GetInputProc, 
	process);
    
    AppMainLoop(process);

    return PH_GUIP_ERR_OK;
}

/*****************************************************************************
 *
 * <give a SINGLE LINE function description here>
 *
 * Authors: Ulrich Frank, SSTD-R&D
 *
 * History: <please describe change history here, covering date, your
 *          name, and change description>
 *
 * Description: 
 * <The Interface function description should be put into
 * the corresponding header file for this function, because it may be
 * used for automatic man page generation. Please describe internal
 * details here and use comments within the code block below!> 
 *
 ***************************************************************************/
int main(int argc, char *argv[], char *envp[]) {
    struct phGuiProcessStruct *tmp;
    phGuiProcessError_t initError;
    char pname[256];

#ifdef DEBUG
    printf("guiProcess started with argv[0] = '%s'\n", argv[0]);
#ifdef STOP_AT_DEBUG
    printf("guiProcess stopped, press <return> to continue....");
    (void) getchar();
#endif
#endif
    /* allocate new guiProcess data type */
    tmp = (struct phGuiProcessStruct *) 
        malloc(sizeof(struct phGuiProcessStruct));
    if (tmp == 0) {
	return PH_GUI_ERR_MEMORY;
    }
    tmp->myself = tmp;

    tmp->simList = (struct simListStruct *) malloc(sizeof(struct simListStruct));
    tmp->simList->next = NULL;

    tmp->visible = False;
    tmp->toplevel = createGroup(PH_GUI_CONT_TOP, NULL, NULL, NULL);
    tmp->toplevel->nr = 1;

    if (argc != 1 ||
	sscanf(argv[0], "%s %d %d %d %d",
	    pname,
	    &tmp->requestPipe[0],
	    &tmp->eventPipe[1],
	    &tmp->dataPipe[1],
	    &tmp->semid) != 5)
    {
	fprintf(stderr, 
	    "This is not a stand alone program.\n"
	    "It will only be used in conjunction with a gui server library\n"
	    "from the generic handler and prober drivers\n");
	exit(1);
    }

    tmp->requestFp = fdopen(tmp->requestPipe[0], "rb"); 
    tmp->eventFp   = fdopen(tmp->eventPipe[1], "wb");
    tmp->dataFp    = fdopen(tmp->dataPipe[1], "wb");

    setbuf(tmp->requestFp, NULL);
    setbuf(tmp->eventFp, NULL);
    setbuf(tmp->dataFp, NULL);

    thisProcess = tmp;
    signal(SIGTERM, quitHandler);
    signal(SIGINT, quitHandler);
    signal(SIGHUP, quitHandler);
    signal(SIGQUIT, quitHandler);

    initError = initClient(tmp);
    if (initError != PH_GUIP_ERR_OK)
    {
	fprintf(stderr, "guiProcess: initClient failed, error %d\n", 
	    (int) initError);
	exit(1);
    }
    else 
	exit(0);
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
