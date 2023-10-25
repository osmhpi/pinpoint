cmake_minimum_required(VERSION 3.5)

project(pinpoint LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

option(ENABLE_NVML "Build with NVML support (if available)" ON)

include_directories(.)
include_directories(src)
find_package(Threads)

if(APPLE AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
  set(APPLE_SILICON TRUE)
else()
  set(APPLE_SILICON FALSE)
endif()

set(APPLE_IOREPORT_LIBRARIES "-lIOReport -framework Foundation -framework IOKit")

if(ENABLE_NVML)
	find_package(NVML)
endif(ENABLE_NVML)

if(NVML_FOUND)
	include_directories(${NVML_INCLUDE_DIRS})
	add_definitions(-DUSE_NVML)
endif(NVML_FOUND)

#####

set(SOURCE_FILES
	src/EnergyDataSource.cpp
	src/Experiment.cpp
	src/PowerDataSource.cpp
	src/Registry.cpp
	src/Sampler.cpp
	src/Settings.cpp
	src/data_sources/A64FX.cpp
	src/data_sources/AppleM.mm
	src/data_sources/INA226.cpp
	src/data_sources/JetsonCounter.cpp
	src/data_sources/MCP_EasyPower.cpp
	src/data_sources/NVML.cpp
	src/data_sources/RAPL.cpp
	src/data_sources/mcp_com.c
	src/main.cpp
)

if(APPLE_SILICON)
	list(APPEND SOURCE_FILES
		src/data_sources/AppleM.mm
	)
endif(APPLE_SILICON)

add_executable(pinpoint ${SOURCE_FILES})

#####

target_link_libraries(pinpoint ${CMAKE_THREAD_LIBS_INIT})

if(NVML_FOUND)
	target_link_libraries(pinpoint ${NVML_LIBRARIES})
endif(NVML_FOUND)


if(APPLE_SILICON)
	message(STATUS "Detected Apple Silicon. Link IOReport")
	target_link_libraries(pinpoint ${APPLE_IOREPORT_LIBRARIES})
endif(APPLE_SILICON)
