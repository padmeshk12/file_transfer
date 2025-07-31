/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : phf_init.c
 * CREATED  : 14 Jul 1999
 *
 * CONTENTS : Framework initialization
 *
 * AUTHORS  : Michael Vogt, SSTD-R&D, initial revision
 *            Jiawei Lin, R&D Shanghai, CR28409
 *            Jiawei Lin, R&D Shanghai, CR42750
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Jul 1999, Michael Vogt, created
 *            Dec 2005, Jiawei Lin, CR28409
 *              allow the user to enable/disable diagnose window by specifying
 *              the driver parameter "enable_diagnose_window"
 *            Oct 2008, Jiawei Lin, CR42750
 *              support the "gpib_end_of_message_action" parameter
 *            7 May 2015, Magco Li, CR93846
 *              add rs232 interface to initCommLink()
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

#include "ci_types.h"

#include "ph_tools.h"
#include "ph_keys.h"

#include "ph_log.h"
#include "ph_conf.h"
#include "ph_state.h"
#include "ph_estate.h"
#include "ph_mhcom.h"
#include "ph_hfunc.h"
#include "ph_hhook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"
#include "ph_plugin.h"
#include "ph_GuiServer.h"

#include "ph_frame_private.h"
#include "reconfigure.h"
#include "phf_init.h"
#include "ph_timer.h"
#include "idrequest.h"
#include "exception.h"
#include "dev/DriverDevKits.hpp"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

/*--- defines ---------------------------------------------------------------*/

#define BUFF_LENGTH 1024
#define INITIAL_MSG_LOG    "/tmp/phlib_initial_msg_log"
#define INITIAL_ERROR_LOG  "/tmp/phlib_initial_error_log"

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/*Begin of Huatek Modifications, Luke Lan, 02/22/2001 */
/*Issue Number: 36   */
/*In string.h file, strdup() has been declared. So comment it*/
/*extern char *strdup(const char *str); */
/*End of Huatek Modifications. */

static int checkServiceStatus(const char* serviceName, struct phFrameStruct *frame)
{
    char szPopenCMD[BUFF_LENGTH] = {0};
    sprintf(szPopenCMD, "pgrep %s", serviceName);

    FILE *pInfo = popen(szPopenCMD, "r");
    phLogFrameMessage(frame->logId, LOG_DEBUG, "Will execute shell command: \"%s\" to query service existence", szPopenCMD);
    if(NULL == pInfo)
    {
        phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR, "Execute shell command failed, please manually execute the shell command!");
        return -1;
    }

    char cmdResultBuff[BUFF_LENGTH] = {0};
    fgets(cmdResultBuff, BUFF_LENGTH, pInfo);

    pclose(pInfo);
    pInfo = NULL;

    if(cmdResultBuff[0] == '\0')
        return 0;
    return 1;
}

static int searchAttribFile(char *config, char *plugin, char *attrib)
{
    char *pathDelPos;
    FILE *testFP;

    /* construct the name of the attributes file out of the pathname
       of the configuration file */
    *attrib = '\0';
    strncpy(attrib, config, 1023);
    pathDelPos = strrchr(attrib, '/');
    if (pathDelPos)
    {
	pathDelPos++;
	strncpy(pathDelPos, PHLIB_S_ATTR_FILE, 
	    1023-(pathDelPos-attrib));
    }
    else
	strcpy(attrib, PHLIB_S_ATTR_FILE);

    testFP = fopen(attrib, "r");
    if (testFP)
    {
	/* found next to configuration */
	fclose(testFP);
	return 1;
    }

    /* construct the name of the attributes file out of the pathname
       of the plugin */
    *attrib = '\0';
    strncpy(attrib, plugin, 
	1023 - strlen("/config/") - strlen(PHLIB_S_ATTR_FILE));
    if (attrib[strlen(attrib)-1] != '/')
	strcat(attrib, "/");
    strcat(attrib, "config/");
    strcat(attrib, PHLIB_S_ATTR_FILE);

    testFP = fopen(attrib, "r");
    if (testFP)
    {
	/* found in plugin/config */
	fclose(testFP);
	return 1;
    }

    /* not found */
    *attrib = '\0';
    return 0;
}


