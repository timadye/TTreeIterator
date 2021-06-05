#include <memory>
#include <utility>
#include <tuple>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <map>

#include <gtest/gtest.h>

#include "TSystem.h"
#include "TFile.h"
#include "TUUID.h"
#include "TH1.h"
#include "TRandom3.h"
#include "TTreeReader.h"

#include "TTreeIterator/TTreeIterator.h"

// ==========================================================================================
// Test configuration
// ==========================================================================================

#define DO_TEST1
#define DO_TEST2
#define DO_TEST3
#define DO_ITER
#define DO_ADDR
#define DO_FILL
#define DO_GET
//#define DO_WAIT
#define FULL_CHECKS

const Long64_t nfill1 = 5;
const Long64_t nfill2 = 5;
const Long64_t nfill22 = 3;
const double vinit = 42.3;  // fill each element with a different value starting from here
const double vinit2 = vinit+7*nfill2;
const UInt_t wait_ms = 1000;
const int verbose = 2;

// ==========================================================================================
// Global definitions
// ==========================================================================================

// Disable tests not selected above
#ifndef DO_TEST1
#define iterTests1 DISABLED_iterTests1
#endif
#ifndef DO_TEST2
#define iterTests2 DISABLED_iterTests2
#endif
#ifndef DO_TEST3
#define iterTests3 DISABLED_iterTests3
#endif
#ifndef DO_ITER
#define FillIter  DISABLED_FillIter
#define GetIter   DISABLED_GetIter
#endif
#ifndef DO_ADDR
#define FillAddr  DISABLED_FillAddr
#define GetAddr   DISABLED_GetAddr
#endif
#ifndef DO_FILL
#define FillIter  DISABLED_FillIter
#define FillIter2 DISABLED_FillIter2
#define FillAddr  DISABLED_FillAddr
#endif
#ifndef DO_GET
#define GetIter   DISABLED_GetIter
#define GetAddr   DISABLED_GetAddr
#endif
#ifndef DO_WAIT
#define WaitIter  DISABLED_WaitIter
#define WaitIter2 DISABLED_WaitIter2
#define WaitAddr  DISABLED_WaitAddr
#define WaitAddr2 DISABLED_WaitAddr2
#endif

// A simple user-defined POD class
struct MyStruct {
  double x[3];
  int i;
  // Specify leaf structure for user class:
  constexpr static const char* leaflist = "x[3]/D:i/I";
  bool operator==(const MyStruct &o) const { return x[0]==o.x[0] && x[1]==o.x[1] && x[2]==o.x[2] && i==o.i; }
};

// Another simple user-defined POD class
struct MyStruct2 {
  double x;
  char s[20];
};
// If it is not possible to add a static member "leaflist" to the class, use this instead:
template<> const char* TTreeIterator::GetLeaflist<MyStruct2>() { return "x/D:s/C"; }

// MyStruct3 is the same as MyStruct, but with instrumentation.
struct MyStruct3 : public MyStruct, public ShowConstructors<MyStruct3> {
  MyStruct3() : MyStruct{{-1.0,-1.0,-1.0},-1}, ShowConstructors() {}
  MyStruct3 (double x0, double x1, double x2, int ii) : MyStruct{{x0,x1,x2},ii}, ShowConstructors(ShowConstructors::quiet()) { ShowConstructors::init(); }
  const char* ContentsAsString() const { return Form("%g,%g,%g,%d",x[0],x[1],x[2],i); }
  using MyStruct::leaflist;
};
template<> MyStruct3 TTreeIterator::type_default() { return MyStruct3(-2.0,-2.0,-2.0,-2); }

#ifdef USE_OrderedMap
TEST(mapTests, map1) {
  OrderedMap<std::string,double> m;
  m.insert({"xy",2.1});
  m.insert({"ab",3.2});
  m.insert({"cd",4.3});
  for (auto it : m) {
    std::cout << it.first << ':' << it.second << ' ';
  }
  std::cout << '\n';
#ifdef __cpp_lib_make_reverse_iterator
  for (auto it = m.rbegin(); it != m.rend(); ++it) {
    std::cout << it->first << ':' << it->second << ' ';
  }
#endif
  std::cout << '\n';
  std::cout << "m[cd] = " << m["cd"] << '\n';
  std::cout << "m[ef] = " << m["ef"] << '\n';
}
#else
#define mapTests DISABLED_mapTests
#endif

