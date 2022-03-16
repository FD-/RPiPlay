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

if( UNIX AND NOT APPLE )
  find_package(OpenSSL 1.1.1 REQUIRED)
  target_compile_definitions(airplay PUBLIC OPENSSL_API_COMPAT=0x10101000L)
  target_link_libraries( airplay OpenSSL::Crypto )
  target_include_directories( airplay PUBLIC ${DNSSD_INCLUDE_DIRS} )
  target_include_directories( airplay PUBLIC ${PLIST_INCLUDE_DIRS} )
  link_directories(${DNS_SD_LIBRARY_DIRS})
  link_directories(${PLIST_LIBRARY_DIRS})
  target_link_libraries( airplay ${PLIST_LIBRARIES})
  target_link_libraries( airplay ${DNSSD_LIBRARIES})
else()
  include_directories( /usr/local/opt/openssl@1.1/include/ )
  target_link_libraries( airplay /usr/local/opt/openssl@1.1/lib/libcrypto.a )
endif()
        
