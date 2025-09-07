#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lib/ssd1306.h"
#include "inc/mpu6050_handler.h"
#include "inc/ntp_client.h"

// ===== DEFINIÇÕES DOS PINOS =====
#define I2C0_SDA 0
#define I2C0_SCL 1
#define I2C1_SDA 14
#define I2C1_SCL 15
#define LED_PIN_GREEN 11

// ===== CONFIGURAÇÕES DO WIFI=====
#define WIFI_SSID "iPhone (2)"
#define WIFI_PASSWORD "12345678"

// ===== CONFIGURAÇÕES DO MQTT=====
#define SEU_NOME "anderson.dantas"
#define MQTT_SERVER "mqtt.iot.natal.br"
#define MQTT_PORT 1883
#define MQTT_USER "desafio20"
#define MQTT_PASS "desafio20.laica"
#define MQTT_CLIENT_ID "anderson.dantas"
#define MQTT_TOPIC_MP6050 "ha/desafio20/anderson.dantas/mpu6050"
#define NUMERO_DESAFIO "20"

// ===== VARIÁVEIS GLOBAIS =====
mqtt_client_t *client;
ip_addr_t mqtt_server_ip;
bool mqtt_connected = false;
volatile bool time_synchronized = false;
volatile time_t current_utc_time = 0;
#define NTP_SERVER "pool.ntp.br" // Servidor NTP do Brasil

// ===== FUNÇÃO PARA ATAUALIZAR O DISPLAY OLED  AO INICIAR =====
void display_message_init(const char *line1, const char *line2, const char *line3, const char *line4)
{
    // Cria área de renderização que cobre toda a tela
    struct render_area area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer));// Limpa o buffer antes de desenhar

    // Escreve cada linha no display (se não for NULL)
    if (line1)
        ssd1306_draw_string(buffer, 5 , 0, (char *)line1);
    if (line2)
        ssd1306_draw_string(buffer, 5, 16, (char *)line2);
    if (line3)
        ssd1306_draw_string(buffer, 0, 32, (char *)line3);
    if (line4)
        ssd1306_draw_string(buffer, 0, 48, (char *)line4);
    render_on_display(buffer, &area);// Atualiza display com conteúdo do buffer
}

// ===== FUNÇÃO PARA ATAUALIZAR O DISPLAY OLED  =====
void display_message(mpu6050_data_t *sensor_data)
{
    struct render_area area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer));

    // Converte dados do sensor em strings
    char tempStr[20];
    snprintf(tempStr, sizeof(tempStr), "Temp: %.1f °C", sensor_data->temperature);

    char accel_giros_x[40];
    snprintf(accel_giros_x, sizeof(accel_giros_x), "X: %.2f   %4.2f", sensor_data->accel_x, sensor_data->gyro_x);

    char accel_giros_y[40];
    snprintf(accel_giros_y, sizeof(accel_giros_y), "y: %.2f   %4.2f", sensor_data->accel_y, sensor_data->gyro_y);

    char accel_giros_z[40];
    snprintf(accel_giros_z, sizeof(accel_giros_z), "z: %.2f   %4.2f", sensor_data->accel_z, sensor_data->gyro_z);

    ssd1306_draw_string(buffer, 10, 0, "ACEL.");
    ssd1306_draw_string(buffer, 70, 0, "GIROS.");
    ssd1306_draw_line(buffer, 0, 12, 150, 12, true);
    ssd1306_draw_string(buffer, 0, 16, accel_giros_x);
    ssd1306_draw_string(buffer, 0, 32, accel_giros_y);
    ssd1306_draw_string(buffer, 0, 45, accel_giros_z);
    ssd1306_draw_string(buffer, 0, 56, tempStr);

    render_on_display(buffer, &area);
}

// ===== CALLBACKS MQTT =====
void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        printf("MQTT: Conectado.\n");
        mqtt_connected = true;
    }
    else
    {
        printf("MQTT: Falha conexao:  Código %d", status);
        mqtt_connected = false;
    }
}

