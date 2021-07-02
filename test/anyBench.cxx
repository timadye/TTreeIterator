#define USE_std_any 1
#define USE_Cpp11_any 1
#define MYBENCH 1
//#define TEST_SAME_TYPE 1

#if defined(USE_std_any) && (__cplusplus < 201703L)   // <version> not available until GCC9, so no way to check __cpp_lib_any without including <any>.
# undef USE_std_any                  // only option is to use Cpp11::any
#endif

#ifdef USE_std_any
# include <any>
#else

#define ANY_TEMPLATE_OPT 1   // optimise templated any methods
//#define NO_ANY_RTTI_CHECK 1  // don't check type_info in any_cast<T>(any), just use templated function pointer - not useful optimisation
//#define NO_ANY_RTTI 1        // don't use type_info (removes any::type() method) - not standard conforming
//#define NO_ANY_ACCESS 1      // don't include any::Manager<T>::manage(Op_access) method - not ABI compatible with std::any
//#define UNCHECKED_ANY 1      // don't check type of any_cast<T>(any)             - not standard conforming
//#define NO_ANY_EXCEPTIONS 1  // don't throw exceptions                           - not standard conforming
//#define ANY_SAME_TYPE 1      // when assigning the same as existing type, don't recreate

#define Cpp11_std_any 1
#include "TTreeIterator/detail/Cpp11_any.h"  // Implementation of std::any, compatible with C++11.

#undef Cpp11_std_any
#undef HEADER_Cpp11_any
#undef ANY_TEMPLATE_OPT
#undef NO_ANY_RTTI_CHECK
#undef NO_ANY_RTTI
#undef NO_ANY_ACCESS
#undef UNCHECKED_ANY
#undef NO_ANY_EXCEPTIONS
#undef ANY_SAME_TYPE

#endif


#ifdef USE_Cpp11_any

#define ANY_TEMPLATE_OPT 1   // optimise templated any methods
//#define NO_ANY_RTTI_CHECK 1  // don't check type_info in any_cast<T>(any), just use templated function pointer - not useful optimisation
#define NO_ANY_RTTI 1        // don't use type_info (removes any::type() method) - not standard conforming
#define NO_ANY_ACCESS 1      // don't include any::Manager<T>::manage(Op_access) method - not ABI compatible with std::any
#define UNCHECKED_ANY 1      // don't check type of any_cast<T>(any)             - not standard conforming
#define NO_ANY_EXCEPTIONS 1  // don't throw exceptions                           - not standard conforming
#define ANY_SAME_TYPE 1      // when assigning the same as existing type, don't recreate

#include "TTreeIterator/detail/Cpp11_any.h"  // Implementation of std::any, compatible with C++11.
#endif

#include "TTreeIterator/detail/TTreeIterator_helpers.h"  // Implementation of std::any, compatible with C++11.

  namespace any_ns1 = ::std;
  using any_type1 = std::any;
#ifdef USE_Cpp11_any
  namespace any_ns2 = Cpp11;
  using any_type2 = Cpp11::any;
#else
  namespace any_ns2 = ::std;
  using any_type2 = std::any;
#endif

#include <vector>
#include <string>
template<typename Any>
Any get_any(size_t i=2) {
  std::vector<Any> vecany{3,std::string("01234567890123456"),1.7,std::string("small")};
  return vecany[i];  // make sure optimiser doesn't know what type it is
}

#ifdef MYBENCH

#include <benchmark/benchmark.h>
using namespace std::string_literals;

static void BM_std_any(benchmark::State& state) {
#ifdef TEST_SAME_TYPE
  auto a = get_any<any_type1>(1);
  const std::string s("abc");
  for (auto _ : state) {
    a = 3.1;
    //    a.emplace<std::string>({'a','x'});
    benchmark::DoNotOptimize(a);
    a = s;
    benchmark::DoNotOptimize(a);
  }
#else
  const auto a = get_any<any_type1>();
  for (auto _ : state) {
    auto d = any_ns1::any_cast<double>(a);
    benchmark::DoNotOptimize(d);
  }
#endif
}
BENCHMARK(BM_std_any);

static void BM_Cpp11_any(benchmark::State& state) {
#ifdef TEST_SAME_TYPE
  auto a = get_any<any_type2>(1);
  const std::string s("abc");
  for (auto _ : state) {
    a = 3.1;
    //    a.emplace<std::string>({'a','x'});
    benchmark::DoNotOptimize(a);
    a = s;
    benchmark::DoNotOptimize(a);
  }
#else
  const auto a = get_any<any_type2>();
  for (auto _ : state) {
    auto d = any_ns2::any_cast<double>(a);
    benchmark::DoNotOptimize(d);
  }
#endif
}
BENCHMARK(BM_Cpp11_any);

BENCHMARK_MAIN();

#else

#include <iostream>
int main(int, char**) {
  auto a1 = get_any<any_type1>();
  auto a2 = get_any<any_type2>();
  std::cout << type_name<decltype(a1)>() << ' ' << type_name<decltype(a2)>() << '\n';
  return any_ns1::any_cast<double>(a1) +
         any_ns2::any_cast<double>(a2);
}
#endif
