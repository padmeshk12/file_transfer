/******************************************************************************
 *
 *       (c) Copyright Advantest
 *
 *-----------------------------------------------------------------------------
 *
 * MODULE   : ph_rs232_mhcom.c
 * CREATED  : Jun 2015
 *
 * CONTENTS : RS232 communication layer entry point
 *
 * AUTHORS  : Magco Li, Shanghai R&D, initial revision
 *
 *-----------------------------------------------------------------------------
 *
 * HISTORY  : Jun 2015, Magco Li, created
 *
 *
 *****************************************************************************/

/*--- system includes -------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>

/*--- module includes -------------------------------------------------------*/

#include "ph_tools.h"
#include "ph_mhcom.h"

#include "gioBridge.h"
#include "ph_rs232_mhcom.h"
#include "ph_mhcom_private.h"
#include "dev/DriverDevKits.hpp"


#ifdef USE_DMALLOC
  #include "dmalloc.h"
#endif


/*--- defines ---------------------------------------------------------------*/

#define PH_SESSION_CHECK

#ifdef DEBUG
#define PH_SESSION_CHECK
#endif

#ifdef PH_SESSION_CHECK
#define CheckSession(x) \
    if (!(x) || (x)->myself != (x)) \
        return PHCOM_ERR_UNKNOWN_DEVICE
#else
#define CheckSession(x)
#endif

#define RELEASE_BUFFER(x) {\
   if ( x != NULL ) \
   {\
     free(x);\
     x = NULL;\
   }\
}

/** Perform a system library call, handling errors.
 *
 * @param call The system call to perform.  Must return -1 on failure.
 * @param err Variable of type RS232_ERROR; updated on failure.
 *
 * @return TRUE on success, FALSE on error.
 */
#define SYSCALL(call, err)                      \
  (((call) < 0)                                 \
    ? ((err) = mapErrno(errno), 0)               \
    : 1)

/*--- global variables ------------------------------------------------------*/
rs232Device rs232Handle = NULL;

/*--- function prototypes ---------------------------------------------------*/


/*--- functions -------------------------------------------------------------*/
/*****************************************************************************
 * handle return value
 *****************************************************************************/
static void handleReturnValue(phComError_t rtnValue)
{
    if(rtnValue == PHCOM_ERR_OK || rtnValue == PHCOM_ERR_TIMEOUT)
        return;
    driver_dev_kits::AlarmController alarmController;
    alarmController.raiseCommunicationAlarm(GPIB_ERROR_DESCRIPTION[rtnValue]);
}

/*****************************************************************************
 *
 * Get timestamp string
 *
 * Author: Magco Li
 *
 * Description:
 * Returns a pointer to a string in the format like
 *
 * 1998/01/15 13:45:12.3456
 *
 * based on the current time and local time zone
 *
 *****************************************************************************/

static char *getTimeString(int withDate)
{
    static char theTimeString[MAX_TIMESTRING_LENGTH];
    struct tm *theLocaltime;
    time_t theTime;
    struct timeval getTime;

    gettimeofday(&getTime, NULL);
    theTime = (time_t) getTime.tv_sec;
    theLocaltime = localtime(&theTime);
    if (withDate)
    {
        sprintf(theTimeString, "%4d/%02d/%02d %02d:%02d:%02d.%04d",
              theLocaltime->tm_year + 1900,
              theLocaltime->tm_mon + 1,
              theLocaltime->tm_mday,
              theLocaltime->tm_hour,
              theLocaltime->tm_min,
              theLocaltime->tm_sec,
              (int)(getTime.tv_usec/100));
    }
    else
    {
        sprintf(theTimeString, "%02d:%02d:%02d.%04d",
              theLocaltime->tm_hour,
              theLocaltime->tm_min,
              theLocaltime->tm_sec,
              (int)(getTime.tv_usec/100));
    }

    return theTimeString;
}

/*****************************************************************************
 *
 * Set timeout
 *
 * Authors: Magco Li
 *
 * Description:.
 * Set timeout before send or receive data, call setTimeout()
 *
 ****************************************************************************/
static phComRs232Error_t rs232SetTimeout(
    rs232Device rs232Handle,
    UINT32 milliSeconds
)
{

    if (!rs232Handle)
    {
        return RS232_ERR_INVALID_ID;
    }
    rs232Handle->timeout = milliSeconds;

    return RS232_ERR_NO_ERROR;
}

/*****************************************************************************
 *
 * Check for termination character
 *
 * Authors: Magco Li
 *
 * Description:
 * Check end of data of received message
 *
 ****************************************************************************/
