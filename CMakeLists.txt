# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
set(FREERTOS_KERNEL_PATH "C:/FreeRTOS-Kernel")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(LibraryAccessControl C CXX ASM)
pico_sdk_init()


include_directories(${CMAKE_SOURCE_DIR}/lib)


add_executable(${PROJECT_NAME}  
        ${PROJECT_NAME}.c 
        lib/ssd1306.c # Biblioteca para o display OLED
       
        )

# Generate PIO header
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lib/ws2818b.pio)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})


        # Link com as bibliotecas necessárias
target_link_libraries(${PROJECT_NAME} 
pico_stdlib 
hardware_pio # para matriz de leds
hardware_clocks # para matriz de leds
hardware_i2c # para comuniccao do display
FreeRTOS-Kernel 
FreeRTOS-Kernel-Heap4
hardware_adc # para o njoystick
hardware_pwm # para o leds RGB
hardware_gpio # PARA AS ENTRADAS GPIO
pico_bootsel_via_double_reset # PARA COLOCAR A PLACA NO MODO DE GRAVACAO
pico_bootrom # PARA COLOCAR A PLACA NO MODO DE GRAVACAO
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})