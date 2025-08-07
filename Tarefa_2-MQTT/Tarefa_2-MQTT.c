#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"

// ===== CONFIGURAÇÕES =====
#define WIFI_SSID "iPhone (2)"
#define WIFI_PASSWORD "12345678"
#define MQTT_SERVER "mqtt.iot.natal.br"
#define MQTT_PORT 1883
#define MQTT_USER "desafio20"
#define MQTT_PASS "desafio20.laica"
#define MQTT_CLIENT_ID "anderson.dantas"
#define MQTT_TOPIC "ha/desafio20/anderson.dantas"
#define MSG_INTERVAL_MS 5000

// ===== VARIÁVEIS GLOBAIS =====
mqtt_client_t *client;
ip_addr_t mqtt_server_ip;
bool mqtt_connected = false;

// ===== CALLBACKS MQTT =====

void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT conectado com sucesso.\n");
        mqtt_connected = true;

        // Enviar uma mensagem de teste após conexão
        char msg[64];
        
        snprintf(msg, sizeof(msg), "teste!");
        err_t err = mqtt_publish(client, MQTT_TOPIC, msg, strlen(msg), 0, 0, NULL, NULL);
        if (err != ERR_OK) {
            printf("Erro ao publicar: %d\n", err);
        } else {
            printf("Mensagem publicada: %s\n", msg);
            sleep_ms(100);
        }

    } else {
        printf("Falha ao conectar no MQTT. Código: %d\n", status);
    }
}

void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr != NULL)
    {
        printf("Broker %s resolvido para: %s\n", name, ipaddr_ntoa(ipaddr));
        ip_addr_copy(mqtt_server_ip, *ipaddr);

        client = mqtt_client_new();
        if (client != NULL)
        {
            struct mqtt_connect_client_info_t ci = {
                .client_id = MQTT_CLIENT_ID,
                .client_user = MQTT_USER,
                .client_pass = MQTT_PASS,
                .keep_alive = 30
            };
            printf("Conectando ao broker MQTT...\n");
            mqtt_client_connect(client, &mqtt_server_ip, MQTT_PORT, mqtt_connection_cb, NULL, &ci);
        }
        else
        {
            printf("Erro ao criar cliente MQTT.\n");
        }
    }
    else
    {
        printf("Erro ao resolver broker MQTT.\n");
    }
}

int main()
{
    stdio_init_all();
    sleep_ms(2000);
    printf("Inicializando Wi-Fi + MQTT\n");

    if (cyw43_arch_init())
    {
        printf("Erro ao inicializar o driver Wi-Fi.\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    // Conectar ao Wi-Fi
    printf("Conectando ao Wi-Fi: %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Falha na conexão Wi-Fi.\n");
        return -1;
    }
    printf("Wi-Fi conectado com sucesso.\n");

    // Resolver o IP do broker
    printf("Resolvendo broker MQTT...\n");
    err_t err = dns_gethostbyname(MQTT_SERVER, &mqtt_server_ip, dns_found_cb, NULL);
    if (err == ERR_OK)
    {
        // DNS já estava resolvido em cache
        dns_found_cb(MQTT_SERVER, &mqtt_server_ip, NULL);
    }
    else if (err != ERR_INPROGRESS)
    {
        printf("Erro ao iniciar resolução DNS: %d\n", err);
        return -1;
    }

    // Loop principal
    while (true)
    {
        cyw43_arch_poll();
        sleep_ms(100);
    }

    return 0;
}
