cmake_minimum_required(VERSION 3.4.1)
include_directories( playfair llhttp )

aux_source_directory(. play_src)
set(DIR_SRCS ${play_src})

add_library( airplay
        STATIC
        ${DIR_SRCS}
        )

find_library( LIBPLIST NAMES plist plist-2.0 )

target_link_libraries( airplay
	    pthread
        playfair
        llhttp
        ${LIBPLIST} )

file(GLOB INCLUDE_FILES "*.h")
set_target_properties(airplay PROPERTIES PUBLIC_HEADER "${INCLUDE_FILES}")

if( UNIX AND NOT APPLE )
  find_package(OpenSSL 1.1.1 REQUIRED)
  target_compile_definitions(airplay PUBLIC OPENSSL_API_COMPAT=0x10101000L)
  target_link_libraries( airplay OpenSSL::Crypto )
  target_link_libraries( airplay dns_sd )
else()
  include_directories( /usr/local/opt/openssl@1.1/include/ )
  target_link_libraries( airplay /usr/local/opt/openssl@1.1/lib/libcrypto.a )
endif()

# For pkg-config file generation
set(DEST_DIR "${CMAKE_INSTALL_PREFIX}")
get_target_property(airplay_libs airplay LINK_LIBRARIES)
foreach(LIB ${airplay_libs} m) # Add math library, needed by playfair
  if( "${LIB}" STREQUAL OpenSSL::Crypto )
    set(PRIVATE_LIBS "${PRIVATE_LIBS} ${OPENSSL_CRYPTO_LIBRARY}")
  elseif( IS_ABSOLUTE "${LIB}" )
    set(PRIVATE_LIBS "${PRIVATE_LIBS} ${LIB}")
  else()
    set(PRIVATE_LIBS "${PRIVATE_LIBS} -l${LIB}")
  endif()
endforeach()
configure_file(airplay.pc.in airplay.pc @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/airplay.pc" DESTINATION lib/pkgconfig)

install(TARGETS airplay
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include/airplay)
