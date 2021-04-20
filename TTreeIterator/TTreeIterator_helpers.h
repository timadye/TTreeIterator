// Some extra tools for TTreeIterator class and tests
// Created by Tim Adye on 15/04/2021.

#ifndef ROOT_TTreeIterator_helpers
#define ROOT_TTreeIterator_helpers

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

#ifndef USE_STD_ANY

#include <typeinfo>
#include <new>
#include <utility>
#include <type_traits>

// A type-safe container of any type.
//
// Based on GCC 10.2.0's std::any implementation, last updated 2020-04-23:
//   https://gcc.gnu.org/git/?p=gcc.git;a=commit;h=d1462b0782555354b4480e1f46498586d5882972
// The code has been fixed to work with C++11 and extensively de-dOxygenified,
// and de-STLified to make it easier to read (for me at least).
//
// An Cpp11::Any object's state is either empty or it stores a contained object of CopyConstructible type.

#ifdef __cpp_if_constexpr
#define Cpp11_constexpr_if constexpr
#else
#define Cpp11_constexpr_if
#endif
#ifdef __cpp_variable_templates
#define Cpp11_in_place_type(T) in_place_type<T>
#else
#define Cpp11_in_place_type(T) in_place_type_t<T>{}
#endif

namespace Cpp11 {

  class Any {
    // Some internal stuff from GCC's std namespace...
    // See specialisations of or_ and and_ at end of class
    template<typename...> struct or_;
    template<typename _B1> struct or_<_B1> : public _B1 {};
    template<typename _B1, typename _B2> struct or_<_B1, _B2> : public std::conditional<_B1::value, _B1, _B2>::type {};
    template<typename _B1, typename _B2, typename _B3, typename... _Bn> struct or_<_B1, _B2, _B3, _Bn...>
      : public std::conditional<_B1::value, _B1, or_<_B2, _B3, _Bn...>>::type {};
    template<typename...> struct and_;
    template<typename _B1> struct and_<_B1> : public _B1 {};
    template<typename _B1, typename _B2> struct and_<_B1, _B2> : public std::conditional<_B1::value, _B2, _B1>::type {};
    template<typename _B1, typename _B2, typename _B3, typename... _Bn> struct and_<_B1, _B2, _B3, _Bn...>
      : public std::conditional<_B1::value, and_<_B2, _B3, _Bn...>, _B1>::type {};
    // remove_cvref_t (std::remove_cvref_t for C++11).
    template<typename T> using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    template<typename T> struct in_place_type_t { explicit in_place_type_t() = default; };
#ifdef __cpp_variable_templates   // for C++11, use in_place_type_t<T>{} instead of in_place_type<T>
    template<typename T> static constexpr in_place_type_t<T> in_place_type{};
#endif
    template<typename> struct is_in_place_type_impl : std::false_type {};
    template<typename T> struct is_in_place_type_impl<in_place_type_t<T>> : std::true_type {};
    template<typename T> struct is_in_place_type : public is_in_place_type_impl<T> {};

    // Exception class thrown by a failed any_cast
    struct bad_any_cast : public std::bad_cast {
      virtual const char* what() const noexcept { return "bad any_cast"; }
    };

    // Holds either pointer to a heap object or the contained object itself.
    union Storage {
      constexpr Storage() : _ptr{nullptr} {}

      // Prevent trivial copies of this type, buffer might hold a non-POD.
      Storage(const Storage&) = delete;
      Storage& operator=(const Storage&) = delete;

      void* _ptr;
      std::aligned_storage<sizeof(_ptr), alignof(void*)>::type _buffer;
    };

    template<typename T,
             typename Safe = std::is_nothrow_move_constructible<T>,
             bool Fits = (sizeof(T) <= sizeof(Storage)) && (alignof(T) <= alignof(Storage))>
    using Internal = std::integral_constant<bool, Safe::value && Fits>;

