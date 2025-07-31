/* This file defines the data structures for PH communication with LAN */

#ifndef _PH_LAN_MHCOM_H_
#define _PH_LAN_MHCOM_H_

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include "ph_mhcom.h"

struct phComLanStruct
{
  int sockFD;
  int sockClientFD;
  char serverAddr[100]; /* either host ip or name (both ipv4 and ipv6 are allowed) */
  unsigned short serverPort;
  char connectionType[4]; /* tcp or udp */
  struct sockaddr sockAddr;
  unsigned int sockAddrLen;
  int epollFD;
};

#endif
