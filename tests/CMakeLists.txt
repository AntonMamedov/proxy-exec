
project("dev_test")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
add_executable(dev_test main.c)
add_executable(dev_test2 tester.cpp)