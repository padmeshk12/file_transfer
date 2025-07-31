/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_contacttest.c
 * CREATED  : 14 Oct 2005
 *
 * CONTENTS : contact test actions
 *
 * AUTHORS  : EDO & kun xiao @ Shangai-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 14 Oct 2005
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
#include "ph_pfunc.h"
#include "ph_phook.h"
#include "ph_tcom.h"
#include "ph_event.h"
#include "ph_frame.h"

#include "ph_timer.h"
#include "ph_frame_private.h"
#include "helpers.h"
#include "exception.h"

#include "ph_contacttest_private.h"

/* NOTE: dmalloc.h should always be the last file in the list of files to be included! */
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- global variables ----------------------------------------------------*/
static phConfId_t sConfId = NULL;

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

/********************************************************************
 *
 * Method:      GetSucc
 *
 * Description: Get next item of a linked list, if any
 *
 ********************************************************************/
BULinkedItem* BULinkedItem_GetSucc(const BULinkedItem* theItem)
{
    if ( theItem )
    {
        return(theItem->next);
    }
    return(0);
}

/********************************************************************
 *
 * Method:	CreateOnHeap
 *
 * Description:	Create an item of a linked list on the heap
 *
 ********************************************************************/
BULinkedItem* BULinkedItem_CreateOnHeap(void)
{
    BULinkedItem* ret = (BULinkedItem*)malloc(sizeof(BULinkedItem));

    if ( ret )
    {
        ret->data = 0;
        ret->next = 0;
    }

    return(ret);
}

/********************************************************************
 *
 * Method:      ClearAndDestroy
 *
 * Description: Remove this linked list item and its owned data from
 *              the heap
 *
 ********************************************************************/
void BULinkedItem_ClearAndDestroy(BULinkedItem** theItem,
                                     void (*Free)(void**))
{
    if ( theItem )
    {
        /* destroy enclosed data with given function, if at all */
        if ( Free )
        { (*Free)((void**)(&((*theItem)->data))); }

        /* free own memory */
        free(*theItem); (*theItem) = 0;
    }
}

/********************************************************************
 *
 * Method:	ClearAndDestroy
 *
 * Description:	Remove this linked list and its owned data from
 *              the heap
 *
 ********************************************************************/
void BULinkedList_ClearAndDestroy(BULinkedList** theList, void (*Free)(void**))
{
    if ( theList && *theList )
    {
        BULinkedItem* item = (*theList)->first;
        (*theList)->first = 0;
        while ( item )
        {
            /* get next child before it is too late */
            BULinkedItem* nextItem = BULinkedItem_GetSucc(item);

            /* destroy current child node */
            BULinkedItem_ClearAndDestroy(&item, Free);

            /* switch to next child node */
            item = nextItem;
        }

        /* free own memory */
        free(*theList); (*theList) = 0;
    }
}


/********************************************************************
 *
 * Method:	CreateOnHeap
 *
 * Description:	Create a new empty linked list on the heap
 *
 ********************************************************************/
BULinkedList* BULinkedList_CreateOnHeap(void)
{
    BULinkedList* ret = (BULinkedList*)malloc(sizeof(BULinkedList));

    if ( ret )
    {
        ret->first = 0;
        ret->last  = 0;
    }

    return(ret);
}

/********************************************************************
 *
 * Method:	Append
 *
 * Description:	Append a new item containing the given user data to
 *              this linked list
 *
 ********************************************************************/
void BULinkedList_Append(BULinkedList* theList, const void* theData)
{
    /* create enclosing BULinkedItem */
    BULinkedItem* newItem = BULinkedItem_CreateOnHeap();

    newItem->data = theData;

    /* chain new item to end of existing items */
    if ( theList->last )
    { theList->last->next = newItem; }
    else        /* no items yet in list */
    { theList->first = newItem; }
    theList->last = newItem;
}

