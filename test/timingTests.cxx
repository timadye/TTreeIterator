#include <gtest/gtest.h>

#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <ctime>

#include "TSystem.h"
#include "TError.h"
#include "TStopwatch.h"
#include "TFile.h"
#include "TTree.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"
#include "TTreeReaderArray.h"

#include "test/GTestSetup.h"
#include "TTreeIterator/TTreeIterator.h"

// ==========================================================================================
// Test configuration
// ==========================================================================================

//#define NO_TEST1 1
//#define NO_TEST2 1
//#define NO_TEST3 1
//#define NO_ITER 1
//#define NO_ADDR 1
//#define NO_FILL 1
//#define NO_GET 1
//#define FAST_CHECKS 1

#ifndef NFILL
#define NFILL 500000
#endif
#ifndef NX
#define NX 100
#endif
#ifndef VERBOSE
#define VERBOSE 0
#endif

const Long64_t nfill1 = NFILL;
const Long64_t nfill2 = NFILL;
const Long64_t nfill3 = NFILL;
constexpr size_t nx1 = NX;
constexpr size_t nx2 = NX;
constexpr size_t nx3 = NX;
const double vinit = 42.3;  // fill each element with a different value starting from here
const int verbose = VERBOSE;
int LimitedEventListener::maxmsg = 10;

// ==========================================================================================
// Global definitions
// ==========================================================================================

// Disable tests not selected above
#ifdef NO_TEST1
#define timingTests1 DISABLED_timingTests1
#endif
#ifdef NO_TEST2
#define timingTests2 DISABLED_timingTests2
#endif
#ifdef NO_TEST3
#define timingTests3 DISABLED_timingTests3
#endif
#ifdef NO_ITER
#define FillIter DISABLED_FillIter
#define GetIter  DISABLED_GetIter
#endif
#ifdef NO_ADDR
#define FillAddr DISABLED_FillAddr
#define GetAddr  DISABLED_GetAddr
#endif
#ifdef NO_FILL
#define FillIter DISABLED_FillIter
#define FillAddr DISABLED_FillAddr
#endif
#ifdef NO_GET
#define GetIter  DISABLED_GetIter
#define GetAddr  DISABLED_GetAddr
#define GetReader DISABLED_GetReader
#endif

Int_t ShowBranches (const TFile& file, TTree* tree, const std::string& branch_type, const char* op = "tree has") {
  Int_t nbranches = tree->GetListOfBranches()->GetEntries();
  const testing::TestInfo* const test_info = testing::UnitTest::GetInstance()->current_test_info();
  ::Info (Form("%s.%s",test_info->test_case_name(),test_info->name()),
          "%s:%s: %s %lld entries, %d branches of type %s",
          file.GetName(), tree->GetName(), op, tree->GetEntries(), nbranches, branch_type.c_str());
  return nbranches;
}

class StartTimer : public TStopwatch {
  TTree* fTree = 0;
  bool fFill = false;
  ULong64_t fNElements = 1;
  bool fPrinted = false;
public:
  StartTimer(TTree* tree=0, bool fill=false, ULong64_t nelem=1) : TStopwatch(), fTree(tree), fFill(fill), fNElements(nelem) {}
  ~StartTimer() override { if (!fPrinted) PrintResults(); }

  void PrintResults() {
    auto realTime = RealTime();
    auto  cpuTime =  CpuTime();
    auto now = std::time(0);

    const char* filename = gSystem->Getenv("TIMELOG");
    const char* label    = gSystem->Getenv("LABEL");
    if (!filename || !*filename) filename = "timingTests.csv";
    if (!label) label = "";
    char stamp[22];
    std::strftime (stamp, sizeof(stamp), "%Y-%m-%d-%H:%M:%S", std::localtime(&now));
    const testing::TestInfo* const test_info = testing::UnitTest::GetInstance()->current_test_info();

    std::ofstream of (filename, std::ios::app);
    if (of.tellp() == 0) {
      of << "time/C,host/C,label/C,testcase/C,test/C,fill/B,entries/L,branches/I,elements/l,ms/D,cpu/D\n";
    }

    std::ostringstream oss;
    std::ostream& os = ((verbose >= 1) ? static_cast<std::ostream&>(oss) : static_cast<std::ostream&>(of));
    os << stamp
       << ',' << gSystem->HostName()
       << ',' << label
       << ',' << test_info->test_case_name()
       << ',' << test_info->name()
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
    Continue();
    fPrinted = true;
  }
};