/*****************************************************************************
 *
 * init equipment communication link
 *
 * Authors: Michael Vogt
 *
 * History: 30 May 2000, Michael Vogt, extracted from global init function
 *
 * Description: Initialize the communication link.
 *
 ***************************************************************************/
static void initCommLink(struct phFrameStruct *frame)
{
    phConfError_t confError;
    phComError_t comError;

    const char *commMode = NULL;
    phComMode_t drvMode;
    phComGpibEomAction_t eomAction = PHCOM_GPIB_EOMA_NONE;
    const char *interfaceType = NULL;
    const char *interfaceName = NULL;
    double busyThreshold = 0.0;
    long drvBusyDelay;
    double pollingInterval = 0.0;
    long drvPollTime;
    double gpibPort = 0.0;
    double serverPort = 0.0;
    int drvGpibPort;
    // RS232 parameter
    double baudRate = 0.0;

    double dataBits = 0.0;

    double stopBits = 0.0;

    double eoi = 0.0;

    double filterTime = 0.0;
    long drvFilterTime;
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/* End of Huatek Modification */

    int found;
    int selection = 0;

    /* get configuration for the communication link */
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_MO_ONLOFF);
    if (confError == PHCONF_ERR_OK)
	confError = phConfConfString(frame->specificConfId, 
	    PHKEY_MO_ONLOFF, 0, NULL, &commMode);
    if (confError != PHCONF_ERR_OK)
	commMode = NULL;

    /* further analysis of communication configuration */
    if (commMode == NULL)
	drvMode = PHCOM_ONLINE;
    else
	drvMode = strcasecmp(commMode, "on-line") == 0 ?
	    PHCOM_ONLINE : PHCOM_OFFLINE;


    /* get configuration to find out what kind of interface we are working on */
    confError = phConfConfString(frame->specificConfId, 
        PHKEY_IF_TYPE, 0, NULL, &interfaceType);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not get interface type from configuration, giving up");
        return;
    }
    
    if(strcasecmp(interfaceType,"lan-server") !=0)
    {
         confError = phConfConfString(frame->specificConfId, 
                	    PHKEY_IF_NAME, 0, NULL, &interfaceName);
        if (confError != PHCONF_ERR_OK)
	        frame->errors++;
    }
    if (strcasecmp(interfaceType, "gpib") == 0)
    {
        frame->connectionType = PHCOM_IFC_GPIB;
	confError = phConfConfNumber(frame->specificConfId,
	    PHKEY_IF_PORT, 0, NULL, &gpibPort);
	if (confError != PHCONF_ERR_OK)
	    frame->errors++;
	drvGpibPort = (int) gpibPort;

      /*
       * the following code is to support the parameter "gpib_end_of_message"
       * CR42750, Jiawei Lin, Oct 2008
       */
      confError = phConfConfStrTest(&selection, frame->specificConfId, PHKEY_GB_EOMACTION,
                                    "none", "toggle-ATN", "serial-poll", NULL);
      if ( (confError != PHCONF_ERR_OK) && (confError != PHCONF_ERR_UNDEF) ) {
        frame->errors++;
      }

      switch(selection) {
        case 2:  /* toggle-ATN */
          eomAction = PHCOM_GPIB_EOMA_TOGGLEATN;
          break;
        case 3:  /* serial poll */
          eomAction = PHCOM_GPIB_EOMA_SPOLL;
          break;
        case 0:  /* no given */
        case 1:  /* "none" specified*/
        default: /* any other value */
          eomAction = PHCOM_GPIB_EOMA_NONE;
          break;
      }


	if (frame->errors)
	{
	    /* are there any errors, also during above analysis? */
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
		"configuration for communication interface incomplete or wrong");
	    return;
	}

        do
        {
            phLogFrameMessage(frame->logId, LOG_DEBUG,
                "initializing GPIB interface \"%s\" for port %d", interfaceName, drvGpibPort);
            comError = phComGpibOpen(&(frame->comId), drvMode, interfaceName, drvGpibPort, frame->logId, eomAction);
            if (comError != PHCOM_ERR_OK)
            {
                char response[128];

                switch (comError)
                {
                  case PHCOM_ERR_GPIB_ICLEAR_FAIL:
                    /* there seems to be no listener on the GPIB bus */
                    frame->warnings++;
                    phLogFrameMessage(frame->logId, PHLOG_TYPE_WARNING,
                        "no handler seems to be connected to the GPIB interface");
                    response[0] = '\0';
                    phGuiShowOptionDialog(frame->logId, PH_GUI_ERROR_ABORT_RETRY,
                        "Handler driver error:\n"
                        "\n"
                        "Starting up the GPIB communication timed out.\n"
                        "The reason could be that no handler is connected\n"
                        "to the GPIB interface.\n"
                        "\n"
                        "Please ensure that the handler is correctly connected\n"
                        "and press RETRY.\n"
                        "\n"
                        "Otherwise press ABORT to quit the testprogram.\n",
                        response);
                    if (strcasecmp(response, "retry") == 0)
                        break;
                    /* else fall into default case ... */
                  default:
                    frame->errors++;
                    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                        "could not open GPIB interface, giving up (error %d)",
                        (int) comError);
                    return;
                }
            }
        } while (comError != PHCOM_ERR_OK);
    }
    else if (strcasecmp(interfaceType, "lan") == 0)
    {
      frame->connectionType = PHCOM_IFC_LAN;
      phLogFrameMessage(frame->logId, LOG_DEBUG, "initializing LAN interface \"%s\"", interfaceName);
      comError = phComLanOpen(&(frame->comId), drvMode, interfaceName, frame->logId);
      if (comError != PHCOM_ERR_OK)
      {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                          "could not open LAN interface, giving up (error %d)",
                          (int) comError);
        return;
      }
    }
     else if (strcasecmp(interfaceType, "lan-server") == 0)
    {
      frame->connectionType = PHCOM_IFC_LAN_SERVER;
      confError = phConfConfNumber(frame->specificConfId, PHKEY_IF_LAN_PORT, 0, NULL, &serverPort);
      if (confError != PHCONF_ERR_OK)
      {
          frame->errors++;
          phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,"must define the port number when set lan-server interface type, giving up(error %d)", (int) confError);
          return;
      }
      phLogFrameMessage(frame->logId, LOG_DEBUG, "create LAN server socket with port \"%d\"", (int)serverPort);
      comError = phComLanServerCreateSocket(&(frame->comId), drvMode, (int)serverPort, frame->logId);
      if (comError != PHCOM_ERR_OK)
      {
          frame->errors++;
          phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL, "could not open LAN interface, giving up (error %d)", (int) comError);
          return;
      }

      while( (comError = phComLanServerWaitingForConnection(frame->comId, frame->logId)) == PHCOM_ERR_TIMEOUT)
      {
          //check system flag here
            long abortFlag = 0;
            phTcomGetSystemFlag(frame->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
            if(abortFlag)
            {
                frame->errors++;
                phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL, "abort flag was set, giving up on executing prober/handler driver call");
                phComLanClose(frame->comId);
                return;
            }
      }
      if (comError != PHCOM_ERR_OK)
      {
          frame->errors++;
          phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL, "could not open LAN interface, giving up (error %d)", (int) comError);
          return;
      }
    }
    else if (strcasecmp(interfaceType, "rs232") == 0)
    {
      struct phComRs232Struct rs232Config;
      phConfType_t defType;
      int found = 0;
      int listLength = 0;
      int i = 0;
      char hex[10] = "";

      confError = phConfConfNumber(frame->specificConfId,
          PHKEY_NAME_RS232_BAUDRATE, 0, NULL, &baudRate);
      if (confError != PHCONF_ERR_OK)
      {
        frame->errors++;
      }
      rs232Config.baudRate = (int) baudRate;

      confError = phConfConfNumber(frame->specificConfId,
          PHKEY_NAME_RS232_DATABITS, 0, NULL, &dataBits);
      if (confError != PHCONF_ERR_OK)
      {
        frame->errors++;
      }
      rs232Config.dataBits = (int) dataBits;

      confError = phConfConfNumber(frame->specificConfId,
          PHKEY_NAME_RS232_STOPBITS, 0, NULL, &stopBits);
      if (confError != PHCONF_ERR_OK)
      {
        frame->errors++;
      }
      rs232Config.stopBits = (int) stopBits;

      confError = phConfConfStrTest(&found, frame->specificConfId,
          PHKEY_NAME_RS232_PARITY, "none", "even", "odd", "mark", "space", NULL);
      if (confError != PHCONF_ERR_OK)
      {
        frame->errors++;
      }

      strcpy(rs232Config.parity, "none");
      switch(found)
      {
          case 1:
              strcpy(rs232Config.parity, "none");
              break;
          case 2:
              strcpy(rs232Config.parity, "even");
              break;
          case 3:
              strcpy(rs232Config.parity, "odd");
              break;
          case 4:
              strcpy(rs232Config.parity, "mark");
              break;
          case 5:
              strcpy(rs232Config.parity, "space");
              break;
          default:
              strcpy(rs232Config.parity, "none");
              phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR, "configuration parameter \"%s\" is not available for the rs232 parity", PHKEY_NAME_RS232_PARITY);
              break;
      }

      confError = phConfConfStrTest(&found, frame->specificConfId,
          PHKEY_NAME_RS232_FLOWCONTROL, "none", "xon_xoff", "rts_cts", NULL);
      if (confError != PHCONF_ERR_OK)
      {
        frame->errors++;
      }

      strcpy(rs232Config.flowControl, "none");
      switch(found)
      {
          case 1:
              strcpy(rs232Config.flowControl, "none");
              break;
          case 2:
              strcpy(rs232Config.flowControl, "xon_xoff");
              break;
          case 3:
              strcpy(rs232Config.flowControl, "rts_cts");
              break;
          default:
              phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR, "configuration parameter \"%s\" is not available for the rs232 flow control", PHKEY_NAME_RS232_FLOWCONTROL);
              break;
      }

      bzero(rs232Config.eois, sizeof(rs232Config.eois));
      rs232Config.eoisNumber = 0;
      confError = phConfConfIfDef(frame->specificConfId, PHKEY_NAME_RS232_EOIS);
      if (confError == PHCONF_ERR_OK)
      {
          confError = phConfConfType(frame->specificConfId,
              PHKEY_NAME_RS232_EOIS, 0, NULL, &defType, &listLength);
          if (confError != PHCONF_ERR_OK || defType != PHCONF_TYPE_LIST)
          {
              phLogFrameMessage(frame->logId, PHLOG_TYPE_ERROR,
                   "configuration '%s' of wrong type,\n",
                    PHKEY_NAME_RS232_EOIS);
              frame->errors++;
          }
          rs232Config.eoisNumber = listLength;
          for (i=0; i<listLength; i++)
          {
              confError = phConfConfNumber(frame->specificConfId,
                  PHKEY_NAME_RS232_EOIS, 1, &i, &eoi);
              if (confError != PHCONF_ERR_OK)
                  frame->errors++;
              else
              {
                  sprintf(hex, "0x%X", (int)eoi);
                  rs232Config.eois[i] = strtol(hex, NULL, 16);
              }
          }
      }
      strcpy(rs232Config.device, interfaceName);

      frame->connectionType = PHCOM_IFC_RS232;
      phLogFrameMessage(frame->logId, LOG_DEBUG, "initializing RS232 interface \"%s\"", interfaceName);
      rs232Config.gSimulatorInUse = 0;
      comError = phComRs232Open(&(frame->comId), drvMode, rs232Config, frame->logId);
      if (comError != PHCOM_ERR_OK)
      {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                          "could not open rs232 interface, giving up (error %d)",
                          (int) comError);
        return;
      }
    }
    else
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "unknown interface type \"%s\", giving up", interfaceType);
	return;
    }
}

