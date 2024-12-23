# This file is included by DuckDB's build system. It specifies which extension to load

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_BUILD ON)

# Extension from this repo
duckdb_extension_load(vcf
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
)


# Any extra extensions that should be built
# e.g.: duckdb_extension_load(json)
