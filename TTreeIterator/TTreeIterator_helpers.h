// Some extra tools for TTreeIterator class and tests
// Created by Tim Adye on 15/04/2021.

#ifndef TTreeIterator_helpers_h
#define TTreeIterator_helpers_h

//#define NO_cxxabi_h

#ifdef _MSC_VER
#define NO_cxxabi_h
#endif

#include <cstdio>
#include <memory>
#include <utility>
#include <string>
#include <cstdlib>
#include <cstddef>
#include <typeinfo>
#include <map>
#include <tuple>
#include <vector>
#include <iterator>

#ifndef NO_cxxabi_h
#include <cxxabi.h>
#endif

#include "TNamed.h"
#include "TString.h"

// generates compilation error and shows real type name (and place of declaration in some cases)
// in an error message, useful for debugging boost::mpl like recurrent types.
// https://stackoverflow.com/a/50726702
#define SA_CONCAT(a,b) SA_CONCAT_(a, b)
#define SA_CONCAT_(a,b) a##b
#define SA_UNIQUE(base) SA_CONCAT(base,__COUNTER__)

#define static_assert_type(type_name) \
  using SA_UNIQUE(_static_assert_type_) = decltype((*(typename ::_static_assert_detail::type_lookup<type_name >::type*)0).operator ,(*(::_static_assert_detail::dummy*)0))

// lookup compile time template typename value
#define static_assert_value(static_param) \
  static_assert_type(_static_assert_detail_param(static_param))

// need this so comma isn't interpreted as separating static_assert_value() macro parameters
#define _static_assert_detail_param(v1) ::_static_assert_detail::StaticAssertParam<decltype(v1), (v1)>

namespace _static_assert_detail
{
  template <typename T> struct type_lookup { using type = T; };
  struct dummy {};
  template <typename T, T v> struct StaticAssertParam {};
}

// =====================================================================

template<typename Key, typename T>
class OrderedMap : public std::map< Key, std::pair<T,size_t> > {
public:
  // implementation typedefs
  using map_type        = std::map< Key, std::pair<T,size_t> >;
  using vector_type     = std::vector<typename map_type::iterator>;
  using map_iterator    = typename map_type::iterator;
  using vector_iterator = typename vector_type::iterator;

  // STL typedef changes
  using key_type    = Key;
  using mapped_type = T;
  using value_type  = std::pair<const Key, T>;
  using reference   = value_type&;
  using pointer     = value_type*;

  // STL typedef inhertance
  using typename map_type::key_compare;
  using typename map_type::allocator_type;
  using size_type       = typename vector_type::size_type;
  using difference_type = typename vector_type::difference_type;

//static_assert (  sizeof (typename OrderedMap::map_type::value_type)                ==   sizeof (typename OrderedMap::value_type) + sizeof (size_t),
//               "std::pair arrangement won't work");
  static_assert (offsetof (typename OrderedMap::map_type::value_type, first)         == offsetof (typename OrderedMap::value_type, first),
                 "bad std::pair arrangement (offsetof Key)");
  static_assert (offsetof (typename OrderedMap::map_type::value_type, second. first) == offsetof (typename OrderedMap::value_type, second),
                 "bad std::pair arrangement (offsetof T)");

  struct iterator : public vector_type::iterator {
    using container_type  = OrderedMap;
    using map_type        = typename container_type::map_type;
    using value_type      = typename container_type::value_type;
    using reference       = value_type&;
    using pointer         = value_type*;
    using vector_iterator = typename vector_type::iterator;
    using map_iterator    = typename map_type::iterator;
    using typename vector_iterator::iterator_category;
    using typename vector_iterator::difference_type;
    iterator() = default;
    iterator(const vector_iterator& vi) : vector_iterator(vi) {}
    iterator(const container_type& om, size_t n) : vector_iterator(std::next (om.begin(), n)) {}
    iterator(const container_type& om, const map_iterator& mi) : vector_iterator(std::next (om.begin(), mi->second.second)) {}
    reference operator*() const {
      map_iterator mit = vector_iterator::operator*();
      return (reference) mit->first;  // nasty hack to give us a ref to pair<Key,T> from pair<Key,pair<T,size_t>> since Key and T are next to each other
    }
    pointer operator->() const { return &(operator*()); }
  };