TEST(mapTests, map2) {
  std::map<std::string,double> m;
  m.insert({"xy",2.1});
  m.insert({"ab",3.2});
  m.insert({"cd",4.3});
  for (auto& it : m) {
    std::cout << it.first << ':' << it.second << ' ';
  }
  std::cout << '\n';
  std::cout << "m[cd] = " << m["cd"] << '\n';
  std::cout << "m[ef] = " << m["ef"] << '\n';
}


// ==========================================================================================
// iterTests1 use TTreeIterator to test writing and reading various types to a TTree.
// ==========================================================================================

TEST(iterTests1, FillIter) {
  TFile f ("iterTests1.root", "recreate");
  if (f.IsZombie()) { Error("iterTests1", "no file"); return; }

  TTreeIterator iter ("test", &f, verbose);
  double v = vinit;  // fill each element with a different value starting from here
  double xsum = 0.0;
  for (auto& entry : iter.FillEntries(nfill1)) {
    Long64_t i=entry.index();
    //    if (i >= 2) break;  // can stop early

    const double& xr = (entry["x"] = v++);
    xsum += xr;
    if (i!=0) entry["y"] = v++;
    if (i!=1) entry["z"] = v++;
    const std::string& sr = entry["s"] = std::string (Form("s:%g",v++));
    if (i!=0) entry["ss"] = std::string (Form("ss:%g",v++));
    std::cout << "x=" << xr << ", s=" << sr << "\n";
    entry["p"] = std::make_pair<std::string,int>(Form("p:%g",v),v+1); v+=2;
    entry["t"] = std::make_tuple<std::string,std::string,double,float>(Form("t0:%g",v),Form("t1:%g",v+1),v+2,v+3); v+=4;
    entry["u"] = TUUID();
    entry["r"] = TRandom3(0);
    if (i!=2) { entry["M"] = MyStruct{v,v+1,v+2,int(v+3)}; v+=4; }
    MyStruct2 M2{v++,""};
    strncpy (M2.s, Form("M2.s:%.1f",v++), sizeof(M2.s)-1);
    entry["M2"] = M2;
    if (i!=3) { entry["v"] = std::vector<std::string>({Form("v:%g",v), Form("v:%g",v+1)}); v+=2; }
    TH1D h(Form("h%lld",i),"h",4,0,4.0);
    h.SetDirectory(0);
    for (int j=0;j<1000+i;j++) h.Fill(gRandom->Gaus(2,0.5));
    entry["h"] = h;
    entry.Fill();
  }
  Info ("FillIter1", "xsum = %g", xsum);  // compare with std::accumulate in AlgIter below
}

TEST(iterTests1, WaitIter) {
  Info ("WaitIter", "sleep for %u ms", wait_ms);
  gSystem->Sleep(wait_ms);
}

TEST(iterTests1, GetIter) {
  TFile f ("iterTests1.root");
  if (f.IsZombie()) { Error("iterTests1", "no file"); return; }

  TTreeIterator iter ("test", &f, verbose);
  for (auto& entry : iter) {
    Long64_t i=entry.index();
    std::string s = entry["s"];
    std::string ss = entry["ss"];
    std::pair<std::string,int> p = entry["p"];
    std::tuple<std::string,std::string,double,float> t = entry["t"];
    MyStruct M = entry["M"];
    MyStruct2 M2 = entry["M2"];
    TRandom3 r = entry["r"];
    std::vector<std::string> vs = entry["v"];
    TUUID u = entry["u"];
    TH1D h = entry["h"];
    std::cout << "Entry " << i
              << ": x=" << double(entry["x"])
              << ", y=" << double(entry["y"])
              << ", z=" << double(entry["z"])
              << ", s=\"" << s << '"'
              << ", ss=\"" << ss << '"'
              << ", p=" << Form("(\"%s\",%d)", p.first.c_str(),p.second)
              << ", t=" << Form("(\"%s\",\"%s\",%g,%g)", std::get<0>(t).c_str(), std::get<1>(t).c_str(), std::get<2>(t), std::get<3>(t))
              << ", r=" << r.GetSeed()
              << ", M=" << Form("(%g,%g,%g,%d)", M.x[0], M.x[1], M.x[2], M.i)
              << ", M2="<< Form("(%g,\"%s\")", M2.x, M2.s)
              << ", u=" << u.AsString()
              << ", v=(";
    for (auto& si : vs) { if (&si != &vs.front()) std::cout << ','; std::cout << '"' << si << '"'; }
    std::cout << ')' << std::endl;
    h.Print("all");
  }
}

