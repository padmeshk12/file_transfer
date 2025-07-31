/******************************************************************************
 *
 *       (c) Copyright Advantest 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : equipment_cmd.c
 * Description: Send/Recieve equipment command, reply and SRQs.
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>

/*--- module includes -------------------------------------------------------*/

#include "ph_tcom.h"
#include "ph_log.h"
#include "ph_conf.h"
#include "ph_mhcom.h"
#include "ph_phook.h"
#include "ph_tools.h"

#include "ph_frame.h"
#include "ph_frame_private.h"
#include "equipment_cmd.h"
#include "ph_keys.h"
#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- functions -------------------------------------------------------------*/

static long readTimeOutValue(struct phFrameStruct *f)
{
  phConfError_t confError = PHCONF_ERR_OK;
  double timeoutConfig = 0.0;
  long timeout = 5000000L;

  //read the timeout value from config file
  confError = phConfConfIfDef(f->specificConfId, PHKEY_FC_HEARTBEAT);
  if (confError == PHCONF_ERR_OK)
  {
    confError = phConfConfNumber(f->specificConfId, PHKEY_FC_HEARTBEAT, 0, NULL, &timeoutConfig);
  }

  if (confError == PHCONF_ERR_OK)
  {
    timeout = (long) (timeoutConfig * 1000.0);
  }

  return timeout;
}

