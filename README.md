# Sistema de Controle de Acesso Interativo para Biblioteca

![Sistema de Controle](logo-embarcaTech.png)

## Descrição

O **Sistema de Controle de Acesso Interativo** é um projeto desenvolvido com a placa **BitDogLab** (microcontrolador **RP2040**) para gerenciar o acesso de até 8 usuários a uma **biblioteca**, utilizando **FreeRTOS** com sincronização por semáforos, mutexes e filas de eventos. O sistema controla entradas (Botão A), saídas (Botão B) e reset (joystick), com feedback visual (display OLED, LED RGB, matriz WS2812B 5x5) e sonoro (buzzer). Animações na matriz, como um boneco caminhando para entrada/saída e uma grade de contagem, tornam o sistema intuitivo e acessível.

Este sistema é **adaptável** para outros cenários que envolvam contagem de pessoas ou recursos, como:
- **Refeitório**: Controle de ocupação de mesas.
- **Sala de aula**: Monitoramento de alunos presentes.
- **Eventos**: Gestão de ingressos ou participantes.
- **Espaços públicos**: Limitação de acesso em museus ou ginásios.

## Funcionalidades

### Entrada de Usuário
- **Botão A** (GP5):
  - Incrementa a contagem de usuários na biblioteca (máximo 8).
  - Exibe "Entrada!" no display OLED.
  - Animação na matriz: Boneco verde caminha da esquerda para a direita (9 frames, 900ms).
  - Se cheio, exibe "Capacidade Maxima!" e emite beep curto.

### Saída de Usuário
- **Botão B** (GP6):
  - Decrementa a contagem de usuários (mínimo 0).
  - Exibe "Saida!" no display OLED.
  - Animação na matriz: Boneco vermelho caminha da direita para a esquerda (9 frames, 900ms).
  - Se não houver usuários, exibe "Nenhum usuario!" e emite beep curto.

### Reset do Sistema
- **Joystick** (GP22):
  - Zera a contagem de usuários e limpa a fila de eventos.
  - Exibe "Sistema Reiniciado!" no display OLED.
  - Animação na matriz: Piscar vermelho (intensidade 10).
  - Feedback sonoro: Beep duplo (1000 Hz, 2x100 ms com pausa).

### Exibição Visual
- **Matriz de LEDs** (5x5, WS2812B, GP7):
  - **Entrada**: Boneco verde (9 frames).
  - **Saída**: Boneco vermelho (9 frames inversos).
  - **Reset**: Piscar vermelho.
  - **Contagem**: Grade 2x4 indicando usuários ativos (0–8).
- **Display OLED** (SSD1306, 128x64, I2C em GP14-SDA, GP15-SCL):
  - Mensagens: "Entrada!", "Saida!", "Capacidade Maxima!", "Nenhum usuario!", "Sistema Reiniciado!", "Controle de Acesso".
  - Contagem: "Usuarios: <N>" (posição 5,50).
- **LED RGB** (GP11-verde, GP12-azul, GP13-vermelho):
  - Azul (0, 0, 255): Nenhum usuário.
  - Verde (0, 255, 0): 1 a 6 usuários.
  - Amarelo (255, 255, 0): 7 usuários.
  - Vermelho (255, 0, 0): 8 usuários.

### Feedback Sonoro
- **Buzzer** (GP21, PWM):
  - Beep curto (1000 Hz, 100 ms): Entrada cheia, saída sem usuários.
  - Beep duplo (1000 Hz, 2x100 ms com pausa): Reset.
- Suporta acessibilidade para deficientes visuais.

### Status Periódico
- Atualização a cada 1 segundos:
  - Exibe "Controle de Acesso" (posição 5,30) e contagem no display.
  - Atualiza LED RGB e matriz (grade 2x4).

## Tecnologias Utilizadas

- **Placa**: BitDogLab (RP2040)
- **Periféricos**:
  - Matriz de LEDs WS2812B (5x5, GP7)
  - Display OLED SSD1306 (I2C, GP14-SDA, GP15-SCL)
  - Joystick (GP22, interrupção)
  - LED RGB (GP11-verde, GP12-azul, GP13-vermelho)
  - Buzzer (PWM, GP21)
  - Botões (GP5-A, GP6-B)
- **Software**: C, Pico SDK, FreeRTOS
- **Bibliotecas**: `ssd1306.h`, `animacoes.h`, `matrizled.c`
- **Sincronização**:
  - `xSemaphoreCreateCounting`: Controle de usuários (`xContadorSem`, máximo 8).
  - `xSemaphoreCreateBinary`: Reset via interrupção (`xResetSem`).
  - `xSemaphoreCreateMutex`: Proteção de display (`xDisplayMutex`), matriz (`xMatrixMutex`), contagem (`xUsuariosMutex`).
  - Fila de eventos (`xEventQueue`) para botões A/B.

## Pré-requisitos

- [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- Placa BitDogLab com RP2040
- Componentes: Matriz WS2812B, display OLED, joystick, buzzer, LED RGB, botões
- Ferramentas: Compilador C (ex.: GCC), CMake, terminal serial (ex.: minicom)

## Como Rodar o Projeto

1. **Clone o Repositório**:
   ```bash
   git clone https://github.com/Danngas/LibraryAccessControl.git
   cd Controle-de-Acesso-Biblioteca-BitDogLab