/*****************************************************************************
 *
 * Read configuration file
 *
 * Authors:  Fabrizio Arca 
 *
 * Description:
 * Performs the necessary.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
static phPFuncError_t phReadContacttestConfFile(
       struct phFrameStruct *f       /* the framework data */,  
       char *filename                /* configuration file path name */,
       phContacttestSetup_t contact  /* contact test ID */
)
{
  phPFuncError_t retVal = PHPFUNC_ERR_OK;
  phConfError_t confError;
  phConfType_t defType;
  int listLength=0, i=0, numOfError=0, found=0;
  double largeStep, smallStep, zLimit, safetyWindow, overdrive, settlingTime, dieCoord;
  /*char *myzSearchAlgorithm, *myContactPointSelection;*/
  const char *pinGroupName = NULL;
  phDieCoordinate_t dieCoordinate[PH_MAX_NUM_COORD];
  phTestCondition_t digTestCondition = NULL;
  phTestCondition_t anaTestCondition = NULL;
  int index[2];

  /*initialize the configuration manager*/
  confError = phConfInit(f->logId);
  if (confError != PHCONF_ERR_OK)
  {
     phLogFrameMessage(f->logId, PHLOG_TYPE_FATAL,
                       "could not initialize configuration (error %d)", (int) confError);
     return PHPFUNC_ERR_NOT_INIT;
  }

  confError = phConfReadConfFile(&sConfId,filename);
  if (confError != PHCONF_ERR_OK)
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_FATAL,
                      "could not read contact test configuration (error %d)", (int) confError);
     return PHPFUNC_ERR_CONFIG;
  }

  /* Read the value for "step" parameter  */
  confError = phConfConfIfDef (sConfId, CONTACT_TEST_STEP);
  if ( confError != PHCONF_ERR_OK ) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter step not defined");
    numOfError++;
  }
  confError=phConfConfType(sConfId, CONTACT_TEST_STEP, 0, NULL, &defType, &listLength);
  if ((confError != PHCONF_ERR_OK) || (defType != PHCONF_TYPE_LIST) || (listLength !=2)) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter step in the configuration file not correct");
    numOfError++;
  }
  i=0;
  confError=phConfConfNumber(sConfId,CONTACT_TEST_STEP ,1,&i,&largeStep);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter contact_test_step not properly defined");
    numOfError++;
  }
  i=1;
  confError=phConfConfNumber(sConfId,CONTACT_TEST_STEP ,1,&i,&smallStep);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter contact_test_step not properly defined");
    numOfError++;
  }

  /* Check largeStep greater than smallStep       */
  if (smallStep > largeStep) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "small step greater than large step");
    numOfError++;
  }

  /* Read the value for "z_search_algorithm" parameter  */
  if(phConfConfIfDef(sConfId, Z_SEARCH_ALGORITHM)==PHCONF_ERR_OK) 
  {
    confError = phConfConfStrTest(&found, sConfId, Z_SEARCH_ALGORITHM, "default", NULL);
    if( (confError != PHCONF_ERR_UNDEF) && (confError != PHCONF_ERR_OK) ) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "configuration value of z_search_algorithm not valid\n");
      numOfError++;
    } 
    else 
    {
      switch( found ) 
      {
        case 1:   /* "default" */
          contact->algorithm = DEFAULT;
          break;
        default:
          phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                         "configuration value of z_search_algorithm must be default\n");
          break;
      }
    }
  } 
  else 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter z_search_algorithm not defined");
    numOfError++;
  }

  /* Read the value for "contact_point_selection" parameter  */
  confError=phConfConfIfDef(sConfId,CONTACT_POINT_SELECTION);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter contact_point_selection not defined");
    numOfError++;
  } 
  else 
  {
    confError = phConfConfStrTest(&found, sConfId,
                                  CONTACT_POINT_SELECTION, "first_some_pass", "all_pass", NULL);

    if( (confError != PHCONF_ERR_UNDEF) && (confError != PHCONF_ERR_OK) ) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "configuration value of contact_point_selection not valid\n");
      numOfError++;
    } 
    else 
    {
      switch( found ) 
      {
        case 1:   /* "first_some_pass" */
          contact->contactPointSelection = SELECTION_FIRST_SOME_PASS;
          break;
        case 2:   /* "all_pass" */
          contact->contactPointSelection = SELECTION_ALL_PASS;
          break;
        default:
          phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                           "configuration value of contact_point_selection must be either first_some_pass or all_pass\n");
          break;
      }
    }
  }
  
  /* Read the value for "z_limit" parameter  */
  confError=phConfConfIfDef(sConfId,Z_LIMIT);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter z_limit not defined");

    numOfError++;
  }
  confError=phConfConfNumber(sConfId, Z_LIMIT, 0, NULL, &zLimit);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter z_limit not properly defined");
    numOfError++;
  }

  /* Read the value for "safety_window" parameter  */
  confError=phConfConfIfDef(sConfId,SAFETY_WINDOW);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter safety_window not defined");
    numOfError++;
  }
  confError=phConfConfNumber(sConfId, SAFETY_WINDOW, 0, NULL, &safetyWindow);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter safety_window not properly defined");
    numOfError++;
  }

  /* Read the value for "overdrive" parameter  */
  confError=phConfConfIfDef(sConfId, OVERDRIVE);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter overdrive not defined");
    numOfError++;
  }
  confError=phConfConfNumber(sConfId, OVERDRIVE, 0, NULL, &overdrive);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter overdrive not properly defined");
    numOfError++;
  }

  /* Read the value for "die_coordinates" parameter  */
  confError=phConfConfIfDef(sConfId, DIE_COORDINATES);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter die_coordinates not defined");
    numOfError++;
  }
  /* Initialization */
  for (i=0; i<PH_MAX_NUM_COORD; i++) 
  {
    dieCoordinate[i].x = 0;
    dieCoordinate[i].y = 0;
  }
  confError=phConfConfType(sConfId, DIE_COORDINATES, 0, NULL, &defType, &listLength);
  if ((confError != PHCONF_ERR_OK) || (defType != PHCONF_TYPE_LIST) || (listLength !=4)) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter die_coordinates in the configuration file not correct");
    numOfError++;
  }
  for (i=0; i<PH_MAX_NUM_COORD; i++) 
  {
    index[0] = i;
    index[1] = 0;
    confError = phConfConfNumber(sConfId, DIE_COORDINATES, 2, index, &dieCoord);
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter die_coordinates not properly defined");

      numOfError++;
    } 
    else
    {
      dieCoordinate[i].x = (int)dieCoord;
    }

    index[1] = 1;
    confError = phConfConfNumber(sConfId, DIE_COORDINATES, 2, index, &dieCoord);
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter die_coordinates not properly defined");
      numOfError++;
    } 
    else
    {
      dieCoordinate[i].y = (int)dieCoord;
    }
  }

  /* Read the value for settling_time parameter  */
  confError=phConfConfIfDef(sConfId, SETTLING_TIME);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter settling_time not defined");
    numOfError++;
  }
  confError = phConfConfNumber(sConfId, SETTLING_TIME, 0, NULL, &settlingTime);
  if (confError != PHCONF_ERR_OK) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter setting_time not properly defined");
    numOfError++;
  }

  /* Read the value for Digital Test Condition parameters  */
  confError = phConfConfIfDef(sConfId, DIGITAL_PIN_GROUP);
  if ( confError != PHCONF_ERR_OK ) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter digital_pin_group not defined");
    numOfError++;
  }
  confError=phConfConfType(sConfId, DIGITAL_PIN_GROUP, 0, NULL, &defType, &listLength);
  if ((confError != PHCONF_ERR_OK) || (defType != PHCONF_TYPE_LIST)) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter digital_pin_group in the configuration file not correct");
    numOfError++;
  }

  contact->digitalPingroup = BULinkedList_CreateOnHeap();
  for (i=0; i < listLength; i++) 
  {
    digTestCondition = PhNew( struct phTestCondition);
    if (digTestCondition == NULL) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,"PH lib: FATAL! "
                      "Could not allocate memory for driver initialization");
      return PHPFUNC_ERR_FATAL;
    } 

    strcpy(digTestCondition->pinGroupName, "");
    digTestCondition->forceCurrent=0;
    digTestCondition->lowLimit=0;
    digTestCondition->highLimit=0;

    confError = phConfConfString(sConfId, DIGITAL_PIN_GROUP,1, &i, &pinGroupName);
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter digital_pin_gropup not properly defined");
      numOfError++;
    }
    strcpy(digTestCondition->pinGroupName, pinGroupName);
    
    confError = phConfConfNumber(sConfId,FORCE_CURRENT_OF_DIGITAL_PIN_GROUP,1,&i,&digTestCondition->forceCurrent); 
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter force_current_of_digital_pin_group not properly defined");
      numOfError++;
    }

    index[0] = i;
    index[1] = 0;
    confError = phConfConfNumber(sConfId, LIMIT_OF_DIGITAL_PIN_GROUP, 2, index, &digTestCondition->lowLimit);
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter limit_of_digital_pin_group not properly defined");
      numOfError++;
    }
    index[1] = 1;
    confError = phConfConfNumber(sConfId, LIMIT_OF_DIGITAL_PIN_GROUP, 2, index, &digTestCondition->highLimit);
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter limit_of_digital_pin_group not properly defined");
      numOfError++;
    }

    BULinkedList_Append (contact->digitalPingroup, digTestCondition);

  } /* end loop for i < listLength Digital */


  /* Read the value for Analog Test Condition parameters  */
  confError = phConfConfIfDef(sConfId, ANALOG_PIN_GROUP);
  if ( confError != PHCONF_ERR_OK ) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter analog_pin_group not defined");
    numOfError++;
  }
  confError = phConfConfType(sConfId, ANALOG_PIN_GROUP, 0, NULL, &defType, &listLength);
  if ((confError != PHCONF_ERR_OK) || (defType != PHCONF_TYPE_LIST)) 
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                      "parameter analog_pin_group in the configuration file not correct");
    numOfError++;
  }

  contact->analogPingroup = BULinkedList_CreateOnHeap();
  for (i=0; i < listLength; i++) 
  {
    /* initialize */
    anaTestCondition = PhNew( struct phTestCondition);
    if (anaTestCondition == NULL) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,"PH lib: FATAL! "
                      "Could not allocate memory for driver initialization");
      return PHPFUNC_ERR_FATAL;
    }

    strcpy(anaTestCondition->pinGroupName, "NULL");
    anaTestCondition->forceCurrent=0;
    anaTestCondition->lowLimit=0;
    anaTestCondition->highLimit=0;

    confError = phConfConfString(sConfId, ANALOG_PIN_GROUP, 1, &i, &pinGroupName); 
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter analog_pin_group not properly defined");
      numOfError++;
    }
    strcpy(anaTestCondition->pinGroupName, pinGroupName);

    confError = phConfConfNumber(sConfId, FORCE_CURRENT_OF_ANALOG_PIN_GROUP, 1, &i, &anaTestCondition->forceCurrent); 
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter force_current_of_analog_pin_group not properly defined");
      numOfError++;
    }

    index[0] = i;
    index[1] = 0;
    confError = phConfConfNumber(sConfId, LIMIT_OF_ANALOG_PIN_GROUP, 2, index, &anaTestCondition->lowLimit);
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter limit_of_analog_pin_group not properly defined");
      numOfError++;
    }
    index[1] = 1;
    confError = phConfConfNumber(sConfId, LIMIT_OF_ANALOG_PIN_GROUP, 2, index, &anaTestCondition->highLimit);
    if (confError != PHCONF_ERR_OK) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                        "parameter limit_of_analog_pin_group not properly defined");
      numOfError++;
    }

    BULinkedList_Append (contact->analogPingroup, (void *) anaTestCondition);
  } /* end loop for i < listLength Analog*/

  /* fill phContacttestSetup structure */
  for (i=0; i<PH_MAX_NUM_COORD; i++) 
  {
    contact->dieCoordinate[i].x = dieCoordinate[i].x;
    contact->dieCoordinate[i].y = dieCoordinate[i].y;
  }
  contact->largeStep = (int)largeStep;
  contact->smallStep = (int)smallStep;
  contact->zLimit = (int)zLimit;
  contact->safetyWindow = (int)safetyWindow;
  contact->overdrive = (int)overdrive;
  contact->settlingTime = settlingTime;

  if (numOfError != 0) 
  {
    retVal =  PHPFUNC_ERR_CONFIG;
  }

  return retVal;
}

