cmake_minimum_required(VERSION 3.1)
project(firmatacpp)

set (CMAKE_CXX_STANDARD 11)

option(FIRMATA_BUILD_EXAMPLES "Build firmata example programs" YES)
option(FIRMATA_WITH_BLUETOOTH "Enable Bluetooth support" YES)

if (FIRMATA_WITH_BLUETOOTH)
    include (ExternalProject)

    ExternalProject_Add(libblepp
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libblepp-master
        PATCH_COMMAND patch -p0 < ../libblepp-master.patch
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vendor/libblepp-master/configure --prefix=<INSTALL_DIR>
        BUILD_COMMAND ${MAKE})
    ExternalProject_Get_Property(libblepp install_dir)
    set(FIRMATA_BLESRC src/firmble.cpp)
    set(FIRMATA_BLEHDR include/firmble.h)
else()
    set(FIRMATA_BLESRC)
    set(FIRMATA_BLEHDR)
endif()


include (GenerateExportHeader)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cmake/serial/CMakeLists.txt DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/vendor/serial)
add_subdirectory(vendor/serial)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR} 
	${CMAKE_CURRENT_SOURCE_DIR}/include 
	${CMAKE_CURRENT_SOURCE_DIR}/vendor/serial/include
	)

if (FIRMATA_WITH_BLUETOOTH)
    include_directories( ${install_dir}/include)
endif()

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

SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g3" )

add_library(firmatacpp ${FIRMATACPP_SOURCES} ${FIRMATA_BLESRC} ${FIRMATACPP_INCLUDES} ${FIRMATA_BLEHDR})
generate_export_header(firmatacpp)
set_target_properties(firmatacpp PROPERTIES
  COMPILE_FLAGS -DLIBSHARED_AND_STATIC_STATIC_DEFINE)

link_directories(
        ${install_dir}/lib
)

target_link_libraries(firmatacpp serial)

if (FIRMATA_BUILD_EXAMPLES)
	add_executable(simple_example examples/simple.cpp)
	target_link_libraries(simple_example firmatacpp)

        if (FIRMATA_WITH_BLUETOOTH)
                add_executable(simple_exampleble examples/simpleble.cpp)
                target_link_libraries(simple_exampleble firmatacpp ble++)
        endif()
endif()
