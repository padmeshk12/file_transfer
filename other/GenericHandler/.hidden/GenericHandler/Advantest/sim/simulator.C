/******************************************************************************
 *
 *       (c) Copyright Advantest 2015   
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.C
 * CREATED  : 29 Apri 2014
 *
 * CONTENTS : Advantest-GPIB handler simulator
 *
 * AUTHORS  : Magco Li, STS-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 29 Apri 2014, Magco Li, modified TEL prober simulator
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





/*--- module includes -------------------------------------------------------*/

#include "AdvantestHandlerController.h"
#include "viewController.h"
/* Begin of Huatek Modifications, Charley Cao, 03/08/2002 */
/* Issue Number: 335 */
/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif
/* End of Huatek Modifications */

#define MAX_PATH_LENTH 512
#define MAX_NAME_LENTH 128
#define MAX_CMD_LINE_LENTH 640


void initializeDefaults(AdvantestHandlerControllerSetup   &simSetup,
                        ViewControllerSetup        &vcSetup)
{
    simSetup.model                            =   AdvantestHandlerController::GS1;

    simSetup.handler.numOfSites               =   4;
    simSetup.handler.siteEnabledMask          =   allSitesEnabled;
    simSetup.handler.autoSetupDelay           =   0.0;
    simSetup.handler.numOfDevicesToTest       =   1000;
    simSetup.handler.pattern                  =   Handler::all_sites_working;
    simSetup.handler.handlerReprobeMode       =   Handler::perform_reprobe_separately;
    simSetup.handler.corruptBinDataScheme     =   Handler::none;
 
    simSetup.handler.categories               =   32;
    simSetup.handler.maxVerifyCount           =   2;
    simSetup.handler.reprobeCat               =   -2;
    simSetup.handler.retestCat                =   -1;

    simSetup.verifyTest                       =   true; 
    simSetup.debugLevel                       =   0;
    simSetup.timeout                          =   5000;
    simSetup.eoc                              =   "\r\n";
    simSetup.gpibComm.hpibDevice              =   strdup("hpib2");

    vcSetup.noGui                             =   false;

    simSetup.playbackFile                     =   NULL;
    simSetup.recordFile                       =   NULL;
    simSetup.queryError                       =   false;
    simSetup.retestTime                       =   -1;           //retest model, default -1

}


