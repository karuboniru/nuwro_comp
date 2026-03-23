#include <array>
#include <iostream>
#include <map>
#include <ranges>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include "ROOT/RDFHelpers.hxx"
#include "TCanvas.h"
#include "TFile.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLine.h"
#include "TPad.h"
#include "TStyle.h"

#include "analysis.h"

namespace po = boost::program_options;

// Draw two already-normalised histogram clones with a ratio pad.
// Returns chi2/ndf (Chi2TestX "WW" mode for weighted histograms).
double drawComparison(TH1D *h1, TH1D *h2, const std::string &label1,
                      const std::string &label2, const std::string &cname,
                      TFile &out) {
  if (h1->Integral() > 0)
    h1->Scale(1.0 / h1->Integral());
  if (h2->Integral() > 0)
    h2->Scale(1.0 / h2->Integral());

  double chi2val = 0.;
  Int_t ndf = 0, igood = 0;
  h1->Chi2TestX(h2, chi2val, ndf, igood, "WW");
  double chi2_ndf = (ndf > 0) ? chi2val / ndf : 0.;

  TCanvas c(cname.c_str(), h1->GetTitle(), 800, 700);

  TPad *pad_main = new TPad("pad_main", "", 0.0, 0.25, 1.0, 1.0);
  TPad *pad_ratio = new TPad("pad_ratio", "", 0.0, 0.00, 1.0, 0.25);
  pad_main->SetBottomMargin(0.02);
  pad_ratio->SetTopMargin(0.03);
  pad_ratio->SetBottomMargin(0.35);
  pad_main->Draw();
  pad_ratio->Draw();

  pad_main->cd();
  h1->SetStats(0);
  h2->SetStats(0);
  h1->SetLineColor(kBlue + 1);
  h1->SetLineWidth(2);
  h2->SetLineColor(kRed + 1);
  h2->SetLineWidth(2);
  h1->GetXaxis()->SetLabelSize(0);
  h1->GetXaxis()->SetTitleSize(0);
  h1->SetMaximum(std::max(h1->GetMaximum(), h2->GetMaximum()) * 1.15);
  h1->Draw("HIST");
  h2->Draw("HIST SAME");

  TLegend leg(0.62, 0.72, 0.88, 0.88);
  leg.SetBorderSize(0);
  leg.AddEntry(h1, label1.c_str(), "l");
  leg.AddEntry(h2, label2.c_str(), "l");
  leg.Draw();

  TLatex tex;
  tex.SetNDC();
  tex.SetTextSize(0.04);
  tex.DrawLatex(0.62, 0.65, Form("#chi^{2}/ndf = %.2f / %d", chi2val, ndf));

  pad_ratio->cd();
  TH1D *ratio = static_cast<TH1D *>(h2->Clone("ratio_tmp"));
  ratio->SetTitle("");
  ratio->Divide(h1);
  ratio->SetLineColor(kBlack);
  ratio->SetLineWidth(1);
  ratio->GetYaxis()->SetTitle((label2 + " / " + label1).c_str());
  ratio->GetYaxis()->SetNdivisions(505);
  ratio->GetYaxis()->SetTitleSize(0.13);
  ratio->GetYaxis()->SetTitleOffset(0.38);
  ratio->GetYaxis()->SetLabelSize(0.11);
  ratio->GetXaxis()->SetTitleSize(0.13);
  ratio->GetXaxis()->SetLabelSize(0.11);
  ratio->GetXaxis()->SetTitleOffset(0.9);
  ratio->SetMinimum(0.9);
  ratio->SetMaximum(1.1);
  ratio->Draw("HIST E");

  TLine line(ratio->GetXaxis()->GetXmin(), 1., ratio->GetXaxis()->GetXmax(),
             1.);
  line.SetLineStyle(2);
  line.SetLineColor(kGray + 1);
  line.Draw();

  out.cd();
  c.Write();
  delete ratio;
  return chi2_ndf;
}

