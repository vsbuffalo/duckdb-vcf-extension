# DuckDB VCF Extension

This is an **experimental** and **under active development** VCF extension for
duckdb, that uses the incredible [htslib](https://github.com/samtools/htslib).
Please drop me an email if you'd like to contribute, I'd love help!

Here's are a few examples, using the [CADD](https://cadd.gs.washington.edu)
`chr22.vcf.bgz` file. Current parsing the sample columns is not implemented
(want to add it? reach out!).

```sql
SELECT chrom, pos, ref, alt info_RS, info_PH
  FROM read_vcf('test_data/chr22.vcf.bgz', info_cols => 'RS,PH')
  LIMIT 5;
┌─────────┬──────────┬─────────┬─────────┬────────────────────┐
│  chrom  │   pos    │   ref   │ info_RS │      info_PH       │
│ varchar │  int32   │ varchar │ varchar │       double       │
├─────────┼──────────┼─────────┼─────────┼────────────────────┤
│ 22      │ 10510000 │ G       │ A       │  9.838000297546387 │
│ 22      │ 10510000 │ G       │ C       │   9.46500015258789 │
│ 22      │ 10510000 │ G       │ T       │  9.343000411987305 │
│ 22      │ 10510001 │ A       │ C       │ 10.770000457763672 │
│ 22      │ 10510001 │ A       │ G       │ 11.119999885559082 │
└─────────┴──────────┴─────────┴─────────┴────────────────────┘
-- Run Time (s): real 0.008 user 0.007143 sys 0.000640
```

```sql
CREATE TABLE cadd_mb AS
  SELECT
      chrom,
      CAST(pos/1e6 AS int) AS window_mb,
      AVG(info_RS) AS avg_rs
  FROM
      read_vcf('test_data/chr22.vcf.bgz',
               info_cols => 'RS,PH')
  GROUP BY
      chrom,
      CAST(pos/1e6 AS int)
  ORDER BY
      avg_rs;
-- Run Time (s): real 140.748 user 274.940815 sys 1.131420
SELECT * FROM cadd_mb LIMIT 5;
┌─────────┬───────────┬─────────────────────┐
│  chrom  │ window_mb │       avg_rs        │
│ varchar │   int32   │       double        │
├─────────┼───────────┼─────────────────────┤
│ 22      │        48 │ 0.11212997754014824 │
│ 22      │        49 │ 0.11807465515453384 │
│ 22      │        47 │ 0.12112692959040292 │
│ 22      │        34 │ 0.14504379306428725 │
│ 22      │        44 │ 0.15174636398923944 │
└─────────┴───────────┴─────────────────────┘
-- Run Time (s): real 0.008 user 0.000513 sys 0.002302
```

If you'd like to try the extension, clone this repository as below, follow the
compilation directions, and run the compiled duckdb version.

## Development

Below are some tips to help other contribute to this extension.

### Cloning the Repository

This needs to be cloned with all the various submodules needed when writing
duckdb extensions, as well as htslib.

```
git clone --recurse-submodules https://github.com/vsbuffalo/duckdb-vcf-extension.git
```

To update these submodules, use `git submodule update --init --recursive`.

### Compiling the Extension


```
$ bash compile_htslib.sh
```

Now build the extension,

```
$ make clean
$ make
```

Note that htslib needs automake and a few other requirements, which can be
installed with e.g. Homebrew with

```
$ brew install automake
$ brew install bzip2 xz curl
```

The main binaries that will be built are:

```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/vcf/vcf.duckdb_extension
```

- `duckdb` is the binary for the duckdb shell with the extension code automatically loaded.
- `unittest` is the test runner of duckdb. Again, the extension is already linked into the binary.
- `vcf.duckdb_extension` is the loadable binary as it would be distributed.

### Running the extension

To run the extension code, simply start the shell with
`./build/release/duckdb`.


```sql
SELECT chrom, pos, ref, alt info_RS, info_PH
  FROM read_vcf('test_data/chr22.vcf.bgz', info_cols => 'RS,PH')
  LIMIT 5;
┌─────────┬──────────┬─────────┬─────────┬────────────────────┐
│  chrom  │   pos    │   ref   │ info_RS │      info_PH       │
│ varchar │  int32   │ varchar │ varchar │       double       │
├─────────┼──────────┼─────────┼─────────┼────────────────────┤
│ 22      │ 10510000 │ G       │ A       │  9.838000297546387 │
│ 22      │ 10510000 │ G       │ C       │   9.46500015258789 │
│ 22      │ 10510000 │ G       │ T       │  9.343000411987305 │
│ 22      │ 10510001 │ A       │ C       │ 10.770000457763672 │
│ 22      │ 10510001 │ A       │ G       │ 11.119999885559082 │
└─────────┴──────────┴─────────┴─────────┴────────────────────┘
```

### Running the tests

DuckDB has a neat way to test extensions, through SQL Logic Tests (see [DuckDB
doc](https://duckdb.org/docs/dev/sqllogictest/intro.html) and [SQLite
doc](https://www.sqlite.org/sqllogictest/doc/trunk/about.wiki)).

The core tests are in `test/sql/vcf.test`; these can be run with,


```sh
make test
```

