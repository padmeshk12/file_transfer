/*
 *-- system includes ------------------------------------------------------
 */

#include <stdio.h>
#include <iostream>
#include <unistd.h> 
#include <sys/stat.h>  
#include <sstream>
#include <vector>
#include <map>
#include <string>
 

/*
 *-- module include -------------------------------------------------------
 */

#include "tpi_c.h"
#include "adiUserI.h"
#include "ci_types.h"
#include "libcicpi.h"

using namespace std; 

/*
 *-- external functions ---------------------------------------------------
 */

/*
 *-- external variables ---------------------------------------------------
 */

/*
 *-- defines --------------------------------------------------------------
 */


/*
 *-- typedefs -------------------------------------------------------------
 */
enum id_source_t {
	SRC_SMT,
	SRC_GUI
};

/*
 *-- globals variables ----------------------------------------------------
 */

/*
 *-- functions ------------------------------------------------------------
 */
int getWaferIDfromGUI(string & waferID)
{
	char fn[] = "getWaferIDfromGUI:";
	char Response[1025] = "";
	char guiCommand[1025] = "/usr/bin/kdialog --inputbox \"Please input the Wafer-ID\" 2>/dev/null";
	FILE *in;

	in = popen (guiCommand,"r");

	if (in == (FILE *)NULL)
	{
		printf("%s: .. ERROR! calling Popen() with kdialog failed!!\n",fn);

		return(1);
	}
	printf("%s:  .. Reading Response from GUI...\n",fn);
	fgets(Response,sizeof(Response),in);

	if(strlen(Response) > 0)
	{
		Response[strlen(Response)-1] = (char)NULL;
		printf("%s:  .. Response = \"%s\"...\n",fn,Response);
		waferID.assign(Response);
		
	}
	
	return(0);
}

/***************************************************************************/
/***************************************************************************/
int main(int argc, char* argv[])
{

	int source=SRC_GUI;
	string waferID("");
	char   wafer_id[257] = "";
	
	if(argc != 2)
	{
		cout << "Missing input parameter!" << endl;
		exit(1);
	}
	else
	{
		
		if(strcmp(argv[1],"SMT") == 0)
			source = SRC_SMT;
		else if(strcmp(argv[1],"GUI") == 0)
			source = SRC_GUI;
		else
		{
			cout << "Unsupported parameter \"" << argv[1] << "\"" << endl;
			exit(1);
		}
		
	}

	/* Connect to the current SmarTest session */
    HpInit();

	if(source == SRC_GUI)
	{
		if(getWaferIDfromGUI(waferID) == 1)
		{

			cout << "ERROR:  WaferID query dialog failed" << endl;
			exit(1);
		}

		strcpy(wafer_id,waferID.c_str());
		SetModelfileString("WAFER_ID",wafer_id);

	
	}
	else
	{
		GetModelfileString("WAFER_ID",wafer_id);
		cout << wafer_id << endl;
	
	}

	/* Disconnect from the current SmarTest session */
    HpTerm();

    exit(CI_CALL_PASS);                /* program completed without error */


}
