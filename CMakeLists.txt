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
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Pico_TFT_ST7796_LVGL C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Pico_TFT_ST7796_LVGL Pico_TFT_ST7796_LVGL.c )

# Pull in LVGL source files via its CMake system
add_subdirectory(lib/lvgl)

pico_set_program_name(Pico_TFT_ST7796_LVGL "Pico_TFT_ST7796_LVGL")
pico_set_program_version(Pico_TFT_ST7796_LVGL "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Pico_TFT_ST7796_LVGL 0)
pico_enable_stdio_usb(Pico_TFT_ST7796_LVGL 1)

# Add the standard library to the build
target_link_libraries(Pico_TFT_ST7796_LVGL
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(Pico_TFT_ST7796_LVGL PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(Pico_TFT_ST7796_LVGL hardware_spi 
    hardware_gpio 
    hardware_timer
    lvgl
    lvgl_examples  # Add if your environment splits them
    lvgl_demos     # Add if your environment splits them
        )

pico_add_extra_outputs(Pico_TFT_ST7796_LVGL)

