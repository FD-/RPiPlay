cmake_minimum_required(VERSION 3.4.1)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
		STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

project(rpiplay)

include(GNUInstallDirs)

set (CMAKE_CXX_STANDARD 11)

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

# Optional systemd service for running RPiPlay at boot. The command line
# can be adjusted with -DRPIPLAY_SERVICE_ARGS="-vr gstreamer -ar alsa -a analog".
set(RPIPLAY_SERVICE_ARGS "" CACHE STRING
	"Command line arguments for the systemd rpiplay.service")
configure_file(systemd/rpiplay.service.in systemd/rpiplay.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/systemd/rpiplay.service
	DESTINATION lib/systemd/system)
