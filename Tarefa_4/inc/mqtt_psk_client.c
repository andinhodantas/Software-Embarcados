// mqtt_psk_client.c
#include "mqtt_psk_client.h"
#include "pico/cyw43_arch.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h> /* for timeval used by lwip setsockopt */

#define DNS_WAIT_MS        5000  // tempo total para resolver DNS
#define CONNECT_TIMEOUT_MS 5000  // timeout connect TCP

/* Estrutura do contexto (defina em mqtt_psk_client.h conforme abaixo) */
/* typedef struct {
    int sockfd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
} mqtt_client_context_t;
*/

/* Wrappers para envio/recebimento usando lwIP */
static int pico_lwip_send(void *ctx, const unsigned char *buf, size_t len) {
    int fd = *((int *)ctx);
    if (fd < 0) return -1;
    ssize_t s = lwip_send(fd, buf, len, 0);
    return (int)s;
}

static int pico_lwip_recv(void *ctx, unsigned char *buf, size_t len) {
    int fd = *((int *)ctx);
    if (fd < 0) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    int r = lwip_read(fd, buf, len);
    if (r < 0) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    return r;
}

static void print_mbedtls_error(const char *func, int ret) {
    char err_buf[128];
    mbedtls_strerror(ret, err_buf, sizeof(err_buf));
    printf("[mbedtls] %s: %s (0x%08X)\n", func, err_buf, ret);
}

static void mqtt_client_context_init(mqtt_client_context_t *ctx) {
    ctx->sockfd = -1;
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
    mbedtls_entropy_init(&ctx->entropy);
}

/* Helper: blocking DNS resolution with timeout (works with lwIP) */
#include "lwip/dns.h"
#include "lwip/ip_addr.h"

// host: nome do host (ex: "broker.exemplo.com")
// out_ip: IP resolvido
// timeout_ms: tempo máximo de espera
static int resolve_host_blocking(const char *host, ip_addr_t *out_ip, uint32_t timeout_ms) {
    ip_addr_t resolved;
    err_t err;

    // Tenta resolver diretamente (caso já esteja no cache DNS)
    err = dns_gethostbyname_addrtype(host, &resolved, NULL, NULL, LWIP_DNS_ADDRTYPE_IPV4);
    if (err == ERR_OK) {
        *out_ip = resolved;
        return 0;
    } else if (err == ERR_INPROGRESS) {
        // Se ainda não resolveu, precisamos aguardar
        absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
        do {
            // Dá tempo ao lwIP para processar DNS
            vTaskDelay(pdMS_TO_TICKS(10));
            err = dns_gethostbyname_addrtype(host, &resolved, NULL, NULL, LWIP_DNS_ADDRTYPE_IPV4);
            if (err == ERR_OK) {
                *out_ip = resolved;
                return 0;
            }
        } while (!time_reached(deadline));
    }

    return -1; // falhou
}


/* Try parse numeric IPv4 dotted string; return true if parsed */
static bool try_parse_ipv4_literal(const char *host, ip4_addr_t *out_ip) {
    return ip4addr_aton(host, out_ip) != 0;
}

/* Set socket timeout for recv/send */
static void socket_set_timeouts(int fd, int ms) {
    if (fd < 0) return;
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

bool mqtt_psk_client_init(mqtt_client_context_t *ctx, const char *host, uint16_t port,
                          const unsigned char *psk, size_t psk_len,
                          const unsigned char *identity, size_t id_len)
{
    mqtt_client_context_init(ctx);
    int ret;

    /* Seed RNG */
    const char *pers = "pico_mqtt_psk_client";
    if ((ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func, &ctx->entropy,
                                     (const unsigned char *)pers, strlen(pers))) != 0) {
        print_mbedtls_error("ctr_drbg_seed", ret);
        goto fail;
    }

    /* Resolve host (first try literal IP) */
    ip4_addr_t ip4;
    if (try_parse_ipv4_literal(host, &ip4)) {
        /* OK */
    } else {
        if (resolve_host_blocking(host, &ip4, DNS_WAIT_MS)==-1) {
            printf("[dns] Falha ao resolver host: %s\n", host);
            goto fail;
        }
    }

    /* Create TCP socket */
    ctx->sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sockfd < 0) {
        printf("[net] Falha ao criar socket: %d\n", ctx->sockfd);
        goto fail;
    }

    /* Set timeouts before connect (helps connect() to fail fast) */
    socket_set_timeouts(ctx->sockfd, CONNECT_TIMEOUT_MS);

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = lwip_htons(port);
    sa.sin_addr.s_addr = ip4.addr;

    if (lwip_connect(ctx->sockfd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        printf("[net] Falha ao conectar %s:%u (errno=%d)\n", host, port, errno);
        goto fail;
    }
    printf("[net] TCP conectado a %s:%u\n", host, port);

    /* TLS config */
    if ((ret = mbedtls_ssl_config_defaults(&ctx->conf, MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        print_mbedtls_error("ssl_config_defaults", ret);
        goto fail;
    }

    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_NONE); /* WARNING: insecure - for testing */

    if ((ret = mbedtls_ssl_conf_psk(&ctx->conf, psk, psk_len, identity, id_len)) != 0) {
        print_mbedtls_error("ssl_conf_psk", ret);
        goto fail;
    }

    if ((ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf)) != 0) {
        print_mbedtls_error("ssl_setup", ret);
        goto fail;
    }

    /* Attach send/recv */
    mbedtls_ssl_set_bio(&ctx->ssl, &ctx->sockfd, pico_lwip_send, pico_lwip_recv, NULL);

    /* Handshake */
    printf("[tls] Iniciando handshake...\n");
    while ((ret = mbedtls_ssl_handshake(&ctx->ssl)) != 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            /* continue */
            continue;
        } else {
            print_mbedtls_error("ssl_handshake", ret);
            goto fail;
        }
    }
    printf("[tls] Handshake concluído.\n");

    /* Good to go */
    return true;

fail:
    mqtt_psk_client_close(ctx);
    return false;
}

int mqtt_psk_client_send(mqtt_client_context_t *ctx, const unsigned char *buf, size_t len) {
    if (!ctx) return -1;
    int r = mbedtls_ssl_write(&ctx->ssl, buf, (int)len);
    if (r < 0) {
        print_mbedtls_error("ssl_write", r);
    }
    return r;
}

int mqtt_psk_client_recv(mqtt_client_context_t *ctx, unsigned char *buf, size_t len) {
    if (!ctx) return -1;
    int r = mbedtls_ssl_read(&ctx->ssl, buf, (int)len);
    if (r < 0) {
        print_mbedtls_error("ssl_read", r);
    }
    return r;
}

void mqtt_psk_client_close(mqtt_client_context_t *ctx) {
    if (!ctx) return;
    if (ctx->sockfd >= 0) {
        mbedtls_ssl_close_notify(&ctx->ssl);
        lwip_close(ctx->sockfd);
        ctx->sockfd = -1;
    }
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);
    /* debug */
    printf("[mqtt_psk_client] recursos liberados\n");
}
