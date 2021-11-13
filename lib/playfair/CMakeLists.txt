cmake_minimum_required(VERSION 3.4.1)
aux_source_directory(. playfair_src)
set(DIR_SRCS ${playfair_src})
include_directories(.)
add_library( playfair
        STATIC
        ${DIR_SRCS})
install(TARGETS playfair ARCHIVE DESTINATION lib)
