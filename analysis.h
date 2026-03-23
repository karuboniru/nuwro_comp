#pragma once
#include <functional>
#include <map>
#include <ranges>
#include <string>
#include <vector>

#include "ROOT/RDataFrame.hxx"
#include "TH1D.h"

#include "event1.h"

// particle::E(), Ek() etc. are not const-qualified, so extractors take event& not const event&
struct VarDef {
    std::string              name;   ///< RDataFrame column name (must not clash with tree leaves)
    ROOT::RDF::TH1DModel     model;  ///< histogram title/binning
    std::function<double(event &)> extract;
};

// The central variable list — add / remove entries here to change every program
inline const std::vector<VarDef> kVars = {
    {"Enu",
     {"h_Enu", "Neutrino energy;E_{#nu} [MeV];1/N dN/dE", 100, 0., 5000.},
     [](event &e) -> double {
         return e.in.empty() ? -999. : e.in[0].E();
     }},
    {"Q2",
     {"h_Q2", "4-mom. transfer;Q^{2} [MeV^{2}];1/N dN/dQ^{2}", 100, 0., 2.e6},
     [](event &e) -> double {
         if (!e.flag.cc || e.in.empty() || e.out.empty()) return -999.;
         vect q = e.in[0] - e.out[0];
         return -(q * q);  // Q^2 = -q^2 > 0
     }},
    {"muKE",
     {"h_muKE", "Leading lepton KE;T_{#mu} [MeV];1/N dN/dT", 100, 0., 3000.},
     [](event &e) -> double {
         if (!e.flag.cc || e.out.empty()) return -999.;
         return e.out[0].Ek();
     }},
    {"npost",
     {"h_nout", "Post-FSI multiplicity;N_{out};1/N dN/dN_{out}", 20, 0., 20.},
     [](event &e) -> double { return static_cast<double>(e.post.size()); }},
    // Note: "dyn" is already a scalar leaf in the tree; use a different name here
    //       and keep the original leaf accessible for the dyn-channel summary.
    {"dyn_ch",
     {"h_dyn", "Interaction channel;dyn;1/N dN/ddyn", 15, -0.5, 14.5},
     [](event &e) -> double { return static_cast<double>(e.dyn); }},
};

struct HistSet {
    std::vector<ROOT::RDF::RResultPtr<TH1D>>    histos;     ///< one entry per VarDef
    ROOT::RDF::RResultPtr<ULong64_t>             total;
    ROOT::RDF::RResultPtr<std::map<int, long>>   dyn_counts; ///< aggregated dyn-channel counts
};

// Build a lazy computation graph over `files` for each variable in `vars`.
// No event loop runs until a result is accessed or RunGraphs() is called.
inline HistSet bookAnalysis(const std::vector<std::string> &files,
                            const std::vector<VarDef>      &vars = kVars) {
    ROOT::RDataFrame df("treeout", files);
    ROOT::RDF::RNode d{df};

    // Chain Define/Redefine for each variable
    for (const auto &v : vars) {
        // Wrap extractor: RDataFrame passes const event&, but extract needs event&
        auto fn = [f = v.extract](const event &e) {
            return f(const_cast<event &>(e));
        };
        d = d.HasColumn(v.name)
                ? d.Redefine(v.name, fn, {"e"})
                : d.Define(  v.name, fn, {"e"});
    }

    // Build the histogram list in one expression; values outside [xlo,xhi] are
    // silently ignored by Histo1D — no explicit Filter needed
    auto histos = vars
        | std::views::transform([&d](const VarDef &v) { return d.Histo1D(v.model, v.name); })
        | std::ranges::to<std::vector>();

    auto dyn_counts = d.Aggregate(
        [](std::map<int, long> &m, int dyn) { m[dyn]++; },
        [](std::vector<std::map<int, long>> &maps) {
            for (std::size_t i = 1; i < maps.size(); ++i)
                for (auto &[k, v] : maps[i])
                    maps[0][k] += v;
        },
        "dyn",
        std::map<int, long>{});

    return {std::move(histos), d.Count(), std::move(dyn_counts)};
}
