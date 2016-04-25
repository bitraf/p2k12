cmake_minimum_required(VERSION 3.5)
project(p2k12)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

set(SOURCE_FILES
        array.c
        array.h
        main.c
        postgresql.c
        postgresql.h)

if (NOT (DEFINED P2K12_MODE))
    set(P2K12_MODE dev)
endif ()

list(APPEND P2K12_COMPILE_DEFINITIONS "P2K12_MODE=${P2K12_MODE}")

add_executable(p2k12 ${SOURCE_FILES})
target_link_libraries(p2k12 pq crypt readline)
target_compile_definitions(p2k12 PUBLIC ${P2K12_COMPILE_DEFINITIONS})