/*****************************************************************************
 *
 * make driver
 *
 * Authors: Michael Vogt
 *
 * History: 30 May 2000, Michael Vogt, extracted from prober framework
 *
 * Description: Create the driver framework.
 *
 ***************************************************************************/
static struct phFrameStruct *makeDriverFramework(void)
{
    struct phFrameStruct *frame;
    
    /* allocate frame struct space */
    frame = PhNew(struct phFrameStruct);
    if (!frame)
    {
	fprintf(stderr, "PH lib: FATAL! "
	    "Could not allocate memory for driver initialization");
	return NULL;
    }

    /* be sure to reset all at the beginning */
    frame->isInitialized = 0;
    frame->initTried = 1;
    frame->errors = 0;
    frame->warnings = 0;

    frame->currentAMCall = PHFRAME_ACT_DRV_START;

    frame->logId = NULL;
    frame->globalConfId = NULL;
    frame->specificConfId = NULL;
    frame->attrId = NULL;
    frame->testerId = NULL;
    frame->stateId = NULL;
    frame->estateId = NULL;
    frame->comId = NULL;
    frame->funcId = NULL;
    frame->eventId = NULL;

    frame->funcAvail = (phFuncAvailability_t) 0;
    frame->hookAvail = (phHookAvailability_t) 0;

