cmake_minimum_required(VERSION 3.31.3)

project(
    kernel
    LANGUAGES C
)

add_library(asdf OBJECT IMPORTED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shell.bin
    DEPENDS $<TARGET_FILE:shell>
    COMMAND
        ${OBJCOPY}
        --set-section-flags .bss=alloc,contents
        -O binary
        $<TARGET_FILE:shell>
        ${CMAKE_CURRENT_BINARY_DIR}/shell.bin
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

string(
    REGEX REPLACE "[^0-9a-zA-Z]+" "_"
    SECTION_PREFIX ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shell.bin.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/shell.bin
    COMMAND
        ${OBJCOPY}
        -I binary
        -O elf32-littleriscv
        --remove-symbol-prefix _binary_${SECTION_PREFIX}
        ${CMAKE_CURRENT_BINARY_DIR}/shell.bin
        ${CMAKE_CURRENT_BINARY_DIR}/shell.bin.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_property(
    TARGET asdf
    PROPERTY IMPORTED_OBJECTS
        ${CMAKE_CURRENT_BINARY_DIR}/shell.bin.o
)

add_executable(kernel kernel.c $<TARGET_OBJECTS:asdf>)
target_include_directories(kernel PRIVATE .)
target_link_libraries(kernel common)
target_link_options(
    kernel
    PUBLIC -T${CMAKE_CURRENT_SOURCE_DIR}/kernel.ld
)