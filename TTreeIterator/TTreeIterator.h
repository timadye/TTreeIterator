// A TTreeIterator allows iterator and member access to the elements of a TTree.
// Created by Tim Adye on 26/03/2021.

#ifndef ROOT_TTreeIterator
#define ROOT_TTreeIterator

#include <string>
#include <vector>
#include <iterator>
#include <utility>

#include "TTree.h"

class TDirectory;

// define some different implementation methods to compare for speed
//#define FEWER_CHECKS 1             // skip sanity/debug checks on every entry
//#define OVERRIDE_BRANCH_ADDRESS 1  // override any other user SetBranchAddress settings
//#define PREFER_PTRPTR 1            // for filling ROOT objects, tree->Branch() uses **obj, rather than *obj
//#define NO_FILL_UNSET_DEFAULT 1    // don't set default values if unset
//#define NO_BranchInfo_STATS 1         // Don't keep stats for optimised BranchInfo lookup. Otherwise, prints in ~TTreeIterator if verbose.
//#define USE_std_any 1              // use C++17's std::any, instead of Cpp11::any from detail/Cpp11_any.h
//#define Cpp11_any_NOOPT 1          // don't use Cpp11::any's optimisations (eg. removing error checking)
//#define NO_DICT 1                  // don't create TTreeIterator dictionary

#if defined(USE_std_any) && (__cplusplus < 201703L)   // <version> not available until GCC9, so no way to check __cpp_lib_any without including <any>.
# undef USE_std_any                  // only option is to use Cpp11::any
# define Cpp11_any_NOOPT 1
#endif

#ifndef USE_std_any
# ifndef Cpp11_any_NOOPT
#  define Cpp11_any_OPTIMIZE 1
# endif
# include "TTreeIterator/detail/Cpp11_any.h"  // Implementation of std::any, compatible with C++11.
#else
# include <any>
#endif

#if defined(USE_std_any) || defined(Cpp11_std_any)
namespace any_namespace = ::std;
#else
namespace any_namespace = Cpp11;
#endif


#include "TTreeIterator/detail/TTreeIterator_helpers.h"

class TTreeIterator : public TNamed {
public:

#ifndef USE_std_any
  using any_type = any_namespace::any;
  using type_code_t = any_namespace::any_type_code;
  template<typename T> static constexpr type_code_t type_code() { return any_namespace::type_code<T>(); }
#else
  using any_type = std::any;
  using type_code_t = std::size_t;
  template<typename T> static constexpr type_code_t type_code() { return typeid(T).hash_code(); }
#endif

  class Entry;
  class Entry_iterator;
  class Fill_iterator;
  // ===========================================================================
  class BranchInfo {
  public:

    const std::string& GetName() const { return fName; }
    type_code_t        GetType() const { return fType; }

    template <typename T> operator const T&() const { return Get<T>(); }
    template <typename T> operator       T&() const { return Get<T>(); }
//  template <typename T> T operator+ (const T& v) const { return T(*this) +  v; }
//  template <typename T> T operator+=(const T& v)       { return T(*this) += v; }
    template <typename T> const T& operator= (T&& val) { return Set<T>(std::forward<T>(val)); }

    // Get value, returning a reference
    template <typename T> T& Get() const { return Get<T>(default_value<remove_cvref_t<T>>()); }

    // Get() allowing the default value (returned if there is an error) to be specified.
    template <typename T> T& Get(T& def) const { return const_cast<T&> (Get<T> (const_cast<const T&>(def))); }
    template <typename T> const T& Get(const T& def) const;
    template <typename T> const T& Set(T&& val);

    Long64_t         index()   const { return fEntry.index();   }
    int              verbose() const { return fEntry.verbose(); }
    Entry&           entry()   const { return fEntry;           }
    Entry_iterator&  iter()    const { return fEntry.iter();    }
    TTreeIterator&   tree()    const { return fEntry.tree();    }
    TTree*           GetTree() const { return fEntry.GetTree(); }

  protected:
    friend Entry_iterator;
    friend Fill_iterator;
    friend Entry;

