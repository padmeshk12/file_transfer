/**
 * Copyright by Advantest, 2023
 */
 
#include "xoc/tcapi/tcapi.hpp"
#include <iostream>
#include <vector>
#include <stdio.h>
 
using namespace xoc::tcapi;
using namespace std;

#define RECIPE_FILE "../recipe/default_test.xml"
 
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

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    cerr << "usage: " << argv[0] << " workspace recipe_path" << endl;
    return 1;
  }

  try{
    // Shut down running sessions
    cleanup();

    string workspace = argv[1];
    string recipePath = argv[2];

    // Get the TestCell instance
    TestCell &tc = TestCell::getInstance();
    
    // Get the model file
    string model_file = getenv("WORKSPACE");
    model_file += "/ph-development/phcontrol/drivers/Generic_93K_Driver/GenericHandler/test/testbed/device/sampleproject/model";

    // Start a tester session
    TesterSession &ts = tc.newTesterSession().setModelFile(model_file);
    ts.start().setWorkspace(workspace);

    // Get the RecipeManager instance
    RecipeManager &manager = RecipeManager::getInstance();
    
    // Create a new Recipe instance
    Recipe &recipe = manager.newRecipe(recipePath);

    // Attach to a running SmarTest session
    recipe.attachToTesterSession(ts);
    
    // Start the recipe execution
    recipe.start();

  } catch(TCException &exc) {
    cout << "***********EXCEPTION******************" << endl;
    cout << "message: " << exc.message << endl;
    cout << "origin:  " << exc.origin << endl;
    cout << "type:    " << exc.typeString << endl;
    cout << "**************************************" << endl;
    return 1;
  }

  return 0;
}
