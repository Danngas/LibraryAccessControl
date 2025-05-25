# LibraryAccessControl

## Descrição
Sistema embarcado com FreeRTOS para controle de acesso interativo a uma biblioteca, utilizando a placa **BitDogLab com RP2040**. O projeto implementa um painel de controle que gerencia a entrada e saída de usuários, com limite máximo de 8 pessoas, fornecendo feedback visual (display OLED, LED RGB) e sonoro (buzzer). Usa **semáforo de contagem**, **semáforo binário**, e **mutex** para sincronização de tarefas, conforme especificado na atividade "Sistemas Multitarefas Parte 3".

**Autor**: Daniel Silva de Souza  
**Data**: 25/05/2025  
**Polo**: [Preencher com seu polo, ex.: Bom Jesus da Lapa]

## Funcionalidades
- **Entrada de usuário**: Incrementa a contagem de usuários via botão A, até o limite de 8.
- **Saída de usuário**: Decrementa a contagem via botão B.
- **Reset do sistema**: Zera a contagem via botão do joystick, com interrupção.
- **Feedback visual**:
  - Display OLED exibe contagem de usuários e mensagens (ex.: "Entrada!", "Capacidade Máxima!").
  - LED RGB indica estados: azul (0 usuários), verde (0 a 6), amarelo (7), vermelho (8).
- **Feedback sonoro**: Buzzer emite beep curto (capacidade máxima) ou duplo (reset).
- **Sincronização**: Mutex protege o acesso ao display; semáforos gerenciam entrada/saída e reset.

## Requisitos
- **Hardware**: Placa BitDogLab com RP2040.
- **Software**:
  - [Pico SDK](https://github.com/raspberrypi/pico-sdk) para compilação.
  - Biblioteca `ssd1306.h` para o display OLED (incluída no diretório `lib/`).
  - FreeRTOS configurado no projeto.
- **Ferramentas**: CMake, GCC, e um ambiente para upload de firmware (ex.: Picotool).

## Instruções de Compilação
1. Clone o repositório:
   ```bash
   git clone https://github.com/Danngas/LibraryAccessControl.git
   cd LibraryAccessControl
   ```
2. Configure o Pico SDK:
   - Defina a variável de ambiente `PICO_SDK_PATH` apontando para o diretório do SDK.
3. Compile o projeto:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```
4. Carregue o firmware na placa BitDogLab (ex.: via USB com Picotool ou modo BOOTSEL).

## Estrutura do Repositório
- `main.c`: Código-fonte principal com as tarefas FreeRTOS.
- `lib/`: Bibliotecas externas (ex.: `ssd1306.h` para o display OLED).
- `README.md`: Este arquivo com instruções e descrição do projeto.

## Branches
- `main`: Código consolidado e funcional.
- `display-oled`: Implementação do display OLED com mutex.
- `botao-a`: Tarefa de entrada (botão A, semáforo de contagem).
- [Outras branches serão criadas para botão B, joystick, LED RGB, e buzzer.]

## Status
- [x] Display OLED com mutex implementado.
- [ ] Botão A (entrada) com semáforo de contagem.
- [ ] Botão B (saída).
- [ ] Joystick (reset com semáforo binário).
- [ ] LED RGB (feedback visual).
- [ ] Buzzer (feedback sonoro).

## Licença
Este projeto é para fins acadêmicos e não possui licença pública.