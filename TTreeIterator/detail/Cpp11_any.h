#ifndef HEADER_Cpp11_any
#define HEADER_Cpp11_any

#ifdef Cpp11_any_OPTIMIZE
#define ANY_TEMPLATE_OPT 1   // optimise templated any methods
//#define NO_ANY_RTTI_CHECK 1  // don't check type_info in any_cast<T>(any), just use templated function pointer - not useful optimisation
#define NO_ANY_RTTI 1        // don't use type_info (removes any::type() method) - not standard conforming
#define NO_ANY_ACCESS 1      // don't include any::Manager<T>::manage(Op_access) method - not ABI compatible with std::any
#define UNCHECKED_ANY 1      // don't check type of any_cast<T>(any)             - not standard conforming
#define NO_ANY_EXCEPTIONS 1  // don't throw exceptions                           - not standard conforming
#define ANY_SAME_TYPE 1      // when assigning the same as existing type, don't recreate
#endif

//#define Cpp11_std_any 1

#include <typeinfo>
#include <new>
#include <utility>
#include <type_traits>
#include <cstdlib>

// A type-safe container of any type.
//
// Based on GCC 10.2.0's std::any implementation, last updated 2020-04-23:
//   https://gcc.gnu.org/git/?p=gcc.git;a=commit;h=d1462b0782555354b4480e1f46498586d5882972
// The code has been fixed to work with C++11 and extensively de-dOxygenified,
// and de-STLified to make it easier to read (for me at least).
//
// A Cpp11::any object's state is either empty or it stores a contained object of CopyConstructible type.

#ifndef ANY_TEMPLATE_OPT
# undef NO_ANY_ACCESS
#endif
#if !__cpp_exceptions
# define NO_ANY_EXCEPTIONS 1
#endif
#undef UNCHECKED_ANY_CONSTEXPR
#ifdef __cpp_if_constexpr
# define Cpp11_constexpr_if constexpr
#else
# define Cpp11_constexpr_if
# ifdef UNCHECKED_ANY
#  define UNCHECKED_ANY_CONSTEXPR 1
# endif
#endif
#ifdef __cpp_variable_templates
# define Cpp11_in_place_type(T) in_place_type<T>
#else
# define Cpp11_in_place_type(T) in_place_type_t<T>{}
#endif

