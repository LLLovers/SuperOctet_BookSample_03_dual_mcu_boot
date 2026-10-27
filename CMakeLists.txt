cmake_minimum_required(VERSION 3.25)
project(BookDualMCUBoot LANGUAGES NONE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
include(ExternalProject)
set(_toolchain_args)
foreach(var K210_SDK_PATH K210_TOOLCHAIN_PATH)
    if(DEFINED ${var})
        list(APPEND _toolchain_args "-D${var}:PATH=${${var}}")
    endif()
endforeach()
set(_pico_args)
foreach(var PICO_SDK_PATH PICOTOOL_FETCH_FROM_GIT_PATH)
    if(DEFINED ${var})
        list(APPEND _pico_args "-D${var}:PATH=${${var}}")
    endif()
endforeach()
ExternalProject_Add(book_k210
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/k210"
    BINARY_DIR "${CMAKE_BINARY_DIR}/k210"
    CMAKE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
        ${_toolchain_args}
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --parallel
    BUILD_ALWAYS TRUE
    BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/k210/k210_dual_blink.bin"
        "${CMAKE_BINARY_DIR}/k210/UniK210Loader.bin"
    INSTALL_COMMAND "")
ExternalProject_Add(book_rp2354a
    DEPENDS book_k210
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/rp2354a"
    BINARY_DIR "${CMAKE_BINARY_DIR}/rp2354a"
    CMAKE_ARGS "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}" ${_pico_args}
        "-DBOOK_LOADER_BIN:FILEPATH=${CMAKE_BINARY_DIR}/k210/UniK210Loader.bin"
        "-DBOOK_K210_BIN:FILEPATH=${CMAKE_BINARY_DIR}/k210/k210_dual_blink.bin"
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --parallel
    BUILD_ALWAYS TRUE
    BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/rp2354a/dual_mcu_boot.uf2"
    INSTALL_COMMAND "")
