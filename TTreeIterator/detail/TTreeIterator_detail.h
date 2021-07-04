// Inline implementation details for TTreeIterator.
// Created by Tim Adye on 18/04/2021.

#ifndef ROOT_TTreeIterator_detail
#define ROOT_TTreeIterator_detail

#include <limits>
#include "TError.h"
#include "TFile.h"
#include "TChain.h"


inline void TTreeIterator::Init (TDirectory* dir /* =nullptr */, bool owned/*=true*/) {
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
      SetBranchStatusAll(false);
    }
    fTreeOwned = true;
  }
}


inline TTree* TTreeIterator::SetTree (TTree* tree) {
  if (fTreeOwned) delete fTree;
  fTree = tree;
  fTreeOwned = false;
  return fTree;
}


// use a TChain
inline Int_t TTreeIterator::Add (const char* name, Long64_t nentries/*=TTree::kMaxEntries*/) {
  auto chain = dynamic_cast<TChain*>(fTree);
  if (!chain) {
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
  if (nfiles > 0 && verbose() >= 1) Info ("Add", "added %d files to chain '%s': %s", nfiles, chain->GetName(), name);
  return nfiles;
}


inline TTreeIterator::~TTreeIterator() /*override*/ {
  if (fTreeOwned) delete fTree;
}


// Forwards to TTree with some extra
inline /*virtual*/ Int_t TTreeIterator::GetEntry (Long64_t index, Int_t getall/*=0*/) {
  if (index < 0) return 0;
  if (!fTree) {
    if (verbose() >= 0) Error ("GetEntry", "no tree available");
    return -1;
  }

  Int_t nbytes = fTree->GetEntry (index, getall);
  if (nbytes > 0) {
    if (verbose() >= 2) {
      std::string allbranches = BranchNamesString();
      Info  ("GetEntry", "read %d bytes from entry %lld for branches: %s", nbytes, index, allbranches.c_str());
    }
  } else if (nbytes == 0) {
    if (verbose() >= 0) {
      Error ("GetEntry", "entry %lld does not exist", index);
    }
  } else {
    if (verbose() >= 0) {
      std::string allbranches = BranchNamesString();
      Error ("GetEntry", "problem reading entry %lld for branches: %s", index, allbranches.c_str());
    }
  }
  return nbytes;
}


template <typename T>
inline /*static*/ void TTreeIterator::BranchInfo::SetDefaultValue (BranchInfo* ibranch) {
  using V = remove_cvref_t<T>;
  if (ibranch->verbose() >= 1) ibranch->tree().Info (tname<T>("Set"), "branch '%s' value was not set for entry %lld - use type's default", ibranch->fName, ibranch->index());
  ibranch->Set<T>(type_default<V>());
}


inline Int_t TTreeIterator::Entry_iterator::Fill() {
  TTree* t = GetTree();
  if (!t) return 0;

#ifndef NO_FILL_UNSET_DEFAULT
  for (auto& b : fBranches) {
    BranchInfo* ibranch = &b;
    if (ibranch->fSet
#ifndef OVERRIDE_BRANCH_ADDRESS
        && !ibranch->fPuser
#endif
       ) {
      if (ibranch->fUnset)
        (*ibranch->fSetDefaultValue) (ibranch);
      else ibranch->fUnset = true;
    }
  }
#endif

  Int_t nbytes = t->Fill();

  if (nbytes >= 0) {
    fTotFill += nbytes;
    if (verbose() >= 2) {
      std::string allbranches = tree().BranchNamesString();
      tree().Info  ("Fill", "Filled %d bytes for entry %lld, branches: %s", nbytes, fIndex, allbranches.c_str());
    }
  } else {
    if (verbose() >= 0) {
      std::string allbranches = tree().BranchNamesString();
      tree().Error ("Fill", "problem writing entry %lld for branches: %s", fIndex, allbranches.c_str());
    }
  }

  if (nbytes > 0) fWriting = true;

  return nbytes;
}


inline Int_t TTreeIterator::Fill_iterator::Write (const char* name/*=0*/, Int_t option/*=0*/, Int_t bufsize/*=0*/) {
  Int_t nbytes = 0;
  TTree* t = GetTree();
  if (fWriting && t && t->GetDirectory() && t->GetDirectory()->IsWritable()) {
    nbytes = t->Write (name, option, bufsize);
    if (nbytes>0) fTotWrite += nbytes;
    if (verbose() >= 1) tree().Info ("Write", "wrote %d bytes to file %s", nbytes, t->GetDirectory()->GetName());
  }
  fWriting = false;
  return nbytes;
}


inline TTreeIterator::Entry_iterator::~Entry_iterator() {
  if (verbose() >= 1) {
#ifndef NO_BranchInfo_STATS
    if (nhits || nmiss)
      tree().Info ("TTreeIterator", "GetBranchInfo optimisation had %lu hits, %lu misses, %.1f%% success rate", nhits, nmiss, double(100*nhits)/double(nhits+nmiss));
#endif
    if (fTotFill>0 || fTotWrite>0)
      tree().Info ("TTreeIterator", "filled %lld bytes total; wrote %lld bytes at end", fTotFill, fTotWrite);
    if (fTotRead>0)
      tree().Info ("TTreeIterator", "read %lld bytes total", fTotRead);
  }
  for (auto ibranch = fBranches.rbegin(); ibranch != fBranches.rend(); ++ibranch) {
    ibranch->ResetAddress();
    ibranch->EnableReset();
  }
}


// std::iterator interface
inline TTreeIterator::iterator TTreeIterator::begin() {
  Long64_t last = GetTree() ? GetTree()->GetEntries() : 0;
  if (verbose() >= 1 && last>0 && GetTree()->GetDirectory())
    Info ("TTreeIterator", "get %lld entries from tree '%s' in file %s", last, GetTree()->GetName(), GetTree()->GetDirectory()->GetName());
  return Entry_iterator (*this, 0,    last);
}


inline TTreeIterator::iterator TTreeIterator::end()   {
  Long64_t last = GetTree() ? GetTree()->GetEntries() : 0;
  return Entry_iterator (*this, last, last);
}


inline TTreeIterator::Fill_iterator TTreeIterator::FillEntries (Long64_t nfill/*=-1*/) {
  if (!GetTree()) return Fill_iterator (*this,0,0);
  Long64_t nentries = GetTree()->GetEntries();
  if (verbose() >= 1 && GetTree()->GetDirectory()) {
    if (nfill < 0) {
      Info ("TTreeIterator", "fill entries into tree '%s' in file %s (%lld so far)", GetTree()->GetName(), GetTree()->GetDirectory()->GetName(), nentries);
    } else if (nfill > 0) {
      Info ("TTreeIterator", "fill %lld entries into tree '%s' in file %s (%lld so far)", nfill, GetTree()->GetName(), GetTree()->GetDirectory()->GetName(), nentries);
    }
  }
  return Fill_iterator (*this, nentries, nfill>=0 ? nentries+nfill : -1);
}


template <typename T>
inline TBranch* TTreeIterator::Entry_iterator::Branch (const char* name, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  if (!GetTree()) {
    if (verbose() >= 0) tree().Error (tname<T>("Branch"), "no tree available");
    return nullptr;
  }
  using V = remove_cvref_t<T>;
  V def = type_default<V>();
  BranchInfo* ibranch = NewBranch<T> (name, std::forward<T>(def), leaflist, bufsize, splitlevel);
  return ibranch->fBranch;
}


template <typename T>
inline const T& TTreeIterator::Entry::Get(const char* name, const T& def) const {
  if (BranchInfo* ibranch = iter().GetBranch<T>(name))
    return ibranch->Get<T>(def);
  else
    return def;
}

template <typename T>
inline const T& TTreeIterator::BranchInfo::Get(const T& def) const {
  const T* pval = GetBranchValue<T>();
  if (pval) return *pval;
  return def;
}

template <typename T>
inline const T* TTreeIterator::BranchInfo::GetBranchValue() const {
  if (fSet) {
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (!fPuser) {
#endif
      const T* pvalue = GetValuePtr<T>();
#ifndef FEWER_CHECKS
      if (fPvalue && fPvalue != pvalue) {
        if (verbose() >= 1) tree().Info (tname<T>("Get"), "branch '%s' object address changed from our @%p to @%p", fName, pvalue, fPvalue);
#ifndef OVERRIDE_BRANCH_ADDRESS
        fPuser = &fPvalue;
#endif
      } else
#endif
        return pvalue;
#ifndef OVERRIDE_BRANCH_ADDRESS
    }
    if (fIsobj) {
      if (fPuser && *fPuser)
        return *(T**)fPuser;
    } else {
      if (fPuser)
        return  (T* )fPuser;
    }
#endif
  }
  return nullptr;
}


template <typename T>
inline const T& TTreeIterator::Entry::Set(const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  using V = remove_cvref_t<T>;
  if (BranchInfo* ibranch = iter().GetBranchInfo<T> (name)) {
    return ibranch->Set<T>(std::forward<T>(val));
  }
  BranchInfo* ibranch = iter().NewBranch<T> (name, std::forward<T>(val), leaflist, bufsize, splitlevel);
  if (!ibranch) return default_value<V>();
  return ibranch->GetValue<V>();
}



// Convenience function to return the type name
template <typename T>
inline /*static*/ const char* TTreeIterator::tname(const char* name/*=0*/) {
  TClass* cl = TClass::GetClass<T>();
  const char* cname = cl ? cl->GetName() : TDataType::GetTypeName (TDataType::GetType(typeid(T)));
  if (!cname || !*cname) cname = type_name<T>();   // use ROOT's shorter name by preference, but fall back on cxxabi or type_info name
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
inline TTreeIterator::BranchInfo* TTreeIterator::Entry_iterator::GetBranch(const char* name) const {
  if (index() < 0) return nullptr;
  BranchInfo* ibranch = GetBranchInfo<T> (name);
  if (ibranch) return ibranch;
  ibranch = SetBranchInfo<T> (name, type_default<T>());
  if (!GetTree()) {
    if (verbose() >= 0) tree().Error (tname<T>("Get"), "no tree available");
  } else if (TBranch* branch = GetTree()->GetBranch(name)) {
    ibranch->fBranch = branch;
    if (!ibranch->SetBranchAddress<T>()) return nullptr;
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (ibranch->fPuser) return ibranch;  // already read value
#endif
    Int_t nread = branch->GetEntry (index());
    if (nread < 0) {
      if (verbose() >= 0) tree().Error (tname<T>("Get"), "GetEntry failed for branch '%s', entry %lld", name, index());
    } else if (nread == 0) {
      if (verbose() >= 0) tree().Error (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, index());
    } else {
      fTotRead += nread;
      if (verbose() >= 1) tree().Info (tname<T>("Get"), "branch '%s' read %d bytes from entry %lld", name, nread, index());
      return ibranch;
    }
  } else {
    if (verbose() >= 0) tree().Error (tname<T>("Get"), "branch '%s' not found", name);
  }
  return nullptr;
}


inline TTreeIterator::BranchInfo* TTreeIterator::Entry_iterator::GetBranchInfo (const char* name, type_code_t type) const {
  if (fTryLast) {
    ++fLastBranch;
    if (fLastBranch == fBranches.end()) fLastBranch = fBranches.begin();
    BranchInfo& b = *fLastBranch;
    if (b.fType == type && b.fName == name) {
#ifndef NO_BranchInfo_STATS
      ++nhits;
#endif
      return &b;
    }
  }
  for (auto ib = fBranches.begin(); ib != fBranches.end(); ib++) {
    if (fTryLast && ib == fLastBranch) continue;    // already checked this one
    BranchInfo& b = *ib;
    if (b.fType == type && b.fName == name) {
      fTryLast = true;
      fLastBranch = ib;
#ifndef NO_BranchInfo_STATS
      ++nmiss;
#endif
      return &b;
    }
  }
  fTryLast = false;
  return nullptr;
}


template <typename T>
inline TTreeIterator::BranchInfo* TTreeIterator::Entry_iterator::GetBranchInfo (const char* name) const {
  using V = remove_cvref_t<T>;
  BranchInfo* ibranch = GetBranchInfo (name, type_code<T>());
  if (!ibranch) return ibranch;
#ifndef FEWER_CHECKS
  if (verbose() >= 2) {
    void* addr;
    const char* user = "";
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (ibranch->fPuser) {
      addr = ibranch->fPuser;
      user = " user";
    } else
#endif
      addr = ibranch->GetValuePtr<V>();
    tree().Info (tname<T>("GetBranchInfo"), "found%s%s branch '%s' of type '%s' @%p", (ibranch->fSet?"":" bad"), user, name, type_name<T>(), addr);
  }
#endif
  return ibranch;
}


template <typename T>
inline const T& TTreeIterator::BranchInfo::Set(T&& val) {
  using V = remove_cvref_t<T>;
  if (fSet) {
    fUnset = false;
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (!fPuser) {
#endif
#ifndef FEWER_CHECKS
      if (fPvalue && fPvalue != GetValuePtr<V>()) {
        if (verbose() >= 1) tree().Info (tname<T>("Set"), "branch '%s' object address changed from our @%p to @%p", fName, GetValuePtr<V>(), fPvalue);
#ifndef OVERRIDE_BRANCH_ADDRESS
        fPuser = &fPvalue;
#endif
      } else
#endif
        if (!fPvalue) {
//        if (verbose() >= 3) tree().Info (tname<T>("Set"), "branch '%s' assign value to @%p", fName, GetValuePtr<V>());
          return GetValue<V>() = std::forward<T>(val);
        } else {
          // This does std::any::emplace, which will reallocate the object if it is larger than sizeof(void*).
          // So must adjust pvalue to point to the new address.
          // In practice, this only occurs if PREFER_PTRPTR is defined.
          T& setval = SetValue<T>(std::forward<T>(val));
          if (fPvalue != &setval) {
//          if (verbose() >= 3) tree().Info (tname<T>("Set"), "branch '%s' object address changed when set from @%p to @%p", fName, &setval, fPvalue);
            fPvalue = &setval;
          }
          return setval;
        }
#ifndef OVERRIDE_BRANCH_ADDRESS
    }
    if (fIsobj) {
      if (fPuser && *fPuser)
        return **(V**)fPuser = std::forward<T>(val);
    } else {
      if (fPuser)
        return  *(V* )fPuser = std::forward<T>(val);
    }
#endif
  }
  return val;
}


template <typename T>
inline TTreeIterator::BranchInfo* TTreeIterator::Entry_iterator::NewBranch (const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  using V = remove_cvref_t<T>;
  TBranch* branch = GetTree() ? GetTree()->GetBranch(name) : nullptr;
  Long64_t nentries = (branch ? branch->GetEntries() : 0);
  BranchInfo* ibranch;
  if (index() <= nentries) {
    ibranch = SetBranchInfo<T> (name, std::forward<T>(val));
  } else {
    V def = type_default<V>();
    ibranch = SetBranchInfo<T> (name, std::forward<T>(def));
  }
  if (!GetTree()) {
    if (verbose() >= 0) tree().Error (tname<T>("Set"), "no tree available");
    return ibranch;
  }
  V* pvalue = ibranch->GetValuePtr<V>();
  if (branch) {
    ibranch->fBranch = branch;
    if (verbose() >= 1) tree().Info (tname<T>("Set"), "new branch '%s' of type '%s' already exists @%p", name, type_name<T>(), pvalue);
    ibranch->SetBranchAddress<V>("Set");
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (ibranch->fPuser) ibranch->Set<T> (std::forward<T>(ibranch->GetValue<T>()));
#endif
  } else if (leaflist && *leaflist) {
    branch = GetTree()->Branch (name, pvalue, leaflist, bufsize);
    if (!branch) {
      if (verbose() >= 0) tree().Error (tname<T>("Set"), "failed to create branch '%s' with leaves '%s' of type '%s'", name, leaflist, type_name<T>());
      return ibranch;
    }
    if   (verbose() >= 1) tree().Info  (tname<T>("Set"), "create branch '%s' with leaves '%s' of type '%s' @%p",       name, leaflist, type_name<T>(), pvalue);
    ibranch->fBranch = branch;
    ibranch->fSet = true;
  } else {
    void* addr;
#ifdef PREFER_PTRPTR
    if (TClass::GetClass<V>()) {  // shouldn't have to use **T for objects, but maybe it's more reliable?
      ibranch->fIsobj = true;
      ibranch->fPvalue = pvalue;
      addr = &ibranch->fPvalue;
      branch = GetTree()->Branch (name, (V**)addr, bufsize, splitlevel);
    } else
#endif
    {
      addr = pvalue;
      branch = GetTree()->Branch (name,    pvalue, bufsize, splitlevel);
    }
    if (!branch) {
      if (verbose() >= 0) tree().Error (tname<T>("Set"), "failed to create branch '%s' %s of type '%s'", name, (ibranch->fIsobj?"object":"variable"), type_name<T>());
      return ibranch;
    }
    if   (verbose() >= 1) tree().Info  (tname<T>("Set"), "create branch '%s' %s of type '%s' @%p",       name, (ibranch->fIsobj?"object":"variable"), type_name<T>(), addr);
    ibranch->fBranch = branch;
    ibranch->fSet = true;
  }
  fWriting = true;
  if (index() > nentries) {
    if (verbose() >= 1) tree().Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, index());
    for (Long64_t i = nentries; i < index(); i++) {
      FillBranch<T> (branch, name);
    }
    ibranch->Set<T>(std::forward<T>(val));
  }
  return ibranch;
}


template <typename T>
inline TTreeIterator::BranchInfo* TTreeIterator::Entry_iterator::SetBranchInfo (const char* name, T&& val) const {
  using V = remove_cvref_t<T>;
  BranchInfo* front = &fBranches.front();
  fBranches.emplace_back (name, type_code<V>(), std::forward<T>(val), *const_cast<Entry_iterator*>(this), &BranchInfo::SetDefaultValue<V>, &BranchInfo::SetValueAddress<V>);
  if (front != &fBranches.front()) SetBranchAddressAll("SetBranchInfo");  // vector data() moved
  return &fBranches.back();
}


template <typename T>
inline bool TTreeIterator::BranchInfo::SetBranchAddress (const char* call/*="Get"*/) {
  Enable();
  TBranch* branch = fBranch;
  TClass* cls = TClass::GetClass<T>();
  if (cls && branch->GetMother() == branch) {
    TClass* expectedClass = 0;
    EDataType expectedType = kOther_t;
    if (!branch->GetExpectedType (expectedClass, expectedType)) {
      if (expectedClass) fIsobj = true;
    } else {
      if (verbose() >= 1) tree().Info (tname<T>("SetBranchAddress"), "GetExpectedType failed for branch '%s'", fName);
    }
  }
#ifndef OVERRIDE_BRANCH_ADDRESS
  if (!tree().fOverrideBranchAddress) {
    void* addr = branch->GetAddress();
    if (addr && !fWasDisabled) {
      EDataType type = (!cls) ? TDataType::GetType(typeid(T)) : kOther_t;
      Int_t res = TTreeProtected::Access(*GetTree()) . CheckBranchAddressType (branch, cls, type, fIsobj);
      if (res < 0) {
        if (verbose() >= 0) tree().Error (tname<T>(call), "branch '%s' %s existing address %p wrong type", fName, (fIsobj?"object":"variable"), addr);
        return false;
      }
      if   (verbose() >= 1) tree().Info  (tname<T>(call), "use branch '%s' %s existing address %p",        fName, (fIsobj?"object":"variable"), addr);
      fPuser = (void**)addr;
      fSet = true;
      return true;
    }
  }
#endif
  return SetValueAddress<T> (this, call);
}

// this is a static function, so we can store its address without needing a 16-byte member function pointer
template <typename T>
inline /*static*/ bool TTreeIterator::BranchInfo::SetValueAddress (BranchInfo* ibranch, const char* call, bool redo/*=false*/) {
  T* pvalue= ibranch->GetValuePtr<T>();
  Int_t stat=0;
  void* addr;
  if (ibranch->fIsobj) {
    ibranch->fPvalue = pvalue;
    addr = &ibranch->fPvalue;
    if (!redo)
      stat = ibranch->GetTree()->SetBranchAddress (ibranch->fName.c_str(), (T**)(addr));
  } else {
    redo=false;  // only helps for objects
    addr = pvalue;
    stat = ibranch->GetTree()->SetBranchAddress (ibranch->fName.c_str(), pvalue);
  }
  if (stat < 0) {
    if (ibranch->verbose() >= 0) ibranch->tree().Error (tname<T>(call), "failed to set branch '%s' %s address %p", ibranch->fName, (ibranch->fIsobj?"object":"variable"), addr);
    ibranch->EnableReset();
    ibranch->fSet = false;
    return false;
  }
  if   (ibranch->verbose() >= 1) ibranch->tree().Info  (tname<T>(call), "set branch '%s' %s address %p%s",         ibranch->fName, (ibranch->fIsobj?"object":"variable"), addr, (redo?" (pointer only)":""));
  ibranch->fSet = true;
  return true;
}


inline void TTreeIterator::Entry_iterator::SetBranchAddressAll (const char* call) const {
  if (verbose() >= 1) tree().Info  (call, "cache reallocated, so need to set all branch addresses again");
  for (auto& b : fBranches) {
    if (b.fSetValueAddress && b.fSet
#ifndef OVERRIDE_BRANCH_ADDRESS
        && !b.fPuser
#endif
       ) {
      (*b.fSetValueAddress) (&b, call, true);
    }
  }
}


template <typename T>
inline Int_t TTreeIterator::Entry_iterator::FillBranch (TBranch* branch, const char* name) {
  Int_t nbytes = branch->Fill();
  if (nbytes > 0) {
    fTotFill += nbytes;
    fWriting = true;
    if (verbose() >= 2) tree().Info  (tname<T>("Set"), "filled branch '%s' with %d bytes for entry %lld", name, nbytes, index());
  } else if (nbytes == 0) {
    if (verbose() >= 0) tree().Error (tname<T>("Set"), "no data filled in branch '%s' for entry %lld",    name,         index());
  } else {
    if (verbose() >= 0) tree().Error (tname<T>("Set"), "error filling branch '%s' for entry %lld",        name,         index());
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
  BranchNames (allbranches, GetTree()->GetListOfBranches(), include_children, include_inactive);
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
