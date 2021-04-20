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

/**
 *  @brief A type-safe container of any type.
 *
 *  Based on GCC 10.2.0's std::any implementation.
 *
 *  An @c any object's state is either empty or it stores a contained object
 *  of CopyConstructible type.
 */
class UncheckedAny
{
  // Some internal stuff from GCC's std namespace...
  // See specialisations of __or_ and __and_ at end of class
  template<typename...> struct __or_;
  template<typename _B1> struct __or_<_B1> : public _B1 {};
  template<typename _B1, typename _B2> struct __or_<_B1, _B2> : public std::conditional<_B1::value, _B1, _B2>::type {};
  template<typename _B1, typename _B2, typename _B3, typename... _Bn> struct __or_<_B1, _B2, _B3, _Bn...>
    : public std::conditional<_B1::value, _B1, __or_<_B2, _B3, _Bn...>>::type {};
  template<typename...> struct __and_;
  template<typename _B1> struct __and_<_B1> : public _B1 {};
  template<typename _B1, typename _B2> struct __and_<_B1, _B2> : public std::conditional<_B1::value, _B2, _B1>::type {};
  template<typename _B1, typename _B2, typename _B3, typename... _Bn> struct __and_<_B1, _B2, _B3, _Bn...>
    : public std::conditional<_B1::value, __and_<_B2, _B3, _Bn...>, _B1>::type {};
  // __remove_cvref_t (std::remove_cvref_t for C++11).
  template<typename _Tp> using __remove_cvref_t = typename std::remove_cv<typename std::remove_reference<_Tp>::type>::type;

#ifdef __cpp_lib_any
  template<typename> struct __is_in_place_type_impl : std::false_type {};
  template<typename _Tp> struct __is_in_place_type_impl<std::in_place_type_t<_Tp>> : std::true_type {};
  template<typename _Tp> struct __is_in_place_type : public __is_in_place_type_impl<_Tp> {};
#endif

  // Holds either pointer to a heap object or the contained object itself.
  union _Storage
  {
    constexpr _Storage() : _M_ptr{nullptr} {}

    // Prevent trivial copies of this type, buffer might hold a non-POD.
    _Storage(const _Storage&) = delete;
    _Storage& operator=(const _Storage&) = delete;

    void* _M_ptr;
    std::aligned_storage<sizeof(_M_ptr), alignof(void*)>::type _M_buffer;
  };

  template<typename _Tp, typename _Safe = std::is_nothrow_move_constructible<_Tp>,
           bool _Fits = (sizeof(_Tp) <= sizeof(_Storage))
           && (alignof(_Tp) <= alignof(_Storage))>
  using _Internal = std::integral_constant<bool, _Safe::value && _Fits>;

  template<typename _Tp>
  struct _Manager_internal; // uses small-object optimization

  template<typename _Tp>
  struct _Manager_external; // creates contained object on the heap

  template<typename _Tp>
  using _Manager = typename std::conditional<_Internal<_Tp>::value,
                                             _Manager_internal<_Tp>,
                                             _Manager_external<_Tp>>::type;

  template<typename _Tp, typename _VTp = typename std::decay<_Tp>::type>
  using _Decay_if_not_any = typename std::enable_if<!std::is_same<_VTp, UncheckedAny>::value, _VTp>::type;

  /// Emplace with an object created from @p __args as the contained object.
  template <typename _Tp, typename... _Args,
            typename _Mgr = _Manager<_Tp>>
  void __do_emplace(_Args&&... __args)
  {
    reset();
    _Mgr::_S_create(_M_storage, std::forward<_Args>(__args)...);
    _M_manager = &_Mgr::_S_manage;
  }

  /// Emplace with an object created from @p __il and @p __args as
  /// the contained object.
  template <typename _Tp, typename _Up, typename... _Args,
            typename _Mgr = _Manager<_Tp>>
  void __do_emplace(std::initializer_list<_Up> __il, _Args&&... __args)
  {
    reset();
    _Mgr::_S_create(_M_storage, __il, std::forward<_Args>(__args)...);
    _M_manager = &_Mgr::_S_manage;
  }

