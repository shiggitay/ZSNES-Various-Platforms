/* The PS3 build has no netplay (macos9/net_stub.c stands in for the
   transport), and PSL1GHT's own network headers define types that clash
   with the emulator's. Just the byte-order helpers, which are no-ops on the
   big-endian Cell. */
#ifndef ZSNES_PS3_ARPA_INET_H
#define ZSNES_PS3_ARPA_INET_H
#include <netinet/in.h>
#endif
