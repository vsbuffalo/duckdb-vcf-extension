cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(TARGET_NAME vcf)
set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

# Detect and set architecture if not specified
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    # Detect native architecture
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures" FORCE)
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures" FORCE)
    endif()
endif()

project(${TARGET_NAME})

# Find required packages
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(CURL REQUIRED)
find_package(LibLZMA REQUIRED)
include(ExternalProject)

# Download HTSlib first
execute_process(
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/download-htslib.sh"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE DOWNLOAD_RESULT
)
if(NOT DOWNLOAD_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to download/extract HTSlib")
endif()

# HTSlib configuration
set(HTSLIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/htslib/htslib-1.21")

# HTSlib configuration modification
ExternalProject_Add(
    htslib_ep
    PREFIX "${CMAKE_BINARY_DIR}/htslib"
    SOURCE_DIR ${HTSLIB_SOURCE_DIR}
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND
        autoreconf -i ${HTSLIB_SOURCE_DIR}
        COMMAND ${HTSLIB_SOURCE_DIR}/configure
            --prefix=${CMAKE_BINARY_DIR}/htslib --disable-bz2
    BUILD_COMMAND make
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND make install
    BUILD_BYPRODUCTS "${CMAKE_BINARY_DIR}/htslib/lib/libhts.a"
)

add_library(htslib STATIC IMPORTED GLOBAL)
set_target_properties(htslib PROPERTIES
    IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/htslib/lib/libhts.a"
    INTERFACE_INCLUDE_DIRECTORIES "${HTSLIB_SOURCE_DIR}"
)
add_dependencies(htslib htslib_ep)

# Extension configuration
include_directories(src/include)
set(EXTENSION_SOURCES src/vcf_extension.cpp src/vcf_reader.cpp)
build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

foreach(TARGET ${EXTENSION_NAME} ${LOADABLE_EXTENSION_NAME})
    target_link_libraries(${TARGET}
        htslib
        CURL::libcurl
        BZip2::BZip2
        ZLIB::ZLIB
        LibLZMA::LibLZMA
    )
endforeach()

install(
    TARGETS ${EXTENSION_NAME}
    EXPORT "${DUCKDB_EXPORT_SET}"
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
    ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
)