    frame->numSites = 0;
    /* frame->stToHandlerSiteMap[PHESTATE_MAX_SITES] = ... */

    frame->binMode =  PHBIN_MODE_DEFAULT;
    frame->binMapHdl = NULL;
    frame->dontTrustBinning = 0;

    frame->autoReprobe = PHFRAME_RPMODE_ALL;
    frame->lotLevelUsed = 0;

    frame->totalTimer = NULL;
    frame->shortTimer = NULL;

    frame->initialPauseFlag = 0L;
    frame->initialQuitFlag = 0L;

    frame->createTestLog = 0;
    frame->testLogOut = NULL;

    /* be default we enable the popup window in case of error, CR 28409 */
    frame->enableDiagnoseWindow = YES;  

    /* initialize timer elements for later usage */
    frame->totalTimer = phFrameGetNewTimerElement();    
    frame->shortTimer = phFrameGetNewTimerElement();

    frame->connectionType = PHCOM_IFC_GPIB;

    return frame;
}


/*****************************************************************************
 *
 * init driver
 *
 * Authors: Michael Vogt
 *
 * History: 15 Jul 1999, Michael Vogt, created
 *
 * Description: Initialize the driver framework. Do everything which
 * is necessary: Create handles for all sub modules, initialize these
 * modules
 *
 ***************************************************************************/