TEST(iterTests1, AlgIter) {
  TFile f ("iterTests1.root");
  if (f.IsZombie()) { Error("iterTests1", "no file"); return; }

  TTreeIterator iter ("test", &f, verbose);
  auto sum = std::accumulate (iter.begin(), iter.end(), 0.0,
                              [](double s, const TTreeIterator& entry) { return s + entry.Get<double>("x"); });
  Info("AlgIter1","xsum=%g",sum);

  std::vector<double> vx;
  std::transform (iter.begin(), iter.end(), std::back_inserter(vx),
                  [](const TTreeIterator& entry) -> double { return entry["x"]; });
  std::cout << "vx = ";
  for (auto& x : vx) { if (&x != &vx.front()) std::cout << ','; std::cout << x; }
  std::cout << '\n';
}

// ==========================================================================================
// iterTests2 use basic TTree operations to test writing and reading some instrumented objects
// to see construction and destruction
// ==========================================================================================

TEST(iterTests2, FillAddr) {
  ShowConstructors<MyStruct3>::verbose = verbose;
  ShowConstructors<TestObj>::verbose = verbose;
  TFile f ("iterTests2.root", "recreate");
  ASSERT_FALSE(f.IsZombie()) << "no file";

  TTree tree("test","");
  double a;
  std::string s, *ps=&s;
  MyStruct3 M;
  TestObj o, *po=&o;
  tree.Branch("a",&a);
  double v = vinit;  // fill each element with a different value starting from here
  for (Long64_t i = 0; i < nfill2; i++) {
    if (i == 3) continue;
    if (i==1) {
      auto bs = tree.Branch("s",&ps); ASSERT_TRUE(bs); bs->Fill();
      auto bo = tree.Branch("o",&po); ASSERT_TRUE(bo); bo->Fill();
    } else if (i==2) {
      auto bM = tree.Branch("M",&M,MyStruct3::leaflist); ASSERT_TRUE(bM); bM->Fill(); bM->Fill();
    }
    a = v++;
    if (i >= 1) s = std::string (Form("s:%g",v++));
    if (i >= 2) { M = MyStruct3(v,v+1,v+2,int(v+3)); v+=4; }
    if (i >= 1) { o = TestObj(v,Form("n:%g",v),Form("t:%g",v)); v++; }
    tree.Fill();
  }
  f.Write();
  tree.ResetBranchAddresses();
}

TEST(iterTests1, WaitAddr) {
  Info ("WaitAddr", "sleep for %u ms", wait_ms);
  gSystem->Sleep(wait_ms);
}

TEST(iterTests2, GetAddr) {
  ShowConstructors<MyStruct3>::verbose = verbose;
  ShowConstructors<TestObj>::verbose = verbose;
  TFile f ("iterTests2.root");
  ASSERT_FALSE(f.IsZombie()) << "no file";

  std::unique_ptr<TTree> tree (f.Get<TTree>("test"));
  ASSERT_TRUE(tree) << "no tree";
  EXPECT_EQ(tree->GetEntries(), nfill2-1);
  double a;
  std::string s, *ps=&s;
  MyStruct3 M;
  TestObj o, *po=&o;
  tree->SetBranchAddress("a",&a);
  tree->SetBranchAddress("s",&ps);
  tree->SetBranchAddress("M",&M);
  tree->SetBranchAddress("o",&po);
  double v = vinit;  // fill each element with a different value starting from here
  Long64_t n = tree->GetEntries();
  for (Long64_t i=0; i<n; i++) {
    tree->GetEntry(i);
    Info("GetAddr2","a=%g, s=\"%s\", M=(%g,%g,%g,%d), o=(%g,\"%s\")",a, s.c_str(), M.x[0],M.x[1],M.x[2],M.i, o.value,o.GetName());
#ifdef FULL_CHECKS
    EXPECT_EQ (a, v++);
    if (i >= 1)   EXPECT_EQ (s, std::string (Form("s:%g",v++)));
    else          EXPECT_EQ (s, std::string());
    if (i >= 2) { EXPECT_EQ (M, MyStruct3(v,v+1,v+2,int(v+3))); v+=4; }
    else          EXPECT_EQ (M, MyStruct3());
    if (i >= 1) { EXPECT_EQ (o.value, v); EXPECT_STREQ (o.GetName(), Form("n:%g",v)); v++; }
    else        { TestObj t; EXPECT_EQ (o.value, t.value); EXPECT_STREQ (o.GetName(), t.GetName()); }
#endif
  }
  tree->ResetBranchAddresses();
}

