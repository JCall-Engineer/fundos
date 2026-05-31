find_package(Qt6 REQUIRED COMPONENTS Widgets)

configure_file(app.rc.in app.rc)
file(GLOB_RECURSE qt_sources CONFIGURE_DEPENDS *.cpp)
qt_add_executable(fundos WIN32
	${qt_sources}
	${CMAKE_CURRENT_BINARY_DIR}/app.rc
)

set(icon_file ${CMAKE_SOURCE_DIR}/res/icon.svg)
set_source_files_properties(${icon_file} PROPERTIES QT_RESOURCE_ALIAS "res/icon.svg")

qt_add_resources(fundos "resources"
	PREFIX "/"
	FILES ${icon_file}
)

target_link_libraries(fundos PRIVATE Qt6::Widgets core)

find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")

if(WINDEPLOYQT)
	add_custom_command(TARGET fundos POST_BUILD
		COMMAND ${WINDEPLOYQT} $<TARGET_FILE:fundos>
		COMMENT "Running windeployqt..."
	)
endif()