  struct const_iterator : public vector_type::const_iterator {
    using container_type  = OrderedMap;
    using map_type        = typename container_type::map_type;
    using value_type      = typename container_type::value_type;
    using reference       = const value_type&;
    using pointer         = const value_type*;
    using vector_iterator = typename vector_type::const_iterator;
    using map_iterator    = typename map_type::const_iterator;
    using typename vector_iterator::iterator_category;
    using typename vector_iterator::difference_type;
    const_iterator() = default;
    const_iterator(const vector_iterator& vi) : vector_iterator(vi) {}
    const_iterator(const container_type& om, size_t n) : vector_iterator(std::next (om.begin(), n)) {}
    const_iterator(const container_type& om, const map_iterator& mi) : vector_iterator(std::next (om.begin(), mi->second.second)) {}
    reference operator*() const {
      map_iterator mit = vector_iterator::operator*();
      return (reference) mit->first;  // nasty hack to give us a ref to pair<Key,T> from pair<Key,pair<T,size_t>> since Key and T are next to each other
    }
    pointer operator->() const { return &(operator*()); }
  };
  using       reverse_iterator = std::reverse_iterator<      iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        iterator  begin()       { return       iterator(_vec. begin()); }
        iterator  end  ()       { return       iterator(_vec. end  ()); }
        iterator  begin() const { return       iterator(const_cast<vector_type&>(_vec).begin()); }
        iterator  end  () const { return       iterator(const_cast<vector_type&>(_vec).end  ()); }
  const_iterator cbegin() const { return const_iterator(_vec.cbegin()); }
  const_iterator cend  () const { return const_iterator(_vec.cend  ()); }
  std::reverse_iterator<      iterator>  rbegin()       { return std::make_reverse_iterator( end  ()); }
  std::reverse_iterator<      iterator>  rend  ()       { return std::make_reverse_iterator( begin()); }
  std::reverse_iterator<const_iterator>  rbegin() const { return std::make_reverse_iterator(cend  ()); }
  std::reverse_iterator<const_iterator>  rend  () const { return std::make_reverse_iterator(cbegin()); }
  std::reverse_iterator<const_iterator> crbegin() const { return std::make_reverse_iterator(cend  ()); }
  std::reverse_iterator<const_iterator> crend  () const { return std::make_reverse_iterator(cbegin()); }

  mapped_type& operator[] (const key_type& k) {
    return insert ({k,T()}).first->second;
  }

  mapped_type& at (const key_type& k) const {
    auto vit = find_last(k);
    if (vit != end()) return vit->second.first;
    return map_type::at(k).first;
  }

  std::pair<iterator, bool> emplace(key_type&& k, mapped_type&& v) {
    return vector_insert (map_type::emplace (std::piecewise_construct, std::forward_as_tuple(k), std::forward_as_tuple(v,0)));
  }
//  I couldn't get the STL signature to work, probably because it refused to default the added 0:
//template<typename... Args> std::pair<iterator, bool> emplace(Args&&... args) {
//  return vector_insert (map_type::emplace (std::forward<Args>(args)...));    }

  std::pair<iterator, bool> insert (const value_type& val) {
    return vector_insert (map_type::insert ({val.first, typename map_type::mapped_type (val.second,0)}));
  }

  iterator find (const key_type& key) const {
    auto vit = find_last(key);
    if (vit != end()) return vit;
    auto mit = map_type::find (key);
    if (mit == map_type::end()) return end();
    _last = mit->second.second;
    _try_last = true;
    return iterator (*this, _last);
  }

  void clear() {
    map_type::clear();
    _vec.clear();
    _try_last = false;
  }

  // we didn't implement erase(), and it wouldn't be good if it were used
  iterator erase(iterator) = delete;
  iterator erase(const_iterator, const_iterator) = delete;
  size_type erase(const key_type&) = delete;

private:
  iterator find_last (const key_type& key) const {
    if (!_try_last || _vec.size() < 2) return end();
    ++_last;
    if (_last >= _vec.size()) _last = 0;
    auto& mit = _vec[_last];
    if (mit->first == key) return iterator(*this, mit);
    _try_last = false;
    return end();
  }

  std::pair<iterator, bool> vector_insert (const std::pair<map_iterator, bool>& ret) {
    auto& mit = ret.first;
    if (ret.second) {
      mit->second.second = _vec.size();
      _vec.push_back(mit);
      _try_last = false;
    } else {
      _last = mit->second.second;
      _try_last = true;
    }
    return {iterator (*this, mit), ret.second};
  }

  mutable bool _try_last = false;
  mutable size_t _last = 0;
  vector_type _vec;
};

// =====================================================================

