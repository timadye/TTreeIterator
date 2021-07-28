#include <string>
#include <vector>
#include <cassert>
#include <cmath>
#include "TFile.h"
#include "TString.h"
#include "TTree.h"
#include "TTreeIterator/TTreeIterator.h"


#ifndef NFILL
#define NFILL 5
#endif
#ifndef NX
#define NX 2
#endif
#ifndef VERBOSE
#define VERBOSE 0
#endif

//#define FAST_CHECKS 1

const Long64_t nfill1 = NFILL;
const Long64_t nfill2 = NFILL;
const Long64_t nfill3 = NFILL;
constexpr size_t nx1 = NX;
constexpr size_t nx2 = NX;
constexpr size_t nx3 = NX;
const double vinit = 42.3;  // fill each element with a different value starting from here
const int verbose = VERBOSE;

// A simple user-defined POD class
struct MyStruct {
  double x[nx2];
};
template<> const char* TTreeIterator::GetLeaflist<MyStruct>() { return Form("x[%d]/D",int(nx2)); }

#ifndef NO_TEST1
#ifndef NO_FILL
void FillIter1() {
  TFile file ("test_timing1.root", "recreate");
  assert (!file.IsZombie());
  std::vector<std::string> bnames;
  bnames.reserve(nx1);
  for (size_t i=0; i<nx1; i++) bnames.emplace_back (Form("x%03zu",i));
  TTreeIterator iter ("test", verbose);
  double v = vinit;
  for (auto& entry : iter.FillEntries(nfill1)) {
    for (auto& b : bnames) entry[b.c_str()] = v++;
    entry.Fill();
  }
  assert (vinit+double(nx1*nfill1) == v);
}
#endif

#ifndef NO_GET
void GetIter1() {
  TFile file ("test_timing1.root");
  assert (!file.IsZombie());
  std::vector<std::string> bnames;
  bnames.reserve(nx1);
  for (size_t i=0; i<nx1; i++) bnames.emplace_back (Form("x%03zu",i));
  TTreeIterator iter ("test", &file, verbose);
  assert (iter.GetTree());
  assert (iter.GetEntries() == nfill1);
  double v = vinit, vsum=0.0;
  for (auto& entry : iter) {
    for (auto& b : bnames) {
      double x = entry[b.c_str()];
      vsum += x;
#ifndef FAST_CHECKS
      assert (x == v++);
#endif
    }
  }
  double vn = double(nx1*nfill1);
  assert (std::abs (1.0 - (0.5*vn*(vn+2*vinit-1) / vsum)) < 1e-6);
}
#endif
#endif


#ifndef NO_TEST2
#ifndef NO_FILL
void FillIter2() {
  static_assert (sizeof(MyStruct::x)/sizeof(MyStruct::x[0]) == nx2, "MyStruct::x wrong size");
  TFile file ("test_timing2.root", "recreate");
  assert (!file.IsZombie());

  TTreeIterator iter ("test", verbose);
  double v = vinit;
  for (auto& entry : iter.FillEntries(nfill2)) {
    MyStruct M;
    for (auto& x : M.x) x = v++;
    const MyStruct& set = entry.Set("M", std::move(M));
    //    MyStruct& set = entry["M"] = M;
#ifndef FAST_CHECKS
    if (verbose >= 2 && nx2 >= 2) iter.Info ("FillIter2", "M=(%g,%g) @%p", set.x[0], set.x[1], &set);
#endif
    entry.Fill();
  }
  assert (vinit+double(nfill2*nx2) == v);
}
#endif

#ifndef NO_GET
void GetIter2() {
  static_assert (sizeof(MyStruct::x)/sizeof(MyStruct::x[0]) == nx2, "MyStruct::x wrong size");
  TFile file ("test_timing2.root");
  assert (!file.IsZombie());

  TTreeIterator iter ("test", &file, verbose);
  assert (iter.GetTree());
  assert (iter.GetEntries() == nfill2);
  double v = vinit, vsum=0.0;
  for (auto& entry : iter) {
    const MyStruct& M = entry["M"];
    for (auto& x : M.x) {
      vsum += x;
#ifndef FAST_CHECKS
      assert (x == v++);
#endif
    }
  }
  double vn = double(nfill2*nx2);
  assert (std::abs (1.0 - 0.5*vn*(vn+2*vinit-1) / vsum) < 1e-6);
}
#endif
#endif

#ifndef NO_TEST3
#ifndef NO_FILL
void FillIter3() {
  TFile file ("test_timing3.root", "recreate");
  assert (!file.IsZombie());

  TTreeIterator iter ("test", verbose);
  double v = vinit;
  for (auto& entry : iter.FillEntries(nfill3)) {
    std::vector<double> vx(nx3);
    for (size_t i=0; i<nx3; i++) vx[i] = v++;
    //    entry["vx"] = std::move(vx);
    entry.Set ("vx", std::move(vx));
    entry.Fill();
  }
  assert (vinit+double(nfill3*nx3) == v);
}
#endif

#ifndef NO_GET
void GetIter3() {
  TFile file ("test_timing3.root");
  assert (!file.IsZombie());

  TTreeIterator iter ("test", &file, verbose);
  assert (iter.GetTree());
  assert (iter.GetEntries() == nfill3);
  double v = vinit, vsum=0.0;
  for (auto& entry : iter) {
    const std::vector<double>& vx = entry["vx"];
    assert (vx.size() == nx3);
    for (auto& x : vx) {
      vsum += x;
#ifndef FAST_CHECKS
      assert (x == v++);
#endif
    }
  }
  double vn = double(nfill3*nx3);
  assert (std::abs (1.0 - (0.5*vn*(vn+2*vinit-1) / vsum)) < 1e-6);
}
#endif
#endif


int main() {
#ifndef NO_TEST1
#ifndef NO_FILL
  FillIter1();
#endif
#ifndef NO_GET
  GetIter1();
#endif
#endif
#ifndef NO_TEST2
#ifndef NO_FILL
  FillIter2();
#endif
#ifndef NO_GET
  GetIter2();
#endif
#endif
#ifndef NO_TEST3
#ifndef NO_FILL
  FillIter3();
#endif
#ifndef NO_GET
  GetIter3();
#endif
#endif
  gDirectory->ls();
}