    template<typename T> struct Manager_internal; // uses small-object optimization
    template<typename T> struct Manager_external; // creates contained object on the heap
    template<typename T>
    using Manager = typename std::conditional<Internal<T>::value,
                                              Manager_internal<T>,
                                              Manager_external<T>>::type;

    template<typename T, typename V = typename std::decay<T>::type>
    using Decay_if_not_any = typename std::enable_if<!std::is_same<V, Any>::value, V>::type;

    // Emplace with an object created from args as the contained object.
    template <typename T, typename... Args, typename Mgr = Manager<T>>
    void do_emplace(Args&&... args) {
      reset();
      Mgr::create(_storage, std::forward<Args>(args)...);
      _manager = &Mgr::manage;
    }

    // Emplace with an object created from il and args as
    // the contained object.
    template <typename T, typename Up, typename... Args, typename Mgr = Manager<T>>
    void do_emplace(std::initializer_list<Up> il, Args&&... args) {
      reset();
      Mgr::create(_storage, il, std::forward<Args>(args)...);
      _manager = &Mgr::manage;
    }

    template <typename Res, typename T, typename... Args>
    using any_constructible = std::enable_if<and_<std::is_copy_constructible<T>, std::is_constructible<T, Args...>>::value, Res>;

    template <typename T, typename... Args>
    using any_constructible_t = typename any_constructible<bool, T, Args...>::type;

    template<typename V, typename... Args>
    using emplace_t = typename any_constructible<V&, V, Args...>::type;

  public:
    // construct/destruct

    // Default constructor, creates an empty object.
    constexpr Any() noexcept : _manager(nullptr) {}

    // Copy constructor, copies the state of other
    Any(const Any& other) {
      if (!other.has_value())
        _manager = nullptr;
      else {
        Arg arg;
        arg._any = this;
        other._manager(Op_clone, &other, &arg);
      }
    }

    // Move constructor, transfer the state from other
    Any(Any&& other) noexcept {
      if (!other.has_value())
        _manager = nullptr;
      else {
        Arg arg;
        arg._any = this;
        other._manager(Op_xfer, &other, &arg);
      }
    }

    // Construct with a copy of value as the contained object.
    template <typename T, typename V = Decay_if_not_any<T>,
              typename Mgr = Manager<V>,
              typename std::enable_if<std::is_copy_constructible<V>::value && !is_in_place_type<V>::value, bool>::type = true>
    Any(T&& value) : _manager(&Mgr::manage) {
      Mgr::create(_storage, std::forward<T>(value));
    }

    // Construct with an object created from args as the contained object.
    template <typename T, typename... Args, typename V = typename std::decay<T>::type, typename Mgr = Manager<V>, any_constructible_t<V, Args&&...> = false>
    explicit Any(in_place_type_t<T>, Args&&... args) : _manager(&Mgr::manage) {
      Mgr::create(_storage, std::forward<Args>(args)...);
    }

    // Construct with an object created from il and args as the contained object.
    template <typename T, typename Up, typename... Args, typename V = typename std::decay<T>::type, typename Mgr = Manager<V>, any_constructible_t<V, std::initializer_list<Up>, Args&&...> = false>
    explicit Any(in_place_type_t<T>, std::initializer_list<Up> il, Args&&... args) : _manager(&Mgr::manage) {
      Mgr::create(_storage, il, std::forward<Args>(args)...);
    }

    // Destructor, calls reset()
    ~Any() { reset(); }

    // assignments

    // Copy the state of another object.
    Any& operator=(const Any& rhs) {
      *this = Any(rhs);
      return *this;
    }

    // Move assignment operator
    Any& operator=(Any&& rhs) noexcept {
      if (!rhs.has_value())
        reset();
      else if (this != &rhs) {
        reset();
        Arg arg;
        arg._any = this;
        rhs._manager(Op_xfer, &rhs, &arg);
      }
      return *this;
    }

