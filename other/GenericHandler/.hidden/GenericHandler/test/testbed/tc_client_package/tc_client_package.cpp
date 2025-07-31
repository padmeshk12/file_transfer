/**
 * Copyright by Advantest, 2022
 */
 
#include "xoc/tcapi/tcapi.hpp"
#include <vector>
#include <boost/lexical_cast.hpp>
 
using namespace xoc::tcapi;
using namespace std;

#define MODEL_FILE "../device/sampleproject/model"
 
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
 
    // Get the model file
    string model_file = getenv("WORKSPACE");
    model_file += "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler/test/testbed/device/sampleproject/model";

    // Start a tester session
    TesterSession &ts = tc.newTesterSession().setModelFile(model_file);
    ts.start().setWorkspace(workspace);
 
    // Activate and bind the test program
    TestProgram &tp = ts.testProgram();
    tp.activate(tpPath).load().bind();
 
    // Set the SYS.LOT_TYPE which is used by
    // the data log formatter and also TCCT GUI
    tp.setTPVariable("SYS.LOT_TYPE", "PACKAGE_TEST");
 
    // Start the PHControl session and connect the equipment
    PHSession &phSession = tc.newPHSession();
    phSession.setDriverPath(driverPath)
             .setConfigPath(configPath).start().connect();
 
    // Lot control
    int lot_count = 2;
    while(lot_count && phSession.loadLot())
    {
      // Get the lot ID from the PH session
      string lotId = string("lot_") + phSession.getIdentification("lot");

      // Set the lot ID for data logging purpose
      tp.setTPVariable("STDF.LOT_ID", lotId);
 
      // Initiate the part ID for the current lot
      long currentPartId = 1;

      // Start the test program for each lot
      tp.run();

      int device_count = 1;
      cout << "++++++++++++++++++++++++++++++++++++++" << endl;
 
      // Control device
      while(device_count <= 5 && phSession.loadDevice())
      {
        cout << "test device start " << device_count << " ..." << endl;

        MultiSiteString partId;

        // Get the enabled sites from the PH session
        vector<long> enabledSites = phSession.getEnabledSites();
 
        // Make the part ID string
        for(unsigned int i = 0; i < enabledSites.size(); i++)
        {
          partId.set(enabledSites[i], boost::lexical_cast<string>(currentPartId ++));
        }
 
        // Set the part ID
        tp.setTPVariable("STDF.PART_ID", partId);
 
        // Execute the test flow
        TestResults results = tp.testflow().setEnabledSites(enabledSites).execute();

        // Send the test result to the PH session and bin the devices
        phSession.setTestResults(results).binDevice();

        cout << "test device done." << endl;
        device_count++;
      }

      cout << "++++++++++++++++++++++++++++++++++++++" << endl;

      // Unload the wafer
      phSession.unloadLot();
      lot_count--;
 
      // Stop the test program
      tp.stop();
    }
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
 
