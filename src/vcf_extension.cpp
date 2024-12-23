// src/vcf_extension.cpp
#define DUCKDB_EXTENSION_MAIN
#include "vcf_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/main/extension_util.hpp"

namespace duckdb {

// Declare the function defined in vcf_reader.cpp
void RegisterVcfReader(DatabaseInstance &db);

void VcfExtension::Load(DuckDB &db) { RegisterVcfReader(*db.instance); }

std::string VcfExtension::Name() { return "vcf"; }

std::string VcfExtension::Version() const {
#ifdef EXT_VERSION_VCF
  return EXT_VERSION_VCF;
#else
  return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void vcf_init(duckdb::DatabaseInstance &db) {
  duckdb::DuckDB db_wrapper(db);
  db_wrapper.LoadExtension<duckdb::VcfExtension>();
}

DUCKDB_EXTENSION_API const char *vcf_version() {
  return duckdb::DuckDB::LibraryVersion();
}
} // namespace duckdb
