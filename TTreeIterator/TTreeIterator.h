// A TTreeIterator allows iterator and member access to the elements of a TTree.
// Created by Tim Adye on 26/03/2021.

#pragma once

#include <string>
#include <iterator>
#include <limits>
#include <utility>
#include <any>

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
    iterator (TTreeIterator& entry, Long64_t first, Long64_t last, bool quiet=false) : fEntry(entry), fIndex(first), fEnd(last) {
      if (!quiet && fEntry.fVerbose >= 1 && fEntry.fTree && fEntry.fTree->GetDirectory() && fEnd-fIndex>0)
        ::Info ("TTreeIterator", "get %lld entries from tree '%s' in file %s", fEnd-fIndex, fEntry.fTree->GetName(), fEntry.fTree->GetDirectory()->GetName());
    }
//  iterator (const iterator& in) : fEntry(in.fEntry), fIndex(in.fIndex), fEnd(in.fEnd) {}  // default probably OK
    iterator& operator++() { ++fIndex; return *this; }
    iterator operator++(int) { iterator it = *this; ++fIndex; return it; }
    bool operator!= (const iterator& other) const { return fIndex != other.fIndex; }
    bool operator== (const iterator& other) const { return fIndex == other.fIndex; }
    const TTreeIterator& operator*() const { return fIndex < fEnd ? fEntry.setIndex(fIndex).GetEntry() : fEntry; }
    iterator begin () { return iterator (fEntry, fIndex, fEnd); }
    iterator end ()   { return iterator (fEntry, fEnd,   fEnd); }
    Long64_t first()  { return fIndex; }
    Long64_t last()   { return fEnd;   }
  protected:
    Long64_t fIndex;
    const Long64_t fEnd;
    TTreeIterator& fEntry;   // TTreeIterator also handles the entry
  };
  typedef iterator iterator_t;

  class fill_iterator : public iterator {
  public:
    fill_iterator (TTreeIterator& entry, Long64_t first, Long64_t last, bool quiet=false) : iterator(entry,first,last,true) {
      if (!quiet && fEntry.fVerbose >= 1 && fEntry.fTree && fEntry.fTree->GetDirectory()) {
        if (fEnd < fIndex) {
          ::Info ("TTreeIterator", "fill entries into tree '%s' in file %s", fEntry.fTree->GetName(), fEntry.fTree->GetDirectory()->GetName());
        } else if (fEnd > fIndex) {
          ::Info ("TTreeIterator", "fill %lld entries into tree '%s' in file %s", fEnd-fIndex, fEntry.fTree->GetName(), fEntry.fTree->GetDirectory()->GetName());
        }
      }
    }
    ~fill_iterator() { fEntry.Write(); }
    fill_iterator& operator++() { fEntry.Fill(); fIndex++; return *this; }
    fill_iterator operator++(int) { fill_iterator it = *this; ++*this; return it; }
    TTreeIterator& operator*() const { return fEntry; }
    fill_iterator begin() { return fill_iterator (fEntry, fIndex, fEnd); }
    fill_iterator end()   { return fill_iterator (fEntry, fEnd,   fEnd); }
  };

  // Wrapper class to provide return-type deduction
  class Getter {
  public:
    Getter(const TTreeIterator& entry, const char* name) : fEntry(entry), fName(name) {}
    template <typename T> operator const T&() const { return fEntry.Get<T>(fName); }
    template <typename T> operator T&() const { return fEntry.Get<T>(fName); }
//  template <typename T> T operator+ (const T& v) const { return T(*this) +  v; }
//  template <typename T> T operator+=(const T& v)       { return T(*this) += v; }
  private:
    const TTreeIterator& fEntry;
    const char* fName;
  };


  class Setter {
  public:
    Setter(TTreeIterator& entry, const char* name) : fEntry(entry), fName(name) {}
    template <typename T> operator const T&() const { return fEntry.Get<T>(fName); }
    template <typename T> operator T&() const { return fEntry.Get<T>(fName); }
    template <typename T> const T& operator= (T&& val) { return fEntry.Set<T>(fName, std::forward<T>(val)); }
  private:
    TTreeIterator& fEntry;
    const char* fName;
  };

  struct BranchInfo {
    size_t type_hash;
    std::any value;
    void* pvalue=nullptr;
    TBranch* branch=nullptr;
    bool disabled=false;
    void  Enable     (int verbose=0) { if ( (disabled = branch->TestBit(kDoNotProcess))) SetBranchStatus ( true, verbose); }
    void  EnableReset(int verbose=0) { if (  disabled)                                   SetBranchStatus (false, verbose); }
    void Disable     (int verbose=0) { if (!(disabled = branch->TestBit(kDoNotProcess))) SetBranchStatus (false, verbose); }
    void DisableReset(int verbose=0) { if (! disabled)                                   SetBranchStatus ( true, verbose); }
    void SetBranchStatus (bool status=true, int verbose=0) { TTreeIterator::SetBranchStatus (branch, status, true, verbose); }
  };

