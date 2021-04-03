// A TTreeIterator allows iterator and member access to the elements of a TTree.
// Created by Tim Adye on 26/03/2021.

#pragma once

#include <string>
#include <iterator>
#include <limits>

#include "TDirectory.h"
#include "TTree.h"
#include "TError.h"

class TTreeIterator : public TNamed {
public:

  // Interface to std::iterator to allow range-based for loop
  class iterator
    : public std::iterator< std::forward_iterator_tag, // iterator_category   [could easily be random_access_iterator if we implemented the operators]
                            TTreeIterator,             // value_type
                            Long64_t,                  // difference_type
                            const TTreeIterator*,      // pointer
                            const TTreeIterator& >     // reference
  {
  public:
    iterator (TTreeIterator& entry, Long64_t first, Long64_t last) : fEntry(entry), fIndex(first), fEnd(last) {}
//  iterator (const iterator& in) : fEntry(in.fEntry), fIndex(in.fIndex), fEnd(in.fEnd) {}  // default probably OK
    iterator& operator++() { ++fIndex; return *this; }
    iterator operator++(int) { iterator it = *this; ++fIndex; return it; }
    bool operator!= (const iterator& other) const { return fIndex != other.fIndex; }
    bool operator== (const iterator& other) const { return fIndex == other.fIndex; }
    const TTreeIterator& operator*() const { return fIndex < fEnd ? fEntry.setIndex(fIndex).GetEntry() : fEntry; }
  private:
    Long64_t fIndex;
    const Long64_t fEnd;
    TTreeIterator& fEntry;   // TTreeIterator also handles the entry
  };
  typedef iterator iterator_t;


  // Wrapper class to provide return-type deduction
  class Getter {
  public:
    Getter(const TTreeIterator& entry, const char* name) : fEntry(entry), fName(name) {}
    template <typename T>          operator T() const       { return fEntry.Get<T>(fName);      }
//  template <typename T> T operator+ (const T& v) const { return T(*this) +  v; }
//  template <typename T> T operator+=(const T& v)       { return T(*this) += v; }
  private:
    const TTreeIterator& fEntry;
    const char* fName;
  };


  class Setter {
  public:
    Setter(      TTreeIterator& entry, const char* name) : fEntry(entry), fName(name) {}
    template <typename T>          operator T() const       { return fEntry.Get<T>(fName);      }
    template <typename T> const T& operator= (const T& val) { return fEntry.Set<T>(fName, val); }
  private:
    TTreeIterator& fEntry;
    const char* fName;
  };

  struct BranchInfo {
    TBranch* branch;
    size_t type_hash;
    bool isobj, disabled;
    void  Enable     (int verbose=0) { if ( (disabled = branch->TestBit(kDoNotProcess))) SetBranchStatus ( true, verbose); }
    void  EnableReset(int verbose=0) { if (  disabled)                                   SetBranchStatus (false, verbose); }
    void Disable     (int verbose=0) { if (!(disabled = branch->TestBit(kDoNotProcess))) SetBranchStatus (false, verbose); }
    void DisableReset(int verbose=0) { if (! disabled)                                   SetBranchStatus ( true, verbose); }
    void SetBranchStatus (bool status=true, int verbose=0) { TTreeIterator::SetBranchStatus (branch, status, verbose); }
  };


  // Constructors and destructors
  TTreeIterator (const char* name="", int verbose=0)
    : TNamed(name, ""),
      fTree(new TTree(name,"")),
      fIndex(0),
      fVerbose(verbose) {}
  TTreeIterator (TTree* tree, int verbose=0)
    : TNamed(tree ? tree->GetName() : "", tree ? tree->GetTitle() : ""),
      fTree(tree),
      fIndex(tree ? tree->GetEntries() : 0),
      fVerbose(verbose) {
    if (fIndex > 0) SetBranchStatusAll(false);
  }
  TTreeIterator (const char* keyname, TDirectory* dir, int verbose=0)
    : TNamed(keyname, ""),
      fTree(nullptr),
      fIndex(0),
      fVerbose(verbose)
  {
    if (!dir) dir = gDirectory;
    dir->GetObject(keyname, fTree);
    if (!fTree)
      Error("TTreeIterator", "TTree '%s' not found in file.", keyname);
    else {
      SetTitle(fTree->GetTitle());
      fIndex = fTree->GetEntries();
      SetBranchStatusAll(false);
    }
  }
//~TTreeIterator() override {}   // default probably OK


  // Access to underlying tree
  TTree* operator->() const { return GetTree(); }
  TTree* GetTree() const { return fTree; }

