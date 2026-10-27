add_executable(UniK210Loader
    src/start.S src/loader.c src/hardware.c src/image.c)
target_include_directories(UniK210Loader PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(UniK210Loader PRIVATE k210_platform)
target_compile_options(UniK210Loader PRIVATE -ffreestanding)
target_link_options(UniK210Loader PRIVATE -nostdlib
    -Wl,--gc-sections -Wl,--no-relax)
k210_set_linker_script(UniK210Loader
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/memory.ld")
set_target_properties(UniK210Loader PROPERTIES
    SUFFIX .elf RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
k210_add_extra_outputs(UniK210Loader)