inline const char* demangle_name (const char* name, const char* varname=0)
{
  static std::string r;   // only store one at a time
#ifndef NO_cxxabi_h
  std::unique_ptr<char, void(*)(void*)> own(abi::__cxa_demangle(name, nullptr, nullptr, nullptr), std::free);
  r = own ? own.get() : name;
#else
  r = name;
#endif
  if (varname) { r += "::"; r += varname; }
  return r.c_str();
}

// type_name<T>() shows type name with reference modifiers
// https://stackoverflow.com/a/53865723
template <class T>
inline const char* type_name (const char* varname=0)
{
  static std::string r;   // only store one at a time
  typedef typename std::remove_reference<T>::type TR;
  r = demangle_name (typeid(TR).name());
  if      (std::is_const           <TR>::value) r += " const";
  if      (std::is_volatile        <TR>::value) r += " volatile";
  if      (std::is_lvalue_reference<T >::value) r += "&";
  else if (std::is_rvalue_reference<T >::value) r += "&&";
  if (varname) { r += "::"; r += varname; }
  return r.c_str();
}

// =====================================================================

// CRTP mix-in to show constructors/destructors/assignment operators.
// See TestObj below for an example (only public inheritance from ShowConstructors<TestObj> required).
template <class T>
struct ShowConstructors {
  struct quiet {};
  ShowConstructors()                                     { Print ("%1$s() @%3$p\n",this);           }
  void init() const                                      { Print ("%1$s(%2$s) @%3$p\n",this);       }
  ShowConstructors(const ShowConstructors& o)            { Print ("%1$s(%1$s(%2$s)) @%3$p\n",&o);   }
  ShowConstructors(      ShowConstructors&&o)            { Print ("%1$s(%1$s&&(%2$s)) @%3$p\n",&o); }
  ShowConstructors(const ShowConstructors::quiet& q) {}  // default constructor, but without Print()
  ~ShowConstructors()                                    { Print ("~%1$s(%2$s) @%3$p\n",this); skip=false; }
  void destroy() const                                   { Print ("~%1$s(%2$s) @%3$p\n",this); skip=true;  }
  ShowConstructors& operator=(const ShowConstructors& o) { Print ("%1$s = %1$s(%2$s) @%3$p\n",&o);   return *this; }
  ShowConstructors& operator=(      ShowConstructors&&o) { Print ("%1$s = %1$s&&(%2$s) @%3$p\n",&o); return *this; }
  const char* ContentsAsString() const { return ""; }
  void Print (const char* fmt, const ShowConstructors* o) const {
    if (verbose>=1 && !skip)
      printf (fmt, type_name<T>(), static_cast<const T*>(o)->ContentsAsString(), this);
  }
  static int verbose;
  static bool skip;
};
template <class T> int  ShowConstructors<T>::verbose = 0;
template <class T> bool ShowConstructors<T>::skip    = false;


// A simple test ROOT object with instrumentation
class TestObj : public TNamed, public ShowConstructors<TestObj> {
public:
  TestObj(Double_t v)                                         : TNamed(),            value(v), ShowConstructors(ShowConstructors::quiet()) { ShowConstructors::init(); }
  TestObj(Double_t v, const char* name, const char* title="") : TNamed(name, title), value(v), ShowConstructors(ShowConstructors::quiet()) { ShowConstructors::init(); }
  ~TestObj() { ShowConstructors::destroy(); value=-3.0; }
  TestObj()                            = default;  // rule of 5
  TestObj(const TestObj& o)            = default;  // rule of 5
  TestObj(      TestObj&&o)            = default;  // rule of 5
  TestObj& operator=(const TestObj& o) = default;  // rule of 5
  TestObj& operator=(      TestObj&&o) = default;  // rule of 5
// Following are member definitions required if we didn't take the defaults
//TestObj()                 : TNamed(),                             ShowConstructors()             {}
//TestObj(const TestObj& o) : TNamed(o),            value(o.value), ShowConstructors(o)            {}
//TestObj(      TestObj&&o) : TNamed(std::move(o)), value(o.value), ShowConstructors(std::move(o)) {}
//TestObj& operator=(const TestObj& o) { ShowConstructors::operator=(o);            TNamed::operator=(o);            value=o.value; return *this; }
//TestObj& operator=(      TestObj&&o) { ShowConstructors::operator=(std::move(o)); TNamed::operator=(std::move(o)); value=o.value; return *this; }

  const char* ContentsAsString() const { return Form("%g",value); }
  Double_t value=-1.0;
  ClassDefOverride(TestObj,1)
};

#endif /* TTreeIterator_helpers_h */