struct phFrameStruct *initDriverFramework(void)
{
    struct phFrameStruct *frame = NULL;

    int tmpErrors = 0;
    int tmpWarnings = 0;
    phLogError_t logError;
    phConfError_t confError;
    phTcomError_t testerError;
    phStateError_t stateError;
    phEstateError_t estateError;
    phPlugError_t plugError;
/* Begin of Huatek Modification, Donnie Tu, 12/13/2001 */
/* Issue Number: 274 */
    phFuncError_t funcError=PHFUNC_ERR_OK;
/* End of Huatek Modification */
    phHookError_t hookError;
    phEventError_t eventError;
    phUserDialogError_t userDialogError;

    const char *envConfig = NULL;
    const char *driverPlugin = NULL;
    const char *driverConf = NULL;
    char usedDriverPlugin[1024];
    char usedDriverConf[1024];
    char usedDriverAttr[1024];
/* Begin of Huatek Modification, Donnie Tu, 12/12/2001 */
/* Issue Number: 315 */
/*    char *pathDelPos;*/
/* End of Huatek Modification */

    phTcomMode_t testerMode;
    const char *testerSimulation;

    const char *idString;

    long abortFlag;

    int completed;
    int success;

    /* be sure to reset all at the beginning */
    frame = makeDriverFramework();

    /* first initialize the logger. It is used by every other
       module. Since no log files are defined so far, two dedicated
       files are used in /tmp */

#ifdef DEBUG
    logError = phLogInit(&frame->logId, PHLOG_MODE_ALL, 
	"-", INITIAL_ERROR_LOG, "-", INITIAL_MSG_LOG, -1);
#else
    logError = phLogInit(&frame->logId, (phLogMode_t)(PHLOG_MODE_NORMAL|PHLOG_MODE_DBG_1),
	"-", INITIAL_ERROR_LOG, "-", INITIAL_MSG_LOG, -1);
#endif