TEST(iterTests2, WaitAddr2) {
  Info ("WaitAddr2", "sleep for %u ms", wait_ms);
  gSystem->Sleep(wait_ms);
}

// ==========================================================================================
// iterTests2 use TTreeIterator to test writing and reading some instrumented objects
// to see construction and destruction
// ==========================================================================================

TEST(iterTests2, FillIter) {
  ShowConstructors<MyStruct3>::verbose = verbose;
  ShowConstructors<TestObj>::verbose = verbose;
  TFile f ("iterTests2.root", "recreate");
  ASSERT_FALSE(f.IsZombie()) << "no file";

  TTreeIterator iter ("test", &f, verbose);
  double v = vinit;
  int j = 0;  // just for skipping an entry, since we can't use entry.index()
  for (auto& entry : iter.FillEntries(nfill2)) {
    Long64_t i=entry.index();
    if (j++ == 3) continue;
    entry["a"] = v++;
    if (i >= 1) entry["s"] = std::string (Form("s:%g",v++));
    if (i >= 2) { entry["M"] = MyStruct3(v,v+1,v+2,int(v+3)); v+= 4; }
    if (i >= 1) { entry["o"] = TestObj(v,Form("n:%g",v),Form("t:%g",v)); v++; }
    entry.Fill();
  }
}

TEST(iterTests2, WaitIter) {
  Info ("WaitIter1", "sleep for %u ms", wait_ms);
  gSystem->Sleep(wait_ms);
}

TEST(iterTests2, FillIter2) {
  ShowConstructors<MyStruct3>::verbose = verbose;
  ShowConstructors<TestObj>::verbose = verbose;
  TFile f ("iterTests2.root", "update");
  ASSERT_FALSE(f.IsZombie()) << "no file";

  TTreeIterator iter ("test", &f, verbose);
  double v = vinit2;
  for (auto& entry : iter.FillEntries(nfill22)) {
    entry["a"] = v++;
    entry["s"] = std::string (Form("s:%g",v++));
    entry["M"] = MyStruct3(v,v+1,v+2,int(v+3)); v+=4;
    entry["o"] = TestObj(v,Form("n:%g",v),Form("t:%g",v)); v++;
    entry.Fill();
  }
}

TEST(iterTests2, WaitIter2) {
  Info ("WaitIter2", "sleep for %u ms", wait_ms);
  gSystem->Sleep(wait_ms);
}

