#ifndef HEADER_OrderedMap
#define HEADER_OrderedMap

#include <tuple>
#include <vector>
#include <iterator>

#include "TError.h"

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

#ifdef OrderedMap_STATS
  ~OrderedMap() {
    if (nhits || nmiss)
      Info ("~OrderedMap", "OrderedMap had %lu hits, %lu misses, %.1f%% success rate", nhits, nmiss, double(100*nhits)/double(nhits+nmiss));
    }
#endif

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

  std::pair<iterator, bool> insert (const value_type& val) {
    return vector_insert (map_type::insert ({val.first, typename map_type::mapped_type (val.second,0)}));
  }

  // I couldn't get the STL signature to work, probably because it refused to default the added 0:
  //template<typename... Args> std::pair<iterator, bool> emplace(Args&&... args) {
  //  return vector_insert (map_type::emplace (std::forward<Args>(args)...));    }

  template<typename K, typename V> std::pair<iterator, bool>
  emplace (const std::piecewise_construct_t& pc, K&& k, std::tuple<V>&& v) {
    return vector_insert (map_type::emplace (pc, std::forward<K>(k), std::forward_as_tuple(std::get<0>(v),0)));
  }

  std::pair<iterator, bool> emplace (value_type&& val) {
    return vector_insert (map_type::emplace (std::piecewise_construct,
                                             std::forward_as_tuple(std::get<0>(val)),
                                             std::forward_as_tuple(std::get<1>(val), 0)));
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
    if (mit->first == key) {
#ifdef OrderedMap_STATS
      ++nhits;
#endif
      return iterator(*this, mit);
    }
#ifdef OrderedMap_STATS
    ++nmiss;
#endif
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
#ifdef OrderedMap_STATS
  mutable size_t nhits=0, nmiss=0;
#endif
  vector_type _vec;
};

#endif /* HEADER_OrderedMap */
