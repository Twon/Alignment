cmake_minimum_required(VERSION 3.8)

project(alignment_example)

add_executable(alignment_example test_alignment.cpp)

set_target_properties(alignment_example PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(alignment_example PRIVATE "/EHsc")
else()
    set(CMAKE_CXX_FLAGS_SANITIZE "-fsanitize=address,undefined")
    set(CMAKE_EXE_LINKER_FLAGS_SANITIZE "-fsanitize=address,undefined")
endif()

if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    target_compile_options(alignment_example PRIVATE "-Werror;-Wall")
endif()

target_include_directories(alignment_example PRIVATE externals/Catch2/single_include)


enable_testing()
add_test(
  NAME alignment_example
  COMMAND alignment_example
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
