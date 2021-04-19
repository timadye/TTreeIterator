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

#include "TFile.h"
#include "TUUID.h"
#include "TH1.h"
#include "TRandom3.h"

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
#define FULL_CHECKS

const Long64_t nfill1 = 5;
const Long64_t nfill2 = 4;
const Long64_t nfill22 = 3;
const double vinit = 42.3;  // fill each element with a different value starting from here
const double vinit2 = vinit+7*nfill2;
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
  MyStruct3() : MyStruct{-1.0,-1.0,-1.0,-1}, ShowConstructors() {}
  MyStruct3 (double x0, double x1, double x2, int ii) : MyStruct{x0,x1,x2,ii}, ShowConstructors(ShowConstructors::quiet()) { ShowConstructors::init(); }
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
  for (auto it = m.rbegin(); it != m.rend(); ++it) {
    std::cout << it->first << ':' << it->second << ' ';
  }
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
  int ml = 1;
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
    entry["p"] = std::make_pair<std::string,int>(Form("p:%g",v++),v++);
    entry["t"] = std::make_tuple<std::string,std::string,double,float>(Form("t0:%g",v++),Form("t1:%g",v++),v++,v++);
    entry["u"] = TUUID();
    entry["r"] = TRandom3(0);
    if (i!=2) entry["M"] = MyStruct{v++,v++,v++,int(v++)};
    MyStruct2 M2{v++,""};
    strncpy (M2.s, Form("M2.s:%.*f",ml++,v++), sizeof(M2.s));
    entry["M2"] = M2;
    if (i!=3) entry["v"] = std::vector<std::string>({Form("v:%g",v++), Form("v:%g",v++)});
    TH1D h(Form("h%lld",i),"h",4,0,4.0);
    h.SetDirectory(0);
    for (int j=0;j<1000+i;j++) h.Fill(gRandom->Gaus(2,0.5));
    entry["h"] = h;
  }
  Info ("FillIter1", "xsum = %g", xsum);  // compare with std::accumulate in AlgIter below
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
                  [](auto& entry) -> double { return entry["x"]; });
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
    if (i==1) {
      auto bs = tree.Branch("s",&ps); ASSERT_TRUE(bs); bs->Fill();
      auto bo = tree.Branch("o",&po); ASSERT_TRUE(bo); bo->Fill();
    } else if (i==2) {
      auto bM = tree.Branch("M",&M,MyStruct3::leaflist); ASSERT_TRUE(bM); bM->Fill(); bM->Fill();
    }
    a = v++;
    if (i >= 1) s = std::string (Form("s:%g",v++));
    if (i >= 2) M = MyStruct3(v++,v++,v++,int(v++));
    if (i >= 1) { o = TestObj(v,Form("n:%g",v),Form("t:%g",v)); v++; }
    tree.Fill();
  }
  f.Write();
  tree.ResetBranchAddresses();
}

TEST(iterTests2, GetAddr) {
  ShowConstructors<MyStruct3>::verbose = verbose;
  ShowConstructors<TestObj>::verbose = verbose;
  TFile f ("iterTests2.root");
  ASSERT_FALSE(f.IsZombie()) << "no file";

  std::unique_ptr<TTree> tree (f.Get<TTree>("test"));
  ASSERT_TRUE(tree) << "no tree";
  EXPECT_EQ(tree->GetEntries(), nfill2);
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
    if (i >= 1) EXPECT_EQ (s, std::string (Form("s:%g",v++)));
    else        EXPECT_EQ (s, std::string());
    if (i >= 2) EXPECT_EQ (M, MyStruct3(v++,v++,v++,int(v++)));
    else        EXPECT_EQ (M, MyStruct3());
    if (i >= 1) { EXPECT_EQ (o.value, v); EXPECT_STREQ (o.GetName(), Form("n:%g",v)); v++; }
    else        { TestObj t; EXPECT_EQ (o.value, t.value); EXPECT_STREQ (o.GetName(), t.GetName()); }
#endif
  }
  tree->ResetBranchAddresses();
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
  for (auto& entry : iter.FillEntries(nfill2)) {
    Long64_t i=entry.index();
    entry["a"] = v++;
    if (i >= 1) entry["s"] = std::string (Form("s:%g",v++));
    if (i >= 2) entry["M"] = MyStruct3(v++,v++,v++,int(v++));
    if (i >= 1) { entry["o"] = TestObj(v,Form("n:%g",v),Form("t:%g",v)); v++; }
  }
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
    entry["M"] = MyStruct3(v++,v++,v++,int(v++));
    entry["o"] = TestObj(v,Form("n:%g",v),Form("t:%g",v)); v++;
  }
}