    // Store a copy of rhs as the contained object.
    template<typename T>
    typename std::enable_if<std::is_copy_constructible<Decay_if_not_any<T>>::value, Any&>::type
    operator=(T&& rhs) {
      *this = Any(std::forward<T>(rhs));
      return *this;
    }

    // Emplace with an object created from args as the contained object.
    template <typename T, typename... Args>
    emplace_t<typename std::decay<T>::type, Args...> emplace(Args&&... args) {
      using V = typename std::decay<T>::type;
      do_emplace<V>(std::forward<Args>(args)...);
      Any::Arg arg;
      this->_manager(Any::Op_access, this, &arg);
      return *static_cast<V*>(arg._obj);
    }

    // Emplace with an object created from il and args as the contained object.
    template <typename T, typename Up, typename... Args>
    emplace_t<typename std::decay<T>::type, std::initializer_list<Up>, Args&&...> emplace(std::initializer_list<Up> il, Args&&... args) {
      using V = typename std::decay<T>::type;
      do_emplace<V, Up>(il, std::forward<Args>(args)...);
      Any::Arg arg;
      this->_manager(Any::Op_access, this, &arg);
      return *static_cast<V*>(arg._obj);
    }

    // modifiers

    // If not empty, destroy the contained object.
    void reset() noexcept {
      if (has_value()) {
        _manager(Op_destroy, this, nullptr);
        _manager = nullptr;
      }
    }

    // Exchange state with another object.
    void swap(Any& rhs) noexcept {
      if (!has_value() && !rhs.has_value()) return;
      if (has_value() && rhs.has_value()) {
        if (this == &rhs) return;
        Any tmp;
        Arg arg;
        arg._any = &tmp;
        rhs._manager(Op_xfer, &rhs, &arg);
        arg._any = &rhs;
        _manager(Op_xfer, this, &arg);
        arg._any = this;
        tmp._manager(Op_xfer, &tmp, &arg);
      } else {
        Any* empty = !has_value() ? this : &rhs;
        Any* full = !has_value() ? &rhs : this;
        Arg arg;
        arg._any = empty;
        full->_manager(Op_xfer, full, &arg);
      }
    }

    // observers

    // Reports whether there is a contained object or not.
    bool has_value() const noexcept { return _manager != nullptr; }

    // The typeid of the contained object, or typeid(void) if empty.
    const std::type_info& type() const noexcept {
      if (!has_value()) return typeid(void);
      Arg arg;
      _manager(Op_get_type_info, this, &arg);
      return *arg._typeinfo;
    }

    template<typename T> static constexpr bool is_valid_cast() { return or_<std::is_reference<T>, std::is_copy_constructible<T>>::value; }

  private:
    enum Op { Op_access, Op_get_type_info, Op_clone, Op_destroy, Op_xfer };

    union Arg {
      void* _obj;
      const std::type_info* _typeinfo;
      Any* _any;
    };

    void (*_manager)(Op, const Any*, Arg*);
    Storage _storage;

    // Manage in-place contained object.
    template<typename T>
    struct Manager_internal {
      static void manage(Op which, const Any* anyp, Arg* arg);

      template<typename Up>
      static void create(Storage& storage, Up&& value) {
        void* addr = &storage._buffer;
        ::new (addr) T(std::forward<Up>(value));
      }

      template<typename... Args>
      static void create(Storage& storage, Args&&... args) {
        void* addr = &storage._buffer;
        ::new (addr) T(std::forward<Args>(args)...);
      }
    };

    // Manage external contained object.
    template<typename T>
    struct Manager_external {
      static void manage(Op which, const Any* anyp, Arg* arg);

      template<typename Up>
      static void create(Storage& storage, Up&& value) {
        storage._ptr = new T(std::forward<Up>(value));
      }
      template<typename... Args>
      static void create(Storage& storage, Args&&... args) {
        storage._ptr = new T(std::forward<Args>(args)...);
      }
    };