    // member function pointer definition to allow access to templated code
    typedef bool (*SetValueAddress_t) (BranchInfo* ibranch, const char* call, bool redo);
    typedef void (*SetDefaultValue_t) (BranchInfo* ibranch);

    std::string  fName;
    type_code_t  fType;
    any_type fValue;
    mutable void*    fPvalue = nullptr;
#ifndef OVERRIDE_BRANCH_ADDRESS
    mutable void**   fPuser  = nullptr;
#endif
    TBranch* fBranch = nullptr;
    Entry&   fEntry;
    SetDefaultValue_t fSetDefaultValue = 0;  // function to set value to the default
    SetValueAddress_t fSetValueAddress = 0;  // function to set the address again
    bool     fSet    = false;
    bool     fUnset  = false;
    bool     fIsobj  = false;
    bool     fWasDisabled = false;
    void  Enable()      { if ( (fWasDisabled = fBranch->TestBit(kDoNotProcess))) SetBranchStatus ( true); }
    void  EnableReset() { if (  fWasDisabled)                                    SetBranchStatus (false); }
    void Disable()      { if (!(fWasDisabled = fBranch->TestBit(kDoNotProcess))) SetBranchStatus (false); }
    void DisableReset() { if (! fWasDisabled)                                    SetBranchStatus ( true); }
    void SetBranchStatus (bool status=true) { TTreeIterator::SetBranchStatus (fBranch, status, true, verbose()); }
    void ResetAddress() {
      if (fBranch && fSet
#ifndef OVERRIDE_BRANCH_ADDRESS
          && !fPuser
#endif
         ) fBranch->ResetAddress();
    }
  public:
    // delete unneeded initialisers so we don't accidentally call them
    BranchInfo() = delete;
    BranchInfo(BranchInfo&&) = delete;
    BranchInfo& operator=(BranchInfo&&) = delete;
    BranchInfo& operator=(const BranchInfo&) = delete;
    // need these initializers to use vector<BranchInfo>
    BranchInfo(const BranchInfo&) = default;
    ~BranchInfo() = default;
    template <typename T> BranchInfo(const char* name, type_code_t type, T&& value, Entry& entry, SetDefaultValue_t fd, SetValueAddress_t fa)
      : fName(name), fType(type), fValue (std::forward<T>(value)), fEntry(entry), fSetDefaultValue(fd), fSetValueAddress(fa) {}
  protected:
    template <typename T>       T& SetValue(T&& value) { return fValue.emplace<T>(std::forward<T>(value)); }
    template <typename T> const T& GetValue()    const { return any_namespace::any_cast<T&>(fValue); }
    template <typename T>       T& GetValue()          { return any_namespace::any_cast<T&>(fValue); }
    template <typename T> const T* GetValuePtr() const { return any_namespace::any_cast<T>(&fValue); }
    template <typename T>       T* GetValuePtr()       { return any_namespace::any_cast<T>(&fValue); }

    template <typename T> const T* GetBranchValue() const;
    template <typename T> bool SetBranchAddress (const char* call="Get");

    template <typename T> static void SetDefaultValue (BranchInfo* ibranch);
    template <typename T> static bool SetValueAddress (BranchInfo* ibranch, const char* call, bool redo=false);
  };

  // ===========================================================================
  // Interface to std::iterator to allow loop over branches
  class BranchInfo_iterator
    : public std::iterator< std::forward_iterator_tag, // iterator_category   [could easily be random_access_iterator if we implemented the operators]
                            BranchInfo,                // value_type
                            Long64_t,                  // difference_type
                            const BranchInfo*,         // pointer
                            const BranchInfo& >        // reference
  {
/*
  public:
    BranchInfo_iterator (TTreeIterator& entry, Long64_t first, Long64_t last) : fIndex(first), fEnd(last), fEntry(entry) {}
//  BranchInfo_iterator (const BranchInfo_iterator& in) : fIndex(in.fIndex), fEnd(in.fEnd), fEntry(in.fEntry) {}  // default probably OK
    ~BranchInfo_iterator() { fEntry.reset(); }
    BranchInfo_iterator& operator++() { ++fIndex; return *this; }
    BranchInfo_iterator  operator++(int) { BranchInfo_iterator it = *this; ++fIndex; return it; }
    bool operator!= (const BranchInfo_iterator& other) const { return fIndex != other.fIndex; }
    bool operator== (const BranchInfo_iterator& other) const { return fIndex == other.fIndex; }
    const TTreeIterator& operator*() const { return fIndex < fEnd ? fEntry.setIndex(fIndex).GetEntry() : fEntry; }
    Long64_t first()  { return fIndex; }
    Long64_t last()   { return fEnd;   }
  protected:
    const Long64_t fEnd;
    TTreeIterator& fEntry;   // TTreeIterator also handles the entry
*/
  };

