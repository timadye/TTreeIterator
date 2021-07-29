#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <cmath>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "TFile.h"
#include "TString.h"
#include "TSystem.h"
#include "TStopwatch.h"
#include "TTree.h"
#include "TTreeIterator/TTreeIterator.h"


#ifndef NFILL
#define NFILL 5
#endif
#ifndef NX
#define NX 2
#endif
#ifndef VERBOSE
#define VERBOSE 3
#endif

#ifndef NONE
#define TEST1
#define TEST2
#define TEST3
#define FILL
#define GET
#endif

//#define FAST_CHECKS 1

const Long64_t nfill1 = NFILL;
const Long64_t nfill2 = NFILL;
const Long64_t nfill3 = NFILL;
constexpr size_t nx1 = NX;
constexpr size_t nx2 = NX;
constexpr size_t nx3 = NX;
const double vinit = 42.3;  // fill each element with a different value starting from here
constexpr int verbose = VERBOSE;

// A simple user-defined POD class
struct MyStruct {
  double x[nx2];
};
template<> const char* TTreeIterator::GetLeaflist<MyStruct>() { return Form("x[%d]/D",int(nx2)); }

#ifdef USE_LOG
typedef std::chrono::steady_clock Timer;
typedef std::chrono::time_point<Timer> TimePoint;

TimePoint log (const char* msg, const char* func, TimePoint start=TimePoint()) {
  using namespace std::literals;
  auto timer = std::chrono::steady_clock::now();
  if (verbose >= 0) {
    auto now = std::time(0);
    std::cout << std::put_time(std::localtime(&now), "%Y-%m-%d-%H:%M:%S")
              << ' ' << msg
              << ' ' << func;
    if (start != TimePoint()) {
      std::cout << " took " << ((timer-start)/1ms)*0.001 << 's';
    }
    std::cout << std::endl;
  }
  return timer;
}
#endif

class StartTimer : public TStopwatch {
  std::string fFunc;
  TTree* fTree = 0;
  bool fFill = false;
  ULong64_t fNElements = 1;
  bool fPrinted = false;
public:
  StartTimer(const char* func, TTree* tree=0, bool fill=false, ULong64_t nelem=1)
    : TStopwatch(),fFunc(func), fTree(tree), fFill(fill),     fNElements(nelem) {
    if (verbose >= 0) {
      auto now = std::time(0);
      std::cout << std::put_time(std::localtime(&now), "%Y-%m-%d-%H:%M:%S")
                << " start " << func << std::endl;
    }
  }
  ~StartTimer() override { if (!fPrinted) PrintResults(); }

  void PrintResults() {
    auto realTime = RealTime();
    auto  cpuTime =  CpuTime();
    auto now = std::time(0);

    std::string prog = gSystem->BaseName(__FILE__);
    auto ext = prog.find_last_of('.');
    if (ext != std::string::npos) prog.resize(ext);

    const char* timelog = gSystem->Getenv("TIMELOG");
    const char* label   = gSystem->Getenv("LABEL");
    std::string filename;
    if (timelog && *timelog) filename = timelog;
    else                     filename = prog+".csv";
    if (!label) label = "";

    std::ofstream of (filename, std::ios::app);
    if (of.tellp() == 0) {
      of << "time/C,host/C,label/C,testcase/C,test/C,fill/B,entries/L,branches/I,elements/l,ms/D,cpu/D\n";
    }

    std::ostringstream oss;
    std::ostream& os = ((verbose >= 1) ? static_cast<std::ostream&>(oss) : static_cast<std::ostream&>(of));
    os << std::put_time(std::localtime(&now), "%Y-%m-%d-%H:%M:%S")
       << ',' << gSystem->HostName()
       << ',' << label
       << ',' << prog
       << ',' << fFunc
       << ',' << fFill
       << ',' << (fTree                               ? fTree->GetEntries()                      : 0)
       << ',' << (fTree && fTree->GetListOfBranches() ? fTree->GetListOfBranches()->GetEntries() : 1)
       << ',' << fNElements
       << ',' << std::llround (realTime*1000.0)
       << ',' << std::llround ( cpuTime*1000.0)
       << '\n';
    if (verbose >= 1) {
      std::cout << oss.str();
      of        << oss.str();
    }
    if (verbose >= 0) {
      std::cout << std::put_time(std::localtime(&now), "%Y-%m-%d-%H:%M:%S")
                << " end   " << fFunc << " took " << Form("%.3f",realTime) << 's' << std::endl;
    }
    Continue();
    fPrinted = true;
  }
};