// ==========================================================================================
// TTreeIterator test 1: write/read a bunch of doubles
// ==========================================================================================

const std::string branch_type1 = "double";

TEST(timingTests1, FillIter) {
  TFile file ("test_timing1.root", "recreate");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  std::vector<std::string> bnames;
  bnames.reserve(nx1);
  for (int i=0; i<nx1; i++) bnames.emplace_back (Form("x%03d",i));

  TTreeIterator iter ("test", verbose);
  StartTimer timer (iter.GetTree(), true);
  double v = vinit;
  for (auto& entry : iter.FillEntries(nfill1)) {
    for (auto& b : bnames) entry[b.c_str()] = v++;
  }
  Int_t nbranches = ShowBranches (file, iter.GetTree(), branch_type1, "filled");
  EXPECT_FLOAT_EQ (vinit+double(nbranches*nfill1), v);
}

TEST(timingTests1, GetIter) {
  TFile file ("test_timing1.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  std::vector<std::string> bnames;
  bnames.reserve(nx1);
  for (int i=0; i<nx1; i++) bnames.emplace_back (Form("x%03d",i));

  TTreeIterator iter ("test", &file, verbose);
  ASSERT_TRUE(iter.GetTree()) << "no tree";
  EXPECT_EQ(iter.GetEntries(), nfill1);
  Int_t nbranches = ShowBranches (file, iter.GetTree(), branch_type1);
  EXPECT_EQ(nbranches, nx1);

  StartTimer timer (iter.GetTree());

  double v = vinit, vsum=0.0;
  for (auto& entry : iter) {
    for (auto& b : bnames) {
      double x = entry[b.c_str()];
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, branch %d",iter.index(),b.c_str());
#endif
    }
  }
  double vn = double(nbranches*nfill1);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}

TEST(timingTests1, GetIter2) {
  TFile file ("test_timing1.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  std::vector<std::string> bnames;
  bnames.reserve(nx1);
  for (int i=0; i<nx1; i++) bnames.emplace_back (Form("x%03d",i));

  TTreeIterator iter ("test", &file, verbose);
  ASSERT_TRUE(iter.GetTree()) << "no tree";
  EXPECT_EQ(iter.GetEntries(), nfill1);
  Int_t nbranches = ShowBranches (file, iter.GetTree(), branch_type1);
  EXPECT_EQ(nbranches, nx1);

  // read once in reverse order for slowest access later
  {
    double vsum=0.0;
    auto& entry = *iter.begin();
    for (std::ptrdiff_t i = nx1-1; i >= 0; --i) {
      double x = entry[bnames[i].c_str()];
      vsum += x;
    }
    double vn = double(nbranches);
    EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
  }

  StartTimer timer (iter.GetTree());

  double v = vinit, vsum=0.0;
  for (auto& entry : iter) {
    for (auto& b : bnames) {
      double x = entry[b.c_str()];
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, branch %d",iter.index(),b.c_str());
#endif
    }
  }
  double vn = double(nbranches*nfill1);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}

// ==========================================================================================
// TTree test 1: write/read a bunch of doubles
// ==========================================================================================

TEST(timingTests1, FillAddr) {
  TFile file ("test_timing1.root", "recreate");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTree tree("test","");
  std::vector<double> vals(nx1);
  StartTimer timer (&tree, true);
  for (int i=0; i<nx1; i++) tree.Branch (Form("x%03d",i), &vals[i]);

  double v = vinit;
  for (Long64_t i = 0; i < nfill1; i++) {
    for (auto& x : vals) x = v++;
    tree.Fill();
  }
  file.Write();

  Int_t nbranches = ShowBranches (file, &tree, branch_type1, "filled");
  EXPECT_FLOAT_EQ (vinit+double(nbranches*nfill1), v);
}

TEST(timingTests1, GetAddr) {
  TFile file ("test_timing1.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  std::unique_ptr<TTree> tree (file.Get<TTree>("test"));
  ASSERT_TRUE(tree) << "no tree";
  EXPECT_EQ(tree->GetEntries(), nfill1);
  Int_t nbranches = ShowBranches (file, tree.get(), branch_type1);
  EXPECT_EQ(nbranches, nx1);

  std::vector<double> vals(nx1);
  StartTimer timer (tree.get());
  for (int i=0; i<nx1; i++) tree->SetBranchAddress (Form("x%03d",i), &vals[i]);

  double v = vinit, vsum=0.0;
  Long64_t n = tree->GetEntries();
  for (Long64_t i=0; i<n; i++) {
    tree->GetEntry(i);
    for (auto& vx : vals) {
      double x = vx;
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, element %d",i,&x-vals.data());
#endif
    }
  }
  double vn = double(nbranches*nfill1);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}

// ==========================================================================================
// TTreeReader test 1: read a bunch of doubles
// ==========================================================================================

TEST(timingTests1, GetReader) {
  TFile file ("test_timing1.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTreeReader reader ("test", &file);
  ASSERT_TRUE(reader.GetTree()) << "no tree";
  EXPECT_EQ(reader.GetEntries(), nfill1);
  Int_t nbranches = ShowBranches (file, reader.GetTree(), branch_type1);
  EXPECT_EQ(nbranches, nx1);

  StartTimer timer (reader.GetTree());
  std::vector<TTreeReaderValue<Double_t>> vals;
  vals.reserve(nx1);
  for (int i=0; i<nx1; i++)
    vals.emplace_back (reader, Form("x%03d",i));

  double v = vinit, vsum=0.0;
  while (reader.Next()) {
    for (auto& vx : vals) {
      double x = *vx;
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, branch %d",reader.GetCurrentEntry(),vx.GetBranchName());
#endif
    }
  }
  double vn = double(nbranches*nfill1);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}

// ==========================================================================================
// TTreeIterator test 2: write/read a bunch of doubles in a POD class
// ==========================================================================================

// A simple user-defined POD class
struct MyStruct {
  double x[nx2];
};
template<> const char* TTreeIterator::GetLeaflist<MyStruct>() { return Form("x[%d]/D",int(nx2)); }
const std::string branch_type2 = TTreeIterator::GetLeaflist<MyStruct>();

TEST(timingTests2, FillIter) {
  ASSERT_EQ (sizeof(MyStruct::x)/sizeof(MyStruct::x[0]), nx2);
  TFile file ("test_timing2.root", "recreate");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTreeIterator iter ("test", verbose);
  StartTimer timer (iter.GetTree(), true, nx2);
  double v = vinit;
  for (auto& entry : iter.FillEntries(nfill2)) {
    MyStruct M;
    for (auto& x : M.x) x = v++;
    entry["M"] = M;
  }
  Int_t nbranches = ShowBranches (file, iter.GetTree(), branch_type2, "filled");
  EXPECT_FLOAT_EQ (vinit+double(nbranches*nfill2*nx2), v);
}

TEST(timingTests2, GetIter) {
  ASSERT_EQ (sizeof(MyStruct::x)/sizeof(MyStruct::x[0]), nx2);
  TFile file ("test_timing2.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTreeIterator iter ("test", &file, verbose);
  ASSERT_TRUE(iter.GetTree()) << "no tree";
  EXPECT_EQ(iter.GetEntries(), nfill2);
  Int_t nbranches = ShowBranches (file, iter.GetTree(), branch_type2);
  EXPECT_EQ(nbranches, 1);

  StartTimer timer (iter.GetTree(), nx2);
  double v = vinit, vsum=0.0;
  for (auto& entry : iter) {
    const MyStruct& M = entry["M"];
    for (auto& x : M.x) {
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, element %d",iter.index(),&x-&M.x[0]);
#endif
    }
  }
  double vn = double(nbranches*nfill2*nx2);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}

// ==========================================================================================
// TTree test 2: write/read a bunch of doubles in a POD class
// ==========================================================================================

TEST(timingTests2, FillAddr) {
  ASSERT_EQ (sizeof(MyStruct::x)/sizeof(MyStruct::x[0]), nx2);
  TFile file ("test_timing2.root", "recreate");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTree tree("test","");
  MyStruct M;
  StartTimer timer (&tree, true, nx2);
  tree.Branch ("M", &M, TTreeIterator::GetLeaflist<MyStruct>());
  double v = vinit;
  for (Long64_t i = 0; i < nfill2; i++) {
    for (auto& x : M.x) x = v++;
    tree.Fill();
  }
  file.Write();

  Int_t nbranches = ShowBranches (file, &tree, branch_type2, "filled");
  EXPECT_FLOAT_EQ (vinit+double(nbranches*nfill2*nx2), v);
}

TEST(timingTests2, GetAddr) {
  ASSERT_EQ (sizeof(MyStruct::x)/sizeof(MyStruct::x[0]), nx2);
  TFile file ("test_timing2.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  std::unique_ptr<TTree> tree (file.Get<TTree>("test"));
  ASSERT_TRUE(tree) << "no tree";
  EXPECT_EQ(tree->GetEntries(), nfill2);
  Int_t nbranches = ShowBranches (file, tree.get(), branch_type2);
  EXPECT_EQ(nbranches, 1);

  MyStruct M;
  StartTimer timer (tree.get(), nx2);
  tree->SetBranchAddress ("M", &M);
  double v = vinit, vsum=0.0;
  Long64_t n = tree->GetEntries();
  for (Long64_t i=0; i<n; i++) {
    tree->GetEntry(i);
    for (auto& x : M.x) {
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, element %d",i,&x-&M.x[0]);
#endif
    }
  }
  double vn = double(nbranches*nfill2*nx2);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}


// ==========================================================================================
// TTreeReader test 2: read a bunch of doubles in a POD class
// ==========================================================================================

TEST(timingTests2, GetReader) {
  TFile file ("test_timing2.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTreeReader reader ("test", &file);
  ASSERT_TRUE(reader.GetTree()) << "no tree";
  EXPECT_EQ(reader.GetEntries(), nfill2);
  Int_t nbranches = ShowBranches (file, reader.GetTree(), branch_type2);
  EXPECT_EQ(nbranches, 1);

  StartTimer timer (reader.GetTree(), nx2);
  TTreeReaderArray<Double_t> vals(reader, "M.x");

  double v = vinit, vsum=0.0;
  while (reader.Next()) {
    size_t n = vals.GetSize();
    for (size_t i=0; i<n; i++) {
      double x = vals[i];
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, element %zu",reader.GetCurrentEntry(),i);
#endif
    }
  }
  double vn = double(nbranches*nfill2*nx2);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}

// ==========================================================================================
// TTreeIterator test 3: write/read a vector of doubles
// ==========================================================================================

const std::string branch_type3 = Form("std::vector<double>(%d)",int(nx3));

TEST(timingTests3, FillIter) {
  TFile file ("test_timing3.root", "recreate");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTreeIterator iter ("test", verbose);
  StartTimer timer (iter.GetTree(), true);
  double v = vinit;
  for (auto& entry : iter.FillEntries(nfill3)) {
    std::vector<double> vx(nx3);
    for (size_t i=0; i<nx3; i++) vx[i] = v++;
    entry["vx"] = std::move(vx);
  }
  Int_t nbranches = ShowBranches (file, iter.GetTree(), branch_type3, "filled");
  EXPECT_FLOAT_EQ (vinit+double(nbranches*nfill3*nx3), v);
}

TEST(timingTests3, GetIter) {
  TFile file ("test_timing3.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTreeIterator iter ("test", &file, verbose);
  ASSERT_TRUE(iter.GetTree()) << "no tree";
  EXPECT_EQ(iter.GetEntries(), nfill3);
  Int_t nbranches = ShowBranches (file, iter.GetTree(), branch_type3);
  EXPECT_EQ(nbranches, 1);

  StartTimer timer (iter.GetTree());
  double v = vinit, vsum=0.0;
  for (auto& entry : iter) {
    const std::vector<double>& vx = entry["vx"];
    EXPECT_EQ (vx.size(), nx3);
    for (auto& x : vx) {
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, element %d",iter.index(),&x-vx.data());
#endif
    }
  }
  double vn = double(nbranches*nfill3*nx3);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}

// ==========================================================================================
// TTree test 3: write/read a vector of doubles
// ==========================================================================================


TEST(timingTests3, FillAddr) {
  TFile file ("test_timing3.root", "recreate");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTree tree("test","");
  std::vector<double>* vx = 0;
  StartTimer timer (&tree, true);
  tree.Branch ("vx", &vx);
  double v = vinit;
  for (Long64_t i = 0; i < nfill3; i++) {
    ASSERT_TRUE(vx);
    vx->clear();
    for (size_t i=0; i<nx3; i++) vx->push_back(v++);
    tree.Fill();
  }
  file.Write();

  Int_t nbranches = ShowBranches (file, &tree, branch_type3, "filled");
  EXPECT_FLOAT_EQ (vinit+double(nbranches*nfill3*nx3), v);
}

TEST(timingTests3, GetAddr) {
  TFile file ("test_timing3.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  std::unique_ptr<TTree> tree (file.Get<TTree>("test"));
  ASSERT_TRUE(tree) << "no tree";
  EXPECT_EQ(tree->GetEntries(), nfill3);
  Int_t nbranches = ShowBranches (file, tree.get(), branch_type3);
  EXPECT_EQ(nbranches, 1);

  std::vector<double>* vx = 0;
  StartTimer timer (tree.get());
  tree->SetBranchAddress ("vx", &vx);
  double v = vinit, vsum=0.0;
  Long64_t n = tree->GetEntries();
  for (Long64_t i=0; i<n; i++) {
    tree->GetEntry(i);
    EXPECT_EQ (vx->size(), nx3);
    for (auto& x : *vx) {
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, element %d",i,&x-vx->data());
#endif
    }
  }
  double vn = double(nbranches*nfill3*nx3);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}

// ==========================================================================================
// TTreeReader test 3: read a vector of doubles
// ==========================================================================================

TEST(timingTests3, GetReader) {
  TFile file ("test_timing3.root");
  ASSERT_FALSE(file.IsZombie()) << "no file";

  TTreeReader reader ("test", &file);
  ASSERT_TRUE(reader.GetTree()) << "no tree";
  EXPECT_EQ(reader.GetEntries(), nfill3);
  Int_t nbranches = ShowBranches (file, reader.GetTree(), branch_type3);
  EXPECT_EQ(nbranches, 1);

  StartTimer timer (reader.GetTree());
  TTreeReaderArray<Double_t> vals(reader, "vx");

  double v = vinit, vsum=0.0;
  while (reader.Next()) {
    size_t n = vals.GetSize();
    for (size_t i=0; i<n; i++) {
      double x = vals[i];
      vsum += x;
#ifndef FAST_CHECKS
      EXPECT_EQ (x, v++) << Form("entry %lld, element %zu",reader.GetCurrentEntry(),i);
#endif
    }
  }
  double vn = double(nbranches*nfill3*nx3);
  EXPECT_FLOAT_EQ (0.5*vn*(vn+2*vinit-1), vsum);
}
