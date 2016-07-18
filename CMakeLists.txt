cmake_minimum_required(VERSION 2.8.5)
project(firmatacpp)

include (GenerateExportHeader)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cmake/serial/CMakeLists.txt DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/vendor/serial)
add_subdirectory(vendor/serial)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR} 
	${CMAKE_CURRENT_SOURCE_DIR}/include 
	${CMAKE_CURRENT_SOURCE_DIR}/vendor/serial/include
	)

set(FIRMATACPP_SOURCES 
	src/firmata.cpp
	src/firmserial.cpp 
	)

set(FIRMATACPP_INCLUDES 
	include/firmata.h
	include/firmio.h 
	include/firmserial.h 
	${CMAKE_CURRENT_BINARY_DIR}/firmatacpp_export.h
	)

add_library(firmatacpp ${FIRMATACPP_SOURCES} ${FIRMATACPP_INCLUDES})
generate_export_header(firmatacpp)
set_target_properties(firmatacpp PROPERTIES
  COMPILE_FLAGS -DLIBSHARED_AND_STATIC_STATIC_DEFINE)

target_link_libraries(firmatacpp serial)