  template <typename _Res, typename _Tp, typename... _Args>
  using __any_constructible
  = std::enable_if<__and_<std::is_copy_constructible<_Tp>,
                          std::is_constructible<_Tp, _Args...>>::value,
                   _Res>;

  template <typename _Tp, typename... _Args>
  using __any_constructible_t
  = typename __any_constructible<bool, _Tp, _Args...>::type;

  template<typename _VTp, typename... _Args>
  using __emplace_t
  = typename __any_constructible<_VTp&, _VTp, _Args...>::type;

public:
  // construct/destruct

  /// Default constructor, creates an empty object.
  constexpr UncheckedAny() noexcept : _M_manager(nullptr) {}

  /// Copy constructor, copies the state of @p __other
  UncheckedAny(const UncheckedAny& __other)
  {
    if (!__other.has_value())
      _M_manager = nullptr;
    else
      {
        _Arg __arg;
        __arg._M_any = this;
        __other._M_manager(_Op_clone, &__other, &__arg);
      }
  }

  // @brief Move constructor, transfer the state from @p __other
  UncheckedAny(UncheckedAny&& __other) noexcept
  {
    if (!__other.has_value())
      _M_manager = nullptr;
    else
      {
        _Arg __arg;
        __arg._M_any = this;
        __other._M_manager(_Op_xfer, &__other, &__arg);
      }
  }

  /// Construct with a copy of @p __value as the contained object.
  template <typename _Tp, typename _VTp = _Decay_if_not_any<_Tp>,
            typename _Mgr = _Manager<_VTp>,
            typename std::enable_if<std::is_copy_constructible<_VTp>::value
#ifdef __cpp_lib_any
                                    && !__is_in_place_type<_VTp>::value
#endif
                                    , bool>::type = true>
  UncheckedAny(_Tp&& __value)
    : _M_manager(&_Mgr::_S_manage)
  {
    _Mgr::_S_create(_M_storage, std::forward<_Tp>(__value));
  }

#ifdef __cpp_lib_any
  /// Construct with an object created from @p __args as the contained object.
  template <typename _Tp, typename... _Args, typename _VTp = typename std::decay<_Tp>::type,
            typename _Mgr = _Manager<_VTp>,
            __any_constructible_t<_VTp, _Args&&...> = false>
  explicit
  UncheckedAny(std::in_place_type_t<_Tp>, _Args&&... __args)
    : _M_manager(&_Mgr::_S_manage)
  {
    _Mgr::_S_create(_M_storage, std::forward<_Args>(__args)...);
  }

  /// Construct with an object created from @p __il and @p __args as
  /// the contained object.
  template <typename _Tp, typename _Up, typename... _Args,
            typename _VTp = typename std::decay<_Tp>::type, typename _Mgr = _Manager<_VTp>,
            __any_constructible_t<_VTp, std::initializer_list<_Up>,
                                  _Args&&...> = false>
  explicit
  UncheckedAny(std::in_place_type_t<_Tp>, std::initializer_list<_Up> __il, _Args&&... __args)
    : _M_manager(&_Mgr::_S_manage)
  {
    _Mgr::_S_create(_M_storage, __il, std::forward<_Args>(__args)...);
  }
#endif

  /// Destructor, calls @c reset()
  ~UncheckedAny() { reset(); }

  // assignments

  /// Copy the state of another object.
  UncheckedAny&
  operator=(const UncheckedAny& __rhs)
  {
    *this = UncheckedAny(__rhs);
    return *this;
  }

  // @brief Move assignment operator
  UncheckedAny&
  operator=(UncheckedAny&& __rhs) noexcept
  {
    if (!__rhs.has_value())
      reset();
    else if (this != &__rhs)
      {
        reset();
        _Arg __arg;
        __arg._M_any = this;
        __rhs._M_manager(_Op_xfer, &__rhs, &__arg);
      }
    return *this;
  }