/*****************************************************************************
 *
 * log contact test configuration
 *
 * Authors: Kun Xiao
 *
 * Description: This fuction logs the parameters of the current
 * contact test configuration
 *
 * Returns: NONE
 *
 ***************************************************************************/
static void logContacttestConf(
       struct phFrameStruct *f       /* the framework data */
)
{
     phConfError_t confError = PHCONF_ERR_OK;
     const char *key;
     char *oneDef;

     phLogFrameMessage(f->logId, LOG_NORMAL,
                       "Logging the current contact test configuration....\n");
     confError = phConfConfFirstKey(sConfId, &key);
     while (confError == PHCONF_ERR_OK)
     {
       confError = phConfGetConfDefinition(sConfId, key, &oneDef);
       phLogFrameMessage(f->logId, LOG_NORMAL, "%s", oneDef);
       confError = phConfConfNextKey(sConfId, &key);
     }
     phLogFrameMessage(f->logId, LOG_NORMAL,
                       "End of contact test configuration Log\n");
}

/*****************************************************************************
 *
 * determine whether contact test is enabled or not
 *
 * Authors:  Kun Xiao
 *
 * Description:
 * determine whether contact test is enabled or not according to specific
 * configuration file parameter "enable_contact_test". returns TRUE if enable
 *
 * Returns: TRUE or FALSE
 *
 ***************************************************************************/
