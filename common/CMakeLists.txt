cmake_minimum_required(VERSION 3.31.3)

project(
    common
    LANGUAGES C
)

add_library(common common.c)
target_include_directories(common INTERFACE .)