  public:
    // Exchange the states of two Any objects.
    static void swap(Any& x, Any& y) noexcept { x.swap(y); }

    // Create an Any holding a T constructed from args.
    template <typename T, typename... Args>
    static Any make_any(Args&&... args) {
      return Any(Cpp11_in_place_type(T), std::forward<Args>(args)...);
    }

    // Create an Any holding a T constructed from il and args.
    template <typename T, typename Up, typename... Args>
    static Any make_any(std::initializer_list<Up> il, Args&&... args) {
      return Any(Cpp11_in_place_type(T), il, std::forward<Args>(args)...);
    }

    // Access the contained object.
    //   ValueType  A const-reference or CopyConstructible type.
    //   any        The object to access.
    //   returns    The contained object.
    template<typename ValueType>
    static ValueType any_cast(const Any& any) {
      using Up = remove_cvref_t<ValueType>;
      static_assert(Any::is_valid_cast<ValueType>(),                    "Template argument must be a reference or CopyConstructible type");
      static_assert(std::is_constructible<ValueType, const Up&>::value, "Template argument must be constructible from a const value.");
      auto p = any_cast<Up>(&any);
      if (p) return static_cast<ValueType>(*p);
      throw bad_any_cast();
    }

    // Access the contained object.
    //   ValueType  A reference or CopyConstructible type.
    //   any        The object to access.
    //   returns    The contained object.
    template<typename ValueType>
    static ValueType any_cast(Any& any) {
      using Up = remove_cvref_t<ValueType>;
      static_assert(Any::is_valid_cast<ValueType>(),              "Template argument must be a reference or CopyConstructible type");
      static_assert(std::is_constructible<ValueType, Up&>::value, "Template argument must be constructible from an lvalue.");
      auto p = any_cast<Up>(&any);
      if (p) return static_cast<ValueType>(*p);
      throw bad_any_cast();
    }

    template<typename ValueType>
    static ValueType any_cast(Any&& any) {
      using Up = remove_cvref_t<ValueType>;
      static_assert(Any::is_valid_cast<ValueType>(),             "Template argument must be a reference or CopyConstructible type");
      static_assert(std::is_constructible<ValueType, Up>::value, "Template argument must be constructible from an rvalue.");
      auto p = any_cast<Up>(&any);
      if (p) return static_cast<ValueType>(std::move(*p));
      throw bad_any_cast();
    }

    template<typename T>
    static void* any_caster(const Any* any) {
      // any_cast<T> returns non-null if any->type() == typeid(T) and typeid(T) ignores cv-qualifiers so remove them:
      using Up = typename std::remove_cv<T>::type;
      // The contained value has a decayed type, so if std::decay<U>::type is not U, then it's not possible to have a contained value of type U:
      if Cpp11_constexpr_if (!std::is_same<typename std::decay<Up>::type, Up>::value) return nullptr;
      // Only copy constructible types can be used for contained values:
      else if Cpp11_constexpr_if (!std::is_copy_constructible<Up>::value) return nullptr;
      // First try comparing function addresses, which works without RTTI
      else if (any->_manager == &Any::Manager<Up>::manage || any->type() == typeid(T)) {
        Any::Arg arg;
        any->_manager(Any::Op_access, any, &arg);
        return arg._obj;
      }
      return nullptr;
    }

    // Access the contained object.
    //   ValueType  The type of the contained object.
    //   any        A pointer to the object to access.
    //   returns    The address of the contained object if
    //                any != nullptr && any.type() == typeid(ValueType)
    //              otherwise a null pointer.
    template<typename ValueType>
    static const ValueType* any_cast(const Any* any) noexcept {
      if Cpp11_constexpr_if (std::is_object<ValueType>::value)
        if (any) return static_cast<ValueType*>(any_caster<ValueType>(any));
      return nullptr;
    }

