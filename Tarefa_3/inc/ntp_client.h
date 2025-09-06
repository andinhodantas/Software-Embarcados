// ntp_client.h

#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include <stdbool.h>
#include <time.h>
#include "pico/types.h" // Necessário para uint32_t

/**
 * @brief Inicializa o cliente NTP e solicita a hora ao servidor.
 * * Esta é uma função bloqueante. Ela aguarda até que a hora seja recebida ou o timeout ocorra.
 * * @param server_hostname Endereço do servidor NTP (ex: "pool.ntp.br").
 * @param timeout_ms Tempo limite para resposta em milissegundos.
 * @return true se a hora foi obtida com sucesso, false em caso de falha.
 */
bool ntp_get_time(const char* server_hostname, uint32_t timeout_ms);

/**
 * @brief Retorna a última hora UTC recebida do servidor NTP.
 * * O valor retornado está no formato time_t (segundos desde a Época Unix - 1970-01-01 00:00:00 UTC).
 * * @return time_t correspondente à hora UTC. Retorna 0 se nenhuma hora foi recebida ainda.
 */
time_t ntp_get_last_time(void);

#endif // NTP_CLIENT_H
