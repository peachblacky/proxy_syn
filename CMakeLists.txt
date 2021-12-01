cmake_minimum_required(VERSION 3.0.0)
project(proxy_syn)

set(CMAKE_CXX_STANDART 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")


add_subdirectory(src)
add_subdirectory(utils)