#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <boost/filesystem.hpp>

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
};

struct wc_point {
  static std::unordered_set<std::string> hists_names;
  std::unordered_map<std::string,double> wc; // Wilson coeff's values
  std::unordered_map<std::string,hist> hists;
  wc_point(const std::string& wc_file, const std::string& yoda_file) {
    { // read Wilson coeffs
      std::string name;
      double value;
      std::ifstream f(wc_file);
      while (f >> name >> value) { wc.emplace(std::move(name),value); }
    }
    { // read yoda file
      std::string line, name;
      auto front = [&line](size_t n){ return line.substr(0,n); };
      auto back  = [&line](size_t n){ return line.substr(n); };
      std::ifstream f(yoda_file);
      bool inside = false;
      hist *h = nullptr;
      while (std::getline(f,line)) {
        if (!inside) {
          if (front(18)!="BEGIN YODA_HISTO1D") continue;
          name = line.substr(line.rfind('/')+1);
          if (!hists_names.count(name)) continue;
          inside = true;
          h = &hists.emplace(std::piecewise_construct,
              std::forward_as_tuple(std::move(name)),
              std::tie()
            ).first->second;
        } else {
          if (front(16)=="END YODA_HISTO1D") {
            inside = false;
            continue;
          } else if (front(6)=="Title=") {
            h->title = back(6);
          }
        }
      } // end while
    }
  } // end constructor
};
std::unordered_set<std::string> wc_point::hists_names {
  "pT_yy"
};

// template <class... T> struct type_test;

int main(int argc, char* argv[]) {
  if (argc!=2) {
    cout << "usage: " << argv[0] << " dir" << endl;
    return 1;
  }
  std::string wc_file("param.dat"), yoda_file("Higgs.yoda");

  std::vector<wc_point> points;
  
  for (const auto& dir : directory_range(argv[1])) {
    cout <<"\033[34;1m"<< dir.path().filename().native() <<"\033[0m"<< endl;
    bool wc_found = false, yoda_found = false;
    for (const auto& file : directory_range(dir)) {
      std::string name = file.path().filename().string();
      if (name==wc_file) { wc_found = true; }
      else if (name==yoda_file) { yoda_found = true; }
      if (wc_found && yoda_found) break;
    }
    if (!wc_found)
      cerr << "\033[31mNo param.dat in " << dir << "\033[0m" << endl;
    if (!yoda_found)
      cerr << "\033[31mNo Higgs.yoda in " << dir << "\033[0m" << endl;
    if (!(wc_found || yoda_found)) continue;

    std::string path = dir.path().string()+'/';
    points.emplace_back(path+wc_file,path+yoda_file);

    for (const auto& h : points.back().hists) { // TEST
      cout << "  " << h.first << ' '
           << h.second.title << endl;
    }
  }

  return 0;
}
