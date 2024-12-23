// src/vcf_reader.cpp
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "vcf_extension.hpp"
// #include <htslib/tbx.h>
#include <cstdio>
#include <htslib/vcf.h>
#include <optional>
#include <sstream>

namespace duckdb {

// Bind data structure to store VCF reader state
struct ReadVcfBindData : public TableFunctionData {
  string filename;
  vector<pair<string, LogicalType>> info_columns;
  idx_t current_position;
  ReadVcfBindData() : current_position(0) {}
  // User options
  vector<string> requested_info_columns;
};

// Global state struct
struct VcfGlobalState : public GlobalTableFunctionState {
  explicit VcfGlobalState(ClientContext &context, string fname)
      : filename(fname) {
    // Open VCF file
    // fprintf(stderr, "Debug: Opening file %s\n", filename.c_str());
    vcf_file = bcf_open(filename.c_str(), "r");
    // fprintf(stderr, "Debug: vcf_file pointer: %p\n", (void *)vcf_file);
    if (!vcf_file) {
      throw IOException("Could not open VCF file: " + filename);
    }

    // Read header
    vcf_header = bcf_hdr_read(vcf_file);
    if (!vcf_header) {
      hts_close(vcf_file);
      throw IOException("Could not read VCF header: " + filename);
    }
  }

  ~VcfGlobalState() {
    if (vcf_header) {
      bcf_hdr_destroy(vcf_header);
    }
    if (vcf_file) {
      hts_close(vcf_file);
    }
  }

  idx_t MaxThreads() const override {
    return 1; // Single-threaded for now
  }

  string filename;
  htsFile *vcf_file;
  bcf_hdr_t *vcf_header;
};

// Local state for VCF reader
struct VcfLocalState : public LocalTableFunctionState {
  explicit VcfLocalState() { vcf_record = bcf_init(); }

  ~VcfLocalState() {
    if (vcf_record) {
      bcf_destroy(vcf_record);
    }
  }

  bcf1_t *vcf_record;
  bool done = false;
};

// Bind function for VCF reader
static unique_ptr<FunctionData> VcfReaderBind(ClientContext &context,
                                              TableFunctionBindInput &input,
                                              vector<LogicalType> &return_types,
                                              vector<string> &names) {
  auto result = make_uniq<ReadVcfBindData>();
  result->filename = input.inputs[0].GetValue<string>();

  // Basic VCF columns
  names.emplace_back("chrom");
  names.emplace_back("pos");
  names.emplace_back("id");
  names.emplace_back("ref");
  names.emplace_back("alt");
  names.emplace_back("qual");
  names.emplace_back("filter");

  return_types.emplace_back(LogicalType::VARCHAR);
  return_types.emplace_back(LogicalType::INTEGER);
  return_types.emplace_back(LogicalType::VARCHAR);
  return_types.emplace_back(LogicalType::VARCHAR);
  return_types.emplace_back(LogicalType::VARCHAR);
  return_types.emplace_back(LogicalType::DOUBLE);
  return_types.emplace_back(LogicalType::VARCHAR);

  // Handle INFO columns if specified
  if (input.named_parameters.count("info_cols")) {
    auto info_cols_str = input.named_parameters.at("info_cols").ToString();

    // Open VCF file to check header types
    htsFile *vcf_file = bcf_open(result->filename.c_str(), "r");
    if (!vcf_file) {
      throw IOException("Could not open VCF file: " + result->filename);
    }
    bcf_hdr_t *vcf_header = bcf_hdr_read(vcf_file);
    if (!vcf_header) {
      hts_close(vcf_file);
      throw IOException("Could not read VCF header: " + result->filename);
    }

    stringstream ss(info_cols_str);
    string token;
    while (getline(ss, token, ',')) {
      // Get the type from header
      int id = bcf_hdr_id2int(vcf_header, BCF_DT_ID, token.c_str());
      if (id < 0) {
        throw InvalidInputException("INFO field not found in header: " + token);
      }

      int type = bcf_hdr_id2type(vcf_header, BCF_HL_INFO, id);
      LogicalType duckdb_type;

      switch (type) {
      case BCF_HT_INT:
        duckdb_type = LogicalType::INTEGER;
        break;
      case BCF_HT_REAL:
        duckdb_type = LogicalType::DOUBLE;
        break;
      case BCF_HT_STR:
        duckdb_type = LogicalType::VARCHAR;
        break;
      case BCF_HT_FLAG:
        duckdb_type = LogicalType::BOOLEAN;
        break;
      default:
        throw InvalidInputException("Unsupported INFO field type for: " +
                                    token);
      }

      // Add "info_" prefix to column name
      names.push_back("info_" + token);
      // Add the correct type
      return_types.push_back(duckdb_type);
      // Store original name and type in info_columns
      result->info_columns.emplace_back(token, duckdb_type);
    }

    bcf_hdr_destroy(vcf_header);
    hts_close(vcf_file);
  }

  return result;
}

// Initialize global state
static unique_ptr<GlobalTableFunctionState>
VcfReaderInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
  auto &bind_data =
      const_cast<ReadVcfBindData &>(input.bind_data->Cast<ReadVcfBindData>());
  auto result = make_uniq<VcfGlobalState>(context, bind_data.filename);

