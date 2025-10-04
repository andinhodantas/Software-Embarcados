// mqtt_psk_client.h
#ifndef MQTT_PSK_CLIENT_H
#define MQTT_PSK_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

// Estrutura para manter o estado do cliente, evitando variáveis globais.
// Isso torna o código reentrante, permitindo múltiplas instâncias de cliente.
typedef struct {
    int sockfd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
} mqtt_client_context_t;


/**
 * @brief Inicializa a conexão TLS-PSK com o broker MQTT.
 *
 * Resolve o hostname, conecta o socket TCP e realiza o handshake TLS.
 *
 * @param ctx Ponteiro para a estrutura de contexto do cliente.
 * @param host Endereço do broker (IP ou hostname).
 * @param port Porta do broker.
 * @param psk A Pre-Shared Key.
 * @param psk_len O comprimento da PSK.
 * @param identity O identificador (identity) para a PSK.
 * @param id_len O comprimento do identificador.
 * @return true em caso de sucesso, false em caso de falha.
 */
bool mqtt_psk_client_init(mqtt_client_context_t *ctx, const char *host, uint16_t port,
                          const unsigned char *psk, size_t psk_len,
                          const unsigned char *identity, size_t id_len);

/**
 * @brief Envia dados criptografados para o broker.
 *
 * @param ctx Ponteiro para o contexto do cliente.
 * @param buf Buffer com os dados a serem enviados.
 * @param len Tamanho dos dados no buffer.
 * @return Número de bytes enviados ou um código de erro do mbedtls.
 */
int mqtt_psk_client_send(mqtt_client_context_t *ctx, const unsigned char *buf, size_t len);

/**
 * @brief Recebe dados criptografados do broker.
 *
 * @param ctx Ponteiro para o contexto do cliente.
 * @param buf Buffer para armazenar os dados recebidos.
 * @param len Tamanho máximo do buffer de recebimento.
 * @return Número de bytes recebidos ou um código de erro do mbedtls.
 */
int mqtt_psk_client_recv(mqtt_client_context_t *ctx, unsigned char *buf, size_t len);

/**
 * @brief Fecha a conexão e libera todos os recursos.
 *
 * @param ctx Ponteiro para o contexto do cliente a ser fechado.
 */
void mqtt_psk_client_close(mqtt_client_context_t *ctx);

#endif // MQTT_PSK_CLIENT_H