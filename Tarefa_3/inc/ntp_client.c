// ntp_client.c

#include "ntp_client.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include <string.h>
#include <stdio.h>

#define NTP_PORT 123
#define NTP_MSG_LEN 48
#define NTP_UNIX_EPOCH_DIFF 2208988800UL // Segundos entre 1900-01-01 e 1970-01-01

// Variáveis de estado do módulo
static time_t last_ntp_time = 0;
static volatile bool ntp_response_received = false;

// Callback para receber a resposta NTP
static void ntp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        const ip_addr_t *addr, u16_t port) {
    if (p != NULL && p->tot_len >= NTP_MSG_LEN) {
        uint8_t buffer[NTP_MSG_LEN];
        pbuf_copy_partial(p, buffer, NTP_MSG_LEN, 0);

        // Extrai os segundos do timestamp de transmissão (bytes 40-43)
        uint32_t seconds_since_1900 = (buffer[40] << 24) | (buffer[41] << 16) |
                                      (buffer[42] << 8)  | buffer[43];

        last_ntp_time = (time_t)(seconds_since_1900 - NTP_UNIX_EPOCH_DIFF);
        ntp_response_received = true;
    }
    pbuf_free(p);
}

bool ntp_get_time(const char* server_hostname, uint32_t timeout_ms) {
    // *** CORREÇÃO IMPORTANTE: Reseta o estado a cada chamada ***
    ntp_response_received = false;
    last_ntp_time = 0;

    ip_addr_t ntp_server_ip;
    err_t err = dns_gethostbyname(server_hostname, &ntp_server_ip, NULL, NULL);

    uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    while (err == ERR_INPROGRESS) {
        if (to_ms_since_boot(get_absolute_time()) - start_ms > timeout_ms) {
            printf("NTP DNS request timed out\n");
            return false;
        }
        // dns_gethostbyname_addrarg não é uma função padrão, usamos a forma correta
        err = dns_gethostbyname(server_hostname, &ntp_server_ip, NULL, NULL);
    }

    if (err != ERR_OK) {
        printf("Failed to resolve NTP server hostname: %s, error: %d\n", server_hostname, err);
        return false;
    }

    struct udp_pcb *pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        printf("Failed to create UDP PCB\n");
        return false;
    }

    udp_recv(pcb, ntp_recv_cb, NULL);

    uint8_t ntp_request[NTP_MSG_LEN] = {0};
    ntp_request[0] = 0x1B; // LI=0, VN=3, Mode=3 (cliente)

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    if (p) {
        memcpy(p->payload, ntp_request, NTP_MSG_LEN);
        udp_sendto(pcb, p, &ntp_server_ip, NTP_PORT);
        pbuf_free(p);
    } else {
        udp_remove(pcb);
        return false;
    }
    
    // Aguarda a resposta (sinalizada pelo callback)
    start_ms = to_ms_since_boot(get_absolute_time());
    while (!ntp_response_received) {
        cyw43_arch_poll();
        if (to_ms_since_boot(get_absolute_time()) - start_ms > timeout_ms) {
            printf("Timeout waiting for NTP response\n");
            udp_remove(pcb);
            return false;
        }
    }

    udp_remove(pcb);
    return true;
}

time_t ntp_get_last_time(void) {
    return last_ntp_time;
}