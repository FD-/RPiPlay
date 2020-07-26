cmake_minimum_required(VERSION 3.4.1)

project(rpiplay)

set (CMAKE_CXX_STANDARD 11)

if(NOT RENDERER)
  set(RENDERER "openmax")
endif()
string(TOUPPER ${RENDERER} RENDERER_UPPERCASE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRENDERER_${RENDERER_UPPERCASE}")

add_subdirectory(lib/curve25519)
add_subdirectory(lib/ed25519)
add_subdirectory(lib/playfair)
add_subdirectory(lib)
add_subdirectory(renderers)

add_executable( rpiplay rpiplay.cpp)
target_link_libraries ( rpiplay renderers airplay )

install(PROGRAMS rpiplay DESTINATION bin)