static bool isEoi(
    rs232Device rs232Handle,
    UINT32 eoi
)
{
    int i=0;
    for (i=0;i<rs232Handle->eoisNumber;i++)
    {
        if (rs232Handle->readEOIs[i] == eoi)
        {
            return true;
        }
    }
    return false;
}

/*****************************************************************************
 *
 * Convert an errno value to a phComRs232Error_t error code
 *
 * Authors: Magco Li
 *
 * Description:
 * Set error map when error occurs
 *
 ****************************************************************************/
static phComRs232Error_t mapErrno(int err)
{
    switch (errno)
    {
        case EACCES:
        case EPERM:
          return RS232_ERR_PERMISSION_DENIED;

        case ENOENT:
        case ENODEV:
        case ENOTDIR:
        case ENXIO:
          return RS232_ERR_DEVICE_NOT_ACCESSIBLE;

        case ENFILE:
        case ENOMEM:
          return RS232_ERR_OUT_OF_RESOURCES;

        case EINTR:
          return RS232_ERR_INTERRUPT;

        default:
          return RS232_ERR_IO_ERROR;
    }
}

/*****************************************************************************
 *
 * Control communication parameters of an RS232 interface
 *
 * Authors: Magco Li
 *
 * Description:
 * actual set baud, width, stop bits, parity and flow control for rs232 interface
 *
 ****************************************************************************/