  // Forwards to TTree
  TTreeIterator& GetEntry(Long64_t entry) {
    return setIndex(entry).GetEntry();
  }
  void Print(Option_t* opt = "") const override {
    if (fTree) fTree->Print(opt);
    else              Print(opt);
  }
  void Browse(TBrowser* b) override {
    if (fTree) fTree->Browse(b);
  }

  std::string ActiveBranchNames() {
    std::string allbranches;
    ActiveBranchNames (fTree->GetListOfBranches(), allbranches);
    return allbranches;
  }

  static void ActiveBranchNames (TObjArray* list, std::string& allbranches, const char* pre="") {
    if (!list) return;
    Int_t nbranches = list->GetEntriesFast();
    for (Int_t i = 0; i < nbranches; ++i) {
      if (TBranch* branch = dynamic_cast<TBranch*>(list->UncheckedAt(i))) {
        if (!branch->TestBit(kDoNotProcess)) {
          if (allbranches.size()) allbranches += ", ";
          allbranches += pre;
          allbranches += branch->GetName();
        }
        std::string newpre = pre;
        newpre += branch->GetName();
        newpre += ".";
        ActiveBranchNames (branch->GetListOfBranches(), allbranches, newpre.c_str());
      }
    }
  }


  // Forwards to TTree with some extra
  virtual TTreeIterator& GetEntry() {
    if (fIndex < 0) return *this;
    if (!fTree) {
      if (fVerbose >= 0) Error ("GetEntry", "no tree available");
      return *this;
    }
    for (auto& ibranch : fBranches) {
      ibranch.second.Disable(fVerbose);  // don't need GetEntry to read our variables
    }
    if (fVerbose >= 1) {
      std::string allbranches = ActiveBranchNames();
      if (allbranches.size()) Info ("GetEntry", "entry %lld read active branches: %s", fIndex, allbranches.c_str());
    }

    Int_t nbytes = fTree->GetEntry(fIndex);
    if (nbytes >= 0) {
      if (fVerbose >= 1) Info  ("GetEntry", "read %d bytes from entry %lld", nbytes, fIndex);
    } else {
      if (fVerbose >= 0) Error ("GetEntry", "problem reading entry %lld", fIndex);
    }

    for (auto& ibranch : fBranches) {
      ibranch.second.DisableReset(fVerbose);
    }
    return *this;
  }


  virtual Int_t Fill() {
    if (!fTree) return 0;
    for (auto& ibranch : fBranches) {
      ibranch.second.Disable(fVerbose);             // don't let Fill do anything with our branch
    }
    if (fVerbose >= 1) {
      std::string allbranches = ActiveBranchNames();
      if (allbranches.size()) Info ("Fill", "entry %lld fill active branches: %s", fIndex, allbranches.c_str());
    }

    Int_t nbytes = fTree->Fill();           // fill any other branches and do autosave
    if (fVerbose >= 1) Info ("Fill", "Filled %d bytes for entry %lld", nbytes, fIndex);

    for (auto& ibranch : fBranches) {
      ibranch.second.DisableReset(fVerbose);
    }
    fIndex++;
    return nbytes;
  }

  // Accessors
  TTreeIterator& setIndex   (Long64_t index) { fIndex   = index;   return *this; }
  TTreeIterator& setVerbose (int    verbose) { fVerbose = verbose; return *this; }
  Long64_t index()   const { return fIndex;   }
  int      verbose() const { return fVerbose; }
  virtual void reset() { fIndex = 0; }

  // std::iterator interface
  iterator begin() {
    Long64_t last = fTree ? fTree->GetEntries() : 0;
    return iterator (*this, 0,    last);
  }

  iterator end()   {
    Long64_t last = fTree ? fTree->GetEntries() : 0;
    return iterator (*this, last, last);
  }


  // Access to the current entry
  Getter Get        (const char* name) const { return Getter(*this,name); }
  Getter operator[] (const char* name) const { return Getter(*this,name); }
  Setter operator[] (const char* name)       { return Setter(*this,name); }   // Setter can also do Get for non-const this


