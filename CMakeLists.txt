cmake_minimum_required(VERSION 3.5)

project(pinpoint LANGUAGES CXX C)

set(PINPOINT_EXECUTABLE_NAME ${CMAKE_PROJECT_NAME})
set(PINPONT_LIBRARY_NAME     "lib${PINPOINT_EXECUTABLE_NAME}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

option(ENABLE_NVML "Build with NVML support (if available)" ON)
option(ENABLE_APPLE_SILICON_MODEL "If building on Apple Silicon, always use 'Energy Model' channel" OFF)

include_directories(.)
include_directories(src)
find_package(Threads)

if(APPLE AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
  set(APPLE_SILICON TRUE)
else()
  set(APPLE_SILICON FALSE)
endif()

set(APPLE_IOREPORT_LIBRARIES "-lIOReport -framework CoreFoundation")

if(ENABLE_NVML)
	find_package(NVML)
endif(ENABLE_NVML)

if(ENABLE_APPLE_SILICON_MODEL)
	add_definitions(-DENABLE_APPLE_SILICON_MODEL=1)
endif(ENABLE_APPLE_SILICON_MODEL)

if(NVML_FOUND)
	include_directories(${NVML_INCLUDE_DIRS})
	add_definitions(-DUSE_NVML=1)
endif(NVML_FOUND)

#####

set(SOURCE_FILES
	src/EnergyDataSource.cpp
	src/Experiment.cpp
	src/PinPointLib.cpp
	src/PowerDataSource.cpp
	src/Registry.cpp
	src/Sampler.cpp
	src/Settings.cpp
	src/pinpoint_c.cpp
	src/data_sources/A64FX.cpp
	src/data_sources/INA226.cpp
	src/data_sources/JetsonCounter.cpp
	src/data_sources/MCP_EasyPower.cpp
	src/data_sources/NVML.cpp
	src/data_sources/RAPL.cpp
	src/data_sources/mcp_com.c
)

if(APPLE_SILICON)
	list(APPEND SOURCE_FILES
		src/data_sources/AppleM.cpp
	)
endif(APPLE_SILICON)

#####

add_library(pinpoint_objects OBJECT ${SOURCE_FILES})

add_library(${PINPONT_LIBRARY_NAME} SHARED $<TARGET_OBJECTS:pinpoint_objects>)
set_target_properties(${PINPONT_LIBRARY_NAME} PROPERTIES OUTPUT_NAME ${PINPOINT_EXECUTABLE_NAME})


add_executable(${PINPOINT_EXECUTABLE_NAME}
	src/main.cpp
)
target_link_libraries(${PINPOINT_EXECUTABLE_NAME}  $<TARGET_OBJECTS:pinpoint_objects>)

#####

set(ADDITIONAL_LIBRARIES)
list(APPEND ADDITIONAL_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

if(NVML_FOUND)
	list(APPEND ADDITIONAL_LIBRARIES ${NVML_LIBRARIES})
endif(NVML_FOUND)


if(APPLE_SILICON)
	message(STATUS "Detected Apple Silicon. Link IOReport")
	list(APPEND ADDITIONAL_LIBRARIES ${APPLE_IOREPORT_LIBRARIES})
endif(APPLE_SILICON)

#####

target_link_libraries(${PINPONT_LIBRARY_NAME} ${ADDITIONAL_LIBRARIES})
target_link_libraries(${PINPOINT_EXECUTABLE_NAME} ${ADDITIONAL_LIBRARIES})
