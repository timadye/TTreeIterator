// Inline implementation details for TTreeIterator.
// Created by Tim Adye on 18/04/2021.

#ifndef ROOT_TTreeIterator_detail
#define ROOT_TTreeIterator_detail

#include <limits>
#include "TError.h"
#include "TFile.h"
#include "TChain.h"

// TTreeIterator ===============================================================

inline void TTreeIterator::Init (TDirectory* dir /* =nullptr */, bool owned/*=true*/) {
  if (owned) {
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
  if (verbose() >= 1 && fBranches.size() > 0)
    Info ("~TTreeIterator", "ResetAddress for %zu branches", fBranches.size());
  for (auto ibranch = fBranches.rbegin(), end = fBranches.rend(); ibranch != end; ++ibranch)
    ibranch->ResetAddress();
  if (fTreeOwned) delete fTree;

  if (verbose() >= 1) {
#ifndef NO_BranchValue_STATS
    if (fNhits || fNmiss)
      Info ("TTreeIterator", "GetBranchValue optimisation had %lu hits, %lu misses, %.1f%% success rate", fNhits, fNmiss, double(100*fNhits)/double(fNhits+fNmiss));
#endif
    if (fTotFill>0 || fTotWrite>0)
      Info ("TTreeIterator", "filled %lld bytes total; wrote %lld bytes at end", fTotFill, fTotWrite);
#ifndef NO_BranchValue_STATS
    if (fTotRead>0)
      Info ("TTreeIterator", "read %lld bytes total", fTotRead);
#endif
  }
}


// std::iterator interface
inline TTreeIterator::Entry_iterator TTreeIterator::begin() {
  Long64_t last = GetTree() ? GetTree()->GetEntries() : 0;
  if (verbose() >= 1 && last>0 && GetTree()->GetDirectory())
    Info ("TTreeIterator", "get %lld entries from tree '%s' in file %s", last, GetTree()->GetName(), GetTree()->GetDirectory()->GetName());
  return Entry_iterator (*this, 0,    last);
}


inline TTreeIterator::Entry_iterator TTreeIterator::end()   {
  Long64_t last = GetTree() ? GetTree()->GetEntries() : 0;
  return Entry_iterator (*this, last, last);
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
#ifndef NO_BranchValue_STATS
    fTotRead += nbytes;
#endif
    if (verbose() >= 2) {
      std::string allbranches = BranchNamesString();
      Info  ("GetEntry", "read %d bytes from entry %lld for branches: %s", nbytes, index, allbranches.c_str());
    }
  } else if (nbytes == 0) {
    if (verbose() >= 0) {
      std::string allbranches = BranchNamesString();
      if (allbranches.size() > 0)
        Error ("GetEntry", "entry %lld does not exist", index);
      else if (verbose() >= 2)
        Info  ("GetEntry", "no active branches to read from entry %lld", index);
    }
  } else {
    if (verbose() >= 0) {
      std::string allbranches = BranchNamesString();
      Error ("GetEntry", "problem reading entry %lld for branches: %s", index, allbranches.c_str());
    }
  }
  return nbytes;
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
inline TBranch* TTreeIterator::Branch (const char* name, Long64_t index, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  if (!GetTree()) {
    if (verbose() >= 0) Error (tname<T>("Branch"), "no tree available");
    return nullptr;
  }
  using V = remove_cvref_t<T>;
  V def = type_default<V>();
  BranchValue* ibranch = NewBranch<T> (name, index, std::forward<T>(def), leaflist, bufsize, splitlevel);
  return ibranch->fBranch;
}


inline /*virtual*/ Int_t TTreeIterator::Fill() {
  TTree* t = GetTree();
  if (!t) return 0;

#ifndef NO_FILL_UNSET_DEFAULT
  for (auto& b : fBranches) {
    BranchValue* ibranch = &b;
    if (ibranch->fHaveAddr
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
      std::string allbranches = BranchNamesString();
      Info  ("Fill", "Filled %d bytes for branches: %s", nbytes, allbranches.c_str());
    }
  } else {
    if (verbose() >= 0) {
      std::string allbranches = BranchNamesString();
      Error ("Fill", "problem filling branches: %s", allbranches.c_str());
    }
  }

  return nbytes;
}


inline Int_t TTreeIterator::Write (const char* name/*=0*/, Int_t option/*=0*/, Int_t bufsize/*=0*/) {
  Int_t nbytes = 0;
  TTree* t = GetTree();
  if (t && t->GetDirectory() && t->GetDirectory()->IsWritable()) {
    nbytes = t->Write (name, option, bufsize);
    if (nbytes>0) fTotWrite += nbytes;
    if (verbose() >= 1) Info ("Write", "wrote %d bytes to file %s", nbytes, t->GetDirectory()->GetName());
  }
  return nbytes;
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


inline /*static*/ void TTreeIterator::BranchNames (std::vector<std::string>& allbranches,
                                                   TObjArray* list,
                                                   bool include_children,
                                                   bool include_inactive,
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


// TTreeIterator protected ========================================================

template <typename T>
inline TTreeIterator::BranchValue* TTreeIterator::GetBranch (const char* name, Long64_t index, Long64_t localIndex) const {
  if (index < 0) return nullptr;
  BranchValue* ibranch = GetBranchValue<T> (name);
  if (!ibranch) {
    ibranch = NewBranchValue<T> (name, type_default<T>());
    if (!GetTree()) {
      if (verbose() >= 0) Error (tname<T>("Get"), "no tree available");
      return nullptr;
    } else if (TBranch* branch = GetTree()->GetBranch(name)) {
      ibranch->fBranch = branch;
      if (!ibranch->SetBranchAddress<T>()) return nullptr;
    } else {
      if (verbose() >= 0) Error (tname<T>("Get"), "branch '%s' not found", name);
      return nullptr;
    }
  }
  Int_t nread = ibranch->GetBranch (index, localIndex);
  if (nread < 0) return nullptr;
#ifndef NO_BranchValue_STATS
  fTotRead += nread;
#endif
  return ibranch;
}


inline TTreeIterator::BranchValue* TTreeIterator::GetBranchValue (const char* name, type_code_t type) const {
  if (fTryLast) {
    ++fLastBranch;
    if (fLastBranch == fBranches.end()) fLastBranch = fBranches.begin();
    BranchValue& b = *fLastBranch;
    if (b.fType == type && b.fName == name) {
#ifndef NO_BranchValue_STATS
      ++fNhits;
#endif
      return &b;
    }
  }
  for (auto ib = fBranches.begin(); ib != fBranches.end(); ib++) {
    if (fTryLast && ib == fLastBranch) continue;    // already checked this one
    BranchValue& b = *ib;
    if (b.fType == type && b.fName == name) {
      fTryLast = true;
      fLastBranch = ib;
#ifndef NO_BranchValue_STATS
      ++fNmiss;
#endif
      return &b;
    }
  }
  fTryLast = false;
  return nullptr;
}


template <typename T>
inline TTreeIterator::BranchValue* TTreeIterator::GetBranchValue (const char* name) const {
  using V = remove_cvref_t<T>;
  BranchValue* ibranch = GetBranchValue (name, type_code<T>());
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
    Info (tname<T>("GetBranchValue"), "found%s%s branch '%s' of type '%s' @%p", (ibranch->fHaveAddr?"":" bad"), user, name, type_name<T>(), addr);
  }
#endif
  return ibranch;
}


template <typename T>
inline TTreeIterator::BranchValue* TTreeIterator::NewBranch (const char* name, Long64_t index, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  using V = remove_cvref_t<T>;
  TBranch* branch = GetTree() ? GetTree()->GetBranch(name) : nullptr;
  Long64_t nentries = (branch ? branch->GetEntries() : 0);
  BranchValue* ibranch;
  if (index <= nentries) {
    ibranch = NewBranchValue<T> (name, std::forward<T>(val));
  } else {
    V def = type_default<V>();
    ibranch = NewBranchValue<T> (name, std::forward<T>(def));
  }
  if (ibranch) {
    ibranch->fBranch = branch;
    ibranch->CreateBranch<T> (leaflist, bufsize, splitlevel);
    if (index > nentries) {
      if (verbose() >= 1) Info (tname<T>("Set"), "branch '%s' catch up %lld entries", name, index);
      for (Long64_t i = nentries; i < index; i++) {
        FillBranch<T> (ibranch->fBranch, name, index);
      }
      ibranch->Set<T>(std::forward<T>(val));
    }
  }
  return ibranch;
}


template <typename T>
inline TTreeIterator::BranchValue* TTreeIterator::NewBranchValue (const char* name, T&& val) const {
  fBranches.reserve (200);   // when we reallocate, SetBranchAddress will be invalidated so have to fix up each time. This is ignored after the first call.
  BranchValue* front = &fBranches.front();
  fBranches.emplace_back (*const_cast<TTreeIterator*>(this), name, std::forward<T>(val));
  if (front != &fBranches.front()) SetBranchAddressAll();  // vector data() moved
  return &fBranches.back();
}


inline void TTreeIterator::SetBranchAddressAll() const {
  if (verbose() >= 1) Info  ("SetBranchAddressAll", "cache reallocated, so need to set all branch addresses again");
  for (auto& b : fBranches) {
    if (b.fSetValueAddress && b.fHaveAddr
#ifndef OVERRIDE_BRANCH_ADDRESS
        && !b.fPuser
#endif
       ) {
      (*b.fSetValueAddress) (&b, true);
    }
  }
}


template <typename T>
inline Int_t TTreeIterator::FillBranch (TBranch* branch, const char* name, Long64_t index) {
  Int_t nbytes = branch->Fill();
  if (nbytes > 0) {
    fTotFill += nbytes;
    if (verbose() >= 2) Info  (tname<T>("Set"), "filled branch '%s' with %d bytes for entry %lld", name, nbytes, index);
  } else if (nbytes == 0) {
    if (verbose() >= 0) Error (tname<T>("Set"), "no data filled in branch '%s' for entry %lld",    name,         index);
  } else {
    if (verbose() >= 0) Error (tname<T>("Set"), "error filling branch '%s' for entry %lld",        name,         index);
  }
  return nbytes;
}


// TTreeIterator::Entry ========================================================

template <typename T>
inline const T& TTreeIterator::Entry::Get (const char* name, const T& def) const {
  if (BranchValue* ibranch = tree().GetBranch<T> (name, fIndex, fLocalIndex))
    return ibranch->Get<T>(def);
  else
    return def;
}


template <typename T>
inline const T& TTreeIterator::Entry::Set (const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  using V = remove_cvref_t<T>;
  if (BranchValue* ibranch = tree().GetBranchValue<T> (name)) {
    return ibranch->Set<T>(std::forward<T>(val));
  }
  BranchValue* ibranch = tree().NewBranch<T> (name, fIndex, std::forward<T>(val), leaflist, bufsize, splitlevel);
  if (!ibranch) return default_value<V>();
  return ibranch->GetValue<V>();
}


// TTreeIterator::BranchValue ==================================================

template <typename T>
inline TTreeIterator::BranchValue::BranchValue (TTreeIterator& tree, const char* name, T&& value)
  : fName(name),
    fValue(std::forward<T>(value)),
    fTreeI(tree)
{
  using V = remove_cvref_t<T>;
  fType = type_code<V>();
  fSetDefaultValue = &BranchValue::SetDefaultValue<V>;
  fSetValueAddress = &BranchValue::SetValueAddress<V>;
}


template <typename T>
inline const T& TTreeIterator::BranchValue::Get(const T& def) const {
  const T* pval = GetBranchValue<T>();
  if (pval) return *pval;
  return def;
}


template <typename T>
inline const T& TTreeIterator::BranchValue::Set(T&& val) {
  using V = remove_cvref_t<T>;
  if (fHaveAddr) {
    fUnset = false;
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (!fPuser) {
#endif
#ifndef FEWER_CHECKS
      if (fPvalue && fPvalue != GetValuePtr<V>()) {
        if (verbose() >= 1) tree().Info (tname<T>("Set"), "branch '%s' object address changed from our @%p to @%p", GetName(), (void*)GetValuePtr<V>(), fPvalue);
#ifndef OVERRIDE_BRANCH_ADDRESS
        fPuser = &fPvalue;
#endif
      } else
#endif
        if (!fPvalue) {
//        if (verbose() >= 3) tree().Info (tname<T>("Set"), "branch '%s' assign value to @%p", GetName(), (void*)GetValuePtr<V>());
          return GetValue<V>() = std::forward<T>(val);
        } else {
          // This does std::any::emplace, which will reallocate the object if it is larger than sizeof(void*).
          // So must adjust pvalue to point to the new address.
          // In practice, this only occurs if PREFER_PTRPTR is defined.
          T& setval = SetValue<T>(std::forward<T>(val));
          if (fPvalue != &setval) {
//          if (verbose() >= 3) tree().Info (tname<T>("Set"), "branch '%s' object address changed when set from @%p to @%p", GetName(), (void*)&setval, fPvalue);
            fPvalue = &setval;
          }
          return setval;
        }
#ifndef OVERRIDE_BRANCH_ADDRESS
    }
    if (fIsObj) {
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
inline void TTreeIterator::BranchValue::CreateBranch (const char* leaflist, Int_t bufsize, Int_t splitlevel) {
  using V = remove_cvref_t<T>;
  if (!GetTree()) {
    if (verbose() >= 0) tree().Error (tname<T>("Set"), "no tree available");
    return;
  }
  if (fBranch) {
    if (verbose() >= 1) tree().Info (tname<T>("Set"), "new branch '%s' of type '%s' already exists @%p", GetName(), type_name<T>(), (void*)GetValuePtr<V>());
    if (!SetBranchAddress<V>()) return;
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (fPuser) Set<T> (std::forward<T>(GetValue<T>()));
#endif
  } else if (leaflist && *leaflist) {
    V* pvalue = GetValuePtr<V>();
    fBranch = GetTree()->Branch (GetName(), pvalue, leaflist, bufsize);
    if (!fBranch) {
      if (verbose() >= 0) tree().Error (tname<T>("Set"), "failed to create branch '%s' with leaves '%s' of type '%s'", GetName(), leaflist, type_name<T>());
      return;
    }
    if   (verbose() >= 1) tree().Info  (tname<T>("Set"), "create branch '%s' with leaves '%s' of type '%s' @%p",       GetName(), leaflist, type_name<T>(), (void*)pvalue);
  } else {
    V* pvalue = GetValuePtr<V>();
    void* addr;
#ifdef PREFER_PTRPTR
    if (TClass::GetClass<V>()) {  // shouldn't have to use **T for objects, but maybe it's more reliable?
      fIsObj = true;
      fPvalue = pvalue;
      addr = &fPvalue;
      fBranch = GetTree()->Branch (GetName(), (V**)addr, bufsize, splitlevel);
    } else
#endif
    {
      addr = pvalue;
      fBranch = GetTree()->Branch (GetName(),    pvalue, bufsize, splitlevel);
    }
    if (!fBranch) {
      if (verbose() >= 0) tree().Error (tname<T>("Set"), "failed to create branch '%s' %s of type '%s'", GetName(), (fIsObj?"object":"variable"), type_name<T>());
      return;
    }
    if   (verbose() >= 1) tree().Info  (tname<T>("Set"), "create branch '%s' %s of type '%s' @%p",       GetName(), (fIsObj?"object":"variable"), type_name<T>(), addr);
  }
  fHaveAddr = true;
}


inline Int_t TTreeIterator::BranchValue::GetBranch (Long64_t index, Long64_t localIndex) const {
  if (!fHaveAddr) return -1;
#ifndef OVERRIDE_BRANCH_ADDRESS
  if (fPuser) return 0;  // already read value
#endif
  if (fLastGet == index) {
    if (verbose() >= 3) tree().Info  ("GetBranch", "branch '%s' already read from entry %lld",           GetName(),        index);
    return 0;
  }
  Int_t nread = fBranch->GetEntry (localIndex, 1);
  if (nread < 0) {
    if (verbose() >= 0) tree().Error ("GetBranch", "GetEntry failed for branch '%s', entry %lld (%lld)", GetName(),        index, localIndex);
  } else if (nread == 0) {
    if (verbose() >= 0) tree().Error ("GetBranch", "branch '%s' read %d bytes from entry %lld (%lld)",   GetName(), nread, index, localIndex);
  } else {
    if (verbose() >= 1) tree().Info  ("GetBranch", "branch '%s' read %d bytes from entry %lld (%lld)",   GetName(), nread, index, localIndex);
    fLastGet = index;
    return nread;
  }
  fLastGet = -1;
  return -1;
}


template <typename T>
inline const T* TTreeIterator::BranchValue::GetBranchValue() const {
  if (fHaveAddr) {
#ifndef OVERRIDE_BRANCH_ADDRESS
    if (!fPuser) {
#endif
      const T* pvalue = GetValuePtr<T>();
#ifndef FEWER_CHECKS
      if (fPvalue && fPvalue != pvalue) {
        if (verbose() >= 1) tree().Info (tname<T>("Get"), "branch '%s' object address changed from our @%p to @%p", GetName(), (void*)pvalue, fPvalue);
#ifndef OVERRIDE_BRANCH_ADDRESS
        fPuser = &fPvalue;
#endif
      } else
#endif
        return pvalue;
#ifndef OVERRIDE_BRANCH_ADDRESS
    }
    if (fIsObj) {
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
inline bool TTreeIterator::BranchValue::SetBranchAddress() {
  TBranch* branch = fBranch;
  TClass* cls = TClass::GetClass<T>();
  if (cls && branch->GetMother() == branch) {
    TClass* expectedClass = 0;
    EDataType expectedType = kOther_t;
    if (!branch->GetExpectedType (expectedClass, expectedType)) {
      if (expectedClass) fIsObj = true;
    } else {
      if (verbose() >= 1) tree().Info (tname<T>("SetBranchAddress"), "GetExpectedType failed for branch '%s'", GetName());
    }
  }
#ifndef OVERRIDE_BRANCH_ADDRESS
  if (!tree().fOverrideBranchAddress) {
    void* addr = branch->GetAddress();
    if (addr && !fBranch->TestBit(kDoNotProcess)) {
      EDataType type = (!cls) ? TDataType::GetType(typeid(T)) : kOther_t;
      Int_t res = TTreeProtected::Access(*GetTree()) . CheckBranchAddressType (branch, cls, type, fIsObj);
      if (res < 0) {
        if (verbose() >= 0) tree().Error (tname<T>("SetBranchAddress"), "branch '%s' %s existing address %p wrong type", GetName(), (fIsObj?"object":"variable"), addr);
        return false;
      }
      if   (verbose() >= 1) tree().Info  (tname<T>("SetBranchAddress"), "use branch '%s' %s existing address %p",        GetName(), (fIsObj?"object":"variable"), addr);
      fPuser = (void**)addr;
      fHaveAddr = true;
      return true;
    }
  }
#endif
  return SetValueAddress<T> (this);
}


inline void TTreeIterator::BranchValue::ResetAddress() {
  if (fBranch && fHaveAddr
#ifndef OVERRIDE_BRANCH_ADDRESS
      && !fPuser
#endif
     )
    fBranch->ResetAddress();
}


template <typename T>
inline /*static*/ void TTreeIterator::BranchValue::SetDefaultValue (BranchValue* ibranch) {
  using V = remove_cvref_t<T>;
  if (ibranch->verbose() >= 1) ibranch->tree().Info (tname<T>("Set"), "branch '%s' value was not set - use type's default", ibranch->GetName());
  ibranch->Set<T>(type_default<V>());
}


// this is a static function, so we can store its address without needing a 16-byte member function pointer
template <typename T>
inline /*static*/ bool TTreeIterator::BranchValue::SetValueAddress (BranchValue* ibranch, bool redo/*=false*/) {
  T* pvalue= ibranch->GetValuePtr<T>();
  Int_t stat=0;
  void* addr;
  if (ibranch->fIsObj) {
    ibranch->fPvalue = pvalue;
    addr = &ibranch->fPvalue;
    if (!redo)
      stat = ibranch->GetTree()->SetBranchAddress (ibranch->GetName(), (T**)(addr));
  } else {
    redo=false;  // only helps for objects
    addr = pvalue;
    stat = ibranch->GetTree()->SetBranchAddress (ibranch->GetName(), pvalue);
  }
  if (stat < 0) {
    if (ibranch->verbose() >= 0) ibranch->tree().Error (tname<T>("SetValueAddress"), "failed to set branch '%s' %s address %p", ibranch->GetName(), (ibranch->fIsObj?"object":"variable"), addr);
    ibranch->fHaveAddr = false;
    return false;
  }
  if   (ibranch->verbose() >= 1) ibranch->tree().Info  (tname<T>("SetValueAddress"), "set branch '%s' %s address %p%s",         ibranch->GetName(), (ibranch->fIsObj?"object":"variable"), addr, (redo?" (pointer only)":""));
  ibranch->fHaveAddr = true;
  return true;
}

#endif /* ROOT_TTreeIterator_detail */
