/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : simulator.C
 * CREATED  : 15 Feb 2000
 *
 * CONTENTS : Seiko-Epson-GPIB handler simulator
 *
 * AUTHORS  : Chris Joyce, SSTD-R&D, initial revision
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



/*--- module includes -------------------------------------------------------*/

#include "DeltaHController.h"
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

void initializeDefaults(DeltaHandlerControllerSetup &simSetup,
                        deltaModel                  &model)
{
    model                                     =   Castle;

    simSetup.releasepartsWhenReady            =   false;

    simSetup.handler.numOfSites               =   4;
    simSetup.handler.minimumBinNumber         =   0;
    simSetup.handler.maximumBinNumber         =   100;
    simSetup.handler.autoSetupDelay           =   0.0;
    simSetup.handler.handlingDelay            =   1000;
    simSetup.handler.numOfDevicesToTest       =   1000;
    simSetup.handler.handlerReprobeMode       =   Handler::perform_reprobe_separately;
    simSetup.handler.maxVerifyCount           =   Handler::no_max_verify_count;
    simSetup.handler.corruptBinDataScheme     =   Handler::none;
    simSetup.retestTime                       =   -1;   //retest model, default -1
}

int arguments(int argc, char *argv[], 
              DeltaHandlerControllerSetup   &simSetup,
              deltaModel                    &model)
{
    int c;
    int errflg = 0;
    extern char *optarg;
    extern int optopt;
  
    /* setup default: */
    while ((c = getopt(argc, argv, "A:d:R:i:t:m:s:M:p:c:r:V:D:N:So:P:T:S:Qxn")) != -1)
    {
	switch (c) 
	{
      case 'c':
        if (strcmp(optarg, "16") == 0)
        {
            simSetup.handler.categories = 16;
        }
        else if (strcmp(optarg, "256") == 0)
        {
            simSetup.handler.categories = 256;
        }
        else
        {
            errflg++;
        }
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
	    if (strcmp(optarg, "Castle") == 0) 
            {
                model = Castle;
            }
	    else if (strcmp(optarg, "Matrix") == 0) 
            {
                model = Matrix;
            }
	    else if (strcmp(optarg, "Flex") == 0) 
            {
                model = Flex;
            }
	    else if (strcmp(optarg, "Rfs") == 0) 
            {
                model = Rfs;
            }
	    else if (strcmp(optarg, "Summit") == 0) 
            {
                model = Summit;
            }
	    else if (strcmp(optarg, "Orion") == 0)
	    {
	        model = Orion;
	    }
	    else if (strcmp(optarg, "Eclipse") == 0)
	    {
	        model = Eclipse;
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
	  case 'r':
	    if (strcmp(optarg, "none") == 0)
            {
                simSetup.handler.handlerReprobeMode = Handler::ignore_reprobe;
            }
	    else if (strcmp(optarg, "sep") == 0)
            {
                simSetup.handler.handlerReprobeMode = Handler::perform_reprobe_separately;
            }
	    else if (strcmp(optarg, "add") == 0)
            {
                simSetup.handler.handlerReprobeMode = Handler::add_new_devices_during_reprobe;
            }
	    else 
                errflg++;
	    break;
	  case 'S':
            simSetup.multipleSrqs = true;
	    break;
	  case 'V':
	    if (strcmp(optarg, "none") == 0)
            {
                simSetup.verifyTest = false;
                simSetup.handler.corruptBinDataScheme = Handler::none;
                simSetup.releasepartsWhenReady = false;
            }
	    else if (strcmp(optarg, "vok") == 0)
            {
                simSetup.verifyTest = true;
                simSetup.handler.corruptBinDataScheme = Handler::none;
            }
	    else if (strcmp(optarg, "verr") == 0)
            {
                simSetup.verifyTest = true;
                simSetup.handler.corruptBinDataScheme = Handler::every_11th_and_15th_bin_data;
            }
	    else if (strcmp(optarg, "vrp") == 0)
            {
                simSetup.verifyTest = true;
                simSetup.releasepartsWhenReady = false;
            }
	    else if (strcmp(optarg, "vrpi") == 0)
            {
                simSetup.verifyTest = true;
                simSetup.releasepartsWhenReady = true;
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
            simSetup.noGui = true;
	    break;
	  case 'x':
            simSetup.corruptMessage = true;
	    break;
	  case 'R':
	    simSetup.retestTime = atoi(optarg);
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
            fprintf(stderr, "                         Castle, Matrix, Flex, Rfs, Summit, Orion, Eclipse, default: Castle\n");
            fprintf(stderr, " -s <number>             number of sites, default: 4\n");
            fprintf(stderr, " -M <number>             site enabled mask, default all sites enabled \n");
            fprintf(stderr, " -p <pattern>            site population pattern, one of: \n");
            fprintf(stderr, "                         one (all sites, one not, one is working), \n");
            fprintf(stderr, "                         all (all sites working), default: all\n");
            fprintf(stderr, " -r <reprobe_mode>       handler reprobe mode, one of: \n");
            fprintf(stderr, "                         none (ignore reprobes and follow <pattern>) \n");
            fprintf(stderr, "                         sep (perform reprobes separately), \n");
            fprintf(stderr, "                         add (add new devices during reprobe), default sep \n");
            fprintf(stderr, " -D <delay>              parts handling delay [sec], default: 1\n");
            fprintf(stderr, " -N <number>             number of parts to be handled before \n");
            fprintf(stderr, "                         end of test, default: infinite\n");
            fprintf(stderr, " -S                      send multiple SRQs for site population\n");
            fprintf(stderr, " -V <verify_mode>        verify test mode, one of: \n");
            fprintf(stderr, "                         none (no verification, normal binning), \n");
            fprintf(stderr, "                         vok  (devices must be verified but always correct), \n");
            fprintf(stderr, "                         verr (devices must be verified bin data corrupted  \n");
            fprintf(stderr, "                               for every 11th and then 15th set bin command), \n");
            fprintf(stderr, "                         vrp  (releaseparts command accepted: each populated  \n");
            fprintf(stderr, "                               site with bin information is released), \n");
            fprintf(stderr, "                         vrpi (releaseparts command is ignored if all populated \n");
            fprintf(stderr, "                               sites have not received bin information), \n");
            fprintf(stderr, "                               default: none (when verification set: vok vrp) \n");
            fprintf(stderr, " -Q                      error in sending replies to queries (status?, srqbyte? \n");
            fprintf(stderr, "                         testpartsready? ) every 7th such query will result in  \n");
            fprintf(stderr, "                         either a \" \\n\" being sent instead of the correct response \n");
            fprintf(stderr, "                         or no response sent at all (forcing a timeout).  \n");
            
            fprintf(stderr, " -x                      corrupt every 1st and 11th testpartsready|srqbyte query\n");

            fprintf(stderr, " -i <gpib-interface>     GPIB interface device, default: hpib2 \n");
            fprintf(stderr, " -t <timeout>            GPIB timeout to be used [msec], default: 5000\n");

            fprintf(stderr, " -T <term string>        terminate message string, one of:\n");
            fprintf(stderr, "                         1 - \\r\\n, 2 - \\n, default: \\n \n");

            fprintf(stderr, " -o <record file>        record GUI states\n");
            fprintf(stderr, " -P <playback file>      play back GUI states\n");

            fprintf(stderr, " -n                      no GUI\n");

            fprintf(stderr, " -c <binon categries>   depend on the number of device tested in paraller \n");
            fprintf(stderr, "                        one of:16,256 categries default: 16 \n");
            fprintf(stderr, " -R <number>            retest model, number is retest times, default -1, means normal model \n");

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

    DeltaHandlerControllerSetup   simSetup;
    deltaModel                    model;
    DeltaHandlerController        *hController = NULL;

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
    initializeDefaults(simSetup, model);

    /* get real operational parameters */
    if (!arguments(argc, argv, simSetup, model))
	exit(1);

    sleep(1);

    try {
        switch (model)
        {
          case Castle:
            hController = new CastleHandlerController(&simSetup);
            break;
          case Matrix:
            hController = new MatrixHandlerController(&simSetup);
            break;
          case Flex:
            hController = new FlexHandlerController(&simSetup);
            break;
          case Rfs:
            hController = new RfsHandlerController(&simSetup);
            break;
          case Summit:
            hController = new SummitHandlerController(&simSetup);
            break;
          case Orion:
            hController = new OrionHandlerController(&simSetup);
            break;
          case Eclipse:
            hController = new EclipseHandlerController(&simSetup);
            break;
        }

        hController->switchOn(model);
        delete hController;
    }
    catch ( Exception &e )
    {
        Std::cerr << "FATAL ERROR: ";
        e.printMessage();
    } 
 
//    delete hController;
    
    return 0;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/
