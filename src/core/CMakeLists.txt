file(GLOB_RECURSE core_sources CONFIGURE_DEPENDS *.cpp)
add_library(core STATIC ${core_sources})

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core PUBLIC sqlite3)

set_target_properties(core PROPERTIES
	VERSION   1.0.1
	SOVERSION 0
)
