#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h" 

// Bibliotecas do display
#include "gfx.h"
#include "ili9341.h"

// Definições dos pinos SPI para o display
#define TFT_DC   20
#define TFT_CS   17
#define TFT_RST  21
#define TFT_MOSI 19
#define TFT_MISO 16
#define TFT_CLK  18

// Definições do LDR
#define LDR_PIN 26
#define LDR_ADC_CHANNEL 0  // GPIO26 = ADC0
#define FIXED_RESISTOR 10000.0  // Resistor fixo do divisor de tensão (10k ohms)
#define VCC 3.3  // Tensão de alimentação

// Cores para o display
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// Estrutura para dados do sensor
typedef struct {
    float voltage;
    float resistance;
    float current;
} SensorData;

// Variáveis globais
Adafruit_ILI9341 *tft;
QueueHandle_t sensorQueue;
SemaphoreHandle_t displayMutex;

// Função para inicializar o display
void init_display() {
    // Configurar pinos SPI
    gpio_init(TFT_CS);
    gpio_set_dir(TFT_CS, GPIO_OUT);
    gpio_put(TFT_CS, 1);
    
    gpio_init(TFT_DC);
    gpio_set_dir(TFT_DC, GPIO_OUT);
    
    gpio_init(TFT_RST);
    gpio_set_dir(TFT_RST, GPIO_OUT);
    
    // Inicializar SPI
    spi_init(spi0, 40000000);  // 40MHz
    gpio_set_function(TFT_MISO, GPIO_FUNC_SPI);
    gpio_set_function(TFT_CLK, GPIO_FUNC_SPI);
    gpio_set_function(TFT_MOSI, GPIO_FUNC_SPI);
    
    // Criar e inicializar o display
    tft = new Adafruit_ILI9341(spi0, TFT_DC, TFT_CS, TFT_RST);
    tft->begin();
    tft->setRotation(3);  // Landscape
    tft->fillScreen(BLACK);
    
    // Título
    tft->setTextColor(WHITE);
    tft->setTextSize(2);
    tft->setCursor(50, 10);
    tft->print("Monitor LDR");
    
    // Labels
    tft->setTextSize(2);
    tft->setTextColor(CYAN);
    tft->setCursor(10, 60);
    tft->print("Voltagem:");
    
    tft->setCursor(10, 100);
    tft->print("Resistencia:");
    
    tft->setCursor(10, 140);
    tft->print("Corrente:");
}

// Função para inicializar o ADC
void init_adc() {
    adc_init();
    adc_gpio_init(LDR_PIN);
    adc_select_input(LDR_ADC_CHANNEL);
}

// Função para ler o LDR e calcular valores
SensorData read_ldr() {
    SensorData data;
    
    // Ler valor do ADC (0-4095 para 12 bits)
    uint16_t adc_value = adc_read();
    
    // Calcular voltagem no LDR
    data.voltage = (adc_value * VCC) / 4095.0;
    
    // Calcular resistência do LDR usando divisor de tensão
    // Vout = Vin * (R2 / (R1 + R2))
    // R2 = R1 * Vout / (Vin - Vout)
    if (data.voltage < VCC && data.voltage > 0) {
        data.resistance = FIXED_RESISTOR * data.voltage / (VCC - data.voltage);
    } else {
        data.resistance = 0;
    }
    
    // Calcular corrente usando Lei de Ohm
    if (data.resistance > 0) {
        data.current = data.voltage / data.resistance * 1000;  // mA
    } else {
        data.current = 0;
    }
    
    return data;
}

// Task para leitura do sensor
void vTaskSensor(void *pvParameters) {
    SensorData data;
    
    while (1) {
        // Ler dados do sensor
        data = read_ldr();
        
        // Enviar dados para a fila
        xQueueSend(sensorQueue, &data, portMAX_DELAY);
        
        // Delay de 100ms entre leituras
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task para atualizar o display
void vTaskDisplay(void *pvParameters) {
    SensorData data;
    char buffer[50];
    
    while (1) {
        // Receber dados da fila
        if (xQueueReceive(sensorQueue, &data, portMAX_DELAY) == pdTRUE) {
            // Obter mutex do display
            if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
                
                // Limpar área dos valores
                tft->fillRect(180, 60, 130, 120, BLACK);
                
                // Mostrar voltagem
                tft->setTextColor(YELLOW);
                tft->setCursor(180, 60);
                sprintf(buffer, "%.2f V", data.voltage);
                tft->print(buffer);
                
                // Mostrar resistência
                tft->setCursor(180, 100);
                if (data.resistance < 1000) {
                    sprintf(buffer, "%.1f ohm", data.resistance);
                } else {
                    sprintf(buffer, "%.1f k", data.resistance / 1000.0);
                }
                tft->print(buffer);
                
                // Mostrar corrente
                tft->setCursor(180, 140);
                sprintf(buffer, "%.2f mA", data.current);
                tft->print(buffer);
                
                // Desenhar barra de luz (baseada na resistência)
                int light_level = map(data.resistance, 100, 10000, 100, 0);
                light_level = constrain(light_level, 0, 100);
                
                // Limpar área da barra
                tft->fillRect(10, 190, 300, 30, BLACK);
                
                // Desenhar borda da barra
                tft->drawRect(10, 190, 300, 30, WHITE);
                
                // Preencher barra de acordo com nível de luz
                int bar_width = (light_level * 296) / 100;
                uint16_t bar_color = (light_level > 70) ? GREEN : 
                                    (light_level > 30) ? YELLOW : RED;
                tft->fillRect(12, 192, bar_width, 26, bar_color);
                
                // Mostrar percentual
                tft->setTextColor(WHITE);
                tft->setCursor(130, 195);
                sprintf(buffer, "Luz: %d%%", light_level);
                tft->print(buffer);
                
                // Liberar mutex
                xSemaphoreGive(displayMutex);
            }
        }
    }
}

// Função auxiliar para mapear valores
int map(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Função auxiliar para limitar valores
int constrain(int x, int a, int b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

int main() {
    // Inicializar stdio
    stdio_init_all();
    
    // Aguardar inicialização
    sleep_ms(2000);
    
    printf("Iniciando sistema...\n");
    
    // Inicializar hardware
    init_display();
    init_adc();
    
    // Criar fila para comunicação entre tasks
    sensorQueue = xQueueCreate(10, sizeof(SensorData));
    
    // Criar mutex para acesso ao display
    displayMutex = xSemaphoreCreateMutex();
    
    // Criar tasks
    xTaskCreate(vTaskSensor, "Sensor Task", 256, NULL, 1, NULL);
    xTaskCreate(vTaskDisplay, "Display Task", 512, NULL, 1, NULL);
    
    printf("Tasks criadas, iniciando scheduler...\n");
    
    // Iniciar o scheduler do FreeRTOS
    vTaskStartScheduler();
    
    // Nunca deve chegar aqui
    while (1) {
        tight_loop_contents();
    }
    
    return 0;
}