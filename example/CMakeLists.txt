cmake_minimum_required(VERSION 3.15)

project(constbin_example)

add_subdirectory(../ constbin)

SET(STATIC_FILES
    printsource.c  
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/output.c ${CMAKE_CURRENT_BINARY_DIR}/output.h
                   COMMAND constbin ${CMAKE_CURRENT_BINARY_DIR}/output.c ${CMAKE_CURRENT_BINARY_DIR}/output.h ${STATIC_FILES}
                   DEPENDS ${STATIC_FILES} constbin
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


add_executable(print_source
     ${CMAKE_CURRENT_BINARY_DIR}/output.c
     ${CMAKE_CURRENT_BINARY_DIR}/output.h
     printsource.c
)

target_include_directories(print_source PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