static phComRs232Error_t rs232Ctrl(
    rs232Device rs232Handle,
    RS232_REQUEST request,
    int setting,
    char *flowControlOrParity
)
{
    phComRs232Error_t err = RS232_ERR_NO_ERROR;

    if (!rs232Handle)
    {
        return RS232_ERR_INVALID_ID;
    }

    if (!rs232Handle->handle)
    {
        // Not supported for device sessions.
        return RS232_ERR_OPERATION_NOT_SUPPORTED;
    }

    /* The termios functions describe a general terminal interface that is
     * provided to control asynchronous communications ports.
     * the structure location:
     * /usr/src/kernels/2.6.18-308.el5-x86_64/include/asm-x86_64/termbits.h
     * /usr/src/kernels/2.6.18-308.el5-x86_64/include/asm-i386/termbits.h
     * This structure contains at least the following members:

          tcflag_t c_iflag;      // input modes
          tcflag_t c_oflag;      // output modes
          tcflag_t c_cflag;      // control modes
          tcflag_t c_lflag;      // local modes
          cc_t     c_cc[NCCS];   // control chars
     */
    struct termios tio;
    //Get the current tio for the port.
    if (SYSCALL(tcgetattr(rs232Handle->handle, &tio), err))
    {
        switch (request)
        {
            case RS232_BAUD:
            {
                speed_t speed = B0;
                switch (setting)
                {
                    case 0:
                      speed = B0;
                      break;
                    case 50:
                      speed = B50;
                      break;
                    case 75:
                      speed = B75;
                      break;
                    case 110:
                      speed = B110;
                      break;
                    case 134:
                      speed = B134;
                      break;
                    case 150:
                      speed = B150;
                      break;
                    case 200:
                      speed = B200;
                      break;
                    case 300:
                      speed = B300;
                      break;
                    case 600:
                      speed = B600;
                      break;
                    case 1200:
                      speed = B1200;
                      break;
                    case 1800:
                      speed = B1800;
                      break;
                    case 2400:
                      speed = B2400;
                      break;
                    case 4800:
                      speed = B4800;
                      break;
                    case 9600:
                      speed = B9600;
                      break;
                    case 19200:
                      speed = B19200;
                      break;
                    case 38400:
                      speed = B38400;
                      break;
                    case 57600:
                      speed = B57600;
                      break;
                    case 115200:
                      speed = B115200;
                      break;
                    case 230400:
                      speed = B230400;
                      break;
                    default:
                      err = RS232_ERR_PARAMETER_ERROR;
                      break;
                }
                if (err == RS232_ERR_NO_ERROR)
                {
                    //Set the baud rates to tio
                    SYSCALL(cfsetispeed(&tio, speed), err);
                    SYSCALL(cfsetospeed(&tio, speed), err);
                }
                break;
            }

            case RS232_PARITY:
                tio.c_cflag &= ~(PARENB | PARODD | CMSPAR); //input and output isn't odd,
                                                            //disable parity
                                                            //
                //no parity checking
                if (strcmp(flowControlOrParity, "none") == 0)
                {
                    tio.c_iflag &= ~INPCK;              //no input parity checking
                }
                //even parity checking
                else if (strcmp(flowControlOrParity, "even") == 0)
                {
                    tio.c_iflag |= INPCK;               //no input parity checking
                    tio.c_cflag |= PARENB;              //Enable parity generation on output and parity checking for input
                }
                //odd parity checking
                else if (strcmp(flowControlOrParity, "odd") == 0)
                {
                    tio.c_iflag |= INPCK;               //enable input parity checking
                    tio.c_cflag |= PARENB | PARODD;     //Enable parity generation on output and parity checking for input
                                                        //input and output is odd
                }
                //mark parity checking
                else if (strcmp(flowControlOrParity, "mark") == 0)
                {
                    tio.c_iflag |= INPCK;                    //enable input parity checking
                    tio.c_cflag |= PARENB | PARODD | CMSPAR; //Enable parity generation on output and parity checking for input
                                                             //input and output is odd
                                                             //
                }
                //space parity checking
                else if (strcmp(flowControlOrParity, "space") == 0)
                {
                    tio.c_iflag |= INPCK;                    //enable input parity checking
                    tio.c_cflag |= PARENB | CMSPAR;          //Enable parity generation on output and parity checking for input
                                                             //
                }
                else
                {
                    err = RS232_ERR_PARAMETER_ERROR;
                }
                break;

            case RS232_STOP_BITS:
                switch (setting)
                {
                    case 1:
                        tio.c_cflag &= ~CSTOPB; //send one stop bit
                        break;
                    case 2:
                        tio.c_cflag |= CSTOPB;  //send two stop bits
                        break;
                    default:
                        err = RS232_ERR_PARAMETER_ERROR;
                        break;
                }
                break;

            case RS232_WIDTH:
                tio.c_cflag &= ~CSIZE;   //mask the character size bits
                switch (setting)
                {

                    case 5:
                      tio.c_cflag |= CS5; //select 5 data bits
                      break;
                    case 6:
                      tio.c_cflag |= CS6; //select 5 data bits
                      break;
                    case 7:
                      tio.c_cflag |= CS7; //select 7 data bits
                      break;
                    case 8:
                      tio.c_cflag |= CS8; //select 8 data bits
                      break;

                    default:
                      err = RS232_ERR_PARAMETER_ERROR;
                      break;
                }
                break;

            case RS232_FLOW_CONTROL:

                if (strcmp(flowControlOrParity, "none") == 0)
                {
                    tio.c_iflag &= ~(IXON | IXOFF);    //disable software flow control
                    tio.c_cflag &= ~CRTSCTS;           //not used RTS/CTS flow control
                }
                else if (strcmp(flowControlOrParity, "xon_xoff") == 0)
                {
                    tio.c_iflag |= IXON | IXOFF;       //enable software flow control
                    tio.c_cflag &= ~CRTSCTS;           //not used RTS/CTS flow control
                }
                else if (strcmp(flowControlOrParity, "rts_cts") == 0)
                {
                    tio.c_iflag &= ~(IXON | IXOFF);    //disable software flow control
                    tio.c_cflag |= CRTSCTS;            //used RTS/CTS flow control
                }
                else
                {
                    err = RS232_ERR_PARAMETER_ERROR;
                }
                break;

            default:
                err = RS232_ERR_PARAMETER_ERROR;
                break;
        }
        if (err == RS232_ERR_NO_ERROR)
        {
            //Set new tio for the port
            SYSCALL(tcsetattr(rs232Handle->handle, TCSADRAIN, &tio), err);//wait until everything has been transmitted
        }
    }

    return err;
}

/*****************************************************************************
 *
 * Control communication parameters of an RS232 interface
 *
 * Authors: Magco Li
 *
 * Description:
 * Set baud, width, stop bits, parity and flow control for rs232 interface
 * call rs232Ctrl()
 *
 *****************************************************************************/
static phComError_t phRs232Ctrl(
    rs232Device rs232Handle,
    int request,
    int setting,
    char *flowControlOrParity
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    int ctrlRtnVal = 0;

#ifdef DEBUG
    char *log_message = NULL;
#endif

    switch (rs232Handle->mode)
    {
        case PHCOM_ONLINE:
        case PHCOM_SIMULATION:
#ifdef DEBUG
          switch (request)
          {
              case RS232_BAUD:
              log_message = "BAUD";
              break;
              case RS232_PARITY:
              log_message = "PARITY";
              break;
              case RS232_STOP_BITS:
              log_message = "STOP_BITS";
              break;
              case RS232_WIDTH:
              log_message = "WIDTH";
              break;
              case RS232_FLOW_CONTROL:
              log_message = "FLOW_CONTROL";
              break;
              case RS232_READ_EOI:
              log_message = "READ_EOI";
              break;
              default:
              log_message = NULL;
              break;
          }
          if (log_message)
          {
              phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE,
                  "rs232ctrl(\"%s\", 0x%08lX)", log_message, setting);
          }
          else
          {
              phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE,
                  "rs232ctrl(%d, 0x%08lX)", request, setting);
          }
    #endif

          /* make actual rs23Ctrl call */
          if (rtnValue == PHCOM_ERR_OK)
          {
              if ((ctrlRtnVal = rs232Ctrl(rs232Handle, (RS232_REQUEST)request, setting, flowControlOrParity)))
              {
                  phLogComMessage(rs232Handle->loggerId, PHLOG_TYPE_ERROR,
                      "rs232ctrl(%d, %d, %s) failed", request, setting, flowControlOrParity);
                  rtnValue = PHCOM_ERR_RS232_ICTRL_FAIL;
              }
          }

          break;
        case PHCOM_OFFLINE:
