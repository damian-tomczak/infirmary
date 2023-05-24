cmake_minimum_required(VERSION 3.10)

project(infirmary VERSION 0.0.1 LANGUAGES CXX)

if(NOT UNIX AND NOT CMAKE_COMPILER_IS_GNUCC)
    message(WARNING "Config was tested only on UNIX system and GNU compiler!")
    if(CMAKE_CXX_COMPILER_VERSION NOT VERSION_EQUAL 9.4.0)
        message(WARNING "Config was tested only on the GNU compiler version 9.4.0 "
            "and adjusted to its limitations such as the lack of constexpr std::string!")
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(EXTERNAL_PATH "external")
get_filename_component(ABSOLUTE_EXTERNAL_PATH ${EXTERNAL_PATH} ABSOLUTE)
set(DATA_PATH "data")
get_filename_component(ABSOLUTE_DATA_PATH ${DATA_PATH} ABSOLUTE)

find_package(Boost REQUIRED COMPONENTS program_options)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${ABSOLUTE_EXTERNAL_PATH}/drogon)

add_subdirectory(${ABSOLUTE_EXTERNAL_PATH}/fmt)

set(SQLITECPP_ENABLE_ASSERT_HANDLER ON CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
add_subdirectory(${ABSOLUTE_EXTERNAL_PATH}/SQLiteCpp)

set(MAILIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MAILIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${ABSOLUTE_EXTERNAL_PATH}/mailio)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)

option(UNIT_TEST "Test the basic operation" ON)
if(UNIT_TEST)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    enable_testing()
    add_subdirectory(${ABSOLUTE_EXTERNAL_PATH}/googletest EXCLUDE_FROM_ALL)
    add_subdirectory(tests)
endif()