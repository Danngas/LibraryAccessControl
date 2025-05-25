/*
 * Painel de Controle Interativo - Display OLED, Botão A e Botão B
 * Autor: Daniel Silva de Souza
 * Data: 25/05/2025
 * Descrição: Sistema de controle de acesso usando display OLED SSD1306,
 * mutex para sincronização de tarefas e semáforos para entrada e saída de usuários.
 * Implementa botão A (entrada) e botão B (saída), com feedback visual no display.
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
#define BOTAO_A 5 // Pino do botão A (entrada)
#define BOTAO_B 6 // Pino do botão B (saída)
#define MAX_usuarios 8 // Capacidade máxima de usuários

/* Variáveis Globais */
ssd1306_t ssd; // Estrutura para o display OLED
SemaphoreHandle_t xDisplayMutex;   // Mutex para proteger o acesso ao display
SemaphoreHandle_t xContadorSem;   // Semáforo de contagem para entradas
SemaphoreHandle_t xSaidaSem;      // Semáforo de contagem para saídas
volatile uint16_t usuariosAtivos = 0; // Contagem atual de usuários

//DEbouncing 
absolute_time_t ultimoA = 0;
absolute_time_t ultimoB = 0;
const uint32_t DEBOUNCE_US = 200000; // 200 ms


/* Função para atualizar o display com mutex */
void update_display(const char* msg, uint16_t count) {
    if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE) {
        char buffer[32];
        ssd1306_fill(&ssd, 0); // Limpa o buffer do display
        ssd1306_draw_string(&ssd, msg, 5, 20); // Exibe a mensagem principal
        snprintf(buffer, sizeof(buffer), "Usuarios: %d", count); // Formata o número de usuários
        ssd1306_draw_string(&ssd, buffer, 5, 40); // Exibe a contagem
        ssd1306_send_data(&ssd); // Atualiza o display
        xSemaphoreGive(xDisplayMutex); // Libera o mutex
    }
}

/* Interrupções para os botões A e B */
void gpio_irq_handler(uint gpio, uint32_t events) {
    absolute_time_t agora = get_absolute_time();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (gpio == BOTAO_A && (events & GPIO_IRQ_EDGE_FALL)) {
        if (absolute_time_diff_us(ultimoA, agora) > DEBOUNCE_US) {
            ultimoA = agora;
            xSemaphoreGiveFromISR(xContadorSem, &xHigherPriorityTaskWoken);
        }
    }

    if (gpio == BOTAO_B && (events & GPIO_IRQ_EDGE_FALL)) {
        if (absolute_time_diff_us(ultimoB, agora) > DEBOUNCE_US) {
            ultimoB = agora;
            xSemaphoreGiveFromISR(xSaidaSem, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Tarefa de Entrada (Botão A) */
void vTaskEntrada(void *params) {
    while (true) {
        if (xSemaphoreTake(xContadorSem, portMAX_DELAY) == pdTRUE) {
            if (usuariosAtivos < MAX_usuarios) {
                usuariosAtivos++; // Incrementa a contagem
                update_display("Entrada!", usuariosAtivos); // Mostra mensagem no display
            } else {
                update_display("Capacidade Maxima!", usuariosAtivos); // Sistema cheio
                // TODO: adicionar beep curto
            }
        }
    }
}

/* Tarefa de Saída (Botão B) */
void vTaskSaida(void *params) {
    while (true) {
        if (xSemaphoreTake(xSaidaSem, portMAX_DELAY) == pdTRUE) {
            if (usuariosAtivos > 0) {
                usuariosAtivos--; // Decrementa a contagem
                update_display("Saida!", usuariosAtivos); // Mostra mensagem no display
            } else {
                update_display("Nenhum usuario!", usuariosAtivos); // Ninguém para sair
                // TODO: adicionar beep de erro
            }
        }
    }
}

/* Tarefa periódica para atualizar o display com status */
void vDisplayTask(void *params) {
    while (true) {
        update_display("Controle de Acesso", usuariosAtivos); // Atualização contínua
        vTaskDelay(pdMS_TO_TICKS(1000)); // Espera 1 segundo
    }
}

/* Função principal */
int main() {
    stdio_init_all(); // Inicializa a saída serial (debug)

    /* Inicialização do barramento I2C e do display OLED */
    i2c_init(I2C_PORT, 400 * 1000); // Frequência de 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, 128, 64, false, ENDERECO_OLED, I2C_PORT); // Display 128x64
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd); // Primeira atualização

    /* Configuração do botão A (entrada) */
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    /* Configuração do botão B (saída) */
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);

    /* Criação de mutex e semáforos */
    xDisplayMutex = xSemaphoreCreateMutex(); // Protege o display OLED
    xContadorSem = xSemaphoreCreateCounting(10, 0); // Até 10 eventos de entrada
    xSaidaSem = xSemaphoreCreateCounting(10, 0);    // Até 10 eventos de saída

    /* Criação das tarefas */
    xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "SaidaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "DisplayTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    /* Inicia o escalonador FreeRTOS */
    vTaskStartScheduler();

    /* Caso o escalonador falhe */
    panic_unsupported();
}
