#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#include <boost/filesystem.hpp>

#include <TCanvas.h>
#include <TH1.h>
#include <TAxis.h>

#include "catstr.hh"

using std::cout;
using std::cerr;
using std::endl;

struct directory_range {
  boost::filesystem::path p;
  directory_range(boost::filesystem::path p): p(p) { }
  using iterator = boost::filesystem::directory_iterator;
  inline iterator begin() const { return iterator(p); }
  inline iterator end()   const { return iterator( ); }
};

struct hist {
  std::string title;
  struct bin {
    double xlow, xhigh;
    std::vector<double> vals;
    inline double operator[](size_t i) const noexcept { return vals[i]; }
  };
  std::vector<bin> bins;
  inline const auto& operator[](size_t i) const noexcept { return bins[i]; }
  unsigned ncwvals = 0;
};

int main(int argc, char* argv[]) {
  if (argc!=2) {
    cout << "usage: " << argv[0] << " dir" << endl;
    return 1;
  }
  std::string wc_file("param.dat"), yoda_file("Higgs.yoda");

  std::vector<std::unordered_map<std::string,double>> wc; // Wilson coeff's values
  std::unordered_map<std::string,hist> hists { // values from yoda histograms
    {"nJet30",{}},
    {"pT_yy",{}},
    {"pT_j1",{}},
    {"m_jj",{}},
    {"absdphi_jj",{}},
    {"dphi_jj",{}}
  };
  
  for (const auto& dir : directory_range(argv[1])) {
    cout <<"\033[34;1m"<< dir.path().filename().native() <<"\033[0m"<< endl;
    bool wc_found = false, yoda_found = false;
    for (const auto& file : directory_range(dir)) {
      std::string name = file.path().filename().string();
      if (name==wc_file) { wc_found = true; }
      else if (name==yoda_file) { yoda_found = true; }
      if (wc_found && yoda_found) break;
    }
    if (!wc_found) {
      cerr << "\033[31mNo " <<   wc_file << " in " << dir << "\033[0m" << endl;
      continue;
    }
    if (!yoda_found) {
      cerr << "\033[31mNo " << yoda_file << " in " << dir << "\033[0m" << endl;
      continue;
    }

    const std::string path = dir.path().string()+'/';
    // ==============================================================
    { // read Wilson coeffs
      wc.emplace_back();
      std::string name;
      double value;
      std::ifstream f(path+wc_file);
      while (f >> name >> value) { wc.back().emplace(std::move(name),value); }
    }
    { // read yoda file
      std::string line, name;
      auto front = [&line](size_t n){ return line.substr(0,n); };
      auto back  = [&line](size_t n){ return line.substr(n); };
      bool state_hist = false, state_bins = false;
      size_t bin_i = 0;
      hist *h = nullptr;
      std::ifstream f(path+yoda_file);
      while (std::getline(f,line)) {
        if (!state_hist) {
          if (front(18)!="BEGIN YODA_HISTO1D") continue;
          name = line.substr(line.rfind('/')+1);
          const auto it = hists.find(name);
          if (it==hists.end()) continue;
          state_hist = true;
          h = &it->second; // hist pointer
        } else {
          if (front(16)=="END YODA_HISTO1D") {
            state_hist = false;
            state_bins = false;
            bin_i = 0;
            ++h->ncwvals;
            continue;
          } else if (state_bins) { // read histogram bin
            // ------------------------------------------------------

            double xlow, xhigh, sumw;
            std::stringstream(line) >> xlow >> xhigh >> sumw;

            hist::bin *b;
            if (h->ncwvals) {
              try {
                b = &h->bins.at(bin_i);
              } catch(...) {
                cerr << "\033[31mnbins mismatch in " << dir
                     << " : " << name << "\033[0m" << endl;
                return 1;
              }
              if (xlow!=b->xlow) {
                cerr << "\033[31mxlow mismatch in " << dir
                     << " : " << name << "\033[0m" << endl;
                return 1;
              }
              if (xhigh!=b->xhigh) {
                cerr << "\033[31mxhigh mismatch in " << dir
                     << " : " << name << "\033[0m" << endl;
                return 1;
              }
            } else {
              h->bins.emplace_back();
              b = &h->bins.back();
              b->xlow = xlow;
              b->xhigh = xhigh;
            }
            b->vals.push_back(sumw);
            ++bin_i;

            // ------------------------------------------------------
          } else if (front(6)=="Title=") {
            h->title = back(6);
          } else if (front(6)=="# xlow") {
            state_bins = true;
          }
        }
      } // end while
    }
    // ==============================================================

    // for (const auto& h : hists) { // TEST
    //   cout << "  " << h.first
    //        << ' ' << h.second.title
    //        << ' ' << h.second.bins.size() << endl;
    // }
  } // end dir loop

  // make sure every histogram was seen the same number of times
  unsigned ncwvals = 0;
  for (const auto& h : hists) {
    static bool first = true;
    if (first) ncwvals = h.second.ncwvals;
    else if (ncwvals != h.second.ncwvals) {
      cerr <<"\033[31m"<< h.first << " filled a different number of times: "
           << h.second.ncwvals << " instead of " << ncwvals << endl;
      return 1;
    }
    first = false;
  }
  cout << "\nNum coeff variations: " << ncwvals <<'\n'<< endl;

  // Draw ===========================================================

  TCanvas canv;

  canv.SaveAs("plot.pdf[");
  for (const auto& yh : hists) {
    const unsigned nbins = yh.second.bins.size();
    
    double ymin = 1e5, ymax = -1e5;
    for (unsigned j=0; j<ncwvals; ++j) {
      for (unsigned i=0; i<nbins; ++i) {
        const double y = yh.second[i][j];
        if (y < ymin) ymin = y;
        if (y > ymax) ymax = y;
      }
    }
    std::tie(ymin,ymax) = std::forward_as_tuple(
      1.05556*ymin - 0.05556*ymax,
      1.05556*ymax - 0.05556*ymin);

    TH1D h(yh.first.c_str(),yh.second.title.c_str(),nbins,0,1);
    h.SetStats(false);
    h.SetLineWidth(2);
    // h.SetLineColor(46);
    h.GetYaxis()->SetRangeUser(ymin,ymax);

    for (unsigned j=0; j<ncwvals; ++j) {
      for (unsigned i=0; i<nbins; ++i)
        h.SetBinContent(i+1,yh.second[i][j]);
      if (j) h.DrawCopy("same");
      else {
        h.DrawCopy();
      }
    }
    canv.SaveAs("plot.pdf");
  }
  canv.SaveAs("plot.pdf]");

  return 0;
}
