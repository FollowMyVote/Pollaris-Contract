cmake_minimum_required(VERSION 3.16)
project(pollaris-contract VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)

# Fix color compile output with Ninja
if( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
    if( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics" )
    endif()
endif()

list(APPEND CONTRACT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Contract/Pollaris.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Contract/Pollaris.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Contract/PollarisTests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Contract/Main.cpp")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Contract")

add_subdirectory(BAL)
BAL_CONTRACT(NAME pollaris BAL_PATH BAL SOURCES ${CONTRACT_SOURCES})

# Add a target to test EOSIO deployment in docker containers. Works even if CDT isn't installed.
add_custom_target(test_deploy_eosio "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test-leap.sh"
                                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
# Add a target for all tests
add_custom_target(test DEPENDS test_deploy_leap)
