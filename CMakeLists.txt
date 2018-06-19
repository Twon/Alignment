cmake_minimum_required(VERSION 2.8)

project(alignment_example)

if(MSVC)
  add_compile_options("/EHsc")
  add_compile_options("/std:c++latest")
else()
  add_compile_options("-std=c++17")
  set(CMAKE_CXX_FLAGS_SANITIZE "-fsanitize=address,undefined")
  set(CMAKE_EXE_LINKER_FLAGS_SANITIZE "-fsanitize=address,undefined")
endif()

if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
  add_compile_options("-Werror;-Wall")
endif()

include_directories(externals/Catch2/single_include)
add_executable(alignment_example test_alignment.cpp)

enable_testing()
add_test(
  NAME alignment_example
  COMMAND alignment_example
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
