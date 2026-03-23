# Vibe Coding with Claude — a field guide

This document reconstructs the session that produced `nuwro_comp` as a
concrete example of how to collaborate with an AI coding assistant effectively.

---

## What we built

A NuWro Monte-Carlo comparison tool that:

- reads two sets of ROOT files via `ROOT::RDataFrame`
- runs both event loops **in parallel** with `ROOT::RDF::RunGraphs`
- produces per-variable comparison canvases (overlay + ratio pad + χ²/ndf)
- is driven by a single variable list in `analysis.h` — add one entry, get it
  everywhere

---

## The session, step by step

### 1. Start concrete, not abstract

> *"write a small analysis program to read from sample/*.root …"*

The first prompt gave the AI full context upfront:
- file layout (`sample/*.root`, tree `treeout`, branch `e` of type `event`)
- where to find headers (`CMAKE_PREFIX_PATH`, `NuWro_DIR`)

**Lesson:** tell the AI what already exists before asking it to write code.
It will read the relevant headers itself and adapt to the actual API.

---

### 2. Let the compiler speak

The first draft had `q.norm2()` which doesn't exist in `vect`. The AI spotted
this immediately from the compiler error and replaced it with `q*q` (the
Minkowski scalar product operator). No back-and-forth needed.

**Lesson:** always build right after generation. Compiler errors are the
fastest feedback loop. Paste the error and the AI fixes it in one shot.

---

### 3. Iterate the design, not just the code

After the working manual-loop version the prompt was simply:

> *"rewrite using ROOT::RDataFrame"*

This triggered a full redesign:
- lazy graph instead of explicit loop
- `Histo1D` / `Count` / `Take` actions
- single-pass event loop

Then another prompt added the comparison program, and a further one
refactored shared logic into `analysis.h`. Each step was a *design* request,
not a bug fix.

**Lesson:** treat the AI as a pair programmer who can do large refactors
instantly. Don't be afraid to say "rewrite this using X" once you have a
working baseline.

---

### 4. RDataFrame-specific pitfalls we hit (and how they were resolved)

| Problem | Cause | Fix |
|---------|-------|-----|
| `cannot define column "dyn"` | `dyn` is already a scalar leaf auto-exposed by RDataFrame | Renamed the VarDef column to `dyn_ch`; `bookAnalysis` uses `HasColumn` to choose `Define` vs `Redefine` automatically |
| `particle::E()` not callable on `const event&` | NuWro particle methods lack `const` qualifier | `const_cast<event&>(e)` wrapper in every extractor lambda |
| `Aggregate` merge error | Wrong merger signature `void(U&, const U&)` — ROOT requires `U(U,U)` or `void(vector<U>&)` | Switched to `void(vector<map<int,long>>&)` which merges partial results in-place |
| Stats box appearing on plots | `gStyle->SetOptStat(0)` is not retroactive | Call `h->SetStats(false)` on each histogram directly in `drawComparison` |

---

### 5. The "one place to change" principle

The central insight of the final design:

```cpp
inline const std::vector<VarDef> kVars = {
    {"Enu",   {/*model*/}, [](event &e) { return e.in[0].E(); }},
    {"Q2",    {/*model*/}, [](event &e) { ... }},
    // add a line here → new histogram in every program, automatically
};
```

`bookAnalysis` uses `std::views::transform | std::ranges::to<std::vector>()`
to turn this list into booked `RResultPtr<TH1D>` objects. `compare.cpp` uses
`std::views::zip(s1.histos, s2.histos, kVars)` to iterate over pairs without
any manual indexing.

**Lesson:** when the AI proposes a data-driven design like this, accept it.
It pays off immediately — adding a variable later cost exactly one line.

---

### 6. Small requests, exact results

Several changes were single-sentence prompts:

- *"limit [ratio panel] to -0.9 to 1.1"* → two-line edit
- *"disable drawing of stat box"* → two-line edit
- *"remove analyze.cpp and related cmake entries, write readme/claude.md, git init, git commit"* → whole cleanup in one shot

**Lesson:** once the structure is stable, fine-tuning is cheap. Batch
housekeeping tasks (clean up + write docs + commit) into a single prompt.

---

### 7. CLAUDE.md pays for itself

We wrote `CLAUDE.md` containing:
- the exact `cmake` and run commands for this machine
- the architectural decisions and *why* they were made

In future sessions the AI reads this file automatically and arrives with full
context — no re-explaining the `const_cast` situation or the `dyn` leaf
conflict.

**Lesson:** write `CLAUDE.md` early. Record *decisions*, not just *facts*.
"We use `dyn_ch` because `dyn` is already a tree leaf" is more useful than
"column name is `dyn_ch`".

---

## Anti-patterns to avoid

**Over-specifying the implementation.** Saying "use a for-loop to iterate over
bins" instead of "aggregate the dyn channel counts" produces worse code. State
the *goal*; let the AI pick the mechanism.

**Ignoring build errors.** Every error in this session was fixed in ≤ 1 prompt.
If you accept broken code and move on, debt accumulates fast.

**Skipping the shared header.** The first working version had everything in one
file. Factoring out `analysis.h` was proposed by the AI and accepted. Resist
the urge to say "good enough" — the refactor took one exchange and the payoff
was immediate.

**Not committing checkpoints.** We committed once the design was stable. Having
a clean commit made it trivial to `git push` and share. Commit early, commit
named.
