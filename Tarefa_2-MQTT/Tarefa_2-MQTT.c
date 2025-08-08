#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"

// ===== DEFINIÇÕES DOS PINOS ====
const uint LED_GREEN = 11;
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
const int VRX = 26;
const int VRY = 27;

uint8_t ssd[ssd1306_buffer_length];
struct render_area frame_area = {
    .start_column = 0,
    .end_column = ssd1306_width - 1,
    .start_page = 0,
    .end_page = ssd1306_n_pages - 1};

// ===== CONFIGURAÇÕES DO WIFI=====
#define WIFI_SSID "iPhone (2)"
#define WIFI_PASSWORD "12345678"

// ===== CONFIGURAÇÕES DO MQTT=====
#define MQTT_SERVER "mqtt.iot.natal.br"
#define MQTT_PORT 1883
#define MQTT_USER "desafio20"
#define MQTT_PASS "desafio20.laica"
#define MQTT_CLIENT_ID "anderson.dantas"
#define MQTT_TOPIC_TEMP "ha/desafio20/anderson.dantas/temp"
#define MQTT_TOPIC_JOY "ha/desafio20/anderson.dantas/joy"

#define MSG_INTERVAL_MS 5000

// ===== VARIÁVEIS GLOBAIS =====
mqtt_client_t *client;
ip_addr_t mqtt_server_ip;
bool mqtt_connected = false;

// ===== TEMPERATURA ===== 

typedef struct
{
    float temperature;
    char movement[16];
} screenInfo;

QueueHandle_t displayQueue;