  // ===========================================================================

  // Wrapper class to provide return-type deduction
  class Getter {
  public:
    Getter(const Entry& entry, const char* name) : fEntry(entry), fName(name) {}
    template <typename T> operator const T&() const { return fEntry.Get<T>(fName); }
    template <typename T> operator T&() const { return fEntry.Get<T>(fName); }
//  template <typename T> T operator+ (const T& v) const { return T(*this) +  v; }
//  template <typename T> T operator+=(const T& v)       { return T(*this) += v; }
  protected:
    const Entry& fEntry;
    const char* fName;
  };

  // Wrapper class to allow setting through operator[]
  class Setter : public Getter {
  public:
    Setter(Entry& entry, const char* name) : Getter(entry,name) {}
    template <typename T> const T& operator= (T&& val) { return const_cast<Entry&>(fEntry).Set<T>(fName, std::forward<T>(val)); }
  };

  // ===========================================================================
  class Entry {
  public:
    using key_type    = std::string;
    using mapped_type = BranchInfo;
    using value_type  = BranchInfo;
    using iterator    = BranchInfo_iterator;
    using reference   = value_type&;
    using pointer     = value_type*;
    using key_compare = std::less<key_type>;
    using size_type   = std::size_t;
    using difference_type = std::ptrdiff_t;

    Entry (Entry_iterator& iter, Long64_t index=0) : fIndex(index), fIter(iter) {}
    ~Entry();

    Getter Get        (const char* name) const { return Getter(*this,name); }
    Getter operator[] (const char* name) const { return Getter(*this,name); }
    Setter operator[] (const char* name)       { return Setter(*this,name); }   // Setter can also do Get for non-const this

    // Get value, returning a reference
    template <typename T> T& Get(const char* name) const { return const_cast<T&> (Get<T>(name, default_value<remove_cvref_t<T>>())); }

    // Get() allowing the default value (returned if there is an error) to be specified.
    template <typename T> T& Get(const char* name, T& val) const { return const_cast<T&> (Get<T> (name, const_cast<const T&>(val))); }
    template <typename T> const T& Get(const char* name, const T& val) const;

    template <typename T>
    const T& Set(const char* name, T&& val) {
      return Set<T> (name, std::forward<T>(val), GetLeaflist<remove_cvref_t<T>>(), tree().fBufsize, tree().fSplitlevel);
    }

    template <typename T>
    const T& Set(const char* name, T&& val, const char* leaflist) {
      return Set<T> (name, std::forward<T>(val), leaflist, tree().fBufsize, tree().fSplitlevel);
    }

    template <typename T>
    const T& Set(const char* name, T&& val, const char* leaflist, Int_t bufsize) {
      return Set<T> (name, std::forward<T>(val), leaflist, bufsize, tree().fSplitlevel);
    }

    template <typename T>
    const T& Set(const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel);

    /*
    BranchInfo_iterator begin () { return BranchInfo_iterator (fEntry, fIndex, fEnd); }
    BranchInfo_iterator end ()   { return BranchInfo_iterator (fEntry, fEnd,   fEnd); }
    */
    Int_t GetEntry (Int_t getall=0) {
      Int_t nbytes = tree().GetEntry (fIndex, getall);
      if (nbytes>0) iter().fTotRead += nbytes;
      return nbytes;
    }
    Int_t Fill();  // not in Fill_iterator, so accessible from Entry

    Long64_t          index()   const { return fIndex;          }
    int               verbose() const { return fIter.verbose(); }
    Entry_iterator&   iter()    const { return fIter;           }
    TTreeIterator&    tree()    const { return fIter.tree();    }
    TTree*            GetTree() const { return fIter.GetTree(); }

