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

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Tarefa_2-MQTT C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(Tarefa_2-MQTT Tarefa_2-MQTT.c lib/ssd1306.c)

pico_generate_pio_header(Tarefa_2-MQTT ${CMAKE_CURRENT_LIST_DIR}/lib/ws2812.pio)

pico_set_program_name(Tarefa_2-MQTT "Tarefa_2-MQTT")
pico_set_program_version(Tarefa_2-MQTT "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Tarefa_2-MQTT 1)
pico_enable_stdio_usb(Tarefa_2-MQTT 1)

# Add the standard library to the build
target_link_libraries(Tarefa_2-MQTT
    pico_stdlib
    hardware_gpio
    hardware_i2c
    hardware_pwm
    hardware_pio
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
    pico_mbedtls
    pico_lwip_mbedtls
    )

# Add the standard include files to the build
target_include_directories(Tarefa_2-MQTT PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/lib
)

pico_add_extra_outputs(Tarefa_2-MQTT)
