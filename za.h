
// za.h

#ifndef _ZA_H
#define _ZA_H

#include "zmalloc.h"
#include <limits>
#include <cstddef>

namespace v {

template <typename T>
class Zalloc;

template <>
class Zalloc<void> {
 public:
  typedef void * pointer;
  typedef const void * const_pointer;
  typedef void value_type;

  template <typename U>
  struct rebind { typedef Zalloc<U> other; };
};


template <typename T>
class Zalloc {
 public:
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T * pointer;
  typedef const T * const_pointer;
  typedef T & reference;
  typedef const T & const_reference;
  typedef T value_type;

  template <typename U>
  struct rebind { typedef Zalloc<U> other; };

  pointer address(reference value) const {
    return &value;
  }

  const_pointer address(const_reference value) const {
    return &value;
  }

  Zalloc() throw () {}
  Zalloc(const Zalloc &) throw () {}

  template <typename U>
  Zalloc(const Zalloc<U> &) throw () {}

  ~Zalloc() throw () {}

  size_type max_size() const throw () {
    return std::numeric_limits<size_type>::max() / sizeof(value_type);
  }

  void construct(pointer p, const value_type &value) {
    new(p) value_type(value);
  }

  void destroy(pointer p) {
    p->~T();
  }

  pointer allocate(size_type n, Zalloc<void>::const_pointer hint = 0) {
    return (pointer)zmalloc(n * sizeof(T));
  }

  void deallocate(pointer p, size_type n) {
    zfree(p);
  }
};

template <typename T, typename U>
bool operator==(const Zalloc<T> &, const Zalloc<U> &) throw () {
  return true;
}

template <typename T, typename U>
bool operator!=(const Zalloc<T> &, const Zalloc<U> &) throw () {
  return false;
}

}

#endif // !_ZA_H