int main(int argc, char *argv[]) {
  po::options_description desc("NuWro comparison tool");
  desc.add_options()("help,h", "show this help")(
      "set1", po::value<std::vector<std::string>>()->multitoken()->required(),
      "first set of .root files")(
      "set2", po::value<std::vector<std::string>>()->multitoken()->required(),
      "second set of .root files")(
      "label1", po::value<std::string>()->default_value("Set 1"),
      "legend label for set 1")(
      "label2", po::value<std::string>()->default_value("Set 2"),
      "legend label for set 2")(
      "output,o",
      po::value<std::string>()->default_value("comparison_out.root"),
      "output ROOT file");

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
    }
    po::notify(vm);
  } catch (const po::error &ex) {
    std::cerr << "Error: " << ex.what() << "\n" << desc << "\n";
    return 1;
  }

  const auto files1 = vm["set1"].as<std::vector<std::string>>();
  const auto files2 = vm["set2"].as<std::vector<std::string>>();
  const auto label1 = vm["label1"].as<std::string>();
  const auto label2 = vm["label2"].as<std::string>();
  const auto outname = vm["output"].as<std::string>();

  std::cout << "Set 1 (" << label1 << "): " << files1.size() << " file(s)\n";
  std::cout << "Set 2 (" << label2 << "): " << files2.size() << " file(s)\n";

  auto s1 = bookAnalysis(files1);
  auto s2 = bookAnalysis(files2);

  // Collect all result handles and run both event loops in parallel
  std::vector<ROOT::RDF::RResultHandle> handles;
  for (auto &h : s1.histos)
    handles.push_back(h);
  for (auto &h : s2.histos)
    handles.push_back(h);
  handles.push_back(s1.total);
  handles.push_back(s2.total);
  handles.push_back(s1.dyns);
  handles.push_back(s2.dyns);

  std::cout << "Running event loops in parallel...\n";
  ROOT::RDF::RunGraphs(handles);

  // Print per-set summaries
  constexpr std::array dyn_names = {"QEL-CC", "QEL-NC", "RES-CC", "RES-NC",
                                    "DIS-CC", "DIS-NC", "COH-CC", "COH-NC",
                                    "MEC-CC", "MEC-NC", "?10",    "?11",
                                    "LEP",    "?13",    "EEL"};
  for (auto &[set, label] : {std::pair{&s1, &label1}, {&s2, &label2}}) {
    std::cout << "\n=== " << *label << " ===\n";
    std::cout << "Total events: " << *set->total << "\n";
    std::map<int, long> cnt;
    for (int v : *set->dyns)
      cnt[v]++;
    for (auto &[dyn, n] : cnt) {
      const char *nm = (dyn >= 0 && dyn < (int)dyn_names.size())
                           ? dyn_names[dyn]
                           : "unknown";
      std::cout << "  dyn=" << dyn << " (" << nm << "): " << n << "\n";
    }
  }

  // Draw comparisons — zip over (s1 histos, s2 histos, variable definitions)
  TFile out(outname.c_str(), "RECREATE");
  gStyle->SetOptStat(0);

  std::cout << "\n=== Chi2 comparison ===\n";
  for (auto &&[h1_ptr, h2_ptr, vd] :
       std::views::zip(s1.histos, s2.histos, kVars)) {
    // auto *h1 = static_cast<TH1D *>(h1_ptr.GetPtr()->Clone());
    // auto *h2 = static_cast<TH1D *>(h2_ptr.GetPtr()->Clone());
    double r = drawComparison(h1_ptr.GetPtr(), h2_ptr.GetPtr(), label1, label2,
                              "cmp_" + vd.name, out);
    std::cout << "  " << vd.name << ": chi2/ndf = " << r << "\n";
  }

  out.Close();
  std::cout << "\nComparison plots saved to " << outname << "\n";
  return 0;
}
