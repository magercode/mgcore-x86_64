#include "mgcore/net.h"

#include "mgcore/console.h"
#include "mgcore/io.h"
#include "mgcore/kstring.h"
#include "mgcore/sched.h"

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define NET_MAX_ADAPTERS 8
#define NET_MAX_ARP_CACHE 4

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

#define RTL_REG_IDR0 0x00
#define RTL_REG_MAR0 0x08
#define RTL_REG_TSD0 0x10
#define RTL_REG_TSAD0 0x20
#define RTL_REG_RBSTART 0x30
#define RTL_REG_CR 0x37
#define RTL_REG_CAPR 0x38
#define RTL_REG_CBR 0x3A
#define RTL_REG_IMR 0x3C
#define RTL_REG_ISR 0x3E
#define RTL_REG_TCR 0x40
#define RTL_REG_RCR 0x44
#define RTL_REG_CONFIG1 0x52

#define RTL_CR_BUFE 0x01
#define RTL_CR_TE 0x04
#define RTL_CR_RE 0x08
#define RTL_CR_RST 0x10

#define RTL_ISR_ROK 0x0001
#define RTL_ISR_RER 0x0002
#define RTL_ISR_TOK 0x0004
#define RTL_ISR_TER 0x0008
#define RTL_ISR_RXOVW 0x0010
#define RTL_ISR_FIFOOVW 0x0040
#define RTL_ISR_LENCHG 0x2000
#define RTL_ISR_SERR 0x8000

#define RTL_RX_BUFFER_SIZE (8192 + 16 + 1500)
#define RTL_TX_BUFFER_SIZE 2048
#define RTL_TX_DESC_COUNT 4

#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IPV4 0x0800
#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4 0x0800
#define ARP_OPER_REQUEST 1
#define ARP_OPER_REPLY 2
#define IP_PROTO_ICMP 1
#define IP_PROTO_UDP 17

#define UDP_PORT_DHCP_SERVER 67
#define UDP_PORT_DHCP_CLIENT 68

#define DHCP_OP_REQUEST 1
#define DHCP_OP_REPLY 2
#define DHCP_HTYPE_ETHERNET 1
#define DHCP_MAGIC_COOKIE 0x63825363U

#define DHCP_OPTION_MSG_TYPE 53
#define DHCP_OPTION_REQ_IP 50
#define DHCP_OPTION_SERVER_ID 54
#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_PARAM_REQ_LIST 55
#define DHCP_OPTION_END 255

#define DHCP_MSG_DISCOVER 1
#define DHCP_MSG_OFFER 2
#define DHCP_MSG_REQUEST 3
#define DHCP_MSG_ACK 5

#define NET_IPV4_ANY 0x00000000U
#define NET_IPV4_BROADCAST 0xFFFFFFFFU

#define NET_DEFAULT_IP 0x0A00020FU   /* 10.0.2.15 */
#define NET_DEFAULT_GW 0x0A000202U   /* 10.0.2.2 */
#define NET_DEFAULT_MASK 0xFFFFFF00U /* 255.255.255.0 */

struct net_adapter {
  uint8_t bus;
  uint8_t slot;
  uint8_t func;
  uint16_t vendor_id;
  uint16_t device_id;
  uint32_t bar0;
  uint8_t irq_line;
  int bound;
};

struct arp_entry {
  uint32_t ip;
  uint8_t mac[6];
  int valid;
  uint64_t updated_tick;
};

struct ping_wait {
  int active;
  uint16_t identifier;
  uint16_t sequence;
  uint32_t target_ip;
  int received;
  uint8_t reply_ttl;
  uint32_t reply_ms;
};

static struct net_adapter g_adapters[NET_MAX_ADAPTERS];
static size_t g_adapter_count;

static uint16_t g_rtl_io_base;
static uint8_t g_rtl_irq_line;
static int g_rtl_ready;
static uint8_t g_mac[6];

