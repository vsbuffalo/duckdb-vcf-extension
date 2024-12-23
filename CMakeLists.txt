cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TARGET_NAME vcf)
project(${TARGET_NAME})

# Find required libraries
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CURL libcurl)
    pkg_check_modules(LIBLZMA liblzma)
endif()

find_package(CURL REQUIRED)
find_package(BZip2 REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)  # Add zlib

# Build htslib
set(HTSLIB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/htslib)
add_custom_command(
    OUTPUT ${HTSLIB_SOURCE_DIR}/libhts.a
    COMMAND autoreconf -i
    COMMAND ./configure
    COMMAND make
    WORKING_DIRECTORY ${HTSLIB_SOURCE_DIR}
)
add_custom_target(htslib_build DEPENDS ${HTSLIB_SOURCE_DIR}/libhts.a)

# Add sources
set(EXTENSION_SOURCES
    src/vcf_extension.cpp
    src/vcf_reader.cpp
)

# Include directories
include_directories(src/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/htslib)

# Build extensions
build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

# Add dependency on htslib
add_dependencies(${EXTENSION_NAME} htslib_build)
add_dependencies(${LOADABLE_EXTENSION_NAME} htslib_build)

# Link dependencies
foreach(TARGET ${EXTENSION_NAME} ${LOADABLE_EXTENSION_NAME})
    target_link_libraries(${TARGET}
        OpenSSL::SSL
        OpenSSL::Crypto
        CURL::libcurl
        BZip2::BZip2
        LibLZMA::LibLZMA
        ZLIB::ZLIB       # Add zlib
        ${HTSLIB_SOURCE_DIR}/libhts.a
    )
endforeach()

# Installation
install(
    TARGETS ${EXTENSION_NAME}
    EXPORT "${DUCKDB_EXPORT_SET}"
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
    ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
)