#ifdef TEST1
#ifdef FILL
void FillIter1() {
  TFile file ("test_timing1.root", "recreate");
  assert (!file.IsZombie());
  std::vector<std::string> bnames;
  bnames.reserve(nx1);
  for (size_t i=0; i<nx1; i++) bnames.emplace_back (Form("x%03zu",i));
  TTreeIterator iter ("test", verbose);
  double v = vinit;
  StartTimer timer (__func__, iter.GetTree(), true);
  for (auto& entry : iter.FillEntries(nfill1)) {
    for (auto& b : bnames) entry[b.c_str()] = v++;
    entry.Fill();
  }
  assert (vinit+double(nx1*nfill1) == v);
}
#endif

#ifdef GET
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
  StartTimer timer (__func__, iter.GetTree());
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


#ifdef TEST2
#ifdef FILL
void FillIter2() {
  static_assert (sizeof(MyStruct::x)/sizeof(MyStruct::x[0]) == nx2, "MyStruct::x wrong size");
  TFile file ("test_timing2.root", "recreate");
  assert (!file.IsZombie());

  TTreeIterator iter ("test", verbose);
  double v = vinit;
  StartTimer timer (__func__, iter.GetTree(), true);
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

#ifdef GET
void GetIter2() {
  static_assert (sizeof(MyStruct::x)/sizeof(MyStruct::x[0]) == nx2, "MyStruct::x wrong size");
  TFile file ("test_timing2.root");
  assert (!file.IsZombie());

  TTreeIterator iter ("test", &file, verbose);
  assert (iter.GetTree());
  assert (iter.GetEntries() == nfill2);
  double v = vinit, vsum=0.0;
  StartTimer timer (__func__, iter.GetTree());
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

#ifdef TEST3
#ifdef FILL
void FillIter3() {
  TFile file ("test_timing3.root", "recreate");
  assert (!file.IsZombie());

  TTreeIterator iter ("test", verbose);
  double v = vinit;
  StartTimer timer (__func__, iter.GetTree(), true);
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

#ifdef GET
void GetIter3() {
  TFile file ("test_timing3.root");
  assert (!file.IsZombie());

  TTreeIterator iter ("test", &file, verbose);
  assert (iter.GetTree());
  assert (iter.GetEntries() == nfill3);
  double v = vinit, vsum=0.0;
  StartTimer timer (__func__, iter.GetTree());
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


int main(int argc, char* argv[]) {
  const char* a = (argc >= 2) ? argv[1] : "1g";
#ifdef TEST1
  if (strchr(a,'1')) {
#ifdef FILL
    if (strchr(a,'f')) FillIter1();
#endif
#ifdef GET
    if (strchr(a,'g')) GetIter1();
#endif
  }
#endif
#ifdef TEST2
  if (strchr(a,'2')) {
#ifdef FILL
    if (strchr(a,'f')) FillIter2();
#endif
#ifdef GET
    if (strchr(a,'g')) GetIter2();
#endif
  }
#endif
#ifdef TEST3
  if (strchr(a,'3')) {
#ifdef FILL
    if (strchr(a,'f')) FillIter3();
#endif
#ifdef GET
    if (strchr(a,'g')) GetIter3();
#endif
  }
#endif
}
