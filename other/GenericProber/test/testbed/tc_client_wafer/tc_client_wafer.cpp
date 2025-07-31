/**
 * Copyright by Advantest, 2022
 */
 
#include "xoc/tcapi/tcapi.hpp"
#include <vector>
#include <boost/lexical_cast.hpp>
 
using namespace xoc::tcapi;
using namespace std;
 
// Clean up sessions
void cleanup()
{
  // Shut down all SmarTest sessions
  vector<TesterSession *> allSessions = TestCell::getInstance().getTesterSessions();
  for(unsigned int i = 0; i < allSessions.size(); i++ )
  {
    allSessions[i]->stop();
  }
 
  // Shut down all PH sessions
  vector<PHSession *> allPHSessions = TestCell::getInstance().getPHSessions();
  for(unsigned int i = 0; i < allPHSessions.size(); i++ )
  {
    allPHSessions[i]->stop();
  }
}
 
int main(int argc, char* argv[])
{
  if (argc != 5)
  {
    cerr << "usage: " << argv[0] << " workspace test_program ph_driver_path ph_driver_config_file" << endl;
    return 1;
  }
 
  try
  {
    // Shut down running sessions
    cleanup();
 
    // Argument parsing
    string workspace = argv[1];
    string tpPath = argv[2];
    string driverPath = argv[3];
    string configPath = argv[4];
 
    // Get the TestCell instance
    TestCell &tc = TestCell::getInstance();
 
    // Start a tester session
    TesterSession &ts = tc.newTesterSession();
    ts.setModelFile(workspace+"/sampleproject/tester.model");
    ts.start().setWorkspace(workspace);
 
    // Activate and bind the test program
    TestProgram &tp = ts.testProgram();
    tp.activate(tpPath).load().bind();
 
    // Set the SYS.LOT_TYPE which is used by
    // the data log formatter and also TCCT GUI
    tp.setTPVariable("SYS.LOT_TYPE", "WAFER_TEST");
 
    // Define the wafer map variables. These variables will
    // be used by the TCCT to draw the wafer map
    tp.setTCVariable("MIN_X", 0);
    tp.setTCVariable("MIN_Y", 0);
    tp.setTCVariable("MAX_X", 20);
    tp.setTCVariable("MAX_Y", 20);
    tp.setTCVariable("QUADRANT", 4);
    tp.setTCVariable("ORIENTATION", 356);
 
    // Start a ph session
    PHSession &phSession = tc.newPHSession();
    phSession.setDriverPath(driverPath)
             .setConfigPath(configPath).start().connect();
 
    // Lot control
    if(phSession.loadLot())
    {
      // Set the lot ID
      string lotId = string("lot_") + phSession.getIdentification("lot");
      tp.setTPVariable("STDF.LOT_ID", lotId);
      string perLotPrefix = string(getenv("HOME")) + "/" + lotId;
 
      // Start the test program for each lot
      tp.run();
 
      // Wafer control
      while(phSession.loadWafer())
      {
        // Set the wafer ID
        string waferId = lotId + string("_wafer_") + phSession.getIdentification("wafer");
        tp.setTPVariable("STDF.WAFER_ID", waferId);
        string perWaferPrefix = string(getenv("HOME")) + "/" + waferId;
 
        // Die control
        int device_count = 1;
        cout << "++++++++++++++++++++++++++++++++++++++" << endl;
        while(phSession.loadDevice())
        {
          cout << "test device start " << device_count << " ..." << endl;
          // Run the test flow
          TestResults results = tp.testflow().setEnabledSites(phSession.getEnabledSites()).execute();
 
          // Bin the device
          phSession.setTestResults(results).binDevice();
          cout << "test device done." << endl;
          device_count++;
        }

        cout << "++++++++++++++++++++++++++++++++++++++" << endl;
 
        // Unload the wafer
        phSession.unloadWafer();
 
      }
 
      // Unload the lot
      phSession.unloadLot();
 
 
      // Stop the test program at the end of each lot
      tp.stop();
 
    }
 
    // Disconnect the PH session from the prober
    phSession.disconnect();
  }
  catch(TCException &exc)
  {
    cout << "***********EXCEPTION******************" << endl;
    cout << "message: " << exc.message << endl;
    cout << "origin:  " << exc.origin << endl;
    cout << "type:    " << exc.typeString << endl;
    cout << "**************************************" << endl;
    return 1;
  }
 
  // Clean up at the end
  cleanup();
  return 0;
}
 
