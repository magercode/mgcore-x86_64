#ifndef MGCORE_NET_H
#define MGCORE_NET_H

#include <stddef.h>

void net_init(void);
void net_startup_connect(void);
void net_handle_irq(void);
size_t net_adapter_count(void);
void net_dump_adapters(void);
void net_dump_driver_tools(void);
void net_scan_nearby(void);
void net_dump_nearby(void);
int net_connect_target(const char *target);
const char *net_connected_name(void);
int net_ping(const char *target, size_t count);

#endif
