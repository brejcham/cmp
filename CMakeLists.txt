cmake_minimum_required(VERSION 3.15)

project(cmp_cli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -O3")

find_package(Boost REQUIRED)

add_executable(cmp_cli cmp_cli.cpp common_tools.cpp)
target_link_libraries(cmp_cli ${Boost_LIBRARIES})

add_executable(cmp_srv cmp_srv.cpp common_tools.cpp)
target_link_libraries(cmp_srv ${Boost_LIBRARIES})

