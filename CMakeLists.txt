cmake_minimum_required(VERSION 3.25)
project(fundos LANGUAGES C CXX)
set(FUNDOS_DESKTOP_VERSION 1.0.0)
set(FUNDOS_DISPLAY_NAME  "FundOS")
set(FUNDOS_VENDOR        "JCall.Engineer")
set(FUNDOS_DESCRIPTION   "Envelope budgeting for people who want to own their data")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)

if(MSVC)
	add_compile_options(
		/W4
		/permissive-
		/Zc:__cplusplus  # Report correct __cplusplus value
		/Zc:inline       # Remove unreferenced functions/data
		/Zc:throwingNew  # Assume operator new throws
	)
else()
	add_compile_options(
		-Wall
		-Wextra
		-Wno-missing-field-initializers # The core library is careful to zero initialize integral types, and relying on default constructors is pervasive in this codebase
	)
endif()

set(VALID_PLATFORMS desktop android)
if(NOT FUNDOS_PLATFORM IN_LIST VALID_PLATFORMS)
	message(FATAL_ERROR "Set FUNDOS_PLATFORM to one of: ${VALID_PLATFORMS}")
endif()

set(FETCHCONTENT_BASE_DIR               ${CMAKE_BINARY_DIR}/build/_deps)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/sqlite3            ${CMAKE_BINARY_DIR}/build/sqlite3)
add_subdirectory(src/core               ${CMAKE_BINARY_DIR}/build/core)
add_subdirectory(tests                  ${CMAKE_BINARY_DIR}/build/tests)
add_subdirectory(src/${FUNDOS_PLATFORM} ${CMAKE_BINARY_DIR}/build/${FUNDOS_PLATFORM})

# ── Packaging ────────────────────────────────────────────────────────────────

set(CPACK_PACKAGE_NAME                ${FUNDOS_DISPLAY_NAME})
set(CPACK_PACKAGE_VENDOR              ${FUNDOS_VENDOR})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${FUNDOS_DESCRIPTION})
set(CPACK_PACKAGE_VERSION             ${FUNDOS_DESKTOP_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   ${FUNDOS_DISPLAY_NAME})
set(CPACK_OUTPUT_FILE_PREFIX          "${CMAKE_SOURCE_DIR}/out/packages/${FUNDOS_DESKTOP_VERSION}")

# Debian
set(CPACK_DEBIAN_PACKAGE_DEPENDS
	"libqt6widgets6t64 (>= 6.4), libqt6svg6 (>= 6.4), libqt6concurrent6t64 (>= 6.4)"
)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "JCall.Engineer <jcall.engineer@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "finance")
set(CPACK_BINARY_DEB "ON")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/res/postinst")

# RPM
set(CPACK_RPM_PACKAGE_REQUIRES
	"qt6-qtbase >= 6.4, qt6-qtsvg >= 6.4"
)
set(CPACK_RPM_PACKAGE_LICENSE "AGPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Finance")
set(CPACK_RPM_PACKAGE_URL "https://github.com/JCall-Engineer/fundos")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/res/postinst")
set(CPACK_BINARY_RPM "ON")

# WiX
set(CPACK_WIX_INSTALL_SCOPE       "perMachine")
set(CPACK_WIX_UPGRADE_GUID        "3ba342f2-c11c-4d98-ae18-cd65770fd8c9")
set(CPACK_WIX_PRODUCT_ICON        "${CMAKE_SOURCE_DIR}/res/icon.ico")
set(CPACK_WIX_LICENSE_RTF         "${CMAKE_SOURCE_DIR}/res/LICENSE.rtf")
set(CPACK_WIX_EXTRA_SOURCES       "${CMAKE_SOURCE_DIR}/res/wix_shortcut.wxs")
set(CPACK_WIX_PATCH_FILE          "${CMAKE_SOURCE_DIR}/res/wix_patch.xml")

include(CPack)
