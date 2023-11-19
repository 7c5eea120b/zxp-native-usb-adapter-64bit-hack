cmake_minimum_required(VERSION 3.26)
project(MQALLOC C)

set(CMAKE_C_STANDARD 11)
add_compile_options(/MT /LD)

add_library(MQALLOC SHARED zebra_msvcr90_alloc_workaround.c)