  /// Store a copy of @p __rhs as the contained object.
  template<typename _Tp>
  typename std::enable_if<std::is_copy_constructible<_Decay_if_not_any<_Tp>>::value, UncheckedAny&>::type
  operator=(_Tp&& __rhs)
  {
    *this = UncheckedAny(std::forward<_Tp>(__rhs));
    return *this;
  }

  /// Emplace with an object created from @p __args as the contained object.
  template <typename _Tp, typename... _Args>
  __emplace_t<typename std::decay<_Tp>::type, _Args...>
  emplace(_Args&&... __args)
  {
    using _VTp = typename std::decay<_Tp>::type;
    __do_emplace<_VTp>(std::forward<_Args>(__args)...);
    UncheckedAny::_Arg __arg;
    this->_M_manager(UncheckedAny::_Op_access, this, &__arg);
    return *static_cast<_VTp*>(__arg._M_obj);
  }

  /// Emplace with an object created from @p __il and @p __args as
  /// the contained object.
  template <typename _Tp, typename _Up, typename... _Args>
  __emplace_t<typename std::decay<_Tp>::type, std::initializer_list<_Up>, _Args&&...>
  emplace(std::initializer_list<_Up> __il, _Args&&... __args)
  {
    using _VTp = typename std::decay<_Tp>::type;
    __do_emplace<_VTp, _Up>(__il, std::forward<_Args>(__args)...);
    UncheckedAny::_Arg __arg;
    this->_M_manager(UncheckedAny::_Op_access, this, &__arg);
    return *static_cast<_VTp*>(__arg._M_obj);
  }

  // modifiers

  /// If not empty, destroy the contained object.
  void reset() noexcept
  {
    if (has_value())
      {
        _M_manager(_Op_destroy, this, nullptr);
        _M_manager = nullptr;
      }
  }

  /// Exchange state with another object.
  void swap(UncheckedAny& __rhs) noexcept
  {
    if (!has_value() && !__rhs.has_value())
      return;

    if (has_value() && __rhs.has_value())
      {
        if (this == &__rhs)
          return;

        UncheckedAny __tmp;
        _Arg __arg;
        __arg._M_any = &__tmp;
        __rhs._M_manager(_Op_xfer, &__rhs, &__arg);
        __arg._M_any = &__rhs;
        _M_manager(_Op_xfer, this, &__arg);
        __arg._M_any = this;
        __tmp._M_manager(_Op_xfer, &__tmp, &__arg);
      }
    else
      {
        UncheckedAny* __empty = !has_value() ? this : &__rhs;
        UncheckedAny* __full = !has_value() ? &__rhs : this;
        _Arg __arg;
        __arg._M_any = __empty;
        __full->_M_manager(_Op_xfer, __full, &__arg);
      }
  }

  // observers

  /// Reports whether there is a contained object or not.
  bool has_value() const noexcept { return _M_manager != nullptr; }

  template<typename _Tp>
  static constexpr bool __is_valid_cast()
  { return __or_<std::is_reference<_Tp>, std::is_copy_constructible<_Tp>>::value; }

private:
  enum _Op {
    _Op_access, _Op_get_type_info, _Op_clone, _Op_destroy, _Op_xfer
  };

  union _Arg
  {
    void* _M_obj;
    UncheckedAny* _M_any;
  };

  void (*_M_manager)(_Op, const UncheckedAny*, _Arg*);
  _Storage _M_storage;

  // Manage in-place contained object.
  template<typename _Tp>
  struct _Manager_internal
  {
    static void
    _S_manage(_Op __which, const UncheckedAny* __anyp, _Arg* __arg);

    template<typename _Up>
    static void
    _S_create(_Storage& __storage, _Up&& __value)
    {
      void* __addr = &__storage._M_buffer;
      ::new (__addr) _Tp(std::forward<_Up>(__value));
    }

    template<typename... _Args>
    static void
    _S_create(_Storage& __storage, _Args&&... __args)
    {
      void* __addr = &__storage._M_buffer;
      ::new (__addr) _Tp(std::forward<_Args>(__args)...);
    }
  };

