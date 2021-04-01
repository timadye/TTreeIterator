// A TTreeIterator allows iterator and member access to the elements of a TTree.
// Created by Tim Adye on 26/03/2021.

#pragma once

#include <string>
#include <iterator>
#include <limits>
#include <memory>
#include <list>

#include "TDirectory.h"
#include "TTree.h"
#include "TError.h"

//#define TTreeIterator_NO_TEMPORARY 1

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
#ifndef TTreeIterator_NO_TEMPORARY
    template <typename T> const T& operator= (const T& val) { return fEntry.Set<T>(fName, val); }
#else
    template <typename T> const T& operator= (      T& val) { return fEntry.Set<T>(fName, val); }
#endif
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
      fVerbose(verbose) {}
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
    if( fTree->GetEntry(fIndex) > 0) {
      if (fVerbose >= 1) Info  ("GetEntry", "Read entry %lld", fIndex);
    } else {
      if (fVerbose >= 0) Error ("GetEntry", "Problem reading entry %lld", fIndex);
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
    Int_t nbytes = fTree->Fill();
    fTree->ResetBranchAddresses();
#ifndef TTreeIterator_NO_TEMPORARY
    fSet.clear();
    fSetPtr.clear();
#endif
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
#ifndef TTreeIterator_NO_TEMPORARY
  const T& Set(const char* name, const T& val, const char* leaflist=0, Int_t bufsize=32000, Int_t splitlevel=99)
#else
  // pass val by non-const reference to prevent a temporary being used
  const T& Set(const char* name,       T& val, const char* leaflist=0, Int_t bufsize=32000, Int_t splitlevel=99)
#endif
  {
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "no tree available");
      return val;
    }
    T* pval = SavePtr (val);
    TBranch* branch = fTree->GetBranch(name);
    if (!branch) {
      if (fIndex <= 0) {
        NewBranch<T> (name, pval, leaflist, bufsize, splitlevel);
        return *pval;
      }
      branch = NewBranch<T> (name, SaveVal<T>(), leaflist, bufsize, splitlevel);
      for (Long64_t i = 0; i < fIndex; i++) {
        branch->Fill();
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, fIndex);
    } else {
      Long64_t n = branch->GetEntries();
      if (n < fIndex) {
        // simple types are filled in fTree->Fill(), so this isn't used - so we don't get the defaults.
        SetBranch<T> (branch, name, SaveVal<T>());
        for (Long64_t i = n; i < fIndex; i++) {
          branch->Fill();
        }
        if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, fIndex-n);
      }
    }
    SetBranch<T> (branch, name, pval);
    return *pval;
  }

  template <typename T>
  T Get(const char* name, T val=type_default<T>()) const {
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "no tree available");
      return val;
    }
    TBranch* branch = fTree->GetBranch(name);
    if (!branch) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "branch '%s' not found", name);
      return val;
    }
    TClass* expectedClass = 0;
    EDataType expectedType = kOther_t;
    if (branch->GetExpectedType (expectedClass, expectedType)) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "GetExpectedType failed for branch '%s'", name);
      return val;
    }
    Int_t stat=0;
    T* pval = &val;  // keep in memory until we call GetEntry()
    if (expectedClass) {
      stat = fTree->SetBranchAddress (name, &pval);
    } else {
      stat = fTree->SetBranchAddress (name, &val);
    }
    if (stat < 0) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "%s SetBranchAddress failed for branch '%s'", (expectedClass?"Object":"Type"), name);
      return val;
    }
    Int_t nread = branch->GetEntry(fIndex);
    branch->ResetAddress();
    if (nread <= 0) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "GetEntry failed for branch '%s', entry %lld", name, fIndex);
    } else {
      if (fVerbose >= 1) Info (tname<T>("Get"), "branch '%s' read from entry %lld", name, fIndex);
    }
    return val;
  }


  // Convenience function to return the type name
  template <typename T>
  static const char* tname(const char* name=0) {
    TClass* cl = TClass::GetClass<T>();
    const char* cname = cl ? cl->GetName() : TDataType::GetTypeName (TDataType::GetType(typeid(T)));
    if (!name || !*name) return cname;
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
  T* SavePtr (const T& val) {
#ifndef TTreeIterator_NO_TEMPORARY
    fSet.emplace_back (std::shared_ptr<T>(new T(val), [](void* v){ delete (T*)v; }));
    T* pval = (T*)fSet.back().get();
#else
    T* pval = &val;
#endif
    return pval;
  }

  template <typename T>
  T* SaveVal (T val=type_default<T>()) {
    return SavePtr (val);
  }

  template <typename T>
  TBranch* NewBranch (const char* name, T* pval, const char* leaflist=0, Int_t bufsize=32000, Int_t splitlevel=99) {
    TBranch* branch;
    if (leaflist && *leaflist) {
      branch = fTree->Branch (name, pval, leaflist, bufsize);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s' with leaves '%s' for entry %lld", name, leaflist, fIndex);
        return branch;
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' created with leaves '%s' for entry %lld", name, leaflist, fIndex);
    } else {
      branch = fTree->Branch (name, pval, bufsize, splitlevel);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch for '%s'", name);
        return branch;
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' created and set for entry %lld", name, fIndex);
    }
    return branch;
  }

  template <typename T>
  bool SetBranch (TBranch* branch, const char* name, T* pval) {
    TClass* expectedClass = 0;
    EDataType expectedType = kOther_t;
    if (branch->GetExpectedType (expectedClass, expectedType)) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "GetExpectedType failed for branch '%s'", name);
      return false;
    }
    Int_t stat=0;
    if (expectedClass) {
#ifndef TTreeIterator_NO_TEMPORARY
      fSetPtr.push_back (pval);
      T** ppval = (T**)&fSetPtr.back();
#else
      T** ppval = &pval;
#endif
      stat = fTree->SetBranchAddress (name, ppval);
    } else {
      stat = fTree->SetBranchAddress (name,  pval);
    }
    if (stat < 0) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "%s SetBranchAddress failed for branch '%s'", (expectedClass?"Object":"Type"), name);
      return false;
    }
    if (fVerbose >= 1) Info (tname<T>("Set"), "%s branch '%s' set for entry %lld", (expectedClass?"Object":"Type"), name, fIndex);
    return true;
  }


  // Member variables
  Long64_t fIndex;
  TTree* fTree;
  int fVerbose;
#ifndef TTreeIterator_NO_TEMPORARY
  // use list (rather than vector) to ensure objects don't change location.
  // use shared_ptr (rather than unique_ptr) since it supports type-erasure (std::any would be nicer, but only in C++17).
  std::list<std::shared_ptr<void> > fSet;
  std::list<void*> fSetPtr;
#endif

  ClassDefOverride(TTreeIterator,0)
};

template<> inline float         TTreeIterator::type_default() { return std::numeric_limits<float      >::quiet_NaN(); }
template<> inline double        TTreeIterator::type_default() { return std::numeric_limits<double     >::quiet_NaN(); }
template<> inline long double   TTreeIterator::type_default() { return std::numeric_limits<long double>::quiet_NaN(); }
template<> inline char          TTreeIterator::type_default() { return '#'; }
template<> inline int           TTreeIterator::type_default() { return -1;  }
template<> inline long int      TTreeIterator::type_default() { return -1;  }
template<> inline long long int TTreeIterator::type_default() { return -1;  }
