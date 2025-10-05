
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Pico SDK & FreeRTOS
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Hardware e Bibliotecas Locais
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"

#include "mqtt_psk_client.h"

// --- Configurações do Projeto ---
#define WIFI_SSID "CALLOC MALLOC" // Nome da rede Wi-Fi
#define WIFI_PASSWORD "JCdantas2020" // Senha da rede Wi-Fi
#define MQTT_BROKER_IP "192.168.18.46" // IP da maquina que está o broker MQTT (Verificar no terminal com "ipconfig" ou "ifconfig")
#define MQTT_BROKER_PORT 8815
#define MQTT_CLIENT_ID "aluno15" // ID de cliente alterado

// --- Definições de Hardware ---
#define TEMP_ADC_CHANNEL 4
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

#define I2C1_SDA 14
#define I2C1_SCL 15

#define MQTT_PACKET_TYPE_SUBACK 0x90
#define MQTT_PACKET_TYPE_PUBLISH 0x30

uint8_t ssd[ssd1306_buffer_length];
struct render_area frame_area = {
    .start_column = 0,
    .end_column = ssd1306_width - 1,
    .start_page = 0,
    .end_page = ssd1306_n_pages - 1};

// --- Definições dos Tópicos MQTT ---
#define TOPIC_SENSOR_TEMP "/aluno15/bitdoglab/temp"
#define TOPIC_SENSOR_BUTTONS "/aluno15/bitdoglab/button"
#define TOPIC_CONTROL_SUB "/aluno15/sub"

// --- Configurações das Tarefas ---
#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 3)
#define RX_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define BUTTON_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#define TEMP_PUBLISH_INTERVAL_MS 5000
#define BUTTON_POLL_INTERVAL_MS 50

// --- Credenciais PSK (Pré-Shared Key) ---
const unsigned char psk_identity[] = "aluno15";
const unsigned char psk_key[] = {0xAB, 0xCD, 0x15, 0xEF, 0x12, 0x34};
const size_t psk_len = sizeof(psk_key);

// --- Estrutura de Parâmetros para Tarefas ---
typedef struct
{
    mqtt_client_context_t *client_ctx;
    SemaphoreHandle_t mqtt_mutex;
} task_shared_params_t;

// --- Protótipos das Funções ---
int build_mqtt_connect(const char *client_id, const char *username, unsigned char *buf, int buf_len);
int build_mqtt_publish(const char *topic, const char *payload, unsigned char *buf, int buf_len);
int build_mqtt_subscribe(const char *topic, unsigned char *buf, int buf_len);
float read_onboard_temperature();

 // --- Implementações das Funções ---
// Lê a temperatura do sensor interno do RP2040
float read_onboard_temperature()
{
    const float conversion_factor = 3.3f / (1 << 12);
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}
// Inicializa o display OLED e exibe mensagens
void display_message_init(const char *line1, const char *line2, const char *line3, const char *line4, const char *line5)
{
    // Cria área de renderização que cobre toda a tela
    struct render_area area = {0, ssd1306_width - 1, 0, ssd1306_n_pages - 1};
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer)); // Limpa o buffer antes de desenhar

    // Escreve cada linha no display (se não for NULL)
    if (line1)
        ssd1306_draw_string(buffer, 5, 0, (char *)line1);
    if (line2)
        ssd1306_draw_string(buffer, 5, 16, (char *)line2);
    if (line3)
        ssd1306_draw_string(buffer, 0, 32, (char *)line3);
    if (line4)
        ssd1306_draw_string(buffer, 0, 48, (char *)line4);
    if (line5)
        ssd1306_draw_string(buffer, 0, 56, (char *)line5);
    render_on_display(buffer, &area); // Atualiza display com conteúdo do buffer
}