#ifdef SHOW_BranchInfo   // and rename BranchInfo above to BranchInfo2
  struct BranchInfo : public BranchInfo2 {
    BranchInfo() { printf("BranchInfo()@%p\n",this); }
    BranchInfo(TBranch* b,size_t t,const std::any& v) : BranchInfo2{b,t,v} { printf("BranchInfo(init)@%p\n",this); }
    BranchInfo(const BranchInfo& o) : BranchInfo2{o} { printf("BranchInfo(BranchInfo())@%p\n",this); }
    BranchInfo(BranchInfo&& o) : BranchInfo2{o} { printf("BranchInfo(BranchInfo&&())@%p\n",this); }
    ~BranchInfo() { printf("~BranchInfo()@%p\n",this); }
    BranchInfo& operator=(const BranchInfo& o) { BranchInfo2::operator=(o); printf("BranchInfo = BranchInfo()@%p\n",this); return *this; }
    BranchInfo& operator=(      BranchInfo&&o) { BranchInfo2::operator=(o); printf("BranchInfo = BranchInfo&&()@%p\n",this); return *this; }
  };
#endif

  // Constructors and destructors
  TTreeIterator (const char* name="", int verbose=0)
    : TNamed(name, ""),
      fTree(new TTree(name,"")),
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
  TTreeIterator& GetEntry (Long64_t entry) {
    return setIndex(entry).GetEntry();
  }
  void Print (Option_t* opt="") const override {
    if (fTree) fTree->Print(opt);
    else              Print(opt);
  }
  void Browse (TBrowser* b) override {
    if (fTree) fTree->Browse(b);
    else              Browse(b);
  }
  Long64_t GetEntries () const { return fTree ? fTree->GetEntries() : 0; }
  void  SetBufsize    (Int_t bufsize)    { fBufsize    = bufsize;    }
  void  SetSplitlevel (Int_t splitlevel) { fSplitlevel = splitlevel; }
  Int_t GetBufsize()    const            { return       fBufsize;    }
  Int_t GetSplitlevel() const            { return       fSplitlevel; }


  std::string BranchNamesString (bool include_children=true, bool include_inactive=false) {
    std::string str;
    auto allbranches = BranchNames (include_children, include_inactive);
    for (auto& name : allbranches) {
      if (!str.empty()) str += ", ";
      str += name;
    }
    return str;
  }

  std::vector<std::string> BranchNames (bool include_children=false, bool include_inactive=false) {
    std::vector<std::string> allbranches;
    BranchNames (allbranches, fTree->GetListOfBranches(), include_children, include_inactive);
    return allbranches;
  }

  // Forwards to TTree with some extra
  virtual TTreeIterator& GetEntry() {
    if (fIndex < 0) return *this;
    if (!fTree) {
      if (fVerbose >= 0) Error ("GetEntry", "no tree available");
      return *this;
    }

    Int_t nbytes = fTree->GetEntry(fIndex);
    if (nbytes >= 0) {
      if (fVerbose >= 1) {
        std::string allbranches = BranchNamesString();
        Info  ("GetEntry", "read %d bytes from entry %lld for branches: %s", nbytes, fIndex, allbranches.c_str());
      }
    } else {
      if (fVerbose >= 0) {
        std::string allbranches = BranchNamesString();
        Error ("GetEntry", "problem reading entry %lld for branches: %s", fIndex, allbranches.c_str());
      }
    }
    return *this;
  }


  virtual Int_t Fill() {
    if (!fTree) return 0;

    Int_t nbytes = fTree->Fill();

    if (nbytes >= 0) {
      if (fVerbose >= 1) {
        std::string allbranches = BranchNamesString();
        Info  ("Fill", "Filled %d bytes for entry %lld, branches: %s", nbytes, fIndex, allbranches.c_str());
      }
    } else {
      if (fVerbose >= 0) {
        std::string allbranches = BranchNamesString();
        Error ("Fill", "problem writing entry %lld for branches: %s", fIndex, allbranches.c_str());
      }
    }

    if (nbytes > 0) fWriting = true;

    fIndex++;
    return nbytes;
  }

  Int_t Write (const char *name=0, Int_t option=0, Int_t bufsize=0) override {
    if (!(fWriting && fTree && fTree->GetDirectory())) return 0;
    Int_t nbytes = fTree->Write (name, option, bufsize);
    if (fVerbose >= 1) Info ("Write", "wrote %d bytes to file %s", nbytes, fTree->GetDirectory()->GetName());
    fWriting = false;
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

  fill_iterator FillEntries (Long64_t nfill=-1) {
    if (!fTree) return fill_iterator (*this,0,0);
    Long64_t np = fTree->GetEntries();
    return fill_iterator (*this, np, nfill>=0 ? np+nfill : -1, true);
  }

  // Create empty branch
  template <typename T>
  TBranch* Branch (const char* name) {
    return Branch<T> (name, GetLeaflist<T>(), fBufsize, fSplitlevel);
  }

  template <typename T>
  TBranch* Branch (const char* name, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Branch"), "no tree available");
      return nullptr;
    }
    using V = typename std::remove_reference<T>::type;
    V def = type_default<V>();
    BranchInfo* ibranch = NewBranch<T> (name, std::forward<T>(def), leaflist, bufsize, splitlevel);
    return ibranch->branch;
  }

  // Access to the current entry
  Getter Get        (const char* name) const { return Getter(*this,name); }
  Getter operator[] (const char* name) const { return Getter(*this,name); }
  Setter operator[] (const char* name)       { return Setter(*this,name); }   // Setter can also do Get for non-const this


  template <typename T>
  const T& Set(const char* name, T&& val) {
    return Set<T> (name, std::forward<T>(val), GetLeaflist<T>(), fBufsize, fSplitlevel);
  }

  template <typename T>
  const T& Set(const char* name, T&& val, const char* leaflist) {
    return Set<T> (name, std::forward<T>(val), leaflist, fBufsize, fSplitlevel);
  }

  template <typename T>
  const T& Set(const char* name, T&& val, const char* leaflist, Int_t bufsize) {
    return Set<T> (name, std::forward<T>(val), leaflist, bufsize, fSplitlevel);
  }

  template <typename T>
  const T& Set(const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel)
  {
    TBranch* branch = nullptr;
    BranchInfo* ibranch = GetBranch<T> (name);
    if (!ibranch) {
      if (fIndex <= 0) {
        ibranch = NewBranch<T> (name, std::forward<T>(val), leaflist, bufsize, splitlevel);
        return std::any_cast<T&>(ibranch->value);
      } else {
        using V = typename std::remove_reference<T>::type;
        V def = type_default<V>();
        ibranch = NewBranch<T> (name, std::forward<T>(def), leaflist, bufsize, splitlevel);
        if (!ibranch->branch) return std::any_cast<T&>(ibranch->value);
        branch = ibranch->branch;
        if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, fIndex);
        for (Long64_t i = 0; i < fIndex; i++) {
          FillBranch<T> (branch, name, 1);
        }
      }
    }
    return ibranch->value.emplace<T>(std::forward<T>(val));
  }

  // Get value, returning a reference
  template <typename T>
  T& Get(const char* name) const {
    static T def {type_default<T>()};   // static default value for each type to allow us to return by reference
    BranchInfo* ibranch = GetImpl<T>(name);
    return ibranch ? std::any_cast<T&>(ibranch->value) : def;
  }

  // Get() allowing the default value (returned if there is an error) to be specified.
  template <typename T>
  T& Get(const char* name, T& val) const {
    BranchInfo* ibranch = GetImpl<T>(name);
    return ibranch ? std::any_cast<T&>(ibranch->value) : val;
  }

  template <typename T>
  const T& Get(const char* name, const T& val) const {
    BranchInfo* ibranch = GetImpl<T>(name);
    return ibranch ? std::any_cast<T&>(ibranch->value) : val;
  }

  // Set the status for a branch and all its sub-branches.
  void SetBranchStatusAll (bool status=true, bool include_children=true) {
    SetBranchStatus (fTree->GetListOfBranches(), status, include_children, fVerbose);
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

  template<typename T> static T           type_default() { return T();                   }
  template<typename T> static const char* GetLeaflist()  { return GetLeaflistImpl<T>(0); }

private:
  template<typename T> static decltype(T::leaflist) GetLeaflistImpl(int)  { return T::leaflist; }
  template<typename T> static const char*           GetLeaflistImpl(long) { return nullptr;     }

protected:

  template <typename T>
  BranchInfo* GetImpl(const char* name) const {
    BranchInfo* ibranch = GetBranch<T> (name);
    if (ibranch) return ibranch;
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "no tree available");
      return nullptr;
    } else if (TBranch* branch = fTree->GetBranch(name)) {
      ibranch = SaveBranch<T> (name, type_default<T>());
      ibranch->branch = branch;
      if (!SetBranch<T> (ibranch, name)) return nullptr;
      Int_t nread = branch->GetEntry (fIndex);
      if (nread < 0) {
        if (fVerbose >= 0) Error (tname<T>("Get"), "GetEntry failed for branch '%s', entry %lld", name, fIndex);
      } else if (nread == 0) {
        if (fVerbose >= 0) Error (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, fIndex);
      } else {
        if (fVerbose >= 1) Info (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, fIndex);
        return ibranch;
      }
    } else {
      if (fVerbose >= 0) Error (tname<T>("Get"), "branch '%s' not found", name);
    }
    return nullptr;
  }

  template <typename T>
  BranchInfo* GetBranch (const char* name) const {
    auto it = fBranches.find(name);
    if (it != fBranches.end()) {
      BranchInfo* ibranch = &it->second;
      if (ibranch->type_hash && ibranch->type_hash == typeid(T).hash_code()) {
        if (fVerbose >= 2) Info (tname<T>("GetBranch"), "found branch '%s' of type '%s' @%p", name, ibranch->value.type().name(), &ibranch->value);
        return ibranch;
      } else {
        if (fVerbose >= 1) Info (tname<T>("GetBranch"), "found branch '%s' of type '%s' @%p, but doesn't match requested type '%s'", name, ibranch->value.type().name(), &ibranch->value, typeid(T).name());
      }
    }
    return nullptr;
  }

  template <typename T>
  BranchInfo* NewBranch (const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
    BranchInfo* ibranch = SaveBranch<T> (name, std::forward<T>(val));
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "no tree available");
      return ibranch;
    }
    TBranch* branch;
    using V = typename std::remove_reference<T>::type;
    if (leaflist && *leaflist) {
      branch = fTree->Branch (name, std::any_cast<V>(&ibranch->value), leaflist, bufsize);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s' with leaves '%s'", name, leaflist);
        return ibranch;
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "create branch '%s' with leaves '%s' of type '%s' @%p", name, leaflist, ibranch->value.type().name(), &ibranch->value);
    } else {
      branch = fTree->Branch (name, std::any_cast<V>(&ibranch->value), bufsize, splitlevel);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s'", name);
        return ibranch;
      }
      if (fVerbose >= 1) Info (tname<T>("Set"), "create branch '%s' of type '%s' @%p", name, ibranch->value.type().name(), &ibranch->value);
    }
    ibranch->branch = branch;
    fWriting = true;
    return ibranch;
  }

  template <typename T>
  BranchInfo* SaveBranch (const char* name, T&& val) const {
    return &(fBranches[name] = BranchInfo {typeid(T).hash_code(), std::forward<T>(val)});
  }

  template <typename T>
  bool SetBranch (BranchInfo* ibranch, const char* name, const char* call="Get") const {
    Int_t stat=0;
    ibranch->Enable(fVerbose);
    TBranch* branch = ibranch->branch;
    bool isobj = false;
    if (TClass::GetClass<T>() && branch->GetMother() == branch) {
      TClass* expectedClass = 0;
      EDataType expectedType = kOther_t;
      if (branch->GetExpectedType (expectedClass, expectedType)) {
        if (fVerbose >= 1) Info (tname<T>("SetBranch"), "GetExpectedType failed for branch '%s'", name);
      } else {
        if (expectedClass) isobj = true;
      }
    }
    if (isobj) {
      ibranch->pvalue = std::any_cast<T>(&ibranch->value);
      stat = fTree->SetBranchAddress (name,              (T**)(&ibranch->pvalue));
    } else {
      stat = fTree->SetBranchAddress (name, std::any_cast<T  >(&ibranch-> value));
    }
    if (stat < 0) {
      if (fVerbose >= 0) Error (tname<T>(call), "failed to set branch '%s' %s address", name, (isobj?"object":"variable"));
      ibranch->EnableReset(fVerbose);
      return false;
    }
    if   (fVerbose >= 1) Info  (tname<T>(call), "set branch '%s' %s address",           name, (isobj?"object":"variable"));
    return true;
  }

  template <typename T>
  Int_t FillBranch (TBranch* branch, const char* name, int quiet=0) {
    Int_t nbytes = branch->Fill();
    if (nbytes > 0) {
      fWriting = true;
      if (fVerbose-quiet >= 1) Info (tname<T>("Set"), "filled branch '%s' with %d bytes for entry %lld", name, nbytes, fIndex);
    } else if (nbytes == 0) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "no data filled in branch '%s' for entry %lld", name, fIndex);
    } else {
      if (fVerbose >= 0) Error (tname<T>("Set"), "error filling branch '%s' for entry %lld", name, fIndex);
    }
    return nbytes;
  }

  static void SetBranchStatus (TObjArray* list, bool status=true, bool include_children=true, int verbose=0, const std::string* pre=nullptr) {
    if (!list) return;
    Int_t nbranches = list->GetEntriesFast();
    for (Int_t i = 0; i < nbranches; ++i) {
      SetBranchStatus (dynamic_cast<TBranch*>(list->UncheckedAt(i)), status, include_children, verbose, pre);
    }
  }

  static void SetBranchStatus (TBranch* branch, bool status=true, bool include_children=true, int verbose=0, const std::string* pre=nullptr) {
    if (!branch) return;
    if (verbose>=2) ::Info ("SetBranchStatus", "%s branch '%s%s'", status?"Enable":"Disable", pre?pre->c_str():"", branch->GetName());
    if (status) branch->ResetBit(kDoNotProcess);
    else        branch->  SetBit(kDoNotProcess);
    if (!include_children) return;
    if (verbose >=2) {
      std::string newpre;
      if (pre) newpre += *pre;
      newpre += branch->GetName();
      newpre += ".";
      SetBranchStatus (branch->GetListOfBranches(), status, include_children, verbose, &newpre);
    } else {
      SetBranchStatus (branch->GetListOfBranches(), status, include_children, verbose);
    }
  }

  static void BranchNames (std::vector<std::string>& allbranches, TObjArray* list, bool include_children, bool include_inactive, const std::string& pre="") {
    if (!list) return;
    Int_t nbranches = list->GetEntriesFast();
    for (Int_t i = 0; i < nbranches; ++i) {
      if (TBranch* branch = dynamic_cast<TBranch*>(list->UncheckedAt(i))) {
        if (include_inactive || !branch->TestBit(kDoNotProcess)) {
          allbranches.emplace_back (pre+branch->GetName());
        }
        if (include_children) {
          std::string newpre = pre + branch->GetName();
          newpre += ".";
          BranchNames (allbranches, branch->GetListOfBranches(), include_children, include_inactive, newpre);
        }
      }
    }
  }

  // Member variables
  Long64_t fIndex=0;
  mutable std::map<std::string,BranchInfo> fBranches;
  TTree* fTree=nullptr;
  Int_t fBufsize=32000;
  Int_t fSplitlevel=99;
  int fVerbose=0;
  bool fWriting=false;

  ClassDefOverride(TTreeIterator,0)
};

template<> inline float         TTreeIterator::type_default() { return std::numeric_limits<float      >::quiet_NaN(); }
template<> inline double        TTreeIterator::type_default() { return std::numeric_limits<double     >::quiet_NaN(); }
template<> inline long double   TTreeIterator::type_default() { return std::numeric_limits<long double>::quiet_NaN(); }
template<> inline char          TTreeIterator::type_default() { return '#'; }
template<> inline int           TTreeIterator::type_default() { return -1;  }
template<> inline long int      TTreeIterator::type_default() { return -1;  }
template<> inline long long int TTreeIterator::type_default() { return -1;  }
