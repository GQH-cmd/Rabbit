cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

option(RABBIT_ENABLE_SELF_TEST "Deprecated: the production state machine always runs its boot self-test" ON)
option(RABBIT_ENABLE_PUMP_LED_TEST "Continuously toggle PE2 and its LED mirror every second" OFF)
option(RABBIT_ENABLE_SIMULATION "Run virtual motor tasks and use only PA1 as the virtual pump indicator" OFF)
option(RABBIT_PUMP_LED_MIRROR "Mirror the pump output state on the active-low PA1 user LED" ON)
set(RABBIT_PUMP_ACTIVE_HIGH 1 CACHE STRING "Pump relay active level: 1=high, 0=low")
if(NOT RABBIT_PUMP_ACTIVE_HIGH MATCHES "^[01]$")
    message(FATAL_ERROR "RABBIT_PUMP_ACTIVE_HIGH must be 0 or 1")
endif()


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME rabbit)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/Src/app.c
    BSP/Src/board_led.c
    BSP/Src/esp32_comm.c
    BSP/Src/esp32_link.c
    BSP/Src/hal_callbacks.c
    BSP/Src/limit_switches.c
    BSP/Src/pump.c
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE Core/Src/self_test.c)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    App/Inc
    BSP/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    APP_ENABLE_SELF_TEST=1
    APP_ENABLE_PUMP_LED_TEST=$<BOOL:${RABBIT_ENABLE_PUMP_LED_TEST}>
    APP_ENABLE_SIMULATION=$<BOOL:${RABBIT_ENABLE_SIMULATION}>
    PUMP_ACTIVE_HIGH=${RABBIT_PUMP_ACTIVE_HIGH}
    PUMP_LED_MIRROR=$<BOOL:${RABBIT_PUMP_LED_MIRROR}>
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
