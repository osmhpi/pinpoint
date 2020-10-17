cmake_minimum_required(VERSION 3.5)

project(pinpoint LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)
find_package(Threads)

add_executable(pinpoint 
	src/Experiment.cpp
	src/Sampler.cpp
    src/Settings.cpp
	src/Registry.cpp
	src/data_sources/JetsonCounter.cpp
	src/data_sources/MCP_EasyPower.cpp
	src/data_sources/mcp_com.c
	src/main.cpp
)

target_link_libraries(pinpoint ${CMAKE_THREAD_LIBS_INIT})
