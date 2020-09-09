cmake_minimum_required(VERSION 3.5)

project(pinpoint LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)
find_package(Threads)

add_executable(pinpoint 
	src/main.cpp
	src/data_sources/mcp_com.c
)

target_link_libraries(pinpoint ${CMAKE_THREAD_LIBS_INIT})
