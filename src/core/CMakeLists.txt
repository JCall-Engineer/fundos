file(GLOB_RECURSE core_sources CONFIGURE_DEPENDS *.cpp)
add_library(core STATIC ${core_sources})

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core PUBLIC sqlite3)

if(NOT MSVC)
	# The core library is careful to zero initialize integral types, and relying on default constructors is pervasive in this codebase
	target_compile_options(core PRIVATE -Wno-missing-field-initializers)
endif()
