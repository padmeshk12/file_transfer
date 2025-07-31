/******************************************************************************
 *
 *       (c) Copyright Advantest, 2015
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : EventObserver.C
 * CREATED  : 5 Jan 2015
 *
 * CONTENTS : Log and Timer definitions
 *
 * AUTHORS  : Magco Li, SHA-R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : 5 Jan 2015, Magco Li , created
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

#include <errno.h>


/*--- module includes -------------------------------------------------------*/

#include "handlerController.h"
#include "eventObserver.h"

/*--- defines ---------------------------------------------------------------*/

/*--- typedefs --------------------------------------------------------------*/

/*--- class definitions -----------------------------------------------------*/

/*--- class implementations -------------------------------------------------*/


/*--- FileException ---------------------------------------------------------*/
FileException::FileException(int errNum) :
    errorNumber(errNum)
{
}

void FileException::printMessage()
{
    Std::cerr << "Error occured in accessing file: ";

    switch (errorNumber)
    {
      default:
        Std::cerr << "errno " << errorNumber;
        break;
    }

}

/*--- PlaybackErrorException ------------------------------------------------*/
PlaybackErrorException::PlaybackErrorException(const char *msg) :
    message(msg)
{
}

void PlaybackErrorException::printMessage()
{
    Std::cerr << "Playback error: " << message;
}


/*--- EventObserver ---------------------------------------------------------*/

EventObserver::EventObserver(phLogId_t l  /* log identifier */) :
    LogObject(l)
{
}



/*****************************************************************************
 * End of file
 *****************************************************************************/
