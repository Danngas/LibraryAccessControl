/*
 * Painel de Controle de Interativo - Display OLED, Botão A, Botão B, Joystick, LED RGB, Buzzer, Matriz de LEDs
 * Autor: Daniel Silva de Souza
 * Data: 25/05/2025
 * Descrição: Sistema de controle de acesso com display OLED, mutex, semáforos, LED RGB, buzzer e matriz de LEDs WS2812B 5x5 para animação de entrada/saída e contagem visual, usando animacoes.h do GuardaChuvas.
 */

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include "animacoes.h"

/* Definições de Hardware */
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO_OLED 0x3C
#define BOTAO_A 5      // Pino do botão A (entrada)
#define BOTAO_B 6      // Pino do botão B (saída)
#define JOYSTICK 22    // Pino do botão do joystick (reset)
#define LED_R 13       // Pino do LED vermelho
#define LED_G 11       // Pino do LED verde
#define LED_B 12       // Pino do LED azul
#define BUZZER 21      // Pino do buzzer
#define MAX_usuarios 8 // Capacidade máxima de usuários
#define MATRIZ_WS2812B 7

/* Variáveis Globais */
ssd1306_t ssd;                        // Estrutura para o display OLED
SemaphoreHandle_t xDisplayMutex;      // Mutex para o display
SemaphoreHandle_t xContadorSem;       // Semáforo de contagem para entradas
SemaphoreHandle_t xSaidaSem;          // Semáforo de contagem para saídas
SemaphoreHandle_t xResetSem;          // Semáforo binário para reset
SemaphoreHandle_t xMatrixMutex;       // Mutex para a matriz de LEDs
volatile uint16_t usuariosAtivos = 0; // Contagem atual de usuários

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

/* Função para configurar o LED RGB */
void set_rgb_color(uint8_t r, uint8_t g, uint8_t b)
{
    gpio_put(LED_R, r);
    gpio_put(LED_G, g);
    gpio_put(LED_B, b);
}

/* Função para atualizar o LED RGB com base na contagem */
void update_rgb_led()
{
    if (usuariosAtivos == 0)
    {
        set_rgb_color(0, 0, 255); // Azul
    }
    else if (usuariosAtivos <= MAX_usuarios - 2)
    {
        set_rgb_color(0, 255, 0); // Verde
    }
    else if (usuariosAtivos == MAX_usuarios - 1)
    {
        set_rgb_color(255, 255, 0); // Amarelo
    }
    else
    {
        set_rgb_color(255, 0, 0); // Vermelho
    }
}

/* Função para atualizar o display com mutex */
void update_display(const char *msg, uint16_t count)
{
    if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
    {
        char buffer[32];
        ssd1306_fill(&ssd, 0);                                   // Sequência o buffer do display
        ssd1306_draw_string(&ssd, msg, 5, 20);                   // Exibe a mensagem principal
        snprintf(buffer, sizeof(buffer), "Usuarios: %d", count); // Exibe o número de usuários
        ssd1306_draw_string(&ssd, buffer, 5, 40);                // Exibe a contagem
        ssd1306_send_data(&ssd);                                 // Atualiza o display
        xSemaphoreGive(xDisplayMutex);                           // Libera o mutex
    }
}