  // Now process the requested INFO columns with the header we've already opened
  for (const auto &col : bind_data.requested_info_columns) {
    int id = bcf_hdr_id2int(result->vcf_header, BCF_DT_ID, col.c_str());
    if (id < 0) {
      throw InvalidInputException("INFO field not found in header: " + col);
    }

    int type = bcf_hdr_id2type(result->vcf_header, BCF_HL_INFO, id);
    LogicalType duckdb_type;

    switch (type) {
    case BCF_HT_INT:
      duckdb_type = LogicalType::INTEGER;
      break;
    case BCF_HT_REAL:
      duckdb_type = LogicalType::DOUBLE;
      break;
    case BCF_HT_STR:
      duckdb_type = LogicalType::VARCHAR;
      break;
    case BCF_HT_FLAG:
      duckdb_type = LogicalType::BOOLEAN;
      break;
    default:
      throw InvalidInputException("Unsupported INFO field type for: " + col);
    }

    bind_data.info_columns.emplace_back(col, duckdb_type);
  }

  return result;
}

// Initialize local state
static unique_ptr<LocalTableFunctionState>
VcfReaderInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
                   GlobalTableFunctionState *global_state) {
  return make_uniq<VcfLocalState>();
}

// Helper functions to handle VCF fields
class VcfFieldExtractor {
private:
  bcf1_t *vcf_record;
  bcf_hdr_t *vcf_header;

public:
  VcfFieldExtractor(bcf1_t *record, bcf_hdr_t *header)
      : vcf_record(record), vcf_header(header) {}

  // Extract chromosome name
  string get_chrom() const {
    auto chrom_name = bcf_hdr_id2name(vcf_header, vcf_record->rid);
    if (!chrom_name) {
      throw IOException("Invalid chromosome name in record");
    }
    return string(chrom_name);
  }

  // Extract position -- internally, we use 0-indexed
  // like htslib.
  int64_t get_pos() const { return vcf_record->pos; }

  // Extract ID
  std::optional<string> get_id() const {
    if (!vcf_record || !vcf_record->d.id) {
      return {};
    }
    if (vcf_record->d.id[0] == '.') {
      return {};
    }
    return string(vcf_record->d.id);
  }

  // Extract REF allele
  std::optional<string> get_ref() const {
    // Check if the REF allele exists and return it
    if (!vcf_record || !vcf_record->d.allele || !vcf_record->d.allele[0]) {
      return {};
    }
    return string(vcf_record->d.allele[0]);
  }

  // Extract ALT alleles
  std::optional<string> get_alt() const {
    if (!vcf_record || !vcf_record->d.allele) {
      return {};
    }

    string alt_str;
    if (vcf_record->n_allele > 1) {
      for (int i = 1; i < vcf_record->n_allele; i++) {
        if (!vcf_record->d.allele[i]) {
          continue;
        }
        if (i > 1) {
          alt_str += ",";
        }
        alt_str += vcf_record->d.allele[i];
      }
    }
    return alt_str.empty() ? std::optional<string>{}
                           : std::optional<string>{alt_str};
  }

  // Extract QUAL
  std::optional<double> get_qual() const {
    if (bcf_float_is_missing(vcf_record->qual)) {
      return {};
    }
    return vcf_record->qual;
  }

  // Extract FILTER
  string get_filter() const {
    if (vcf_record->d.n_flt == 0) {
      return "PASS";
    }

    string filter_str;
    for (int i = 0; i < vcf_record->d.n_flt; i++) {
      if (i > 0) {
        filter_str += ";";
      }
      filter_str += bcf_hdr_int2id(vcf_header, BCF_DT_ID, vcf_record->d.flt[i]);
    }
    return filter_str;
  }
};

