/* Classic Mac OS has no BSD sockets; the netplay transport is a stub
   (macos9/net_stub.c). Just the byte-order helpers, which are no-ops on a
   big-endian machine. */
#ifndef MACOS9_ARPA_INET_H
#define MACOS9_ARPA_INET_H
#include <netinet/in.h>
#endif