  // Manage external contained object.
  template<typename _Tp>
  struct _Manager_external
  {
    static void
    _S_manage(_Op __which, const UncheckedAny* __anyp, _Arg* __arg);

    template<typename _Up>
    static void
    _S_create(_Storage& __storage, _Up&& __value)
    {
      __storage._M_ptr = new _Tp(std::forward<_Up>(__value));
    }
    template<typename... _Args>
    static void
    _S_create(_Storage& __storage, _Args&&... __args)
    {
      __storage._M_ptr = new _Tp(std::forward<_Args>(__args)...);
    }
  };

public:
  /// Exchange the states of two @c any objects.
  static void swap(UncheckedAny& __x, UncheckedAny& __y) noexcept { __x.swap(__y); }

  /// Create an any holding a @c _Tp constructed from @c __args.
  template <typename _Tp, typename... _Args>
  static UncheckedAny make_any(_Args&&... __args)
  {
    return UncheckedAny(
#ifdef __cpp_lib_any
                        std::in_place_type<_Tp>,
#endif
                        std::forward<_Args>(__args)...);
  }

  /// Create an any holding a @c _Tp constructed from @c __il and @c __args.
  template <typename _Tp, typename _Up, typename... _Args>
  static UncheckedAny make_any(std::initializer_list<_Up> __il, _Args&&... __args)
  {
    return UncheckedAny(
#ifdef __cpp_lib_any
                        std::in_place_type<_Tp>,
#endif
                        __il, std::forward<_Args>(__args)...);
  }

  /**
   * @brief Access the contained object.
   *
   * @tparam  _ValueType  A const-reference or CopyConstructible type.
   * @param   __any       The object to access.
   * @return  The contained object.
   */
  template<typename _ValueType>
  static _ValueType any_cast(const UncheckedAny& __any)
  {
    using _Up = __remove_cvref_t<_ValueType>;
    static_assert(UncheckedAny::__is_valid_cast<_ValueType>(),
                  "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<_ValueType, const _Up&>::value,
                  "Template argument must be constructible from a const value.");
    auto __p = any_cast<_Up>(&__any);
    if (__p)
      return static_cast<_ValueType>(*__p);
    static _Up bad;
    return bad;
  }

  /**
   * @brief Access the contained object.
   *
   * @tparam  _ValueType  A reference or CopyConstructible type.
   * @param   __any       The object to access.
   * @return  The contained object.
   */
  template<typename _ValueType>
  static _ValueType any_cast(UncheckedAny& __any)
  {
    using _Up = __remove_cvref_t<_ValueType>;
    static_assert(UncheckedAny::__is_valid_cast<_ValueType>(),
                  "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<_ValueType, _Up&>::value,
                  "Template argument must be constructible from an lvalue.");
    auto __p = any_cast<_Up>(&__any);
    if (__p)
      return static_cast<_ValueType>(*__p);
    static _Up bad;
    return bad;
  }

  template<typename _ValueType>
  static _ValueType any_cast(UncheckedAny&& __any)
  {
    using _Up = __remove_cvref_t<_ValueType>;
    static_assert(UncheckedAny::__is_valid_cast<_ValueType>(),
                  "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<_ValueType, _Up>::value,
                  "Template argument must be constructible from an rvalue.");
    auto __p = any_cast<_Up>(&__any);
    if (__p)
      return static_cast<_ValueType>(std::move(*__p));
    static _Up bad;
    return bad;
  }

