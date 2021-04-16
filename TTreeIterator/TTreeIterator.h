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

#include "TTreeIterator_detail.h"

// define these for fastest speed
//#define FEWER_CHECKS 1             // skip sanity/debug checks if on every entry
//#define OVERRIDE_BRANCH_ADDRESS 1  // override any other user SetBranchAddress settings

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
    ~iterator() { fEntry.reset(); }
    iterator& operator++() { ++fIndex; return *this; }
    iterator  operator++(int) { iterator it = *this; ++fIndex; return it; }
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
    fill_iterator (TTreeIterator& entry, Long64_t first, Long64_t last) : iterator(entry,first,last) {}
    ~fill_iterator() { fEntry.Write(); }
    fill_iterator& operator++() { fEntry.Fill(); fIndex++; return *this; }
    fill_iterator  operator++(int) { fill_iterator it = *this; ++*this; return it; }
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
  protected:
    const TTreeIterator& fEntry;
    const char* fName;
  };

  // Wrapper class to allow setting through operator[]
  class Setter : public Getter {
  public:
    Setter(TTreeIterator& entry, const char* name) : Getter(entry,name) {}
    template <typename T> const T& operator= (T&& val) { return const_cast<TTreeIterator&>(fEntry).Set<T>(fName, std::forward<T>(val)); }
  };

  // Hack to allow access to protected method TTree::CheckBranchAddressType()
  struct TTreeProtected : public TTree {
    static TTreeProtected& Access (TTree& t) { return (TTreeProtected&) t; }
    using TTree::CheckBranchAddressType;
  };

  // Our local cache of branch information
  struct BranchInfo {
    std::any value;
    void*    pvalue = nullptr;
#ifndef OVERRIDE_BRANCH_ADDRESS
    void**   puser  = nullptr;
#endif
    TBranch* branch = nullptr;
    bool     set    = false;
    bool     isobj  = false;
    bool     was_disabled = false;
    void  Enable     (int verbose=0) { if ( (was_disabled = branch->TestBit(kDoNotProcess))) SetBranchStatus ( true, verbose); }
    void  EnableReset(int verbose=0) { if (  was_disabled)                                   SetBranchStatus (false, verbose); }
    void Disable     (int verbose=0) { if (!(was_disabled = branch->TestBit(kDoNotProcess))) SetBranchStatus (false, verbose); }
    void DisableReset(int verbose=0) { if (! was_disabled)                                   SetBranchStatus ( true, verbose); }
    void SetBranchStatus (bool status=true, int verbose=0) { TTreeIterator::SetBranchStatus (branch, status, true, verbose); }
    void ResetAddress() {
      if (branch && set
#ifndef OVERRIDE_BRANCH_ADDRESS
          && !puser
#endif
         ) branch->ResetAddress();
    }
  };

  // Constructors and destructors
  // Creates new TTree, or uses existing tree, in current gDirectory
  TTreeIterator (const char* name="", int verbose=0)
    : TNamed(name, ""),
      fVerbose(verbose)
  { Init(); }

  // Creates new TTree, or uses existing tree, in given TDirectory/TFile
  TTreeIterator (const char* name, TDirectory* dir, int verbose=0)
    : TNamed(name, ""),
      fVerbose(verbose)
  { Init(dir); }

  // Use given TTree
  TTreeIterator (TTree* tree, int verbose=0)
    : TNamed(tree ? tree->GetName() : "", tree ? tree->GetTitle() : ""),
      fTree(tree),
      fIndex(tree ? tree->GetEntries() : 0),
      fVerbose(verbose) {
    SetBranchStatusAll(false);
  }

  ~TTreeIterator() override {
    if (fTreeOwned) delete fTree;
  }

  // Access to underlying tree
  TTree* operator->() const { return GetTree(); }
  TTree* GetTree() const { return fTree; }
  TTree* SetTree (TTree* tree) {
    reset();
    if (fTreeOwned) delete fTree;
    fTree = tree;
    fTreeOwned = false;
    return fTree;
  }

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
  void  SetBufsize    (Int_t bufsize)     { fBufsize    = bufsize;         }
  void  SetSplitlevel (Int_t splitlevel)  { fSplitlevel = splitlevel;      }
  void  SetPreferPP   (bool preferPP)     { fPreferPP   = preferPP;        }
  Int_t GetBufsize()    const             { return       fBufsize;         }
  Int_t GetSplitlevel() const             { return       fSplitlevel;      }
  bool  GetPreferPP() const               { return       fPreferPP;        }
#ifndef OVERRIDE_BRANCH_ADDRESS  // only need flag if compiled in
  void  SetOverrideBranchAddress (bool o) { fOverrideBranchAddress = o;    }
  bool  GetOverrideBranchAddress() const  { return fOverrideBranchAddress; }
