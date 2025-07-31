/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : sim.c
 * CREATED  : 01 Feb 2000
 *
 * CONTENTS : prober simulator
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 01 Feb 2000, Michael Vogt, created
 *            25 Oct 2000, Chris Joyce 
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

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_log.h"
#include "ph_mhcom.h"
#include "ph_GuiServer.h"
#include "ph_diag.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif


/*--- defines ---------------------------------------------------------------*/

#define END_OF_COMMAND "\r\n"

#define SETUP_HARDWARE "\n"                                  \
    "    Please ensure the following: \n"                    \
    "\n"                                                     \
    "      o SmarTest is currently not actively running \n"  \
    "\n"                                                     \
    "      o Hardware is set-up as shown below \n"           \
    "\n"                                                     \
    "     +-------------------------+ \n"                    \
    "     |                         | \n"                    \
    "     |                         | \n"                    \
    "     |          This           |                        +------------+ \n"  \
    "     |       Workstation       |                        |            | \n"  \
    "     |                         |                        |  External  | \n"  \
    "     |                         |                        |   Device   | \n"  \
    "     |                         |                        |            | \n"  \
    "     +-------------------------+                        +------------+ \n"  \
    "               gpib                                          gpib \n"       \
    "             interface ------------ gpib cable ----------- interface \n"    \
    " \n"  \
    " \n"  \
    " \n"


/*--- global variables ------------------------------------------------------*/


static phLogId_t logId = NULL;
static phComId_t comId = NULL;

/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 285 */
/*-----functions prototype----------------------------------------------------*/
/* End of Huatek Modification */

/*--- functions -------------------------------------------------------------*/

int arguments(
    int argc, char *argv[], 
    int *debugLevel,
    const char **hpibDevice,
    int *port,
    int *master,
    int *timeout
)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;

    /* setup default: */

    while ((c = getopt(argc, argv, ":sd:i:p:t:")) != -1)
    {
	switch (c) 
	{
	  case 's':
	    *master = 0;
	    break;
	  case 'd':
	    *debugLevel = atoi(optarg);
	    break;
	  case 'i':
	    *hpibDevice = strdup(optarg);
	    break;
	  case 'p':
	    *port = atoi(optarg);
	    break;
	  case 't':
	    *timeout = atoi(optarg);
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
	    fprintf(stderr, "usage: %s [<options>]\n", argv[0]);
	    fprintf(stderr, "    [-d <debug-level]         -1 to 4, default 0\n");
	    fprintf(stderr, "    [-i <gpib-interface>]     GPIB interface device, default hpib\n");
	    fprintf(stderr, "    [-t <timeout>]            GPIB timeout to be used (slave mode only, obsoleted!!!) \n");
	    fprintf(stderr, "                              [msec], default 5000\n");
	    fprintf(stderr, "    [-p port]                 port to connect to (master mode only), default 22\n");
	    fprintf(stderr, "    [-s]                      operate in slave mode, obsoleted!!!\n");
	    fputs(SETUP_HARDWARE,stderr);
	    return 0;
	}
    }
    return 1;
}


int main(int argc, char *argv[])
{

    phDiagError_t diagError;
    phDiagId_t diagID;

    int debugLevel = 0;
    const char *hpibDevice = "hpib";
    int port = 22;
    int master = 1;
    int timeout = 5000;

    phLogMode_t modeFlags = PHLOG_MODE_NORMAL;
    char response[128];

    if (!arguments(argc, argv, &debugLevel, &hpibDevice, &port, &master, &timeout))
	exit(1);

    master = 1;

    switch (debugLevel)
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

    phLogInit(&logId, modeFlags, "-", NULL, "-", NULL, 0);

    phGuiShowOptionDialog(logId, PH_GUI_OK_CANCEL,
            "GPIB COMMUNICATION TOOL\n"
            "\n"
            "ATTENTION:\n"
            "\n"
            "This tool is likely to interfere with any GPIB communication currently taking place.\n"
            "\n"
            "It should not be used while SmarTest is actively running a GPIB session with an\n"
            "external handler/prober. \n"
            "\n"
            "This tool should also not be left running after its use, as it is likely to prevent\n"
            "SmarTest from successfully communicating with external GPIB handler/probers.\n"
            "\n"
            "Press [CANCEL] to terminate, press [OK] to start running GPIB Communication tool. \n",
            response);

    if (strcasecmp(response, "cancel") == 0)
    {
	exit(1);
    }

    phLogDiagMessage(logId, PHLOG_TYPE_MESSAGE_0,
                     "master on device '%s', connect to port %d", 
                     hpibDevice, port);

    if (phComGpibOpen(&comId, PHCOM_ONLINE, hpibDevice, port, logId,
	PHCOM_GPIB_EOMA_NONE) != PHCOM_ERR_OK)
    {
        phLogDiagMessage(logId,


                          PHLOG_TYPE_FATAL,
                          "error returned from GpibOpen ");
	exit(1);
    }

    diagError=phDiagnosticsInit( &diagID, comId, logId);
    if ( diagError != DIAG_ERR_OK )
    {
        phLogDiagMessage(logId,
                          PHLOG_TYPE_FATAL,
                          "error returned from diagnostics initialization (%d)",
                          diagError);
	exit(1);
    }

    diagError=gpibDiagnosticsMaster(diagID,STAND_ALONE);

    return 0;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
