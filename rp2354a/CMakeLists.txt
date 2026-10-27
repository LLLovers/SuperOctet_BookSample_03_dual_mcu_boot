cmake_minimum_required(VERSION 3.25)
if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
set(PICO_BOARD hardware_config)
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}")
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(BookBootHost LANGUAGES C CXX ASM)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
if(NOT PICO_SDK_VERSION_STRING VERSION_EQUAL "2.3.1")
    message(FATAL_ERROR "Pico SDK 2.3.1 is required")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_embedded "${CMAKE_CURRENT_BINARY_DIR}/embedded_images.S")
set(_manifest "${CMAKE_CURRENT_BINARY_DIR}/embedded_images.json")
add_custom_command(OUTPUT "${_embedded}" "${_manifest}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_images.py"
        --loader "${BOOK_LOADER_BIN}" --helios "${BOOK_K210_BIN}"
        --assembly "${_embedded}" --manifest "${_manifest}" --build-id book-blink
    DEPENDS "${BOOK_LOADER_BIN}" "${BOOK_K210_BIN}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_images.py"
    VERBATIM)
add_executable(dual_mcu_boot main.cpp "${_embedded}"
    src/board/k210_boot.cpp
    src/board/k210_boot_io.cpp
    src/board/k210_boot_io_rom.cpp
    src/platform/uni_crc.cpp)
target_include_directories(dual_mcu_boot PRIVATE . include)
target_compile_definitions(dual_mcu_boot PRIVATE
    PICO_STACK_SIZE=0x1000 PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)
target_link_libraries(dual_mcu_boot PRIVATE pico_stdlib pico_multicore
    hardware_uart hardware_spi hardware_dma)
pico_enable_stdio_usb(dual_mcu_boot 0)
pico_enable_stdio_uart(dual_mcu_boot 0)
pico_add_extra_outputs(dual_mcu_boot)
