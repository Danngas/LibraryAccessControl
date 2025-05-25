/*
 * Painel de Controle Interativo - Display OLED e Botão A
 * Autor: Daniel Silva de Souza
 * Data: 25/05/2025
 * Descrição: Configuração do display OLED SSD1306 com mutex e botão A para
 * incrementar contagem de usuários usando semáforo de contagem.
 */

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

/* Definições de Hardware */
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO_OLED 0x3C
#define BOTAO_A 5 // Pino do botão A
#define MAX_usuarios 8 // Máximo de usuários

/* Variáveis Globais */
ssd1306_t ssd; // Estrutura para o display OLED
SemaphoreHandle_t xDisplayMutex; // Mutex para proteger o display
SemaphoreHandle_t xContadorSem; // Semáforo de contagem para entradas
volatile uint16_t usuariosAtivos = 0; // Contagem inicial de usuários

/* Função para atualizar o display com mutex */
void update_display(const char* msg, uint16_t count) {
    if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
        char buffer[32];
        ssd1306_fill(&ssd, 0); // Limpa o buffer
        ssd1306_draw_string(&ssd, msg, 5, 20); // Exibe mensagem
        snprintf(buffer, sizeof(buffer), "Usuarios: %d", count); // Formata contagem
        ssd1306_draw_string(&ssd, buffer, 5, 40); // Exibe contagem
        ssd1306_send_data(&ssd); // Atualiza o display
        xSemaphoreGive(xDisplayMutex); // Libera o mutex
    }
}

/* ISR para o botão A */
void gpio_irq_handler(uint gpio, uint32_t events) {
    if (gpio == BOTAO_A && events & GPIO_IRQ_EDGE_FALL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xContadorSem, &xHigherPriorityTaskWoken); // Libera o semáforo
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken); // Troca de contexto, se necessário
    }
}

/* Tarefa de Entrada (Botão A) */
void vTaskEntrada(void *params) {
    while (true) {
        if (xSemaphoreTake(xContadorSem, portMAX_DELAY) == pdTRUE) {
            if (usuariosAtivos < MAX_usuarios) {
                usuariosAtivos++; // Incrementa contagem
                update_display("Entrada!", usuariosAtivos); // Atualiza display
            } else {
                update_display("Capacidade Maxima!", usuariosAtivos); // Mensagem de limite
                // Placeholder para beep curto (implementaremos depois)
            }
        }
    }
}

/* Tarefa do Display */
void vDisplayTask(void *params) {
    while (true) {
        update_display("Controle de Acesso", usuariosAtivos); // Mensagem inicial
        vTaskDelay(pdMS_TO_TICKS(1000)); // Atualiza a cada 1s
    }
}

/* Função Principal */
int main() {
    stdio_init_all(); // Inicializa comunicação serial

    /* Inicializa I2C e display OLED */
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, 128, 64, false, ENDERECO_OLED, I2C_PORT); // 128x64, sem VCC externo
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    /* Configura o botão A */
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    /* Cria o mutex do display e o semáforo de contagem */
    xDisplayMutex = xSemaphoreCreateMutex();
    xContadorSem = xSemaphoreCreateCounting(10, 0); // Máximo 10 eventos, inicial 0

    /* Cria as tarefas */
    xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "DisplayTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    /* Inicia o escalonador */
    vTaskStartScheduler();
    panic_unsupported(); // Caso o escalonador falhe
}