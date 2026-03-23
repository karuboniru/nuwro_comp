# Project notes for Claude

## Build

```sh
cmake -B build \
  -DCMAKE_PREFIX_PATH=/var/home/yan/code/nuwro_mh_working_tree/install \
  -DNuWro_DIR=/var/home/yan/code/nuwro_mh_working_tree/install/lib/cmake/NuWro
cmake --build build
```

Run:

```sh
LD_LIBRARY_PATH=/var/home/yan/code/nuwro_mh_working_tree/install/lib:$LD_LIBRARY_PATH \
  ./build/compare --set1 sample/test_mh.root --set2 sample/test_rej.root \
                  --label1 MH --label2 Rej
```

Sample files live in `sample/` (not committed).

## Architecture

- `analysis.h` — shared header: `VarDef`, `kVars`, `HistSet`, `bookAnalysis()`
- `compare.cpp` — CLI tool (Boost program_options); calls `bookAnalysis` for
  each set, fires both loops with `RunGraphs`, draws with `drawComparison`
- `CMakeLists.txt` — single target `compare`; requires C++23

## Key design decisions

- **No filters**: `Histo1D` silently drops out-of-range values; extractors
  return `-999.` as sentinel for inapplicable events (e.g. Q² for NC).
- **`const_cast` in extractors**: NuWro `particle::E()` / `Ek()` are not
  `const`-qualified. RDataFrame passes `const event&`; the wrapper in
  `bookAnalysis` casts away const for read-only access.
- **`dyn` leaf conflict**: the tree already exposes `dyn` as a scalar leaf.
  The VarDef entry uses `dyn_ch` as its column name; `bookAnalysis` uses
  `HasColumn` to pick `Define` vs `Redefine` automatically.
- **Parallel loops**: `ROOT::RDF::RunGraphs` runs the two independent
  computation graphs concurrently; results are accessed only after it returns.