// Helper to set values in output vectors
class VcfOutputSetter {
private:
  DataChunk &output;
  idx_t chunk_index;

public:
  VcfOutputSetter(DataChunk &out, idx_t index)
      : output(out), chunk_index(index) {}

  void set_value(size_t col_idx, const std::optional<string> &value) {
    output.data[col_idx].SetValue(
        chunk_index, value.has_value() ? Value(value.value()) : Value());
  }

  void set_value(size_t col_idx, const std::optional<double> &value) {
    output.data[col_idx].SetValue(
        chunk_index,
        value.has_value() ? Value::DOUBLE(value.value()) : Value());
  }

  void set_value(size_t col_idx, const string &value) {
    output.data[col_idx].SetValue(chunk_index, Value(value));
  }
};

// Modified VcfReaderFunction using helpers
static void VcfReaderFunction(ClientContext &context,
                              TableFunctionInput &data_p, DataChunk &output) {
  auto &bind_data = data_p.bind_data->Cast<ReadVcfBindData>();
  auto &global_state = data_p.global_state->Cast<VcfGlobalState>();
  auto &local_state = data_p.local_state->Cast<VcfLocalState>();

  if (local_state.done) {
    output.SetCardinality(0);
    return;
  }

  idx_t chunk_index = 0;
  while (chunk_index < STANDARD_VECTOR_SIZE) {
    int ret = bcf_read(global_state.vcf_file, global_state.vcf_header,
                       local_state.vcf_record);

    if (ret < 0) {
      local_state.done = true;
      break;
    }

    // Unpack up to FILTER (TODO make option?)
    bcf_unpack(local_state.vcf_record, BCF_UN_FLT);

    // Unpack INFO fields if we have any to extract
    if (!bind_data.info_columns.empty()) {
      bcf_unpack(local_state.vcf_record, BCF_UN_INFO);
    }

    auto extractor =
        VcfFieldExtractor(local_state.vcf_record, global_state.vcf_header);
    auto setter = VcfOutputSetter(output, chunk_index);

    // Set basic fields
    setter.set_value(0, extractor.get_chrom());
    setter.set_value(1, extractor.get_pos());
    setter.set_value(2, extractor.get_id());
    setter.set_value(3, extractor.get_ref());
    setter.set_value(4, extractor.get_alt());
    setter.set_value(5, extractor.get_qual());
    setter.set_value(6, extractor.get_filter());

    // Extract INFO fields
    for (size_t i = 0; i < bind_data.info_columns.size(); i++) {
      const auto &info_col = bind_data.info_columns[i];
      bcf_info_t *info =
          bcf_get_info(global_state.vcf_header, local_state.vcf_record,
                       info_col.first.c_str());

      if (!info) {
        // Field not present in this record
        output.data[7 + i].SetValue(chunk_index, Value());
        continue;
      }

      // Convert the value based on its type
      if (info->len <= 0) {
        output.data[7 + i].SetValue(chunk_index, Value());
        continue;
      }

      switch (info_col.second.id()) {
      case LogicalTypeId::INTEGER:
        output.data[7 + i].SetValue(chunk_index, Value::INTEGER(info->v1.i));
        break;
      case LogicalTypeId::DOUBLE:
        output.data[7 + i].SetValue(chunk_index, Value::DOUBLE(info->v1.f));
        break;
      case LogicalTypeId::VARCHAR:
        output.data[7 + i].SetValue(
            chunk_index, Value(string((char *)info->vptr, info->len)));
        break;
      case LogicalTypeId::BOOLEAN:
        output.data[7 + i].SetValue(chunk_index, Value::BOOLEAN(true));
        break;
      default:
        output.data[7 + i].SetValue(chunk_index, Value());
        break;
      }
    }

    chunk_index++;
  }

  output.SetCardinality(chunk_index);
}

void RegisterVcfReader(DatabaseInstance &db) {
  TableFunction vcf_reader("read_vcf", {LogicalType::VARCHAR},
                           VcfReaderFunction, VcfReaderBind,
                           VcfReaderInitGlobal, VcfReaderInitLocal);

  // Add this line to register the named parameter
  vcf_reader.named_parameters["info_cols"] = LogicalType::VARCHAR;

  ExtensionUtil::RegisterFunction(db, vcf_reader);
}

} // namespace duckdb