// ===== CALLBACK DE RESOLUÇÃO DNS =====
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
                .keep_alive = 30};
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

     // === INICIALIZA I2C0 PARA MPU-6050 ===
    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);
    if (!mpu6050_init(i2c0, GYRO_FS_250_DPS, ACCEL_FS_2G))
    {
        printf("Falha ao inicializar o MPU6050!\n");
    }

    // === INICIALIZA I2C1 PARA DISPLAY OLED ===
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);
    ssd1306_init(i2c1);

    // === CONFIGURA LED INDICADOR ===
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    display_message_init("conctando ", " Ao Wi-Fi", NULL, NULL);

    // === INICIALIZA WIFI ===
    if (cyw43_arch_init())
    {
        display_message_init("ERRO", " no driver Wi-Fi", NULL, NULL);
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        display_message_init("ERRO", "WiFi Nao Conecta", NULL, NULL);
        return -1;
    }

    // === RESOLVE E CONECTA AO BROKER MQTT ===
    display_message_init(NULL, "Conectando ao ", "MQTT", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    dns_gethostbyname(MQTT_SERVER, &mqtt_server_ip, dns_found_cb, NULL);

    // Variáveis de controle de tempo
    absolute_time_t last_sensor_read = get_absolute_time();
    absolute_time_t last_ntp_sync = get_absolute_time();
    absolute_time_t last_publish_time = get_absolute_time();
    ;
    mpu6050_data_t last_published_data = {0};

    while (1)
    {
        cyw43_arch_poll();// Mantém Wi-Fi e TCP/IP funcionando

        // === LEITURA DO SENSOR ===
        if (absolute_time_diff_us(last_sensor_read, get_absolute_time()) >= 1000000)
        {
            last_sensor_read = get_absolute_time();

            mpu6050_data_t sensor_data;
            if (mpu6050_read_data(&sensor_data))
            {
                // Atualiza display com dados do sensor
                display_message(&sensor_data);

                // Publica no MQTT se houver mudança significativa ou se passou 60s
                bool has_changed = (fabs(sensor_data.accel_x - last_published_data.accel_x) > 0.1);
                if ((absolute_time_diff_us(last_publish_time, get_absolute_time()) > 10e6 && has_changed) ||
                    (absolute_time_diff_us(last_publish_time, get_absolute_time()) > 60e6))
                {

                    if (mqtt_connected && mqtt_client_is_connected(client))
                    {
                        char payload[512];
                        char timestamp_str[20];

                        // Se tempo foi sincronizado, gera timestamp
                        if (time_synchronized)
                        {
                            static absolute_time_t last_time_update = {0};
                            

                            // Inicializa se for a primeira vez
                            if (last_time_update == 0)
                            {
                                last_time_update = get_absolute_time();
                            }

                            // Calcula diferença desde a última atualização
                            int64_t diff_us = absolute_time_diff_us(last_time_update, get_absolute_time());
                            current_utc_time += diff_us / 1000000; // converte microssegundos para segundos
                            last_time_update = get_absolute_time();

                            // Agora gera timestamp
                            time_t brasil_time = current_utc_time + (-3 * 3600); // UTC-3
                            struct tm *local_tm = gmtime(&brasil_time);
                            strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%S", local_tm);
                        }

                        else
                        {
                            strcpy(timestamp_str, "1970-01-01T00:00:00");
                        }

                        // Monta JSON para publicação no MQTT
                        snprintf(payload, sizeof(payload),
                                 "{\"team\":\"desafio%s\",\"device\":\"bitdoglab_%s\",\"ip\":\"%s\",\"ssid\":\"%s\",\"sensor\":\"MPU-6050\",\"data\":{\"accel\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"gyro\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},\"temperature\":%.1f},\"timestamp\":\"%s\"}",
                                 NUMERO_DESAFIO, SEU_NOME,
                                 ip4addr_ntoa(netif_ip4_addr(netif_default)), WIFI_SSID,
                                 sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z,
                                 sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z,
                                 sensor_data.temperature, timestamp_str);

                        // Publica no broker
                        if (mqtt_connected && mqtt_client_is_connected(client))
                        {

                            if (mqtt_publish(client, MQTT_TOPIC_MP6050, payload, strlen(payload), 0, 0, NULL, NULL) == ERR_OK)
                            {
                                printf("Publicado no MQTT: timestamp: %s\n", timestamp_str);
                                gpio_put(LED_PIN_GREEN, 1);
                                sleep_ms(50);
                                gpio_put(LED_PIN_GREEN, 0);
                                last_publish_time = get_absolute_time();
                                last_published_data = sensor_data;
                            }
                            else
                            {
                                printf("MQTT: Erro ao publicar.\n");
                                display_message_init("ERRO", "ao publicar", NULL, NULL);
                            }
                        }
                        else
                        {
                            printf("MQTT ainda não conectado. Aguardando...\n");
                        }
                    }
                }
            }
        }

        // === SINCRONIZAÇÃO NTP (executa até obter tempo válido) ===
        while (!time_synchronized)
        {
            if (ntp_get_time(NTP_SERVER, 5000))
            {
                current_utc_time = ntp_get_last_time();
                time_synchronized = true;
            }
            else
            {
                printf("Tentando novamente NTP...\n");
                sleep_ms(5000);
            }
        }

        sleep_ms(50); // Pequena pausa para não ocupar 100% da CPU
    }
}
