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
//#define FEWER_CHECKS 1             // skip sanity/debug checks if on every entry
//#define OVERRIDE_BRANCH_ADDRESS 1  // override any other user SetBranchAddress settings
//#define PREFER_PTRPTR 1            // for ROOT objects, tree->Branch() gives **obj, rather than *obj
//#define NO_FILL_UNSET_DEFAULT 1    // don't set default values if unset
//#define USE_map 1                  // BranchInfo container is a std::map (otherwise use a std::vector)
//#define USE_OrderedMap 1           // BranchInfo container is an OrderedMap from TTreeIterator_helpers.h
//#define NO_BranchInfo_STATS 1      // Don't keep stats for optimised BranchInfo lookup. Otherwise, prints in ~TTreeIterator if verbose.
//#define USE_std_any 1              // use C++17's std::any, instead of Cpp11::any from detail/Cpp11_any.h
//#define Cpp11_any_NOOPT 1          // don't use Cpp11::any's optimisations (mostly removing error checking)

#if defined(USE_OrderedMap) && !defined(USE_map)
# define USE_map 1
#endif

#if defined(USE_std_any) && (__cplusplus < 201703L)   // <version> not available until GCC9, so no way to check __cpp_lib_any without including <any>.
# undef USE_std_any                  // only option is to use Cpp11::any
# define Cpp11_any_NOOPT 1
#endif

#ifdef USE_OrderedMap
# ifndef NO_BranchInfo_STATS
#  define OrderedMap_STATS 1
#  define NO_BranchInfo_STATS 1
# else
# endif
# include "TTreeIterator/detail/OrderedMap.h"
#elif defined(USE_map)
# include <map>
#define NO_BranchInfo_STATS 1
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
    fill_iterator& operator++() { fIndex++; return *this; }
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
  { Init(0,false); }

  ~TTreeIterator() override;

  // Access to underlying tree
  TTree* operator->() const { return GetTree(); }
  TTree* GetTree() const { return fTree; }
  TTree* SetTree (TTree* tree);

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
  virtual TTreeIterator& GetEntry();
  virtual Int_t Fill();
  Int_t Write (const char* name=0, Int_t option=0, Int_t bufsize=0) override;

  // use a TChain
  Int_t Add (const char* name, Long64_t nentries=TTree::kMaxEntries);

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
    using V = remove_cvref_t<T>;
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
    using V = remove_cvref_t<T>;
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
    static T def = type_default<T>();   // static default value for each type to allow us to return by reference (NB. sticks around until program exit)
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

// ====================================================================
// Implementation details
// ====================================================================

private:
  template <typename T> static decltype(T::leaflist) GetLeaflistImpl(int)  { return T::leaflist; }
  template <typename T> static const char*           GetLeaflistImpl(long) { return nullptr;     }

protected:

  // BranchInfo definitions
  class BranchInfo;
#ifdef USE_OrderedMap
  template <typename K, typename V> using branch_map_type = OrderedMap<K,V>;
#else
  template <typename K, typename V> using branch_map_type = std::map<K,V>;
#endif
#ifndef USE_std_any
  using any_type = any_namespace::any;
  using type_code_t = any_namespace::any_type_code;
  template<typename T> static constexpr type_code_t type_code() { return any_namespace::type_code<T>(); }
#else
  using any_type = std::any;
  using type_code_t = std::size_t;
  template<typename T> static constexpr type_code_t type_code() { return typeid(T).hash_code(); }
