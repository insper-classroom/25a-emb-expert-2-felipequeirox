/*
 * FreeRTOS + LCD - VERSÃO FINAL FUNCIONAL
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <math.h>

#include "ili9341.h"

// Estrutura de dados dos sensores
typedef struct {
    float voltage;
    float resistance;
    float current;
} sensor_data_t;

// Variáveis globais
static sensor_data_t sensor_readings = {0.0, 0.0, 0.0};
static SemaphoreHandle_t data_mutex;

// Task para leitura de sensores
void sensor_task(void *p) {
    sensor_data_t local_data;
    
    printf("Sensor task started\n");
    
    while (1) {
        // Lê voltagem do ADC
        uint16_t adc_raw = adc_read();
        local_data.voltage = (adc_raw * 3.3f) / 4095.0f;
        
        // Simula resistência variável
        static float time_counter = 0;
        local_data.resistance = 1000 + 500 * sin(time_counter);
        time_counter += 0.1f;
        
        // Calcula corrente pela Lei de Ohm
        if (local_data.resistance > 0) {
            local_data.current = local_data.voltage / local_data.resistance * 1000; // mA
        } else {
            local_data.current = 0.0f;
        }
        
        // Atualiza dados compartilhados
        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensor_readings = local_data;
            xSemaphoreGive(data_mutex);
        }
        
        printf("V:%.2fV R:%.0fΩ I:%.2fmA\n", 
               local_data.voltage, local_data.resistance, local_data.current);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Task para controle do LCD
void display_task(void *p) {
    char buffer[32];
    
    printf("Display task started\n");
    
    // Inicializa LCD
    ili9341_init(spi0, 17, 20, 21);
    ili9341_set_rotation(1);
    
    // Interface inicial
    ili9341_fill_screen(BLACK);
    ili9341_set_cursor(50, 20);
    ili9341_set_text_color(CYAN, BLACK);
    ili9341_set_text_size(3);
    ili9341_write_string("SENSOR");
    
    ili9341_set_cursor(45, 50);
    ili9341_write_string("MONITOR");
    
    while (1) {
        sensor_data_t current_data;
        
        // Pega dados atuais protegido por mutex
        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            current_data = sensor_readings;
            xSemaphoreGive(data_mutex);
            
            // Limpa e atualiza display
            ili9341_fill_rect(20, 100, 280, 120, BLACK);
            
            // Voltagem em verde
            ili9341_set_cursor(20, 100);
            ili9341_set_text_size(2);
            ili9341_set_text_color(GREEN, BLACK);
            sprintf(buffer, "V: %.2f V", current_data.voltage);
            ili9341_write_string(buffer);
            
            // Resistência em amarelo
            ili9341_set_cursor(20, 130);
            ili9341_set_text_color(YELLOW, BLACK);
            sprintf(buffer, "R: %.0f Ohm", current_data.resistance);
            ili9341_write_string(buffer);
            
            // Corrente em cyan
            ili9341_set_cursor(20, 160);
            ili9341_set_text_color(CYAN, BLACK);
            sprintf(buffer, "I: %.2f mA", current_data.current);
            ili9341_write_string(buffer);
            
            // Bargraph da voltagem
            int bar_width = (int)((current_data.voltage / 3.3f) * 200);
            ili9341_fill_rect(20, 200, 200, 15, BLACK);
            ili9341_fill_rect(20, 200, bar_width, 15, GREEN);
            ili9341_draw_rect(20, 200, 200, 15, WHITE);
            
            // Label do bargraph
            ili9341_set_cursor(20, 220);
            ili9341_set_text_size(1);
            ili9341_set_text_color(WHITE, BLACK);
            ili9341_write_string("Voltage Level 0-3.3V");
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main() {
    stdio_init_all();
    
    printf("=== LCD SENSOR MONITOR ===\n");
    
    // Aguarda estabilização
    sleep_ms(1000);
    
    // Inicializa ADC
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    
    // Inicializa SPI para LCD
    spi_init(spi0, 125000000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    gpio_set_function(16, GPIO_FUNC_SPI); // MISO
    gpio_set_function(19, GPIO_FUNC_SPI); // MOSI
    gpio_set_function(18, GPIO_FUNC_SPI); // CLK
    
    gpio_init(17); gpio_set_dir(17, GPIO_OUT); gpio_put(17, 1); // CS
    gpio_init(20); gpio_set_dir(20, GPIO_OUT);                  // DC
    gpio_init(21); gpio_set_dir(21, GPIO_OUT);                  // RST
    
    printf("Hardware initialized\n");
    
    // CORREÇÃO: Usa função correta do FreeRTOS
    data_mutex = xSemaphoreCreateMutex();  // ← FUNÇÃO CORRETA!
    
    if (data_mutex == NULL) {
        printf("ERRO: Failed to create mutex!\n");
        return -1;
    }
    
    printf("Mutex created successfully\n");
    
    // Cria tasks
    if (xTaskCreate(sensor_task, "Sensor", 512, NULL, 2, NULL) != pdPASS) {
        printf("ERRO: Failed to create sensor task!\n");
        return -1;
    }
    
    if (xTaskCreate(display_task, "Display", 1024, NULL, 2, NULL) != pdPASS) {
        printf("ERRO: Failed to create display task!\n");
        return -1;
    }
    
    printf("Tasks created successfully\n");
    printf("Starting FreeRTOS scheduler...\n");
    printf("Connect potentiometer to GP26 for voltage reading\n");
    
    // Inicia o scheduler
    vTaskStartScheduler();
    
    // Nunca deve chegar aqui
    while (true) {
        printf("ERROR: Scheduler stopped!\n");
        sleep_ms(1000);
    }
    
    return 0;
}