cmake_minimum_required(VERSION 3.31.3)

project(
    shell
    LANGUAGES C
)

add_executable(shell shell.c)
target_include_directories(shell PRIVATE .)
target_link_libraries(shell user)
target_link_options(
    shell
    PUBLIC -T${CMAKE_CURRENT_SOURCE_DIR}/../libuser/user.ld
)