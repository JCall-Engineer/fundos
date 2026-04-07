include(FetchContent)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS *.cpp)
add_executable(tests ${test_sources})

target_link_libraries(tests PRIVATE core GTest::gtest_main)

include(CTest)
include(GoogleTest)
gtest_discover_tests(tests)
