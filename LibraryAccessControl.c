/*
 * Painel de Controle Interativo - Controle de Acesso a Laboratório
 * Autor: Daniel Silva de Azevedo
 * Data: 26/05/2025
 * Descrição: Sistema de controle de acesso para laboratório usando BitDogLab (RP2040) com FreeRTOS.
 *            Utiliza display OLED, botões A/B, joystick, LED RGB, buzzer e matriz WS2812B 5x5.
 *            Implementa entrada/saída de usuários, reset, feedback visual/sonoro e animações.
 */

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include <stdio.h>
#include "animacoes.h"

/* Definições de Hardware */
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO_OLED 0x3C
#define BOTAO_A 5        // Botão A: entrada de usuário
#define BOTAO_B 6        // Botão B: saída de usuário
#define JOYSTICK 22      // Joystick: reset do sistema
#define LED_R 13         // LED RGB: vermelho
#define LED_G 11         // LED RGB: verde
#define LED_B 12         // LED RGB: azul
#define BUZZER 21        // Buzzer: feedback sonoro
#define MATRIZ_WS2812B 7 // Matriz WS2812B 5x5
#define MAX_USUARIOS 8   // Máximo de usuários simultâneos

/* Estrutura para eventos */
typedef enum
{
    EVENTO_ENTRADA,
    EVENTO_SAIDA
} EventoTipo;

typedef struct
{
    EventoTipo tipo;
} Evento;

/* Variáveis Globais */
ssd1306_t disp;                       // Display OLED
SemaphoreHandle_t xDisplayMutex;      // Mutex para display
SemaphoreHandle_t xMatrixMutex;       // Mutex para matriz WS2812B
SemaphoreHandle_t xUsuariosMutex;     // Mutex para usuariosAtivos
SemaphoreHandle_t xContadorSem;       // Semáforo de contagem (entradas)
SemaphoreHandle_t xResetSem;          // Semáforo binário (reset)
QueueHandle_t xEventQueue;            // Fila para eventos de botões
volatile uint16_t usuariosAtivos = 0; // Contagem de usuários ativos

/* Debouncing */
absolute_time_t ultimoA = 0;
absolute_time_t ultimoB = 0;
absolute_time_t ultimoJoystick = 0;
const uint32_t DEBOUNCE_US = 200000; // 200 ms

/* Configuração do Buzzer */
#define BUZZER_FREQ 1000 // Frequência do buzzer (1000 Hz)
void buzzer_beep_curto()
{
    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER), true);
    vTaskDelay(pdMS_TO_TICKS(100)); // Beep de 100ms
    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER), false);
}

void buzzer_beep_duplo()
{
    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER), true);
    vTaskDelay(pdMS_TO_TICKS(100)); // Primeiro beep
    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER), false);
    vTaskDelay(pdMS_TO_TICKS(100)); // Pausa
    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER), true);
    vTaskDelay(pdMS_TO_TICKS(100)); // Segundo beep
    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER), false);
}

/* Configuração do LED RGB */
void set_rgb_color(uint8_t r, uint8_t g, uint8_t b)
{
    gpio_put(LED_R, r ? 1 : 0);
    gpio_put(LED_G, g ? 1 : 0);
    gpio_put(LED_B, b ? 1 : 0);
}

/* Atualiza LED RGB conforme ocupação */
void update_rgb_led()
{
    if (xSemaphoreTake(xUsuariosMutex, portMAX_DELAY) == pdTRUE)
    {
        if (usuariosAtivos == 0)
        {
            set_rgb_color(0, 0, 255); // Azul: nenhum usuário
        }
        else if (usuariosAtivos <= MAX_USUARIOS - 2)
        {
            set_rgb_color(0, 255, 0); // Verde: 1 a 6 usuários
        }
        else if (usuariosAtivos == MAX_USUARIOS - 1)
        {
            set_rgb_color(255, 255, 0); // Amarelo: 7 usuários
        }
        else
        {
            set_rgb_color(255, 0, 0); // Vermelho: 8 usuários
        }
        xSemaphoreGive(xUsuariosMutex);
    }
}

