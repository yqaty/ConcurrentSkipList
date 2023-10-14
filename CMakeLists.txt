cmake_minimum_required(VERSION 3.22)

project(skiplist)

set(CMAKE_CXX_STANDARD 20)

aux_source_directory(./src DIR_SRC)

add_executable(skiplist ${DIR_SRC} ./test/skiplist_test.cc)

target_include_directories(skiplist PRIVATE ./include)