static int isContacttestEnable(
        struct phFrameStruct *f     /* the framework data */
)
{
  int found = 0;
  int retVal = FALSE; 
  phConfError_t confError;


  confError = phConfConfIfDef(f->specificConfId,PHKEY_OP_CONTACTTEST);
  if (confError != PHCONF_ERR_OK)
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                     "parameter enable_contact_test not defined, contact test ignored!");
    retVal = FALSE;          
  } 
  else
  {
    confError = phConfConfStrTest(&found, f->specificConfId, PHKEY_OP_CONTACTTEST, "yes", "no", NULL);
    if ((confError != PHCONF_ERR_UNDEF) && (confError != PHCONF_ERR_OK)) 
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR, "configuration value of \"%s\" not valid,"
                       " contact test ignored!",  PHKEY_OP_CONTACTTEST);
      retVal = FALSE;
    } 
    else
    {
      switch(found)
      {
        case 1:   /*contact test is enabled*/
          retVal = TRUE;
          break;

        case 2:   /*contact test is disabled*/
          retVal = FALSE;
          break;

        default:
          phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR, "configuration value of \"%s\" must "
                            "be either \"yes\" or \"no\", contact test ignored!", PHKEY_OP_CONTACTTEST);
          retVal = FALSE;
      }
    }
  }

  return retVal;
}