#ifdef DEBUG
          phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE,
              "offline rs232ctrl(%d, %ld, %s)", request, setting, flowControlOrParity);
#endif
          break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
        //empty
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Set timeout
 *
 * Authors: Magco Li
 *
 * Description:
 * actual Set timeout before send or receive data
 *
 ****************************************************************************/
static phComError_t setTimeout(
    rs232Device rs232Handle            /* rs232 device wrapper */,
    long tval                          /* timout in milli seconds */
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    int rs232RtnVal = 0;

    switch (rs232Handle->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE, "settimeout(%ld)", tval);
#endif

        /* set timeout */
        if ((rs232RtnVal = rs232SetTimeout(rs232Handle, tval)))
        {
            phLogComMessage(rs232Handle->loggerId, PHLOG_TYPE_ERROR,
                "rs232SetTimeout(%ld) failed: ", tval);
            rtnValue = PHCOM_ERR_RS232_ITIMEOUT_FAIL;
        }
        break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE, "offline setimeout()");
#endif
        break;
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Close the rs232 device session
 *
 * Author: Magco Li
 *
 * Description:
 * free rs232 device wrapper.
 *
 *****************************************************************************/

static phComError_t closeRs232Session(
    rs232Device rs232Handle
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;

    switch (rs232Handle->mode)
    {
        case PHCOM_ONLINE:
        case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE, "closeRs232Session()");
#endif
            free(rs232Handle);
            rs232Handle = NULL;
            break;
        case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(loggerId, LOG_VERBOSE, "offline closeRs232Session()");
#endif
            break;
    }

    return rtnValue;
}

/*****************************************************************************
 *
 * Open the underlaying RS232 session
 *
 * Author: Magco Li
 *
 * Description:
 * Open rs232 session and set rs232 parameter, 0: success, others: failed
 *
 *****************************************************************************/

