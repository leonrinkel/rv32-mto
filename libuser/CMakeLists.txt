cmake_minimum_required(VERSION 3.31.3)

project(
    user
    LANGUAGES C
)

add_library(user user.c)
target_include_directories(user INTERFACE .)
target_link_libraries(user common)