  template<typename _Tp>
  static void* __any_caster(const UncheckedAny* __any)
  {
    // any_cast<T> returns non-null if __any->type() == typeid(T) and
    // typeid(T) ignores cv-qualifiers so remove them:
    using _Up = typename std::remove_cv<_Tp>::type;
#ifdef __cpp_if_constexpr
    // The contained value has a decayed type, so if std::decay<U>::type is not U,
    // then it's not possible to have a contained value of type U:
    if constexpr (!std::is_same<typename std::decay<_Up>::type, _Up>::value)
                   return nullptr;
    // Only copy constructible types can be used for contained values:
    else if constexpr (!std::is_copy_constructible<_Up>::value)
                        return nullptr;
    // First try comparing function addresses, which works without RTTI
    else
#endif
    if (__any->_M_manager == &UncheckedAny::_Manager<_Up>::_S_manage)
      {
        UncheckedAny::_Arg __arg;
        __any->_M_manager(UncheckedAny::_Op_access, __any, &__arg);
        return __arg._M_obj;
      }
    return nullptr;
  }

  /**
   * @brief Access the contained object.
   *
   * @tparam  _ValueType  The type of the contained object.
   * @param   __any       A pointer to the object to access.
   * @return  The address of the contained object if <code>
   *          __any != nullptr && __any.type() == typeid(_ValueType)
   *          </code>, otherwise a null pointer.
   */
  template<typename _ValueType>
  static const _ValueType* any_cast(const UncheckedAny* __any) noexcept
  {
#ifdef __cpp_if_constexpr
    if constexpr (std::is_object<_ValueType>::value)
#endif
                   if (__any)
                     return static_cast<_ValueType*>(__any_caster<_ValueType>(__any));
    return nullptr;
  }

  template<typename _ValueType>
  static _ValueType* any_cast(UncheckedAny* __any) noexcept
  {
#ifdef __cpp_if_constexpr
    if constexpr (std::is_object<_ValueType>::value)
#endif
                   if (__any)
                     return static_cast<_ValueType*>(__any_caster<_ValueType>(__any));
    return nullptr;
  }
};

template<typename _Tp>
void
UncheckedAny::_Manager_internal<_Tp>::
_S_manage(_Op __which, const UncheckedAny* __any, _Arg* __arg)
{
  // The contained object is in _M_storage._M_buffer
  auto __ptr = reinterpret_cast<const _Tp*>(&__any->_M_storage._M_buffer);
  switch (__which)
    {
    case _Op_access:
      __arg->_M_obj = const_cast<_Tp*>(__ptr);
      break;
    case _Op_get_type_info:
      break;
    case _Op_clone:
      ::new(&__arg->_M_any->_M_storage._M_buffer) _Tp(*__ptr);
      __arg->_M_any->_M_manager = __any->_M_manager;
      break;
    case _Op_destroy:
      __ptr->~_Tp();
      break;
    case _Op_xfer:
      ::new(&__arg->_M_any->_M_storage._M_buffer) _Tp
        (std::move(*const_cast<_Tp*>(__ptr)));
      __ptr->~_Tp();
      __arg->_M_any->_M_manager = __any->_M_manager;
      const_cast<UncheckedAny*>(__any)->_M_manager = nullptr;
      break;
    }
}

template<typename _Tp>
void
UncheckedAny::_Manager_external<_Tp>::
_S_manage(_Op __which, const UncheckedAny* __any, _Arg* __arg)
{
  // The contained object is *_M_storage._M_ptr
  auto __ptr = static_cast<const _Tp*>(__any->_M_storage._M_ptr);
  switch (__which)
    {
    case _Op_access:
      __arg->_M_obj = const_cast<_Tp*>(__ptr);
      break;
    case _Op_get_type_info:
      break;
    case _Op_clone:
      __arg->_M_any->_M_storage._M_ptr = new _Tp(*__ptr);
      __arg->_M_any->_M_manager = __any->_M_manager;
      break;
    case _Op_destroy:
      delete __ptr;
      break;
    case _Op_xfer:
      __arg->_M_any->_M_storage._M_ptr = __any->_M_storage._M_ptr;
      __arg->_M_any->_M_manager = __any->_M_manager;
      const_cast<UncheckedAny*>(__any)->_M_manager = nullptr;
      break;
    }
}

// specialisations of __or_ and __and_ from top of UncheckedAny
template<> struct UncheckedAny::__or_<>  : public std::false_type {};
template<> struct UncheckedAny::__and_<> : public std::true_type  {};

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
