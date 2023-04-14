cmake_minimum_required(VERSION 3.4.1)
include_directories( playfair llhttp )

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPLIST REQUIRED libplist-2.0)
include_directories(${LIBPLIST_INCLUDE_DIRS})
link_directories(${LIBPLIST_LIBRARY_DIRS})

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

if( UNIX )
  find_package(OpenSSL 1.1.1 REQUIRED)
  target_compile_definitions(airplay PUBLIC OPENSSL_API_COMPAT=0x10101000L)
  target_link_libraries( airplay OpenSSL::Crypto )
  if (NOT APPLE)
    target_link_libraries( airplay dns_sd )
  endif ()
else()
  include_directories( /usr/local/opt/openssl@1.1/include/ )
  target_link_libraries( airplay /usr/local/opt/openssl@1.1/lib/libcrypto.a )
endif()
        
