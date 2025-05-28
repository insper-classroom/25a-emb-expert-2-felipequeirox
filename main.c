#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

// Bibliotecas para o display ILI9341
#include "tft_lcd_ili9341/ili9341/ili9341.h"
#include "tft_lcd_ili9341/gfx/gfx.h"

// Biblioteca para o touch resistivo
#include "tft_lcd_ili9341/touch_resistive/touch_resistive.h"

// === Definições para ILI9341 ===
const uint LITE = 15;         // Pino de controle da luz de fundo (backlight)
#define SCREEN_WIDTH 240      // Largura da tela em pixels

int main(void) {
    stdio_init_all();         // Inicializa entrada/saída padrão (USB serial)

    // Inicialização do display LCD e sistema gráfico
    LCD_initDisplay();        // Inicializa o controlador do display
    LCD_setRotation(0);       // Define a rotação da tela (0 = retrato padrão)
    GFX_createFramebuf();     // Cria um framebuffer em memória para renderização

    configure_touch();        // Inicializa o sistema de leitura do touch resistivo

    // Configura o pino de backlight como saída e ativa a luz de fundo
    gpio_init(LITE);
    gpio_set_dir(LITE, GPIO_OUT);
    gpio_put(LITE, 1);        // Liga o backlight (nível alto)

    int px, py;               // Variáveis para armazenar coordenadas do toque

    while (true) {
        // === Parte de atualização gráfica da tela ===
        GFX_clearScreen();              // Limpa o framebuffer
        GFX_setCursor(0, 10);           // Define posição do texto
        GFX_printf("Touch Demo\n");     // Mostra título/demonstração

        // Verifica se há toque na tela e lê as coordenadas
        if (readPoint(&px, &py)) {
            px = SCREEN_WIDTH - px;    // Inverte eixo X, para exibir o valor correto
            GFX_printf("X:%03d Y:%03d\n", px, py);  // Exibe coordenadas do toque
        } else {
            GFX_printf("Sem toque\n"); // Mensagem quando não há toque detectado
        }

        GFX_flush();       // Atualiza o display com o conteúdo do framebuffer
        sleep_ms(1);       // Pequeno atraso para evitar uso excessivo da CPU
    }
}