#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <algorithm>

#include <boost/filesystem.hpp>

#include <TFile.h>
#include <TCanvas.h>
#include <TH1.h>
#include <TAxis.h>

#include "catstr.hh"

using std::cout;
using std::cerr;
using std::endl;

template <typename T>
struct less_deref {
  constexpr bool operator()(const T *lhs, const T *rhs) const {
    return (*lhs) < (*rhs);
  }
};

class directory_range {
  boost::filesystem::path p;
  using iterator = boost::filesystem::directory_iterator;
public:
  directory_range(boost::filesystem::path p): p(p) { }
  inline iterator begin() const { return iterator(p); }
  inline iterator   end() const { return iterator( ); }
};
class sorted_directory_range {
  std::vector<boost::filesystem::path> v;
  using iterator = boost::filesystem::directory_iterator;
public:
  sorted_directory_range(boost::filesystem::path p)
  : v(iterator(p), iterator()) { std::sort(v.begin(), v.end()); }
  inline auto begin() const { return v.begin(); }
  inline auto   end() const { return v.end  (); }
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
  unsigned n = 0, found = 0;

  void operator()(TH1* h, unsigned wci) const {
    TAxis * const ax = h->GetXaxis();
    ax->SetLabelSize(0.055);
    const unsigned nbins = bins.size();
    for (unsigned i=0; i<nbins; ++i) {
      const unsigned bi = i+1;
      const bin& b = bins[i];
      h->SetBinContent(bi,b[wci]);
      ax->SetBinLabel(bi,cat('[',b.xlow,',',b.xhigh,')').c_str());
    }
  }
};

struct wc_container {
  std::string name;
  struct wc {
    std::string name, value;
    wc(std::string&& name, std::string&& value)
    : name(std::move(name)), value(std::move(value)) { }
  };
  std::vector<wc> wcs;
  wc_container(std::string&& name): name(std::move(name)), wcs() { }
  inline auto begin() const noexcept { return wcs.begin(); }
  inline auto   end() const noexcept { return wcs.  end(); }
  template <typename... Args>
  inline void emplace(Args&& ...args) {
    wcs.emplace_back(std::forward<Args>(args)...);
  }
};

template <typename T> struct bad_type;

