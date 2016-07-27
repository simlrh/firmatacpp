cmake_minimum_required(VERSION 3.1)
project(firmatacpp)

set (CMAKE_CXX_STANDARD 11)

option(FIRMATA_BUILD_EXAMPLES "Build firmata example programs" YES)

include (GenerateExportHeader)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cmake/serial/CMakeLists.txt DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/vendor/serial)
add_subdirectory(vendor/serial)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR} 
	${CMAKE_CURRENT_SOURCE_DIR}/include 
	${CMAKE_CURRENT_SOURCE_DIR}/vendor/serial/include
	)

set(FIRMATACPP_SOURCES 
	src/firmbase.cpp
	src/firmi2c.cpp
	src/firmserial.cpp 
	)

set(FIRMATACPP_INCLUDES 
	include/firmata_constants.h
	include/firmata.h
	include/firmbase.h
	include/firmi2c.h
	include/firmio.h 
	include/firmserial.h 
	${CMAKE_CURRENT_BINARY_DIR}/firmatacpp_export.h
	)

add_library(firmatacpp ${FIRMATACPP_SOURCES} ${FIRMATACPP_INCLUDES})
generate_export_header(firmatacpp)
set_target_properties(firmatacpp PROPERTIES
  COMPILE_FLAGS -DLIBSHARED_AND_STATIC_STATIC_DEFINE)

target_link_libraries(firmatacpp serial)

if (FIRMATA_BUILD_EXAMPLES)
	add_executable(simple_example examples/simple.cpp)
	target_link_libraries(simple_example firmatacpp)
endif()