/*****************************************************************************
 *
 * Send one-way command to the equipment.
 *
 * Description:
 *    This function sends protocol command to the equipment without waiting for
 *    the response.
 *
 * Returns: CI_CALL_PASS on success
 *
 * Note: The function name is *Gpib* due to historical reason; actually this function
 * can send out command over GPIB, LAN and RS232 connections.
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameExecGpibCmd(
                                    struct phFrameStruct *f,
                                    char *commandString,
                                    char **responseString
)
{
  static char response[PHCOM_MAX_MESSAGE_LENGTH + 1] = "";
  phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
  phComError_t comRetVal = PHCOM_ERR_OK;
  long abortFlag = 0;
  long timeout = readTimeOutValue(f);
  bool done = false;
  phConfError_t confError;
  int found;
  char eos[4] = "\r\n";
  char command[10240] = "";

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "entering phFrameExecGpibCmd");

  strcpy(response, "");
  *responseString = response;

  //read the EOS from config file
  confError = phConfConfStrTest(&found, f->specificConfId,
      PHKEY_GB_EOS, "\\r", "\\n", "\\r\\n", NULL);
  if (confError == PHCONF_ERR_OK)
  {
    switch ( found )
    {
      case 1:
        strcpy(eos, "\r");
        break;
      case 2:
        strcpy(eos, "\n");
        break;
      case 3:
        strcpy(eos, "\r\n");
        break;
      default:
        break;
    }
  }

  // fix MI-1322, append end-of-string if sting without line break
  if(commandString!=NULL){
    if(strlen(commandString) >= 1 \
        && (commandString[strlen(commandString)-1] == '\n' || commandString[strlen(commandString)-1] == '\r'))
    {
      strncpy(command, commandString, sizeof(command)-1);
    }
    else
    {
      sprintf(command, "%s%s", commandString, eos);
    }
  }

  //send out the command
  while(!done)
  {
    //check the abort flag
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if(abortFlag == 1)
    {
      //if the abort flag is set, break the loop with error
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                        "Interrupt detected while sending command to the equipment.");
      retVal = PHTCOM_CI_CALL_ERROR;
      break;
    }

    //send out the command
    comRetVal = phComGpibSend(f->comId, command, strlen(command),timeout);

    //check the return value
    switch(comRetVal)
    {
    case PHCOM_ERR_TIMEOUT:
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                        "Time out sending command to the equipment; will keep waiting");
      break;
    case PHCOM_ERR_OK:
      retVal = PHTCOM_CI_CALL_PASS;
      done = true;
      break;
    default:
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                              "Failed to send the command to the equipment.");
      retVal = PHTCOM_CI_CALL_ERROR;
      done = true;
      break;
    }
  }

  return retVal;
}

/*****************************************************************************
 *
 * Send query command to the equipment.
 *
 * Description:
 *    This function sends protocol command to the equipment and then wait until
 *    a response string is received.
 *
 * Returns: CI_CALL_PASS on success
 *
 * Note: The function name is *Gpib* due to historical reason; actually this
 * function can send out command over GPIB, LAN and RS232 connections.
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameExecGpibQuery(
                                    struct phFrameStruct *f,
                                    char *commandString,
                                    char **responseString
)
{
  static char response[PHCOM_MAX_MESSAGE_LENGTH + 1] = "";
  phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
  phComError_t comRetVal = PHCOM_ERR_OK;
  long abortFlag = 0;
  int responseLen = 0;
  long timeout = readTimeOutValue(f);
  bool done = false;

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "entering phFrameExecGpibQuery");

  //firstly send out the command
  retVal = phFrameExecGpibCmd(f, commandString, responseString);

  if (retVal != PHTCOM_CI_CALL_PASS)
    return retVal;

  strcpy(response, "");
  *responseString = response;

  //waiting for the response of the query
  while(!done)
  {
    //check the abort flag
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if(abortFlag == 1)
    {
      //if the abort flag is set, break the loop with error
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                        "Interrupt detected while receiving the results from the equipment");
      retVal = PHTCOM_CI_CALL_ERROR;
      break;
    }

    //send out the command
    const char *tmpString = NULL;

    comRetVal = phComGpibReceive(f->comId, &tmpString, &responseLen, timeout);

    //check the return value
    switch(comRetVal)
    {
    case PHCOM_ERR_TIMEOUT:
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                        "Time out waiting for response from the equipment; will keep waiting");
      break;
    case PHCOM_ERR_OK:
      strncpy(response, tmpString, sizeof(response) - 1);
      phTrimLeadingTrailingDelimiter(response, "\r\n");
      retVal = PHTCOM_CI_CALL_PASS;
      done = true;
      break;
    default:
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                              "Failed to send the command to the equipment.");
      retVal = PHTCOM_CI_CALL_ERROR;
      done = true;
      break;
    }
  }

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
                    "exiting phFrameExecGpibQuery, responseString = ->%s<-", *responseString);
  return retVal;
}



/*****************************************************************************
 *
 * get expected SRQ status byte
 *
 * Authors: Xiaofei Han
 *
 * Description:
 *    This fucntion is used to get a SRQ status byte
 *
 * Input/Output:
 *    Input parameter "commandString" is the expected SRQ list in which each
 *    SRQ value is delimited by a white space. When the list is empty, the
 *    current SRQ in the FIFO will be returned. If no SRQ is currently available
 *    in the FIFO or there is no matched SRQ in the FIFO, this function will
 *    keep waiting. For this case, this function can only be aborted  by ph_interrupt().
 *    The responseString output parameter contains the SRQ returned.
 *
 * Returns: CI_CALL_PASS on success
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameGetSrqStatusByte(
                                    struct phFrameStruct *f,
                                    char *commandString,
                                    char **responseString
)
{
  static char response[PHCOM_MAX_MESSAGE_LENGTH + 1] = "";
  phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
  phComError_t comRetVal = PHCOM_ERR_OK;
  phComGpibEvent_t event;
  std::vector<int> expected;
  std::vector<std::string> splitCommandString;
  std::vector<std::string> splitSrqString;
  long abortFlag = 0;
  int i = 0;
  long timeout = readTimeOutValue(f);
  bool done = false;
  long totalTime = 0;

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "entering phFrameGetSrqStatusByte");

  strcpy(response, "");
  *responseString = response;

  //split the expected SRQs
  if(commandString!=NULL){
    if(strlen(commandString) != 0)
    {
      std::string cmdString(commandString);
      boost::split(splitCommandString, cmdString, boost::is_any_of(" "), boost::token_compress_on);
    }
  }

  // from version 2.7, this function can also support ph_read_status_byte(srqlist = {}, timeout = 0)
  // the commad string passed from ph_read_status_byte will be like "1,2,3 4000" or "null 4000", and old format is like "1 2 3" or ""
  // to the end of compatibility:
  // firstly, the string will be seperated by " "
  // if we can find "," or "null"(means empty vector) in first string means it's called by ph_read_status_byte
  // else it is called by ph_get_srq_status_byte, and the format is old one, just seperate string by " "
  if(splitCommandString[0].find(',') == std::string::npos && splitCommandString[0] != "null")
  {
      for(unsigned int i = 0; i < splitCommandString.size(); i++)
      {
          expected.push_back(atoi(splitCommandString[i].c_str()));
      }
  }
  else
  {
      totalTime = atoi(splitCommandString[1].c_str());
      if(totalTime == 0)
          timeout = 0;
      if(totalTime == -1)
          totalTime = INT_MAX;
      phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
              "phFrameGetSrqStatusByte invoked by ph_read_status, timeout value for the invocation is: %ld", totalTime);
      if(splitCommandString[0] != "null")
      {
          std::string srqString(splitCommandString[0]);
          boost::split(splitSrqString, srqString, boost::is_any_of(","), boost::token_compress_on);
          for(unsigned int i = 0; i < splitSrqString.size(); i++)
              if(splitSrqString[i] != "")
                  expected.push_back(atoi(splitSrqString[i].c_str()));
      }
  }

  long accumulatedTime = 0;
  //waiting for the SRQ in a loop
  while(!done)
  {
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if(abortFlag == 1)
    {
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                        "Interrupt detected while waiting for SRQ");
      retVal = PHTCOM_CI_CALL_ERROR;
      break;
    }

    //SRQ query
    comRetVal = phComGpibGetExpectedEvents(f->comId, event, expected, timeout);

    //check the return value
    switch(comRetVal)
    {
    case PHCOM_ERR_TIMEOUT:
        accumulatedTime += timeout/1000;
        if(accumulatedTime > totalTime)
        {
            phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "Time out querying SRQs from the equipment; will abort waiting");
            retVal = PHTCOM_CI_CALL_TIMEOUT;
            done = true;
        }
        else
        {
            phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "Time out querying SRQs from the equipment; will keep waiting");
        }
      break;
    case PHCOM_ERR_OK:
      retVal = PHTCOM_CI_CALL_PASS;
      phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_4, "Got expected SRQ 0x%02x (%d)",
        (int) event.d.srq_byte, (int) event.d.srq_byte);
      sprintf(response, "%d", (int)event.d.srq_byte);
      done = true;
      break;
    default:
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                              "Failed to query the SRQs from the equipment.");
      retVal = PHTCOM_CI_CALL_ERROR;
      done = true;
      break;
    }
  }

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
                    "exiting phFrameGetSrqStatusByte, responseString = ->%s<-", *responseString);
  return retVal;
}

/*****************************************************************************
 *
 * receive reply from the equipment.
 *
 * Description:
 *    This function wait until a response string is received.
 *
 * Returns: CI_CALL_PASS on success CI_CALL_TIMEOUT on timeout
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameReceive(
    struct phFrameStruct *f             /* the framework data */,
    char *commandString,
    char **responseString
)
{
  static char response[PHCOM_MAX_MESSAGE_LENGTH + 1] = "";
  phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
  phComError_t comRetVal = PHCOM_ERR_OK;
  long abortFlag = 0;
  int responseLen = 0;
  long timeout = readTimeOutValue(f);
  bool done = false;
  long totalTime = 0;

  strcpy(response, "");
  *responseString = response;

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "entering phFrameReceive");

  totalTime = atoi(commandString);
  if(totalTime == -1)
      totalTime = INT_MAX;
  phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
          "phFrameReceive invoked by ph_receive, timeout value for the invocation is: %ld", totalTime);

  long accumulatedTime = 0;
  //waiting for the response
  while(!done)
  {
    //check the abort flag
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if(abortFlag == 1)
    {
      //if the abort flag is set, break the loop with error
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                        "Interrupt detected while receiving the results from the equipment");
      retVal = PHTCOM_CI_CALL_ERROR;
      break;
    }

    //receive buff
    const char *tmpString = NULL;

    comRetVal = phComGpibReceive(f->comId, &tmpString, &responseLen, timeout);

    //check the return value
    switch(comRetVal)
    {
    case PHCOM_ERR_TIMEOUT:
        accumulatedTime += timeout/1000;
        if(accumulatedTime > totalTime)
        {
            phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "Time out waiting for response from the equipment; will abort waiting");
            retVal = PHTCOM_CI_CALL_TIMEOUT;
            done = true;
        }
        else
        {
            phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "Time out waiting for response from the equipment; will keep waiting");
        }
      break;
    case PHCOM_ERR_OK:
      strncpy(response, tmpString, sizeof(response) - 1);
      phTrimLeadingTrailingDelimiter(response, "\r\n");
      retVal = PHTCOM_CI_CALL_PASS;
      done = true;
      break;
    default:
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                              "Failed to send the command to the equipment.");
      retVal = PHTCOM_CI_CALL_ERROR;
      done = true;
      break;
    }
  }

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE,
                    "exiting phFrameExecGpibQuery, responseString = ->%s<-", *responseString);
  return retVal;
}

