cmake_minimum_required( VERSION 3.0 FATAL_ERROR )
set( CMAKE_VERBOSE_MAKEFILE ON )
set(CMAKE_CXX_STANDARD 14)
set(CINDER_COPY_ASSETS ON)
project( LightConttol )

get_filename_component( CINDER_PATH "../../cinder_0.9.1_mac" ABSOLUTE )
get_filename_component( APP_PATH "." ABSOLUTE )


SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CINDER_PATH}/proj/cmake/modules)
include( "${CINDER_PATH}/proj/cmake/modules/cinderMakeApp.cmake" )

SET(VENDOR_DIR ${APP_PATH}/vendor)
add_subdirectory("${VENDOR_DIR}/poco-poco-1.9.0-release")

set( SRC_FILES
	${APP_PATH}/src/LightControlApp.cpp
	${APP_PATH}/src/Output.cpp
)

message(STATUS "Poco components: ${Poco_COMPONENTS}")

ci_make_app(
	SOURCES     ${SRC_FILES}
	CINDER_PATH ${CINDER_PATH}
	BLOCKS      DMXusbPro Cinder-ImGui OSC
	LIBRARIES 	PocoFoundation PocoNet PocoDNSSD PocoDNSSDBonjour
)

