#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"

// Definições de pinos
const uint led_pin_blue = 12;
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


typedef struct
{
    float temperature;
    char movement[16];
} screenInfo;

QueueHandle_t displayQueue;

// Função para ler temperatura do sensor interno
float read_onboard_temperature()
{
    const float conversion_factor = 3.3f / (1 << 12);
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * conversion_factor;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

// Tarefa para atualizar o display OLED
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

// Tarefa para piscar o LED azul
void vLEDTask(void *pvParameters)
{
    for (;;)
    {
        gpio_put(led_pin_blue, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_put(led_pin_blue, 0);
        vTaskDelay(pdMS_TO_TICKS(950));
    }
}

// Tarefa para ler a posição do joystick
void vjoystick(void *pvParameters)
{
    for (;;)
    {
        adc_select_input(0); // Y
        uint adc_y_raw = adc_read();
        adc_select_input(1); // X
        uint adc_x_raw = adc_read();

        screenInfo data;
        if (xQueuePeek(displayQueue, &data, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            if (adc_y_raw < 300)
                strcpy(data.movement, "Baixo");
            else if (adc_y_raw > 3000)
                strcpy(data.movement, "Cima");
            else if (adc_x_raw < 300)
                strcpy(data.movement, "Esquerda");
            else if (adc_x_raw > 3000)
                strcpy(data.movement, "Direita");

            xQueueOverwrite(displayQueue, &data);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Tarefa para ler o sensor de temperatura
void vSensorTask(void *pvParameters)
{
    for (;;)
    {
        float temp = read_onboard_temperature();

        screenInfo data;
        if (xQueuePeek(displayQueue, &data, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            data.temperature = temp;
            xQueueOverwrite(displayQueue, &data);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void main()
{
    stdio_init_all();

    // Inicialização do LED
    gpio_init(led_pin_blue);
    gpio_set_dir(led_pin_blue, GPIO_OUT);

    // Inicialização do ADC (joystick + temperatura)
    adc_init();
    adc_gpio_init(VRX);
    adc_gpio_init(VRY);

    //Leitura do sensor
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
    //Primeiro valor da fila
    screenInfo init_data = {.temperature = 0.0f, .movement = "Aguardando"};
    xQueueOverwrite(displayQueue, &init_data);

    // Criação das tarefas
    xTaskCreate(vLEDTask, "LED Task", 128, NULL, 1, NULL);
    xTaskCreate(vjoystick, "Joystick Task", 256, NULL, 1, NULL);
    xTaskCreate(vdisplayTask, "Display Task", 256, NULL, 1, NULL);
    xTaskCreate(vSensorTask, "Sensor Task", 256, NULL, 1, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();

    // Nunca deve chegar aqui
    while (1)
        ;
}
