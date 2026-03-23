#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "ROOT/RDataFrame.hxx"
#include "TFile.h"
#include "TSystem.h"

#include "event1.h"

int main(int argc, char *argv[]) {
  std::vector<std::string> files;
  if (argc > 1) {
    for (int i = 1; i < argc; ++i)
      files.push_back(argv[i]);
  } else {
    void *dirp = gSystem->OpenDirectory("sample");
    if (!dirp) { std::cerr << "Cannot open sample/ directory\n"; return 1; }
    const char *entry;
    while ((entry = gSystem->GetDirEntry(dirp))) {
      std::string name(entry);
      if (name.size() > 5 && name.substr(name.size() - 5) == ".root")
        files.push_back("sample/" + name);
    }
    gSystem->FreeDirectory(dirp);
  }

  if (files.empty()) { std::cerr << "No .root files found\n"; return 1; }

  ROOT::RDataFrame df("treeout", files);

  // particle::E()/Ek() are not const-qualified; cast away const for read-only access
  auto nc = [](const event &e) -> event & { return const_cast<event &>(e); };

  // "dyn" is already a scalar leaf exposed by RDataFrame; define only computed columns
  auto d = df
    .Define("Enu", [&nc](const event &e) {
      return e.in.empty() ? -1.0 : nc(e).in[0].E();
    }, {"e"})
    .Define("npost", [](const event &e) { return (int)e.post.size(); }, {"e"})
    .Define("Q2", [&nc](const event &e) -> double {
      if (!e.flag.cc || e.in.empty() || e.out.empty()) return -1.0;
      vect q = nc(e).in[0] - nc(e).out[0];
      return -(q * q); // Minkowski: Q2 = -q^2 > 0
    }, {"e"})
    .Define("muKE", [&nc](const event &e) -> double {
      if (!e.flag.cc || e.out.empty()) return -1.0;
      return nc(e).out[0].Ek();
    }, {"e"});

  // Lazy histogram booking (single-pass evaluation)
  auto h_Enu  = d.Filter("Enu  >= 0").Histo1D({"h_Enu",  "Neutrino energy;E_{#nu} [MeV];Events",        100, 0, 5000}, "Enu");
  auto h_Q2   = d.Filter("Q2   >= 0").Histo1D({"h_Q2",   "Momentum transfer;Q^{2} [MeV^{2}];Events",   100, 0, 2e6},  "Q2");
  auto h_muKE = d.Filter("muKE >= 0").Histo1D({"h_muKE", "Leading lepton KE;T_{#mu} [MeV];Events",     100, 0, 3000}, "muKE");
  auto h_nout = d.Histo1D({"h_nout", "Post-FSI multiplicity;N_{out};Events", 20, 0, 20}, "npost");
  auto h_dyn  = d.Histo1D({"h_dyn",  "Interaction channel;dyn;Events",       15, -0.5, 14.5}, "dyn");
  auto total  = d.Count();
  auto dyns   = d.Take<int>("dyn");

  // --- trigger event loop ---

  std::cout << "\n=== Summary ===\n";
  std::cout << "Total events: " << *total << "\n";

  std::map<int, long> dyn_count;
  for (int v : *dyns) dyn_count[v]++;

  const char *dyn_names[] = {"QEL-CC", "QEL-NC", "RES-CC", "RES-NC", "DIS-CC",
                             "DIS-NC", "COH-CC", "COH-NC", "MEC-CC", "MEC-NC",
                             "?10",    "?11",    "LEP",    "?13",    "EEL"};
  std::cout << "Events by dyn channel:\n";
  for (auto &[dyn, cnt] : dyn_count) {
    const char *name = (dyn >= 0 && dyn < 15) ? dyn_names[dyn] : "unknown";
    std::cout << "  dyn=" << dyn << " (" << name << "): " << cnt << "\n";
  }

  TFile out("analysis_out.root", "RECREATE");
  h_Enu ->Write();
  h_Q2  ->Write();
  h_muKE->Write();
  h_nout->Write();
  h_dyn ->Write();
  out.Close();
  std::cout << "\nHistograms saved to analysis_out.root\n";

  return 0;
}