  template <typename T>
  const T& Set(const char* name, const T& val, const char* leaflist=0, Int_t bufsize=32000, Int_t splitlevel=99)
  {
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "no tree available");
      return val;
    }
    T* pval = const_cast<T*>(&val);  // keep in scope until Filled

    TBranch* branch = nullptr;
    BranchInfo* ibranch = GetBranch<T> (name);
    if (!ibranch) {
      if (fIndex <= 0) {
        ibranch = NewBranch<T> (name, pval, leaflist, bufsize, splitlevel);
        branch = ibranch->branch;
        FillBranch<T> (branch, name);
        branch->ResetAddress();
        return val;
      }
      T def = type_default<T>();  // keep in scope until Filled
      T* pdef = &def;             // keep in scope until Filled
      ibranch = NewBranch<T> (name, pdef, leaflist, bufsize, splitlevel);
      branch = ibranch->branch;
      if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, fIndex);
      for (Long64_t i = 0; i < fIndex; i++) {
        FillBranch<T> (branch, name, 1);
      }

    } else {

      branch = ibranch->branch;
      Long64_t n = branch->GetEntries();
      if (n == fIndex) {
        // go straight to SetBranch()
      } else if (n < fIndex) {
        T def = type_default<T>();      // keep in scope until Filled
        T* pdef = &def;                 // keep in scope until Filled
        T** ppdef = &pdef;              // keep in scope until Filled
        SetBranch (ibranch, name, ppdef);
        if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, fIndex-n);
        for (Long64_t i = n; i < fIndex; i++) {
          FillBranch<T> (branch, name, 1);
        }
      } else if (n == fIndex+1) {
        if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' skip filling entry %lld - already filled", name, fIndex);
        ibranch->EnableReset(fVerbose);
        return val;
      } else {   // if (n > fIndex+1)
        if (fVerbose >= 0) Error (tname<T>("Set"), "branch '%s' entry %lld is already %lld ahead of the rest of the tree", name, fIndex, n-(fIndex+1));
        ibranch->EnableReset(fVerbose);
        return val;
      }
    }

    T** ppval = &pval;   // keep in scope until Filled
    SetBranch (ibranch, name, ppval);
    FillBranch<T> (branch, name);
    branch->ResetAddress();
    ibranch->EnableReset(fVerbose);
    return val;
  }

  template <typename T>
  T Get(const char* name, T val=type_default<T>()) const {
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "no tree available");
      return val;
    }
    BranchInfo* ibranch = GetBranch<T> (name);
    if (!ibranch) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "branch '%s' not found", name);
      return val;
    }
    TBranch* branch = ibranch->branch;
    T* pval = &val;                  // keep in scope until GetEntry()
    T** ppval = &pval;   // keep in scope until GetEntry()
    if (!SetBranch (ibranch, name, ppval)) {
      ibranch->EnableReset(fVerbose);
      return val;
    }
    Int_t nread = branch->GetEntry (fIndex);
    branch->ResetAddress();
    if (nread < 0) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "GetEntry failed for branch '%s', entry %lld", name, fIndex);
    } else if (nread == 0) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, fIndex);
    } else {
      if (fVerbose >= 1) Info (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, fIndex);
    }
    ibranch->EnableReset(fVerbose);
    return val;
  }


  // Set the status for a branch and all its sub-branches.
  void SetBranchStatusAll (bool status=true) {
    SetBranchStatus (fTree->GetListOfBranches(), status, fVerbose);
  }

  static void SetBranchStatus (TBranch* branch, bool status=true, int verbose=0, const char* pre="") {
    if (!branch) return;
    if (verbose>=2) ::Info ("SetBranchStatus", "%s branch '%s%s'", status?"Enable":"Disable", pre, branch->GetName());
    if (status) branch->ResetBit(kDoNotProcess);
    else        branch->  SetBit(kDoNotProcess);
    if (verbose>=2) {
      std::string newpre = pre;
      newpre += branch->GetName();
      newpre += ".";
      SetBranchStatus (branch->GetListOfBranches(), status, verbose, newpre.c_str());
    } else {
      SetBranchStatus (branch->GetListOfBranches(), status, verbose);
    }
  }

  static void SetBranchStatus (TObjArray* list, bool status=true, int verbose=0, const char* pre="") {
    if (!list) return;
    Int_t nbranches = list->GetEntriesFast();
    for (Int_t i = 0; i < nbranches; ++i) {
      SetBranchStatus (dynamic_cast<TBranch*>(list->UncheckedAt(i)), status, verbose, pre);
    }
  }

  // Convenience function to return the type name
  template <typename T>
  static const char* tname(const char* name=0) {
    TClass* cl = TClass::GetClass<T>();
    const char* cname = cl ? cl->GetName() : TDataType::GetTypeName (TDataType::GetType(typeid(T)));
    if (!cname || !*cname) return name ? name : "";
    if (!name  || !*name)  return cname;
    static std::string ret;  // keep here so the c_str() is still valid at the end (until the next call).
    ret.clear();
    ret.reserve(strlen(name)+strlen(cname)+3);
    ret = name;
    ret += "<";
    ret += cname;
    ret += ">";
    return ret.c_str();
  }

