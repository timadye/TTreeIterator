// Some extra tools for TTreeIterator class and tests
// Created by Tim Adye on 15/04/2021.

#pragma once

#include <cstdio>
#include <memory>
#include <string>
#include <cstdlib>
#ifndef _MSC_VER
#include <cxxabi.h>
#endif

#include "TNamed.h"
#include "TString.h"

// type_name<T>() shows type name with reference modifiers
// https://stackoverflow.com/a/53865723
inline const char* demangle_name (const char* name, const char* varname=0)
{
  static std::string r;   // only store one at a time
#ifndef _MSC_VER
  std::unique_ptr<char, void(*)(void*)> own(abi::__cxa_demangle(name, nullptr, nullptr, nullptr), std::free);
  r = own ? own.get() : name;
#else
  r = name;
#endif
  if (varname) { r += "::"; r += varname; }
  return r.c_str();
}

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
