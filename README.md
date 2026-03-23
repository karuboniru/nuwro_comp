# nuwro-compare

Side-by-side comparison of two sets of NuWro output ROOT files.

## What it does

`compare` reads two sets of `treeout` trees (NuWro output), computes a
configurable list of kinematic variables via `ROOT::RDataFrame`, runs both
event loops in parallel with `ROOT::RDF::RunGraphs`, and writes a ROOT file
containing one canvas per variable with:

- overlaid unit-area histograms for each set
- χ²/ndf printed on the plot (Chi2TestX WW mode)
- a ratio pad (set2 / set1)

## Dependencies

| Library | Version |
|---------|---------|
| NuWro   | from `nuwro_mh_working_tree` install |
| ROOT    | ≥ 6.22 (RDataFrame, RunGraphs) |
| Boost   | ≥ 1.70 (program_options) |
| C++     | 23 |

## Build

```sh
cmake -B build \
  -DCMAKE_PREFIX_PATH=/path/to/nuwro/install \
  -DNuWro_DIR=/path/to/nuwro/install/lib/cmake/NuWro
cmake --build build
```

Or source the NuWro setup script first:

```sh
source /path/to/nuwro/install/setup.sh
cmake -B build && cmake --build build
```

## Usage

```sh
./build/compare \
  --set1 run_a/*.root \
  --set2 run_b/*.root \
  --label1 "Config A" \
  --label2 "Config B" \
  --output comparison_out.root   # default
```

## Adding / removing variables

Edit `kVars` in `analysis.h`. Each entry is a `VarDef`:

```cpp
{"column_name",
 {"hist_name", "Title;x axis;y axis", nbins, xlo, xhi},
 [](event &e) -> double { return /* scalar from event */; }},
```

Values outside `[xlo, xhi]` are silently ignored (no explicit filter needed).
The column name must not collide with an existing tree leaf; if it does,
`bookAnalysis` will call `Redefine` instead of `Define` automatically.