TEST(iterTests2, GetIter) {
  ShowConstructors<MyStruct3>::verbose = verbose;
  ShowConstructors<TestObj>::verbose = verbose;
  TFile f ("iterTests2.root");
  if (f.IsZombie()) { Error("TestFill2", "no file"); return; }

  TTreeIterator iter ("test", &f, verbose);
  ASSERT_TRUE(iter.GetTree()) << "no tree";
  bool only2 = (iter.GetEntries() == nfill2);
  if (only2) EXPECT_EQ(iter.GetEntries(), nfill2-1);
  else       EXPECT_EQ(iter.GetEntries(), nfill2+nfill22-1);
  double v = vinit;
  for (auto& entry : iter) {
    Long64_t i=entry.index();
    // let's try accessing elements in a different order from creation (but print them in the creation order)
    std::string s = entry["s"];
    const TestObj& o = entry["o"];
    MyStruct3& M = entry["M"];
    double a = entry["a"];
    Info("GetIter2","Entry %lld: a=%g, s=\"%s\", M=(%g,%g,%g,%d), o=(%g,\"%s\")", i, a, s.c_str(), M.x[0],M.x[1],M.x[2],M.i, o.value,o.GetName());
#ifdef FULL_CHECKS
    if (i == nfill2-1) v = vinit2;
    EXPECT_EQ (a, v++);
    if (i >= 1)   EXPECT_EQ (s, std::string (Form("s:%g",v++)));
    else          EXPECT_EQ (s, std::string());
    if (i >= 2) { EXPECT_EQ (M, MyStruct3(v,v+1,v+2,int(v+3))); v+=4; }
    else          EXPECT_EQ (M, TTreeIterator::type_default<MyStruct3>());
    if (i >= 1) { EXPECT_EQ (o.value, v); EXPECT_STREQ (o.GetName(), Form("n:%g",v)); v++; }
    else        { TestObj t; EXPECT_EQ (o.value, t.value); EXPECT_STREQ (o.GetName(), t.GetName()); }
#endif
  }
}

// ==========================================================================================
// iterTests3 tests reading from an example TTree
// ==========================================================================================

TEST(iterTests3, GetIter) {
  TFile f ("scan_result_example.root");
  if (f.IsZombie()) { Error("GetIter3", "no file"); return; }

  TTreeIterator iter ("myFits", &f, verbose);

  for (auto& entry : iter) {
    Long64_t i=entry.index();
//  auto covQual = entry.Get<int>("covQual");
//  int covQual = entry.Get("covQual");
    int covQual = entry["covQual"];
    Info ("GetIter3", "Entry %lld covQual = %d", i, covQual);

//  int bad_int = entry["bad_int"];
    auto bad_int = entry.Get("bad_int",-9999);
    Info ("GetIter3", "Entry %lld bad_int = %d", i, bad_int);

//  auto mu = entry.Get<double>("const.mu");
//  double mu = entry.Get("const.mu",-999.0);
    double mu = entry["const.mu"];
    Info ("GetIter3", "Entry %lld const.mu = %g", i, mu);

    int imu = entry["const.mu"];
    Info ("GetIter3", "Entry %lld const.mu = %d (get as int)", i, imu);

    float fmu = entry["const.mu"];
    Info ("GetIter3", "Entry %lld const.mu = %g (get as float)", i, fmu);

//  double bad_double = entry.Get("bad_double",-999.0);
    double bad_double = entry["bad_double"];
    Info ("GetIter3", "Entry %lld bad_double = %g", i, bad_double);

//  auto hash = entry.Get<std::pair<int,int>>("hash");
//  std::pair<int,int> hash = entry.Get("hash");
    std::pair<int,int> hash = entry["hash"];
    Info ("GetIter3", "Entry %lld hash = (0x%08x,0x%08x)", i, hash.first, hash.second);

    std::pair<int,int> bad_pair = entry["bad_pair"];
    Info ("GetIter3", "Entry %lld bad_pair = (0x%08x,0x%08x)", i, bad_pair.first, bad_pair.second);

//  auto statusHistory = entry.Get<std::vector<std::pair<std::string,int>>>("statusHistory");
//  std::vector<std::pair<std::string,int>> statusHistory = entry.Get("statusHistory");
    std::vector<std::pair<std::string,int>> statusHistory = entry["statusHistory"];
    Info ("GetIter3", "Entry %lld statusHistory(%lu) =", i, statusHistory.size());
    for (auto& p : statusHistory) {
      std::cout << "  '" << p.first << "': " << p.second << "\n";
    }

    std::vector<std::pair<std::string,int>> bad_vector = entry["bad_vector"];
    Info ("GetIter3", "Entry %lld bad_vector(%lu) =", i, bad_vector.size());
    for (auto& p : bad_vector) {
      std::cout << "  '" << p.first << "': " << p.second << "\n";
    }

//  auto uuid = entry.Get<TUUID>("uuid");
//  TUUID uuid = entry.Get("uuid");
    TUUID uuid = entry["uuid"];

    Info ("GetIter3", "Entry %lld uuid:", i);
    std::cout << "  "; uuid.Print();
    TUUID bad_uuid = entry["bad_uuid"];

    Info ("GetIter3", "Entry %lld bad_uuid:", i);
    std::cout << "  "; bad_uuid.Print();

    if (i >= 1) break;  // only need to test twice
  }
}

