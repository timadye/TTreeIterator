// A TTreeIterator allows iterator and member access to the elements of a TTree.
// Created by Tim Adye on 26/03/2021.

#ifndef ROOT_TTreeIterator
#define ROOT_TTreeIterator

#include <string>
#include <iterator>
#include <utility>
#include <map>
#include <any>

#include "TTree.h"

#include "TTreeIterator_helpers.h"

class TDirectory;

// define these for fastest speed
//#define FEWER_CHECKS 1             // skip sanity/debug checks if on every entry
//#define OVERRIDE_BRANCH_ADDRESS 1  // override any other user SetBranchAddress settings
//#define USE_OrderedMap

class TTreeIterator : public TNamed {
public:
  template <typename K, typename V> using branch_map_type =
#ifdef USE_OrderedMap
    OrderedMap<K,V>;
#else
    std::map<K,V>;
#endif

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
      fVerbose(verbose)
  { SetBranchStatusAll(false); }

  ~TTreeIterator() override { if (fTreeOwned) delete fTree; }

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

  std::string BranchNamesString (bool include_children=true, bool include_inactive=false);
  std::vector<std::string> BranchNames (bool include_children=false, bool include_inactive=false);

  // Forwards to TTree with some extra
  virtual TTreeIterator& GetEntry();
  virtual Int_t Fill();
  Int_t Write (const char* name=0, Int_t option=0, Int_t bufsize=0) override;

  // Accessors
  TTreeIterator& setIndex   (Long64_t index) { fIndex   = index;   return *this; }
  TTreeIterator& setVerbose (int    verbose) { fVerbose = verbose; return *this; }
  Long64_t index()   const { return fIndex;   }
  int      verbose() const { return fVerbose; }

  virtual void reset();

  // std::iterator interface
  iterator begin();
  iterator end();
  fill_iterator FillEntries (Long64_t nfill=-1);

  // Create empty branch
  template <typename T>
  TBranch* Branch (const char* name) {
    using V = typename std::remove_reference<T>::type;
    return Branch<T> (name, GetLeaflist<V>(), fBufsize, fSplitlevel);
  }

  template <typename T>
  TBranch* Branch (const char* name, const char* leaflist, Int_t bufsize, Int_t splitlevel);

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
  const T& Set(const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel);

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

  template <typename T> const T& Get(const char* name, const T& val) const;

  // Set the status for a branch and all its sub-branches.
  void SetBranchStatusAll (bool status=true, bool include_children=true) {
    SetBranchStatus (fTree->GetListOfBranches(), status, include_children, fVerbose);
  }

  // Convenience function to return the type name
  template <typename T> static const char* tname(const char* name=0);
  template <typename T> static T           type_default() { return T();                   }
  template <typename T> static const char* GetLeaflist()  { return GetLeaflistImpl<T>(0); }

private:
  template <typename T> static decltype(T::leaflist) GetLeaflistImpl(int)  { return T::leaflist; }
  template <typename T> static const char*           GetLeaflistImpl(long) { return nullptr;     }

protected:

  // internal methods
  void Init (TDirectory* dir=nullptr);
  template <typename T> BranchInfo* GetBranch(const char* name) const;
  template <typename T> BranchInfo* GetBranchInfo (const char* name) const;
  template <typename T> const T& SetValue (BranchInfo* ibranch, const char* name, T&& val);
  template <typename T> BranchInfo* NewBranch (const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel);
  template <typename T> BranchInfo* SetBranchInfo (const char* name, T&& val) const;
  template <typename T> bool SetBranchAddress (BranchInfo* ibranch, const char* name, const char* call="Get") const;
  template <typename T> Int_t FillBranch (TBranch* branch, const char* name);
  static void SetBranchStatus (TObjArray* list, bool status=true, bool include_children=true, int verbose=0, const std::string* pre=nullptr);
  static void SetBranchStatus (TBranch* branch, bool status=true, bool include_children=true, int verbose=0, const std::string* pre=nullptr);
  static void BranchNames (std::vector<std::string>& allbranches, TObjArray* list, bool include_children, bool include_inactive, const std::string& pre="");

  // Member variables
  Long64_t fIndex=0;
  mutable branch_map_type<std::pair<std::string,size_t>,BranchInfo> fBranches;
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

#include "TTreeIterator_detail.h"

#endif /* ROOT_TTreeIterator */