    template<typename ValueType>
    static ValueType* any_cast(Any* any) noexcept {
      if Cpp11_constexpr_if (std::is_object<ValueType>::value)
        if (any) return static_cast<ValueType*>(any_caster<ValueType>(any));
      return nullptr;
    }
  };

  template<typename T>
  void Any::Manager_internal<T>::manage(Op which, const Any* any, Arg* arg) {
    // The contained object is in _storage._buffer
    auto ptr = reinterpret_cast<const T*>(&any->_storage._buffer);
    switch (which) {
    case Op_access:
      arg->_obj = const_cast<T*>(ptr);
      break;
    case Op_get_type_info:
      arg->_typeinfo = &typeid(T);
      break;
    case Op_clone:
      ::new(&arg->_any->_storage._buffer) T(*ptr);
      arg->_any->_manager = any->_manager;
      break;
    case Op_destroy:
      ptr->~T();
      break;
    case Op_xfer:
      ::new(&arg->_any->_storage._buffer) T(std::move(*const_cast<T*>(ptr)));
      ptr->~T();
      arg->_any->_manager = any->_manager;
      const_cast<Any*>(any)->_manager = nullptr;
      break;
    }
  }

  template<typename T>
  void Any::Manager_external<T>::manage(Op which, const Any* any, Arg* arg) {
    // The contained object is *_storage._ptr
    auto ptr = static_cast<const T*>(any->_storage._ptr);
    switch (which) {
    case Op_access:
      arg->_obj = const_cast<T*>(ptr);
      break;
    case Op_get_type_info:
      arg->_typeinfo = &typeid(T);
      break;
    case Op_clone:
      arg->_any->_storage._ptr = new T(*ptr);
      arg->_any->_manager = any->_manager;
      break;
    case Op_destroy:
      delete ptr;
      break;
    case Op_xfer:
      arg->_any->_storage._ptr = any->_storage._ptr;
      arg->_any->_manager = any->_manager;
      const_cast<Any*>(any)->_manager = nullptr;
      break;
    }
  }

  // specialisations of or_ and and_ from top of Any
  template<> struct Any::or_<>  : public std::false_type {};
  template<> struct Any::and_<> : public std::true_type  {};
}

#endif

// =====================================================================

#ifdef USE_OrderedMap

#include <tuple>
#include <vector>
#include <iterator>

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

        iterator  begin()       { return       iterator(_vec. begin()); }
        iterator  end  ()       { return       iterator(_vec. end  ()); }
        iterator  begin() const { return       iterator(const_cast<vector_type&>(_vec).begin()); }
        iterator  end  () const { return       iterator(const_cast<vector_type&>(_vec).end  ()); }
  const_iterator cbegin() const { return const_iterator(_vec.cbegin()); }
  const_iterator cend  () const { return const_iterator(_vec.cend  ()); }
#ifdef __cpp_lib_make_reverse_iterator
  using       reverse_iterator = std::reverse_iterator<      iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  std::reverse_iterator<      iterator>  rbegin()       { return std::make_reverse_iterator( end  ()); }
  std::reverse_iterator<      iterator>  rend  ()       { return std::make_reverse_iterator( begin()); }
  std::reverse_iterator<const_iterator>  rbegin() const { return std::make_reverse_iterator(cend  ()); }
  std::reverse_iterator<const_iterator>  rend  () const { return std::make_reverse_iterator(cbegin()); }
  std::reverse_iterator<const_iterator> crbegin() const { return std::make_reverse_iterator(cend  ()); }
  std::reverse_iterator<const_iterator> crend  () const { return std::make_reverse_iterator(cbegin()); }
#endif

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

#endif

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
  static std::string r;   // keeps one string per type (T) until exit
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
  ShowConstructors()                                     { Print ("%1$s() [%2$s] @%3$p\n",this);    }
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

#endif /* ROOT_TTreeIterator_helpers */
