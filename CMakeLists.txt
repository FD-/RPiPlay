cmake_minimum_required(VERSION 3.4.1)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
		STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

project(rpiplay)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_C_FLAGS -fPIC)
set (CMAKE_CXX_FLAGS -fPIC)

set (RENDERER_FLAGS "")

add_subdirectory(lib/playfair)
add_subdirectory(lib/llhttp)
add_subdirectory(lib)
add_subdirectory(renderers)

# Make sure the main executable is aware of the available renderers
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RENDERER_FLAGS}" )

add_executable( rpiplay rpiplay.cpp)
target_link_libraries ( rpiplay renderers airplay )

install(TARGETS rpiplay RUNTIME DESTINATION bin)
