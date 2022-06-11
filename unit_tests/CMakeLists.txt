project(Google_tests)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
add_subdirectory(lib/googletest)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_executable(Google_Tests_run pec_store_map_test.cpp)
target_link_libraries(Google_Tests_run gtest gtest_main)