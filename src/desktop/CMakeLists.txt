find_package(Qt6 REQUIRED COMPONENTS Concurrent Network Widgets Svg SvgWidgets LinguistTools)

set(CMAKE_AUTOMOC ON)

configure_file(app.rc.in app.rc)
configure_file(version.hpp.in version.hpp)

file(GLOB_RECURSE qt_sources CONFIGURE_DEPENDS *.cpp *.hpp)
qt_add_executable(fundos WIN32
	${qt_sources}
	${CMAKE_CURRENT_BINARY_DIR}/app.rc
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/translations)
qt_add_translations(fundos
	TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/fundos_en.ts
	LUPDATE_OPTIONS -no-obsolete
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

target_include_directories(fundos PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(fundos PRIVATE Qt6::Concurrent Qt6::Network Qt6::Widgets Qt6::Svg Qt6::SvgWidgets core)

find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")

if(WINDEPLOYQT)
	add_custom_command(TARGET fundos POST_BUILD
		COMMAND ${WINDEPLOYQT} $<TARGET_FILE:fundos>
		COMMENT "Running windeployqt..."
	)
endif()

if(WIN32)
	install(TARGETS fundos
		RUNTIME DESTINATION .
	)
else()
	install(TARGETS fundos
		RUNTIME DESTINATION bin
	)
endif()

if(WIN32)
	install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
		DESTINATION .
	)
else()
	install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
		DESTINATION share/doc/fundos
	)
	install(FILES "${CMAKE_SOURCE_DIR}/res/fundos.desktop"
		DESTINATION share/applications
	)
	install(FILES "${CMAKE_SOURCE_DIR}/res/icon.svg"
		DESTINATION share/icons/hicolor/scalable/apps
		RENAME fundos.svg
	)
	install(FILES "${CMAKE_SOURCE_DIR}/res/icon.png"
		DESTINATION share/icons/hicolor/256x256/apps
		RENAME fundos.png
	)
endif()

if(WINDEPLOYQT)
	install(CODE "
		execute_process(
			COMMAND \"${WINDEPLOYQT}\" \"\${CMAKE_INSTALL_PREFIX}/fundos.exe\"
		)
	")
endif()
