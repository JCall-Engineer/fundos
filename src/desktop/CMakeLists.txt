find_package(Qt6 REQUIRED COMPONENTS Widgets)

file(GLOB_RECURSE qt_sources CONFIGURE_DEPENDS *.cpp)
qt_add_executable(fundos ${qt_sources})

target_link_libraries(fundos PRIVATE Qt6::Widgets core)

find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")

if(WINDEPLOYQT)
	add_custom_command(TARGET fundos POST_BUILD
		COMMAND ${WINDEPLOYQT} $<TARGET_FILE:fundos>
		COMMENT "Running windeployqt..."
	)
endif()
