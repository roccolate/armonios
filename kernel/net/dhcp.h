#ifndef ARMONIOS_KERNEL_NET_DHCP_H
#define ARMONIOS_KERNEL_NET_DHCP_H

#include <stdint.h>

#include "kernel/kernel_compiler.h"

#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

#define DHCP_PORT_SERVER 67
#define DHCP_PORT_CLIENT 68

#define DHCP_CHADDR_LEN  16
#define DHCP_SNAME_LEN   64
#define DHCP_FILE_LEN    128
#define DHCP_OPTIONS_LEN 312

typedef struct {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t  ciaddr[4];
    uint8_t  yiaddr[4];
    uint8_t  siaddr[4];
    uint8_t  giaddr[4];
    uint8_t  chaddr[DHCP_CHADDR_LEN];
    uint8_t  sname[DHCP_SNAME_LEN];
    uint8_t  file[DHCP_FILE_LEN];
    uint8_t  options[DHCP_OPTIONS_LEN];
} KERNEL_PACKED dhcp_packet_t;

typedef struct {
    uint8_t  mac[6];
    uint32_t ip;
    uint32_t subnet;
    uint32_t gateway;
    uint32_t dns;
    uint32_t dhcp_server;
    uint8_t  dhcp_state;
    uint8_t  discovered;
} net_info_t;

int net_init(void);
/* Drain all currently available receive frames and return the number drained. */
uint32_t net_poll(void);
int net_is_link_up(void);

#endif