protected:
  template<typename T> static T type_default() { return T(); }

  template <typename T>
  BranchInfo* GetBranch (const char* name) const {
    BranchInfo* ibranch = nullptr;
    auto it = fBranches.find(name);
    if (it != fBranches.end()) {
      ibranch = &it->second;
      ibranch->Enable(fVerbose);
    } else if (TBranch* branch = fTree->GetBranch(name)) {
      ibranch = SaveBranch<T> (name, branch);
      ibranch->Enable(fVerbose);
    } else {
      return nullptr;
    }
    return ibranch;
  }

  template <typename T>
  BranchInfo* NewBranch (const char* name, T* pval, const char* leaflist=0, Int_t bufsize=32000, Int_t splitlevel=99) {
    TBranch* branch;
    if (leaflist && *leaflist) {
      branch = fTree->Branch (name, pval, leaflist, bufsize);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s' with leaves '%s'", name, leaflist);
        return nullptr;
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "create branch '%s' with leaves '%s'", name, leaflist);
    } else {
      branch = fTree->Branch (name, pval, bufsize, splitlevel);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s'", name);
        return nullptr;
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "create branch '%s'", name);
    }
    return SaveBranch<T> (name, branch, true);
  }

  template <typename T>
  BranchInfo* SaveBranch (const char* name, TBranch* branch, bool checked=false) const {
#ifdef FAST_ISOBJ
    bool isobj = (branch->IsA() == TBranch::Class());
#else
    TClass* expectedClass = 0;
    EDataType expectedType = kOther_t;
    if (branch->GetExpectedType (expectedClass, expectedType)) {
      if (fVerbose >= 0) Error (tname<T>("SaveBranch"), "GetExpectedType failed for branch '%s'", name);
      return nullptr;
    }
    bool isobj = (expectedClass != nullptr);
#endif
    size_t type_hash = checked ? typeid(T).hash_code() : 0;
    return &(fBranches[name] = BranchInfo {branch, type_hash, isobj, false});
  }

  template <typename T>
  bool SetBranch (BranchInfo* ibranch, const char* name, T** ppval) const {
    bool isobj = ibranch->isobj;
    if (ibranch->type_hash && ibranch->type_hash == typeid(T).hash_code()) {
      // type check already done for T
      TBranch* branch = ibranch->branch;
      if (isobj) {
        branch->SetAddress ( ppval);
      } else {
        branch->SetAddress (*ppval);
      }
      if (fVerbose >= 2) {
        Info (tname<T>("SetBranch"), "set branch '%s' %s address", name, (isobj?"object":"variable"));
      }
    } else {
      Int_t stat=0;
      TBranch* branch = nullptr;
      if (isobj) {
        stat = fTree->SetBranchAddress (name,  ppval, &branch);
      } else {
        stat = fTree->SetBranchAddress (name, *ppval, &branch);
      }
      if (stat < 0) {
        if (fVerbose >= 0) Error (tname<T>("SetBranch"), "failed to set branch '%s' %s address", name, (isobj?"object":"variable"));
        return false;
      }
      if (branch == ibranch->branch) {
        ibranch->type_hash = typeid(T).hash_code();
        if (fVerbose >= 1) {
          Info (tname<T>("SetBranch"), "set branch '%s' %s address (saved)", name, (isobj?"object":"variable"));
        }
      } else {
        if (fVerbose >= 1) {
          Info (tname<T>("SetBranch"), "set branch '%s' %s address (not saved)", name, (isobj?"object":"variable"));
        }
      }
    }
    return true;
  }

  template <typename T>
  Int_t FillBranch (TBranch* branch, const char* name, int quiet=0) {
    Int_t nbytes = branch->Fill();
    if (nbytes > 0) {
      if (fVerbose-quiet >= 1) Info (tname<T>("Set"), "filled branch '%s' with %d bytes for entry %lld", name, nbytes, fIndex);
    } else if (nbytes == 0) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "no data filled in branch '%s' for entry %lld", name, fIndex);
    } else {
      if (fVerbose >= 0) Error (tname<T>("Set"), "error filling branch '%s' for entry %lld", name, fIndex);
    }
    return nbytes;
  }


  // Member variables
  Long64_t fIndex;
  mutable std::map<std::string,BranchInfo> fBranches;
  TTree* fTree;
  int fVerbose;

  ClassDefOverride(TTreeIterator,0)
};

template<> inline float         TTreeIterator::type_default() { return std::numeric_limits<float      >::quiet_NaN(); }
template<> inline double        TTreeIterator::type_default() { return std::numeric_limits<double     >::quiet_NaN(); }
template<> inline long double   TTreeIterator::type_default() { return std::numeric_limits<long double>::quiet_NaN(); }
template<> inline char          TTreeIterator::type_default() { return '#'; }
template<> inline int           TTreeIterator::type_default() { return -1;  }
template<> inline long int      TTreeIterator::type_default() { return -1;  }
template<> inline long long int TTreeIterator::type_default() { return -1;  }
