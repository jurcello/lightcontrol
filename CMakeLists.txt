cmake_minimum_required( VERSION 3.0 FATAL_ERROR )

set( CMAKE_VERBOSE_MAKEFILE ON )
set(CMAKE_CXX_STANDARD 14)
set(MACOSX_DEPLOYMENT_TARGET 10.8)
# for all binaries created in a CMake project:

set(CINDER_COPY_ASSETS ON)
project( LightControl )

get_filename_component( CINDER_PATH "../../cinder_0.9.1_mac" ABSOLUTE )
SET(ARG_ASSETS_PATH ${APP_PATH}/assets)
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

SET(OLDER_OSX_CFLAGS "-g -O2 -stdlib=libc++ -mmacosx-version-min=10.8 -isysroot /Developer/SDKs/MacOSX10.8.sdk")
SET(OLDER_OSX_CXXFLAGS "-g -O2 -stdlib=libc++ -mmacosx-version-min=10.8 -isysroot /Developer/SDKs/MacOSX10.8.sdk")
SET(OLDER_OSX_LDFLAGS "-mmacosx-version-min=10.8 -stdlib=libc++ -isysroot /Developer/SDKs/MacOSX10.8.sdk")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OLDER_OSX_CXXFLAGS}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OLDER_OSX_CFLAGS}")
SET(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} ${OLDER_OSX_LDFLAGS}")
SET(CMAKE_INSTALL_RPATH "@loader_path/lib")