#endif

  // remove_cvref_t (std::remove_cvref_t for C++11).
  template<typename T> using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

  // member function pointer definition to allow access to templated code
  typedef bool (*SetValueAddress_t) (const TTreeIterator& tt, BranchInfo* ibranch, const char* name, const char* call, bool redo);
  typedef void (*SetDefaultValue_t) (      TTreeIterator& tt, BranchInfo* ibranch, const char* name);

  // Our local cache of branch information
  struct BranchInfo {
#ifndef USE_map
    std::string  name;
    type_code_t  type;
#endif
    any_type value;
    void*    pvalue = nullptr;
#ifndef OVERRIDE_BRANCH_ADDRESS
    void**   puser  = nullptr;
#endif
    TBranch* branch = nullptr;
    SetDefaultValue_t SetDefaultValue = 0;  // function to set value to the default
#ifndef USE_map
    SetValueAddress_t SetValueAddress = 0;  // function to set the address again
#endif
    bool     set    = false;
    bool     unset  = false;
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
#ifndef USE_map
    template <typename T> BranchInfo(const char* nam, type_code_t typ, T&& val, SetDefaultValue_t fd, SetValueAddress_t fa)
      : name(nam), type(typ), value (std::forward<T>(val)), SetDefaultValue(fd), SetValueAddress(fa) {}
#else
    template <typename T> BranchInfo(T&& val, SetDefaultValue_t fd)
      :                       value (std::forward<T>(val)), SetDefaultValue(fd) {}
#endif
    template <typename T>       T& SetValue(T&& val)   { return value.emplace<T>(std::forward<T>(val)); }
    template <typename T> const T& GetValue()    const { return any_namespace::any_cast<T&>(value); }
    template <typename T>       T& GetValue()          { return any_namespace::any_cast<T&>(value); }
    template <typename T> const T* GetValuePtr() const { return any_namespace::any_cast<T>(&value); }
    template <typename T>       T* GetValuePtr()       { return any_namespace::any_cast<T>(&value); }
  };

  // internal methods
  void Init (TDirectory* dir=nullptr, bool owned=true);
  template <typename T> BranchInfo* GetBranch(const char* name) const;
  template <typename T> BranchInfo* GetBranchInfo (const char* name) const;
                        BranchInfo* GetBranchInfo (const char* name, type_code_t type) const;
  template <typename T> const T& SetValue (BranchInfo* ibranch, const char* name, T&& val);
  template <typename T> BranchInfo* NewBranch (const char* name, T&& val, const char* leaflist, Int_t bufsize, Int_t splitlevel);
  template <typename T> BranchInfo* SetBranchInfo (const char* name, T&& val) const;
  template <typename T> bool SetBranchAddress (BranchInfo* ibranch, const char* name, const char* call="Get") const;
  template <typename T> Int_t FillBranch (TBranch* branch, const char* name);
  template <typename T> static bool SetValueAddress (const TTreeIterator& tt, BranchInfo* ibranch, const char* name, const char* call, bool redo=false);
  template <typename T> static void SetDefaultValue (      TTreeIterator& tt, BranchInfo* ibranch, const char* name);
  static void SetBranchStatus (TObjArray* list, bool status=true, bool include_children=true, int verbose=0, const std::string* pre=nullptr);
  static void SetBranchStatus (TBranch* branch, bool status=true, bool include_children=true, int verbose=0, const std::string* pre=nullptr);
  static void BranchNames (std::vector<std::string>& allbranches, TObjArray* list, bool include_children, bool include_inactive, const std::string& pre="");
#ifndef USE_map
  void SetBranchAddressAll (const char* call="SetBranchInfo") const;
#endif

  // Hack to allow access to protected method TTree::CheckBranchAddressType()
  struct TTreeProtected : public TTree {
    static TTreeProtected& Access (TTree& t) { return (TTreeProtected&) t; }
    using TTree::CheckBranchAddressType;
  };

  // Member variables
  Long64_t fIndex=0;
  mutable ULong64_t fTotFill=0, fTotWrite=0, fTotRead=0;
#ifndef USE_map
  mutable std::vector<BranchInfo> fBranches;
  mutable std::vector<BranchInfo>::iterator fLastBranch;
  mutable bool fTryLast = false;
#ifndef NO_BranchInfo_STATS
  mutable size_t nhits=0, nmiss=0;
#endif
#else
  mutable branch_map_type<std::pair<std::string,type_code_t>,BranchInfo> fBranches;
#endif
  TTree* fTree=nullptr;
  bool fTreeOwned=false;
  Int_t fBufsize=32000;
  Int_t fSplitlevel=99;
  int fVerbose=0;
  bool fWriting=false;
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

#include "TTreeIterator/detail/TTreeIterator_detail.h"

#endif /* ROOT_TTreeIterator */