static phComError_t openRs232Session(
    struct phComStruct *session
)
{
    phComError_t rtnValue = PHCOM_ERR_OK;
    char tmpString[SYMBOLIC_NAME_LENGTH+1] = "";

    sprintf(tmpString, "%s", session->rs232->device);

   if (PHCOM_ONLINE == session->mode
       || PHCOM_SIMULATION == session->mode)
   {
        char *deviceName = strchr(tmpString, '/');
        if (deviceName == NULL)
        {
           return static_cast<phComError_t>(RS232_ERR_INVALID_SYMBOLIC_NAME);
        }
        // Try to open the device.

        int handle = open(deviceName, O_RDWR | O_NOCTTY); //read write or pName is a terminal device
        if (handle < 0)
        {
            return PHCOM_ERR_OK;
        }
        if (fcntl(handle, F_SETFD, FD_CLOEXEC) < 0) //Set the handle flags to the value specified by FD_CLOEXEC
        {
            close(handle);
            return PHCOM_ERR_OK;
        }

        // Put the device into raw (non-canonical) mode.
        struct termios tio;
        if (tcgetattr(handle, &tio) < 0)
        {
            close(handle);
            return PHCOM_ERR_OK;
        }

        tio.c_iflag &= ~(IGNBRK | BRKINT             // break -> '\0'
            | IGNPAR | PARMRK                        // parity error -> '\0'
            | ISTRIP | INLCR | IGNCR | ICRNL | IUCLC // pass data unchanged
            | IXANY);                                // only XON restarts flow

        tio.c_oflag &= ~OPOST;                       // pass data unchanged

        tio.c_cflag |= CLOCAL | CREAD;               // enable receiver

        tio.c_lflag &= ~(ECHO | ECHONL               // no echo
            | ISIG                                   // no signals
            | ICANON | IEXTEN);                      // no line editing


        if (tcsetattr(handle, TCSADRAIN, &tio) < 0)//sets the parameters associated with the terminal from the tio structure
                                                   //TCSADRAIN: the change occurs after all output written to handle has been transmitted
        {
            close(handle);
            return PHCOM_ERR_OK;
        }

        // OK, create a descriptor
        rs232Handle = PhNew(struct device);
        if (!rs232Handle)
        {
              phLogComMessage(session->loggerId, PHLOG_TYPE_ERROR,
                      "unable to establish rs232 device session for interface \"%s\"", tmpString);
              free(rs232Handle);
              rs232Handle = NULL;
              phComRs232Close(session);
              close(handle);
              return PHCOM_ERR_OK;
        }

        rs232Handle->handle = handle;
        rs232Handle->timeout = 0;
        rs232Handle->eoisNumber = session->rs232->eoisNumber;
        rs232Handle->mode = session->mode;
        rs232Handle->loggerId = session->loggerId;

        memcpy(rs232Handle->readEOIs, session->rs232->eois, session->rs232->eoisNumber*sizeof(unsigned long));

        if (session->mode == PHCOM_OFFLINE)
        {
            strcpy(rs232Handle->card, "OFF-LINE RS232");
        }
        else
        {
            strcpy(rs232Handle->card, "RS232");
        }

#ifdef DEBUG
        phLogComMessage(session->loggerId, LOG_VERBOSE, "openDevice(deviceName)");
#endif
    }
        else
        {
#ifdef DEBUG
            phLogComMessage(session->loggerId, LOG_VERBOSE,
                    "offline openDevice(deviceName)");
#endif
        }


    /* pretty print the action */
    phLogComMessage(session->loggerId, LOG_DEBUG,
        "%s Timestamp                Communication", rs232Handle->card);
    phLogComMessage(session->loggerId, LOG_DEBUG,
        "%s", rs232Handle->card);
    phLogComMessage(session->loggerId, LOG_DEBUG,
        "%s %s opened %s", rs232Handle->card, getTimeString(1), tmpString);

    int ctrlRtnValue = 0;
    if ((ctrlRtnValue = phRs232Ctrl(rs232Handle, RS232_FLOW_CONTROL, 0, session->rs232->flowControl)) != PHCOM_ERR_OK
            || (ctrlRtnValue = phRs232Ctrl(rs232Handle, RS232_PARITY, 0, session->rs232->parity)) != PHCOM_ERR_OK
            || (ctrlRtnValue = phRs232Ctrl(rs232Handle, RS232_STOP_BITS, session->rs232->stopBits, 0)) != PHCOM_ERR_OK
            || (ctrlRtnValue = phRs232Ctrl(rs232Handle, RS232_WIDTH, session->rs232->dataBits, 0)) != PHCOM_ERR_OK
            || (ctrlRtnValue = phRs232Ctrl(rs232Handle, RS232_BAUD, session->rs232->baudRate, 0)) != PHCOM_ERR_OK)
    {
        phLogComMessage(session->loggerId, PHLOG_TYPE_FATAL,
                "phComRs232Ctrl failed  %s, %s, %d, %d, %d", session->rs232->flowControl,
                session->rs232->parity, session->rs232->stopBits, session->rs232->dataBits,
                session->rs232->baudRate);
        rtnValue = (ctrlRtnValue == RS232_ERR_IO_TIMEOUT) ?
                        PHCOM_ERR_TIMEOUT : PHCOM_ERR_UNKNOWN;
        phComRs232Close(session);
        return rtnValue;
    }
    return rtnValue;
}

/*****************************************************************************
 *
 * Wait until device is ready for I/O or a timeout occurs
 *
 * Authors: Magco Li
 *
 * Description:
 * Wait until device is ready for I/O or a timeout occurs
 *
 ****************************************************************************/
static phComRs232Error_t rs232Wait(
    rs232Device rs232Handle,
    short events
)
{
    struct pollfd fd = { rs232Handle->handle, events, 0 };
    int timeout = rs232Handle->timeout ?
        rs232Handle->timeout : -1; //infinite timeout

    do
    {
        int result = poll(&fd, 1, timeout);

        switch (result)
        {
            case 0:
                return RS232_ERR_IO_TIMEOUT;
            case 1:
                if (fd.revents & (POLLERR | POLLHUP))
                {
                    return RS232_ERR_IO_ERROR;
                }
                if (fd.revents & POLLNVAL)
                {
                    return RS232_ERR_INVALID_ID;
                }
                return RS232_ERR_NO_ERROR;
            default:
                break;
        }
    } while (errno == EINTR);

    return mapErrno(errno);
}