// ===== FUNÇÃO PARA LER A TEMPERATURA DO SENSOR INTERNO =====
float read_onboard_temperature()
{
    const float conversion_factor = 3.3f / (1 << 12);
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

// ===== TAREFA PARA ATAUALIZAR O DISPLAY OLED =====
void vdisplayTask(void *pvParameters)
{
    screenInfo data;
    for (;;)
    {
        if (xQueuePeek(displayQueue, &data, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            memset(ssd, 0, ssd1306_buffer_length);

            char tempStr[20];
            snprintf(tempStr, sizeof(tempStr), " %.1f°C", data.temperature);

            ssd1306_draw_line(ssd, 10, 0, 110, 0, true);
            ssd1306_draw_line(ssd, 110, 0, 110, 15, true);
            ssd1306_draw_line(ssd, 10, 0, 10, 15, true);
            ssd1306_draw_line(ssd, 10, 15, 110, 15, true);

            ssd1306_draw_string_scaled(ssd, 15, 5, "Temperatura:", 1);
            ssd1306_draw_string_scaled(ssd, 25, 20, tempStr, 1);

            ssd1306_draw_line(ssd, 20, 35, 100, 35, true);
            ssd1306_draw_line(ssd, 100, 35, 100, 49, true);
            ssd1306_draw_line(ssd, 20, 35, 20, 49, true);
            ssd1306_draw_line(ssd, 20, 49, 100, 49, true);

            ssd1306_draw_string_scaled(ssd, 25, 39, "Joystick:", 1);
            ssd1306_draw_string_scaled(ssd, 25, 55, data.movement, 1);

            if (strcmp(data.movement, "Cima") == 0)
            {
                ssd1306_draw_line(ssd, 70, 60, 70, 55, true);
                ssd1306_draw_line(ssd, 70, 52, 67, 55, true);
                ssd1306_draw_line(ssd, 70, 52, 73, 55, true);
            }
            else if (strcmp(data.movement, "Baixo") == 0)
            {
                ssd1306_draw_line(ssd, 75, 63, 75, 55, true);
                ssd1306_draw_line(ssd, 75, 63, 72, 60, true);
                ssd1306_draw_line(ssd, 75, 63, 78, 60, true);
            }

            else if (strcmp(data.movement, "Esquerda") == 0)
            {
                ssd1306_draw_line(ssd, 95, 58, 103, 58, true);
                ssd1306_draw_line(ssd, 95, 58, 98, 55, true);
                ssd1306_draw_line(ssd, 95, 58, 98, 61, true);
            }
            else if (strcmp(data.movement, "Direita") == 0)
            {
                ssd1306_draw_line(ssd, 90, 58, 98, 58, true);
                ssd1306_draw_line(ssd, 95, 55, 98, 58, true);
                ssd1306_draw_line(ssd, 95, 61, 98, 58, true);
            }
            render_on_display(ssd, &frame_area);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
// ===== CALLBACKS MQTT =====

volatile bool mqtt_ready_to_publish = false;

void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        printf("MQTT conectado com sucesso.\n");
        mqtt_connected = true;
        mqtt_ready_to_publish = true; // sinaliza que pode publicar
    }
    else
    {
        printf("Falha ao conectar no MQTT. Código: %d\n", status);
        mqtt_connected = false;
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

// ===== t
void vjoystick(void *pvParameters)
{
    char ultimaDirecao[16] = "";

    for (;;)
    {
        adc_select_input(0); // Y
        uint adc_y_raw = adc_read();
        adc_select_input(1); // X
        uint adc_x_raw = adc_read();

        screenInfo data;
        if (xQueuePeek(displayQueue, &data, pdMS_TO_TICKS(50)) == pdTRUE)
        {

            if (adc_y_raw < 500)
                strcpy(data.movement, "Baixo");
            else if (adc_y_raw > 3000)
                strcpy(data.movement, "Cima");
            else if (adc_x_raw < 500)
                strcpy(data.movement, "Esquerda");
            else if (adc_x_raw > 3000)
                strcpy(data.movement, "Direita");

            xQueueOverwrite(displayQueue, &data);
            // Publica no MQTT somente se a direção mudou
            if (mqtt_connected && strcmp(data.movement, ultimaDirecao) && mqtt_ready_to_publish != 0)
            {
                char msg[32];
                snprintf(msg, sizeof(msg), "%s", data.movement);

                err_t err = mqtt_publish(client, MQTT_TOPIC_JOY, msg, strlen(msg), 0, 1, NULL, NULL);
                if (err == ERR_OK)
                {
                    printf("Joystick publicado: %s\n", msg);

                    gpio_put(LED_GREEN, 1);    // ACENDE O LED VERDE
                    vTaskDelay(pdMS_TO_TICKS(50)); // ESPERA 50 ms
                    gpio_put(LED_GREEN, 0);    // APAGA O LED VERDE
                }

                else
                    printf("Erro ao publicar joystick: %d\n", err);

                strcpy(ultimaDirecao, data.movement);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ===== TAREFA PARA FAZER A LEITURA DO SENSOR DE TEMPERATURA =====
void vSensorTask(void *pvParameters)
{
    bool primeiraVez = true;
     vTaskDelay(pdMS_TO_TICKS(1000));
    for (;;)
    {
       
        float temp = read_onboard_temperature();

        screenInfo data;
        if (xQueuePeek(displayQueue, &data, pdMS_TO_TICKS(50)) == pdTRUE && mqtt_connected)
        {
            data.temperature = temp;
            xQueueOverwrite(displayQueue, &data);

            if (mqtt_ready_to_publish && mqtt_connected && mqtt_ready_to_publish != 0)
            {
                char msg[64];
                snprintf(msg, sizeof(msg), " %.0f", data.temperature);
                err_t err = mqtt_publish(client, MQTT_TOPIC_TEMP, msg, strlen(msg), 0, 1, NULL, NULL);
                if (err == ERR_OK)
                {
                    printf("temp: %s\n", msg);
                    gpio_put(LED_GREEN, 1);    // ACENDE O LED VERDE
                    vTaskDelay(pdMS_TO_TICKS(50)); // ESPERA 50 ms
                    gpio_put(LED_GREEN, 0);    // APAGA O LED VERDE
                }
                else
                {
                    printf("Erro ao publicar: %d\n", err);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(30000)); 
        }
    }
}

int main()
{
    stdio_init_all();
    sleep_ms(2000);
    // Inicialização do LED
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    // Inicialização do ADC (joystick + temperatura)
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);

    // Leitura do sensor
    adc_set_temp_sensor_enabled(true);

    // Inicialização do I2C e display
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();
    calculate_render_area_buffer_length(&frame_area);

    // Criação da fila
    displayQueue = xQueueCreate(1, sizeof(screenInfo));
    if (displayQueue == NULL)
    {
        printf("Não foi possivel criar fila.\n");
        while (1)
            ;
    }
    // Primeiro valor da fila
    screenInfo init_data = {.temperature = 0.0f, .movement = ""};
    xQueueOverwrite(displayQueue, &init_data);

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
    // Criação das tarefas
    xTaskCreate(vSensorTask, "Sensor Task", 256, NULL, 1, NULL);
    xTaskCreate(vjoystick, "Joystick Task", 256, NULL, 1, NULL);
    xTaskCreate(vdisplayTask, "Display Task", 256, NULL, 1, NULL);
    

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();
    // Loop principal
    while (1)
    {
        sleep_ms(100);
    }

    return 0;
}