#ifdef Cpp11_std_any
namespace std
#else
namespace Cpp11
#endif
{
#ifndef NO_ANY_EXCEPTIONS
    // Exception class thrown by a failed any_cast
    struct bad_any_cast : public std::bad_cast {
      virtual const char* what() const noexcept { return "bad any_cast"; }
    };
#endif
  using any_type_code = const void*;

  class any {
  public:
    // Some internal stuff from GCC's std namespace...
    // See specialisations of or_ and and_ at end of file
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

#ifndef NO_ANY_EXCEPTIONS
    [[noreturn]] static void throw_bad_any_cast() { throw bad_any_cast{}; }
#else
    [[noreturn]] static void throw_bad_any_cast() { std::abort();         }
#endif

  private:

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
    using Decay_if_not_any = typename std::enable_if<!std::is_same<V, any>::value, V>::type;

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
    constexpr any() noexcept : _manager(nullptr) {}

    // Copy constructor, copies the state of other
    any(const any& other) {
      if (!other.has_value())
        _manager = nullptr;
      else {
        Arg arg;
        arg._any = this;
        other._manager(Op_clone, &other, &arg);
      }
    }

    // Move constructor, transfer the state from other
    any(any&& other) noexcept {
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
    any(T&& value) : _manager(&Mgr::manage) {
      Mgr::create(_storage, std::forward<T>(value));
    }

    // Construct with an object created from args as the contained object.
    template <typename T, typename... Args, typename V = typename std::decay<T>::type, typename Mgr = Manager<V>, any_constructible_t<V, Args&&...> = false>
    explicit any(in_place_type_t<T>, Args&&... args) : _manager(&Mgr::manage) {
      Mgr::create(_storage, std::forward<Args>(args)...);
    }

    // Construct with an object created from il and args as the contained object.
    template <typename T, typename Up, typename... Args, typename V = typename std::decay<T>::type, typename Mgr = Manager<V>, any_constructible_t<V, std::initializer_list<Up>, Args&&...> = false>
    explicit any(in_place_type_t<T>, std::initializer_list<Up> il, Args&&... args) : _manager(&Mgr::manage) {
      Mgr::create(_storage, il, std::forward<Args>(args)...);
    }

    // Destructor, calls reset()
    ~any() { reset(); }

    // assignments

    // Copy the state of another object.
    any& operator=(const any& rhs) {
      *this = any(rhs);
      return *this;
    }

    // Move assignment operator
    any& operator=(any&& rhs) noexcept {
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
    typename std::enable_if<std::is_copy_constructible<Decay_if_not_any<T>>::value, any&>::type
    operator=(T&& rhs) {
#ifdef ANY_SAME_TYPE
      using V = typename std::decay<T>::type;
      if (_manager == &any::Manager<V>::manage) {
        V* ptr = unchecked_any_caster<V>();
        ptr->~V();
        ::new(ptr) V(std::forward<T>(rhs));
      } else
#endif
        *this = any(std::forward<T>(rhs));
      return *this;
    }

    // Emplace with an object created from args as the contained object.
    template <typename T, typename... Args>
    emplace_t<typename std::decay<T>::type, Args...> emplace(Args&&... args) {
      using V = typename std::decay<T>::type;
#ifdef ANY_SAME_TYPE
      if (_manager == &any::Manager<V>::manage) {
        V* ptr = unchecked_any_caster<V>();
        ptr->~V();
        ::new(ptr) V(std::forward<Args>(args)...);
        return *ptr;
      }
#endif
      do_emplace<V>(std::forward<Args>(args)...);
      return *unchecked_any_caster<V>();
    }

    // Emplace with an object created from il and args as the contained object.
    template <typename T, typename Up, typename... Args>
    emplace_t<typename std::decay<T>::type, std::initializer_list<Up>, Args&&...> emplace(std::initializer_list<Up> il, Args&&... args) {
      using V = typename std::decay<T>::type;
#ifdef ANY_SAME_TYPE
      if (_manager == &any::Manager<V>::manage) {
        V* ptr = unchecked_any_caster<V>();
        ptr->~V();
        ::new(ptr) V(il, std::forward<Args>(args)...);
        return *ptr;
      }
#endif
      do_emplace<V, Up>(il, std::forward<Args>(args)...);
      return *unchecked_any_caster<V>();
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
    void swap(any& rhs) noexcept {
      if (!has_value() && !rhs.has_value()) return;
      if (has_value() && rhs.has_value()) {
        if (this == &rhs) return;
        any tmp;
        Arg arg;
        arg._any = &tmp;
        rhs._manager(Op_xfer, &rhs, &arg);
        arg._any = &rhs;
        _manager(Op_xfer, this, &arg);
        arg._any = this;
        tmp._manager(Op_xfer, &tmp, &arg);
      } else {
        any* empty = !has_value() ? this : &rhs;
        any* full = !has_value() ? &rhs : this;
        Arg arg;
        arg._any = empty;
        full->_manager(Op_xfer, full, &arg);
      }
    }

    // observers

    // Reports whether there is a contained object or not.
    bool has_value() const noexcept { return _manager != nullptr; }

#ifndef NO_ANY_RTTI
    // The typeid of the contained object, or typeid(void) if empty.
    const std::type_info& type() const noexcept {
      if (!has_value()) return typeid(void);
      Arg arg;
      _manager(Op_get_type_info, this, &arg);
      return *arg._typeinfo;
    }
#endif

    any_type_code type_code() const noexcept {
      return reinterpret_cast<any_type_code>(_manager);
    }

    template<typename T> static constexpr any_type_code type_code() {
      using Up = remove_cvref_t<T>;
      return reinterpret_cast<any_type_code>(&any::Manager<Up>::manage);
    }

    template<typename T> static constexpr bool is_valid_cast() { return or_<std::is_reference<T>, std::is_copy_constructible<T>>::value; }

  private:
    enum Op {
#ifndef NO_ANY_ACCESS
      Op_access,
#endif
#ifndef NO_ANY_RTTI
      Op_get_type_info,
#endif
      Op_clone,
      Op_destroy,
      Op_xfer
    };

    union Arg {
      void* _obj;
      const std::type_info* _typeinfo;
      any* _any;
    };

    void (*_manager)(Op, const any*, Arg*);
    Storage _storage;

    // Manage in-place contained object.
    template<typename T>
    struct Manager_internal {
      static void manage(Op which, const any* anyp, Arg* arg);

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

#ifdef ANY_TEMPLATE_OPT
      static T* access(const Storage& storage) {
        // The contained object is in _storage._buffer
        return const_cast<T*>( reinterpret_cast<const T*>(&storage._buffer) );
      }
#endif
    };

    // Manage external contained object.
    template<typename T>
    struct Manager_external {
      static void manage(Op which, const any* anyp, Arg* arg);

      template<typename Up>
      static void create(Storage& storage, Up&& value) {
        storage._ptr = new T(std::forward<Up>(value));
      }
      template<typename... Args>
      static void create(Storage& storage, Args&&... args) {
        storage._ptr = new T(std::forward<Args>(args)...);
      }

#ifdef ANY_TEMPLATE_OPT
      static T* access(const Storage& storage) {
        // The contained object is *_storage._ptr
        return const_cast<T*>( static_cast<const T*>(storage._ptr) );
      }
#endif
    };

  public:
    template<typename T>
    T* unchecked_any_caster() const {
#ifndef ANY_TEMPLATE_OPT
      any::Arg arg;
      _manager(any::Op_access, this, &arg);
      return static_cast<T*>(arg._obj);
#else
      return any::Manager<T>::access(_storage);
#endif
    }

    template<typename T>
    T* any_caster() const {
      // any_cast<T> returns non-null if any->type() == typeid(T) and typeid(T) ignores cv-qualifiers so remove them:
      using Up = typename std::remove_cv<T>::type;
#ifndef UNCHECKED_ANY_CONSTEXPR
      if Cpp11_constexpr_if (!std::is_object<T>::value) return nullptr;
      // The contained value has a decayed type, so if std::decay<U>::type is not U, then it's not possible to have a contained value of type U:
      else if Cpp11_constexpr_if (!std::is_same<typename std::decay<Up>::type, Up>::value) return nullptr;
      // Only copy constructible types can be used for contained values:
      else if Cpp11_constexpr_if (!std::is_copy_constructible<Up>::value) return nullptr;
      // First try comparing function addresses, which works without RTTI
      else
#endif
#ifndef UNCHECKED_ANY
        if (_manager == &any::Manager<Up>::manage
#if !defined(NO_ANY_RTTI) && !defined(NO_ANY_RTTI_CHECK)
            // see https://gcc.gnu.org/git/?p=gcc.git;a=commit;h=aa573a6a3e165632103f2f8defb9768106db6a61
            // for why this is needed. Fortunately it is rarely used, so doesn't usually slow us down
            || type() == typeid(T)
#endif
           )
#endif
          return unchecked_any_caster<Up>();
#ifndef UNCHECKED_ANY
      return nullptr;
#endif
    }

  };

#ifdef Cpp11_std_any
  using Cpp11_any = std::any;
#else
  using Cpp11_any = Cpp11::any;
#endif

  // Exchange the states of two any objects.
  inline void swap(any& x, any& y) noexcept { x.swap(y); }

  // Create an any holding a T constructed from args.
  template <typename T, typename... Args>
  inline any make_any(Args&&... args) {
    return any(Cpp11_any::Cpp11_in_place_type(T), std::forward<Args>(args)...);
  }

  // Create an any holding a T constructed from il and args.
  template <typename T, typename Up, typename... Args>
  inline any make_any(std::initializer_list<Up> il, Args&&... args) {
    return any(Cpp11_any::Cpp11_in_place_type(T), il, std::forward<Args>(args)...);
  }

  // Access the contained object.
  //   ValueType  A const-reference or CopyConstructible type.
  //   any_       The object to access.
  //   returns    The contained object.
  template<typename ValueType>
  inline ValueType any_cast(const any& any_) {
    using Up = Cpp11_any::remove_cvref_t<ValueType>;
    static_assert(any::is_valid_cast<ValueType>(),                    "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<ValueType, const Up&>::value, "Template argument must be constructible from a const value.");
    auto p = any_.any_caster<Up>();
    if (p) return static_cast<ValueType>(*p);
    Cpp11_any::throw_bad_any_cast();
  }

  // Access the contained object.
  //   ValueType  A reference or CopyConstructible type.
  //   any_       The object to access.
  //   returns    The contained object.
  template<typename ValueType>
  inline ValueType any_cast(any& any_) {
    using Up = Cpp11_any::remove_cvref_t<ValueType>;
    static_assert(any::is_valid_cast<ValueType>(),              "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<ValueType, Up&>::value, "Template argument must be constructible from an lvalue.");
    auto p = any_.any_caster<Up>();
    if (p) return static_cast<ValueType>(*p);
    Cpp11_any::throw_bad_any_cast();
  }

  template<typename ValueType>
  inline ValueType any_cast(any&& any_) {
    using Up = Cpp11_any::remove_cvref_t<ValueType>;
    static_assert(any::is_valid_cast<ValueType>(),             "Template argument must be a reference or CopyConstructible type");
    static_assert(std::is_constructible<ValueType, Up>::value, "Template argument must be constructible from an rvalue.");
    auto p = any_.any_caster<Up>();
    if (p) return static_cast<ValueType>(std::move(*p));
    Cpp11_any::throw_bad_any_cast();
  }

  // Access the contained object.
  //   ValueType  The type of the contained object.
  //   any_       A pointer to the object to access.
  //   returns    The address of the contained object if
  //                any_ != nullptr && any_.type() == typeid(ValueType)
  //              otherwise a null pointer.
  template<typename ValueType>
  inline const ValueType* any_cast(const any* any_) noexcept {
    if (!any_) return nullptr;
    return any_->any_caster<ValueType>();
  }

  template<typename ValueType>
  inline ValueType* any_cast(any* any_) noexcept {
    if (!any_) return nullptr;
    return any_->any_caster<ValueType>();
  }

  template<typename ValueType>
  inline constexpr any_type_code type_code() {
    return any::type_code<ValueType>();
  }

  template<typename T>
  void any::Manager_internal<T>::manage(Op which, const any* any_, Arg* arg) {
    // The contained object is in _storage._buffer
    auto ptr = reinterpret_cast<const T*>(&any_->_storage._buffer);
    switch (which) {
#ifndef NO_ANY_ACCESS
    case Op_access:
      arg->_obj = const_cast<T*>(ptr);
      break;
#endif
#ifndef NO_ANY_RTTI
    case Op_get_type_info:
      arg->_typeinfo = &typeid(T);
      break;
#endif
    case Op_clone:
      ::new(&arg->_any->_storage._buffer) T(*ptr);
      arg->_any->_manager = any_->_manager;
      break;
    case Op_destroy:
      ptr->~T();
      break;
    case Op_xfer:
      ::new(&arg->_any->_storage._buffer) T(std::move(*const_cast<T*>(ptr)));
      ptr->~T();
      arg->_any->_manager = any_->_manager;
      const_cast<any*>(any_)->_manager = nullptr;
      break;
    }
  }

  template<typename T>
  void any::Manager_external<T>::manage(Op which, const any* any_, Arg* arg) {
    // The contained object is *_storage._ptr
    auto ptr = static_cast<const T*>(any_->_storage._ptr);
    switch (which) {
#ifndef NO_ANY_ACCESS
    case Op_access:
      arg->_obj = const_cast<T*>(ptr);
      break;
#endif
#ifndef NO_ANY_RTTI
    case Op_get_type_info:
      arg->_typeinfo = &typeid(T);
      break;
#endif
    case Op_clone:
      arg->_any->_storage._ptr = new T(*ptr);
      arg->_any->_manager = any_->_manager;
      break;
    case Op_destroy:
      delete ptr;
      break;
    case Op_xfer:
      arg->_any->_storage._ptr = any_->_storage._ptr;
      arg->_any->_manager = any_->_manager;
      const_cast<any*>(any_)->_manager = nullptr;
      break;
    }
  }

  // specialisations of or_ and and_ from top of any
  template<> struct any::or_<>  : public std::false_type {};
  template<> struct any::and_<> : public std::true_type  {};
}

#endif /* HEADER_Cpp11_any */
