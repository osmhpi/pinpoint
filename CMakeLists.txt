cmake_minimum_required(VERSION 3.5)

project(pinpoint LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

option(ENABLE_NVML "Build with NVML support (if available)" ON)

include_directories(.)
include_directories(src)
find_package(Threads)

if(ENABLE_NVML)
	find_package(NVML)
endif(ENABLE_NVML)

if(NVML_FOUND)
	include_directories(${NVML_INCLUDE_DIRS})
	add_definitions(-DUSE_NVML)
endif(NVML_FOUND)

add_executable(pinpoint
	src/EnergyDataSource.cpp
	src/Experiment.cpp
	src/PowerDataSource.cpp
	src/Registry.cpp
	src/Sampler.cpp
	src/Settings.cpp
	src/data_sources/INA226.cpp
	src/data_sources/JetsonCounter.cpp
	src/data_sources/MCP_EasyPower.cpp
	src/data_sources/RAPL.cpp
	src/data_sources/mcp_com.c
	src/data_sources/NVML.cpp
	src/main.cpp
)

target_link_libraries(pinpoint ${CMAKE_THREAD_LIBS_INIT})

if(NVML_FOUND)
	target_link_libraries(pinpoint ${NVML_LIBRARIES})
endif(NVML_FOUND)