/*****************************************************************************
 *
 * Read data from RS232 interface
 *
 * Authors: Magco Li
 *
 * Description:
 * Read data from rs232 interface
 *
 ****************************************************************************/

static phComRs232Error_t rs232Read(
    rs232Device rs232Handle,
    char *buffer,
    size_t len,
    int *pReasons,
    UINT32 *pActual
)
{
    size_t totalRead = 0;
    int reasons = 0;

    phComRs232Error_t error = RS232_ERR_NO_ERROR;
    do
    {
        if ((error = rs232Wait(rs232Handle, POLLIN)) == RS232_ERR_NO_ERROR)
        {
            ssize_t didRead = read(rs232Handle->handle, buffer + totalRead,
                len - totalRead);
            if (didRead < 0)
            {
                if (errno != EINTR)
                {
                    error = mapErrno(errno);
                }
            }
            else
            {
                totalRead += didRead;
                if (totalRead > 0)
                {
                    if (isEoi(rs232Handle, buffer[totalRead - 1]))
                    {
                        reasons |= RS232_REASON_CHR;
                    }
                    if (totalRead >= 2)
                    {
                        if ((buffer[totalRead-1] == 0xA && buffer[totalRead-2] == 0x0D)
                            || (buffer[totalRead-1] == 0xD && buffer[totalRead-2] == 0x0A))
                        {
                            reasons |= RS232_REASON_CHR;
                        }
                    }
                }
            }
        }
    } while (error == RS232_ERR_NO_ERROR && reasons == 0);

    if (pReasons)
    {
        *pReasons = reasons;
    }
    if (pActual)
    {
        *pActual = totalRead;
    }

    return error;
}

/*****************************************************************************
 *
 * Write data to RS232 interface
 *
 * Authors: Magco Li
 *
 * Description:
 * Write data to RS232 interface
 *
 ****************************************************************************/
static phComRs232Error_t rs232Write(
    rs232Device rs232Handle,
    char *buffer,
    int length,
    UINT32 *pActual
)
{
    phComRs232Error_t error = RS232_ERR_NO_ERROR;
    size_t totalWritten = 0;
    do
    {
        if ((error = rs232Wait(rs232Handle, POLLOUT)) == RS232_ERR_NO_ERROR)
        {
            ssize_t didWrite = write(rs232Handle->handle,
                buffer + totalWritten, length - totalWritten);
            if (didWrite < 0)
            {
                error = RS232_ERR_IO_ERROR;
            }
            else
            {
                totalWritten += didWrite;
            }
        }
    } while (error == RS232_ERR_NO_ERROR && length > totalWritten);

    if (pActual)
    {
        *pActual = totalWritten;
    }

    return error;
}

/*****************************************************************************
 *
 * Open a RS232 Session
 *
 * Author: Magco Li
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComRs232Open(
    phComId_t *newSession,
    phComMode_t mode,
    struct phComRs232Struct rs232Config,
    phLogId_t logger
)
{
    phComError_t rtnValue=PHCOM_ERR_OK;
    struct phComStruct *tmpId = NULL;

    phLogComMessage(logger, PHLOG_TYPE_TRACE,
		    "phComRs232Open(P%p, %d, \"%s\", P%p)",
		    newSession, (int) mode, rs232Config.device, logger);

    /* set session ID to NULL in case anything goes wrong */
    *newSession = NULL;

    /* allocate new communication data type */
    tmpId = PhNew(struct phComStruct);
    if (tmpId == NULL)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL,
                        "not enough memory during call to phComRs232Open()");

        return PHCOM_ERR_MEMORY;
    }
    tmpId->rs232 = PhNew(struct phComRs232Struct);
    if (tmpId->rs232 == NULL)
    {
        phLogComMessage(logger, PHLOG_TYPE_FATAL,
                        "not enough memory during call to phComRs232Open()1");

        RELEASE_BUFFER(tmpId);
        return PHCOM_ERR_MEMORY;
    }
    tmpId->gpib = NULL;
    tmpId->lan = NULL;

    /* initialize with default values */
    tmpId->myself = NULL;
    tmpId->mode = mode;
    tmpId->gioId = NULL;
    tmpId->loggerId = logger;
    tmpId->intfc = PHCOM_IFC_RS232;

    /* initialize RS232 specific data */    
    if (strlen(rs232Config.device) > 0)
    {
        strncpy(tmpId->rs232->device, rs232Config.device, SYMBOLIC_NAME_LENGTH);
        tmpId->rs232->device[SYMBOLIC_NAME_LENGTH-1] = '\0';
    }

    *(tmpId->rs232) = rs232Config;

    /* in off-line mode validate the handle and return, all done */
    if (mode == PHCOM_OFFLINE)
    {
        tmpId->myself = tmpId;
        *newSession  = tmpId;
        return rtnValue;
    }

    /* initialize some delays and timestamps */
    if (mode == PHCOM_SIMULATION ||
        getenv("PHCOM_PROBER_SIMULATION") ||
        getenv("PHCOM_SIMULATION"))
    {
        tmpId->mode =  PHCOM_ONLINE;
        tmpId->rs232->gSimulatorInUse = 1;
        gettimeofday(&tmpId->rs232->gLastSentTime, NULL);
        tmpId->rs232->gLastRecvTime = tmpId->rs232->gLastSentTime;
        phLogComMessage(logger, PHLOG_TYPE_WARNING,
            "phComRs232Open():     expecting simulator in use (delayed timing)");
    }
    else
    {
        tmpId->rs232->gSimulatorInUse = 0;
    }

    /* open the RS232 session to the interface */
    rtnValue = openRs232Session(tmpId);
    if (rtnValue != PHCOM_ERR_OK)
    {
        RELEASE_BUFFER(tmpId->rs232);
        RELEASE_BUFFER(tmpId);
        return rtnValue;
    }

    /* validate the handle */
    tmpId->myself = tmpId;
    *newSession  = tmpId;

    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComRs232Open
 *****************************************************************************/