#else
  void  SetOverrideBranchAddress (bool)   {                                }
  bool  GetOverrideBranchAddress() const  { return false;                  }
#endif

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
      if (fVerbose >= 2) {
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
      if (fVerbose >= 2) {
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
  virtual void reset() {
    fIndex = 0;
    for (auto& it : fBranches) {
      BranchInfo& ibranch = it.second;
      ibranch.ResetAddress();
      ibranch.EnableReset();
    }
    fBranches.clear();
  }

  // std::iterator interface
  iterator begin() {
    Long64_t last = fTree ? fTree->GetEntries() : 0;
    if (fVerbose >= 1 && last>0 && fTree->GetDirectory())
      Info ("TTreeIterator", "get %lld entries from tree '%s' in file %s", last, fTree->GetName(), fTree->GetDirectory()->GetName());
    return iterator (*this, 0,    last);
  }

  iterator end()   {
    Long64_t last = fTree ? fTree->GetEntries() : 0;
    return iterator (*this, last, last);
  }

  fill_iterator FillEntries (Long64_t nfill=-1) {
    if (!fTree) return fill_iterator (*this,0,0);
    Long64_t nentries = fTree->GetEntries();
    if (fVerbose >= 1 && fTree->GetDirectory()) {
      if (nfill < 0) {
        Info ("TTreeIterator", "fill entries into tree '%s' in file %s (%lld so far)", fTree->GetName(), fTree->GetDirectory()->GetName(), nentries);
      } else if (nfill > 0) {
        Info ("TTreeIterator", "fill %lld entries into tree '%s' in file %s (%lld so far)", nfill, fTree->GetName(), fTree->GetDirectory()->GetName(), nentries);
      }
    }
    return fill_iterator (*this, nentries, nfill>=0 ? nentries+nfill : -1);
  }

  // Create empty branch
  template <typename T>
  TBranch* Branch (const char* name) {
    using V = typename std::remove_reference<T>::type;
    return Branch<T> (name, GetLeaflist<V>(), fBufsize, fSplitlevel);
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
    using V = typename std::remove_reference<T>::type;
    return Set<T> (name, std::forward<T>(val), GetLeaflist<V>(), fBufsize, fSplitlevel);
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
  const T& Set(const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
    using V = typename std::remove_reference<T>::type;
    if (BranchInfo* ibranch = GetBranchInfo<T> (name)) {
      return SetValue<T>(ibranch, name, std::forward<T>(val));
    }
    BranchInfo* ibranch = NewBranch<T> (name, std::forward<T>(val), leaflist, bufsize, splitlevel);
    return std::any_cast<V&>(ibranch->value);
  }

  // Get value, returning a reference
  template <typename T>
  T& Get(const char* name) const {
    static T def {type_default<T>()};   // static default value for each type to allow us to return by reference (NB. sticks around until program exit)
    return Get<T>(name, def);
  }

  // Get() allowing the default value (returned if there is an error) to be specified.
  template <typename T>
  T& Get(const char* name, T& val) const {
    return const_cast<T&> (Get<T> (name, const_cast<const T&>(val)));
  }

  template <typename T>
  const T& Get(const char* name, const T& val) const {
    if (BranchInfo* ibranch = GetBranch<T>(name)) {
      if (ibranch->set) {
#ifndef OVERRIDE_BRANCH_ADDRESS
        if (!ibranch->puser) {
#endif
          T* pvalue = std::any_cast<T>(&ibranch->value);
#ifndef FEWER_CHECKS
          if (ibranch->pvalue && ibranch->pvalue != pvalue) {
            if (fVerbose >= 1) Info (tname<T>("Get"), "branch '%s' object address changed from our @%p to @%p", name, pvalue, ibranch->pvalue);
#ifndef OVERRIDE_BRANCH_ADDRESS
            ibranch->puser = &ibranch->pvalue;
#endif
          } else
#endif
            return *pvalue;
#ifndef OVERRIDE_BRANCH_ADDRESS
        }
        if (ibranch->isobj) {
          if (ibranch->puser && *ibranch->puser)
            return **(T**)ibranch->puser;
        } else {
          if (ibranch->puser)
            return  *(T* )ibranch->puser;
        }
#endif
      }
    }
    return val;
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

  void Init (TDirectory* dir=nullptr) {
    if (!dir) dir = gDirectory;
    if ( dir) dir->GetObject(GetName(), fTree);
    if (!fTree) {
      if (dir && !dir->IsWritable()) {
        Error ("TTreeIterator", "TTree '%s' not found in file %s.", GetName(), dir->GetName());
        return;
      }
      fTree = new TTree(GetName(),"",99,dir);
    } else {
      SetTitle(fTree->GetTitle());
      fIndex = fTree->GetEntries();
      SetBranchStatusAll(false);
    }
    fTreeOwned = true;
  }

  template <typename T>
  BranchInfo* GetBranch(const char* name) const {
    BranchInfo* ibranch = GetBranchInfo<T> (name);
    if (ibranch) return ibranch;
    ibranch = SetBranchInfo<T> (name, type_default<T>());
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Get"), "no tree available");
    } else if (TBranch* branch = fTree->GetBranch(name)) {
      ibranch->branch = branch;
      if (!SetBranchAddress<T> (ibranch, name)) return nullptr;
#ifndef OVERRIDE_BRANCH_ADDRESS
      if (ibranch->puser) return ibranch;  // already read value
#endif
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
  BranchInfo* GetBranchInfo (const char* name) const {
    auto it = fBranches.find({name,typeid(T).hash_code()});
    if (it == fBranches.end()) return nullptr;
    BranchInfo* ibranch = &it->second;
#ifndef FEWER_CHECKS
    if (fVerbose >= 2) {
#ifndef OVERRIDE_BRANCH_ADDRESS
      if (ibranch->puser)
        Info (tname<T>("GetBranchInfo"), "found branch '%s' of type '%s' @%p", name, type_name<T>(), ibranch->puser);
      else
#endif
        Info (tname<T>("GetBranchInfo"), "found branch '%s' of type '%s' @%p", name, type_name<T>(), &ibranch->value);
    }
#endif
    return ibranch;
  }

  template <typename T>
  const T& SetValue (BranchInfo* ibranch, const char* name, T&& val) {
    using V = typename std::remove_reference<T>::type;
    if (ibranch->set) {
#ifndef OVERRIDE_BRANCH_ADDRESS
      if (!ibranch->puser) {
#endif
#ifndef FEWER_CHECKS
        if (ibranch->pvalue && ibranch->pvalue != std::any_cast<V>(&ibranch->value)) {
          if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' object address changed from our @%p to @%p", name, std::any_cast<V>(&ibranch->value));
#ifndef OVERRIDE_BRANCH_ADDRESS
          ibranch->puser = &ibranch->pvalue;
#endif
        } else
#endif
          return ibranch->value.emplace<T>(std::forward<T>(val));
#ifndef OVERRIDE_BRANCH_ADDRESS
      }
      if (ibranch->isobj) {
        if (ibranch->puser && *ibranch->puser)
          return **(V**)ibranch->puser = std::forward<T>(val);
      } else {
        if (ibranch->puser)
          return  *(V* )ibranch->puser = std::forward<T>(val);
      }
#endif
    }
    return val;
  }

  template <typename T>
  BranchInfo* NewBranch (const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
    using V = typename std::remove_reference<T>::type;
    TBranch* branch = fTree ? fTree->GetBranch(name) : nullptr;
    Long64_t nentries = (branch ? branch->GetEntries() : 0);
    BranchInfo* ibranch;
    if (fIndex <= nentries) {
      ibranch = SetBranchInfo<T> (name, std::forward<T>(val));
    } else {
      V def = type_default<V>();
      ibranch = SetBranchInfo<T> (name, std::forward<T>(def));
    }
    if (!fTree) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "no tree available");
      return ibranch;
    }
    V* pvalue = std::any_cast<V>(&ibranch->value);
    if (branch) {
      ibranch->branch = branch;
      if (fVerbose >= 1) Info (tname<T>("Set"), "new branch '%s' of type '%s' already exists @%p", name, type_name<T>(), pvalue);
      SetBranchAddress<V> (ibranch, name, "Set");
#ifndef OVERRIDE_BRANCH_ADDRESS
      if (ibranch->puser) SetValue<T> (ibranch, name, std::any_cast<T>(ibranch->value));
#endif
    } else if (leaflist && *leaflist) {
      branch = fTree->Branch (name, pvalue, leaflist, bufsize);
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s' with leaves '%s' of type '%s'", name, leaflist, type_name<T>());
        return ibranch;
      }
      if   (fVerbose >= 1) Info  (tname<T>("Set"), "create branch '%s' with leaves '%s' of type '%s' @%p",       name, leaflist, type_name<T>(), pvalue);
      ibranch->branch = branch;
      ibranch->set = true;
    } else {
      void* addr;
      if (fPreferPP && TClass::GetClass<V>()) {  // shouldn't have to use **T for objects, but maybe it's more reliable?
        ibranch->isobj = true;
        ibranch->pvalue = pvalue;
        addr = &ibranch->pvalue;
        branch = fTree->Branch (name, (V**)addr, bufsize, splitlevel);
      } else {
        addr = pvalue;
        branch = fTree->Branch (name,    pvalue, bufsize, splitlevel);
      }
      if (!branch) {
        if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s' of type '%s'", name, (ibranch->isobj?"object":"variable"), type_name<T>());
        return ibranch;
      }
      if   (fVerbose >= 1) Info  (tname<T>("Set"), "create branch '%s' of type '%s' @%p",       name, (ibranch->isobj?"object":"variable"), type_name<T>(), addr);
      ibranch->branch = branch;
      ibranch->set = true;
    }
    fWriting = true;
    if (fIndex > nentries) {
      if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, fIndex);
      for (Long64_t i = nentries; i < fIndex; i++) {
        FillBranch<T> (branch, name);
      }
      SetValue<T>(ibranch, name, std::forward<T>(val));
    }
    return ibranch;
  }

  template <typename T>
  BranchInfo* SetBranchInfo (const char* name, T&& val) const {
    return &(fBranches[{name,typeid(T).hash_code()}] = BranchInfo {std::forward<T>(val)});
  }

  template <typename T>
  bool SetBranchAddress (BranchInfo* ibranch, const char* name, const char* call="Get") const {
    Int_t stat=0;
    ibranch->Enable(fVerbose);
    TBranch* branch = ibranch->branch;
    TClass* cls = TClass::GetClass<T>();
    if (cls && branch->GetMother() == branch) {
      TClass* expectedClass = 0;
      EDataType expectedType = kOther_t;
      if (!branch->GetExpectedType (expectedClass, expectedType)) {
        if (expectedClass) ibranch->isobj = true;
      } else {
        if (fVerbose >= 1) Info (tname<T>("SetBranchAddress"), "GetExpectedType failed for branch '%s'", name);
      }
    }
    void* addr;
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (!fOverrideBranchAddress) {
      addr = branch->GetAddress();
      if (addr && !ibranch->was_disabled) {
        EDataType type = (!cls) ? TDataType::GetType(typeid(T)) : kOther_t;
        Int_t res = TTreeProtected::Access(*fTree) . CheckBranchAddressType (branch, cls, type, ibranch->isobj);
        if (res < 0) {
          if (fVerbose >= 0) Error (tname<T>(call), "branch '%s' %s existing address %p wrong type", name, (ibranch->isobj?"object":"variable"), addr);
          return false;
        }
        if   (fVerbose >= 1) Info  (tname<T>(call), "use branch '%s' %s existing address %p",        name, (ibranch->isobj?"object":"variable"), addr);
        ibranch->puser = (void**)addr;
        ibranch->set = true;
        return true;
      }
    }
#endif
    T* pvalue= std::any_cast<T>(&ibranch->value);
    if (ibranch->isobj) {
      ibranch->pvalue = pvalue;
      addr = &ibranch->pvalue;
      stat = fTree->SetBranchAddress (name, (T**)(addr));
    } else {
      addr = pvalue;
      stat = fTree->SetBranchAddress (name, pvalue);
    }
    if (stat < 0) {
      if (fVerbose >= 0) Error (tname<T>(call), "failed to set branch '%s' %s address %p", name, (ibranch->isobj?"object":"variable"), addr);
      ibranch->EnableReset(fVerbose);
      return false;
    }
    if   (fVerbose >= 1) Info  (tname<T>(call), "set branch '%s' %s address %p",           name, (ibranch->isobj?"object":"variable"), addr);
    ibranch->set = true;
    return true;
  }

  template <typename T>
  Int_t FillBranch (TBranch* branch, const char* name) {
    Int_t nbytes = branch->Fill();
    if (nbytes > 0) {
      fWriting = true;
      if (fVerbose >= 2) Info  (tname<T>("Set"), "filled branch '%s' with %d bytes for entry %lld", name, nbytes, fIndex);
    } else if (nbytes == 0) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "no data filled in branch '%s' for entry %lld",    name,         fIndex);
    } else {
      if (fVerbose >= 0) Error (tname<T>("Set"), "error filling branch '%s' for entry %lld",        name,         fIndex);
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
  mutable std::map<std::pair<std::string,size_t>,BranchInfo> fBranches;
  TTree* fTree=nullptr;
  bool fTreeOwned=false;
  Int_t fBufsize=32000;
  Int_t fSplitlevel=99;
  int fVerbose=0;
  bool fWriting=false;
  bool fPreferPP=true;   // for ROOT objects, tree->Branch() gives **obj, rather than *obj
#ifndef OVERRIDE_BRANCH_ADDRESS  // only need flag if compiled in
  bool fOverrideBranchAddress=false;
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
