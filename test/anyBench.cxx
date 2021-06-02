#define USE_std_any 1
#define USE_Cpp11_any 1
#define MYBENCH

#ifdef USE_std_any
# include <any>
#endif

#ifdef USE_Cpp11_any
//#define ANY_TEMPLATE_OPT 1   // optimise templated any methods
//#define NO_ANY_RTTI_CHECK 1  // don't check type_info in any_cast<T>(any), just use templated function pointer
//#define NO_ANY_RTTI 1        // don't use type_info (removes any::type() method) - not standard conforming
//#define NO_ANY_ACCESS 1      // don't include any::Manager<T>::manage(Op_access) method - not ABI compatible with std::any
//#define UNCHECKED_ANY 1      // don't check type of any_cast<T>(any)             - not standard conforming
//#define NO_ANY_EXCEPTIONS 1  // don't throw exceptions                           - not standard conforming
//#define Cpp11_std_any 1
# include "TTreeIterator/detail/Cpp11_any.h"  // Implementation of std::any, compatible with C++11.
#endif

#ifdef USE_std_any
  namespace any_ns1 = ::std;
  using any_type1 = std::any;
#else
  namespace any_ns1 = Cpp11;
  using any_type1 = Cpp11::any;
#endif
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
  std::vector<Any> vecany{3,std::string("01234567890123456"),1.7};
  return vecany[i];  // make sure optimiser doesn't know what type it is
}

#ifdef MYBENCH

#include <benchmark/benchmark.h>

static void BM_std_any(benchmark::State& state) {
  const auto a = get_any<any_type1>();
  for (auto _ : state) {
    auto d = any_ns1::any_cast<double>(a);
    benchmark::DoNotOptimize(d);
  }
}
BENCHMARK(BM_std_any);

static void BM_Cpp11_any(benchmark::State& state) {
  const auto a = get_any<any_type2>();
  for (auto _ : state) {
    auto d = any_ns2::any_cast<double>(a);
    benchmark::DoNotOptimize(d);
  }
}
BENCHMARK(BM_Cpp11_any);

BENCHMARK_MAIN();

#else

int main(int, char**) {
  auto a1 = get_any<any_type1>();
  auto a2 = get_any<any_type2>();
  return any_ns1::any_cast<double>(a1) +
         any_ns2::any_cast<double>(a2);
}
#endif
