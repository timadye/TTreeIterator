// A TTreeIterator allows iterator and member access to the elements of a TTree.
// Created by Tim Adye on 26/03/2021.

#pragma once

#include <string>
#include <iterator>
#include <limits>
#include <utility>

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
//  fTree->SetBranchStatus("*",0);
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
//    fTree->SetBranchStatus("*",0);
    }
  }
//~TTreeIterator() override {}   // default probably OK


  // Access to underlying tree
  TTree* operator->() const { return GetTree(); }
  TTree* GetTree() const { return fTree; }

  // Forwards to TTree
  virtual TTreeIterator& GetEntry() {
    if (fIndex < 0) return *this;
    if (!fTree) {
      if (fVerbose >= 0) Error ("GetEntry", "no tree available");
      return *this;
    }
    for (auto& ibranch : fBranches) {
      ibranch.second.first->SetBit(kDoNotProcess);  // don't need GetEntry to read out variables
    }
    Int_t nbytes = fTree->GetEntry(fIndex);
    for (auto& ibranch : fBranches) {
      ibranch.second.first->ResetBit(kDoNotProcess);
    }
    if (nbytes >= 0) {
      if (fVerbose >= 1) Info  ("GetEntry", "read %d bytes from entry %lld", nbytes, fIndex);
    } else {
      if (fVerbose >= 0) Error ("GetEntry", "problem reading entry %lld", fIndex);
    }
    return *this;
  }
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
  virtual Int_t Fill() {
    if (!fTree) return 0;
    fIndex++;
    for (auto& ibranch : fBranches) {
      ibranch.second.first->SetBit(kDoNotProcess);  // don't let Fill do anything with our branch
    }
    Int_t nbytes = fTree->Fill();           // fill any other branches and do autosave
    for (auto& ibranch : fBranches) {
      ibranch.second.first->ResetBit(kDoNotProcess);
    }
    if (fVerbose >= 1) Info ("Fill", "Filled %d bytes for entry %lld", nbytes, fIndex);
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

    bool isobj = false;
    TBranch* branch = GetBranch (name, isobj);
    if (!branch) {
      if (fIndex <= 0) {
        branch = NewBranch<T> (name, pval, leaflist, bufsize, splitlevel);
        FillBranch<T> (branch, name);
        branch->ResetAddress();
//      branch->SetBit(kDoNotProcess);
        return val;
      }
      T def = type_default<T>();  // keep in scope until Filled
      T* pdef = &def;             // keep in scope until Filled
      branch = NewBranch<T> (name, pdef, leaflist, bufsize, splitlevel);
      if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, fIndex);
      for (Long64_t i = 0; i < fIndex; i++) {
        FillBranch<T> (branch, name, 1);
      }

    } else {

      Long64_t n = branch->GetEntries();
      if (n == fIndex) {
        // go straight to SetBranch()
      } else if (n < fIndex) {
        T def = type_default<T>();      // keep in scope until Filled
        T* pdef = &def;                 // keep in scope until Filled
        void** ppdef = (void**) &pdef;  // keep in scope until Filled
        SetBranch (branch, name, ppdef, isobj);
        if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, fIndex-n);
        for (Long64_t i = n; i < fIndex; i++) {
          FillBranch<T> (branch, name, 1);
        }
      } else if (n == fIndex+1) {
        if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' skip filling entry %lld - already filled", name, fIndex);
//      branch->SetBit(kDoNotProcess);
        return val;
      } else {   // if (n > fIndex+1)
        if (fVerbose >= 0) Error (tname<T>("Set"), "branch '%s' entry %lld is already %lld ahead of the rest of the tree", name, fIndex, n-(fIndex+1));
//      branch->SetBit(kDoNotProcess);
        return val;
      }
    }

    void** ppval = (void**) &pval;   // keep in scope until Filled
    SetBranch (branch, name, ppval, isobj);
    FillBranch<T> (branch, name);
    branch->ResetAddress();
//  branch->SetBit(kDoNotProcess);
    return val;
  }

  template <typename T>
  T Get(const char* name, T val=type_default<T>()) const {
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "no tree available");
      return val;
    }
    bool isobj = false;
    TBranch* branch = GetBranch (name, isobj);
    if (!branch) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "branch '%s' not found", name);
      return val;
    }
    T* pval = &val;                  // keep in scope until GetEntry()
    void** ppval = (void**) &pval;   // keep in scope until GetEntry()
    SetBranch (branch, name, ppval, isobj);
    Int_t nread = branch->GetEntry (fIndex);
    branch->ResetAddress();
//  branch->SetBit(kDoNotProcess);
    if (nread < 0) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "GetEntry failed for branch '%s', entry %lld", name, fIndex);
    } else if (nread == 0) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, fIndex);
    } else {
      if (fVerbose >= 1) Info (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, fIndex);
    }
    return val;
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
    ret.reserve(strlen(name)+strlen(cname)+2);
    ret = name;
    ret += "<";
    ret += cname;
//  ret += ">";  // Info("SUB","MSG") prints "STR>: MSG". Let's ballance <> in SUB<CLS>.
    return ret.c_str();
  }

protected:
  template<typename T> static T type_default() { return T(); }

  TBranch* GetBranch (const char* name, bool& isobj) const {
    TBranch* branch = nullptr;
    isobj = false;
    auto it = fBranches.find(name);
    if (it != fBranches.end()) {
      branch = it->second.first;
      isobj  = it->second.second;
    } else {
      branch = fTree->GetBranch(name);
      if (!branch) return branch;
      isobj = SaveBranch (name, branch);
    }
//  branch->ResetBit(kDoNotProcess);
    return branch;
  }

  template <typename T>
  TBranch* NewBranch (const char* name, T* pval, const char* leaflist=0, Int_t bufsize=32000, Int_t splitlevel=99) {
    TBranch* branch;
    if (leaflist && *leaflist) {
      branch = fTree->Branch (name, pval, leaflist, bufsize);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s' with leaves '%s'", name, leaflist);
        return branch;
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "create branch '%s' with leaves '%s'", name, leaflist);
    } else {
      branch = fTree->Branch (name, pval, bufsize, splitlevel);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s'", name);
        return branch;
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "create branch '%s'", name);
    }
    SaveBranch (name, branch);
    return branch;
  }

  bool SaveBranch (const char* name, TBranch* branch) const {
    TClass* expectedClass = 0;
    EDataType expectedType = kOther_t;
    if (branch->GetExpectedType (expectedClass, expectedType)) {
      if (fVerbose >= 0) Error ("Set", "GetExpectedType failed for branch '%s'", name);
      return false;
    }
    fBranches[name] = std::make_pair(branch,bool(expectedClass));
    return bool(expectedClass);
  }

  bool SetBranch (TBranch* branch, const char* name, void** ppval, bool isobj) const {
    branch->SetAddress (isobj ? ppval : *ppval);
    if (fVerbose >= 2) {
      Info ("Set", "set branch '%s' %s address", name, (isobj?"object":"variable"));
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
  mutable std::map<std::string,std::pair<TBranch*,bool> > fBranches;
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