// ==========================================================================================
// iterTests4 use TTreeIterator to test simple filling and reading
// ==========================================================================================

#include "TH2.h"
#include "TCanvas.h"
#include "TPython.h"

TEST(iterTests4, FillIter) {
  TFile file ("xyz.root", "recreate");
  if (file.IsZombie()) return;
  gRandom->SetSeed(654321);

  TTreeIterator tree("xyz", &file);
  for (auto& entry : tree.FillEntries(10000)) {
    entry["vx"] = gRandom->Gaus(2,3);
    entry["vy"] = gRandom->Gaus(-1,2);
    entry["vz"] = gRandom->Gaus(0,100);
    entry.Fill();
  }
}

TEST(iterTests4, GetIter) {
  TFile file ("xyz.root");
  if (file.IsZombie()) return;

  TH2D hxy ("vxy", "vxy", 48, -6, 6, 32, -4, 4);
  TH1D hz  ("vz",  "vz",  100, -200, 200);

  TTreeIterator tree("xyz", &file);
  for (auto& entry : tree) {
    hxy.Fill (entry.Get<double>("vx"), entry.Get<double>("vy"));
    hz .Fill (entry["vz"]);
  }

  TCanvas c1("c1");
  hxy.Draw("colz");
  c1.Print("xyz.pdf(");
  hz.Draw();
  c1.Print("xyz.pdf)");
}

TEST(iterTests4, GetAddr) {
  TFile file ("xyz.root");
  if (file.IsZombie()) return;

  TH2D hxy ("vxy", "vxy", 48, -6, 6, 32, -4, 4);
  TH1D hz  ("vz",  "vz",  100, -200, 200);

  std::unique_ptr<TTree> tree (file.Get<TTree>("xyz"));

  double vx, vy, vz;
  tree->SetBranchAddress("vx",&vx);
  tree->SetBranchAddress("vy",&vx);  // <-- spot the mistake!
  tree->SetBranchAddress("vz",&vz);
  Long64_t n = tree->GetEntries();
  for (Long64_t i=0; i<n; i++) {
    tree->GetEntry(i);
    hxy.Fill (vx, vy);
    hz .Fill (vz);
  }
  tree->ResetBranchAddresses();

  TCanvas c1("c1");
  hxy.Draw("colz");
  c1.Print("xyza.pdf(");
  hz.Draw();
  c1.Print("xyza.pdf)");
}

TEST(iterTests4, GetReader) {
  TFile file ("xyz.root");
  if (file.IsZombie()) return;

  TH2D hxy ("vxy", "vxy", 48, -6, 6, 32, -4, 4);
  TH1D hz  ("vz",  "vz",  100, -200, 200);

  TTreeReader tree("xyz", &file);
  TTreeReaderValue<double> vx (tree, "vx2");
  TTreeReaderValue<double> vy (tree, "vx");  // <-- spot the mistake!
  TTreeReaderValue<double> vz (tree, "vz");
  while (tree.Next()) {
    std::cout << *vx << '\n';
    hxy.Fill (*vx, *vy);
    hz .Fill (*vz);
  }

  TCanvas c1("c1");
  hxy.Draw("colz");
  c1.Print("xyzr.pdf(");
  hz.Draw();
  c1.Print("xyzr.pdf)");
}

TEST(iterTests4, PyROOT) {
  TPython::Exec (R"python(
import ROOT
ROOT.gROOT.SetBatch(True)

f = ROOT.TFile.Open("xyz.root")

hxy = ROOT.TH2D ("vxy", "vxy", 48, -6, 6, 32, -4, 4);
hz  = ROOT.TH1D ("vz",  "vz",  100, -200, 200);

tree = f.Get("xyz")
for entry in tree:
  hxy.Fill (entry.vx, entry.vy)
  hz .Fill (entry.vz)

c1 = ROOT.TCanvas("c1")
hxy.Draw("colz");
c1.Print("xyzp.pdf(");
hz.Draw();
c1.Print("xyzp.pdf)");
)python");
}