  protected:
    friend Entry_iterator;
    friend Fill_iterator;
    friend BranchInfo;

    template <typename T> BranchInfo* GetBranch     (const char* name) const;
    template <typename T> BranchInfo* GetBranchInfo (const char* name) const;
                          BranchInfo* GetBranchInfo (const char* name, type_code_t type) const;
    template <typename T> BranchInfo* NewBranch     (const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel);
    template <typename T> BranchInfo* SetBranchInfo (const char* name, T&& val) const;
    template <typename T> Int_t       FillBranch    (TBranch* branch, const char* name);
    void SetBranchAddressAll (const char* call="SetBranchInfo") const;

    // Create empty branch
    template <typename T>
    TBranch* Branch (const char* name) {
      return Branch<T> (name, GetLeaflist<remove_cvref_t<T>>(), tree().fBufsize, tree().fSplitlevel);
    }
    template <typename T> TBranch* Branch (const char* name, const char* leaflist, Int_t bufsize, Int_t splitlevel);

    Long64_t fIndex;
    Entry_iterator& fIter;

    mutable std::vector<BranchInfo> fBranches;
    mutable std::vector<BranchInfo>::iterator fLastBranch;
    mutable bool fTryLast = false;
  };

  // ===========================================================================
  // Interface to std::iterator to allow range-based for loop
  class Entry_iterator
    : public std::iterator< std::forward_iterator_tag, // iterator_category   [could easily be random_access_iterator if we implemented the operators]
                            Entry,                     // value_type
                            Long64_t,                  // difference_type
                            const Entry*,              // pointer
                            const Entry& >             // reference
  {
  public:

    Entry_iterator (TTreeIterator& treeI, Long64_t first, Long64_t last) : fIndex(first), fEnd(last), fTreeI(treeI), fEntry(*this,0) {}
//  Entry_iterator (const Entry_iterator& in) : fIndex(in.fIndex), fEnd(in.fEnd), fTreeI(in.fTreeI) {}  // default probably OK
    ~Entry_iterator();
    Entry_iterator& operator++() { ++fIndex; return *this; }
    Entry_iterator  operator++(int) { Entry_iterator it = *this; ++fIndex; return it; }
    bool operator!= (const Entry_iterator& other) const { return fIndex != other.fIndex; }
    bool operator== (const Entry_iterator& other) const { return fIndex == other.fIndex; }
    const Entry& operator*() const { fEntry.fIndex = fIndex < fEnd ? fIndex : -1; fEntry.GetEntry(); return fEntry; }
    Long64_t last() { return fEnd; }

    Long64_t       index()   const { return fIndex;           }
    int            verbose() const { return tree().fVerbose;  }
    TTreeIterator& tree()    const { return fTreeI;           }
    TTree*         GetTree() const { return tree().GetTree(); }

  protected:
    friend Entry;

    Long64_t fIndex;
    const Long64_t fEnd;
    TTreeIterator& fTreeI;
    mutable Entry fEntry;   // local copy so we can return it by reference

    mutable ULong64_t fTotFill=0, fTotWrite=0, fTotRead=0;
#ifndef NO_BranchInfo_STATS
    mutable size_t fNhits=0, fNmiss=0;
#endif
    bool fWriting=false;
  };

  // ===========================================================================
  class Fill_iterator : public Entry_iterator {
  public:
    Fill_iterator (TTreeIterator& treeI, Long64_t first, Long64_t last) : Entry_iterator(treeI,first,last) {}
    ~Fill_iterator() { Write(); }
    Fill_iterator& operator++() { ++fIndex; return *this; }
    Fill_iterator  operator++(int) { Fill_iterator it = *this; ++fIndex; return it; }
    Entry& operator*() const { fEntry.fIndex = fIndex; return fEntry; }

    Fill_iterator begin() { return Fill_iterator (fTreeI, fIndex, fEnd); }
    Fill_iterator end()   { return Fill_iterator (fTreeI, fEnd,   fEnd); }

    Int_t Write (const char* name=0, Int_t option=0, Int_t bufsize=0);
  };

