/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.C
 * CREATED  : 5 Feb 2015
 *
 * CONTENTS : MCT GPIB handler simulator
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 5 Jan 2015, Magco Li, created
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

#include "MCTHandlerController.h"
#include "viewController.h"

#define MAX_PATH_LENTH 512
#define MAX_NAME_LENTH 128
#define MAX_CMD_LINE_LENTH 640


void initializeDefaults(MCTHandlerControllerSetup   &simSetup,
                        ViewControllerSetup           &vcSetup)
{
    simSetup.model                            =   MCTHandlerController::FH1200;

    simSetup.handler.numOfSites               =   4;
    simSetup.handler.siteEnabledMask          =   allSitesEnabled;
    simSetup.handler.autoSetupDelay           =   0.0;
    simSetup.handler.handlingDelay            =   1000;
    simSetup.handler.numOfDevicesToTest       =   1000;
    simSetup.handler.pattern                  =   Handler::all_sites_working;
    simSetup.handler.handlerReprobeMode       =   Handler::ignore_reprobe;
    simSetup.handler.retestCat                =   -1;
    simSetup.handler.reprobeCat               =   -2;
    simSetup.handler.maxVerifyCount           =   2;
    simSetup.handler.corruptBinDataScheme     =   Handler::none;

    simSetup.debugLevel                       =   0;
    simSetup.timeout                          =   1000;
    simSetup.eoc                              =   "\r\n";
    simSetup.queryError                       =   false;
    simSetup.recordFile                       =   NULL;
    simSetup.playbackFile                     =   NULL;

    simSetup.gpibComm.hpibDevice              =   strdup("hpib2");

    vcSetup.noGui                             =   false;
}


int arguments(int argc, char *argv[],
              MCTHandlerControllerSetup   &simSetup,
              ViewControllerSetup           &vcSetup)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;

    /* setup default: */

    while ((c = getopt(argc, argv, ":d:i:t:m:s:M:p:R:r:D:A:N:o:P:T:Qn")) != -1)
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
        if (strcmp(optarg, "FH1200") == 0)
            {
                simSetup.model = MCTHandlerController::FH1200;
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
        fprintf(stderr, "                         FH1200 default: FH1200\n");
        fprintf(stderr, " -s <number>             number of sites, default: 4\n");
        fprintf(stderr, " -M <number>             site enabled mask, default all sites enabled \n");
        fprintf(stderr, " -p <pattern>            site population pattern, one of: \n");
        fprintf(stderr, "                         one (all sites, one not, one is working), \n");
        fprintf(stderr, "                         all (all sites working), default: all\n");
        fprintf(stderr, " -R <number>             retest bin number, default -1 \n");
        fprintf(stderr, " -r <reprobe_mode>       handler reprobe mode, one of: \n");
        fprintf(stderr, "                         none (ignore reprobes and follow <pattern>) \n");
        fprintf(stderr, "                         sep (perform reprobes separately), \n");
        fprintf(stderr, "                         add (add new devices during reprobe), default sep \n");
        fprintf(stderr, "                         <number> reprobe bin number, default -2 \n");
        fprintf(stderr, " -D <delay>              parts handling delay [sec], default: 1\n");
        fprintf(stderr, " -A <delay>              automatic start after initialization delay [sec]\n");
        fprintf(stderr, "                         default: requires a manual start \n");
        fprintf(stderr, " -N <number>             number of parts to be handled before \n");
        fprintf(stderr, "                         end of test, default: infinite\n");
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

    MCTHandlerControllerSetup   TSimSetup;
    ViewControllerSetup           vcSetup;
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
    initializeDefaults(TSimSetup, vcSetup);

    /* get real operational parameters */
    if (!arguments(argc, argv, TSimSetup,vcSetup))
    exit(1);

    sleep(1);

    try {
        MCTHandlerController MCTHandler(&TSimSetup,&vcSetup);

        MCTHandler.switchOn();
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
