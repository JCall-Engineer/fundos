find_package(Qt6 REQUIRED COMPONENTS Widgets Svg SvgWidgets LinguistTools)

set(CMAKE_AUTOMOC ON)

configure_file(app.rc.in app.rc)
file(GLOB_RECURSE qt_sources CONFIGURE_DEPENDS *.cpp *.hpp)
qt_add_executable(fundos WIN32
	${qt_sources}
	${CMAKE_CURRENT_BINARY_DIR}/app.rc
)

qt_add_resources(fundos "resources"
	PREFIX "/"
	BASE ${CMAKE_SOURCE_DIR}/res
	FILES ${CMAKE_SOURCE_DIR}/res/icon.svg
)

file(GLOB_RECURSE qt_icons CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/res/tabler/*.svg)
qt_add_resources(fundos "icons"
	PREFIX "/icons"
	BASE ${CMAKE_SOURCE_DIR}/res/tabler
	FILES ${qt_icons}
)

target_include_directories(fundos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fundos PRIVATE Qt6::Widgets Qt6::Svg Qt6::SvgWidgets core)

find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")

if(WINDEPLOYQT)
	add_custom_command(TARGET fundos POST_BUILD
		COMMAND ${WINDEPLOYQT} $<TARGET_FILE:fundos>
		COMMENT "Running windeployqt..."
	)
endif()