  // ===========================================================================

  using value_type  = Entry;
  using iterator    = Entry_iterator;
  using reference   = value_type&;
  using pointer     = value_type*;
  using size_type   = std::size_t;
  using difference_type = std::ptrdiff_t;

  // ===========================================================================
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
      fVerbose(verbose)
  { Init(0,false); }

  ~TTreeIterator() override;

  // Access to underlying tree
  TTree* operator->() const { return GetTree(); }
  TTree* GetTree() const { return fTree; }
  TTree* SetTree (TTree* tree);

  // Forwards to TTree
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
  Int_t GetBufsize()    const             { return       fBufsize;         }
  Int_t GetSplitlevel() const             { return       fSplitlevel;      }
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
  virtual Int_t GetEntry (Long64_t index, Int_t getall=0);

  // use a TChain
  Int_t Add (const char* name, Long64_t nentries=TTree::kMaxEntries);

  // Accessors
  TTreeIterator& setVerbose (int    verbose) { fVerbose = verbose; return *this; }
  int verbose() const { return fVerbose; }

  // std::iterator interface
  Entry_iterator begin();
  Entry_iterator end();
  Fill_iterator FillEntries (Long64_t nfill=-1);

  // Set the status for a branch and all its sub-branches.
  void SetBranchStatusAll (bool status=true, bool include_children=true) {
    SetBranchStatus (fTree->GetListOfBranches(), status, include_children, fVerbose);
  }

  // Convenience function to return the type name
  template <typename T> static const char* tname(const char* name=0);
  template <typename T> static T type_default() { return T(); }
  template <typename T> static const char* GetLeaflist()  { return GetLeaflistImpl<T>(0); }

  template <typename T> static T& default_value() {
    static T def = type_default<T>();   // static default value for each type to allow us to return by reference (NB. sticks around until program exit)
    return def;
  }

// ====================================================================
// Implementation details
// ====================================================================

private:
  template <typename T> static decltype(T::leaflist) GetLeaflistImpl(int)  { return T::leaflist; }
  template <typename T> static const char*           GetLeaflistImpl(long) { return nullptr;     }

protected:

  // remove_cvref_t (std::remove_cvref_t for C++11).
  template<typename T> using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

  // internal methods
  void Init (TDirectory* dir=nullptr, bool owned=true);
  static void SetBranchStatus (TObjArray* list, bool status=true, bool include_children=true, int verbose=0, const std::string* pre=nullptr);
  static void SetBranchStatus (TBranch* branch, bool status=true, bool include_children=true, int verbose=0, const std::string* pre=nullptr);
  static void BranchNames (std::vector<std::string>& allbranches, TObjArray* list, bool include_children, bool include_inactive, const std::string& pre="");

  // Hack to allow access to protected method TTree::CheckBranchAddressType()
  struct TTreeProtected : public TTree {
    static TTreeProtected& Access (TTree& t) { return (TTreeProtected&) t; }
    using TTree::CheckBranchAddressType;
  };

  // Member variables
  TTree* fTree=nullptr;
  bool fTreeOwned=false;
  Int_t fBufsize=32000;
  Int_t fSplitlevel=99;
  int fVerbose=0;
#ifndef OVERRIDE_BRANCH_ADDRESS  // only need flag if compiled in
  bool fOverrideBranchAddress=false;
#endif

#ifndef NO_DICT
  ClassDefOverride(TTreeIterator,0);
#endif
};

template<> inline float         TTreeIterator::type_default() { return std::numeric_limits<float      >::quiet_NaN(); }
template<> inline double        TTreeIterator::type_default() { return std::numeric_limits<double     >::quiet_NaN(); }
template<> inline long double   TTreeIterator::type_default() { return std::numeric_limits<long double>::quiet_NaN(); }
template<> inline char          TTreeIterator::type_default() { return '#'; }
template<> inline int           TTreeIterator::type_default() { return -1;  }
template<> inline long int      TTreeIterator::type_default() { return -1;  }
template<> inline long long int TTreeIterator::type_default() { return -1;  }

#include "TTreeIterator/detail/TTreeIterator_detail.h"

#endif /* ROOT_TTreeIterator */
