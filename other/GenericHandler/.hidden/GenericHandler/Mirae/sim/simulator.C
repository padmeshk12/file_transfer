/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.C
 * CREATED  : 15 Feb 2000
 *
 * CONTENTS : Mirae GPIB handler simulator
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
 *            Ken Ward, BitsoftSystems, Mirae revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 22 May 2000, Chris Joyce, modified TEL prober simulator
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
#endif

#include <cstring>

#define MAX_PATH_LENTH 512
#define MAX_NAME_LENTH 128
#define MAX_CMD_LINE_LENTH 640


/*--- module includes -------------------------------------------------------*/

#include "MiraehandlerController.h"
#include "viewController.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */



void initializeDefaults(MiraeHandlerControllerSetup   &simSetup,
                        ViewControllerSetup        &vcSetup)
{
    simSetup.model                            =   MiraeHandlerController::MR5800;

    simSetup.handler.numOfSites               =   4;
    simSetup.handler.siteEnabledMask          =   allSitesEnabled;
    simSetup.handler.autoSetupDelay           =   0.0;
    simSetup.handler.handlingDelay            =   1000;
    simSetup.handler.numOfDevicesToTest       =   1000;
    simSetup.handler.pattern                  =   Handler::all_sites_working;
    simSetup.handler.handlerReprobeMode       =   Handler::ignore_reprobe;
    simSetup.handler.maxVerifyCount           =   2;
    simSetup.handler.corruptBinDataScheme     =   Handler::none;

    simSetup.verifyTest                       =   true;
    simSetup.multipleSrqs                     =   false;

    simSetup.debugLevel                       =   0;
    simSetup.timeout                          =   5000;
    simSetup.eoc                              =   "\n";
    simSetup.queryError                       =   false;
    simSetup.recordFile                       =   NULL;
    simSetup.playbackFile                     =   NULL;
    
    simSetup.gpibComm.hpibDevice              =   strdup("hpib2");

    vcSetup.noGui                             =   false;
}


