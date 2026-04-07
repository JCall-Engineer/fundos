cmake_minimum_required(VERSION 3.25)
project(fundos VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(lib/sqlite3)
add_subdirectory(src/core)
add_subdirectory(tests)

set(VALID_PLATFORMS desktop android)
if(NOT FUNDOS_PLATFORM IN_LIST VALID_PLATFORMS)
	message(FATAL_ERROR "Set FUNDOS_PLATFORM to one of: ${VALID_PLATFORMS}")
endif()

add_subdirectory(src/${FUNDOS_PLATFORM})