int main(int argc, char* argv[]) {
  if (argc!=3) {
    cerr << "usage: " << argv[0] << " input_dir output.(pdf|root)" << endl;
    return 1;
  }
  const char* const out_ext = strrchr(argv[2],'.')+1;
  bool out_root = false;
  if (!(out_ext&&((out_root = !strcmp(out_ext,"root"))||!strcmp(out_ext,"pdf")))) {
    cout << out_ext << endl;
    cerr << "\033[31moutput file name \"" << argv[2]
         << "\" doesn't end with .root or .pdf\033[0m"
         << endl;
    return 1;
  }

  const std::string wc_file("param.dat"), yoda_file("Higgs-scaled.yoda");
  const std::vector<std::string> hists_names {
    "N_j_30",
    "pT_yy",
    "pT_j1_30",
    "m_jj_30",
    "Dphi_j_j_30",
    "Dphi_j_j_30_signed"
  };

  std::vector<wc_container> wcs; // Wilson coeff's values
  // values from yoda histograms
  std::map<const std::string*,hist,less_deref<std::string>> hists;
  for (const auto& name : hists_names) {
    hists.emplace(std::piecewise_construct,
                  std::forward_as_tuple(&name),std::tie());
  }
  unsigned nwcs = 0;

  for (const auto& dir : sorted_directory_range(argv[1])) {
    std::string dir_name = dir.filename().string();
    cout <<"\033[34;1m"<< dir_name <<"\033[0m"<< endl;
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

    const std::string path = dir.string()+'/';
    // ==============================================================
    ++nwcs;
    if (out_root) { // read Wilson coeffs
      wcs.emplace_back(std::move(dir_name));
      std::string name, value;
      std::ifstream f(path+wc_file);
      while (f >> name >> value)
        wcs.back().emplace(std::move(name),std::move(value));
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
          const auto it = hists.find(&name);
          if (it==hists.end()) continue;
          state_hist = true;
          h = &it->second; // hist pointer
        } else {
          if (front(16)=="END YODA_HISTO1D") {
            state_hist = false;
            state_bins = false;
            bin_i = 0;
            ++h->n;
            ++h->found;
            continue;
          } else if (state_bins) { // read histogram bin
            // ------------------------------------------------------

            double xlow, xhigh, sumw;
            std::stringstream(line) >> xlow >> xhigh >> sumw;

            hist::bin *b;
            if (h->n) {
              try {
                b = &h->bins.at(bin_i);
              } catch(...) {
                cerr << "\033[31mnbins mismatch in " << dir
                     << " : \"" << name << "\"\033[0m" << endl;
                return 1;
              }
              if (xlow!=b->xlow) {
                cerr << "\033[31mxlow mismatch in " << dir
                     << " : \"" << name << "\"\033[0m" << endl;
                return 1;
              }
              if (xhigh!=b->xhigh) {
                cerr << "\033[31mxhigh mismatch in " << dir
                     << " : \"" << name << "\"\033[0m" << endl;
                return 1;
              }
            } else {
              h->bins.emplace_back();
              b = &h->bins.back();
              b->xlow = xlow;
              b->xhigh = xhigh;
            }
            if (h->found>1) {
              cerr << "\033[31mmultiple histograms with name \"" << name
                   << "\" found in " << dir << "\033[0m" << endl;
              return 1;
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

    for (auto& h : hists) {
      if (!h.second.found) {
        cerr << "\033[31mNo histograme named \"" << h.first
             << "\" found in " << dir << "\033[0m" << endl;
        return 1;
      }
      h.second.found = 0;
    }
  } // end dir loop

  cout << "\nNum coeff variations: " << nwcs <<'\n'<< endl;

  // Draw ===========================================================

  if (out_root) { // ROOT output ------------------------------------
    TFile fout(argv[2],"recreate");

    for (const auto& name : hists_names) {
      const auto& yh = hists[&name];
      const unsigned nbins = yh.bins.size();

      for (unsigned ci=0; ci<nwcs; ++ci) {
        std::string title;
        for (const auto wc: wcs[ci]) title += wc.name + '=' + wc.value + ' ';
        TH1D *h = new TH1D(
          (name+':'+wcs[ci].name).c_str(),title.c_str(),nbins,0,1);
        h->SetXTitle(yh.title.c_str());
        yh(h,ci);
      }
    }

    fout.Write();
    cout << "Output file: " << fout.GetName() << endl;
  } else { // PDF output --------------------------------------------
    TCanvas canv;

    canv.SaveAs(cat(argv[2],'[').c_str());
    for (const auto& name : hists_names) {
      const auto& yh = hists[&name];
      const unsigned nbins = yh.bins.size();
      
      double ymin = 1e5, ymax = -1e5;
      for (unsigned ci=0; ci<nwcs; ++ci) {
        for (unsigned i=0; i<nbins; ++i) {
          const double y = yh[i][ci];
          if (y < ymin) ymin = y;
          if (y > ymax) ymax = y;
        }
      }
      const bool ymin_pos = ymin > 0;
      std::tie(ymin,ymax) = std::forward_as_tuple(
        1.05556*ymin - 0.05556*ymax,
        1.05556*ymax - 0.05556*ymin);
      if (ymin_pos && ymin < 0) ymin = 0;

      TH1D h(name.c_str(),yh.title.c_str(),nbins,0,1);
      h.SetStats(false);
      h.SetLineWidth(2);
      h.GetYaxis()->SetRangeUser(ymin,ymax);

      for (unsigned ci=0; ci<nwcs; ++ci) {
        yh(&h,ci);
        if (ci) h.DrawCopy("same");
        else h.DrawCopy();
      }
      canv.SaveAs(argv[2]);
    }
    canv.SaveAs(cat(argv[2],']').c_str());
  } // --------------------------------------------------------------

  return 0;
}