int arguments(int argc, char *argv[],
              MiraeHandlerControllerSetup   &simSetup,
              ViewControllerSetup      &vcSetup)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;

    /* setup default: */

    while ((c = getopt(argc, argv, ":d:i:t:m:s:M:p:V:D:A:N:ISo:P:T:S:QZn")) != -1)
    {
	switch (c) 
	{
	  case 'd':
	    simSetup.debugLevel = atoi(optarg);
	    break;
	  case 'i':
            simSetup.gpibComm.hpibDevice = strdup(optarg);
	    break;
	  case 't':
	    simSetup.timeout = atoi(optarg);
	    break;

	  case 'm':
      if (strcmp(optarg, "MR5800") == 0)
      {
        simSetup.model = MiraeHandlerController::MR5800;
      }
      else if (strcmp(optarg, "M660") == 0 )
      {
        simSetup.model = MiraeHandlerController::M660;
      }
      else if (strcmp(optarg, "M330") == 0)
      {
        simSetup.model = MiraeHandlerController::M330;
      }
	    else 
                errflg++;
	    break;
          case 's':
            simSetup.handler.numOfSites =  atoi(optarg);
            break;
	  case 'M':
            simSetup.handler.siteEnabledMask = atol(optarg);
	    break;
	  case 'p':
	    if (strcmp(optarg, "one") == 0) 
            {
                simSetup.handler.pattern = Handler::all_one_not_one_is_working;
            }
	    else if (strcmp(optarg, "all") == 0) 
            {
                simSetup.handler.pattern = Handler::all_sites_working;
            }
	    else errflg++;
	    break;
	  case 'S':
            simSetup.multipleSrqs = true;
	    break;
	  case 'V':
	    if (strcmp(optarg, "none") == 0)
            {
                simSetup.verifyTest = false;
                simSetup.handler.corruptBinDataScheme = Handler::none;
//                all->releasepartsWhenReady = false;
            }
	    else if (strcmp(optarg, "vok") == 0)
            {
                simSetup.verifyTest = true;
                simSetup.handler.corruptBinDataScheme = Handler::none;
            }
	    else if (strcmp(optarg, "verr1") == 0)
            {
                simSetup.verifyTest = true;
                simSetup.handler.corruptBinDataScheme = Handler::every_17th_bin_data;
            }
	    else if (strcmp(optarg, "verr2") == 0)
            {
                simSetup.verifyTest = true;
                simSetup.handler.corruptBinDataScheme   =   Handler::every_17th_and_18th_bin_data;
            }
	    else 
                errflg++;
	    break;
	  case 'Q':
            simSetup.queryError = true;
	    break;
	  case 'A':
            simSetup.handler.autoSetupDelay =  atof(optarg) * 1000;
	    break;
	  case 'D':
            simSetup.handler.handlingDelay = atof(optarg) * 1000;
	    break;
	  case 'N':
            simSetup.handler.numOfDevicesToTest = atoi(optarg);
	    break;

	  case 'I':
//	    all->interactiveDelay = 1;
	    break;
	  case 'o':
            simSetup.recordFile=strdup(optarg);
	    break;

	  case 'P':
            simSetup.playbackFile=strdup(optarg);
	    break;

	  case 'T':
            if ( strcmp(optarg, "1") == 0 )
            {
                simSetup.eoc = "\r\n";
            }
            else if ( strcmp(optarg, "2") == 0 )
            {
                simSetup.eoc = "\n";
            }
            else errflg++;
            break;
	  case 'Z':
//
//          all->autoStartTime = atoi(optarg);
//
	    break;
	  case 'n':
            vcSetup.noGui = true;
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
	    fprintf(stderr, " -d <debug-level>       -1 to 4, default: 0\n");

	    fprintf(stderr, " -m <prober model>       prober model, one of:\n");
	    fprintf(stderr, "                         MR5800|M660|M330 default: MR5800\n");
	    fprintf(stderr, " -s <number>             number of sites, default: 4, max site number: 32\n");
	    fprintf(stderr, " -M <number>             site enabled mask, default all sites enabled \n");
	    fprintf(stderr, " -p <pattern>            site population pattern, one of: \n");
	    fprintf(stderr, "                         one (all sites, one not, one is working), \n");
	    fprintf(stderr, "                         all (all sites working), default: all\n");
	    fprintf(stderr, " -D <delay>              parts handling delay [sec], default: 1\n");
	    fprintf(stderr, " -A <delay>              automatic start after initialization delay [sec]\n");
	    fprintf(stderr, "                         default: requires a manual start \n");
	    fprintf(stderr, " -N <number>             number of parts to be handled before \n");
	    fprintf(stderr, "                         end of test, default: infinite\n");
	    fprintf(stderr, " -S                      send multiple SRQs for site population\n");
	    fprintf(stderr, " -V <verify_mode>        verify test mode, one of: \n");
	    fprintf(stderr, "                         none (no verification, normal binning), \n");
	    fprintf(stderr, "                         vok  (devices must be verified but always correct), \n");
	    fprintf(stderr, "                         verr1 (devices must be verified bin data corrupted  \n");
	    fprintf(stderr, "                               for every 17th bin command), \n");
	    fprintf(stderr, "                         verr2 (devices must be verified bin data corrupted  \n");
	    fprintf(stderr, "                               for every 17th and 18th bin command), \n");
	    fprintf(stderr, " -Q                      error in sending replies to queries (fullsites?) \n");
	    fprintf(stderr, "                         every 7th such query will result in  \n");
	    fprintf(stderr, "                         a \" \\n\" being sent instead of the correct response \n");

	    fprintf(stderr, " -i <gpib-interface>     GPIB interface device, default: hpib2 \n");
	    fprintf(stderr, " -t <timeout>            GPIB timeout to be used [msec], default: 1000\n");

            fprintf(stderr, " -T <term string>        terminate message string, one of:\n");
            fprintf(stderr, "                         1 - \\r\\n, 2 - \\n, default: \\n \n");

	    fprintf(stderr, " -o <record file>        record GUI states\n");                          
	    fprintf(stderr, " -P <playback file>      play back GUI states\n");                        

	    fprintf(stderr, " -n                      no GUI\n");

	    return 0;
	}
    }
    return 1;
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

    MiraeHandlerControllerSetup   MiraeSimSetup;
    ViewControllerSetup        vcSetup;
    int i;

    /* Disable the generation of the HVM alarm */
    setenv("DISABLE_HVM_ALARM_GENERATION_FOR_TCI", "1", 1);

    /* Output command line arguments */
    Std::cout << "Simulator started: ";
    for  ( i=0 ; i < argc ; i++ )
    {
        Std::cout << argv[i] << " ";
    } 
    Std::cout << '\n';
    Std::cout.flush();

    /* init defaults */
    initializeDefaults(MiraeSimSetup, vcSetup);

    /* get real operational parameters */
    if (!arguments(argc, argv, MiraeSimSetup,vcSetup))
	exit(1);

    sleep(1);
    
    try {
        MiraeHandlerController Miraehandler(&MiraeSimSetup,&vcSetup);

        Miraehandler.switchOn();
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
    } 
    
    return 0;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