/* Interrupções para botões A, B e joystick */
void gpio_irq_handler(uint gpio, uint32_t events)
{
    absolute_time_t agora = get_absolute_time();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (gpio == BOTAO_A && (events & GPIO_IRQ_EDGE_FALL))
    {
        if (absolute_time_diff_us(ultimoA, agora) > DEBOUNCE_US)
        {
            ultimoA = agora;
            xSemaphoreGiveFromISR(xContadorSem, &xHigherPriorityTaskWoken);
        }
    }

    if (gpio == BOTAO_B && (events & GPIO_IRQ_EDGE_FALL))
    {
        if (absolute_time_diff_us(ultimoB, agora) > DEBOUNCE_US)
        {
            ultimoB = agora;
            xSemaphoreGiveFromISR(xSaidaSem, &xHigherPriorityTaskWoken);
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
    while (true)
    {
        if (xSemaphoreTake(xContadorSem, portMAX_DELAY) == pdTRUE)
        {
            if (usuariosAtivos < MAX_usuarios)
            {
                usuariosAtivos++; // Incrementa a contagem
                update_display("Entrada!", usuariosAtivos);
                update_rgb_led(); // Atualiza o LED RGB
                anim_entrada(xMatrixMutex);
                                  //  Animacao1();      // Animação de entrada
            }
            else
            {
                update_display("Capacidade Maxima!", usuariosAtivos);
                buzzer_beep_curto(); // Beep curto
            }
        }
    }
}

/* Tarefa de Saída (Botão B) */
void vTaskSaida(void *params)
{
    while (true)
    {
        if (xSemaphoreTake(xSaidaSem, portMAX_DELAY) == pdTRUE)
        {
            if (usuariosAtivos > 0)
            {
                usuariosAtivos--; // Decrementa a contagem
                update_display("Saida!", usuariosAtivos);
                update_rgb_led(); // Atualiza o LED RGB
                anim_saida(xMatrixMutex);
                                  // Animacao1(); // Animação de saída
            }
            else
            {
                update_display("Nenhum usuario!", usuariosAtivos);
                buzzer_beep_curto(); // Beep de erro
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
            usuariosAtivos = 0; // Zera a contagem
            update_display("Sistema Reiniciado!", usuariosAtivos);
            update_rgb_led();    // Atualiza o LED RGB
            buzzer_beep_duplo(); // Beep duplo
            anim_reset(xMatrixMutex);
                                 // Animacao1(); // Animação de reset
        }
    }
}

/* Tarefa periódica para atualizar o display com status */
void vDisplayTask(void *params)
{
    while (true)
    {
        update_display("Controle de Acesso", usuariosAtivos);
        update_rgb_led();                // Atualiza o LED RGB
        //#anim_saida();                    // Atualiza a contagem na matriz
        anim_contagem(usuariosAtivos,xMatrixMutex);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Espera 5 segundos
        
    }
}

/* Função principal */
int main()
{
    stdio_init_all(); // Inicializa a saída serial (debug)

    /* Inicialização do barramento I2C e do display OLED */
    i2c_init(I2C_PORT, 400 * 1000); // Frequência de 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SDA);
    ssd1306_init(&ssd, 128, 64, false, ENDERECO_OLED, I2C_PORT); // Display 128x64
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd); // Primeira atualização

    /* Configurações dos botões A, B, joystick, LED RGB e buzzer */
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

    npInit(MATRIZ_WS2812B);

    /* Configuração do PWM para o buzzer */
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f); // Divisor para frequência
    pwm_config_set_wrap(&config, 1000);     // Período para ~1000 Hz
    pwm_init(slice_num, &config, false);    // Inicializa PWM, mas não ativa
    pwm_set_gpio_level(BUZZER, 500);        // Duty cycle de 50%

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(JOYSTICK, GPIO_IRQ_EDGE_FALL, true);

    /* Criação de mutex e semáforos */
    xDisplayMutex = xSemaphoreCreateMutex();        // Protege o display OLED
    xMatrixMutex = xSemaphoreCreateMutex();         // Protege a matriz de LEDs
    xContadorSem = xSemaphoreCreateCounting(10, 0); // Até 10 eventos de entrada
    xSaidaSem = xSemaphoreCreateCounting(10, 0);    // Até 10 eventos de saída
    xResetSem = xSemaphoreCreateBinary();           // Semáforo binário para reset

    /* Criação das tarefas */
    xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "SaidaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "ResetTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "DisplayTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    /* Inicia o escalonador FreeRTOS */
    vTaskStartScheduler();

    /* Caso o escalonador falhe */
    panic_unsupported();
}