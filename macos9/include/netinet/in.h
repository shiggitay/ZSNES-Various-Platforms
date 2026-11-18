#ifndef MACOS9_NETINET_IN_H
#define MACOS9_NETINET_IN_H
#include <stdint.h>
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
struct in_addr { in_addr_t s_addr; };
#define htonl(x) ((uint32_t)(x))
#define htons(x) ((uint16_t)(x))
#define ntohl(x) ((uint32_t)(x))
#define ntohs(x) ((uint16_t)(x))
#endif
