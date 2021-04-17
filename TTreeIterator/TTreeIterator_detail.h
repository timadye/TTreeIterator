// Some extra tools for TTreeIterator class and tests
// Created by Tim Adye on 15/04/2021.

#pragma once

//#define NO_cxxabi_h

#ifdef _MSC_VER
#define NO_cxxabi_h
#endif

#include <cstdio>
#include <memory>
#include <utility>
#include <string>
#include <cstdlib>
#include <typeinfo>
#include <map>
#include <vector>
#include <iterator>

#ifndef NO_cxxabi_h
#include <cxxabi.h>
#endif

#include "TNamed.h"
#include "TString.h"


template<typename Key, typename T>
class OrderedMap : public std::map< Key, std::pair<size_t,T> > {
public:
  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<const Key, T>;
  using map_type = std::map< Key, std::pair<size_t,T> >;
  using vector_type = std::vector<typename map_type::iterator>;

  struct iterator : public vector_type::iterator {
    iterator() = default;
    iterator(const typename vector_type::iterator& vi) : vector_type::iterator(vi) {}
    value_type operator*() const {
      typename OrderedMap::map_type::iterator mit = vector_type::iterator::operator*();
      return OrderedMap::value_type (mit->first, mit->second.second);
    }
  };
  struct reverse_iterator : public vector_type::reverse_iterator {
    reverse_iterator() = default;
    reverse_iterator(const typename vector_type::reverse_iterator& vi) : vector_type::reverse_iterator(vi) {}
    const value_type& operator*() const {
      typename OrderedMap::map_type::iterator mit = vector_type::reverse_iterator::operator*();
      return OrderedMap::value_type (mit->first, mit->second.second);
    }
  };
  iterator  begin() { return          iterator(_vec. begin()); }
  iterator  end  () { return          iterator(_vec. end  ()); }
  iterator rbegin() { return  reverse_iterator(_vec.rbegin()); }
  iterator rend  () { return  reverse_iterator(_vec.rend  ()); }
  /*
  typename vector_type::iterator  begin() { return _vec. begin(); }
  typename vector_type::iterator  end() { return _vec. end(); }
  */
  iterator& next (iterator& it) {  // circular next
    if (it == end()) return it;
    it++;
    if (it == end()) it = begin();
    return it;
  }

  mapped_type& operator[] (const key_type& k) {
    //    return insert (value_type(k, T())).first->second;
    auto& val = map_type::operator[](k);  // doesn't add to _vec
    return val.second;
  }

  mapped_type& at (const key_type& k) {
    auto& val = map_type::at(k);
    return val.second;
  }

  std::pair<iterator, bool> insert (const value_type& val) {
    auto ret = map_type::insert (typename map_type::value_type (val.first, typename map_type::mapped_type (0, val.second)));
    if (!ret.second) return std::pair<iterator,bool> (end(), false);
    auto& mit = ret.first;
    mit->second.first = _vec.size();
    _vec.push_back(mit);
    return std::pair<iterator,bool> (std::next (begin(), mit->second.first), true);
  }

  iterator find (const key_type& key) {
    auto mit = map_type::find (key);
    if (mit == map_type::end()) return end();
    return std::next (begin(), mit->second.first);
  }

private:
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
