#include "matrizled.c"
#include "FreeRTOS.h"
#include "task.h"
#include <time.h>
#include <stdlib.h>

// Intensidade padrão para as animações
#define intensidade 1

void printNum(void) {
    npWrite();
    npClear();
}

// Sprites existentes
int OFF[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
};

int ATENCAO[5][5][3] = {
    {{0, 0, 0}, {128, 128, 0}, {128, 128, 0}, {128, 128, 0}, {0, 0, 0}},
    {{128, 128, 0}, {128, 128, 0}, {0, 0, 0}, {128, 128, 0}, {128, 128, 0}},
    {{128, 128, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {128, 128, 0}},
    {{128, 128, 0}, {0, 0, 0}, {128, 128, 0}, {0, 0, 0}, {128, 128, 0}},
    {{0, 0, 0}, {128, 128, 0}, {128, 128, 0}, {128, 128, 0}, {0, 0, 0}}
};

int SETA_VERDE[5][5][3] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 128, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 128, 0}, {0, 0, 0}},
    {{0, 128, 0}, {0, 128, 0}, {0, 128, 0}, {0, 128, 0}, {0, 128, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 128, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {0, 128, 0}, {0, 0, 0}, {0, 0, 0}}
};

int X_VERMELHO[5][5][3] = {
    {{128, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {128, 0, 0}},
    {{0, 0, 0}, {128, 0, 0}, {0, 0, 0}, {128, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {0, 0, 0}, {128, 0, 0}, {0, 0, 0}, {0, 0, 0}},
    {{0, 0, 0}, {128, 0, 0}, {0, 0, 0}, {128, 0, 0}, {0, 0, 0}},
    {{128, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {128, 0, 0}}
};

// Funções existentes
void PedestreSIGA(void) {
    desenhaSprite(SETA_VERDE, intensidade);
    printNum();
}

void PedestrePARE(void) {
    desenhaSprite(X_VERMELHO, intensidade);
    printNum();
}

void Amarelo_Noturno(void) {
    desenhaSprite(ATENCAO, intensidade);
    printNum();
}

void DesligaMatriz(void) {
    desenhaSprite(OFF, intensidade);
    printNum();
}

// Animação para SEGURO: seta verde estática
void anim_seguro(void) {
    PedestreSIGA();
    vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz
}

// Animação para ALERTA: chuva piscante azul
void anim_alerta(void) {
    npClear();
    for (int i = 0; i < 3; i++) {
        int x = rand() % 5;
        int y = rand() % 5;
        int posicao = getIndex(x, y);
        npSetLED(posicao, 0, 0, 100); // Azul
    }
    npWrite();
    vTaskDelay(pdMS_TO_TICKS(500)); // 2 Hz
}

// Animação para ENCHENTE: linhas azuis baseadas no nível de água
void anim_enchente(uint16_t nivel_agua) {
    npClear();

    // Converte nível de água para percentual (0–100)
    uint8_t percent_agua = (nivel_agua * 100) / 4095;

    // Verifica intervalos e acende linhas horizontais em azul (de baixo para cima)
    if (percent_agua >= 98) {
        // Linhas 4, 3, 2, 1, 0 (todas)
        for (int row = 0; row < 5; row++) {
            for (int col = 0; col < 5; col++) {
                int posicao = getIndex(col, row);
                npSetLED(posicao, 0, 0, 100); // Azul
            }
        }
    } else if (percent_agua >= 80) {
        // Linhas 4, 3, 2, 1
        for (int row = 1; row <= 4; row++) {
            for (int col = 0; col < 5; col++) {
                int posicao = getIndex(col, row);
                npSetLED(posicao, 0, 0, 100); // Azul
            }
        }
    } else if (percent_agua >= 60) {
        // Linhas 4, 3, 2
        for (int row = 2; row <= 4; row++) {
            for (int col = 0; col < 5; col++) {
                int posicao = getIndex(col, row);
                npSetLED(posicao, 0, 0, 100); // Azul
            }
        }
    } else if (percent_agua >= 40) {
        // Linhas 4, 3
        for (int row = 3; row <= 4; row++) {
            for (int col = 0; col < 5; col++) {
                int posicao = getIndex(col, row);
                npSetLED(posicao, 0, 0, 100); // Azul
            }
        }
    } else if (percent_agua >= 20) {
        // Linha 4
        for (int col = 0; col < 5; col++) {
            int posicao = getIndex(col, 4);
            npSetLED(posicao, 0, 0, 100); // Azul
        }
    }
    // 0–19%: Nenhuma linha (npClear já limpou)

    npWrite();
    vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz
}

