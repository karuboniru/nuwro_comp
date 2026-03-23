#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "TFile.h"
#include "TH1D.h"
#include "TROOT.h"
#include "TSystem.h"
#include "TTree.h"

#include "event1.h"

int main(int argc, char *argv[]) {
  // Collect input files from arguments or glob sample/*.root
  std::vector<std::string> files;
  if (argc > 1) {
    for (int i = 1; i < argc; ++i)
      files.push_back(argv[i]);
  } else {
    // Default: all root files in sample/
    void *dirp = gSystem->OpenDirectory("sample");
    if (!dirp) {
      std::cerr << "Cannot open sample/ directory\n";
      return 1;
    }
    const char *entry;
    while ((entry = gSystem->GetDirEntry(dirp))) {
      std::string name(entry);
      if (name.size() > 5 && name.substr(name.size() - 5) == ".root")
        files.push_back("sample/" + name);
    }
    gSystem->FreeDirectory(dirp);
  }

  if (files.empty()) {
    std::cerr << "No .root files found\n";
    return 1;
  }

  // Histograms
  TH1D h_Enu("h_Enu", "Neutrino energy;E_{#nu} [MeV];Events", 100, 0, 5000);
  TH1D h_Q2("h_Q2", "Momentum transfer;Q^{2} [MeV^{2}];Events", 100, 0, 2e6);
  TH1D h_nout("h_nout", "Outgoing multiplicity;N_{out};Events", 20, 0, 20);
  TH1D h_dyn("h_dyn", "Interaction channel;dyn;Events", 15, -0.5, 14.5);
  TH1D h_muKE("h_muKE", "Leading muon KE;T_{#mu} [MeV];Events", 100, 0, 3000);

  std::map<int, long> dyn_count;
  long total = 0;

  event *e = nullptr;

  for (const auto &fname : files) {
    TFile f(fname.c_str(), "READ");
    if (f.IsZombie()) {
      std::cerr << "Cannot open " << fname << "\n";
      continue;
    }

    TTree *tree = nullptr;
    f.GetObject("treeout", tree);
    if (!tree) {
      std::cerr << "No tree 'treeout' in " << fname << "\n";
      continue;
    }

    tree->SetBranchAddress("e", &e);

    Long64_t nentries = tree->GetEntries();
    std::cout << fname << ": " << nentries << " events\n";

    for (Long64_t i = 0; i < nentries; ++i) {
      tree->GetEntry(i);

      // Neutrino energy (first incoming particle)
      if (!e->in.empty())
        h_Enu.Fill(e->in[0].E());

      // Q2 from incoming neutrino and outgoing lepton (CC events)
      if (e->flag.cc && e->in.size() >= 1 && e->out.size() >= 1) {
        vect q = e->in[0] - e->out[0];
        double Q2 = -(q * q); // Minkowski: Q2 = -q^2 > 0
        h_Q2.Fill(Q2);

        // Leading lepton (first outgoing particle)
        h_muKE.Fill(e->out[0].Ek());
      }

      h_nout.Fill((double)e->post.size());
      h_dyn.Fill(e->dyn);
      dyn_count[e->dyn]++;
      total++;
    }
    // tree owned by file, don't delete
  }

  std::cout << "\n=== Summary ===\n";
  std::cout << "Total events: " << total << "\n";
  std::cout << "Events by dyn channel:\n";
  const char *dyn_names[] = {"QEL-CC", "QEL-NC", "RES-CC", "RES-NC", "DIS-CC",
                             "DIS-NC", "COH-CC", "COH-NC", "MEC-CC", "MEC-NC",
                             "?10",    "?11",    "LEP",    "?13",    "EEL"};
  for (auto &[dyn, cnt] : dyn_count) {
    const char *name = (dyn >= 0 && dyn < 15) ? dyn_names[dyn] : "unknown";
    std::cout << "  dyn=" << dyn << " (" << name << "): " << cnt << "\n";
  }

  // Save histograms
  TFile out("analysis_out.root", "RECREATE");
  h_Enu.Write();
  h_Q2.Write();
  h_nout.Write();
  h_dyn.Write();
  h_muKE.Write();
  out.Close();
  std::cout << "\nHistograms saved to analysis_out.root\n";

  return 0;
}
