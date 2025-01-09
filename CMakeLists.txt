cmake_minimum_required(VERSION 3.31.3)

project(
    mto
    LANGUAGES C
)

add_subdirectory(common)
add_subdirectory(libuser)
add_subdirectory(shell)
add_subdirectory(kernel)