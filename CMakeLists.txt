cmake_minimum_required(VERSION 3.25)
project(fundos VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

if(MSVC)
	add_compile_options(
		/W4
		/permissive-
		/Zc:__cplusplus  # Report correct __cplusplus value
		/Zc:inline       # Remove unreferenced functions/data
		/Zc:throwingNew  # Assume operator new throws
	)
else()
	add_compile_options(-Wall -Wextra)
endif()

set(VALID_PLATFORMS desktop android)
if(NOT FUNDOS_PLATFORM IN_LIST VALID_PLATFORMS)
	message(FATAL_ERROR "Set FUNDOS_PLATFORM to one of: ${VALID_PLATFORMS}")
endif()

set(FETCHCONTENT_BASE_DIR               ${CMAKE_BINARY_DIR}/build/_deps)

add_subdirectory(lib/sqlite3            ${CMAKE_BINARY_DIR}/build/sqlite3)
add_subdirectory(src/core               ${CMAKE_BINARY_DIR}/build/core)
add_subdirectory(tests                  ${CMAKE_BINARY_DIR}/build/tests)
add_subdirectory(src/${FUNDOS_PLATFORM} ${CMAKE_BINARY_DIR}/build/${FUNDOS_PLATFORM})
