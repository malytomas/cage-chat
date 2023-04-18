cmake_minimum_required(VERSION 3.22)
project(cagechat CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cage/cmake")
include(cage_all)
cage_default_init()

add_subdirectory(externals/cage)

file(GLOB_RECURSE common-sources "sources/common/*")
file(GLOB_RECURSE server-sources "sources/server/*")
file(GLOB_RECURSE client-sources "sources/client/*")

add_library(common STATIC ${common-sources})
target_link_libraries(common cage-core)
cage_ide_category(common cagechat)
cage_ide_sort_files(common)

add_executable(cagechat-server ${server-sources})
target_link_libraries(cagechat-server common)
cage_ide_category(cagechat-server cagechat)
cage_ide_sort_files(cagechat-server)
cage_ide_working_dir_in_place(cagechat-server)

add_executable(cagechat ${client-sources})
target_link_libraries(cagechat common cage-simple)
cage_ide_category(cagechat cagechat)
cage_ide_sort_files(cagechat)
cage_ide_working_dir_in_place(cagechat)