// Tarefa para receber mensagens MQTT
static void mqtt_receive_task(void *pvParameters)
{
    mqtt_client_context_t *client_ctx = (mqtt_client_context_t *)pvParameters;
    // Buffer de recepção local para esta tarefa
    unsigned char mqtt_rx_buf[512];

    while (true)
    {
        int len = mqtt_psk_client_recv(client_ctx, mqtt_rx_buf, sizeof(mqtt_rx_buf));
        if (len > 0)
        {
            uint8_t packet_type = mqtt_rx_buf[0] & 0xF0;
            switch (packet_type)
            {
            case MQTT_PACKET_TYPE_SUBACK: // SUBACK
                printf("[RX_TASK] Inscrição no tópico confirmada.\n");
                break;
            case MQTT_PACKET_TYPE_PUBLISH:
            { // PUBLISH
                int topic_len = (mqtt_rx_buf[2] << 8) | mqtt_rx_buf[3];
                char topic[128];
                memcpy(topic, &mqtt_rx_buf[4], topic_len);
                topic[topic_len] = '\0';
                int payload_offset = 4 + topic_len;
                char payload[256];
                memcpy(payload, &mqtt_rx_buf[payload_offset], len - payload_offset);
                payload[len - payload_offset] = '\0';
                printf("[RX_TASK] Mensagem: Tópico='%s', Payload='%s'\n", topic, payload);
                break;
            }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Função para tratar o evento de pressionar um botão
void handle_button_press(char button_name, task_shared_params_t *params)
{
    char payload_buf[128];
    unsigned char btn_mqtt_buf[256];

    // Monta o payload JSON e a mensagem do display dinamicamente
    snprintf(payload_buf, sizeof(payload_buf), "{\"botao\": \"%c\", \"estado\": \"pressionado\"}", button_name);

    printf("[BTN_TASK] Botao %c pressionado. Publicando: %s\n", button_name, payload_buf);

    // Envia a mensagem MQTT
    if (xSemaphoreTake(params->mqtt_mutex, portMAX_DELAY) == pdTRUE)
    {
        int len = build_mqtt_publish(TOPIC_SENSOR_BUTTONS, payload_buf, btn_mqtt_buf, sizeof(btn_mqtt_buf));
        mqtt_psk_client_send(params->client_ctx, btn_mqtt_buf, len);
        xSemaphoreGive(params->mqtt_mutex);
    }
}
static void button_monitor_task(void *pvParameters)
{
    task_shared_params_t *params = (task_shared_params_t *)pvParameters;
    unsigned char btn_mqtt_buf[256];
    char payload_buf[128];

    // Variáveis de estado para debounce (detecção de borda)
    static bool btn_a_last_state = true; // true = solto
    static bool btn_b_last_state = true;

    while (true)
    {
        bool btn_a_current_state = gpio_get(BUTTON_A_PIN);
        bool btn_b_current_state = gpio_get(BUTTON_B_PIN);

        // Verifica mudança de estado do Botão A (de solto para pressionado)
        if (btn_a_current_state == false && btn_a_last_state == true)
        {
            char display_str[16]; // Nome mais claro
            float temp = read_onboard_temperature();
            snprintf(display_str, sizeof(display_str), " %.2f°C", temp);
            handle_button_press('A', params);
            display_message_init("Temperatura:", display_str, "Bot.A Precionado", "Bot. B solto.", NULL);
        }

        // Verifica mudança de estado do Botão B (de solto para pressionado)
        if (btn_b_current_state == false && btn_b_last_state == true)
        {
            char display_str[16]; // Nome mais claro
            float temp = read_onboard_temperature();
            snprintf(display_str, sizeof(display_str), "%.2f°C", temp);
            handle_button_press('B', params);
            display_message_init("Temperatura:", display_str, "Bot. A solto", "Bot.B Precionado.", NULL);
        }

        btn_a_last_state = btn_a_current_state;
        btn_b_last_state = btn_b_current_state;

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
    }
}

// Tarefa principal para gerenciar conexão Wi-Fi e MQTT
static void connection_manager_task(void *pvParameters)
{

    printf(" Inicializando sistema...\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    vTaskDelay(pdMS_TO_TICKS(500));
    display_message_init("conctando ", " Ao Wi-Fi", NULL, NULL, NULL);
    printf(" Conectando ao Wi-Fi: %s\n", WIFI_SSID);
    
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf(" Falha crítica na conexão Wi-Fi. Reiniciando.\n");
        display_message_init("Falha na", "conexão Wi-Fi", "Reiniciando em", "5s", NULL);
        vTaskDelay(pdMS_TO_TICKS(5000));
        while (1)
            ;
    }
    printf(" Conexão Wi-Fi estabelecida!\n");
    display_message_init("Conectado ao", "WIFI", WIFI_SSID, NULL, NULL);

    static mqtt_client_context_t client_ctx;
    unsigned char local_mqtt_buf[256];

    task_shared_params_t params;
    params.client_ctx = &client_ctx;
    params.mqtt_mutex = xSemaphoreCreateMutex();

    bool sub_tasks_running = false;
    // *** CORREÇÃO: Variável local para controlar o estado da conexão ***
    bool is_mqtt_connected = false;

    while (true)
    {
        // *** CORREÇÃO: Usa a variável local ao invés da função inexistente ***
        if (!is_mqtt_connected)
        {
            display_message_init(NULL, "Conectando ao ", "MQTT", ip4addr_ntoa(netif_ip4_addr(netif_default)), NULL);
            printf(" Desconectado. Tentando conectar ao broker MQTT...\n");
            if (mqtt_psk_client_init(&client_ctx, MQTT_BROKER_IP, MQTT_BROKER_PORT, psk_key, psk_len, psk_identity, sizeof(psk_identity) - 1))
            {
                int len = build_mqtt_connect(MQTT_CLIENT_ID, (const char *)psk_identity, local_mqtt_buf, sizeof(local_mqtt_buf));
                mqtt_psk_client_send(&client_ctx, local_mqtt_buf, len);

                int received = mqtt_psk_client_recv(&client_ctx, local_mqtt_buf, sizeof(local_mqtt_buf));
                if (received > 0 && local_mqtt_buf[0] == 0x20 && local_mqtt_buf[2] == 0x00)
                {
                    display_message_init(NULL, "Conectado ao ", "MQTT", "com sucesso", NULL);
                    printf(" Conectado ao broker com sucesso!\n");
                    // *** CORREÇÃO: Atualiza o estado da conexão ***
                    is_mqtt_connected = true;

                    len = build_mqtt_subscribe(TOPIC_CONTROL_SUB, local_mqtt_buf, sizeof(local_mqtt_buf));
                    mqtt_psk_client_send(&client_ctx, local_mqtt_buf, len);

                    if (!sub_tasks_running)
                    {
                        xTaskCreate(mqtt_receive_task, "RxTask", 2048, &client_ctx, RX_TASK_PRIORITY, NULL);
                        xTaskCreate(button_monitor_task, "ButtonTask", 2048, &params, BUTTON_TASK_PRIORITY, NULL);
                        sub_tasks_running = true;
                    }
                }
                else
                {
                    mqtt_psk_client_close(&client_ctx);
                    display_message_init("Falha na", "conexão MQTT ", "Tentando", "novamente em 5s", NULL);
                    printf(" Falha na conexão MQTT. Tentando novamente em 5s.\n");
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
        else
        {
            char payload_buf[128];
            char display_str[16]; // Nome mais claro
            float temp = read_onboard_temperature();

            snprintf(display_str, sizeof(display_str), "  %.2f°C", temp);
            display_message_init("Temperatura:", display_str, "Bot. A Solto", "Bot. B Solto", NULL);

            snprintf(payload_buf, sizeof(payload_buf), "{\"Temperatura\": %.2f}", temp);

            if (xSemaphoreTake(params.mqtt_mutex, portMAX_DELAY) == pdTRUE)
            {
                printf("Publicando temperatura: %.2f C\n", temp);
                int len = build_mqtt_publish(TOPIC_SENSOR_TEMP, payload_buf, local_mqtt_buf, sizeof(local_mqtt_buf));
                if (mqtt_psk_client_send(&client_ctx, local_mqtt_buf, len) < 0)
                {
                    printf("Falha ao publicar. Fechando conexão.\n");
                    mqtt_psk_client_close(&client_ctx);
                    // *** CORREÇÃO: Atualiza o estado para forçar a reconexão ***
                    is_mqtt_connected = false;
                }
                xSemaphoreGive(params.mqtt_mutex);
            }

            // Só executa o delay se a conexão ainda estiver ativa
            if (is_mqtt_connected)
            {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(TEMP_PUBLISH_INTERVAL_MS));
            }
        }
    }
}

int main()
{
    stdio_init_all();

    sleep_ms(2000);
    // === INICIALIZA I2C1 PARA DISPLAY OLED ===
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);
    ssd1306_init(i2c1);

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(TEMP_ADC_CHANNEL);
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);

    printf("\n--- Monitor de Sensores MQTT v2.0 ---\n");

    xTaskCreate(connection_manager_task, "MainTask", 4096, NULL, MAIN_TASK_PRIORITY, NULL);
    vTaskStartScheduler();
    // O código nunca deve chegar aqui
    while (true)
        ;
    return 0;
}

// --- Funções de construção de pacotes MQTT (sem alterações) ---
// (As funções build_mqtt_connect, build_mqtt_publish, build_mqtt_subscribe permanecem aqui, idênticas às originais)
int build_mqtt_connect(const char *client_id, const char *username, unsigned char *buf, int buf_len)
{
    int client_id_len = strlen(client_id);
    int username_len = strlen(username);
    int payload_len = 2 + client_id_len + 2 + username_len;
    int remaining_len = 10 + payload_len;
    if (buf_len < 2 + remaining_len)
        return -1;
    int pos = 0;
    buf[pos++] = 0x10;
    int rl = remaining_len;
    do
    {
        unsigned char encoded_byte = rl % 128;
        rl /= 128;
        if (rl > 0)
            encoded_byte |= 128;
        buf[pos++] = encoded_byte;
    } while (rl > 0);
    buf[pos++] = 0x00;
    buf[pos++] = 0x04;
    buf[pos++] = 'M';
    buf[pos++] = 'Q';
    buf[pos++] = 'T';
    buf[pos++] = 'T';
    buf[pos++] = 0x04;
    buf[pos++] = 0x82;
    buf[pos++] = 0x00;
    buf[pos++] = 0x3C;
    buf[pos++] = (client_id_len >> 8) & 0xFF;
    buf[pos++] = client_id_len & 0xFF;
    memcpy(&buf[pos], client_id, client_id_len);
    pos += client_id_len;
    buf[pos++] = (username_len >> 8) & 0xFF;
    buf[pos++] = username_len & 0xFF;
    memcpy(&buf[pos], username, username_len);
    pos += username_len;
    return pos;
}

int build_mqtt_publish(const char *topic, const char *payload, unsigned char *buf, int buf_len)
{
    int topic_len = strlen(topic);
    int payload_len = strlen(payload);
    int remaining_len = 2 + topic_len + payload_len;
    if (buf_len < 2 + remaining_len)
        return -1;
    int pos = 0;
    buf[pos++] = 0x30;
    do
    {
        unsigned char encoded_byte = remaining_len % 128;
        remaining_len /= 128;
        if (remaining_len > 0)
        {
            encoded_byte |= 128;
        }
        buf[pos++] = encoded_byte;
    } while (remaining_len > 0);
    buf[pos++] = 0x00;
    buf[pos++] = topic_len;
    memcpy(&buf[pos], topic, topic_len);
    pos += topic_len;
    memcpy(&buf[pos], payload, payload_len);
    pos += payload_len;
    return pos;
}

int build_mqtt_subscribe(const char *topic, unsigned char *buf, int buf_len)
{
    int topic_len = strlen(topic);
    int remaining_len = 2 + 2 + topic_len + 1;
    if (buf_len < 2 + remaining_len)
        return -1;
    int pos = 0;
    buf[pos++] = 0x82;
    buf[pos++] = remaining_len;
    buf[pos++] = 0x00;
    buf[pos++] = 0x01;
    buf[pos++] = 0x00;
    buf[pos++] = topic_len;
    memcpy(&buf[pos], topic, topic_len);
    pos += topic_len;
    buf[pos++] = 0x01;
    return pos;
}
