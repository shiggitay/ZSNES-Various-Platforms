/* Netplay transport for Mac OS 9: none yet. Open Transport would be the way;
   until then every call fails cleanly and the netplay menu says so. */
#include "../net/transport.h"

#include "macos9.h"

NetSocket net_open(int udp) { (void)udp; return NET_SOCKET_NONE; }
void net_close(NetSocket s) { (void)s; }
void net_adopt(NetSocket s, int udp) { (void)s; (void)udp; }
int net_bind_any(NetSocket s, uint16_t port) { (void)s; (void)port; return -1; }
int net_listen(NetSocket s, int backlog) { (void)s; (void)backlog; return -1; }
NetSocket net_accept(NetSocket s) { (void)s; return NET_SOCKET_NONE; }
int net_wait(NetSocket s, int w, int t) { (void)s; (void)w; (void)t; return -1; }
int net_wait_until(NetSocket s, int w, uint64_t d) { (void)s; (void)w; (void)d; return -1; }
int net_connect_pending_ok(NetSocket s) { (void)s; return 0; }
long net_send(NetSocket s, void const* b, size_t n) { (void)s; (void)b; (void)n; return -1; }
long net_recv(NetSocket s, void* b, size_t n) { (void)s; (void)b; (void)n; return -1; }
long net_recv_from(NetSocket s, void* b, size_t n, NetAddr* f) { (void)s; (void)b; (void)n; (void)f; return -1; }
long net_send_to(NetSocket s, void const* b, size_t n, NetAddr const* t) { (void)s; (void)b; (void)n; (void)t; return -1; }
int net_retry(void) { return 0; }
int net_connect_addr(NetSocket s, NetAddr const* a) { (void)s; (void)a; return -1; }
int net_send_all(NetSocket s, void const* b, size_t n, int t) { (void)s; (void)b; (void)n; (void)t; return 0; }
int net_recv_all(NetSocket s, void* b, size_t n, int t) { (void)s; (void)b; (void)n; (void)t; return 0; }
uint64_t net_now_ms(void) { return macos9_microseconds() / 1000u; }

uint32_t net_random_u32(void)
{
    static uint32_t x;
    uint64_t const t = macos9_microseconds();

    x ^= (uint32_t)t ^ (uint32_t)(t >> 32);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

int net_connect_start(char const* host, uint16_t port, int udp) { (void)host; (void)port; (void)udp; return -1; }
int net_connect_poll(NetSocket* out) { (void)out; return NET_CONNECT_FAILED; }
void net_connect_cancel(void) { }
void net_extip_start(void) { }
int net_extip_busy(void) { return 0; }
char const* net_extip(void) { return ""; }