static uint8_t g_rx_buffer[RTL_RX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t g_tx_buffer[RTL_TX_DESC_COUNT][RTL_TX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint16_t g_rx_read_offset;
static uint8_t g_tx_index;

static uint32_t g_ip_addr;
static uint32_t g_ip_gw;
static uint32_t g_ip_mask;
static int g_connected;

static uint64_t g_rx_frames;
static uint64_t g_tx_frames;
static uint64_t g_rx_dropped;

static struct arp_entry g_arp_cache[NET_MAX_ARP_CACHE];
static struct ping_wait g_ping_wait;

struct dhcp_lease {
  int active;
  uint32_t xid;
  uint32_t offered_ip;
  uint32_t server_ip;
  uint32_t subnet_mask;
  uint32_t router;
  int got_offer;
  int got_ack;
};

static struct dhcp_lease g_dhcp;

static uint16_t be16_load(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static uint32_t be32_load(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void be16_store(uint8_t *p, uint16_t value) {
  p[0] = (uint8_t)((value >> 8) & 0xFF);
  p[1] = (uint8_t)(value & 0xFF);
}

static void be32_store(uint8_t *p, uint32_t value) {
  p[0] = (uint8_t)((value >> 24) & 0xFF);
  p[1] = (uint8_t)((value >> 16) & 0xFF);
  p[2] = (uint8_t)((value >> 8) & 0xFF);
  p[3] = (uint8_t)(value & 0xFF);
}

static uint16_t checksum16(const uint8_t *data, size_t length) {
  uint32_t sum = 0;
  size_t i;
  for (i = 0; i + 1 < length; i += 2) {
    sum += (uint32_t)((data[i] << 8) | data[i + 1]);
  }
  if (length & 1U) {
    sum += (uint32_t)(data[length - 1] << 8);
  }
  while ((sum >> 16) != 0) {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  return (uint16_t)(~sum);
}

static void format_ip(uint32_t ip, char *out, size_t out_size) {
  uint8_t a = (uint8_t)((ip >> 24) & 0xFF);
  uint8_t b = (uint8_t)((ip >> 16) & 0xFF);
  uint8_t c = (uint8_t)((ip >> 8) & 0xFF);
  uint8_t d = (uint8_t)(ip & 0xFF);
  ksnprintf(out, out_size, "%u.%u.%u.%u", (unsigned)a, (unsigned)b, (unsigned)c, (unsigned)d);
}

static int parse_ipv4(const char *text, uint32_t *out_ip) {
  uint32_t value = 0;
  uint32_t octet = 0;
  unsigned part = 0;
  const char *p = text;

  if (!text || !*text || !out_ip) {
    return 0;
  }

  while (*p) {
    char c = *p++;
    if (c >= '0' && c <= '9') {
      octet = octet * 10U + (uint32_t)(c - '0');
      if (octet > 255U) {
        return 0;
      }
      continue;
    }
    if (c == '.') {
      if (part >= 3) {
        return 0;
      }
      value = (value << 8) | octet;
      octet = 0;
      ++part;
      continue;
    }
    return 0;
  }

  if (part != 3) {
    return 0;
  }

  value = (value << 8) | octet;
  *out_ip = value;
  return 1;
}

static uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
  uint32_t address = (1u << 31)
                   | ((uint32_t)bus << 16)
                   | ((uint32_t)slot << 11)
                   | ((uint32_t)func << 8)
                   | (offset & 0xFCu);
  io_out32(PCI_CONFIG_ADDRESS, address);
  return io_in32(PCI_CONFIG_DATA);
}

static void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
  uint32_t address = (1u << 31)
                   | ((uint32_t)bus << 16)
                   | ((uint32_t)slot << 11)
                   | ((uint32_t)func << 8)
                   | (offset & 0xFCu);
  io_out32(PCI_CONFIG_ADDRESS, address);
  io_out32(PCI_CONFIG_DATA, value);
}

static int rtl_out_of_bounds_tx(size_t length) {
  return length > RTL_TX_BUFFER_SIZE;
}

static int rtl_send_raw_frame(const uint8_t *frame, size_t length) {
  uint32_t tsd;
  uint8_t *slot;
  size_t send_length = length;
  int spin;

  if (!g_rtl_ready || !frame || length == 0 || rtl_out_of_bounds_tx(length)) {
    return -1;
  }

  if (send_length < 60U) {
    send_length = 60U;
  }

  slot = g_tx_buffer[g_tx_index];
  kmemset(slot, 0, send_length);
  kmemcpy(slot, frame, length);

  io_out32((uint16_t)(g_rtl_io_base + RTL_REG_TSAD0 + (uint16_t)(g_tx_index * 4U)),
           (uint32_t)(uintptr_t)slot);
  io_out32((uint16_t)(g_rtl_io_base + RTL_REG_TSD0 + (uint16_t)(g_tx_index * 4U)),
           (uint32_t)send_length);

  for (spin = 0; spin < 100000; ++spin) {
    tsd = io_in32((uint16_t)(g_rtl_io_base + RTL_REG_TSD0 + (uint16_t)(g_tx_index * 4U)));
    if ((tsd & (1u << 15)) != 0u) {
      break;
    }
  }

  g_tx_index = (uint8_t)((g_tx_index + 1U) % RTL_TX_DESC_COUNT);
  ++g_tx_frames;
  return 0;
}

static void arp_cache_update(uint32_t ip, const uint8_t mac[6]) {
  size_t i;
  size_t free_slot = NET_MAX_ARP_CACHE;

  for (i = 0; i < NET_MAX_ARP_CACHE; ++i) {
    if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
      kmemcpy(g_arp_cache[i].mac, mac, 6);
      g_arp_cache[i].updated_tick = scheduler_uptime_ticks();
      return;
    }
    if (!g_arp_cache[i].valid && free_slot == NET_MAX_ARP_CACHE) {
      free_slot = i;
    }
  }

  if (free_slot == NET_MAX_ARP_CACHE) {
    free_slot = 0;
  }

  g_arp_cache[free_slot].valid = 1;
  g_arp_cache[free_slot].ip = ip;
  kmemcpy(g_arp_cache[free_slot].mac, mac, 6);
  g_arp_cache[free_slot].updated_tick = scheduler_uptime_ticks();
}

static int arp_cache_lookup(uint32_t ip, uint8_t out_mac[6]) {
  size_t i;
  for (i = 0; i < NET_MAX_ARP_CACHE; ++i) {
    if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
      kmemcpy(out_mac, g_arp_cache[i].mac, 6);
      return 1;
    }
  }
  return 0;
}

static int arp_send_request(uint32_t target_ip) {
  uint8_t frame[42];

  kmemset(frame, 0, sizeof(frame));
  kmemset(frame, 0xFF, 6);
  kmemcpy(frame + 6, g_mac, 6);
  be16_store(frame + 12, ETH_TYPE_ARP);

  be16_store(frame + 14, ARP_HTYPE_ETHERNET);
  be16_store(frame + 16, ARP_PTYPE_IPV4);
  frame[18] = 6;
  frame[19] = 4;
  be16_store(frame + 20, ARP_OPER_REQUEST);
  kmemcpy(frame + 22, g_mac, 6);
  be32_store(frame + 28, g_ip_addr);
  kmemset(frame + 32, 0, 6);
  be32_store(frame + 38, target_ip);

  return rtl_send_raw_frame(frame, sizeof(frame));
}

static int arp_send_reply(const uint8_t target_mac[6], uint32_t target_ip) {
  uint8_t frame[42];

  kmemset(frame, 0, sizeof(frame));
  kmemcpy(frame, target_mac, 6);
  kmemcpy(frame + 6, g_mac, 6);
  be16_store(frame + 12, ETH_TYPE_ARP);

  be16_store(frame + 14, ARP_HTYPE_ETHERNET);
  be16_store(frame + 16, ARP_PTYPE_IPV4);
  frame[18] = 6;
  frame[19] = 4;
  be16_store(frame + 20, ARP_OPER_REPLY);
  kmemcpy(frame + 22, g_mac, 6);
  be32_store(frame + 28, g_ip_addr);
  kmemcpy(frame + 32, target_mac, 6);
  be32_store(frame + 38, target_ip);

  return rtl_send_raw_frame(frame, sizeof(frame));
}

static int ip_send_icmp_echo(uint32_t target_ip, const uint8_t dst_mac[6], uint16_t identifier, uint16_t sequence) {
  uint8_t frame[14 + 20 + 32];
  uint8_t *ip = frame + 14;
  uint8_t *icmp = ip + 20;
  size_t i;

  kmemset(frame, 0, sizeof(frame));
  kmemcpy(frame, dst_mac, 6);
  kmemcpy(frame + 6, g_mac, 6);
  be16_store(frame + 12, ETH_TYPE_IPV4);

  ip[0] = 0x45;
  ip[1] = 0;
  be16_store(ip + 2, (uint16_t)(20 + 32));
  be16_store(ip + 4, sequence);
  be16_store(ip + 6, 0);
  ip[8] = 64;
  ip[9] = IP_PROTO_ICMP;
  be32_store(ip + 12, g_ip_addr);
  be32_store(ip + 16, target_ip);
  be16_store(ip + 10, checksum16(ip, 20));

  icmp[0] = 8;
  icmp[1] = 0;
  be16_store(icmp + 4, identifier);
  be16_store(icmp + 6, sequence);
  for (i = 8; i < 32; ++i) {
    icmp[i] = (uint8_t)i;
  }
  be16_store(icmp + 2, checksum16(icmp, 32));

  return rtl_send_raw_frame(frame, sizeof(frame));
}

static int ip_send_udp(uint32_t src_ip,
                       uint32_t dst_ip,
                       uint16_t src_port,
                       uint16_t dst_port,
                       const uint8_t *payload,
                       size_t payload_length,
                       const uint8_t dst_mac[6]) {
  uint8_t frame[14 + 20 + 8 + 548];
  uint8_t *ip = frame + 14;
  uint8_t *udp = ip + 20;
  size_t total_payload = 8U + payload_length;
  size_t frame_len;

  if (payload_length > 548U) {
    return -1;
  }

  frame_len = 14U + 20U + total_payload;
  kmemset(frame, 0, frame_len);
  kmemcpy(frame, dst_mac, 6);
  kmemcpy(frame + 6, g_mac, 6);
  be16_store(frame + 12, ETH_TYPE_IPV4);

  ip[0] = 0x45;
  ip[1] = 0;
  be16_store(ip + 2, (uint16_t)(20U + total_payload));
  be16_store(ip + 4, (uint16_t)(scheduler_uptime_ticks() & 0xFFFFU));
  be16_store(ip + 6, 0);
  ip[8] = 64;
  ip[9] = IP_PROTO_UDP;
  be32_store(ip + 12, src_ip);
  be32_store(ip + 16, dst_ip);
  be16_store(ip + 10, checksum16(ip, 20));

  be16_store(udp + 0, src_port);
  be16_store(udp + 2, dst_port);
  be16_store(udp + 4, (uint16_t)total_payload);
  be16_store(udp + 6, 0); /* checksum optional for IPv4 */
  kmemcpy(udp + 8, payload, payload_length);

  return rtl_send_raw_frame(frame, frame_len);
}

static void dhcp_parse_options(const uint8_t *options, size_t length, uint8_t *msg_type) {
  size_t i = 0;
  *msg_type = 0;

  while (i < length) {
    uint8_t code = options[i++];
    uint8_t opt_len;
    if (code == DHCP_OPTION_END) {
      break;
    }
    if (code == 0) {
      continue;
    }
    if (i >= length) {
      break;
    }
    opt_len = options[i++];
    if (i + opt_len > length) {
      break;
    }
    if (code == DHCP_OPTION_MSG_TYPE && opt_len == 1) {
      *msg_type = options[i];
    } else if (code == DHCP_OPTION_SERVER_ID && opt_len == 4) {
      g_dhcp.server_ip = be32_load(options + i);
    } else if (code == DHCP_OPTION_SUBNET_MASK && opt_len == 4) {
      g_dhcp.subnet_mask = be32_load(options + i);
    } else if (code == DHCP_OPTION_ROUTER && opt_len >= 4) {
      g_dhcp.router = be32_load(options + i);
    }
    i += opt_len;
  }
}

static void dhcp_process_packet(const uint8_t *payload, size_t length) {
  uint8_t msg_type;
  uint32_t xid;
  uint32_t yiaddr;

  if (!g_dhcp.active || length < 240U) {
    return;
  }
  if (payload[0] != DHCP_OP_REPLY || payload[1] != DHCP_HTYPE_ETHERNET || payload[2] != 6) {
    return;
  }

  xid = be32_load(payload + 4);
  if (xid != g_dhcp.xid) {
    return;
  }
  if (kmemcmp(payload + 28, g_mac, 6) != 0) {
    return;
  }
  if (be32_load(payload + 236) != DHCP_MAGIC_COOKIE) {
    return;
  }

  yiaddr = be32_load(payload + 16);
  dhcp_parse_options(payload + 240, length - 240, &msg_type);
  if (msg_type == DHCP_MSG_OFFER) {
    g_dhcp.offered_ip = yiaddr;
    if (g_dhcp.subnet_mask == 0) {
      g_dhcp.subnet_mask = NET_DEFAULT_MASK;
    }
    if (g_dhcp.router == 0) {
      g_dhcp.router = NET_DEFAULT_GW;
    }
    g_dhcp.got_offer = 1;
  } else if (msg_type == DHCP_MSG_ACK) {
    g_dhcp.offered_ip = yiaddr;
    if (g_dhcp.subnet_mask == 0) {
      g_dhcp.subnet_mask = NET_DEFAULT_MASK;
    }
    if (g_dhcp.router == 0) {
      g_dhcp.router = NET_DEFAULT_GW;
    }
    g_dhcp.got_ack = 1;
  }
}

static int dhcp_send_discover(void) {
  uint8_t payload[300];
  size_t o = 240;
  uint8_t bcast[6];

  kmemset(payload, 0, sizeof(payload));
  payload[0] = DHCP_OP_REQUEST;
  payload[1] = DHCP_HTYPE_ETHERNET;
  payload[2] = 6;
  be32_store(payload + 4, g_dhcp.xid);
  be16_store(payload + 10, 0x8000);
  kmemcpy(payload + 28, g_mac, 6);
  be32_store(payload + 236, DHCP_MAGIC_COOKIE);
  payload[o++] = DHCP_OPTION_MSG_TYPE;
  payload[o++] = 1;
  payload[o++] = DHCP_MSG_DISCOVER;
  payload[o++] = DHCP_OPTION_PARAM_REQ_LIST;
  payload[o++] = 3;
  payload[o++] = DHCP_OPTION_SUBNET_MASK;
  payload[o++] = DHCP_OPTION_ROUTER;
  payload[o++] = DHCP_OPTION_SERVER_ID;
  payload[o++] = DHCP_OPTION_END;
  kmemset(bcast, 0xFF, sizeof(bcast));
  return ip_send_udp(NET_IPV4_ANY,
                     NET_IPV4_BROADCAST,
                     UDP_PORT_DHCP_CLIENT,
                     UDP_PORT_DHCP_SERVER,
                     payload,
                     o,
                     bcast);
}

static int dhcp_send_request(void) {
  uint8_t payload[300];
  size_t o = 240;
  uint8_t bcast[6];

  if (!g_dhcp.got_offer || g_dhcp.server_ip == 0 || g_dhcp.offered_ip == 0) {
    return -1;
  }

  kmemset(payload, 0, sizeof(payload));
  payload[0] = DHCP_OP_REQUEST;
  payload[1] = DHCP_HTYPE_ETHERNET;
  payload[2] = 6;
  be32_store(payload + 4, g_dhcp.xid);
  be16_store(payload + 10, 0x8000);
  kmemcpy(payload + 28, g_mac, 6);
  be32_store(payload + 236, DHCP_MAGIC_COOKIE);
  payload[o++] = DHCP_OPTION_MSG_TYPE;
  payload[o++] = 1;
  payload[o++] = DHCP_MSG_REQUEST;
  payload[o++] = DHCP_OPTION_REQ_IP;
  payload[o++] = 4;
  be32_store(payload + o, g_dhcp.offered_ip);
  o += 4;
  payload[o++] = DHCP_OPTION_SERVER_ID;
  payload[o++] = 4;
  be32_store(payload + o, g_dhcp.server_ip);
  o += 4;
  payload[o++] = DHCP_OPTION_END;
  kmemset(bcast, 0xFF, sizeof(bcast));
  return ip_send_udp(NET_IPV4_ANY,
                     NET_IPV4_BROADCAST,
                     UDP_PORT_DHCP_CLIENT,
                     UDP_PORT_DHCP_SERVER,
                     payload,
                     o,
                     bcast);
}

static uint32_t route_next_hop(uint32_t dst_ip) {
  if ((dst_ip & g_ip_mask) == (g_ip_addr & g_ip_mask)) {
    return dst_ip;
  }
  return g_ip_gw;
}

static void net_process_arp(const uint8_t *payload, size_t length) {
  uint16_t htype;
  uint16_t ptype;
  uint16_t oper;
  uint8_t sha[6];
  uint32_t sip;
  uint32_t tip;

  if (length < 28) {
    return;
  }

  htype = be16_load(payload + 0);
  ptype = be16_load(payload + 2);
  oper = be16_load(payload + 6);
  if (htype != ARP_HTYPE_ETHERNET || ptype != ARP_PTYPE_IPV4 || payload[4] != 6 || payload[5] != 4) {
    return;
  }

  kmemcpy(sha, payload + 8, 6);
  sip = be32_load(payload + 14);
  tip = be32_load(payload + 24);

  arp_cache_update(sip, sha);

  if (oper == ARP_OPER_REQUEST && tip == g_ip_addr) {
    arp_send_reply(sha, sip);
  }
}

static void net_process_icmp(const uint8_t *ip, size_t ip_length) {
  uint8_t ihl = (uint8_t)((ip[0] & 0x0F) * 4);
  const uint8_t *icmp;
  size_t icmp_length;
  uint8_t type;
  uint16_t ident;
  uint16_t seq;

  if (ip_length < 20 || ihl < 20 || ip_length < ihl + 8) {
    return;
  }

  icmp = ip + ihl;
  icmp_length = ip_length - ihl;
  if (checksum16(icmp, icmp_length) != 0) {
    return;
  }

  type = icmp[0];
  ident = be16_load(icmp + 4);
  seq = be16_load(icmp + 6);

  if (type == 0 && g_ping_wait.active) {
    if (g_ping_wait.identifier == ident && g_ping_wait.sequence == seq) {
      g_ping_wait.received = 1;
      g_ping_wait.reply_ttl = ip[8];
      g_ping_wait.reply_ms = (uint32_t)scheduler_uptime_ms();
    }
  }
}

static void net_process_udp(const uint8_t *ip, size_t ip_length) {
  uint8_t ihl = (uint8_t)((ip[0] & 0x0F) * 4);
  const uint8_t *udp;
  uint16_t udp_len;
  uint16_t src_port;
  uint16_t dst_port;

  if (ip_length < 20 || ihl < 20 || ip_length < ihl + 8) {
    return;
  }

  udp = ip + ihl;
  udp_len = be16_load(udp + 4);
  if (udp_len < 8 || ip_length < ihl + udp_len) {
    return;
  }

  src_port = be16_load(udp + 0);
  dst_port = be16_load(udp + 2);
  if (src_port == UDP_PORT_DHCP_SERVER && dst_port == UDP_PORT_DHCP_CLIENT) {
    dhcp_process_packet(udp + 8, (size_t)(udp_len - 8));
  }
}

static void net_process_ipv4(const uint8_t *payload, size_t length) {
  uint8_t version;
  uint8_t ihl;
  uint16_t total_length;
  uint16_t flags_fragment;
  uint8_t protocol;
  uint32_t dst_ip;

  if (length < 20) {
    return;
  }

  version = (uint8_t)((payload[0] >> 4) & 0x0F);
  ihl = (uint8_t)((payload[0] & 0x0F) * 4);
  if (version != 4 || ihl < 20 || length < ihl) {
    return;
  }

  total_length = be16_load(payload + 2);
  if (total_length < ihl || total_length > length) {
    return;
  }

  if (checksum16(payload, ihl) != 0) {
    return;
  }

  flags_fragment = be16_load(payload + 6);
  if ((flags_fragment & 0x3FFFU) != 0) {
    return;
  }

  dst_ip = be32_load(payload + 16);
  protocol = payload[9];
  if (protocol == IP_PROTO_UDP) {
    if (!(dst_ip == g_ip_addr || dst_ip == NET_IPV4_BROADCAST || g_ip_addr == NET_IPV4_ANY)) {
      return;
    }
  } else if (dst_ip != g_ip_addr) {
    return;
  }

  if (protocol == IP_PROTO_ICMP) {
    net_process_icmp(payload, total_length);
  } else if (protocol == IP_PROTO_UDP) {
    net_process_udp(payload, total_length);
  }
}

static void net_process_frame(const uint8_t *frame, size_t length) {
  uint16_t eth_type;
  if (length < 14) {
    return;
  }

  eth_type = be16_load(frame + 12);
  if (eth_type == ETH_TYPE_ARP) {
    net_process_arp(frame + 14, length - 14);
    return;
  }
  if (eth_type == ETH_TYPE_IPV4) {
    net_process_ipv4(frame + 14, length - 14);
  }
}

static void rtl_poll_rx(void) {
  int processed = 0;

  if (!g_rtl_ready) {
    return;
  }

  while ((io_in8((uint16_t)(g_rtl_io_base + RTL_REG_CR)) & RTL_CR_BUFE) == 0) {
    uint8_t *packet = &g_rx_buffer[g_rx_read_offset];
    uint16_t status = (uint16_t)(packet[0] | (uint16_t)(packet[1] << 8));
    uint16_t length = (uint16_t)(packet[2] | (uint16_t)(packet[3] << 8));
    size_t frame_length;

    if (length < 4 || length > 1792) {
      ++g_rx_dropped;
      break;
    }

    frame_length = (size_t)(length - 4U);
    if ((status & 0x0001U) != 0) {
      net_process_frame(packet + 4, frame_length);
      ++g_rx_frames;
    } else {
      ++g_rx_dropped;
    }

    g_rx_read_offset = (uint16_t)((g_rx_read_offset + length + 4U + 3U) & ~3U);
    g_rx_read_offset = (uint16_t)(g_rx_read_offset % 8192U);
    io_out16((uint16_t)(g_rtl_io_base + RTL_REG_CAPR), (uint16_t)((g_rx_read_offset - 16U) & 0x1FFFU));

    ++processed;
    if (processed > 64) {
      break;
    }
  }
}

static int rtl_wait_for_reset_clear(void) {
  int spin;
  for (spin = 0; spin < 1000000; ++spin) {
    if ((io_in8((uint16_t)(g_rtl_io_base + RTL_REG_CR)) & RTL_CR_RST) == 0) {
      return 1;
    }
  }
  return 0;
}

static int rtl_hw_init(struct net_adapter *adapter) {
  uint32_t command;
  size_t i;

  g_rtl_io_base = (uint16_t)(adapter->bar0 & ~0x3U);
  g_rtl_irq_line = adapter->irq_line;

  if ((adapter->bar0 & 0x1U) == 0U) {
    return 0;
  }

  command = pci_config_read32(adapter->bus, adapter->slot, adapter->func, 0x04);
  command |= 0x00000005U; /* I/O space + bus master. */
  pci_config_write32(adapter->bus, adapter->slot, adapter->func, 0x04, command);

  io_out8((uint16_t)(g_rtl_io_base + RTL_REG_CONFIG1), 0x00);
  io_out8((uint16_t)(g_rtl_io_base + RTL_REG_CR), RTL_CR_RST);
  if (!rtl_wait_for_reset_clear()) {
    return 0;
  }

  io_out32((uint16_t)(g_rtl_io_base + RTL_REG_RBSTART), (uint32_t)(uintptr_t)g_rx_buffer);
  io_out16((uint16_t)(g_rtl_io_base + RTL_REG_IMR),
           (uint16_t)(RTL_ISR_ROK | RTL_ISR_RER | RTL_ISR_TOK | RTL_ISR_TER |
                      RTL_ISR_RXOVW | RTL_ISR_FIFOOVW | RTL_ISR_LENCHG | RTL_ISR_SERR));
  io_out16((uint16_t)(g_rtl_io_base + RTL_REG_ISR), 0xFFFFU);

  io_out32((uint16_t)(g_rtl_io_base + RTL_REG_RCR),
           (uint32_t)((1U << 7) | (1U << 3) | (1U << 2) | (1U << 1) | (1U << 0) | (7U << 13)));
  io_out32((uint16_t)(g_rtl_io_base + RTL_REG_TCR), 0x03000700U);

  g_rx_read_offset = 0;
  io_out16((uint16_t)(g_rtl_io_base + RTL_REG_CAPR), 0xFFF0U);

  io_out8((uint16_t)(g_rtl_io_base + RTL_REG_CR), (uint8_t)(RTL_CR_RE | RTL_CR_TE));

  for (i = 0; i < 6; ++i) {
    g_mac[i] = io_in8((uint16_t)(g_rtl_io_base + RTL_REG_IDR0 + (uint16_t)i));
  }

  g_tx_index = 0;
  g_rtl_ready = 1;
  return 1;
}

static void net_wait_event_tick(void) {
  __asm__ volatile ("hlt");
}

static int net_resolve_arp(uint32_t target_ip, uint8_t out_mac[6], uint64_t timeout_ticks) {
  uint64_t start = scheduler_uptime_ticks();
  uint64_t next_probe = start;

  if (arp_cache_lookup(target_ip, out_mac)) {
    return 1;
  }

  while (scheduler_uptime_ticks() - start < timeout_ticks) {
    uint64_t now = scheduler_uptime_ticks();
    if (now >= next_probe) {
      arp_send_request(target_ip);
      next_probe = now + (MGCORE_HZ / 2U);
    }

    rtl_poll_rx();
    if (arp_cache_lookup(target_ip, out_mac)) {
      return 1;
    }

    net_wait_event_tick();
  }

  return 0;
}

static int net_dhcp_acquire(uint64_t timeout_ticks) {
  uint64_t start;

  if (!g_rtl_ready) {
    return 0;
  }

  kmemset(&g_dhcp, 0, sizeof(g_dhcp));
  g_dhcp.active = 1;
  g_dhcp.xid = (uint32_t)((scheduler_uptime_ticks() << 16) ^ 0x4D474443U);

  if (dhcp_send_discover() != 0) {
    g_dhcp.active = 0;
    return 0;
  }

  start = scheduler_uptime_ticks();
  while (scheduler_uptime_ticks() - start < timeout_ticks) {
    rtl_poll_rx();
    if (g_dhcp.got_offer) {
      break;
    }
    net_wait_event_tick();
  }
  if (!g_dhcp.got_offer) {
    g_dhcp.active = 0;
    return 0;
  }

  if (dhcp_send_request() != 0) {
    g_dhcp.active = 0;
    return 0;
  }

  start = scheduler_uptime_ticks();
  while (scheduler_uptime_ticks() - start < timeout_ticks) {
    rtl_poll_rx();
    if (g_dhcp.got_ack) {
      g_ip_addr = g_dhcp.offered_ip;
      g_ip_mask = g_dhcp.subnet_mask ? g_dhcp.subnet_mask : NET_DEFAULT_MASK;
      g_ip_gw = g_dhcp.router ? g_dhcp.router : NET_DEFAULT_GW;
      g_dhcp.active = 0;
      return 1;
    }
    net_wait_event_tick();
  }

  g_dhcp.active = 0;
  return 0;
}

void net_init(void) {
  uint16_t bus;
  uint8_t slot;

  g_adapter_count = 0;
  g_rtl_ready = 0;
  g_connected = 0;
  g_ip_addr = NET_IPV4_ANY;
  g_ip_gw = NET_DEFAULT_GW;
  g_ip_mask = NET_DEFAULT_MASK;
  g_rx_frames = 0;
  g_tx_frames = 0;
  g_rx_dropped = 0;
  kmemset(g_arp_cache, 0, sizeof(g_arp_cache));
  kmemset(&g_ping_wait, 0, sizeof(g_ping_wait));
  kmemset(&g_dhcp, 0, sizeof(g_dhcp));

  for (bus = 0; bus < 256; ++bus) {
    for (slot = 0; slot < 32; ++slot) {
      uint8_t func;
      uint8_t header;
      uint8_t max_func;
      uint16_t vendor0 = (uint16_t)(pci_config_read32((uint8_t)bus, slot, 0, 0x00) & 0xFFFFU);

      if (vendor0 == 0xFFFFU) {
        continue;
      }

      header = (uint8_t)((pci_config_read32((uint8_t)bus, slot, 0, 0x0C) >> 16) & 0xFFU);
      max_func = (header & 0x80U) ? 8U : 1U;

      for (func = 0; func < max_func; ++func) {
        uint32_t id = pci_config_read32((uint8_t)bus, slot, func, 0x00);
        uint16_t vendor = (uint16_t)(id & 0xFFFFU);
        uint16_t device = (uint16_t)((id >> 16) & 0xFFFFU);
        uint32_t class_reg;
        uint8_t class_code;
        uint8_t subclass;

        if (vendor == 0xFFFFU) {
          continue;
        }

        class_reg = pci_config_read32((uint8_t)bus, slot, func, 0x08);
        class_code = (uint8_t)((class_reg >> 24) & 0xFFU);
        subclass = (uint8_t)((class_reg >> 16) & 0xFFU);
        if (class_code != 0x02U || subclass != 0x00U) {
          continue;
        }

        if (g_adapter_count < NET_MAX_ADAPTERS) {
          struct net_adapter *adapter = &g_adapters[g_adapter_count++];
          uint32_t irq_reg;
          adapter->bus = (uint8_t)bus;
          adapter->slot = slot;
          adapter->func = func;
          adapter->vendor_id = vendor;
          adapter->device_id = device;
          adapter->bar0 = pci_config_read32((uint8_t)bus, slot, func, 0x10);
          irq_reg = pci_config_read32((uint8_t)bus, slot, func, 0x3C);
          adapter->irq_line = (uint8_t)(irq_reg & 0xFFU);
          adapter->bound = 0;

          if (!g_rtl_ready && vendor == RTL8139_VENDOR_ID && device == RTL8139_DEVICE_ID) {
            if (rtl_hw_init(adapter)) {
              adapter->bound = 1;
            }
          }
        }
      }
    }
  }

  console_printf("net: ethernet adapters detected=%u\n", (unsigned)g_adapter_count);
  if (g_rtl_ready) {
    console_printf("net0: rtl8139 ready io=0x%x irq=%u mac=%x:%x:%x:%x:%x:%x\n",
                   (unsigned)g_rtl_io_base,
                   (unsigned)g_rtl_irq_line,
                   (unsigned)g_mac[0], (unsigned)g_mac[1], (unsigned)g_mac[2],
                   (unsigned)g_mac[3], (unsigned)g_mac[4], (unsigned)g_mac[5]);
  } else {
    console_writeln("net0: rtl8139 not bound");
  }
}

void net_startup_connect(void) {
  char ip[20];
  char gw[20];

  if (!g_rtl_ready) {
    return;
  }

  console_writeln("net0: startup DHCP begin");
  if (net_dhcp_acquire(MGCORE_HZ * 3U)) {
    g_connected = 1;
    format_ip(g_ip_addr, ip, sizeof(ip));
    format_ip(g_ip_gw, gw, sizeof(gw));
    console_printf("net0: DHCP lease acquired ip=%s gw=%s\n", ip, gw);
    return;
  }

  g_ip_addr = NET_DEFAULT_IP;
  g_ip_gw = NET_DEFAULT_GW;
  g_ip_mask = NET_DEFAULT_MASK;
  g_connected = 1;
  format_ip(g_ip_addr, ip, sizeof(ip));
  format_ip(g_ip_gw, gw, sizeof(gw));
  console_printf("net0: DHCP timeout, fallback static ip=%s gw=%s\n", ip, gw);
}

void net_handle_irq(void) {
  uint16_t isr;
  if (!g_rtl_ready) {
    return;
  }

  isr = io_in16((uint16_t)(g_rtl_io_base + RTL_REG_ISR));
  if (isr == 0) {
    return;
  }

  io_out16((uint16_t)(g_rtl_io_base + RTL_REG_ISR), isr);
  if ((isr & (RTL_ISR_ROK | RTL_ISR_RER | RTL_ISR_RXOVW | RTL_ISR_FIFOOVW | RTL_ISR_LENCHG)) != 0U) {
    rtl_poll_rx();
  }
}

size_t net_adapter_count(void) {
  return g_adapter_count;
}

void net_dump_adapters(void) {
  size_t i;
  if (g_adapter_count == 0) {
    console_writeln("net: no ethernet controller detected");
    return;
  }

  for (i = 0; i < g_adapter_count; ++i) {
    const struct net_adapter *a = &g_adapters[i];
    console_printf("eth%u: pci=%u:%u.%u vendor=%x device=%x irq=%u driver=%s state=%s\n",
                   (unsigned)i,
                   (unsigned)a->bus,
                   (unsigned)a->slot,
                   (unsigned)a->func,
                   (unsigned)a->vendor_id,
                   (unsigned)a->device_id,
                   (unsigned)a->irq_line,
                   (a->vendor_id == RTL8139_VENDOR_ID && a->device_id == RTL8139_DEVICE_ID) ? "rtl8139" : "unbound",
                   a->bound ? "up" : "down");
  }
}

void net_dump_driver_tools(void) {
  console_writeln("network drivers:");
  console_writeln("  rtl8139       : implemented (active target)");
  console_writeln("  e1000/e1000e  : not implemented");
  console_writeln("  virtio-net    : not implemented");
  console_writeln("  ne2k-pci      : not implemented");
}

void net_scan_nearby(void) {
  uint8_t gw_mac[6];
  if (!g_rtl_ready) {
    console_writeln("scan: net0 down (rtl8139 not ready)");
    return;
  }

  rtl_poll_rx();
  if (net_resolve_arp(g_ip_gw, gw_mac, MGCORE_HZ)) {
    console_writeln("scan: gateway ARP reachable");
  } else {
    console_writeln("scan: gateway ARP timeout");
  }
}

void net_dump_nearby(void) {
  char ip[20];
  char gw[20];
  char mask[20];
  size_t arp_count = 0;
  size_t i;

  format_ip(g_ip_addr, ip, sizeof(ip));
  format_ip(g_ip_gw, gw, sizeof(gw));
  format_ip(g_ip_mask, mask, sizeof(mask));

  for (i = 0; i < NET_MAX_ARP_CACHE; ++i) {
    if (g_arp_cache[i].valid) {
      ++arp_count;
    }
  }

  console_printf("net0: driver=rtl8139 state=%s connected=%s\n",
                 g_rtl_ready ? "up" : "down",
                 g_connected ? "yes" : "no");
  console_printf("net0: ip=%s mask=%s gw=%s\n", ip, mask, gw);
  console_printf("net0: rx=%u tx=%u drop=%u arp_entries=%u\n",
                 (unsigned)g_rx_frames,
                 (unsigned)g_tx_frames,
                 (unsigned)g_rx_dropped,
                 (unsigned)arp_count);
}

int net_connect_target(const char *target) {
  uint8_t gw_mac[6];
  if (!target || *target == '\0') {
    return -1;
  }
  if (!g_rtl_ready) {
    return -2;
  }

  if (kstrcmp(target, "net0") != 0 && kstrcmp(target, "default") != 0 && kstrcmp(target, "up") != 0) {
    return -4;
  }

  if (!net_resolve_arp(g_ip_gw, gw_mac, MGCORE_HZ)) {
    return -3;
  }

  g_connected = 1;
  return 0;
}

const char *net_connected_name(void) {
  return g_connected ? "net0" : NULL;
}

int net_ping(const char *target, size_t count) {
  size_t seq;
  uint32_t dst_ip;
  uint32_t next_hop;
  uint8_t dst_mac[6];
  uint16_t identifier;
  char ip_text[20];
  int received = 0;

  if (!target || *target == '\0') {
    console_writeln("ping: target required");
    return -1;
  }

  if (kstrcmp(target, "localhost") == 0 || kstrcmp(target, "127.0.0.1") == 0) {
    console_writeln("PING localhost (loopback):");
    for (seq = 0; seq < (count ? count : 4); ++seq) {
      console_printf("64 bytes from 127.0.0.1: icmp_seq=%u ttl=64 time=0 ms\n", (unsigned)(seq + 1));
    }
    return 0;
  }

  if (!parse_ipv4(target, &dst_ip)) {
    console_writeln("ping: target must be literal IPv4 (example: 8.8.8.8)");
    return -1;
  }

  if (!g_rtl_ready) {
    console_writeln("ping: network adapter down");
    return -1;
  }

  if (count == 0) {
    count = 4;
  }

  next_hop = route_next_hop(dst_ip);
  if (!net_resolve_arp(next_hop, dst_mac, MGCORE_HZ * 2U)) {
    format_ip(next_hop, ip_text, sizeof(ip_text));
    console_printf("ping: ARP resolve failed for next-hop %s\n", ip_text);
    return -1;
  }

  format_ip(dst_ip, ip_text, sizeof(ip_text));
  console_printf("PING %s from net0:\n", ip_text);
  identifier = (uint16_t)((scheduler_uptime_ticks() & 0xFFFFU) ^ 0x4D47U);

  for (seq = 0; seq < count; ++seq) {
    uint64_t start_ms = scheduler_uptime_ms();
    uint64_t start_ticks = scheduler_uptime_ticks();
    int ok = 0;

    kmemset(&g_ping_wait, 0, sizeof(g_ping_wait));
    g_ping_wait.active = 1;
    g_ping_wait.identifier = identifier;
    g_ping_wait.sequence = (uint16_t)(seq + 1U);
    g_ping_wait.target_ip = dst_ip;

    if (ip_send_icmp_echo(dst_ip, dst_mac, identifier, (uint16_t)(seq + 1U)) != 0) {
      console_printf("icmp_seq=%u send failed\n", (unsigned)(seq + 1));
      g_ping_wait.active = 0;
      continue;
    }

    while (scheduler_uptime_ticks() - start_ticks < MGCORE_HZ) {
      rtl_poll_rx();
      if (g_ping_wait.received) {
        uint64_t rtt = scheduler_uptime_ms() - start_ms;
        console_printf("64 bytes from %s: icmp_seq=%u ttl=%u time=%u ms\n",
                       ip_text,
                       (unsigned)(seq + 1),
                       (unsigned)g_ping_wait.reply_ttl,
                       (unsigned)rtt);
        ok = 1;
        ++received;
        break;
      }
      net_wait_event_tick();
    }

    if (!ok) {
      console_printf("icmp_seq=%u timeout\n", (unsigned)(seq + 1));
    }

    g_ping_wait.active = 0;
  }

  console_printf("--- %s ping statistics ---\n", ip_text);
  console_printf("%u packets transmitted, %u received, %u%% packet loss\n",
                 (unsigned)count,
                 (unsigned)received,
                 (unsigned)(((count - (size_t)received) * 100U) / count));

  return received > 0 ? 0 : -1;
}
