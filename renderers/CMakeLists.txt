cmake_minimum_required(VERSION 3.4.1)

# Common Linux cflags
if( UNIX AND NOT APPLE )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT   -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g" )
endif()

# Common x86/x86_64 cflags
if( CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)" )
    set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -march=native" )
endif()

# Always compile the dummy renderers
set( RENDERER_FLAGS "${RENDERER_FLAGS} -DHAS_DUMMY_RENDERER" )
set( RENDERER_SOURCES audio_renderer_dummy.c video_renderer_dummy.c )
set( RENDERER_LINK_LIBS "" )
set( RENDERER_INCLUDE_DIRS "" )

# Check for availability of OpenMAX libraries on Raspberry Pi
find_library( BRCM_GLES_V2 brcmGLESv2 HINTS ${CMAKE_SYSROOT}/opt/vc/lib/ )
find_library( BRCM_EGL brcmEGL HINTS ${CMAKE_SYSROOT}/opt/vc/lib/ )
find_library( OPENMAXIL openmaxil HINTS ${CMAKE_SYSROOT}/opt/vc/lib/ )
find_library( BCM_HOST bcm_host HINTS ${CMAKE_SYSROOT}/opt/vc/lib/ )
find_library( VCOS vcos HINTS ${CMAKE_SYSROOT}/opt/vc/lib/ )
find_library( VCHIQ_ARM vchiq_arm HINTS ${CMAKE_SYSROOT}/opt/vc/lib/ )

if( BRCM_GLES_V2 AND BRCM_EGL AND OPENMAXIL AND BCM_HOST AND VCOS AND VCHIQ_ARM )
  # We have OpenMAX libraries available! Use them!
  message( STATUS "Found OpenMAX libraries for Raspberry Pi" )
  include_directories( ${CMAKE_SYSROOT}/opt/vc/include/ 
  	${CMAKE_SYSROOT}/opt/vc/include/interface/vcos/pthreads 
  	${CMAKE_SYSROOT}/opt/vc/include/interface/vmcs_host/linux 
  	${CMAKE_SYSROOT}/opt/vc/src/hello_pi/libs/ilclient )

  option(BUILD_SHARED_LIBS "" OFF)
  add_subdirectory(fdk-aac EXCLUDE_FROM_ALL)
  add_subdirectory( h264-bitstream )

  set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX   -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi" )
  
  aux_source_directory( ${CMAKE_SYSROOT}/opt/vc/src/hello_pi/libs/ilclient/ ilclient_src )
  set( DIR_SRCS ${ilclient_src} )
  add_library( ilclient STATIC ${DIR_SRCS} )

  target_link_libraries( ilclient ${BRCM_GLES_V2} ${BRCM_EGL} ${OPENMAXIL} 
                         ${BCM_HOST} ${VCOS} ${VCHIQ_ARM} pthread rt m )

  set( RENDERER_FLAGS "${RENDERER_FLAGS} -DHAS_RPI_RENDERER" )
  set( RENDERER_SOURCES ${RENDERER_SOURCES} audio_renderer_rpi.c video_renderer_rpi.c )
  set( RENDERER_LINK_LIBS ${RENDERER_LINK_LIBS} ilclient airplay fdk-aac h264-bitstream )
else()
  message( STATUS "OpenMAX libraries not found, skipping compilation of Raspberry Pi renderer" )
endif()


# Check for availability of gstreamer
find_package( PkgConfig )
#if( PKG_CONFIG_FOUND )
if ( 0 )
  pkg_check_modules( GST gstreamer-1.0>=1.4
                         gstreamer-sdp-1.0>=1.4
                         gstreamer-video-1.0>=1.4
                         gstreamer-app-1.0>=1.4 )
  if( GST_FOUND )
    set( RENDERER_FLAGS "${RENDERER_FLAGS} -DHAS_GSTREAMER_RENDERER" )
    set( RENDERER_SOURCES ${RENDERER_SOURCES} audio_renderer_gstreamer.c video_renderer_gstreamer.c )
    set( RENDERER_LINK_LIBS ${RENDERER_LINK_LIBS} ${GST_LIBRARIES} )
    set( RENDERER_INCLUDE_DIRS ${RENDERER_INCLUDE_DIRS} ${GST_INCLUDE_DIRS} )
  else()
    message( STATUS "GStreamer not found, skipping compilation of GStreamer renderer" )
  endif()
else()
  message( STATUS "pkg-config not found, skipping compilation of GStreamer renderer" )
endif()

# Create the renderers library and link against everything
add_library( renderers STATIC ${RENDERER_SOURCES})
target_link_libraries ( renderers ${RENDERER_LINK_LIBS} )
target_include_directories( renderers PRIVATE ${RENDERER_INCLUDE_DIRS} )

# Pass the final renderer flags up to the parent scope so it knows which renderers
# will be available to use.
set( RENDERER_FLAGS "${RENDERER_FLAGS}" PARENT_SCOPE )