/*****************************************************************************
 *
 * free data
 *
 * Authors: Kun Xiao
 *
 * Description:
 * Performs the necessary. For detail
 *
 * Returns: NONE
 *
 ***************************************************************************/
static void freeData(void **data)
{
  if (data != NULL)
  {
    if (*data != NULL)
    {
      free(*data);
    }
    *data = NULL;
  }
} 

/*****************************************************************************
 *
 * free phContacttestSetup_t
 *
 * Authors: Kun Xiao
 *
 * Description:
 * Performs the necessary.
 *
 * Returns: NONE
 *
 ***************************************************************************/
static void freeContacttestSetup(
       phContacttestSetup_t contact    /* contact test ID */
)
{
  if (contact != NULL) 
  {
    BULinkedList_ClearAndDestroy(&(contact->digitalPingroup), freeData);
    BULinkedList_ClearAndDestroy(&(contact->analogPingroup), freeData);
    free(contact); 
  }
}

/*****************************************************************************
 *
 * log continuity test parameters
 *
 * Authors: Kun Xiao
 *
 * Description:
 * Performs the necessary.
 *
 * Returns: NONE
 *
 ***************************************************************************/
static void logContinuityTestParam(
       struct phFrameStruct *f          /* the framework data */,
       phContacttestSetup_t contact     /* contact test ID */
)
{
  char pinGroupName[32];
  double forceCurrent;
  double lowLimit;
  double highLimit;
  BULinkedItem *p = NULL;
  phTestCondition_t testCondition = NULL; 


  /*print continuity test parameters for verification*/ 
  phLogFuncMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
                   "**********Begin to log configuration parameters for continuity test *************");

  phLogFuncMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
                   "settling time: %f", contact->settlingTime);

  /*log digital continuity test parameters*/
  phLogFuncMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
                   "---------------------- for digital pin group --------------------------------");
  p = contact->digitalPingroup->first;
  while (p != NULL) {
    testCondition = (phTestCondition_t)(p->data);
    strcpy(pinGroupName, testCondition->pinGroupName);
    forceCurrent = testCondition->forceCurrent;
    lowLimit = testCondition->lowLimit;
    highLimit = testCondition->highLimit;
        
    phLogFuncMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
                     "{Group name: %s, forceCurrent: %lf, voltage limit: [%lf, %lf]}", 
                     pinGroupName, forceCurrent, lowLimit, highLimit);

    p = p->next;
  }

  /*log analog continuity test parameters*/
  phLogFuncMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
                   "---------------------- for analog pin group ---------------------------------");
  p = contact->analogPingroup->first;
  while(p != NULL){  
    testCondition = (phTestCondition_t)(p->data);
    strcpy(pinGroupName, testCondition->pinGroupName);
    forceCurrent = testCondition->forceCurrent;
    lowLimit = testCondition->lowLimit;
    highLimit = testCondition->highLimit;

    phLogFuncMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
                     "{Group name: %s, forceCurrent: %lf, voltage limit: [%lf, %lf]}", 
                     pinGroupName, forceCurrent, lowLimit, highLimit);

    p = p->next;
  }

  phLogFuncMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
                   "******************************* End log********************************************");   
}

