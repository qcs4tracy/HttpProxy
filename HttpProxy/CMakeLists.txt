cmake_minimum_required(VERSION 2.8)
project(HttpProxy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
include_directories(/usr/include/)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "./")

add_executable(http_proxy main.cpp socket.h socket.cpp message.h mem_alloc.h mem_alloc.cpp http_parser.h http_parser_cpp.cpp httplib.h httplib.cpp blacklist.h blacklist.cpp jsoncpp.cpp filter/porter2_stemmer.cpp filter/wordfilter.cpp)
target_link_libraries(http_proxy pthread)