/* Atualiza display com mutex */
void update_display(const char *msg, uint16_t count)
{
    if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
    {
        char buffer[32];
        ssd1306_fill(&disp, 0);                 // Limpa o display
        ssd1306_draw_string(&disp, msg, 0, 20); // Mensagem
        snprintf(buffer, sizeof(buffer), "Usuarios: %d", count);
        ssd1306_draw_string(&disp, buffer, 5, 50); // Contagem
        ssd1306_send_data(&disp);                  // Atualiza display
        xSemaphoreGive(xDisplayMutex);
    }
}

/* Interrupções para botões A, B e joystick */
void gpio_irq_handler(uint gpio, uint32_t events)
{
    absolute_time_t agora = get_absolute_time();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    Evento evento;

    if (gpio == BOTAO_A && (events & GPIO_IRQ_EDGE_FALL))
    {
        if (absolute_time_diff_us(ultimoA, agora) > DEBOUNCE_US)
        {
            ultimoA = agora;
            evento.tipo = EVENTO_ENTRADA;
            xQueueSendFromISR(xEventQueue, &evento, &xHigherPriorityTaskWoken);
        }
    }

    if (gpio == BOTAO_B && (events & GPIO_IRQ_EDGE_FALL))
    {
        if (absolute_time_diff_us(ultimoB, agora) > DEBOUNCE_US)
        {
            ultimoB = agora;
            evento.tipo = EVENTO_SAIDA;
            xQueueSendFromISR(xEventQueue, &evento, &xHigherPriorityTaskWoken);
        }
    }

    if (gpio == JOYSTICK && (events & GPIO_IRQ_EDGE_FALL))
    {
        if (absolute_time_diff_us(ultimoJoystick, agora) > DEBOUNCE_US)
        {
            ultimoJoystick = agora;
            xSemaphoreGiveFromISR(xResetSem, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Tarefa de Entrada (Botão A) */
void vTaskEntrada(void *params)
{
    Evento evento;
    while (true)
    {
        if (xQueueReceive(xEventQueue, &evento, portMAX_DELAY) == pdTRUE)
        {
            if (evento.tipo == EVENTO_ENTRADA)
            {
                if (xSemaphoreTake(xContadorSem, 0) == pdTRUE)
                {
                    if (xSemaphoreTake(xUsuariosMutex, portMAX_DELAY) == pdTRUE)
                    {
                        if (usuariosAtivos < MAX_USUARIOS)
                        {
                            usuariosAtivos++;
                            xSemaphoreGive(xUsuariosMutex);
                            update_display("Entrada!", usuariosAtivos);
                            update_rgb_led();
                            anim_entrada(xMatrixMutex); // Boneco verde
                            // buzzer_beep_curto();
                        }
                        else
                        {
                            xSemaphoreGive(xContadorSem);
                            xSemaphoreGive(xUsuariosMutex);
                            update_display("Capacidade Maxima!", usuariosAtivos);
                            buzzer_beep_curto();
                        }
                    }
                }
                else
                {
                    update_display("Capacidade Maxima!", usuariosAtivos);
                    buzzer_beep_curto();
                }
            }
            else
            {
                xQueueSendToFront(xEventQueue, &evento, 0); // Devolve evento
            }
        }
    }
}

/* Tarefa de Saída (Botão B) */
void vTaskSaida(void *params)
{
    Evento evento;
    while (true)
    {
        if (xQueueReceive(xEventQueue, &evento, portMAX_DELAY) == pdTRUE)
        {
            if (evento.tipo == EVENTO_SAIDA)
            {
                if (xSemaphoreTake(xUsuariosMutex, portMAX_DELAY) == pdTRUE)
                {
                    if (usuariosAtivos > 0)
                    {
                        usuariosAtivos--;
                        xSemaphoreGive(xContadorSem);
                        xSemaphoreGive(xUsuariosMutex);
                        update_display("Saida!", usuariosAtivos);
                        update_rgb_led();
                        anim_saida(xMatrixMutex); // Boneco vermelho
                        // buzzer_beep_curto();
                    }
                    else
                    {
                        xSemaphoreGive(xUsuariosMutex);
                        update_display("Nenhum usuario!", usuariosAtivos);
                        buzzer_beep_curto();
                    }
                }
            }
            else
            {
                xQueueSendToFront(xEventQueue, &evento, 0); // Devolve evento
            }
        }
    }
}

/* Tarefa de Reset (Joystick) */
void vTaskReset(void *params)
{
    while (true)
    {
        if (xSemaphoreTake(xResetSem, portMAX_DELAY) == pdTRUE)
        {
            // Limpa a fila de eventos
            xQueueReset(xEventQueue);

            // Reseta o semáforo de contagem
            if (xSemaphoreTake(xUsuariosMutex, portMAX_DELAY) == pdTRUE)
            {
                usuariosAtivos = 0;
                xSemaphoreGive(xUsuariosMutex);
            }
            while (xSemaphoreTake(xContadorSem, 0) == pdTRUE)
                ; // Esvazia
            for (int i = 0; i < MAX_USUARIOS; i++)
            {
                xSemaphoreGive(xContadorSem); // Repõe MAX_USUARIOS
            }

            update_display("Sistema Reiniciado!", usuariosAtivos);
            update_rgb_led();
            buzzer_beep_duplo();
            anim_reset(xMatrixMutex); // Piscar vermelho
        }
    }
}

/* Tarefa Periódica para Atualizar Status */
void vDisplayTask(void *params)
{
    while (true)
    {
        update_display("Controle de Acesso", usuariosAtivos);
        update_rgb_led();
        anim_contagem(usuariosAtivos, xMatrixMutex); // Grade 2x4
        vTaskDelay(pdMS_TO_TICKS(1000));             // 5 segundos
    }
}

/* Função Principal */
int main()
{
    stdio_init_all(); // Inicializa saída serial (debug)

    /* Inicialização do I2C e Display OLED */
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&disp, 128, 64, false, ENDERECO_OLED, I2C_PORT);
    ssd1306_config(&disp);
    ssd1306_send_data(&disp);

    /* Configuração dos Botões, LED RGB e Buzzer */
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_init(JOYSTICK);
    gpio_set_dir(JOYSTICK, GPIO_IN);
    gpio_pull_up(JOYSTICK);
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);

    /* Inicialização da Matriz WS2812B */
    npInit(MATRIZ_WS2812B);

    /* Configuração do PWM para Buzzer */
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f);
    pwm_config_set_wrap(&config, 1000); // ~1000 Hz
    pwm_init(slice_num, &config, false);
    pwm_set_gpio_level(BUZZER, 500); // 50% duty cycle

    /* Configuração das Interrupções */
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(JOYSTICK, GPIO_IRQ_EDGE_FALL, true);

    /* Criação de Mutexes, Semáforos e Fila */
    xDisplayMutex = xSemaphoreCreateMutex();                             // Display OLED
    xMatrixMutex = xSemaphoreCreateMutex();                              // Matriz WS2812B
    xUsuariosMutex = xSemaphoreCreateMutex();                            // usuariosAtivos
    xContadorSem = xSemaphoreCreateCounting(MAX_USUARIOS, MAX_USUARIOS); // Entradas
    xResetSem = xSemaphoreCreateBinary();                                // Reset
    xEventQueue = xQueueCreate(10, sizeof(Evento));                      // Fila de eventos

    /* Criação das Tarefas */
    xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
    xTaskCreate(vTaskSaida, "SaidaTask", configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);
    xTaskCreate(vTaskReset, "ResetTask", configMINIMAL_STACK_SIZE + 128, NULL, 3, NULL);
    xTaskCreate(vDisplayTask, "DisplayTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    /* Inicia o Escalonador FreeRTOS */
    vTaskStartScheduler();

    /* Caso o escalonador falhe */
    panic_unsupported();
}