    if (logError != PHLOG_ERR_OK && logError != PHLOG_ERR_FILE)
    {
        frame->errors++;
        fprintf(stderr, "PH lib: FATAL! Could not initialize logger (error %d)\n", (int) logError);
        return frame;
    }

    if(1 == checkServiceStatus("firewalld", frame))
        phLogFuncMessage(frame->logId, PHLOG_TYPE_WARNING, "Detected firewall service which may impact on the SRQ handling, please stop the service by following commands: \"systemctl stop firewalld.service\"");

    if(0 == checkServiceStatus("rpcbind", frame))
    {
        phLogFuncMessage(frame->logId, PHLOG_TYPE_WARNING, "Didn't detect rpcbind service, may break PH communication, please start the service by following commands: \"systemctl start rpcbind.service\"");
        driver_dev_kits::AlarmController ac;
        ac.raiseCommunicationAlarm("Indispensable rpcbind service is missing");
    }

    /* now initialize the configuration manager. */
    confError = phConfInit(frame->logId);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not initialize configuration (error %d)", (int) confError);
        return frame;
    }

    /* read in the global configuration file. It determines, were to find
       the driver plugin and were to read the detailed configuration */
    envConfig = getenv(PHLIB_ENV_GCONF);
    if (envConfig && strlen(envConfig) != 0)
    {
	phLogFrameMessage(frame->logId, LOG_NORMAL, 
	    "looking for global configuration file \"%s\"\n"
	    "(environment variable %s is set)", 
	    envConfig, PHLIB_ENV_GCONF);
	confError = phConfReadConfFile(&frame->globalConfId, 
	    envConfig);	
    }
    if (!envConfig || strlen(envConfig) == 0)
    {
	phLogFrameMessage(frame->logId, LOG_NORMAL, 
	    "looking for global configuration file \"%s\"", 
	    PHLIB_CONF_PATH "/" PHLIB_CONF_DIR "/" PHLIB_GB_CONF_FILE);
	confError = phConfReadConfFile(&frame->globalConfId, 
	    PHLIB_CONF_PATH "/" PHLIB_CONF_DIR "/" PHLIB_GB_CONF_FILE);
    }
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not read global configuration (error %d)", (int) confError);
        return frame;
    }

    /* from the global configuration, get the name of the driver
       specific configuration */
    confError = phConfConfString(frame->globalConfId, 
	PHKEY_G_PLUGIN, 0, NULL, &driverPlugin);
    if (confError != PHCONF_ERR_OK)
	frame->errors++;
    confError = phConfConfString(frame->globalConfId, 
	PHKEY_G_CONFIG, 0, NULL, &driverConf);
    if (confError != PHCONF_ERR_OK)
	frame->errors++;
    if (frame->errors>0)
    {
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "basic configuration settings could not be read.\n");
        return frame;	
    }
    reconfigureCompletePath(usedDriverPlugin, driverPlugin);
    reconfigureCompletePath(usedDriverConf, driverConf);

    /* read in the specific configuration file. */
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"looking for specific configuration file \"%s\"", usedDriverConf);

    confError = phConfReadConfFile(&frame->specificConfId, usedDriverConf);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not read specific configuration (error %d)", 
                (int) confError);
        return frame;
    }

    /* construct the name of the attributes file out of the pathname
       of the configuration file */
    if (!searchAttribFile(usedDriverConf, usedDriverPlugin, usedDriverAttr))
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not locate attributes file \"%s\"\n"
                "(searched next to file %s/config and \n"
                "in directory %s)", 
                PHLIB_S_ATTR_FILE, usedDriverConf, usedDriverPlugin);
        return frame;	
    }
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"looking for specific attributes file \"%s\"", usedDriverAttr);

    /* read the attributes file */
    confError = phConfReadAttrFile(&frame->attrId, usedDriverAttr);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not read attributes file (error %d)", 
                (int) confError);
        return frame;
    }
    
    /* check configuration against attributes */
    confError = phConfValidate(frame->specificConfId, frame->attrId);
    if (confError != PHCONF_ERR_OK)
    {
        frame->errors++;
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "The handler specific configuration is not valid, "
                "giving up (error %d)", 
                (int) confError);
        return frame;
    }

    /* Now first get the location of the final log files before doing
       anything else. Ignore the case, that log files have not been
       specified, but print out a warning */
    reconfigureLogger(frame, &tmpErrors, &tmpWarnings);
    frame->errors += tmpErrors;
    frame->warnings += tmpWarnings;

    /* Now we have a valid and working configuration and also a
       working log mechanism. Now all remaining modules are
       initialized and handles are passed around */

    /* identify myself, now where the logger logs.... */
    phFrameGetID(frame, PHKEY_NAME_ID_DRIVER, &idString);

    /* initialize the tester communication */
    testerMode = PHTCOM_MODE_ONLINE;
    confError = phConfConfIfDef(frame->specificConfId, PHKEY_MO_STSIM);
    if (confError == PHCONF_ERR_OK)
    {
	confError = phConfConfString(frame->specificConfId, PHKEY_MO_STSIM,
	    0, NULL, &testerSimulation);
	if (confError == PHCONF_ERR_OK && 
	    strcasecmp(testerSimulation, "yes") == 0)
	    testerMode = PHTCOM_MODE_SIMULATED;
    }
    testerError = phTcomInit(&(frame->testerId), frame->logId, testerMode);
    if (testerError != PHTCOM_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize link to tester, giving up (error %d)", 
	    (int) testerError);
	return frame;
    }

    /* before doing anything else, check the abort flag. This may be a
       retry or restart of the driver after a fatal situation, we
       don't want to go on then.... */
    abortFlag = 0;
    phTcomGetSystemFlag(frame->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if (abortFlag)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "abort flag was set, giving up on executing prober/handler driver call");
	return frame;
    }

    /* initialize the state controller */
    stateError = phStateInit(&(frame->stateId),
	frame->testerId, frame->logId);
    if (stateError != PHSTATE_ERR_OK)
	frame->errors++;
    
    /* initialize the equipment specific state controller */
    estateError = phEstateInit(&(frame->estateId), frame->logId);
    if (estateError != PHESTATE_ERR_OK)
        frame->errors++;
    
    /* set some parameters for the state controller */
    reconfigureStateController(frame, &tmpErrors, &tmpWarnings);
    frame->errors += tmpErrors;
    frame->warnings += tmpWarnings;
    if (frame->errors)
    {
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize state controller, giving up");
	return frame;
    }
 
    reconfigureSiteManagement(frame, &tmpErrors, &tmpWarnings);
    frame->errors += tmpErrors;
    frame->warnings += tmpWarnings;
    if (frame->errors)
    {
        phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
                "could not initialize site management, giving up (error %d)", 
                (int) stateError);
        return frame;
    }
    
    /* initialize hardware interface (GPIB, RS232, LAN...) */

    initCommLink(frame);
    if (frame->errors)
	return frame;
	
    /* activate driver plugin */
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"trying to load driver plugin \"%s\"", usedDriverPlugin);
    plugError = phPlugLoadDriver(frame->logId, usedDriverPlugin, 
	&frame->funcAvail);
    if (plugError != PHPLUG_ERR_OK && plugError != PHPLUG_ERR_DIFFER)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not load handler specific driver plugin, "
	    "giving up (error %d)", 
	    (int) plugError);
	return frame;
    }
    if ((frame->funcAvail & PHFUNC_AV_MINIMUM) != PHFUNC_AV_MINIMUM)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "Minimum functional requirements for driver plugin not met, "
	    "giving up");
	return frame;
    }
    
    /* initialize handler specific driver */
    phLogFrameMessage(frame->logId, LOG_NORMAL,
	"waiting for handler to initialize");
    phFrameStartTimer(frame->totalTimer);
    phFrameStartTimer(frame->shortTimer);
    success = 0; 
    completed = 0;
    while (!success && !completed)
    {
	funcError = phPlugFuncInit(&frame->funcId, frame->testerId, frame->comId, frame->logId, 
	    frame->specificConfId, frame->estateId);
	phFrameHandlePluginResult(frame, funcError, PHFUNC_AV_INIT,
	    &completed, &success, NULL);
    }

    if (!success)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize handler specific driver plugin, "
	    "giving up (error %d)", 
	    (int) funcError);
	return frame;
    }

    /* activate hook plugin */
    phLogFrameMessage(frame->logId, LOG_NORMAL, 
	"trying to load hook plugin \"%s\"", usedDriverPlugin);
    plugError = phPlugLoadHook(frame->logId, usedDriverPlugin, 
	&frame->hookAvail);
    if (plugError != PHPLUG_ERR_OK && 
	plugError != PHPLUG_ERR_DIFFER &&
	plugError != PHPLUG_ERR_NEXIST)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not load handler specific hook plugin, "
	    "giving up (error %d)", 
	    (int) plugError);
	return frame;
    }

    /* only go on with hooks if existent */
    if (plugError == PHPLUG_ERR_OK || plugError == PHPLUG_ERR_DIFFER)
    {
	if ((frame->hookAvail & PHHOOK_AV_MINIMUM) != PHHOOK_AV_MINIMUM)
	{
	    frame->errors++;
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
		"Minimum functional requirements for hook plugin not met, "
		"giving up");
	    return frame;
	}
    
	/* initialize handler specific hooks */
	hookError = phPlugHookInit(frame->funcId, frame->comId, frame->logId, 
	    frame->specificConfId, frame->stateId, frame->estateId,
	    frame->testerId);
	if (hookError != PHHOOK_ERR_OK)
	{
	    frame->errors++;
	    phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
		"could not initialize handler specific hook plugin, "
		"giving up (error %d)", 
		(int) hookError);
	    return frame;
	}
    }

    /* initialize the event handler */
    eventError = phEventInit(&frame->eventId, frame->funcId, frame->comId, 
	frame->logId, frame->specificConfId, frame->stateId, frame->estateId,
	frame->testerId);
    if (eventError != PHEVENT_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize event handler, "
	    "giving up (error %d)", 
	    (int) funcError);
	return frame;
    }

    /* initialize [ Communication Start ] User Dialog */
    userDialogError = initUserDialog(&frame->dialog_comm_start,
                                     frame->logId,
                                     frame->specificConfId, 
                                     frame->testerId,
                                     frame->eventId,
                                     PHKEY_OP_DIAGCOMMST,
                                     "Communication Start",
                                     "Please press Continue as soon as \n"
                                     "the handler is ready to \n"
                                     "start communicating with the tester.\n"
                                     "Press Quit to end the testprogram.",
                                     ud_freq_once);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize user dialog, "
	    "giving up (error %d)", 
	    (int) userDialogError);
	return frame;
    }

    /* initialize [ Configuration Start ] User Dialog */
    userDialogError = initUserDialog(&frame->dialog_config_start,
                                     frame->logId,
                                     frame->specificConfId, 
                                     frame->testerId,
                                     frame->eventId,
                                     PHKEY_OP_DIAGCONFST,
                                     "Configuration Start",
                                     "Please press Continue as soon as \n"
                                     "the handler is ready to \n"
                                     "receive setup information.\n"
                                     "Press Quit to end the testprogram.",
                                     ud_freq_once);
    if (userDialogError != PHUSERDIALOG_ERR_OK)
    {
	frame->errors++;
	phLogFrameMessage(frame->logId, PHLOG_TYPE_FATAL,
	    "could not initialize user dialog, "
	    "giving up (error %d)", 
	    (int) userDialogError);
	return frame;
    }

    /* find out whether we want to generate an output file that can be
       used for automatic driver test */
    if (getenv(TEST_LOG_ENV_VAR))
    {
        frame->testLogOut = fopen(getenv(TEST_LOG_ENV_VAR), "w");
        if ( frame->testLogOut &&
             fclose(frame->testLogOut)== 0 )
        {
            frame->testLogOut = fopen(getenv(TEST_LOG_ENV_VAR), "a");
        }
        if ( frame->testLogOut )
            frame->createTestLog = 1;
    }

    /* all done, puhhh....... */
    frame->isInitialized = 1;
    phLogFrameMessage(frame->logId, LOG_NORMAL,
	"Handler driver framework initialized\n");

    return frame;
}


/*****************************************************************************
 * End of file
 *****************************************************************************/
