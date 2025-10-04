#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Adicione estas duas linhas para que LwIP "conheça" as macros do FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

#include "lwipopts_examples_common.h"

#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 1)
#define MQTT_REQ_MAX_IN_FLIGHT 3

#define MEM_LIBC_MALLOC                 0
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (24 * 1024)
#define MEMP_MEM_MALLOC                 1
#define LWIP_TCP                        1

#define TCP_WND                         (8 * TCP_MSS)
#define TCP_SND_BUF                     (8 * TCP_MSS)
#define TCP_SND_QUEUELEN                ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

#define MEMP_NUM_TCP_SEG                TCP_SND_QUEUELEN
#define LWIP_ALTCP                      1 // Não usamos altcp
#define LWIP_ALTCP_TLS                  1 // Não usamos altcp
#define LWIP_DNS                        1
#define MQTT_OUTPUT_RINGBUF_SIZE        2048
#define NO_SYS                          0
#define LWIP_SOCKET                     1
#define LWIP_NETCONN                    1
#define LWIP_STATS                      0

#define LWIP_TIMEVAL_PRIVATE        0
#define LWIP_SYS_TIME_FORMAT        1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_IGMP                   1
#define LWIP_MDNS_RESPONDER         1
#define LWIP_NUM_MDNS_SERVICES      1
#define MEMP_NUM_TCP_PCB            10
#define MEMP_NUM_UDP_PCB            6
#define MEMP_NUM_TCP_PCB_LISTEN     5
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_SYS_TIMEOUT        10
#define PBUF_POOL_SIZE              24
#define LWIP_ETHERNET               1
#define LWIP_ARP                    1
#define LWIP_DNS                    1
// --- Configurações de Threading e Sistema Operacional (A CHAVE DO PROBLEMA) ---
#define SYS_LIGHTWEIGHT_PROT        1
#define TCPIP_THREAD_STACKSIZE      1024
#define TCPIP_THREAD_PRIO           (tskIDLE_PRIORITY + 3)
#define TCPIP_MBOX_SIZE             6
#define DEFAULT_UDP_RECVMBOX_SIZE   6
#define DEFAULT_TCP_RECVMBOX_SIZE   6
#define DEFAULT_ACCEPTMBOX_SIZE     6

// --- Configurações de Features de Rede ---
#define LWIP_TCP                    1
#define LWIP_DNS                    1
#define LWIP_IGMP                   1
#define LWIP_DHCP                   1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#endif