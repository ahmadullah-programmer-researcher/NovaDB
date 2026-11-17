cmake_minimum_required(VERSION 3.16)
project(NovaDB VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(novadb src/db.cpp src/codec.cpp)
target_include_directories(novadb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(novadb PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

add_executable(novadb_demo examples/basic.cpp)
target_link_libraries(novadb_demo PRIVATE novadb)

add_executable(novadb_quickstart examples/quickstart.cpp)
target_link_libraries(novadb_quickstart PRIVATE novadb)

include(CTest)
if(BUILD_TESTING)
  add_executable(novadb_tests tests/db_test.cpp)
  target_link_libraries(novadb_tests PRIVATE novadb)
  add_test(NAME novadb_tests COMMAND novadb_tests)
endif()
