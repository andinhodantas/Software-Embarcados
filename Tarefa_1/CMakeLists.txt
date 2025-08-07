# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

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
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOSv202406.01-LTS/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(Tarefa_1 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Tarefa_1 Tarefa_1.c inc/ssd1306_i2c.c )


pico_set_program_name(Tarefa_1 "Tarefa_1")
pico_set_program_version(Tarefa_1 "0.1")

# # Generate PIO header
# pico_generate_pio_header(Tarefa_1 ${CMAKE_CURRENT_LIST_DIR}/Tarefa_1.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Tarefa_1 1)
pico_enable_stdio_usb(Tarefa_1 1)

# Add the standard library to the build
target_link_libraries(Tarefa_1
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(Tarefa_1 PRIVATE
         ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/config
)
target_compile_definitions(Tarefa_1 PRIVATE
        configNUMBER_OF_CORES=1
)

target_link_libraries(Tarefa_1
        FreeRTOS-Kernel-Heap4
        )

# Add any user requested libraries
target_link_libraries(Tarefa_1 
        hardware_i2c
        hardware_pio
        hardware_adc
        hardware_pwm
        )

pico_add_extra_outputs(Tarefa_1)

