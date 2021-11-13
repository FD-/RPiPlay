cmake_minimum_required(VERSION 3.4.1)

set(fdk_aac_path .)

add_library(fdk-aac STATIC)

set(COMPONENTS
	libAACdec
	libArithCoding
	libDRCdec
	libFDK
	libMpegTPDec
	libPCMutils
	libSACdec
	libSBRdec
	libSYS
)

foreach(COMPONENT ${COMPONENTS})
	aux_source_directory(${fdk_aac_path}/${COMPONENT}/src SRCS_${COMPONENT})
	target_sources(fdk-aac PRIVATE ${SRCS_${COMPONENT}})
	target_include_directories(fdk-aac PRIVATE ${fdk_aac_path}/${COMPONENT}/include)
endforeach(COMPONENT)
