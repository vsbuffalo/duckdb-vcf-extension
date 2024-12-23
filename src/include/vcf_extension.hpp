// src/include/vcf_extension.hpp
#pragma once
#include "duckdb.hpp"

namespace duckdb {

class VcfExtension : public Extension {
public:
  void Load(DuckDB &db) override;
  std::string Name() override;
  std::string Version() const override;
};

} // namespace duckdb
