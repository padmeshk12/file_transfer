/* *********************************************************************************
 * NOTICE: PLEASE READ CAREFULLY: ADVANTEST PROVIDES THIS SOFTWARE TO YOU ONLY UPON 
 * YOUR ACCEPTANCE OF ADVANTEST'S TERMS OF USE. THE SOFTWARE IS PROVIDED "AS IS" 
 * WITHOUT WARRANTY OF ANY KIND AND ADVANTEST'S LIABILITY FOR SUCH SOFTWARE LIMITED 
 * TO THE FULLEST EXTENT PERMITTED UNDER THE LAW.
 ************************************************************************************
 */

//////////////////////////////////////////////////////////////////////////////////////////////
///
///   HVM API application that loads the wafer description file
///  
///
///////////////////////////////////////////////////////////////////////////////////////////// 


#include <iostream>
#include <unistd.h> 
#include <sys/stat.h>  
#include <sstream>
#include <vector>
#include <map> 
 
#include "xoc/hvmapi/hvmapi.hpp"
#include "xoc/hvmapi/Status.hpp"
#include "xoc/hvmapi/ParamList.hpp" 
 
using namespace std; 
using namespace xoc::hvmapi;
 
///////////////////////////////////////////////////////////////////// 
/// Desc:        This function displays HVM API error code and text
/// Parameters:  HVM API status object
///
/// Return:      Nothing
///
/// Change Log:
/// P.Gauthier  January 6 2015 Initial version 
/////////////////////////////////////////////////////////////////////

void handleError(xoc::hvmapi::Status & errObj){ 
    
   cout << "ERROR: " << errObj.code() << " (" << errObj.text() << ")" << endl; 
   exit(1);
    
} 
 

///////////////////////////////////////////////////////////////////// 
/// Desc:        Main: loads a wafer recipe file
///
/// Parameters:  Not used
///
/// Return:      0 if no error 
///
/// Change Log:
/// P.Gauthier  January 6 2015 Initial version 
/////////////////////////////////////////////////////////////////////
 
int main(int argc, char **argv) {  
  
   xoc::hvmapi::ParamList cmdParams;
   xoc::hvmapi::ParamList outputParams;
   xoc::hvmapi::Status errObj;
   
   if(argc != 2)
   {
       cerr << "  *************************************************" << endl;
       cerr << "  * ERROR! missing Wafer Description File name.   *" << endl;
	   cerr << "  *************************************************" << endl;
	   return 1;
   }
   
   cmdParams.clear();
   cmdParams["TEST_TYPE"] = "WAFER_SORT";
   cmdParams["ENABLE_IMPLICIT_EVENT"] = "FALSE";
   errObj  = syncCommand("SetBLCProfile", cmdParams, LOCAL_SESSION_ID );

   if ( !errObj.isSuccess() ) {  
      handleError(errObj);
   } 
 
   cmdParams.clear();
   cmdParams["WAFER_DESCRIPTION_FILE"] = argv[1];
   errObj  = syncCommand("EvalWaferDescriptionFile", cmdParams, LOCAL_SESSION_ID);

   if ( !errObj.isSuccess() ) {  
      handleError(errObj);
   } 
 
   cmdParams.clear();
   cmdParams["TEST_TYPE"] = "WAFER_SORT";
   cmdParams["ENABLE_IMPLICIT_EVENT"] = "FALSE";
   errObj  = syncCommand("ResetBLCProfile", cmdParams, LOCAL_SESSION_ID);

   if ( !errObj.isSuccess() ) {  
      handleError(errObj);
   }  
   
   return 0;
  
}