/*****************************************************************************
 *
 * Send command to the equipment.
 *
 * Description:
 *    This function sends protocol command to the equipment
 *
 * Returns: CI_CALL_PASS on success CI_CALL_TIMEOUT on timeout
 *
 ***************************************************************************/
phTcomUserReturn_t phFrameSend(
                                    struct phFrameStruct *f,
                                    char *commandString,
                                    char **responseString
)
{
  static char response[PHCOM_MAX_MESSAGE_LENGTH + 1] = "";
  strcpy(response, "");
  *responseString = response;
  phTcomUserReturn_t retVal = PHTCOM_CI_CALL_PASS;
  phComError_t comRetVal = PHCOM_ERR_OK;
  long abortFlag = 0;
  long timeout = readTimeOutValue(f);
  bool done = false;
  std::vector<std::string> splitCommandString;
  std::string commandStringWithoutTimeout = "";
  long totalTime = 0;

  phLogFrameMessage(f->logId, PHLOG_TYPE_TRACE, "entering phFrameSend");


  // we have been ensure the command string will not be empty
  std::string cmdString(commandString);
  boost::split(splitCommandString, cmdString, boost::is_any_of(" "), boost::token_compress_on);

  totalTime = atoi(splitCommandString[splitCommandString.size()-1].c_str());
  if(totalTime == -1)
      totalTime = INT_MAX;
  phLogFrameMessage(f->logId, PHLOG_TYPE_MESSAGE_4,
          "phFrameSend invoked by ph_send, timeout value for the invocation is: %ld", totalTime);

  int iloop = 0;
  while(iloop < splitCommandString.size()-1)
  {
      commandStringWithoutTimeout.append(splitCommandString[iloop]);
      if(++iloop < splitCommandString.size()-1)
          commandStringWithoutTimeout.append(" ");
  }

  long accumulatedTime = 0;
  //send out the command
  while(!done)
  {
    //check the abort flag
    phTcomGetSystemFlag(f->testerId, PHTCOM_SF_CI_ABORT, &abortFlag);
    if(abortFlag == 1)
    {
      //if the abort flag is set, break the loop with error
      phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING,
                        "Interrupt detected while sending command to the equipment.");
      retVal = PHTCOM_CI_CALL_ERROR;
      break;
    }

    comRetVal = phComGpibSend(f->comId, (char*)(commandStringWithoutTimeout.c_str()), commandStringWithoutTimeout.length(),timeout);

    //check the return value
    switch(comRetVal)
    {
    case PHCOM_ERR_TIMEOUT:
        accumulatedTime += timeout/1000;
        if(accumulatedTime > totalTime)
        {
            phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "Time out sending command to the equipment; will abort waiting");
            retVal = PHTCOM_CI_CALL_TIMEOUT;
            done = true;
        }
        else
        {
            phLogFrameMessage(f->logId, PHLOG_TYPE_WARNING, "Time out sending command to the equipment; will keep waiting");
        }
      break;
    case PHCOM_ERR_OK:
      retVal = PHTCOM_CI_CALL_PASS;
      done = true;
      break;
    default:
      phLogFrameMessage(f->logId, PHLOG_TYPE_ERROR,
                              "Failed to send the command to the equipment.");
      retVal = PHTCOM_CI_CALL_ERROR;
      done = true;
      break;
    }
  }

  return retVal;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/

