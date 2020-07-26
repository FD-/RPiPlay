cmake_minimum_required(VERSION 3.4.1)

if(RENDERER STREQUAL "openmax")
  include_directories( /opt/vc/include/ 
  	/opt/vc/include/interface/vcos/pthreads 
  	/opt/vc/include/interface/vmcs_host/linux 
  	/opt/vc/src/hello_pi/libs/ilclient )
  
  add_subdirectory( fdk-aac )
  add_subdirectory( h264-bitstream )

  include_directories( fdk-aac/libAACdec/include
      fdk-aac/libAACenc/include
      fdk-aac/libFDK/include
      fdk-aac/libMpegTPDec/include
      fdk-aac/libMpegTPEnc/include
      fdk-aac/libPCMutils/include
      fdk-aac/libSBRdec/include
      fdk-aac/libSBRenc/include
      fdk-aac/libSYS/include )
  
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT   -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX   -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi")
  
  aux_source_directory(/opt/vc/src/hello_pi/libs/ilclient/ ilclient_src)
  set(DIR_SRCS ${ilclient_src})
  add_library( ilclient
          STATIC
          ${DIR_SRCS})
  
  find_library( BRCM_GLES_V2 brcmGLESv2 HINTS /opt/vc/lib/ )
  find_library( BRCM_EGL brcmEGL HINTS /opt/vc/lib/ )
  find_library( OPENMAXIL openmaxil HINTS /opt/vc/lib/ )
  find_library( BCM_HOST bcm_host HINTS /opt/vc/lib/ )
  find_library( VCOS vcos HINTS /opt/vc/lib/ )
  find_library( VCHIQ_ARM vchiq_arm HINTS /opt/vc/lib/ )
  target_link_libraries( ilclient ${BRCM_GLES_V2} ${BRCM_EGL} ${OPENMAXIL} 
                         ${BCM_HOST} ${VCOS} ${VCHIQ_ARM} pthread rt m ) 
  
  add_library( renderers
          STATIC
          audio_renderer_rpi.c video_renderer_rpi.c)
  target_link_libraries ( renderers ilclient airplay fdk-aac h264-bitstream )
elseif(RENDERER STREQUAL "gstreamer")
  find_package(PkgConfig)

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -march=native -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g")

  pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4
                                 gstreamer-sdp-1.0>=1.4
                                 gstreamer-video-1.0>=1.4
                                 gstreamer-app-1.0>=1.4)

  add_library( renderers
        STATIC
        audio_renderer_gstreamer.c video_renderer_gstreamer.c)
  include_directories ( renderers ${GST_INCLUDE_DIRS} )
  target_link_libraries ( renderers ${GST_LIBRARIES} airplay )
elseif(RENDERER STREQUAL "dummy")
  add_subdirectory( h264-bitstream )
  add_library( renderers
          STATIC
          audio_renderer_dummy.c video_renderer_dummy.c)
  target_link_libraries ( renderers h264-bitstream )
else()
	message(FATAL_ERROR "Invalid renderer specified")
endif()
