#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "hardware/adc.h" 
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "inc/ssd1306.h"
#include <string.h>



const uint led_pin_blue = 12;
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
// Definição dos pinos usados para o joystick
int VRX ;          // Pino de leitura do eixo X do joystick (conectado ao ADC)
int VRY;          // Pino de leitura do eixo Y do joystick (conectado ao ADC)
const int ADC_CHANNEL_0 = 0; // Canal ADC para o eixo X do joystick
const int ADC_CHANNEL_1 = 1; // Canal ADC para o eixo Y do joystick
const uint16_t PERIOD = 4096;            // Período do PWM (valor máximo do contador)

uint8_t ssd[ssd1306_buffer_length];
struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
};
char *text[] = {
        "  Teperatura   ",
        "  joystick ",
        "Sem movimento"};

void vdisplayTask()
{
    for (;;)
    {
    memset(ssd, 0, ssd1306_buffer_length);
    int y = 5; // Linha inicial

    // Linha inicial
    ssd1306_draw_string_scaled(ssd, 5, y, text[0], 1); // Mostra o texto com tamanho 3x
    ssd1306_draw_string_scaled(ssd, 15, y+30, text[1], 1); // Mostra o texto com tamanho 3x
    ssd1306_draw_string_scaled(ssd, 15, y+45, text[2], 1); // Mostra o texto com tamanho 3x

    y += 8;
    render_on_display(ssd, &frame_area);
    }
}

void vLEDTask()
{

    for (;;)
    {

        gpio_put(led_pin_blue, 1);

        vTaskDelay(50);

        gpio_put(led_pin_blue, 0);

        vTaskDelay(950);

        printf("LEDing\n");
    }
}

void vjoystick(){
    for (;;)
    {
        adc_select_input(1); // Seleciona eixo X (canal 0)
        uint adc_x_raw = adc_read();
        adc_select_input(0); // Seleciona eixo Y (canal 1)
        uint adc_y_raw = adc_read();

        
            // Movimenta para cima
    if (adc_y_raw < 100)
    {
        printf("Ultimo para baixo\n");
        text[2]= "   Baixo";
        vTaskDelay(300);
    }
    // Movimenta para baixo
    else if (adc_y_raw > 3000)
    {
        printf("Ultimo para cima\n");
        text[2]= "  Acima";
        vTaskDelay(300);
    }

    if (adc_x_raw < 100)
    {
        printf("Ultimo para <-\n");
        text[2]= "   Esquerda";
        vTaskDelay(300);
    }
    // Movimenta para baixo
    else if (adc_x_raw > 3000)
    {
        printf("Ultimo para ->\n");
        text[2]= "   Direita";
        vTaskDelay(300);
    }
        
    }
    
}

void main()
{

    stdio_init_all();

    gpio_init(led_pin_blue);

    gpio_set_dir(led_pin_blue, GPIO_OUT);

// Inicializa o ADC e os pinos de entrada analógica
    adc_init();         // Inicializa o módulo ADC
    adc_gpio_init(VRX); // Configura o pino VRX (eixo X) para entrada ADC
    adc_gpio_init(VRY); // Configura o pino VRY (eixo Y) para entrada ADC
    adc_select_input(1); // Seleciona entrada ADC1 (eixo Y)
    adc_select_input(0);
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();

    calculate_render_area_buffer_length(&frame_area);

    xTaskCreate(vLEDTask, "LED Task", 128, NULL, 1, NULL);
    xTaskCreate(vjoystick, "joystick Task", 128, NULL, 1, NULL);
    xTaskCreate(vdisplayTask, "Display Task", 128, NULL, 1, NULL);

    vTaskStartScheduler();
}