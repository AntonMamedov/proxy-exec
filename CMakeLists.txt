cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("Linux Kernel Module with CLion IDE support / CMake" VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find kernel headers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(KernelHeaders REQUIRED)


# find MODULE_LICENSE("GPL"), MODULE_AUTHOR() etc.
# thanks to "merseyviking" from stack overflow
add_definitions(-D__KERNEL__ -DMODULE)
include_directories(include)
add_subdirectory(tests)
add_subdirectory(unit_tests)
add_executable(pec src/proxy_execute.c src/callsyms.c src/page_rw.c src/store.c src/pec_buffer.c include/program_args.h src/program_args.c include/logger.h)
add_executable(fsf tools/free_space_finder.c)
target_include_directories("pec" PRIVATE ${KERNELHEADERS_INCLUDE_DIRS})
target_include_directories("fsf" PRIVATE ${KERNELHEADERS_INCLUDE_DIRS})