#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"

// Arquivo .pio
#include "pio_matrix.pio.h"

// Mapeamento de hardware
const uint RED = 13; // LED vermelho
const uint MATRIZ = 7; // Matriz de LED
const uint BUTTON_A = 5; // Botão A
const uint BUTTON_B = 6; // Botão B

static volatile uint32_t last_time = 0; // Variável de tempo
static volatile char flag = 0; // Flag para controle de contagem

// Prototipação da função de interrupção
static void gpio_irq_handler(uint gpio, uint32_t events);

//Função para criar o temporizador do LED vermelhor
bool repeating_timer_callback(struct repeating_timer *t) { 
    gpio_put(RED, !gpio_get(RED));
    return true;
}

// Frames utilizados na matriz de LED
double frames[10][25] = {
        {0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000},

        {0.000, 0.000, 0.034, 0.000, 0.000,
         0.000, 0.000, 0.034, 0.000, 0.000,
         0.000, 0.000, 0.034, 0.000, 0.000,
         0.000, 0.000, 0.034, 0.000, 0.000,
         0.000, 0.000, 0.034, 0.000, 0.000},

        {0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.000, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.000, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000},

        {0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.000, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.000, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000},

        {0.000, 0.034, 0.000, 0.000, 0.000,
         0.000, 0.000, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000},

        {0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.000, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.000, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000},

        {0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.000, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000},

        {0.000, 0.034, 0.000, 0.000, 0.000,
         0.000, 0.000, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.000, 0.000,
         0.000, 0.000, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000},

        {0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000},

        {0.000, 0.034, 0.000, 0.000, 0.000,
         0.000, 0.000, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000,
         0.000, 0.034, 0.000, 0.034, 0.000,
         0.000, 0.034, 0.034, 0.034, 0.000},
    };

// Inicializar função de contagem para a matriz de LEDs
void contagem(PIO pio, uint sm);

// Função para criar o binário para a matriz de LEDs
uint32_t matriz_rgb(double r, double g, double b) {
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G << 24) | (R << 16) | (B << 8);
}

int main()
{
    // Setando o clock do programa para 128 kHz
    bool set_clock;
    set_clock = set_sys_clock_khz(128000, false);

    // Inicializando as variavéis para a Matriz de LEDs
    PIO pio = pio0; 
    uint32_t valor_led;
    double r = 0.000, b = 0.000, g = 0.000;
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, MATRIZ);

    stdio_init_all();
  
    gpio_init(RED); // Inicializa o pino do LED
    gpio_set_dir(RED, GPIO_OUT); // Configura o pino como saída
                                      
    gpio_init(BUTTON_A); // Inicializa o Botão A
    gpio_set_dir(BUTTON_A, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BUTTON_A); // Habilita o pull-up interno

    gpio_init(BUTTON_B); // Inicializa o Botão B
    gpio_set_dir(BUTTON_B, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BUTTON_B); // Habilita o pull-up interno

    // Interrupção por borda de descida para o Botão A e Botão B
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Configura o temporizador para chamar a função de callback a cada 1 segundo.
    struct repeating_timer timer;
    add_repeating_timer_ms(200, repeating_timer_callback, NULL, &timer);

    while (true) {
        // Inicializa a matriz de LEDs
        contagem(pio, sm);
        sleep_ms(50);
    }
}

// Função realizada na interrupção
void gpio_irq_handler(uint gpio, uint32_t events)
{
    // Obtém o tempo atual em microssegundos
    uint32_t current_time = to_us_since_boot(get_absolute_time());
  
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time > 200000) {// 200 ms de debouncing
        last_time = current_time; // Atualiza o tempo do último evento
        if (gpio == BUTTON_A){ // Se o Botão A foi pressionado, aumenta o valor se for menor que 9
            if (flag < 9)
                flag += 1;
        }
        if (gpio == BUTTON_B){ 
            if (flag > 0) // Senão, o Botão B foi pressionado e diminui o valor se for maior que 0
                flag -= 1;
        }
    }
}

void contagem(PIO pio, uint sm) {
        for (int i = 0; i < 25; i++) {
            double r = frames[flag][i]; // Intensidade de cada pixel vermelho no frame
            double g = frames[flag][i]; // Intensidade de cada pixel verde no frame
            double b = frames[flag][i]; // Intensidade de cada pixel azul no frame
            uint32_t color = matriz_rgb(r, g, b);
            pio_sm_put_blocking(pio, sm, color);
        }
    }