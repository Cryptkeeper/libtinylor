cmake_minimum_required(VERSION 3.27)

project(tinylor C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

add_library(tinylor STATIC src/tinylor.c src/tinylor.h)
set_target_properties(tinylor PROPERTIES PUBLIC_HEADER tinylor.h)

find_program(M4 m4)
if (M4)
    add_custom_target(tinylor.h ALL COMMAND m4 -I${CMAKE_SOURCE_DIR} src/tinylor.m4 > ${CMAKE_BINARY_DIR}/tinylor.h DEPENDS src/tinylor.c src/tinylor.h)
    message(STATUS "generated ${CMAKE_BINARY_DIR}/tinylor.h")
else ()
    message(WARNING "m4 not found, tinylor.h will not be generated")
endif ()

enable_testing()

add_executable(tinylor_test src/tinylor_test.c src/tinylor.c)
add_test(NAME tinylor_test COMMAND tinylor_test)