phComError_t phComRs232Open(phComId_t *newSession, phComMode_t mode, struct phComRs232Struct rs232Config, phLogId_t logger)
{
    phComError_t rtnValue = _phComRs232Open(newSession, mode, rs232Config, logger);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Send a message to the RS232 interface
 *
 * Authors:  Magco Li
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComRs232Send(
    phComId_t session, 
    char *message, 
    int length, 
    long timeout 
)
{
    UINT32 act_msg; //actual write message length
    unsigned long timeout_ms;
    int writeRtnVal = 0; //return value of write message

    phComError_t rtnValue=PHCOM_ERR_OK;
  
    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
		    "phComRs232Send(P%p, \"%s\", %d, %ld)",
		    session, message ? message : "(NULL)", length, timeout);

    if (message == NULL || session->mode == PHCOM_OFFLINE)
    {
	return rtnValue;
    }

    timeout_ms = timeout/1000;   /* convert from microseconds to milliseconds*/
    if ((rtnValue = setTimeout(rs232Handle, timeout_ms))
       != PHCOM_ERR_OK)
    {
        return rtnValue;
    }

    switch (rs232Handle->mode)
    {
        case PHCOM_ONLINE:
        case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE,
            "phComRs232Send(\"%s\", %ld, %ld)", message, length, timeout);
#endif
            /* write string to RS232 interface */
            if ((writeRtnVal = rs232Write(rs232Handle, message, strlen(message), &act_msg)))
            {
                phLogComMessage(rs232Handle->loggerId,
                    writeRtnVal == RS232_ERR_IO_TIMEOUT ? LOG_DEBUG : PHLOG_TYPE_ERROR,
                    "rs232Write(\"%s\", %ld) failed:", message, strlen(message));
                rtnValue = (writeRtnVal == RS232_ERR_IO_TIMEOUT) ?
                    PHCOM_ERR_TIMEOUT : PHCOM_ERR_UNKNOWN;
            }
            break;
        case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE, "offline phComRs232Send()");
#endif
            act_msg = strlen(message);
            break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
        if (act_msg == strlen(message))
            phLogComMessage(rs232Handle->loggerId, LOG_DEBUG,
                "%s %s MSG send %s \"%s\"", rs232Handle->card,
                getTimeString(1), "--> ", message);
        else
            phLogComMessage(rs232Handle->loggerId, LOG_DEBUG,
                "%s %s MSG send %s \"%s\" INCOMPLETE, %d bytes", rs232Handle->card,
                getTimeString(1), "--> ", message, act_msg);
    }
    else
    {
        return rtnValue;
    }

    if (session->rs232->gSimulatorInUse)
    {
        gettimeofday(&session->rs232->gLastSentTime, NULL);
    }

    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComRs232Send
 *****************************************************************************/