int arguments(int argc, char *argv[], 
              AdvantestHandlerControllerSetup   &simSetup,
              ViewControllerSetup      &vcSetup)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;

    /* setup default: */

    while ((c = getopt(argc, argv, "A:T:d:i:t:m:s:c:N:p:V:o:P:R:r:n:G")) != -1)
    {
	switch (c) 
	{
#if 0
        case 'G':
            vcSetup.gpioTest = true;
            break;
#endif
        case 'A':
            simSetup.handler.autoSetupDelay =  atof(optarg) * 1000;
            break;
        case 'T':
            simSetup.retestTime = atoi(optarg);
            break;
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
	    if (strcmp(optarg, "GS1") == 0) 
            {
                simSetup.model = AdvantestHandlerController::GS1;
            }
	    else if (strcmp(optarg, "M45") == 0) 
            {
                simSetup.model = AdvantestHandlerController::M45;
            }
	    else if (strcmp(optarg, "M48") == 0) 
            {
                simSetup.model = AdvantestHandlerController::M48;
            }
	    else if(strcmp(optarg,"DLT")==0)  /*DLT enhance*/
	    {
	        simSetup.model = AdvantestHandlerController::DLT;
	    }
        /*Begin CR92686, Adam Huang, 4 Feb 2015*/
	    else if(strcmp(optarg,"Yushan")==0)  /*Yushan enhance*/
	    {
	        simSetup.model = AdvantestHandlerController::Yushan;
	    }
        /*End*/
	    else
            { 
                errflg++;
            }
	    break;
          case 's':
            simSetup.handler.numOfSites = atoi(optarg);
            break;      
          case 'c':
            if (strcmp(optarg, "16") == 0)
            {
                simSetup.handler.categories = 16;
            }
            else if (strcmp(optarg, "32") == 0)
            {
                simSetup.handler.categories = 32;
            }
            else
            {
                errflg++;
            }
            break;
          case 'N':
            simSetup.handler.numOfDevicesToTest = atoi(optarg);
            break;
          case 'p':
            if (strcmp(optarg, "one") == 0)
            {
                simSetup.handler.pattern = Handler::all_one_not_one_is_working;
            }
            else if (strcmp(optarg,"all") == 0)
            {
                simSetup.handler.pattern = Handler::all_sites_working;
            }
            else
            {
                errflg++;
            }
            break;
          case 'V':
            if (strcmp(optarg, "none")==0)
            {
                simSetup.verifyTest = false;
                simSetup.handler.corruptBinDataScheme = Handler::none;
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
                simSetup.handler.corruptBinDataScheme = Handler::every_17th_and_18th_bin_data;
            }
            else
            {
                errflg++;
            }
            break;
          case 'o':
            simSetup.recordFile=strdup(optarg);
	    break;
	  case 'P':
            simSetup.playbackFile=strdup(optarg);
	    break;
          case 'R':
            simSetup.handler.retestCat = atoi(optarg);
            break;
          case 'r':
            if (strcmp(optarg, "none") == 0) simSetup.handler.handlerReprobeMode = Handler::ignore_reprobe;
            else if (strcmp(optarg, "sep") == 0) simSetup.handler.handlerReprobeMode = Handler::perform_reprobe_separately;
            else if (strcmp(optarg, "add") == 0) simSetup.handler.handlerReprobeMode = Handler::add_new_devices_during_reprobe;
            else
            {
                if ( sscanf(optarg,"%d",&(simSetup.handler.reprobeCat) ) != 1 )
                    errflg++;
            }
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
	    fprintf(stderr, " -A <delay>              automatic start after initialization delay [sec]\n");
	    fprintf(stderr, " -d <debug-level>       -1 to 4, default: 0 \n");
            fprintf(stderr, " -t <timeout>           GPIB timeout to be used[msec], default:1000 \n");
	    fprintf(stderr, " -m <prober model>      prober model, one of: \n");
	    fprintf(stderr, "                        GS1, M45, M48, DLT, Yushan default: GS1 \n");
	    fprintf(stderr, " -s <number>            number of sites, default: 4 \n");
            fprintf(stderr, " -N <number>            number of parts to be handled before \n");
            fprintf(stderr, "                        end of test,default:infinite \n"); 
            fprintf(stderr, " -c <binon categries>   depend on the number of device tested in paraller \n");
            fprintf(stderr, "                        one of:16,32 categries default: 32 \n");
            fprintf(stderr, " -i <gpib-interface>    GPIB interface device, default: hpib3 \n");
            fprintf(stderr, " -p <pattern>           site population pattern, one of: \n");
            fprintf(stderr, "                        one(all sites, one not,one is working),\n");
            fprintf(stderr, "                        all(all sites working),default:all \n");
            fprintf(stderr, " -V <verify_mode>       verify test mode, one of : \n");
            fprintf(stderr, "                        none (no verification, normal binning), \n");
	    fprintf(stderr, "                        vok  (devices must be verified but always correct), \n");
	    fprintf(stderr, "                        verr1 (devices must be verified bin data corrupted  \n");
	    fprintf(stderr, "                             for every 17th bin command), \n");
	    fprintf(stderr, "                        verr2 (devices must be verified bin data corrupted  \n");
	    fprintf(stderr, "                              for every 17th and 18th bin command), \n");
            fprintf(stderr, " -o <record file>       record GUI states\n");                          
	    fprintf(stderr, " -P <playback file>     play back GUI states\n"); 
            fprintf(stderr, " -R <number>            retest bin number, default -1 \n");
            fprintf(stderr, " -r <reprobe_mode>      handler reprobe mode, one of: \n");
            fprintf(stderr, "                        none (ignore reprobes and follow <pattern>) \n");
            fprintf(stderr, "                        sep (perform reprobes separately), \n");
            fprintf(stderr, "                        add (add new devices during reprobe), default sep \n");
            fprintf(stderr, " -T <number>            retest model, number is retest times, default -1, means normal model \n");
            //fprintf(stderr, " -G                     thermal alarm can trigger gpio data out signa, default off \n");
           
            fprintf(stderr, " -n                     no GUI \n");
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

    AdvantestHandlerControllerSetup   AdvantestSimSetup;
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
    initializeDefaults(AdvantestSimSetup, vcSetup);

    /* get real operational parameters */
    if (!arguments(argc, argv, AdvantestSimSetup,vcSetup))
	exit(1);

    sleep(1);

    try {
        AdvantestHandlerController AdvantestHandler(&AdvantestSimSetup,&vcSetup);

        AdvantestHandler.switchOn();
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