TEST(iterTests2, GetIter) {
  ShowConstructors<MyStruct3>::verbose = verbose;
  ShowConstructors<TestObj>::verbose = verbose;
  TFile f ("iterTests2.root");
  if (f.IsZombie()) { Error("TestFill2", "no file"); return; }

  TTreeIterator iter ("test", &f, verbose);
  ASSERT_TRUE(iter.GetTree()) << "no tree";
  bool only2 = (iter.GetEntries() == nfill2);
  if (only2) EXPECT_EQ(iter.GetEntries(), nfill2);
  else       EXPECT_EQ(iter.GetEntries(), nfill2+nfill22);
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
    if (i == nfill2) v = vinit2;
    EXPECT_EQ (a, v++);
    if (i >= 1) EXPECT_EQ (s, std::string (Form("s:%g",v++)));
    else        EXPECT_EQ (s, std::string());
    if (i >= 2) EXPECT_EQ (M, MyStruct3(v++,v++,v++,int(v++)));
    else        EXPECT_EQ (M, TTreeIterator::type_default<MyStruct3>());
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
    Info ("GetIter3", "Entry %d covQual = %d", i, covQual);

//  int bad_int = entry["bad_int"];
    auto bad_int = entry.Get("bad_int",-9999);
    Info ("GetIter3", "Entry %d bad_int = %d", i, bad_int);

//  auto mu = entry.Get<double>("const.mu");
//  double mu = entry.Get("const.mu",-999.0);
    double mu = entry["const.mu"];
    Info ("GetIter3", "Entry %d const.mu = %g", i, mu);

    int imu = entry["const.mu"];
    Info ("GetIter3", "Entry %d const.mu = %d (get as int)", i, imu);

    float fmu = entry["const.mu"];
    Info ("GetIter3", "Entry %d const.mu = %g (get as float)", i, fmu);

//  double bad_double = entry.Get("bad_double",-999.0);
    double bad_double = entry["bad_double"];
    Info ("GetIter3", "Entry %d bad_double = %g", i, bad_double);

//  auto hash = entry.Get<std::pair<int,int>>("hash");
//  std::pair<int,int> hash = entry.Get("hash");
    std::pair<int,int> hash = entry["hash"];
    Info ("GetIter3", "Entry %d hash = (0x%08x,0x%08x)", i, hash.first, hash.second);

    std::pair<int,int> bad_pair = entry["bad_pair"];
    Info ("GetIter3", "Entry %d bad_pair = (0x%08x,0x%08x)", i, bad_pair.first, bad_pair.second);

//  auto statusHistory = entry.Get<std::vector<std::pair<std::string,int>>>("statusHistory");
//  std::vector<std::pair<std::string,int>> statusHistory = entry.Get("statusHistory");
    std::vector<std::pair<std::string,int>> statusHistory = entry["statusHistory"];
    Info ("GetIter3", "Entry %d statusHistory(%lu) =", i, statusHistory.size());
    for (auto& p : statusHistory) {
      std::cout << "  '" << p.first << "': " << p.second << "\n";
    }

    std::vector<std::pair<std::string,int>> bad_vector = entry["bad_vector"];
    Info ("GetIter3", "Entry %d bad_vector(%lu) =", i, bad_vector.size());
    for (auto& p : bad_vector) {
      std::cout << "  '" << p.first << "': " << p.second << "\n";
    }

//  auto uuid = entry.Get<TUUID>("uuid");
//  TUUID uuid = entry.Get("uuid");
    TUUID uuid = entry["uuid"];

    Info ("GetIter3", "Entry %d uuid:", i);
    std::cout << "  "; uuid.Print();
    TUUID bad_uuid = entry["bad_uuid"];

    Info ("GetIter3", "Entry %d bad_uuid:", i);
    std::cout << "  "; bad_uuid.Print();

    if (i >= 1) break;  // only need to test twice
  }
}
