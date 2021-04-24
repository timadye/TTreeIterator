// Inline implementation details for TTreeIterator.
// Created by Tim Adye on 18/04/2021.

#ifndef ROOT_TTreeIterator_detail
#define ROOT_TTreeIterator_detail

#include <limits>
#include "TError.h"
#include "TFile.h"
#include "TChain.h"

#define USE_MAP_EMPLACE 1  // use map::emplace instead of map::insert, which is probably a good idea but probably makes little difference


inline void TTreeIterator::Init (TDirectory* dir /* =nullptr */, bool owned/*=true*/) {
#ifdef USE_Vector_BranchInfo
  fBranches.reserve (200);   // when we reallocate, SetBranchAddress will be invalidated so have to fix up each time
#endif
  if (!owned) {
    SetBranchStatusAll(false);
  } else {
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
}


inline TTree* TTreeIterator::SetTree (TTree* tree) {
  reset();
  if (fTreeOwned) delete fTree;
  fTree = tree;
  fTreeOwned = false;
  return fTree;
}


// use a TChain
inline Int_t TTreeIterator::Add (const char* name, Long64_t nentries/*=TTree::kMaxEntries*/) {
  auto chain = dynamic_cast<TChain*>(fTree);
  if (!chain) {
    reset();
    chain = new TChain (GetName(), GetTitle());
    if (fTree && fTree->GetEntriesFast()) {
      Write();   // only writes if there's something to write and somewhere to write it to
      if (fTree->GetCurrentFile())
        chain->Add (fTree->GetCurrentFile()->GetName());
      else
        Warning ("Add", "cannot include %lld entries from in-memory TTree '%s' in new TChain of same name - existing in-memory TTree will be dropped",
                 fTree->GetEntriesFast(), GetName());
    }
    if (fTreeOwned) delete fTree;
    fTree = chain;
    fTreeOwned = true;
  }
  Int_t nfiles = chain->Add (name, nentries);
  if (nfiles > 0 && fVerbose >= 1) Info ("Add", "added %d files to chain '%s': %s", nfiles, chain->GetName(), name);
  return nfiles;
}


inline TTreeIterator::~TTreeIterator() /*override*/ {
  if (fTreeOwned) delete fTree;
  if (fVerbose >= 1) {
#ifdef BranchInfo_STATS
    if (nhits || nmiss)
      Info ("TTreeIterator", "GetBranchInfo optimisation had %lu hits, %lu misses, %.1f%% success rate", nhits, nmiss, double(100*nhits)/double(nhits+nmiss));
#endif
    if (fTotFill>0 || fTotWrite>0)
      Info ("TTreeIterator", "filled %lld bytes total; wrote %lld bytes at end", fTotFill, fTotWrite);
    if (fTotRead>0)
      Info ("TTreeIterator", "read %lld bytes total", fTotRead);
  }
}


// Forwards to TTree with some extra
inline /*virtual*/ TTreeIterator& TTreeIterator::GetEntry() {
  if (fIndex < 0) return *this;
  if (!fTree) {
    if (fVerbose >= 0) Error ("GetEntry", "no tree available");
    return *this;
  }

  Int_t nbytes = fTree->GetEntry(fIndex);
  if (nbytes >= 0) {
    fTotRead += nbytes;
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


inline /*virtual*/ Int_t TTreeIterator::Fill() {
  if (!fTree) return 0;

  Int_t nbytes = fTree->Fill();

  if (nbytes >= 0) {
    fTotFill += nbytes;
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


inline Int_t TTreeIterator::Write (const char* name/*=0*/, Int_t option/*=0*/, Int_t bufsize/*=0*/) /*override*/ {
  Int_t nbytes = 0;
  if (fWriting && fTree && fTree->GetDirectory() && fTree->GetDirectory()->IsWritable()) {
    nbytes = fTree->Write (name, option, bufsize);
    if (nbytes>0) fTotWrite += nbytes;
    if (fVerbose >= 1) Info ("Write", "wrote %d bytes to file %s", nbytes, fTree->GetDirectory()->GetName());
  }
  fWriting = false;
  return nbytes;
}


inline /*virtual*/ void TTreeIterator::reset() {
  fIndex = 0;
#if defined(USE_Vector_BranchInfo) || (defined(USE_OrderedMap) && defined(__cpp_lib_make_reverse_iterator))
  for (auto it = fBranches.rbegin(); it != fBranches.rend(); ++it) {
#else
  for (auto it = fBranches. begin(); it != fBranches. end(); ++it) {
#endif
#ifdef USE_Vector_BranchInfo
    BranchInfo& ibranch = *it;
#else
    BranchInfo& ibranch = it->second;
#endif
    ibranch.ResetAddress();
    ibranch.EnableReset();
  }
  fBranches.clear();
}


// std::iterator interface
inline TTreeIterator::iterator TTreeIterator::begin() {
  Long64_t last = fTree ? fTree->GetEntries() : 0;
  if (fVerbose >= 1 && last>0 && fTree->GetDirectory())
    Info ("TTreeIterator", "get %lld entries from tree '%s' in file %s", last, fTree->GetName(), fTree->GetDirectory()->GetName());
  return iterator (*this, 0,    last);
}


inline TTreeIterator::iterator TTreeIterator::end()   {
  Long64_t last = fTree ? fTree->GetEntries() : 0;
  return iterator (*this, last, last);
}


inline TTreeIterator::fill_iterator TTreeIterator::FillEntries (Long64_t nfill/*=-1*/) {
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


template <typename T>
inline TBranch* TTreeIterator::Branch (const char* name, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  if (!fTree) {
    if (fVerbose >= 0) Error (tname<T>("Branch"), "no tree available");
    return nullptr;
  }
  using V = typename std::remove_reference<T>::type;
  V def = type_default<V>();
  BranchInfo* ibranch = NewBranch<T> (name, std::forward<T>(def), leaflist, bufsize, splitlevel);
  return ibranch->branch;
}


template <typename T>
inline const T& TTreeIterator::Set(const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  using V = typename std::remove_reference<T>::type;
  if (BranchInfo* ibranch = GetBranchInfo<T> (name)) {
    return SetValue<T>(ibranch, name, std::forward<T>(val));
  }
  BranchInfo* ibranch = NewBranch<T> (name, std::forward<T>(val), leaflist, bufsize, splitlevel);
  return ibranch->GetValue<V>();
}


template <typename T>
inline const T& TTreeIterator::Get(const char* name, const T& val) const {
  if (BranchInfo* ibranch = GetBranch<T>(name)) {
    if (ibranch->set) {
#ifndef OVERRIDE_BRANCH_ADDRESS
      if (!ibranch->puser) {
#endif
        T* pvalue = ibranch->GetValuePtr<T>();
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


// Convenience function to return the type name
template <typename T>
inline /*static*/ const char* TTreeIterator::tname(const char* name/*=0*/) {
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


template <typename T>
inline TTreeIterator::BranchInfo* TTreeIterator::GetBranch(const char* name) const {
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
      fTotRead += nread;
      if (fVerbose >= 1) Info (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, fIndex);
      return ibranch;
    }
  } else {
    if (fVerbose >= 0) Error (tname<T>("Get"), "branch '%s' not found", name);
  }
  return nullptr;
}


inline TTreeIterator::BranchInfo* TTreeIterator::GetBranchInfo (const char* name, type_code_t type) const {
#ifdef USE_Vector_BranchInfo
  if (fTryLast) {
    ++fLastBranch;
    if (fLastBranch == fBranches.end()) fLastBranch = fBranches.begin();
    BranchInfo& b = *fLastBranch;
    if (b.type == type && b.name == name) {
#ifdef BranchInfo_STATS
      ++nhits;
#endif
      return &b;
    }
  }
  for (auto ib = fBranches.begin(); ib != fBranches.end(); ib++) {
    if (ib == fLastBranch) continue;    // already checked this one
    BranchInfo& b = *ib;
    if (b.type == type && b.name == name) {
      fTryLast = true;
      fLastBranch = ib;
#ifdef BranchInfo_STATS
      ++nmiss;
#endif
      return &b;
    }
  }
  fTryLast = false;
  return nullptr;
#else
  auto it = fBranches.find ({name, type});
  if (it == fBranches.end()) return nullptr;
  return &it->second;
#endif
}


template <typename T>
inline TTreeIterator::BranchInfo* TTreeIterator::GetBranchInfo (const char* name) const {
  using V = typename std::remove_reference<T>::type;
  BranchInfo* ibranch = GetBranchInfo (name, typeid(T).hash_code());
  if (!ibranch) return ibranch;
#ifndef FEWER_CHECKS
  if (fVerbose >= 2) {
    void* addr;
    const char* user = "";
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (ibranch->puser) {
      addr = ibranch->puser;
      user = " user";
    } else
#endif
      addr = ibranch->GetValuePtr<V>();
    Info (tname<T>("GetBranchInfo"), "found%s%s branch '%s' of type '%s' @%p", (ibranch->set?"":" bad"), user, name, type_name<T>(), addr);
  }
#endif
  return ibranch;
}


template <typename T>
inline const T& TTreeIterator::SetValue (BranchInfo* ibranch, const char* name, T&& val) {
  using V = typename std::remove_reference<T>::type;
  if (ibranch->set) {
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (!ibranch->puser) {
#endif
#ifndef FEWER_CHECKS
      if (ibranch->pvalue && ibranch->pvalue != ibranch->GetValuePtr<V>()) {
        if (fVerbose >= 1) Info (tname<T>("Set"), "branch '%s' object address changed from our @%p to @%p", name, ibranch->GetValuePtr<V>());
#ifndef OVERRIDE_BRANCH_ADDRESS
        ibranch->puser = &ibranch->pvalue;
#endif
      } else
#endif
        return ibranch->SetValue<T>(std::forward<T>(val));
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
inline TTreeIterator::BranchInfo* TTreeIterator::NewBranch (const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
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
  V* pvalue = ibranch->GetValuePtr<V>();
  if (branch) {
    ibranch->branch = branch;
    if (fVerbose >= 1) Info (tname<T>("Set"), "new branch '%s' of type '%s' already exists @%p", name, type_name<T>(), pvalue);
    SetBranchAddress<V> (ibranch, name, "Set");
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (ibranch->puser) SetValue<T> (ibranch, name, std::forward<T>(ibranch->GetValue<T>()));
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
#ifdef PREFER_PTRPTR
    if (TClass::GetClass<V>()) {  // shouldn't have to use **T for objects, but maybe it's more reliable?
      ibranch->isobj = true;
      ibranch->pvalue = pvalue;
      addr = &ibranch->pvalue;
      branch = fTree->Branch (name, (V**)addr, bufsize, splitlevel);
    } else
#endif
    {
      addr = pvalue;
      branch = fTree->Branch (name,    pvalue, bufsize, splitlevel);
    }
    if (!branch) {
      if (fVerbose >= 0) Error (tname<T>("Set"), "failed to create branch '%s' %s of type '%s'", name, (ibranch->isobj?"object":"variable"), type_name<T>());
      return ibranch;
    }
    if   (fVerbose >= 1) Info  (tname<T>("Set"), "create branch '%s' %s of type '%s' @%p",       name, (ibranch->isobj?"object":"variable"), type_name<T>(), addr);
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
inline TTreeIterator::BranchInfo* TTreeIterator::SetBranchInfo (const char* name, T&& val) const {
#ifdef USE_Vector_BranchInfo
  using V = typename std::remove_reference<T>::type;
  BranchInfo* front = &fBranches.front();
  fBranches.emplace_back (name, typeid(T).hash_code(), &TTreeIterator::SetBranchAddressImpl<V>, std::forward<T>(val));
  if (front != &fBranches.front()) SetBranchAddressAll("SetBranchInfo");  // vector data() moved
  BranchInfo* ibranch = &fBranches.back();
#else
#ifdef USE_MAP_EMPLACE
  auto ret = fBranches.emplace (std::piecewise_construct, std::forward_as_tuple(name, typeid(T).hash_code()), std::forward_as_tuple(val));
#else
  auto ret = fBranches.insert  ({{name, typeid(T).hash_code()}, std::forward<T>(val)});
#endif
  BranchInfo* ibranch = &ret.first->second;
#ifndef FEWER_CHECKS
  if (!ret.second) {
    if (fVerbose >= 1) Info ("SetBranchInfo", "somehow we have already saved branch '%s' info", name);
#ifndef USE_MAP_EMPLACE
    ibranch->SetValue<T>(std::forward<T>(val));
#endif
  }
#endif
#endif
  return ibranch;
}


template <typename T>
inline bool TTreeIterator::SetBranchAddress (BranchInfo* ibranch, const char* name, const char* call/*="Get"*/) const {
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
#ifndef OVERRIDE_BRANCH_ADDRESS
  if (!fOverrideBranchAddress) {
    void* addr = branch->GetAddress();
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
  return SetBranchAddressImpl<T> (ibranch, name, call);
}

template <typename T>
inline bool TTreeIterator::SetBranchAddressImpl (BranchInfo* ibranch, const char* name, const char* call, bool redo/*=false*/) const {
  T* pvalue= ibranch->GetValuePtr<T>();
  Int_t stat=0;
  void* addr;
  if (ibranch->isobj) {
    ibranch->pvalue = pvalue;
    addr = &ibranch->pvalue;
    if (!redo)
      stat = fTree->SetBranchAddress (name, (T**)(addr));
  } else {
    redo=false;  // only helps for objects
    addr = pvalue;
    stat = fTree->SetBranchAddress (name, pvalue);
  }
  if (stat < 0) {
    if (fVerbose >= 0) Error (tname<T>(call), "failed to set branch '%s' %s address %p", name, (ibranch->isobj?"object":"variable"), addr);
    ibranch->EnableReset(fVerbose);
    ibranch->set = false;
    return false;
  }
  if   (fVerbose >= 1) Info  (tname<T>(call), "set branch '%s' %s address %p%s",         name, (ibranch->isobj?"object":"variable"), addr, (redo?" (pointer only)":""));
  ibranch->set = true;
  return true;
}


#ifdef USE_Vector_BranchInfo
inline void TTreeIterator::SetBranchAddressAll (const char* call) const {
  if (fVerbose >= 1) Info  (call, "cache reallocated, so need to set all branch addresses again");
  for (auto& b : fBranches) {
    if (b.SetBranchAddressImpl && b.set && !b.puser) {
      (this->*b.SetBranchAddressImpl) (&b, b.name.c_str(), call, true);
    }
  }
}
#endif


template <typename T>
inline Int_t TTreeIterator::FillBranch (TBranch* branch, const char* name) {
  Int_t nbytes = branch->Fill();
  if (nbytes > 0) {
    fTotFill += nbytes;
    fWriting = true;
    if (fVerbose >= 2) Info  (tname<T>("Set"), "filled branch '%s' with %d bytes for entry %lld", name, nbytes, fIndex);
  } else if (nbytes == 0) {
    if (fVerbose >= 0) Error (tname<T>("Set"), "no data filled in branch '%s' for entry %lld",    name,         fIndex);
  } else {
    if (fVerbose >= 0) Error (tname<T>("Set"), "error filling branch '%s' for entry %lld",        name,         fIndex);
  }
  return nbytes;
}


inline /*static*/ void TTreeIterator::SetBranchStatus (TObjArray* list, bool status/*=true*/, bool include_children/*=true*/,
                                                       int verbose/*=0*/, const std::string* pre/*=nullptr*/) {
  if (!list) return;
  Int_t nbranches = list->GetEntriesFast();
  for (Int_t i = 0; i < nbranches; ++i) {
    SetBranchStatus (dynamic_cast<TBranch*>(list->UncheckedAt(i)), status, include_children, verbose, pre);
  }
}


inline /*static*/ void TTreeIterator::SetBranchStatus (TBranch* branch, bool status/*=true*/, bool include_children/*=true*/,
                                                       int verbose/*=0*/, const std::string* pre/*=nullptr*/) {
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


inline std::string TTreeIterator::BranchNamesString (bool include_children/*=true*/, bool include_inactive/*=false*/) {
  std::string str;
  auto allbranches = BranchNames (include_children, include_inactive);
  for (auto& name : allbranches) {
    if (!str.empty()) str += ", ";
    str += name;
  }
  return str;
}


inline std::vector<std::string> TTreeIterator::BranchNames (bool include_children/*=false*/, bool include_inactive/*=false*/) {
  std::vector<std::string> allbranches;
  BranchNames (allbranches, fTree->GetListOfBranches(), include_children, include_inactive);
  return allbranches;
}


inline /*static*/ void TTreeIterator::BranchNames (std::vector<std::string>& allbranches, TObjArray* list, bool include_children, bool include_inactive,
                                                   const std::string& pre/*=""*/) {
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

#endif /* ROOT_TTreeIterator_detail */