/*****************************************************************************
 *
 * perform contact test
 *
 * Authors: Fabrizio Arca & Kun Xiao
 *
 * Description:
 *
 * Initialize contact test setup structure, and perform 
 * contact test.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameContacttest(
              struct phFrameStruct *f    /* the framework data */,
              char *FileNameString       /* contact test configuration file path name */
)
{
  phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
  char confFilePath[CI_MAX_COMMENT_LEN];
  char *token = NULL;
  int found = FALSE;
  phContacttestSetup_t contacttestSetupID = NULL;
  
  phPFuncError_t funcError = PHPFUNC_ERR_OK;
  
  contacttestSetupID = PhNew (struct phContacttestSetup);
  if ( contacttestSetupID == NULL )
  {
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR, "fail to allocate memory!");
    return PHTCOM_CI_CALL_ERROR;
  }

  if (!isContacttestEnable(f)) 
  {
    free(contacttestSetupID);
    contacttestSetupID = NULL;
    phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "contact test is disabled!");
    return retVal;
  }

  token = strtok(FileNameString, " \t\n");
  while (token != NULL) 
  {
    strncpy(confFilePath, token, sizeof(confFilePath) - 1);
    token = strtok(NULL, " \t\n");
    found = TRUE;
  }                                              

  if (!found) 
  { 
    strcpy(confFilePath, PHLIB_CONF_PATH "/" PHLIB_CONF_DIR "/" PHLIB_CONTACT_TEST_FILE);
    phLogFrameMessage(f->logId, LOG_NORMAL, "using default contact test " 
                      "configuration file: %s", confFilePath);
  }
  
  funcError = phReadContacttestConfFile (f, confFilePath, contacttestSetupID);
  if(funcError != PHPFUNC_ERR_OK)
  {
    free(contacttestSetupID);
    contacttestSetupID = NULL;
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR, "Contact test configuration error!");
    return PHTCOM_CI_CALL_ERROR;
  }

  /*log contact test configuration*/
  logContacttestConf(f);

  /* log continuity test coniguration */
  logContinuityTestParam(f, contacttestSetupID);

  /*
  phFrameStartTimer(f->totalTimer);
  phFrameStartTimer(f->shortTimer);
  while(!success && !completed)
  {
    funcError = phPlugPFuncContacttest(f->funcId, contacttestSetupID); 
    phFrameHandlePluginResult(f, funcError, PHPFUNC_AV_CONTACTTEST, &completed, &success, pRetVal, &exceptionAction);
  }
  */

  funcError = phPlugPFuncContacttest(f->funcId, contacttestSetupID);
  if (funcError != PHPFUNC_ERR_OK)
  {
    retVal = PHTCOM_CI_CALL_ERROR;
    phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR, "phPlugPFuncContacttest \"%s\" returned error: %d, "
                      "Contact test failed!", FileNameString, funcError);
  } 
  else
  {
    phLogFrameMessage(f->logId, LOG_NORMAL, "phPlugPFuncContacttest \"%s\", "
                      "Contact test succeed!", FileNameString);
  }

  /* free contacttestSetupID */
  freeContacttestSetup(contacttestSetupID); 

  /* free sConfId */
  if (sConfId != NULL)
  {
    phConfDestroyConf(sConfId);
    free(sConfId);
    sConfId = NULL;
  }

  return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/