phComError_t phComRs232Send(phComId_t session, char *message, int length, long timeout)
{
    phComError_t rtnValue = _phComRs232Send(session, message, length, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Receive a string over the RS232 bus  
 *
 * Authors:  Magco Li
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComRs232Receive( 
    phComId_t session, 
    const char **message, 
    int *length,
    long timeout 
)
{
    unsigned long timeout_ms;
    UINT32 read_nbr_bytes; // read message bytes
    int reason;
    int readRtnVal = 0; // return value of read message

    static char buf[PHCOM_MAX_MESSAGE_LENGTH+1] = "";
    phComError_t rtnValue=PHCOM_ERR_OK;

    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
        "phComRs232Receive(P%p, P%p, P%p, %ld)",
        session, message, length, timeout);

    if (session->mode == PHCOM_OFFLINE)
    {
        return rtnValue;
    }

    timeout_ms = timeout/1000;   /* convert from microseconds to milliseconds*/
    if ((rtnValue = setTimeout(rs232Handle, timeout_ms))
        != PHCOM_ERR_OK)
    {
        return rtnValue;
    }

    switch (rs232Handle->mode)
    {
      case PHCOM_ONLINE:
      case PHCOM_SIMULATION:
#ifdef DEBUG
        phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE,
            "phComRs232Receive(P%p, P%p, P%p, %ld)",
            session, message, length, timeout);
#endif

        /* read string from rs232 interface */
        buf[0] = '\0';
        if ((readRtnVal = rs232Read(rs232Handle, buf, sizeof(buf), &reason, &read_nbr_bytes)))
        {
            phLogComMessage(rs232Handle->loggerId,
                readRtnVal == RS232_ERR_IO_TIMEOUT ? LOG_DEBUG : PHLOG_TYPE_ERROR,
                "rs232Read(-> \"%s\", %ld, -> %d, -> %ld) failed:", buf, sizeof(buf),
                reason, read_nbr_bytes);
            rtnValue = (readRtnVal == RS232_ERR_IO_TIMEOUT) ?
                PHCOM_ERR_TIMEOUT : PHCOM_ERR_UNKNOWN;
        }
        else
        {
            buf[read_nbr_bytes] = '\0';
        }
        break;
      case PHCOM_OFFLINE:
#ifdef DEBUG
        phLogComMessage(rs232Handle->loggerId, LOG_VERBOSE, "offline phComRs232Receive(P%p, P%p, P%p, %ld)",
            session, message, length, timeout););
#endif
        if (strlen(buf) > 0)
        {
            strcpy(buf, "\n");
            read_nbr_bytes = 1;
        }
        else
        {
            read_nbr_bytes = 0;
        }
        reason = RS232_REASON_END;
        break;
    }

    /* pretty print the action */
    if (rtnValue == PHCOM_ERR_OK)
    {
        phLogComMessage(rs232Handle->loggerId, LOG_DEBUG,
            "%s %s MSG recv %s \"%s\"", rs232Handle->card,
            getTimeString(1), "<-- ", buf);

        buf[read_nbr_bytes] = '\0';
        *length = read_nbr_bytes;
        *message = buf;
    }
    else
    {
        return rtnValue;
    }

    if (session->rs232->gSimulatorInUse)
    {
        gettimeofday(&session->rs232->gLastRecvTime, NULL);
    }
   
    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComRs232Receive
 *****************************************************************************/
phComError_t phComRs232Receive(phComId_t session, const char **message, int *length, long timeout)
{
    phComError_t rtnValue = _phComRs232Receive(session, message, length, timeout);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 *
 * Close a RS232 session 
 *
 * Authors: Magco Li
 *
 * Description: 
 * Please refer to ph_mhcom.h
 *
 *****************************************************************************/

phComError_t _phComRs232Close(
    phComId_t session
)
{
    phComError_t rtnValue=PHCOM_ERR_OK;
    phComMode_t mode;

    CheckSession(session);

    phLogComMessage(session->loggerId, PHLOG_TYPE_TRACE,
        "phComRs232Close(P%p)", session);

    /* don't allow interrupts */
    mode = session->mode;

    /* don't free the memory: In case the session ID is abused
       from outside this module, we do prevent segmentation
       faults. This is not a big memory leak, since sessions are
       usually only opened once */

    if (session->mode == PHCOM_OFFLINE)
    {
        /* make phComId invalid */
        session->myself = NULL;
        /* free(session); */

        return rtnValue;
    }

    if ((rtnValue = closeRs232Session(rs232Handle)) == PHCOM_ERR_OK)
    {
        /* make phComId invalid */
        session->myself = NULL;
        /* free(session); */
    }

    return rtnValue;
}

/*****************************************************************************
 * wrapper of phComRs232Close
 *****************************************************************************/
phComError_t phComRs232Close(phComId_t session)
{
    phComError_t rtnValue = _phComRs232Close(session);
    handleReturnValue(rtnValue);
    return rtnValue;
}

/*****************************************************************************
 * End of file
 *****************************************************************************/

